#pragma once

#include "core/network/Courier.h"
#include "core/network/DownloadStatus.hpp"
#include "core/status/StatusReporter.hpp"

#include <QString>

namespace core::network
{
    class CourierBridge : public status::StatusReporter
    {
        Q_OBJECT

        public:
            static CourierBridge& instance();

            static courier_progress_cb progress_callback();
            static courier_done_cb     done_callback();

            void report(const DownloadStatus& ds);

            signals:
                void download_status(const DownloadStatus& ds);

        private:
            CourierBridge();
            CourierBridge(const CourierBridge&)            = delete;
            CourierBridge& operator=(const CourierBridge&) = delete;
    };
}