#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace core::runtime
{
    inline constexpr int k_launcher_runtime_contract = 1;
    inline constexpr auto k_managed_active_selector = "managed://active";

    struct RuntimeEntrypoints
    {
        QString wine;
        QString wineserver;
        QString wineboot;
        QString self_test;
    };

    struct RuntimeManifest
    {
        int schema_version {};
        int launcher_contract {};
        int prefix_schema {};
        QString runtime_id;
        QString display_name;
        QString runtime_version;
        QString build_id;
        QString channel;
        QString platform;
        QString host_arch;
        QString wine_version;
        QString wine_commit;
        bool requires_rosetta_on_arm64 {};
        QStringList graphics_backends;
        RuntimeEntrypoints entrypoints;

        [[nodiscard]] bool valid() const;
        [[nodiscard]] QString identity() const;

        static bool parse(const QByteArray& json,
                          RuntimeManifest& result,
                          QString* error = nullptr);
        static bool parse(const QJsonObject& object,
                          RuntimeManifest& result,
                          QString* error = nullptr);
    };

    [[nodiscard]] bool is_safe_runtime_component(const QString& value);
    [[nodiscard]] bool is_safe_relative_runtime_path(const QString& value);
}
