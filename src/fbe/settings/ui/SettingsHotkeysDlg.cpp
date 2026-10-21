// SettingsHotkeysDlg.cpp : Implementation of CSettingsHotkeysDlg

#include "stdafx.h"
#include "../hotkeys/HotkeyStore.h"
#include "../SettingsPaths.h"
#include "../../../common/ModernFileDialog.h"
#include "SettingsHotkeysDlg.h"
#include "..\\..\\KeyboardLayoutSelection.h"
#include "..\\..\\utils\\utils.h"
#include "..\\..\\Settings.h"
#include "..\\..\\res1.h"
#include "..\\..\\RuntimeLocalization.h"
#include "..\\hotkeys\\HotkeyExport.h"

extern CSettings _Settings;

class KeyboardLayoutRestore
{
public:
	explicit KeyboardLayoutRestore(HKL originalLayout) : m_originalLayout(originalLayout) { }
	~KeyboardLayoutRestore() { if(m_originalLayout != NULL) ::ActivateKeyboardLayout(m_originalLayout, 0); }
private:
	HKL m_originalLayout;
};

static CString GetKeyboardLayoutId(HKL layout, HKL originalLayout)
{
	KeyboardLayoutRestore restore(originalLayout);
	wchar_t name[KL_NAMELENGTH] = {};
	if(::ActivateKeyboardLayout(layout, 0) && ::GetKeyboardLayoutName(name))
		return CString(name);
	return CString();
}

static void SetRuntimeHotkeysText(HWND dialog, int controlId, LPCWSTR key, LPCWSTR fallback)
{
	const CString text = FbeLoadRuntimeStringByKey(key, fallback);
	if (!text.IsEmpty())
		::SetDlgItemText(dialog, controlId, text);
}

static CString GetHotkeyDisplayName(const CHotkey& hotkey)
{
	return hotkey.m_name_resource_id ? FbeLoadRuntimeString(hotkey.m_name_resource_id, hotkey.m_name) : hotkey.m_name;
}

static CString GetHotkeyGroupDisplayName(const CHotkeysGroup& group)
{
	return group.m_name_resource_id ? FbeLoadRuntimeString(group.m_name_resource_id, group.m_name) : group.m_name;
}

static void UpdateInfoScrollbar(HWND edit)
{
	if(!::IsWindow(edit)) return;
	::ShowScrollBar(edit, SB_VERT, FALSE);
	RECT client = {};
	::GetClientRect(edit, &client);
	HDC dc = ::GetDC(edit);
	HFONT font = reinterpret_cast<HFONT>(::SendMessageW(edit, WM_GETFONT, 0, 0));
	HFONT previous = dc && font ? reinterpret_cast<HFONT>(::SelectObject(dc, font)) : NULL;
	TEXTMETRICW metrics = {};
	if(dc) ::GetTextMetricsW(dc, &metrics);
	if(dc) { if(previous) ::SelectObject(dc, previous); ::ReleaseDC(edit, dc); }
	const int lines = static_cast<int>(::SendMessageW(edit, EM_GETLINECOUNT, 0, 0));
	::ShowScrollBar(edit, SB_VERT,
		lines * max(1, static_cast<int>(metrics.tmHeight + metrics.tmExternalLeading)) > client.bottom - client.top - 4);
}

static LRESULT CALLBACK ReadOnlyInfoSubclass(HWND edit, UINT message, WPARAM wParam, LPARAM lParam,
	UINT_PTR, DWORD_PTR focusTarget)
{
	if(message == WM_LBUTTONDOWN) return 0;
	if(message == WM_SETFOCUS)
	{
		if(::IsWindow(reinterpret_cast<HWND>(focusTarget))) ::SetFocus(reinterpret_cast<HWND>(focusTarget));
		return 0;
	}
	if(message == WM_NCDESTROY) ::RemoveWindowSubclass(edit, ReadOnlyInfoSubclass, 1);
	const LRESULT result = ::DefSubclassProc(edit, message, wParam, lParam);
	if(message == WM_SETTEXT) UpdateInfoScrollbar(edit);
	return result;
}

// CSettingsHotkeysDlg
CSettingsHotkeysDlg::CSettingsHotkeysDlg(): m_count(0), m_accel(),
											m_initHkGroups(_Settings.m_hotkey_groups),
											m_selGr(0),
											m_selHk(0)
{
	for(unsigned int i = 0; i < _Settings.m_hotkey_groups.size(); ++i)
	{
		CHotkeysGroup& group = _Settings.m_hotkey_groups[i];
		CString groupname = group.m_reg_name;
		for(unsigned int j = 0; j < group.m_hotkeys.size(); ++j)
		{
			CHotkey& hotkey = group.m_hotkeys[j];
			CString fullname = groupname + L"\\" + hotkey.m_reg_name;
			// added by SeNS: do not show empty hotkeys
//			if (!hotkey.m_name.IsEmpty())
				m_mapHotkeys.Add(fullname, &hotkey);
		}
	}

	m_wrongHkMsg = FbeLoadCString(IDS_HOTKEY_WRONG);
}

CSettingsHotkeysDlg::~CSettingsHotkeysDlg()
{
	if(m_filterRegistered) if(CMessageLoop* loop = _Module.GetMessageLoop()) loop->RemoveMessageFilter(this);
}

