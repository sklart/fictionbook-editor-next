#include "stdafx.h"
#include "search\\ui\\ComboBoxEdit.h"
#include "search\\ui\\RegexQuickReferencePopup.h"
#include "ThemeManager.h"
#include "UiMetrics.h"

CAppModule _Module;

CString FbeLoadRuntimeStringByKey(LPCWSTR, LPCWSTR fallback)
{
    return CString(fallback ? fallback : L"");
}

namespace ThemeManager
{
void ApplyToWindow(HWND) {}
COLORREF TextColor() { return RGB(0, 0, 0); }
COLORREF WindowColor() { return RGB(255, 255, 255); }
COLORREF ControlColor() { return RGB(255, 255, 255); }
COLORREF BorderColor() { return RGB(96, 96, 96); }
COLORREF SeparatorColor() { return RGB(192, 192, 192); }
COLORREF SecondaryTextColor() { return RGB(96, 96, 96); }
COLORREF SelectionBackgroundColor() { return RGB(0, 120, 215); }
COLORREF SelectionTextColor() { return RGB(255, 255, 255); }
HBRUSH WindowBrush() { static HBRUSH window = ::CreateSolidBrush(RGB(255, 255, 255)); return window; }
HBRUSH Brush(ThemeColorRole role)
{
    static HBRUSH control = ::CreateSolidBrush(ControlColor());
    static HBRUSH selection = ::CreateSolidBrush(SelectionBackgroundColor());
    return role == THEME_COLOR_SELECTION_BACKGROUND ? selection : control;
}
}

namespace
{
void DispatchMessages(CMessageLoop& messageLoop)
{
    MSG message = {};
    while (::PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
    {
        if (!messageLoop.PreTranslateMessage(&message))
        {
            ::TranslateMessage(&message);
            ::DispatchMessage(&message);
        }
    }
}

void DispatchUntilIdle(CMessageLoop& messageLoop);

bool Check(bool value)
{
    return value;
}

bool TestComboInsertion(HWND owner)
{
    HWND combo = ::CreateWindowEx(0, WC_COMBOBOX, L"abcdef", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
        0, 0, 160, 120, owner, NULL, _Module.GetModuleInstance(), NULL);
    COMBOBOXINFO comboInfo = { sizeof(comboInfo) };
    if (!::GetComboBoxInfo(combo, &comboInfo) || !comboInfo.hwndItem) return false;
    FbeComboBoxEdit::SetText(combo, L"abcdef");
    ::SetFocus(comboInfo.hwndItem);
    if (!combo || !FbeComboBoxEdit::SetSelection(combo, 3, 3)) return false;
    int start = 0;
    int end = 0;
    if (!FbeComboBoxEdit::GetSelection(combo, start, end) || start != 3 || end != 3) return false;
    FbeSearchPresets::RegexQuickReferenceEntry entry = {};
    entry.insertionText = L"\\d";
    entry.caretOffset = 2;
    entry.selectionStart = 2;
    const FbeSearchPresets::RegexQuickReferenceInsertion insertion = FbeSearchPresets::InsertRegexQuickReference(FbeComboBoxEdit::GetText(combo), start, end, entry);
    FbeComboBoxEdit::SetText(combo, insertion.text);
    if (!FbeComboBoxEdit::SetSelection(combo, insertion.selectionStart, insertion.selectionStart + insertion.selectionLength)) return false;
    if (FbeComboBoxEdit::GetText(combo) != L"abc\\ddef") return false;
    if (!FbeComboBoxEdit::GetSelection(combo, start, end) || start != 5 || end != 5) return false;

    FbeComboBoxEdit::SetText(combo, L"abcdef");
    if (!FbeComboBoxEdit::SetSelection(combo, 1, 4)) return false;
    if (!FbeComboBoxEdit::GetSelection(combo, start, end) || start != 1 || end != 4) return false;
    entry.insertionText = L"(...)";
    entry.caretOffset = 1;
    entry.selectionStart = 1;
    entry.selectionLength = 0;
    const FbeSearchPresets::RegexQuickReferenceInsertion replacement = FbeSearchPresets::InsertRegexQuickReference(FbeComboBoxEdit::GetText(combo), start, end, entry);
    FbeComboBoxEdit::SetText(combo, replacement.text);
    FbeComboBoxEdit::SetSelection(combo, replacement.selectionStart, replacement.selectionStart + replacement.selectionLength);
    const bool valid = FbeComboBoxEdit::GetText(combo) == L"a(...)ef" && FbeComboBoxEdit::GetSelection(combo, start, end) && start == 2 && end == 2;
    ::DestroyWindow(combo);
    return valid;
}

LRESULT CALLBACK OutsideWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_LBUTTONDOWN)
        ++*reinterpret_cast<int*>(::GetWindowLongPtr(window, GWLP_USERDATA));
    return ::DefWindowProc(window, message, wParam, lParam);
}

