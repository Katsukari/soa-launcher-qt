#include "core/Log.hpp"
#include "core/QtLogSink.hpp"
#include "core/wine/MacWineRuntime.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <QStandardPaths>
#include <QSysInfo>
#include <QDir>
#include <vector>
#include <cstdio>

namespace core::log
{
    namespace
    {
        constexpr std::size_t k_max_file_size = 5 * 1024 * 1024;
        constexpr std::size_t k_max_files     = 3;
    }

    void init()
    {
#if defined(Q_OS_MACOS)
        const QString dir = core::wine::macos::default_log_root();
#else
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#endif
        if (!QDir().mkpath(dir))
        {
            std::fprintf(stderr, "log: could not create log directory %s\n", dir.toStdString().c_str());
        }

        const std::string log_path = (dir + "/launcher.log").toStdString();

        std::vector<spdlog::sink_ptr> sinks;

        const auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console->set_pattern("[%H:%M:%S] [%^%l%$] %v");
        sinks.push_back(console);

        const auto file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_path, k_max_file_size, k_max_files);
        file->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v");
        sinks.push_back(file);

        const auto qt = std::make_shared<QtSignalSink_mt>();
        qt->set_pattern("[%H:%M:%S] [%l] %v");
        sinks.push_back(qt);

        const auto logger = std::make_shared<spdlog::logger>("soa", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::trace);
        logger->flush_on(spdlog::level::warn);
        spdlog::set_default_logger(logger);

        (void) LogBridge::instance();

        SPDLOG_INFO("logging initialised -> {}", log_path);
        SPDLOG_INFO("platform: {}", QSysInfo::prettyProductName().toStdString());
        SPDLOG_INFO("kernel: {} {}", QSysInfo::kernelType().toStdString(), QSysInfo::kernelVersion().toStdString());
        SPDLOG_INFO("arch: {}", QSysInfo::currentCpuArchitecture().toStdString());
        SPDLOG_INFO("qt: {}", qVersion());
    }
}
