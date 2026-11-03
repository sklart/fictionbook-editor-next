#pragma once

#include <windows.h>
#include <vector>

namespace PalettePreviewClip {
struct Part { RECT rect; int role; LPCWSTR text; int background; };

inline RECT TextBounds(HDC dc, const Part& part)
{
    TEXTMETRICW metric = {}; ::GetTextMetricsW(dc, &metric);
    const int top = (part.rect.top + part.rect.bottom - metric.tmHeight) / 2;
    return {part.rect.left + 3, top - 1, part.rect.right, top + metric.tmHeight + 1};
}

inline void Apply(HDC dc, const RECT& client, const std::vector<Part>& parts, int index)
{
    ::IntersectClipRect(dc, client.left, client.top, client.right, client.bottom);
    for(int i = index + 1; i < static_cast<int>(parts.size()); ++i)
        ::ExcludeClipRect(dc, parts[i].rect.left, parts[i].rect.top, parts[i].rect.right, parts[i].rect.bottom);
    if(*parts[index].text) {
        const RECT text = TextBounds(dc, parts[index]);
        ::ExcludeClipRect(dc, text.left, text.top, text.right, text.bottom);
    }
}
}