bool ShowPopup(HWND owner, HWND anchor, int& inserts, int& fullHelp, HWND& popupWindow, RegexQuickReferencePopup*& popup)
{
    popup = new RegexQuickReferencePopup();
    if (!popup->Show(owner, anchor, FbeSearchPresets::SearchUiContext::Design, FbeSearchPresets::RegexQuickReferenceMode::Search,
        [&inserts](const FbeSearchPresets::RegexQuickReferenceEntry&) { ++inserts; }, [&fullHelp]() { ++fullHelp; })) return false;
    popupWindow = popup->m_hWnd;
    HWND caption = ::GetDlgItem(popupWindow, IDC_REGEX_QUICK_CAPTION);
    HWND left = ::GetDlgItem(popupWindow, IDC_REGEX_QUICK_LEFT);
    HWND right = ::GetDlgItem(popupWindow, IDC_REGEX_QUICK_RIGHT);
    HWND fullHelpButton = ::GetDlgItem(popupWindow, IDC_REGEX_QUICK_FULL_HELP);
    wchar_t captionText[64] = {};
    wchar_t fullHelpText[64] = {};
    if (!Check(::IsWindow(popupWindow)) || !Check(caption) || !Check(left) || !Check(right) || !Check(fullHelpButton) ||
        !Check(::GetWindowText(caption, captionText, _countof(captionText)) > 0) ||
        !Check(::GetWindowText(fullHelpButton, fullHelpText, _countof(fullHelpText)) > 0) ||
        !Check(CString(captionText) == L"Design — Find") || !Check(CString(fullHelpText) == L"Full help...") ||
        !Check(::SendMessage(left, LB_GETCOUNT, 0, 0) > 0)) return false;
    return true;
}

void DispatchSyntheticPointerMessage(RegexQuickReferencePopup& popup, CMessageLoop& messageLoop,
    HWND target, UINT message, const POINT& point)
{
    // Posted mouse moves depend on the active desktop's pointer state and can
    // be coalesced before an ATL message filter sees them. Exercise the same
    // production filter directly with the actual list-relative coordinates;
    // keyboard/focus scenarios below still traverse the normal queue.
    MSG input = {};
    input.hwnd = target;
    input.message = message;
    input.lParam = MAKELPARAM(point.x, point.y);
    if (!popup.PreTranslateMessage(&input))
        ::SendMessage(target, message, 0, input.lParam);
    // Hover selection is applied synchronously by the popup's message filter.
    // Do not drain unrelated post-creation focus notifications here: on a
    // headless desktop they can race this synthetic pointer assertion.
    if (message != WM_MOUSEMOVE)
        DispatchUntilIdle(messageLoop);
}

int FailCheckpoint(int checkpoint, const char* message)
{
    fprintf(stderr, "FAIL checkpoint %d: %s\n", checkpoint, message);
    return checkpoint;
}

bool GetItemCenter(HWND list, int row, POINT& center, RECT& item)
{
    item = {};
    if (::SendMessage(list, LB_GETITEMRECT, row, reinterpret_cast<LPARAM>(&item)) == LB_ERR)
        return false;
    center.x = (item.left + item.right) / 2;
    center.y = (item.top + item.bottom) / 2;
    return true;
}

