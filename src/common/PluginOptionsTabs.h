#pragma once
#include <windows.h>
#include <commctrl.h>
#include <algorithm>
#include <string>
#include <vector>

namespace FbePluginUi {
inline HWND FindControl(HWND parent, int id) {
    if(HWND direct = ::GetDlgItem(parent, id)) return direct;
    for(HWND child = ::GetWindow(parent, GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT))
        if(HWND found = FindControl(child, id)) return found;
    return NULL;
}

// Native tab control plus a clipped, scrollable page viewport. Parameters stay
// in their original Win32 controls; changing pages never reloads their values.
class OptionsTabs {
    struct Item { HWND window; int page; };
    HWND owner = NULL, tabs = NULL, viewport = NULL;
    std::vector<Item> items;
    std::vector<HWND> footer;
    HWND tooltip = NULL;
    std::vector<std::wstring> pageTips;
    UINT dpi = 96;
    HFONT font = NULL;
    int page = 0, scroll = 0, contentHeight = 0;
    HHOOK filter = NULL;
    OptionsTabs* previous = NULL;
    inline static thread_local OptionsTabs* active = NULL;
    int Scale(int value) const { return ::MulDiv(value, static_cast<int>(dpi), 96); }
    static std::wstring Text(HWND window) {
        const int count = ::GetWindowTextLengthW(window);
        std::wstring text(static_cast<size_t>(count) + 1, L'\0');
        ::GetWindowTextW(window, &text[0], count + 1); text.resize(static_cast<size_t>(count)); return text;
    }
    int TextHeight(HWND window, int width, bool single = false) const {
        HDC dc = ::GetDC(owner); HGDIOBJ old = ::SelectObject(dc, font);
        RECT bounds = {0, 0, width, 0}; const auto text = Text(window);
        ::DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &bounds, DT_CALCRECT | DT_NOPREFIX | (single ? DT_SINGLELINE : DT_WORDBREAK));
        ::SelectObject(dc, old); ::ReleaseDC(owner, dc);
        return single ? bounds.right : bounds.bottom;
    }
    static LRESULT CALLBACK Filter(int code, WPARAM wp, LPARAM lp) {
        if(code == MSGF_DIALOGBOX && active && active->ProcessMessage(*reinterpret_cast<MSG*>(lp))) return TRUE;
        return ::CallNextHookEx(NULL, code, wp, lp);
    }
    static LRESULT CALLBACK ViewportProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) {
        OptionsTabs* self = reinterpret_cast<OptionsTabs*>(data);
        if(msg == WM_COMMAND || msg == WM_NOTIFY) return ::SendMessageW(self->owner, msg, wp, lp);
        if(msg == WM_VSCROLL && !lp) {
            const int step = self->Scale(24);
            switch(LOWORD(wp)) {
            case SB_LINEUP: self->scroll -= step; break;
            case SB_LINEDOWN: self->scroll += step; break;
            case SB_PAGEUP: { RECT r = {}; ::GetClientRect(hwnd, &r); self->scroll -= r.bottom; break; }
            case SB_PAGEDOWN: { RECT r = {}; ::GetClientRect(hwnd, &r); self->scroll += r.bottom; break; }
            case SB_TOP: self->scroll = 0; break;
            case SB_BOTTOM: self->scroll = self->contentHeight; break;
            case SB_THUMBTRACK: case SB_THUMBPOSITION: {
                SCROLLINFO info = {sizeof(info), SIF_TRACKPOS}; ::GetScrollInfo(hwnd, SB_VERT, &info); self->scroll = info.nTrackPos; break;
            }
            default: return 0;
            }
            self->Layout(self->dpi); return 0;
        }
        if(msg == WM_MOUSEWHEEL) {
            self->scroll -= ::MulDiv(GET_WHEEL_DELTA_WPARAM(wp), self->Scale(72), WHEEL_DELTA);
            self->Layout(self->dpi); return 0;
        }
        return ::DefSubclassProc(hwnd, msg, wp, lp);
    }
