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
        const int base = AppearancePreviewLayout::Calculate(dpi, 16, 16).totalHeight;
        const int larger = AppearancePreviewLayout::Calculate(dpi, 32, 16).totalHeight;
        RECT preview = {10, 30, 310, 30 + base}, toolbar = {0, 10, 320, 40 + base};
        RECT description = {0, 50 + base, 320, 100 + base};
        RECT label = {10, 60 + base, 100, 80 + base}, field = {110, 60 + base, 300, 80 + base};
        const RECT originalPreview = preview, originalToolbar = toolbar, originalDescription = description;
        const RECT originalLabel = label, originalField = field;
        LONG content = 110 + base; int natural = 110 + base;
        passed = AppearancePreviewLayout::ResizePage(preview, toolbar, description, label, field, content, natural, larger) &&
            preview.bottom - preview.top == larger && toolbar.bottom >= preview.bottom &&
            description.top >= toolbar.bottom && label.top >= description.top && field.top >= description.top &&
            content == 110 + larger && natural == 110 + larger && passed;
        passed = AppearancePreviewLayout::ResizePage(preview, toolbar, description, label, field, content, natural, base) &&
            ::EqualRect(&preview, &originalPreview) && ::EqualRect(&toolbar, &originalToolbar) &&
            ::EqualRect(&description, &originalDescription) && ::EqualRect(&label, &originalLabel) &&
            ::EqualRect(&field, &originalField) && content == 110 + base && natural == 110 + base && passed;
        passed = !AppearancePreviewLayout::ResizePage(preview, toolbar, description, label, field, content, natural, 0) &&
            ::EqualRect(&preview, &originalPreview) && content == 110 + base && passed;
    }
    const auto fallback = AppearancePreviewLayout::Calculate(0, 20, 24);
    const auto normal = AppearancePreviewLayout::Calculate(96, 20, 24);
    passed = fallback.totalHeight == normal.totalHeight && fallback.mainPixels == normal.mainPixels &&
        fallback.scriptPixels == normal.scriptPixels && passed;
    std::puts(passed ? "Appearance preview layout: PASS" : "Appearance preview layout: FAIL");
    return passed ? 0 : 1;
}
