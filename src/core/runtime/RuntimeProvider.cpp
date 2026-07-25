#include "core/runtime/RuntimeProvider.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include "core/runtime/RuntimeManager.hpp"
#include "core/wine/MacWineRuntime.hpp"

namespace core::runtime
{
    namespace
    {
        RuntimeResolution inspect(const QString& selector,
                                  const RuntimeSource source,
                                  const QString& name)
        {
            RuntimeResolution result;
            result.source = source;
            result.selector = selector;
            result.display_name = name;
#if defined(Q_OS_MACOS)
            QString probeSelector = selector;
            const bool managed = source == RuntimeSource::Managed
                || RuntimeManager::is_managed_selector(selector);
            const bool packaged = !managed
                && QFileInfo(QDir(selector).filePath(
                       QStringLiteral("runtime.json"))).isFile();
            if (managed)
            {
                const RuntimeInstallation package = RuntimeManager().active();
                if (!package.usable)
                {
                    result.failure = package.failure;
                    return result;
                }
                if (!package.manifest.graphics_backends.contains(
                        QStringLiteral("wined3d-opengl"),
                        Qt::CaseInsensitive))
                {
                    result.failure = QStringLiteral(
                        "The managed runtime does not declare the supported macOS graphics backend.");
                    return result;
                }
                probeSelector = package.wine_executable;
                result.display_name = package.manifest.display_name;
            }
            else if (packaged)
            {
                const RuntimeInstallation package =
                    RuntimeManager::inspect_package(selector);
                if (!package.usable)
                {
                    result.failure = package.failure;
                    return result;
                }
                if (!package.manifest.graphics_backends.contains(
                        QStringLiteral("wined3d-opengl"),
                        Qt::CaseInsensitive))
                {
                    result.failure = QStringLiteral(
                        "The runtime package does not declare the supported macOS graphics backend.");
                    return result;
                }
                probeSelector = package.wine_executable;
                result.display_name = package.manifest.display_name;
            }

            const auto probe = core::wine::macos::probe_runtime(probeSelector);
            result.executable = probe.executable;
            result.failure = probe.failure;
            result.usable = probe.usable;
            result.requires_rosetta = probe.requires_rosetta;
            result.rosetta_available = probe.rosetta_available;
#else
            const QFileInfo executable(selector);
            result.executable = executable.absoluteFilePath();
            result.usable = executable.isFile() && executable.isExecutable();
            if (!result.usable)
                result.failure = QStringLiteral("Runtime executable is missing or not executable.");
#endif
            return result;
        }
    }

    QString RuntimeProvider::bundled_runtime_root()
    {
#if defined(Q_OS_MACOS)
        const QDir appDir(QCoreApplication::applicationDirPath());
        const QStringList candidates {
            appDir.filePath(QStringLiteral("../Resources/Story of Alicia Runtime")),
            appDir.filePath(QStringLiteral("../Resources/Story of Alicia Runtime.app")),
            appDir.filePath(QStringLiteral("../Resources/runtime")),
            appDir.filePath(QStringLiteral("../SharedSupport/runtime"))
        };
        for (const QString& candidate : candidates)
        {
            if (QFileInfo(candidate).isDir()
                && !core::wine::macos::resolve_wine_executable(candidate).isEmpty())
            {
                return QDir::cleanPath(candidate);
            }
        }
#endif
        return {};
    }

    QString RuntimeProvider::development_runtime_root()
    {
#if defined(Q_OS_MACOS)
        QDir directory(QCoreApplication::applicationDirPath());
        const QStringList relatives {
            QStringLiteral("../../../../soa_wine_runtime/runtime/out/Story of Alicia Runtime.app"),
            QStringLiteral("../../../soa_wine_runtime/runtime/out/Story of Alicia Runtime.app"),
            QStringLiteral("../../runtime/Story of Alicia Runtime.app"),
            QStringLiteral("../runtime")
        };
        for (const QString& relative : relatives)
        {
            const QString candidate = QDir::cleanPath(directory.filePath(relative));
            if (QFileInfo(candidate).isDir()
                && !core::wine::macos::resolve_wine_executable(candidate).isEmpty())
            {
                return candidate;
            }
        }
#endif
        return {};
    }

