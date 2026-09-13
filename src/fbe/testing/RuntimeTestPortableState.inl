
// Test-only OLE parent for the Split undo probe.  It is intentionally kept
// outside production structural code until MSHTML proves this composition.
class CSplitUndoProbeParent : public CComObjectRootEx<CComSingleThreadModel>, public IOleParentUndoUnit {
public:
	BEGIN_COM_MAP(CSplitUndoProbeParent)
		COM_INTERFACE_ENTRY(IOleUndoUnit)
		COM_INTERFACE_ENTRY(IOleParentUndoUnit)
	END_COM_MAP()
	STDMETHOD(Do)(IOleUndoManager* manager) { for (std::vector<CComPtr<IOleUndoUnit>>::reverse_iterator it = m_units.rbegin(); it != m_units.rend(); ++it) { HRESULT hr = (*it)->Do(manager); if (FAILED(hr)) return hr; } return manager ? manager->Add(this) : S_OK; }
	STDMETHOD(GetDescription)(BSTR* description) { if (!description) return E_POINTER; *description = ::SysAllocString(L"split undo probe parent"); return *description ? S_OK : E_OUTOFMEMORY; }
	STDMETHOD(GetUnitType)(CLSID* classId, LONG* id) { if (!classId || !id) return E_POINTER; *classId = CLSID_NULL; *id = 0; return S_OK; }
	STDMETHOD(OnNextAdd)() { return S_OK; }
	STDMETHOD(Open)(IOleParentUndoUnit*) { return S_OK; }
	STDMETHOD(Close)(IOleParentUndoUnit*, BOOL) { return S_OK; }
	STDMETHOD(Add)(IOleUndoUnit* unit) { if (!unit) return E_POINTER; m_units.push_back(unit); return S_OK; }
	STDMETHOD(FindUnit)(IOleUndoUnit* unit) { for (std::vector<CComPtr<IOleUndoUnit>>::const_iterator it = m_units.begin(); it != m_units.end(); ++it) if (it->p == unit) return S_OK; return S_FALSE; }
	STDMETHOD(GetParentState)(DWORD* state) { if (!state) return E_POINTER; *state = 0; return S_OK; }
	long UnitCount() const { return static_cast<long>(m_units.size()); }
private:
	std::vector<CComPtr<IOleUndoUnit>> m_units;
};

