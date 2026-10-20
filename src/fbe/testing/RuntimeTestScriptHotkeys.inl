// Addressed native UI/runtime regression; uses only the isolated test profile.
void CMainFrame::RunScriptHotkeyManagementTest()
{
	bool passed = true;
	CStringA report;
	const auto check = [&](const char* name, bool value) {
		report.AppendFormat("%s=%d\n", name, value ? 1 : 0); passed = passed && value;
	};
	wchar_t root[32768] = {}, phase[32] = {};
	::GetEnvironmentVariableW(L"FBE_NEXT_HOTKEY_TEST_ROOT", root, _countof(root));
	::GetEnvironmentVariableW(L"FBE_NEXT_HOTKEY_TEST_PHASE", phase, _countof(phase));
	_Settings.SetScriptsFolder(root); _Settings.SetCheckScriptsOnStartup(false);
	check("initialize", InitializeScripts());
	auto* group = FbeSettings::Hotkeys::FindGroup(_Settings.m_hotkey_groups, L"Scripts");
	check("last-first", group && !group->m_hotkeys.empty() && group->m_hotkeys.front().m_reg_name == L"LastScript");
	const ScriptDescriptor* first = NULL;
	for(const auto& script : m_scripts.Menu().Items()) if(!script.isFolder) { first = &script; break; }
	check("fixture", first && group && group->m_hotkeys.size() > 3);
	if(!first || !group) { ::PostQuitMessage(1); return; }
	const CString uid = first->uid;
	const CString firstPath = first->path;
	const CString identity = L"script:" + uid;
	const UINT command = ID_SCRIPT_BASE + first->commandId;
	const HKL originalKeyboard = ::GetKeyboardLayout(0);
	const HKL russianKeyboard = ::LoadKeyboardLayoutW(L"00000419", KLF_NOTELLSHELL);
	const HKL englishKeyboard = ::LoadKeyboardLayoutW(L"00000409", KLF_NOTELLSHELL);
	ACCEL lastDefault = { FVIRTKEY | FCONTROL, VK_OEM_3, ID_LAST_SCRIPT };
	if(russianKeyboard) ::ActivateKeyboardLayout(russianKeyboard, 0);
	check("russian-oem-key", russianKeyboard && U::AccelToString(lastDefault) == L"Ctrl+\x0401");
	if(englishKeyboard) ::ActivateKeyboardLayout(englishKeyboard, 0);
	check("english-oem-key", englishKeyboard && U::AccelToString(lastDefault) == L"Ctrl+`");
	::ActivateKeyboardLayout(originalKeyboard, 0);
	auto* last = FbeSettings::Hotkeys::FindHotkey(*group, L"LastScript");
	auto* scriptKey = FbeSettings::Hotkeys::FindHotkey(*group, identity);
	if(!last || !scriptKey) { ::PostQuitMessage(1); return; }
	if(wcscmp(phase, L"read") == 0) {
		_Settings.LoadHotkeyGroups();
		check("restart-assignment", scriptKey->m_accel.key == VK_F6 && last->m_accel.key == VK_OEM_3 &&
			(last->m_accel.fVirt & (FCONTROL | FSHIFT)) == (FCONTROL | FSHIFT));
	} else {
		check("default-last", last->m_def_accel.key == VK_OEM_3 && last->m_def_accel.fVirt == (FVIRTKEY | FCONTROL));
		scriptKey->m_accel.fVirt = FVIRTKEY; scriptKey->m_accel.key = VK_F6;
		last->m_accel.fVirt = FVIRTKEY | FCONTROL | FSHIFT; last->m_accel.key = VK_OEM_3;
	}
	m_scripts.SetLastScript(*first);
	const size_t count = group->m_hotkeys.size();
	check("refresh", InitializeScripts() && InitializeScripts());
	group = FbeSettings::Hotkeys::FindGroup(_Settings.m_hotkey_groups, L"Scripts");
	scriptKey = group ? FbeSettings::Hotkeys::FindHotkey(*group, identity) : NULL;
	last = group ? FbeSettings::Hotkeys::FindHotkey(*group, L"LastScript") : NULL;
	check("refresh-identity", group && group->m_hotkeys.size() == count && scriptKey && scriptKey->m_accel.key == VK_F6 &&
		scriptKey->m_accel.cmd == command && last && last->m_accel.key == VK_OEM_3);
	check("last-history", m_scripts.LastScript() && m_scripts.LastScript()->uid == uid);
	BOOL runHandled = TRUE;
	OnLastScript(0, ID_LAST_SCRIPT, NULL, runHandled);
	check("last-executes", m_status_state.EffectiveMainText(false, false, CString()) == L"hotkey-last-ran");
	const auto menuMatches = [&](const auto& self, HMENU menu, const CString& parent) -> bool {
		int position = 0;
		for(const auto& item : m_scripts.Menu().Items()) if(item.parentId == parent) {
			wchar_t caption[1024] = {};
			::GetMenuStringW(menu, position, caption, _countof(caption), MF_BYPOSITION);
			CString actual(caption); const int tab = actual.Find(L'\t'); if(tab >= 0) actual = actual.Left(tab);
			if(actual != item.name) return false;
			if(item.isFolder && !self(self, ::GetSubMenu(menu, position), item.id)) return false;
			++position;
		}
		return position == ::GetMenuItemCount(menu);
	};
	check("menu-catalog-order", menuMatches(menuMatches, ::GetSubMenu(m_MenuBar.GetMenu(), 6), CString()));
	auto& tree = m_document_tree.m_tree.m_tree;
	tree.SetScriptMode(true);
	bool hierarchy = true;
	for(const auto& item : m_scripts.Menu().Items())
		if(!item.parentId.IsEmpty()) for(const auto& parent : m_scripts.Menu().Items())
			if(parent.id == item.parentId) hierarchy = tree.HasScriptTreeParent(item.relativePath, parent.relativePath) && hierarchy;
	check("tree-hierarchy", hierarchy);
	const HTREEITEM treeItem = m_scripts.LastScript() ? tree.FindScriptTreeItem(m_scripts.LastScript()->relativePath) : NULL;
	wchar_t treeText[1024] = {};
	NMTVGETINFOTIP treeTip = {}; treeTip.hItem = treeItem; treeTip.pszText = treeText; treeTip.cchTextMax = _countof(treeText);
	BOOL treeHandled = TRUE; tree.OnScriptInfoTip(0, &treeTip.hdr, treeHandled);
	check("tree-tooltip", treeHandled && CString(treeText).Find(U::AccelToString(scriptKey->m_accel)) >= 0);
	const CString tip = GetRuntimeToolbarToolTipText(command);
	check("live-tooltip", tip.Find(U::AccelToString(scriptKey->m_accel)) >= 0);
	check("last-tooltip", GetRuntimeToolbarToolTipText(ID_LAST_SCRIPT).Find(U::AccelToString(last->m_accel)) >= 0);
	{
		const bool frameVisible = IsWindowVisible() != FALSE;
		ShowWindow(SW_SHOWNOACTIVATE);
		CSettingsHotkeysDlg dialog;
		dialog.Create(m_hWnd); dialog.ShowWindow(SW_SHOW);
		BOOL handled = FALSE;
		for(int index = 0; index < dialog.m_hkGroups.GetCount(); ++index)
			if(_Settings.m_hotkey_groups[index].m_reg_name == L"Scripts") dialog.m_hkGroups.SetCurSel(index);
		dialog.OnGroupsSelChange(0, 0, NULL, handled);
		check("ui-last-first", dialog.m_hotkeys.GetItemCount() > 0 &&
			dialog.m_hotkeys.GetItemData(0) == ID_LAST_SCRIPT);
		dialog.OnBnClickedButtonDefault(0, 0, NULL, handled);
		check("default-restores-last", last->m_accel.key == VK_OEM_3 && last->m_accel.fVirt == (FVIRTKEY | FCONTROL));
		dialog.OnBnClickedButtonHotkeyDelete(0, 0, NULL, handled);
		check("last-unassigned", last->m_accel.key == 0 && last->m_accel.cmd == ID_LAST_SCRIPT);
		BYTE keyboard[256] = {}; ::GetKeyboardState(keyboard);
		BYTE modified[256] = {}; memcpy(modified, keyboard, sizeof(modified)); modified[VK_CONTROL] = 0x80;
		::SetKeyboardState(modified);
		::SendMessageW(dialog.GetDlgItem(IDC_BUTTON_DEFAULT), WM_KEYDOWN, 'F', 0);
		check("ctrl-f", ::GetFocus() == dialog.m_search);
		dialog.m_editHotkey.SetFocus();
		MSG recording = {}; recording.hwnd = dialog.m_editHotkey; recording.message = WM_KEYDOWN; recording.wParam = 'F';
		check("recording-bypasses-search", !dialog.PreTranslateMessage(&recording) && ::GetFocus() == dialog.m_editHotkey);
		dialog.m_editHotkey.SendMessage(WM_KEYDOWN, VK_CONTROL);
		dialog.m_editHotkey.SendMessage(WM_KEYDOWN, 'F');
		check("record-ctrl-f", dialog.m_accel.key == 'F' && dialog.m_accel.fVirt == (FVIRTKEY | FCONTROL) && ::GetFocus() == dialog.m_editHotkey);
		// Remove the existing Ctrl+F only in this disposable dialog snapshot;
		// the collision confirmation path is covered separately by Settings tests.
		for(auto& entry : _Settings.m_hotkey_groups) for(auto& key : entry.m_hotkeys)
			if(key.m_accel.cmd != ID_LAST_SCRIPT && key.m_accel.key == 'F' && key.m_accel.fVirt == (FVIRTKEY | FCONTROL)) {
				key.m_accel.key = 0; key.m_accel.fVirt = 0;
			}
		dialog.OnBnClickedButtonHotkeyAssign(0, 0, NULL, handled);
		check("assign-ctrl-f", last->m_accel.key == 'F' && last->m_accel.fVirt == (FVIRTKEY | FCONTROL));
		dialog.m_editHotkey.SendMessage(WM_KEYUP, 'F');
		dialog.m_editHotkey.SendMessage(WM_KEYUP, VK_CONTROL);
		::SetKeyboardState(keyboard);
		dialog.m_search.SetFocus();
		dialog.m_search.SetWindowText(L"ПАПКА");
		check("folder-search", dialog.m_hotkeys.GetItemCount() == 2);
		dialog.m_search.SetWindowText(L"ПОВТОР");
		check("unicode-search", dialog.m_hotkeys.GetItemCount() == 2);
		dialog.m_search.SetWindowText(L"F6");
		bool foundShortcut = false;
		for(int row = 0; row < dialog.m_hotkeys.GetItemCount(); ++row)
			if(dialog.m_hotkeys.GetItemData(row) == command) {
				dialog.m_hotkeys.SetItemState(row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED); foundShortcut = true; break;
			}
		check("shortcut-search", foundShortcut);
		dialog.m_accel = scriptKey->m_accel; dialog.m_accel.key = VK_F24; dialog.m_accel.fVirt = FVIRTKEY | FCONTROL | FALT;
		dialog.OnBnClickedButtonHotkeyAssign(0, 0, NULL, handled);
		check("assign-selection", scriptKey->m_accel.key == VK_F24 && dialog.HasSelection() &&
			&_Settings.m_hotkey_groups[dialog.m_selGr].m_hotkeys[dialog.m_selHk] == scriptKey);
		CString displayed;
		const int row = dialog.m_hotkeys.GetNextItem(-1, LVNI_SELECTED);
		if(row >= 0) dialog.m_hotkeys.GetItemText(row, 1, displayed);
		check("column-current", displayed == U::AccelToString(scriptKey->m_accel));
		check("tooltip-updated", GetRuntimeToolbarToolTipText(command).Find(displayed) >= 0);
		CString compact(displayed); compact.Remove(L' ');
		dialog.m_search.SetWindowText(compact);
		check("compact-shortcut-search", dialog.HasSelection() &&
			_Settings.m_hotkey_groups[dialog.m_selGr].m_hotkeys[dialog.m_selHk].m_accel.cmd == command);
		dialog.OnBnClickedButtonHotkeyDelete(0, 0, NULL, handled);
		check("delete-command-id", scriptKey->m_accel.key == 0 && scriptKey->m_accel.cmd == command && dialog.HasSelection());
		const ScriptDescriptor* current = m_scripts.LastScript();
		check("tooltip-no-empty", current && GetRuntimeToolbarToolTipText(command) == current->name);
		dialog.m_search.SetWindowText(L"no-such-command-12345");
		check("empty-search", dialog.m_hotkeys.GetItemCount() == 0 && !dialog.HasSelection());
		dialog.OnBnClickedButtonHotkeyDelete(0, 0, NULL, handled);
		MSG escape = {}; escape.hwnd = dialog.m_search; escape.message = WM_KEYDOWN; escape.wParam = VK_ESCAPE;
		check("escape-clears", dialog.PreTranslateMessage(&escape) && dialog.m_search.GetWindowTextLength() == 0 && dialog.m_hotkeys.GetItemCount() > 0);
		for(size_t index = 0; index < _Settings.m_hotkey_groups.size(); ++index)
			if(_Settings.m_hotkey_groups[index].m_hotkeys.size() > 20) { dialog.m_hkGroups.SetCurSel(static_cast<int>(index)); break; }
		dialog.OnGroupsSelChange(0, 0, NULL, handled);
		dialog.m_hotkeys.SendMessage(WM_VSCROLL, SB_BOTTOM);
		const bool bottom = dialog.m_hotkeys.GetTopIndex() > 0;
		dialog.m_hotkeys.SendMessage(WM_VSCROLL, MAKEWPARAM(SB_THUMBPOSITION, 2));
		const bool thumb = dialog.m_hotkeys.GetTopIndex() > 0;
		dialog.m_hotkeys.SendMessage(WM_VSCROLL, SB_TOP);
		check("native-list-scroll", bottom && thumb && dialog.m_hotkeys.GetTopIndex() == 0);
		dialog.CancelChanges(); dialog.DestroyWindow();
		if(!frameVisible) ShowWindow(SW_HIDE);
	}
	group = FbeSettings::Hotkeys::FindGroup(_Settings.m_hotkey_groups, L"Scripts");
	scriptKey = group ? FbeSettings::Hotkeys::FindHotkey(*group, identity) : NULL;
	check("cancel-restores", scriptKey && scriptKey->m_accel.key == VK_F6);
	_Settings.SaveHotkeyGroups();
	const CString hiddenPath = firstPath + L".test-hidden";
	const bool hidden = ::MoveFileW(firstPath, hiddenPath) != FALSE;
	check("missing-script", hidden && InitializeScripts() && m_scripts.LastScript() == NULL);
	_Settings.SaveHotkeyGroups();
	const bool restored = hidden && ::MoveFileW(hiddenPath, firstPath) != FALSE;
	check("return-script", restored && InitializeScripts());
	group = FbeSettings::Hotkeys::FindGroup(_Settings.m_hotkey_groups, L"Scripts");
	scriptKey = group ? FbeSettings::Hotkeys::FindHotkey(*group, identity) : NULL;
	check("missing-assignment-preserved", scriptKey && scriptKey->m_accel.key == VK_F6 && m_scripts.LastScript() != NULL);
	if(scriptKey) {
		auto legacyGroups = _Settings.m_hotkey_groups;
		auto* legacyScripts = FbeSettings::Hotkeys::FindGroup(legacyGroups, L"Scripts");
		auto* legacyKey = FbeSettings::Hotkeys::FindHotkey(*legacyScripts, identity);
		legacyKey->m_reg_name = L"Z:/old/Scripts/" + scriptKey->m_desc;
		CXMLSerializer legacy(FbeSettings::HotkeysFilePath(), L"FBE", false);
		std::vector<void*> objects; for(auto& entry : legacyGroups) objects.push_back(&entry);
		check("legacy-fixture", legacy.Serialize(objects));
		scriptKey->m_accel.key = 0;
		_Settings.LoadHotkeyGroups();
		check("legacy-migration", scriptKey->m_accel.key == VK_F6 && scriptKey->m_reg_name == identity);
	}
	_Settings.SaveHotkeyGroups(); _Settings.SaveHotkeyGroups();
	MSXML2::IXMLDOMDocument2Ptr saved = U::CreateDocument();
	const bool loaded = saved && saved->load(_variant_t(FbeSettings::HotkeysFilePath())) != VARIANT_FALSE;
	check("no-xml-duplicates", loaded && group &&
		saved->selectNodes(L"/FBE/HkGroups/HkGroup[GroupName='Scripts']/Hotkeys/Hotkey")->length == static_cast<long>(group->m_hotkeys.size()));
	wchar_t refreshText[16] = {};
	::GetEnvironmentVariableW(L"FBE_NEXT_HOTKEY_TEST_REFRESHES", refreshText, _countof(refreshText));
	const int refreshes = _wtoi(refreshText);
	if(refreshes >= 200 && refreshes <= 500 && wcscmp(phase, L"write") == 0) {
		const auto privateBytes = []() -> SIZE_T {
			PROCESS_MEMORY_COUNTERS_EX memory = {}; memory.cb = sizeof(memory);
			return ::GetProcessMemoryInfo(::GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory))
				? memory.PrivateUsage : 0;
		};
		for(int index = 0; index < 30; ++index) { _Settings.keycodes = 0; _Settings.LoadHotkeyGroups(); }
		const SIZE_T loadBefore = privateBytes();
		for(int index = 0; index < refreshes; ++index) { _Settings.keycodes = 0; _Settings.LoadHotkeyGroups(); }
		const SIZE_T loadAfter = privateBytes();
		report.AppendFormat("load-iterations=%d\nload-private-before=%Iu\nload-private-after=%Iu\n", refreshes, loadBefore, loadAfter);
		check("load-memory-bounded", loadBefore != 0 && loadAfter <= loadBefore + 4 * 1024 * 1024);
		for(int index = 0; index < 20; ++index) InitializeScripts();
		const SIZE_T before = privateBytes();
		const DWORD userBefore = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
		const DWORD gdiBefore = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
		bool refreshed = true;
		for(int index = 0; index < refreshes; ++index) {
			refreshed = InitializeScripts() && refreshed;
			if((index + 1) % 100 == 0) report.AppendFormat("refresh-%d-private=%Iu\n", index + 1, privateBytes());
		}
		const SIZE_T after = privateBytes();
		const DWORD userAfter = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
		const DWORD gdiAfter = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
		report.AppendFormat("refresh-gdi-before=%lu\nrefresh-gdi-after=%lu\n", gdiBefore, gdiAfter);
		check("refresh-gdi-handles-bounded", gdiBefore != 0 && gdiAfter <= gdiBefore + 4);
		report.AppendFormat("refresh-user-before=%lu\nrefresh-user-after=%lu\n", userBefore, userAfter);
		check("refresh-user-handles-bounded", userBefore != 0 && userAfter <= userBefore + 2);
		report.AppendFormat("refresh-iterations=%d\nrefresh-private-before=%Iu\nrefresh-private-after=%Iu\n", refreshes, before, after);
		check("refresh-memory-bounded", refreshed && before != 0 && after <= before + 8 * 1024 * 1024);
		group = FbeSettings::Hotkeys::FindGroup(_Settings.m_hotkey_groups, L"Scripts");
		scriptKey = group ? FbeSettings::Hotkeys::FindHotkey(*group, identity) : NULL;
		check("stress-assignments", group && group->m_hotkeys.size() == count && scriptKey && scriptKey->m_accel.key == VK_F6);
	}
	report.AppendFormat("result=%s\n", passed ? "pass" : "fail");
	CAtlFile output; DWORD written = 0;
	if(SUCCEEDED(output.Create(AU::_ARGS.source_memory_benchmark_path, GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS)))
		output.Write(report, static_cast<DWORD>(report.GetLength()), &written);
	output.Close();
	::PostQuitMessage(passed && written == static_cast<DWORD>(report.GetLength()) ? 0 : 1);
}
