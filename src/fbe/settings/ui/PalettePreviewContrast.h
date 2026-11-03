#pragma once

#include <windows.h>
#include <cmath>

namespace PalettePreviewContrast {
inline COLORREF ReadableInk(COLORREF background)
{
    const auto linear = [](BYTE value) {
        const double channel = value / 255.0;
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    const double luminance = 0.2126 * linear(GetRValue(background)) + 0.7152 * linear(GetGValue(background)) +
        0.0722 * linear(GetBValue(background));
    return luminance >= 0.1791287847 ? RGB(0,0,0) : RGB(255,255,255);
}
}