void CMainFrame::RunPortableStateTestScenario()
{
	const bool ordinaryWrite = IsFbeTestScenario(L"portable-state-write");
	const bool ordinaryRead = IsFbeTestScenario(L"portable-state-read");
	const bool emptyToolbarWrite = IsFbeTestScenario(L"portable-toolbar-empty-write");
	const bool emptyToolbarRead = IsFbeTestScenario(L"portable-toolbar-empty-read");
	const bool toolbarLayoutWrite = IsFbeTestScenario(L"portable-toolbar-layout-write");
	const bool toolbarLayoutRead = IsFbeTestScenario(L"portable-toolbar-layout-read");
	const bool missingScriptRead = IsFbeTestScenario(L"portable-toolbar-missing-script-read");
	const bool malformedToolbarRead = IsFbeTestScenario(L"portable-toolbar-malformed-read");
	const bool scriptsReload = IsFbeTestScenario(L"portable-scripts-reload");
	const bool legacyHotkeyRead = IsFbeTestScenario(L"portable-legacy-hotkey-read");
	const bool diagnosticCleanup = IsFbeTestScenario(L"portable-diagnostic-cleanup");
	if (!ordinaryWrite && !ordinaryRead && !emptyToolbarWrite && !emptyToolbarRead && !toolbarLayoutWrite && !toolbarLayoutRead && !missingScriptRead && !malformedToolbarRead && !scriptsReload && !legacyHotkeyRead && !diagnosticCleanup)
		return;

	const CString diagnosticsDirectory(DeploymentContext::DiagnosticsDirectory().c_str());
	const CString scriptsDirectory(DeploymentContext::UserScriptsDirectory().c_str());
	const CString diagnosticsMarker(diagnosticsDirectory + L"portable-state-sentinel.txt");
	const CString recoveryMarker(m_recovery.SnapshotPath());
	const CString reportPath(diagnosticsDirectory + L"portable-state-report.txt");
	const WORD portableStateHotkeyFlags = FVIRTKEY | FCONTROL | FSHIFT;
	const WORD portableStateHotkeyKey = VK_F24;
	const int portableStateToolbarWidth = 731;
	const UINT portableStateToolbarBandId = ATL_IDW_BAND_FIRST;
	if (DeploymentContext::CurrentMode() != DeploymentContext::Mode::Portable)
	{
		WritePortableStateTestText(reportPath, "phase=failed\nreason=not-portable\n");
		PostMessage(WM_CLOSE);
		return;
	}
	if (diagnosticCleanup)
	{
		// This narrow test hook exercises the public cleanup operation after the
		// portable startup trace has been opened.  The PowerShell regression seeds
		// more than ten completed sessions and checks a separate user directory.
		const StartupTrace::DiagnosticLogCleanupResult cleanup = StartupTrace::ClearOldLogSessions();
		CStringA report;
		report.Format("phase=diagnostic-cleanup\nportable=1\nsessions-found=%u\nsessions-deleted=%u\nfiles-deleted=%u\nfiles-failed=%u\nresult=%s\n",
			cleanup.sessionsFound, cleanup.sessionsFullyDeleted, cleanup.filesDeleted, cleanup.filesFailed,
			cleanup.filesFailed == 0 && cleanup.sessionsFailed == 0 && cleanup.sessionsPartiallyDeleted == 0 ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report);
		PostMessage(WM_CLOSE);
		return;
	}

	::CreateDirectory(scriptsDirectory, NULL);
	auto catalogButton = [&](HWND toolbar, int ordinal, TBBUTTON& button) -> bool
	{
		const int catalogIndex = m_aButtons.FindKey(toolbar);
		if(catalogIndex < 0) return false;
		TBBUTTONS catalog = m_aButtons.GetValueAt(catalogIndex);
		int found = 0;
		for(int index = 0; index < catalog.GetSize(); ++index)
			if((catalog[index].fsStyle & TBSTYLE_SEP) == 0 && catalog[index].idCommand != 0)
				if(found++ == ordinal) { button = catalog[index]; return true; }
		return false;
	};
	auto catalogButtonByCommand = [&](HWND toolbar, int command, TBBUTTON& button) -> bool
	{
		const int catalogIndex = m_aButtons.FindKey(toolbar);
		if(catalogIndex < 0) return false;
		TBBUTTONS catalog = m_aButtons.GetValueAt(catalogIndex);
		for(int index = 0; index < catalog.GetSize(); ++index)
			if(catalog[index].idCommand == command) { button = catalog[index]; return true; }
		return false;
	};
	auto hasButtons = [](CToolBarCtrl& toolbar, const TBBUTTON& first, const TBBUTTON& second, const TBBUTTON& third) -> bool
	{
		TBBUTTON current = {};
		return toolbar.GetButtonCount() == 3 &&
			toolbar.GetButton(0, &current) && current.idCommand == first.idCommand &&
			toolbar.GetButton(1, &current) && (current.fsStyle & TBSTYLE_SEP) != 0 && current.iBitmap == second.iBitmap &&
			toolbar.GetButton(2, &current) && current.idCommand == third.idCommand;
	};
	if (emptyToolbarWrite)
	{
		while(m_CmdToolbar.GetButtonCount() > 0) m_CmdToolbar.DeleteButton(0);
		while(m_ScriptsToolbar.GetButtonCount() > 0) m_ScriptsToolbar.DeleteButton(0);
		m_CmdToolbar.AutoSize();
		m_ScriptsToolbar.AutoSize();
		WritePortableStateTestText(reportPath, "phase=toolbar-empty-write\nresult=pass\n");
	}
	else if (emptyToolbarRead)
	{
		const bool empty = m_CmdToolbar.GetButtonCount() == 0 && m_ScriptsToolbar.GetButtonCount() == 0;
		CStringA report;
		report.Format("phase=toolbar-empty-read\nempty-toolbar=%d\nresult=%s\n", empty, empty ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report);
	}
	else if (malformedToolbarRead)
	{
		const bool defaultsKept = m_CmdToolbar.GetButtonCount() > 0 && m_ScriptsToolbar.GetButtonCount() > 0;
		CStringA report;
		report.Format("phase=toolbar-malformed-read\ndefaults-kept=%d\nresult=%s\n", defaultsKept, defaultsKept ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report);
	}
	else if (toolbarLayoutWrite || toolbarLayoutRead || missingScriptRead)
	{
		TBBUTTON commandFirst = {}, commandAdded = {}, alphaButton = {}, betaButton = {}, separator = {};
		separator.iBitmap = 13;
		separator.fsStyle = TBSTYLE_SEP;
		const bool commandCatalogReady = catalogButton(m_CmdToolbar, 0, commandFirst) &&
			catalogButton(m_CmdToolbar, 2, commandAdded);
		int alphaCommand = 0, betaCommand = 0;
		CStringA discoveredScripts;
		for(int index = 0; index < m_scripts.Menu().Count(); ++index)
			if(!m_scripts.Menu().Item(index).isFolder)
			{
				const ScriptDescriptor& script = m_scripts.Menu().Item(index);
				CStringA entry;
				entry.Format("%ls:%d;", static_cast<LPCWSTR>(script.relativePath), script.commandId);
				discoveredScripts += entry;
				if(script.relativePath == L"test/alpha.js") alphaCommand = ID_SCRIPT_BASE + script.commandId;
				if(script.relativePath == L"test/beta.js") betaCommand = ID_SCRIPT_BASE + script.commandId;
		}
		const bool alphaInCatalog = alphaCommand != 0 && catalogButtonByCommand(m_ScriptsToolbar, alphaCommand, alphaButton);
		const bool betaInCatalog = betaCommand != 0 && catalogButtonByCommand(m_ScriptsToolbar, betaCommand, betaButton);
		const bool scriptsCatalogReady = alphaInCatalog && betaInCatalog;
		const bool catalogReady = commandCatalogReady && scriptsCatalogReady;
		if(toolbarLayoutWrite && catalogReady)
		{
			// This mirrors a real customization: remove a default command, add a
			// different one, and retain the resulting order on both toolbar rows.
			while(m_CmdToolbar.GetButtonCount() > 0) m_CmdToolbar.DeleteButton(0);
			m_CmdToolbar.AddButton(&commandAdded);
			m_CmdToolbar.AddButton(&separator);
			m_CmdToolbar.AddButton(&commandFirst);
			while(m_ScriptsToolbar.GetButtonCount() > 0) m_ScriptsToolbar.DeleteButton(0);
			m_ScriptsToolbar.AddButton(&alphaButton);
			m_ScriptsToolbar.AddButton(&separator);
			m_ScriptsToolbar.AddButton(&betaButton);
			for(int index = 0; index < m_scripts.Menu().Count(); ++index)
				if(!m_scripts.Menu().Item(index).isFolder && m_scripts.Menu().Item(index).relativePath == L"test/beta.js")
					m_scripts.SetLastScript(m_scripts.Menu().Item(index));
			m_CmdToolbar.AutoSize();
			m_ScriptsToolbar.AutoSize();
		}
		const bool commandLayout = catalogReady && hasButtons(m_CmdToolbar, commandAdded, separator, commandFirst);
		const bool scriptsLayout = catalogReady && hasButtons(m_ScriptsToolbar, alphaButton, separator, betaButton);
		const ScriptDescriptor* lastScript = m_scripts.LastScript();
		const bool lastScriptIsBeta = lastScript != NULL && lastScript->relativePath == L"test/beta.js";
		TBBUTTON missingSeparator = {};
		const bool missingScriptSafe = alphaInCatalog && m_ScriptsToolbar.GetButtonCount() == 2 &&
			m_ScriptsToolbar.GetButton(0, &alphaButton) && alphaButton.idCommand == alphaCommand &&
			m_ScriptsToolbar.GetButton(1, &missingSeparator) && (missingSeparator.fsStyle & TBSTYLE_SEP) != 0 && !m_scripts.HasLastScript();
		CStringA report;
		report.Format("phase=toolbar-layout-%s\nscript-count=%d\ndiscovered-scripts=%s\nalpha-command=%d\nbeta-command=%d\ncommand-catalog=%d\nalpha-catalog=%d\nbeta-catalog=%d\nnonempty-command=%d\nnonempty-scripts=%d\nlast-script-beta=%d\nmissing-script-safe=%d\nresult=%s\n",
			missingScriptRead ? "missing-script-read" : toolbarLayoutWrite ? "write" : "read", m_scripts.Menu().Count(), static_cast<LPCSTR>(discoveredScripts), alphaCommand, betaCommand, commandCatalogReady, alphaInCatalog, betaInCatalog, commandLayout, scriptsLayout, lastScriptIsBeta, missingScriptSafe,
			missingScriptRead ? (missingScriptSafe ? "pass" : "fail") : commandLayout && scriptsLayout && lastScriptIsBeta ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report);
	}
	else if (scriptsReload)
	{
		const DWORD before = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
		InitializeExtensionUi();
		InitializeExtensionUi();
		InitializeExtensionUi();
		const DWORD after = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
		bool validRun = false, invalidRejected = true, noRunRejected = true;
		for (int index = 0; index < m_scripts.Menu().Count(); ++index)
		{
			const ScriptDescriptor& script = m_scripts.Menu().Item(index);
			if (script.isFolder) continue;
			if (script.relativePath == L"foo.js") validRun = true;
			if (script.relativePath == L"invalid.js") invalidRejected = false;
			if (script.relativePath == L"no-run.js") noRunRejected = false;
		}
		CStringA report;
		const bool passed = after <= before && validRun && invalidRejected && noRunRejected;
		report.Format("phase=scripts-reload\ngdi-before=%lu\ngdi-after=%lu\ngdi-stable=%d\nvalid-run=%d\ninvalid-js-rejected=%d\nno-run-rejected=%d\nresult=%s\n",
			before, after, after <= before, validRun, invalidRejected, noRunRejected, passed ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report);
	}
	else if (legacyHotkeyRead)
	{
		CHotkeysGroup* scripts = _Settings.GetGroupByName(L"Scripts");
		CHotkey* foo = scripts ? _Settings.GetHotkeyByName(L"tools/foo.js", *scripts) : NULL;
		const bool migrated = foo != NULL && foo->m_accel.fVirt == (FVIRTKEY | FCONTROL) && foo->m_accel.key == VK_F9;
		CStringA report;
		report.Format("phase=legacy-hotkey-read\nlegacy-hotkey=%d\nresult=%s\n", migrated, migrated ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report);
	}
	else if (ordinaryWrite)
	{
		// These deterministic mutations go through the same settings, MRU,
		// toolbar and recovery code that normal UI actions persist on close.
		_Settings.SetSplitterPos(271);
		_Settings.SetShowFullPathInWindowTitle(true);
		_Settings.SetInterfaceLanguage(FBE_INTERFACE_LANGUAGE_RUSSIAN);
		_Settings.SetScriptsFolder(scriptsDirectory, true);
		_Settings.m_words.push_back(WordsItem(L"portable-state-sentinel", 17));
		m_mru.AddToList(U::GetProgDirFile(L"portable-state-sentinel.fb2"));
		if (CHotkeysGroup* tools = _Settings.GetGroupByName(L"Tools"))
			if (CHotkey* hotkey = _Settings.GetHotkeyByName(L"Words", *tools))
			{
				hotkey->m_accel.fVirt = portableStateHotkeyFlags;
				hotkey->m_accel.key = portableStateHotkeyKey;
				_Settings.SaveHotkeyGroups();
			}
		if (m_rebar.GetBandCount() > 0)
		{
			REBARBANDINFO band = {}; band.cbSize = sizeof(band); band.fMask = RBBIM_ID | RBBIM_SIZE | RBBIM_STYLE;
			if (m_rebar.GetBandInfo(0, &band) && band.wID == portableStateToolbarBandId)
			{
				band.cx = portableStateToolbarWidth;
				m_rebar.SetBandInfo(0, &band);
			}
		}
		WritePortableStateTestText(diagnosticsMarker, "portable-state-diagnostics\n");
		WritePortableStateTestText(recoveryMarker, "portable-state-recovery\n");
		WritePortableStateTestText(reportPath, "phase=write\nresult=pass\n");
	}
	else if (ordinaryRead)
	{
		bool wordFound = false, mruFound = false;
		for (size_t index = 0; index < _Settings.m_words.size(); ++index)
			if (_Settings.m_words[index].m_word == L"portable-state-sentinel" && _Settings.m_words[index].m_count == 17) wordFound = true;
		CHotkeysGroup* tools = _Settings.GetGroupByName(L"Tools");
		CHotkey* hotkey = tools ? _Settings.GetHotkeyByName(L"Words", *tools) : NULL;
		const bool hotkeys = hotkey != NULL && hotkey->m_accel.fVirt == portableStateHotkeyFlags &&
			hotkey->m_accel.key == portableStateHotkeyKey;
		for (int index = 0; index < m_mru.m_arrDocs.GetSize(); ++index)
			if (CString(m_mru.m_arrDocs[index].szDocName).Find(L"portable-state-sentinel.fb2") >= 0) mruFound = true;
		const bool settings = _Settings.GetShowFullPathInWindowTitle();
		const bool locale = _Settings.GetInterfaceLocaleName() == L"ru-RU";
		const bool scripts = _Settings.GetScriptsFolder().CompareNoCase(scriptsDirectory) == 0;
		const bool toolbar = HasPortableStateToolbarWidth(_Settings.GetToolbarsSettings(),
			portableStateToolbarBandId, portableStateToolbarWidth);
		const bool diagnostics = ::GetFileAttributes(diagnosticsMarker) != INVALID_FILE_ATTRIBUTES;
		const bool recovery = ::GetFileAttributes(recoveryMarker) != INVALID_FILE_ATTRIBUTES;
		CStringA report;
		report.Format("phase=read\nsettings=%d\nhotkeys=%d\nwords=%d\nlocale=%d\nmru=%d\ntoolbar=%d\nscripts=%d\ndiagnostics=%d\nrecovery=%d\nresult=%s\n",
			settings, hotkeys, wordFound, locale, mruFound, toolbar, scripts, diagnostics, recovery,
			settings && hotkeys && wordFound && locale && mruFound && toolbar && scripts && diagnostics && recovery ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report);
	}
	PostMessage(WM_CLOSE);
}