static LRESULT CALLBACK HotkeySearchKeys(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
	UINT_PTR id, DWORD_PTR reference)
{
	auto* dialog = reinterpret_cast<CSettingsHotkeysDlg*>(reference);
	if(message == WM_NCDESTROY) ::RemoveWindowSubclass(window, HotkeySearchKeys, id);
	const bool recordingShortcut = ::GetFocus() == dialog->m_editHotkey.m_hWnd;
	const bool searchKey = !recordingShortcut && ((wParam == VK_ESCAPE && dialog->m_search.GetWindowTextLength() > 0) ||
		(wParam == 'F' && (::GetKeyState(VK_CONTROL) & 0x8000)));
	if(message == WM_GETDLGCODE && searchKey)
		return ::DefSubclassProc(window, message, wParam, lParam) | DLGC_WANTMESSAGE;
	if(message == WM_KEYDOWN && searchKey) {
		MSG key = {}; key.hwnd = window; key.message = message; key.wParam = wParam; key.lParam = lParam;
		if(dialog->PreTranslateMessage(&key)) return 0;
	}
	return ::DefSubclassProc(window, message, wParam, lParam);
}

BOOL CSettingsHotkeysDlg::PreTranslateMessage(MSG* message)
{
	if(!message || !::IsWindow(m_hWnd) || !::IsWindowVisible(m_hWnd) ||
		(message->hwnd != GetParent() && !::IsChild(GetParent(), message->hwnd))) return FALSE;
	// The recording edit owns every key chord, including Ctrl+F and Escape.
	if(::GetFocus() == m_editHotkey.m_hWnd) return FALSE;
	if(message->message == WM_KEYDOWN) {
		if(message->wParam == 'F' && (::GetKeyState(VK_CONTROL) & 0x8000)) {
			m_search.SetFocus(); m_search.SetSel(0, -1); return TRUE;
		}
		if(message->wParam == VK_ESCAPE && m_search.GetWindowTextLength() > 0) {
			m_search.SetWindowText(L""); return TRUE;
		}
	}
	return FALSE;
}

LRESULT CSettingsHotkeysDlg::OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled)
{
	if(m_filterRegistered) if(CMessageLoop* loop = _Module.GetMessageLoop()) loop->RemoveMessageFilter(this);
	m_filterRegistered = false;
	handled = FALSE;
	return 0;
}

bool CSettingsHotkeysDlg::HasSelection() const
{
	return m_selGr >= 0 && static_cast<size_t>(m_selGr) < _Settings.m_hotkey_groups.size() &&
		m_selHk >= 0 && static_cast<size_t>(m_selHk) < _Settings.m_hotkey_groups[m_selGr].m_hotkeys.size();
}

static CString CommandListName(const CHotkeysGroup& group, const CHotkey& hotkey)
{
	if(group.m_reg_name == L"Scripts" && !hotkey.m_displayPath.IsEmpty()) return hotkey.m_displayPath;
	CString name = GetHotkeyDisplayName(hotkey);
	if(group.m_reg_name == L"Scripts" && hotkey.m_reg_name.Left(7) == L"script:") {
		// Display folder names from the same catalog names used by menus/tree.
		const int separator = name.ReverseFind(L'/');
		if(separator < 0 && !hotkey.m_desc.IsEmpty()) {
			CString folder = hotkey.m_desc.Left(hotkey.m_desc.ReverseFind(L'/') + 1);
			if(!folder.IsEmpty()) name = folder + name;
		}
	}
	return name;
}

