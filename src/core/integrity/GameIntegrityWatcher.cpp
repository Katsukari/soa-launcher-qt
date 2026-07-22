#include "core/integrity/GameIntegrityWatcher.hpp"

#include "util/Config.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace core::integrity
{
    namespace
    {
        constexpr int k_refresh_delay_ms = 800;
    }

    GameIntegrityWatcher::GameIntegrityWatcher(QObject* parent)
        : QObject(parent),
          watcher(new QFileSystemWatcher(this)),
          refresh_timer(new QTimer(this))
    {
        refresh_timer->setSingleShot(true);
        refresh_timer->setInterval(k_refresh_delay_ms);
        connect(refresh_timer, &QTimer::timeout, this, &GameIntegrityWatcher::refresh);
        connect(watcher, &QFileSystemWatcher::fileChanged,
                this, &GameIntegrityWatcher::inspect_file);
        connect(watcher, &QFileSystemWatcher::directoryChanged,
                this, &GameIntegrityWatcher::inspect_directory);
        connect(&util::config::Config::instance(), &util::config::Config::changed,
                this, [this]()
        {
            refresh_timer->start();
        });
    }

    int GameIntegrityWatcher::key(const core::game::GameVersion version)
    {
        return version == core::game::GameVersion::Alicia2 ? 2 : 1;
    }

    void GameIntegrityWatcher::set_suspended(const bool value)
    {
        if (suspended == value)
            return;
        suspended = value;
        if (!suspended)
            reset_after_suspension();
    }

    void GameIntegrityWatcher::reset_after_suspension()
    {
        pending_refresh = false;
        refresh_timer->start();
    }

    void GameIntegrityWatcher::clear_watchers()
    {
        const QStringList files = watcher->files();
        if (!files.isEmpty())
            watcher->removePaths(files);
        const QStringList directories = watcher->directories();
        if (!directories.isEmpty())
            watcher->removePaths(directories);
        file_versions.clear();
        directory_versions.clear();
    }

    void GameIntegrityWatcher::refresh()
    {
        if (suspended)
        {
            pending_refresh = true;
            return;
        }

        clear_watchers();
        contexts.clear();
        load_context(core::game::GameVersion::Playtest);
        load_context(core::game::GameVersion::Alicia2);
    }

    void GameIntegrityWatcher::load_context(const core::game::GameVersion version)
    {
        auto& config = util::config::Config::instance();
        const QString root = config.game_install_path(version);
        if (root.isEmpty() || !config.path_inside_prefix(root))
            return;

        const QString marker = QDir(root).filePath(
            QString::fromLatin1(core::game::profile(version).install_marker_file));
        QFile file(marker);
        if (!file.open(QIODevice::ReadOnly))
            return;

        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        const QString build = document.object().value(QStringLiteral("version")).toString();
        if (build.isEmpty())
            return;

        Context context;
        context.root = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
        context.version = build;
        contexts.insert(key(version), context);
        fetch_manifest(version, context.root, build);
    }

    void GameIntegrityWatcher::fetch_manifest(const core::game::GameVersion version,
                                              const QString& root,
                                              const QString& build)
    {
        const QString base = QString::fromLatin1(core::game::profile(version).cdn_base_url);
        QNetworkRequest request(QUrl(QStringLiteral("%1/%2/manifest.json").arg(base, build)));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply* reply = network.get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, version, root, build]()
        {
            const QByteArray payload = reply->readAll();
            const bool ok = reply->error() == QNetworkReply::NoError;
            reply->deleteLater();
            auto it = contexts.find(key(version));
            if (!ok || it == contexts.end() || it->root != root || it->version != build)
                return;
            apply_manifest(version, payload);
        });
    }

    QString GameIntegrityWatcher::safe_relative_path(const QString& value)
    {
        QString normalized = value;
        normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
        normalized = QDir::cleanPath(normalized);
        if (normalized.isEmpty() || normalized == QStringLiteral(".")
            || normalized.startsWith(QLatin1Char('/'))
            || normalized == QStringLiteral("..")
            || normalized.startsWith(QStringLiteral("../")))
        {
            return {};
        }
        const QStringList parts = normalized.split(QLatin1Char('/'));
        for (const QString& part : parts)
        {
            if (part.isEmpty() || part == QStringLiteral(".") || part == QStringLiteral(".."))
                return {};
        }
        return normalized;
    }

    void GameIntegrityWatcher::apply_manifest(const core::game::GameVersion version,
                                              const QByteArray& payload)
    {
        auto it = contexts.find(key(version));
        if (it == contexts.end() || suspended)
            return;

        const QJsonDocument document = QJsonDocument::fromJson(payload);
        const QJsonArray files = document.object().value(QStringLiteral("files")).toArray();
        if (files.isEmpty())
            return;

        it->hashes.clear();
        it->sizes.clear();
        for (const QJsonValue& value : files)
        {
            const QJsonObject object = value.toObject();
            const QString relative = safe_relative_path(object.value(QStringLiteral("path")).toString());
            const QByteArray hash = object.value(QStringLiteral("hash")).toString().toLatin1().toLower();
            const qint64 size = object.value(QStringLiteral("size")).toVariant().toLongLong();
            if (relative.isEmpty() || hash.isEmpty() || size < 0)
                continue;
            it->hashes.insert(relative, hash);
            it->sizes.insert(relative, size);
        }

        if (it->hashes.isEmpty())
            return;

        it->baseline_files = scan_files(it->root);
        it->ready = true;
        install_watchers(version);
    }

    QSet<QString> GameIntegrityWatcher::scan_files(const QString& root)
    {
        QSet<QString> result;
        QDirIterator iterator(root, QDir::Files | QDir::NoDotAndDotDot,
                              QDirIterator::Subdirectories);
        const QDir base(root);
        while (iterator.hasNext())
        {
            const QString absolute = iterator.next();
            const QString relative = QDir::cleanPath(base.relativeFilePath(absolute));
            if (!ignored_path(relative))
                result.insert(relative);
        }
        return result;
    }

    bool GameIntegrityWatcher::ignored_path(const QString& relative)
    {
        const QString lower = relative.toLower();
        return lower == QStringLiteral("version.json")
            || lower.endsWith(QStringLiteral(".download"))
            || lower.endsWith(QStringLiteral(".partial"))
            || lower.endsWith(QStringLiteral(".tmp"))
            || lower.contains(QStringLiteral(".update-staging/"))
            || lower.contains(QStringLiteral(".rollback/"));
    }

    void GameIntegrityWatcher::install_watchers(const core::game::GameVersion version)
    {
        auto it = contexts.find(key(version));
        if (it == contexts.end() || !it->ready)
            return;

        const QDir root(it->root);
        QSet<QString> directories;
        directories.insert(it->root);
        for (auto file = it->hashes.cbegin(); file != it->hashes.cend(); ++file)
        {
            const QString absolute = QDir::cleanPath(root.filePath(file.key()));
            const QFileInfo info(absolute);
            QString directory = info.absolutePath();
            while (directory.startsWith(it->root))
            {
                directories.insert(directory);
                if (directory == it->root)
                    break;
                directory = QFileInfo(directory).dir().absolutePath();
            }
            if (info.exists())
            {
                it->watched_files.insert(absolute);
                file_versions.insert(absolute, key(version));
            }
        }

        for (const QString& directory : directories)
        {
            if (!QFileInfo(directory).isDir())
                continue;
            it->watched_directories.insert(directory);
            directory_versions.insert(directory, key(version));
        }

        if (!it->watched_files.isEmpty())
            watcher->addPaths(it->watched_files.values());
        if (!it->watched_directories.isEmpty())
            watcher->addPaths(it->watched_directories.values());
    }

    QByteArray GameIntegrityWatcher::md5_file(const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        QCryptographicHash hash(QCryptographicHash::Md5);
        if (!hash.addData(&file))
            return {};
        return hash.result().toHex().toLower();
    }

    void GameIntegrityWatcher::inspect_file(const QString& path)
    {
        if (suspended)
        {
            pending_refresh = true;
            return;
        }

        const auto map = file_versions.constFind(QDir::cleanPath(path));
        if (map == file_versions.cend())
            return;
        const int context_key = map.value();
        auto it = contexts.find(context_key);
        if (it == contexts.end() || !it->ready || it->alerted)
            return;

        const QString relative = QDir(it->root).relativeFilePath(path);
        const QFileInfo info(path);
        bool changed = !info.exists();
        if (!changed)
        {
            const qint64 expected_size = it->sizes.value(relative, -1);
            changed = expected_size >= 0 && info.size() != expected_size;
            if (!changed)
                changed = md5_file(path) != it->hashes.value(relative);
            if (!watcher->files().contains(path))
                watcher->addPath(path);
        }
        if (changed)
        {
            report_change(context_key == 2 ? core::game::GameVersion::Alicia2
                                           : core::game::GameVersion::Playtest,
                          {relative});
        }
    }

    void GameIntegrityWatcher::inspect_directory(const QString& path)
    {
        if (suspended)
        {
            pending_refresh = true;
            return;
        }

        const auto map = directory_versions.constFind(QDir::cleanPath(path));
        if (map == directory_versions.cend())
            return;
        const int context_key = map.value();
        auto it = contexts.find(context_key);
        if (it == contexts.end() || !it->ready || it->alerted)
            return;

        QStringList changed;
        const QDir root(it->root);
        for (auto file = it->hashes.cbegin(); file != it->hashes.cend(); ++file)
        {
            const QString absolute = QDir::cleanPath(root.filePath(file.key()));
            const QFileInfo info(absolute);
            if (!info.exists() || info.size() != it->sizes.value(file.key(), -1))
                changed.append(file.key());
            else if (!watcher->files().contains(absolute))
            {
                watcher->addPath(absolute);
                file_versions.insert(absolute, context_key);
            }
        }

        const QSet<QString> current = scan_files(it->root);
        const QSet<QString> additions = current - it->baseline_files;
        for (const QString& relative : additions)
        {
            if (!it->hashes.contains(relative))
                changed.append(relative);
        }

        if (!changed.isEmpty())
        {
            changed.removeDuplicates();
            report_change(context_key == 2 ? core::game::GameVersion::Alicia2
                                           : core::game::GameVersion::Playtest,
                          changed);
        }
    }

    void GameIntegrityWatcher::report_change(const core::game::GameVersion version,
                                             const QStringList& paths)
    {
        auto it = contexts.find(key(version));
        if (it == contexts.end() || it->alerted)
            return;
        it->alerted = true;
        emit protected_files_changed(version, paths);
    }
}
