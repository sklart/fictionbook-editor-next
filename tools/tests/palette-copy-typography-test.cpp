#include <windows.h>
#include <cstdlib>
#include <cwchar>
#include <initializer_list>
#include <cstdio>

static bool Fits(HDC dc, LPCWSTR text, int width, int height, int inset)
{
    RECT measured = {0, 0, 0, 0};
    return ::DrawTextW(dc, text, -1, &measured, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX) > 0 &&
        measured.right <= width - inset && measured.bottom <= height;
}

int wmain(int argc, wchar_t** argv)
{
    if(argc != 11) return 1;
    const int promptWidth = _wtoi(argv[5]), promptHeight = _wtoi(argv[6]);
    const int buttonWidth = _wtoi(argv[7]), buttonHeight = _wtoi(argv[8]), pointSize = _wtoi(argv[10]);
    if(promptWidth <= 0 || promptHeight <= 0 || buttonWidth <= 0 || buttonHeight <= 0 || pointSize <= 0) return 1;
    HDC dc = ::CreateCompatibleDC(NULL); if(!dc) return 1;
    bool passed = true;
    for(UINT dpi : {96U, 120U, 144U, 192U}) {
        HFONT font = ::CreateFontW(-::MulDiv(pointSize, static_cast<int>(dpi), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, argv[9]);
        if(!font) { passed = false; continue; }
        HGDIOBJ oldFont = ::SelectObject(dc, font); TEXTMETRICW metric = {}; SIZE alphabet = {};
        const wchar_t letters[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        const bool measured = ::GetTextMetricsW(dc, &metric) && ::GetTextExtentPoint32W(dc, letters, 52, &alphabet);
        const int baseX = (alphabet.cx / 26 + 1) / 2;
        const int inset = ::MulDiv(8, static_cast<int>(dpi), 96);
        const bool fits = measured && baseX > 0 && metric.tmHeight > 0 &&
            Fits(dc, argv[2], ::MulDiv(promptWidth, baseX, 4), ::MulDiv(promptHeight, metric.tmHeight, 8), 0) &&
            Fits(dc, argv[3], ::MulDiv(buttonWidth, baseX, 4), ::MulDiv(buttonHeight, metric.tmHeight, 8), inset) &&
            Fits(dc, argv[4], ::MulDiv(buttonWidth, baseX, 4), ::MulDiv(buttonHeight, metric.tmHeight, 8), inset);
        if(!fits) std::wprintf(L"Typography does not fit: %ls at %u DPI\n", argv[1], dpi);
        passed = fits && passed;
        if(oldFont) ::SelectObject(dc, oldFont); ::DeleteObject(font);
    }
    ::DeleteDC(dc);
    return passed ? 0 : 1;
}