void CSettingsHotkeysDlg::RebuildCommandList(bool preserveSelection)
{
	if(!::IsWindow(m_hotkeys)) return;
	CHotkey* selected = HasSelection() ? &_Settings.m_hotkey_groups[m_selGr].m_hotkeys[m_selHk] : NULL;
	CString query; m_search.GetWindowText(query); query.Trim(); query.MakeLower();
	CString shortcutQuery(query); shortcutQuery.Remove(L' '); shortcutQuery.Remove(L'\t');
	const int selectedGroup = m_hkGroups.GetCurSel();
	m_rebuilding = true;
	m_hotkeys.SetRedraw(FALSE);
	m_hotkeys.DeleteAllItems();
	int selectedRow = -1;
	for(size_t groupIndex = 0; groupIndex < _Settings.m_hotkey_groups.size(); ++groupIndex) {
		auto& group = _Settings.m_hotkey_groups[groupIndex];
		if(query.IsEmpty() && static_cast<int>(groupIndex) != selectedGroup) continue;
		for(auto& hotkey : group.m_hotkeys) {
			const CString name = CommandListName(group, hotkey);
			const CString shortcut = U::AccelToString(hotkey.m_accel);
			CString compactShortcut(shortcut); compactShortcut.Remove(L' '); compactShortcut.Remove(L'\t'); compactShortcut.MakeLower();
			CString searchable = GetHotkeyGroupDisplayName(group) + L" " + name + L" " + hotkey.m_desc + L" " + shortcut;
			searchable.MakeLower();
			if(!query.IsEmpty() && searchable.Find(query) < 0 && compactShortcut.Find(shortcutQuery) < 0 &&
				!(preserveSelection && selected == &hotkey)) continue;
			const CString label = query.IsEmpty() ? name : GetHotkeyGroupDisplayName(group) + L" / " + name;
			const int row = m_hotkeys.InsertItem(m_hotkeys.GetItemCount(), label);
			m_hotkeys.SetItemText(row, 1, shortcut);
			m_hotkeys.SetItemData(row, hotkey.m_accel.cmd);
			if(selected == &hotkey) selectedRow = row;
		}
	}
	m_rebuilding = false;
	m_hotkeys.SetRedraw(TRUE);
	m_hotkeys.Invalidate();
	if(selectedRow < 0 && m_hotkeys.GetItemCount() > 0) selectedRow = 0;
	if(selectedRow >= 0) {
		m_hotkeys.SetItemState(selectedRow, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		m_hotkeys.EnsureVisible(selectedRow, FALSE);
	} else { m_selGr = m_selHk = -1; ClearAndSet(); }
}

LRESULT CSettingsHotkeysDlg::OnSearchChanged(WORD, WORD, HWND, BOOL&)
{
	RebuildCommandList();
	return 0;
}

LRESULT CSettingsHotkeysDlg::OnCommandSelection(int, LPNMHDR, BOOL&)
{
	if(m_rebuilding) return 0;
	const int row = m_hotkeys.GetNextItem(-1, LVNI_SELECTED);
	m_selGr = m_selHk = -1;
	if(row >= 0) {
		const WORD selectedCommand = static_cast<WORD>(m_hotkeys.GetItemData(row));
		for(size_t group = 0; group < _Settings.m_hotkey_groups.size(); ++group)
			for(size_t key = 0; key < _Settings.m_hotkey_groups[group].m_hotkeys.size(); ++key)
				if(_Settings.m_hotkey_groups[group].m_hotkeys[key].m_accel.cmd == selectedCommand) {
					m_selGr = static_cast<int>(group); m_selHk = static_cast<int>(key);
				}
		if(m_selGr >= 0) m_hkGroups.SetCurSel(m_selGr);
	}
	ClearAndSet();
	return 0;
}

LRESULT CSettingsHotkeysDlg::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CAxDialogImpl<CSettingsHotkeysDlg>::OnInitDialog(uMsg, wParam, lParam, bHandled);
	if(CMessageLoop* loop = _Module.GetMessageLoop()) { loop->AddMessageFilter(this); m_filterRegistered = true; }
	m_search = GetDlgItem(IDC_HOTKEY_SEARCH);
	SetRuntimeHotkeysText(m_hWnd, IDC_HOTKEY_SEARCH_LABEL, L"fbe.settings.hotkeys.search", L"Search:");

	SetRuntimeHotkeysText(m_hWnd, IDC_STATIC_HOTKEY_DESCRIPTION, L"fbe.dialog.idd_hotkeys.description", L"Description");
	SetRuntimeHotkeysText(m_hWnd, IDC_STATIC_HOTKEYS_GROUPS, L"fbe.dialog.idd_hotkeys.groups", L"Groups:");
	SetRuntimeHotkeysText(m_hWnd, IDC_STATIC_HOTKEYS, L"fbe.dialog.idd_hotkeys.commands", L"Commands:");
	SetRuntimeHotkeysText(m_hWnd, IDC_HOTKEYS_SHORTCUT_LABEL, L"fbe.dialog.idd_hotkeys.shortcut", L"Shortcut:");
	SetRuntimeHotkeysText(m_hWnd, IDC_BUTTON_DEFAULT, L"fbe.dialog.idd_hotkeys.default", L"Default");
	SetRuntimeHotkeysText(m_hWnd, IDC_BUTTON_HOTKEY_DELETE, L"fbe.dialog.idd_hotkeys.delete", L"Delete");
	SetRuntimeHotkeysText(m_hWnd, IDC_STATIC_HOTKEY_ACTIONS, L"fbe.dialog.idd_hotkeys.actions", L"Actions");
	SetRuntimeHotkeysText(m_hWnd, IDC_STATIC_HOTKEY_COLLISION, L"fbe.dialog.idd_hotkeys.collision", L"Collision");
	SetRuntimeHotkeysText(m_hWnd, IDC_BUTTON_HOTKEY_ASSIGN, L"fbe.dialog.idd_hotkeys.assign", L"Assign");
	SetRuntimeHotkeysText(m_hWnd, IDC_BUTTON_HOTKEY_EXPORT, L"fbe.dialog.idd_hotkeys.export", L"Export...");
	SetRuntimeHotkeysText(m_hWnd, IDC_SETTINGS_OTHER_KEYBOARD, L"fbe.dialog.idd_setting_other.keyboard", L"Keyboard layout");
	SetRuntimeHotkeysText(m_hWnd, IDC_CHANGE_KEYB, L"fbe.dialog.idd_setting_other.change_keyboard", L"Change layout on startup");
	SetRuntimeHotkeysText(m_hWnd, IDC_SETTINGS_OTHER_CHANGE_TO, L"fbe.dialog.idd_setting_other.layout", L"Change to:");

	m_hkGroups = GetDlgItem(IDC_LIST_HOTKEYS_GROUPS);
	for(unsigned int i = 0; i < _Settings.m_hotkey_groups.size(); ++i)
		m_hkGroups.AddString(GetHotkeyGroupDisplayName(_Settings.m_hotkey_groups[i]));

	m_hkGroups.SetCurSel(m_selGr);

	m_hotkeys = GetDlgItem(IDC_LIST_HOTKEYS);
	m_hotkeys.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
	RECT listRect = {}; m_hotkeys.GetClientRect(&listRect);
	const int width = max(120, listRect.right);
	m_hotkeys.InsertColumn(0, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_hotkeys.commands", L"Commands"), LVCFMT_LEFT, width * 2 / 3);
	m_hotkeys.InsertColumn(1, FbeLoadRuntimeStringByKey(L"fbe.dialog.idd_hotkeys.shortcut", L"Shortcut"), LVCFMT_LEFT, width / 3);
	for(const int id : { IDC_EDIT_HOTKEY_DESCRIPTION, IDC_EDIT_HOTKEY_COLLISION })
		::SetWindowSubclass(GetDlgItem(id), ReadOnlyInfoSubclass, 1,
			reinterpret_cast<DWORD_PTR>(m_hotkeys.m_hWnd));

	m_editHotkey.SubclassWindow(GetDlgItem(IDC_EDIT_HOTKEY));
	RebuildCommandList();
	m_changeKeyb = GetDlgItem(IDC_CHANGE_KEYB);
	m_keybLayout = GetDlgItem(IDC_KEYB_LAYOUT);
	m_tooltips.Initialize(m_hWnd);
	m_tooltips.Add(m_hkGroups, L"fbe.settings.tooltip.hotkeys.groups", L"Select a group of commands.");
	m_tooltips.Add(m_hotkeys, L"fbe.settings.tooltip.hotkeys.commands", L"Select the command whose shortcut you want to change.");
	m_tooltips.Add(m_editHotkey, L"fbe.settings.tooltip.hotkeys.shortcut", L"Press the shortcut to assign to the selected command.");
	m_tooltips.Add(GetDlgItem(IDC_BUTTON_DEFAULT), L"fbe.settings.tooltip.hotkeys.default", L"Restore the selected command's default shortcut.");
	m_tooltips.Add(GetDlgItem(IDC_BUTTON_HOTKEY_DELETE), L"fbe.settings.tooltip.hotkeys.delete", L"Remove the shortcut from the selected command.");
	m_tooltips.Add(GetDlgItem(IDC_BUTTON_HOTKEY_ASSIGN), L"fbe.settings.tooltip.hotkeys.assign", L"Assign the entered shortcut to the selected command.");
	m_tooltips.Add(GetDlgItem(IDC_BUTTON_HOTKEY_EXPORT), L"fbe.settings.tooltip.hotkeys.export", L"Export assigned shortcuts to HTML or UTF-8 text.");
	m_tooltips.Add(m_changeKeyb, L"fbe.settings.tooltip.hotkeys.change_layout", L"Changes the keyboard layout when FictionBook Editor Next starts.");
	m_tooltips.Add(m_keybLayout, L"fbe.settings.tooltip.hotkeys.layout", L"Keyboard layout used at startup when layout switching is enabled.");
	m_tooltips.Add(GetDlgItem(IDC_SETTINGS_OTHER_CHANGE_TO), L"fbe.settings.tooltip.hotkeys.layout_disabled", L"Available after enabling Change layout on startup.");
	m_tooltips.AddDisabledControlArea(m_keybLayout, L"fbe.settings.tooltip.hotkeys.layout_disabled", L"Available after enabling Change layout on startup.");
	m_changeKeyb.SetCheck(_Settings.GetChangeKeybLayout());
	TCHAR name[255] = {}; const HKL originalLayout = GetKeyboardLayout(GetCurrentThreadId()); const DWORD activeLayout = static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(originalLayout)); const int layoutCount = GetKeyboardLayoutList(0, NULL);
	std::vector<HKL> layouts(layoutCount > 0 ? layoutCount : 0);
	const int count = layouts.empty() ? 0 : GetKeyboardLayoutList(static_cast<int>(layouts.size()), &layouts[0]);
	std::vector<KeyboardLayoutEntry> keyboardLayouts;
	for(int i = 0; i < count; ++i)
	{
		name[0] = L'\0';
		const DWORD legacyHkl = static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(layouts[i]));
		const CString klid = GetKeyboardLayoutId(layouts[i], originalLayout);
		LCID locale = MAKELCID((LANGID)(legacyHkl & 0xffff), SORT_DEFAULT);
		if(!GetLocaleInfo(locale, LOCALE_SLANGUAGE, name, _countof(name))) name[0] = L'\0';
		CString label; label.Format(L"%s \x2014 %s", name[0] ? name : FbeLoadRuntimeStringByKey(L"fbe.settings.hotkeys.keyboard_layout", L"Keyboard layout").GetString(), klid.IsEmpty() ? FbeLoadRuntimeStringByKey(L"fbe.settings.hotkeys.unavailable", L"Unavailable").GetString() : klid.GetString());
		const int item = m_keybLayout.AddString(label);
		m_keybLayout.SetItemData(item, legacyHkl);
		m_keyboardLayoutIds.push_back(klid);
		keyboardLayouts.push_back({ legacyHkl, std::wstring(klid.GetString()) });
	}
	const CString storedId = _Settings.GetKeyboardLayoutId();
	const KeyboardLayoutSelection layoutSelection = ResolveKeyboardLayoutSelection(std::wstring(storedId.GetString()), _Settings.GetKeybLayout(), activeLayout, keyboardLayouts);
	int selection = layoutSelection.index;
	if(layoutSelection.kind == KeyboardLayoutSelectionKind::UnavailableKlid) { selection = m_keybLayout.AddString(storedId + L" \x2014 " + FbeLoadRuntimeStringByKey(L"fbe.settings.hotkeys.unavailable", L"Unavailable")); m_keybLayout.SetItemData(selection, 0); m_keyboardLayoutIds.push_back(storedId); }
	if(layoutSelection.kind == KeyboardLayoutSelectionKind::UnresolvedLegacy) { CString label; label.Format(FbeLoadRuntimeStringByKey(L"fbe.settings.hotkeys.legacy_unresolved", L"Legacy layout %04x — unresolved"), _Settings.GetKeybLayout() & 0xffff); selection = m_keybLayout.AddString(label); m_keybLayout.SetItemData(selection, 0); m_keyboardLayoutIds.push_back(CString()); }
	if(selection >= 0) m_keybLayout.SetCurSel(selection);
	UpdateKeyboardLayoutDependencies();
	ClearAndSet();
	::EnumChildWindows(m_hWnd, [](HWND child, LPARAM reference) -> BOOL {
		::SetWindowSubclass(child, HotkeySearchKeys, 1, static_cast<DWORD_PTR>(reference)); return TRUE;
	}, reinterpret_cast<LPARAM>(this));

	return 0;
}