int FailHoverCheckpoint(HWND list, int expectedRow, const POINT& point, const RECT& item)
{
    const LRESULT itemFromPoint = ::SendMessage(list, LB_ITEMFROMPOINT, 0, MAKELPARAM(point.x, point.y));
    fprintf(stderr, "FAIL checkpoint 16: hover did not select an entry\n");
    fprintf(stderr, "  dpi=%u curSel=%ld itemFromPoint=%ld row=%d outside=%d expectedRow=%d rect=(%ld,%ld)-(%ld,%ld) point=(%ld,%ld)\n",
        UiMetrics::DpiForWindow(list), static_cast<long>(::SendMessage(list, LB_GETCURSEL, 0, 0)),
        static_cast<long>(itemFromPoint), LOWORD(itemFromPoint), HIWORD(itemFromPoint), expectedRow,
        item.left, item.top, item.right, item.bottom, point.x, point.y);
    return 16;
}
int FailFocusCheckpoint(int checkpoint, const char* message, HWND expected, HWND popup, HWND left, HWND right, HWND owner)
{
    const HWND focus = ::GetFocus();
    fprintf(stderr, "FAIL checkpoint %d: %s\n", checkpoint, message);
    fprintf(stderr, "  focus=%p expected=%p popup=%p left=%p right=%p owner=%p IsWindow(popup)=%d IsChild(popup, focus)=%d\n",
        static_cast<void*>(focus), static_cast<void*>(expected), static_cast<void*>(popup), static_cast<void*>(left),
        static_cast<void*>(right), static_cast<void*>(owner), ::IsWindow(popup), ::IsChild(popup, focus));
    return checkpoint;
}

void DispatchUntilIdle(CMessageLoop& messageLoop)
{
    // Some focus notifications are queued by child controls while the key
    // message is dispatched. Drain the queue twice without timing sleeps.
    DispatchMessages(messageLoop);
    DispatchMessages(messageLoop);
}

