#pragma once

#include <windows.h>

namespace AppearancePreviewLayout {
struct Metrics {
    int menuHeight;
    int mainPixels;
    int scriptPixels;
    int mainRowHeight;
    int scriptRowHeight;
    int totalHeight;
};

inline Metrics Calculate(UINT dpi, int mainSize, int scriptSize)
{
    const int effectiveDpi = static_cast<int>(dpi ? dpi : 96);
    const auto scale = [effectiveDpi](int value) { return ::MulDiv(value, effectiveDpi, 96); };
    Metrics result = {};
    result.menuHeight = scale(22);
    result.mainPixels = scale(mainSize);
    result.scriptPixels = scale(scriptSize);
    const int padding = scale(10);
    result.mainRowHeight = result.mainPixels + padding;
    result.scriptRowHeight = result.scriptPixels + padding;
    // One pixel at the bottom belongs to the outer preview border.
    result.totalHeight = result.menuHeight + result.mainRowHeight + result.scriptRowHeight + 1;
    return result;
}

inline bool ResizePage(RECT& preview, RECT& toolbarGroup, RECT& descriptionGroup,
    RECT& descriptionLabel, RECT& descriptionField, LONG& contentHeight, int& naturalHeight, int desiredHeight)
{
    if(desiredHeight <= 0 || preview.bottom <= preview.top || toolbarGroup.bottom < preview.bottom) return false;
    const int delta = desiredHeight - (preview.bottom - preview.top);
    if(contentHeight + delta < 0 || naturalHeight + delta < 0) return false;
    preview.bottom += delta;
    toolbarGroup.bottom += delta;
    ::OffsetRect(&descriptionGroup, 0, delta);
    ::OffsetRect(&descriptionLabel, 0, delta);
    ::OffsetRect(&descriptionField, 0, delta);
    contentHeight += delta;
    naturalHeight += delta;
    return true;
}
}
