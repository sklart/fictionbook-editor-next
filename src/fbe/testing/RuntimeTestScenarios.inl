// Оркестрация runtime-сценариев только для тестов. Файл включается из
// mainfrm.cpp, чтобы узкая граница CMainFrame сохранила private-доступ.

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
		for(int index = 0; index < m_script_menu.Count(); ++index)
			if(!m_script_menu.Item(index).isFolder)
			{
				const ScriptDescriptor& script = m_script_menu.Item(index);
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
			for(int index = 0; index < m_script_menu.Count(); ++index)
				if(!m_script_menu.Item(index).isFolder && m_script_menu.Item(index).relativePath == L"test/beta.js")
					m_last_script = &m_script_menu.Item(index);
			m_CmdToolbar.AutoSize();
			m_ScriptsToolbar.AutoSize();
		}
		const bool commandLayout = catalogReady && hasButtons(m_CmdToolbar, commandAdded, separator, commandFirst);
		const bool scriptsLayout = catalogReady && hasButtons(m_ScriptsToolbar, alphaButton, separator, betaButton);
		const bool lastScriptIsBeta = m_last_script != NULL && m_last_script->relativePath == L"test/beta.js";
		TBBUTTON missingSeparator = {};
		const bool missingScriptSafe = alphaInCatalog && m_ScriptsToolbar.GetButtonCount() == 2 &&
			m_ScriptsToolbar.GetButton(0, &alphaButton) && alphaButton.idCommand == alphaCommand &&
			m_ScriptsToolbar.GetButton(1, &missingSeparator) && (missingSeparator.fsStyle & TBSTYLE_SEP) != 0 && m_last_script == NULL;
		CStringA report;
		report.Format("phase=toolbar-layout-%s\nscript-count=%d\ndiscovered-scripts=%s\nalpha-command=%d\nbeta-command=%d\ncommand-catalog=%d\nalpha-catalog=%d\nbeta-catalog=%d\nnonempty-command=%d\nnonempty-scripts=%d\nlast-script-beta=%d\nmissing-script-safe=%d\nresult=%s\n",
			missingScriptRead ? "missing-script-read" : toolbarLayoutWrite ? "write" : "read", m_script_menu.Count(), static_cast<LPCSTR>(discoveredScripts), alphaCommand, betaCommand, commandCatalogReady, alphaInCatalog, betaInCatalog, commandLayout, scriptsLayout, lastScriptIsBeta, missingScriptSafe,
			missingScriptRead ? (missingScriptSafe ? "pass" : "fail") : commandLayout && scriptsLayout && lastScriptIsBeta ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report);
	}
	else if (scriptsReload)
	{
		const DWORD before = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
		InitPlugins();
		InitPlugins();
		InitPlugins();
		const DWORD after = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
		bool validRun = false, invalidRejected = true, noRunRejected = true;
		for (int index = 0; index < m_script_menu.Count(); ++index)
		{
			const ScriptDescriptor& script = m_script_menu.Item(index);
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

LRESULT CMainFrame::OnSourceMemoryBenchmark(UINT, WPARAM, LPARAM, BOOL&)
{
	CAtlFile output;
	if (FAILED(output.Create(AU::_ARGS.source_memory_benchmark_path, GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS)))
		return 0;
	if (IsFbeTestScenario(L"archive-open-runtime"))
	{
		const bool archiveSource = m_document_session.Location().IsArchive();
		const bool fb2 = m_doc->GetDocumentFileType() == FictionBookFileType::Fb2;
		const bool fbd = m_doc->GetDocumentFileType() == FictionBookFileType::Fbd;
		const bool htmlReady = m_doc->m_body.Document() != NULL;
		const bool rar = m_document_session.Location().containerKind == DocumentContainerKind::Rar;
		// Runtime reports are consumed as UTF-8 by the PowerShell regressions.
		// Do not let the runner's ANSI code page convert a Unicode ZIP entry:
		// on an English Windows image that conversion can throw before the
		// archive-open diagnostic is written.
		const CStringA entryUtf8(CW2A(m_document_session.Location().entryPath, CP_UTF8));
		CStringA report;
		report.Format("archive=%d\nfb2=%d\nfbd=%d\nmshtml=%d\nrar=%d\nentry=%s\n", archiveSource, fb2, fbd, htmlReady, rar,
			static_cast<LPCSTR>(entryUtf8));
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
		::PostQuitMessage(archiveSource && (fb2 || fbd) && htmlReady ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"archive-runtime") || IsFbeTestScenario(L"archive-rar-save-runtime"))
	{
		const bool archiveSource = m_document_session.Location().IsArchive();
		const bool fb2 = m_doc->GetDocumentFileType() == FictionBookFileType::Fb2;
		const bool htmlReady = m_doc->m_body.Document() != NULL;
		ShowView(SOURCE);
		const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
		const char* markerText = "ARCHIVE_RUNTIME_BEFORE";
		char* marker = strstr(source.data(), markerText);
		if (!marker)
		{
			// The checked-in RAR5 fixture is an existing valid FB2 regression
			// document.  Its stable author field is the edit target for Save As.
			markerText = "FBE Test";
			marker = strstr(source.data(), markerText);
		}
		const bool markerFound = marker != NULL;
		if (markerFound) { const size_t offset = static_cast<size_t>(marker - source.data()); m_source.SendMessage(SCI_SETSEL, offset, offset + strlen(markerText)); m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("ARCHIVE_RUNTIME_AFTER")); }
		int sourceLine = 0, sourceColumn = 0;
		const bool sourceCommitted = markerFound && m_doc->SetXMLAndValidate(m_source, false, sourceLine, sourceColumn);
		ShowView(BODY);
		const bool bodyActive = !IsSourceActive();
		const bool readOnlyArchive = m_document_session.Location().containerKind == DocumentContainerKind::Rar;
		const bool saveAs = IsFbeTestScenario(L"archive-rar-save-runtime");
		const bool shouldSave = !readOnlyArchive || saveAs;
		const bool saved = sourceCommitted && bodyActive && markerFound && (!shouldSave || SaveFile(false) == OK);
		wchar_t serializedChanged[4] = {};
		const bool archiveSaveSerializedChanged = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_SERIALIZED_CHANGED", serializedChanged, _countof(serializedChanged)) == 1 && serializedChanged[0] == L'1';
		wchar_t archiveWriteError[64] = {};
		::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_WRITE_ERROR", archiveWriteError, _countof(archiveWriteError));
		CStringA report;
		report.Format("archive=%d\nfb2=%d\nfbd=%d\nmshtml=%d\nrar=%d\nsave_as=%d\nsource_committed=%d\nbody_active=%d\narchive_save_serialized_changed=%d\narchive_write_error=%S\nentry=%S\nsaved=%d\n", archiveSource, fb2,
			m_doc->GetDocumentFileType() == FictionBookFileType::Fbd, htmlReady, readOnlyArchive, saveAs,
			sourceCommitted, bodyActive, archiveSaveSerializedChanged, archiveWriteError, static_cast<LPCWSTR>(m_document_session.Location().entryPath), saved);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(saved ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"body-source-transition-runtime"))
	{
		FB::Doc* const originalDocument = m_doc;
		const wchar_t* const unicodeMarker = L"\x0413\x0420\x0410\x041D\x042C_UNICODE";
		// Exercise the actual MSHTML selection -> Scintilla selection -> MSHTML
		// selection path before the legacy source-edit checks below.  The fixture
		// deliberately includes Unicode, repeated text and inline boundaries: a
		// source position alone cannot prove that these ranges survived a view
		// transition.
		auto selectBodyRange = [&](const wchar_t* elementId, const wchar_t* beginMarker, const wchar_t* endMarker, bool collapsed)
		{
			MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
			MSHTML::IHTMLDocument3Ptr document3(document);
			MSHTML::IHTMLBodyElementPtr body(document ? document->body : NULL);
			MSHTML::IHTMLElementPtr element(document3 ? document3->getElementById(elementId) : NULL);
			if (!body || !element) return false;
			MSHTML::IHTMLTxtRangePtr begin(body->createTextRange());
			MSHTML::IHTMLTxtRangePtr end(body->createTextRange());
			if (!begin || !end) return false;
			begin->moveToElementText(element); end->moveToElementText(element);
			const bool wholeElement = beginMarker == NULL || *beginMarker == L'\0';
			if (!wholeElement && !begin->findText(beginMarker, 0, 0)) return false;
			if (collapsed)
				begin->collapse(VARIANT_TRUE);
			else
			{
				if (!wholeElement && (!end->findText(endMarker, 0, 0) || FAILED(begin->setEndPoint(L"EndToEnd", end)))) return false;
			}
			begin->select();
			return true;
		};
		auto selectedBodyText = [&]()
		{
			MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
			MSHTML::IHTMLSelectionObjectPtr selection(document ? document->selection : NULL);
			MSHTML::IHTMLTxtRangePtr range(selection ? selection->createRange() : NULL);
			return range ? CString(static_cast<LPCWSTR>(range->text)) : CString();
		};
		CStringA selectionRoundTripDiagnostics;
		auto roundTripBodySelection = [&](const wchar_t* elementId, const wchar_t* beginMarker, const wchar_t* endMarker, const wchar_t* expectedText, bool collapsed)
		{
			const bool selected = selectBodyRange(elementId, beginMarker, endMarker, collapsed);
			if (!selected) { selectionRoundTripDiagnostics.AppendFormat("%S=select;", elementId); return false; }
			const CString before(selectedBodyText());
			ShowView(SOURCE);
			const sptr_t sourceStart = m_source.SendMessage(SCI_GETSELECTIONSTART);
			const sptr_t sourceEnd = m_source.SendMessage(SCI_GETSELECTIONEND);
			const bool sourceSelection = IsSourceActive() && m_editor_selection_state.BodySource().bodyToSourceTransferred &&
				sourceStart >= 0 && sourceEnd >= sourceStart && (collapsed ? sourceStart == sourceEnd : sourceEnd > sourceStart);
			const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
			std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
			m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
			const CStringA expectedUtf8(CW2A(expectedText, CP_UTF8));
			const bool sourceContainsExpected = collapsed || (sourceSelection && CStringA(source.data() + sourceStart,
				static_cast<int>(sourceEnd - sourceStart)).Find(expectedUtf8) >= 0);
			ShowView(BODY);
			const CString after(selectedBodyText());
			// IHTMLTxtRange::text omits inline markup content for an outer paragraph
			// range.  Its UTF-8 source range above is the authoritative check for
			// this case; the visual comparison still proves that the same range came
			// back after the round trip.
			const bool outerInlineRange = wcscmp(elementId, L"body-source-inline") == 0;
			const bool beforeMatches = collapsed ? before.IsEmpty() : (outerInlineRange ? !before.IsEmpty() : before.Find(expectedText) >= 0);
			const bool afterMatches = collapsed ? after.IsEmpty() : (outerInlineRange ? !after.IsEmpty() : after.Find(expectedText) >= 0);
			const bool exactRoundTrip = before == after;
			selectionRoundTripDiagnostics.AppendFormat("%S=%d/%d/%d/%d/%d/%d;", elementId, sourceSelection, sourceContainsExpected, beforeMatches, afterMatches, exactRoundTrip, !IsSourceActive());
			return sourceSelection && sourceContainsExpected && !IsSourceActive() && beforeMatches && afterMatches && exactRoundTrip;
		};
		const bool unicodeInlineSelection = roundTripBodySelection(L"body-source-unicode", L"UNICODE_BEGIN", L"UNICODE_END", unicodeMarker, false);
		const bool repeatedTextSelection = roundTripBodySelection(L"body-source-repeat", L"REPEAT_TOKEN", L"REPEAT_TOKEN_REPEAT_END", L"REPEAT_TOKEN middle REPEAT_TOKEN_REPEAT_END", false);
		const bool tagBoundarySelection = roundTripBodySelection(L"body-source-boundary", L"BOUNDARY_BEGIN", L"BOUNDARY_END", L"BOUNDARY_BEGIN", false);
		const bool collapsedCaretSelection = roundTripBodySelection(L"body-source-caret", L"CARET_UNICODE", L"CARET_UNICODE", L"", true);
		ShowView(SOURCE);
		const bool sourceActive = IsSourceActive();
		const sptr_t initialLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> initialSource(static_cast<size_t>(initialLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, initialLength + 1, reinterpret_cast<LPARAM>(initialSource.data()));
		const char* const originalMarker = "BODY_SOURCE_ORIGINAL";
		const bool sourceCurrent = strstr(initialSource.data(), originalMarker) != NULL;
		ShowView(BODY);
		const bool bodyWithoutChange = !IsSourceActive() && m_doc == originalDocument && m_doc->m_body.Document() != NULL;
		ShowView(SOURCE);
		char* marker = strstr(initialSource.data(), originalMarker);
		const bool markerFound = marker != NULL;
		if(markerFound)
		{
			const sptr_t position = static_cast<sptr_t>(marker - initialSource.data());
			m_source.SendMessage(SCI_SETSEL, position, position + strlen(originalMarker));
			m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("BODY_SOURCE_EDITED"));
		}
		ShowView(BODY);
		const bool validEditApplied = markerFound && !IsSourceActive() && m_doc == originalDocument && m_doc->m_body.Document() != NULL;
		ShowView(SOURCE);
		const sptr_t editedLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> editedSource(static_cast<size_t>(editedLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, editedLength + 1, reinterpret_cast<LPARAM>(editedSource.data()));
		const bool editedSourceCurrent = strstr(editedSource.data(), "BODY_SOURCE_EDITED") != NULL;
		bool cycles = true;
		for(int cycle = 0; cycle < 3; ++cycle) { ShowView(BODY); cycles = cycles && !IsSourceActive(); ShowView(SOURCE); cycles = cycles && IsSourceActive(); }
		const sptr_t preservedSelectionStart = m_source.SendMessage(SCI_GETSELECTIONSTART);
		const sptr_t preservedSelectionEnd = m_source.SendMessage(SCI_GETSELECTIONEND);
		m_source.SendMessage(SCI_SELECTALL);
		m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("<FictionBook><broken>"));
		const bool invalidRejected = !SourceToHTML();
		const bool invalidPreserved = invalidRejected && IsSourceActive() && m_doc == originalDocument &&
			m_source.SendMessage(SCI_GETLENGTH) > 0 && m_source.SendMessage(SCI_GETSELECTIONSTART) >= 0 && m_source.SendMessage(SCI_GETSELECTIONEND) >= 0;
		CStringA report;
		report.Format("source_active=%d\nsource_current=%d\nbody_without_change=%d\nvalid_edit=%d\nedited_source=%d\ncycles=%d\ninvalid_rejected=%d\ninvalid_preserved=%d\nselection_saved=%d\nunicode_inline_selection=%d\nrepeated_text_selection=%d\ntag_boundary_selection=%d\ncollapsed_caret_selection=%d\nselection_diagnostics=%s\n", sourceActive, sourceCurrent, bodyWithoutChange, validEditApplied, editedSourceCurrent, cycles, invalidRejected, invalidPreserved, preservedSelectionStart >= 0 && preservedSelectionEnd >= 0, unicodeInlineSelection, repeatedTextSelection, tagBoundarySelection, collapsedCaretSelection, static_cast<LPCSTR>(selectionRoundTripDiagnostics));
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(sourceActive && sourceCurrent && bodyWithoutChange && validEditApplied && editedSourceCurrent && cycles && invalidPreserved && unicodeInlineSelection && repeatedTextSelection && tagBoundarySelection && collapsedCaretSelection ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"settings-dialog-runtime") || IsFbeTestScenario(L"settings-dialog-runtime-verify"))
	{
		const bool verifyOnly = IsFbeTestScenario(L"settings-dialog-runtime-verify");
		if (!verifyOnly)
			SendMessage(WM_COMMAND, MAKEWPARAM(ID_TOOLS_OPTIONS, 0), 0);
		const bool backupPersisted = _Settings.GetCreateBackupFile();
		const bool nbspPersisted = _Settings.GetNBSPChar() == L"\x25AB";
		const bool wrapPersisted = _Settings.XmlSrcWrap();
		const bool persisted = backupPersisted && nbspPersisted && wrapPersisted;
		const bool applied = verifyOnly || (m_source.SendMessage(SCI_GETWRAPMODE) != SC_WRAP_NONE);
		CStringA report;
		report.Format("settings_dialog=%d\npersisted=%d\nbackup=%d\nnbsp=%d\nwrap=%d\napplied=%d\n", !verifyOnly, persisted, backupPersisted, nbspPersisted, wrapPersisted, applied);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(persisted && applied ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"editor-view-lifecycle-runtime"))
	{
		FB::Doc* const originalDocument = m_doc;
		const auto isBodyHostActive = [&]() { return m_view.GetActiveWnd() == m_doc->m_body; };
		const auto descriptionModeEnabled = [&]()
		{
			MSHTML::IHTMLDocument3Ptr document(m_doc->m_body.Document());
			MSHTML::IHTMLElementPtr description = document ? document->getElementById(L"fbw_desc") : MSHTML::IHTMLElementPtr();
			return description && description->style && U::scmp(description->style->display, L"block") == 0;
		};
		ShowView(DESC);
		const bool bodyToDescription = m_editor_view_state.Current() == DESC && m_editor_view_state.Previous() == BODY && isBodyHostActive() && descriptionModeEnabled();
		ShowView(BODY);
		const bool descriptionToBody = m_editor_view_state.Current() == BODY && m_editor_view_state.Previous() == DESC && isBodyHostActive() && !descriptionModeEnabled();
		ShowView(SOURCE);
		const bool bodyToSource = m_editor_view_state.Current() == SOURCE && IsSourceActive() && m_editor_view_state.Previous() == BODY;
		ShowView(BODY);
		const bool sourceToBody = m_editor_view_state.Current() == BODY && !IsSourceActive() && m_editor_view_state.Previous() == SOURCE && isBodyHostActive();
		ShowView(DESC);
		ShowView(SOURCE);
		const bool descriptionToSource = m_editor_view_state.Current() == SOURCE && IsSourceActive() && m_editor_view_state.Previous() == DESC;
		ShowView(DESC);
		const bool sourceToDescription = m_editor_view_state.Current() == DESC && !IsSourceActive() && m_editor_view_state.Previous() == SOURCE && isBodyHostActive();
		ShowView(SOURCE);
		ShowView(BODY);
		ShowView(DESC);
		const bool sourceBodyDescription = m_editor_view_state.Current() == DESC && m_editor_view_state.Previous() == BODY && isBodyHostActive();
		ShowView(SOURCE);
		ShowView(DESC);
		ShowView(BODY);
		const bool sourceDescriptionBody = m_editor_view_state.Current() == BODY && m_editor_view_state.Previous() == DESC && isBodyHostActive();
		bool cycles = true;
		for(int cycle = 0; cycle < 3; ++cycle)
		{
			ShowView(DESC); cycles = cycles && m_editor_view_state.Current() == DESC && isBodyHostActive();
			ShowView(SOURCE); cycles = cycles && IsSourceActive();
			ShowView(BODY); cycles = cycles && m_editor_view_state.Current() == BODY && isBodyHostActive();
		}
		const bool documentPreserved = m_doc == originalDocument && m_doc->m_body.Document() != NULL;
		CStringA report;
		report.Format("body_desc=%d\ndesc_body=%d\nbody_source=%d\nsource_body=%d\ndesc_source=%d\nsource_desc=%d\nsource_body_desc=%d\nsource_desc_body=%d\ncycles=%d\ndocument=%d\n", bodyToDescription, descriptionToBody, bodyToSource, sourceToBody, descriptionToSource, sourceToDescription, sourceBodyDescription, sourceDescriptionBody, cycles, documentPreserved);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(bodyToDescription && descriptionToBody && bodyToSource && sourceToBody && descriptionToSource && sourceToDescription && sourceBodyDescription && sourceDescriptionBody && cycles && documentPreserved ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"archive-two-phase-runtime"))
	{
		wchar_t failedArchive[MAX_PATH] = {};
		const DWORD failedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_FAILURE_PATH", failedArchive, _countof(failedArchive));
		ShowView(SOURCE);
		const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
		char* marker = strstr(source.data(), "ARCHIVE_RUNTIME_BEFORE");
		if (marker)
		{
			const size_t offset = static_cast<size_t>(marker - source.data());
			m_source.SendMessage(SCI_SETSEL, static_cast<WPARAM>(offset), static_cast<LPARAM>(offset + strlen("ARCHIVE_RUNTIME_BEFORE")));
			m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("ARCHIVE_RUNTIME_UNSAVED"));
		}
		const CString filenameBefore(m_doc->m_filename);
		CString mruBefore;
		for (int index = 0; index < m_mru.m_arrDocs.GetSize(); ++index)
			mruBefore.AppendFormat(L"%d:%s\n", index, static_cast<LPCWSTR>(m_mru.m_arrDocs[index].szDocName));
		const FILE_OP_STATUS result = failedLength && failedLength < _countof(failedArchive) ? LoadFile(failedArchive) : FAIL;
		const bool sourceStillModified = m_source.SendMessage(SCI_GETMODIFY) != 0;
		const bool sameDocument = CString(m_doc->m_filename) == filenameBefore && !m_document_session.Location().IsArchive();
		CString mruAfter;
		for (int index = 0; index < m_mru.m_arrDocs.GetSize(); ++index)
			mruAfter.AppendFormat(L"%d:%s\n", index, static_cast<LPCWSTR>(m_mru.m_arrDocs[index].szDocName));
		const bool mruUnchanged = mruAfter == mruBefore;
		CStringA report;
		report.Format("open_cancelled=%d\nmodified=%d\nsame_document=%d\nmru_unchanged=%d\n", result == CANCELLED, sourceStillModified, sameDocument, mruUnchanged);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(result == CANCELLED && sourceStillModified && sameDocument && mruUnchanged ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"failed-open-runtime"))
	{
		wchar_t failedPath[MAX_PATH] = {};
		const DWORD failedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_FAILED_OPEN_PATH", failedPath, _countof(failedPath));
		FB::Doc* const original = m_doc;
		const DocumentLocation originalLocation = m_document_session.Location();
		CString mruBefore;
		for (int index = 0; index < m_mru.m_arrDocs.GetSize(); ++index) mruBefore.AppendFormat(L"%d:%s\n", index, static_cast<LPCWSTR>(m_mru.m_arrDocs[index].szDocName));
		const FILE_OP_STATUS result = failedLength && failedLength < _countof(failedPath) ? LoadFile(failedPath) : FAIL;
		CString mruAfter;
		for (int index = 0; index < m_mru.m_arrDocs.GetSize(); ++index) mruAfter.AppendFormat(L"%d:%s\n", index, static_cast<LPCWSTR>(m_mru.m_arrDocs[index].szDocName));
		const bool preserved = result == FAIL && m_doc == original && FB::Doc::m_active_doc == m_doc &&
			m_document_session.Location().storagePath == originalLocation.storagePath && mruBefore == mruAfter;
		CStringA report; report.Format("failed=%d\nidentity=%d\nactive=%d\nsession=%d\nmru_unchanged=%d\n", result == FAIL, m_doc == original, FB::Doc::m_active_doc == m_doc, m_document_session.Location().storagePath == originalLocation.storagePath, mruBefore == mruAfter);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(preserved ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"malformed-source-fallback-runtime"))
	{
		wchar_t malformedPath[MAX_PATH] = {};
		const DWORD malformedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_MALFORMED_SOURCE_PATH", malformedPath, _countof(malformedPath));
		FB::Doc* const original = m_doc;
		const DocumentLocation originalLocation = m_document_session.Location();
		const FILE_OP_STATUS result = malformedLength && malformedLength < _countof(malformedPath) ? LoadFile(malformedPath) : FAIL;
		const bool sourceFallback = result == OK && m_doc == original && FB::Doc::m_active_doc == m_doc &&
			m_bad_xml && m_bad_filename == malformedPath && m_editor_view_state.Current() == SOURCE &&
			m_document_session.Location().storagePath == originalLocation.storagePath;
		CStringA report; report.Format("fallback=%d\nidentity=%d\nactive=%d\nsession=%d\nsource=%d\n", result == OK, m_doc == original,
			FB::Doc::m_active_doc == m_doc, m_document_session.Location().storagePath == originalLocation.storagePath, m_bad_xml && m_bad_filename == malformedPath && m_editor_view_state.Current() == SOURCE);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(sourceFallback ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"successful-open-runtime"))
	{
		wchar_t openedPath[MAX_PATH] = {};
		const DWORD openedLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SUCCESSFUL_OPEN_PATH", openedPath, _countof(openedPath));
		FB::Doc* const original = m_doc;
		const FILE_OP_STATUS result = openedLength && openedLength < _countof(openedPath) ? LoadFile(openedPath) : FAIL;
		const bool opened = result == OK && m_doc != original && FB::Doc::m_active_doc == m_doc &&
			m_document_session.Location().storagePath == openedPath && m_doc->m_filename == openedPath && m_doc->m_body.Document() != NULL;
		CStringA report; report.Format("opened=%d\nidentity_changed=%d\nactive=%d\nsession=%d\nfilename=%d\nvalid=%d\n", result == OK,
			m_doc != original, FB::Doc::m_active_doc == m_doc, m_document_session.Location().storagePath == openedPath,
			m_doc->m_filename == openedPath, m_doc->m_body.Document() != NULL);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(opened ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"new-document-runtime"))
	{
		FB::Doc* const original = m_doc;
		BOOL handled = FALSE;
		OnFileNew(0, ID_FILE_NEW, NULL, handled);
		const bool created = m_doc != original && FB::Doc::m_active_doc == m_doc &&
			m_document_session.Location().storagePath.IsEmpty() && !m_document_session.Location().IsArchive() && m_doc->m_body.Document() != NULL;
		CStringA report; report.Format("created=%d\nidentity_changed=%d\nactive=%d\nsession_new=%d\nvalid=%d\n", created,
			m_doc != original, FB::Doc::m_active_doc == m_doc, m_document_session.Location().storagePath.IsEmpty() && !m_document_session.Location().IsArchive(), m_doc->m_body.Document() != NULL);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(created ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"reload-success-runtime") || IsFbeTestScenario(L"reload-failure-runtime"))
	{
		const bool expectSuccess = IsFbeTestScenario(L"reload-success-runtime");
		FB::Doc* const original = m_doc;
		const DocumentLocation originalLocation = m_document_session.Location();
		if (!expectSuccess) ::SetEnvironmentVariable(L"FBE_NEXT_FAULT_INJECT", L"api-load-return-false");
		const bool result = ReloadFile();
		if (!expectSuccess) ::SetEnvironmentVariable(L"FBE_NEXT_FAULT_INJECT", NULL);
		const bool reloadMatches = expectSuccess
			? result && m_doc != original && FB::Doc::m_active_doc == m_doc && m_doc->m_body.Document() != NULL &&
				m_document_session.Location().storagePath == originalLocation.storagePath
			: !result && m_doc == original && FB::Doc::m_active_doc == m_doc &&
				m_document_session.Location().storagePath == originalLocation.storagePath;
		CStringA report; report.Format("reloaded=%d\nidentity=%d\nactive=%d\nsession=%d\nvalid=%d\n", result,
			expectSuccess ? m_doc != original : m_doc == original, FB::Doc::m_active_doc == m_doc,
			m_document_session.Location().storagePath == originalLocation.storagePath, m_doc->m_body.Document() != NULL);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(reloadMatches ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"archive-mru-runtime"))
	{
		wchar_t secondEntry[MAX_PATH] = {}, occurrenceText[16] = {}, normalEntries[8192] = {};
		const DWORD secondLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_MRU_SECOND_ENTRY", secondEntry, _countof(secondEntry));
		::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_MRU_SECOND_OCCURRENCE", occurrenceText, _countof(occurrenceText));
		const DWORD normalLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_MRU_NORMAL_ENTRIES", normalEntries, _countof(normalEntries));
		DocumentLocation first = m_document_session.Location(), second = first;
		unsigned int occurrence = 0;
		const bool secondValid = secondLength > 0 && secondLength < _countof(secondEntry) && FbeRecentDocuments::ParseArchiveMruUnsigned(occurrenceText, occurrence);
		second.entryPath = secondEntry; second.entryOccurrence = occurrence; second.documentType = DetectFictionBookFileType(second.entryPath);
		// This scenario exercises MRU routing, not the unsaved-changes prompt.
		// A freshly loaded MSHTML document can carry a transient form-change bit.
		m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT);
		ResolvedOpenDocument secondResolved; FbeArchive::Error secondError;
		const bool secondFound = secondValid && FbeArchiveUi::ResolveOpenRequest(second.storagePath, secondResolved, &second, &secondError);
		const FILE_OP_STATUS secondOpen = secondFound ? LoadFile(second.storagePath, &second) : CANCELLED;
		if (secondOpen == OK) FbeRecentDocuments::RememberArchiveMruRecord(m_mru, m_document_session.Location());
		if (secondOpen == OK) { m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT); }
		std::vector<FbeRecentDocuments::ArchiveMruRecord> records; FbeRecentDocuments::ReadArchiveMruRecords(records);
		const CString firstKey = FbeRecentDocuments::ArchiveMruKey(first); WORD firstCommand = 0;
		for (int offset = 0; offset < m_mru.m_arrDocs.GetSize() && offset <= ID_FILE_MRU_LAST - ID_FILE_MRU_FIRST; ++offset) { CString key; const WORD candidate = MruCommandId(offset); if (m_mru.GetFromList(candidate, key) && key == firstKey) { firstCommand = candidate; break; } }
		DocumentLocation menuFirst;
		const bool menuLookup = firstCommand != 0 && FbeRecentDocuments::FindArchiveMruRecord(firstKey, menuFirst) && FbeRecentDocuments::SameArchiveMruIdentity(menuFirst, first);
		BOOL handled = FALSE;
		const LRESULT handlerResult = secondOpen == OK && menuLookup ? OnFileOpenMRU(0, firstCommand, NULL, handled) : 1;
		const FILE_OP_STATUS firstOpen = handlerResult == 0 && FbeRecentDocuments::SameArchiveMruIdentity(m_document_session.Location(), first) ? OK : FAIL;
		const bool reopenedFirst = firstOpen == OK && FbeRecentDocuments::SameArchiveMruIdentity(m_document_session.Location(), first);
		bool normalEntriesOpened = normalLength == 0;
		if (normalLength > 0 && normalLength < _countof(normalEntries))
		{
			int position = 0;
			while (position >= 0)
			{
				const CString normal = CString(normalEntries).Tokenize(L"|", position);
				if (normal.IsEmpty()) continue;
				m_doc->MarkSavePoint(); m_source.SendMessage(SCI_SETSAVEPOINT);
				if (LoadFile(normal) != OK) { normalEntriesOpened = false; break; }
				FbeRecentDocuments::RememberNormalMruRecord(m_mru, normal);
				normalEntriesOpened = true;
			}
		}
		FbeRecentDocuments::ReadArchiveMruRecords(records);
		bool hasFirst = false, hasSecond = false;
		for (size_t index = 0; index < records.size(); ++index) { hasFirst = hasFirst || FbeRecentDocuments::SameArchiveMruIdentity(records[index].location, first); hasSecond = hasSecond || FbeRecentDocuments::SameArchiveMruIdentity(records[index].location, second); }
		DocumentLocation missing = first; missing.entryPath = L"missing.fb2"; missing.entryOccurrence = 0;
		ResolvedOpenDocument ignored; FbeArchive::Error missingError;
		const bool missingRejected = !FbeArchiveUi::ResolveOpenRequest(first.storagePath, ignored, &missing, &missingError) && missingError.code == FbeArchive::ErrorCode::EntryNotFound;
		int menuCount = 0, visibleArchiveCount = 0; bool menuClean = true;
		const HMENU mruMenu = m_mru.GetMenuHandle();
		if (mruMenu != NULL) for (int index = 0; index < ::GetMenuItemCount(mruMenu); ++index)
		{
			MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_ID;
			if (!::GetMenuItemInfo(mruMenu, index, TRUE, &item) || item.wID < ID_FILE_MRU_FIRST || item.wID > ID_FILE_MRU_LAST) continue;
			++menuCount; wchar_t caption[512] = {};
			::GetMenuString(mruMenu, item.wID, caption, _countof(caption), MF_BYCOMMAND);
			DocumentLocation captionLocation;
			if (FbeRecentDocuments::ParseArchiveMruKey(caption, captionLocation) || CString(caption).Left(2) == L"&1" || CString(caption).Left(2) == L"&2") menuClean = false;
			CString key; DocumentLocation keyLocation;
			if (m_mru.GetFromList(item.wID, key) && FbeRecentDocuments::ParseArchiveMruKey(key, keyLocation)) ++visibleArchiveCount;
		}
		const CString firstCaption = FbeRecentDocuments::ArchiveMruCaption(firstKey), secondCaption = FbeRecentDocuments::ArchiveMruCaption(FbeRecentDocuments::ArchiveMruKey(second));
		const bool captionsDifferent = firstCaption.Compare(secondCaption) != 0;
		const bool captionsDistinct = visibleArchiveCount < 2 || captionsDifferent;
		FbeRecentDocuments::WritePortableMru(m_mru);
		CStringA report; report.Format("first=%d\nsecond=%d\nmenu_lookup=%d\nsecond_found=%d\nsecond_error=%d\nsecond_open=%d\nfirst_open=%d\nreopened_first=%d\nmissing_entry=%d\narchive_records=%u\nnormal_entries=%d\nmenu_count=%d\nmenu_clean=%d\ncaption_diff=%d\ncaptions_distinct=%d\n", hasFirst, hasSecond, menuLookup, secondFound, static_cast<int>(secondError.code), secondOpen, firstOpen, reopenedFirst, missingRejected, static_cast<unsigned int>(records.size()), normalEntriesOpened, menuCount, menuClean, captionsDifferent, captionsDistinct);
		DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
		::PostQuitMessage(hasFirst && hasSecond && menuLookup && reopenedFirst && missingRejected && normalEntriesOpened && menuCount > 0 && menuCount <= 10 && menuClean && captionsDistinct ? 0 : 1);
		return 0;
	}
	if (IsFbeTestScenario(L"archive-mru-restart-runtime"))
	{
		std::vector<CString> order; FbeRecentDocuments::ReadMruOrder(order);
		const int count = m_mru.m_arrDocs.GetSize(); bool exactOrder = count == 10 && order.size() == 10, cleanMenu = true; int menuCount = 0;
		for (int index = 0; index < count && exactOrder; ++index) exactOrder = CString(m_mru.m_arrDocs[index].szDocName) == order[order.size() - 1 - index];
		for (int index = 0; index < count; ++index) { DocumentLocation location; if (FbeRecentDocuments::ParseArchiveMruKey(CString(m_mru.m_arrDocs[index].szDocName), location) && !FbeRecentDocuments::FindArchiveMruRecord(CString(m_mru.m_arrDocs[index].szDocName), location)) cleanMenu = false; }
		std::vector<CString> captions; const HMENU menu = m_mru.GetMenuHandle(); int emptyCaption = 0, rawCaption = 0, numberedCaption = 0, duplicateCaption = 0, disabledCaption = 0; bool minimalFolderContexts = false;
		if (menu == NULL) cleanMenu = false; else for (int index = 0; index < ::GetMenuItemCount(menu); ++index)
		{
			MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_ID;
			if (!::GetMenuItemInfo(menu, index, TRUE, &item) || item.wID < ID_FILE_MRU_FIRST || item.wID > ID_FILE_MRU_LAST) continue;
			++menuCount; wchar_t text[512] = {}; ::GetMenuString(menu, index, text, _countof(text), MF_BYPOSITION); CString caption(text), parsedKey;
			item.fMask = MIIM_STATE; ::GetMenuItemInfo(menu, index, TRUE, &item); DocumentLocation parsed; if (caption == m_mru.m_szNoEntries) { ++emptyCaption; } if ((item.fState & (MFS_DISABLED | MFS_GRAYED)) != 0) { cleanMenu = false; ++disabledCaption; } if (FbeRecentDocuments::ParseArchiveMruKey(caption, parsed)) { cleanMenu = false; ++rawCaption; } if (caption.GetLength() > 1 && caption[0] == L'&' && caption[1] >= L'0' && caption[1] <= L'9') { cleanMenu = false; ++numberedCaption; }
			for (size_t previous = 0; previous < captions.size(); ++previous) if (captions[previous].CompareNoCase(caption) == 0) { cleanMenu = false; ++duplicateCaption; }
			captions.push_back(caption);
		}
		for (size_t i = 0; i < captions.size(); ++i) for (size_t j = i + 1; j < captions.size(); ++j)
			if ((captions[i].Find(L"A\\Books\\archive.zip") >= 0 && captions[j].Find(L"B\\Books\\archive.zip") >= 0) || (captions[i].Find(L"B\\Books\\archive.zip") >= 0 && captions[j].Find(L"A\\Books\\archive.zip") >= 0)) minimalFolderContexts = true;
		wchar_t path[MAX_PATH] = {}, entry[MAX_PATH] = {}, occurrenceText[16] = {}; ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_MRU_REOPEN_PATH", path, _countof(path)); ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_ENTRY", entry, _countof(entry)); ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_OCCURRENCE", occurrenceText, _countof(occurrenceText));
		unsigned int occurrence = 0; DocumentLocation target; target.containerKind = DetectDocumentContainerKind(path); target.storagePath = path; target.entryPath = entry; target.entryOccurrence = FbeRecentDocuments::ParseArchiveMruUnsigned(occurrenceText, occurrence) ? occurrence : 0; target.documentType = DetectFictionBookFileType(target.entryPath);
		WORD command = 0; const CString key = FbeRecentDocuments::ArchiveMruKey(target); for (int offset = 0; offset < count && offset <= ID_FILE_MRU_LAST - ID_FILE_MRU_FIRST; ++offset) { CString value; const WORD candidate = MruCommandId(offset); if (m_mru.GetFromList(candidate, value) && value == key) { command = candidate; break; } }
		BOOL handled = FALSE; const bool reopened = command != 0 && OnFileOpenMRU(0, command, NULL, handled) == 0 && FbeRecentDocuments::SameArchiveMruIdentity(m_document_session.Location(), target);
		auto getMruItem = [&](UINT id, CString& caption, UINT& state) -> bool
		{
			const int position = FindMenuPositionByCommand(menu, id);
			if (position < 0) return false;
			wchar_t text[512] = {}; if (::GetMenuString(menu, position, text, _countof(text), MF_BYPOSITION) <= 0) return false;
			MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_STATE;
			if (!::GetMenuItemInfo(menu, position, TRUE, &item)) return false;
			caption = text; state = item.fState; return true;
		};
		CString firstBefore, firstRussian; UINT firstBeforeState = 0, firstRussianState = 0;
		CString firstKeyBefore; const bool firstMappedBefore = m_mru.GetFromList(ID_FILE_MRU_FIRST, firstKeyBefore);
		const bool firstVisibleBefore = getMruItem(ID_FILE_MRU_FIRST, firstBefore, firstBeforeState);
		_Settings.SetInterfaceLanguage(FBE_INTERFACE_LANGUAGE_RUSSIAN);
		FbePublishRuntimeLocaleName(_Settings.GetInterfaceLocaleName()); FbeResetRuntimeLocalization(); RefreshLocalizedMainFrameUi();
		const bool russianLocaleSelected = _Settings.GetInterfaceLocaleName() == L"ru-RU";
		const CString russianLocalizedEmpty = FbeLoadRuntimeStringByKey(L"fbe.menu.idr_mainframe.recent.empty", L"No Recent Files");
		CString firstKeyRussian; const bool firstMappedRussian = m_mru.GetFromList(ID_FILE_MRU_FIRST, firstKeyRussian);
		const bool firstVisibleRussian = getMruItem(ID_FILE_MRU_FIRST, firstRussian, firstRussianState);
		const bool nonEmptyMruLocalized = firstMappedBefore && firstMappedRussian && firstKeyBefore == firstKeyRussian && firstVisibleBefore && firstVisibleRussian && firstBefore == firstRussian && firstRussian != m_mru.m_szNoEntries && (firstRussianState & (MFS_DISABLED | MFS_GRAYED)) == 0;
		m_mru.m_arrDocs.RemoveAll();
		RefreshMruEmptyStateText(m_mru); FbeRecentDocuments::RebuildMruMenu(m_mru);
		CString russianEmpty; UINT russianEmptyState = 0; int russianEmptyCount = 0;
		for (int index = 0; index < ::GetMenuItemCount(menu); ++index) { MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_ID; if (::GetMenuItemInfo(menu, index, TRUE, &item) && item.wID >= ID_FILE_MRU_FIRST && item.wID <= ID_FILE_MRU_LAST) ++russianEmptyCount; }
		const bool russianEmptyOk = russianEmptyCount == 1 && !russianLocalizedEmpty.IsEmpty() && getMruItem(ID_FILE_MRU_FIRST, russianEmpty, russianEmptyState) && russianEmpty == russianLocalizedEmpty && (russianEmptyState & (MFS_DISABLED | MFS_GRAYED)) != 0;
		_Settings.SetInterfaceLanguage(FBE_INTERFACE_LANGUAGE_ENGLISH);
		FbePublishRuntimeLocaleName(_Settings.GetInterfaceLocaleName()); FbeResetRuntimeLocalization(); RefreshLocalizedMainFrameUi();
		CString englishEmpty; UINT englishEmptyState = 0; int englishEmptyCount = 0;
		for (int index = 0; index < ::GetMenuItemCount(menu); ++index) { MENUITEMINFO item = { sizeof(item) }; item.fMask = MIIM_ID; if (::GetMenuItemInfo(menu, index, TRUE, &item) && item.wID >= ID_FILE_MRU_FIRST && item.wID <= ID_FILE_MRU_LAST) ++englishEmptyCount; }
		const bool englishEmptyOk = englishEmptyCount == 1 && getMruItem(ID_FILE_MRU_FIRST, englishEmpty, englishEmptyState) && englishEmpty == L"No Recent Files" && (englishEmptyState & (MFS_DISABLED | MFS_GRAYED)) != 0;
		CStringA report; report.Format("count=%d\nmenu_count=%d\norder=%d\nclean=%d\nempty=%d\ndisabled=%d\nraw=%d\nnumbered=%d\nduplicates=%d\nfolders=%d\nreopened=%d\nlocalized_nonempty=%d\nrussian_empty=%d\nrussian_locale=%d\nenglish_empty=%d\n", count, menuCount, exactOrder, cleanMenu, emptyCaption, disabledCaption, rawCaption, numberedCaption, duplicateCaption, minimalFolderContexts, reopened, nonEmptyMruLocalized, russianEmptyOk, russianLocaleSelected, englishEmptyOk); DWORD written = 0; output.Write(report, static_cast<DWORD>(report.GetLength()), &written); output.Close();
		::PostQuitMessage(count == 10 && menuCount == 10 && exactOrder && cleanMenu && reopened && nonEmptyMruLocalized && russianEmptyOk && englishEmptyOk ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"archive-recovery-create"))
	{
		ShowView(SOURCE);
		const sptr_t length = m_source.SendMessage(SCI_GETLENGTH); std::vector<char> source(static_cast<size_t>(length) + 1);
		m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(source.data()));
		char* marker = strstr(source.data(), "ARCHIVE_RUNTIME_BEFORE");
		if (marker) { const size_t offset = static_cast<size_t>(marker - source.data()); m_source.SendMessage(SCI_SETSEL, offset, offset + strlen("ARCHIVE_RUNTIME_BEFORE")); m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("ARCHIVE_RUNTIME_RECOVERY")); }
		else
		{
			char* titleEnd = strstr(source.data(), "</book-title>");
			const size_t offset = titleEnd ? static_cast<size_t>(titleEnd - source.data()) : static_cast<size_t>(length);
			m_source.SendMessage(SCI_SETSEL, offset, offset);
			m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>(" ARCHIVE_RUNTIME_RECOVERY"));
		}
		const bool saved = SourceToHTML() && SaveRecoveryNow(); CStringA report; report.Format("recovery_created=%d\n", saved); DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Close(); ::PostQuitMessage(saved ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"archive-recovery-verify"))
	{
		ShowView(SOURCE);
		const sptr_t length = m_source.SendMessage(SCI_GETLENGTH); std::vector<char> source(static_cast<size_t>(length) + 1);
		m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(source.data()));
		const bool archive = m_document_session.Location().IsArchive(); const bool fbd = m_doc->GetDocumentFileType() == FictionBookFileType::Fbd;
		const bool payload = strstr(source.data(), "ARCHIVE_RUNTIME_RECOVERY") != NULL;
		CStringA report; report.Format("archive=%d\nfbd=%d\nrecovery_payload=%d\n", archive, fbd, payload); DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Close(); ::PostQuitMessage(archive && payload ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"archive-recovery-external-verify"))
	{
		ShowView(SOURCE);
		const sptr_t length = m_source.SendMessage(SCI_GETLENGTH); std::vector<char> source(static_cast<size_t>(length) + 1);
		m_source.SendMessage(SCI_GETTEXT, length + 1, reinterpret_cast<LPARAM>(source.data()));
		char* marker = strstr(source.data(), "ARCHIVE_RUNTIME_RECOVERY");
		if (marker) { const size_t offset = static_cast<size_t>(marker - source.data()); m_source.SendMessage(SCI_SETSEL, offset, offset + strlen("ARCHIVE_RUNTIME_RECOVERY")); m_source.SendMessage(SCI_REPLACESEL, 0, reinterpret_cast<LPARAM>("ARCHIVE_RUNTIME_EXTERNAL")); }
		::SetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_SAVE_ERROR", NULL);
		const bool blocked = marker != NULL && SourceToHTML() && SaveFile(false) == FAIL;
		wchar_t errorCode[16] = {};
		const bool modifiedExternally = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_ARCHIVE_SAVE_ERROR", errorCode, _countof(errorCode)) > 0 &&
			_wtoi(errorCode) == static_cast<int>(FbeArchive::ErrorCode::ModifiedExternally);
		CStringA report; report.Format("archive=%d\nblocked=%d\nmodified_externally=%d\nerror_code=%S\n", m_document_session.Location().IsArchive(), blocked, modifiedExternally, errorCode); DWORD written = 0; output.Write(report, report.GetLength(), &written); output.Close(); ::PostQuitMessage(blocked && modifiedExternally ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"table-roundtrip"))
	{
		const ULONGLONG start = ::GetTickCount64();
		auto appendTablePhase = [&](const char* phase)
		{
			const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
			std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
			m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
			auto countTag = [&](const char* tag) -> long { long count = 0; for (const char* position = source.data(); (position = strstr(position, tag)) != NULL; ++position) ++count; return count; };
			const ProcessMemorySnapshot memory = GetProcessMemorySnapshot();
			CStringA row;
			row.Format("%s\t%I64u\t%I64u\t%I64u\t%ld\t%ld\t%ld\t%ld\r\n", phase,
				::GetTickCount64() - start, static_cast<unsigned __int64>(memory.privateBytes), static_cast<unsigned __int64>(memory.workingSetBytes),
				countTag("<table"), countTag("<tr"), countTag("<td"), countTag("<th"));
			DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
		};
		CStringA header("phase\telapsed_ms\tprivate_bytes\tworking_set_bytes\ttable_count\ttr_count\ttd_count\tth_count\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written); output.Flush();
		appendTablePhase("open-complete");
		for (int cycle = 1; cycle <= 5; ++cycle)
		{
			CStringA phase; phase.Format("source-%d-start", cycle); appendTablePhase(phase);
			ShowView(SOURCE); phase.Format("source-%d-complete", cycle); appendTablePhase(phase);
			phase.Format("body-%d-start", cycle); appendTablePhase(phase);
			ShowView(BODY); phase.Format("body-%d-complete", cycle); appendTablePhase(phase);
		}
		appendTablePhase("save-1-start");
		if (!m_doc->Save())
		{
			appendTablePhase("save-1-failed;phase=save-1;operation=Save;actual_hresult=unavailable;symbolic_hresult=unavailable");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		appendTablePhase("save-1-complete");
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"editor-background-runtime"))
	{
		StartupTrace::AppendTestStartupBreadcrumb("scenario-enter");
		CStringA header("phase\timage\tcss_url\trepeat\tposition\tsize\tattachment\tmodified\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		auto backgroundPathFromEnvironment = [](const wchar_t* name) -> CString
		{
			wchar_t value[1024] = {};
			const DWORD length = ::GetEnvironmentVariable(name, value, _countof(value));
			return length && length < _countof(value) ? CString(value) : CString();
		};
		auto appendBackgroundPhase = [&](const char* phase)
		{
			MSHTML::IHTMLStylePtr style(m_doc->m_body.Document() ? m_doc->m_body.Document()->body->style : MSHTML::IHTMLStylePtr());
			CString image, repeat, position, attachment, cssText, size(L"auto");
			CString cssUrl;
			if(style) {
				image = static_cast<LPCWSTR>(style->backgroundImage); repeat = static_cast<LPCWSTR>(style->backgroundRepeat);
				position = static_cast<LPCWSTR>(style->backgroundPosition); attachment = static_cast<LPCWSTR>(style->backgroundAttachment);
				cssText = static_cast<LPCWSTR>(style->cssText);
				if(cssText.Find(L"background-size: contain") >= 0) size = L"contain";
				else if(cssText.Find(L"background-size: cover") >= 0) size = L"cover";
				else { _variant_t sizeAttribute(style->getAttribute(L"background-size", 0)); if(sizeAttribute.vt == VT_BSTR && sizeAttribute.bstrVal) size = sizeAttribute.bstrVal; }
			}
			CString path;
			if(_Settings.GetEditorBackgroundKind() == L"builtin") EditorBackgrounds::ResolveBuiltIn(_Settings.GetEditorBackgroundId(), path);
			else if(_Settings.GetEditorBackgroundKind() == L"custom") path = _Settings.GetEditorBackgroundCustomPath();
			if(!path.IsEmpty()) { const CString uri = U::UrlFromPath(path); if(!uri.IsEmpty()) cssUrl.Format(L"url(\"%s\")", static_cast<LPCWSTR>(uri)); }
			CStringA imageA(CW2A(image, CP_UTF8)), repeatA(CW2A(repeat, CP_UTF8)), positionA(CW2A(position, CP_UTF8));
			CStringA cssUrlA(CW2A(cssUrl, CP_UTF8)), sizeA(CW2A(size, CP_UTF8)), attachmentA(CW2A(attachment, CP_UTF8));
			CStringA row; row.Format("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%d\r\n", phase,
				imageA.GetString(), cssUrlA.GetString(), repeatA.GetString(), positionA.GetString(), sizeA.GetString(), attachmentA.GetString(), m_doc->DocChanged() ? 1 : 0);
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
		};
		_Settings.SetEditorBackgroundKind(L"none"); _Settings.SetEditorBackgroundId(CString()); _Settings.SetEditorBackgroundCustomPath(CString()); _Settings.SetEditorBackgroundLayout(L"tile");
		StartupTrace::AppendTestStartupBreadcrumb("none-start"); m_doc->ApplyConfChanges(); appendBackgroundPhase("none"); StartupTrace::AppendTestStartupBreadcrumb("none-complete");
		_Settings.SetEditorBackgroundKind(L"builtin"); _Settings.SetEditorBackgroundId(L"01_clean_white"); _Settings.SetEditorBackgroundLayout(L"tile");
		StartupTrace::AppendTestStartupBreadcrumb("builtin-tile-start"); m_doc->ApplyConfChanges(); appendBackgroundPhase("builtin-tile"); StartupTrace::AppendTestStartupBreadcrumb("builtin-tile-complete");
		for(const wchar_t* layout : { L"center", L"contain", L"cover" }) {
			_Settings.SetEditorBackgroundLayout(layout); m_doc->ApplyConfChanges();
			if(wcscmp(layout, L"center") == 0) { appendBackgroundPhase("builtin-center"); StartupTrace::AppendTestStartupBreadcrumb("builtin-center-complete"); }
			else if(wcscmp(layout, L"contain") == 0) { appendBackgroundPhase("builtin-contain"); StartupTrace::AppendTestStartupBreadcrumb("builtin-contain-complete"); }
			else { appendBackgroundPhase("builtin-cover"); StartupTrace::AppendTestStartupBreadcrumb("builtin-cover-complete"); }
		}
		StartupTrace::AppendTestStartupBreadcrumb("source-view-start"); ShowView(SOURCE); StartupTrace::AppendTestStartupBreadcrumb("source-view-complete"); StartupTrace::AppendTestStartupBreadcrumb("body-view-start"); ShowView(BODY); StartupTrace::AppendTestStartupBreadcrumb("body-view-complete");
		// Source-to-Body recreates the MSHTML document in this test harness.  Its
		// freshly loaded version is the baseline before checking that UI-only
		// background changes leave the document clean.
		m_doc->MarkSavePoint();
		m_doc->ApplyConfChanges(); appendBackgroundPhase("builtin-after-view-recreate"); StartupTrace::AppendTestStartupBreadcrumb("view-recreate-complete");
		_Settings.SetEditorBackgroundId(L"unknown-background"); m_doc->ApplyConfChanges(); appendBackgroundPhase("unknown-builtin"); StartupTrace::AppendTestStartupBreadcrumb("unknown-builtin-complete");
		_Settings.SetEditorBackgroundKind(L"custom"); _Settings.SetEditorBackgroundCustomPath(backgroundPathFromEnvironment(L"FBE_NEXT_TEST_BACKGROUND_MISSING_PATH")); m_doc->ApplyConfChanges(); appendBackgroundPhase("missing-custom"); StartupTrace::AppendTestStartupBreadcrumb("missing-custom-complete");
		_Settings.SetEditorBackgroundCustomPath(backgroundPathFromEnvironment(L"FBE_NEXT_TEST_BACKGROUND_PATH")); _Settings.SetEditorBackgroundLayout(L"contain"); m_doc->ApplyConfChanges(); appendBackgroundPhase("custom"); StartupTrace::AppendTestStartupBreadcrumb("custom-complete");
		_Settings.SetEditorBackgroundKind(L"builtin"); _Settings.SetEditorBackgroundId(L"01_clean_white"); _Settings.SetEditorBackgroundLayout(L"tile"); m_doc->ApplyConfChanges(); appendBackgroundPhase("before-save"); StartupTrace::AppendTestStartupBreadcrumb("before-save");
		StartupTrace::AppendTestStartupBreadcrumb("save-start");
		if(!m_doc->Save()) {
			CString saveFailure;
			saveFailure.Format(L"editor background runtime save failed; name-valid=%d", m_doc->m_namevalid ? 1 : 0);
			StartupTrace::HResult(L"test", L"TST202", m_doc->GetLastSaveError(), saveFailure);
			CStringA savePhase;
			savePhase.Format("save-failed-hr-0x%08lX", static_cast<unsigned long>(m_doc->GetLastSaveError()));
			appendBackgroundPhase(savePhase);
			StartupTrace::AppendTestStartupBreadcrumb(m_doc->m_namevalid ? "save-failed" : "save-failed-name-invalid");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		StartupTrace::AppendTestStartupBreadcrumb("save-complete");
		appendBackgroundPhase("after-save"); output.Flush(); StartupTrace::AppendTestStartupBreadcrumb("after-save"); StartupTrace::AppendTestStartupBreadcrumb("report-flush"); output.Close(); StartupTrace::AppendTestStartupBreadcrumb("report-closed"); StartupTrace::AppendTestStartupBreadcrumb("shutdown-requested"); ::PostQuitMessage(0); StartupTrace::AppendTestStartupBreadcrumb("shutdown-quit-posted"); return 0;
	}
	if (IsFbeTestScenario(L"structural-trace-contract"))
	{
		wchar_t tracePath[MAX_PATH] = {};
		const DWORD traceLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_TRACE", tracePath, _countof(tracePath));
		FbeStructure::StructuralTrace trace(traceLength ? tracePath : nullptr, L"cite\ttrace", L"case\r\ntrace");
		trace.Before(L"phase\tone", L"TAB\tCR\rLF\n");
		trace.After(L"phase-two", L"complete");
		trace.Hr(L"failed-write", E_ACCESSDENIED, L"denied");
		CStringA row; row.Format("enabled\twrite_failed\tlast_error\tresult\r\n%d\t%d\t0x%08lX\tpass\r\n", trace.IsEnabled() ? 1 : 0, trace.HasWriteFailure() ? 1 : 0, static_cast<unsigned long>(trace.LastError()));
		DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(0); return 0;
	}
	if (IsFbeTestScenario(L"cite-poem-undo"))
	{
		wchar_t operation[16] = {};
		wchar_t target[16] = {};
		wchar_t selectionMode[16] = {};
		wchar_t tracePath[MAX_PATH] = {};
		wchar_t traceCase[64] = {};
		wchar_t route[16] = {}, citePoemFault[32] = {};
		const DWORD operationLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_OPERATION", operation, _countof(operation));
		const DWORD targetLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_TARGET", target, _countof(target));
		const DWORD selectionModeLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_SELECTION_MODE", selectionMode, _countof(selectionMode));
		const DWORD traceLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_TRACE", tracePath, _countof(tracePath));
		const DWORD traceCaseLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_CASE", traceCase, _countof(traceCase));
		const DWORD routeLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_ROUTE", route, _countof(route));
		const DWORD citePoemFaultLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_CITE_POEM_FAULT", citePoemFault, _countof(citePoemFault));
		const bool cite = operationLength == 4 && wcscmp(operation, L"cite") == 0;
		const bool poem = operationLength == 4 && wcscmp(operation, L"poem") == 0;
		const wchar_t* targetClass = targetLength ? target : L"section";
		const CStringA targetName((CW2A(targetClass)));
		const bool selectCaret = selectionModeLength == 5 && wcscmp(selectionMode, L"caret") == 0;
		const CStringA selectionName(selectCaret ? "caret" : "selected");
		const bool repeat = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_REPEAT", nullptr, 0) != 0;
		const bool viaWrapper = routeLength == 7 && wcscmp(route, L"wrapper") == 0;
		CStringA header("operation\ttarget\tselection_mode\tselection_collapsed\tselection_text_utf16\tselection_html_utf16\tselection_parent_utf16\tselection_start_to_first_start\tselection_end_to_first_end\tcheck_allowed\tbefore_equals_undo\tafter_equals_redo\tsequential_cycle\tbefore_paragraphs\tafter_cites\tafter_poems\tafter_stanzas\tpoem_text_utf16\tempty_divs\tempty_paragraphs\tempty_stanzas\tsaved\tresult\tcheck_status\tapply_status\thresult\tdocument_changed\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		auto writeFailure = [&](const char* reason)
		{
			CStringA row; row.Format("%s\t%s\t%s\t0\t-\t-\t-\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t-\t0\t0\t0\t0\t%s\r\n", cite ? "cite" : poem ? "poem" : "unknown", (LPCSTR)targetName, (LPCSTR)selectionName, reason);
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1);
		};
		if (!cite && !poem) { writeFailure("invalid-operation"); return 0; }
		MSHTML::IHTMLElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr divs(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"DIV") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr container;
		for (long index = 0; divs && index < divs->length; ++index) {
			MSHTML::IHTMLElementPtr div(divs->item(_variant_t(index), _variant_t()));
			if (div && U::scmp(div->className, targetClass) == 0) { container = div; break; }
		}
		MSHTML::IHTMLElementCollectionPtr paragraphs(container ? MSHTML::IHTMLElement2Ptr(container)->getElementsByTagName(L"P") : MSHTML::IHTMLElementCollectionPtr());
		if (!body || !container || !paragraphs || paragraphs->length == 0) { writeFailure("missing-paragraph"); return 0; }
		MSHTML::IHTMLElementPtr first(paragraphs->item(_variant_t(0L), _variant_t()));
		MSHTML::IHTMLElementPtr last(paragraphs->item(_variant_t(paragraphs->length - 1), _variant_t()));
		MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		MSHTML::IHTMLTxtRangePtr rangeEnd(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		if (!first || !last || !range || !rangeEnd) { writeFailure("selection-create"); return 0; }
		m_doc->m_body.SetFocus();
		range->moveToElementText(first); range->collapse(VARIANT_TRUE);
		// Keep the caret inside an empty P.  Moving one character from its start
		// makes MSHTML place the range after the P, in an editor-owned DIV.
		if (!CString((const wchar_t*)first->innerText).IsEmpty())
			range->move(L"character", 1);
		if (!selectCaret) {
			rangeEnd->moveToElementText(last);
			rangeEnd->collapse(VARIANT_FALSE); rangeEnd->move(L"character", -1);
			// Both boundaries must be inside their P elements. MSHTML otherwise
			// reports the enclosing DIV as parentElement(), and
			// The structural editor rejects this selection.
			range->setEndPoint(L"EndToEnd", rangeEnd);
		}
		auto utf16Summary = [](const CString& value) -> CStringA
		{
			if (value.IsEmpty()) return CStringA("-");
			CStringA summary;
			for (int index = 0; index < value.GetLength(); ++index) {
				if (index) summary += ',';
				CStringA codeUnit; codeUnit.Format("%04X", static_cast<unsigned int>(static_cast<unsigned short>(value[index])));
				summary += codeUnit;
			}
			return summary;
		};
		const CString selectionText((const wchar_t*)range->text), selectionHtml((const wchar_t*)range->htmlText);
		const bool selectionCollapsed = range->compareEndPoints(L"StartToEnd", range) == 0;
		MSHTML::IHTMLTxtRangePtr firstRange(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		firstRange->moveToElementText(first);
		MSHTML::IHTMLElementPtr selectionParent(range->parentElement());
		CString selectionParentText = selectionParent ? CString((const wchar_t*)selectionParent->tagName) + L":" + CString((const wchar_t*)selectionParent->className) : CString(L"-");
		const long selectionStartToFirstStart = range->compareEndPoints(L"StartToStart", firstRange);
		const long selectionEndToFirstEnd = range->compareEndPoints(L"EndToEnd", firstRange);
		const CStringA selectionTextSummary(utf16Summary(selectionText)), selectionHtmlSummary(utf16Summary(selectionHtml));
		const CStringA selectionParentSummary(utf16Summary(selectionParentText));
		range->select();
		auto countEmpty = [&](const wchar_t* tagName, const wchar_t* className = nullptr) -> long
		{
			long count = 0;
			MSHTML::IHTMLElementCollectionPtr elements(MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(tagName));
			for (long index = 0; elements && index < elements->length; ++index) {
				MSHTML::IHTMLElementPtr element(elements->item(_variant_t(index), _variant_t()));
				MSHTML::IHTMLElementCollectionPtr children(element ? element->children : MSHTML::IHTMLElementCollectionPtr());
				if (element && (!className || U::scmp(element->className, className) == 0) && (!children || children->length == 0) && CString((const wchar_t*)element->innerHTML).Trim().IsEmpty()) ++count;
			}
			return count;
		};
		const long beforeEmptyDivs = countEmpty(L"DIV"), beforeEmptyParagraphs = countEmpty(L"P"), beforeEmptyStanzas = countEmpty(L"DIV", L"stanza");
		const CString before((const wchar_t*)body->innerHTML);
		const long beforeParagraphs = paragraphs->length;
		const bool dirtyBefore = m_doc->DocChanged();
		FbeStructure::StructuralTrace trace(traceLength ? tracePath : nullptr,
			cite ? L"cite" : L"poem", traceCaseLength ? traceCase : L"runtime");
		FbeStructure::BodyStructuralEditor extracted(m_doc->m_body.Document(), m_doc->m_body.MarkupServices(), trace.IsEnabled() ? &trace : nullptr);
		const FbeStructure::CitePoemFailurePoint failurePoint = citePoemFaultLength
			? (wcscmp(citePoemFault, L"after-insert") == 0 ? FbeStructure::CitePoemFailurePoint::AfterInsert
				: wcscmp(citePoemFault, L"before-selection") == 0 ? FbeStructure::CitePoemFailurePoint::BeforeSelection
				: FbeStructure::CitePoemFailurePoint::BeforeMutation)
			: FbeStructure::CitePoemFailurePoint::None;
		auto apply = [&](bool checkOnly) -> FbeStructure::StructuralOperationResult {
			if (viaWrapper)
				return cite ? m_doc->m_body.InsertCiteResult(checkOnly, failurePoint) : m_doc->m_body.InsertPoemResult(checkOnly, failurePoint);
			return cite ? extracted.InsertCite(checkOnly, failurePoint) : extracted.InsertPoem(checkOnly, failurePoint);
		};
		const FbeStructure::StructuralOperationResult checkResult = apply(true);
		MSHTML::IHTMLTxtRangePtr selectionAfterCheck(m_doc->m_body.Document()->selection->createRange());
		const bool checkDomUnchanged = before == CString((const wchar_t*)body->innerHTML);
		const bool checkSelectionUnchanged = selectionAfterCheck &&
			range->compareEndPoints(L"StartToStart", selectionAfterCheck) == 0 &&
			range->compareEndPoints(L"EndToEnd", selectionAfterCheck) == 0;
		const bool checkDirtyUnchanged = dirtyBefore == m_doc->DocChanged();
		const FbeStructure::StructuralOperationResult applyResult = apply(false);
		const bool checkAllowed = checkResult.IsApplied();
		const bool applied = applyResult.IsApplied();
		const CString after((const wchar_t*)body->innerHTML);
		if (citePoemFaultLength) {
			MSHTML::IHTMLTxtRangePtr selectionAfterApply(m_doc->m_body.Document()->selection->createRange());
			const bool selectionUnchanged = selectionAfterApply &&
				range->compareEndPoints(L"StartToStart", selectionAfterApply) == 0 &&
				range->compareEndPoints(L"EndToEnd", selectionAfterApply) == 0;
			const bool observedChanged = before != after;
			const bool dirtyChanged = dirtyBefore != m_doc->DocChanged();
			BOOL handled = FALSE;
			if (observedChanged) m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
			const bool expectedChanged = wcscmp(citePoemFault, L"before-mutation") != 0;
			const bool restored = before == CString((const wchar_t*)body->innerHTML);
			const bool passed = checkResult.IsApplied() && checkDomUnchanged && checkSelectionUnchanged && checkDirtyUnchanged &&
				applyResult.HasTechnicalFailure() && applyResult.error == E_FAIL && observedChanged == expectedChanged &&
				applyResult.documentChanged == observedChanged && (expectedChanged ? restored : selectionUnchanged && !dirtyChanged && restored);
			CStringA row;
			row.Format("%s\t%s\t%s\t%d\t%s\t%s\t%s\t%ld\t%ld\t%d\t%d\t1\t1\t%ld\t0\t0\t0\t-\t0\t0\t0\t0\t%s\t%s\t%s\t0x%08lX\t%d\r\n", cite ? "cite" : "poem", (LPCSTR)targetName, (LPCSTR)selectionName, selectionCollapsed, (LPCSTR)selectionTextSummary, (LPCSTR)selectionHtmlSummary, (LPCSTR)selectionParentSummary, selectionStartToFirstStart, selectionEndToFirstEnd, checkAllowed, restored, beforeParagraphs, passed ? "pass" : "fail", checkResult.IsApplied() ? "applied" : checkResult.HasTechnicalFailure() ? "failed" : "not-applicable", applyResult.IsApplied() ? "applied" : applyResult.HasTechnicalFailure() ? "failed" : "not-applicable", static_cast<unsigned long>(applyResult.error), applyResult.documentChanged ? 1 : 0);
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
		}
		if (!applied || before == after) {
			MSHTML::IHTMLTxtRangePtr selectionAfterApply(m_doc->m_body.Document()->selection->createRange());
			const bool applySelectionUnchanged = selectionAfterApply &&
				range->compareEndPoints(L"StartToStart", selectionAfterApply) == 0 &&
				range->compareEndPoints(L"EndToEnd", selectionAfterApply) == 0;
			const bool applyDomUnchanged = before == after;
			const bool applyDirtyUnchanged = dirtyBefore == m_doc->DocChanged();
			const bool genuineNotApplicable = applyResult.status == FbeStructure::StructuralOperationStatus::NotApplicable &&
				checkDomUnchanged && checkSelectionUnchanged && checkDirtyUnchanged &&
				applyDomUnchanged && applySelectionUnchanged && applyDirtyUnchanged;
			const char* reason = genuineNotApplicable ? "not-applicable" : "operation-failed";
			CStringA row;
			row.Format("%s\t%s\t%s\t%d\t%s\t%s\t%s\t%ld\t%ld\t%d\t0\t0\t0\t%ld\t0\t0\t0\t-\t0\t0\t0\t0\t%s\t%s\t%s\t0x%08lX\t%d\r\n", cite ? "cite" : "poem", (LPCSTR)targetName, (LPCSTR)selectionName, selectionCollapsed, (LPCSTR)selectionTextSummary, (LPCSTR)selectionHtmlSummary, (LPCSTR)selectionParentSummary, selectionStartToFirstStart, selectionEndToFirstEnd, checkAllowed, beforeParagraphs, reason, checkResult.IsApplied() ? "applied" : checkResult.HasTechnicalFailure() ? "failed" : "not-applicable", applyResult.IsApplied() ? "applied" : applyResult.HasTechnicalFailure() ? "failed" : "not-applicable", static_cast<unsigned long>(applyResult.error), applyResult.documentChanged ? 1 : 0);
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1); return 0;
		}
		BOOL handled = FALSE;
		m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
		const CString undo((const wchar_t*)body->innerHTML);
		m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled);
		const CString redo((const wchar_t*)body->innerHTML);
		auto countClass = [&](const wchar_t* className) -> long
		{
			long count = 0;
			MSHTML::IHTMLElementCollectionPtr divs(MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"DIV"));
			for (long index = 0; divs && index < divs->length; ++index) {
				MSHTML::IHTMLElementPtr div(divs->item(_variant_t(index), _variant_t()));
				if (div && U::scmp(div->className, className) == 0) ++count;
			}
			return count;
		};
		const long citeCount = countClass(L"cite"), poemCount = countClass(L"poem"), stanzaCount = countClass(L"stanza");
		CString poemText;
		MSHTML::IHTMLElementCollectionPtr poemElements(MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"DIV"));
		for (long index = 0; poemElements && index < poemElements->length; ++index) {
			MSHTML::IHTMLElementPtr poemElement(poemElements->item(_variant_t(index), _variant_t()));
			if (poemElement && U::scmp(poemElement->className, L"poem") == 0) { poemText = (const wchar_t*)poemElement->innerText; break; }
		}
		const CStringA poemTextSummary(utf16Summary(poemText));
		const long emptyDivsAfterRedo = countEmpty(L"DIV"), emptyParagraphsAfterRedo = countEmpty(L"P"), emptyStanzasAfterRedo = countEmpty(L"DIV", L"stanza");
		m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
		const CString restored((const wchar_t*)body->innerHTML);
		const long undoEmptyDivs = countEmpty(L"DIV"), undoEmptyParagraphs = countEmpty(L"P"), undoEmptyStanzas = countEmpty(L"DIV", L"stanza");
		const long emptyDivs = (emptyDivsAfterRedo > undoEmptyDivs ? emptyDivsAfterRedo : undoEmptyDivs) - beforeEmptyDivs;
		const long emptyParagraphs = (emptyParagraphsAfterRedo > undoEmptyParagraphs ? emptyParagraphsAfterRedo : undoEmptyParagraphs) - beforeEmptyParagraphs;
		const long emptyStanzas = (emptyStanzasAfterRedo > undoEmptyStanzas ? emptyStanzasAfterRedo : undoEmptyStanzas) - beforeEmptyStanzas;
		const bool undone = before == undo && before == restored;
		const bool redone = after == redo;
		bool sequential = true;
		if (repeat) {
			range->select();
			const bool secondApplied = apply(false).IsApplied();
			const CString secondAfter((const wchar_t*)body->innerHTML);
			m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
			const CString secondUndo((const wchar_t*)body->innerHTML);
			m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled);
			const CString secondRedo((const wchar_t*)body->innerHTML);
			m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
			const CString secondRestored((const wchar_t*)body->innerHTML);
			sequential = secondApplied && before == secondUndo && secondAfter == secondRedo && before == secondRestored;
		}
		const bool structure = cite ? citeCount == 1 && poemCount == 0 : poemCount == 1 && stanzaCount >= 1;
		// The final Undo proves restoration of the original DOM.  Persist the
		// operation result after a fresh Redo: an intentionally empty fixture is
		// not itself a valid FictionBook section and must not open a validation UI.
		m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled);
		const bool redoForSave = after == CString((const wchar_t*)body->innerHTML);
		int validationLine = 0, validationColumn = 0;
		const bool saved = redoForSave && m_doc->Validate(validationLine, validationColumn) && m_doc->Save();
		const bool passed = undone && redone && sequential && structure && emptyDivs == 0 && emptyParagraphs == 0 && emptyStanzas == 0 && saved;
		CStringA row;
		row.Format("%s\t%s\t%s\t%d\t%s\t%s\t%s\t%ld\t%ld\t%d\t%d\t%d\t%d\t%ld\t%ld\t%ld\t%ld\t%s\t%ld\t%ld\t%ld\t%d\t%s\t%s\t%s\t0x%08lX\t%d\r\n", cite ? "cite" : "poem", (LPCSTR)targetName, (LPCSTR)selectionName, selectionCollapsed, (LPCSTR)selectionTextSummary, (LPCSTR)selectionHtmlSummary, (LPCSTR)selectionParentSummary, selectionStartToFirstStart, selectionEndToFirstEnd, checkAllowed, undone, redone, sequential,
			beforeParagraphs, citeCount, poemCount, stanzaCount, (LPCSTR)poemTextSummary, emptyDivs, emptyParagraphs, emptyStanzas, saved, passed ? "pass" : "fail", checkResult.IsApplied() ? "applied" : checkResult.HasTechnicalFailure() ? "failed" : "not-applicable", applyResult.IsApplied() ? "applied" : applyResult.HasTechnicalFailure() ? "failed" : "not-applicable", static_cast<unsigned long>(applyResult.error), applyResult.documentChanged ? 1 : 0);
		output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"split-undo-probe"))
	{
		wchar_t variant[48] = {};
		::GetEnvironmentVariable(L"FBE_NEXT_TEST_SPLIT_UNDO_PROBE_VARIANT", variant, _countof(variant));
		CStringA header("variant\thresult\tdom_before\tdom_after\tdom_undo_1\tdom_undo_2\tdom_redo_1\tundos_required\tresult\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		try {
			MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
			MSHTML::IHTMLElementPtr editable(document ? document->all->item(L"fbw_body") : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementCollectionPtr bodies(editable ? MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"DIV") : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementPtr root;
			for (long i = 0; bodies && i < bodies->length; ++i) { MSHTML::IHTMLElementPtr e(bodies->item(_variant_t(i), _variant_t())); if (e && U::scmp(e->className, L"body") == 0) { root = e; break; } }
			if (!root) throw _com_error(E_FAIL);
			root->innerHTML = L"<DIV id=probe-old class=section><P>AAA 123 ZZZ</P></DIV>";
			IServiceProviderPtr service(document); CComPtr<IOleUndoManager> undoManager;
			if (service) service->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&undoManager));
			if (undoManager) undoManager->DiscardFrom(NULL);
			auto snapshot = [&]() { CString value(static_cast<const wchar_t*>(root->innerHTML)); value.Replace(L"\r", L""); value.Replace(L"\n", L""); value.Replace(L"\t", L" "); return value; };
			const CString before(snapshot()); HRESULT operationHr = S_OK;
			MSHTML::IHTMLElementPtr old(document->all->item(L"probe-old"));
			auto makeNext = [&]() { MSHTML::IHTMLElementPtr next(document->createElement(L"DIV")); next->className = L"section"; next->id = L"probe-old"; next->innerHTML = L"<DIV class=title><P>123</P></DIV><P>ZZZ</P>"; return next; };
			m_doc->m_body.BeginUndoUnit(L"split undo probe");
			try {
				if (wcscmp(variant, L"insert-adjacent") == 0) { MSHTML::IHTMLElementPtr next(makeNext()); old->id = L""; old->innerHTML = L"<P>AAA</P>"; MSHTML::IHTMLElement2Ptr(old)->insertAdjacentElement(L"afterEnd", next); }
				else if (wcscmp(variant, L"insert-before") == 0) { MSHTML::IHTMLElementPtr next(makeNext()); old->id = L""; old->innerHTML = L"<P>AAA</P>"; MSHTML::IHTMLDOMNodePtr parent(old->parentElement), oldNode(old); parent->insertBefore(MSHTML::IHTMLDOMNodePtr(next), oldNode->nextSibling.GetInterfacePtr()); }
				else if (wcscmp(variant, L"append-child") == 0) { MSHTML::IHTMLElementPtr next(makeNext()); old->id = L""; old->innerHTML = L"<P>AAA</P>"; MSHTML::IHTMLDOMNodePtr(root)->appendChild(MSHTML::IHTMLDOMNodePtr(next)); }
				else if (wcscmp(variant, L"pastehtml-content") == 0) { MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange()); range->moveToElementText(old); range->pasteHTML(L"<P>AAA</P></DIV><DIV id=probe-old class=section><DIV class=title><P>123</P></DIV><P>ZZZ</P>"); }
				else if (wcscmp(variant, L"pastehtml-root") == 0) { MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange()); range->moveToElementText(root); range->pasteHTML(L"<DIV class=section><P>AAA</P></DIV><DIV id=probe-old class=section><DIV class=title><P>123</P></DIV><P>ZZZ</P></DIV>"); }
				else if (wcscmp(variant, L"markup-after-end") == 0 || wcscmp(variant, L"markup-before-begin") == 0) { MSHTML::IMarkupServices2Ptr markup(m_doc->m_body.MarkupServices()); MSHTML::IMarkupPointerPtr start, finish; markup->CreateMarkupPointer(&start); markup->CreateMarkupPointer(&finish); const MSHTML::_ELEMENT_ADJACENCY edge = wcscmp(variant, L"markup-before-begin") == 0 ? MSHTML::ELEM_ADJ_BeforeBegin : MSHTML::ELEM_ADJ_AfterEnd; start->MoveAdjacentToElement(old, edge); finish->MoveAdjacentToElement(old, edge); MSHTML::IHTMLElementPtr next(makeNext()); operationHr = markup->InsertElement(next, start, finish); if (SUCCEEDED(operationHr)) { old->id = L""; old->innerHTML = L"<P>AAA</P>"; } }
				else if (wcscmp(variant, L"markup-parse-copy") == 0) { MSHTML::IMarkupServices2Ptr markup(m_doc->m_body.MarkupServices()); MSHTML::IMarkupPointerPtr sourceStart, sourceFinish, targetStart, targetFinish; MSHTML::IMarkupContainerPtr parsed; markup->CreateMarkupPointer(&sourceStart); markup->CreateMarkupPointer(&sourceFinish); operationHr = markup->ParseString(L"<DIV class=section><P>AAA</P></DIV><DIV id=probe-old class=section><DIV class=title><P>123</P></DIV><P>ZZZ</P></DIV>", 0, &parsed, sourceStart, sourceFinish); if (SUCCEEDED(operationHr)) { markup->CreateMarkupPointer(&targetStart); markup->CreateMarkupPointer(&targetFinish); targetStart->MoveAdjacentToElement(old, MSHTML::ELEM_ADJ_BeforeBegin); targetFinish->MoveAdjacentToElement(old, MSHTML::ELEM_ADJ_AfterEnd); operationHr = markup->remove(targetStart, targetFinish); if (SUCCEEDED(operationHr)) operationHr = markup->Copy(sourceStart, sourceFinish, targetStart); } }
				else if (wcscmp(variant, L"detached-subtree") == 0) { MSHTML::IHTMLElementPtr next(makeNext()); old->id = L""; MSHTML::IHTMLDOMNodePtr parent(old->parentElement), oldNode(old); parent->insertBefore(MSHTML::IHTMLDOMNodePtr(next), oldNode->nextSibling.GetInterfacePtr()); old->innerHTML = L"<P>AAA</P>"; }
				else if (wcscmp(variant, L"whole-innerhtml") == 0) { root->innerHTML = L"<DIV class=section><P>AAA</P></DIV><DIV id=probe-old class=section><DIV class=title><P>123</P></DIV><P>ZZZ</P></DIV>"; }
				else operationHr = E_INVALIDARG;
			} catch (const _com_error& error) { operationHr = error.Error(); }
			m_doc->m_body.EndUndoUnit();
			const CString after(snapshot()); BOOL handled = FALSE; m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled); const CString undo1(snapshot()); m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled); const CString undo2(snapshot()); m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled); const CString redo1(snapshot());
			const CString expected(L"<DIV class=section><P>AAA</P></DIV><DIV id=probe-old class=section><DIV class=title><P>123</P></DIV><P>ZZZ</P></DIV>");
			const bool oneUndo = SUCCEEDED(operationHr) && undo1 == before && redo1 == after;
			const long required = undo1 == before ? 1 : undo2 == before ? 2 : 3;
			CStringA row; row.Format("%S\t0x%08lX\t%S\t%S\t%S\t%S\t%S\t%ld\t%s\r\n", variant, static_cast<unsigned long>(operationHr), static_cast<LPCWSTR>(before), static_cast<LPCWSTR>(after), static_cast<LPCWSTR>(undo1), static_cast<LPCWSTR>(undo2), static_cast<LPCWSTR>(redo1), required, oneUndo && after == expected ? "pass" : "fail");
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(0); return 0;
		} catch (const _com_error& error) { CStringA row; row.Format("%S\t0x%08lX\t\t\t\t\t\t0\tfail\r\n", variant, static_cast<unsigned long>(error.Error())); output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1); return 0; }
	}
	if (IsFbeTestScenario(L"split-ole-undo-probe"))
	{
		CStringA header("manager_hr\topen_hr\tclose_hr\tparent_units\tstate_before\tstate_after\tstate_undo\tstate_redo\tdom_before\tdom_after\tdom_undo\tdom_redo\tundo_description_before\tundo_description_after\tundo_description_undo\tundo_description_redo\tresult\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		try {
			MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document()); IServiceProviderPtr service(document); CComPtr<IOleUndoManager> manager;
			const HRESULT managerHr = service ? service->QueryService(SID_SOleUndoManager, IID_IOleUndoManager, reinterpret_cast<void**>(&manager)) : E_NOINTERFACE;
			if (FAILED(managerHr) || !manager) { CStringA row; row.Format("0x%08lX\t\t\t0\t\t\t\t\t\t\t\t\t\t\t\t\tfail\r\n", static_cast<unsigned long>(managerHr)); output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(0); return 0; }
			MSHTML::IHTMLElementPtr editable(document->all->item(L"fbw_body")), root; MSHTML::IHTMLElementCollectionPtr divs(editable ? MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"DIV") : MSHTML::IHTMLElementCollectionPtr()); for (long i = 0; divs && i < divs->length; ++i) { MSHTML::IHTMLElementPtr div(divs->item(_variant_t(i), _variant_t())); if (div && U::scmp(div->className, L"body") == 0) { root = div; break; } } if (!root) throw _com_error(E_FAIL);
			root->innerHTML = L"<DIV id=probe-old class=section><P>AAA 123 ZZZ</P></DIV>"; manager->DiscardFrom(NULL);
			auto compact = [](CString value) { value.Replace(L"\r", L""); value.Replace(L"\n", L""); value.Replace(L"\t", L" "); return value; };
			auto dom = [&]() { return compact(CString(static_cast<const wchar_t*>(root->innerHTML))); };
			auto state = [&]() { DWORD value = 0; return SUCCEEDED(manager->GetOpenParentState(&value)) ? value : 0xffffffffUL; };
			auto description = [&](bool redo) { BSTR value = NULL; HRESULT hr = redo ? manager->GetLastRedoDescription(&value) : manager->GetLastUndoDescription(&value); CString result = SUCCEEDED(hr) && value ? value : L""; if (value) ::SysFreeString(value); return compact(result); };
			const CString before(dom()), descriptionBefore(description(false)); const DWORD stateBefore = state();
			CComObject<CSplitUndoProbeParent>* rawParent = NULL; HRESULT openHr = CComObject<CSplitUndoProbeParent>::CreateInstance(&rawParent); if (SUCCEEDED(openHr)) rawParent->AddRef(); CComPtr<IOleParentUndoUnit> parent(rawParent);
			if (SUCCEEDED(openHr)) openHr = manager->Open(parent);
			if (SUCCEEDED(openHr)) { MSHTML::IHTMLElementPtr old(document->all->item(L"probe-old")), next(document->createElement(L"DIV")); next->className = L"section"; next->id = L"probe-old"; next->innerHTML = L"<DIV class=title><P>123</P></DIV><P>ZZZ</P>"; old->id = L""; old->innerHTML = L"<P>AAA</P>"; MSHTML::IHTMLDOMNodePtr parentNode(old->parentElement), oldNode(old); parentNode->insertBefore(MSHTML::IHTMLDOMNodePtr(next), oldNode->nextSibling.GetInterfacePtr()); }
			const HRESULT closeHr = SUCCEEDED(openHr) ? manager->Close(parent, TRUE) : E_FAIL; const CString after(dom()), descriptionAfter(description(false)); const DWORD stateAfter = state(); const long childCount = rawParent ? rawParent->UnitCount() : 0;
			BOOL handled = FALSE; m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled); const CString undone(dom()), descriptionUndo(description(true)); const DWORD stateUndo = state(); m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled); const CString redone(dom()), descriptionRedo(description(false)); const DWORD stateRedo = state();
			const bool passed = SUCCEEDED(managerHr) && SUCCEEDED(openHr) && SUCCEEDED(closeHr) && before == undone && after == redone;
			CStringA row; row.Format("0x%08lX\t0x%08lX\t0x%08lX\t%ld\t0x%08lX\t0x%08lX\t0x%08lX\t0x%08lX\t%S\t%S\t%S\t%S\t%S\t%S\t%S\t%S\t%s\r\n", static_cast<unsigned long>(managerHr), static_cast<unsigned long>(openHr), static_cast<unsigned long>(closeHr), childCount, stateBefore, stateAfter, stateUndo, stateRedo, static_cast<LPCWSTR>(before), static_cast<LPCWSTR>(after), static_cast<LPCWSTR>(undone), static_cast<LPCWSTR>(redone), static_cast<LPCWSTR>(descriptionBefore), static_cast<LPCWSTR>(descriptionAfter), static_cast<LPCWSTR>(descriptionUndo), static_cast<LPCWSTR>(descriptionRedo), passed ? "pass" : "fail"); output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); if (rawParent) rawParent->Release(); ::PostQuitMessage(0); return 0;
		} catch (const _com_error& error) { CStringA row; row.Format("0x%08lX\t\t\t0\t\t\t\t\t\t\t\t\t\t\t\t\tfail\r\n", static_cast<unsigned long>(error.Error())); output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1); return 0; }
	}
	if (IsFbeTestScenario(L"split-container"))
	{
		wchar_t position[16] = {}, containerClass[16] = {}, containerId[64] = {}, tracePath[MAX_PATH] = {}, traceCase[64] = {}, route[16] = {}, selectionStartMarker[64] = {}, selectionEndMarker[64] = {};
		wchar_t splitFault[32] = {};
		const DWORD positionLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SPLIT_POSITION", position, _countof(position));
		const DWORD containerClassLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SPLIT_CONTAINER_CLASS", containerClass, _countof(containerClass));
		const DWORD containerIdLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SPLIT_CONTAINER_ID", containerId, _countof(containerId));
		const DWORD traceLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_TRACE", tracePath, _countof(tracePath));
		const DWORD traceCaseLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_CASE", traceCase, _countof(traceCase));
		const DWORD splitFaultLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SPLIT_FAULT", splitFault, _countof(splitFault));
		const DWORD routeLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_STRUCTURE_ROUTE", route, _countof(route));
		const DWORD selectionStartLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SPLIT_SELECTION_START", selectionStartMarker, _countof(selectionStartMarker));
		const DWORD selectionEndLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SPLIT_SELECTION_END", selectionEndMarker, _countof(selectionEndMarker));
		const bool viaWrapper = routeLength == 7 && wcscmp(route, L"wrapper") == 0;
		CStringA header("requested_container\tactual_container\tselection_collapsed\tselection_start_relative\tselection_end_relative\tselection_parent\tcheck_allowed\tcheck_dom_unchanged\tcheck_selection_unchanged\tcheck_dirty_unchanged\tchanged\tbefore_equals_undo\tafter_equals_redo\tselection_in_new\tfragments_preserved\tsaved\tresult\tfault_error\tdocument_changed\tcheck_status\tapply_status\thresult\tselection_text\tnew_title_text\tnew_remaining_text\tcaret_inserted\tcaret_undo_redo\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		try {
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		MSHTML::IHTMLElementPtr body(document ? document->body : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementPtr editable(document ? document->all->item(L"fbw_body") : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr divs(editable ? MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"DIV") : MSHTML::IHTMLElementCollectionPtr());
		const wchar_t* requestedContainerClass = containerClassLength ? containerClass : L"section";
		MSHTML::IHTMLElementPtr container;
		for (long index = 0; divs && index < divs->length; ++index) {
			MSHTML::IHTMLElementPtr div(divs->item(_variant_t(index), _variant_t()));
			if (div && U::scmp(div->className, requestedContainerClass) == 0 &&
				(!containerIdLength || U::scmp(div->id, containerId) == 0)) { container = div; break; }
		}
		CStringA requestedContainer; requestedContainer.Format("%S#%S", requestedContainerClass, containerIdLength ? containerId : L"");
		CStringA actualContainer;
		if (container) actualContainer.Format("%S#%S", static_cast<LPCWSTR>(container->className), containerIdLength ? static_cast<LPCWSTR>(container->id) : L"");
		if (!body || !editable || !container) { CStringA row; row.Format("%s\t%s\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\tmissing-container\r\n", (LPCSTR)requestedContainer, (LPCSTR)actualContainer); output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1); return 0; }
		MSHTML::IHTMLElementPtr contentRoot(container->parentElement);
		if (!contentRoot) { CStringA row; row.Format("%s\t%s\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\tmissing-content-root\r\n", (LPCSTR)requestedContainer, (LPCSTR)actualContainer); output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1); return 0; }
		auto contentHtml = [&]() -> CString {
			MSHTML::IHTMLElementCollectionPtr currentDivs(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"DIV"));
			for (long index = 0; currentDivs && index < currentDivs->length; ++index) {
				MSHTML::IHTMLElementPtr div(currentDivs->item(_variant_t(index), _variant_t()));
				if (div && U::scmp(div->className, L"body") == 0) return CString((const wchar_t*)div->innerHTML);
			}
			return CString();
		};
		m_doc->m_body.SetFocus();
		MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		const bool atStart = positionLength == 5 && wcscmp(position, L"start") == 0;
		const bool atEnd = positionLength == 3 && wcscmp(position, L"end") == 0;
		const bool caretScenario = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SPLIT_CARET", nullptr, 0) > 0;
		if (caretScenario) {
			range->moveToElementText(container);
			if (!range->findText(L"abc", 0, 0)) { CStringA row; row.Format("%s\t%s\t0\tmissing-caret-marker\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\tmissing-caret-marker\r\n", (LPCSTR)requestedContainer, (LPCSTR)actualContainer); output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1); return 0; }
			range->collapse(VARIANT_FALSE);
		}
		else if (atStart) { range->moveToElementText(container); range->collapse(VARIANT_TRUE); }
		else {
			MSHTML::IHTMLElementCollectionPtr paragraphs(MSHTML::IHTMLElement2Ptr(container)->getElementsByTagName(L"P"));
			const long index = atEnd ? paragraphs->length - 1 : paragraphs->length / 2;
			MSHTML::IHTMLElementPtr paragraph(paragraphs && paragraphs->length ? paragraphs->item(_variant_t(index), _variant_t()) : MSHTML::IHTMLElementPtr());
			if (!paragraph) { CStringA row; row.Format("%s\t%s\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\tmissing-paragraph\r\n", (LPCSTR)requestedContainer, (LPCSTR)actualContainer); output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1); return 0; }
			const bool nonEmptySelection = positionLength >= 9 && wcsncmp(position, L"selection", 9) == 0;
			if (atEnd) { range->moveToElementText(container); range->collapse(VARIANT_FALSE); }
			else if (nonEmptySelection) {
				range->moveToElementText(container);
				const wchar_t* const startMarker = selectionStartLength && selectionStartLength < _countof(selectionStartMarker) ? selectionStartMarker : L"123";
				const wchar_t* const endMarker = selectionEndLength && selectionEndLength < _countof(selectionEndMarker) ? selectionEndMarker : startMarker;
				MSHTML::IHTMLTxtRangePtr selectionEnd(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
				selectionEnd->moveToElementText(container);
				// findText already leaves the range end immediately after the marker.
				// Moving it again would include following user text in a single-marker
				// selection (for example, "123 ZZZ" instead of "123").
				if (!range->findText(startMarker, 0, 0) || !selectionEnd->findText(endMarker, 0, 0) || FAILED(range->setEndPoint(L"EndToEnd", selectionEnd))) { CStringA row; row.Format("%s\t%s\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\tmissing-selection-marker\r\n", (LPCSTR)requestedContainer, (LPCSTR)actualContainer); output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); ::PostQuitMessage(1); return 0; }
			}
			else { range->moveToElementText(paragraph); range->collapse(VARIANT_TRUE); }
			// Stanza splitting is defined between verses.  Keep the test caret at
			// the beginning of the second visual paragraph instead of advancing
			// into its first character.
			if (!atEnd && !nonEmptySelection && wcscmp(requestedContainerClass, L"stanza") != 0 && !CString((const wchar_t*)paragraph->innerText).IsEmpty()) range->move(L"character", 1);
		}
		m_doc->m_body.SetFocus();
		range->select();
		MSHTML::IHTMLTxtRangePtr selectionBefore(document->selection->createRange());
		const CString selectionTextBefore(selectionBefore ? static_cast<const wchar_t*>(selectionBefore->text) : L"");
		MSHTML::IHTMLTxtRangePtr containerRange(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
		containerRange->moveToElementText(container);
		const bool selectionCollapsed = selectionBefore && selectionBefore->compareEndPoints(L"StartToEnd", selectionBefore) == 0;
		const long selectionStartRelative = selectionBefore ? selectionBefore->compareEndPoints(L"StartToStart", containerRange) : 99;
		const long selectionEndRelative = selectionBefore ? selectionBefore->compareEndPoints(L"EndToEnd", containerRange) : 99;
		const CString before(contentHtml());
		const bool dirtyBefore = m_doc->DocChanged();
		FbeStructure::StructuralTrace trace(traceLength ? tracePath : nullptr, L"split", traceCaseLength ? traceCase : L"runtime");
		FbeStructure::BodyStructuralEditor editor(document, m_doc->m_body.MarkupServices(), trace.IsEnabled() ? &trace : nullptr);
		const FbeStructure::StructuralOperationResult checkResult = viaWrapper ? m_doc->m_body.SplitContainerResult(true) : editor.SplitContainer(true);
		const bool checkAllowed = checkResult.IsApplied();
		MSHTML::IHTMLTxtRangePtr selectionAfterCheck(document->selection->createRange());
		const bool checkDomUnchanged = before == contentHtml();
		const bool checkSelectionUnchanged = selectionAfterCheck &&
			selectionBefore && selectionBefore->compareEndPoints(L"StartToStart", selectionAfterCheck) == 0 &&
			selectionBefore->compareEndPoints(L"EndToEnd", selectionAfterCheck) == 0;
		const bool checkDirtyUnchanged = dirtyBefore == m_doc->DocChanged();
		const FbeStructure::SplitFailurePoint failurePoint = splitFaultLength
			? (wcscmp(splitFault, L"after-first-mutation") == 0
				? FbeStructure::SplitFailurePoint::AfterFirstMutation
				: FbeStructure::SplitFailurePoint::BeforeMutation)
			: FbeStructure::SplitFailurePoint::None;
		const FbeStructure::StructuralOperationResult splitResult = viaWrapper ? m_doc->m_body.SplitContainerResult(false, failurePoint) : editor.SplitContainer(false, failurePoint);
		const bool applied = splitResult.IsApplied();
		const CString after(contentHtml());
		trace.After(L"split-content", after);
		auto statusName = [](const FbeStructure::StructuralOperationResult& result) -> const char* {
			return result.IsApplied() ? "applied" : result.HasTechnicalFailure() ? "failed" : "not-applicable";
		};
		if (splitFaultLength) {
			BOOL handled = FALSE;
			if (splitResult.documentChanged) m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
			const bool expectedChanged = wcscmp(splitFault, L"after-first-mutation") == 0;
			// A false documentChanged must not be accepted as evidence of restoration:
			// this fault scenario exists specifically to catch an incorrect flag.
			const bool undoRestored = before == contentHtml();
			const bool passed = checkAllowed && splitResult.HasTechnicalFailure() && splitResult.error == E_FAIL &&
				splitResult.documentChanged == expectedChanged && (expectedChanged ? before != after && undoRestored : before == after);
			CStringA row; row.Format("%s\t%s\t%d\t%ld\t%ld\t\t%d\t%d\t%d\t%d\t%d\t%d\t1\t1\t1\t0\t%s\t0x%08lX\t%d\t%s\t%s\t0x%08lX\r\n", (LPCSTR)requestedContainer, (LPCSTR)actualContainer, selectionCollapsed, selectionStartRelative, selectionEndRelative, checkAllowed, checkDomUnchanged, checkSelectionUnchanged, checkDirtyUnchanged, before != after, undoRestored, passed ? "pass" : "fail", static_cast<unsigned long>(splitResult.error), splitResult.documentChanged ? 1 : 0, statusName(checkResult), statusName(splitResult), static_cast<unsigned long>(splitResult.error));
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
		}
		if (checkResult.status == FbeStructure::StructuralOperationStatus::NotApplicable && splitResult.status == FbeStructure::StructuralOperationStatus::NotApplicable) {
			const bool passed = checkResult.error == S_OK && splitResult.error == S_OK && !checkResult.documentChanged && !splitResult.documentChanged && checkDomUnchanged && checkSelectionUnchanged && checkDirtyUnchanged && before == after;
			CStringA row; row.Format("%s\t%s\t%d\t%ld\t%ld\t\t%d\t%d\t%d\t%d\t0\t1\t1\t1\t1\t1\t%s\t0x%08lX\t%d\t%s\t%s\t0x%08lX\r\n", (LPCSTR)requestedContainer, (LPCSTR)actualContainer, selectionCollapsed, selectionStartRelative, selectionEndRelative, checkAllowed, checkDomUnchanged, checkSelectionUnchanged, checkDirtyUnchanged, passed ? "pass" : "fail", static_cast<unsigned long>(splitResult.error), splitResult.documentChanged ? 1 : 0, statusName(checkResult), statusName(splitResult), static_cast<unsigned long>(splitResult.error));
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
		}
		MSHTML::IHTMLTxtRangePtr selectionAfter(document->selection->createRange());
		MSHTML::IHTMLElementPtr selectionParent(selectionAfter ? selectionAfter->parentElement() : MSHTML::IHTMLElementPtr());
		while (selectionParent && U::scmp(selectionParent->tagName, L"DIV")) selectionParent = selectionParent->parentElement;
		CStringA selectionParentSummary;
		if (selectionParent) selectionParentSummary.Format("%S#%S", static_cast<LPCWSTR>(selectionParent->className), static_cast<LPCWSTR>(selectionParent->id));
		MSHTML::IHTMLElementPtr newContainer;
		if (containerIdLength) {
			MSHTML::IHTMLElementPtr adjacent(MSHTML::IHTMLDOMNodePtr(container)->nextSibling);
			if (adjacent && U::scmp(adjacent->className, container->className) == 0 && U::scmp(adjacent->id, containerId) == 0) newContainer = adjacent;
		} else {
			MSHTML::IHTMLElementCollectionPtr currentDivs(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"DIV"));
			for (long index = 0; currentDivs && index < currentDivs->length; ++index) {
				MSHTML::IHTMLElementPtr div(currentDivs->item(_variant_t(index), _variant_t()));
				if (div && U::scmp(div->className, container->className) == 0 && div->sourceIndex != container->sourceIndex) { newContainer = div; break; }
			}
		}
		MSHTML::IHTMLTxtRangePtr newContainerRange(newContainer ? MSHTML::IHTMLBodyElementPtr(body)->createTextRange() : MSHTML::IHTMLTxtRangePtr());
		if (newContainerRange) newContainerRange->moveToElementText(newContainer);
		const long selectionStartInNew = selectionAfter && newContainerRange ? selectionAfter->compareEndPoints(L"StartToStart", newContainerRange) : 99;
		const long selectionEndInNew = selectionAfter && newContainerRange ? selectionAfter->compareEndPoints(L"EndToEnd", newContainerRange) : 99;
		const bool selectionInNew = newContainer && selectionStartInNew >= 0 && selectionEndInNew <= 0;
		CString selectionMembership;
		selectionMembership.Format(L"start=%ld; end=%ld; new=%d", selectionStartInNew, selectionEndInNew, selectionInNew ? 1 : 0);
		trace.After(L"selection-membership", selectionMembership);
		BOOL handled = FALSE; CString undone, redone;
		if (!caretScenario) {
			m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled); undone = contentHtml(); trace.After(L"undo-content", undone);
			m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled); redone = contentHtml();
		}
		bool caretInserted = !caretScenario, caretUndoRedo = !caretScenario;
		if (caretScenario && selectionAfter && newContainer) {
			const CString beforeInput(static_cast<const wchar_t*>(newContainer->innerText));
			m_doc->m_body.SetFocus();
			const HWND inputFocus = ::GetFocus();
			const bool insertedWithCommand = inputFocus && ::SendMessage(inputFocus, WM_CHAR, L'X', 0) != 0;
			const CString afterInput(static_cast<const wchar_t*>(newContainer->innerText));
			trace.After(L"caret-input-after", contentHtml());
			BOOL inputHandled = FALSE;
			m_doc->m_body.OnUndo(0, 0, m_doc->m_body, inputHandled);
			const CString undoneInput(static_cast<const wchar_t*>(newContainer->innerText));
			const CString splitUndoneAfterInput(contentHtml());
			trace.After(L"caret-input-undo", splitUndoneAfterInput);
			m_doc->m_body.OnUndo(0, 0, m_doc->m_body, inputHandled);
			const CString splitUndoDom(contentHtml());
			trace.After(L"caret-split-undo", splitUndoDom);
			m_doc->m_body.OnRedo(0, 0, m_doc->m_body, inputHandled);
			const CString splitRedoDom(contentHtml());
			trace.After(L"caret-split-redo", splitRedoDom);
			m_doc->m_body.OnRedo(0, 0, m_doc->m_body, inputHandled);
			const CString redoneInput(static_cast<const wchar_t*>(newContainer->innerText));
			trace.After(L"caret-input-redo", contentHtml());
			undone = splitUndoDom;
			redone = splitRedoDom;
			const bool hasTail = beforeInput.Find(L"def") >= 0;
			// A split at end creates an intentionally empty paragraph.  Its
			// regression is that a real WM_CHAR lands in that paragraph; all
			// non-empty cases must remain the stricter Xdef/not-defX contract.
			caretInserted = (insertedWithCommand || afterInput != beforeInput) &&
				(hasTail ? (afterInput.Find(L"Xdef") >= 0 && afterInput.Find(L"defX") < 0) : afterInput.Find(L"X") >= 0);
			caretUndoRedo = beforeInput == undoneInput && splitUndoneAfterInput == after && splitUndoDom == before && splitRedoDom == after && afterInput == redoneInput;
		}
		bool endTextInserted = !atEnd;
		if (atEnd && newContainer) {
			MSHTML::IHTMLElementPtr inputContainer(document->all->item(containerId));
			MSHTML::IHTMLElementCollectionPtr leaves(inputContainer ? MSHTML::IHTMLElement2Ptr(inputContainer)->getElementsByTagName(L"P") : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementPtr leaf(leaves && leaves->length ? leaves->item(0L) : MSHTML::IHTMLElementPtr());
			if (leaf) {
				MSHTML::IHTMLTxtRangePtr input(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
				input->moveToElementText(leaf); leaf->innerText = L"Tail";
				endTextInserted = inputContainer && CString((const wchar_t*)inputContainer->innerText).Find(L"Tail") >= 0;
			}
			CString endWorkflow;
			endWorkflow.Format(L"container=%d; leaf=%d; text=%d", inputContainer ? 1 : 0, leaf ? 1 : 0, endTextInserted ? 1 : 0);
			trace.After(L"end-workflow", endWorkflow);
		}
		int validationLine = 0, validationColumn = 0;
		const bool saved = applied && caretInserted && caretUndoRedo && endTextInserted && m_doc->Validate(validationLine, validationColumn) && m_doc->Save();
		const bool changed = before != after;
		auto countText = [&](const wchar_t* text) { long count = 0, offset = 0; while ((offset = after.Find(text, offset)) >= 0) { ++count; offset += static_cast<int>(wcslen(text)); } return count; };
		const bool selectionScenario = positionLength == 9 && wcscmp(position, L"selection") == 0;
		const CString originalText(container ? static_cast<const wchar_t*>(container->innerText) : L"");
		const CString splitText(newContainer ? static_cast<const wchar_t*>(newContainer->innerText) : L"");
		MSHTML::IHTMLElementPtr newTitle;
		MSHTML::IHTMLElementCollectionPtr newDivs(newContainer ? MSHTML::IHTMLElement2Ptr(newContainer)->getElementsByTagName(L"DIV") : MSHTML::IHTMLElementCollectionPtr());
		for (long index = 0; newDivs && index < newDivs->length; ++index) { MSHTML::IHTMLElementPtr div(newDivs->item(_variant_t(index), _variant_t())); if (div && U::scmp(div->className, L"title") == 0) { newTitle = div; break; } }
		const CString newTitleText(newTitle ? static_cast<const wchar_t*>(newTitle->innerText) : L"");
		CString newRemainingText;
		MSHTML::IHTMLElementCollectionPtr newParagraphs(newContainer ? MSHTML::IHTMLElement2Ptr(newContainer)->getElementsByTagName(L"P") : MSHTML::IHTMLElementCollectionPtr());
		for (long index = 0; newParagraphs && index < newParagraphs->length; ++index) { MSHTML::IHTMLElementPtr paragraph(newParagraphs->item(_variant_t(index), _variant_t())); if (paragraph && paragraph->parentElement != newTitle) newRemainingText += static_cast<const wchar_t*>(paragraph->innerText); }
		const bool fragmentsPreserved = !selectionScenario ||
			(countText(L"AAA") == 1 && countText(L"123") == 1 && countText(L"ZZZ") == 1 &&
			 selectionTextBefore == L"123" && originalText.Find(L"AAA") >= 0 && originalText.Find(L"123") < 0 && originalText.Find(L"ZZZ") < 0 &&
			 newTitleText == L"123" && newRemainingText.Find(L"ZZZ") >= 0 && splitText.Find(L"AAA") < 0);
		const bool passed = checkAllowed && checkDomUnchanged && checkSelectionUnchanged && checkDirtyUnchanged && applied && changed && before == undone && after == redone && selectionInNew && fragmentsPreserved && caretInserted && caretUndoRedo && endTextInserted && saved;
		auto tsvField = [](CString value) { value.Replace(L"\r", L"\\r"); value.Replace(L"\n", L"\\n"); value.Replace(L"\t", L"\\t"); return CStringA(CW2A(value, CP_UTF8)); };
		const CStringA selectionTextField(tsvField(selectionTextBefore)), newTitleField(tsvField(newTitleText)), newRemainingField(tsvField(newRemainingText));
		CStringA row; row.Format("%s\t%s\t%d\t%ld\t%ld\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\t0x%08lX\t%d\t%s\t%s\t0x%08lX\t%s\t%s\t%s\t%d\t%d\r\n", (LPCSTR)requestedContainer, (LPCSTR)actualContainer, selectionCollapsed, selectionStartRelative, selectionEndRelative, (LPCSTR)selectionParentSummary, checkAllowed, checkDomUnchanged, checkSelectionUnchanged, checkDirtyUnchanged, changed, before == undone, after == redone, selectionInNew, fragmentsPreserved, saved, passed ? "pass" : "fail", static_cast<unsigned long>(splitResult.error), splitResult.documentChanged ? 1 : 0, statusName(checkResult), statusName(splitResult), static_cast<unsigned long>(splitResult.error), (LPCSTR)selectionTextField, (LPCSTR)newTitleField, (LPCSTR)newRemainingField, caretInserted ? 1 : 0, caretUndoRedo ? 1 : 0);
		output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(passed ? 0 : 1); return 0;
		} catch (_com_error& error) {
			CStringA row; row.Format("unknown\tunknown\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\tcom-0x%08lX\r\n", static_cast<unsigned long>(error.Error()));
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(1); return 0;
		}
	}
	if (IsFbeTestScenario(L"split-container-reopen"))
	{
		wchar_t containerId[64] = {};
		const DWORD containerIdLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_SPLIT_CONTAINER_ID", containerId, _countof(containerId));
		CStringA header("id_restored\tsection_complete\tsaved\tresult\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		try {
			MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
			MSHTML::IHTMLElementPtr section(document && containerIdLength ? document->all->item(containerId) : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementCollectionPtr paragraphs(section ? MSHTML::IHTMLElement2Ptr(section)->getElementsByTagName(L"P") : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementPtr firstParagraph(paragraphs && paragraphs->length ? paragraphs->item(0L) : MSHTML::IHTMLElementPtr());
			const bool idRestored = section && U::scmp(section->id, containerId) == 0;
			const bool sectionComplete = firstParagraph && !CString(static_cast<const wchar_t*>(firstParagraph->innerText)).IsEmpty();
			const bool saved = idRestored && sectionComplete && m_doc->Save();
			CStringA row; row.Format("%d\t%d\t%d\t%s\r\n", idRestored ? 1 : 0, sectionComplete ? 1 : 0, saved ? 1 : 0, saved ? "pass" : "fail");
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(saved ? 0 : 1); return 0;
		} catch (_com_error& error) {
			CStringA row; row.Format("0\t0\t0\tcom-0x%08lX\r\n", static_cast<unsigned long>(error.Error()));
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close(); ::PostQuitMessage(1); return 0;
		}
	}
	if (IsFbeTestScenario(L"visual-dom-normalizer"))
	{
		CStringA header("case\tparagraphs\tempty_divs\tbrs\texact_paragraphs\tempty_line\tnbsp\tformatting\tresult\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		MSHTML::IHTMLElementPtr body(document ? document->body : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementPtr editable(document ? document->all->item(L"fbw_body") : MSHTML::IHTMLElementPtr());
		if (!body || !editable)
		{
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		const CString originalHtml((const wchar_t*)editable->innerHTML);
		const CString pastePayload(L"paste-alpha\x00a0bold\r\npaste-beta\r\n\r\npaste-gamma");
		bool pasteNormalized = false;
		CComPtr<IDataObject> originalClipboard;
		if (SUCCEEDED(::OleGetClipboard(&originalClipboard)) && ::OpenClipboard(m_hWnd)) {
			::EmptyClipboard();
			const SIZE_T pasteBytes = static_cast<SIZE_T>(pastePayload.GetLength() + 1) * sizeof(wchar_t);
			HGLOBAL pasteMemory = ::GlobalAlloc(GMEM_MOVEABLE, pasteBytes);
			wchar_t* pasteText = pasteMemory ? static_cast<wchar_t*>(::GlobalLock(pasteMemory)) : nullptr;
			if (pasteText) { wcscpy_s(pasteText, pastePayload.GetLength() + 1, pastePayload); ::GlobalUnlock(pasteMemory); }
			const bool clipboardReady = pasteMemory && pasteText && ::SetClipboardData(CF_UNICODETEXT, pasteMemory);
			if (!clipboardReady && pasteMemory) ::GlobalFree(pasteMemory);
			::CloseClipboard();
			if (clipboardReady) {
				MSHTML::IHTMLElementCollectionPtr initialParagraphs(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"P"));
				MSHTML::IHTMLElementPtr initialParagraph(initialParagraphs && initialParagraphs->length ? initialParagraphs->item(0L) : MSHTML::IHTMLElementPtr());
				if (initialParagraph) { MSHTML::IHTMLTxtRangePtr pasteRange(MSHTML::IHTMLBodyElementPtr(body)->createTextRange()); pasteRange->moveToElementText(initialParagraph); pasteRange->collapse(VARIANT_FALSE); pasteRange->select(); BOOL pasteHandled = FALSE; m_doc->m_body.OnPaste(0, ID_EDIT_PASTE, 0, pasteHandled); }
				editable = document->all->item(L"fbw_body");
				const CString pastedHtml(editable ? static_cast<LPCWSTR>(editable->innerHTML) : L"");
				const CString pastedText(editable ? static_cast<LPCWSTR>(editable->innerText) : L"");
				const int alpha = pastedText.Find(L"paste-alpha"), beta = pastedText.Find(L"paste-beta"), gamma = pastedText.Find(L"paste-gamma");
				pasteNormalized = alpha >= 0 && beta > alpha && gamma > beta && pastedHtml.Find(L"<BR") < 0 && (pastedHtml.Find(L"&nbsp;") >= 0 || pastedHtml.Find(L"\x00a0") >= 0);
			}
			::OleSetClipboard(originalClipboard);
		}
		CStringA pasteRow; pasteRow.Format("paste-normal\t0\t0\t0\t%d\t%d\t%d\t%d\t%s\r\n", pasteNormalized ? 1 : 0, pasteNormalized ? 1 : 0, pasteNormalized ? 1 : 0, 1, pasteNormalized ? "pass" : "fail");
		output.Write(pasteRow, static_cast<DWORD>(pasteRow.GetLength()), &written);
		editable = document->all->item(L"fbw_body");
		if (!editable) { output.Close(); ::PostQuitMessage(1); return 0; }
		editable->innerHTML = originalHtml.AllocSysString();
		struct NormalizerCase { const wchar_t* name; const wchar_t* html; const wchar_t* text[3]; long paragraphs; bool nbsp; bool formatting; };
		const NormalizerCase cases[] = {
			{ L"single-br", L"<DIV class='section'><P>alpha<BR>beta</P></DIV>", { L"alpha", L"beta", L"" }, 2, false, false },
			{ L"double-br", L"<DIV class='section'><P>alpha<BR><BR>beta</P></DIV>", { L"alpha", L"", L"beta" }, 3, false, false },
			{ L"empty-p", L"<DIV class='section'><P>alpha</P><P></P><P>beta</P></DIV>", { L"alpha", L"", L"beta" }, 3, false, false },
			{ L"nbsp-p", L"<DIV class='section'><P>alpha</P><P>&nbsp;</P><P>beta</P></DIV>", { L"alpha", L"\x00a0", L"beta" }, 3, true, false },
			{ L"formatted-br", L"<DIV class='section'><P><STRONG>alpha</STRONG><BR><EM>beta</EM></P></DIV>", { L"alpha", L"beta", L"" }, 2, false, true }
		};
		bool allPassed = true;
		for (const NormalizerCase& testCase : cases)
		{
			editable->innerHTML = testCase.html;
			m_doc->m_body.Normalize(MSHTML::IHTMLDOMNodePtr(body));
			// SplitBRs replaces outerHTML, invalidating the original element proxy.
			editable = document->all->item(L"fbw_body");
			if (!editable) { output.Close(); ::PostQuitMessage(1); return 0; }
			auto countElements = [&](const wchar_t* tagName) -> long {
				MSHTML::IHTMLElementCollectionPtr elements(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(tagName));
				return elements ? elements->length : 0;
			};
			long emptyDivs = 0;
			MSHTML::IHTMLElementCollectionPtr divs(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"DIV"));
			for (long index = 0; divs && index < divs->length; ++index) {
				MSHTML::IHTMLElementPtr div(divs->item(_variant_t(index), _variant_t()));
				if (div && CString((const wchar_t*)div->innerHTML).Trim().IsEmpty()) ++emptyDivs;
			}
			MSHTML::IHTMLElementCollectionPtr paragraphs(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"P"));
			bool exactParagraphs = paragraphs && paragraphs->length == testCase.paragraphs;
			for (long index = 0; exactParagraphs && index < testCase.paragraphs; ++index) {
				MSHTML::IHTMLElementPtr paragraph(paragraphs->item(_variant_t(index), _variant_t()));
				exactParagraphs = paragraph && (testCase.nbsp && index == 1 || CString(static_cast<LPCWSTR>(paragraph->innerText)) == testCase.text[index]);
			}
			const bool emptyLine = paragraphs && paragraphs->length >= 3 && CString(static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(paragraphs->item(_variant_t(1L), _variant_t()))->innerText)).IsEmpty();
			CString middleHtml;
			if (paragraphs && paragraphs->length >= 3) middleHtml = static_cast<LPCWSTR>(MSHTML::IHTMLElementPtr(paragraphs->item(_variant_t(1L), _variant_t()))->innerHTML);
			middleHtml.MakeLower();
			const bool nbsp = !testCase.nbsp || middleHtml.Find(L"&nbsp;") >= 0 || middleHtml.Find(L"\x00a0") >= 0;
			const bool formatting = !testCase.formatting || (countElements(L"STRONG") == 1 && countElements(L"EM") == 1);
			const bool passed = countElements(L"BR") == 0 && emptyDivs == 0 && exactParagraphs &&
				(wcscmp(testCase.name, L"double-br") || emptyLine) && nbsp && formatting;
			allPassed = allPassed && passed;
			CStringA row;
			row.Format("%S\t%ld\t%ld\t%ld\t%d\t%d\t%d\t%d\t%s\r\n", testCase.name, countElements(L"P"), emptyDivs, countElements(L"BR"), exactParagraphs, emptyLine, nbsp, formatting, passed ? "pass" : "fail");
			output.Write(row, static_cast<DWORD>(row.GetLength()), &written);
		}
		// Restore the fixture before exercising the ordinary production save path.
		editable->innerHTML = originalHtml.AllocSysString();
		int validationLine = 0, validationColumn = 0;
		const bool saved = m_doc->Validate(validationLine, validationColumn) && m_doc->Save();
		output.Flush(); output.Close();
		::PostQuitMessage(allPassed && saved ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"link-navigation-runtime"))
	{
		CStringA header("nested\ttarget\tsame_document\tbroken\treturned_second\tinserted_navigate\tinserted_same_unique\tinserted_returned\tinserted_returned_origin\tinserted_before\tdeleted_origin_fallback\tother_document_opened\tunchanged\tsecond_dom_unchanged\tsecond_dirty_unchanged\torigin_dom_unchanged\torigin_dirty_unchanged\tresult\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		MSHTML::IHTMLElementPtr editable(FBELinkNavigation::GetEditableBody(document));
		MSHTML::IHTMLElementCollectionPtr links(editable ? MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"A") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr internal(links && links->length > 0 ? links->item(0L) : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementPtr second(links && links->length > 1 ? links->item(1L) : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementPtr broken(links && links->length > 2 ? links->item(2L) : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr strongs(internal ? MSHTML::IHTMLElement2Ptr(internal)->getElementsByTagName(L"STRONG") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr nested(strongs && strongs->length ? strongs->item(0L) : MSHTML::IHTMLElementPtr());
		const CString before(editable ? static_cast<LPCWSTR>(editable->innerHTML) : L"");
		MSHTML::IHTMLElementPtr nearest(FBELinkNavigation::FindNearestLinkElement(nested, editable));
		const CString targetId(FBELinkNavigation::GetInternalLinkTargetId(document, nearest));
		MSHTML::IHTMLElementPtr target(FBELinkNavigation::FindTargetElement(document, targetId));
		CString documentUrl;
		try { MSHTML::IHTMLDocument4Ptr document4(document); if(document4) documentUrl = static_cast<LPCWSTR>(document4->URLUnencoded); }
		catch(const _com_error&) { }
		const CString sameDocumentHref = documentUrl + L"#note-1";
		const bool sameDocument = FBELinkNavigation::GetInternalTargetId(static_cast<LPCWSTR>(sameDocumentHref), static_cast<LPCWSTR>(documentUrl)) == L"note-1";
		auto navigationLeavesDocumentUntouched = [&](const std::function<bool()>& operation, bool& domUnchanged, bool& dirtyUnchanged) -> bool {
			const CString snapshot(editable ? static_cast<LPCWSTR>(editable->innerHTML) : L"");
			const bool dirty = m_doc->DocChanged();
			const bool result = operation();
			domUnchanged = editable && snapshot == CString(static_cast<LPCWSTR>(editable->innerHTML));
			dirtyUnchanged = dirty == m_doc->DocChanged();
			return result && domUnchanged && dirtyUnchanged;
		};
		bool secondDomUnchanged = false, secondDirtyUnchanged = false, originDomUnchanged = false, originDirtyUnchanged = false;
		const CString secondTargetId(FBELinkNavigation::GetInternalLinkTargetId(document, second));
		const long secondUniqueNumber = FBELinkNavigation::GetLinkUniqueNumber(second);
		const bool secondNavigated = second && secondTargetId == targetId && navigationLeavesDocumentUntouched([&]() { return m_doc->m_body.NavigateInternalLink(second, secondTargetId); }, secondDomUnchanged, secondDirtyUnchanged);
		OnGoToFootnote(0, ID_GOTO_FOOTNOTE, nullptr);
		MSHTML::IHTMLTxtRangePtr returnedRange(document->selection->createRange());
		MSHTML::IHTMLElementPtr returnedLink(returnedRange ? FBELinkNavigation::FindNearestLinkElement(returnedRange->parentElement(), editable) : MSHTML::IHTMLElementPtr());
		const bool returnedSecond = secondNavigated && returnedLink == second;
		const CString brokenTargetId(FBELinkNavigation::GetInternalLinkTargetId(document, broken));
		const bool brokenInternal = !brokenTargetId.IsEmpty() && !FBELinkNavigation::FindTargetElement(document, brokenTargetId);
		MSHTML::IHTMLElementPtr inserted;
		MSHTML::IHTMLDOMNodePtr secondNode(second);
		MSHTML::IHTMLDOMNodePtr secondParent(secondNode ? secondNode->parentNode : MSHTML::IHTMLDOMNodePtr());
		bool insertedBefore = false;
		bool insertedNavigate = false, insertedSameUnique = false, insertedReturned = false, insertedReturnedOrigin = false;
		try {
			inserted = document->createElement(L"A");
			if (inserted && second && secondParent) {
				const bool originSaved = navigationLeavesDocumentUntouched([&]() { return m_doc->m_body.NavigateInternalLink(second, secondTargetId); }, originDomUnchanged, originDirtyUnchanged);
				inserted->setAttribute(L"href", _variant_t(L"#note-1"), 2);
				inserted->innerText = L"inserted source";
				secondParent->insertBefore(MSHTML::IHTMLDOMNodePtr(inserted), secondNode.GetInterfacePtr());
				MSHTML::IHTMLElementCollectionPtr currentLinks(MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"A"));
				MSHTML::IHTMLElementPtr currentSecond;
				for (long index = 0; currentLinks && index < currentLinks->length; ++index) {
					MSHTML::IHTMLElementPtr candidate(currentLinks->item(index));
					if (candidate && CString(static_cast<LPCWSTR>(candidate->innerText)) == L"second source") { currentSecond = candidate; break; }
				}
				const long currentSecondUniqueNumber = FBELinkNavigation::GetLinkUniqueNumber(currentSecond);
				// The saved history belongs to the pre-insertion second link.  Do not
				// navigate again here: that would replace the origin under test.
				insertedNavigate = originSaved;
				insertedSameUnique = currentSecondUniqueNumber == secondUniqueNumber;
				insertedReturned = m_doc->m_body.ReturnToLinkNavigationOrigin();
				MSHTML::IHTMLTxtRangePtr returnedOriginRange(document->selection->createRange());
				MSHTML::IHTMLTxtRangePtr currentSecondRange(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
				if (currentSecondRange && currentSecond) currentSecondRange->moveToElementText(currentSecond);
				insertedReturnedOrigin = insertedReturned && currentSecondRange &&
					returnedOriginRange->compareEndPoints(L"StartToStart", currentSecondRange) >= 0 &&
					returnedOriginRange->compareEndPoints(L"EndToEnd", currentSecondRange) <= 0;
				insertedBefore = insertedNavigate && insertedSameUnique && insertedReturnedOrigin;
				second = currentSecond;
			}
		} catch (const _com_error&) { insertedBefore = false; }
		bool deletedOriginFallback = false;
		try {
			MSHTML::IHTMLDOMNodePtr currentSecondNode(second);
			if (currentSecondNode && m_doc->m_body.NavigateInternalLink(second, secondTargetId)) {
				currentSecondNode->removeNode(VARIANT_TRUE);
				deletedOriginFallback = !m_doc->m_body.ReturnToLinkNavigationOrigin();
			}
		} catch (const _com_error&) { deletedOriginFallback = false; }
		wchar_t replacementPath[MAX_PATH] = {};
		const DWORD replacementPathLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_NAVIGATION_SECOND_FILE", replacementPath, _countof(replacementPath));
		bool otherDocumentOpened = false;
		if (replacementPathLength && replacementPathLength < _countof(replacementPath) && internal && m_doc->m_body.NavigateInternalLink(internal, targetId)) {
			// The preceding insertion/removal is intentional history coverage.  It
			// must not prompt during this separate ordinary-open regression step.
			m_doc->MarkSavePoint();
			otherDocumentOpened = LoadFile(replacementPath) == OK && !m_doc->m_body.ReturnToLinkNavigationOrigin();
		}
		// Both ordinary transitions above were independently compared with the
		// live DOM and dirty state.  Later insertion/removal intentionally edits
		// the fixture and is covered by its own history assertions.
		const bool unchanged = secondDomUnchanged && secondDirtyUnchanged && originDomUnchanged && originDirtyUnchanged;
		const bool passed = nearest == internal && target && sameDocument && brokenInternal && returnedSecond && insertedBefore && deletedOriginFallback && otherDocumentOpened && unchanged;
		CStringA row;
		row.Format("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\r\n", nearest == internal, target ? 1 : 0, sameDocument, brokenInternal, returnedSecond, insertedNavigate, insertedSameUnique, insertedReturned, insertedReturnedOrigin, insertedBefore, deletedOriginFallback, otherDocumentOpened, unchanged, secondDomUnchanged, secondDirtyUnchanged, originDomUnchanged, originDirtyUnchanged, passed ? "pass" : "fail");
		output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(passed ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"reference-navigation-runtime"))
	{
		CStringA header("footnote_check\tfootnote_target\treference_check\treference_target\tcheck_unchanged\tdom_unchanged\tresult\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		MSHTML::IHTMLDocument2Ptr document(m_doc->m_body.Document());
		MSHTML::IHTMLElementPtr editable(FBELinkNavigation::GetEditableBody(document));
		MSHTML::IHTMLElementCollectionPtr links(editable ? MSHTML::IHTMLElement2Ptr(editable)->getElementsByTagName(L"A") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr link(links && links->length ? links->item(0L) : MSHTML::IHTMLElementPtr());
		MSHTML::IHTMLElementCollectionPtr strongs(link ? MSHTML::IHTMLElement2Ptr(link)->getElementsByTagName(L"STRONG") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr nested(strongs && strongs->length ? strongs->item(0L) : MSHTML::IHTMLElementPtr());
		const CString originalHtml(editable ? static_cast<LPCWSTR>(editable->innerHTML) : L"");
		auto selectElement = [&](MSHTML::IHTMLElementPtr element) -> bool
		{
			if (!element || !document || !document->body) return false;
			MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange());
			range->moveToElementText(element); range->collapse(VARIANT_TRUE); range->select(); return true;
		};
		auto sameSelection = [&]() -> bool
		{
			MSHTML::IHTMLTxtRangePtr first(document && document->selection ? MSHTML::IHTMLTxtRangePtr(document->selection->createRange()) : MSHTML::IHTMLTxtRangePtr());
			if (!first) return false;
			const bool canFootnote = m_doc->m_body.GoToFootnote(true);
			MSHTML::IHTMLTxtRangePtr after(document && document->selection ? MSHTML::IHTMLTxtRangePtr(document->selection->createRange()) : MSHTML::IHTMLTxtRangePtr());
			const bool unchangedFootnote = after && first->compareEndPoints(L"StartToStart", after) == 0 &&
				first->compareEndPoints(L"EndToEnd", after) == 0;
			return canFootnote && unchangedFootnote;
		};
		const bool selectedLink = selectElement(nested ? nested : link);
		const bool footnoteCheck = selectedLink && sameSelection();
		const bool footnoteMoved = footnoteCheck && m_doc->m_body.GoToFootnote(false);
		MSHTML::IHTMLElementPtr noteParent;
		try { noteParent = MSHTML::IHTMLTxtRangePtr(document->selection->createRange())->parentElement(); } catch (const _com_error&) { }
		while (noteParent && CString(static_cast<LPCWSTR>(noteParent->id)) != L"note-1") noteParent = noteParent->parentElement;
		const bool footnoteTarget = footnoteMoved && noteParent;
		MSHTML::IHTMLTxtRangePtr beforeReference(document && document->selection ? MSHTML::IHTMLTxtRangePtr(document->selection->createRange()) : MSHTML::IHTMLTxtRangePtr());
		const bool referenceCheck = beforeReference && m_doc->m_body.GoToReference(true);
		MSHTML::IHTMLTxtRangePtr afterReference(document && document->selection ? MSHTML::IHTMLTxtRangePtr(document->selection->createRange()) : MSHTML::IHTMLTxtRangePtr());
		const bool referenceCheckUnchanged = beforeReference && afterReference &&
			beforeReference->compareEndPoints(L"StartToStart", afterReference) == 0 &&
			beforeReference->compareEndPoints(L"EndToEnd", afterReference) == 0;
		m_doc->m_body.GoToReference(false);
		MSHTML::IHTMLTxtRangePtr selectedAfterReference(document && document->selection ? MSHTML::IHTMLTxtRangePtr(document->selection->createRange()) : MSHTML::IHTMLTxtRangePtr());
		MSHTML::IHTMLTxtRangePtr expectedReference(link && document && document->body ? MSHTML::IHTMLBodyElementPtr(document->body)->createTextRange() : MSHTML::IHTMLTxtRangePtr());
		if (expectedReference)
		{
			expectedReference->moveToElementText(link); expectedReference->collapse(VARIANT_TRUE);
			expectedReference->move(L"character", CString(static_cast<LPCWSTR>(link->innerText)).GetLength());
		}
		const bool referenceTarget = selectedAfterReference && expectedReference &&
			selectedAfterReference->compareEndPoints(L"StartToStart", expectedReference) == 0 &&
			selectedAfterReference->compareEndPoints(L"EndToEnd", expectedReference) == 0;
		const bool unchanged = editable && originalHtml == CString(static_cast<LPCWSTR>(editable->innerHTML));
		const bool passed = footnoteCheck && footnoteTarget && referenceCheck && referenceCheckUnchanged && referenceTarget && unchanged;
		CStringA row;
		row.Format("%d\t%d\t%d\t%d\t%d\t%d\t%s\r\n", footnoteCheck, footnoteTarget, referenceCheck,
			referenceTarget, referenceCheckUnchanged, unchanged, passed ? "pass" : "fail");
		output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush(); output.Close();
		::PostQuitMessage(passed ? 0 : 1); return 0;
	}
	if (IsFbeTestScenario(L"table-structural"))
	{
		const ULONGLONG start = ::GetTickCount64();
		auto appendStructuralPhase = [&](const char* phase, long gridBuildCalls = -1)
		{
			MSHTML::IHTMLElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementCollectionPtr tables(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TABLE") : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementCollectionPtr rows(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TR") : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementCollectionPtr td(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TD") : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementCollectionPtr th(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TH") : MSHTML::IHTMLElementCollectionPtr());
			CStringA row;
			row.Format("%s\t%I64u\t%ld\t%ld\t%ld\t%ld\t%ld\t%s\r\n", phase, ::GetTickCount64() - start,
				tables ? tables->length : 0, rows ? rows->length : 0, td ? td->length : 0, th ? th->length : 0, gridBuildCalls, (LPCSTR)m_doc->m_body.TableStructuralSnapshot());
			DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
		};
		auto selectFirstCell = [&]() -> bool
		{
			MSHTML::IHTMLElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementCollectionPtr cells(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TD") : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementPtr cell(cells && cells->length ? cells->item(_variant_t(0L), _variant_t()) : MSHTML::IHTMLElementPtr());
			if (!cell) { cells = body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"TH") : MSHTML::IHTMLElementCollectionPtr(); cell = cells && cells->length ? cells->item(_variant_t(0L), _variant_t()) : MSHTML::IHTMLElementPtr(); }
			if (!cell) return false;
			MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
			range->moveToElementText(cell); range->collapse(VARIANT_TRUE); range->select(); return true;
		};
		auto selectFirstTwoCells = [&](bool headers) -> bool
		{
			MSHTML::IHTMLElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementCollectionPtr cells(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(headers ? L"TH" : L"TD") : MSHTML::IHTMLElementCollectionPtr());
			if (!cells || cells->length < 2) cells = body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(headers ? L"TD" : L"TH") : MSHTML::IHTMLElementCollectionPtr();
			if (!cells || cells->length < 2) return false;
			MSHTML::IHTMLElementPtr first(cells->item(_variant_t(0L), _variant_t())), last(cells->item(_variant_t(1L), _variant_t()));
			MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange()), end(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
			 range->moveToElementText(first); end->moveToElementText(last); range->setEndPoint(L"EndToEnd", end); range->select(); return true;
		};
		auto selectConfiguredCells = [&](bool bulk, bool headers) -> bool
		{
			wchar_t target[64] = {};
			const DWORD length = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TABLE_TARGET", target, _countof(target));
			if (!length || length >= _countof(target)) return bulk ? selectFirstTwoCells(headers) : selectFirstCell();
			long firstRow = -1, firstColumn = -1, lastRow = -1, lastColumn = -1;
			if (swscanf_s(target, L"%ld,%ld:%ld,%ld", &firstRow, &firstColumn, &lastRow, &lastColumn) != 4) {
				if (swscanf_s(target, L"%ld,%ld", &firstRow, &firstColumn) != 2) return false;
				lastRow = firstRow; lastColumn = firstColumn;
			}
			return m_doc->m_body.SelectTableLogicalRangeForTest(firstRow, firstColumn, lastRow, lastColumn);
		};
		auto applyConfiguredRuntimeCellStyle = [&]() -> bool
		{
			wchar_t cssText[256] = {};
			const DWORD length = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TABLE_RUNTIME_STYLE", cssText, _countof(cssText));
			if (!length) return true;
			if (length >= _countof(cssText)) return false;
			MSHTML::IHTMLElementPtr cell(m_doc->m_body.SelectionStructTableCon());
			MSHTML::IHTMLStylePtr style(cell ? cell->style : MSHTML::IHTMLStylePtr());
			if (!style) return false;
			style->cssText = cssText;
			return true;
		};
		typedef LRESULT (CFBEView::*TableHandler)(WORD, WORD, HWND, BOOL&);
		struct Operation { const char* name; UINT command; TableHandler handler; bool bulk, selectHeaders; };
		const Operation operations[] = {
			{ "toggle-header", ID_TABLE_TOGGLE_HEADER_CELL, &CFBEView::OnTableToggleHeaderCell, false, false }, { "insert-row-above", ID_TABLE_INSERT_ROW_ABOVE, &CFBEView::OnTableInsertRowAbove, false, false },
			{ "insert-row-below", ID_TABLE_INSERT_ROW_BELOW, &CFBEView::OnTableInsertRowBelow, false, false }, { "delete-row", ID_TABLE_DELETE_ROW, &CFBEView::OnTableDeleteRow, false, false },
			{ "insert-column-left", ID_TABLE_INSERT_COLUMN_LEFT, &CFBEView::OnTableInsertColumnLeft, false, false }, { "insert-column-right", ID_TABLE_INSERT_COLUMN_RIGHT, &CFBEView::OnTableInsertColumnRight, false, false },
			{ "delete-column", ID_TABLE_DELETE_COLUMN, &CFBEView::OnTableDeleteColumn, false, false }, { "make-header", ID_TABLE_MAKE_HEADER_CELLS, &CFBEView::OnTableMakeHeaderCells, true, false },
			{ "make-normal", ID_TABLE_MAKE_NORMAL_CELLS, &CFBEView::OnTableMakeNormalCells, true, true }
		};
		wchar_t routeThroughFrame[4] = {};
		const bool useCommandRoute = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TABLE_ROUTE", routeThroughFrame, _countof(routeThroughFrame)) == 1 && routeThroughFrame[0] == L'1';
		auto invokeOperation = [&](const Operation& operation, BOOL& handled) -> bool
		{
			wchar_t target[64] = {};
			const DWORD targetLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TABLE_TARGET", target, _countof(target));
			long row = -1, column = -1;
			if (targetLength && targetLength < _countof(target) && strcmp(operation.name, "delete-column") == 0 &&
				swscanf_s(target, L"%ld,%ld", &row, &column) == 2 && column >= 0) {
				return m_doc->m_body.DeleteTableLogicalColumnForTest(column);
			}
			if(useCommandRoute)
			{
				m_doc->m_body.SetFocus();
				::SendMessage(m_hWnd, WM_COMMAND, MAKEWPARAM(operation.command, 0), 0);
				return true;
			}
			(m_doc->m_body.*operation.handler)(0, 0, m_doc->m_body, handled);
			return true;
		};
		CStringA header("phase\telapsed_ms\ttable_count\ttr_count\ttd_count\tth_count\tgrid_build_calls\tgrid_signature\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written); output.Flush();
		for (size_t index = 0; index < _countof(operations); ++index)
		{
			wchar_t requestedOperation[64] = {};
			const DWORD requestedOperationLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TABLE_OPERATION", requestedOperation, _countof(requestedOperation));
			if (requestedOperationLength && (requestedOperationLength >= _countof(requestedOperation) || _stricmp((LPCSTR)CStringA(requestedOperation), operations[index].name) != 0)) continue;
			if (!selectConfiguredCells(operations[index].bulk, operations[index].selectHeaders) || !applyConfiguredRuntimeCellStyle()) { output.Close(); ::PostQuitMessage(1); return 0; }
			CStringA phase; phase.Format("%s-before", operations[index].name); appendStructuralPhase(phase);
			CFBEView::ResetTableGridBuildCountForTest();
			BOOL handled = FALSE; if (!invokeOperation(operations[index], handled)) { output.Close(); ::PostQuitMessage(1); return 0; }
			const long gridBuildCalls = CFBEView::TableGridBuildCountForTest();
			phase.Format("%s-after", operations[index].name); appendStructuralPhase(phase, gridBuildCalls);
			wchar_t secondOperation[64] = {};
			const DWORD secondOperationLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_TABLE_SECOND_OPERATION", secondOperation, _countof(secondOperation));
			if (secondOperationLength && secondOperationLength < _countof(secondOperation)) {
				for (size_t secondIndex = 0; secondIndex < _countof(operations); ++secondIndex) {
					if (_stricmp((LPCSTR)CStringA(secondOperation), operations[secondIndex].name) != 0) continue;
					if (!selectConfiguredCells(operations[secondIndex].bulk, operations[secondIndex].selectHeaders)) { output.Close(); ::PostQuitMessage(1); return 0; }
					phase.Format("%s-second-before", operations[secondIndex].name); appendStructuralPhase(phase);
					CFBEView::ResetTableGridBuildCountForTest();
					if (!invokeOperation(operations[secondIndex], handled)) { output.Close(); ::PostQuitMessage(1); return 0; }
					phase.Format("%s-second-after", operations[secondIndex].name); appendStructuralPhase(phase, CFBEView::TableGridBuildCountForTest());
					break;
				}
			}
			m_doc->m_body.OnUndo(0, 0, m_doc->m_body, handled);
			phase.Format("%s-undo", operations[index].name); appendStructuralPhase(phase);
			m_doc->m_body.OnRedo(0, 0, m_doc->m_body, handled);
			phase.Format("%s-redo", operations[index].name); appendStructuralPhase(phase);
		}
		if (!m_doc->Save()) { appendStructuralPhase("save-failed;phase=save;operation=Save;actual_hresult=unavailable;symbolic_hresult=unavailable"); output.Close(); ::PostQuitMessage(1); return 0; }
		appendStructuralPhase("save-complete"); output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"binary-import-image"))
	{
		static bool imageImportRunnerQueued = false;
		if (!imageImportRunnerQueued)
		{
			imageImportRunnerQueued = true;
			output.Close();
			SetTimer(IMAGE_IMPORT_TEST_TIMER_ID, 250);
			return 0;
		}
		const ULONGLONG start = ::GetTickCount64();
		auto appendImportPhase = [&](const char* phase)
		{
			const ProcessMemorySnapshot memory = GetProcessMemorySnapshot();
			CStringA row;
			row.Format("%s\t%I64u\t%I64u\t%I64u\r\n", phase, ::GetTickCount64() - start,
				static_cast<unsigned __int64>(memory.privateBytes), static_cast<unsigned __int64>(memory.workingSetBytes));
			DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
		};
		CStringA header("phase\telapsed_ms\tprivate_bytes\tworking_set_bytes\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written); output.Flush();

		wchar_t imagePath[MAX_PATH] = {};
		const DWORD imagePathLength = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_IMAGE_PATH", imagePath, _countof(imagePath));
		if (imagePathLength == 0 || imagePathLength >= _countof(imagePath) || ::GetFileAttributes(imagePath) == INVALID_FILE_ATTRIBUTES)
		{
			appendImportPhase("import-failed;phase=import;reason=image-path");
			output.Close(); ::PostQuitMessage(1); return 0;
		}

		appendImportPhase("open-complete");
		appendImportPhase("import-start");
		// Exercise the same image-import route as the UI, including the generated
		// binary id and apiAddBinary call, rather than constructing FB2 XML here.
		::ShowWindow(m_hWnd, SW_RESTORE);
		::SetForegroundWindow(m_hWnd);
		m_doc->m_body.SetFocus();
		MSHTML::IHTMLBodyElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLBodyElementPtr());
		MSHTML::IHTMLElementCollectionPtr paragraphs(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"P") : MSHTML::IHTMLElementCollectionPtr());
		auto isSectionDiv = [](const MSHTML::IHTMLElementPtr& element) -> bool
		{
			if (!element) return false;
			const _bstr_t tagName(element->tagName);
			const _bstr_t className(element->className);
			const wchar_t* const tagText = tagName;
			const wchar_t* const classText = className;
			return tagText && classText && _wcsicmp(tagText, L"DIV") == 0 && _wcsicmp(classText, L"section") == 0;
		};
		MSHTML::IHTMLElementPtr paragraph;
		for (long index = 0; paragraphs && index < paragraphs->length && !paragraph; ++index)
		{
			MSHTML::IHTMLElementPtr candidate(paragraphs->item(_variant_t(index), _variant_t()));
			for (MSHTML::IHTMLElementPtr ancestor(candidate); ancestor; ancestor = ancestor->parentElement)
			{
				if (isSectionDiv(ancestor))
				{
					paragraph = candidate;
					break;
				}
			}
		}
		MSHTML::IHTMLTxtRangePtr range(body ? body->createTextRange() : MSHTML::IHTMLTxtRangePtr());
		if (!range || !paragraph)
		{
			appendImportPhase("import-failed;phase=import;reason=section-range");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		range->moveToElementText(paragraph);
		range->collapse(VARIANT_TRUE);
		// Keep the test caret inside the paragraph rather than on its boundary;
		// InsImage then resolves its enclosing section just like a UI insertion.
		if (range->move(L"character", 1) != 1)
		{
			appendImportPhase("import-failed;phase=import;reason=section-caret");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		range->select();
		auto selectionIsInSection = [&]() -> bool
		{
			MSHTML::IHTMLTxtRangePtr selected(m_doc->m_body.Document()->selection->createRange());
			MSHTML::IHTMLElementPtr element(selected ? selected->parentElement() : MSHTML::IHTMLElementPtr());
			while (element && !isSectionDiv(element)) element = element->parentElement;
			return isSectionDiv(element);
		};
		const ULONGLONG selectionDeadline = ::GetTickCount64() + 1000;
		while (!selectionIsInSection() && ::GetTickCount64() < selectionDeadline)
		{
			MSG message = {};
			if (::PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
			{
				if (message.message == WM_QUIT) { ::PostQuitMessage(static_cast<int>(message.wParam)); break; }
				::TranslateMessage(&message);
				::DispatchMessage(&message);
			}
			else ::Sleep(1);
		}
		if (!selectionIsInSection())
		{
			appendImportPhase("import-failed;phase=import;reason=section-selection");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		wchar_t inlineMode[2] = {};
		const bool inlineImage = ::GetEnvironmentVariable(L"FBE_NEXT_TEST_IMAGE_INLINE", inlineMode, _countof(inlineMode)) != 1 || inlineMode[0] != L'0';
		m_doc->m_body.AddImage(imagePath, inlineImage);
		appendImportPhase("import-complete");
		appendImportPhase("save-start");
		if (!m_doc->Save())
		{
			appendImportPhase("save-failed;phase=save;operation=Save;actual_hresult=unavailable;symbolic_hresult=unavailable");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		appendImportPhase("save-complete");
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"binary-roundtrip"))
	{
		const ULONGLONG start = ::GetTickCount64();
		auto appendBinaryPhase = [&](const char* phase)
		{
			const ProcessMemorySnapshot memory = GetProcessMemorySnapshot();
			CStringA row;
			row.Format("%s\t%I64u\t%I64u\t%I64u\r\n", phase, ::GetTickCount64() - start,
				static_cast<unsigned __int64>(memory.privateBytes), static_cast<unsigned __int64>(memory.workingSetBytes));
			DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Flush();
		};
		CStringA header("phase\telapsed_ms\tprivate_bytes\tworking_set_bytes\r\n");
		DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written); output.Flush();
		appendBinaryPhase("open-complete");
		appendBinaryPhase("save-start");
		if (!m_doc->Save())
		{
			appendBinaryPhase("save-failed;phase=save;operation=Save;actual_hresult=unavailable;symbolic_hresult=unavailable");
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		appendBinaryPhase("save-complete");
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"spellcheck-local-edit"))
	{
		MSHTML::IHTMLBodyElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLBodyElementPtr());
		MSHTML::IHTMLElementCollectionPtr paragraphs(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(L"P") : MSHTML::IHTMLElementCollectionPtr());
		MSHTML::IHTMLElementPtr paragraph(paragraphs && paragraphs->length ? paragraphs->item(_variant_t(paragraphs->length - 1), _variant_t()) : MSHTML::IHTMLElementPtr());
		if (!body || !paragraph || !m_Speller)
		{
			output.Close(); ::PostQuitMessage(1); return 0;
		}
		m_doc->m_body.SetFocus();
		MSHTML::IHTMLTxtRangePtr range(body->createTextRange());
		if (!range) { output.Close(); ::PostQuitMessage(1); return 0; }
		range->moveToElementText(paragraph);
		range->collapse(VARIANT_TRUE);
		if (range->move(L"character", 1) != 1) { output.Close(); ::PostQuitMessage(1); return 0; }
		range->select();
		_bstr_t paragraphText(paragraph->innerText);
		CString editedText(static_cast<const wchar_t*>(paragraphText));
		editedText += L" localedit";
		paragraph->innerText = _bstr_t(static_cast<const wchar_t*>(editedText));
		m_Speller->SetEnabled(true);
		// Set the same settings gate used by OnEdChange, but do not trigger a
		// viewport-wide highlight pass before the local-edit measurement.
		_Settings.SetHighlightMisspells(true);
		m_Speller->ResetTestDiagnostics();
		BOOL handled = FALSE;
		OnEdChange(0, 0, NULL, handled);
		CStringA row;
		row.Format("paragraph_count\t%ld\r\ncheck_element_calls\t%ld\r\nvisited_paragraphs\t%ld\r\n", paragraphs->length, m_Speller->GetTestCheckElementCalls(), m_Speller->GetTestVisitedParagraphs());
		DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close();
		// The fixture is intentionally edited in memory only; terminate the
		// unattended message loop without opening the normal dirty-document UI.
		::PostQuitMessage(0); return 0;
	}
	if (IsFbeTestScenario(L"table-toolbar-rendering"))
	{
		// This is deliberately a UI-level probe.  The toolbar state and the
		// pixels it paints are recorded independently, so a disabled command is
		// never confused with an enabled command rendered as disabled.
		auto selectElement = [&](const wchar_t* tag, long index) -> bool
		{
			MSHTML::IHTMLElementPtr body(m_doc->m_body.Document() ? m_doc->m_body.Document()->body : MSHTML::IHTMLElementPtr());
			MSHTML::IHTMLElementCollectionPtr elements(body ? MSHTML::IHTMLElement2Ptr(body)->getElementsByTagName(tag) : MSHTML::IHTMLElementCollectionPtr());
			MSHTML::IHTMLElementPtr element(elements && elements->length > index ? elements->item(_variant_t(index), _variant_t()) : MSHTML::IHTMLElementPtr());
			if (!element) return false;
			m_doc->m_body.SetFocus();
			MSHTML::IHTMLTxtRangePtr range(MSHTML::IHTMLBodyElementPtr(body)->createTextRange());
			if (!range) return false;
			range->moveToElementText(element);
			range->collapse(VARIANT_TRUE);
			// Keep the test caret inside the target element rather than on its boundary.
			if (range->move(L"character", 1) != 1) return false;
			range->select();

			// MSHTML can publish the new selection asynchronously. Wait until
			// SelectionStructTableCon observes the context required by this phase
			// before the toolbar state is sampled.
			const bool expectTableContext = _wcsicmp(tag, L"TD") == 0 || _wcsicmp(tag, L"TH") == 0;
			const ULONGLONG deadline = ::GetTickCount64() + 1000;
			for (;;)
			{
				const bool hasTableContext = (bool)m_doc->m_body.SelectionStructTableCon();
				if (hasTableContext == expectTableContext)
					return true;
				if (::GetTickCount64() >= deadline)
					return false;

				MSG msg = {};
				bool pumpedMessage = false;
				while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{
					if (msg.message == WM_QUIT)
					{
						::PostQuitMessage(static_cast<int>(msg.wParam));
						return false;
					}
					::TranslateMessage(&msg);
					::DispatchMessage(&msg);
					pumpedMessage = true;
				}
				if (!pumpedMessage)
					::Sleep(1);
			}
		};
		auto updateTableCommands = [&](bool tableCommandEnabled)
		{
			// selectElement has just synchronously verified this same selection gate.
			// Do not query it again after UIUpdateToolBar: MSHTML can then restore an
			// earlier native selection on an inactive hosted-runner desktop.
			// Let the toolbar settle first. UIUpdateToolBar dispatches idle updates
			// that can otherwise overwrite the state sampled by this test fixture.
			UIUpdateToolBar();
			for (size_t index = 0; index < kTableToolbarCommandCount; ++index)
			{
				const UINT commandId = kTableToolbarCommands[index].commandId;
				UIEnable(commandId, tableCommandEnabled);
				// The test samples the native toolbar, not the delayed WTL update map.
				m_CmdToolbar.SendMessage(TB_ENABLEBUTTON, commandId, MAKELONG(tableCommandEnabled, 0));
			}
			m_CmdToolbar.Invalidate(); m_CmdToolbar.UpdateWindow();
		};
		auto chromaPixels = [&](const RECT& rect) -> long
		{
			HDC source = ::GetDC(m_CmdToolbar); if (!source) return -1;
			RECT client = {}; ::GetClientRect(m_CmdToolbar, &client);
			HDC memory = ::CreateCompatibleDC(source); HBITMAP bitmap = ::CreateCompatibleBitmap(source, client.right, client.bottom);
			HGDIOBJ old = memory && bitmap ? ::SelectObject(memory, bitmap) : NULL;
			if (!memory || !bitmap || !old || !::PrintWindow(m_CmdToolbar, memory, PW_CLIENTONLY)) { if (old) ::SelectObject(memory, old); if (bitmap) ::DeleteObject(bitmap); if (memory) ::DeleteDC(memory); ::ReleaseDC(m_CmdToolbar, source); return -1; }
			long chroma = 0;
			for (int y = rect.top; y < rect.bottom; ++y) for (int x = rect.left; x < rect.right; ++x) { const COLORREF pixel = ::GetPixel(memory, x, y); const int r = GetRValue(pixel), g = GetGValue(pixel), b = GetBValue(pixel); if (max(r, max(g, b)) - min(r, min(g, b)) >= 32) ++chroma; }
			::SelectObject(memory, old); ::DeleteObject(bitmap); ::DeleteDC(memory); ::ReleaseDC(m_CmdToolbar, source); return chroma;
		};
		auto imageBlackPixels = [&](const RECT& rect) -> long
		{
			HDC source = ::GetDC(m_CmdToolbar); if (!source) return -1;
			RECT client = {}; ::GetClientRect(m_CmdToolbar, &client);
			HDC memory = ::CreateCompatibleDC(source); HBITMAP bitmap = ::CreateCompatibleBitmap(source, client.right, client.bottom);
			HGDIOBJ old = memory && bitmap ? ::SelectObject(memory, bitmap) : NULL;
			if (!memory || !bitmap || !old || !::PrintWindow(m_CmdToolbar, memory, PW_CLIENTONLY)) { if (old) ::SelectObject(memory, old); if (bitmap) ::DeleteObject(bitmap); if (memory) ::DeleteDC(memory); ::ReleaseDC(m_CmdToolbar, source); return -1; }
			const int left = rect.left + (rect.right - rect.left - 24) / 2;
			const int top = rect.top + (rect.bottom - rect.top - 24) / 2;
			long black = 0;
			for (int y = top; y < top + 24; ++y) for (int x = left; x < left + 24; ++x) if (::GetPixel(memory, x, y) == RGB(0, 0, 0)) ++black;
			::SelectObject(memory, old); ::DeleteObject(bitmap); ::DeleteDC(memory); ::ReleaseDC(m_CmdToolbar, source); return black;
		};
		const bool imageListHasMask = ImageListHasMaskPlane(m_CmdToolbar.GetImageList());
		auto appendPhase = [&](const char* phase)
		{
			for (size_t index = 0; index < kTableToolbarCommandCount; ++index)
			{
				const UINT command = kTableToolbarCommands[index].commandId;
				RECT rect = {}; const bool hasRect = m_CmdToolbar.GetItemRect(m_CmdToolbar.CommandToIndex(command), &rect) != FALSE;
				const DWORD state = static_cast<DWORD>(m_CmdToolbar.SendMessage(TB_GETSTATE, command, 0));
				const int image = static_cast<int>(m_CmdToolbar.SendMessage(TB_GETBITMAP, command, 0));
				CStringA row; row.Format("%s\t%u\t%lu\t%d\t%d\t%d\t%d\t%ld\t%d\t%ld\r\n", phase, command, state,
					(state & TBSTATE_ENABLED) != 0 ? 1 : 0, (state & TBSTATE_CHECKED) != 0 ? 1 : 0,
					(state & TBSTATE_HIDDEN) != 0 ? 1 : 0, image, hasRect ? chromaPixels(rect) : -1,
					imageListHasMask ? 1 : 0, hasRect ? imageBlackPixels(rect) : -1);
				DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written);
			}
			output.Flush();
		};
		CStringA header("phase\tcommand_id\ttb_state\tenabled\tchecked\thidden\timage_index\tchroma_pixels\timage_list_has_mask\timage_black_pixels\r\n"); DWORD written = 0; output.Write(header, static_cast<DWORD>(header.GetLength()), &written);
		ShowView(BODY);
		if (!selectElement(L"P", 0)) { output.Close(); ::PostQuitMessage(1); return 0; }
		updateTableCommands(false); appendPhase("outside-1");
		if (!selectElement(L"TD", 0)) { output.Close(); ::PostQuitMessage(1); return 0; }
		updateTableCommands(true); appendPhase("inside-1");
		if (!selectElement(L"TD", 1)) { output.Close(); ::PostQuitMessage(1); return 0; }
		updateTableCommands(true); appendPhase("inside-multi");
		if (!selectElement(L"P", 0)) { output.Close(); ::PostQuitMessage(1); return 0; }
		updateTableCommands(false); appendPhase("outside-2");
		if (!selectElement(L"TH", 0)) { output.Close(); ::PostQuitMessage(1); return 0; }
		updateTableCommands(true); appendPhase("inside-2");
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"context-attribute-bars-runtime"))
	{
		const ContextAttributeBarsDiagnostics diagnostics = m_contextAttributeBars.RunDiagnostics();
		CStringA row; row.Format("controls\t%d\r\nids\t%d\r\ncatalogs\t%d\r\nstate\t%d\r\navailability\t%d\r\nlayout\t%d\r\n", diagnostics.controls ? 1 : 0, diagnostics.ids ? 1 : 0, diagnostics.catalogs ? 1 : 0, diagnostics.state ? 1 : 0, diagnostics.availability ? 1 : 0, diagnostics.layout ? 1 : 0);
		DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"source-editor-ui-runtime"))
	{
		const SourceEditorControlDiagnostics diagnostics = m_source.RunDiagnostics();
		CStringA row; row.Format("created\t%d\r\nutf8\t%d\r\neol\t%d\r\neol_visibility\t%d\r\nwrapping\t%d\r\nwhitespace\t%d\r\nline_numbers\t%d\r\nfolding\t%d\r\nstyles\t%d\r\ntag_state\t%d\r\nmetrics\t%d\r\nreapply\t%d\r\n", diagnostics.created ? 1 : 0, diagnostics.utf8 ? 1 : 0, diagnostics.eol ? 1 : 0, diagnostics.eolVisibility ? 1 : 0, diagnostics.wrapping ? 1 : 0, diagnostics.whitespace ? 1 : 0, diagnostics.lineNumbers ? 1 : 0, diagnostics.folding ? 1 : 0, diagnostics.styles ? 1 : 0, diagnostics.tagState ? 1 : 0, diagnostics.metrics ? 1 : 0, diagnostics.reapply ? 1 : 0);
		DWORD written = 0; output.Write(row, static_cast<DWORD>(row.GetLength()), &written); output.Close(); PostMessage(WM_CLOSE); return 0;
	}
	if (IsFbeTestScenario(L"export-html"))
	{
		// The plugin itself receives deterministic options through its test-only
		// environment hook; activation and Export still follow the normal FBE
		// local-COM production path.
		BOOL handled = FALSE;
		OnToolsExport(0, ID_EXPORT_BASE, m_hWnd, handled);
		output.Close(); PostMessage(WM_CLOSE); return 0;
	}

	CStringA rows("phase\telapsed_ms\tprivate_bytes\tworking_set_bytes\tcommitted_bytes\treserved_bytes\tsource_bytes\tsource_lines\tundo_selection_history\r\n");
	const ULONGLONG start = ::GetTickCount64();
	auto appendSnapshot = [&](const char* phase)
	{
		const ProcessMemorySnapshot memory = GetProcessMemorySnapshot();
		const sptr_t sourceBytes = m_source.SendMessage(SCI_GETLENGTH);
		const sptr_t sourceLines = m_source.SendMessage(SCI_GETLINECOUNT);
		CStringA row;
		row.Format("%s\t%I64u\t%I64u\t%I64u\t%I64u\t%I64u\t%Id\t%Id\t%d\r\n", phase,
			::GetTickCount64() - start, static_cast<unsigned __int64>(memory.privateBytes),
			static_cast<unsigned __int64>(memory.workingSetBytes), static_cast<unsigned __int64>(memory.committedBytes),
			static_cast<unsigned __int64>(memory.reservedBytes), sourceBytes, sourceLines,
			AU::_ARGS.disable_undo_selection_history ? 0 : 1);
		rows += row;
	};

	appendSnapshot("document-open");
	auto appendShowSourceProfile = [&](const char* scenario)
	{
		for (const SourceProfileSample& sample : g_show_source_profile)
		{
			const ProcessMemorySnapshot memory = GetProcessMemorySnapshot();
			CStringA phase("showsource-");
			phase += scenario;
			phase += ":";
			phase += sample.phase;
			CStringA row;
			row.Format("%s\t%.3f\t%I64u\t%I64u\t%I64u\t%I64u\t%Id\t%Id\t%d\r\n", phase.GetString(),
				sample.elapsedMilliseconds, static_cast<unsigned __int64>(memory.privateBytes),
				static_cast<unsigned __int64>(memory.workingSetBytes), static_cast<unsigned __int64>(memory.committedBytes),
				static_cast<unsigned __int64>(memory.reservedBytes), m_source.SendMessage(SCI_GETLENGTH),
				m_source.SendMessage(SCI_GETLINECOUNT), AU::_ARGS.disable_undo_selection_history ? 0 : 1);
			rows += row;
		}
	};
	auto appendTableSnapshot = [&](const char* phase)
	{
		const sptr_t sourceLength = m_source.SendMessage(SCI_GETLENGTH);
		std::vector<char> source(static_cast<size_t>(sourceLength) + 1);
		m_source.SendMessage(SCI_GETTEXT, sourceLength + 1, reinterpret_cast<LPARAM>(source.data()));
		auto countTag = [&](const char* tag) -> long
		{
			long count = 0;
			for (const char* position = source.data(); (position = strstr(position, tag)) != NULL; ++position)
				++count;
			return count;
		};
		CStringA row;
		row.Format("%s:table=%ld;tr=%ld;td=%ld;th=%ld\t%I64u\t0\t0\t0\t0\t%Id\t%Id\t%d\r\n",
			phase, countTag("<table"), countTag("<tr"), countTag("<td"), countTag("<th"),
			::GetTickCount64() - start, sourceLength, m_source.SendMessage(SCI_GETLINECOUNT),
			AU::_ARGS.disable_undo_selection_history ? 0 : 1);
		rows += row;
	};
	ShowView(SOURCE);
	appendShowSourceProfile("first");
	appendTableSnapshot("table-source-first");
	for (int repeat = 1; repeat <= 5; ++repeat)
	{
		ShowView(BODY);
		ShowView(SOURCE);
		appendTableSnapshot("table-unchanged-body-source");
		CStringA scenario;
		scenario.Format("unchanged-%d", repeat);
		appendShowSourceProfile(scenario);
	}
	appendSnapshot("source-unchanged-body-source-5");
	m_source.SendMessage(SCI_COLOURISE, 0, -1);
	appendSnapshot("source-styled-wrap-word");
	SourceEditorConfig benchmarkConfig = BuildSourceEditorConfig();
	benchmarkConfig.wrap = false;
	m_source.ApplyConfiguration(benchmarkConfig);
	m_source.SendMessage(SCI_COLOURISE, 0, -1);
	appendSnapshot("source-styled-wrap-none");
	m_source.FoldAll();
	appendSnapshot("fold-all");
	m_source.FoldAll();
	appendSnapshot("expand-all");

	const sptr_t length = m_source.SendMessage(SCI_GETLENGTH);
	const sptr_t stride = max<sptr_t>(1, length / 997);
	const char* const sectionNeedle = "<section";
	sptr_t searchStart = 0;
	for (int iteration = 0; iteration < 1000; ++iteration)
	{
		m_source.SendMessage(SCI_SETTARGETSTART, searchStart);
		m_source.SendMessage(SCI_SETTARGETEND, length);
		const sptr_t found = m_source.SendMessage(SCI_SEARCHINTARGET, strlen(sectionNeedle),
			reinterpret_cast<LPARAM>(sectionNeedle));
		searchStart = found < 0 ? 0 : m_source.SendMessage(SCI_GETTARGETEND);
	}
	appendSnapshot("find-section-1000");
	m_source.SendMessage(SCI_EMPTYUNDOBUFFER);
	searchStart = 0;
	for (int iteration = 0; iteration < 100; ++iteration)
	{
		m_source.SendMessage(SCI_SETTARGETSTART, searchStart);
		m_source.SendMessage(SCI_SETTARGETEND, length);
		const sptr_t found = m_source.SendMessage(SCI_SEARCHINTARGET, strlen(sectionNeedle),
			reinterpret_cast<LPARAM>(sectionNeedle));
		if (found < 0) { searchStart = 0; continue; }
		m_source.SendMessage(SCI_REPLACETARGET, strlen(sectionNeedle), reinterpret_cast<LPARAM>(sectionNeedle));
		searchStart = m_source.SendMessage(SCI_GETTARGETEND);
	}
	m_source.SendMessage(SCI_EMPTYUNDOBUFFER);
	m_source.SendMessage(SCI_SETSAVEPOINT);
	appendSnapshot("replace-section-same-text-100");
	const sptr_t lineCount = m_source.SendMessage(SCI_GETLINECOUNT);
	for (int iteration = 0; iteration < 1000; ++iteration)
	{
		const sptr_t line = (static_cast<sptr_t>(iteration) * 37) % lineCount;
		m_source.SendMessage(SCI_SETCURRENTPOS, m_source.SendMessage(SCI_POSITIONFROMLINE, line));
	}
	appendSnapshot("navigate-source-lines-1000");
	m_source.SendMessage(SCI_EMPTYUNDOBUFFER);
	for (int iteration = 0; iteration < 10000; ++iteration)
	{
		const sptr_t position = length + iteration;
		m_source.SendMessage(SCI_SETSEL, position, position);
		m_source.SendMessage(SCI_INSERTTEXT, position, reinterpret_cast<LPARAM>(" "));
	}
	appendSnapshot("undo-selection-history-10000-edits");
	for (int iteration = 0; iteration < 10000 && m_source.SendMessage(SCI_CANUNDO); ++iteration)
		m_source.SendMessage(SCI_UNDO);
	appendSnapshot("undo-all-10000-edits");
	for (int iteration = 0; iteration < 10000 && m_source.SendMessage(SCI_CANREDO); ++iteration)
		m_source.SendMessage(SCI_REDO);
	appendSnapshot("redo-all-10000-edits");
	for (int iteration = 0; iteration < 10000 && m_source.SendMessage(SCI_CANUNDO); ++iteration)
		m_source.SendMessage(SCI_UNDO);
	m_source.SendMessage(SCI_EMPTYUNDOBUFFER);
	m_source.SendMessage(SCI_SETSAVEPOINT);

	auto runMatchedTags = [&](int first, int last)
	{
		for (int iteration = first; iteration < last; ++iteration)
		{
			const sptr_t position = (static_cast<sptr_t>(iteration) * stride) % length;
		// Stress the same caret/update lifecycle as keyboard navigation without
		// forcing every position through viewport scroll policy and layout cache.
		m_source.SendMessage(SCI_SETCURRENTPOS, position);
		m_source.UpdateTagHighlight({ true, _Settings.XmlSrcTagHighlightMode() ? XmlTagHighlightMode::FullTag : XmlTagHighlightMode::NameOnly, _Settings.XmlSrcTagHighlightAttributes(), _Settings.XmlSrcTagHighlightErrors() });
		}
	};
	runMatchedTags(0, 10000);
	appendSnapshot("matched-tags-10000-positions");
	runMatchedTags(10000, 50000);
	appendSnapshot("matched-tags-50000-positions");
	runMatchedTags(50000, 100000);
	appendSnapshot("matched-tags-100000-positions");
	if (AU::_ARGS.run_source_view_cycles)
	{
		for (int cycle = 1; cycle <= 100; ++cycle)
		{
			ShowView(BODY);
			ShowView(SOURCE);
			m_source.SendMessage(SCI_COLOURISE, 0, -1);
			if (cycle == 1 || cycle == 10 || cycle == 50 || cycle == 100)
			{
				CStringA phase;
				phase.Format("body-source-cycle-%d", cycle);
				const ProcessMemorySnapshot memory = GetProcessMemorySnapshot();
				const sptr_t sourceBytes = m_source.SendMessage(SCI_GETLENGTH);
				const sptr_t sourceLines = m_source.SendMessage(SCI_GETLINECOUNT);
				CStringA row;
				row.Format("%s\t%I64u\t%I64u\t%I64u\t%I64u\t%I64u\t%Id\t%Id\t%d\r\n", phase.GetString(),
					::GetTickCount64() - start, static_cast<unsigned __int64>(memory.privateBytes),
					static_cast<unsigned __int64>(memory.workingSetBytes), static_cast<unsigned __int64>(memory.committedBytes),
					static_cast<unsigned __int64>(memory.reservedBytes), sourceBytes, sourceLines,
					AU::_ARGS.disable_undo_selection_history ? 0 : 1);
				rows += row;
				appendTableSnapshot("table-body-source");
			}
		}
	}
	if (AU::_ARGS.save_benchmark_document)
	{
		ShowView(BODY);
		if (!m_doc->Save())
		{
			// A failed serialization transaction must leave a deterministic
			// diagnostic trail for the production safety test.  A second Save
			// verifies that the document has been fail-closed in memory.
			const bool secondSaveRejected = !m_doc->Save();
			CStringA row;
			row.Format("table-save-rejected:second-save-rejected=%d\t%I64u\t0\t0\t0\t0\t0\t0\t%d\r\n",
				secondSaveRejected ? 1 : 0, ::GetTickCount64() - start,
				AU::_ARGS.disable_undo_selection_history ? 0 : 1);
			rows += row;
			DWORD written = 0;
			output.Write(rows, static_cast<DWORD>(rows.GetLength()), &written);
			output.Close();
			// This is an internal benchmark failure, not an interactive close:
			// do not enter the dirty-document prompt after Save was rejected.
			::PostQuitMessage(1);
			return 0;
		}
		ShowView(SOURCE);
		appendTableSnapshot("table-after-save");
	}

	DWORD written = 0;
	output.Write(rows, static_cast<DWORD>(rows.GetLength()), &written);
	output.Close();
	// -b is an unattended contract.  WM_CLOSE routes through DiscardChanges(),
	// which may legitimately ask an interactive user about a dirty FBD document
	// after reopening it.  There is no user in batch mode, and the report was
	// already written, so end the message loop directly.
	::PostQuitMessage(0);
	return 0;
}
