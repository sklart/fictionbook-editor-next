#include "../../src/fbe/settings/ui/PalettePreviewClip.h"
#include <cstdio>

static bool CheckDpi(UINT dpi)
{
    HDC dc = ::CreateCompatibleDC(NULL);
    auto scale = [dpi](int value) { return ::MulDiv(value, static_cast<int>(dpi), 96); };
    BITMAPINFO info = {}; info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = scale(100); info.bmiHeader.biHeight = -scale(80);
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    void* pixels = NULL;
    HBITMAP bitmap = dc ? ::CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, NULL, 0) : NULL;
    if(!dc || !bitmap || !pixels) { if(bitmap) ::DeleteObject(bitmap); if(dc) ::DeleteDC(dc); return false; }
    HGDIOBJ previous = ::SelectObject(dc, bitmap);
    HFONT font = ::CreateFontW(-::MulDiv(8, static_cast<int>(dpi), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"MS Shell Dlg 2");
    HGDIOBJ oldFont = font ? ::SelectObject(dc, font) : NULL;
    const RECT client = {0, 0, scale(100), scale(80)};
    const std::vector<PalettePreviewClip::Part> parts = {
        {{0, 0, scale(100), scale(80)}, 0, L"Label", 0},
        {{scale(30), 0, scale(50), scale(12)}, 1, L"Child", 0},
        {{0, scale(30), scale(20), scale(50)}, 2, L"", 0}
    };
    ::FillRect(dc, &client, static_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH)));
    const int saved = ::SaveDC(dc);
    bool passed = saved != 0 && font != NULL;
    if(saved) {
        PalettePreviewClip::Apply(dc, client, parts, 0);
        const RECT text = PalettePreviewClip::TextBounds(dc, parts[0]);
        passed = !::RectVisible(dc, &parts[1].rect) && !::RectVisible(dc, &parts[2].rect) &&
            !::RectVisible(dc, &text) && !::PtVisible(dc, -1, 0) && !::PtVisible(dc, client.right, client.bottom - 1) && passed;
        ::FrameRect(dc, &parts[0].rect, static_cast<HBRUSH>(::GetStockObject(BLACK_BRUSH)));
        ::RestoreDC(dc, saved);
        passed = ::GetPixel(dc, scale(10), 0) == RGB(0,0,0) && ::GetPixel(dc, scale(35), 0) == RGB(255,255,255) &&
            ::GetPixel(dc, 0, scale(35)) == RGB(255,255,255) && passed;
    }
    if(oldFont) ::SelectObject(dc, oldFont); if(font) ::DeleteObject(font);
    ::SelectObject(dc, previous); ::DeleteObject(bitmap); ::DeleteDC(dc);
    return passed;
}

int main()
{
    bool passed = true;
    for(UINT dpi : {96U, 120U, 144U, 192U}) passed = CheckDpi(dpi) && passed;
    std::puts(passed ? "Palette preview clipping: PASS" : "Palette preview clipping: FAIL");
    return passed ? 0 : 1;
}