void CSettingsHotkeysDlg::RefreshInfoScrollbars()
{
	UpdateInfoScrollbar(GetDlgItem(IDC_EDIT_HOTKEY_DESCRIPTION));
	UpdateInfoScrollbar(GetDlgItem(IDC_EDIT_HOTKEY_COLLISION));
}

void CSettingsHotkeysDlg::UpdateKeyboardLayoutDependencies()
{
	const BOOL enabled = m_changeKeyb.GetCheck() == BST_CHECKED;
	m_keybLayout.EnableWindow(enabled);
}

LRESULT CSettingsHotkeysDlg::OnChangeKeyboardLayout(WORD, WORD, HWND, BOOL&)
{
	UpdateKeyboardLayoutDependencies();
	return 0;
}

int CSettingsHotkeysDlg::GetTextLen(CString text)
{
	CDC DC = m_hotkeys.GetDC();

	CSize size;
	DC.GetTextExtent(text, text.GetLength(), &size);
	size.cx += 3;

	ReleaseDC(DC);

	return size.cx;
}

LRESULT CSettingsHotkeysDlg::OnGroupsSelChange(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	if(m_rebuilding) return 0;
	m_selGr = m_hkGroups.GetCurSel(); m_selHk = -1;
	if(m_search.GetWindowTextLength() > 0) m_search.SetWindowText(L"");
	else RebuildCommandList();

	return 0;
}

