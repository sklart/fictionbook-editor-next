#include "stdafx.h"
#include "ScriptsToolbarCustomizeDlg.h"
#include "Settings.h"
#include "RuntimeLocalization.h"
#include "UiMetrics.h"

CScriptsToolbarCustomizeDlg::CScriptsToolbarCustomizeDlg(HWND toolbar,
	const std::vector<ScriptsToolbarCommand>& available, const CSimpleArray<TBBUTTON>& defaults,
	CSettings& settings) : m_toolbar(toolbar), m_available(available), m_defaults(defaults), m_settings(settings)
{
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
	m_availableList = GetDlgItem(IDC_SCRIPTS_TOOLBAR_AVAILABLE);
	m_currentList = GetDlgItem(IDC_SCRIPTS_TOOLBAR_CURRENT);
	m_toolTip.Create(m_hWnd); m_toolTip.Activate(TRUE);
	m_toolTip.AddTool(m_availableList, LPSTR_TEXTCALLBACKW, NULL, 1);
	m_toolTip.AddTool(m_currentList, LPSTR_TEXTCALLBACKW, NULL, 2);
	CRect client;
	const CSize logical = m_settings.GetScriptsToolbarCustomizeSize();
	SetWindowPos(NULL, 0, 0, UiMetrics::Scale(logical.cx), UiMetrics::Scale(logical.cy), SWP_NOMOVE | SWP_NOZORDER);
	CenterWindow(GetParent());
	GetClientRect(client); LayoutControls(client.Width(), client.Height());
	PopulateAvailable(); PopulateCurrent();
	return TRUE;
}

void CScriptsToolbarCustomizeDlg::PopulateAvailable()
{
	CString search; GetDlgItemText(IDC_SCRIPTS_TOOLBAR_SEARCH, search); search.MakeLower();
	m_availableList.ResetContent();
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
			const int row = m_currentList.AddString(L"--- Separator ---");
			m_currentList.SetItemData(row, static_cast<DWORD_PTR>(i));
			continue;
		}
		CString name;
		for(size_t j = 0; j < m_available.size(); ++j) if(m_available[j].command == button.idCommand) { name = m_available[j].name; break; }
		if(name.IsEmpty()) name.Format(L"Command %d", button.idCommand);
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
	const DWORD_PTR item = m_availableList.GetItemData(row); return item < m_available.size() ? m_available[item].command : 0;
}

