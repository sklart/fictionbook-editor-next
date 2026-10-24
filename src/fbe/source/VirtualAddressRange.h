#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>

namespace FbeSourceDiagnostics {
// VirtualQuery regions describe address-space ranges, not C++ array objects.
inline bool NextVirtualRegion(std::uintptr_t base, std::size_t size,
    std::uintptr_t current, std::uintptr_t& next) noexcept
{
    if (size > (std::numeric_limits<std::uintptr_t>::max)() - base) return false;
    const std::uintptr_t candidate = base + size;
    if (candidate <= current) return false;
    next = candidate;
    return true;
}
}