LRESULT CSettingsHotkeysDlg::OnHotkeysSelChange(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	LPNMHDR header = NULL; BOOL handled = FALSE;
	OnCommandSelection(0, header, handled);

	return 0;
}

hkIndex CSettingsHotkeysDlg::GetCollIndex(ACCEL newAccel)
{
	hkIndex index = {-1, -1};

	if(newAccel.key == NULL)
		goto ret;

	for(unsigned int i = 0; i < _Settings.m_hotkey_groups.size(); ++i)
	{
		for(unsigned int j = 0; j < _Settings.m_hotkey_groups[i].m_hotkeys.size(); ++j)
		{
			if(_Settings.m_hotkey_groups[i].m_hotkeys[j].m_accel.fVirt == newAccel.fVirt
				&& _Settings.m_hotkey_groups[i].m_hotkeys[j].m_accel.key == newAccel.key
				&& ((m_selGr < 0 || i != static_cast<unsigned int>(m_selGr)) || (m_selHk < 0 || j != static_cast<unsigned int>(m_selHk))))
			{
				index.group = i;
				index.hotkey = j;
				goto ret;
			}
		}
	}

ret:
	return index;
}

bool CSettingsHotkeysDlg::TestAndSet()
{
	if(!HasSelection()) return false;
	bool collisions = false;
	for(int i = 0; i< m_mapHotkeys.GetSize(); ++i)
	{
		CHotkey* hotkey = m_mapHotkeys.GetValueAt(i);
		if(hotkey->m_accel.fVirt == m_accel.fVirt && hotkey->m_accel.key == m_accel.key)
		{
			hotkey->m_accel.fVirt = NULL;
			hotkey->m_accel.key = NULL;

			collisions = true;
		}
	}

	_Settings.m_hotkey_groups[m_selGr].m_hotkeys[m_selHk].m_accel = m_accel;
	RebuildCommandList(true);

	return collisions;
}

LRESULT CSettingsHotkeysDlg::OnBnClickedButtonDefault(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	if(!HasSelection()) return 0;
	hkIndex index = GetCollIndex(_Settings.m_hotkey_groups[m_selGr].m_hotkeys[m_selHk].m_def_accel);
	if(index.group != -1 && index.hotkey != -1)
	{
		wchar_t collDefMsg[MAX_LOAD_STRING + 1];
		FbeLoadString(_Module.GetResourceInstance(),
					IDS_HOTKEY_DEFAULT_COLLISION,
					collDefMsg,
					MAX_LOAD_STRING + 1);

		CString collCmdName;
		collCmdName += GetHotkeyGroupDisplayName(_Settings.m_hotkey_groups[index.group]);
		collCmdName += L'\\';
		collCmdName += GetHotkeyDisplayName(_Settings.m_hotkey_groups[index.group].m_hotkeys[index.hotkey]);

		CString collDefCmdMsg;
		collDefCmdMsg.Format(collDefMsg, static_cast<LPCWSTR>(collCmdName));

		wchar_t errCaption[MAX_LOAD_STRING + 1];
		FbeLoadString(_Module.GetResourceInstance(),
			IDS_ERRMSGBOX_CAPTION,
			errCaption,
			MAX_LOAD_STRING + 1);

		if(MessageBox(collDefCmdMsg, errCaption, MB_YESNO | MB_ICONEXCLAMATION) == IDYES)
		{
			m_accel = _Settings.m_hotkey_groups[m_selGr].m_hotkeys[m_selHk].m_def_accel;
			TestAndSet();
			ClearAndSet();
		}
	}
	else
	{
		m_accel = _Settings.m_hotkey_groups[m_selGr].m_hotkeys[m_selHk].m_def_accel;
		TestAndSet();
		ClearAndSet();
	}

	return 0;
}