LRESULT CScriptsToolbarCustomizeDlg::OnSearchChanged(WORD, WORD, HWND, BOOL&) { PopulateAvailable(); return 0; }
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
		if(item < m_available.size()) { m_toolTipText = m_available[item].name; if(!m_available[item].relativePath.IsEmpty()) m_toolTipText += L"\n" + m_available[item].relativePath; }
	} else {
		const int buttonIndex = static_cast<int>(m_currentList.GetItemData(row)); TBBUTTON button = {};
		if(CToolBarCtrl(m_toolbar).GetButton(buttonIndex, &button)) for(size_t i = 0; i < m_available.size(); ++i)
			if(m_available[i].command == button.idCommand) { m_toolTipText = m_available[i].name; if(!m_available[i].relativePath.IsEmpty()) m_toolTipText += L"\n" + m_available[i].relativePath; break; }
	}
	if(m_toolTipText.IsEmpty()) { bHandled = FALSE; return 0; }
	info->lpszText = const_cast<LPWSTR>(static_cast<LPCWSTR>(m_toolTipText));
	return 0;
}
LRESULT CScriptsToolbarCustomizeDlg::OnAdd(WORD, WORD, HWND, BOOL&)
{
	const int command = SelectedAvailableCommand(); if(command == 0 || ToolbarContainsCommand(command)) return 0;
	for(size_t i = 0; i < m_available.size(); ++i) if(m_available[i].command == command) {
		CToolBarCtrl(m_toolbar).AddButton(command, m_available[i].button.fsStyle,
			m_available[i].button.fsState, m_available[i].button.iBitmap, m_available[i].name, 0);
		break;
	}
	CToolBarCtrl(m_toolbar).AutoSize(); PopulateAvailable(); PopulateCurrent(m_currentList.GetCount()); return 0;
}
LRESULT CScriptsToolbarCustomizeDlg::OnRemove(WORD, WORD, HWND, BOOL&)
{
	const int row = m_currentList.GetCurSel(); if(row < 0) return 0;
	const int index = static_cast<int>(m_currentList.GetItemData(row)); CToolBarCtrl(m_toolbar).DeleteButton(index);
	CToolBarCtrl(m_toolbar).AutoSize(); PopulateAvailable(); PopulateCurrent(row); return 0;
}
LRESULT CScriptsToolbarCustomizeDlg::OnUp(WORD, WORD, HWND, BOOL&)
{
	const int row = m_currentList.GetCurSel(); if(row <= 0) return 0; CToolBarCtrl tb = m_toolbar; const int index = static_cast<int>(m_currentList.GetItemData(row)); TBBUTTON button = {}; if(tb.GetButton(index, &button)) { tb.DeleteButton(index); tb.InsertButton(index - 1, &button); tb.AutoSize(); PopulateCurrent(row - 1); } return 0;
}
LRESULT CScriptsToolbarCustomizeDlg::OnDown(WORD, WORD, HWND, BOOL&)
{
	const int row = m_currentList.GetCurSel(); if(row < 0 || row + 1 >= m_currentList.GetCount()) return 0; CToolBarCtrl tb = m_toolbar; const int index = static_cast<int>(m_currentList.GetItemData(row)); TBBUTTON button = {}; if(tb.GetButton(index, &button)) { tb.DeleteButton(index); tb.InsertButton(index + 1, &button); tb.AutoSize(); PopulateCurrent(row + 1); } return 0;
}
LRESULT CScriptsToolbarCustomizeDlg::OnReset(WORD, WORD, HWND, BOOL&)
{
	CToolBarCtrl tb = m_toolbar; while(tb.GetButtonCount() > 0) tb.DeleteButton(0); if(m_defaults.GetSize()) tb.AddButtons(m_defaults.GetSize(), m_defaults.GetData()); tb.AutoSize(); PopulateAvailable(); PopulateCurrent(); return 0;
}
void CScriptsToolbarCustomizeDlg::LayoutControls(int width, int height)
{
	const int gap = UiMetrics::NormalGap(), buttonWidth = UiMetrics::Scale(86), buttonColumn = buttonWidth;
	const int top = UiMetrics::Scale(45), bottom = UiMetrics::Scale(42);
	const int listWidth = (width - buttonColumn - gap * 4) / 2;
	const int left = gap, buttonsLeft = left + listWidth + gap, right = buttonsLeft + buttonColumn + gap;
	const int listHeight = height - top - bottom;
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_SEARCH_LABEL).MoveWindow(left, gap, UiMetrics::Scale(55), UiMetrics::Scale(24));
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_SEARCH).MoveWindow(left + UiMetrics::Scale(58), gap, listWidth - UiMetrics::Scale(58), UiMetrics::Scale(24));
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_AVAILABLE_LABEL).MoveWindow(left, UiMetrics::Scale(29), listWidth, UiMetrics::Scale(18));
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_CURRENT_LABEL).MoveWindow(right, UiMetrics::Scale(29), listWidth, UiMetrics::Scale(18));
	m_availableList.MoveWindow(left, top, listWidth, listHeight);
	m_currentList.MoveWindow(right, top, listWidth, listHeight);
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_ADD).MoveWindow(buttonsLeft, top + UiMetrics::Scale(25), buttonWidth, UiMetrics::Scale(25));
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_REMOVE).MoveWindow(buttonsLeft, top + UiMetrics::Scale(55), buttonWidth, UiMetrics::Scale(25));
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_UP).MoveWindow(buttonsLeft, top + UiMetrics::Scale(105), buttonWidth, UiMetrics::Scale(25));
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_DOWN).MoveWindow(buttonsLeft, top + UiMetrics::Scale(135), buttonWidth, UiMetrics::Scale(25));
	GetDlgItem(IDC_SCRIPTS_TOOLBAR_RESET).MoveWindow(gap, height - bottom + gap, buttonWidth, UiMetrics::Scale(26));
	GetDlgItem(IDCANCEL).MoveWindow(width - gap - buttonWidth, height - bottom + gap, buttonWidth, UiMetrics::Scale(26));
}
LRESULT CScriptsToolbarCustomizeDlg::OnSize(UINT, WPARAM, LPARAM lParam, BOOL&) { LayoutControls(LOWORD(lParam), HIWORD(lParam)); return 0; }
LRESULT CScriptsToolbarCustomizeDlg::OnGetMinMaxInfo(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
	info->ptMinTrackSize.x = m_minimumSize.cx;
	info->ptMinTrackSize.y = m_minimumSize.cy;
	return 0;
}
void CScriptsToolbarCustomizeDlg::SaveSize() { CRect rect; GetWindowRect(rect); const int scale96 = max(1, UiMetrics::Scale(96)); m_settings.SetScriptsToolbarCustomizeSize(CSize(MulDiv(rect.Width(), 96, scale96), MulDiv(rect.Height(), 96, scale96)), true); }
void CScriptsToolbarCustomizeDlg::UpdateMetrics()
{
	UiMetrics::UpdateForWindow(m_hWnd);
	const WPARAM font = reinterpret_cast<WPARAM>(UiMetrics::DialogFont());
	::SendMessage(m_hWnd, WM_SETFONT, font, TRUE);
	const UINT controls[] = { IDC_SCRIPTS_TOOLBAR_SEARCH_LABEL, IDC_SCRIPTS_TOOLBAR_SEARCH, IDC_SCRIPTS_TOOLBAR_AVAILABLE_LABEL,
		IDC_SCRIPTS_TOOLBAR_CURRENT_LABEL, IDC_SCRIPTS_TOOLBAR_AVAILABLE, IDC_SCRIPTS_TOOLBAR_CURRENT, IDC_SCRIPTS_TOOLBAR_ADD,
		IDC_SCRIPTS_TOOLBAR_REMOVE, IDC_SCRIPTS_TOOLBAR_UP, IDC_SCRIPTS_TOOLBAR_DOWN, IDC_SCRIPTS_TOOLBAR_RESET, IDCANCEL };
	for(int i = 0; i < _countof(controls); ++i) ::SendMessage(GetDlgItem(controls[i]), WM_SETFONT, font, TRUE);
	m_minimumSize = CSize(UiMetrics::Scale(560), UiMetrics::Scale(330));
}
LRESULT CScriptsToolbarCustomizeDlg::OnWindowClose(UINT, WPARAM, LPARAM, BOOL&) { SaveSize(); EndDialog(IDCANCEL); return 0; }
LRESULT CScriptsToolbarCustomizeDlg::OnDpiChanged(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
	if(suggested) SetWindowPos(NULL, suggested->left, suggested->top, suggested->right - suggested->left, suggested->bottom - suggested->top, SWP_NOACTIVATE | SWP_NOZORDER);
	UpdateMetrics(); CRect client; GetClientRect(client); LayoutControls(client.Width(), client.Height());
	return 0;
}
LRESULT CScriptsToolbarCustomizeDlg::OnClose(WORD, WORD, HWND, BOOL&) { SaveSize(); EndDialog(IDCANCEL); return 0; }