public:
    static const int TabId = 7900;
    ~OptionsTabs() { Detach(); }
    void Detach() {
        if(filter) { ::UnhookWindowsHookEx(filter); filter = NULL; }
        if(active == this) active = previous;
        if(::IsWindow(viewport)) ::RemoveWindowSubclass(viewport, ViewportProc, 1);
        owner = NULL; viewport = NULL; tabs = NULL;
    }
    bool Create(HWND window, const std::vector<std::wstring>& labels, const std::vector<int>& buttons) {
        owner = window; font = reinterpret_cast<HFONT>(::SendMessageW(window, WM_GETFONT, 0, 0));
        if(!font) font = reinterpret_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
        ::SetWindowLongW(window, GWL_STYLE, (::GetWindowLongW(window, GWL_STYLE) | WS_CLIPCHILDREN) & ~(WS_VSCROLL | WS_HSCROLL));
        ::ShowScrollBar(window, SB_BOTH, FALSE);
        tabs = ::CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | TCS_MULTILINE,
            0, 0, 1, 1, window, reinterpret_cast<HMENU>(TabId), ::GetModuleHandleW(NULL), NULL);
        viewport = ::CreateWindowExW(WS_EX_CONTROLPARENT, L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL,
            0, 0, 1, 1, window, NULL, ::GetModuleHandleW(NULL), NULL);
        if(!tabs || !viewport || !::SetWindowSubclass(viewport, ViewportProc, 1, reinterpret_cast<DWORD_PTR>(this))) return false;
        if(!::SetPropW(tabs, L"FBE.PluginOptionsTabs", reinterpret_cast<HANDLE>(1))) return false;
        for(size_t index = 0; index < labels.size(); ++index) {
            TCITEMW item = {}; item.mask = TCIF_TEXT; item.pszText = const_cast<LPWSTR>(labels[index].c_str());
            TabCtrl_InsertItem(tabs, static_cast<int>(index), &item);
        }
        for(int id : buttons) if(HWND button = ::GetDlgItem(window, id)) footer.push_back(button); else return false;
        previous = active; active = this;
        filter = ::SetWindowsHookExW(WH_MSGFILTER, Filter, NULL, ::GetCurrentThreadId());
        SetFont(font); return filter != NULL;
    }
    bool Add(int id, int targetPage) {
        HWND control = ::GetDlgItem(owner, id); if(!control) return false;
        ::SetParent(control, viewport);
        wchar_t type[32] = {}; ::GetClassNameW(control, type, _countof(type));
        if(::lstrcmpiW(type, L"Button") == 0)
            ::SetWindowLongW(control, GWL_STYLE, ::GetWindowLongW(control, GWL_STYLE) | BS_MULTILINE);
        items.push_back({control, targetPage}); ::ShowWindow(control, targetPage == page ? SW_SHOW : SW_HIDE); return true;
    }
    void SetFont(HFONT value) {
        font = value;
        for(HWND control : {tabs, viewport}) if(control) ::SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        for(const auto& item : items) ::SendMessageW(item.window, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        for(HWND control : footer) ::SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    void SetTooltips(HWND window, const std::vector<std::wstring>& tips) { tooltip = window; pageTips = tips; }
    bool Notify(LPARAM lp) {
        const NMHDR* message = reinterpret_cast<NMHDR*>(lp);
        if(message && message->hwndFrom == tabs && message->code == TCN_SELCHANGE) { Select(TabCtrl_GetCurSel(tabs)); return true; }
        return false;
    }
    void Select(int value) {
        if(value < 0 || value >= TabCtrl_GetItemCount(tabs)) return;
        if(::IsChild(viewport, ::GetFocus())) ::SetFocus(tabs);
        page = value; scroll = 0; TabCtrl_SetCurSel(tabs, page);
        for(const auto& item : items) ::ShowWindow(item.window, item.page == page ? SW_SHOW : SW_HIDE);
        Layout(dpi);
    }
    bool ProcessMessage(const MSG& message) {
        if(message.message == WM_KEYDOWN && message.wParam == VK_TAB && (::GetKeyState(VK_CONTROL) & 0x8000) &&
            (message.hwnd == owner || ::IsChild(owner, message.hwnd))) {
            const int count = TabCtrl_GetItemCount(tabs);
            Select((page + ((::GetKeyState(VK_SHIFT) & 0x8000) ? count - 1 : 1)) % count); ::SetFocus(tabs); return true;
        }
        return false;
    }
    void RevealFocus() {
        HWND focus = ::GetFocus(); if(!viewport || !::IsChild(viewport, focus)) return;
        RECT r = {}, client = {}; ::GetWindowRect(focus, &r); ::MapWindowPoints(NULL, viewport, reinterpret_cast<POINT*>(&r), 2); ::GetClientRect(viewport, &client);
        if(r.top < 0) scroll += r.top;
        else if(r.bottom > client.bottom) scroll += r.bottom - client.bottom;
        else return;
        Layout(dpi);
    }
    void Wheel(WPARAM value) { scroll -= ::MulDiv(GET_WHEEL_DELTA_WPARAM(value), Scale(72), WHEEL_DELTA); Layout(dpi); }
    void Layout(UINT value) {
        if(!owner || !tabs) return;
        dpi = value; RECT client = {}; ::GetClientRect(owner, &client);
        const int margin = Scale(12), gap = Scale(8), available = (std::max)(Scale(40), static_cast<int>(client.right) - 2 * margin);
        // Wrap the fixed footer when localized preset captions need more room.
        struct Button { HWND window; int x, row, width; };
        std::vector<Button> buttons; int x = 0, row = 0, buttonHeight = Scale(30);
        for(HWND control : footer) {
            const int width = (std::min)(available, (std::max)(Scale(88), TextHeight(control, available, true) + Scale(28)));
            buttonHeight = (std::max)(buttonHeight, TextHeight(control, width - Scale(16)) + Scale(12));
            if(x && x + width > available) { ++row; x = 0; }
            buttons.push_back({control, x, row, width}); x += width + gap;
        }
        const int footerHeight = (row + 1) * (buttonHeight + gap) + margin;
        const int footerTop = (std::max)(margin, static_cast<int>(client.bottom) - footerHeight);
        for(const auto& button : buttons) ::SetWindowPos(button.window, NULL, margin + button.x, footerTop + button.row * (buttonHeight + gap), button.width, buttonHeight, SWP_NOZORDER | SWP_NOACTIVATE);
        ::SetWindowPos(tabs, NULL, margin, margin, available, (std::max)(Scale(30), footerTop - 2 * margin), SWP_NOZORDER | SWP_NOACTIVATE);
        if(tooltip) for(size_t index = 0; index < pageTips.size(); ++index) {
            TOOLINFOW tool = {}; tool.cbSize = sizeof(tool); tool.hwnd = tabs; tool.uId = index + 1;
            tool.uFlags = TTF_SUBCLASS; TabCtrl_GetItemRect(tabs, static_cast<int>(index), &tool.rect);
            tool.lpszText = const_cast<LPWSTR>(pageTips[index].c_str());
            if(!::SendMessageW(tooltip, TTM_GETTOOLINFOW, 0, reinterpret_cast<LPARAM>(&tool)))
                ::SendMessageW(tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool));
            else { TabCtrl_GetItemRect(tabs, static_cast<int>(index), &tool.rect); ::SendMessageW(tooltip, TTM_NEWTOOLRECTW, 0, reinterpret_cast<LPARAM>(&tool)); }
        }
        RECT pageRect = {}; ::GetClientRect(tabs, &pageRect); TabCtrl_AdjustRect(tabs, FALSE, &pageRect);
        ::MapWindowPoints(tabs, owner, reinterpret_cast<POINT*>(&pageRect), 2);
        const int viewportHeight = (std::max)(0L, pageRect.bottom - pageRect.top);
        ::SetWindowPos(viewport, HWND_TOP, pageRect.left, pageRect.top, (std::max)(1L, pageRect.right - pageRect.left), viewportHeight, SWP_NOACTIVATE);
        auto heightFor = [&](HWND control, int width) {
            wchar_t kind[32] = {}; ::GetClassNameW(control, kind, _countof(kind));
            if(::lstrcmpiW(kind, L"ComboBox") == 0 || ::lstrcmpiW(kind, L"Edit") == 0) return Scale(28);
            const bool checkbox = ::lstrcmpiW(kind, L"Button") == 0;
            return (std::max)(Scale(24), TextHeight(control, (std::max)(1, width - Scale(checkbox ? 34 : 12))) + Scale(8));
        };
        RECT view = {}; ::GetClientRect(viewport, &view);
        auto measure = [&](int width) { int total = gap; for(const auto& item : items) if(item.page == page) total += heightFor(item.window, width) + Scale(4); return total + gap; };
        contentHeight = measure((std::max)(1L, view.right - 2 * gap));
        ::ShowScrollBar(viewport, SB_VERT, contentHeight > viewportHeight);
        ::GetClientRect(viewport, &view);
        const int width = (std::max)(1L, view.right - 2 * gap);
        contentHeight = measure(width); scroll = (std::min)((std::max)(0, scroll), (std::max)(0, contentHeight - viewportHeight));
        SCROLLINFO info = {sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS}; info.nMax = contentHeight - 1; info.nPage = static_cast<UINT>(viewportHeight); info.nPos = scroll;
        ::SetScrollInfo(viewport, SB_VERT, &info, TRUE);
        int y = gap - scroll;
        for(const auto& item : items) if(item.page == page) {
            const int height = heightFor(item.window, width);
            wchar_t kind[32] = {}; ::GetClassNameW(item.window, kind, _countof(kind));
            ::SetWindowPos(item.window, NULL, gap, y, width, ::lstrcmpiW(kind, L"ComboBox") == 0 ? Scale(180) : height, SWP_NOZORDER | SWP_NOACTIVATE);
            y += height + Scale(4);
        }
        ::InvalidateRect(viewport, NULL, TRUE);
    }
};
}
