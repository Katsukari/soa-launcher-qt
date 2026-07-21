#include "core/system/SystemProfile.hpp"

#include <QtGlobal>

#if defined(Q_OS_LINUX)
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#endif

namespace core::system
{
#if defined(Q_OS_LINUX)
    namespace
    {
        QString run_command(const QString& program, const QStringList& arguments,
                            const int timeout_ms)
        {
            if (program.isEmpty())
                return {};

            QProcess process;
            process.setProgram(program);
            process.setArguments(arguments);
            process.start();
            if (!process.waitForStarted(qMin(timeout_ms, 2000)))
                return {};
            if (!process.waitForFinished(timeout_ms))
            {
                process.kill();
                process.waitForFinished(1000);
                return {};
            }
            return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        }

        GpuVendor vendor_from_text(const QString& text)
        {
            const QString lower = text.toLower();
            if (lower.contains(QStringLiteral("nvidia")) || lower.contains(QStringLiteral("10de")))
                return GpuVendor::Nvidia;
            if (lower.contains(QStringLiteral("amd")) || lower.contains(QStringLiteral("ati"))
                || lower.contains(QStringLiteral("advanced micro devices"))
                || lower.contains(QStringLiteral("1002")))
                return GpuVendor::Amd;
            if (lower.contains(QStringLiteral("intel")) || lower.contains(QStringLiteral("8086")))
                return GpuVendor::Intel;
            return GpuVendor::Unknown;
        }

        GpuVendor detect_linux_gpu_vendor()
        {
            const QString lspci = QStandardPaths::findExecutable(QStringLiteral("lspci"));
            const QString output = run_command(
                lspci, {QStringLiteral("-mm"), QStringLiteral("-nn")}, 3000);
            for (const QString& line : output.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
            {
                const QString lower = line.toLower();
                if (!lower.contains(QStringLiteral("vga compatible controller"))
                    && !lower.contains(QStringLiteral("3d controller"))
                    && !lower.contains(QStringLiteral("display controller")))
                {
                    continue;
                }

                const GpuVendor vendor = vendor_from_text(line);
                if (vendor != GpuVendor::Unknown)
                    return vendor;
            }

            QDir drm(QStringLiteral("/sys/class/drm"));
            const QRegularExpression card_pattern(QStringLiteral(R"(^card\d+$)"));
            for (const QString& entry : drm.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            {
                if (!card_pattern.match(entry).hasMatch())
                    continue;

                QFile vendor_file(drm.filePath(entry + QStringLiteral("/device/vendor")));
                if (!vendor_file.open(QIODevice::ReadOnly | QIODevice::Text))
                    continue;

                const GpuVendor vendor = vendor_from_text(
                    QString::fromUtf8(vendor_file.readAll()).trimmed());
                if (vendor != GpuVendor::Unknown)
                    return vendor;
            }
            return GpuVendor::Unknown;
        }

        bool linux_vulkan_likely()
        {
            const QStringList roots {
                QStringLiteral("/usr/share/vulkan/icd.d"),
                QStringLiteral("/etc/vulkan/icd.d"),
                QStringLiteral("/usr/local/share/vulkan/icd.d"),
                QDir(QDir::homePath()).filePath(QStringLiteral(".local/share/vulkan/icd.d"))
            };
            for (const QString& root : roots)
            {
                const QDir dir(root);
                if (dir.exists()
                    && !dir.entryList({QStringLiteral("*.json")}, QDir::Files).isEmpty())
                {
                    return true;
                }
            }

            return !QStandardPaths::findExecutable(QStringLiteral("vulkaninfo")).isEmpty()
                || QFileInfo::exists(QStringLiteral("/usr/lib/libvulkan.so.1"))
                || QFileInfo::exists(QStringLiteral("/usr/lib64/libvulkan.so.1"))
                || QFileInfo::exists(
                    QStringLiteral("/usr/lib/x86_64-linux-gnu/libvulkan.so.1"));
        }
    }
#endif

    SystemProfile detect_system_profile()
    {
        SystemProfile profile;

        #if defined(Q_OS_LINUX)
                profile.gpu_vendor = detect_linux_gpu_vendor();
                profile.vulkan_likely = linux_vulkan_likely();
        #endif
                return profile;
    }
}
