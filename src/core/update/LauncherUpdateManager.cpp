#include "core/update/LauncherUpdateManager.hpp"

#include "util/LanguageManager.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QProcess>
#include <QRegularExpression>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>

#include <spdlog/spdlog.h>

#include <limits>
#include <algorithm>
#include <utility>

#ifndef SOA_LAUNCHER_VERSION
#define SOA_LAUNCHER_VERSION "0.3.0"
#endif

#ifndef SOA_LAUNCHER_UPDATE_MANIFEST_URL
#define SOA_LAUNCHER_UPDATE_MANIFEST_URL "https://r2.storyofalicia.com/launcher/linux_launcher_version.json"
#endif

#ifndef SOA_LAUNCHER_UPDATE_PUBLIC_KEY_HEX
#define SOA_LAUNCHER_UPDATE_PUBLIC_KEY_HEX ""
#endif

#ifndef SOA_LAUNCHER_UPDATE_FALLBACK_MANIFEST_URL
#define SOA_LAUNCHER_UPDATE_FALLBACK_MANIFEST_URL "https://github.com/Story-Of-Alicia/soa-launcher-qt/releases/latest/download/linux_launcher_version.json"
#endif

namespace core::update
{
    LauncherUpdateManager::LauncherUpdateManager(QObject* parent)
        : QObject(parent)
    {
        QString manifest_url = qEnvironmentVariable("SOA_LAUNCHER_UPDATE_MANIFEST_URL").trimmed();
        if (manifest_url.isEmpty())
            manifest_url = QString::fromUtf8(SOA_LAUNCHER_UPDATE_MANIFEST_URL).trimmed();
        const QString public_key = QString::fromUtf8(SOA_LAUNCHER_UPDATE_PUBLIC_KEY_HEX).trimmed();
        const QString fallback_url = QString::fromUtf8(
            SOA_LAUNCHER_UPDATE_FALLBACK_MANIFEST_URL).trimmed();
        const QByteArray manifest_url_bytes = manifest_url.toUtf8();
        const QByteArray public_key_bytes = public_key.toUtf8();
        const QByteArray fallback_url_bytes = fallback_url.toUtf8();
        const QByteArray platform_bytes = detected_platform_key().toUtf8();
        const QByteArray directory_bytes = download_directory().toUtf8();
        const QByteArray user_agent = QByteArray("Story-Of-Alicia-Launcher/") + SOA_LAUNCHER_VERSION;
        const bool signing_key_valid = QRegularExpression(
            QStringLiteral("^[0-9a-fA-F]{64}$")).match(public_key).hasMatch();
        if (detected_platform_key() == QStringLiteral("linux-x86_64") && !signing_key_valid)
        {
            SPDLOG_DEBUG("launcher updates disabled: this build has no signing public key");
            schedule_health_checkpoint();
            return;
        }
        updater = soa_launcher_updater_create(
            manifest_url_bytes.constData(),
            fallback_url_bytes.constData(),
            public_key_bytes.constData(),
            SOA_LAUNCHER_VERSION,
            platform_bytes.constData(),
            directory_bytes.constData(),
            user_agent.constData(),
            qEnvironmentVariableIntValue("SOA_ALLOW_INSECURE_UPDATE_URLS") == 1,
            &LauncherUpdateManager::check_callback,
            &LauncherUpdateManager::progress_callback,
            &LauncherUpdateManager::download_callback,
            this);
        schedule_health_checkpoint();
    }

    LauncherUpdateManager::~LauncherUpdateManager()
    {
        if (updater)
        {
            soa_launcher_updater_shutdown(updater);
            soa_launcher_updater_destroy(updater);
            updater = nullptr;
        }
    }

    void LauncherUpdateManager::check_for_updates()
    {
        if (downloading)
            return;
        if (!updater)
        {
            emit no_update_available();
            return;
        }
        managing_versions = false;
        reset_release();
        emit check_started();
        soa_launcher_updater_check(updater);
    }

