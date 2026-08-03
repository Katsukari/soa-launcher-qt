#include "core/wine/RuntimeLocator.hpp"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <utility>

#include "core/runtime/RuntimeManager.hpp"
#include "core/wine/MacWineRuntime.hpp"
#include "core/wine/WineRegistry.hpp"
#include "util/Config.hpp"
#include <spdlog/spdlog.h>

namespace core::wine
{
    using util::config::Config;

    namespace
    {
        RuntimeSettings current_settings()
        {
            const Config& config = Config::instance();
            return {config.wine_binary(), config.prefix_root(), config.proton_compat_data_root(),
                    config.wine_arch(),   config.wine_args(),   config.use_dxvk()};
        }
    }

    RuntimeLocator::RuntimeLocator(SettingsProvider settings_provider)
        : settings_provider_(settings_provider ? std::move(settings_provider)
                                               : SettingsProvider(current_settings))
    {
    }

    RuntimeSettings RuntimeLocator::settings() const
    {
        return settings_provider_();
    }

    QString RuntimeLocator::wine_binary() const
    {
        const QString configured = settings().configured_runtime;
        if (!configured.isEmpty() && WineRegistry::identify(configured) == RuntimeType::Proton)
        {
            return configured;
        }

        const QString resolved = WineRegistry::resolve_wine_executable(configured);
#if defined(Q_OS_MACOS)
        return resolved;
#else
        return resolved.isEmpty() ? (configured.isEmpty() ? QStringLiteral("wine") : configured)
                                  : resolved;
#endif
    }

    QString RuntimeLocator::proton_root() const
    {
        const QFileInfo info(wine_binary());
        return info.isFile() ? info.dir().absolutePath() : info.absoluteFilePath();
    }

    QString RuntimeLocator::proton_binary() const
    {
        const QFileInfo info(wine_binary());
        if (info.isFile() &&
            info.fileName().compare(QStringLiteral("proton"), Qt::CaseInsensitive) == 0)
        {
            return info.absoluteFilePath();
        }
        return QDir(info.absoluteFilePath()).filePath(QStringLiteral("proton"));
    }

    QString RuntimeLocator::proton_wine_binary() const
    {
        const QString root = proton_root();
        for (const QString& relative :
             {QStringLiteral("files/bin/wine"), QStringLiteral("dist/bin/wine"),
              QStringLiteral("bin/wine")})
        {
            const QString candidate = QDir(root).filePath(relative);
            if (QFileInfo(candidate).isExecutable())
                return candidate;
        }
        return {};
    }

    QString RuntimeLocator::proton_wineserver_binary() const
    {
        const QString wine = proton_wine_binary();
        return wine.isEmpty() ? QString()
                              : QFileInfo(wine).dir().filePath(QStringLiteral("wineserver"));
    }

    QString RuntimeLocator::wineserver_binary() const
    {
        if (runtime_is_proton())
        {
            const QString proton_server = proton_wineserver_binary();
            return QFileInfo(proton_server).isExecutable() ? proton_server : QString();
        }

        if (core::runtime::RuntimeManager::is_managed_selector(settings().configured_runtime))
        {
            return core::runtime::RuntimeManager().resolve_active_entrypoint(
                QStringLiteral("wineserver"));
        }

        const QString wine = resolved_executable(wine_binary());
        if (!wine.isEmpty())
        {
            const QString sibling = QFileInfo(wine).dir().filePath(QStringLiteral("wineserver"));
            if (QFileInfo(sibling).isExecutable())
                return sibling;
        }
        return QStandardPaths::findExecutable(QStringLiteral("wineserver"));
    }

    QString RuntimeLocator::wineboot_binary() const
    {
        if (core::runtime::RuntimeManager::is_managed_selector(settings().configured_runtime))
        {
            const QString managed = core::runtime::RuntimeManager().resolve_active_entrypoint(
                QStringLiteral("wineboot"));
            if (!managed.isEmpty())
                return managed;
        }

        const QString wine = resolved_executable(wine_binary());
        if (QFileInfo(wine).isAbsolute())
        {
            const QString sibling = QFileInfo(wine).dir().filePath(QStringLiteral("wineboot"));
            if (QFileInfo(sibling).isExecutable())
                return sibling;
        }
        return wine;
    }

    QString RuntimeLocator::runtime_self_test_binary() const
    {
        if (core::runtime::RuntimeManager::is_managed_selector(settings().configured_runtime))
        {
            return core::runtime::RuntimeManager().resolve_active_entrypoint(
                QStringLiteral("self_test"));
        }

#if defined(Q_OS_MACOS)
        const QString runtime_root = macos::runtime_root_for_executable(wine_binary());
        for (const QString& relative :
             {QStringLiteral("Contents/Resources/tools/self-test-macos.sh"),
              QStringLiteral("tools/self-test-macos.sh")})
        {
            const QString candidate = QDir(runtime_root).filePath(relative);
            if (QFileInfo(candidate).isFile() && QFileInfo(candidate).isExecutable())
            {
                return candidate;
            }
        }
#endif
        return {};
    }

