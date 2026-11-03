#include "../../src/fbe/settings/ui/PalettePreviewContrast.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

static double Luminance(COLORREF color)
{
    auto channel = [](BYTE c) {
        const double v = c / 255.0;
        return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(GetRValue(color)) + 0.7152 * channel(GetGValue(color)) + 0.0722 * channel(GetBValue(color));
}

static bool Check(COLORREF background)
{
    const COLORREF foreground = PalettePreviewContrast::ReadableInk(background);
    const double a = Luminance(background), b = Luminance(foreground);
    const double ratio = ((std::max)(a, b) + 0.05) / ((std::min)(a, b) + 0.05);
    return (foreground == RGB(0,0,0) || foreground == RGB(255,255,255)) && ratio >= 4.5;
}

int main()
{
    bool passed = true;
    for(int r = 0; r <= 255; r += 15) for(int g = 0; g <= 255; g += 15) for(int b = 0; b <= 255; b += 15)
        passed = Check(RGB(r,g,b)) && passed;
    for(int gray = 0; gray <= 255; ++gray) passed = Check(RGB(gray,gray,gray)) && passed;
    std::puts(passed ? "Palette preview label contrast: PASS" : "Palette preview label contrast: FAIL");
    return passed ? 0 : 1;
}
