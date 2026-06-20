#pragma once

#include "core/network/soa_bridge.h"

#include <QObject>
#include <QString>

namespace core::network
{
    class DownloadBridge : public QObject
    {
        Q_OBJECT

    public:
        static DownloadBridge& instance();

        static soa_progress_cb progress_callback();
        static soa_done_cb     done_callback();

        signals:
            void progress(const QString& message,
                          int            percent,
                          qulonglong     received,
                          qulonglong     total,
                          qulonglong     throughput);

        void finished(bool ok, const QString& message);

    private:
        DownloadBridge() = default;
        DownloadBridge(const DownloadBridge&)            = delete;
        DownloadBridge& operator=(const DownloadBridge&) = delete;
    };
}