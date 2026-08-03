#include "core/runtime/RuntimeManager.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QDateTime>

namespace core::runtime
{
    namespace
    {
        bool path_is_within(const QString& candidate, const QString& root)
        {
            if (candidate == root)
                return true;
            const QString prefix = root.endsWith(QLatin1Char('/'))
                ? root : root + QLatin1Char('/');
            return candidate.startsWith(prefix);
        }

        QString resolve_entrypoint(const QString& installationRoot,
                                   const QString& relative,
                                   const bool required,
                                   QString* error)
        {
            if (relative.isEmpty())
            {
                if (required && error)
                    *error = QStringLiteral("Required runtime entrypoint is missing from the manifest.");
                return {};
            }
            if (!is_safe_relative_runtime_path(relative))
            {
                if (error)
                    *error = QStringLiteral("Runtime entrypoint path is unsafe: %1").arg(relative);
                return {};
            }

            const QString canonicalRoot = QFileInfo(installationRoot).canonicalFilePath();
            const QFileInfo entry(QDir(installationRoot).filePath(relative));
            const QString canonicalEntry = entry.canonicalFilePath();
            if (canonicalRoot.isEmpty() || canonicalEntry.isEmpty()
                || !path_is_within(canonicalEntry, canonicalRoot))
            {
                if (error)
                    *error = QStringLiteral("Runtime entrypoint resolves outside its installation: %1")
                                 .arg(relative);
                return {};
            }
            if (!entry.isFile() || !entry.isExecutable())
            {
                if (error)
                    *error = QStringLiteral("Runtime entrypoint is missing or not executable: %1")
                                 .arg(relative);
                return {};
            }
            return canonicalEntry;
        }

        bool write_active_state(const QString& path,
                                const QString& runtimeId,
                                const QString& buildId,
                                const QString& previousRuntimeId,
                                const QString& previousBuildId,
                                QString* error)
        {
            QJsonObject object;
            object.insert(QStringLiteral("schema_version"), 1);
            object.insert(QStringLiteral("runtime_id"), runtimeId);
            object.insert(QStringLiteral("build_id"), buildId);
            object.insert(QStringLiteral("activated_at_utc"),
                          QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
            if (!previousRuntimeId.isEmpty() && !previousBuildId.isEmpty())
            {
                QJsonObject previous;
                previous.insert(QStringLiteral("runtime_id"), previousRuntimeId);
                previous.insert(QStringLiteral("build_id"), previousBuildId);
                object.insert(QStringLiteral("previous"), previous);
            }

            QSaveFile state(path);
            state.setDirectWriteFallback(false);
            if (!state.open(QIODevice::WriteOnly | QIODevice::Truncate))
            {
                if (error)
                    *error = QStringLiteral("Could not write managed runtime state: %1")
                                 .arg(state.errorString());
                return false;
            }
            const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
            if (state.write(bytes) != bytes.size())
            {
                if (error)
                    *error = QStringLiteral("Could not serialize managed runtime state: %1")
                                 .arg(state.errorString());
                state.cancelWriting();
                return false;
            }
            if (!state.commit())
            {
                if (error)
                    *error = QStringLiteral("Could not atomically activate managed runtime: %1")
                                 .arg(state.errorString());
                return false;
            }
            if (error)
                error->clear();
            return true;
        }

        bool read_active_identity(const QString& path,
                                  QString& runtimeId,
                                  QString& buildId,
                                  QString* error)
        {
            QFile state(path);
            if (!state.open(QIODevice::ReadOnly))
            {
                if (error)
                    *error = QStringLiteral("Managed runtime active.json could not be opened: %1")
                                 .arg(state.errorString());
                return false;
            }
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(state.readAll(), &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject())
            {
                if (error)
                    *error = QStringLiteral("Managed runtime active.json is invalid: %1")
                                 .arg(parseError.errorString());
                return false;
            }
            const QJsonObject object = document.object();
            if (object.value(QStringLiteral("schema_version")).toInt(-1) != 1)
            {
                if (error)
                    *error = QStringLiteral("Managed runtime active.json has an unsupported schema.");
                return false;
            }
            runtimeId = object.value(QStringLiteral("runtime_id")).toString().trimmed();
            buildId = object.value(QStringLiteral("build_id")).toString().trimmed();
            if (!is_safe_runtime_component(runtimeId) || !is_safe_runtime_component(buildId))
            {
                if (error)
                    *error = QStringLiteral("Managed runtime active.json contains an unsafe runtime or build ID.");
                return false;
            }
            if (error)
                error->clear();
            return true;
        }
    }

    RuntimeManager::RuntimeManager(QString storeRoot)
        : root(storeRoot.trimmed().isEmpty() ? default_store_root()
                                             : QDir::cleanPath(storeRoot))
    {
    }