    void LauncherUpdateManager::manage_versions()
    {
        if (downloading)
            return;
        if (!updater)
        {
            emit manual_check_failed(util::i18n::translate(
                "This launcher build does not contain an update signing key."));
            return;
        }
        managing_versions = true;
        reset_release();
        emit check_started();
        soa_launcher_updater_check(updater);
    }

    void LauncherUpdateManager::download_and_install()
    {
        if (!updater || downloading || !update_available())
            return;
        downloading = true;
        emit download_started();
        soa_launcher_updater_download(updater);
    }

    void LauncherUpdateManager::cancel_download()
    {
        if (!updater || !downloading)
            return;
        soa_launcher_updater_cancel(updater);
    }

    void LauncherUpdateManager::check_callback(const soa_launcher_check_result result,
                                               const soa_launcher_error error_code,
                                               const int http_status,
                                               const char* error_detail,
                                               const char* version,
                                               const char* minimum_version,
                                               const char* release_message,
                                               const char* package_kind,
                                               const char* package_file_name,
                                               const char* package_url,
                                               const char* sha256,
                                               const uint64_t expected_size,
                                               const bool required,
                                               const char* releases_json,
                                               void* ctx)
    {
        auto* self = static_cast<LauncherUpdateManager*>(ctx);
        if (!self)
            return;
        QMetaObject::invokeMethod(self,
            [self,
             result,
             error_code,
             http_status,
             error_detail = QString::fromUtf8(error_detail ? error_detail : ""),
             version = QString::fromUtf8(version ? version : ""),
             minimum_version = QString::fromUtf8(minimum_version ? minimum_version : ""),
             release_message = QString::fromUtf8(release_message ? release_message : ""),
             package_kind = QString::fromUtf8(package_kind ? package_kind : ""),
             package_file_name = QString::fromUtf8(package_file_name ? package_file_name : ""),
             package_url = QUrl(QString::fromUtf8(package_url ? package_url : "")),
             sha256 = QByteArray(sha256 ? sha256 : ""),
             releases_json = QByteArray(releases_json ? releases_json : "[]"),
             expected_size,
             required]() mutable
            {
                self->handle_check(result,
                                   error_code,
                                   http_status,
                                   std::move(error_detail),
                                   std::move(version),
                                   std::move(minimum_version),
                                   std::move(release_message),
                                   std::move(package_kind),
                                   std::move(package_file_name),
                                   std::move(package_url),
                                   std::move(sha256),
                                   expected_size,
                                   required,
                                   std::move(releases_json));
            },
            Qt::QueuedConnection);
    }

    void LauncherUpdateManager::progress_callback(const uint64_t received,
                                                  const uint64_t total,
                                                  void* ctx)
    {
        auto* self = static_cast<LauncherUpdateManager*>(ctx);
        if (!self)
            return;
        QMetaObject::invokeMethod(self, [self, received, total]()
        {
            emit self->download_progress(static_cast<qint64>(qMin<qulonglong>(received, std::numeric_limits<qint64>::max())),
                                         static_cast<qint64>(qMin<qulonglong>(total, std::numeric_limits<qint64>::max())));
        }, Qt::QueuedConnection);
    }

    void LauncherUpdateManager::download_callback(const soa_launcher_download_result result,
                                                  const soa_launcher_error error_code,
                                                  const int http_status,
                                                  const char* error_detail,
                                                  const char* final_path,
                                                  void* ctx)
    {
        auto* self = static_cast<LauncherUpdateManager*>(ctx);
        if (!self)
            return;
        QMetaObject::invokeMethod(self,
            [self,
             result,
             error_code,
             http_status,
             error_detail = QString::fromUtf8(error_detail ? error_detail : ""),
             final_path = QString::fromUtf8(final_path ? final_path : "")]() mutable
            {
                self->handle_download(result,
                                      error_code,
                                      http_status,
                                      std::move(error_detail),
                                      std::move(final_path));
            },
            Qt::QueuedConnection);
    }

