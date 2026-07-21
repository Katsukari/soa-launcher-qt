#pragma once

#include <QString>

namespace core::system
{
    enum class GpuVendor
    {
        Amd,
        Nvidia,
        Intel,
        Apple,
        Unknown
    };

    struct SystemProfile
    {
        QString os_name;
        QString cpu_name;
        QString gpu_name;
        GpuVendor gpu_vendor {GpuVendor::Unknown};
        bool vulkan_likely {};
    };

    [[nodiscard]] SystemProfile detect_system_profile();
    [[nodiscard]] QString gpu_vendor_name(GpuVendor vendor);
}
