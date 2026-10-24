#include <windows.h>
#include <atlbase.h>
#include <atlapp.h>
WTL::CAppModule _Module;
#include <atlwin.h>
#include <atltypes.h>
#include <atlmisc.h>
#include <atlctrls.h>
#include <atlframe.h>
#include <atlctrlx.h>
#include <cstdio>

int main()
{
    _Module.Init(nullptr, ::GetModuleHandleW(nullptr));
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_BAR_CLASSES};
    ::InitCommonControlsEx(&controls);
    HWND parent = ::CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, nullptr, nullptr);
    WTL::CPaneContainer pane;
    RECT bounds = {0, 0, 120, 80};
    HWND window = static_cast<ATL::CWindowImpl<WTL::CPaneContainer>&>(pane).Create(
        parent, bounds, nullptr, WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
    TBBUTTON button = {};
    const bool passed = parent && window && pane.m_tb.IsWindow() &&
        pane.m_tb.GetButtonCount() == 1 && pane.m_tb.GetButton(0, &button) &&
        button.idCommand == WTL::CPaneContainer::m_nCloseBtnID &&
        (button.fsState & TBSTATE_ENABLED) != 0;
    if (window) pane.DestroyWindow();
    if (parent) ::DestroyWindow(parent);
    _Module.Term();
    std::puts(passed ? "Pane close button: PASS" : "Pane close button: FAIL");
    return passed ? 0 : 1;
}
