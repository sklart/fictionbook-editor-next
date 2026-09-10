#include "stdafx.h"
#include "ScriptsToolbarCustomizeDlg.h"
#include "Settings.h"
#include "RuntimeLocalization.h"
#include "UiMetrics.h"

namespace
{
	const DWORD_PTR kSeparatorItem = static_cast<DWORD_PTR>(-1);
}

CScriptsToolbarCustomizeDlg::CScriptsToolbarCustomizeDlg(HWND toolbar,
	const std::vector<ScriptsToolbarCommand>& available, const CSimpleArray<TBBUTTON>& defaults,
	CSettings& settings) : m_toolbar(toolbar), m_available(available), m_defaults(defaults), m_settings(settings), m_dialogFont(NULL), m_dpi(96), m_dragging(false), m_dragSource(-1), m_dragInsert(-1), m_dragScrollDirection(0)
{
}

CScriptsToolbarCustomizeDlg::~CScriptsToolbarCustomizeDlg()
{
	if(m_dialogFont != NULL) ::DeleteObject(m_dialogFont);
}

LRESULT CScriptsToolbarCustomizeDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	UpdateMetrics();
	FbeApplyRuntimeDialogLocalization(m_hWnd, IDD);
	SetWindowText(FbeLoadRuntimeStringByKey(L"fbe.scripts_toolbar_customize.caption", L"Customize scripts toolbar"));
	const struct { UINT id; LPCWSTR key; LPCWSTR fallback; } labels[] = {
		{ IDC_SCRIPTS_TOOLBAR_SEARCH_LABEL, L"fbe.scripts_toolbar_customize.search", L"Search:" },
		{ IDC_SCRIPTS_TOOLBAR_AVAILABLE_LABEL, L"fbe.scripts_toolbar_customize.available", L"Available scripts:" },
		{ IDC_SCRIPTS_TOOLBAR_CURRENT_LABEL, L"fbe.scripts_toolbar_customize.current", L"Toolbar buttons:" },
		{ IDC_SCRIPTS_TOOLBAR_ADD, L"fbe.scripts_toolbar_customize.add", L"Add >" },
		{ IDC_SCRIPTS_TOOLBAR_REMOVE, L"fbe.scripts_toolbar_customize.remove", L"< Remove" },
		{ IDC_SCRIPTS_TOOLBAR_UP, L"fbe.scripts_toolbar_customize.up", L"Up" },
		{ IDC_SCRIPTS_TOOLBAR_DOWN, L"fbe.scripts_toolbar_customize.down", L"Down" },
		{ IDC_SCRIPTS_TOOLBAR_RESET, L"fbe.scripts_toolbar_customize.reset", L"Reset" },
		{ IDCANCEL, L"fbe.scripts_toolbar_customize.close", L"Close" }
	};
	for(int i = 0; i < _countof(labels); ++i) ::SetWindowText(GetDlgItem(labels[i].id), FbeLoadRuntimeStringByKey(labels[i].key, labels[i].fallback));
	::SetDlgItemText(m_hWnd, IDCANCEL, FbeLoadRuntimeStringByKey(L"fbe.scripts_toolbar_customize.close", L"Close"));
	m_availableList = GetDlgItem(IDC_SCRIPTS_TOOLBAR_AVAILABLE);
	m_currentList = GetDlgItem(IDC_SCRIPTS_TOOLBAR_CURRENT);
	::SetWindowSubclass(m_currentList, CurrentListSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
	m_toolTip.Create(m_hWnd); m_toolTip.Activate(TRUE);
	m_toolTip.AddTool(m_availableList, LPSTR_TEXTCALLBACKW, NULL, 1);
	m_toolTip.AddTool(m_currentList, LPSTR_TEXTCALLBACKW, NULL, 2);
	CRect client;
	RestorePlacement();
	GetClientRect(client); LayoutControls(client.Width(), client.Height());
	PopulateAvailable(); PopulateCurrent();
	UpdateButtonState();
	return TRUE;
}

