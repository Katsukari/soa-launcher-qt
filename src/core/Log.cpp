#include "core/Log.hpp"
#include "core/QtLogSink.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <QStandardPaths>
#include <QDir>
#include <vector>

namespace core::log
{
    void init()
    {
        // ~/.local/share/soa-launcher/launcher.log (or platform equivalent)
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        const std::string log_path = (dir + "/launcher.log").toStdString();

        std::vector<spdlog::sink_ptr> sinks;

        // OS terminal - only visible when launched from a shell. Colored.
        const auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console->set_pattern("[%H:%M:%S] [%^%l%$] %v");
        sinks.push_back(console);

        // File - full detail incl. source location, truncated each run.
        const auto file = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path, true);
        file->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v");
        sinks.push_back(file);

        // In-app WineTerminal, via the bridge.
        const auto qt = std::make_shared<QtSignalSink_mt>();
        qt->set_pattern("[%H:%M:%S] [%l] %v");
        sinks.push_back(qt);

        const auto logger = std::make_shared<spdlog::logger>("soa", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::trace);    // runtime: let everything through
        logger->flush_on(spdlog::level::trace);      // flush file on warn+
        spdlog::set_default_logger(logger);

        (void) LogBridge::instance();               // pin bridge to GUI thread

        SPDLOG_INFO("logging initialised -> {}", log_path);
    }
}