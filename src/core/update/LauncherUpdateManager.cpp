#include "core/update/LauncherUpdateManager.hpp"

#include "util/LanguageManager.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QVersionNumber>

#include <spdlog/spdlog.h>

#ifndef SOA_LAUNCHER_VERSION
#define SOA_LAUNCHER_VERSION "0.3.0"
#endif

#ifndef SOA_LAUNCHER_GITHUB_REPOSITORY
#define SOA_LAUNCHER_GITHUB_REPOSITORY "Story-Of-Alicia/soa-launcher-qt"
#endif

namespace
{
    QString normalized_version(QString value)
    {
        value = value.trimmed();
        if (value.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
            value.remove(0, 1);
        return value;
    }

    QStringList prerelease_parts(const QString& version)
    {
        const QString without_build = version.section(QLatin1Char('+'), 0, 0);
        const int dash = without_build.indexOf(QLatin1Char('-'));
        if (dash < 0)
            return {};
        return without_build.mid(dash + 1).split(QLatin1Char('.'), Qt::KeepEmptyParts);
    }

    QVersionNumber numeric_version(const QString& version, bool& valid)
    {
        QString core = normalized_version(version).section(QLatin1Char('+'), 0, 0);
        const int dash = core.indexOf(QLatin1Char('-'));
        if (dash >= 0)
            core.truncate(dash);
        qsizetype suffix = 0;
        const QVersionNumber parsed = QVersionNumber::fromString(core, &suffix);
        valid = !parsed.isNull() && suffix == core.size();
        return parsed;
    }

    bool numeric_identifier(const QString& value, qulonglong& number)
    {
        if (value.isEmpty())
            return false;
        for (const QChar ch : value)
        {
            if (!ch.isDigit())
                return false;
        }
        bool ok = false;
        number = value.toULongLong(&ok);
        return ok;
    }

    QString release_directive(const QString& body, const QString& key)
    {
        const QRegularExpression expression(
            QStringLiteral("<!--\\s*soa-launcher-%1\\s*:\\s*([^>]*?)\\s*-->")
                .arg(QRegularExpression::escape(key)),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch match = expression.match(body);
        return match.hasMatch() ? match.captured(1).trimmed() : QString();
    }

    bool required_directive(const QString& body)
    {
        const QString value = release_directive(body, QStringLiteral("update")).toLower();
        if (value == QStringLiteral("required") || value == QStringLiteral("mandatory")
            || value == QStringLiteral("true") || value == QStringLiteral("yes"))
            return true;
        return body.contains(QStringLiteral("[launcher-update-required]"), Qt::CaseInsensitive);
    }

    QString release_summary(QString body)
    {
        const QRegularExpression directives(
            QStringLiteral("<!--\\s*soa-launcher-[^>]*-->"),
            QRegularExpression::CaseInsensitiveOption);
        body.remove(directives);
        body.replace(QStringLiteral("[launcher-update-required]"), QString(),
                     Qt::CaseInsensitive);
        body.replace(QRegularExpression(QStringLiteral("<[^>]+>")), QStringLiteral(" "));
        body.replace(QRegularExpression(QStringLiteral("\\[([^\\]]+)\\]\\([^\\)]+\\)")),
                     QStringLiteral("\\1"));
        const QStringList lines = body.split(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                                             Qt::SkipEmptyParts);
        for (QString line : lines)
        {
            line = line.trimmed();
            line.remove(QRegularExpression(QStringLiteral("^[#>*_`~\\-\\s]+")));
            line = line.simplified();
            if (line.isEmpty())
                continue;
            if (line.size() > 220)
                line = line.left(217).trimmed() + QStringLiteral("...");
            return line;
        }
        return {};
    }

    int asset_score(const QString& name, const QString& platform)
    {
        const QString lower = name.toLower();
        int score = 0;
        if (platform == QStringLiteral("linux-x86_64"))
        {
            if (!lower.endsWith(QStringLiteral(".appimage")))
                return -1;
            score = 100;
            if (lower.contains(QStringLiteral("x86_64"))
                || lower.contains(QStringLiteral("amd64")))
                score += 100;
            if (lower.contains(QStringLiteral("arm64"))
                || lower.contains(QStringLiteral("aarch64")))
                return -1;
        }
        else if (platform == QStringLiteral("macos-arm64"))
        {
            if (!lower.endsWith(QStringLiteral(".dmg")))
                return -1;
            score = 100;
            if (lower.contains(QStringLiteral("arm64"))
                || lower.contains(QStringLiteral("aarch64"))
                || lower.contains(QStringLiteral("apple-silicon")))
                score += 100;
            else if (lower.contains(QStringLiteral("universal")))
                score += 60;
            if (lower.contains(QStringLiteral("x86_64"))
                || lower.contains(QStringLiteral("amd64"))
                || lower.contains(QStringLiteral("intel")))
                return -1;
        }
        else if (platform == QStringLiteral("macos-x86_64"))
        {
            if (!lower.endsWith(QStringLiteral(".dmg")))
                return -1;
            score = 100;
            if (lower.contains(QStringLiteral("x86_64"))
                || lower.contains(QStringLiteral("amd64"))
                || lower.contains(QStringLiteral("intel")))
                score += 100;
            else if (lower.contains(QStringLiteral("universal")))
                score += 60;
            if (lower.contains(QStringLiteral("arm64"))
                || lower.contains(QStringLiteral("aarch64"))
                || lower.contains(QStringLiteral("apple-silicon")))
                return -1;
        }
        else
        {
            return -1;
        }

        if (lower.contains(QStringLiteral("story_of_alicia"))
            || lower.contains(QStringLiteral("story-of-alicia"))
            || lower.contains(QStringLiteral("soa-launcher")))
            score += 20;
        return score;
    }

    QJsonObject select_release_asset(const QJsonArray& assets, const QString& platform)
    {
        QJsonObject selected;
        int selected_score = -1;
        for (const QJsonValue& value : assets)
        {
            if (!value.isObject())
                continue;
            const QJsonObject asset = value.toObject();
            if (asset.value(QStringLiteral("state")).toString() != QStringLiteral("uploaded"))
                continue;
            const int score = asset_score(asset.value(QStringLiteral("name")).toString(), platform);
            if (score > selected_score)
            {
                selected = asset;
                selected_score = score;
            }
        }
        return selected;
    }

    QByteArray github_sha256(const QJsonObject& asset)
    {
        QString digest = asset.value(QStringLiteral("digest")).toString().trimmed().toLower();
        if (digest.startsWith(QStringLiteral("sha256:")))
            digest.remove(0, 7);
        static const QRegularExpression expression(QStringLiteral("^[0-9a-f]{64}$"));
        return expression.match(digest).hasMatch() ? digest.toLatin1() : QByteArray();
    }

    qint64 json_integer(const QJsonValue& value)
    {
        if (value.isDouble())
            return static_cast<qint64>(value.toDouble(-1));
        if (value.isString())
        {
            bool ok = false;
            const qint64 parsed = value.toString().toLongLong(&ok);
            return ok ? parsed : -1;
        }
        return -1;
    }
}

namespace core::update
{
    LauncherUpdateManager::LauncherUpdateManager(QObject* parent)
        : QObject(parent)
    {
        network = new QNetworkAccessManager(this);
        check_timeout = new QTimer(this);
        check_timeout->setSingleShot(true);
        download_timeout = new QTimer(this);
        download_timeout->setSingleShot(true);
        cleanup_previous_linux_package();
    }

    LauncherUpdateManager::~LauncherUpdateManager()
    {
        if (download_reply)
        {
            download_reply->disconnect(this);
            download_reply->abort();
        }
        if (download_file.isOpen())
            download_file.close();
        delete download_hash;
    }

    void LauncherUpdateManager::reset_release()
    {
        release_version.clear();
        minimum_version.clear();
        message.clear();
        package_kind.clear();
        package_file_name.clear();
        final_download_path.clear();
        partial_download_path.clear();
        package_url.clear();
        expected_sha256.clear();
        expected_size = -1;
        required = false;
    }

    QString LauncherUpdateManager::github_api_url() const
    {
        QString repository = qEnvironmentVariable("SOA_LAUNCHER_GITHUB_REPOSITORY").trimmed();
        if (repository.isEmpty())
            repository = QString::fromUtf8(SOA_LAUNCHER_GITHUB_REPOSITORY).trimmed();
        static const QRegularExpression expression(
            QStringLiteral("^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$"));
        if (!expression.match(repository).hasMatch())
            return {};
        return QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(repository);
    }

    bool LauncherUpdateManager::insecure_urls_allowed() const
    {
        return qEnvironmentVariableIntValue("SOA_ALLOW_INSECURE_UPDATE_URLS") == 1;
    }

    bool LauncherUpdateManager::validate_package_url(const QUrl& url) const
    {
        if (!url.isValid() || url.host().isEmpty())
            return false;
        if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0)
            return true;
        return insecure_urls_allowed()
            && url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0;
    }

    void LauncherUpdateManager::check_for_updates()
    {
        if (check_reply || downloading)
            return;

        reset_release();
        const QUrl url(github_api_url());
        if (!validate_package_url(url))
        {
            emit check_failed(util::i18n::translate(
                "The launcher GitHub repository setting is invalid."));
            return;
        }

        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                             QNetworkRequest::AlwaysNetwork);
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          QByteArray("Story-Of-Alicia-Launcher/") + SOA_LAUNCHER_VERSION);
        request.setRawHeader("Accept", "application/vnd.github+json");
        request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");