void CScriptsToolbarCustomizeDlg::PopulateAvailable()
{
	CString search; GetDlgItemText(IDC_SCRIPTS_TOOLBAR_SEARCH, search); search.MakeLower();
	m_availableList.ResetContent();
	const CString separator = FbeLoadRuntimeStringByKey(L"fbe.scripts_toolbar_customize.separator", L"--- Separator ---");
	CString separatorProbe(separator); separatorProbe.MakeLower();
	if(search.IsEmpty() || separatorProbe.Find(search) >= 0) {
		const int row = m_availableList.AddString(separator);
		m_availableList.SetItemData(row, kSeparatorItem);
	}
	for(size_t i = 0; i < m_available.size(); ++i) {
		if(ToolbarContainsCommand(m_available[i].command)) continue;
		CString name(m_available[i].name); CString path(m_available[i].relativePath); CString probe(name + L"\n" + path); probe.MakeLower();
		if(!search.IsEmpty() && probe.Find(search) < 0) continue;
		const int row = m_availableList.AddString(name); m_availableList.SetItemData(row, static_cast<DWORD_PTR>(i));
	}
}

void CScriptsToolbarCustomizeDlg::PopulateCurrent(int select)
{
	m_currentList.ResetContent(); CToolBarCtrl toolbar = m_toolbar;
	for(int i = 0; i < toolbar.GetButtonCount(); ++i) {
		TBBUTTON button = {}; if(!toolbar.GetButton(i, &button)) continue;
		if(button.fsStyle & TBSTYLE_SEP) {
			const int row = m_currentList.AddString(FbeLoadRuntimeStringByKey(L"fbe.scripts_toolbar_customize.separator", L"--- Separator ---"));
			m_currentList.SetItemData(row, static_cast<DWORD_PTR>(i));
			continue;
		}
		CString name;
		for(size_t j = 0; j < m_available.size(); ++j) if(m_available[j].command == button.idCommand) { name = m_available[j].name; break; }
		if(name.IsEmpty()) name.Format(FbeLoadRuntimeStringByKey(
			L"fbe.scripts_toolbar_customize.unknown_command", L"Command %d"), button.idCommand);
		const int row = m_currentList.AddString(name); m_currentList.SetItemData(row, static_cast<DWORD_PTR>(i));
	}
	if(select >= 0 && select < m_currentList.GetCount()) m_currentList.SetCurSel(select);
}

bool CScriptsToolbarCustomizeDlg::ToolbarContainsCommand(int command) const
{
	CToolBarCtrl toolbar = m_toolbar;
	for(int i = 0; i < toolbar.GetButtonCount(); ++i) {
		TBBUTTON button = {};
		if(toolbar.GetButton(i, &button) && !(button.fsStyle & TBSTYLE_SEP) && button.idCommand == command)
			return true;
	}
	return false;
}

int CScriptsToolbarCustomizeDlg::SelectedAvailableCommand() const
{
	const int row = m_availableList.GetCurSel(); if(row < 0) return 0;
	const DWORD_PTR item = m_availableList.GetItemData(row);
	if(item == kSeparatorItem) return -1;
	return item < m_available.size() ? m_available[item].command : 0;
}

