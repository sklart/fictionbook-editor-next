#include <windows.h>
#include <atlbase.h>
#include <atlapp.h>
WTL::CAppModule _Module;
#include <atlwin.h>
#include <atltypes.h>
#include <atlmisc.h>
#include <atlctrls.h>
#include <atlframe.h>
#include <cstdio>
#include <string>
using namespace ATL;
using namespace WTL;
#include "../../src/fbe/extras/atlctrlsext.h"

struct Host : CCustomizableToolBarCommands<Host> {
    void Register(HWND toolbar) { TBBUTTONS buttons; m_aButtons.Add(toolbar, buttons); m_aDefaultButtons.Add(toolbar, buttons); }
};
int main() {
    _Module.Init(NULL, ::GetModuleHandleW(NULL));
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_BAR_CLASSES}; ::InitCommonControlsEx(&controls);
    int checks = 0, failures = 0;
    auto check = [&](bool value, const char* message) { ++checks; if(!value) { ++failures; std::printf("FAIL: %s\n", message); } };
    HWND parent = ::CreateWindowExW(0, L"STATIC", L"", 0, 0,0,0,0, HWND_MESSAGE, NULL, NULL, NULL);
    HWND toolbar = ::CreateWindowExW(0, TOOLBARCLASSNAMEW, L"", WS_CHILD | CCS_ADJUSTABLE, 0,0,0,0, parent, NULL, NULL, NULL);
    check(parent && toolbar, "real toolbar created");
    if(toolbar) {
        CToolBarCtrl control = toolbar; control.SetButtonStructSize();
        Host host; host.Register(toolbar);
        const wchar_t* caption = L"Длинная подпись сценария: toolbar caption survives deletion and every rebuild";
        TBBUTTON original = {}; original.iBitmap = I_IMAGENONE; original.idCommand = 100; original.fsState = TBSTATE_ENABLED; original.fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE; original.iString = reinterpret_cast<INT_PTR>(caption);
        // Match AddTbButton's AddButton/GetButton/cache/DeleteButton sequence.
        check(control.AddButton(100, original.fsStyle, original.fsState, 0, caption, 0) != FALSE, "initial live button added");
        TBBUTTON borrowed = {}; check(control.GetButton(0, &borrowed) != FALSE, "control-owned caption captured");
        check(host.AddToolbarButton(toolbar, borrowed, caption) != FALSE, "button stored in persistent command catalog");
        check(control.DeleteButton(0) != FALSE, "original button deleted");
        Host::TBBUTTONS catalog; check(host.GetAvailableButtons(toolbar, catalog) && catalog.GetSize() == 1, "catalog copied after deletion");
        if(catalog.GetSize() == 1) {
            check(std::basic_string<TCHAR>(reinterpret_cast<LPCTSTR>(catalog[0].iString)) == caption, "cached text outlives original control allocation");
            for(int index = 0; index < 100; ++index) {
                CString text; text.Format(L"Extra unique caption %d", index);
                TBBUTTON extra = original; extra.idCommand = 200 + index;
                check(host.AddToolbarButton(toolbar, extra, text) != FALSE, "caption storage grows without invalidating snapshots");
            }
            for(int repeat = 0; repeat < 20; ++repeat) {
                check(control.AddButtons(1, &catalog[0]) != FALSE, "cached button recreated");
                wchar_t text[256] = {};
                check(control.GetButtonText(100, text) >= 0 && std::wstring(text) == caption, "recreated button preserves full Unicode caption");
                check(control.DeleteButton(0) != FALSE, "recreated button removed");
            }
        }
        ::DestroyWindow(toolbar);
    }
    if(parent) ::DestroyWindow(parent);
    _Module.Term();
    std::printf("Toolbar caption lifetime: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
