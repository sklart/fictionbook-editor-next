#include "../../src/fbe/toolbars/ToolbarBitmapScaling.h"
#include <commctrl.h>
#include <cstdio>
#include <initializer_list>

static bool Check(HBITMAP source, UINT dpi, bool report = true)
{
    int previousWidth[6] = {}, previousHeight[6] = {};
    for(int logical : {16, 20, 24, 32}) {
        const int size = ::MulDiv(logical, static_cast<int>(dpi), 96);
        HIMAGELIST list = ::ImageList_Create(size, size, ILC_COLOR32, 6, 0);
        HDC dc = ::CreateCompatibleDC(NULL);
        BITMAPINFO info = {}; info.bmiHeader.biSize = sizeof(info.bmiHeader); info.bmiHeader.biWidth = size;
        info.bmiHeader.biHeight = -size; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        void* bits = NULL; HBITMAP canvas = dc ? ::CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, NULL, 0) : NULL;
        if(!list || !dc || !canvas || !bits) {
            if(canvas) ::DeleteObject(canvas); if(dc) ::DeleteDC(dc); if(list) ::ImageList_Destroy(list); return false;
        }
        HGDIOBJ previous = ::SelectObject(dc, canvas); bool valid = true; int growing = 0;
        for(int icon = 0; icon < 6 && valid; ++icon) {
            HBITMAP alpha = ToolbarBitmapScaling::CreateAlphaBitmapCell(source, 24, icon);
            HBITMAP scaled = ToolbarBitmapScaling::ScaleHighResolutionAlphaBitmap(alpha, 24, size);
            DIBSECTION bitmap = {};
            if(!scaled || ::GetObjectW(scaled, sizeof(bitmap), &bitmap) != sizeof(bitmap) || !bitmap.dsBm.bmBits) {
                if(scaled) ::DeleteObject(scaled); valid = false; break;
            }
            const DWORD* data = static_cast<const DWORD*>(bitmap.dsBm.bmBits);
            int transparent = 0, visible = 0;
            for(int pixel = 0; pixel < size * size; ++pixel) {
                const DWORD value = data[pixel], a = value >> 24;
                if(a == 0) ++transparent; else ++visible;
                if(((value >> 16) & 255) > a || ((value >> 8) & 255) > a || (value & 255) > a) valid = false;
            }
            valid = visible > 0 && (dpi != 96 || logical != 24 || transparent > 0) && valid;
            if(!valid) std::printf("dpi=%u size=%d icon=%d alpha visible=%d transparent=%d\n", dpi, logical, icon, visible, transparent);
            const int added = ::ImageList_Add(list, scaled, NULL); ::DeleteObject(scaled);
            if(added != icon) { valid = false; break; }
            const COLORREF background = RGB(29,67,101); HBRUSH brush = ::CreateSolidBrush(background);
            if(!brush) { valid = false; break; }
            const RECT bounds = {0, 0, size, size}; ::FillRect(dc, &bounds, brush); ::DeleteObject(brush);
            if(!::ImageList_Draw(list, icon, dc, 0, 0, ILD_TRANSPARENT)) { valid = false; break; }
            int left = size, top = size, right = -1, bottom = -1;
            for(int y = 0; y < size; ++y) for(int x = 0; x < size; ++x) if(::GetPixel(dc, x, y) != background) {
                left = min(left, x); top = min(top, y); right = max(right, x); bottom = max(bottom, y);
            }
            const int width = right - left + 1, height = bottom - top + 1;
            if(report && icon == 0) std::printf("dpi=%u logical=%d visible=%dx%d cell=%d\n", dpi, logical, width, height, size);
            if(icon == 0 && dpi == 96 && logical == 24 && width >= size) valid = false;
            if(width <= 0 || height <= 0 || width < previousWidth[icon] || height < previousHeight[icon]) valid = false;
            if(width > previousWidth[icon] && height > previousHeight[icon]) ++growing;
            previousWidth[icon] = width; previousHeight[icon] = height;
        }
        int width = 0, height = 0;
        valid = ::ImageList_GetIconSize(list, &width, &height) && width == size && height == size && growing >= 4 && valid;
        ::SelectObject(dc, previous); ::DeleteObject(canvas); ::DeleteDC(dc); ::ImageList_Destroy(list);
        if(!valid) { std::printf("dpi=%u size=%d growth=%d list=%dx%d\n", dpi, logical, growing, width, height); return false; }
    }
    return true;
}

int wmain(int argc, wchar_t** argv)
{
    if(argc != 2) return 1;
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_BAR_CLASSES};
    if(!::InitCommonControlsEx(&controls)) return 1;
    HBITMAP source = static_cast<HBITMAP>(::LoadImageW(NULL, argv[1], IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION));
    if(!source) return 1;
    bool passed = true;
    for(UINT dpi : {96U, 120U, 144U, 192U}) passed = Check(source, dpi) && passed;
    const DWORD gdiBefore = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    const DWORD userBefore = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    for(int iteration = 0; iteration < 20; ++iteration) passed = Check(source, 144, false) && passed;
    const DWORD gdiAfter = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    const DWORD userAfter = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    passed = gdiAfter <= gdiBefore && userAfter <= userBefore && passed;
    std::printf("offscreen-resource-count gdi=%lu:%lu user=%lu:%lu\n", gdiBefore, gdiAfter, userBefore, userAfter);
    ::DeleteObject(source);
    std::puts(passed ? "Toolbar bitmap scaling: PASS" : "Toolbar bitmap scaling: FAIL");
    return passed ? 0 : 1;
}
