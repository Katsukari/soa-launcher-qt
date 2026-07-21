#pragma once

namespace core::system
{
    enum class GpuVendor
    {
        Amd,
        Nvidia,
        Intel,
        Unknown
    };




    struct SystemProfile
    {
        GpuVendor gpu_vendor {GpuVendor::Unknown};
        bool vulkan_likely {};
    };

    [[nodiscard]] SystemProfile detect_system_profile();
}
