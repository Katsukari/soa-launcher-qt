#include "util/Config.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QVariantMap>

#include <utility>

#include "core/Log.hpp"
#include "core/wine/WineRegistry.hpp"
#include "util/CredentialStore.hpp"
#include <spdlog/spdlog.h>

namespace util::config
{
    namespace
    {
        QString absolute_clean_path(const QString& path)
        {
            return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
        }

        QString canonical_or_absolute(const QString& path)
        {
            const QFileInfo info(path);
            const QString canonical = info.canonicalFilePath();
            return canonical.isEmpty() ? absolute_clean_path(path) : QDir::cleanPath(canonical);
        }

        bool path_has_prefix(const QString& candidate, const QString& root)
        {
            return candidate == root || candidate.startsWith(root + QDir::separator());
        }

        bool existing_chain_has_symlink(const QString& root, const QString& candidate)
        {
            const QString relative = QDir(root).relativeFilePath(candidate);
            if (relative == QStringLiteral("..") || relative.startsWith(QStringLiteral("../")))
                return true;

            QString current = root;
            for (const QString& component : relative.split(QDir::separator(), Qt::SkipEmptyParts))
            {
                if (component == QStringLiteral("."))
                    continue;
                current = QDir(current).filePath(component);
                const QFileInfo info(current);
                if (info.exists() && info.isSymLink())
                    return true;
            }
            return false;
        }
    }

    class Config::Impl
    {
    public:
        QVariantMap values;
        QString username;
        QString token;
        QString display_name;
    };

    Config& Config::instance()
    {
        static Config instance;
        return instance;
    }

    Config::Config(QObject* parent) : QObject(parent), d(new Impl)
    {
        const QString directory = QFileInfo(file_path()).absolutePath();
        if (!QDir().mkpath(directory))
            SPDLOG_ERROR("config: could not create config directory {}", directory.toStdString());

        load();
        apply_defaults();
        if (keep_signed_in())
        {
            load_credentials();
        }
        else
        {
            d->username.clear();
            d->token.clear();
            d->display_name.clear();
            if (!clear_saved_credentials())
                SPDLOG_WARN("config: could not fully clear non-persistent credentials at startup");
        }
        save();

        watcher = new QFileSystemWatcher(this);
        watch_files();
        const auto schedule_reload = [this](const QString&)
        {
            if (writing)
                return;
            QTimer::singleShot(50, this, [this]() { reload_from_disk(); });
        };
        connect(watcher, &QFileSystemWatcher::fileChanged, this, schedule_reload);
        connect(watcher, &QFileSystemWatcher::directoryChanged, this, schedule_reload);
    }

    Config::~Config()
    {
        delete d;
    }

    QString Config::file_path() const
    {
        return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
            .filePath(QStringLiteral("config.json"));
    }

    QString Config::env_path() const
    {
        return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
            .filePath(QStringLiteral(".env"));
    }

    void Config::watch_files()
    {
        if (!watcher)
            return;

        const QString directory = QFileInfo(file_path()).absolutePath();
        if (QFileInfo(directory).isDir() && !watcher->directories().contains(directory))
            watcher->addPath(directory);

        const QStringList desired {file_path(), env_path()};
        for (const QString& path : desired)
        {
            if (QFileInfo::exists(path) && !watcher->files().contains(path))
                watcher->addPath(path);
        }
    }

    void Config::load()
    {
        QVariantMap loaded;
        QFile file(file_path());
        if (file.open(QIODevice::ReadOnly))
        {
            QJsonParseError error;
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error == QJsonParseError::NoError && document.isObject())
                loaded = document.object().toVariantMap();
            else
                SPDLOG_ERROR("config: failed to parse config.json: {}", error.errorString().toStdString());
        }
        else
        {
            SPDLOG_DEBUG("config: no existing file at {}, will create", file_path().toStdString());
        }