LRESULT CSettingsHotkeysDlg::OnBnClickedButtonHotkeyDelete(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	if(!HasSelection()) return 0;
	auto& accel = _Settings.m_hotkey_groups[m_selGr].m_hotkeys[m_selHk].m_accel;
	accel.fVirt = 0; accel.key = 0;
	RebuildCommandList(true);
	ClearAndSet();

	return 0;
}

LRESULT CSettingsHotkeysDlg::OnClickedOK(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	return ForwardSettingsDialogCommand(m_hWnd, IDOK);
}

LRESULT CSettingsHotkeysDlg::OnClickedCancel(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	return ForwardSettingsDialogCommand(m_hWnd, IDCANCEL);
}

bool CSettingsHotkeysDlg::Validate()
{
	if(m_changeKeyb.GetCheck() == BST_CHECKED && (m_keybLayout.GetCurSel() < 0 || m_keyboardLayoutIds[m_keybLayout.GetCurSel()].IsEmpty() || m_keybLayout.GetItemData(m_keybLayout.GetCurSel()) == 0))
	{
		MessageBeep(MB_ICONERROR); m_keybLayout.SetFocus(); return false;
	}
	return true;
}
bool CSettingsHotkeysDlg::PrepareCommit(SettingsFileTransaction& files, CString& error)
{
	bool changed = false;
	for(size_t i = 0; i < m_initHkGroups.size(); ++i)
		for(size_t j = 0; j < m_initHkGroups[i].m_hotkeys.size(); ++j)
		{
			const ACCEL& old = m_initHkGroups[i].m_hotkeys[j].m_accel;
			const ACCEL& now = _Settings.m_hotkey_groups[i].m_hotkeys[j].m_accel;
			changed = changed || old.fVirt != now.fVirt || old.key != now.key || old.cmd != now.cmd;
		}
	if(!changed) return true;
	const CString path = FbeSettings::HotkeysFilePath();
	const CString temporary = SettingsFileTransaction::Temporary(path);
	if(temporary.IsEmpty()) return SettingsPreparationFailed(error, path);
	files.Add(path, temporary);
	::SetLastError(ERROR_SUCCESS);
	if(FbeSettings::Hotkeys::SaveToFile(_Settings.m_hotkey_groups, temporary)) return true;
	return SettingsPreparationFailed(error, path);
}

void CSettingsHotkeysDlg::Commit()
{
	_Settings.SetChangeKeybLayout(m_changeKeyb.GetCheck() == BST_CHECKED);
	if(m_keybLayout.GetCurSel() >= 0 && m_keybLayout.GetItemData(m_keybLayout.GetCurSel()) != 0 && !m_keyboardLayoutIds[m_keybLayout.GetCurSel()].IsEmpty()) { _Settings.SetKeybLayout(m_keybLayout.GetItemData(m_keybLayout.GetCurSel())); _Settings.SetKeyboardLayoutId(m_keyboardLayoutIds[m_keybLayout.GetCurSel()]); }
	// The frame rebuilds accelerators and dynamic tips on Apply/OK.
}

bool CSettingsHotkeysDlg::CancelChanges() { _Settings.m_hotkey_groups = m_initHkGroups; return true; }

LRESULT CSettingsHotkeysDlg::OnEditSetFocus(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	if(!HasSelection()) return 0;
	m_editHotkey.SetWindowText(NULL);
	::SetWindowText(GetDlgItem(IDC_EDIT_HOTKEY_COLLISION), NULL);

	m_accel.cmd = _Settings.m_hotkey_groups[m_selGr].m_hotkeys[m_selHk].m_accel.cmd;

	return 0;
}

bool CSettingsHotkeysDlg::Test()
{
	hkIndex index = GetCollIndex(m_accel);
	wchar_t collMsg[MAX_LOAD_STRING +1];

	if(index.group != -1 && index.hotkey != -1)
	{
		CString collCmdName;
		collCmdName += GetHotkeyGroupDisplayName(_Settings.m_hotkey_groups[index.group]);
		collCmdName += L'\\';
		collCmdName += GetHotkeyDisplayName(_Settings.m_hotkey_groups[index.group].m_hotkeys[index.hotkey]);

		FbeLoadString(_Module.GetResourceInstance(),
			IDS_HOTKEY_ASSIGN_COLLISION,
			collMsg,
			MAX_LOAD_STRING + 1);

		CString collCmdMsg;
		collCmdMsg.Format(collMsg, static_cast<LPCWSTR>(collCmdName));

		::SetWindowText(GetDlgItem(IDC_EDIT_HOTKEY_COLLISION), collCmdMsg);

		return true;
	}
	else
	{
		FbeLoadString(_Module.GetResourceInstance(),
			IDS_HOTKEY_ASSIGN_NO_COLLISION,
			collMsg,
			MAX_LOAD_STRING + 1);

		::SetWindowText(GetDlgItem(IDC_EDIT_HOTKEY_COLLISION), collMsg);

		return false;
	}
}