        emit check_started();
        QNetworkReply* reply = network->get(request);
        check_reply = reply;
        connect(check_timeout, &QTimer::timeout, reply, [reply]()
        {
            if (reply->isRunning())
                reply->abort();
        });
        check_timeout->start(12000);
        connect(reply, &QNetworkReply::finished, this, [this, reply]()
        {
            finish_check(reply);
        });
    }

    void LauncherUpdateManager::finish_check(QNetworkReply* reply)
    {
        check_timeout->stop();
        check_reply = nullptr;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray payload = reply->readAll();
        const QNetworkReply::NetworkError network_error = reply->error();
        const QString network_message = reply->errorString();
        reply->deleteLater();

        if (network_error != QNetworkReply::NoError || status < 200 || status >= 300)
        {
            const QString reason = status > 0
                ? util::i18n::translate("GitHub launcher update check returned HTTP %1.").arg(status)
                : util::i18n::translate("GitHub launcher update check failed: %1").arg(network_message);
            SPDLOG_WARN("{}", reason.toStdString());
            emit check_failed(reason);
            return;
        }

        if (payload.size() > 4 * 1024 * 1024)
        {
            emit check_failed(util::i18n::translate(
                "The GitHub release response is unexpectedly large."));
            return;
        }

        QJsonParseError parse_error;
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parse_error);
        if (parse_error.error != QJsonParseError::NoError || !document.isObject())
        {
            emit check_failed(util::i18n::translate(
                "GitHub returned invalid release information."));
            return;
        }

        const QJsonObject release = document.object();
        if (release.value(QStringLiteral("draft")).toBool(false)
            || release.value(QStringLiteral("prerelease")).toBool(false))
        {
            reset_release();
            emit no_update_available();
            return;
        }

        release_version = normalized_version(release.value(QStringLiteral("tag_name")).toString());
        if (release_version.isEmpty())
            release_version = normalized_version(release.value(QStringLiteral("name")).toString());
        if (release_version.isEmpty())
        {
            emit check_failed(util::i18n::translate(
                "The latest GitHub release has no valid version tag."));
            return;
        }

        bool release_version_valid = false;
        numeric_version(release_version, release_version_valid);
        if (!release_version_valid)
        {
            emit check_failed(util::i18n::translate(
                "The latest GitHub release has an invalid version tag."));
            return;
        }

        const QString current = QString::fromUtf8(SOA_LAUNCHER_VERSION);
        if (compare_versions(release_version, current) <= 0)
        {
            reset_release();
            emit no_update_available();
            return;
        }

        const QString body = release.value(QStringLiteral("body")).toString();
        minimum_version = normalized_version(
            release_directive(body, QStringLiteral("minimum-version")));
        if (!minimum_version.isEmpty())
        {
            bool minimum_version_valid = false;
            numeric_version(minimum_version, minimum_version_valid);
            if (!minimum_version_valid)
            {
                emit check_failed(util::i18n::translate(
                    "The GitHub release contains an invalid minimum launcher version."));
                return;
            }
        }

        const QJsonObject asset = select_release_asset(
            release.value(QStringLiteral("assets")).toArray(), detected_platform_key());
        if (asset.isEmpty())
        {
            emit check_failed(util::i18n::translate(
                "The GitHub release has no launcher package for this platform."));
            return;
        }

        package_file_name = safe_file_name(asset.value(QStringLiteral("name")).toString());
        package_url = QUrl(asset.value(QStringLiteral("browser_download_url")).toString());
        expected_sha256 = github_sha256(asset);
        expected_size = json_integer(asset.value(QStringLiteral("size")));

