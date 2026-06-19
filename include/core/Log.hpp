#pragma once
#include <spdlog/spdlog.h>

namespace core::log
{
    void init();   // call ONCE at startup, on the GUI thread (before any logging)
}