#include "core/network/Courier.h"

#include <spdlog/spdlog.h>

#include "core/network/CourierBridge.hpp"

#include <QMetaObject>
#include <QString>

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
    void on_progress_cb(courier_phase phase,
                        const char* message,
                        int         percent,
                        uint64_t    received,
                        uint64_t    total,
                        uint64_t    throughput,
                        void*       )
    {
        const QString msg = QString::fromUtf8(message ? message : "");
        QMetaObject::invokeMethod(
            &core::network::CourierBridge::instance(),
            [phase, msg, percent, received, total, throughput]()
            {
                emit core::network::CourierBridge::instance().progress(
                    phase, msg, percent,
                    static_cast<qulonglong>(received),
                    static_cast<qulonglong>(total),
                    static_cast<qulonglong>(throughput));
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
                emit core::network::CourierBridge::instance().finished(ok, msg);
            },
            Qt::QueuedConnection);
    }
}

namespace core::network
{
    CourierBridge& CourierBridge::instance()
    {
        static CourierBridge bridge;
        return bridge;
    }

    courier_progress_cb CourierBridge::progress_callback()
    {
        return &on_progress_cb;
    }

    courier_done_cb CourierBridge::done_callback()
    {
        return &on_done_cb;
    }
}