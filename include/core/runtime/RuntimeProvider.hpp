#pragma once

#include <QString>

namespace core::runtime
{
    enum class RuntimeSource
    {
        None,
        Custom,
        Bundled,
        Managed,
        Development,
        System
    };

    struct RuntimeResolution
    {
        RuntimeSource source {RuntimeSource::None};
        QString selector;
        QString executable;
        QString display_name;
        QString failure;
        bool usable {};
        bool requires_rosetta {};
        bool rosetta_available {};
    };

    class RuntimeProvider
    {
    public:
        [[nodiscard]] static RuntimeResolution resolve(const QString& saved_selector,
                                                       bool allow_system_fallback = false);


        [[nodiscard]] static QString default_selector();
        [[nodiscard]] static QString bundled_runtime_root();
        [[nodiscard]] static QString development_runtime_root();
        [[nodiscard]] static QString source_name(RuntimeSource source);
    };
}