LRESULT CScriptsToolbarCustomizeDlg::OnSearchChanged(WORD, WORD, HWND, BOOL&) { PopulateAvailable(); UpdateButtonState(); return 0; }
LRESULT CScriptsToolbarCustomizeDlg::OnSelectionChanged(WORD, WORD, HWND, BOOL&) { UpdateButtonState(); return 0; }
LRESULT CScriptsToolbarCustomizeDlg::OnToolTipText(int, LPNMHDR hdr, BOOL& bHandled)
{
	LPNMTTDISPINFO info = reinterpret_cast<LPNMTTDISPINFO>(hdr);
	POINT point = {}; ::GetCursorPos(&point);
	CListBox list = info->hdr.idFrom == 1 ? m_availableList : m_currentList;
	list.ScreenToClient(&point); BOOL outside = FALSE; const int row = list.ItemFromPoint(point, outside);
	if(outside || row < 0) { bHandled = FALSE; return 0; }
	m_toolTipText.Empty();
	if(info->hdr.idFrom == 1) {
		const DWORD_PTR item = m_availableList.GetItemData(row);
		if(item == kSeparatorItem) m_toolTipText = FbeLoadRuntimeStringByKey(L"fbe.scripts_toolbar_customize.separator", L"--- Separator ---");
		else if(item < m_available.size()) { m_toolTipText = m_available[item].name; if(!m_available[item].relativePath.IsEmpty()) m_toolTipText += L"\n" + m_available[item].relativePath; }
	} else {
		const int buttonIndex = static_cast<int>(m_currentList.GetItemData(row)); TBBUTTON button = {};
		if(CToolBarCtrl(m_toolbar).GetButton(buttonIndex, &button)) {
			if(button.fsStyle & TBSTYLE_SEP) m_toolTipText = FbeLoadRuntimeStringByKey(L"fbe.scripts_toolbar_customize.separator", L"--- Separator ---");
			else for(size_t i = 0; i < m_available.size(); ++i)
				if(m_available[i].command == button.idCommand) { m_toolTipText = m_available[i].name; if(!m_available[i].relativePath.IsEmpty()) m_toolTipText += L"\n" + m_available[i].relativePath; break; }
		}
	}
	if(m_toolTipText.IsEmpty()) { bHandled = FALSE; return 0; }
	info->lpszText = const_cast<LPWSTR>(static_cast<LPCWSTR>(m_toolTipText));
	return 0;
}
LRESULT CScriptsToolbarCustomizeDlg::OnAdd(WORD, WORD, HWND, BOOL&)
{
	const int command = SelectedAvailableCommand(); if(command == 0 || (command > 0 && ToolbarContainsCommand(command))) return 0;
	if(command < 0) {
		TBBUTTON separator = {}; separator.fsStyle = TBSTYLE_SEP; separator.iBitmap = Scale(8);
		CToolBarCtrl(m_toolbar).AddButtons(1, &separator);
		CToolBarCtrl(m_toolbar).AutoSize(); PopulateAvailable(); PopulateCurrent(m_currentList.GetCount()); UpdateButtonState(); return 0;
	}
	for(size_t i = 0; i < m_available.size(); ++i) if(m_available[i].command == command) {
		CToolBarCtrl(m_toolbar).AddButton(command, m_available[i].button.fsStyle,
			m_available[i].button.fsState, m_available[i].button.iBitmap, m_available[i].name, 0);
		break;
	}
	CToolBarCtrl(m_toolbar).AutoSize(); PopulateAvailable(); PopulateCurrent(m_currentList.GetCount()); UpdateButtonState(); return 0;
}
LRESULT CScriptsToolbarCustomizeDlg::OnRemove(WORD, WORD, HWND, BOOL&)
{
	const int row = m_currentList.GetCurSel(); if(row < 0) return 0;
	const int index = static_cast<int>(m_currentList.GetItemData(row)); CToolBarCtrl(m_toolbar).DeleteButton(index);
	CToolBarCtrl(m_toolbar).AutoSize(); PopulateAvailable(); PopulateCurrent(row); UpdateButtonState(); return 0;
}
LRESULT CScriptsToolbarCustomizeDlg::OnUp(WORD, WORD, HWND, BOOL&)
{
	const int row = m_currentList.GetCurSel(); if(row <= 0) return 0; CToolBarCtrl tb = m_toolbar; const int index = static_cast<int>(m_currentList.GetItemData(row)); TBBUTTON button = {}; if(tb.GetButton(index, &button)) { tb.DeleteButton(index); tb.InsertButton(index - 1, &button); tb.AutoSize(); PopulateCurrent(row - 1); } UpdateButtonState(); return 0;
}
LRESULT CScriptsToolbarCustomizeDlg::OnDown(WORD, WORD, HWND, BOOL&)
{
	const int row = m_currentList.GetCurSel(); if(row < 0 || row + 1 >= m_currentList.GetCount()) return 0; CToolBarCtrl tb = m_toolbar; const int index = static_cast<int>(m_currentList.GetItemData(row)); TBBUTTON button = {}; if(tb.GetButton(index, &button)) { tb.DeleteButton(index); tb.InsertButton(index + 1, &button); tb.AutoSize(); PopulateCurrent(row + 1); } UpdateButtonState(); return 0;
}
LRESULT CScriptsToolbarCustomizeDlg::OnReset(WORD, WORD, HWND, BOOL&)
{
	CToolBarCtrl tb = m_toolbar; while(tb.GetButtonCount() > 0) tb.DeleteButton(0); if(m_defaults.GetSize()) tb.AddButtons(m_defaults.GetSize(), m_defaults.GetData()); tb.AutoSize(); PopulateAvailable(); PopulateCurrent(); UpdateButtonState(); return 0;
}
void CScriptsToolbarCustomizeDlg::LayoutControls(int width, int height)
{
	const int gap = Scale(7), buttonWidth = Scale(86), buttonColumn = buttonWidth;
	const int top = Scale(45), bottom = Scale(42);
	const int listWidth = (width - buttonColumn - gap * 4) / 2;
	const int left = gap, buttonsLeft = left + listWidth + gap, right = buttonsLeft + buttonColumn + gap;
	const int listHeight = height - top - bottom;
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_SEARCH_LABEL).MoveWindow(left, gap, Scale(55), Scale(24));
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_SEARCH).MoveWindow(left + Scale(58), gap, listWidth - Scale(58), Scale(24));
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_AVAILABLE_LABEL).MoveWindow(left, Scale(29), listWidth, Scale(18));
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_CURRENT_LABEL).MoveWindow(right, Scale(29), listWidth, Scale(18));
	m_availableList.MoveWindow(left, top, listWidth, listHeight);
	m_currentList.MoveWindow(right, top, listWidth, listHeight);
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_ADD).MoveWindow(buttonsLeft, top + Scale(25), buttonWidth, Scale(25));
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_REMOVE).MoveWindow(buttonsLeft, top + Scale(55), buttonWidth, Scale(25));
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_UP).MoveWindow(buttonsLeft, top + Scale(105), buttonWidth, Scale(25));
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_DOWN).MoveWindow(buttonsLeft, top + Scale(135), buttonWidth, Scale(25));
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_RESET).MoveWindow(buttonsLeft, top + Scale(205), buttonWidth, Scale(26));
	GetDlgItem(IDCANCEL).MoveWindow(width - gap - buttonWidth, height - bottom + gap, buttonWidth, Scale(26));
}
LRESULT CScriptsToolbarCustomizeDlg::OnSize(UINT, WPARAM, LPARAM lParam, BOOL&) { LayoutControls(LOWORD(lParam), HIWORD(lParam)); return 0; }
LRESULT CScriptsToolbarCustomizeDlg::OnGetMinMaxInfo(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
	info->ptMinTrackSize.x = m_minimumSize.cx;
	info->ptMinTrackSize.y = m_minimumSize.cy;
	return 0;
}
void CScriptsToolbarCustomizeDlg::RestorePlacement()
{
	WINDOWPLACEMENT placement = {}; placement.length = sizeof(placement);
	if(m_settings.GetScriptsToolbarCustomizePlacement(placement))
	{
		placement.showCmd = SW_SHOWNORMAL;
		HMONITOR monitor = ::MonitorFromRect(&placement.rcNormalPosition, MONITOR_DEFAULTTONULL);
		MONITORINFO info = {}; info.cbSize = sizeof(info);
		CRect rect(placement.rcNormalPosition);
		if(monitor != NULL && ::GetMonitorInfo(monitor, &info) && rect.Width() > 0 && rect.Height() > 0)
		{
			const CRect work(info.rcWork);
			const int width = min(rect.Width(), work.Width()), height = min(rect.Height(), work.Height());
			rect.left = max(work.left, min(rect.left, work.right - width));
			rect.top = max(work.top, min(rect.top, work.bottom - height));
			rect.right = rect.left + width; rect.bottom = rect.top + height;
			placement.rcNormalPosition = rect;
			SetWindowPlacement(&placement);
			return;
		}
	}
	const CSize logical = m_settings.GetScriptsToolbarCustomizeSize();
	SetWindowPos(NULL, 0, 0, Scale(logical.cx), Scale(logical.cy), SWP_NOMOVE | SWP_NOZORDER);
	CenterWindow(GetParent());
}
void CScriptsToolbarCustomizeDlg::SavePlacement()
{
	WINDOWPLACEMENT placement = {}; placement.length = sizeof(placement); GetWindowPlacement(&placement);
	placement.showCmd = SW_SHOWNORMAL; placement.flags = 0;
	const CRect rect(placement.rcNormalPosition); const int scale96 = max(1, Scale(96));
	m_settings.SetScriptsToolbarCustomizeSize(CSize(MulDiv(rect.Width(), 96, scale96), MulDiv(rect.Height(), 96, scale96)));
	m_settings.SetScriptsToolbarCustomizePlacement(placement, true);
}
void CScriptsToolbarCustomizeDlg::UpdateMetrics()
{
	m_dpi = UiMetrics::DpiForWindow(m_hWnd);
	if(m_dialogFont != NULL) ::DeleteObject(m_dialogFont);
	m_dialogFont = UiMetrics::CreateDialogFontForDpi(m_dpi);
	const WPARAM font = reinterpret_cast<WPARAM>(m_dialogFont != NULL ? m_dialogFont : ::GetStockObject(DEFAULT_GUI_FONT));
	::SendMessage(m_hWnd, WM_SETFONT, font, TRUE);
	const UINT controls[] = { IDC_SCRIPTS_TOOLBAR_SEARCH_LABEL, IDC_SCRIPTS_TOOLBAR_SEARCH, IDC_SCRIPTS_TOOLBAR_AVAILABLE_LABEL,
		IDC_SCRIPTS_TOOLBAR_CURRENT_LABEL, IDC_SCRIPTS_TOOLBAR_AVAILABLE, IDC_SCRIPTS_TOOLBAR_CURRENT, IDC_SCRIPTS_TOOLBAR_ADD,
		IDC_SCRIPTS_TOOLBAR_REMOVE, IDC_SCRIPTS_TOOLBAR_UP, IDC_SCRIPTS_TOOLBAR_DOWN, IDC_SCRIPTS_TOOLBAR_RESET, IDCANCEL };
	for(int i = 0; i < _countof(controls); ++i) ::SendMessage(GetDlgItem(controls[i]), WM_SETFONT, font, TRUE);
	m_minimumSize = CSize(Scale(560), Scale(330));
}
int CScriptsToolbarCustomizeDlg::Scale(int px) const { return UiMetrics::ScaleForDpi(px, m_dpi); }
void CScriptsToolbarCustomizeDlg::UpdateButtonState()
{
	const int available = SelectedAvailableCommand();
	const int current = m_currentList.GetCurSel(), count = m_currentList.GetCount();
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_ADD).EnableWindow(available != 0);
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_REMOVE).EnableWindow(current >= 0);
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_UP).EnableWindow(current > 0);
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_DOWN).EnableWindow(current >= 0 && current + 1 < count);
}
void CScriptsToolbarCustomizeDlg::DrawListItem(const DRAWITEMSTRUCT& item)
{
	if(item.itemID == static_cast<UINT>(-1)) return;
	const bool available = item.CtlID == IDC_SCRIPTS_TOOLBAR_AVAILABLE;
	CDCHandle dc(item.hDC); CRect rect(item.rcItem);
	const bool selected = (item.itemState & ODS_SELECTED) != 0;
	dc.FillSolidRect(rect, ::GetSysColor(selected ? COLOR_HIGHLIGHT : COLOR_WINDOW));
	dc.SetTextColor(::GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
	dc.SetBkMode(TRANSPARENT);
	const int textLength = static_cast<int>(::SendMessage(item.hwndItem, LB_GETTEXTLEN, item.itemID, 0));
	CString text; LPWSTR textBuffer = text.GetBuffer(textLength); ::SendMessage(item.hwndItem, LB_GETTEXT, item.itemID, reinterpret_cast<LPARAM>(textBuffer)); text.ReleaseBuffer();
	int left = rect.left + Scale(7);
	const DWORD_PTR data = ::SendMessage(item.hwndItem, LB_GETITEMDATA, item.itemID, 0);
	TBBUTTON button = {}; bool drawIcon = false;
	if(available && data != kSeparatorItem && data < m_available.size()) { button = m_available[data].button; drawIcon = button.iBitmap >= 0; }
	if(!available && CToolBarCtrl(m_toolbar).GetButton(static_cast<int>(data), &button)) drawIcon = !(button.fsStyle & TBSTYLE_SEP) && button.iBitmap >= 0;
	HIMAGELIST images = reinterpret_cast<HIMAGELIST>(::SendMessage(m_toolbar, TB_GETIMAGELIST, 0, 0));
	if(drawIcon && images) { ImageList_Draw(images, button.iBitmap, item.hDC, left, rect.top + (rect.Height() - Scale(16)) / 2, ILD_TRANSPARENT); left += Scale(20); }
	rect.left = left; dc.DrawText(text, -1, rect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
	DrawDragIndicator(item);
}
void CScriptsToolbarCustomizeDlg::DrawDragIndicator(const DRAWITEMSTRUCT& item)
{
	if(!m_dragging || item.CtlID != IDC_SCRIPTS_TOOLBAR_CURRENT) return;
	const int count = m_currentList.GetCount();
	if(m_dragInsert != static_cast<int>(item.itemID) && !(m_dragInsert == count && item.itemID + 1 == static_cast<UINT>(count))) return;
	const int y = m_dragInsert == count ? item.rcItem.bottom - Scale(2) : item.rcItem.top;
	CDCHandle(item.hDC).FillSolidRect(item.rcItem.left, y, item.rcItem.right - item.rcItem.left, Scale(2), ::GetSysColor(COLOR_HIGHLIGHT));
}
void CScriptsToolbarCustomizeDlg::UpdateDragInsert(POINT point)
{
	CRect client; m_currentList.GetClientRect(client); const int count = m_currentList.GetCount();
	int insert = 0;
	if(point.y >= client.bottom) insert = count;
	else if(point.y > 0) {
		BOOL outside = FALSE; const int item = m_currentList.ItemFromPoint(point, outside);
		if(!outside && item >= 0) { CRect rect; m_currentList.GetItemRect(item, &rect); insert = point.y < (rect.top + rect.bottom) / 2 ? item : item + 1; }
	}
	if(insert != m_dragInsert) { m_dragInsert = insert; m_currentList.Invalidate(); }
	UpdateDragScroll(point);
}
void CScriptsToolbarCustomizeDlg::UpdateDragScroll(POINT point)
{
	CRect client; m_currentList.GetClientRect(client); const int edge = Scale(18);
	const int direction = point.y < edge ? -1 : (point.y >= client.bottom - edge ? 1 : 0);
	if(direction == m_dragScrollDirection) return;
	m_dragScrollDirection = direction;
	if(direction == 0) m_currentList.KillTimer(1); else m_currentList.SetTimer(1, 80);
}
void CScriptsToolbarCustomizeDlg::FinishDrag(bool commit, POINT point)
{
	if(!m_dragging) return;
	m_currentList.KillTimer(1); m_dragScrollDirection = 0;
	CRect client; m_currentList.GetClientRect(client);
	if(commit && client.PtInRect(point) && m_dragSource >= 0 && m_dragInsert >= 0 && m_dragInsert != m_dragSource && m_dragInsert != m_dragSource + 1)
	{
		const int destination = m_dragSource < m_dragInsert ? m_dragInsert - 1 : m_dragInsert;
		TBBUTTON button = {}; CToolBarCtrl toolbar = m_toolbar;
		if(toolbar.GetButton(m_dragSource, &button)) { toolbar.DeleteButton(m_dragSource); toolbar.InsertButton(destination, &button); toolbar.AutoSize(); PopulateCurrent(destination); UpdateButtonState(); }
	}
	m_dragging = false; m_dragSource = m_dragInsert = -1; m_currentList.Invalidate();
	if(::GetCapture() == m_currentList) ::ReleaseCapture();
}
LRESULT CALLBACK CScriptsToolbarCustomizeDlg::CurrentListSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR reference)
{
	CScriptsToolbarCustomizeDlg* dialog = reinterpret_cast<CScriptsToolbarCustomizeDlg*>(reference);
	if(dialog == NULL) return ::DefSubclassProc(window, message, wParam, lParam);
	CListBox list(window);
	switch(message)
	{
	case WM_LBUTTONDOWN:
		{
			POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }; BOOL outside = FALSE; const int row = list.ItemFromPoint(point, outside);
			dialog->m_dragSource = outside ? -1 : row; dialog->m_dragStartPoint = point; dialog->m_dragInsert = -1;
		}
		break;
	case WM_MOUSEMOVE:
		if(dialog->m_dragSource >= 0 && !dialog->m_dragging)
		{
			const int threshold = max(2, ::GetSystemMetrics(SM_CXDRAG));
			if(abs(GET_X_LPARAM(lParam) - dialog->m_dragStartPoint.x) >= threshold || abs(GET_Y_LPARAM(lParam) - dialog->m_dragStartPoint.y) >= threshold)
			{
				dialog->m_dragging = true; ::SetCapture(window); dialog->UpdateDragInsert(POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
				return 0;
			}
		}
		if(dialog->m_dragging) { dialog->UpdateDragInsert(POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }); return 0; }
		break;
	case WM_LBUTTONUP:
		if(dialog->m_dragging) { dialog->FinishDrag(true, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }); return 0; }
		dialog->m_dragSource = -1;
		break;
	case WM_KEYDOWN:
		if(wParam == VK_ESCAPE && dialog->m_dragging) { POINT point = {}; dialog->FinishDrag(false, point); return 0; }
		break;
	case WM_TIMER:
		if(wParam == 1 && dialog->m_dragging && dialog->m_dragScrollDirection != 0)
		{
			::SendMessage(window, WM_VSCROLL, dialog->m_dragScrollDirection < 0 ? SB_LINEUP : SB_LINEDOWN, 0);
			POINT point = {}; ::GetCursorPos(&point); ::ScreenToClient(window, &point); dialog->UpdateDragInsert(point); return 0;
		}
		break;
	}
	return ::DefSubclassProc(window, message, wParam, lParam);
}
LRESULT CScriptsToolbarCustomizeDlg::OnDrawItem(UINT, WPARAM, LPARAM lParam, BOOL&) { DrawListItem(*reinterpret_cast<DRAWITEMSTRUCT*>(lParam)); return TRUE; }
LRESULT CScriptsToolbarCustomizeDlg::OnMeasureItem(UINT, WPARAM, LPARAM lParam, BOOL&) { reinterpret_cast<MEASUREITEMSTRUCT*>(lParam)->itemHeight = Scale(22); return TRUE; }
LRESULT CScriptsToolbarCustomizeDlg::OnWindowClose(UINT, WPARAM, LPARAM, BOOL&) { SavePlacement(); EndDialog(IDCANCEL); return 0; }
LRESULT CScriptsToolbarCustomizeDlg::OnDpiChanged(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
	if(suggested) SetWindowPos(NULL, suggested->left, suggested->top, suggested->right - suggested->left, suggested->bottom - suggested->top, SWP_NOACTIVATE | SWP_NOZORDER);
	UpdateMetrics(); CRect client; GetClientRect(client); LayoutControls(client.Width(), client.Height());
	return 0;
}
LRESULT CScriptsToolbarCustomizeDlg::OnClose(WORD, WORD, HWND, BOOL&) { SavePlacement(); EndDialog(IDCANCEL); return 0; }
