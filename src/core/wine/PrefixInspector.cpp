#include "core/wine/PrefixInspector.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>

namespace core::wine
{
    namespace
    {
        QString marker_path(const QString& prefix)
        {
            return QDir(prefix).filePath(QStringLiteral(".soa-prefix-ready"));
        }
    }

    PrefixArchitecture PrefixInspector::architecture(const QString& prefix)
    {
        QFile registry(QDir(prefix).filePath(QStringLiteral("system.reg")));
        if (!registry.open(QIODevice::ReadOnly | QIODevice::Text))
            return PrefixArchitecture::Unknown;

        const QString header = QString::fromUtf8(registry.read(128 * 1024));
        static const QRegularExpression expression(
            QStringLiteral(R"((?:^|\n)\s*#arch\s*=\s*(win32|win64)\s*(?:\r?$))"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
        const auto match = expression.match(header);
        if (!match.hasMatch())
        {
            // Some Wine builds omit or relocate the header marker. A syswow64
            // directory is still a reliable indication that the prefix contains
            // the 32-bit side of a 64-bit prefix.
            if (QFileInfo::exists(QDir(prefix).filePath(QStringLiteral("drive_c/windows/syswow64"))))
                return PrefixArchitecture::Win64;
            return PrefixArchitecture::Unknown;
        }
        return match.captured(1).compare(QStringLiteral("win64"), Qt::CaseInsensitive) == 0
            ? PrefixArchitecture::Win64
            : PrefixArchitecture::Win32;
    }

    QString PrefixInspector::game_dll_directory(const QString& prefix)
    {
        const QString windows = QDir(prefix).filePath(QStringLiteral("drive_c/windows"));
        return QDir(windows).filePath(
            architecture(prefix) == PrefixArchitecture::Win64
                ? QStringLiteral("syswow64")
                : QStringLiteral("system32"));
    }

    bool PrefixInspector::component_exists(const QString& prefix, const QString& file_name)
    {
        return QFileInfo::exists(QDir(game_dll_directory(prefix)).filePath(file_name));
    }

    bool PrefixInspector::dxvk_installed(const QString& prefix)
    {
        QFile log(QDir(prefix).filePath(QStringLiteral("winetricks.log")));
        if (log.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            while (!log.atEnd())
            {
                if (QString::fromUtf8(log.readLine()).trimmed()
                    .compare(QStringLiteral("dxvk"), Qt::CaseInsensitive) == 0)
                {
                    return true;
                }
            }
        }

        // Wine itself ships d3d9.dll, so file existence alone cannot prove that
        // DXVK is active. Launcher-managed DXVK is installed through Winetricks
        // and therefore must be recorded in winetricks.log.
        return false;
    }

    bool PrefixInspector::marker_valid(const QString& prefix, const QString& runtime)
    {
        QFile marker(marker_path(prefix));
        if (!marker.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        const QString version = QString::fromUtf8(marker.readLine()).trimmed();
        const QString recordedRuntime = QString::fromUtf8(marker.readLine()).trimmed();
        return (version == QStringLiteral("1") || version == QStringLiteral("2"))
            && recordedRuntime == runtime;
    }

    bool PrefixInspector::write_marker(const QString& prefix, const QString& runtime)
    {
        if (!QDir().mkpath(prefix))
            return false;
        QSaveFile marker(marker_path(prefix));
        if (!marker.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        marker.write("2\n");
        marker.write(runtime.toUtf8());
        marker.write("\n");
        return marker.commit();
    }

    bool PrefixInspector::remove_marker(const QString& prefix)
    {
        const QString path = marker_path(prefix);
        return !QFileInfo::exists(path) || QFile::remove(path);
    }

    PrefixInspection PrefixInspector::inspect(const QString& prefix,
                                               const QString& runtime,
                                               const bool proton)
    {
        PrefixInspection result;
        result.exists = !prefix.isEmpty() && QDir(prefix).exists(QStringLiteral("drive_c"));
        if (!result.exists)
            return result;

        result.architecture = architecture(prefix);
        result.marker_valid = marker_valid(prefix, runtime);
        result.d3dx9_43 = component_exists(prefix, QStringLiteral("d3dx9_43.dll"));
        result.d3dcompiler_47 = component_exists(prefix, QStringLiteral("d3dcompiler_47.dll"));
        result.msvc_runtime = proton
            || (component_exists(prefix, QStringLiteral("msvcp140.dll"))
                && component_exists(prefix, QStringLiteral("vcruntime140.dll")));
        result.dxvk_installed = proton || dxvk_installed(prefix);
        return result;
    }

    QStringList PrefixInspector::missing_packages(const QString& prefix,
                                                  const bool proton,
                                                  const bool request_dxvk)
    {
        QStringList packages;
        if (!component_exists(prefix, QStringLiteral("d3dx9_43.dll")))
            packages << QStringLiteral("d3dx9");
        if (!component_exists(prefix, QStringLiteral("d3dcompiler_47.dll")))
            packages << QStringLiteral("d3dcompiler_47");
        if (!proton
            && (!component_exists(prefix, QStringLiteral("msvcp140.dll"))
                || !component_exists(prefix, QStringLiteral("vcruntime140.dll"))))
        {
            packages << QStringLiteral("vcrun2019");
        }
        if (!proton && request_dxvk && !dxvk_installed(prefix))
            packages << QStringLiteral("dxvk");
        return packages;
    }
}
