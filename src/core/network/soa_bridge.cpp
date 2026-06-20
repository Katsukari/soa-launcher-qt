#include "core/network/soa_bridge.h"

#include <spdlog/spdlog.h>

#include "core/network/DownloadBridge.hpp"

#include <QMetaObject>
#include <QString>

// Swift -> C++ logging. Wraps the SPDLOG_* macros (which can't cross the C ABI)
// so Swift logs land in the same sinks (console + file + in-app log window)

extern "C" void soa_log(int level, const char* message)
{
    switch (level)
    {
        case 0:  SPDLOG_TRACE("[swift] {}", message); break;
        case 1:  SPDLOG_DEBUG("[swift] {}", message); break;
        case 2:  SPDLOG_INFO ("[swift] {}", message); break;
        case 3:  SPDLOG_WARN ("[swift] {}", message); break;
        case 4:  SPDLOG_ERROR("[swift] {}", message); break;
        default: SPDLOG_INFO ("[swift] {}", message); break;
    }
}

// Download callbacks (Swift -> C++)
namespace
{
    void on_progress_cb(const char* message,
                        int         percent,
                        uint64_t    received,
                        uint64_t    total,
                        uint64_t    throughput,
                        void*       /*ctx*/)
    {
        // Copy the C string into a QString now (before the pointer is freed
        // Swift-side) and hand off to the GUI thread.
        const QString msg = QString::fromUtf8(message ? message : "");
        QMetaObject::invokeMethod(
            &core::network::DownloadBridge::instance(),
            [msg, percent, received, total, throughput]()
            {
                emit core::network::DownloadBridge::instance().progress(
                    msg, percent,
                    static_cast<qulonglong>(received),
                    static_cast<qulonglong>(total),
                    static_cast<qulonglong>(throughput));
            },
            Qt::QueuedConnection);
    }

    void on_done_cb(bool ok, const char* message, void* /*ctx*/)
    {
        const QString msg = QString::fromUtf8(message ? message : "");
        QMetaObject::invokeMethod(
            &core::network::DownloadBridge::instance(),
            [ok, msg]()
            {
                emit core::network::DownloadBridge::instance().finished(ok, msg);
            },
            Qt::QueuedConnection);
    }
}

namespace core::network
{
    DownloadBridge& DownloadBridge::instance()
    {
        static DownloadBridge bridge;
        return bridge;
    }

    soa_progress_cb DownloadBridge::progress_callback()
    {
        return &on_progress_cb;
    }

    soa_done_cb DownloadBridge::done_callback()
    {
        return &on_done_cb;
    }
}