    QString RuntimeProvider::default_selector()
    {
#if defined(Q_OS_MACOS)
        const QString bundled = bundled_runtime_root();
        if (!bundled.isEmpty())
            return bundled;

        const RuntimeInstallation active = RuntimeManager().active();
        if (active.usable)
            return QString::fromLatin1(k_managed_active_selector);

        return development_runtime_root();
#else
        return {};
#endif
    }

    RuntimeResolution RuntimeProvider::resolve(const QString& saved_selector,
                                               const bool allow_system_fallback)
    {
        const QString saved = saved_selector.trimmed();

#if defined(Q_OS_MACOS)
        const QString bundled = bundled_runtime_root();
        const QString development = development_runtime_root();
        if (!saved.isEmpty())
        {
            if (RuntimeManager::is_managed_selector(saved))
            {
                return inspect(saved, RuntimeSource::Managed,
                               QStringLiteral("Managed Story of Alicia Runtime"));
            }
            if (!bundled.isEmpty()
                && QDir::cleanPath(saved) == QDir::cleanPath(bundled))
            {
                return inspect(bundled, RuntimeSource::Bundled,
                               QStringLiteral("Story of Alicia Runtime"));
            }
            if (!development.isEmpty()
                && QDir::cleanPath(saved) == QDir::cleanPath(development))
            {
                return inspect(development, RuntimeSource::Development,
                               QStringLiteral("Development Story of Alicia Runtime"));
            }


            return inspect(saved, RuntimeSource::Custom,
                           QStringLiteral("Custom Runtime"));
        }

        if (!bundled.isEmpty())
        {
            auto result = inspect(bundled, RuntimeSource::Bundled,
                                  QStringLiteral("Story of Alicia Runtime"));
            if (result.usable) return result;
        }

        RuntimeManager manager;
        const auto active = manager.active();
        if (active.usable)
        {
            auto result = inspect(QString::fromLatin1(k_managed_active_selector),
                                  RuntimeSource::Managed,
                                  QStringLiteral("Managed Story of Alicia Runtime"));
            if (result.usable) return result;
        }

        if (!development.isEmpty())
        {
            auto result = inspect(development, RuntimeSource::Development,
                                  QStringLiteral("Development Story of Alicia Runtime"));
            if (result.usable) return result;
        }

        if (allow_system_fallback)
        {
            const QString systemWine = QStandardPaths::findExecutable(QStringLiteral("wine"));
            if (!systemWine.isEmpty())
                return inspect(systemWine, RuntimeSource::System,
                               QStringLiteral("System Wine (development fallback)"));
        }

        RuntimeResolution failure;
        failure.failure = QStringLiteral(
            "The Story of Alicia runtime was not found. Select a custom runtime in Settings.");
        return failure;
#else
        if (!saved.isEmpty())
            return inspect(saved, RuntimeSource::Custom, QStringLiteral("Selected Runtime"));
        RuntimeResolution failure;
        failure.failure = QStringLiteral("No runtime is selected.");
        return failure;
#endif
    }

    QString RuntimeProvider::source_name(const RuntimeSource source)
    {
        switch (source)
        {
            case RuntimeSource::Custom: return QStringLiteral("custom");
            case RuntimeSource::Bundled: return QStringLiteral("bundled");
            case RuntimeSource::Managed: return QStringLiteral("managed");
            case RuntimeSource::Development: return QStringLiteral("development");
            case RuntimeSource::System: return QStringLiteral("system");
            case RuntimeSource::None: break;
        }
        return QStringLiteral("none");
    }
}
