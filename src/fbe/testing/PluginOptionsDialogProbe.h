#pragma once
#include <windows.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <map>
#include <string>
#include <vector>
#include "../../common/PluginOptionsTabs.h"
#include "../../common/PluginWindowPlacement.h"


namespace PluginOptionsProbe {
// The standalone COM host has native controls; only the editor knows its
// current theme. Supply expectations without linking editor internals here.
struct Palette {
    bool dark, highContrast;
    COLORREF text, disabledText, control;
    UINT dpi;
};
inline Palette NativePalette(HWND window) {
    HIGHCONTRASTW contrast = {sizeof(contrast)};
    ::SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
    return {false, (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0,
        ::GetSysColor(COLOR_WINDOWTEXT), ::GetSysColor(COLOR_GRAYTEXT),
        ::GetSysColor(COLOR_BTNFACE), FbePluginUi::WindowDpi(window)};
}
inline Palette (*paletteForWindow)(HWND) = NativePalette;
enum class Action { Inspect, EditSave, EditCancel, DefaultsCancel, PresetDefault, PresetCompat, PresetRich, ReadOnly, Conflict, Corrupt, PositionDefault, PositionMove, PositionVerify };
inline int plugin = 0, failedLine = 0, stage = 0, pages = 0;
inline Action action = Action::Inspect;
inline bool PositionProbe() { return action == Action::PositionDefault || action == Action::PositionMove || action == Action::PositionVerify; }
inline HWND dialog = NULL, errorDialog = NULL, editedControl = NULL;
inline LRESULT editedValue = 0;
inline std::wstring captures, prefix, profile;
inline std::wstring expectedTitle, expectedConfirmText;
inline std::wstring expectedCancelText;
inline bool expectedDarkTabs = false;
inline std::vector<BYTE> beforeFile;
inline DWORD originalAttributes = INVALID_FILE_ATTRIBUTES;
inline bool errorSeen = false;
inline std::wstring errorText;
inline std::map<HWND, std::wstring> editedSnapshot;
inline BYTE savedKeys[256] = {};
inline bool (*peerSave)() = NULL;
inline HHOOK initializationHook = NULL;
inline HHOOK keyboardHook = NULL;
inline int keyboardStage = 0;
inline bool keyboardInjected = false;
inline std::vector<HWND> initializedDialogs;
inline HWND ConfirmationButton(HWND window) {
    HWND button = ::GetDlgItem(window, IDOK);
    return button ? button : ::GetDlgItem(window, IDCANCEL);
}
inline bool CloseError(HWND window) {
    if(!dialog || window == dialog || ::IsWindowEnabled(dialog) || ::GetWindow(window, GW_OWNER) != dialog) return false;
    const HWND button = ConfirmationButton(window); if(!button) return false;
    errorSeen = true; errorDialog = window;
    if(errorText.empty()) for(HWND child = ::GetWindow(window, GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT)) {
        wchar_t kind[32] = {}, text[2048] = {}; ::GetClassNameW(child, kind, _countof(kind));
        if(::lstrcmpiW(kind, L"Static") == 0) { ::GetWindowTextW(child, text, _countof(text)); errorText += text; errorText += L'\n'; }
    }
    ::PostMessageW(window, WM_COMMAND, static_cast<WPARAM>(::GetDlgCtrlID(button)), reinterpret_cast<LPARAM>(button));
    return true;
}
inline LRESULT CALLBACK Initialized(int code, WPARAM wp, LPARAM lp) {
    if(code >= 0) {
        const auto* message = reinterpret_cast<const CWPRETSTRUCT*>(lp);
        if(message->message == WM_INITDIALOG) { initializedDialogs.push_back(message->hwnd); CloseError(message->hwnd); }
    }
    return ::CallNextHookEx(initializationHook, code, wp, lp);
}
inline LRESULT CALLBACK KeyboardMessage(int code, WPARAM wp, LPARAM lp) {
    if(code >= 0 && wp == PM_REMOVE) {
        const auto* message = reinterpret_cast<const MSG*>(lp);
        if(message->message == WM_KEYDOWN && message->wParam == VK_TAB && stage >= 10 && stage <= 13 &&
            dialog && ::IsChild(dialog, message->hwnd)) {
            BYTE keys[256]; memcpy(keys, savedKeys, sizeof(keys));
            keys[VK_CONTROL] = stage <= 11 ? 0x80 : 0;
            keys[VK_SHIFT] = stage == 11 || stage == 13 ? 0x80 : 0;
            ::SetKeyboardState(keys); keyboardStage = stage; keyboardInjected = true;
        }
    }
    return ::CallNextHookEx(keyboardHook, code, wp, lp);
}
inline void Stop() {
    if(initializationHook) ::UnhookWindowsHookEx(initializationHook);
    if(keyboardHook) ::UnhookWindowsHookEx(keyboardHook);
    initializationHook = keyboardHook = NULL;
    if(keyboardInjected) ::SetKeyboardState(savedKeys);
    keyboardInjected = false;
}

inline std::vector<BYTE> Read(const std::wstring& path) {
    HANDLE file = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if(file == INVALID_HANDLE_VALUE) return {};
    const DWORD length = ::GetFileSize(file, NULL); std::vector<BYTE> bytes(length); DWORD read = 0;
    if(length) ::ReadFile(file, bytes.data(), length, &read, NULL);
    ::CloseHandle(file); if(read != length) return {}; return bytes;
}
inline bool Write(const std::wstring& path, const std::vector<BYTE>& bytes) {
    HANDLE file = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if(file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0; const BOOL result = ::WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, NULL);
    ::FlushFileBuffers(file); ::CloseHandle(file); return result && written == bytes.size();
}
inline HWND Tab(HWND parent) {
    for(HWND child = ::GetWindow(parent, GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT)) {
        wchar_t kind[64] = {}; ::GetClassNameW(child, kind, _countof(kind));
        if(::lstrcmpiW(kind, WC_TABCONTROLW) == 0) return child;
        if(HWND found = Tab(child)) return found;
    }
    return NULL;
}
inline bool Checkbox(HWND control) {
    wchar_t kind[32] = {}; ::GetClassNameW(control, kind, _countof(kind));
    return ::lstrcmpiW(kind, L"Button") == 0 && ((::GetWindowLongW(control, GWL_STYLE) & BS_TYPEMASK) == BS_AUTOCHECKBOX);
}
inline void Collect(HWND parent, std::map<HWND, std::wstring>& values) {
    for(HWND child = ::GetWindow(parent, GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT)) {
        wchar_t kind[32] = {}, text[4096] = {}; ::GetClassNameW(child, kind, _countof(kind));
        if(Checkbox(child)) values[child] = std::to_wstring(::SendMessageW(child, BM_GETCHECK, 0, 0));
        else if(::lstrcmpiW(kind, L"Edit") == 0 || ::lstrcmpiW(kind, L"ComboBox") == 0) {
            ::GetWindowTextW(child, text, _countof(text)); values[child] = text;
        }
        Collect(child, values);
    }
}
inline std::map<HWND, std::wstring> Snapshot(HWND window) { std::map<HWND, std::wstring> values; Collect(window, values); return values; }
inline bool Check(bool success, int line) { if(!success && !failedLine) failedLine = line; return success; }
#define FBE_OPTIONS_CHECK(value) PluginOptionsProbe::Check((value), __LINE__)

inline bool Capture(HWND window, const std::wstring& name) {
    if(captures.empty()) return true;
    const Palette palette = paletteForWindow(window);
    RECT bounds = {}; ::GetWindowRect(window, &bounds);
    const int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
    HDC source = ::GetWindowDC(window), memory = ::CreateCompatibleDC(source);
    HBITMAP bitmap = ::CreateCompatibleBitmap(source, width, height);
    if(!source || !memory || !bitmap) return false;
    HGDIOBJ old = ::SelectObject(memory, bitmap);
    ::RedrawWindow(window, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    const BOOL copied = ::PrintWindow(window, memory, 0); ::GdiFlush();
    bool visibleContent=true;wchar_t windowClass[64]={};::GetClassNameW(window,windowClass,_countof(windowClass));
    if(!wcscmp(windowClass,L"ToolbarWindow32")) {
        const COLORREF background=::GetPixel(memory,width-1,height-1);unsigned ink=0;
        for(int y=0;y<height&&ink<100;++y)for(int x=0;x<(std::min)(width,1200)&&ink<100;++x) {
            const COLORREF pixel=::GetPixel(memory,x,y);
            if(pixel!=CLR_INVALID&&abs(static_cast<int>(GetRValue(pixel))-GetRValue(background))+abs(static_cast<int>(GetGValue(pixel))-GetGValue(background))+abs(static_cast<int>(GetBValue(pixel))-GetBValue(background))>80)++ink;
        }
        visibleContent=ink>=100;
    }
    if(expectedDarkTabs) {
        const COLORREF footer = ::GetPixel(memory, width - 20, height - 20);
        FBE_OPTIONS_CHECK(footer != CLR_INVALID && GetRValue(footer) < 128 && GetGValue(footer) < 128 && GetBValue(footer) < 128);
        HWND tab = Tab(window); RECT r = {}, client = {}; ::GetWindowRect(tab, &r); ::GetClientRect(tab, &client);
        for(POINT point : {POINT{1, client.bottom / 2}, POINT{client.right - 2, 2}}) {
            const COLORREF pixel = ::GetPixel(memory, r.left - bounds.left + point.x, r.top - bounds.top + point.y);
            FBE_OPTIONS_CHECK(pixel != CLR_INVALID && GetRValue(pixel) < 128 && GetGValue(pixel) < 128 && GetBValue(pixel) < 128);
        }
        std::vector<HWND> controls;
        ::EnumChildWindows(window,[](HWND child,LPARAM state)->BOOL {reinterpret_cast<std::vector<HWND>*>(state)->push_back(child);return TRUE;},reinterpret_cast<LPARAM>(&controls));
        for(HWND control:controls) if(::IsWindowVisible(control)&&Checkbox(control)&&::GetWindowTextLengthW(control)>0) {
            RECT controlRect={},clip={};::GetWindowRect(control,&controlRect);::GetClientRect(::GetParent(control),&clip);::MapWindowPoints(::GetParent(control),NULL,reinterpret_cast<POINT*>(&clip),2);
            if(controlRect.top<clip.top||controlRect.bottom>clip.bottom||controlRect.left<clip.left||controlRect.right>clip.right) continue;
            // Inspect rendered caption pixels, excluding the check glyph. An
            // HDC colour query alone misses Win10's themed black checkbox text.
            const COLORREF expected=::IsWindowEnabled(control)?palette.text:palette.disabledText;
            bool foreground=false;
            const int glyph=::MulDiv(22,static_cast<int>(palette.dpi),96);
            for(int y=controlRect.top;y<controlRect.bottom&&!foreground;++y)for(int x=controlRect.left+glyph;x<controlRect.right;++x) {
                const COLORREF pixel=::GetPixel(memory,x-bounds.left,y-bounds.top);
                if(pixel!=CLR_INVALID&&abs(static_cast<int>(GetRValue(pixel))-GetRValue(expected))<24&&abs(static_cast<int>(GetGValue(pixel))-GetGValue(expected))<24&&abs(static_cast<int>(GetBValue(pixel))-GetBValue(expected))<24){foreground=true;break;}
            }
            FBE_OPTIONS_CHECK(foreground);
        }
        for(HWND control:controls) if(::IsWindowVisible(control)&&!::IsWindowEnabled(control)&&::GetWindowTextLengthW(control)>0) {
            wchar_t kind[32]={};::GetClassNameW(control,kind,_countof(kind));if(wcscmp(kind,L"Static"))continue;
            RECT controlRect={},clip={};::GetWindowRect(control,&controlRect);::GetClientRect(::GetParent(control),&clip);::MapWindowPoints(::GetParent(control),NULL,reinterpret_cast<POINT*>(&clip),2);
            if(controlRect.top<clip.top||controlRect.bottom>clip.bottom||controlRect.left<clip.left||controlRect.right>clip.right)continue;
            bool foreground=false;const COLORREF expected=palette.disabledText;
            for(int y=controlRect.top;y<controlRect.bottom&&!foreground;++y)for(int x=controlRect.left;x<controlRect.right;++x){const COLORREF pixel=::GetPixel(memory,x-bounds.left,y-bounds.top);if(pixel!=CLR_INVALID&&abs(static_cast<int>(GetRValue(pixel))-GetRValue(expected))<24&&abs(static_cast<int>(GetGValue(pixel))-GetGValue(expected))<24&&abs(static_cast<int>(GetBValue(pixel))-GetBValue(expected))<24){foreground=true;break;}}
            if(!foreground)errorText=L"disabled label palette: "+std::to_wstring(::GetDlgCtrlID(control));FBE_OPTIONS_CHECK(foreground);
        }
    }
    ::SelectObject(memory, old); ::DeleteDC(memory); ::ReleaseDC(window, source);
    Gdiplus::GdiplusStartupInput input; ULONG_PTR token = 0;
    bool saved = false;
    if(Gdiplus::GdiplusStartup(&token, &input, NULL) == Gdiplus::Ok) {
        { Gdiplus::Bitmap image(bitmap, NULL);
          const CLSID png = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
          saved = image.Save((captures + L"\\" + prefix + L"-" + name + L".png").c_str(), &png, NULL) == Gdiplus::Ok; }
        Gdiplus::GdiplusShutdown(token);
    }
    ::DeleteObject(bitmap); return copied && saved && visibleContent;
}
// Render the native control, then inspect the actual pixels (not only TTM getters).
inline bool TooltipPixels(HWND tip,const std::wstring& name) {
    const Palette palette = paletteForWindow(tip);
    const COLORREF background=palette.highContrast||!palette.dark?::GetSysColor(COLOR_INFOBK):palette.control;
    const COLORREF foreground=palette.highContrast||!palette.dark?::GetSysColor(COLOR_INFOTEXT):palette.text;
    bool ok=static_cast<COLORREF>(::SendMessage(tip,TTM_GETTIPBKCOLOR,0,0))==background&&static_cast<COLORREF>(::SendMessage(tip,TTM_GETTIPTEXTCOLOR,0,0))==foreground;
    RECT rect={};::GetWindowRect(tip,&rect);const int width=rect.right-rect.left,height=rect.bottom-rect.top;
    HDC source=::GetWindowDC(tip),memory=::CreateCompatibleDC(source);HBITMAP bitmap=::CreateCompatibleBitmap(source,width,height);
    if(!source||!memory||!bitmap||width<=0||height<=0){if(bitmap)::DeleteObject(bitmap);if(memory)::DeleteDC(memory);if(source)::ReleaseDC(tip,source);return false;}
    HGDIOBJ old=::SelectObject(memory,bitmap);ok=::PrintWindow(tip,memory,0)!=FALSE&&ok;::GdiFlush();
    unsigned ink=0,paper=0;
    auto matchesColor=[](COLORREF a,COLORREF b){return abs(static_cast<int>(GetRValue(a))-GetRValue(b))<24&&abs(static_cast<int>(GetGValue(a))-GetGValue(b))<24&&abs(static_cast<int>(GetBValue(a))-GetBValue(b))<24;};
    for(int y=2;y<height-2;++y)for(int x=2;x<width-2;++x){const COLORREF pixel=::GetPixel(memory,x,y);if((palette.dark||palette.highContrast)?matchesColor(pixel,foreground):(GetRValue(pixel)<112&&GetGValue(pixel)<112&&GetBValue(pixel)<112))++ink;if((palette.dark||palette.highContrast)?matchesColor(pixel,background):(GetRValue(pixel)>230&&GetGValue(pixel)>230&&GetBValue(pixel)>200))++paper;}
    ::SelectObject(memory,old);::DeleteObject(bitmap);::DeleteDC(memory);::ReleaseDC(tip,source);
    const bool prior=expectedDarkTabs;expectedDarkTabs=false;const bool saved=Capture(tip,name);expectedDarkTabs=prior;
    if(!ok||ink<8||paper<100)errorText=L"tooltip: property="+std::to_wstring(ok)+L" ink="+std::to_wstring(ink)+L" paper="+std::to_wstring(paper);
    return ok&&saved&&ink>=8&&paper>=100;
}
inline bool ShowTooltip(HWND tip,TOOLINFOW& tool,const std::wstring& name) {
    // Add a short-lived tracking entry with the real registered text. Never
    // replace an application's stored text pointer with a test stack buffer.
    TOOLINFOW tracking=tool;tracking.uFlags=TTF_TRACK|TTF_ABSOLUTE;tracking.uId=0xFBE06464u;
    if(!::SendMessageW(tip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&tracking)))return false;
    RECT owner={};::GetWindowRect(tool.hwnd,&owner);::SendMessage(tip,TTM_TRACKPOSITION,0,MAKELPARAM(owner.left+40,owner.top+70));
    ::SendMessageW(tip,TTM_TRACKACTIVATE,TRUE,reinterpret_cast<LPARAM>(&tracking));
    const bool ready=::IsWindowVisible(tip)&&TooltipPixels(tip,name);
    ::SendMessageW(tip,TTM_TRACKACTIVATE,FALSE,reinterpret_cast<LPARAM>(&tracking));
    ::SendMessageW(tip,TTM_DELTOOLW,0,reinterpret_cast<LPARAM>(&tracking));return ready;
}
inline bool CaptureOwnedTooltip(HWND owner,const std::wstring& name) {
    std::vector<HWND> tips;
    ::EnumThreadWindows(::GetCurrentThreadId(),[](HWND window,LPARAM value)->BOOL {
        wchar_t kind[64]={};::GetClassNameW(window,kind,_countof(kind));
        if(!wcscmp(kind,TOOLTIPS_CLASSW))reinterpret_cast<std::vector<HWND>*>(value)->push_back(window);return TRUE;
    },reinterpret_cast<LPARAM>(&tips));
    for(HWND tip:tips)for(int i=0;i<static_cast<int>(::SendMessage(tip,TTM_GETTOOLCOUNT,0,0));++i) {
        wchar_t text[4096]={};TOOLINFOW tool={};tool.cbSize=sizeof(tool);tool.lpszText=text;
        if(!::SendMessageW(tip,TTM_ENUMTOOLSW,i,reinterpret_cast<LPARAM>(&tool)))continue;
        if(!::IsWindowVisible(tool.hwnd)||(tool.hwnd!=owner&&!::IsChild(owner,tool.hwnd)))continue;
        if((tool.uFlags&TTF_IDISHWND)&&!::IsWindowVisible(reinterpret_cast<HWND>(tool.uId)))continue;
        const bool shown=ShowTooltip(tip,tool,name);if(!shown&&errorText.empty())errorText=L"visible page tooltip show/pixel check failed";return shown;
    }
    errorText=L"visible page tooltip not found";return false;
}
inline void Select(HWND window, HWND tab, int page) {
    TabCtrl_SetCurSel(tab, page); NMHDR change = {tab, static_cast<UINT_PTR>(::GetDlgCtrlID(tab)), TCN_SELCHANGE};
    ::SendMessageW(window, WM_NOTIFY, change.idFrom, reinterpret_cast<LPARAM>(&change));
    ::RedrawWindow(window, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}
inline void Validate(HWND window) {
    RECT client = {}; ::GetClientRect(window, &client);
    std::vector<RECT> footer;
    size_t fixedButtons = 0;
    for(HWND child = ::GetWindow(window, GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT)) {
        wchar_t kind[32] = {}; ::GetClassNameW(child, kind, _countof(kind));
        if(::lstrcmpiW(kind, L"Button") != 0 || !::IsWindowVisible(child)) continue;
        const LONG type = ::GetWindowLongW(child, GWL_STYLE) & BS_TYPEMASK;
        if(type != BS_PUSHBUTTON && type != BS_DEFPUSHBUTTON) continue;
        RECT bounds = {}; ::GetWindowRect(child, &bounds); ::MapWindowPoints(NULL, window, reinterpret_cast<POINT*>(&bounds), 2);
        FBE_OPTIONS_CHECK(bounds.left >= 0 && bounds.top >= 0 && bounds.right <= client.right && bounds.bottom <= client.bottom);
        const int id = ::GetDlgCtrlID(child);
        const bool fixed = id == IDOK || id == IDCANCEL || (plugin == 1 && id == 1123) || (plugin == 2 && id == 2201) || (plugin == 3 && id >= 1023 && id <= 1025);
        if(fixed) {
            ++fixedButtons;
            POINT center = {(bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2};
            FBE_OPTIONS_CHECK(::RealChildWindowFromPoint(window, center) == child);
        }
        for(const RECT& previous : footer) { RECT intersection = {}; FBE_OPTIONS_CHECK(!::IntersectRect(&intersection, &bounds, &previous)); }
        footer.push_back(bounds);
    }
    const size_t expectedButtons[] = {2, 3, 3, 5};
    FBE_OPTIONS_CHECK(fixedButtons == expectedButtons[plugin]);
    HWND tab = Tab(window); FBE_OPTIONS_CHECK(tab != NULL);
    if(tab) for(int index = 0; index < TabCtrl_GetItemCount(tab); ++index) {
        wchar_t text[512] = {}; TCITEMW item = {}; item.mask = TCIF_TEXT; item.pszText = text; item.cchTextMax = _countof(text);
        TabCtrl_GetItem(tab, index, &item); RECT r = {}; TabCtrl_GetItemRect(tab, index, &r);
        HDC dc = ::GetDC(tab); HGDIOBJ old = ::SelectObject(dc, reinterpret_cast<HGDIOBJ>(::SendMessageW(tab, WM_GETFONT, 0, 0)));
        SIZE size = {}; ::GetTextExtentPoint32W(dc, text, static_cast<int>(wcslen(text)), &size);
        ::SelectObject(dc, old); ::ReleaseDC(tab, dc); FBE_OPTIONS_CHECK(size.cx <= r.right - r.left);
    }
}
inline void EditVisible(HWND window) {
    const auto controls = Snapshot(window);
    for(const auto& control : controls) if(::IsWindowVisible(control.first) && ::IsWindowEnabled(control.first) && Checkbox(control.first)) {
        editedControl = control.first; editedValue = ::SendMessageW(editedControl, BM_GETCHECK, 0, 0) == BST_CHECKED ? BST_UNCHECKED : BST_CHECKED;
        ::SendMessageW(editedControl, BM_SETCHECK, editedValue, 0); return;
    }
}
inline void ValidateScroll(HWND window) {
    HWND viewport = NULL;
    std::map<HWND, RECT> footer;
    for(HWND child = ::GetWindow(window, GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT)) {
        if(::GetWindowLongW(child, GWL_EXSTYLE) & WS_EX_CONTROLPARENT) viewport = child;
        wchar_t kind[32] = {}; ::GetClassNameW(child, kind, _countof(kind));
        if(::lstrcmpiW(kind, L"Button") == 0 && ::IsWindowVisible(child)) ::GetWindowRect(child, &footer[child]);
    }
    FBE_OPTIONS_CHECK(viewport != NULL);
    if(!viewport) return;
    const auto before = Snapshot(window);
    SCROLLINFO info = {sizeof(info), SIF_ALL};
    FBE_OPTIONS_CHECK(::GetScrollInfo(viewport, SB_VERT, &info) != FALSE);
    ::SendMessageW(viewport, WM_VSCROLL, SB_BOTTOM, 0);
    ::GetScrollInfo(viewport, SB_VERT, &info);
    const int maximum = (std::max)(0, info.nMax - static_cast<int>(info.nPage) + 1);
    FBE_OPTIONS_CHECK(info.nPos == maximum);
    if(maximum > 0) {
        RECT client = {}; ::GetClientRect(viewport, &client);
        LONG bottom = 0;
        for(HWND child = ::GetWindow(viewport, GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT)) if(::IsWindowVisible(child)) {
            RECT r = {}; ::GetWindowRect(child, &r); ::MapWindowPoints(NULL, viewport, reinterpret_cast<POINT*>(&r), 2);
            bottom = (std::max)(bottom, r.bottom);
        }
        FBE_OPTIONS_CHECK(bottom > 0 && bottom <= client.bottom);
        ::SendMessageW(viewport, WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), 0);
        ::GetScrollInfo(viewport, SB_VERT, &info); FBE_OPTIONS_CHECK(info.nPos < maximum);
    }
    ::SendMessageW(viewport, WM_VSCROLL, SB_TOP, 0);
    ::GetScrollInfo(viewport, SB_VERT, &info); FBE_OPTIONS_CHECK(info.nPos == 0);
    for(const auto& button : footer) { RECT r = {}; ::GetWindowRect(button.first, &r); FBE_OPTIONS_CHECK(::EqualRect(&r, &button.second) != FALSE); }
    FBE_OPTIONS_CHECK(Snapshot(window) == before);
}
inline BOOL CALLBACK Visit(HWND window, LPARAM) {
    if(CloseError(window)) return TRUE;
    // Nested initialization can dispatch timers before all pages exist. The
    // post-call hook observes completion of WM_INITDIALOG, including hidden
    // test processes whose first ShowWindow is suppressed by STARTUPINFO.
    if(!::GetDlgItem(window, IDOK) || (!dialog && !Tab(window))) return TRUE;
    if(!dialog && plugin != 2 && std::find(initializedDialogs.begin(), initializedDialogs.end(), window) == initializedDialogs.end()) return TRUE;
    if(!dialog) {
        dialog = window; ::ShowWindow(window, SW_SHOWNORMAL);
        if(!::IsWindowVisible(window)) ::ShowWindow(window, SW_SHOWNORMAL);
        ::SetForegroundWindow(window);
        wchar_t caption[512] = {}; ::GetWindowTextW(window, caption, _countof(caption));
        if(!expectedTitle.empty()) FBE_OPTIONS_CHECK(expectedTitle == caption);
        if(!expectedConfirmText.empty()) { ::GetWindowTextW(::GetDlgItem(window, IDOK), caption, _countof(caption)); if(expectedConfirmText != caption) errorText = L"confirmation expected=" + expectedConfirmText + L"; actual=" + caption; FBE_OPTIONS_CHECK(expectedConfirmText == caption); }
        if(!expectedCancelText.empty()) { ::GetWindowTextW(::GetDlgItem(window, IDCANCEL), caption, _countof(caption)); FBE_OPTIONS_CHECK(expectedCancelText == caption); }
        if(PositionProbe()) {
            RECT r = {}; ::GetWindowRect(window, &r);
            MONITORINFO monitor = {sizeof(monitor)}; FBE_OPTIONS_CHECK(::GetMonitorInfoW(::MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor) != FALSE);
            if(action == Action::PositionDefault) {
                FBE_OPTIONS_CHECK(abs((r.left - monitor.rcWork.left) - (monitor.rcWork.right - r.right)) <= 1);
                FBE_OPTIONS_CHECK(abs((r.top - monitor.rcWork.top) - (monitor.rcWork.bottom - r.bottom)) <= 1);
            } else if(action == Action::PositionVerify) {
                const wchar_t* names[] = {L"ExportHTML.", L"ExportDOCX.", L"ImportEPUB.", L"ExportEPUB."};
                FbePluginSettings::Store stored(L"plugin-windows", L"Software\\FBETeam\\FictionBook Editor Next\\PluginWindowPlacement");
                auto value = [&](LPCWSTR field) { return FbePluginSettings::ReadDword(stored, (std::wstring(names[plugin]) + field).c_str(), MAXDWORD); };
                FBE_OPTIONS_CHECK(SUCCEEDED(stored.Error()) && r.left == static_cast<LONG>(value(L"Left")) && r.top == static_cast<LONG>(value(L"Top")));
                FBE_OPTIONS_CHECK(r.right - r.left == static_cast<LONG>(value(L"Width")) && r.bottom - r.top == static_cast<LONG>(value(L"Height")));
            }
        }
        HWND tab = Tab(window); const auto initial = Snapshot(window);
        FBE_OPTIONS_CHECK(tab && TabCtrl_GetItemCount(tab) == (plugin == 2 ? 5 : 4));
        if(!tab) { ::PostMessageW(window, WM_COMMAND, IDCANCEL, 0); return TRUE; }
        for(int page = 0; page < TabCtrl_GetItemCount(tab); ++page) {
            Select(window, tab, page); Validate(window); ++pages;
            FBE_OPTIONS_CHECK(Snapshot(window) == initial);
            FBE_OPTIONS_CHECK(Capture(window, L"page" + std::to_wstring(page)));
            FBE_OPTIONS_CHECK(CaptureOwnedTooltip(window,L"page"+std::to_wstring(page)+L"-tooltip"));
        }
        // Controlled WM_DPICHANGED probes exercise layout at 100--200%; these
        // are reported separately from the native monitor's actual DPI.
        HDC dc = ::GetDC(window); const UINT nativeDpi = static_cast<UINT>(::GetDeviceCaps(dc, LOGPIXELSX)); ::ReleaseDC(window, dc);
        if(plugin >= 2 && !PositionProbe()) for(UINT dpi : {96u, 120u, 144u, 192u, nativeDpi}) {
            RECT compact = {}; ::GetWindowRect(window, &compact); compact.right = compact.left + ::MulDiv(460, dpi, 96); compact.bottom = compact.top + ::MulDiv(340, dpi, 96);
            ::SendMessageW(window, WM_DPICHANGED, MAKELONG(dpi, dpi), reinterpret_cast<LPARAM>(&compact));
            for(int page = 0; page < TabCtrl_GetItemCount(tab); ++page) { Select(window, tab, page); Validate(window); ValidateScroll(window); }
            RECT r = {}; ::GetWindowRect(window, &r); r.right = r.left + ::MulDiv(780, dpi, 96); r.bottom = r.top + ::MulDiv(500, dpi, 96);
            ::SendMessageW(window, WM_DPICHANGED, MAKELONG(dpi, dpi), reinterpret_cast<LPARAM>(&r));
            for(int page = 0; page < TabCtrl_GetItemCount(tab); ++page) { Select(window, tab, page); Validate(window); }
        }
        FBE_OPTIONS_CHECK(Snapshot(window) == initial);
        if(action != Action::Inspect && !PositionProbe()) for(int page = 0; page < TabCtrl_GetItemCount(tab); ++page) { Select(window, tab, page); EditVisible(window); }
        editedSnapshot = Snapshot(window);
        if(action != Action::Inspect && !PositionProbe()) FBE_OPTIONS_CHECK(editedSnapshot != initial);
        if(action == Action::PositionMove) {
            RECT r = {}; ::GetWindowRect(window, &r); MONITORINFO monitor = {sizeof(monitor)};
            FBE_OPTIONS_CHECK(::GetMonitorInfoW(::MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor) != FALSE);
            const int width = (std::min)(r.right - r.left + 32, monitor.rcWork.right - monitor.rcWork.left - 32);
            const int height = (std::min)(r.bottom - r.top + 24, monitor.rcWork.bottom - monitor.rcWork.top - 32);
            ::SetWindowPos(window, NULL, monitor.rcWork.left + 16, monitor.rcWork.top + 16, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        if(action == Action::DefaultsCancel || action == Action::PresetDefault || action == Action::PresetCompat || action == Action::PresetRich) {
            const int button = plugin == 2 ? 2201 : (action == Action::PresetCompat ? 1024 : (action == Action::PresetRich ? 1025 : 1023));
            const auto before = Read(profile); ::SendMessageW(window, WM_COMMAND, button, 0); FBE_OPTIONS_CHECK(Read(profile) == before);
        }
        beforeFile = Read(profile);
        if(action == Action::ReadOnly) { originalAttributes = ::GetFileAttributesW(profile.c_str()); FBE_OPTIONS_CHECK(::SetFileAttributesW(profile.c_str(), FILE_ATTRIBUTE_READONLY) != FALSE); }
        if(action == Action::Conflict) {
            if(peerSave) FBE_OPTIONS_CHECK(peerSave());
            else { auto newer = beforeFile; newer.push_back('\n'); FBE_OPTIONS_CHECK(Write(profile, newer)); }
            FBE_OPTIONS_CHECK(Read(profile) != beforeFile); beforeFile = Read(profile);
        }
        if(action == Action::Corrupt) { const std::vector<BYTE> invalid = {'{','b','a','d'}; FBE_OPTIONS_CHECK(Write(profile, invalid)); beforeFile = invalid; }
        if(plugin < 2) {
            stage = 1;
            const bool cancel = action == Action::EditCancel || PositionProbe() || failedLine;
            if(action == Action::PositionMove) ::PostMessageW(window, WM_CLOSE, 0, 0);
            else ::PostMessageW(window, WM_COMMAND, cancel ? IDCANCEL : IDOK, 0);
            return TRUE;
        }
        FBE_OPTIONS_CHECK(::GetKeyboardState(savedKeys) != FALSE);
        BYTE keys[256]; memcpy(keys, savedKeys, sizeof(keys)); keys[VK_CONTROL] = 0x80; keys[VK_SHIFT] = 0;
        ::SetActiveWindow(window); ::SetForegroundWindow(window);
        ::SetKeyboardState(keys); ::SetFocus(tab); stage = 10;
        ::PostMessageW(tab, WM_KEYDOWN, VK_TAB, 0);
    } else if(window == dialog && stage >= 10) {
        if(keyboardStage != stage) return TRUE;
        HWND tab = Tab(window);
        if(stage == 10) {
            FBE_OPTIONS_CHECK(TabCtrl_GetCurSel(tab) == 0);
            BYTE keys[256]; memcpy(keys, savedKeys, sizeof(keys)); keys[VK_CONTROL] = keys[VK_SHIFT] = 0x80;
            ::SetKeyboardState(keys); stage = 11; ::PostMessageW(tab, WM_KEYDOWN, VK_TAB, 0);
        } else if(stage == 11) {
            FBE_OPTIONS_CHECK(TabCtrl_GetCurSel(tab) == TabCtrl_GetItemCount(tab) - 1);
            BYTE keys[256]; memcpy(keys, savedKeys, sizeof(keys)); keys[VK_CONTROL] = keys[VK_SHIFT] = 0;
            ::SetKeyboardState(keys); stage = 12; ::PostMessageW(tab, WM_KEYDOWN, VK_TAB, 0);
        } else if(stage == 12) {
            FBE_OPTIONS_CHECK(::GetFocus() && ::IsWindowVisible(::GetFocus()));
            BYTE keys[256]; memcpy(keys, savedKeys, sizeof(keys)); keys[VK_CONTROL] = 0; keys[VK_SHIFT] = 0x80;
            ::SetKeyboardState(keys); stage = 13; ::PostMessageW(::GetFocus(), WM_KEYDOWN, VK_TAB, 0);
        } else {
            FBE_OPTIONS_CHECK(::GetFocus() && ::IsWindowVisible(::GetFocus()));
            ::SetKeyboardState(savedKeys); stage = 1;
            const bool cancel = action == Action::EditCancel || action == Action::DefaultsCancel || PositionProbe() || failedLine;
            // Real queued key messages pass through the dialog keyboard loop.
            if(!cancel) ::SetFocus(::GetDlgItem(window, IDOK));
            if(action == Action::PositionMove) ::PostMessageW(window, WM_CLOSE, 0, 0);
            else ::PostMessageW(::GetFocus(), WM_KEYDOWN, cancel ? VK_ESCAPE : VK_RETURN, 0);
        }
    } else if(window == dialog && stage == 1 && errorSeen && ::IsWindowEnabled(window) && !::IsWindow(errorDialog)) {
        FBE_OPTIONS_CHECK(Snapshot(window) == editedSnapshot);
        FBE_OPTIONS_CHECK(Read(profile) == beforeFile);
        if(originalAttributes != INVALID_FILE_ATTRIBUTES) ::SetFileAttributesW(profile.c_str(), originalAttributes);
        if(action == Action::Corrupt) { /* Keep the deliberately damaged fixture for its caller to restore. */ }
        stage = 2; ::PostMessageW(window, WM_COMMAND, IDCANCEL, 0);
    }
    return TRUE;
}
inline void CALLBACK Timer(HWND, UINT, UINT_PTR, DWORD) { ::EnumThreadWindows(::GetCurrentThreadId(), Visit, 0); }
inline void Start(int index, Action requested, const std::wstring& directory, const std::wstring& name, const std::wstring& settingsPath) {
    Stop(); initializedDialogs.clear();
    initializationHook = ::SetWindowsHookExW(WH_CALLWNDPROCRET, Initialized, NULL, ::GetCurrentThreadId());
    keyboardStage = 0;
    keyboardHook = ::SetWindowsHookExW(WH_GETMESSAGE, KeyboardMessage, NULL, ::GetCurrentThreadId());
    plugin = index; action = requested; captures = directory; prefix = name; profile = settingsPath;
    failedLine = stage = pages = 0; dialog = errorDialog = editedControl = NULL; errorSeen = false; originalAttributes = INVALID_FILE_ATTRIBUTES;
    beforeFile.clear(); editedSnapshot.clear(); errorText.clear(); expectedTitle.clear(); expectedConfirmText.clear(); expectedCancelText.clear(); expectedDarkTabs = false;
}
inline bool Passed() {
    const bool expectedError = action == Action::ReadOnly || action == Action::Conflict || action == Action::Corrupt;
    return !failedLine && pages == (plugin == 2 ? 5 : 4) && errorSeen == expectedError && (!expectedError || stage == 2);
}
#undef FBE_OPTIONS_CHECK
}
