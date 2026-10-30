#include <windows.h>
#include <atlbase.h>
#include <atlapp.h>
WTL::CAppModule _Module;
#include <atlwin.h>
#include <atltypes.h>
#include <atlmisc.h>
#include <atlctrls.h>
using namespace WTL;
#include "../../src/fbe/settings/ui/EditorFontFamilies.h"

int main()
{
    _Module.Init(nullptr, GetModuleHandleW(nullptr));
    HWND parent = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED, 0, 0, 200, 200, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    CComboBox first, second, fresh;
    first.Create(parent, CWindow::rcDefault, nullptr, WS_CHILD | CBS_DROPDOWNLIST);
    second.Create(parent, CWindow::rcDefault, nullptr, WS_CHILD | CBS_DROPDOWNLIST);
    fresh.Create(parent, CWindow::rcDefault, nullptr, WS_CHILD | CBS_DROPDOWNLIST);
    const wchar_t* missingFirst = L"FBE missing test font 31A9";
    const wchar_t* missingSecond = L"FBE missing test font 31B9";
    const wchar_t* marker = L"FBE cache test marker 31C9";
    bool ok = parent && first.IsWindow() && second.IsWindow() && fresh.IsWindow();
    {
        EditorFontFamilies::ScopedCache scope;
        EditorFontFamilies::Fill(first, missingFirst);
        ok = ok && EditorFontFamilies::currentCache->loaded;
        EditorFontFamilies::Fill(second, missingSecond);
        ok = ok && first.FindStringExact(-1, missingFirst) != CB_ERR && second.FindStringExact(-1, missingSecond) != CB_ERR;
        ok = ok && first.FindStringExact(-1, missingSecond) == CB_ERR && second.FindStringExact(-1, missingFirst) == CB_ERR;
        ok = ok && first.GetCurSel() != CB_ERR && second.GetCurSel() != CB_ERR;
        EditorFontFamilies::currentCache->names.push_back(marker);
    }
    {
        EditorFontFamilies::ScopedCache scope;
        EditorFontFamilies::Fill(fresh, L"");
        ok = ok && fresh.GetCount() > 0 && fresh.FindStringExact(-1, marker) == CB_ERR;
    }
    ok = ok && EditorFontFamilies::currentCache == nullptr;
    if (parent) DestroyWindow(parent);
    _Module.Term();
    return ok ? 0 : 1;
}
