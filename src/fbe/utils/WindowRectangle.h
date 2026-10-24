#pragma once

#include <windows.h>
#include <cstdint>
#include <climits>

namespace WindowRectangle {
// WTL's default RECT is {CW_USEDEFAULT, CW_USEDEFAULT, 0, 0}.
// Win32 expects the resulting width/height to retain CW_USEDEFAULT's bits.
// Express the legacy 32-bit wrap explicitly, without signed overflow.
inline int Span(LONG end, LONG begin) noexcept
{
    static_assert(sizeof(LONG) == 4 && sizeof(int) == 4, "Win32 coordinate size");
    std::int64_t value = static_cast<std::int64_t>(end) - begin;
    if (value > INT_MAX) value -= (std::int64_t{1} << 32);
    if (value < INT_MIN) value += (std::int64_t{1} << 32);
    return static_cast<int>(value);
}
}