    void LauncherUpdateManager::handle_check(const soa_launcher_check_result result,
                                             const soa_launcher_error error_code,
                                             const int http_status,
                                             QString error_detail,
                                             QString version,
                                             QString minimum,
                                             QString release_message,
                                             QString kind,
                                             QString file_name,
                                             QUrl url,
                                             QByteArray sha256,
                                             const qulonglong size,
                                             const bool is_required,
                                             QByteArray releases_json)
    {
        const bool manual_check = managing_versions;
        if (result != soa_launcher_check_no_update
            && result != soa_launcher_check_update_available)
        {
            reset_release();
            managing_versions = false;
            const QString reason = error_message(error_code, http_status, error_detail);
            SPDLOG_WARN("launcher update check failed: {}", reason.toStdString());
            if (manual_check)
                emit manual_check_failed(reason);
            else
                emit check_failed(reason);
            return;
        }
        if (!parse_catalogue(releases_json))
        {
            reset_release();
            managing_versions = false;
            const QString reason = util::i18n::translate(
                "No valid signed launcher releases could be found.");
            SPDLOG_WARN("launcher update check failed: {}", reason.toStdString());
            if (manual_check)
                emit manual_check_failed(reason);
            else
                emit check_failed(reason);
            return;
        }
        if (result == soa_launcher_check_no_update)
        {
            reset_release();
            if (managing_versions && !releases.isEmpty())
            {
                managing_versions = false;
                const QString current = current_version();
                if (!select_version(current))
                    select_version(releases.constFirst().version);
                emit catalogue_ready();
                return;
            }
            managing_versions = false;
            emit no_update_available();
            return;
        }
        release_version = std::move(version);
        minimum_version = std::move(minimum);
        message = std::move(release_message);
        package_kind = std::move(kind);
        package_file_name = std::move(file_name);
        package_url = std::move(url);
        expected_sha256 = std::move(sha256);
        expected_size = size;
        required = is_required;
        const bool catalogue_requested = managing_versions;
        managing_versions = false;
        if (catalogue_requested)
            emit catalogue_ready();
        else
            emit update_found();
    }

    bool LauncherUpdateManager::parse_catalogue(const QByteArray& releases_json)
    {
        releases.clear();
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(releases_json, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject())
            return false;
        const QJsonArray entries = document.object().value(QStringLiteral("releases")).toArray();
        for (const QJsonValue& value : entries)
        {
            const QJsonObject entry = value.toObject();
            Release release;
            release.version = entry.value(QStringLiteral("version")).toString();
            release.message = entry.value(QStringLiteral("message")).toString();
            release.package_kind = QStringLiteral("appimage");
            release.file_name = entry.value(QStringLiteral("file_name")).toString();
            release.url = QUrl(entry.value(QStringLiteral("url")).toString());
            release.sha256 = entry.value(QStringLiteral("sha256")).toString().toLatin1();
            release.size = entry.value(QStringLiteral("size")).toVariant().toULongLong();
            if (release.version.isEmpty() || release.file_name.isEmpty() || !release.url.isValid()
                || release.sha256.size() != 64 || release.size == 0)
                return false;
            releases.push_back(std::move(release));
        }
        return !releases.isEmpty();
    }

    bool LauncherUpdateManager::select_version(const QString& version)
    {
        if (!updater || downloading)
            return false;
        const auto found = std::find_if(releases.cbegin(), releases.cend(), [&version](const Release& release)
        {
            return release.version == version;
        });
        if (found == releases.cend())
            return false;
        const QByteArray version_bytes = found->version.toUtf8();
        if (!soa_launcher_updater_select_version(updater, version_bytes.constData()))
            return false;
        release_version = found->version;
        minimum_version.clear();
        message = found->message;
        package_kind = found->package_kind;
        package_file_name = found->file_name;
        package_url = found->url;
        expected_sha256 = found->sha256;
        expected_size = found->size;
        required = false;
        return true;
    }

    void LauncherUpdateManager::handle_download(const soa_launcher_download_result result,
                                                const soa_launcher_error error_code,
                                                const int http_status,
                                                QString error_detail,
                                                QString final_path)
    {
        downloading = false;
        if (result != soa_launcher_download_completed)
        {
            const QString reason = error_message(error_code, http_status, error_detail);
            SPDLOG_ERROR("launcher update failed: {}", reason.toStdString());
            emit update_failed(reason);
            return;
        }
        final_download_path = std::move(final_path);
        install_downloaded_package();
    }