    QString RuntimeManager::default_store_root()
    {
#if defined(Q_OS_MACOS)
        QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        if (base.isEmpty())
            base = QDir::home().filePath(QStringLiteral("Library/Application Support"));
        return QDir(base).filePath(QStringLiteral("Story of Alicia/runtimes"));
#else
        QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (base.isEmpty())
            base = QDir::home().filePath(QStringLiteral(".local/share/Story of Alicia"));
        return QDir(base).filePath(QStringLiteral("runtimes"));
#endif
    }

    bool RuntimeManager::is_managed_selector(const QString& value)
    {
        return value.trimmed().compare(QString::fromLatin1(k_managed_active_selector),
                                       Qt::CaseInsensitive) == 0;
    }

    QString RuntimeManager::store_root() const
    {
        return root;
    }

    QString RuntimeManager::installed_root() const
    {
        return QDir(root).filePath(QStringLiteral("installed"));
    }

    QString RuntimeManager::active_state_path() const
    {
        return QDir(root).filePath(QStringLiteral("active.json"));
    }

    RuntimeInstallation RuntimeManager::active() const
    {
        RuntimeInstallation result;
        QFile state(active_state_path());
        if (!state.open(QIODevice::ReadOnly))
        {
            result.failure = QStringLiteral("No managed Story of Alicia runtime is active.");
            return result;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(state.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            result.failure = QStringLiteral("Managed runtime active.json is invalid: %1")
                                 .arg(parseError.errorString());
            return result;
        }

        const QJsonObject object = document.object();
        if (object.value(QStringLiteral("schema_version")).toInt(-1) != 1)
        {
            result.failure = QStringLiteral("Managed runtime active.json has an unsupported schema.");
            return result;
        }
        const QString runtimeId = object.value(QStringLiteral("runtime_id")).toString().trimmed();
        const QString buildId = object.value(QStringLiteral("build_id")).toString().trimmed();
        if (!is_safe_runtime_component(runtimeId) || !is_safe_runtime_component(buildId))
        {
            result.failure = QStringLiteral("Managed runtime active.json contains an unsafe runtime or build ID.");
            return result;
        }

        const QString installation = QDir(installed_root()).filePath(
            runtimeId + QLatin1Char('/') + buildId);
        result = inspect_installation(installation);
        if (result.usable
            && (result.manifest.runtime_id != runtimeId || result.manifest.build_id != buildId))
        {
            result.usable = false;
            result.failure = QStringLiteral("Managed runtime manifest identity does not match active.json.");
        }
        return result;
    }

    RuntimeInstallation RuntimeManager::inspect_installation(
        const QString& installationRoot) const
    {
        RuntimeInstallation result;
        result.installation_root = QFileInfo(installationRoot).absoluteFilePath();

        const QString canonicalInstalled = QFileInfo(installed_root()).canonicalFilePath();
        const QString canonicalInstallation = QFileInfo(result.installation_root).canonicalFilePath();
        if (canonicalInstalled.isEmpty() || canonicalInstallation.isEmpty()
            || !path_is_within(canonicalInstallation, canonicalInstalled))
        {
            result.failure = QStringLiteral("Runtime installation is outside the managed runtime store.");
            return result;
        }

        return inspect_package(result.installation_root);
    }

    RuntimeInstallation RuntimeManager::inspect_package(
        const QString& installationRoot)
    {
        RuntimeInstallation result;
        result.installation_root = QFileInfo(installationRoot).absoluteFilePath();
        result.manifest_path = QDir(result.installation_root)
                                   .filePath(QStringLiteral("runtime.json"));

        QFile manifestFile(result.manifest_path);
        if (!manifestFile.open(QIODevice::ReadOnly))
        {
            result.failure = QStringLiteral("Runtime manifest could not be opened: %1")
                                 .arg(manifestFile.errorString());
            return result;
        }
        if (!RuntimeManifest::parse(manifestFile.readAll(), result.manifest, &result.failure))
            return result;

#if defined(Q_OS_MACOS)
        if (result.manifest.platform.compare(QStringLiteral("macos"), Qt::CaseInsensitive) != 0)
        {
            result.failure = QStringLiteral("The runtime package is not a macOS runtime.");
            return result;
        }
#elif defined(Q_OS_LINUX)
        if (result.manifest.platform.compare(QStringLiteral("linux"), Qt::CaseInsensitive) != 0)
        {
            result.failure = QStringLiteral("The runtime package is not a Linux runtime.");
            return result;
        }
#endif

        QString entryError;
        result.wine_executable = resolve_entrypoint(result.installation_root,
                                                     result.manifest.entrypoints.wine,
                                                     true, &entryError);
        if (result.wine_executable.isEmpty())
        {
            result.failure = entryError;
            return result;
        }
        result.wineserver_executable = resolve_entrypoint(
            result.installation_root, result.manifest.entrypoints.wineserver,
            !result.manifest.entrypoints.wineserver.isEmpty(), &entryError);
        if (!result.manifest.entrypoints.wineserver.isEmpty()
            && result.wineserver_executable.isEmpty())
        {
            result.failure = entryError;
            return result;
        }
        result.wineboot_executable = resolve_entrypoint(
            result.installation_root, result.manifest.entrypoints.wineboot,
            !result.manifest.entrypoints.wineboot.isEmpty(), &entryError);
        if (!result.manifest.entrypoints.wineboot.isEmpty()
            && result.wineboot_executable.isEmpty())
        {
            result.failure = entryError;
            return result;
        }
        result.self_test_executable = resolve_entrypoint(
            result.installation_root, result.manifest.entrypoints.self_test,
            !result.manifest.entrypoints.self_test.isEmpty(), &entryError);
        if (!result.manifest.entrypoints.self_test.isEmpty()
            && result.self_test_executable.isEmpty())
        {
            result.failure = entryError;
            return result;
        }
        result.usable = true;
        result.failure.clear();
        return result;
    }

    QString RuntimeManager::resolve_active_entrypoint(const QString& entrypointName,
                                                       QString* error) const
    {
        const RuntimeInstallation installation = active();
        if (!installation.usable)
        {
            if (error)
                *error = installation.failure;
            return {};
        }

        QString result;
        if (entrypointName == QStringLiteral("wine"))
            result = installation.wine_executable;
        else if (entrypointName == QStringLiteral("wineserver"))
            result = installation.wineserver_executable;
        else if (entrypointName == QStringLiteral("wineboot"))
            result = installation.wineboot_executable;
        else if (entrypointName == QStringLiteral("self_test"))
            result = installation.self_test_executable;
        else if (error)
            *error = QStringLiteral("Unknown runtime entrypoint: %1").arg(entrypointName);

        if (result.isEmpty() && error && error->isEmpty())
            *error = QStringLiteral("The active runtime does not provide entrypoint '%1'.")
                         .arg(entrypointName);
        else if (!result.isEmpty() && error)
            error->clear();
        return result;
    }

    bool RuntimeManager::activate(const QString& runtimeId,
                                  const QString& buildId,
                                  QString* error) const
    {
        if (!is_safe_runtime_component(runtimeId) || !is_safe_runtime_component(buildId))
        {
            if (error)
                *error = QStringLiteral("Runtime ID and build ID are unsafe.");
            return false;
        }

        const QString installation = QDir(installed_root()).filePath(
            runtimeId + QLatin1Char('/') + buildId);
        const RuntimeInstallation candidate = inspect_installation(installation);
        if (!candidate.usable)
        {
            if (error)
                *error = candidate.failure;
            return false;
        }
        if (candidate.manifest.runtime_id != runtimeId
            || candidate.manifest.build_id != buildId)
        {
            if (error)
                *error = QStringLiteral("Runtime manifest identity does not match the requested activation.");
            return false;
        }

        if (!QDir().mkpath(root))
        {
            if (error)
                *error = QStringLiteral("Could not create the managed runtime store.");
            return false;
        }

        QLockFile lock(QDir(root).filePath(QStringLiteral("active.lock")));
        lock.setStaleLockTime(60'000);
        if (!lock.tryLock(2'000))
        {
            if (error)
                *error = QStringLiteral("Another runtime activation is already in progress.");
            return false;
        }

        QString previousRuntimeId;
        QString previousBuildId;
        if (QFileInfo::exists(active_state_path()))
            read_active_identity(active_state_path(), previousRuntimeId, previousBuildId, nullptr);
        if (previousRuntimeId == runtimeId && previousBuildId == buildId)
        {
            if (error)
                error->clear();
            return true;
        }

        return write_active_state(active_state_path(), runtimeId, buildId,
                                  previousRuntimeId, previousBuildId, error);
    }

    bool RuntimeManager::rollback(QString* error) const
    {
        QFile state(active_state_path());
        if (!state.open(QIODevice::ReadOnly))
        {
            if (error)
                *error = QStringLiteral("There is no active managed runtime to roll back.");
            return false;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(state.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            if (error)
                *error = QStringLiteral("Managed runtime active.json is invalid: %1")
                             .arg(parseError.errorString());
            return false;
        }
        const QJsonObject previous = document.object()
            .value(QStringLiteral("previous")).toObject();
        const QString runtimeId = previous.value(QStringLiteral("runtime_id"))
            .toString().trimmed();
        const QString buildId = previous.value(QStringLiteral("build_id"))
            .toString().trimmed();
        if (!is_safe_runtime_component(runtimeId) || !is_safe_runtime_component(buildId))
        {
            if (error)
                *error = QStringLiteral("No valid previous managed runtime is recorded.");
            return false;
        }
        return activate(runtimeId, buildId, error);
    }
}