LRESULT CSettingsHotkeysDlg::OnKeyPressed(UINT /* unused: uMsg */, WPARAM wParam, LPARAM /* unused: lParam */, BOOL& /* unused: bHandled */)
{
	if(!HasSelection()) return 0;
	if (wParam > UCHAR_MAX)
		return 0;
	const WORD virtualKey = static_cast<WORD>(wParam);
	CString wndText;
	m_editHotkey.GetWindowText(wndText);
	const ACCEL keyOnly = { FVIRTKEY, virtualKey, 0 };
	CString keyText = U::AccelToString(keyOnly);

	switch(m_count)
	{
		case 0:
			{
				m_editHotkey.SetWindowText(NULL);
				ZeroMemory(&m_accel, sizeof(ACCEL));
				m_accel.fVirt = FVIRTKEY;
				m_accel.cmd = _Settings.m_hotkey_groups[m_selGr].m_hotkeys[m_selHk].m_accel.cmd;

				if(virtualKey == VK_CONTROL || virtualKey == U::StringToKeycode(L"Alt") || virtualKey == VK_SHIFT)
				{
					wndText = keyText;
					m_editHotkey.SetWindowText(wndText);

					m_accel.fVirt |= static_cast<BYTE>(U::VKToFVirt(virtualKey));
					m_count++;
				}
				else if (virtualKey >= VK_F1 && virtualKey <= VK_F12)
				{
					wndText += keyText;
					m_editHotkey.SetWindowText(wndText);

					m_accel.key = virtualKey;
					m_count = 0;
					Test();
					::EnableWindow(GetDlgItem(IDC_BUTTON_HOTKEY_ASSIGN), TRUE);
				}
				else
				{
					m_editHotkey.SetWindowText(m_wrongHkMsg);
					m_count = 0;
					::EnableWindow(GetDlgItem(IDC_BUTTON_HOTKEY_ASSIGN), FALSE);
					::SetWindowText(GetDlgItem(IDC_EDIT_HOTKEY_COLLISION), NULL);
				}

				break;
			}
		case 1:
		{
			if(virtualKey == VK_CONTROL || virtualKey == U::StringToKeycode(L"Alt") || virtualKey == VK_SHIFT)
			{
				wndText += L" + ";
				wndText += keyText;
				m_editHotkey.SetWindowText(wndText);

				m_accel.fVirt |= static_cast<BYTE>(U::VKToFVirt(virtualKey));
				m_count++;
			}
			else if(!keyText.IsEmpty())
			{
/*				if(m_accel.fVirt & FSHIFT)
				{
					m_editHotkey.SetWindowText(m_wrongHkMsg);
					m_count = 0;
					::EnableWindow(GetDlgItem(IDC_BUTTON_HOTKEY_ASSIGN), FALSE);
					::SetWindowText(GetDlgItem(IDC_EDIT_HOTKEY_COLLISION), NULL);
				}
				else */
				{ 
					wndText += L" + ";
					wndText += keyText;
					m_editHotkey.SetWindowText(wndText);

					m_accel.key = virtualKey;
					m_count = 0;
					Test();
					::EnableWindow(GetDlgItem(IDC_BUTTON_HOTKEY_ASSIGN), TRUE);
				}
			}

			break;
		}
		case 2:
		{
			if(virtualKey == VK_CONTROL || virtualKey == U::StringToKeycode(L"Alt") || virtualKey == VK_SHIFT)
			{
				wndText += L" + ";
				wndText += keyText;
				m_editHotkey.SetWindowText(wndText);

				m_accel.fVirt |= static_cast<BYTE>(U::VKToFVirt(virtualKey));
				m_count++;
			}
			else if(!keyText.IsEmpty())
			{
				wndText += L" + ";
				wndText += keyText;
				m_editHotkey.SetWindowText(wndText);

				m_accel.key = virtualKey;
				m_count = 0;
				Test();
				::EnableWindow(GetDlgItem(IDC_BUTTON_HOTKEY_ASSIGN), TRUE);
			}

			break;
		}
		case 3:
		{
			if(!keyText.IsEmpty()
				&& virtualKey != VK_CONTROL
				&& virtualKey != U::StringToKeycode(L"Alt")
				&& virtualKey != VK_SHIFT)
			{
				wndText += L" + ";
				wndText += keyText;
				m_editHotkey.SetWindowText(wndText);

				m_accel.key = virtualKey;
				m_count = 0;
				Test();
				::EnableWindow(GetDlgItem(IDC_BUTTON_HOTKEY_ASSIGN), TRUE);
			}
			else
			{
				m_editHotkey.SetWindowText(m_wrongHkMsg);
				m_count = 0;
				::EnableWindow(GetDlgItem(IDC_BUTTON_HOTKEY_ASSIGN), FALSE);
				::SetWindowText(GetDlgItem(IDC_EDIT_HOTKEY_COLLISION), NULL);
			}

			break;
		}
	}

	return 0;
}

LRESULT CSettingsHotkeysDlg::OnKeyReleased(UINT /* unused: uMsg */, WPARAM /* unused: wParam */, LPARAM /* unused: lParam */, BOOL& /* unused: bHandled */)
{
	if(m_accel.key == NULL)
	{
		ClearAndSet();
	}

	return 0;
}

