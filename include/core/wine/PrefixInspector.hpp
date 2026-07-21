#pragma once

#include <QString>
#include <QStringList>

namespace core::wine
{
    enum class PrefixArchitecture
    {
        Unknown,
        Win32,
        Win64
    };

    struct PrefixInspection
    {
        bool exists {};
        bool marker_valid {};
        bool d3dx9_43 {};
        bool d3dcompiler_47 {};
        bool msvc_runtime {};
        bool dxvk_installed {};
        PrefixArchitecture architecture {PrefixArchitecture::Unknown};

        [[nodiscard]] bool required_components_present(bool proton) const
        {
            return d3dx9_43 && d3dcompiler_47 && (proton || msvc_runtime);
        }
    };

    class PrefixInspector
    {
    public:
        static PrefixArchitecture architecture(const QString& prefix);
        static QString game_dll_directory(const QString& prefix);
        static bool component_exists(const QString& prefix, const QString& file_name);
        static bool dxvk_installed(const QString& prefix);
        static bool marker_valid(const QString& prefix, const QString& runtime);
        static bool write_marker(const QString& prefix, const QString& runtime);
        static bool remove_marker(const QString& prefix);
        static PrefixInspection inspect(const QString& prefix, const QString& runtime, bool proton);
        static QStringList missing_packages(const QString& prefix, bool proton, bool request_dxvk);
    };
}
