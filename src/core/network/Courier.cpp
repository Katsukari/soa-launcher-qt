#include "core/network/Courier.h"

#include <spdlog/spdlog.h>

#include "core/network/CourierBridge.hpp"
#include "core/network/DownloadStatus.hpp"

#include <QMetaObject>
#include <QString>

using core::status::State;
using core::status::Status;
using core::network::DownloadStatus;

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

namespace
{
    const char* phase_name(courier_phase p)
    {
        switch (p)
        {
            case courier_phase_preparing:   return "preparing";
            case courier_phase_checking:    return "checking";
            case courier_phase_downloading: return "downloading";
            case courier_phase_verifying:   return "verifying";
        }
        return "preparing";
    }

    void on_progress_cb(courier_phase phase,
                        const char* message,
                        int         percent,
                        uint64_t    received,
                        uint64_t    total,
                        uint64_t    throughput,
                        int         file_index,
                        int         file_count,
                        void*       )
    {
        const QString msg = QString::fromUtf8(message ? message : "");
        QMetaObject::invokeMethod(
            &core::network::CourierBridge::instance(),
            [phase, msg, percent, received, total, throughput, file_index, file_count]()
            {
                DownloadStatus ds;
                ds.base.state    = State::Working;
                ds.base.phase    = phase_name(phase);
                ds.base.message  = msg;
                ds.base.progress = percent / 100.0;
                ds.phase      = phase;
                ds.received   = static_cast<qulonglong>(received);
                ds.total      = static_cast<qulonglong>(total);
                ds.speed      = static_cast<qulonglong>(throughput);
                ds.file_index = file_index;
                ds.file_count = file_count;
                core::network::CourierBridge::instance().report(ds);
            },
            Qt::QueuedConnection);
    }

    void on_done_cb(bool ok, const char* message, void* )
    {
        const QString msg = QString::fromUtf8(message ? message : "");
        QMetaObject::invokeMethod(
            &core::network::CourierBridge::instance(),
            [ok, msg]()
            {
                DownloadStatus ds;
                ds.base.state    = ok ? State::Done : State::Failed;
                ds.base.message  = msg;
                ds.base.progress = ok ? 1.0 : -1.0;
                core::network::CourierBridge::instance().report(ds);
            },
            Qt::QueuedConnection);
    }
}

namespace core::network
{
    CourierBridge::CourierBridge() : StatusReporter("courier") {}

    CourierBridge& CourierBridge::instance()
    {
        static CourierBridge bridge;
        return bridge;
    }

    void CourierBridge::report(const DownloadStatus& ds)
    {
        set_status(ds.base);
        emit download_status(ds);
    }

    courier_progress_cb CourierBridge::progress_callback()
    {
        return &on_progress_cb;
    }

    courier_done_cb CourierBridge::done_callback()
    {
        return & on_done_cb;
    }
}