#pragma once

#include "core/status/Status.hpp"
#include "core/network/Courier.h"

namespace core::network
{
    struct DownloadStatus
    {
        qulonglong           operation_id {0};
        status::Status       base;
        courier_phase        phase      {courier_phase_preparing};
        qulonglong           received   {0};
        qulonglong           total      {0};
        qulonglong           speed      {0};
        int                  file_index {0};
        int                  file_count {0};
    };
}