    void LauncherUpdateManager::reset_release()
    {
        release_version.clear();
        minimum_version.clear();
        message.clear();
        package_kind.clear();
        package_file_name.clear();
        final_download_path.clear();
        package_url.clear();
        expected_sha256.clear();
        expected_size = 0;
        required = false;
    }

    QString LauncherUpdateManager::error_message(const soa_launcher_error error_code,
                                                 const int http_status,
                                                 const QString& detail) const
    {
        switch (error_code)
        {
            case soa_launcher_error_busy:
                return util::i18n::translate("A launcher update operation is already running.");
            case soa_launcher_error_invalid_configuration:
                return util::i18n::translate("The launcher update configuration is invalid.");
            case soa_launcher_error_http:
                return util::i18n::translate("The launcher update server returned HTTP %1.").arg(http_status);
            case soa_launcher_error_response_too_large:
                return util::i18n::translate("The launcher update response is unexpectedly large.");
            case soa_launcher_error_invalid_release:
                return util::i18n::translate("The launcher update server returned invalid release information.");
            case soa_launcher_error_missing_asset:
                return util::i18n::translate("No compatible launcher package was attached to the release.");
            case soa_launcher_error_unsafe_url:
                return util::i18n::translate("The launcher release contains an invalid or untrusted package URL.");
            case soa_launcher_error_missing_digest:
                return util::i18n::translate("The launcher release package has no valid SHA-256 digest.");
            case soa_launcher_error_invalid_size:
                return util::i18n::translate("The launcher release package has an invalid size.");
            case soa_launcher_error_destination:
                return util::i18n::translate("The launcher could not create the update file.");
            case soa_launcher_error_write:
                return util::i18n::translate("The launcher could not write the downloaded update.");
            case soa_launcher_error_size_mismatch:
                return util::i18n::translate("The downloaded launcher update has an unexpected size.");
            case soa_launcher_error_digest_mismatch:
                return util::i18n::translate("The downloaded launcher update failed SHA-256 verification.");
            case soa_launcher_error_finalize:
                return util::i18n::translate("The launcher could not finalize the downloaded update file.");
            case soa_launcher_error_cancelled:
                return util::i18n::translate("The launcher update was cancelled.");
            case soa_launcher_error_invalid_signature:
                return util::i18n::translate("The launcher update metadata has an invalid signature.");
            case soa_launcher_error_network:
                return detail.isEmpty()
                    ? util::i18n::translate("The launcher update network request failed.")
                    : util::i18n::translate("The launcher update network request failed: %1").arg(detail);
            default:
                return detail.isEmpty()
                    ? util::i18n::translate("The launcher update failed.")
                    : detail;
        }
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
            if (!QFile::copy(final_download_path, staging)
                || !QFile::setPermissions(staging, executable_permissions))
            {
                QFile::remove(staging);
                emit update_failed(util::i18n::translate("The launcher could not stage the replacement AppImage beside the current launcher."));
                return;
            }
            QFile::remove(previous);
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

    void LauncherUpdateManager::schedule_health_checkpoint()
    {
#if defined(Q_OS_LINUX)
        if (!QCoreApplication::arguments().contains(QStringLiteral("--launcher-updated")))
            return;
        const QString current = qEnvironmentVariable("APPIMAGE").trimmed();
        if (current.isEmpty())
            return;
        QTimer::singleShot(15000, this, [current]()
        {
            QFile::remove(current + QStringLiteral(".soa-previous"));
        });
#endif
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

    QString LauncherUpdateManager::download_directory()
    {
        QString directory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (directory.isEmpty())
            directory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        if (directory.isEmpty())
            directory = QDir::tempPath();
        return directory;
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

    QStringList LauncherUpdateManager::available_versions() const
    {
        QStringList versions;
        versions.reserve(releases.size());
        for (const Release& release : releases)
            versions.push_back(release.version);
        return versions;
    }

    QString LauncherUpdateManager::current_version()
    {
        return QString::fromLatin1(SOA_LAUNCHER_VERSION);
    }
}
