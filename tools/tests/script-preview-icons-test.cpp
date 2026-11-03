#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <initializer_list>

static bool Check(LPCWSTR path, UINT dpi, COLORREF background)
{
    int previousWidth = 0, previousHeight = 0;
    for(int logical : {16, 20, 24, 32}) {
        const int size = ::MulDiv(logical, static_cast<int>(dpi), 96);
        HICON icon = static_cast<HICON>(::LoadImageW(NULL, path, IMAGE_ICON, size, size, LR_LOADFROMFILE));
        HIMAGELIST images = ::ImageList_Create(size, size, ILC_COLOR32, 1, 0);
        HDC preview = ::CreateCompatibleDC(NULL), toolbar = ::CreateCompatibleDC(NULL);
        BITMAPINFO info = {}; info.bmiHeader.biSize = sizeof(info.bmiHeader); info.bmiHeader.biWidth = size;
        info.bmiHeader.biHeight = -size; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        void *previewBits = NULL, *toolbarBits = NULL;
        HBITMAP previewBitmap = preview ? ::CreateDIBSection(preview, &info, DIB_RGB_COLORS, &previewBits, NULL, 0) : NULL;
        HBITMAP toolbarBitmap = toolbar ? ::CreateDIBSection(toolbar, &info, DIB_RGB_COLORS, &toolbarBits, NULL, 0) : NULL;
        if(!icon || !images || !preview || !toolbar || !previewBitmap || !toolbarBitmap || !previewBits || !toolbarBits) {
            if(previewBitmap) ::DeleteObject(previewBitmap); if(toolbarBitmap) ::DeleteObject(toolbarBitmap);
            if(preview) ::DeleteDC(preview); if(toolbar) ::DeleteDC(toolbar);
            if(images) ::ImageList_Destroy(images); if(icon) ::DestroyIcon(icon); return false;
        }
        HGDIOBJ oldPreview = ::SelectObject(preview, previewBitmap), oldToolbar = ::SelectObject(toolbar, toolbarBitmap);
        HBRUSH brush = ::CreateSolidBrush(background);
        const RECT bounds = {0, 0, size, size}; bool valid = brush != NULL;
        if(brush) { ::FillRect(preview, &bounds, brush); ::FillRect(toolbar, &bounds, brush); ::DeleteObject(brush); }
        valid = ::DrawIconEx(preview, 0, 0, icon, size, size, 0, NULL, DI_NORMAL) && valid;
        valid = ::ImageList_AddIcon(images, icon) == 0 && ::ImageList_Draw(images, 0, toolbar, 0, 0, ILD_TRANSPARENT) && valid;
        int left = size, top = size, right = -1, bottom = -1, maxDifference = 0;
        for(int y = 0; y < size; ++y) for(int x = 0; x < size; ++x) {
            const COLORREF a = ::GetPixel(preview, x, y), b = ::GetPixel(toolbar, x, y);
            if(a != background) { left = min(left, x); top = min(top, y); right = max(right, x); bottom = max(bottom, y); }
            maxDifference = max(maxDifference, abs(static_cast<int>(GetRValue(a)) - GetRValue(b)));
            maxDifference = max(maxDifference, abs(static_cast<int>(GetGValue(a)) - GetGValue(b)));
            maxDifference = max(maxDifference, abs(static_cast<int>(GetBValue(a)) - GetBValue(b)));
        }
        const int width = right - left + 1, height = bottom - top + 1;
        valid = width > previousWidth && height > previousHeight && maxDifference <= 2 && valid;
        std::printf("dpi=%u logical=%d visible=%dx%d max-channel-difference=%d\n", dpi, logical, width, height, maxDifference);
        previousWidth = width; previousHeight = height;
        ::SelectObject(preview, oldPreview); ::SelectObject(toolbar, oldToolbar);
        ::DeleteObject(previewBitmap); ::DeleteObject(toolbarBitmap); ::DeleteDC(preview); ::DeleteDC(toolbar);
        ::ImageList_Destroy(images); ::DestroyIcon(icon);
        if(!valid) return false;
    }
    return true;
}

int wmain(int argc, wchar_t** argv)
{
    if(argc != 3) return 1;
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_BAR_CLASSES};
    if(!::InitCommonControlsEx(&controls)) return 1;
    bool passed = true;
    for(int icon = 1; icon < argc; ++icon) {
        std::printf("source-icon=%d\n", icon);
        for(COLORREF background : {RGB(29,67,101), RGB(244,239,220)}) {
            std::printf("background=%s\n", background == RGB(29,67,101) ? "dark" : "light");
            for(UINT dpi : {96U, 120U, 144U, 192U}) passed = Check(argv[icon], dpi, background) && passed;
        }
    }
    std::puts(passed ? "Script preview icons: PASS" : "Script preview icons: FAIL");
    return passed ? 0 : 1;
}