int TestPopupMessageLoop(HWND owner, HWND anchor, CMessageLoop& messageLoop)
{
    int inserts = 0;
    int fullHelp = 0;
    HWND popupWindow = NULL;
    RegexQuickReferencePopup* popup = NULL;

    if (!ShowPopup(owner, anchor, inserts, fullHelp, popupWindow, popup)) return FailCheckpoint(10, "create-popup-for-Escape");
    ::PostMessage(::GetDlgItem(popupWindow, IDC_REGEX_QUICK_LEFT), WM_KEYDOWN, VK_ESCAPE, 0);
    DispatchUntilIdle(messageLoop);
    if (::IsWindow(popupWindow) || inserts != 0 || fullHelp != 0) return FailCheckpoint(11, "Escape did not close popup");

    if (!ShowPopup(owner, anchor, inserts, fullHelp, popupWindow, popup)) return FailCheckpoint(12, "create-popup-for-Enter");
    ::PostMessage(::GetDlgItem(popupWindow, IDC_REGEX_QUICK_LEFT), WM_KEYDOWN, VK_RETURN, 0);
    DispatchUntilIdle(messageLoop);
    if (::IsWindow(popupWindow) || inserts != 1 || fullHelp != 0) return FailCheckpoint(13, "Enter did not insert and close popup");

    if (!ShowPopup(owner, anchor, inserts, fullHelp, popupWindow, popup)) return FailCheckpoint(14, "create-popup-for-pointer-and-focus");
    const HWND left = ::GetDlgItem(popupWindow, IDC_REGEX_QUICK_LEFT);
    const HWND right = ::GetDlgItem(popupWindow, IDC_REGEX_QUICK_RIGHT);
    // Verify the focus selected by RegexQuickReferencePopup itself before any interaction.
    if (::GetFocus() != left) return FailFocusCheckpoint(15, "popup did not activate left list before keyboard navigation", left, popupWindow, left, right, owner);
    const int headerRow = 0;
    const int entryRow = 1;
    POINT headerCenter = {};
    POINT entryCenter = {};
    RECT headerRect = {};
    RECT entryRect = {};
    if (!GetItemCenter(left, headerRow, headerCenter, headerRect) || !GetItemCenter(left, entryRow, entryCenter, entryRect))
        return FailCheckpoint(16, "could not get list item rectangles");
    DispatchSyntheticPointerMessage(*popup, messageLoop, left, WM_MOUSEMOVE, entryCenter);
    if (::SendMessage(left, LB_GETCURSEL, 0, 0) != entryRow || ::SendMessage(right, LB_GETCURSEL, 0, 0) != LB_ERR || inserts != 1)
        return FailHoverCheckpoint(left, entryRow, entryCenter, entryRect);
    DispatchSyntheticPointerMessage(*popup, messageLoop, left, WM_LBUTTONUP, headerCenter);
    if (!::IsWindow(popupWindow) || inserts != 1) return FailCheckpoint(17, "category/header click inserted or closed popup");
    DispatchSyntheticPointerMessage(*popup, messageLoop, left, WM_MOUSEMOVE, entryCenter);
    ::PostMessage(left, WM_KEYDOWN, VK_RIGHT, 0);
    DispatchUntilIdle(messageLoop);
    if (::GetFocus() != right) return FailFocusCheckpoint(18, "VK_RIGHT did not move focus to right list", right, popupWindow, left, right, owner);
    ::PostMessage(right, WM_KEYDOWN, VK_LEFT, 0);
    DispatchUntilIdle(messageLoop);
    if (::GetFocus() != left) return FailFocusCheckpoint(19, "VK_LEFT did not move focus to left list", left, popupWindow, left, right, owner);
    DispatchSyntheticPointerMessage(*popup, messageLoop, left, WM_LBUTTONUP, entryCenter);
    if (::IsWindow(popupWindow) || inserts != 2) return FailCheckpoint(20, "selected regexp was not inserted");

    if (!ShowPopup(owner, anchor, inserts, fullHelp, popupWindow, popup)) return FailCheckpoint(21, "create-popup-for-Full-help");
    HWND fullHelpButton = ::GetDlgItem(popupWindow, IDC_REGEX_QUICK_FULL_HELP);
    ::PostMessage(popupWindow, WM_COMMAND, MAKEWPARAM(IDC_REGEX_QUICK_FULL_HELP, BN_CLICKED), reinterpret_cast<LPARAM>(fullHelpButton));
    DispatchUntilIdle(messageLoop);
    if (::IsWindow(popupWindow) || inserts != 2 || fullHelp != 1) return FailCheckpoint(22, "Full help button did not close popup");

    if (!ShowPopup(owner, anchor, inserts, fullHelp, popupWindow, popup)) return FailCheckpoint(23, "create-popup-for-F1");
    ::PostMessage(::GetDlgItem(popupWindow, IDC_REGEX_QUICK_LEFT), WM_KEYDOWN, VK_F1, 0);
    DispatchUntilIdle(messageLoop);
    if (::IsWindow(popupWindow) || inserts != 2 || fullHelp != 2) return FailCheckpoint(24, "F1 did not open full help");

    int outsideClicks = 0;
    WNDCLASS windowClass = {};
    windowClass.lpfnWndProc = OutsideWindowProc;
    windowClass.hInstance = _Module.GetModuleInstance();
    windowClass.lpszClassName = L"FBERegexQuickReferenceOutside";
    if (!::RegisterClass(&windowClass) && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return FailCheckpoint(25, "register outside window class");
    HWND outside = ::CreateWindowEx(0, windowClass.lpszClassName, L"outside", WS_CHILD | WS_VISIBLE, 60, 60, 80, 40, owner, NULL, _Module.GetModuleInstance(), NULL);
    if (!outside) return FailCheckpoint(26, "create outside window");
    ::SetWindowLongPtr(outside, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&outsideClicks));
    if (!ShowPopup(owner, anchor, inserts, fullHelp, popupWindow, popup)) { ::DestroyWindow(outside); return FailCheckpoint(27, "create-popup-for-outside-click"); }
    ::PostMessage(outside, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(4, 4));
    DispatchUntilIdle(messageLoop);
    const bool outsideResult = !::IsWindow(popupWindow) && outsideClicks == 1;
    ::DestroyWindow(outside);
    return outsideResult ? 0 : FailCheckpoint(28, "outside click did not close popup");
}

}

int wmain()
{
    INITCOMMONCONTROLSEX controls = { sizeof(controls), ICC_WIN95_CLASSES };
    if (!::InitCommonControlsEx(&controls)) return 1;
    _Module.Init(NULL, ::GetModuleHandle(NULL));
    CMessageLoop messageLoop;
    if (!_Module.AddMessageLoop(&messageLoop)) { _Module.Term(); return 1; }
    HWND owner = ::CreateWindowEx(0, WC_STATIC, L"owner", WS_OVERLAPPEDWINDOW, 0, 0, 320, 200, NULL, NULL, _Module.GetModuleInstance(), NULL);
    HWND anchor = ::CreateWindowEx(0, WC_BUTTON, L"?", WS_CHILD | WS_VISIBLE, 10, 10, 20, 20, owner, NULL, _Module.GetModuleInstance(), NULL);
    if (owner) ::ShowWindow(owner, SW_SHOW);
    int result = 0;
    if (!owner || !anchor) result = 1;
    else if (!TestComboInsertion(owner)) result = 2;
    else result = TestPopupMessageLoop(owner, anchor, messageLoop);
    if (owner) ::DestroyWindow(owner);
    _Module.RemoveMessageLoop();
    _Module.Term();
    return result;
}
