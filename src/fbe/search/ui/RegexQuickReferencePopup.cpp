#include "stdafx.h"
#include "RegexQuickReferencePopup.h"
#include "..\\..\\RuntimeLocalization.h"
#include "..\\..\\UiMetrics.h"
#include "..\\..\\ThemeManager.h"

RegexQuickReferencePopup::RegexQuickReferencePopup() : m_context(FbeSearchPresets::SearchUiContext::Design), m_mode(FbeSearchPresets::RegexQuickReferenceMode::Search), m_messageLoop(NULL), m_monospaceFont(NULL), m_syntaxColumnWidth(0) {}
RegexQuickReferencePopup::~RegexQuickReferencePopup() { if (m_monospaceFont != NULL) ::DeleteObject(m_monospaceFont); }

LRESULT RegexQuickReferencePopup::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
    RECT client = {}; GetClientRect(&client);
    const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
    const int border = (std::max)(1, UiMetrics::ScaleForDpi(1, dpi));
    const int gap = UiMetrics::ScaleForDpi(6, dpi);
    const int inset = border + gap;
    const int dividerWidth = (std::max)(1, UiMetrics::ScaleForDpi(1, dpi));
    const int captionHeight = UiMetrics::ScaleForDpi(18, dpi);
    const int buttonHeight = UiMetrics::ScaleForDpi(22, dpi);
    CRect captionRect(inset, inset, client.right - inset, inset + captionHeight);
    m_caption.Create(m_hWnd, captionRect, Caption(), WS_CHILD | WS_VISIBLE, 0, IDC_REGEX_QUICK_CAPTION);
    const int middle = client.right / 2;
    const DWORD listStyle = WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS;
    CRect leftRect(inset, inset + captionHeight, middle - dividerWidth, client.bottom - buttonHeight - inset * 2);
    CRect rightRect(middle + dividerWidth, inset + captionHeight, client.right - inset, client.bottom - buttonHeight - inset * 2);
    CRect fullHelpRect(inset, client.bottom - buttonHeight - inset, client.right - inset, client.bottom - inset);
    m_left.Create(m_hWnd, leftRect, NULL, listStyle, 0, IDC_REGEX_QUICK_LEFT);
    m_right.Create(m_hWnd, rightRect, NULL, listStyle, 0, IDC_REGEX_QUICK_RIGHT);
    m_fullHelp.Create(m_hWnd, fullHelpRect, FbeLoadRuntimeStringByKey(L"fbe.regex_quick.full_help", L"Full help..."), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, IDC_REGEX_QUICK_FULL_HELP);
    const HFONT font = UiMetrics::DialogFont();
    m_monospaceFont = ::CreateFontW(-::MulDiv(9, static_cast<int>(dpi), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    if (m_monospaceFont == NULL)
        m_monospaceFont = ::CreateFontW(-::MulDiv(9, static_cast<int>(dpi), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Lucida Console");
    m_caption.SetFont(font); m_left.SetFont(font); m_right.SetFont(font); m_fullHelp.SetFont(font);
    HDC dc = ::GetDC(m_hWnd); HFONT old = m_monospaceFont ? static_cast<HFONT>(::SelectObject(dc, m_monospaceFont)) : NULL;
    SIZE extent = {}; int widest = 0;
    for (size_t index = 0; index < m_entries.size(); ++index) { ::GetTextExtentPoint32(dc, m_entries[index].displaySyntax, m_entries[index].displaySyntax.GetLength(), &extent); widest = (std::max)(widest, static_cast<int>(extent.cx)); }
    if (old) ::SelectObject(dc, old); ::ReleaseDC(m_hWnd, dc);
    const int listWidth = (std::max)(1, middle - gap - dividerWidth);
    m_syntaxColumnWidth = (std::max)(listWidth * 25 / 100, (std::min)(listWidth * 38 / 100, widest + gap * 2));
    std::vector<int> leftIndexes, rightIndexes;
    for(size_t index = 0; index < m_entries.size(); ++index) {
        const bool characters = m_entries[index].category == FbeSearchPresets::RegexQuickReferenceCategory::Characters;
        if(m_mode == FbeSearchPresets::RegexQuickReferenceMode::Search ? characters : index < (m_entries.size() + 1) / 2)
            leftIndexes.push_back(static_cast<int>(index));
        else
            rightIndexes.push_back(static_cast<int>(index));
    }
    AddRows(m_left, m_leftRows, leftIndexes); AddRows(m_right, m_rightRows, rightIndexes);
    m_toolTip = ::CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, m_hWnd, NULL, _Module.GetModuleInstance(), NULL);
    if (m_toolTip != NULL) { AddDescriptionToolTip(m_left); AddDescriptionToolTip(m_right); }
    const int leftFirst = FirstEntryRow(m_leftRows); const int rightFirst = FirstEntryRow(m_rightRows);
    if(leftFirst >= 0) { m_left.SetCurSel(leftFirst); m_left.SetFocus(); }
    else if(rightFirst >= 0) { m_right.SetCurSel(rightFirst); m_right.SetFocus(); }
    ThemeManager::ApplyToWindow(m_hWnd);
    return 0;
}

void RegexQuickReferencePopup::AddRows(CListBox& list, std::vector<int>& rows, const std::vector<int>& indexes) {
    rows.clear();
    FbeSearchPresets::RegexQuickReferenceCategory category = static_cast<FbeSearchPresets::RegexQuickReferenceCategory>(-1);
    for(size_t i = 0; i < indexes.size(); ++i) {
        const int index = indexes[i];
        if(m_entries[index].category != category) {
            category = m_entries[index].category;
            CString heading;
            heading.Format(L"— %s —", static_cast<LPCWSTR>(CategoryCaption(category)));
            list.AddString(heading);
            rows.push_back(-1);
        }
        // The description is drawn in its own column.  Do not concatenate it
        // with the syntax: its font and clipping rules are intentionally separate.
        list.AddString(m_entries[index].displaySyntax);
        rows.push_back(index);
    }
}

void RegexQuickReferencePopup::DrawListItem(const DRAWITEMSTRUCT& draw, const std::vector<int>& rows) {
    if(draw.itemID == static_cast<UINT>(-1) || draw.itemID >= rows.size()) return;
    const int entryIndex = rows[draw.itemID];
    HDC dc = draw.hDC;
    RECT row = draw.rcItem;
    const bool heading = entryIndex < 0;
    const bool selected = !heading && (draw.itemState & ODS_SELECTED) != 0;
    const COLORREF background = selected ? ThemeManager::SelectionBackgroundColor() : ThemeManager::WindowColor();
    ::FillRect(dc, &row, ThemeManager::Brush(selected ? THEME_COLOR_SELECTION_BACKGROUND : THEME_COLOR_WINDOW));
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, selected ? ThemeManager::SelectionTextColor() : (heading ? ThemeManager::SecondaryTextColor() : ThemeManager::TextColor()));
    RECT text = row;
    const int padding = UiMetrics::ScaleForDpi(5, UiMetrics::DpiForWindow(m_hWnd));
    ::InflateRect(&text, -padding, 0);
    if(heading) {
        CString headingText;
        const HWND list = draw.CtlID == IDC_REGEX_QUICK_LEFT ? m_left : m_right;
        const int length = ::GetWindowTextLength(list); // retained only for the control's standard text lifetime
        (void)length;
        // Headers are kept in the list text; retrieve it without inventing a selectable entry.
        const int count = static_cast<int>(::SendMessage(draw.hwndItem, LB_GETTEXTLEN, draw.itemID, 0));
        std::vector<wchar_t> buffer(static_cast<size_t>((std::max)(0, count)) + 1);
        ::SendMessage(draw.hwndItem, LB_GETTEXT, draw.itemID, reinterpret_cast<LPARAM>(&buffer[0]));
        headingText = &buffer[0];
        ::DrawText(dc, headingText, headingText.GetLength(), &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    } else if(static_cast<size_t>(entryIndex) < m_entries.size()) {
        const FbeSearchPresets::RegexQuickReferenceEntry& entry = m_entries[entryIndex];
        RECT syntax = text;
        syntax.right = (std::min)(text.right, syntax.left + m_syntaxColumnWidth);
        HFONT old = static_cast<HFONT>(::SelectObject(dc, m_monospaceFont ? m_monospaceFont : UiMetrics::DialogFont()));
        ::DrawText(dc, entry.displaySyntax, entry.displaySyntax.GetLength(), &syntax, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        ::SelectObject(dc, old);
        RECT description = text;
        description.left = syntax.right + padding;
        const CString descriptionText = FbeLoadRuntimeStringByKey(entry.descriptionKey, entry.descriptionFallback);
        ::DrawText(dc, descriptionText, descriptionText.GetLength(), &description, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    if (heading) {
        HPEN pen = ::CreatePen(PS_SOLID, 1, ThemeManager::SeparatorColor());
        HGDIOBJ oldPen = ::SelectObject(dc, pen);
        ::MoveToEx(dc, row.left, row.bottom - 1, NULL); ::LineTo(dc, row.right, row.bottom - 1);
        ::SelectObject(dc, oldPen); ::DeleteObject(pen);
    }    (void)background;
}

LRESULT RegexQuickReferencePopup::OnDrawItem(UINT, WPARAM, LPARAM data, BOOL&) {
    const DRAWITEMSTRUCT* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(data);
    if(!draw || (draw->CtlID != IDC_REGEX_QUICK_LEFT && draw->CtlID != IDC_REGEX_QUICK_RIGHT)) return 0;
    DrawListItem(*draw, draw->CtlID == IDC_REGEX_QUICK_LEFT ? m_leftRows : m_rightRows);
    return TRUE;
}

void RegexQuickReferencePopup::AddDescriptionToolTip(HWND list) {
    TOOLINFOW tool = {}; tool.cbSize = sizeof(tool); tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    tool.hwnd = m_hWnd; tool.uId = reinterpret_cast<UINT_PTR>(list); tool.lpszText = LPSTR_TEXTCALLBACKW;
    ::SendMessage(m_toolTip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool));
}

bool RegexQuickReferencePopup::DescriptionIsTruncated(HWND list, int row, const std::vector<int>& rows, CString& text) const {
    text.Empty(); if (row < 0 || static_cast<size_t>(row) >= rows.size() || rows[row] < 0) return false;
    const int entryIndex = rows[row]; if (static_cast<size_t>(entryIndex) >= m_entries.size()) return false;
    const FbeSearchPresets::RegexQuickReferenceEntry& entry = m_entries[entryIndex];
    const CString description = FbeLoadRuntimeStringByKey(entry.descriptionKey, entry.descriptionFallback);
    RECT item = {}; if (::SendMessage(list, LB_GETITEMRECT, row, reinterpret_cast<LPARAM>(&item)) == LB_ERR) return false;
    const int padding = UiMetrics::ScaleForDpi(5, UiMetrics::DpiForWindow(m_hWnd));
    const int available = item.right - item.left - padding * 3 - m_syntaxColumnWidth;
    HDC dc = ::GetDC(list); HFONT old = static_cast<HFONT>(::SelectObject(dc, UiMetrics::DialogFont())); SIZE extent = {};
    ::GetTextExtentPoint32(dc, description, description.GetLength(), &extent); if (old) ::SelectObject(dc, old); ::ReleaseDC(list, dc);
    if (available <= 0 || extent.cx <= available) return false;
    text.Format(L"%s — %s", static_cast<LPCWSTR>(entry.displaySyntax), static_cast<LPCWSTR>(description));
    return true;
}

LRESULT RegexQuickReferencePopup::OnToolTipGetDispInfo(int, LPNMHDR header, BOOL&) {
    NMTTDISPINFOW* notification = reinterpret_cast<NMTTDISPINFOW*>(header);
    const HWND list = reinterpret_cast<HWND>(header->idFrom);
    const std::vector<int>* rows = list == m_left ? &m_leftRows : list == m_right ? &m_rightRows : NULL;
    if (rows == NULL) return 0;
    POINT point = {}; ::GetCursorPos(&point); ::ScreenToClient(list, &point);
    const LRESULT rowResult = ::SendMessage(list, LB_ITEMFROMPOINT, 0, MAKELPARAM(point.x, point.y));
    const int row = LOWORD(rowResult);
    const BOOL outside = HIWORD(rowResult) != 0;
    notification->lpszText = !outside && DescriptionIsTruncated(list, row, *rows, m_tooltipText)
        ? const_cast<LPWSTR>(static_cast<LPCWSTR>(m_tooltipText)) : const_cast<LPWSTR>(L"");
    return 0;
}
LRESULT RegexQuickReferencePopup::OnMeasureItem(UINT, WPARAM, LPARAM data, BOOL&) {
    MEASUREITEMSTRUCT* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(data);
    if(!measure || (measure->CtlID != IDC_REGEX_QUICK_LEFT && measure->CtlID != IDC_REGEX_QUICK_RIGHT)) return 0;
    measure->itemHeight = UiMetrics::ScaleForDpi(20, UiMetrics::DpiForWindow(m_hWnd));
    return TRUE;
}

bool RegexQuickReferencePopup::Show(HWND owner, HWND anchor, FbeSearchPresets::SearchUiContext context, FbeSearchPresets::RegexQuickReferenceMode mode, const std::function<void(const FbeSearchPresets::RegexQuickReferenceEntry&)>& insert, const std::function<void()>& fullHelp) {
    m_insert = insert; m_openFullHelp = fullHelp; m_context = context; m_mode = mode; FbeSearchPresets::GetRegexQuickReferenceEntries(context, mode, m_entries); if(m_entries.empty()) return false;
    const UINT dpi = UiMetrics::DpiForWindow(anchor); RECT rc = {}; ::GetWindowRect(anchor, &rc); HMONITOR monitor = MonitorFromWindow(anchor, MONITOR_DEFAULTTONEAREST); MONITORINFO info = { sizeof(info) }; GetMonitorInfo(monitor, &info);
    const int workMargin = UiMetrics::ScaleForDpi(12, dpi);
    const int maxWidth = max(1, info.rcWork.right - info.rcWork.left - workMargin * 2);
    const int maxHeight = max(1, info.rcWork.bottom - info.rcWork.top - workMargin * 2);
    const int width = min(UiMetrics::ScaleForDpi(560, dpi), maxWidth);
    const int height = min(DesiredPopupHeight(dpi), maxHeight);
    int x = rc.right + width <= info.rcWork.right ? rc.right : rc.left - width; int y = rc.bottom + height <= info.rcWork.bottom ? rc.bottom : rc.top - height;
    x = max(info.rcWork.left, min(x, info.rcWork.right - width)); y = max(info.rcWork.top, min(y, info.rcWork.bottom - height));
    CRect popupRect(x, y, x + width, y + height);
    HWND hwnd = Create(owner, popupRect, NULL, WS_POPUP, WS_EX_TOOLWINDOW);
    if(hwnd == NULL) return false;
    m_messageLoop = _Module.GetMessageLoop();
    if(m_messageLoop) m_messageLoop->AddMessageFilter(this);
    ShowWindow(SW_SHOW); UpdateWindow(); return true;
}

int RegexQuickReferencePopup::DesiredPopupHeight(UINT dpi) const
{
    std::vector<int> leftIndexes, rightIndexes;
    for (size_t index = 0; index < m_entries.size(); ++index)
    {
        const bool characters = m_entries[index].category == FbeSearchPresets::RegexQuickReferenceCategory::Characters;
        if (m_mode == FbeSearchPresets::RegexQuickReferenceMode::Search ? characters : index < (m_entries.size() + 1) / 2)
            leftIndexes.push_back(static_cast<int>(index));
        else
            rightIndexes.push_back(static_cast<int>(index));
    }
    const auto countRows = [this](const std::vector<int>& indexes) {
        int rows = 0; int previous = -1;
        for (size_t index = 0; index < indexes.size(); ++index)
        {
            const int category = static_cast<int>(m_entries[indexes[index]].category);
            if (category != previous) { ++rows; previous = category; }
            ++rows;
        }
        return rows;
    };
    const int border = (std::max)(1, UiMetrics::ScaleForDpi(1, dpi));
    const int gap = UiMetrics::ScaleForDpi(6, dpi);
    const int captionHeight = UiMetrics::ScaleForDpi(18, dpi);
    const int buttonHeight = UiMetrics::ScaleForDpi(22, dpi);
    const int rowHeight = UiMetrics::ScaleForDpi(20, dpi);
    const int rows = (std::max)(countRows(leftIndexes), countRows(rightIndexes));
    // ListBox reserves its own border and can round the client area down at fractional DPI.
    // Reserve one extra row plus chrome so a fully fitting catalog does not show a needless scrollbar.
    const int listChrome = border * 2 + UiMetrics::ScaleForDpi(3, dpi);
    const int reserve = UiMetrics::ScaleForDpi(4, dpi);
    return border * 2 + gap * 3 + captionHeight + buttonHeight + listChrome + reserve + rowHeight * (std::max)(1, rows);
}

LRESULT RegexQuickReferencePopup::OnPaint(UINT, WPARAM, LPARAM, BOOL&)
{
    PAINTSTRUCT paint = {}; HDC dc = ::BeginPaint(m_hWnd, &paint);
    RECT client = {}; ::GetClientRect(m_hWnd, &client);
    const UINT dpi = UiMetrics::DpiForWindow(m_hWnd);
    const int border = (std::max)(1, UiMetrics::ScaleForDpi(1, dpi));
    const int gap = UiMetrics::ScaleForDpi(6, dpi);
    const int inset = border + gap;
    const int captionHeight = UiMetrics::ScaleForDpi(18, dpi);
    const int buttonHeight = UiMetrics::ScaleForDpi(22, dpi);
    ::FillRect(dc, &client, ThemeManager::WindowBrush());
    HBRUSH borderBrush = ::CreateSolidBrush(ThemeManager::SeparatorColor());
    ::FrameRect(dc, &client, borderBrush);
    ::DeleteObject(borderBrush);
    RECT separator = { client.right / 2, inset + captionHeight, client.right / 2 + border, client.bottom - buttonHeight - inset * 2 };
    ::FillRect(dc, &separator, ThemeManager::Brush(THEME_COLOR_SEPARATOR));
    ::EndPaint(m_hWnd, &paint);
    return 0;
}


CString RegexQuickReferencePopup::Caption() const {
    const bool source = m_context == FbeSearchPresets::SearchUiContext::Source;
    const bool replacement = m_mode == FbeSearchPresets::RegexQuickReferenceMode::Replacement;
    return FbeLoadRuntimeStringByKey(source ? (replacement ? L"fbe.regex_quick.source.replace" : L"fbe.regex_quick.source.search") : (replacement ? L"fbe.regex_quick.design.replace" : L"fbe.regex_quick.design.search"), source ? (replacement ? L"Source — Replace" : L"Source — Find") : (replacement ? L"Design — Replace" : L"Design — Find"));
}

CString RegexQuickReferencePopup::CategoryCaption(FbeSearchPresets::RegexQuickReferenceCategory category) const {
    switch(category) {
    case FbeSearchPresets::RegexQuickReferenceCategory::Characters: return FbeLoadRuntimeStringByKey(L"fbe.regex_quick.category.characters", L"Characters and anchors");
    case FbeSearchPresets::RegexQuickReferenceCategory::Quantifiers: return FbeLoadRuntimeStringByKey(L"fbe.regex_quick.category.quantifiers", L"Quantifiers");
    case FbeSearchPresets::RegexQuickReferenceCategory::Groups: return FbeLoadRuntimeStringByKey(L"fbe.regex_quick.category.groups", L"Groups and assertions");
    case FbeSearchPresets::RegexQuickReferenceCategory::Options: return FbeLoadRuntimeStringByKey(L"fbe.regex_quick.category.options", L"Inline options");
    default: return FbeLoadRuntimeStringByKey(L"fbe.regex_quick.category.replacement", L"Replacement");
    }
}

BOOL RegexQuickReferencePopup::PreTranslateMessage(MSG* message) {
    const bool leftList = message->hwnd == m_left.m_hWnd;
    const bool rightList = message->hwnd == m_right.m_hWnd;
    if(message->message == WM_LBUTTONUP && (leftList || rightList)) {
        POINT point = { GET_X_LPARAM(message->lParam), GET_Y_LPARAM(message->lParam) };
        ActivateAtPoint(message->hwnd, point);
        return TRUE;
    }
    if(message->message == WM_MOUSEMOVE && (leftList || rightList)) {
        POINT point = { GET_X_LPARAM(message->lParam), GET_Y_LPARAM(message->lParam) };
        UpdateHoverSelection(message->hwnd, point);
        TRACKMOUSEEVENT tracking = { sizeof(tracking), TME_LEAVE, message->hwnd, 0 }; ::TrackMouseEvent(&tracking);
        // Hover is fully handled above. Letting the native list box process
        // the same move afterward can clear the selection on some desktops.
        return TRUE;
    }
    if(message->message == WM_LBUTTONDOWN || message->message == WM_RBUTTONDOWN || message->message == WM_MBUTTONDOWN || message->message == WM_NCLBUTTONDOWN) {
        if(message->hwnd != m_hWnd && !::IsChild(m_hWnd, message->hwnd)) DestroyWindow();
        return FALSE;
    }
    if(message->message != WM_KEYDOWN) return FALSE;
    if(message->wParam == VK_ESCAPE) { DestroyWindow(); return TRUE; }
    if(message->wParam == VK_RETURN) { Activate(); return TRUE; }
    if(message->wParam == VK_F1) { BOOL ignored = FALSE; OnFullHelp(0, 0, NULL, ignored); return TRUE; }
    if(message->wParam == VK_LEFT) { MoveColumn(false); return TRUE; }
    if(message->wParam == VK_RIGHT) { MoveColumn(true); return TRUE; }
    if(message->wParam == VK_UP || message->wParam == VK_DOWN || message->wParam == VK_HOME || message->wParam == VK_END) {
        if(message->hwnd == m_left || message->hwnd == m_right) {
            const int direction = message->wParam == VK_UP ? -1 : message->wParam == VK_DOWN ? 1 : message->wParam == VK_HOME ? -2 : 2;
            return MoveSelection(message->hwnd, direction) ? TRUE : FALSE;
        }
    }
    return FALSE;
}
int RegexQuickReferencePopup::FirstEntryRow(const std::vector<int>& rows) const { for (size_t index = 0; index < rows.size(); ++index) if (rows[index] >= 0) return static_cast<int>(index); return -1; }

void RegexQuickReferencePopup::ClearOtherSelection(HWND listWindow)
{
    if(listWindow == m_left) m_right.SetCurSel(-1); else if(listWindow == m_right) m_left.SetCurSel(-1);
}

bool RegexQuickReferencePopup::UpdateHoverSelection(HWND listWindow, POINT point)
{
    if (listWindow != m_left.m_hWnd && listWindow != m_right.m_hWnd) return false;
    const bool rightList = listWindow == m_right.m_hWnd;
    const std::vector<int>& rows = rightList ? m_rightRows : m_leftRows;
    CListBox& list = rightList ? m_right : m_left;
    const LRESULT item = ::SendMessage(listWindow, LB_ITEMFROMPOINT, 0, MAKELPARAM(point.x, point.y));
    const int row = LOWORD(item);
    if(HIWORD(item) != 0 || row < 0 || static_cast<size_t>(row) >= rows.size() || rows[row] < 0) return false;
    if (list.GetCurSel() != row && list.SetCurSel(row) == LB_ERR) return false;
    if (list.GetCurSel() != row) return false;
    ClearOtherSelection(listWindow);
    return true;
}

bool RegexQuickReferencePopup::ActivateAtPoint(HWND listWindow, POINT point)
{
    const std::vector<int>& rows = listWindow == m_right ? m_rightRows : m_leftRows;
    const LRESULT item = ::SendMessage(listWindow, LB_ITEMFROMPOINT, 0, MAKELPARAM(point.x, point.y));
    const int row = LOWORD(item);
    CListBox& list = listWindow == m_right ? m_right : m_left;
    if(HIWORD(item) != 0 || row < 0 || static_cast<size_t>(row) >= rows.size() || rows[row] < 0) { list.SetCurSel(-1); return false; }
    list.SetCurSel(row); ClearOtherSelection(listWindow); list.SetFocus(); Activate(); return true;
}

bool RegexQuickReferencePopup::MoveSelection(HWND listWindow, int direction)
{
    CListBox& list = listWindow == m_right ? m_right : m_left;
    const std::vector<int>& rows = listWindow == m_right ? m_rightRows : m_leftRows;
    if (rows.empty()) return false;
    int row = list.GetCurSel();
    if (direction == -2) row = FirstEntryRow(rows);
    else if (direction == 2) { row = static_cast<int>(rows.size()) - 1; while (row >= 0 && rows[row] < 0) --row; }
    else { if (row < 0) row = direction > 0 ? -1 : static_cast<int>(rows.size()); do { row += direction; } while (row >= 0 && row < static_cast<int>(rows.size()) && rows[row] < 0); }
    if (row < 0 || row >= static_cast<int>(rows.size()) || rows[row] < 0) return false;
    list.SetCurSel(row); ClearOtherSelection(listWindow); list.SetFocus(); return true;
}

void RegexQuickReferencePopup::MoveColumn(bool right) { CListBox& destination = right ? m_right : m_left; const std::vector<int>& rows = right ? m_rightRows : m_leftRows; const int first = FirstEntryRow(rows); if(first < 0) return; destination.SetCurSel(first); ClearOtherSelection(destination); destination.SetFocus(); }
void RegexQuickReferencePopup::Activate() { CListBox& list = ::GetFocus() == m_right.m_hWnd ? m_right : m_left; std::vector<int>& rows = ::GetFocus() == m_right.m_hWnd ? m_rightRows : m_leftRows; const int row = list.GetCurSel(); const int index = row >= 0 && static_cast<size_t>(row) < rows.size() ? rows[row] : -1; if(index >= 0 && static_cast<size_t>(index) < m_entries.size() && m_insert) { const FbeSearchPresets::RegexQuickReferenceEntry entry = m_entries[index]; const std::function<void(const FbeSearchPresets::RegexQuickReferenceEntry&)> callback = m_insert; DestroyWindow(); callback(entry); } }
LRESULT RegexQuickReferencePopup::OnActivate(WORD, WORD, HWND, BOOL&) { Activate(); return 0; }
LRESULT RegexQuickReferencePopup::OnFullHelp(WORD, WORD, HWND, BOOL&) { const std::function<void()> callback = m_openFullHelp; DestroyWindow(); if(callback) callback(); return 0; }
LRESULT RegexQuickReferencePopup::OnKeyDown(UINT, WPARAM key, LPARAM, BOOL&) { if(key == VK_ESCAPE) DestroyWindow(); else if(key == VK_RETURN) Activate(); else if(key == VK_F1) { BOOL ignored = FALSE; OnFullHelp(0, 0, NULL, ignored); } return 0; }
LRESULT RegexQuickReferencePopup::OnKillFocus(UINT, WPARAM nextFocus, LPARAM, BOOL&)
{
    // WM_KILLFOCUS supplies the destination HWND. GetFocus() is transient at
    // this point and may still be this popup (or NULL), which made child
    // list-box focus transitions close the popup depending on message timing.
    const HWND focus = reinterpret_cast<HWND>(nextFocus);
    if (focus != m_hWnd && !::IsChild(m_hWnd, focus))
        PostMessage(WM_CLOSE);
    return 0;
}
LRESULT RegexQuickReferencePopup::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&) { ThemeManager::ApplyToWindow(m_hWnd); m_left.Invalidate(); m_right.Invalidate(); m_caption.Invalidate(); m_fullHelp.Invalidate(); return 0; }
LRESULT RegexQuickReferencePopup::OnNcDestroy(UINT, WPARAM, LPARAM, BOOL& handled) { if(m_toolTip != NULL) { ::DestroyWindow(m_toolTip); m_toolTip = NULL; } if(m_messageLoop) { m_messageLoop->RemoveMessageFilter(this); m_messageLoop = NULL; } handled = FALSE; return 0; }
