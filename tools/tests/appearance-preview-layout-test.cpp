#include "../../src/fbe/settings/ui/AppearancePreviewLayout.h"
#include <cstdio>
#include <initializer_list>

int main()
{
    bool passed = true;
    const int sizes[] = {16, 20, 24, 32};
    for(UINT dpi : {96U, 120U, 144U, 192U}) {
        for(int mainSize : sizes) for(int scriptSize : sizes) {
            const auto layout = AppearancePreviewLayout::Calculate(dpi, mainSize, scriptSize);
            const auto changedMain = AppearancePreviewLayout::Calculate(dpi, 32, scriptSize);
            const auto changedScript = AppearancePreviewLayout::Calculate(dpi, mainSize, 32);
            passed = layout.mainPixels == ::MulDiv(mainSize, static_cast<int>(dpi), 96) &&
                layout.scriptPixels == ::MulDiv(scriptSize, static_cast<int>(dpi), 96) &&
                layout.mainRowHeight > layout.mainPixels && layout.scriptRowHeight > layout.scriptPixels &&
                layout.totalHeight == layout.menuHeight + layout.mainRowHeight + layout.scriptRowHeight + 1 &&
                changedMain.scriptRowHeight == layout.scriptRowHeight && changedScript.mainRowHeight == layout.mainRowHeight &&
                (mainSize == 32 || changedMain.mainRowHeight > layout.mainRowHeight) &&
                (scriptSize == 32 || changedScript.scriptRowHeight > layout.scriptRowHeight) && passed;
        }
    }
    const auto fallback = AppearancePreviewLayout::Calculate(0, 20, 24);
    const auto normal = AppearancePreviewLayout::Calculate(96, 20, 24);
    passed = fallback.totalHeight == normal.totalHeight && fallback.mainPixels == normal.mainPixels &&
        fallback.scriptPixels == normal.scriptPixels && passed;
    std::puts(passed ? "Appearance preview layout: PASS" : "Appearance preview layout: FAIL");
    return passed ? 0 : 1;
}
