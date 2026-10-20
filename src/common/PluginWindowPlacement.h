#pragma once
#include "PluginSettingsStore.h"
#include <algorithm>

namespace FbePluginUi {
inline UINT WindowDpi(HWND window) {
    using GetDpi = UINT (WINAPI*)(HWND);
    const auto getDpi = reinterpret_cast<GetDpi>(::GetProcAddress(::GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if(getDpi) return getDpi(window);
    HDC dc = ::GetDC(window); const UINT dpi = dc ? static_cast<UINT>(::GetDeviceCaps(dc, LOGPIXELSX)) : 96;
    if(dc) ::ReleaseDC(window, dc);
    return dpi ? dpi : 96;
}

// Geometry is independent of the confirmed conversion parameters. Remembering
// a cancelled window must not change their revision or overwrite newer edits.
inline HRESULT WindowPlacement(HWND window, LPCWSTR name, bool save) {
    FbePluginSettings::Store store(L"plugin-windows", L"Software\\FBETeam\\FictionBook Editor Next\\PluginWindowPlacement");
    RECT bounds = {}; if(!::GetWindowRect(window, &bounds)) return HRESULT_FROM_WIN32(::GetLastError());
    const std::wstring prefix = std::wstring(name) + L".";
    const UINT dpi = WindowDpi(window);
    if(save) {
        if(::IsIconic(window)) return S_FALSE;
        store.SetDWORDValue((prefix + L"Left").c_str(), static_cast<DWORD>(bounds.left));
        store.SetDWORDValue((prefix + L"Top").c_str(), static_cast<DWORD>(bounds.top));
        store.SetDWORDValue((prefix + L"Width").c_str(), static_cast<DWORD>(bounds.right - bounds.left));
        store.SetDWORDValue((prefix + L"Height").c_str(), static_cast<DWORD>(bounds.bottom - bounds.top));
        store.SetDWORDValue((prefix + L"Dpi").c_str(), dpi);
        return store.Commit();
    }
    auto read = [&](LPCWSTR field, DWORD fallback) { return FbePluginSettings::ReadDword(store, (prefix + field).c_str(), fallback); };
    const DWORD oldDpi = read(L"Dpi", 0), oldWidth = read(L"Width", 0), oldHeight = read(L"Height", 0);
    const bool remembered = SUCCEEDED(store.Error()) && oldDpi >= 48 && oldDpi <= 768 && oldWidth >= 100 && oldWidth <= 32768 && oldHeight >= 100 && oldHeight <= 32768;
    if(remembered) {
        bounds.left = static_cast<LONG>(read(L"Left", 0)); bounds.top = static_cast<LONG>(read(L"Top", 0));
        // Corrupt coordinates must not overflow when forming the monitor rect.
        bounds.left = (std::max)(-100000L, (std::min)(100000L, bounds.left));
        bounds.top = (std::max)(-100000L, (std::min)(100000L, bounds.top));
        bounds.right = bounds.left + ::MulDiv(static_cast<int>(oldWidth), dpi, oldDpi);
        bounds.bottom = bounds.top + ::MulDiv(static_cast<int>(oldHeight), dpi, oldDpi);
    }
    MONITORINFO monitor = {sizeof(monitor)};
    HMONITOR screen = remembered ? ::MonitorFromRect(&bounds, MONITOR_DEFAULTTONEAREST)
        : ::MonitorFromWindow(::GetParent(window) ? ::GetParent(window) : window, MONITOR_DEFAULTTONEAREST);
    if(!::GetMonitorInfoW(screen, &monitor)) return HRESULT_FROM_WIN32(::GetLastError());
    const int width = (std::min)(bounds.right - bounds.left, monitor.rcWork.right - monitor.rcWork.left);
    const int height = (std::min)(bounds.bottom - bounds.top, monitor.rcWork.bottom - monitor.rcWork.top);
    const int x = remembered ? (std::max)(monitor.rcWork.left, (std::min)(bounds.left, monitor.rcWork.right - width))
        : monitor.rcWork.left + (monitor.rcWork.right - monitor.rcWork.left - width) / 2;
    const int y = remembered ? (std::max)(monitor.rcWork.top, (std::min)(bounds.top, monitor.rcWork.bottom - height))
        : monitor.rcWork.top + (monitor.rcWork.bottom - monitor.rcWork.top - height) / 2;
    ::SetWindowPos(window, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    return store.Error();
}
inline void ReportWindowPlacement(HWND window, LPCWSTR name, bool save, LPCWSTR errorText) {
    const HRESULT hr = WindowPlacement(window, name, save);
    FbePluginSettings::ShowSaveError(window, hr, errorText, errorText);
}
}
