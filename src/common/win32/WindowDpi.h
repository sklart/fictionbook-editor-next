#pragma once

#include <windows.h>

namespace FbeWin32
{
inline UINT WindowDpi(HWND window)
{
    using GetDpi = UINT (WINAPI*)(HWND);
    static const auto getDpi = reinterpret_cast<GetDpi>(
        ::GetProcAddress(::GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (getDpi && window) {
        if (const UINT dpi = getDpi(window)) return dpi;
    }
    HDC dc = ::GetDC(window);
    const int dpi = dc ? ::GetDeviceCaps(dc, LOGPIXELSX) : 0;
    if (dc) ::ReleaseDC(window, dc);
    return dpi > 0 ? static_cast<UINT>(dpi) : 96;
}
}
