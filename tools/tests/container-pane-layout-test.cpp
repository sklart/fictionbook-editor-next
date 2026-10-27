#include <windows.h>
#include <commctrl.h>
#include <atlbase.h>
#include <atlapp.h>
WTL::CAppModule _Module;
#include <atlwin.h>
#include <atltypes.h>
#include <cstdio>
using namespace ATL;
using namespace WTL;
#include "ContainerWnd.h"

// Palette dependency only; the production container and native window sizing
// are exercised unchanged, without loading the editor's settings service.
HBRUSH ThemeManager::WindowBrush() { return ::GetSysColorBrush(COLOR_WINDOW); }

static LRESULT CALLBACK ProbeProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_SIZE) {
        int* count = reinterpret_cast<int*>(::GetWindowLongPtr(window, GWLP_USERDATA));
        if (count) ++*count;
    }
    return ::DefWindowProc(window, message, wparam, lparam);
}

static RECT Bounds(HWND window, HWND parent)
{
    RECT rect = {}; ::GetWindowRect(window, &rect);
    ::MapWindowPoints(nullptr, parent, reinterpret_cast<POINT*>(&rect), 2);
    return rect;
}

int main()
{
    _Module.Init(nullptr, ::GetModuleHandleW(nullptr));
    WNDCLASSW klass = {}; klass.hInstance = ::GetModuleHandleW(nullptr);
    klass.lpfnWndProc = ProbeProc; klass.lpszClassName = L"FbePaneLayoutProbe";
    ::RegisterClassW(&klass);
    HWND parent = ::CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
    CContainerWnd view; RECT initial = {0, 0, 200, 180};
    HWND container = view.Create(parent, initial);
    HWND active = ::CreateWindowExW(0, klass.lpszClassName, L"", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, container, nullptr, klass.hInstance, nullptr);
    HWND hidden = ::CreateWindowExW(0, klass.lpszClassName, L"", WS_CHILD, 17, 19, 31, 47, container, nullptr, klass.hInstance, nullptr);
    HWND grandchild = ::CreateWindowExW(0, klass.lpszClassName, L"", WS_CHILD | WS_VISIBLE, 3, 4, 5, 6, active, nullptr, klass.hInstance, nullptr);
    if (!parent || !container || !active || !hidden || !grandchild) { std::fputs("window creation failed\n", stderr); return 1; }
    int activeSizes = 0, hiddenSizes = 0, grandchildSizes = 0;
    ::SetWindowLongPtr(active, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&activeSizes));
    ::SetWindowLongPtr(hidden, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&hiddenSizes));
    ::SetWindowLongPtr(grandchild, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&grandchildSizes));
    const RECT hiddenBefore = Bounds(hidden, container), grandchildBefore = Bounds(grandchild, active);
    bool passed = true;
    auto check = [&](bool value, const char* message) { if (!value) { passed = false; std::fprintf(stderr, "%s\n", message); } };
    ::MoveWindow(container, 0, 0, 400, 300, FALSE);
    RECT client = {}; ::GetClientRect(container, &client);
    RECT actual = Bounds(active, container); check(::EqualRect(&actual, &client) != FALSE, "active pane does not fit client");
    actual = Bounds(hidden, container); check(::EqualRect(&actual, &hiddenBefore) != FALSE && hiddenSizes == 0, "hidden pane resized");
    actual = Bounds(grandchild, active); check(::EqualRect(&actual, &grandchildBefore) != FALSE && grandchildSizes == 0, "container resized a grandchild");
    const RECT cachedActive = Bounds(active, container);
    const int cachedActiveSizes = activeSizes;
    view.HideActiveWnd(); ::MoveWindow(container, 0, 0, 500, 330, FALSE);
    actual = Bounds(active, container); check(::EqualRect(&actual, &cachedActive) != FALSE && activeSizes == cachedActiveSizes, "hidden active pane lost cached dimensions");
    view.ActivateWnd(hidden); ::GetClientRect(container, &client);
    actual = Bounds(hidden, container); check(::EqualRect(&actual, &client) != FALSE && view.GetActiveWnd() == hidden, "activated pane was not fitted");
    const int fittedSizes = hiddenSizes;
    view.ActivateWnd(hidden); check(hiddenSizes == fittedSizes, "unchanged activation sent WM_SIZE");
    view.HideActiveWnd(); ::MoveWindow(container, 0, 0, 400, 300, FALSE); view.ActivateWnd(active);
    check(activeSizes == cachedActiveSizes, "return to cached pane invalidated its size");
    view.DestroyWindow(); ::DestroyWindow(parent); ::UnregisterClassW(klass.lpszClassName, klass.hInstance); _Module.Term();
    std::puts(passed ? "Container pane layout: PASS" : "Container pane layout: FAIL");
    return passed ? 0 : 1;
}
