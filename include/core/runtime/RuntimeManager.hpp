#pragma once

#include <QString>

#include "core/runtime/RuntimeManifest.hpp"

namespace core::runtime
{
    struct RuntimeInstallation
    {
        QString installation_root;
        QString manifest_path;
        RuntimeManifest manifest;
        QString wine_executable;
        QString wineserver_executable;
        QString wineboot_executable;
        QString self_test_executable;
        QString failure;
        bool usable {};
    };

    class RuntimeManager
    {
    public:
        explicit RuntimeManager(QString store_root = {});

        [[nodiscard]] QString store_root() const;
        [[nodiscard]] QString installed_root() const;
        [[nodiscard]] QString active_state_path() const;
        [[nodiscard]] RuntimeInstallation active() const;
        [[nodiscard]] RuntimeInstallation inspect_installation(
            const QString& installation_root) const;



        [[nodiscard]] static RuntimeInstallation inspect_package(
            const QString& installation_root);
        [[nodiscard]] QString resolve_active_entrypoint(
            const QString& entrypoint_name,
            QString* error = nullptr) const;
        bool activate(const QString& runtime_id,
                      const QString& build_id,
                      QString* error = nullptr) const;
        bool rollback(QString* error = nullptr) const;

        [[nodiscard]] static QString default_store_root();
        [[nodiscard]] static bool is_managed_selector(const QString& value);

    private:
        QString root;
    };
}