        d->values = std::move(loaded);
    }

    bool Config::save()
    {
        const QJsonDocument document(QJsonObject::fromVariantMap(d->values));
        writing = true;
        if (watcher)
        {
            watcher->removePath(file_path());
            watcher->removePath(env_path());
        }

        QSaveFile file(file_path());
        bool ok = file.open(QIODevice::WriteOnly);
        if (ok)
        {
            ok = file.write(document.toJson(QJsonDocument::Indented)) >= 0 && file.commit();
        }
        if (!ok)
            SPDLOG_ERROR("config: failed to save {}", file_path().toStdString());

        writing = false;
        watch_files();
        return ok;
    }

    bool Config::load_env_fallback()
    {
        QFile file(env_path());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;

        const QByteArray contents = file.readAll();
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(contents, &error);
        if (error.error == QJsonParseError::NoError && document.isObject())
        {
            const QJsonObject object = document.object();
            d->username = object.value(QStringLiteral("user")).toString();
            d->token = object.value(QStringLiteral("token")).toString();
            d->display_name = object.value(QStringLiteral("display_name")).toString();
            return has_auth();
        }


        const QString text = QString::fromUtf8(contents);
        for (const QString& rawLine : text.split(QLatin1Char('\n')))
        {
            const QString line = rawLine.trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                continue;
            const int equals = line.indexOf(QLatin1Char('='));
            if (equals <= 0)
                continue;

            const QString key = line.left(equals).trimmed();
            QString value = line.mid(equals + 1).trimmed();
            if (value.size() >= 2 && value.startsWith(QLatin1Char('"'))
                && value.endsWith(QLatin1Char('"')))
            {
                value = value.mid(1, value.size() - 2);
            }

            if (key == QStringLiteral("SOA_USER")) d->username = value;
            else if (key == QStringLiteral("SOA_TOKEN")) d->token = value;
            else if (key == QStringLiteral("SOA_USERNAME")) d->display_name = value;
        }
        return has_auth();
    }

    void Config::load_credentials()
    {
        d->username.clear();
        d->token.clear();
        d->display_name.clear();

        util::credentials::Credentials credentials;
        if (util::credentials::CredentialStore::load(credentials))
        {
            d->username = credentials.user;
            d->token = credentials.token;
            d->display_name = credentials.display_name;
            SPDLOG_DEBUG("config: loaded credentials from platform credential store");
            return;
        }

        if (load_env_fallback())
            SPDLOG_WARN("config: using protected .env credential fallback");
        else
            SPDLOG_DEBUG("config: no saved credentials");
    }

    bool Config::save_env_fallback()
    {
        if (!has_auth())
            return !QFileInfo::exists(env_path()) || QFile::remove(env_path());

        const QJsonObject object {
            {QStringLiteral("user"), d->username},
            {QStringLiteral("token"), d->token},
            {QStringLiteral("display_name"), d->display_name}
        };
        const QByteArray contents = QJsonDocument(object).toJson(QJsonDocument::Compact);

        QSaveFile file(env_path());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        if (file.write(contents) != contents.size() || !file.commit())
            return false;
        if (!QFile::setPermissions(env_path(), QFileDevice::ReadOwner | QFileDevice::WriteOwner))
        {
            SPDLOG_ERROR("config: could not restrict .env permissions");
            QFile::remove(env_path());
            return false;
        }
        return true;
    }

    bool Config::clear_saved_credentials()
    {
        const bool store_cleared = !util::credentials::CredentialStore::available()
            || util::credentials::CredentialStore::clear();
        const bool fallback_cleared = !QFileInfo::exists(env_path()) || QFile::remove(env_path());
        watch_files();
        return store_cleared && fallback_cleared;
    }

    bool Config::save_credentials()
    {
        if (!has_auth())
        {
            return clear_saved_credentials();
        }

        const util::credentials::Credentials credentials {
            d->username, d->token, d->display_name
        };
        if (util::credentials::CredentialStore::save(credentials))
        {
            QFile::remove(env_path());
            watch_files();
            return true;
        }

        SPDLOG_WARN("config: platform credential store unavailable; using protected .env fallback");
        const bool ok = save_env_fallback();
        watch_files();
        return ok;
    }

    QString Config::game_install_path_key(const core::game::GameVersion version)
    {
        return version == core::game::GameVersion::Alicia2
            ? QStringLiteral("game_install_path_2_0")
            : QStringLiteral("game_install_path_1_0");
    }

    QString Config::derive_game_path(const QString& prefix,
                                     const core::game::GameVersion version) const
    {
        const bool proton = core::wine::WineRegistry::identify(wine_binary())
            == core::wine::RuntimeType::Proton;
        const QString user = proton
            ? QStringLiteral("steamuser")
            : qEnvironmentVariable("USER", QDir::homePath().section(QLatin1Char('/'), -1));
        const auto& game_profile = core::game::profile(version);
        const QString folder = QString::fromLatin1(game_profile.default_install_directory);
        const QString subdirectory = QString::fromLatin1(game_profile.default_install_subdirectory);
        const QString roaming = QDir(prefix).filePath(
            QStringLiteral("drive_c/users/%1/AppData/Roaming/%2").arg(user, folder));
        return subdirectory.isEmpty() ? roaming : QDir(roaming).filePath(subdirectory);
    }

    void Config::probe_system_paths()
    {
        QStringList extraDirectories;
#ifdef Q_OS_MACOS
        extraDirectories << QStringLiteral("/opt/homebrew/bin") << QStringLiteral("/usr/local/bin");
#endif
        auto probe = [this, &extraDirectories](const QString& key, const QString& executable)
        {
            if (!d->values.value(key).toString().isEmpty())
                return;
            QString found = QStandardPaths::findExecutable(executable);
            if (found.isEmpty() && !extraDirectories.isEmpty())
                found = QStandardPaths::findExecutable(executable, extraDirectories);
            if (!found.isEmpty())
                d->values[key] = found;
        };

        probe(QStringLiteral("wine_binary"), QStringLiteral("wine"));
        if (d->values.value(QStringLiteral("wine_binary")).toString().isEmpty())
            probe(QStringLiteral("wine_binary"), QStringLiteral("wine64"));
        probe(QStringLiteral("winetricks_binary"), QStringLiteral("winetricks"));
    }

    void Config::apply_defaults()
    {
        probe_system_paths();
        auto setIfMissing = [this](const QString& key, const QVariant& value)
        {
            if (!d->values.contains(key))
                d->values[key] = value;
        };

        const QString defaultPrefix = QDir(QDir::homePath()).filePath(QStringLiteral("soa-launcher"));
        const bool hadProtonRoot = d->values.contains(QStringLiteral("proton_compat_data_root"));
        const QString legacyPrefix = d->values.value(QStringLiteral("wine_prefix"), defaultPrefix).toString();

        if (!hadProtonRoot && runtime_is_proton())
        {


            d->values[QStringLiteral("proton_compat_data_root")] =
                normalize_proton_compat_root(legacyPrefix);
            d->values[QStringLiteral("wine_prefix")] = defaultPrefix;
        }
        else
        {
            setIfMissing(QStringLiteral("wine_prefix"), defaultPrefix);
            setIfMissing(QStringLiteral("proton_compat_data_root"), defaultPrefix);
        }

        setIfMissing(QStringLiteral("wine_arch"), QStringLiteral("win64"));
        setIfMissing(QStringLiteral("use_dxvk"), false);
        setIfMissing(QStringLiteral("runtime_selected"), false);
        setIfMissing(QStringLiteral("wine_args"), QString());
        setIfMissing(QStringLiteral("prerequisites_confirmed"), false);
        setIfMissing(QStringLiteral("setup_assistant_version"), 0);
        setIfMissing(QStringLiteral("rules_accepted"), false);
        setIfMissing(QStringLiteral("keep_signed_in"), false);
        setIfMissing(QStringLiteral("launch_on_startup"), false);
        setIfMissing(QStringLiteral("after_game_start"), QStringLiteral("keep"));
        setIfMissing(QStringLiteral("launcher_size"), QStringLiteral("1400x846"));
        setIfMissing(QStringLiteral("game_version"), QStringLiteral("1.0"));
        setIfMissing(QStringLiteral("game_args"), QString());

        const QString legacyPath = d->values.value(QStringLiteral("game_install_path")).toString();
        setIfMissing(QStringLiteral("game_install_path_1_0"), legacyPath);
        setIfMissing(QStringLiteral("game_install_path_2_0"), QString());
        d->values.remove(QStringLiteral("game_install_path"));
        d->values.remove(QStringLiteral("game_id"));
        d->values.remove(QStringLiteral("rosetta_x87_path"));
        d->values.remove(QStringLiteral("setup_runtime_preference"));
        d->values.remove(QStringLiteral("setup_pc_age"));

        d->values[QStringLiteral("wine_prefix")] = normalize_wine_prefix(
            d->values.value(QStringLiteral("wine_prefix")).toString());
        d->values[QStringLiteral("proton_compat_data_root")] = normalize_proton_compat_root(
            d->values.value(QStringLiteral("proton_compat_data_root")).toString());
    }

    void Config::reload_from_disk()
    {
        if (writing)
            return;
        SPDLOG_INFO("config: files changed externally, reloading");
        load();
        apply_defaults();
        if (keep_signed_in())
        {
            load_credentials();
        }
        else
        {
            d->username.clear();
            d->token.clear();
            d->display_name.clear();
            if (!clear_saved_credentials())
                SPDLOG_WARN("config: could not fully clear non-persistent credentials after reload");
        }
        watch_files();
        emit changed();
    }

    QString Config::wine_binary() const { return d->values.value(QStringLiteral("wine_binary")).toString(); }
    QString Config::winetricks_binary() const { return d->values.value(QStringLiteral("winetricks_binary")).toString(); }
    QString Config::wine_arch() const
    {
        const QString value = d->values.value(QStringLiteral("wine_arch")).toString();
        return value.isEmpty() ? QStringLiteral("win64") : value;
    }
    bool Config::use_dxvk() const { return d->values.value(QStringLiteral("use_dxvk")).toBool(); }
    bool Config::runtime_selected() const { return d->values.value(QStringLiteral("runtime_selected")).toBool(); }
    QString Config::wine_args() const { return d->values.value(QStringLiteral("wine_args")).toString(); }
    bool Config::prerequisites_confirmed() const
    {
        return d->values.value(QStringLiteral("prerequisites_confirmed")).toBool()
            && d->values.value(QStringLiteral("setup_assistant_version")).toInt() >= 1;
    }
    bool Config::rules_accepted() const { return d->values.value(QStringLiteral("rules_accepted")).toBool(); }
    bool Config::keep_signed_in() const { return d->values.value(QStringLiteral("keep_signed_in")).toBool(); }
    bool Config::launch_on_startup() const { return d->values.value(QStringLiteral("launch_on_startup")).toBool(); }
    QString Config::after_game_start() const
    {
        const QString value = d->values.value(QStringLiteral("after_game_start")).toString();
        return value.isEmpty() ? QStringLiteral("keep") : value;
    }
    QString Config::launcher_size() const
    {
        const QString value = d->values.value(QStringLiteral("launcher_size")).toString();
        return value.isEmpty() ? QStringLiteral("1400x846") : value;
    }

    core::game::GameVersion Config::game_version() const
    {
        return core::game::game_version_from_string(
            d->values.value(QStringLiteral("game_version")).toString());
    }

    QString Config::game_args() const
    {
        return d->values.value(QStringLiteral("game_args")).toString();
    }

    bool Config::runtime_is_proton() const
    {
        return core::wine::WineRegistry::identify(wine_binary())
            == core::wine::RuntimeType::Proton;
    }

    QString Config::normalize_wine_prefix(const QString& path) const
    {
        QString configured = path.trimmed();
        if (configured.isEmpty())
            configured = QDir(QDir::homePath()).filePath(QStringLiteral("soa-launcher"));
        return absolute_clean_path(configured);
    }

    QString Config::normalize_proton_compat_root(const QString& path) const
    {
        QString configured = normalize_wine_prefix(path);
        if (QFileInfo(configured).fileName().compare(QStringLiteral("pfx"), Qt::CaseInsensitive) == 0)
        {
            const QString parent = QFileInfo(configured).dir().absolutePath();
            SPDLOG_INFO("config: normalized Proton pfx selection {} to compat-data root {}",
                        configured.toStdString(), parent.toStdString());
            configured = QDir::cleanPath(parent);
        }
        return configured;
    }

    QString Config::wine_prefix() const
    {
        return runtime_is_proton()
            ? proton_compat_data_root()
            : normalize_wine_prefix(d->values.value(QStringLiteral("wine_prefix")).toString());
    }

    QString Config::proton_compat_data_root() const
    {
        return normalize_proton_compat_root(
            d->values.value(QStringLiteral("proton_compat_data_root")).toString());
    }

    QString Config::prefix_root() const
    {
        return runtime_is_proton()
            ? QDir(proton_compat_data_root()).filePath(QStringLiteral("pfx"))
            : normalize_wine_prefix(d->values.value(QStringLiteral("wine_prefix")).toString());
    }

    QString Config::normalize_game_path(const QString& path) const
    {
        if (path.trimmed().isEmpty())
            return {};

        const QString candidate = absolute_clean_path(path);
        const QString prefix = absolute_clean_path(prefix_root());
        if (path_has_prefix(candidate, prefix))
            return candidate;

        if (runtime_is_proton())
        {
            const QString compat = absolute_clean_path(proton_compat_data_root());
            if (compat != prefix && path_has_prefix(candidate, compat))
            {
                const QString relative = QDir(compat).relativeFilePath(candidate);
                return relative == QStringLiteral(".")
                    ? prefix
                    : QDir::cleanPath(QDir(prefix).filePath(relative));
            }
        }
        return candidate;
    }

    QString Config::game_install_path() const
    {
        const auto version = game_version();
        const QString stored = d->values.value(game_install_path_key(version)).toString();
        return normalize_game_path(stored.isEmpty() ? derive_game_path(prefix_root(), version) : stored);
    }

    bool Config::path_inside_prefix(const QString& path) const
    {
        const QString candidate = normalize_game_path(path);
        if (candidate.isEmpty())
            return false;

        const QString rootAbsolute = absolute_clean_path(prefix_root());
        if (!path_has_prefix(candidate, rootAbsolute))
            return false;
        if (existing_chain_has_symlink(rootAbsolute, candidate))
            return false;

        const QString rootCanonical = canonical_or_absolute(rootAbsolute);
        QString ancestor = candidate;
        while (!QFileInfo::exists(ancestor))
        {
            const QString parent = QFileInfo(ancestor).dir().absolutePath();
            if (parent == ancestor)
                break;
            ancestor = parent;
        }
        const QString ancestorCanonical = canonical_or_absolute(ancestor);
        return path_has_prefix(ancestorCanonical, rootCanonical);
    }

    QString Config::username() const { return d->username; }
    QString Config::token() const { return d->token; }
    QString Config::display_name() const { return d->display_name; }
    bool Config::has_auth() const { return !d->username.isEmpty() && !d->token.isEmpty(); }

    void Config::set_wine_binary(const QString& value)
    {
        d->values[QStringLiteral("wine_binary")] = value;
        save(); emit changed();
    }
    void Config::set_winetricks_binary(const QString& value) { d->values[QStringLiteral("winetricks_binary")] = value; save(); emit changed(); }
    void Config::set_use_dxvk(const bool value) { d->values[QStringLiteral("use_dxvk")] = value; save(); emit changed(); }
    void Config::set_runtime_selected(const bool value) { d->values[QStringLiteral("runtime_selected")] = value; save(); emit changed(); }
    void Config::set_wine_args(const QString& value) { d->values[QStringLiteral("wine_args")] = value; save(); emit changed(); }
    void Config::set_prerequisites_confirmed(const bool value)
    {
        d->values[QStringLiteral("prerequisites_confirmed")] = value;
        if (value)
            d->values[QStringLiteral("setup_assistant_version")] = 1;
        save(); emit changed();
    }
    void Config::set_rules_accepted(const bool value) { d->values[QStringLiteral("rules_accepted")] = value; save(); emit changed(); }
    void Config::set_keep_signed_in(const bool value)
    {
        if (keep_signed_in() == value)
            return;

        d->values[QStringLiteral("keep_signed_in")] = value;
        save();

        if (value)
        {
            if (has_auth() && !save_credentials())
                SPDLOG_ERROR("config: failed to persist credentials after enabling keep-signed-in");
        }
        else if (!clear_saved_credentials())
        {
            SPDLOG_WARN("config: could not fully remove saved credentials after disabling keep-signed-in");
        }

        emit changed();
    }
    void Config::set_launch_on_startup(const bool value) { d->values[QStringLiteral("launch_on_startup")] = value; save(); emit changed(); }
    void Config::set_after_game_start(const QString& value) { d->values[QStringLiteral("after_game_start")] = value; save(); emit changed(); }
    void Config::set_launcher_size(const QString& value) { d->values[QStringLiteral("launcher_size")] = value; save(); emit changed(); }
    void Config::set_game_version(const core::game::GameVersion value) { d->values[QStringLiteral("game_version")] = core::game::to_string(value); save(); emit changed(); }
    void Config::set_game_args(const QString& value) { d->values[QStringLiteral("game_args")] = value; save(); emit changed(); }

    void Config::set_wine_prefix(const QString& value)
    {
        if (runtime_is_proton())
            d->values[QStringLiteral("proton_compat_data_root")] = normalize_proton_compat_root(value);
        else
            d->values[QStringLiteral("wine_prefix")] = normalize_wine_prefix(value);
        save(); emit changed();
    }

    void Config::set_game_install_path(const QString& value)
    {
        d->values[game_install_path_key(game_version())] = normalize_game_path(value);
        save(); emit changed();
    }

    bool Config::game_installed() const
    {
        const QDir directory(game_install_path());
        if (!directory.exists())
            return false;
        const auto& game = core::game::profile(game_version());
        const QFileInfo versionFile(directory.filePath(QString::fromLatin1(game.install_marker_file)));
        const QFileInfo executable(directory.filePath(QString::fromLatin1(game.executable_name)));
        return versionFile.isFile() && versionFile.size() > 0 && executable.isFile();
    }

    void Config::set_auth(const QString& username, const QString& token,
                          const QString& displayName)
    {
        d->username = username;
        d->token = token;
        d->display_name = displayName;
        if (keep_signed_in())
        {
            if (!save_credentials())
                SPDLOG_ERROR("config: failed to persist credentials securely");
        }
        else if (!clear_saved_credentials())
        {
            SPDLOG_WARN("config: could not clear old saved credentials for this session-only login");
        }
        emit changed();
    }

    bool Config::reset_launcher_config()
    {
        writing = true;
        if (watcher)
        {
            watcher->removePath(file_path());
            watcher->removePath(env_path());
        }

        const bool configRemoved = !QFileInfo::exists(file_path()) || QFile::remove(file_path());
        d->values.clear();
        d->username.clear();
        d->token.clear();
        d->display_name.clear();
        const bool credentialsCleared = save_credentials();
        apply_defaults();
        writing = false;
        const bool saved = save();
        watch_files();
        emit changed();
        return configRemoved && credentialsCleared && saved;
    }
}
