#pragma once

#include "core/network/Courier.h"
#include "core/network/DownloadStatus.hpp"
#include "core/status/StatusReporter.hpp"

#include <QString>
#include <QSet>

namespace core::network
{
    class CourierBridge : public status::StatusReporter
    {
        Q_OBJECT

        public:
            static CourierBridge& instance();

            static courier_progress_cb progress_callback();
            static courier_done_cb     done_callback();

            void begin_operation(qulonglong operation_id);
            void clear_operation(qulonglong operation_id = 0);
            [[nodiscard]] bool has_operation(qulonglong operation_id) const { return active_operations.contains(operation_id); }
            void report(const DownloadStatus& ds);

        signals:
            void download_status(const DownloadStatus& ds);

        private:
            CourierBridge();
            CourierBridge(const CourierBridge&)            = delete;
            CourierBridge& operator=(const CourierBridge&) = delete;

            QSet<qulonglong> active_operations;
    };
}
