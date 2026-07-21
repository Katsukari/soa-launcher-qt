#include "core/system/SystemProfile.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <QStringList>

namespace core::system
{
    namespace
    {
        QString read_first_value(const QString& path, const QStringList& keys)
        {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                return {};

            while (!file.atEnd())
            {
                const QString line = QString::fromUtf8(file.readLine()).trimmed();
                const int separator = line.indexOf(QLatin1Char(':'));
                if (separator <= 0)
                    continue;

                const QString key = line.left(separator).trimmed();
                for (const QString& candidate : keys)
                {
                    if (key.compare(candidate, Qt::CaseInsensitive) == 0)
                        return line.mid(separator + 1).trimmed();
                }
            }
            return {};
        }

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
            if (lower.contains(QStringLiteral("apple")) || lower.contains(QStringLiteral("106b")))
                return GpuVendor::Apple;
            return GpuVendor::Unknown;
        }

#if defined(Q_OS_LINUX)
        void detect_linux_gpu(SystemProfile& profile)
        {
            const QString lspci = QStandardPaths::findExecutable(QStringLiteral("lspci"));
            const QString output = run_command(lspci,
                {QStringLiteral("-mm"), QStringLiteral("-nn")}, 3000);
            for (const QString& line : output.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
            {
                const QString lower = line.toLower();
                if (!lower.contains(QStringLiteral("vga compatible controller"))
                    && !lower.contains(QStringLiteral("3d controller"))
                    && !lower.contains(QStringLiteral("display controller")))
                {
                    continue;
                }

                profile.gpu_name = line.simplified();
                profile.gpu_vendor = vendor_from_text(line);
                break;
            }

            if (profile.gpu_vendor != GpuVendor::Unknown)
                return;

            QDir drm(QStringLiteral("/sys/class/drm"));
            const QRegularExpression card_pattern(QStringLiteral(R"(^card\d+$)"));
            for (const QString& entry : drm.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            {
                if (!card_pattern.match(entry).hasMatch())
                    continue;

                QFile vendor_file(drm.filePath(entry + QStringLiteral("/device/vendor")));
                if (!vendor_file.open(QIODevice::ReadOnly | QIODevice::Text))
                    continue;

                const QString vendor_id = QString::fromUtf8(vendor_file.readAll()).trimmed();
                const GpuVendor vendor = vendor_from_text(vendor_id);
                if (vendor == GpuVendor::Unknown)
                    continue;

                profile.gpu_vendor = vendor;
                profile.gpu_name = gpu_vendor_name(vendor) + QStringLiteral(" graphics");
                break;
            }
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
                if (dir.exists() && !dir.entryList({QStringLiteral("*.json")}, QDir::Files).isEmpty())
                    return true;
            }

            return !QStandardPaths::findExecutable(QStringLiteral("vulkaninfo")).isEmpty()
                || QFileInfo::exists(QStringLiteral("/usr/lib/libvulkan.so.1"))
                || QFileInfo::exists(QStringLiteral("/usr/lib64/libvulkan.so.1"))
                || QFileInfo::exists(QStringLiteral("/usr/lib/x86_64-linux-gnu/libvulkan.so.1"));
        }
#endif

#if defined(Q_OS_MACOS)
        void detect_macos_gpu(SystemProfile& profile)
        {
            const QString profiler = QStringLiteral("/usr/sbin/system_profiler");
            const QString output = run_command(profiler,
                {QStringLiteral("SPDisplaysDataType")}, 7000);
            for (const QString& raw : output.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
            {
                const QString line = raw.trimmed();
                if (line.startsWith(QStringLiteral("Chipset Model:"), Qt::CaseInsensitive))
                {
                    profile.gpu_name = line.section(QLatin1Char(':'), 1).trimmed();
                    profile.gpu_vendor = vendor_from_text(profile.gpu_name);
                    if (profile.gpu_vendor == GpuVendor::Unknown
                        && profile.gpu_name.contains(QStringLiteral("Apple"), Qt::CaseInsensitive))
                    {
                        profile.gpu_vendor = GpuVendor::Apple;
                    }
                    break;
                }
            }
        }

        bool macos_vulkan_likely()
        {
            return QFileInfo::exists(QStringLiteral("/opt/homebrew/lib/libMoltenVK.dylib"))
                || QFileInfo::exists(QStringLiteral("/usr/local/lib/libMoltenVK.dylib"));
        }
#endif
    }

    QString gpu_vendor_name(const GpuVendor vendor)
    {
        switch (vendor)
        {
            case GpuVendor::Amd:     return QStringLiteral("AMD");
            case GpuVendor::Nvidia:  return QStringLiteral("NVIDIA");
            case GpuVendor::Intel:   return QStringLiteral("Intel");
            case GpuVendor::Apple:   return QStringLiteral("Apple");
            case GpuVendor::Unknown: return QStringLiteral("Unknown");
        }
        return QStringLiteral("Unknown");
    }

    SystemProfile detect_system_profile()
    {
        SystemProfile profile;
        profile.os_name = QSysInfo::prettyProductName().trimmed();
        if (profile.os_name.isEmpty())
            profile.os_name = QSysInfo::productType();

#if defined(Q_OS_LINUX)
        profile.cpu_name = read_first_value(QStringLiteral("/proc/cpuinfo"),
            {QStringLiteral("model name"), QStringLiteral("Hardware"), QStringLiteral("Processor")});
        detect_linux_gpu(profile);
        profile.vulkan_likely = linux_vulkan_likely();
#elif defined(Q_OS_MACOS)
        profile.cpu_name = run_command(QStringLiteral("/usr/sbin/sysctl"),
            {QStringLiteral("-n"), QStringLiteral("machdep.cpu.brand_string")}, 3000);
        if (profile.cpu_name.isEmpty())
        {
            profile.cpu_name = run_command(QStringLiteral("/usr/sbin/sysctl"),
                {QStringLiteral("-n"), QStringLiteral("hw.model")}, 3000);
        }
        detect_macos_gpu(profile);
        profile.vulkan_likely = macos_vulkan_likely();
#else
        profile.cpu_name = QSysInfo::currentCpuArchitecture();
#endif

        if (profile.cpu_name.isEmpty())
            profile.cpu_name = QSysInfo::currentCpuArchitecture();
        if (profile.gpu_name.isEmpty())
            profile.gpu_name = gpu_vendor_name(profile.gpu_vendor) == QStringLiteral("Unknown")
                ? QStringLiteral("Could not identify graphics hardware")
                : gpu_vendor_name(profile.gpu_vendor) + QStringLiteral(" graphics");

        return profile;
    }
}
