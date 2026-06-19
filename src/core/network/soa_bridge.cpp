#include "core/network/soa_bridge.h"
#include "core/Log.hpp"
#include <spdlog/spdlog.h>

extern "C" void soa_log(int level, const char* message)
{
    switch (level)
    {
        case 0: SPDLOG_TRACE("[swift] {}", message); break;
        case 1: SPDLOG_DEBUG("[swift] {}", message); break;
        case 2: SPDLOG_INFO ("[swift] {}", message); break;
        case 3: SPDLOG_WARN ("[swift] {}", message); break;
        case 4: SPDLOG_ERROR("[swift] {}", message); break;
        default: SPDLOG_INFO("[swift] {}", message); break;
    }
}