LRESULT CSettingsHotkeysDlg::OnBnClickedButtonHotkeyAssign(WORD /* unused: wNotifyCode */, WORD /* unused: wID */, HWND /* unused: hWndCtl */, BOOL& /* unused: bHandled */)
{
	if(!HasSelection()) return 0;
	const hkIndex conflict = GetCollIndex(m_accel);
	if(conflict.group >= 0 && conflict.hotkey >= 0)
	{
		CString prompt;
		prompt.Format(FbeLoadRuntimeStringByKey(L"fbe.settings.hotkeys.reassign", L"This shortcut is already assigned to \"%s\". Reassign it?"),
			static_cast<LPCWSTR>(GetHotkeyGroupDisplayName(_Settings.m_hotkey_groups[conflict.group]) + L"\\" +
			GetHotkeyDisplayName(_Settings.m_hotkey_groups[conflict.group].m_hotkeys[conflict.hotkey])));
		if(MessageBox(prompt, FbeLoadCString(IDS_ERRMSGBOX_CAPTION), MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES) return 0;
	}
	TestAndSet();
	ClearAndSet();
	return 0;
}

LRESULT CSettingsHotkeysDlg::OnBnClickedButtonHotkeyExport(WORD, WORD, HWND, BOOL&)
{
	const CString htmlFilterName(FbeLoadRuntimeStringByKey(L"fbe.hotkey.export.filter.html", L"HTML files (*.html)"));
	const CString textFilterName(FbeLoadRuntimeStringByKey(L"fbe.hotkey.export.filter.text", L"Text files (*.txt)"));
	const COMDLG_FILTERSPEC filters[] = { {htmlFilterName, L"*.html"}, {textFilterName, L"*.txt"} };
	ModernFileDialog::Request request;
	request.save = true; request.pathMustExist = true;
	request.customize = [](IFileDialogCustomize* custom) -> HRESULT {
		CComQIPtr<IFileDialog> dialog(custom);
		if(!dialog) return E_NOINTERFACE;
		DWORD options = 0; const HRESULT hr = dialog->GetOptions(&options);
		return FAILED(hr) ? hr : dialog->SetOptions(options & ~FOS_OVERWRITEPROMPT);
	};
	request.defaultExtension = L"html"; request.initialFileName = L"FBE-Next-Hotkeys.html";
	request.filters = filters; request.filterCount = _countof(filters); request.filterIndex = 1;
	const auto result = ModernFileDialog::Show(m_hWnd, request);
	if(result.outcome != ModernFileDialog::Outcome::Accepted || result.paths.empty()) return 0;
	const bool html = result.filterIndex != 2;
	CString path(result.paths.front().c_str());
	const int directorySeparator = max(path.ReverseFind(L'\\'), path.ReverseFind(L'/'));
	const int extension = path.ReverseFind(L'.');
	if(extension > directorySeparator) path = path.Left(extension);
	path += html ? L".html" : L".txt";
	const bool replaceExisting = ::GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
	if(replaceExisting)
	{
		CString prompt;
		prompt.Format(FbeLoadRuntimeStringByKey(L"fbe.settings.file.replace", L"Replace the existing file?\n%s"), path.GetString());
		if(U::MessageBox(m_hWnd, prompt, L"FictionBook Editor", MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES) return 0;
	}
	const FbeSettings::Hotkeys::ExportData data(FbeSettings::Hotkeys::BuildExportData(_Settings.m_hotkey_groups));
	const CString content = html ? FbeSettings::Hotkeys::BuildHtmlExport(data, _Settings.GetInterfaceLocaleName()) : FbeSettings::Hotkeys::BuildTextExport(data);
	if(!FbeSettings::Hotkeys::WriteUtf8ExportFile(path, content, replaceExisting))
		U::MessageBox(m_hWnd, FbeLoadRuntimeStringByKey(L"fbe.hotkey.export.error", L"Could not export hotkeys."), L"FictionBook Editor", MB_OK | MB_ICONERROR);
	return 0;
}

void CSettingsHotkeysDlg::ClearAndSet()
{
	m_count = 0;
	ZeroMemory(&m_accel, sizeof(ACCEL));
	::EnableWindow(GetDlgItem(IDC_BUTTON_HOTKEY_ASSIGN), FALSE);
	const bool selected = HasSelection();
	for(const int id : { IDC_EDIT_HOTKEY, IDC_BUTTON_DEFAULT, IDC_BUTTON_HOTKEY_DELETE })
		::EnableWindow(GetDlgItem(id), selected);
	if(!selected) {
		m_editHotkey.SetWindowText(L"");
		::SetWindowText(GetDlgItem(IDC_EDIT_HOTKEY_DESCRIPTION), L"");
		::SetWindowText(GetDlgItem(IDC_EDIT_HOTKEY_COLLISION), L"");
		return;
	}
	m_editHotkey.SetWindowText(U::AccelToString(_Settings.m_hotkey_groups[m_selGr].m_hotkeys[m_selHk].m_accel));
	::SetWindowText(GetDlgItem(IDC_EDIT_HOTKEY_COLLISION), NULL);
	if(!_Settings.m_hotkey_groups[m_selGr].m_hotkeys[m_selHk].m_desc.IsEmpty())
		::SetWindowText(GetDlgItem(IDC_EDIT_HOTKEY_DESCRIPTION), _Settings.m_hotkey_groups[m_selGr].m_hotkeys[m_selHk].m_desc);
	else
		::SetWindowText(GetDlgItem(IDC_EDIT_HOTKEY_DESCRIPTION), NULL);
}