#if defined(Q_OS_MACOS)
        package_kind = QStringLiteral("dmg");
#elif defined(Q_OS_LINUX)
        package_kind = QStringLiteral("appimage");
#endif

        if (package_file_name.isEmpty() || !validate_package_url(package_url)
            || expected_sha256.isEmpty() || expected_size <= 0)
        {
            emit check_failed(util::i18n::translate(
                "The GitHub release asset is missing its URL, size, or SHA-256 digest."));
            return;
        }

        required = required_directive(body);
        if (!minimum_version.isEmpty() && compare_versions(current, minimum_version) < 0)
            required = true;
        message = release_summary(body);

        SPDLOG_INFO("launcher update available from GitHub: current={} available={} required={} platform={} asset={}",
                    current.toStdString(), release_version.toStdString(), required,
                    detected_platform_key().toStdString(), package_file_name.toStdString());
        emit update_found();
    }

    void LauncherUpdateManager::download_and_install()
    {
        if (!update_available() || downloading)
            return;
        begin_download();
    }

    QString LauncherUpdateManager::choose_download_path() const
    {
        QString root = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (root.isEmpty())
            root = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        if (root.isEmpty())
            root = QDir::tempPath();
        QDir directory(root);
        directory.mkpath(QStringLiteral("Story of Alicia Launcher Updates"));
        directory.cd(QStringLiteral("Story of Alicia Launcher Updates"));
        return directory.filePath(package_file_name);
    }

    void LauncherUpdateManager::begin_download()
    {
        final_download_path = choose_download_path();
        partial_download_path = final_download_path + QStringLiteral(".part");
        QFile::remove(partial_download_path);
        download_file.setFileName(partial_download_path);
        if (!download_file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            emit update_failed(util::i18n::translate("The launcher could not create the update download file: %1")
                                   .arg(download_file.errorString()));
            return;
        }

        delete download_hash;
        download_hash = new QCryptographicHash(QCryptographicHash::Sha256);
        downloaded_size = 0;
        download_write_failed = false;
        downloading = true;

        QNetworkRequest request(package_url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                             QNetworkRequest::AlwaysNetwork);
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          QByteArray("Story-Of-Alicia-Launcher/") + SOA_LAUNCHER_VERSION);

        QNetworkReply* reply = network->get(request);
        download_reply = reply;
        emit download_started();

        connect(download_timeout, &QTimer::timeout, reply, [reply]()
        {
            if (reply->isRunning())
                reply->abort();
        });
        download_timeout->start(60000);
        connect(reply, &QIODevice::readyRead, this, [this]()
        {
            consume_download_data();
        });
        connect(reply, &QNetworkReply::downloadProgress, this,
                [this](const qint64 received, const qint64 total)
        {
            download_timeout->start(60000);
            emit download_progress(received, total);
        });
        connect(reply, &QNetworkReply::finished, this, [this]()
        {
            finish_download();
        });
    }

    void LauncherUpdateManager::consume_download_data()
    {
        if (!download_reply || !download_file.isOpen() || download_write_failed)
            return;
        const QByteArray data = download_reply->readAll();
        if (data.isEmpty())
            return;
        if (download_file.write(data) != data.size())
        {
            download_write_failed = true;
            download_reply->abort();
            return;
        }
        download_hash->addData(data);
        downloaded_size += data.size();
    }

    void LauncherUpdateManager::finish_download()
    {
        download_timeout->stop();
        QNetworkReply* reply = download_reply;
        consume_download_data();
        download_reply = nullptr;
        const int status = reply
            ? reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
            : 0;
        const QNetworkReply::NetworkError network_error = reply
            ? reply->error()
            : QNetworkReply::UnknownNetworkError;
        const QString network_message = reply
            ? reply->errorString()
            : QStringLiteral("Unknown network error");
        if (reply)
            reply->deleteLater();

        download_file.flush();
        download_file.close();

        if (download_write_failed)
        {
            fail_download(util::i18n::translate("The launcher could not write the downloaded update."));
            return;
        }
        if (network_error != QNetworkReply::NoError || status < 200 || status >= 300)
        {
            fail_download(status > 0
                ? util::i18n::translate("The launcher update download returned HTTP %1.").arg(status)
                : util::i18n::translate("The launcher update download failed: %1").arg(network_message));
            return;
        }
        if (expected_size >= 0 && downloaded_size != expected_size)
        {
            fail_download(util::i18n::translate("The downloaded launcher update has an unexpected size."));
            return;
        }
        const QByteArray actual_hash = download_hash->result().toHex().toLower();
        if (actual_hash != expected_sha256)
        {
            fail_download(util::i18n::translate("The downloaded launcher update failed SHA-256 verification."));
            return;
        }

        QFile::remove(final_download_path);
        if (!QFile::rename(partial_download_path, final_download_path))
        {
            fail_download(util::i18n::translate("The launcher could not finalize the downloaded update file."));
            return;
        }

        downloading = false;
        delete download_hash;
        download_hash = nullptr;
        install_downloaded_package();
    }

    void LauncherUpdateManager::fail_download(const QString& reason)
    {
        downloading = false;
        if (download_file.isOpen())
            download_file.close();
        QFile::remove(partial_download_path);
        delete download_hash;
        download_hash = nullptr;
        SPDLOG_ERROR("launcher update failed: {}", reason.toStdString());
        emit update_failed(reason);
    }

    void LauncherUpdateManager::cancel_download()
    {
        if (!downloading)
            return;
        if (download_reply)
            download_reply->abort();
    }

    void LauncherUpdateManager::install_downloaded_package()
    {
#if defined(Q_OS_MACOS)
        open_macos_installer();
#elif defined(Q_OS_LINUX)
        install_linux_appimage();
#else
        emit update_failed(util::i18n::translate("Automatic launcher updates are unsupported on this platform."));
#endif
    }

    void LauncherUpdateManager::open_macos_installer()
    {
        if (package_kind != QStringLiteral("dmg"))
        {
            emit update_failed(util::i18n::translate("The macOS launcher update is not a DMG installer."));
            return;
        }
        if (!QProcess::startDetached(QStringLiteral("/usr/bin/open"), {final_download_path}))
        {
            emit update_failed(util::i18n::translate("The launcher could not open the downloaded macOS installer."));
            return;
        }
        emit installer_started(final_download_path);
    }

    void LauncherUpdateManager::install_linux_appimage()
    {
        if (package_kind != QStringLiteral("appimage"))
        {
            emit update_failed(util::i18n::translate("The Linux launcher update is not an AppImage."));
            return;
        }

        const QFile::Permissions executable_permissions =
            QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
            | QFileDevice::ReadGroup | QFileDevice::ExeGroup
            | QFileDevice::ReadOther | QFileDevice::ExeOther;
        if (!QFile::setPermissions(final_download_path, executable_permissions))
        {
            emit update_failed(util::i18n::translate("The launcher could not make the downloaded AppImage executable."));
            return;
        }

        QProcess validation;
        validation.start(final_download_path, {QStringLiteral("--appimage-offset")});
        if (!validation.waitForStarted(5000)
            || !validation.waitForFinished(10000)
            || validation.exitStatus() != QProcess::NormalExit
            || validation.exitCode() != 0
            || validation.readAllStandardOutput().trimmed().isEmpty())
        {
            emit update_failed(util::i18n::translate("The downloaded launcher update is not a valid AppImage."));
            return;
        }

        QString target = final_download_path;
        const QString current = qEnvironmentVariable("APPIMAGE").trimmed();
        if (!current.isEmpty() && QFileInfo::exists(current))
        {
            const QString staging = current + QStringLiteral(".soa-update-new");
            const QString previous = current + QStringLiteral(".soa-previous");
            QFile::remove(staging);
            QFile::remove(previous);
            if (!QFile::copy(final_download_path, staging)
                || !QFile::setPermissions(staging, executable_permissions))
            {
                QFile::remove(staging);
                emit update_failed(util::i18n::translate("The launcher could not stage the replacement AppImage beside the current launcher."));
                return;
            }
            if (!QFile::rename(current, previous))
            {
                QFile::remove(staging);
                emit update_failed(util::i18n::translate("The current AppImage could not be replaced. Check folder permissions."));
                return;
            }
            if (!QFile::rename(staging, current))
            {
                QFile::rename(previous, current);
                QFile::remove(staging);
                emit update_failed(util::i18n::translate("The new AppImage could not be installed. The previous launcher was restored."));
                return;
            }
            target = current;
        }

        const QString command = QStringLiteral(
            "pid=\"$1\"; target=\"$2\"; "
            "while kill -0 \"$pid\" 2>/dev/null; do sleep 0.1; done; "
            "exec \"$target\" --launcher-updated");
        const QStringList arguments {
            QStringLiteral("-c"),
            command,
            QStringLiteral("soa-launcher-update"),
            QString::number(QCoreApplication::applicationPid()),
            target
        };
        if (!QProcess::startDetached(QStringLiteral("/bin/sh"), arguments))
        {
            emit update_failed(util::i18n::translate("The updated AppImage was installed, but the launcher could not schedule its restart."));
            return;
        }
        emit installer_started(target);
    }

    int LauncherUpdateManager::compare_versions(const QString& left, const QString& right)
    {
        bool left_valid = false;
        bool right_valid = false;
        const QVersionNumber left_number = numeric_version(left, left_valid);
        const QVersionNumber right_number = numeric_version(right, right_valid);
        if (!left_valid || !right_valid)
            return QString::compare(normalized_version(left), normalized_version(right), Qt::CaseInsensitive);

        const int numeric_comparison = QVersionNumber::compare(left_number, right_number);
        if (numeric_comparison != 0)
            return numeric_comparison;

        const QStringList left_pre = prerelease_parts(normalized_version(left));
        const QStringList right_pre = prerelease_parts(normalized_version(right));
        if (left_pre.isEmpty() && right_pre.isEmpty())
            return 0;
        if (left_pre.isEmpty())
            return 1;
        if (right_pre.isEmpty())
            return -1;

        const qsizetype count = qMin(left_pre.size(), right_pre.size());
        for (qsizetype index = 0; index < count; ++index)
        {
            qulonglong left_value = 0;
            qulonglong right_value = 0;
            const bool left_numeric = numeric_identifier(left_pre[index], left_value);
            const bool right_numeric = numeric_identifier(right_pre[index], right_value);
            if (left_numeric && right_numeric)
            {
                if (left_value < right_value) return -1;
                if (left_value > right_value) return 1;
                continue;
            }
            if (left_numeric != right_numeric)
                return left_numeric ? -1 : 1;
            const int comparison = QString::compare(left_pre[index], right_pre[index], Qt::CaseInsensitive);
            if (comparison != 0)
                return comparison;
        }
        if (left_pre.size() < right_pre.size()) return -1;
        if (left_pre.size() > right_pre.size()) return 1;
        return 0;
    }

    QString LauncherUpdateManager::detected_platform_key()
    {
#if defined(Q_OS_MACOS) && defined(Q_PROCESSOR_ARM_64)
        return QStringLiteral("macos-arm64");
#elif defined(Q_OS_MACOS)
        return QStringLiteral("macos-x86_64");
#elif defined(Q_OS_LINUX) && defined(Q_PROCESSOR_X86_64)
        return QStringLiteral("linux-x86_64");
#elif defined(Q_OS_LINUX) && defined(Q_PROCESSOR_ARM_64)
        return QStringLiteral("linux-arm64");
#else
        return QStringLiteral("unsupported");
#endif
    }

    QString LauncherUpdateManager::safe_file_name(const QString& value)
    {
        QString result = QFileInfo(value.trimmed()).fileName();
        result.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._ -]")), QStringLiteral("_"));
        while (result.startsWith(QLatin1Char('.')))
            result.remove(0, 1);
        return result.left(180);
    }

    void LauncherUpdateManager::cleanup_previous_linux_package()
    {
#if defined(Q_OS_LINUX)
        const QString current = qEnvironmentVariable("APPIMAGE").trimmed();
        if (!current.isEmpty())
            QFile::remove(current + QStringLiteral(".soa-previous"));
#endif
    }

    bool LauncherUpdateManager::update_available() const
    {
        return !release_version.isEmpty() && package_url.isValid();
    }

    bool LauncherUpdateManager::update_required() const
    {
        return required;
    }

    QString LauncherUpdateManager::available_version() const
    {
        return release_version;
    }

    QString LauncherUpdateManager::release_message() const
    {
        return message;
    }

    QString LauncherUpdateManager::platform_key() const
    {
        return detected_platform_key();
    }

    QString LauncherUpdateManager::downloaded_path() const
    {
        return final_download_path;
    }
}