    bool RuntimeLocator::runtime_is_proton() const
    {
#if defined(Q_OS_MACOS)
        return false;
#else
        return WineRegistry::identify(wine_binary()) == RuntimeType::Proton;
#endif
    }

    QString RuntimeLocator::steam_root() const
    {
        const QString home = QDir::homePath();
        for (const QString& root :
             {home + QStringLiteral("/.local/share/Steam"), home + QStringLiteral("/.steam/steam"),
              home + QStringLiteral("/.steam/root"),
              home + QStringLiteral("/.var/app/com.valvesoftware.Steam/data/Steam")})
        {
            if (QFileInfo::exists(root))
                return root;
        }
        return home + QStringLiteral("/.steam");
    }

    QProcessEnvironment RuntimeLocator::proton_env() const
    {
        const RuntimeSettings values = settings();
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("STEAM_COMPAT_DATA_PATH"),
                           values.proton_compat_data_root);
        environment.insert(QStringLiteral("STEAM_COMPAT_CLIENT_INSTALL_PATH"), steam_root());
        environment.insert(QStringLiteral("WINEDLLOVERRIDES"), QStringLiteral("winegstreamer="));
        if (!values.use_dxvk)
        {
            environment.insert(QStringLiteral("PROTON_USE_WINED3D"), QStringLiteral("1"));
        }
        apply_wine_environment_entries(environment, values.wine_args);
        return environment;
    }

    QProcessEnvironment RuntimeLocator::winetricks_environment() const
    {
        const RuntimeSettings values = settings();
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("WINEPREFIX"), values.prefix_root);
        if (runtime_is_proton())
        {
            const QString wine = proton_wine_binary();
            const QString server = proton_wineserver_binary();
            if (!wine.isEmpty())
                environment.insert(QStringLiteral("WINE"), wine);
            if (!server.isEmpty())
            {
                environment.insert(QStringLiteral("WINESERVER"), server);
            }
        }
        else
        {
            const QString wine = resolved_executable(wine_binary());
            const QString server = wineserver_binary();
            environment.insert(QStringLiteral("WINE"), wine);
            if (!server.isEmpty())
            {
                environment.insert(QStringLiteral("WINESERVER"), server);
            }
            environment.insert(QStringLiteral("WINEARCH"), values.wine_arch);
        }
        apply_wine_environment_entries(environment, values.wine_args);
        return environment;
    }

    void RuntimeLocator::apply_wine_environment(QProcessEnvironment& environment) const
    {
        apply_wine_environment_entries(environment, settings().wine_args);
    }

    QString RuntimeLocator::resolved_executable(const QString& program) const
    {
        const QFileInfo info(program);
        if (info.isAbsolute())
            return info.absoluteFilePath();
        return QStandardPaths::findExecutable(program);
    }

    bool RuntimeLocator::is_wine_installed() const
    {
        const QString program =
            runtime_is_proton() ? proton_binary() : resolved_executable(wine_binary());
        const QFileInfo info(program);
        bool valid = info.isFile() && info.isExecutable();
#if defined(Q_OS_MACOS)
        const QString configured = settings().configured_runtime;
        if (valid && core::runtime::RuntimeManager::is_managed_selector(configured))
        {
            const auto package = core::runtime::RuntimeManager().active();
            valid =
                package.usable &&
                package.manifest.graphics_backends.contains(QStringLiteral("wined3d-opengl"),
                                                            Qt::CaseInsensitive) &&
                QFileInfo(package.wine_executable).canonicalFilePath() == info.canonicalFilePath();
            if (!valid)
            {
                SPDLOG_ERROR("managed runtime validation failed: {}",
                             package.failure.toStdString());
            }
        }
        else if (valid &&
                 QFileInfo(QDir(configured).filePath(QStringLiteral("runtime.json"))).isFile())
        {
            const auto package = core::runtime::RuntimeManager::inspect_package(configured);
            valid =
                package.usable &&
                package.manifest.graphics_backends.contains(QStringLiteral("wined3d-opengl"),
                                                            Qt::CaseInsensitive) &&
                QFileInfo(package.wine_executable).canonicalFilePath() == info.canonicalFilePath();
            if (!valid)
            {
                SPDLOG_ERROR("runtime package validation failed: {}",
                             package.failure.toStdString());
            }
        }
        if (valid && macos::executable_requires_rosetta(program) && !macos::rosetta_is_available())
        {
            valid = false;
        }
#endif
        return valid;
    }
}
