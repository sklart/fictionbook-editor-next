
#include "../settings/hotkeys/HotkeyExport.h"
#include "Script64ResultChecks.h"

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

class CUndoManagerEnableScope {
public:
	CUndoManagerEnableScope(IOleUndoManager* manager) : m_manager(manager), m_enabled(false) { if (m_manager && SUCCEEDED(m_manager->Enable(FALSE))) m_enabled = true; }
	~CUndoManagerEnableScope() { if (m_enabled) m_manager->Enable(TRUE); }
	bool Active() const { return m_enabled; }
private:
	CComPtr<IOleUndoManager> m_manager;
	bool m_enabled;
};

class CBlockImageUndoProbeUnit : public CComObjectRootEx<CComSingleThreadModel>, public IOleUndoUnit {
public:
	BEGIN_COM_MAP(CBlockImageUndoProbeUnit) COM_INTERFACE_ENTRY(IOleUndoUnit) END_COM_MAP()
	void Initialize(const MSHTML::IHTMLElementPtr& image, const MSHTML::IHTMLElementPtr& anchor) { m_image = image; m_anchor = anchor; m_inserted = true; }
	STDMETHOD(Do)(IOleUndoManager* manager) {
		CUndoManagerEnableScope disabled(manager);
		if (!disabled.Active() || !m_image || !m_anchor) return E_UNEXPECTED;
		if (m_inserted) { if (m_image->parentElement) MSHTML::IHTMLDOMNodePtr(m_image)->removeNode(VARIANT_TRUE); }
		else { MSHTML::IHTMLElement2Ptr(m_anchor)->insertAdjacentElement(L"beforeBegin", m_image); }
		m_inserted = !m_inserted;
		return manager->Add(this);
	}
	STDMETHOD(GetDescription)(BSTR* description) { if (!description) return E_POINTER; *description = ::SysAllocString(L"block image undo probe"); return *description ? S_OK : E_OUTOFMEMORY; }
	STDMETHOD(GetUnitType)(CLSID* classId, LONG* id) { if (!classId || !id) return E_POINTER; *classId = CLSID_NULL; *id = 0; return S_OK; }
	STDMETHOD(OnNextAdd)() { return S_OK; }
private:
	MSHTML::IHTMLElementPtr m_image, m_anchor;
	bool m_inserted;
};

static VOID CALLBACK CloseNavigationPopupForTest(HWND, UINT, UINT_PTR timer, DWORD)
{
	::KillTimer(NULL, timer);
	::EndMenu();
}

void CMainFrame::RunPortableStateTestScenario()
{
    if(IsFbeTestScenario(L"interface-theme-policy-runtime")) {
        _Settings.SetInterfaceTheme(INTERFACE_THEME_DARK);
        CSettingsGeneralPage page(false);
        bool passed = page.Create(m_hWnd) != NULL;
        if(passed) {
            page.ConfigureInterfaceTheme(false);
            const HWND choice = page.GetDlgItem(IDC_INTERFACE_THEME);
            passed = ::SendMessage(choice, CB_GETCOUNT, 0, 0) == 2 && ::SendMessage(choice, CB_GETCURSEL, 0, 0) == INTERFACE_THEME_LIGHT &&
                ::GetWindowTextLength(page.GetDlgItem(IDC_INTERFACE_THEME_NOTE)) > 0;
            RECT notice = {}, openGroup = {}, interfaceGroup = {};
            ::GetWindowRect(page.GetDlgItem(IDC_INTERFACE_THEME_NOTE), &notice);
            ::GetWindowRect(page.GetDlgItem(IDC_SETTINGS_OTHER_OPEN_GROUP), &openGroup);
            ::GetWindowRect(page.GetDlgItem(IDC_OPTIONS_INTERFACE_GROUP), &interfaceGroup);
            passed = passed && notice.bottom <= openGroup.top && notice.right <= interfaceGroup.right && notice.left >= interfaceGroup.left;
            page.Commit(); passed = passed && _Settings.GetInterfaceTheme() == INTERFACE_THEME_DARK;
            page.CancelChanges(); passed = passed && _Settings.GetInterfaceTheme() == INTERFACE_THEME_DARK;
            ::SendMessage(choice, CB_SETCURSEL, INTERFACE_THEME_AUTOMATIC, 0);
            page.Commit(); passed = passed && _Settings.GetInterfaceTheme() == INTERFACE_THEME_AUTOMATIC;
            _Settings.SetInterfaceTheme(INTERFACE_THEME_DARK); page.ConfigureInterfaceTheme(false);
            page.SendMessage(WM_COMMAND, MAKEWPARAM(IDC_INTERFACE_THEME, CBN_SELENDOK), reinterpret_cast<LPARAM>(choice));
            page.Commit(); passed = passed && _Settings.GetInterfaceTheme() == INTERFACE_THEME_LIGHT;
            _Settings.SetInterfaceTheme(INTERFACE_THEME_DARK); page.ConfigureInterfaceTheme(true);
            passed = passed && ::SendMessage(choice, CB_GETCOUNT, 0, 0) == 3 && ::SendMessage(choice, CB_GETCURSEL, 0, 0) == INTERFACE_THEME_DARK;
            page.DestroyWindow();
        }
        _Settings.SetInterfaceTheme(INTERFACE_THEME_DARK, true);
        WritePortableStateTestText(CString(DeploymentContext::DiagnosticsDirectory().c_str()) + L"portable-state-report.txt",
            passed ? "controlled-theme-choice-policy=1\nresult=pass\n" : "controlled-theme-choice-policy=0\nresult=fail\n");
        PostMessage(WM_CLOSE); return;
    }
    if(IsFbeTestScenario(L"interface-theme-policy-read")) {
        const bool passed = _Settings.GetInterfaceTheme() == INTERFACE_THEME_DARK && ThemeManager::GetSelectedTheme() == INTERFACE_THEME_DARK;
        WritePortableStateTestText(CString(DeploymentContext::DiagnosticsDirectory().c_str()) + L"portable-state-report.txt",
            passed ? "theme-preference-restart=1\nresult=pass\n" : "theme-preference-restart=0\nresult=fail\n");
        PostMessage(WM_CLOSE); return;
    }
    if(IsFbeTestScenario(L"script-icon-size-read")) {
        int width = 0, height = 0;
        const int expected = UiMetrics::ScaleForDpi(32, UiMetrics::DpiForWindow(m_hWnd));
        const bool passed = _Settings.GetScriptIconSize() == 32 &&
            ::ImageList_GetIconSize(m_scriptsToolbarImages, &width, &height) && width == expected && height == expected;
        WritePortableStateTestText(CString(DeploymentContext::DiagnosticsDirectory().c_str()) + L"portable-state-report.txt",
            passed ? "icon-size-restart=1\nresult=pass\n" : "icon-size-restart=0\nresult=fail\n");
        PostMessage(WM_CLOSE); return;
    }
#include "RuntimeTestScript64.inl"
#include "RuntimeTestNoteCreation.inl"
	const bool ordinaryWrite = IsFbeTestScenario(L"portable-state-write");
	const bool ordinaryRead = IsFbeTestScenario(L"portable-state-read");
	const bool emptyToolbarWrite = IsFbeTestScenario(L"portable-toolbar-empty-write");
	const bool emptyToolbarRead = IsFbeTestScenario(L"portable-toolbar-empty-read");
	const bool toolbarLayoutWrite = IsFbeTestScenario(L"portable-toolbar-layout-write");
	const bool toolbarLayoutRead = IsFbeTestScenario(L"portable-toolbar-layout-read");
	const bool missingScriptRead = IsFbeTestScenario(L"portable-toolbar-missing-script-read");
	const bool returnedScriptRead = IsFbeTestScenario(L"portable-toolbar-returned-script-read");
	const bool malformedToolbarRead = IsFbeTestScenario(L"portable-toolbar-malformed-read");
	const bool scriptsReload = IsFbeTestScenario(L"portable-scripts-reload");
	const bool legacyHotkeyRead = IsFbeTestScenario(L"portable-legacy-hotkey-read");
	const bool hotkeyExport = IsFbeTestScenario(L"hotkey-export-runtime");
	const bool diagnosticCleanup = IsFbeTestScenario(L"portable-diagnostic-cleanup");
	const bool scriptToolbarLifecycle = IsFbeTestScenario(L"script-toolbar-lifecycle-runtime");
	const bool scriptToolbarLifecycleReload = IsFbeTestScenario(L"script-toolbar-lifecycle-reload-runtime");
	const bool scriptToolbarRollbackNoMain = IsFbeTestScenario(L"script-toolbar-rollback-no-main-runtime");
	const bool scriptToolbarRollbackPersisted = IsFbeTestScenario(L"script-toolbar-rollback-persisted-runtime");
	const bool scriptToolbarRollbackPartial = IsFbeTestScenario(L"script-toolbar-rollback-partial-runtime");
	const bool scriptToolbarRuntimeSize = IsFbeTestScenario(L"script-toolbar-runtime-size");
	const bool toolbarLayoutAdapterTransaction = IsFbeTestScenario(L"toolbar-layout-adapter-transaction");
	const bool scriptsToolbarCustomizeCtrlF = IsFbeTestScenario(L"scripts-toolbar-customize-ctrl-f-runtime");
	const bool navigationScriptsRuntime = IsFbeTestScenario(L"navigation-scripts-runtime");
	const bool navigationSearchFavoritesRuntime = IsFbeTestScenario(L"navigation-search-favorites-runtime");
	const bool navigationScriptsMultiRuntime = IsFbeTestScenario(L"navigation-scripts-multiselect-runtime");
	const bool navigationFavoritesReadRuntime = IsFbeTestScenario(L"navigation-favorites-read-runtime");
	const bool navigationViewBarElementsRuntime = IsFbeTestScenario(L"navigation-viewbar-elements-runtime");
	const bool navigationScriptsReloadRuntime = IsFbeTestScenario(L"navigation-scripts-reload-runtime");
	const bool scriptLiveReloadRuntime = IsFbeTestScenario(L"script-live-reload-runtime");
	const bool scriptCardDialogRuntime = IsFbeTestScenario(L"script-card-dialog-runtime");
	const bool scriptCatalogRefreshRuntime = IsFbeTestScenario(L"script-catalog-refresh-runtime");
	const bool scriptStartupValidationOn = IsFbeTestScenario(L"script-startup-validation-on");
	const bool scriptStartupValidationOffWrite = IsFbeTestScenario(L"script-startup-validation-off-write");
	const bool scriptStartupValidationOffRead = IsFbeTestScenario(L"script-startup-validation-off-read");
	if (!ordinaryWrite && !ordinaryRead && !emptyToolbarWrite && !emptyToolbarRead && !toolbarLayoutWrite && !toolbarLayoutRead && !missingScriptRead && !returnedScriptRead && !malformedToolbarRead && !scriptsReload && !legacyHotkeyRead && !hotkeyExport && !diagnosticCleanup && !scriptToolbarLifecycle && !scriptToolbarLifecycleReload && !scriptToolbarRollbackNoMain && !scriptToolbarRollbackPersisted && !scriptToolbarRollbackPartial && !scriptToolbarRuntimeSize && !toolbarLayoutAdapterTransaction && !scriptsToolbarCustomizeCtrlF && !navigationScriptsRuntime && !navigationSearchFavoritesRuntime && !navigationScriptsMultiRuntime && !navigationFavoritesReadRuntime && !navigationViewBarElementsRuntime && !navigationScriptsReloadRuntime && !scriptLiveReloadRuntime && !scriptCardDialogRuntime && !scriptCatalogRefreshRuntime && !scriptStartupValidationOn && !scriptStartupValidationOffWrite && !scriptStartupValidationOffRead)
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
	if (DeploymentContext::CurrentMode() != DeploymentContext::Mode::Portable && !scriptToolbarLifecycle && !scriptToolbarLifecycleReload && !scriptToolbarRollbackNoMain && !scriptToolbarRollbackPersisted && !scriptToolbarRollbackPartial && !scriptToolbarRuntimeSize && !toolbarLayoutAdapterTransaction && !scriptsToolbarCustomizeCtrlF && !navigationScriptsRuntime && !navigationSearchFavoritesRuntime && !navigationScriptsMultiRuntime && !navigationFavoritesReadRuntime && !navigationScriptsReloadRuntime && !scriptLiveReloadRuntime && !scriptCardDialogRuntime && !scriptCatalogRefreshRuntime)
	{
		WritePortableStateTestText(reportPath, "phase=failed\nreason=not-portable\n");
		PostMessage(WM_CLOSE);
		return;
	}
	if(toolbarLayoutAdapterTransaction)
	{
		TBBUTTONS available;
		std::vector<TBBUTTON> catalog;
		std::vector<PortableToolbarItem> previous, actual, candidate;
		const bool catalogReady = GetAvailableButtons(m_CmdToolbar, available);
		if(catalogReady) for(int index = 0; index < available.GetSize(); ++index) catalog.push_back(available[index]);
		const bool capturedPrevious = ToolbarLayoutAdapter::Capture(m_CmdToolbar, previous);
		candidate = previous;
		if(candidate.size() > 1) std::swap(candidate[0], candidate[1]);
		else { PortableToolbarItem separator = {}; separator.separator = true; separator.width = 8; candidate.push_back(separator); }
		auto same = [](const std::vector<PortableToolbarItem>& left, const std::vector<PortableToolbarItem>& right) {
			if(left.size() != right.size()) return false;
			for(size_t index = 0; index < left.size(); ++index)
				if(left[index].separator != right[index].separator || left[index].command != right[index].command || left[index].width != right[index].width) return false;
			return true;
		};
		ToolbarLayoutAdapter::SetTestFailurePointForTest(ToolbarLayoutAdapter::TestFailureBeforeDelete);
		const bool deleteRejected = catalogReady && !ToolbarLayoutAdapter::Apply(m_CmdToolbar, candidate, catalog);
		const bool capturedAfterDelete = ToolbarLayoutAdapter::Capture(m_CmdToolbar, actual);
		const bool deleteRolledBack = capturedAfterDelete && same(previous, actual);
		ToolbarLayoutAdapter::SetTestFailurePointForTest(ToolbarLayoutAdapter::TestFailureBeforeAdd);
		const bool addRejected = catalogReady && !ToolbarLayoutAdapter::Apply(m_CmdToolbar, candidate, catalog);
		const bool capturedAfterAdd = ToolbarLayoutAdapter::Capture(m_CmdToolbar, actual);
		const bool addRolledBack = capturedAfterAdd && same(previous, actual);
		ToolbarLayoutAdapter::SetTestFailurePointForTest(ToolbarLayoutAdapter::TestFailureBeforeRollback);
		const bool rollbackRejected = catalogReady && !ToolbarLayoutAdapter::Apply(m_CmdToolbar, candidate, catalog);
		const bool capturedAfterRollbackFailure = ToolbarLayoutAdapter::Capture(m_CmdToolbar, actual);
		const bool rollbackFailureObserved = capturedAfterRollbackFailure && !same(previous, actual);
		ToolbarLayoutAdapter::SetTestFailurePointForTest(ToolbarLayoutAdapter::TestFailureNone);
		const bool rollbackRecovered = ToolbarLayoutAdapter::Apply(m_CmdToolbar, previous, catalog) && ToolbarLayoutAdapter::Capture(m_CmdToolbar, actual) && same(previous, actual);
		const bool passed = catalogReady && capturedPrevious && !previous.empty() && deleteRejected && deleteRolledBack && addRejected && addRolledBack && rollbackRejected && rollbackFailureObserved && rollbackRecovered;
		CStringA report; report.Format("phase=toolbar-layout-adapter-transaction\ncatalog=%d\ndelete-rejected=%d\ndelete-rollback=%d\nadd-rejected=%d\nadd-rollback=%d\nrollback-rejected=%d\nrollback-failure-observed=%d\nrollback-recovered=%d\nresult=%s\n", catalogReady, deleteRejected, deleteRolledBack, addRejected, addRolledBack, rollbackRejected, rollbackFailureObserved, rollbackRecovered, passed ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report); PostMessage(WM_CLOSE); return;
	}
	if(scriptsToolbarCustomizeCtrlF)
	{
		std::vector<ScriptsToolbarCommand> sample;
		for (int i = 0; i < 4; ++i) { ScriptsToolbarCommand item = {}; item.command = 30000 + i; item.button.iBitmap = -1; sample.push_back(item); }
		sample[0].name = L"Alpha"; sample[0].relativePath = L"z\\alpha.js";
		sample[1].name = L"Zulu"; sample[1].relativePath = L"A\\zulu.js";
		sample[2].name = L"\u0422\u0435\u0441\u0442"; sample[2].relativePath = L"A\\nested\\one.js";
		sample[3].name = L"\u0422\u0435\u0441\u0442"; sample[3].relativePath = L"a\\nested\\two.js";
		ScriptsToolbarTarget target = {}; target.id = L"test"; target.name = L"test";
		CSimpleArray<TBBUTTON> defaults;
		CScriptsToolbarCustomizeDlg sorting(NULL, sample, defaults, _Settings, std::vector<ScriptsToolbarTarget>(1, target),
			[](const CString&, const std::vector<PortableToolbarItem>&) { return true; }, true, CString());
		bool sorted = sorting.Create(m_hWnd) != NULL;
		if (sorted) {
			CListBox list(sorting.GetDlgItem(IDC_SCRIPTS_TOOLBAR_AVAILABLE));
			sorted = list.GetCount() == 5 && list.GetItemData(1) == 1 && list.GetItemData(2) == 2 && list.GetItemData(3) == 3 && list.GetItemData(4) == 0;
			CString firstDuplicate, secondDuplicate;
			list.GetText(2, firstDuplicate); list.GetText(3, secondDuplicate);
			sorted = sorted && firstDuplicate != secondDuplicate && firstDuplicate.Find(L"one.js") >= 0 && secondDuplicate.Find(L"two.js") >= 0;
			::SendMessage(list, LB_SETSEL, TRUE, 2);
			sorting.SetDlgItemText(IDC_SCRIPTS_TOOLBAR_SEARCH, L"zulu");
			sorted = sorted && list.GetCount() == 1 && list.GetItemData(0) == 1 && list.GetSelCount() == 0;
			sorting.SetDlgItemText(IDC_SCRIPTS_TOOLBAR_SEARCH, L"");
			sorted = sorted && list.GetCount() == 5 && list.GetItemData(2) == 2 && ::SendMessage(list, LB_GETSEL, 2, 0) > 0;
			sorting.SetDlgItemText(IDC_SCRIPTS_TOOLBAR_SEARCH, L"nested");
			sorted = sorted && list.GetCount() == 2 && list.GetItemData(0) == 2 && list.GetItemData(1) == 3 && ::SendMessage(list, LB_GETSEL, 0, 0) > 0;
			::SendMessage(list, LB_SETSEL, TRUE, 1);
			::SendMessage(sorting, WM_COMMAND, MAKEWPARAM(IDC_SCRIPTS_TOOLBAR_AVAILABLE, LBN_SELCHANGE), reinterpret_cast<LPARAM>(list.m_hWnd));
			sorting.SetDlgItemText(IDC_SCRIPTS_TOOLBAR_SEARCH, L"two.js");
			sorted = sorted && list.GetCount() == 1 && list.GetItemData(0) == 3 && ::SendMessage(list, LB_GETSEL, 0, 0) > 0;
			sorting.SetDlgItemText(IDC_SCRIPTS_TOOLBAR_SEARCH, L"");
			sorted = sorted && ::SendMessage(list, LB_GETSEL, 2, 0) > 0 && ::SendMessage(list, LB_GETSEL, 3, 0) > 0;
			sorting.SetDlgItemText(IDC_SCRIPTS_TOOLBAR_SEARCH, L"zulu");
			::SendMessage(list, LB_SETSEL, TRUE, 0);
			::SendMessage(sorting, WM_COMMAND, MAKEWPARAM(IDC_SCRIPTS_TOOLBAR_AVAILABLE, LBN_SELCHANGE), reinterpret_cast<LPARAM>(list.m_hWnd));
			sorting.SetDlgItemText(IDC_SCRIPTS_TOOLBAR_SEARCH, L"");
			sorted = sorted && ::SendMessage(list, LB_GETSEL, 1, 0) > 0 && ::SendMessage(list, LB_GETSEL, 2, 0) == 0 && ::SendMessage(list, LB_GETSEL, 3, 0) == 0;
			sorting.DestroyWindow();
		}
		PortableToolbarItem existing = {}; existing.command = sample[0].command; target.items.push_back(existing);
		std::vector<PortableToolbarItem> savedItems;
		CScriptsToolbarCustomizeDlg insertion(NULL, sample, defaults, _Settings, std::vector<ScriptsToolbarTarget>(1, target),
			[&savedItems](const CString&, const std::vector<PortableToolbarItem>& items) { savedItems = items; return true; }, true, CString());
		bool inserted = insertion.Create(m_hWnd) != NULL;
		bool keyboardMove = false;
		if (inserted) {
			CListBox current(insertion.GetDlgItem(IDC_SCRIPTS_TOOLBAR_CURRENT));
			CListBox available(insertion.GetDlgItem(IDC_SCRIPTS_TOOLBAR_AVAILABLE));
			::SendMessage(current, LB_SETSEL, TRUE, 0);
			::SendMessage(available, WM_SETFOCUS, 0, 0);
			::SendMessage(available, LB_SETSEL, TRUE, 1);
			::SendMessage(insertion, WM_COMMAND, MAKEWPARAM(IDC_SCRIPTS_TOOLBAR_ADD, BN_CLICKED), reinterpret_cast<LPARAM>(insertion.GetDlgItem(IDC_SCRIPTS_TOOLBAR_ADD).m_hWnd));
			inserted = savedItems.size() == 2 && savedItems[0].command == sample[1].command && savedItems[1].command == sample[0].command;
			HWND down = insertion.GetDlgItem(IDC_SCRIPTS_TOOLBAR_DOWN), up = insertion.GetDlgItem(IDC_SCRIPTS_TOOLBAR_UP);
			inserted = inserted && ::IsWindowEnabled(down) && !::IsWindowEnabled(up);
			::SendMessage(down, BM_CLICK, 0, 0);
			inserted = inserted && savedItems.size() == 2 && savedItems[0].command == sample[0].command && savedItems[1].command == sample[1].command;
			inserted = inserted && ::IsWindowEnabled(up) && !::IsWindowEnabled(down);
			::SendMessage(up, BM_CLICK, 0, 0);
			inserted = inserted && savedItems.size() == 2 && savedItems[0].command == sample[1].command && savedItems[1].command == sample[0].command;
			for (int repeat = 0; repeat < 2; ++repeat) {
				::SendMessage(available, WM_SETFOCUS, 0, 0);
				::SendMessage(available, LB_SETSEL, FALSE, -1);
				::SendMessage(available, LB_SETSEL, TRUE, 0);
				::SendMessage(insertion, WM_COMMAND, MAKEWPARAM(IDC_SCRIPTS_TOOLBAR_ADD, BN_CLICKED), reinterpret_cast<LPARAM>(insertion.GetDlgItem(IDC_SCRIPTS_TOOLBAR_ADD).m_hWnd));
			}
			inserted = inserted && savedItems.size() == 4 && savedItems[0].separator && savedItems[1].separator && savedItems[2].command == sample[1].command;
			::SendMessage(current, LB_SETSEL, FALSE, -1);
			::SendMessage(current, LB_SETSEL, TRUE, 0);
			::SendMessage(current, LB_SETSEL, TRUE, 1);
			::SendMessage(insertion, WM_COMMAND, MAKEWPARAM(IDC_SCRIPTS_TOOLBAR_CURRENT, LBN_SELCHANGE), reinterpret_cast<LPARAM>(current.m_hWnd));
			::SendMessage(down, BM_CLICK, 0, 0);
			inserted = inserted && savedItems.size() == 4 && savedItems[0].command == sample[1].command && savedItems[1].separator && savedItems[2].separator && savedItems[3].command == sample[0].command;
			// A disjoint selection may have one item at a boundary while another
			// still moves. Check the model, retained selection and button states.
			::SendMessage(current, LB_SETSEL, FALSE, -1);
			::SendMessage(current, LB_SETSEL, TRUE, 0);
			::SendMessage(current, LB_SETSEL, TRUE, 3);
			::SendMessage(insertion, WM_COMMAND, MAKEWPARAM(IDC_SCRIPTS_TOOLBAR_CURRENT, LBN_SELCHANGE), reinterpret_cast<LPARAM>(current.m_hWnd));
			inserted = inserted && ::IsWindowEnabled(up) && ::IsWindowEnabled(down);
			::SendMessage(up, BM_CLICK, 0, 0);
			inserted = inserted && savedItems.size() == 4 && savedItems[0].command == sample[1].command && savedItems[1].separator && savedItems[2].command == sample[0].command && savedItems[3].separator &&
				::SendMessage(current, LB_GETSEL, 0, 0) > 0 && ::SendMessage(current, LB_GETSEL, 2, 0) > 0;
			::SendMessage(down, BM_CLICK, 0, 0);
			inserted = inserted && savedItems.size() == 4 && savedItems[0].separator && savedItems[1].command == sample[1].command && savedItems[2].separator && savedItems[3].command == sample[0].command &&
				::SendMessage(current, LB_GETSEL, 1, 0) > 0 && ::SendMessage(current, LB_GETSEL, 3, 0) > 0;
			::SendMessage(current, LB_SETSEL, TRUE, -1);
			::SendMessage(insertion, WM_COMMAND, MAKEWPARAM(IDC_SCRIPTS_TOOLBAR_CURRENT, LBN_SELCHANGE), reinterpret_cast<LPARAM>(current.m_hWnd));
			inserted = inserted && !::IsWindowEnabled(up) && !::IsWindowEnabled(down);
			::SendMessage(current, LB_SETSEL, FALSE, -1);
			::SendMessage(current, LB_SETSEL, TRUE, 1);
			::SendMessage(insertion, WM_COMMAND, MAKEWPARAM(IDC_SCRIPTS_TOOLBAR_CURRENT, LBN_SELCHANGE), reinterpret_cast<LPARAM>(current.m_hWnd));
			BYTE originalKeys[256] = {}, ctrlKeys[256] = {};
			if (::GetKeyboardState(originalKeys)) {
				memcpy(ctrlKeys, originalKeys, sizeof(ctrlKeys)); ctrlKeys[VK_CONTROL] |= 0x80;
				if (::SetKeyboardState(ctrlKeys)) {
					::SendMessage(current, WM_KEYDOWN, VK_DOWN, 0);
					const bool movedDown = savedItems.size() == 4 && savedItems[0].separator && savedItems[1].separator && savedItems[2].command == sample[1].command && savedItems[3].command == sample[0].command && ::SendMessage(current, LB_GETSEL, 2, 0) > 0;
					::SendMessage(current, WM_KEYDOWN, VK_UP, 0);
					keyboardMove = movedDown && savedItems.size() == 4 && savedItems[0].separator && savedItems[1].command == sample[1].command && savedItems[2].separator && savedItems[3].command == sample[0].command && ::SendMessage(current, LB_GETSEL, 1, 0) > 0;
				}
				::SetKeyboardState(originalKeys);
			}
			insertion.DestroyWindow();
		}
		ScriptsToolbarTarget multiTarget = {}; multiTarget.id = L"multi-test"; multiTarget.name = L"multi-test";
		for (int index : {0, 1}) { PortableToolbarItem item = {}; item.command = sample[index].command; multiTarget.items.push_back(item); }
		std::vector<PortableToolbarItem> multiSaved;
		CScriptsToolbarCustomizeDlg multi(NULL, sample, defaults, _Settings, std::vector<ScriptsToolbarTarget>(1, multiTarget),
			[&multiSaved](const CString&, const std::vector<PortableToolbarItem>& items) { multiSaved = items; return true; }, true, CString());
		bool multiInsert = multi.Create(m_hWnd) != NULL;
		if (multiInsert) {
			CListBox current(multi.GetDlgItem(IDC_SCRIPTS_TOOLBAR_CURRENT));
			CListBox available(multi.GetDlgItem(IDC_SCRIPTS_TOOLBAR_AVAILABLE));
			::SendMessage(current, LB_SETSEL, TRUE, 1);
			::SendMessage(available, WM_SETFOCUS, 0, 0);
			::SendMessage(available, LB_SETSEL, TRUE, 1);
			::SendMessage(available, LB_SETSEL, TRUE, 2);
			::SendMessage(multi, WM_COMMAND, MAKEWPARAM(IDC_SCRIPTS_TOOLBAR_ADD, BN_CLICKED), reinterpret_cast<LPARAM>(multi.GetDlgItem(IDC_SCRIPTS_TOOLBAR_ADD).m_hWnd));
			multiInsert = multiSaved.size() == 4 && multiSaved[0].command == sample[0].command && multiSaved[1].command == sample[2].command && multiSaved[2].command == sample[3].command && multiSaved[3].command == sample[1].command &&
				::SendMessage(current, LB_GETSEL, 1, 0) > 0 && ::SendMessage(current, LB_GETSEL, 2, 0) > 0;
			::SendMessage(multi, WM_COMMAND, MAKEWPARAM(IDC_SCRIPTS_TOOLBAR_REMOVE, BN_CLICKED), reinterpret_cast<LPARAM>(multi.GetDlgItem(IDC_SCRIPTS_TOOLBAR_REMOVE).m_hWnd));
			multiInsert = multiInsert && multiSaved.size() == 2 && multiSaved[0].command == sample[0].command && multiSaved[1].command == sample[1].command &&
				::SendMessage(available, LB_GETSEL, 1, 0) > 0 && ::SendMessage(available, LB_GETSEL, 2, 0) > 0;
			::SendMessage(multi, WM_COMMAND, MAKEWPARAM(IDC_SCRIPTS_TOOLBAR_ADD, BN_CLICKED), reinterpret_cast<LPARAM>(multi.GetDlgItem(IDC_SCRIPTS_TOOLBAR_ADD).m_hWnd));
			multiInsert = multiInsert && multiSaved.size() == 4 && multiSaved[0].command == sample[0].command && multiSaved[1].command == sample[1].command && multiSaved[2].command == sample[2].command && multiSaved[3].command == sample[3].command;
			multi.DestroyWindow();
		}
		ScriptsToolbarCustomizeRuntimeTest::ResetCtrlFSmoke();
		ShowScriptsToolbarCustomizeDialog(m_ScriptsToolbar);
		CStringA report = ScriptsToolbarCustomizeRuntimeTest::CtrlFSmokeReport();
		if (!sorted || !inserted || !keyboardMove || !multiInsert) report.Replace("result=pass", "result=fail");
		report.AppendFormat("directory-sort=%d\n", sorted);
		report.AppendFormat("insert-before=%d\n", inserted);
		report.AppendFormat("keyboard-move=%d\n", keyboardMove);
		report.AppendFormat("multi-insert-remove=%d\n", multiInsert);
		WritePortableStateTestText(reportPath, report);
		PostMessage(WM_CLOSE); return;
	}
	auto currentDefinitions = [&]() { std::vector<ScriptToolbarDefinition> result; for(size_t index = 0; index < m_scriptToolbars.Items().size(); ++index) result.push_back(m_scriptToolbars.Items()[index].definition); return result; };
	auto mainHasDefault = [&]() { TBBUTTON button = {}; return m_ScriptsToolbar.GetButtonCount() > 0 && m_ScriptsToolbar.GetButton(0, &button) && button.idCommand == ID_LAST_SCRIPT; };
	auto findBrokenScript = [&]() -> const ScriptDescriptor* {
		for(int index = 0; index < m_scripts.Menu().Count(); ++index) {
			const ScriptDescriptor& script = m_scripts.Menu().Item(index);
			if(!script.isFolder && script.relativePath == L"broken.js") return &script;
		}
		return NULL;
	};
	if(hotkeyExport)
	{
		wchar_t locale[LOCALE_NAME_MAX_LENGTH] = {};
		::GetEnvironmentVariableW(L"FBE_NEXT_TEST_HOTKEY_EXPORT_LOCALE", locale, _countof(locale));
		const bool russian = ::CompareStringOrdinal(locale, -1, L"ru-RU", -1, TRUE) == CSTR_EQUAL;
		_Settings.SetInterfaceLanguage(russian ? FBE_INTERFACE_LANGUAGE_RUSSIAN : FBE_INTERFACE_LANGUAGE_ENGLISH);
		FbePublishRuntimeLocaleName(russian ? L"ru-RU" : L"en-US");
		FbeResetRuntimeLocalization();
		std::vector<CHotkeysGroup> groups(_Settings.m_hotkey_groups);
		for(size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
			if(groups[groupIndex].m_reg_name == L"Scripts")
			{
				groups[groupIndex].m_hotkeys.push_back(CHotkey(L"runtime-export-script", L"Test <Fix> & \"Normalize\"", FCONTROL, ID_LAST_SCRIPT, VK_F9));
				groups[groupIndex].m_hotkeys.push_back(CHotkey(L"runtime-export-unassigned", L"Unassigned <ignore>", FCONTROL, ID_LAST_SCRIPT + 1, 0));
				break;
			}
		const FbeSettings::Hotkeys::ExportData data(FbeSettings::Hotkeys::BuildExportData(groups));
		const CString text(FbeSettings::Hotkeys::BuildTextExport(data));
		const CString html(FbeSettings::Hotkeys::BuildHtmlExport(data, russian ? L"ru-RU" : L"en-US"));
		WritePortableStateTestText(diagnosticsDirectory + L"hotkey-export.txt", CStringA(CW2A(text, CP_UTF8)));
		WritePortableStateTestText(diagnosticsDirectory + L"hotkey-export.html", CStringA(CW2A(html, CP_UTF8)));
		const CString title = russian ? CString(L"Горячие клавиши") : CString(L"Hotkeys");
		const CString command = russian ? CString(L"Команда") : CString(L"Command");
		const CString shortcut = russian ? CString(L"Горячая клавиша") : CString(L"Shortcut");
		const CString edit = russian ? CString(L"Редактировать") : CString(L"Edit");
		const CString merge = russian ? CString(L"Слить") : CString(L"Merge");
		const CString space = russian ? CString(L"Пробел") : CString(L"Space");
		const CString scripts = russian ? CString(L"Скрипты") : CString(L"Scripts");
		const CString plugins = russian ? CString(L"Плагины") : CString(L"Plugins");
		const bool header = text.Find(L"FictionBook Editor Next\r\n" + title + L"\r\n") == 0;
		const bool localizedNames = text.Find(L"\r\n" + edit + L"\r\n") >= 0 && text.Find(merge + L"\tAlt+Delete\r\n") >= 0;
		const bool scriptAndPluginGroups = text.Find(L"\r\n" + scripts + L"\r\n") >= 0 && text.Find(L"Test <Fix> & \"Normalize\"\tCtrl+F9\r\n") >= 0 && text.Find(L"\r\n" + plugins + L"\r\n") >= 0;
		const bool specialKeys = text.Find(space) >= 0 && text.Find(L"Backspace") >= 0 && text.Find(L"Delete") >= 0 && text.Find(L"Insert") >= 0 && text.Find(L"F2") >= 0 && text.Find(L"F7") >= 0 && text.Find(L"F8") >= 0;
		const bool tabs = text.Find(L"\t") >= 0 && text.Find(L"    ") < 0;
		const bool noEmptyGroups = text.Find(L"\r\n\r\n\r\n") < 0 && text.Find(L"Unassigned <ignore>") < 0;
		const bool htmlStructure = html.Find(L"<!doctype html>\r\n<html lang=\"") == 0 && html.Find(L"<meta charset=\"utf-8\">") >= 0 && html.Find(L"<table>") >= 0 && html.Find(L"<thead><tr><th>" + command) >= 0 && html.Find(L"<th>" + shortcut + L"</th>") >= 0 && html.Find(L"</html>") >= 0;
		const bool htmlLocalized = html.Find(L"<h2>" + title + L"</h2>") >= 0 && html.Find(L"<th colspan=\"2\">" + scripts + L"</th>") >= 0 && html.Find(L"<th colspan=\"2\">" + plugins + L"</th>") >= 0 && html.Find(space) >= 0;
		const bool htmlEscaping = html.Find(L"Test &lt;Fix&gt; &amp; &quot;Normalize&quot;") >= 0 && html.Find(L"Unassigned &lt;ignore&gt;") < 0;
		const bool sameData = data.size() > 0 && html.Find(L"<kbd>Ctrl+F9</kbd>") >= 0 && text.Find(L"Ctrl+F9") >= 0;
		const CString exportPath = reportPath + L".txt";
		bool atomicExport = FbeSettings::Hotkeys::WriteUtf8ExportFile(exportPath, L"original");
		HANDLE locked = ::CreateFileW(exportPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		atomicExport = atomicExport && locked != INVALID_HANDLE_VALUE && !FbeSettings::Hotkeys::WriteUtf8ExportFile(exportPath, L"replacement");
		if(locked != INVALID_HANDLE_VALUE)
		{
			char bytes[32] = {}; DWORD read = 0;
			atomicExport = atomicExport && ::ReadFile(locked, bytes, sizeof(bytes), &read, NULL) && read == 11 && memcmp(bytes + 3, "original", 8) == 0;
			::CloseHandle(locked);
		}
		atomicExport = atomicExport && FbeSettings::Hotkeys::WriteUtf8ExportFile(exportPath, L"replacement");
		::DeleteFileW(exportPath);
		const bool passed = atomicExport && header && localizedNames && scriptAndPluginGroups && specialKeys && tabs && noEmptyGroups && htmlStructure && htmlLocalized && htmlEscaping && sameData;
		CStringA report;
		report.Format("phase=hotkey-export\nlocale=%ls\nheader=%d\nlocalized-names=%d\nscripts-plugins=%d\nspecial-keys=%d\ntabs=%d\nno-empty-groups=%d\nhtml-structure=%d\nhtml-localized=%d\nhtml-escaping=%d\nsame-data=%d\nresult=%s\n",
			locale, header, localizedNames, scriptAndPluginGroups, specialKeys, tabs, noEmptyGroups, htmlStructure, htmlLocalized, htmlEscaping, sameData, passed ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report);
		PostMessage(WM_CLOSE);
		return;
	}	if(scriptLiveReloadRuntime)
	{
		::CreateDirectory(scriptsDirectory, NULL);
		const CString scriptPath = scriptsDirectory + L"live-reload.js";
		const bool documentReady = m_doc != NULL;
		const bool wroteFirst = WritePortableStateTestText(scriptPath, "function Run(){window.external.SetStatusBarText('version-1');}\n");
		if(documentReady && wroteFirst) m_doc->RunScript(scriptPath);
		const bool version1 = m_status_state.EffectiveMainText(false, false, CString()) == L"version-1";
		const bool wroteSecond = WritePortableStateTestText(scriptPath, "function Run(){window.external.SetStatusBarText('version-2');}\n");
		if(documentReady && wroteSecond) m_doc->RunScript(scriptPath);
		const bool version2 = m_status_state.EffectiveMainText(false, false, CString()) == L"version-2";
		CString cp1251Source = L"// \x041F\x0440\x0438\x043C\x0435\x0447\x0430\x043D\x0438\x0435: \x0438\x0441\x0445\x043E\x0434\x043D\x044B\x0439 \x0441\x043A\x0440\x0438\x043F\x0442 \x043D\x0430 \x0440\x0443\x0441\x0441\x043A\x043E\x043C \x044F\x0437\x044B\x043A\x0435.\n";
		for(int index = 0; index < 12; ++index) cp1251Source += L"// \x041F\x0440\x043E\x0432\x0435\x0440\x043A\x0430 \x043A\x043E\x0434\x0438\x0440\x043E\x0432\x043A\x0438 \x0434\x043B\x044F \x0438\x0441\x0442\x043E\x0440\x0438\x0447\x0435\x0441\x043A\x043E\x0433\x043E \x0441\x043A\x0440\x0438\x043F\x0442\x0430.\n";
		cp1251Source += L"function Run(){window.external.SetStatusBarText('\x041F\x0440\x0438\x0432\x0435\x0442 \x0438\x0437 CP1251');}\n";
		const CStringA cp1251Code(CW2A(cp1251Source, 1251));
		const bool wroteCp1251 = WritePortableStateTestText(scriptPath, cp1251Code);
		if(documentReady && wroteCp1251) m_doc->RunScript(scriptPath);
		const CString plainText = m_status_state.EffectiveMainText(false, false, CString());
		const bool plainCp1251 = plainText == L"\x041F\x0440\x0438\x0432\x0435\x0442 \x0438\x0437 CP1251";
		std::string cardScript = "/*<FBE-SCRIPT-CARD version=\"1\">\n{\"schema\":1,\"padding\":\"";
		cardScript.append(1500, 'x');
		cardScript += "\"}\n</FBE-SCRIPT-CARD>*/\n";
		cardScript += cp1251Code.GetString();
		const bool wroteCard = WritePortableStateTestText(scriptPath, cardScript.c_str());
		if(documentReady && wroteCard) m_doc->RunScript(scriptPath);
		const CString cardText = m_status_state.EffectiveMainText(false, false, CString());
		const bool cardCp1251 = cardText == L"\x041F\x0440\x0438\x0432\x0435\x0442 \x0438\x0437 CP1251";
		const bool passed = documentReady && wroteFirst && version1 && wroteSecond && version2 && wroteCp1251 && plainCp1251 && wroteCard && cardCp1251;
		CStringA report; report.Format("phase=script-live-reload\nversion-1=%d\nversion-2=%d\nplain-cp1251=%d\ncard-cp1251=%d\nplain-text=%s\ncard-text=%s\nresult=%s\n",
			version1, version2, plainCp1251, cardCp1251, static_cast<LPCSTR>(CW2A(plainText, CP_UTF8)),
			static_cast<LPCSTR>(CW2A(cardText, CP_UTF8)), passed ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report); PostMessage(WM_CLOSE); return;
	}
	if(scriptCardDialogRuntime)
	{
		FbePublishRuntimeLocaleName(L"ru-RU");
		FbeResetRuntimeLocalization();
		::CreateDirectory(scriptsDirectory, NULL);
		const CString missingPath = scriptsDirectory + L"card-missing.js";
		const CString cardPath = scriptsDirectory + L"card-present.js";
		const bool wroteMissing = WritePortableStateTestText(missingPath, "function Run(){}\n");
		ScriptDescriptor script = {};
		script.name = L"Card test";
		script.path = missingPath;
		script.relativePath = L"Card test.js";
		script.uid = L"card-test-missing";
		CString resourceRoot(scriptsDirectory);
		resourceRoot.TrimRight(L"\\/");
		const int lastSlash = max(resourceRoot.ReverseFind(L'\\'), resourceRoot.ReverseFind(L'/'));
		if(lastSlash >= 0) resourceRoot = resourceRoot.Left(lastSlash);
		int runCount = 0;
		auto openCard = [this, &script, &resourceRoot, &runCount]() {
			return m_script_card_window.ShowScript(m_hWnd, script, resourceRoot,
				[&runCount](const CString&) { ++runCount; });
		};
		auto awaitCard = [this]() {
			const DWORD deadline = ::GetTickCount() + 5000;
			while(m_script_card_window.IsLoading() && static_cast<LONG>(::GetTickCount() - deadline) < 0)
			{
				MSG message = {};
				while(::PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
				{
					::TranslateMessage(&message);
					::DispatchMessageW(&message);
				}
				::Sleep(10);
			}
			return !m_script_card_window.IsLoading();
		};
		const bool openedMissing = wroteMissing && openCard() && awaitCard();
		const HWND firstWindow = m_script_card_window.m_hWnd;
		const bool missingFallback = openedMissing &&
			m_script_card_window.BodyText().Find(FbeLoadRuntimeStringByKey(L"fbe.script_card.missing",
				L"Detailed information about this script is not available yet.")) >= 0;
		const std::string cardSource =
			"/*<FBE-SCRIPT-CARD version=\"1\">\n"
			"{\"schema\":1,\"title\":\"Card test\",\"summary\":\"Card runtime summary\","
			"\"description\":\"Card runtime description\",\"version\":\"1.0\","
			"\"scope\":\"Current document, including every section, annotation, image caption and nested body selected for processing by this script\",\"modifiesDocument\":false,\"requiresSelection\":false,"
			"\"usage\":[\"Open card\"],\"warnings\":[\"Review changes before running\"],\"dependencies\":[],"
			"\"settings\":[{\"title\":\"Mode\",\"name\":\"ModeCode\",\"value\":\"safe\","
			"\"expression\":\"+extracting(0)\",\"valueMeaning\":\"fallback value\",\"description\":\"Change mode\"},"
			"{\"name\":\"Second\",\"value\":false,\"expression\":\"false\",\"description\":\"Selection enabled: false \\u2014 \\u043d\\u0435\\u0442, true \\u2014 \\u0434\\u0430.\"},"
			"{\"name\":\"Empty\",\"value\":\"\",\"description\":\"Empty setting\"},"
			"{\"name\":\"Computed\",\"expression\":\"N100(extracting(0))\",\"description\":\"Computed setting\"},"
			"{\"name\":\"Count\",\"value\":42,\"description\":\"Numeric setting\"}],"
			"\"examples\":[{\"before\":\"abc\",\"after\":\"ABC\"}],"
			"\"undo\":{\"status\":\"explicit_calls_found\",\"description\":\"BeginUndoUnit/EndUndoUnit found\"},\"tags\":[\"case\"],"
			"\"authors\":[{\"name\":\"Tester\",\"role\":\"author\"},{\"name\":\"Second author\",\"role\":\"author\"},{\"name\":\"Helper\",\"role\":\"coauthor\"},{\"name\":\"Modifier\",\"role\":\"modifier\"},{\"name\":\"Another\",\"role\":\"modifier\"}],"
			"\"sourceRevision\":{\"algorithm\":\"sha256-exclude-fbe-card-v1\","
			"\"reviewStatus\":\"generated\","
			"\"sha256\":\"b93000a4f6aabe6d01c3db1f2b99000a48756d7181161e50dd4011758dc89c89\"}}"
			"\n</FBE-SCRIPT-CARD>*/\nfunction Run(){}\n";
		const bool wroteCard = WritePortableStateTestText(cardPath, cardSource.c_str());
		script.path = cardPath;
		script.uid = L"card-test-present";
		const bool openedCard = wroteCard && openCard() && awaitCard();
		const bool reused = openedCard && m_script_card_window.m_hWnd == firstWindow;
		const bool rendered = openedCard && m_script_card_window.BodyText().Find(L"Card runtime summary") >= 0;
		const bool structured = openedCard && m_script_card_window.VisualBlockCount() >= 9 &&
			m_script_card_window.HeaderRightText().Find(L"Helper") >= 0 &&
			m_script_card_window.HeaderRightText().Find(FbeLoadRuntimeStringByKey(L"fbe.script_card.role.coauthor", L"Co-author")) >= 0 &&
			m_script_card_window.HeaderRightText().Find(L"Tester — ") >= 0 &&
			m_script_card_window.HeaderRightText().Find(L"Second author — ") >= 0 &&
			m_script_card_window.HeaderRightText().Find(L"Modifier — ") >= 0 &&
			m_script_card_window.HeaderRightText().Find(L"Another — ") >= 0 &&
			m_script_card_window.BodyText().Find(L"Modifier") < 0 &&
			m_script_card_window.BodyText().Find(L"Review changes before running") >= 0;
		RECT originalCardBounds = {};
		::GetWindowRect(m_script_card_window.m_hWnd, &originalCardBounds);
		const int cardDpi = UiMetrics::DpiForWindow(m_script_card_window.m_hWnd);
		::SetWindowPos(m_script_card_window.m_hWnd, NULL, 0, 0,
			UiMetrics::ScaleForDpi(700, cardDpi), UiMetrics::ScaleForDpi(520, cardDpi),
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		const bool canScroll = m_script_card_window.CanScroll();
		const HWND viewport = m_script_card_window.ViewportWindow();
		::SendMessageW(viewport, WM_VSCROLL, SB_LINEDOWN, 0);
		const int linePosition = m_script_card_window.ScrollPosition();
		::SendMessageW(viewport, WM_VSCROLL, SB_PAGEDOWN, 0);
		const int pagePosition = m_script_card_window.ScrollPosition();
		::SendMessageW(viewport, WM_VSCROLL, SB_BOTTOM, 0);
		const int bottomPosition = m_script_card_window.ScrollPosition();
		const int thumbTarget = bottomPosition / 2;
		::SendMessageW(viewport, WM_VSCROLL, MAKEWPARAM(SB_THUMBPOSITION, thumbTarget), 0);
		const bool thumbPosition = thumbTarget > 0 && m_script_card_window.ScrollPosition() == thumbTarget;
		::SendMessageW(viewport, WM_VSCROLL, SB_BOTTOM, 0);
		HWND richBlock = ::GetWindow(viewport, GW_CHILD);
		if(richBlock) ::SendMessageW(richBlock, WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), 0);
		const int wheelPosition = m_script_card_window.ScrollPosition();
		if(richBlock) ::SendMessageW(richBlock, WM_KEYDOWN, VK_HOME, 0);
		const int homePosition = m_script_card_window.ScrollPosition();
		if(richBlock) ::SendMessageW(richBlock, WM_KEYDOWN, VK_END, 0);
		const int endPosition = m_script_card_window.ScrollPosition();
		if(richBlock) ::SendMessageW(richBlock, WM_KEYDOWN, VK_PRIOR, 0);
		const int pageUpPosition = m_script_card_window.ScrollPosition();
		const bool responsive = thumbPosition && canScroll && linePosition > 0 && pagePosition > linePosition &&
			bottomPosition >= pagePosition && wheelPosition < bottomPosition &&
			homePosition == 0 && endPosition == bottomPosition && pageUpPosition < endPosition;
		::SetWindowPos(m_script_card_window.m_hWnd, NULL, originalCardBounds.left, originalCardBounds.top,
			originalCardBounds.right - originalCardBounds.left, originalCardBounds.bottom - originalCardBounds.top,
			SWP_NOZORDER | SWP_NOACTIVATE);
		::SendMessageW(viewport, WM_VSCROLL, SB_TOP, 0);
		const bool optionalRendered = openedCard &&
			m_script_card_window.BodyText().Find(L"including every section, annotation, image caption and nested body") >= 0 &&
			m_script_card_window.BodyText().Find(L"Mode\r\nModeCode\r\nЗначение по умолчанию: safe") >= 0 &&
			m_script_card_window.BodyText().Find(L"Selection enabled\r\nSecond\r\nЗначение в исходном коде: Нет") >= 0 &&
			m_script_card_window.BodyText().Find(L"false — нет") < 0 &&
			m_script_card_window.BodyText().Find(L"Значение в исходном коде: Пустая строка") >= 0 &&
			m_script_card_window.BodyText().Find(L"Значение: Определяется при выполнении скрипта") >= 0 &&
			m_script_card_window.BodyText().Find(L"Значение в исходном коде: 42") >= 0 &&
			m_script_card_window.BodyText().Find(L"+extracting(") < 0 &&
			m_script_card_window.BodyText().Find(L"N100(") < 0 &&
			m_script_card_window.BodyText().Find(L"Параметры задаются в общих настройках скриптов") >= 0 &&
			m_script_card_window.BodyText().Find(L"\\u0434") < 0 &&
			m_script_card_window.BodyText().Find(L"abc → ABC") >= 0 &&
			m_script_card_window.BodyText().Find(L"Отмена изменений") < 0 &&
			m_script_card_window.BodyText().Find(L"BeginUndoUnit") < 0;
		auto layoutAtWidth = [&](int logicalWidth) {
			::SetWindowPos(m_script_card_window.m_hWnd, NULL, 0, 0,
				UiMetrics::ScaleForDpi(logicalWidth, cardDpi), UiMetrics::ScaleForDpi(520, cardDpi),
				SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
			return m_script_card_window.CharacteristicLayoutRows();
		};
		const int wideCharacteristics = layoutAtWidth(1200);
		HWND characteristicViewport = m_script_card_window.ViewportWindow();
		auto layoutAtViewportWidth = [&](int logicalWidth, UINT dpi) {
			::SetWindowPos(characteristicViewport, NULL, 0, 0,
				UiMetrics::ScaleForDpi(logicalWidth, dpi), UiMetrics::ScaleForDpi(100, dpi),
				SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
			::SendMessageW(characteristicViewport, WM_VSCROLL, SB_TOP, 0);
			::SendMessageW(characteristicViewport, WM_VSCROLL, SB_BOTTOM, 0);
			return m_script_card_window.CharacteristicLayoutRows();
		};
		const int pairedCharacteristics = layoutAtViewportWidth(420, cardDpi);
		// The dialog's minimum tracking width prevents a real 260 px window.
		// Exercise the narrow content viewport directly, as can occur in a host pane.
		const int narrowCharacteristics = layoutAtViewportWidth(260, cardDpi);
		const bool characteristicLayout = wideCharacteristics == 1 && pairedCharacteristics == 2 &&
			narrowCharacteristics == 3 &&
			(::GetWindowLongPtrW(m_script_card_window.ViewportWindow(), GWL_STYLE) & WS_HSCROLL) == 0;
		bool characteristicDpiMatrix = true;
		for(const UINT dpi : { 96U, 120U, 144U, 192U })
		{
			::SendMessageW(m_script_card_window.m_hWnd, WM_DPICHANGED, MAKELONG(dpi, dpi), 0);
			for(const int logicalWidth : { 1200, 420, 260 })
			{
				characteristicDpiMatrix = characteristicDpiMatrix &&
					layoutAtViewportWidth(logicalWidth, dpi) ==
					(logicalWidth == 1200 ? 1 : logicalWidth == 420 ? 2 : 3);
			}
		}
		::SendMessageW(m_script_card_window.m_hWnd, WM_DPICHANGED,
			MAKELONG(cardDpi, cardDpi), 0);
		::SetWindowPos(m_script_card_window.m_hWnd, NULL, originalCardBounds.left, originalCardBounds.top,
			originalCardBounds.right - originalCardBounds.left, originalCardBounds.bottom - originalCardBounds.top,
			SWP_NOZORDER | SWP_NOACTIVATE);
		const bool hashesHidden = openedCard && m_script_card_window.BodyText().Find(L"b93000a4f6") < 0;
		const bool originHidden = openedCard && m_script_card_window.OriginText().IsEmpty() &&
			m_script_card_window.StatusText().IsEmpty() &&
			!::IsWindowVisible(m_script_card_window.WarningWindow());
		const bool windowIcons = openedCard &&
			::SendMessageW(m_script_card_window.m_hWnd, WM_GETICON, ICON_SMALL, 0) != 0 &&
			::SendMessageW(m_script_card_window.m_hWnd, WM_GETICON, ICON_BIG, 0) != 0;
		HWND bodyWindow = m_script_card_window.GetDlgItem(IDC_SCRIPT_CARD_HEADER);
		CHARRANGE headingRange = { 0, 5 };
		::SendMessageW(bodyWindow, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&headingRange));
		CHARFORMAT2W headingFormat = {}; headingFormat.cbSize = sizeof(headingFormat);
		::SendMessageW(bodyWindow, EM_GETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&headingFormat));
		const bool boldHeading = (headingFormat.dwEffects & CFE_BOLD) != 0;
		const bool technicalAbsent = !::IsWindow(m_script_card_window.GetDlgItem(1703)) &&
			!::IsWindow(m_script_card_window.GetDlgItem(1704)) &&
			!::IsWindow(m_script_card_window.GetDlgItem(1705));
		auto clipboardText = [&]() -> CString {
			CString value;
			if(::OpenClipboard(m_script_card_window.m_hWnd)) {
				HANDLE data = ::GetClipboardData(CF_UNICODETEXT);
				if(data) { LPCWSTR text = static_cast<LPCWSTR>(::GlobalLock(data)); if(text) { value = text; ::GlobalUnlock(data); } }
				::CloseClipboard();
			}
			return value;
		};
		::SendMessageW(m_script_card_window.GetDlgItem(IDC_SCRIPT_CARD_COPY), BM_CLICK, 0, 0);
		const CString copied = clipboardText();
		const bool copiedDetails = copied.Find(L"Card test") >= 0 && copied.Find(L"Card runtime summary") >= 0 &&
			copied.Find(L"Tester — ") >= 0 && copied.Find(L"Second author — ") >= 0 &&
			copied.Find(L"Modifier — ") >= 0 && copied.Find(L"Another — ") >= 0 &&
			copied.Find(cardPath) >= 0 && copied.Find(L"b93000a4f6") < 0 &&
			copied.Find(L"Значение по умолчанию: safe") >= 0 && copied.Find(L"+extracting(") < 0;
		const bool uidUpdated = m_script_card_window.CurrentUid() == script.uid;
		const bool altered = WritePortableStateTestText(cardPath, (cardSource + " ").c_str());
		script.relativePath = L"Иллюстрации/Подраздел/card-present.js";
		const bool reopened = altered && openCard() && awaitCard();
		wchar_t header[512] = {};
		::GetWindowTextW(m_script_card_window.GetDlgItem(IDC_SCRIPT_CARD_HEADER), header, _countof(header));
		const bool categoryShown = CString(header).Find(L"Иллюстрации/Подраздел") >= 0;
		const bool changedWarning = reopened && m_script_card_window.StatusText() ==
			FbeLoadRuntimeStringByKey(L"fbe.script_card.changed", L"⚠ Code changed. Description may be outdated") &&
			::IsWindowVisible(m_script_card_window.WarningWindow());
		auto checkReviewOrigin = [&](const char* review) {
			std::string source = cardSource;
			const std::string marker = "\"reviewStatus\":\"generated\"";
			const size_t position = source.find(marker);
			if(position == std::string::npos) return false;
			source.replace(position, marker.size(), std::string("\"reviewStatus\":\"") + review + "\"");
			return WritePortableStateTestText(cardPath, source.c_str()) && openCard() && awaitCard() &&
				m_script_card_window.OriginText().IsEmpty() && m_script_card_window.StatusText().IsEmpty() &&
				!::IsWindowVisible(m_script_card_window.WarningWindow());
		};
		const bool originStates = checkReviewOrigin("reviewed") && checkReviewOrigin("unknown");
		const bool buttons = ::IsWindow(m_script_card_window.GetDlgItem(IDC_SCRIPT_CARD_RUN)) &&
			::IsWindow(m_script_card_window.GetDlgItem(IDC_SCRIPT_CARD_OPEN)) &&
			::IsWindow(m_script_card_window.GetDlgItem(IDC_SCRIPT_CARD_COPY));
		m_script_card_window.ShowWindow(SW_HIDE);
		_Settings.SetScriptsFolder(scriptsDirectory, true);
		_Settings.SetDocumentTreeScripts(true, true);
		const bool initialized = InitializeScripts();
		RefreshNavigationScriptTree();
		CTreeView& tree = m_document_tree.m_tree.m_tree;
		if(!tree.IsScriptMode()) m_document_tree.m_tree.ToggleScriptMode();
		tree.SetScriptFilter(CString());
		m_document_tree.ShowWindow(SW_SHOW);
		m_document_tree.m_tree.ShowWindow(SW_SHOW);
		tree.ShowWindow(SW_SHOW);
		HTREEITEM selectedItem = tree.FindScriptTreeItem(L"card-missing.js");
		HTREEITEM infoItem = tree.FindScriptTreeItem(L"card-present.js");
		CString infoUid;
		for(int index = 0; index < m_scripts.Menu().Count(); ++index)
			if(m_scripts.Menu().Item(index).relativePath == L"card-present.js") infoUid = m_scripts.Menu().Item(index).uid;
		::SetWindowPos(tree.m_hWnd, NULL, 0, 0, 400, 300, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		if(infoItem != NULL) tree.EnsureVisible(infoItem);
		if(selectedItem != NULL) tree.SelectItem(selectedItem);
		RECT infoRow = {}, treeClient = {};
		const bool rowVisible = infoItem != NULL && tree.GetItemRect(infoItem, &infoRow, FALSE);
		tree.GetClientRect(&treeClient);
		const int infoX = treeClient.right - UiMetrics::ScaleForDpi(15, UiMetrics::DpiForWindow(tree.m_hWnd));
		const int infoY = (infoRow.top + infoRow.bottom) / 2;
		POINT priorCursor = {};
		::GetCursorPos(&priorCursor);
		POINT infoPoint = { infoX, infoY };
		tree.ClientToScreen(&infoPoint);
		if(rowVisible) {
			::SetCursorPos(infoPoint.x, infoPoint.y);
			::SendMessageW(tree.m_hWnd, WM_MOUSEMOVE, 0, MAKELPARAM(infoX, infoY));
		}
		POINT observedCursor = {};
		::GetCursorPos(&observedCursor);
		const bool cursorPlaced = observedCursor.x == infoPoint.x && observedCursor.y == infoPoint.y;
		const bool previewDelayed = rowVisible && !tree.ScriptPreviewVisible();
		// Isolated runtime desktops can reject SetCursorPos. Advance the same hover timer
		// after its delay before dispatching queued mouse-leave messages in that case.
		if(rowVisible && (!cursorPlaced || ::WindowFromPoint(observedCursor) != tree.m_hWnd)) {
			::Sleep(470);
			::SendMessageW(tree.m_hWnd, WM_MOUSEMOVE, 0, MAKELPARAM(infoX, infoY));
			::SendMessageW(tree.m_hWnd, WM_TIMER, 0x53435001, 0);
			if(!tree.ScriptPreviewVisible()) {
				BOOL handled = FALSE;
				tree.OnScriptPreviewTimer(WM_TIMER, 0x53435001, 0, handled);
			}
		}
		const DWORD previewDeadline = ::GetTickCount() + 900;
		while(rowVisible && !tree.ScriptPreviewVisible() && ::GetTickCount() < previewDeadline) {
			MSG message = {};
			while(::PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
				::TranslateMessage(&message);
				::DispatchMessageW(&message);
			}
			::Sleep(10);
		}
		const bool previewShown = tree.ScriptPreviewVisible() && tree.ScriptPreviewUid() == infoUid &&
			tree.ScriptPreviewText().Find(L"Card runtime summary") >= 0;
		const HWND previewWindow = tree.ScriptPreviewWindow();
		const bool previewReusable = previewWindow != NULL && ::IsWindow(previewWindow);
		RECT previewBounds = {};
		const bool previewBoundsValid = previewReusable && ::GetWindowRect(previewWindow, &previewBounds);
		const int previewDpi = UiMetrics::DpiForWindow(tree.m_hWnd);
		const POINT bridgePoint = { infoPoint.x,
			previewBounds.top > infoPoint.y ? previewBounds.top - UiMetrics::ScaleForDpi(2, previewDpi) :
				previewBounds.bottom + UiMetrics::ScaleForDpi(2, previewDpi) };
		const bool previewBridge = previewBoundsValid && tree.ScriptPreviewTransitionContains(bridgePoint);
		const POINT popupPoint = { previewBounds.right - UiMetrics::ScaleForDpi(20, previewDpi),
			previewBounds.top + UiMetrics::ScaleForDpi(23, previewDpi) };
		const POINT emptyPoint = { previewBounds.left - UiMetrics::ScaleForDpi(20, previewDpi),
			(previewBounds.top + previewBounds.bottom) / 2 };
		auto pumpPreviewFor = [](DWORD milliseconds) {
			const DWORD deadline = ::GetTickCount() + milliseconds;
			while(::GetTickCount() < deadline) {
				MSG message = {};
				while(::PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
					::TranslateMessage(&message);
					::DispatchMessageW(&message);
				}
				::Sleep(10);
			}
		};
		::SetCursorPos(bridgePoint.x, bridgePoint.y);
		POINT bridgeCursor = {};
		::GetCursorPos(&bridgeCursor);
		::SendMessageW(tree.m_hWnd, WM_MOUSELEAVE, 0, 0);
		pumpPreviewFor(320);
		const bool previewBridgeTimer = previewBridge && bridgeCursor.x == bridgePoint.x &&
			bridgeCursor.y == bridgePoint.y && tree.ScriptPreviewVisible() && tree.ScriptPreviewUid() == infoUid;
		::SetCursorPos(emptyPoint.x, emptyPoint.y);
		pumpPreviewFor(320);
		POINT exitCursor = {};
		::GetCursorPos(&exitCursor);
		const bool previewBridgeExit = previewBridgeTimer && exitCursor.x == emptyPoint.x &&
			exitCursor.y == emptyPoint.y && !tree.ScriptPreviewVisible();
		::SetCursorPos(infoPoint.x, infoPoint.y);
		::SendMessageW(tree.m_hWnd, WM_MOUSEMOVE, 0, MAKELPARAM(infoX, infoY));
		::Sleep(470);
		::SendMessageW(tree.m_hWnd, WM_TIMER, 0x53435001, 0);
		::SetCursorPos(bridgePoint.x, bridgePoint.y);
		::SendMessageW(tree.m_hWnd, WM_MOUSELEAVE, 0, 0);
		::SetCursorPos(popupPoint.x, popupPoint.y);
		::SendMessageW(previewWindow, WM_MOUSEMOVE, 0, MAKELPARAM(popupPoint.x - previewBounds.left, popupPoint.y - previewBounds.top));
		const bool previewPointerLink = previewBridgeExit && tree.ScriptPreviewVisible() &&
			tree.ScriptPreviewUid() == infoUid && ::WindowFromPoint(popupPoint) == previewWindow;
		::SetCursorPos(emptyPoint.x, emptyPoint.y);
		::SendMessageW(previewWindow, WM_MOUSELEAVE, 0, 0);
		pumpPreviewFor(320);
		::GetCursorPos(&exitCursor);
		const bool previewPointerExit = previewPointerLink && exitCursor.x == emptyPoint.x &&
			exitCursor.y == emptyPoint.y && !tree.ScriptPreviewVisible();
		::SetCursorPos(infoPoint.x, infoPoint.y);
		::SendMessageW(tree.m_hWnd, WM_MOUSEMOVE, 0, MAKELPARAM(infoX, infoY));
		::Sleep(470);
		::SendMessageW(tree.m_hWnd, WM_TIMER, 0x53435001, 0);
		const bool previewReopened = tree.ScriptPreviewVisible() && tree.ScriptPreviewUid() == infoUid;
		RECT neighborRow = {};
		const bool neighborVisible = selectedItem != NULL && tree.GetItemRect(selectedItem, &neighborRow, FALSE);
		const int neighborY = (neighborRow.top + neighborRow.bottom) / 2;
		POINT neighborPoint = { infoX, neighborY };
		tree.ClientToScreen(&neighborPoint);
		if(neighborVisible) {
			::SetCursorPos(neighborPoint.x, neighborPoint.y);
			::SendMessageW(tree.m_hWnd, WM_MOUSEMOVE, 0, MAKELPARAM(infoX, neighborY));
			::SetCursorPos(bridgePoint.x, bridgePoint.y);
			::SendMessageW(tree.m_hWnd, WM_MOUSELEAVE, 0, 0);
			::Sleep(470);
			::SendMessageW(tree.m_hWnd, WM_TIMER, 0x53435001, 0);
			::SetCursorPos(popupPoint.x, popupPoint.y);
			::SendMessageW(previewWindow, WM_MOUSEMOVE, 0, MAKELPARAM(popupPoint.x - previewBounds.left, popupPoint.y - previewBounds.top));
		}
		const bool previewPointerCrossing = previewReopened && neighborVisible && tree.ScriptPreviewVisible() &&
			tree.ScriptPreviewUid() == infoUid && tree.ScriptPreviewWindow() == previewWindow;
		BOOL activationHandled = FALSE;
		OnCaretMarkerActivation(WM_ACTIVATEAPP, FALSE, 0, activationHandled);
		const bool previewDeactivated = previewPointerCrossing && !tree.ScriptPreviewVisible();
		::SetCursorPos(infoPoint.x, infoPoint.y);
		::SendMessageW(tree.m_hWnd, WM_MOUSEMOVE, 0, MAKELPARAM(infoX, infoY));
		::Sleep(470);
		::SendMessageW(tree.m_hWnd, WM_TIMER, 0x53435001, 0);
		if(neighborVisible) ::SendMessageW(tree.m_hWnd, WM_MOUSEMOVE, 0, MAKELPARAM(infoX, neighborY));
		const bool previewCrossing = neighborVisible && tree.ScriptPreviewVisible() &&
			tree.ScriptPreviewUid() == infoUid && tree.ScriptPreviewWindow() == previewWindow;
		if(neighborVisible) ::SendMessageW(tree.m_hWnd, WM_MOUSEMOVE, 0, MAKELPARAM(infoX, infoY));
		if(neighborVisible) ::SendMessageW(tree.m_hWnd, WM_MOUSEMOVE, 0, MAKELPARAM(infoX, neighborY));
		::Sleep(470);
		if(neighborVisible) ::SendMessageW(tree.m_hWnd, WM_TIMER, 0x53435001, 0);
		const CString neighborUid = tree.ScriptPreviewUid();
		const bool previewSwitch = previewCrossing && tree.ScriptPreviewVisible() &&
			neighborUid != infoUid && tree.ScriptPreviewWindow() == previewWindow;
		if(previewSwitch) ::SendMessageW(previewWindow, WM_LBUTTONUP, 0,
			MAKELPARAM(UiMetrics::ScaleForDpi(35, previewDpi), UiMetrics::ScaleForDpi(48, previewDpi)));
		const bool previewSurface = previewSwitch && awaitCard() &&
			m_script_card_window.CurrentUid() == neighborUid &&
			tree.GetSelectedItem() == selectedItem && runCount == 0;
		if(rowVisible) ::SendMessageW(tree.m_hWnd, WM_MOUSEMOVE, 0, MAKELPARAM(infoX, infoY));
		::Sleep(470);
		if(rowVisible) ::SendMessageW(tree.m_hWnd, WM_TIMER, 0x53435001, 0);
		const int linkX = previewBounds.right - previewBounds.left - UiMetrics::ScaleForDpi(20, previewDpi);
		const int linkY = UiMetrics::ScaleForDpi(23, previewDpi);
		if(previewBoundsValid) ::SendMessageW(previewWindow, WM_LBUTTONUP, 0, MAKELPARAM(linkX, linkY));
		const bool previewLink = previewBoundsValid && awaitCard() &&
			m_script_card_window.CurrentUid() == infoUid && !tree.ScriptPreviewVisible();
		::SetCursorPos(priorCursor.x, priorCursor.y);
		if(rowVisible) {
			::SendMessageW(tree.m_hWnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(infoX, infoY));
			::SendMessageW(tree.m_hWnd, WM_LBUTTONUP, 0, MAKELPARAM(infoX, infoY));
		}
		const bool infoOpened = initialized && rowVisible && awaitCard() &&
			m_script_card_window.CurrentUid() == infoUid && tree.GetSelectedItem() == selectedItem &&
			m_script_card_window.m_hWnd == firstWindow;
		if(rowVisible) ::SendMessageW(tree.m_hWnd, WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(infoX, infoY));
		const bool infoDoubleClickSafe = runCount == 0;
		if(infoItem != NULL) tree.SelectItem(infoItem);
		const bool contextOpened = infoItem != NULL && tree.ExecuteScriptPopupCommand(NavigationPopupAboutScript) &&
			awaitCard() && m_script_card_window.CurrentUid() == infoUid && m_script_card_window.m_hWnd == firstWindow;
		if(selectedItem != NULL) tree.SetItemState(selectedItem, TVIS_SELECTED, TVIS_SELECTED);
		const bool multiCardDisabled = selectedItem != NULL && !tree.ExecuteScriptPopupCommand(NavigationPopupAboutScript) &&
			m_script_card_window.CurrentUid() == infoUid;
		wchar_t artifactDirectory[MAX_PATH] = {};
		const DWORD artifactLength = ::GetEnvironmentVariableW(L"FBE_NEXT_TEST_ARTIFACT_DIR", artifactDirectory, _countof(artifactDirectory));
		auto captureWindow = [&](HWND window, LPCWSTR filename) -> bool {
			if(artifactLength == 0) return true;
			if(artifactLength >= _countof(artifactDirectory) || !::IsWindow(window)) return false;
			RECT bounds = {};
			if(!::GetWindowRect(window, &bounds)) return false;
			const int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
			if(width < 100 || height < 20) return false;
			BITMAPINFO info = {};
			info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			info.bmiHeader.biWidth = width;
			info.bmiHeader.biHeight = -height;
			info.bmiHeader.biPlanes = 1;
			info.bmiHeader.biBitCount = 32;
			void* pixels = NULL;
			HBITMAP bitmap = ::CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &pixels, NULL, 0);
			HDC memory = bitmap ? ::CreateCompatibleDC(NULL) : NULL;
			HGDIOBJ previous = memory ? ::SelectObject(memory, bitmap) : NULL;
			bool saved = false;
			if(pixels && previous && ::PrintWindow(window, memory, 0)) {
				BITMAPFILEHEADER header = {};
				header.bfType = 0x4d42;
				header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
				header.bfSize = header.bfOffBits + width * height * 4;
				const CString path = CString(artifactDirectory) + L"\\" + filename;
				HANDLE file = ::CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				if(file != INVALID_HANDLE_VALUE) {
					DWORD written = 0;
					saved = ::WriteFile(file, &header, sizeof(header), &written, NULL) && written == sizeof(header) &&
						::WriteFile(file, &info.bmiHeader, sizeof(info.bmiHeader), &written, NULL) && written == sizeof(info.bmiHeader) &&
						::WriteFile(file, pixels, width * height * 4, &written, NULL) && written == static_cast<DWORD>(width * height * 4);
					::CloseHandle(file);
				}
			}
			if(previous) ::SelectObject(memory, previous);
			if(memory) ::DeleteDC(memory);
			if(bitmap) ::DeleteObject(bitmap);
			return saved;
		};
		const bool cardScreenshot = captureWindow(m_script_card_window.m_hWnd, L"script-card.bmp");
		const bool treeScreenshot = captureWindow(tree.m_hWnd, L"script-tree-info.bmp");
		const InterfaceTheme originalTheme = ThemeManager::GetSelectedTheme();
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		::RedrawWindow(tree.m_hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
		::RedrawWindow(m_script_card_window.m_hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		const bool darkCardScreenshot = captureWindow(m_script_card_window.m_hWnd, L"script-card-dark.bmp");
		const bool darkTreeScreenshot = captureWindow(tree.m_hWnd, L"script-tree-info-dark.bmp");
		ThemeManager::SetSelectedTheme(originalTheme);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		const bool infoUidMatched = m_script_card_window.CurrentUid() == infoUid;
		const bool infoSelectionMatched = tree.GetSelectedItem() == selectedItem;
		const int infoRunCount = runCount;
		int commentRunCount = 0;
		wchar_t executable[MAX_PATH] = {};
		::GetModuleFileNameW(NULL, executable, _countof(executable));
		CString runtimePath(executable);
		runtimePath = runtimePath.Left(runtimePath.ReverseFind(L'\\'));
		const CString commentPath = runtimePath + L"\\Scripts\\03_Комментарии\\01_Добавить комментарий, остаться на его ссылке.js";
		const ScriptCards::Card commentCard = ScriptCards::LoadFile(commentPath);
		ScriptDescriptor comment = {};
		comment.name = L"Добавить комментарий, остаться на его ссылке";
		comment.path = commentPath;
		comment.relativePath = L"03_Комментарии/01_Добавить комментарий, остаться на его ссылке.js";
		comment.uid = L"comment-card-visual";
		const bool commentOpened = m_script_card_window.ShowScript(m_hWnd, comment, runtimePath,
			[&commentRunCount](const CString&) { ++commentRunCount; }) && awaitCard();
		const bool commentContent = commentOpened && commentCard.state == ScriptCards::State::Current &&
			commentCard.reviewStatus == L"generated" && commentCard.settingRows.size() == 11 &&
			commentCard.settingRows.front().title == L"Шаблон ID комментария" &&
			commentCard.warnings.size() == 1 && commentCard.undo.Find(L"BeginUndoUnit") < 0 &&
			m_script_card_window.HeaderRightText().Find(L"Sclex — Автор") >= 0 &&
			m_script_card_window.BodyText().Find(L"InsideTag") < 0 &&
			m_script_card_window.BodyText().Find(L"Пустая строка") >= 0 &&
			m_script_card_window.BodyText().Find(L"Значение литерала/резервное значение") < 0 &&
			m_script_card_window.BodyText().Find(L"Запуск внешнего приложения") < 0 &&
			m_script_card_window.StatusText().IsEmpty() &&
			!::IsWindowVisible(m_script_card_window.WarningWindow());
		::SetWindowPos(m_script_card_window.m_hWnd, NULL, 0, 0,
			UiMetrics::ScaleForDpi(620, cardDpi), UiMetrics::ScaleForDpi(460, cardDpi),
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		const bool commentNarrow = m_script_card_window.CanScroll() &&
			captureWindow(m_script_card_window.m_hWnd, L"comment-card-narrow.bmp");
		::SendMessageW(m_script_card_window.ViewportWindow(), WM_VSCROLL, SB_BOTTOM, 0);
		const bool commentSettings = m_script_card_window.ScrollPosition() > 0 &&
			captureWindow(m_script_card_window.m_hWnd, L"comment-card-settings.bmp");
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		const bool commentDark = captureWindow(m_script_card_window.m_hWnd, L"comment-card-settings-dark.bmp");
		ThemeManager::SetSelectedTheme(originalTheme);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		const CString alteredComment = scriptsDirectory + L"comment-changed.js";
		const bool commentCopied = ::CopyFileW(commentPath, alteredComment, FALSE) != FALSE;
		bool commentAltered = false;
		if(commentCopied) {
			HANDLE file = ::CreateFileW(alteredComment, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if(file != INVALID_HANDLE_VALUE) { DWORD written = 0; commentAltered = ::WriteFile(file, " ", 1, &written, NULL) && written == 1; ::CloseHandle(file); }
		}
		comment.path = alteredComment;
		const bool commentReopened = commentAltered && m_script_card_window.ShowScript(m_hWnd, comment, runtimePath,
			[&commentRunCount](const CString&) { ++commentRunCount; }) && awaitCard();
		RECT warningBounds = {}, copyBounds = {};
		::GetWindowRect(m_script_card_window.WarningWindow(), &warningBounds);
		::GetWindowRect(m_script_card_window.GetDlgItem(IDC_SCRIPT_CARD_COPY), &copyBounds);
		const bool commentWarning = commentReopened && ::IsWindowVisible(m_script_card_window.WarningWindow()) &&
			warningBounds.right <= copyBounds.left &&
			m_script_card_window.WarningText() == L"⚠ Код изменён. Описание может быть устаревшим" &&
			captureWindow(m_script_card_window.m_hWnd, L"comment-card-changed.bmp");
		TOOLINFOW warningTip = { sizeof(warningTip) };
		wchar_t warningTipText[160] = {};
		warningTip.hwnd = m_script_card_window.m_hWnd;
		warningTip.uId = reinterpret_cast<UINT_PTR>(m_script_card_window.WarningWindow());
		warningTip.lpszText = warningTipText;
		::SendMessageW(m_script_card_window.WarningTooltipWindow(), TTM_GETTEXTW, 0, reinterpret_cast<LPARAM>(&warningTip));
		const bool commentTooltip = CString(warningTipText) == m_script_card_window.WarningText();
		::SetWindowPos(m_script_card_window.m_hWnd, NULL, 0, 0,
			UiMetrics::ScaleForDpi(1100, cardDpi), UiMetrics::ScaleForDpi(460, cardDpi),
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		wchar_t warningLabel[160] = {};
		::GetWindowTextW(m_script_card_window.WarningWindow(), warningLabel, _countof(warningLabel));
		const bool commentWide = CString(warningLabel) == m_script_card_window.WarningText() &&
			captureWindow(m_script_card_window.m_hWnd, L"comment-card-changed-wide.bmp");
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		const bool commentChangedDark = ::IsWindowVisible(m_script_card_window.WarningWindow()) &&
			captureWindow(m_script_card_window.m_hWnd, L"comment-card-changed-dark.bmp");
		ThemeManager::SetSelectedTheme(originalTheme);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		if(selectedItem != NULL) tree.EnsureVisible(selectedItem);
		RECT missingRow = {};
		const bool missingRowVisible = selectedItem != NULL && tree.GetItemRect(selectedItem, &missingRow, FALSE);
		if(missingRowVisible) {
			RECT missingClient = {};
			tree.GetClientRect(&missingClient);
			const int missingX = missingClient.right - UiMetrics::ScaleForDpi(15, UiMetrics::DpiForWindow(tree.m_hWnd));
			const int missingY = (missingRow.top + missingRow.bottom) / 2;
			POINT missingPoint = { missingX, missingY };
			tree.ClientToScreen(&missingPoint);
			::SetCursorPos(missingPoint.x, missingPoint.y);
			::SendMessageW(tree.m_hWnd, WM_MOUSELEAVE, 0, 0);
			::SendMessageW(tree.m_hWnd, WM_MOUSEMOVE, 0, MAKELPARAM(missingX, missingY));
			::SendMessageW(tree.m_hWnd, WM_TIMER, 0x53435001, 0);
		}
		const bool previewFallback = missingRowVisible && tree.ScriptPreviewVisible() &&
			tree.ScriptPreviewText() == FbeLoadRuntimeStringByKey(L"fbe.script_card.preview_empty", L"No description is available yet.") &&
			tree.ScriptPreviewWindow() == previewWindow;
		const bool fallbackVisible = tree.ScriptPreviewVisible();
		const bool fallbackWindow = tree.ScriptPreviewWindow() == previewWindow;
		const bool fallbackText = tree.ScriptPreviewText() == FbeLoadRuntimeStringByKey(L"fbe.script_card.preview_empty", L"No description is available yet.");
		::SendMessageW(tree.m_hWnd, WM_VSCROLL, SB_LINEDOWN, 0);
		const bool previewScrollHidden = !tree.ScriptPreviewVisible();
		const CString standardPath = runtimePath + L"\\Scripts\\21_Поиск и замена\\36_Стандартизация символов.js";
		const ScriptCards::Card standardCard = ScriptCards::LoadFile(standardPath);
		ScriptDescriptor standard = {};
		standard.name = L"Стандартизация символов";
		standard.path = standardPath;
		standard.relativePath = L"21_Поиск и замена/36_Стандартизация символов.js";
		standard.uid = L"standard-card-runtime";
		const bool standardOpened = m_script_card_window.ShowScript(m_hWnd, standard, runtimePath,
			[&commentRunCount](const CString&) { ++commentRunCount; }) && awaitCard();
		const bool standardValues = standardOpened && !standardCard.settingRows.empty() &&
			standardCard.settingRows.front().title == L"Вид апострофа" &&
			standardCard.settingRows.front().value == L"1" &&
			standardCard.settingRows.front().defaultValue && standardCard.settingRows.front().sharedSetting &&
			m_script_card_window.BodyText().Find(L"Значение по умолчанию: 1") >= 0 &&
			m_script_card_window.BodyText().Find(L"+extracting(") < 0 &&
			m_script_card_window.BodyText().Find(L"N100(") < 0;
		wchar_t formattedCategory[512] = {};
		::GetWindowTextW(m_script_card_window.GetDlgItem(IDC_SCRIPT_CARD_HEADER), formattedCategory, _countof(formattedCategory));
		const bool standardCategory = CString(formattedCategory).Find(L"Поиск и замена") >= 0 &&
			CString(formattedCategory).Find(L"21_") < 0;
		bool cleaningCategory = true;
		bool cleaningValues = true;
		for(const wchar_t* filename : { L"01_Генеральная уборка.js", L"02_Генеральная уборка (версия Александра Ка).js" })
		{
			const CString path = runtimePath + L"\\Scripts\\06_Чистка\\" + filename;
			const ScriptCards::Card parsed = ScriptCards::LoadFile(path);
			ScriptDescriptor cleaning = {};
			cleaning.name = L"Генеральная уборка";
			cleaning.path = path;
			cleaning.relativePath = CString(L"06_Чистка/") + filename;
			cleaning.uid = filename;
			const bool shown = m_script_card_window.ShowScript(m_hWnd, cleaning, runtimePath,
				[&commentRunCount](const CString&) { ++commentRunCount; }) && awaitCard();
			::GetWindowTextW(m_script_card_window.GetDlgItem(IDC_SCRIPT_CARD_HEADER), formattedCategory, _countof(formattedCategory));
			cleaningCategory = cleaningCategory && CString(formattedCategory).Find(L"Чистка") >= 0 &&
				CString(formattedCategory).Find(L"06_") < 0;
			cleaningValues = cleaningValues && shown && !parsed.settingRows.empty() &&
				((wcscmp(filename, L"01_Генеральная уборка.js") == 0 &&
					parsed.settingRows.front().valueKind == ScriptCards::Setting::ValueKind::Boolean &&
					m_script_card_window.BodyText().Find(L"Значение в исходном коде: Да") >= 0) ||
					(wcscmp(filename, L"01_Генеральная уборка.js") != 0 &&
					parsed.settingRows.front().sharedSetting && parsed.settingRows.front().defaultValue &&
					m_script_card_window.BodyText().Find(L"Значение по умолчанию: 0") >= 0));
			if(shown && wcscmp(filename, L"01_Генеральная уборка.js") == 0)
			{
				::SendMessageW(m_script_card_window.ViewportWindow(), WM_VSCROLL, SB_BOTTOM, 0);
				cleaningValues = cleaningValues && captureWindow(m_script_card_window.m_hWnd, L"cleaning-card-settings.bmp");
			}
		}
		for(const wchar_t* filename : { L"01_Генеральная уборка.js", L"09_Генеральная уборка_(ИНОСТРАНКА).js" })
		{
			const CString path = runtimePath + L"\\Scripts\\06_Чистка\\" + filename;
			const ScriptCards::Card parsed = ScriptCards::LoadFile(path);
			const wchar_t* expectedVersion = wcscmp(filename, L"01_Генеральная уборка.js") == 0 ? L"2.10" : L"2.11";
			ScriptDescriptor cleaning = {};
			cleaning.name = L"Генеральная уборка";
			cleaning.path = path;
			cleaning.relativePath = CString(L"06_Чистка/") + filename;
			cleaning.uid = filename;
			const bool shown = m_script_card_window.ShowScript(m_hWnd, cleaning, runtimePath,
				[&commentRunCount](const CString&) { ++commentRunCount; }) && awaitCard();
			wchar_t cleaningHeader[1024] = {};
			::GetWindowTextW(m_script_card_window.GetDlgItem(IDC_SCRIPT_CARD_HEADER), cleaningHeader, _countof(cleaningHeader));
			cleaningValues = cleaningValues && shown && parsed.state == ScriptCards::State::Current &&
				parsed.version == expectedVersion && !parsed.changelog.empty() &&
				CString(cleaningHeader).Find(expectedVersion) >= 0 && m_script_card_window.BodyText().Find(L"правило №67") >= 0;
		}
		ScriptDescriptor longCard = {};
		longCard.name = L"Проверить правильность нумерации заголовков";
		longCard.relativePath = L"13_Заголовки, подзаголовки, пустые строки/615_Проверить правильность нумерации заголовков.js";
		longCard.path = runtimePath + L"\\Scripts\\" + longCard.relativePath;
		longCard.uid = L"long-heading-card";
		const bool longCardOpened = m_script_card_window.ShowScript(m_hWnd, longCard, runtimePath,
			[&commentRunCount](const CString&) { ++commentRunCount; }) && awaitCard();
		if (longCardOpened) ::SendMessageW(m_script_card_window.ViewportWindow(), WM_VSCROLL, SB_BOTTOM, 0);
		const bool longSettingsFit = longCardOpened && m_script_card_window.CanScroll() && m_script_card_window.SettingsLastLineFits();
		ScriptDescriptor symbols = {};
		symbols.name = L"Спецсимволы";
		symbols.path = runtimePath + L"\\Scripts\\07_Символы\\01_Спецсимволы.js";
		symbols.relativePath = L"07_Символы/01_Спецсимволы.js";
		symbols.uid = L"symbols-category-runtime";
		const bool symbolsOpened = m_script_card_window.ShowScript(m_hWnd, symbols, runtimePath,
			[&commentRunCount](const CString&) { ++commentRunCount; }) && awaitCard();
		::GetWindowTextW(m_script_card_window.GetDlgItem(IDC_SCRIPT_CARD_HEADER), formattedCategory, _countof(formattedCategory));
		const bool categoryFormatted = standardCategory && cleaningCategory && symbolsOpened &&
			CString(formattedCategory).Find(L"Символы") >= 0 && CString(formattedCategory).Find(L"07_") < 0;
		// Metadata-only variants keep the executable hash and exercise rendering
		// without executing any bundled script.
		std::string semanticSource = cardSource;
		const size_t authorsBegin = semanticSource.find("\"authors\":[");
		const size_t authorsEnd = semanticSource.find("],", authorsBegin) + 2;
		semanticSource.replace(authorsBegin, authorsEnd - authorsBegin,
			"\"authors\":[{\"name\":\"Helper\",\"role\":\"modifier\"},{\"name\":\"Helper\",\"role\":\"contributor\"},{\"role\":\"unknown\"}],");
		auto showSemantic = [&](const std::string& source) {
			return WritePortableStateTestText(cardPath, source.c_str()) &&
				m_script_card_window.ShowScript(m_hWnd, script, resourceRoot,
					[&runCount](const CString&) { ++runCount; }) && awaitCard();
		};
		const bool unknownShown = showSemantic(semanticSource) &&
			m_script_card_window.HeaderRightText().Find(L"Автор не установлен") >= 0 &&
			m_script_card_window.HeaderRightText().Find(L"Helper") == m_script_card_window.HeaderRightText().ReverseFind(L'H') &&
			m_script_card_window.HeaderRightText().Find(L"Доработка, Участник разработки") >= 0 &&
			m_script_card_window.BodyText().Find(L"BeginUndoUnit") < 0;
		const size_t undoBegin = semanticSource.find("\"undo\":{");
		const size_t undoEnd = semanticSource.find("},", undoBegin) + 2;
		semanticSource.replace(undoBegin, undoEnd - undoBegin,
			"\"undo\":{\"status\":\"explicit_calls_found\",\"description\":\"\"},");
		const size_t modifiesAt = semanticSource.find("\"modifiesDocument\":false");
		semanticSource.replace(modifiesAt, std::string("\"modifiesDocument\":false").size(), "\"modifiesDocument\":true");
		const bool unverifiedHidden = showSemantic(semanticSource) &&
			m_script_card_window.BodyText().Find(L"Отмена изменений") < 0 &&
			m_script_card_window.BodyText().Find(L"explicit_calls_found") < 0;
		const size_t statusAt = semanticSource.find("explicit_calls_found");
		semanticSource.replace(statusAt, std::string("explicit_calls_found").size(), "verified_single_action");
		const bool verifiedShown = showSemantic(semanticSource) &&
			m_script_card_window.BodyText().Find(L"Изменения можно отменить одним действием (Ctrl+Z)") >= 0;
		const bool metadataSemantics = unknownShown && unverifiedHidden && verifiedShown;
		const bool passed = metadataSemantics && openedMissing && missingFallback && reused && rendered && structured && responsive && optionalRendered &&
			characteristicLayout && characteristicDpiMatrix && standardValues && cleaningValues && longSettingsFit && categoryFormatted &&
			hashesHidden && originHidden && originStates && technicalAbsent && windowIcons && boldHeading &&
			copiedDetails && categoryShown && previewDelayed && previewShown && previewReusable && previewBridge &&
			previewBridgeTimer && previewBridgeExit && previewPointerLink && previewPointerExit && previewPointerCrossing && previewDeactivated &&
			previewCrossing && previewSwitch && previewSurface && previewLink &&
			previewFallback && previewScrollHidden && uidUpdated &&
			changedWarning && buttons && infoOpened && infoDoubleClickSafe && contextOpened && multiCardDisabled &&
			cardScreenshot && treeScreenshot && darkCardScreenshot && darkTreeScreenshot &&
			commentContent && commentNarrow && commentSettings && commentDark &&
			commentWarning && commentTooltip && commentWide && commentChangedDark &&
			infoUidMatched && infoRunCount == 0 && commentRunCount == 0;
		CStringA report;
		report.Format("phase=script-card-dialog\nmissing=%d\nreused=%d\nrendered=%d\nstructured=%d\nresponsive=%d\noptional=%d\nhashes-hidden=%d\norigin-hidden=%d\norigin-states-hidden=%d\ntechnical-absent=%d\nwindow-icons=%d\nbold-heading=%d\ncopied-details=%d\ncategory=%d\npreview-delayed=%d\npreview-shown=%d\npreview-reusable=%d\npreview-bridge=%d\npreview-crossing=%d\npreview-switch=%d\npreview-surface=%d\npreview-link=%d\npreview-fallback=%d\nmissing-row=%d\nfallback-visible=%d\nfallback-window=%d\nfallback-text=%d\npreview-scroll-hidden=%d\nuid=%d\nchanged-warning=%d\nbuttons=%d\ninfo-click=%d\ninfo-double-click-safe=%d\ncontext-opens=%d\nmulti-card-disabled=%d\ncomment-content=%d\ncomment-narrow=%d\ncomment-settings=%d\ncomment-dark=%d\ncomment-warning=%d\ncomment-tooltip=%d\ncomment-wide=%d\ncomment-changed-dark=%d\ninfo-initialized=%d\ninfo-item=%d\ninfo-row=%d\ninfo-nodes=%u\ninfo-uid=%d\ninfo-selection=%d\nrun-count=%d\nresult=%s\n",
			missingFallback, reused, rendered, structured, responsive, optionalRendered, hashesHidden, originHidden, originStates, technicalAbsent, windowIcons, boldHeading,
			copiedDetails, categoryShown, previewDelayed, previewShown,
			previewReusable, previewBridge, previewCrossing, previewSwitch, previewSurface, previewLink, previewFallback, missingRowVisible, fallbackVisible, fallbackWindow, fallbackText,
			previewScrollHidden, uidUpdated, changedWarning, buttons, infoOpened,
			infoDoubleClickSafe, contextOpened, multiCardDisabled,
			commentContent, commentNarrow, commentSettings, commentDark, commentWarning, commentTooltip, commentWide, commentChangedDark,
			initialized, infoItem != NULL, rowVisible, static_cast<unsigned>(tree.ScriptTreeNodeCount()),
			infoUidMatched,
			infoSelectionMatched, infoRunCount,
			passed ? "pass" : "fail");
		report.AppendFormat("characteristic-layout=%d\ncharacteristic-dpi-matrix=%d\ncharacteristic-wide=%d\ncharacteristic-paired=%d\ncharacteristic-narrow=%d\nstandard-values=%d\ncleaning-values=%d\n",
			characteristicLayout, characteristicDpiMatrix, wideCharacteristics, pairedCharacteristics, narrowCharacteristics,
			standardValues, cleaningValues);
		report.AppendFormat("thumb-position=%d\n", thumbPosition);
		report.AppendFormat("long-settings-fit=%d\n", longSettingsFit);
		report.AppendFormat("category-formatted=%d\nmetadata-semantics=%d\nunknown-author=%d\nunverified-undo-hidden=%d\nverified-undo=%d\n", categoryFormatted, metadataSemantics, unknownShown, unverifiedHidden, verifiedShown);
		report.AppendFormat("preview-bridge-timer=%d\npreview-bridge-exit=%d\npreview-pointer-link=%d\npreview-pointer-exit=%d\npreview-pointer-crossing=%d\npreview-deactivated=%d\n",
			previewBridgeTimer, previewBridgeExit, previewPointerLink, previewPointerExit, previewPointerCrossing, previewDeactivated);
		report.AppendFormat("preview-cursor-placed=%d\npreview-bridge-cursor=%d,%d\npreview-bridge-point=%d,%d\npreview-bridge-current=%d\n",
			cursorPlaced, bridgeCursor.x, bridgeCursor.y, bridgePoint.x, bridgePoint.y, tree.ScriptPreviewTransitionContains(bridgeCursor));
		WritePortableStateTestText(reportPath, report);
		PostMessage(WM_CLOSE);
		return;
	}
	if(scriptCatalogRefreshRuntime)
	{
		_Settings.SetScriptsFolder(scriptsDirectory, true);
		_Settings.SetDocumentTreeScripts(true, true);
		const bool initialized = InitializeScripts();
		auto findScript = [this](LPCWSTR relativePath) -> const ScriptDescriptor* {
			for(int index = 0; index < m_scripts.Menu().Count(); ++index)
				if(!m_scripts.Menu().Item(index).isFolder && m_scripts.Menu().Item(index).relativePath.CompareNoCase(relativePath) == 0) return &m_scripts.Menu().Item(index);
			return NULL;
		};
		const ScriptDescriptor* rootBefore = initialized ? findScript(L"root.js") : NULL;
		const CString rootUid = rootBefore == NULL ? CString() : rootBefore->uid;
		const CString folderA = scriptsDirectory + L"FolderA";
		const CString renamedFolder = scriptsDirectory + L"RenamedFolder";
		const bool changedContents = WritePortableStateTestText(scriptsDirectory + L"Root.js", "function Run(){document.title='catalog-v2';}\n");
		const bool added = WritePortableStateTestText(scriptsDirectory + L"Added.js", "function Run(){}\n");
		const bool kept = WritePortableStateTestText(folderA + L"\\Keep.js", "function Run(){}\n");
		const bool iconChanged = ::CopyFile(scriptsDirectory + L"Root.ico", scriptsDirectory + L"Added.ico", FALSE) != FALSE;
		const bool childRemoved = ::DeleteFile(folderA + L"\\Child.js") != FALSE;
		::DeleteFile(folderA + L"\\Child.bmp");
		const bool folderRenamed = ::MoveFile(folderA, renamedFolder) != FALSE;
		BOOL handled = FALSE;
		OnToolsRefreshScripts(0, ID_TOOLS_REFRESH_SCRIPTS, NULL, handled);
		const ScriptDescriptor* rootAfter = findScript(L"root.js");
		const ScriptDescriptor* addedAfter = findScript(L"added.js");
		const ScriptDescriptor* movedAfter = findScript(L"renamedfolder/folderb/deep.js");
		const ScriptDescriptor* keptAfter = findScript(L"renamedfolder/keep.js");
		const bool removed = findScript(L"foldera/child.js") == NULL && findScript(L"foldera/folderb/deep.js") == NULL;
		bool addedVisual = false;
		for(int index = 0; index < m_scripts.Menu().Count(); ++index)
			if(addedAfter != NULL && &m_scripts.Menu().Item(index) == addedAfter)
				addedVisual = m_scripts.Menu().VisualAt(index).icon != NULL || m_scripts.Menu().VisualAt(index).bitmap != NULL;
		CTreeView& tree = m_document_tree.m_tree.m_tree;
		if(!tree.IsScriptMode()) m_document_tree.m_tree.ToggleScriptMode();
		const bool treeUpdated = tree.IsScriptMode() && tree.FindScriptTreeItem(L"renamedfolder/keep.js") != NULL && tree.FindScriptTreeItem(L"foldera/child.js") == NULL;
		const bool retainedUid = rootBefore != NULL && rootAfter != NULL && rootAfter->uid == rootUid;
		const bool documentAlive = m_doc != NULL;
		const bool passed = initialized && changedContents && added && kept && iconChanged && childRemoved && folderRenamed && rootAfter != NULL && addedAfter != NULL && movedAfter != NULL && keptAfter != NULL && removed && addedVisual && treeUpdated && retainedUid && documentAlive;
		CStringA report; report.Format("phase=script-catalog-refresh\ncontents=%d\nadded=%d\nremoved=%d\nrenamed=%d\nicon=%d\nroot-before=%d\nroot-after=%d\nroot-uid-length=%d\nroot-after-uid-length=%d\nuid=%d\ntree=%d\ndocument=%d\nresult=%s\n", changedContents, addedAfter != NULL, removed, movedAfter != NULL && keptAfter != NULL, addedVisual, rootBefore != NULL, rootAfter != NULL, rootUid.GetLength(), rootAfter == NULL ? 0 : rootAfter->uid.GetLength(), retainedUid, treeUpdated, documentAlive, passed ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report); PostMessage(WM_CLOSE); return;
	}
	if(scriptStartupValidationOn)
	{
		_Settings.SetScriptsFolder(scriptsDirectory, true);
		const bool rejected = InitializeScripts() && _Settings.CheckScriptsOnStartup() && findBrokenScript() == NULL;
		CStringA report; report.Format("phase=script-startup-validation-on\nvalidation-on=%d\nbroken-in-catalog=%d\nresult=%s\n", _Settings.CheckScriptsOnStartup(), findBrokenScript() != NULL, rejected ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report); PostMessage(WM_CLOSE); return;
	}
	if(scriptStartupValidationOffWrite)
	{
		_Settings.SetCheckScriptsOnStartup(false, true);
		const bool saved = !_Settings.CheckScriptsOnStartup();
		CStringA report; report.Format("phase=script-startup-validation-off-write\nvalidation-off=%d\nresult=%s\n", saved, saved ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report); PostMessage(WM_CLOSE); return;
	}
	if(scriptStartupValidationOffRead)
	{
		const ScriptDescriptor* broken = findBrokenScript();
		const bool ready = !_Settings.CheckScriptsOnStartup() && broken != NULL;
		const bool interactiveRun = ready && m_doc != NULL && !FbeScriptDiagnostics::DialogsSuppressed();
		if(interactiveRun) m_doc->RunScript(broken->path);
		CStringA report; report.Format("phase=script-startup-validation-off-read\nvalidation-off=%d\nbroken-in-catalog=%d\ninteractive-run=%d\ndialogs-suppressed=%d\nresult=%s\n", !_Settings.CheckScriptsOnStartup(), broken != NULL, interactiveRun, FbeScriptDiagnostics::DialogsSuppressed(), interactiveRun ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report); PostMessage(WM_CLOSE); return;
	}
	if(scriptToolbarRuntimeSize)
	{
		const int initialIconSize = _Settings.GetScriptIconSize();
		::CreateDirectory(scriptsDirectory, NULL);
		_Settings.SetScriptsFolder(scriptsDirectory, true);
		const bool fixtureWritten = WritePortableStateTestText(scriptsDirectory + L"RuntimeSize.js", "function Run() { document.title = 'Icon size command passed'; }\n");
		const CString fixtureIcon = scriptsDirectory + L"RuntimeSize.ico";
		const CString bundledIcon = U::GetProgDir() + L"Scripts\\06_\u0427\u0438\u0441\u0442\u043a\u0430.ico";
		const bool fixtureIconCopied = ::CopyFileW(bundledIcon, fixtureIcon, FALSE) != FALSE;
		const bool dualScriptWritten = WritePortableStateTestText(scriptsDirectory + L"DualSidecar.js", "function Run() {}\n");
		const bool dualIconCopied = ::CopyFileW(bundledIcon, scriptsDirectory + L"DualSidecar.ico", FALSE) != FALSE;
		bool dualBitmapWritten = false;
		{
			const CString path = scriptsDirectory + L"DualSidecar.bmp";
			HANDLE file = ::CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if(file != INVALID_HANDLE_VALUE) {
				BITMAPFILEHEADER header = {}; BITMAPINFOHEADER info = {}; BYTE pixels[16 * 16 * 3] = {};
				header.bfType = 0x4d42; header.bfOffBits = sizeof(header) + sizeof(info); header.bfSize = header.bfOffBits + sizeof(pixels);
				info.biSize = sizeof(info); info.biWidth = 16; info.biHeight = 16; info.biPlanes = 1; info.biBitCount = 24; info.biCompression = BI_RGB; info.biSizeImage = sizeof(pixels);
				for(size_t index=0; index<sizeof(pixels); index+=3) pixels[index+1] = 200;
				DWORD written = 0;
				dualBitmapWritten = ::WriteFile(file,&header,sizeof(header),&written,NULL) && written == sizeof(header) &&
					::WriteFile(file,&info,sizeof(info),&written,NULL) && written == sizeof(info) &&
					::WriteFile(file,pixels,sizeof(pixels),&written,NULL) && written == sizeof(pixels);
				::CloseHandle(file);
			}
		}
		const bool bitmapOnlyScriptWritten = WritePortableStateTestText(scriptsDirectory + L"BitmapOnly.js", "function Run() {}\n");
		const bool bitmapOnlyCopied = dualBitmapWritten && ::CopyFileW(scriptsDirectory + L"DualSidecar.bmp", scriptsDirectory + L"BitmapOnly.bmp", FALSE) != FALSE;
		const bool rectScriptWritten = WritePortableStateTestText(scriptsDirectory + L"RectBitmap.js", "function Run() {}\n");
		bool rectBitmapWritten = false;
		{
			HANDLE file = ::CreateFileW(scriptsDirectory + L"RectBitmap.bmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if(file != INVALID_HANDLE_VALUE) {
				BITMAPFILEHEADER header = {}; BITMAPINFOHEADER info = {}; BYTE pixels[16 * 12 * 3] = {};
				header.bfType = 0x4d42; header.bfOffBits = sizeof(header) + sizeof(info); header.bfSize = header.bfOffBits + sizeof(pixels);
				info.biSize = sizeof(info); info.biWidth = 16; info.biHeight = 12; info.biPlanes = 1; info.biBitCount = 24; info.biCompression = BI_RGB; info.biSizeImage = sizeof(pixels);
				for(size_t index=0; index<sizeof(pixels); index+=3) pixels[index+1] = 200;
				DWORD written = 0;
				rectBitmapWritten = ::WriteFile(file,&header,sizeof(header),&written,NULL) && written == sizeof(header) &&
					::WriteFile(file,&info,sizeof(info),&written,NULL) && written == sizeof(info) &&
					::WriteFile(file,pixels,sizeof(pixels),&written,NULL) && written == sizeof(pixels);
				::CloseHandle(file);
			}
		}
		const CString iconFolder = scriptsDirectory + L"IconFolder\\";
		const bool alphaScriptWritten = WritePortableStateTestText(scriptsDirectory + L"AlphaRect.js", "function Run() {}\n");
		bool alphaBitmapWritten = false;
		{
			HANDLE file = ::CreateFileW(scriptsDirectory + L"AlphaRect.bmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if(file != INVALID_HANDLE_VALUE) {
				BITMAPFILEHEADER header = {}; BITMAPINFOHEADER info = {}; DWORD pixels[16 * 12] = {};
				header.bfType = 0x4d42; header.bfOffBits = sizeof(header) + sizeof(info); header.bfSize = header.bfOffBits + sizeof(pixels);
				info.biSize = sizeof(info); info.biWidth = 16; info.biHeight = 12; info.biPlanes = 1; info.biBitCount = 32; info.biCompression = BI_RGB; info.biSizeImage = sizeof(pixels);
				for(DWORD& pixel : pixels) pixel = 0x8000c800u;
				DWORD written = 0;
				alphaBitmapWritten = ::WriteFile(file,&header,sizeof(header),&written,NULL) && written == sizeof(header) &&
					::WriteFile(file,&info,sizeof(info),&written,NULL) && written == sizeof(info) &&
					::WriteFile(file,pixels,sizeof(pixels),&written,NULL) && written == sizeof(pixels);
				::CloseHandle(file);
			}
		}
		const bool largeRectScriptWritten = WritePortableStateTestText(scriptsDirectory + L"LargeRect.js", "function Run() {}\n");
		bool largeRectWritten = false;
		{
			HANDLE file = ::CreateFileW(scriptsDirectory + L"LargeRect.bmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if(file != INVALID_HANDLE_VALUE) {
				BITMAPFILEHEADER header = {}; BITMAPINFOHEADER info = {}; BYTE pixels[32 * 24 * 3] = {};
				header.bfType = 0x4d42; header.bfOffBits = sizeof(header) + sizeof(info); header.bfSize = header.bfOffBits + sizeof(pixels);
				info.biSize = sizeof(info); info.biWidth = 32; info.biHeight = 24; info.biPlanes = 1; info.biBitCount = 24; info.biCompression = BI_RGB; info.biSizeImage = sizeof(pixels);
				for(size_t index=0; index<sizeof(pixels); index+=3) pixels[index+1] = 200;
				DWORD written = 0;
				largeRectWritten = ::WriteFile(file,&header,sizeof(header),&written,NULL) && written == sizeof(header) &&
					::WriteFile(file,&info,sizeof(info),&written,NULL) && written == sizeof(info) &&
					::WriteFile(file,pixels,sizeof(pixels),&written,NULL) && written == sizeof(pixels);
				::CloseHandle(file);
			}
		}
		::CreateDirectory(iconFolder, NULL);
		const bool nestedFixtureWritten = WritePortableStateTestText(iconFolder + L"Nested.js", "function Run() {}\n");
		if(!fixtureWritten || !fixtureIconCopied || !dualScriptWritten || !dualIconCopied || !dualBitmapWritten || !bitmapOnlyScriptWritten || !bitmapOnlyCopied || !rectScriptWritten || !rectBitmapWritten || !largeRectScriptWritten || !largeRectWritten || !alphaScriptWritten || !alphaBitmapWritten || !nestedFixtureWritten || !InitializeScripts()) { WritePortableStateTestText(reportPath, "phase=script-toolbar-runtime-size\nreason=initial-reload\nresult=fail\n"); PostMessage(WM_CLOSE); return; }
		CString scriptUid; UINT scriptCommand = 0, dualCommand = 0, bitmapOnlyCommand = 0, rectCommand = 0, largeRectCommand = 0, alphaCommand = 0;
		for(int index = 0; index < m_scripts.Menu().Count(); ++index)
		{
			if(m_scripts.Menu().Item(index).relativePath.CompareNoCase(L"RuntimeSize.js") == 0)
			{ scriptUid = m_scripts.Menu().Item(index).uid; scriptCommand = ID_SCRIPT_BASE + m_scripts.Menu().Item(index).commandId; }
			if(m_scripts.Menu().Item(index).relativePath.CompareNoCase(L"DualSidecar.js") == 0)
				dualCommand = ID_SCRIPT_BASE + m_scripts.Menu().Item(index).commandId;
			if(m_scripts.Menu().Item(index).relativePath.CompareNoCase(L"BitmapOnly.js") == 0)
				bitmapOnlyCommand = ID_SCRIPT_BASE + m_scripts.Menu().Item(index).commandId;
			if(m_scripts.Menu().Item(index).relativePath.CompareNoCase(L"RectBitmap.js") == 0)
				rectCommand = ID_SCRIPT_BASE + m_scripts.Menu().Item(index).commandId;
			if(m_scripts.Menu().Item(index).relativePath.CompareNoCase(L"LargeRect.js") == 0)
				largeRectCommand = ID_SCRIPT_BASE + m_scripts.Menu().Item(index).commandId;
			if(m_scripts.Menu().Item(index).relativePath.CompareNoCase(L"AlphaRect.js") == 0)
				alphaCommand = ID_SCRIPT_BASE + m_scripts.Menu().Item(index).commandId;
		}
		std::vector<ScriptToolbarDefinition> previous = currentDefinitions(), definitions = previous;
		for(int index = 1; index <= 8; ++index) { ScriptToolbarDefinition item; item.id.Format(L"runtime-size-%d", index); item.name.Format(L"Runtime size %d", index); item.visible = true; if(index == 1 && !scriptUid.IsEmpty()) { PortableToolbarItem button = {}; button.scriptUid = scriptUid; item.items.push_back(button); } definitions.push_back(item); }
		RECT stockRect = {}; ::GetWindowRect(m_ScriptsToolbar, &stockRect);
		const int stockHeight = stockRect.bottom - stockRect.top;
		auto oneRow = [&]() {
			int matching = 0;
			for(size_t index = 0; index < m_scriptToolbars.Items().size(); ++index) {
				const ScriptToolbarRuntime& runtime = m_scriptToolbars.Items()[index]; if(runtime.definition.id.Left(13) != L"runtime-size-") continue;
				const int band = m_rebar.IdToIndex(runtime.rebarBandId); RECT toolbarRect = {}, bandRect = {};
				if(runtime.window != NULL && band >= 0 && ::GetWindowRect(runtime.window, &toolbarRect) && m_rebar.GetRect(band, &bandRect) &&
					abs(toolbarRect.bottom - toolbarRect.top - stockHeight) <= ::GetSystemMetrics(SM_CYEDGE) * 2 && abs(bandRect.bottom - bandRect.top - stockHeight) <= ::GetSystemMetrics(SM_CYEDGE) * 2) ++matching;
			}
			return stockHeight > 0 && matching == 8;
		};
		const bool created = ApplyScriptToolbarDefinitions(previous, definitions) && oneRow();
		std::vector<ScriptToolbarDefinition> current = currentDefinitions();
		for(size_t index = 0; index < current.size(); ++index) if(current[index].id == L"runtime-size-1") { PortableToolbarItem item = {}; item.scriptUid = L"runtime-size-missing-uid"; current[index].items.push_back(item); break; }
		const bool added = created && ApplyScriptToolbarDefinitions(definitions, current) && oneRow();
		auto runtimeBandId = [&](const CString& id) { ScriptToolbarRuntime* runtime = m_scriptToolbars.Find(id); return runtime == NULL ? 0U : runtime->rebarBandId; };
		auto duplicateBandIds = [&]() {
			std::vector<UINT> ids;
			int duplicates = 0;
			for(int band = 0; band < static_cast<int>(m_rebar.GetBandCount()); ++band) {
				REBARBANDINFO info = {}; info.cbSize = sizeof(info); info.fMask = RBBIM_ID;
				if(!m_rebar.GetBandInfo(band, &info)) continue;
				for(size_t index = 0; index < ids.size(); ++index) if(ids[index] == info.wID) { ++duplicates; break; }
				ids.push_back(info.wID);
			}
			return duplicates;
		};
		const UINT bandBefore = runtimeBandId(L"runtime-size-2");
		std::vector<ScriptToolbarDefinition> hidden = current;
		for(size_t index = 0; index < hidden.size(); ++index) if(hidden[index].id == L"runtime-size-2") { hidden[index].visible = false; break; }
		const bool hiddenOk = added && ApplyScriptToolbarDefinitions(current, hidden);
		const bool shown = hiddenOk && ApplyScriptToolbarDefinitions(hidden, current);
		const bool showOneRow = shown && oneRow();
		const UINT bandAfter = runtimeBandId(L"runtime-size-2");
		const int duplicateIds = duplicateBandIds();
		const bool restarted = showOneRow && duplicateIds == 0 && InitializeScripts() && oneRow();
		CStringA matrixDetails;
		auto greenBitmapAtNativeSize = [&](HIMAGELIST images, int imageIndex, int size, int sourceWidth, int sourceHeight) {
			bool matches = false; HDC screen = ::GetDC(NULL); HDC dc = screen ? ::CreateCompatibleDC(screen) : NULL;
			HBITMAP bitmap = screen ? ::CreateCompatibleBitmap(screen,size,size) : NULL;
			if(dc && bitmap && images && imageIndex >= 0 && imageIndex < ::ImageList_GetImageCount(images)) {
				HGDIOBJ oldBitmap = ::SelectObject(dc,bitmap); RECT cell = {0,0,size,size}; ::FillRect(dc,&cell,static_cast<HBRUSH>(::GetStockObject(BLACK_BRUSH)));
				matches = ::ImageList_Draw(images,imageIndex,dc,0,0,ILD_TRANSPARENT) != FALSE;
				const int offsetX = (size - sourceWidth) / 2, offsetY = (size - sourceHeight) / 2;
				for(int y=0; y<size; ++y) for(int x=0; x<size; ++x) {
					const bool inSource = x >= offsetX && x < offsetX + sourceWidth && y >= offsetY && y < offsetY + sourceHeight;
					if(::GetPixel(dc,x,y) != (inSource ? RGB(0,200,0) : RGB(0,0,0))) matches = false;
				}
				::SelectObject(dc,oldBitmap);
			}
			if(bitmap) ::DeleteObject(bitmap);
			if(dc) ::DeleteDC(dc);
			if(screen) ::ReleaseDC(NULL,screen);
			return matches;
		};
		auto alphaBitmapPixels = [&](HIMAGELIST images, int imageIndex, int size) {
			bool matches = images && imageIndex >= 0 && imageIndex < ::ImageList_GetImageCount(images);
			HDC screen = ::GetDC(NULL); HDC dc = screen ? ::CreateCompatibleDC(screen) : NULL;
			HBITMAP bitmap = screen ? ::CreateCompatibleBitmap(screen,size,size) : NULL;
			if(matches && dc && bitmap) {
				HGDIOBJ old = ::SelectObject(dc,bitmap); RECT cell = {0,0,size,size};
				for(int background : {32, 255}) {
					HBRUSH brush = ::CreateSolidBrush(RGB(background,background,background)); ::FillRect(dc,&cell,brush); ::DeleteObject(brush);
					if(!::ImageList_Draw(images,imageIndex,dc,0,0,ILD_TRANSPARENT)) matches = false;
					const COLORREF pixel = ::GetPixel(dc,size/2,size/2);
					const int expectedOther = (background * 127 + 127) / 255;
					const int expectedGreen = (200 * 128 + background * 127 + 127) / 255;
					matrixDetails.AppendFormat("alpha-detail=%d/%06lx/%06lx\n", background, pixel, ::GetPixel(dc,0,0));
					if(abs(static_cast<int>(GetRValue(pixel))-expectedOther)>2 || abs(static_cast<int>(GetGValue(pixel))-expectedGreen)>2 || abs(static_cast<int>(GetBValue(pixel))-expectedOther)>2 || ::GetPixel(dc,0,0) != RGB(background,background,background)) matches = false;
				}
				::SelectObject(dc,old);
			} else matches = false;
			if(bitmap) ::DeleteObject(bitmap); if(dc) ::DeleteDC(dc); if(screen) ::ReleaseDC(NULL,screen);
			return matches;
		};
		auto compareFixturePixels = [&](HIMAGELIST images, int imageIndex, int size) {
			bool matches = false;
			HICON reference = static_cast<HICON>(::LoadImageW(NULL, fixtureIcon, IMAGE_ICON, size, size, LR_LOADFROMFILE));
			HDC screen = ::GetDC(NULL); HDC actual = screen ? ::CreateCompatibleDC(screen) : NULL; HDC expectedDc = screen ? ::CreateCompatibleDC(screen) : NULL;
			HBITMAP actualBitmap = screen ? ::CreateCompatibleBitmap(screen, size, size) : NULL; HBITMAP expectedBitmap = screen ? ::CreateCompatibleBitmap(screen, size, size) : NULL;
			if(reference && actual && expectedDc && actualBitmap && expectedBitmap && images && imageIndex >= 0 && imageIndex < ::ImageList_GetImageCount(images)) {
				HGDIOBJ oldActual = ::SelectObject(actual, actualBitmap); HGDIOBJ oldExpected = ::SelectObject(expectedDc, expectedBitmap);
				RECT rect = {0, 0, size, size};
				matches = true;
				for(COLORREF background : {RGB(255,255,255), RGB(32,32,32)}) {
					HBRUSH brush = ::CreateSolidBrush(background); ::FillRect(actual, &rect, brush); ::FillRect(expectedDc, &rect, brush); ::DeleteObject(brush);
					if(!::ImageList_Draw(images, imageIndex, actual, 0, 0, ILD_TRANSPARENT) || !::DrawIconEx(expectedDc, 0, 0, reference, size, size, 0, NULL, DI_NORMAL)) matches = false;
					int mismatches = 0, maxError = 0, firstX = -1, firstY = -1; COLORREF firstActual = 0, firstExpected = 0;
					for(int y=0; y<size; ++y) for(int x=0; x<size; ++x) {
						const COLORREF observed = ::GetPixel(actual,x,y), original = ::GetPixel(expectedDc,x,y);
						maxError = (std::max)(maxError, (std::max)(abs(static_cast<int>(GetRValue(observed))-static_cast<int>(GetRValue(original))),
							(std::max)(abs(static_cast<int>(GetGValue(observed))-static_cast<int>(GetGValue(original))), abs(static_cast<int>(GetBValue(observed))-static_cast<int>(GetBValue(original))))));
						if(observed != original) { if(mismatches == 0) { firstX=x; firstY=y; firstActual=observed; firstExpected=original; } ++mismatches; }
					}
					// The two native composition paths can round a channel by one level.
					if(maxError > 1) matches = false;
					matrixDetails.AppendFormat("fixture-pixel-diff=%d/max:%d/%d,%d:%06lx/%06lx;background:%06lx\n", mismatches, maxError, firstX, firstY, firstActual, firstExpected, background);
				}
				::SelectObject(actual,oldActual); ::SelectObject(expectedDc,oldExpected);
			}
			if(actualBitmap) ::DeleteObject(actualBitmap);
			if(expectedBitmap) ::DeleteObject(expectedBitmap);
			if(actual) ::DeleteDC(actual);
			if(expectedDc) ::DeleteDC(expectedDc);
			if(screen) ::ReleaseDC(NULL,screen);
			if(reference) ::DestroyIcon(reference);
			return matches;
		};
		auto metrics = [&](HWND window, UINT dpi, bool requireVisibleScript) {
			if(!::IsWindow(window)) return false;
			const HIMAGELIST images = reinterpret_cast<HIMAGELIST>(::SendMessage(window, TB_GETIMAGELIST, 0, 0));
			int width = 0, height = 0; const int expected = UiMetrics::ScaleForDpi(_Settings.GetScriptIconSize(), dpi);
			const bool imageSizeRead = images != NULL && ::ImageList_GetIconSize(images, &width, &height) != FALSE;
			const LRESULT buttonSize = ::SendMessage(window, TB_GETBUTTONSIZE, 0, 0);
			RECT bounds = {}; ::GetWindowRect(window, &bounds);
			TBBUTTONS catalog; const bool available = GetAvailableButtons(window, catalog);
			bool scriptImage = false; int scriptImageIndex = -1;
			int dualImageIndex = -1, bitmapOnlyIndex = -1, rectIndex = -1, largeRectIndex = -1, alphaIndex = -1;
			for(int index = 0; available && index < catalog.GetSize(); ++index)
			{
				if(catalog[index].idCommand == static_cast<int>(scriptCommand))
				{ scriptImageIndex = catalog[index].iBitmap; scriptImage = scriptImageIndex >= m_scriptsToolbarBaseImageCount && scriptImageIndex < ::ImageList_GetImageCount(images); }
				if(catalog[index].idCommand == static_cast<int>(dualCommand)) dualImageIndex = catalog[index].iBitmap;
				if(catalog[index].idCommand == static_cast<int>(bitmapOnlyCommand)) bitmapOnlyIndex = catalog[index].iBitmap;
				if(catalog[index].idCommand == static_cast<int>(rectCommand)) rectIndex = catalog[index].iBitmap;
				if(catalog[index].idCommand == static_cast<int>(largeRectCommand)) largeRectIndex = catalog[index].iBitmap;
				if(catalog[index].idCommand == static_cast<int>(alphaCommand)) alphaIndex = catalog[index].iBitmap;
			}
			bool scriptPixelsMatch = true;
			bool dualBitmapPreferred = true;
			bool bitmapOnlyCorrect = true;
			bool rectBitmapCorrect = true;
			if(dpi == 96 && _Settings.GetScriptIconSize() == 16) {
				scriptPixelsMatch = scriptImage && compareFixturePixels(images, scriptImageIndex, 16);
				matrixDetails.AppendFormat("fixture-16px-pixels=%d\n", scriptPixelsMatch);
				dualBitmapPreferred = greenBitmapAtNativeSize(images,dualImageIndex,16,16,16);
				bitmapOnlyCorrect = greenBitmapAtNativeSize(images,bitmapOnlyIndex,16,16,16);
				rectBitmapCorrect = greenBitmapAtNativeSize(images,rectIndex,16,16,12);
				matrixDetails.AppendFormat("dual-bitmap-preferred=%d\n", dualBitmapPreferred);
				matrixDetails.AppendFormat("bitmap-only-16=%d\n", bitmapOnlyCorrect);
				matrixDetails.AppendFormat("rect-bitmap-16=%d\n", rectBitmapCorrect);
				matrixDetails.AppendFormat("large-rect-16=%d\n", greenBitmapAtNativeSize(images,largeRectIndex,16,16,12));
				matrixDetails.AppendFormat("alpha-rect-16=%d\n", alphaBitmapPixels(images,alphaIndex,16));
			}
			if(dpi == 120 && _Settings.GetScriptIconSize() == 16) {
				dualBitmapPreferred = compareFixturePixels(images, dualImageIndex, 20);
				bitmapOnlyCorrect = greenBitmapAtNativeSize(images,bitmapOnlyIndex,20,16,16);
				rectBitmapCorrect = greenBitmapAtNativeSize(images,rectIndex,20,16,12);
				matrixDetails.AppendFormat("dual-larger-ico-preferred=%d\n", dualBitmapPreferred);
				matrixDetails.AppendFormat("bitmap-only-20=%d\n", bitmapOnlyCorrect);
				matrixDetails.AppendFormat("rect-bitmap-20=%d\n", rectBitmapCorrect);
				matrixDetails.AppendFormat("large-rect-20=%d\n", greenBitmapAtNativeSize(images,largeRectIndex,20,20,15));
			}
			// Measure the painted glyph, not only the ImageList cell. The source
			// has transparent corners but its painted area must grow with DPI.
			HDC screen = ::GetDC(NULL);
			HDC memory = screen != NULL ? ::CreateCompatibleDC(screen) : NULL;
			HBITMAP bitmap = memory != NULL ? ::CreateCompatibleBitmap(screen, expected, expected) : NULL;
			bool graphicScaled = false;
			if(bitmap != NULL)
			{
				HGDIOBJ oldBitmap = ::SelectObject(memory, bitmap);
				RECT cell = { 0, 0, expected, expected };
				HBRUSH background = ::CreateSolidBrush(RGB(0, 0, 0));
				::FillRect(memory, &cell, background); ::DeleteObject(background);
				if(::ImageList_Draw(images, 0, memory, 0, 0, ILD_TRANSPARENT))
				{
					int left = expected, top = expected, right = -1, bottom = -1;
					for(int y = 0; y < expected; ++y) for(int x = 0; x < expected; ++x)
						if(::GetPixel(memory, x, y) != RGB(0, 0, 0))
						{ left = (std::min)(left, x); top = (std::min)(top, y); right = (std::max)(right, x); bottom = (std::max)(bottom, y); }
					graphicScaled = right - left + 1 >= expected * 3 / 4 && bottom - top + 1 >= expected * 3 / 4;
				}
				::SelectObject(memory, oldBitmap); ::DeleteObject(bitmap);
			}
			if(memory != NULL) ::DeleteDC(memory);
			if(screen != NULL) ::ReleaseDC(NULL, screen);
			const bool visibleScript = !requireVisibleScript || ::SendMessage(window, TB_COMMANDTOINDEX, scriptCommand, 0) >= 0;
			matrixDetails.AppendFormat("metric-%u=%dx%d,button:%dx%d,toolbar:%d,catalog:%d,script-image:%d,visible:%d,graphic:%d\n",
				dpi, width, height, LOWORD(buttonSize), HIWORD(buttonSize), bounds.bottom - bounds.top, available, scriptImage, visibleScript, graphicScaled);
			return imageSizeRead && width == expected && height == expected &&
				LOWORD(buttonSize) >= UiMetrics::ScaleForDpi(_Settings.GetScriptIconSize() + 7, dpi) && HIWORD(buttonSize) >= UiMetrics::ScaleForDpi(_Settings.GetScriptIconSize() + 7, dpi) &&
				bounds.bottom - bounds.top >= HIWORD(buttonSize) && scriptImage && visibleScript && graphicScaled && scriptPixelsMatch && dualBitmapPreferred && bitmapOnlyCorrect && rectBitmapCorrect;
		};
		const UINT dpis[] = { 96, 120, 144, 168, 192 };
		bool dpiMatrix = restarted && scriptCommand != 0 && !scriptUid.IsEmpty();
		HIMAGELIST previousMainImages = m_scriptsToolbarImages;
		std::vector<HIMAGELIST> previousCustomImages;
		for(size_t index = 0; index < m_scriptToolbars.Items().size(); ++index)
			previousCustomImages.push_back(m_scriptToolbars.Items()[index].images);
		for(int logicalSize : {16, 20, 24, 32}) {
		_Settings.SetScriptIconSize(logicalSize);
		for(UINT dpi : dpis)
		{
			const bool refreshed = UpdateScriptToolbarDpiMetrics(dpi);
			bool customMetrics = true, bandMetrics = true, mainBand = false;
			for(size_t index = 0; index < m_scriptToolbars.Items().size(); ++index)
			{
				const ScriptToolbarRuntime& runtime = m_scriptToolbars.Items()[index];
				if(runtime.definition.id.Left(13) != L"runtime-size-") continue;
				customMetrics = customMetrics && runtime.images != previousCustomImages[index] &&
					runtime.images == reinterpret_cast<HIMAGELIST>(::SendMessage(runtime.window, TB_GETIMAGELIST, 0, 0)) &&
					metrics(runtime.window, dpi, runtime.definition.id == L"runtime-size-1");
				previousCustomImages[index] = runtime.images;
				const int band = m_rebar.IdToIndex(runtime.rebarBandId); RECT toolbarRect = {}, bandRect = {};
				bandMetrics = bandMetrics && band >= 0 && ::GetWindowRect(runtime.window, &toolbarRect) && m_rebar.GetRect(band, &bandRect) &&
					abs((toolbarRect.bottom - toolbarRect.top) - (bandRect.bottom - bandRect.top)) <= ::GetSystemMetrics(SM_CYEDGE) * 2;
			}
			for(int band = 0; band < static_cast<int>(m_rebar.GetBandCount()); ++band)
			{
				REBARBANDINFO info = {}; info.cbSize = sizeof(info); info.fMask = RBBIM_CHILD;
				if(!m_rebar.GetBandInfo(band, &info) || info.hwndChild != m_ScriptsToolbar) continue;
				RECT toolbarRect = {}, bandRect = {};
				mainBand = ::GetWindowRect(m_ScriptsToolbar, &toolbarRect) && m_rebar.GetRect(band, &bandRect) &&
					abs((toolbarRect.bottom - toolbarRect.top) - (bandRect.bottom - bandRect.top)) <= ::GetSystemMetrics(SM_CYEDGE) * 2;
				break;
			}
			const bool mainImagesChanged = m_scriptsToolbarImages != previousMainImages;
			const bool mainMetrics = metrics(m_ScriptsToolbar, dpi, false);
			matrixDetails.AppendFormat("dpi-%u=refresh:%d,main-images:%d,main-metrics:%d,custom-metrics:%d,custom-bands:%d,main-band:%d\n",
				dpi, refreshed, mainImagesChanged, mainMetrics, customMetrics, bandMetrics, mainBand);
			dpiMatrix = dpiMatrix && refreshed && mainImagesChanged && mainMetrics && customMetrics && bandMetrics && mainBand;
			previousMainImages = m_scriptsToolbarImages;
		}
		}
		const bool restoredDpi = UpdateScriptToolbarDpiMetrics(UiMetrics::DpiForWindow(m_hWnd));
		CTreeView& iconTree = m_document_tree.m_tree.m_tree;
		_Settings.SetDocumentTreeScripts(true); iconTree.SetScriptMode(true); RefreshNavigationScriptTree();
		_Settings.SetScriptIconSize(16);
		iconTree.UpdateScriptDpiMetrics(96);
		const HTREEITEM fixtureTreeItem = iconTree.FindScriptTreeItem(L"runtimesize.js");
		TVITEMW treeItem = {}; treeItem.mask = TVIF_IMAGE; treeItem.hItem = fixtureTreeItem;
		const bool fixtureTreePixels = fixtureTreeItem && ::SendMessageW(iconTree, TVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&treeItem)) &&
			compareFixturePixels(iconTree.ScriptImageList(), treeItem.iImage, 16);
		matrixDetails.AppendFormat("tree-fixture-16px-pixels=%d\n", fixtureTreePixels);
		const HTREEITEM dualTreeItem = iconTree.FindScriptTreeItem(L"dualsidecar.js");
		TVITEMW dualTree = {}; dualTree.mask = TVIF_IMAGE; dualTree.hItem = dualTreeItem;
		bool treeBitmapPreferred = false;
		HDC treeScreen = ::GetDC(NULL); HDC treeDc = treeScreen ? ::CreateCompatibleDC(treeScreen) : NULL;
		HBITMAP treeBitmap = treeScreen ? ::CreateCompatibleBitmap(treeScreen,16,16) : NULL;
		if(dualTreeItem && ::SendMessageW(iconTree,TVM_GETITEMW,0,reinterpret_cast<LPARAM>(&dualTree)) && treeDc && treeBitmap) {
			HGDIOBJ previousBitmap = ::SelectObject(treeDc,treeBitmap); RECT cell = {0,0,16,16}; ::FillRect(treeDc,&cell,static_cast<HBRUSH>(::GetStockObject(BLACK_BRUSH)));
			treeBitmapPreferred = ::ImageList_Draw(iconTree.ScriptImageList(),dualTree.iImage,treeDc,0,0,ILD_TRANSPARENT) && ::GetPixel(treeDc,8,8) == RGB(0,200,0);
			::SelectObject(treeDc,previousBitmap);
		}
		if(treeBitmap) ::DeleteObject(treeBitmap);
		if(treeDc) ::DeleteDC(treeDc);
		if(treeScreen) ::ReleaseDC(NULL,treeScreen);
		const HTREEITEM bitmapOnlyTreeItem = iconTree.FindScriptTreeItem(L"bitmaponly.js");
		TVITEMW bitmapOnlyTree = {}; bitmapOnlyTree.mask = TVIF_IMAGE; bitmapOnlyTree.hItem = bitmapOnlyTreeItem;
		const bool treeBitmapOnly16 = bitmapOnlyTreeItem && ::SendMessageW(iconTree,TVM_GETITEMW,0,reinterpret_cast<LPARAM>(&bitmapOnlyTree)) &&
			greenBitmapAtNativeSize(iconTree.ScriptImageList(),bitmapOnlyTree.iImage,16,16,16);
		const HTREEITEM rectTreeItem = iconTree.FindScriptTreeItem(L"rectbitmap.js");
		TVITEMW rectTree = {}; rectTree.mask = TVIF_IMAGE; rectTree.hItem = rectTreeItem;
		const bool treeRect16 = rectTreeItem && ::SendMessageW(iconTree,TVM_GETITEMW,0,reinterpret_cast<LPARAM>(&rectTree)) &&
			greenBitmapAtNativeSize(iconTree.ScriptImageList(),rectTree.iImage,16,16,12);
		const HTREEITEM largeRectTreeItem = iconTree.FindScriptTreeItem(L"largerect.js");
		TVITEMW largeRectTree = {}; largeRectTree.mask = TVIF_IMAGE; largeRectTree.hItem = largeRectTreeItem;
		const bool treeLargeRect16 = largeRectTreeItem && ::SendMessageW(iconTree,TVM_GETITEMW,0,reinterpret_cast<LPARAM>(&largeRectTree)) &&
			greenBitmapAtNativeSize(iconTree.ScriptImageList(),largeRectTree.iImage,16,16,12);
		const HTREEITEM alphaTreeItem = iconTree.FindScriptTreeItem(L"alpharect.js");
		TVITEMW alphaTree = {}; alphaTree.mask = TVIF_IMAGE; alphaTree.hItem = alphaTreeItem;
		const bool treeAlpha16 = alphaTreeItem && ::SendMessageW(iconTree,TVM_GETITEMW,0,reinterpret_cast<LPARAM>(&alphaTree)) && alphaBitmapPixels(iconTree.ScriptImageList(),alphaTree.iImage,16);
		matrixDetails.AppendFormat("tree-alpha-rect-16=%d\n", treeAlpha16);
		iconTree.UpdateScriptDpiMetrics(120);
		const bool treeLargeIconPreferred = dualTreeItem && ::SendMessageW(iconTree,TVM_GETITEMW,0,reinterpret_cast<LPARAM>(&dualTree)) &&
			compareFixturePixels(iconTree.ScriptImageList(),dualTree.iImage,20);
		const bool treeBitmapOnly20 = bitmapOnlyTreeItem && ::SendMessageW(iconTree,TVM_GETITEMW,0,reinterpret_cast<LPARAM>(&bitmapOnlyTree)) &&
			greenBitmapAtNativeSize(iconTree.ScriptImageList(),bitmapOnlyTree.iImage,20,16,16);
		const bool treeRect20 = rectTreeItem && ::SendMessageW(iconTree,TVM_GETITEMW,0,reinterpret_cast<LPARAM>(&rectTree)) &&
			greenBitmapAtNativeSize(iconTree.ScriptImageList(),rectTree.iImage,20,16,12);
		const bool treeLargeRect20 = largeRectTreeItem && ::SendMessageW(iconTree,TVM_GETITEMW,0,reinterpret_cast<LPARAM>(&largeRectTree)) &&
			greenBitmapAtNativeSize(iconTree.ScriptImageList(),largeRectTree.iImage,20,20,15);
		matrixDetails.AppendFormat("tree-large-rect-16=%d\ntree-large-rect-20=%d\n", treeLargeRect16, treeLargeRect20);
		matrixDetails.AppendFormat("tree-dual-bitmap=%d\ntree-dual-larger-ico=%d\ntree-bitmap-only-16=%d\ntree-bitmap-only-20=%d\ntree-rect-16=%d\ntree-rect-20=%d\n", treeBitmapPreferred, treeLargeIconPreferred, treeBitmapOnly16, treeBitmapOnly20, treeRect16, treeRect20);
		iconTree.UpdateScriptDpiMetrics(UiMetrics::DpiForWindow(iconTree));
		iconTree.SelectItem(iconTree.FindScriptTreeItem(L"runtimesize.js"));
		const bool favoriteReady = iconTree.ExecuteScriptPopupCommand(NavigationPopupToggleFavorite);
		const HTREEITEM scriptItem = iconTree.FindScriptTreeItem(L"runtimesize.js");
		iconTree.SelectItem(scriptItem);
		const HTREEITEM folderItem = iconTree.FindScriptTreeItem(L"iconfolder");
		if(folderItem) iconTree.Expand(folderItem, TVE_EXPAND);
		auto commands = [&]() {
			std::vector<std::vector<int>> result;
			auto capture = [&](HWND window) {
				std::vector<int> ids; CToolBarCtrl toolbar(window);
				for(int i = 0; i < toolbar.GetButtonCount(); ++i) { TBBUTTON button = {}; toolbar.GetButton(i, &button); ids.push_back(button.idCommand); }
				result.push_back(ids);
			};
			capture(m_ScriptsToolbar);
			for(const auto& runtime : m_scriptToolbars.Items()) if(::IsWindow(runtime.window)) capture(runtime.window);
			return result;
		};
		const auto originalCommands = commands();
		const CString originalFavorites = _Settings.GetFavoriteScripts();
		auto runScript = [&]() {
			if(!m_doc || !scriptCommand) return false;
			m_doc->m_body.Document()->title = L"Before command";
			SendMessage(WM_COMMAND, scriptCommand);
			return CString(static_cast<LPCWSTR>(m_doc->m_body.Document()->title)) == L"Icon size command passed";
		};
		const bool scriptBeforeResize = runScript();
		bool sizesPassed = fixtureTreePixels && treeBitmapPreferred && treeLargeIconPreferred && treeBitmapOnly16 && treeBitmapOnly20 && treeRect16 && treeRect20 && scriptItem != NULL && folderItem != NULL && favoriteReady && iconTree.IsFavoriteScript(scriptUid) && scriptBeforeResize;
		DWORD gdiWarm = 0, userWarm = 0, gdiEnd = 0, userEnd = 0;
		for(int cycle = 0; cycle < 100; ++cycle) {
			const int sizes[] = {20, 24, 32, 16}; const int size = sizes[cycle % 4];
			_Settings.SetScriptIconSize(size);
			sizesPassed = ApplyScriptIconSize() && sizesPassed;
			int width = 0, height = 0;
			const int expected = UiMetrics::ScaleForDpi(size, UiMetrics::DpiForWindow(m_hWnd));
			sizesPassed = ::ImageList_GetIconSize(m_scriptsToolbarImages, &width, &height) && width == expected && height == expected && sizesPassed;
			sizesPassed = ::ImageList_GetIconSize(iconTree.ScriptImageList(), &width, &height) && width == expected && height == expected && sizesPassed;
			sizesPassed = iconTree.GetSelectedItem() == scriptItem && commands() == originalCommands && sizesPassed;
			sizesPassed = folderItem != NULL && (iconTree.GetItemState(folderItem, TVIS_EXPANDED) & TVIS_EXPANDED) != 0 && sizesPassed;
			int treeImage = 0, treeRow = 0, treeIndent = 0; bool expanders = false;
			sizesPassed = iconTree.GetScriptTreeMetrics(treeImage, treeRow, treeIndent, expanders) && treeImage == expected &&
				_Settings.GetFavoriteScripts() == originalFavorites && sizesPassed;
			const HIMAGELIST commandsImages = reinterpret_cast<HIMAGELIST>(m_CmdToolbar.SendMessage(TB_GETIMAGELIST));
			sizesPassed = ::ImageList_GetIconSize(commandsImages, &width, &height) &&
				width == ToolbarFactory::CommandToolbarImageSize(UiMetrics::DpiForWindow(m_hWnd)) && sizesPassed;
			for(const auto& runtime : m_scriptToolbars.Items()) if(runtime.definition.id != L"scripts-main" && ::IsWindow(runtime.window))
				sizesPassed = ::ImageList_GetIconSize(runtime.images, &width, &height) && width == expected && height == expected && sizesPassed;
			if(cycle == 0) matrixDetails.AppendFormat("first-switch=%d,tree-item:%d,script-before:%d\n", sizesPassed, scriptItem != NULL, scriptBeforeResize);
			::GdiFlush();
			if(cycle == 19) { gdiWarm = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS); userWarm = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS); }
		}
		gdiEnd = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS); userEnd = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
		const bool scriptAfterResize = runScript();
		matrixDetails.AppendFormat("script-before=%d\nscript-after=%d\n", scriptBeforeResize, scriptAfterResize);
		sizesPassed = sizesPassed && gdiWarm > 0 && userWarm > 0 && gdiEnd <= gdiWarm + 4 && userEnd <= userWarm + 2 && scriptAfterResize;
		_Settings.SetScriptIconSize(32); sizesPassed = ApplyScriptIconSize() && sizesPassed; _Settings.Save();
		const bool refreshedCatalog = InitializeScripts();
		int refreshedWidth = 0, refreshedHeight = 0;
		const int refreshedExpected = UiMetrics::ScaleForDpi(32, UiMetrics::DpiForWindow(m_hWnd));
		const HTREEITEM refreshedFolder = iconTree.FindScriptTreeItem(L"iconfolder");
		const bool catalogPassed = refreshedCatalog && runScript() && commands() == originalCommands && iconTree.IsFavoriteScript(scriptUid) &&
			refreshedFolder != NULL && (iconTree.GetItemState(refreshedFolder, TVIS_EXPANDED) & TVIS_EXPANDED) != 0 &&
			::ImageList_GetIconSize(m_scriptsToolbarImages, &refreshedWidth, &refreshedHeight) && refreshedWidth == refreshedExpected && refreshedHeight == refreshedExpected;
		sizesPassed = catalogPassed && sizesPassed;
		matrixDetails.AppendFormat("catalog-after-resize=%d\n", catalogPassed);
		matrixDetails.AppendFormat("initial-icon-size=%d\nsize-switches=100\nsize-switches-pass=%d\ngdi-warm=%lu\ngdi-end=%lu\nuser-warm=%lu\nuser-end=%lu\n", initialIconSize, sizesPassed, gdiWarm, gdiEnd, userWarm, userEnd);
		const bool passed = created && added && hiddenOk && shown && showOneRow && bandBefore != 0 && bandAfter != 0 && duplicateIds == 0 && restarted && dpiMatrix && restoredDpi && sizesPassed;
		CStringA report; report.Format("phase=script-toolbar-runtime-size\ncreated=%d\nadd-script=%d\nhide-result=%d\nshow-result=%d\nshow-one-row=%d\nband-before=%u\nband-after=%u\nduplicate-band-ids=%d\nhide-show=%d\nrestart=%d\nscript-command=%u\ndpi-matrix=%d\ndpi-restored=%d\nresult=%s\n", created, added, hiddenOk, shown, showOneRow, bandBefore, bandAfter, duplicateIds, hiddenOk && shown, restarted, scriptCommand, dpiMatrix, restoredDpi, passed ? "pass" : "fail");
		report += matrixDetails;
		WritePortableStateTestText(reportPath, report); PostMessage(WM_CLOSE); return;
	}
	if(navigationScriptsReloadRuntime)
	{
		const bool initialized = InitializeScripts(); RefreshNavigationScriptTree(); CTreeView& tree = m_document_tree.m_tree.m_tree;
		CString deepUid; for(int index = 0; index < m_scripts.Menu().Count(); ++index) if(m_scripts.Menu().Item(index).relativePath == L"foldera/folderb/deep.js") deepUid = m_scripts.Menu().Item(index).uid;
		PortableToolbarLayout persisted; PortableToolbarStore::Load(persisted); ScriptToolbarDefinition* toolbar = NULL;
		for(size_t toolbarIndex = 0; toolbarIndex < persisted.scriptToolbars.size(); ++toolbarIndex) if(persisted.scriptToolbars[toolbarIndex].id == L"navigation-runtime-toolbar") { toolbar = &persisted.scriptToolbars[toolbarIndex]; break; }
		bool uidRestored = false;
		for(size_t index = 0; toolbar != NULL && toolbar->name == L"Navigation renamed" && index < toolbar->items.size(); ++index) if(toolbar->items[index].scriptUid == deepUid) { uidRestored = true; break; }
		const bool modeRestored = _Settings.DocumentTreeScripts() && tree.IsScriptMode(); const bool treeRestored = tree.ScriptTreeNodeCount() == 5 && tree.FindScriptTreeItem(L"foldera/folderb/deep.js") != NULL;
		const bool restored = initialized && modeRestored && treeRestored && uidRestored;
		CStringA report; report.Format("phase=navigation-scripts-reload\ninitialized=%d\nmode=%d\ntree=%d\nuid=%d\nrestored=%d\nresult=%s\n", initialized, modeRestored, treeRestored, uidRestored, restored, restored ? "pass" : "fail"); WritePortableStateTestText(reportPath, report); PostMessage(WM_CLOSE); return;
	}
	if(navigationViewBarElementsRuntime)
	{
		m_document_tree.m_tree.RefreshLocalizedMenuCaptions();
		const CString expected = FbeLoadRuntimeString(IDS_DOCTREE_MENU_ELEMENTS);
		CString text; int width = 0, measuredWidth = 0, padding = 0, actualHeight = 0, minimumHeight = 0;
		auto viewBarReady = [&]() { return m_document_tree.m_tree.GetViewBarElementProbe(text, width, measuredWidth, padding, actualHeight, minimumHeight) && text == expected && width >= measuredWidth + padding && actualHeight >= minimumHeight; };
		const InterfaceTheme originalTheme = ThemeManager::GetSelectedTheme();
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_LIGHT);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		const bool lightBeforeCheckmarks = m_document_tree.m_tree.GetStructureMenuCheckmarkProbe(false);
		const bool lightBefore = viewBarReady() && lightBeforeCheckmarks;
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		const bool popupPreparedDark = m_document_tree.m_tree.PrepareViewBarPopupThemeProbe();
		const bool darkCheckmarksAfterPopup = m_document_tree.m_tree.GetStructureMenuCheckmarkProbe(true);
		const bool dpiCheckmarks = m_document_tree.m_tree.VerifyStructureMenuCheckmarkDpiBitmaps();
		const bool dark = ThemeManager::IsDark() && viewBarReady() && darkCheckmarksAfterPopup;
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_LIGHT);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		const bool lightAfterCheckmarks = m_document_tree.m_tree.GetStructureMenuCheckmarkProbe(false);
		const bool lightAfter = !ThemeManager::IsDark() && viewBarReady() && lightAfterCheckmarks;
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		const bool popupPreparedDarkAgain = m_document_tree.m_tree.PrepareViewBarPopupThemeProbe();
		const bool darkAgainCheckmarks = m_document_tree.m_tree.GetStructureMenuCheckmarkProbe(true);
		ThemeManager::SetSelectedTheme(originalTheme);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		const bool passed = lightBefore && dark && popupPreparedDark && dpiCheckmarks && lightAfter && popupPreparedDarkAgain && darkAgainCheckmarks;
		CStringA report; report.Format("phase=navigation-viewbar-elements\nviewbar-elements=%d\nviewbar-light-before=%d\nviewbar-dark=%d\nviewbar-popup-prepared-dark=%d\nviewbar-light-after=%d\nviewbar-popup-prepared-dark-again=%d\nviewbar-checkmarks-light-before=%d\nviewbar-checkmarks-dark-after-popup=%d\nviewbar-checkmarks-light-after=%d\nviewbar-checkmarks-dark-again=%d\nviewbar-checkmarks-dpi-100-125-150-200=%d\nviewbar-width=%d\nviewbar-measured-width=%d\nviewbar-padding=%d\nviewbar-height=%d\nviewbar-minimum-height=%d\nresult=%s\n", passed, lightBefore, dark, popupPreparedDark, lightAfter, popupPreparedDarkAgain, lightBeforeCheckmarks, darkCheckmarksAfterPopup, lightAfterCheckmarks, darkAgainCheckmarks, dpiCheckmarks, width, measuredWidth, padding, actualHeight, minimumHeight, passed ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report); PostMessage(WM_CLOSE); return;
	}
	if(navigationFavoritesReadRuntime)
	{
		_Settings.SetScriptsFolder(scriptsDirectory, true);
		const bool initialized = InitializeScripts();
		RefreshNavigationScriptTree();
		if(!m_document_tree.m_tree.m_tree.IsScriptMode()) m_document_tree.m_tree.ToggleScriptMode();
		CTreeView& tree = m_document_tree.m_tree.m_tree;
		CString uid;
		for(int index = 0; index < m_scripts.Menu().Count(); ++index)
			if(m_scripts.Menu().Item(index).relativePath == L"02_чистка/dashrenamed.js") uid = m_scripts.Menu().Item(index).uid;
		const bool persisted = initialized && !uid.IsEmpty() && tree.IsFavoriteScript(uid) && tree.FavoriteScriptTreeItem(uid) != NULL;
		const bool malformedIgnored = tree.ScriptTreeNodeCount() == 8;
		CStringA report; report.Format("phase=navigation-favorites-read\npersisted=%d\nmalformed-ignored=%d\nresult=%s\n", persisted, malformedIgnored, persisted && malformedIgnored ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report); PostMessage(WM_CLOSE); return;
	}
	if(navigationScriptsMultiRuntime)
	{
		_Settings.SetScriptsFolder(scriptsDirectory, true);
		_Settings.SetFavoriteScripts(CString(), true);
		_Settings.SetDocumentTreeScripts(true, true);
		const bool initialized = InitializeScripts();
		RefreshNavigationScriptTree();
		CTreeView& tree = m_document_tree.m_tree.m_tree;
		if(!tree.IsScriptMode()) m_document_tree.m_tree.ToggleScriptMode();
		tree.SetScriptFilter(L"иллюстрации");
		const bool folderNameExcluded = tree.ScriptTreeNodeCount() == 0;
		tree.SetScriptFilter(L"captions");
		const bool filenameMatch = tree.ScriptTreeNodeCount() == 2 && tree.FindScriptTreeItem(L"иллюстрации/captions.js") != NULL &&
			tree.HasScriptTreeParent(L"иллюстрации/captions.js", L"иллюстрации") && tree.FindScriptTreeItem(L"иллюстрации/move image.js") == NULL;
		tree.SetScriptFilter(CString());
		HTREEITEM a = tree.FindScriptTreeItem(L"a.js"), b = tree.FindScriptTreeItem(L"b.js"), c = tree.FindScriptTreeItem(L"c.js");
		CString uidA, uidB, uidC;
		for(int index = 0; index < m_scripts.Menu().Count(); ++index)
		{
			const ScriptDescriptor& script = m_scripts.Menu().Item(index);
			if(script.relativePath == L"a.js") uidA = script.uid;
			if(script.relativePath == L"b.js") uidB = script.uid;
			if(script.relativePath == L"c.js") uidC = script.uid;
		}
		if(a != NULL) tree.SelectItem(a);
		for(HTREEITEM item : { b, c })
		{
			RECT bounds = {};
			if(item != NULL && tree.GetItemRect(item, &bounds, TRUE))
				::SendMessage(tree.m_hWnd, WM_LBUTTONDOWN, MK_CONTROL, MAKELPARAM(bounds.left + 3, bounds.top + 3));
		}
		const std::vector<const ScriptDescriptor*> selected = tree.SelectedScripts();
		const bool threeSelected = selected.size() == 3 && selected[0]->uid == uidA && selected[1]->uid == uidB && selected[2]->uid == uidC;
		RECT bBounds = {};
		const UINT popupsBefore = tree.ScriptPopupInvocations();
		if(b != NULL && tree.GetItemRect(b, &bBounds, TRUE))
		{
			::SendMessage(tree.m_hWnd, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(bBounds.left + 3, bBounds.top + 3));
			const UINT_PTR timer = ::SetTimer(NULL, 0, 100, CloseNavigationPopupForTest);
			::SendMessage(tree.m_hWnd, WM_RBUTTONUP, 0, MAKELPARAM(bBounds.left + 3, bBounds.top + 3));
			if(timer != 0) ::KillTimer(NULL, timer);
		}
		const bool rightClickPreserves = threeSelected && tree.ScriptPopupInvocations() == popupsBefore + 1 && tree.SelectedScripts().size() == 3 &&
			!tree.ScriptPopupRunEnabled() && !tree.ScriptPopupOpenEnabled();
		const bool unsafeDisabled = !tree.ExecuteScriptPopupCommand(NavigationPopupRunScript) && !tree.ExecuteScriptPopupCommand(NavigationPopupOpenLocation);
		const bool favoritesAdded = tree.ExecuteScriptPopupCommand(NavigationPopupToggleFavorite) &&
			tree.IsFavoriteScript(uidA) && tree.IsFavoriteScript(uidB) && tree.IsFavoriteScript(uidC);
		a = tree.FindScriptTreeItem(L"a.js");
		if(a != NULL) tree.SelectItem(a);
		const bool oneRemoved = tree.ExecuteScriptPopupCommand(NavigationPopupToggleFavorite) && !tree.IsFavoriteScript(uidA) &&
			tree.IsFavoriteScript(uidB) && tree.IsFavoriteScript(uidC);
		a = tree.FindScriptTreeItem(L"a.js"); b = tree.FindScriptTreeItem(L"b.js"); c = tree.FindScriptTreeItem(L"c.js");
		if(a != NULL) tree.SelectItem(a);
		if(b != NULL) tree.SetItemState(b, TVIS_SELECTED, TVIS_SELECTED);
		if(c != NULL) tree.SetItemState(c, TVIS_SELECTED, TVIS_SELECTED);
		const bool mixedFavoriteAdd = oneRemoved && !tree.SelectedScriptsAreAllFavorites() &&
			tree.ExecuteScriptPopupCommand(NavigationPopupToggleFavorite) && tree.IsFavoriteScript(uidA) &&
			tree.IsFavoriteScript(uidB) && tree.IsFavoriteScript(uidC);
		const HTREEITEM group = tree.FavoriteGroupItem();
		wchar_t groupLabel[128] = {};
		if(group != NULL) tree.GetItemText(group, groupLabel, _countof(groupLabel));
		const bool dedicatedImage = group != NULL && tree.FavoriteImageIndex() == 1 && tree.ScriptTreeImage(group) == tree.FavoriteImageIndex() &&
			tree.ScriptTreeImage(group) != tree.ScriptTreeImage(tree.FindScriptTreeItem(L"a.js")) && CString(groupLabel).Find(L'★') < 0;
		auto iconVisibleOn = [&](COLORREF background) {
			BITMAPINFO info = {}; info.bmiHeader.biSize = sizeof(info.bmiHeader); info.bmiHeader.biWidth = 20; info.bmiHeader.biHeight = -20;
			info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
			void* bits = NULL; HBITMAP bitmap = ::CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &bits, NULL, 0);
			HDC dc = bitmap != NULL ? ::CreateCompatibleDC(NULL) : NULL;
			if(dc == NULL) { if(bitmap != NULL) ::DeleteObject(bitmap); return false; }
			HGDIOBJ oldBitmap = ::SelectObject(dc, bitmap);
			HBRUSH brush = ::CreateSolidBrush(background); RECT bounds = { 0, 0, 20, 20 };
			::FillRect(dc, &bounds, brush); ::DeleteObject(brush);
			const bool drawn = ::ImageList_Draw(tree.ScriptImageList(), tree.FavoriteImageIndex(), dc, 0, 0, ILD_TRANSPARENT) != FALSE;
			const COLORREF center = ::GetPixel(dc, 10, 10);
			::SelectObject(dc, oldBitmap); ::DeleteDC(dc); ::DeleteObject(bitmap);
			return drawn && center != background;
		};
		const bool iconLightDark = dedicatedImage && iconVisibleOn(RGB(255, 255, 255)) && iconVisibleOn(RGB(32, 32, 32));
		a = tree.FindScriptTreeItem(L"a.js"); b = tree.FindScriptTreeItem(L"b.js"); c = tree.FindScriptTreeItem(L"c.js");
		if(a != NULL) tree.SelectItem(a);
		if(b != NULL) tree.SetItemState(b, TVIS_SELECTED, TVIS_SELECTED);
		if(c != NULL) tree.SetItemState(c, TVIS_SELECTED, TVIS_SELECTED);
		const UINT popupsBeforeKeyboard = tree.ScriptPopupInvocations();
		const UINT_PTR keyboardTimer = ::SetTimer(NULL, 0, 100, CloseNavigationPopupForTest);
		::SendMessage(tree.m_hWnd, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(tree.m_hWnd), MAKELPARAM(-1, -1));
		if(keyboardTimer != 0) ::KillTimer(NULL, keyboardTimer);
		const bool keyboardContextPreserves = tree.ScriptPopupInvocations() == popupsBeforeKeyboard + 1 && tree.SelectedScripts().size() == 3 &&
			tree.ScriptPopupFavoriteLabel() == FbeLoadRuntimeStringByKey(L"fbe.document_tree.scripts.remove_favorite", L"★ Remove from favorites");
		const HTREEITEM displayFolder = tree.FindScriptTreeItem(L"иллюстрации");
		if(displayFolder != NULL) tree.SetItemState(displayFolder, TVIS_SELECTED, TVIS_SELECTED);
		if(tree.FavoriteGroupItem() != NULL) tree.SetItemState(tree.FavoriteGroupItem(), TVIS_SELECTED, TVIS_SELECTED);
		const bool virtualNodesExcluded = displayFolder != NULL && tree.SelectedScripts().size() == 3;
		if(displayFolder != NULL) tree.SetItemState(displayFolder, 0, TVIS_SELECTED);
		if(tree.FavoriteGroupItem() != NULL) tree.SetItemState(tree.FavoriteGroupItem(), 0, TVIS_SELECTED);
		const bool duplicateRepresentation = tree.FavoriteScriptTreeItem(uidA) != NULL &&
			(tree.SetItemState(tree.FavoriteScriptTreeItem(uidA), TVIS_SELECTED, TVIS_SELECTED), tree.SelectedScripts().size() == 3);
		std::vector<ScriptTreeToolbarTarget> actualTargets;
		for(const auto& toolbar : m_scriptToolbars.Items()) { ScriptTreeToolbarTarget target; target.id = toolbar.definition.id; target.name = toolbar.definition.name; actualTargets.push_back(target); }
		ScriptTreeToolbarTarget firstTarget; firstTarget.id = L"scripts-main"; firstTarget.name = L"Main renamed";
		ScriptTreeToolbarTarget secondTarget; secondTarget.id = L"test-hidden"; secondTarget.name = L"Hidden renamed";
		auto buildToolbarMenu = [&]() { HMENU menu = ::CreatePopupMenu(); if(menu != NULL) tree.AppendScriptToolbarMenu(menu); return menu; };
		tree.SetScriptToolbarTargets(std::vector<ScriptTreeToolbarTarget>{ firstTarget });
		HMENU singleMenu = buildToolbarMenu();
		const bool singleMenuDirect = singleMenu != NULL && ::GetMenuItemCount(singleMenu) == 1 && ::GetSubMenu(singleMenu, 0) == NULL && ::GetMenuItemID(singleMenu, 0) == NavigationPopupAddToolbarBase;
		if(singleMenu != NULL) ::DestroyMenu(singleMenu);
		tree.SetScriptToolbarTargets(std::vector<ScriptTreeToolbarTarget>{ firstTarget, secondTarget });
		HMENU multiMenu = buildToolbarMenu(); HMENU submenu = multiMenu == NULL ? NULL : ::GetSubMenu(multiMenu, 0);
		wchar_t firstLabel[128] = {}, secondLabel[128] = {};
		if(submenu != NULL) { ::GetMenuString(submenu, 0, firstLabel, _countof(firstLabel), MF_BYPOSITION); ::GetMenuString(submenu, 1, secondLabel, _countof(secondLabel), MF_BYPOSITION); }
		const bool multiMenuNamed = submenu != NULL && ::GetMenuItemCount(submenu) == 2 && CString(firstLabel) == firstTarget.name && CString(secondLabel) == secondTarget.name && ::GetMenuItemID(submenu, 0) == NavigationPopupAddToolbarBase && ::GetMenuItemID(submenu, 1) == NavigationPopupAddToolbarBase + 1;
		if(multiMenu != NULL) ::DestroyMenu(multiMenu);
		tree.SetScriptToolbarTargets(actualTargets);
		ScriptToolbarRuntime* mainToolbar = m_scriptToolbars.Find(L"scripts-main");
		const size_t beforeCount = mainToolbar == NULL ? 0 : mainToolbar->definition.items.size();
		const bool invalidRejected = mainToolbar != NULL && !AddScriptsToToolbar(std::vector<CString>{ uidA, L"missing-script-uid" }, L"scripts-main") &&
			mainToolbar->definition.items.size() == beforeCount;
		const bool addedToToolbar = mainToolbar != NULL && tree.ExecuteScriptPopupCommand(NavigationPopupAddToolbarBase);
		mainToolbar = m_scriptToolbars.Find(L"scripts-main");
		bool toolbarOrder = addedToToolbar && mainToolbar != NULL && mainToolbar->definition.items.size() == beforeCount + 3;
		if(toolbarOrder)
		{
			const std::vector<PortableToolbarItem>& items = mainToolbar->definition.items;
			toolbarOrder = items[beforeCount].scriptUid == uidA && items[beforeCount + 1].scriptUid == uidB && items[beforeCount + 2].scriptUid == uidC;
		}
		const bool noDuplicates = toolbarOrder && tree.ExecuteScriptPopupCommand(NavigationPopupAddToolbarBase) &&
			mainToolbar->definition.items.size() == beforeCount + 3;
		PortableToolbarLayout persisted;
		bool toolbarPersisted = noDuplicates && PortableToolbarStore::Load(persisted);
		bool foundPersistedToolbar = false;
		for(size_t index = 0; toolbarPersisted && index < persisted.scriptToolbars.size(); ++index)
			if(persisted.scriptToolbars[index].id == L"scripts-main")
			{
				foundPersistedToolbar = true;
				const std::vector<PortableToolbarItem>& items = persisted.scriptToolbars[index].items;
				toolbarPersisted = items.size() == beforeCount + 3 && items[beforeCount].scriptUid == uidA &&
					items[beforeCount + 1].scriptUid == uidB && items[beforeCount + 2].scriptUid == uidC;
				break;
			}
		toolbarPersisted = toolbarPersisted && foundPersistedToolbar;
		std::vector<ScriptToolbarDefinition> beforeInsert, withInsert;
		for(const auto& toolbar : m_scriptToolbars.Items()) beforeInsert.push_back(toolbar.definition);
		withInsert = beforeInsert;
		ScriptToolbarDefinition insertTarget; insertTarget.id = L"navigation-insertion-test"; insertTarget.name = L"Insertion test"; insertTarget.visible = true;
		PortableToolbarItem separator = {}; separator.separator = true;
		PortableToolbarItem existing = {}; existing.scriptUid = uidC;
		insertTarget.items.push_back(separator); insertTarget.items.push_back(existing); withInsert.push_back(insertTarget);
		bool insertedAtPosition = ApplyScriptToolbarDefinitions(beforeInsert, withInsert) &&
			AddScriptsToToolbar(std::vector<CString>{ uidA, uidB, uidC }, insertTarget.id, 1);
		const ScriptToolbarRuntime* insertionRuntime = m_scriptToolbars.Find(insertTarget.id);
		insertedAtPosition = insertedAtPosition && insertionRuntime != NULL && insertionRuntime->definition.items.size() == 4 &&
			insertionRuntime->definition.items[0].separator && insertionRuntime->definition.items[1].scriptUid == uidA &&
			insertionRuntime->definition.items[2].scriptUid == uidB && insertionRuntime->definition.items[3].scriptUid == uidC;
		PortableToolbarLayout insertedLayout;
		bool insertedPersisted = insertedAtPosition && PortableToolbarStore::Load(insertedLayout);
		bool foundInsert = false;
		for(const auto& toolbar : insertedLayout.scriptToolbars) if(toolbar.id == insertTarget.id)
		{
			foundInsert = true;
			insertedPersisted = insertedPersisted && toolbar.items.size() == 4 && toolbar.items[0].separator &&
				toolbar.items[1].scriptUid == uidA && toolbar.items[2].scriptUid == uidB && toolbar.items[3].scriptUid == uidC;
		}
		insertedPersisted = insertedPersisted && foundInsert;
		std::vector<ScriptToolbarDefinition> beforeDrag, withDrag;
		for(const auto& toolbar : m_scriptToolbars.Items()) beforeDrag.push_back(toolbar.definition);
		withDrag = beforeDrag;
		ScriptToolbarDefinition dragTarget; dragTarget.id = L"navigation-drag-test"; dragTarget.name = L"Drag test"; dragTarget.visible = true;
		dragTarget.items.push_back(separator); dragTarget.items.push_back(existing); withDrag.push_back(dragTarget);
		ScriptToolbarDefinition emptyTarget; emptyTarget.id = L"navigation-empty-drag-test"; emptyTarget.name = L"Empty drag test"; emptyTarget.visible = true; withDrag.push_back(emptyTarget);
		const bool dragSetup = ApplyScriptToolbarDefinitions(beforeDrag, withDrag);
		const ScriptToolbarRuntime* dragRuntime = m_scriptToolbars.Find(dragTarget.id);
		RECT firstButton = {}; POINT dragPoint = {};
		const bool dragGeometry = dragSetup && dragRuntime != NULL && ::IsWindowVisible(dragRuntime->window) &&
			::SendMessage(dragRuntime->window, TB_GETITEMRECT, 0, reinterpret_cast<LPARAM>(&firstButton));
		if(dragGeometry) { dragPoint.x = firstButton.left + 1; dragPoint.y = (firstButton.top + firstButton.bottom) / 2; ::ClientToScreen(dragRuntime->window, &dragPoint); ::ScreenToClient(tree.m_hWnd, &dragPoint); }
		POINT hiddenPoint = dragPoint; if(dragGeometry) ::ClientToScreen(tree.m_hWnd, &hiddenPoint);
		if(dragGeometry) ::ShowWindow(dragRuntime->window, SW_HIDE);
		if(dragGeometry) HandleScriptTreeDrag(1, hiddenPoint, std::vector<CString>{ uidA });
		const bool hiddenDropRejected = dragGeometry && dragRuntime->definition.items.size() == 2;
		if(dragGeometry) ::ShowWindow(dragRuntime->window, SW_SHOW);
		a = tree.FindScriptTreeItem(L"a.js"); b = tree.FindScriptTreeItem(L"b.js"); c = tree.FindScriptTreeItem(L"c.js");
		if(a != NULL) tree.SelectItem(a);
		if(b != NULL) tree.SetItemState(b, TVIS_SELECTED, TVIS_SELECTED);
		if(c != NULL) tree.SetItemState(c, TVIS_SELECTED, TVIS_SELECTED);
		NMTREEVIEW dragEvent = {}; dragEvent.itemNew.hItem = a; BOOL dragEventHandled = FALSE;
		if(dragGeometry) tree.OnBegindrag(0, reinterpret_cast<LPNMHDR>(&dragEvent), dragEventHandled);
		BOOL moveHandled = FALSE;
		if(dragGeometry) tree.OnMouseMove(WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(dragPoint.x, dragPoint.y), moveHandled);
		TBINSERTMARK mark = {}; mark.iButton = -2;
		if(dragGeometry) ::SendMessage(dragRuntime->window, TB_GETINSERTMARK, 0, reinterpret_cast<LPARAM>(&mark));
		const bool dragMark = dragGeometry && dragEventHandled && moveHandled && mark.iButton == 0;
		const ScriptToolbarRuntime* secondRuntime = m_scriptToolbars.Find(insertTarget.id);
		RECT secondButton = {}; POINT secondPoint = {};
		const bool secondGeometry = secondRuntime != NULL && ::IsWindowVisible(secondRuntime->window) &&
			::SendMessage(secondRuntime->window, TB_GETITEMRECT, 0, reinterpret_cast<LPARAM>(&secondButton));
		if(secondGeometry) { secondPoint.x = secondButton.left + 1; secondPoint.y = (secondButton.top + secondButton.bottom) / 2; ::ClientToScreen(secondRuntime->window, &secondPoint); ::ScreenToClient(tree.m_hWnd, &secondPoint); tree.OnMouseMove(WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(secondPoint.x, secondPoint.y), moveHandled); }
		TBINSERTMARK oldMark = {}, newMark = {}; oldMark.iButton = newMark.iButton = -2;
		if(dragGeometry) ::SendMessage(dragRuntime->window, TB_GETINSERTMARK, 0, reinterpret_cast<LPARAM>(&oldMark));
		if(secondGeometry) ::SendMessage(secondRuntime->window, TB_GETINSERTMARK, 0, reinterpret_cast<LPARAM>(&newMark));
		const bool dragSwitched = dragMark && secondGeometry && oldMark.iButton == -1 && newMark.iButton == 0;
		if(dragGeometry) tree.OnMouseMove(WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(dragPoint.x, dragPoint.y), moveHandled);
		BOOL cancelHandled = FALSE; if(dragGeometry) tree.OnKeyDown(WM_KEYDOWN, VK_ESCAPE, 0, cancelHandled);
		if(dragGeometry) ::SendMessage(dragRuntime->window, TB_GETINSERTMARK, 0, reinterpret_cast<LPARAM>(&mark));
		const bool dragCanceled = dragMark && cancelHandled && mark.iButton == -1 && dragRuntime->definition.items.size() == 2;
		if(dragGeometry) tree.OnBegindrag(0, reinterpret_cast<LPNMHDR>(&dragEvent), dragEventHandled);
		if(dragGeometry) tree.OnMouseMove(WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(dragPoint.x, dragPoint.y), moveHandled);
		if(dragGeometry && ::GetCapture() == tree.m_hWnd) ::ReleaseCapture();
		if(dragGeometry) ::SendMessage(dragRuntime->window, TB_GETINSERTMARK, 0, reinterpret_cast<LPARAM>(&mark));
		const bool dragCaptureCanceled = dragGeometry && mark.iButton == -1 && dragRuntime->definition.items.size() == 2;
		if(dragGeometry) tree.OnBegindrag(0, reinterpret_cast<LPNMHDR>(&dragEvent), dragEventHandled);
		if(dragGeometry) tree.OnMouseMove(WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(dragPoint.x, dragPoint.y), moveHandled);
		POINT outside = { -32000, -32000 };
		if(dragGeometry) tree.OnMouseMove(WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(outside.x, outside.y), moveHandled);
		if(dragGeometry) ::SendMessage(dragRuntime->window, TB_GETINSERTMARK, 0, reinterpret_cast<LPARAM>(&mark));
		BOOL outsideHandled = FALSE; if(dragGeometry) tree.OnLButtonUp(WM_LBUTTONUP, 0, MAKELPARAM(outside.x, outside.y), outsideHandled);
		const bool dragOutsideCanceled = dragGeometry && outsideHandled && mark.iButton == -1 && dragRuntime->definition.items.size() == 2;
		if(dragGeometry) tree.OnBegindrag(0, reinterpret_cast<LPNMHDR>(&dragEvent), dragEventHandled);
		BOOL dropHandled = FALSE; if(dragGeometry) tree.OnLButtonUp(WM_LBUTTONUP, 0, MAKELPARAM(dragPoint.x, dragPoint.y), dropHandled);
		dragRuntime = m_scriptToolbars.Find(dragTarget.id);
		const bool dragDropped = dragCanceled && dropHandled && dragRuntime != NULL && dragRuntime->definition.items.size() == 4 &&
			dragRuntime->definition.items[0].scriptUid == uidA && dragRuntime->definition.items[1].scriptUid == uidB &&
			dragRuntime->definition.items[2].separator && dragRuntime->definition.items[3].scriptUid == uidC;
		PortableToolbarLayout dragLayout; bool dragPersisted = dragDropped && PortableToolbarStore::Load(dragLayout); bool foundDrag = false;
		for(const auto& toolbar : dragLayout.scriptToolbars) if(toolbar.id == dragTarget.id)
		{
			foundDrag = true; dragPersisted = dragPersisted && toolbar.items.size() == 4 && toolbar.items[0].scriptUid == uidA &&
				toolbar.items[1].scriptUid == uidB && toolbar.items[2].separator && toolbar.items[3].scriptUid == uidC;
		}
		dragPersisted = dragPersisted && foundDrag;
		const ScriptToolbarRuntime* emptyRuntime = m_scriptToolbars.Find(emptyTarget.id);
		RECT emptyRect = {};
		const bool emptyGeometry = emptyRuntime != NULL && ::IsWindowVisible(emptyRuntime->window) &&
			::GetWindowRect(emptyRuntime->window, &emptyRect) && emptyRect.right > emptyRect.left && emptyRect.bottom > emptyRect.top;
		POINT emptyPoint = { emptyRect.left + (emptyRect.right - emptyRect.left) / 2, emptyRect.top + (emptyRect.bottom - emptyRect.top) / 2 };
		if(emptyGeometry) ::ScreenToClient(tree.m_hWnd, &emptyPoint);
		if(emptyGeometry) tree.OnBegindrag(0, reinterpret_cast<LPNMHDR>(&dragEvent), dragEventHandled);
		if(emptyGeometry) tree.OnMouseMove(WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(emptyPoint.x, emptyPoint.y), moveHandled);
		const bool emptyMark = emptyGeometry && ::IsWindow(m_scriptTreeEmptyMarker) && ::IsWindowVisible(m_scriptTreeEmptyMarker) && ::GetParent(m_scriptTreeEmptyMarker) == emptyRuntime->window;
		const HWND emptyMarkerWindow = m_scriptTreeEmptyMarker;
		if(emptyGeometry) tree.OnMouseMove(WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(emptyPoint.x, emptyPoint.y), moveHandled);
		const bool emptyMarkStable = emptyMark && m_scriptTreeEmptyMarker == emptyMarkerWindow && ::IsWindow(emptyMarkerWindow);
		BOOL emptyCancelHandled = FALSE; if(emptyGeometry) tree.OnKeyDown(WM_KEYDOWN, VK_ESCAPE, 0, emptyCancelHandled);
		const bool emptyMarkCanceled = emptyMarkStable && emptyCancelHandled && !::IsWindow(emptyMarkerWindow) && m_scriptTreeEmptyMarker == NULL && emptyRuntime->definition.items.empty();
		if(emptyGeometry) tree.OnBegindrag(0, reinterpret_cast<LPNMHDR>(&dragEvent), dragEventHandled);
		if(emptyGeometry) tree.OnMouseMove(WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(emptyPoint.x, emptyPoint.y), moveHandled);
		BOOL emptyDropHandled = FALSE; if(emptyGeometry) tree.OnLButtonUp(WM_LBUTTONUP, 0, MAKELPARAM(emptyPoint.x, emptyPoint.y), emptyDropHandled);
		emptyRuntime = m_scriptToolbars.Find(emptyTarget.id);
		const bool emptyDrop = emptyGeometry && emptyDropHandled && emptyRuntime != NULL && emptyRuntime->definition.items.size() == 3 &&
			emptyRuntime->definition.items[0].scriptUid == uidA && emptyRuntime->definition.items[1].scriptUid == uidB && emptyRuntime->definition.items[2].scriptUid == uidC;
		const bool removeAction = tree.SelectedScriptsAreAllFavorites();
		const bool favoritesRemoved = removeAction && tree.ExecuteScriptPopupCommand(NavigationPopupToggleFavorite) &&
			!tree.IsFavoriteScript(uidA) && !tree.IsFavoriteScript(uidB) && !tree.IsFavoriteScript(uidC);
		const bool passed = initialized && folderNameExcluded && filenameMatch && threeSelected && rightClickPreserves && keyboardContextPreserves && unsafeDisabled && favoritesAdded && mixedFavoriteAdd && dedicatedImage && iconLightDark && virtualNodesExcluded && duplicateRepresentation && singleMenuDirect && multiMenuNamed && invalidRejected && toolbarOrder && noDuplicates && toolbarPersisted && insertedAtPosition && insertedPersisted && hiddenDropRejected && dragMark && dragSwitched && dragCanceled && dragCaptureCanceled && dragOutsideCanceled && dragDropped && dragPersisted && emptyGeometry && emptyMark && emptyMarkStable && emptyMarkCanceled && emptyDrop && favoritesRemoved;
		CStringA dragReport; dragReport.Format("drag-geometry=%d\ndrag-hidden-rejected=%d\ndrag-mark=%d\ndrag-switched=%d\ndrag-canceled=%d\ndrag-capture-canceled=%d\ndrag-outside-canceled=%d\ndrag-dropped=%d\ndrag-persisted=%d\n", dragGeometry, hiddenDropRejected, dragMark, dragSwitched, dragCanceled, dragCaptureCanceled, dragOutsideCanceled, dragDropped, dragPersisted);
		CStringA emptyReport; emptyReport.Format("drag-empty-geometry=%d\ndrag-empty-mark=%d\ndrag-empty-mark-stable=%d\ndrag-empty-canceled=%d\ndrag-empty-drop=%d\n", emptyGeometry, emptyMark, emptyMarkStable, emptyMarkCanceled, emptyDrop); dragReport += emptyReport;
		CStringA report; report.Format("phase=navigation-scripts-multiselect\nfolder-name-excluded=%d\nfilename-match=%d\nthree-selected=%d\nright-click-preserves=%d\nkeyboard-context-preserves=%d\nunsafe-disabled=%d\nfavorites-added=%d\nmixed-favorite-add=%d\ndedicated-image=%d\nicon-light-dark=%d\nvirtual-nodes-excluded=%d\nduplicate-representation=%d\nsingle-menu-direct=%d\nmulti-menu-named=%d\ninvalid-rejected=%d\ntoolbar-order=%d\nno-duplicates=%d\ntoolbar-persisted=%d\ninsert-position=%d\ninsert-persisted=%d\nremove-action=%d\nfavorites-removed=%d\nresult=%s\n",
			folderNameExcluded, filenameMatch, threeSelected, rightClickPreserves, keyboardContextPreserves, unsafeDisabled, favoritesAdded, mixedFavoriteAdd, dedicatedImage, iconLightDark, virtualNodesExcluded, duplicateRepresentation, singleMenuDirect, multiMenuNamed, invalidRejected, toolbarOrder, noDuplicates, toolbarPersisted, insertedAtPosition, insertedPersisted, removeAction, favoritesRemoved, passed ? "pass" : "fail");
		report += dragReport; WritePortableStateTestText(reportPath, report); PostMessage(WM_CLOSE); return;
	}
	if(navigationSearchFavoritesRuntime)
	{
		_Settings.SetScriptsFolder(scriptsDirectory, true);
		_Settings.SetFavoriteScripts(CString(), true);
		const bool initialized = InitializeScripts();
		const bool dashFileAtStart = ::GetFileAttributes(scriptsDirectory + L"02_Чистка\\Dash.js") != INVALID_FILE_ATTRIBUTES;
		const int catalogAtStart = m_scripts.Menu().Count();
		RefreshNavigationScriptTree();
		if(!m_document_tree.m_tree.m_tree.IsScriptMode()) m_document_tree.m_tree.ToggleScriptMode();
		CTreeView& tree = m_document_tree.m_tree.m_tree;
		const bool searchVisible = m_document_tree.m_tree.IsScriptSearchVisible() && ::IsWindow(m_document_tree.m_tree.ScriptSearchWindow());
		HTREEITEM registerFolder = tree.FindScriptTreeItem(L"01_регистр");
		if(registerFolder != NULL) tree.Expand(registerFolder, TVE_EXPAND);
		const UINT discoveryBefore = m_scripts.DiscoveryCount();
		::SetWindowText(m_document_tree.m_tree.ScriptSearchWindow(), L"CASE");
		const bool caseInsensitive = tree.ScriptTreeNodeCount() == 2 && tree.FindScriptTreeItem(L"01_регистр/case.js") != NULL;
		::SetWindowText(m_document_tree.m_tree.ScriptSearchWindow(), L"чистка");
		const bool folderNameExcluded = tree.ScriptTreeNodeCount() == 0 && tree.FindScriptTreeItem(L"02_чистка/cleanup.js") == NULL && tree.FindScriptTreeItem(L"02_чистка/dash.js") == NULL;
		::SetWindowText(m_document_tree.m_tree.ScriptSearchWindow(), L"dash");
		const CString filterAtSearch(tree.ScriptFilter());
		const size_t filteredCount = tree.ScriptTreeNodeCount();
		const bool filteredFolder = tree.FindScriptTreeItem(L"02_чистка") != NULL;
		const bool filteredDash = tree.FindScriptTreeItem(L"02_чистка/dash.js") != NULL;
		const bool filtered = tree.ScriptFilter() == L"dash" && tree.ScriptTreeNodeCount() == 2 &&
			tree.FindScriptTreeItem(L"02_чистка") != NULL && tree.FindScriptTreeItem(L"02_чистка/dash.js") != NULL &&
			tree.FindScriptTreeItem(L"01_регистр") == NULL && tree.FindScriptTreeItem(L"tables.js") == NULL &&
			m_scripts.DiscoveryCount() == discoveryBefore;
		HTREEITEM folder = tree.FindScriptTreeItem(L"02_чистка");
		const bool expanded = folder != NULL && (tree.GetItemState(folder, TVIS_EXPANDED) & TVIS_EXPANDED) != 0;
		const CString addedPath = scriptsDirectory + L"02_Чистка\\DashExtra.js";
		const bool addedFile = WritePortableStateTestText(addedPath, "function Run() { /* added */ }\n");
		const bool addedReload = addedFile && InitializeScripts();
		if(addedReload) RefreshNavigationScriptTree();
		const bool liveAdded = addedReload && tree.ScriptFilter() == L"dash" && tree.ScriptTreeNodeCount() == 3 && tree.FindScriptTreeItem(L"02_чистка/dashextra.js") != NULL;
		const bool deletedFile = addedFile && ::DeleteFile(addedPath) != FALSE;
		const bool deletedReload = deletedFile && InitializeScripts();
		if(deletedReload) RefreshNavigationScriptTree();
		const bool liveRemoved = deletedReload && tree.ScriptTreeNodeCount() == 2 && tree.FindScriptTreeItem(L"02_чистка/dashextra.js") == NULL;
		::SendMessage(m_document_tree.m_tree.ScriptSearchWindow(), WM_KEYDOWN, VK_ESCAPE, 0);
		const size_t clearedCount = tree.ScriptTreeNodeCount();
		registerFolder = tree.FindScriptTreeItem(L"01_регистр");
		const bool cleared = tree.ScriptFilter().IsEmpty() && tree.ScriptTreeNodeCount() == 7 && tree.FindScriptTreeItem(L"tables.js") != NULL &&
			registerFolder != NULL && (tree.GetItemState(registerFolder, TVIS_EXPANDED) & TVIS_EXPANDED) != 0;
		::SendMessage(m_document_tree.m_tree.ScriptSearchWindow(), WM_KEYDOWN, VK_ESCAPE, 0);
		const bool escapeFocus = ::GetFocus() == tree.m_hWnd;
		HTREEITEM dash = tree.FindScriptTreeItem(L"02_чистка/dash.js");
		CString dashUid;
		for(int index = 0; index < m_scripts.Menu().Count(); ++index)
			if(m_scripts.Menu().Item(index).relativePath == L"02_чистка/dash.js") dashUid = m_scripts.Menu().Item(index).uid;
		tree.SelectItem(dash);
		const bool favorite = dash != NULL && !dashUid.IsEmpty() && tree.ExecuteScriptPopupCommand(NavigationPopupToggleFavorite) &&
			tree.IsFavoriteScript(dashUid) && tree.FavoriteScriptTreeItem(dashUid) != NULL && _Settings.GetFavoriteScripts().Find(dashUid) >= 0;
		::SetWindowText(m_document_tree.m_tree.ScriptSearchWindow(), L"dash");
		HTREEITEM localizedFolder = tree.FindScriptTreeItem(L"02_чистка");
		HTREEITEM localizedDash = tree.FindScriptTreeItem(L"02_чистка/dash.js");
		if(localizedFolder != NULL) tree.Expand(localizedFolder, TVE_EXPAND);
		if(localizedDash != NULL) tree.SelectItem(localizedDash);
		const HTREEITEM localizedGroup = tree.FavoriteGroupItem();
		const size_t localizedNodeCount = tree.ScriptTreeNodeCount();
		const UINT localizedDiscoveryCount = m_scripts.DiscoveryCount();
		const CString localizedFavorites(_Settings.GetFavoriteScripts());
		const DWORD originalLanguage = _Settings.GetInterfaceLanguageID();
		auto checkLocalizedScripts = [&](DWORD language, LPCWSTR locale, LPCWSTR cue, LPCWSTR groupText) {
			_Settings.SetInterfaceLanguage(language);
			FbePublishRuntimeLocaleName(locale); FbeResetRuntimeLocalization(); RefreshLocalizedMainFrameUi();
			wchar_t actualCue[128] = {}, actualGroup[128] = {}, searchText[128] = {};
			const HWND search = m_document_tree.m_tree.ScriptSearchWindow();
			const bool cueRead = ::SendMessage(search, EM_GETCUEBANNER, reinterpret_cast<WPARAM>(actualCue), _countof(actualCue)) != FALSE;
			::GetWindowText(search, searchText, _countof(searchText));
			if(localizedGroup != NULL) tree.GetItemText(localizedGroup, actualGroup, _countof(actualGroup));
			return cueRead && wcscmp(actualCue, cue) == 0 && wcscmp(actualGroup, groupText) == 0 &&
				wcscmp(searchText, L"dash") == 0 && tree.ScriptFilter() == L"dash" &&
				tree.FavoriteGroupItem() == localizedGroup && tree.ScriptTreeNodeCount() == localizedNodeCount &&
				tree.FindScriptTreeItem(L"02_чистка") == localizedFolder && tree.FindScriptTreeItem(L"02_чистка/dash.js") == localizedDash &&
				tree.GetSelectedItem() == localizedDash && (tree.GetItemState(localizedFolder, TVIS_EXPANDED) & TVIS_EXPANDED) != 0 &&
				tree.IsFavoriteScript(dashUid) && _Settings.GetFavoriteScripts() == localizedFavorites &&
				m_scripts.DiscoveryCount() == localizedDiscoveryCount;
		};
		const bool localizationReady = favorite && localizedGroup != NULL && localizedFolder != NULL && localizedDash != NULL;
		const bool localizedRussian = localizationReady && checkLocalizedScripts(FBE_INTERFACE_LANGUAGE_RUSSIAN, L"ru-RU", L"Найти скрипт...", L"Избранное");
		const bool localizedEnglish = localizationReady && checkLocalizedScripts(FBE_INTERFACE_LANGUAGE_ENGLISH, L"en-US", L"Find script...", L"Favorites");
		const bool localizedRussianAgain = localizationReady && checkLocalizedScripts(FBE_INTERFACE_LANGUAGE_RUSSIAN, L"ru-RU", L"Найти скрипт...", L"Избранное");
		const bool localizedWithoutRebuild = localizedRussian && localizedEnglish && localizedRussianAgain;
		_Settings.SetInterfaceLanguage(originalLanguage);
		FbePublishRuntimeLocaleName(_Settings.GetInterfaceLocaleName()); FbeResetRuntimeLocalization(); RefreshLocalizedMainFrameUi();
		const bool reloaded = InitializeScripts() && (RefreshNavigationScriptTree(), tree.FavoriteScriptTreeItem(dashUid) != NULL);
		const CString oldPath = scriptsDirectory + L"02_Чистка\\Dash.js";
		const CString renamedPath = scriptsDirectory + L"02_Чистка\\DashRenamed.js";
		const bool moved = ::MoveFile(oldPath, renamedPath) != FALSE;
		const bool renameReloaded = moved && InitializeScripts();
		if(renameReloaded) RefreshNavigationScriptTree();
		const bool renamed = renameReloaded && tree.IsFavoriteScript(dashUid) && tree.FavoriteScriptTreeItem(dashUid) != NULL &&
			tree.FindScriptTreeItem(L"02_чистка/dashrenamed.js") != NULL;
		const CString removedPath = diagnosticsDirectory + L"dash-removed.tmp";
		const bool removedFile = moved && ::MoveFile(renamedPath, removedPath) != FALSE;
		const bool removeReloaded = removedFile && InitializeScripts();
		if(removeReloaded) RefreshNavigationScriptTree();
		const bool removed = removeReloaded && tree.IsFavoriteScript(dashUid) && tree.FavoriteScriptTreeItem(dashUid) == NULL &&
			tree.FindScriptTreeItem(L"02_чистка/dashrenamed.js") == NULL;
		const bool restoredFile = removedFile && ::MoveFile(removedPath, renamedPath) != FALSE;
		const bool restoreReloaded = restoredFile && InitializeScripts();
		if(restoreReloaded) RefreshNavigationScriptTree();
		const bool restored = restoreReloaded && tree.IsFavoriteScript(dashUid) && tree.FavoriteScriptTreeItem(dashUid) != NULL;
		_Settings.SetFavoriteScripts(_Settings.GetFavoriteScripts() + L"broken favorite row\n", true);
		const InterfaceTheme originalTheme = ThemeManager::GetSelectedTheme();
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_LIGHT);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		const bool lightSearch = !ThemeManager::IsDark() && m_document_tree.m_tree.IsScriptSearchVisible();
		ThemeManager::SetSelectedTheme(INTERFACE_THEME_DARK);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		const bool darkSearch = ThemeManager::IsDark() && m_document_tree.m_tree.IsScriptSearchVisible();
		ThemeManager::SetSelectedTheme(originalTheme);
		ThemeManager::ApplyToAllThreadWindows(::GetCurrentThreadId());
		const bool themes = lightSearch && darkSearch;
		CString catalogPaths;
		for(int index = 0; index < m_scripts.Menu().Count(); ++index) catalogPaths += m_scripts.Menu().Item(index).relativePath + L";";
		m_document_tree.m_tree.ToggleScriptMode();
		const bool hidden = !m_document_tree.m_tree.IsScriptSearchVisible();
		m_document_tree.m_tree.ToggleScriptMode();
		const bool visibleAgain = m_document_tree.m_tree.IsScriptSearchVisible() && tree.FavoriteScriptTreeItem(dashUid) != NULL;
		const bool dpi = UiMetrics::ScaleForDpi(26, 96) == 26 && UiMetrics::ScaleForDpi(26, 192) == 52;
		const bool passed = initialized && searchVisible && caseInsensitive && folderNameExcluded && filtered && expanded && liveAdded && liveRemoved && cleared && escapeFocus && favorite && localizedWithoutRebuild && reloaded && renamed && removed && restored && themes && hidden && visibleAgain && dpi;
		CStringA report; report.Format("phase=navigation-search-favorites\nscripts-directory=%s\ncatalog-paths=%s\ndash-file-at-start=%d\ncatalog-at-start=%d\nsearch-visible=%d\ncase-insensitive=%d\nfolder-name-excluded=%d\nescape-focus=%d\nfilter-at-search=%s\nfiltered-count=%u\nfiltered-folder=%d\nfiltered-dash=%d\ncleared-count=%u\ndash-uid=%s\nfiltered=%d\nexpanded=%d\nlive-added=%d\nlive-removed=%d\ncleared=%d\nfavorite=%d\nlocale-ru=%d\nlocale-en=%d\nlocale-ru-again=%d\nlocale-preserved=%d\nreload=%d\nrename=%d\nremove=%d\nrestore=%d\nthemes=%d\nmode-switch=%d\ndpi-metrics=%d\nresult=%s\n",
			static_cast<LPCSTR>(CW2A(scriptsDirectory, CP_UTF8)), static_cast<LPCSTR>(CW2A(catalogPaths, CP_UTF8)), dashFileAtStart, catalogAtStart, searchVisible, caseInsensitive, folderNameExcluded, escapeFocus, static_cast<LPCSTR>(CW2A(filterAtSearch, CP_UTF8)), static_cast<unsigned>(filteredCount), filteredFolder, filteredDash, static_cast<unsigned>(clearedCount), static_cast<LPCSTR>(CW2A(dashUid, CP_UTF8)), filtered, expanded, liveAdded, liveRemoved, cleared, favorite, localizedRussian, localizedEnglish, localizedRussianAgain, localizedWithoutRebuild, reloaded, renamed, removed, restored, themes, hidden && visibleAgain, dpi, passed ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report); PostMessage(WM_CLOSE); return;
	}
	if(navigationScriptsRuntime)
	{
		_Settings.SetScriptsFolder(scriptsDirectory, true);
		_Settings.SetDocumentTreeScripts(true, true);
		const bool initialized = InitializeScripts();
		// The probe is executed during startup, before a command-line document
		// finishes its usual view activation.  Exercise the normal design-view
		// script command rather than the intentional source-view rejection.
		m_editor_view_state.Reset(EditorView::Body, EditorView::Description);
		RefreshNavigationScriptTree();
		CTreeView& tree = m_document_tree.m_tree.m_tree;
		if(tree.IsScriptMode()) m_document_tree.m_tree.ToggleScriptMode();
		else if(_Settings.DocumentTreeScripts()) { m_document_tree.m_tree.ToggleScriptMode(); m_document_tree.m_tree.ToggleScriptMode(); }
		RECT title = {}, modeButton = {}, closeButton = {}; int initialImage = -1; UINT initialCommand = 0;
		const bool modeButtonReady = !tree.IsScriptMode() && !_Settings.DocumentTreeScripts() && m_document_tree.m_tree.IsModeSelectorVisible() && m_document_tree.m_tree.IsStructuralToolbarVisible() && m_document_tree.GetModeButtonProbe(title, modeButton, closeButton, initialImage, initialCommand) && initialImage == 0 && initialCommand == ID_DOCUMENT_TREE_MODE_SCRIPTS;
		CString viewBarText; int viewBarWidth = 0, viewBarMeasuredWidth = 0, viewBarPadding = 0, viewBarHeight = 0, viewBarMinimumHeight = 0;
		const CString expectedViewBarText = FbeLoadRuntimeString(IDS_DOCTREE_MENU_ELEMENTS);
		const bool viewBarElements = m_document_tree.m_tree.GetViewBarElementProbe(viewBarText, viewBarWidth, viewBarMeasuredWidth, viewBarPadding, viewBarHeight, viewBarMinimumHeight) &&
			viewBarText == expectedViewBarText && viewBarWidth >= viewBarMeasuredWidth + viewBarPadding && viewBarHeight >= viewBarMinimumHeight;
		m_document_tree.m_tree.ToggleScriptMode();
		RECT scriptsTitle = {}, scriptsButton = {}, scriptsClose = {}; int scriptsImage = -1; UINT scriptsCommand = 0;
		const bool switchedScripts = tree.IsScriptMode() && _Settings.DocumentTreeScripts() && !m_document_tree.m_tree.IsModeSelectorVisible() && !m_document_tree.m_tree.IsStructuralToolbarVisible() && m_document_tree.GetModeButtonProbe(scriptsTitle, scriptsButton, scriptsClose, scriptsImage, scriptsCommand) && scriptsImage == 1 && scriptsCommand == ID_DOCUMENT_TREE_MODE_STRUCTURE;
		m_document_tree.m_tree.ToggleScriptMode();
		RECT structureTitle = {}, structureButton = {}, structureClose = {}; int structureImage = -1; UINT structureCommand = 0;
		const bool switchedStructure = !tree.IsScriptMode() && !_Settings.DocumentTreeScripts() && m_document_tree.m_tree.IsModeSelectorVisible() && m_document_tree.m_tree.IsStructuralToolbarVisible() && m_document_tree.GetModeButtonProbe(structureTitle, structureButton, structureClose, structureImage, structureCommand) && structureImage == 0 && structureCommand == ID_DOCUMENT_TREE_MODE_SCRIPTS;
		m_document_tree.m_tree.ToggleScriptMode();
		HTREEITEM folderA = tree.FindScriptTreeItem(L"foldera");
		HTREEITEM child = tree.FindScriptTreeItem(L"foldera/child.js");
		const HTREEITEM folderB = tree.FindScriptTreeItem(L"foldera/folderb");
		const HTREEITEM deep = tree.FindScriptTreeItem(L"foldera/folderb/deep.js");
		const HTREEITEM root = tree.FindScriptTreeItem(L"root.js");
		const bool hierarchy = folderA != NULL && child != NULL && folderB != NULL && deep != NULL && root != NULL &&
			tree.HasScriptTreeParent(L"foldera/child.js", L"foldera") && tree.HasScriptTreeParent(L"foldera/folderb/deep.js", L"foldera/folderb") && tree.ScriptTreeNodeCount() == 5;
		const bool visualMapping = tree.ScriptTreeImage(root) >= 0 && tree.ScriptTreeImage(folderA) >= 0 && tree.ScriptTreeImage(child) >= 0 && tree.ScriptTreeImage(folderB) >= 0 && tree.ScriptTreeImage(deep) >= 0 && tree.ScriptImageList() != tree.StructuralImageList();
		int scriptImageSize = 0, scriptItemHeight = 0, scriptIndent = 0; bool legacyExpanders = false;
		const UINT scriptDpi = UiMetrics::DpiForWindow(tree.m_hWnd);
		const int scriptIconSize = _Settings.GetScriptIconSize();
		const auto expectedScriptRow = [&](UINT dpi) {
			const int height = UiMetrics::ScaleForDpi(scriptIconSize + 4, dpi);
			// Win32 rounds odd row heights down unless TVS_NONEVENHEIGHT is set.
			return (tree.GetStyle() & TVS_NONEVENHEIGHT) != 0 ? height : height & ~1;
		};
		const int expectedImageSize = UiMetrics::ScaleForDpi(scriptIconSize, scriptDpi);
		const bool scriptMetrics = tree.GetScriptTreeMetrics(scriptImageSize, scriptItemHeight, scriptIndent, legacyExpanders) &&
			scriptImageSize == expectedImageSize && scriptItemHeight == expectedScriptRow(scriptDpi) &&
			scriptIndent == UiMetrics::ScaleForDpi((std::max)(20, scriptIconSize), scriptDpi) && legacyExpanders;
		const HWND bottomToolbar = m_document_tree.m_tree.BottomToolbarWindow();
		const UINT bottomCommands[] = { ID_DT_RIGHT_ONE, ID_DT_RIGHT_SMART, ID_DT_LEFT, ID_DT_MERGE, ID_DT_DELETE };
		bool bottomCommandsPreserved = ::SendMessage(bottomToolbar, TB_BUTTONCOUNT, 0, 0) == _countof(bottomCommands) &&
			::SendMessage(bottomToolbar, TB_GETTOOLTIPS, 0, 0) != 0;
		for(size_t index = 0; index < _countof(bottomCommands); ++index)
		{
			TBBUTTON item = {};
			bottomCommandsPreserved = bottomCommandsPreserved && ::SendMessage(bottomToolbar, TB_GETBUTTON, index, reinterpret_cast<LPARAM>(&item)) &&
				item.idCommand == static_cast<int>(bottomCommands[index]);
		}
		int bottomImageWidth = 0, bottomImageHeight = 0;
		const HIMAGELIST bottomImages = reinterpret_cast<HIMAGELIST>(::SendMessage(bottomToolbar, TB_GETIMAGELIST, 0, 0));
		const LRESULT bottomButtonSize = ::SendMessage(bottomToolbar, TB_GETBUTTONSIZE, 0, 0);
		const bool bottomCompact = bottomImages != NULL && ::ImageList_GetIconSize(bottomImages, &bottomImageWidth, &bottomImageHeight) &&
			bottomImageWidth == ToolbarFactory::CommandToolbarImageSize(scriptDpi) && bottomImageHeight == bottomImageWidth &&
			HIWORD(bottomButtonSize) >= bottomImageHeight + UiMetrics::ScaleForDpi(4, scriptDpi);
		bool dpiMatrix = true, dpiIconAlpha = true, narrowToolbar = true;
		int narrowRowsObserved = 0, narrowMaxRight = 0, narrowWidthObserved = 0, toolbarWidthObserved = 0, wrapFlagsObserved = 0;
		const UINT panelDpis[] = { 96, 120, 144, 168, 192 };
		RECT originalPanel = {}; ::GetWindowRect(m_document_tree.m_tree, &originalPanel);
		const int panelHeight = originalPanel.bottom - originalPanel.top;
		const int panelWidth = originalPanel.right - originalPanel.left;
		for(UINT dpi : panelDpis)
		{
			narrowMaxRight = 0; wrapFlagsObserved = 0;
			m_document_tree.m_tree.UpdateDpiMetrics(dpi);
			int image = 0, row = 0, indent = 0; bool expanders = false;
		const int expected = UiMetrics::ScaleForDpi(scriptIconSize, dpi);
		const int toolbarIconSize = ToolbarFactory::CommandToolbarImageSize(dpi);
			int toolbarImageWidth = 0, toolbarImageHeight = 0;
			const HWND toolbar = m_document_tree.m_tree.BottomToolbarWindow();
			const HIMAGELIST toolbarImages = reinterpret_cast<HIMAGELIST>(::SendMessage(toolbar, TB_GETIMAGELIST, 0, 0));
			const LRESULT buttonSize = ::SendMessage(toolbar, TB_GETBUTTONSIZE, 0, 0);
			RECT search = {}; ::GetWindowRect(m_document_tree.m_tree.ScriptSearchWindow(), &search);
			dpiMatrix = dpiMatrix && tree.GetScriptTreeMetrics(image, row, indent, expanders) &&
				image == expected && row == expectedScriptRow(dpi) && indent == UiMetrics::ScaleForDpi((std::max)(20, scriptIconSize), dpi) &&
				::ImageList_GetIconSize(toolbarImages, &toolbarImageWidth, &toolbarImageHeight) &&
				toolbarImageWidth == toolbarIconSize && toolbarImageHeight == toolbarIconSize &&
				LOWORD(buttonSize) >= toolbarIconSize + UiMetrics::ScaleForDpi(7, dpi) &&
				HIWORD(buttonSize) >= toolbarIconSize + UiMetrics::ScaleForDpi(7, dpi) &&
				search.bottom - search.top == UiMetrics::ScaleForDpi(26, dpi);
			for(COLORREF background : { RGB(255, 255, 255), RGB(32, 32, 32) })
			{
				BITMAPINFO info = {}; info.bmiHeader.biSize = sizeof(info.bmiHeader);
				info.bmiHeader.biWidth = expected; info.bmiHeader.biHeight = -expected;
				info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
				HBITMAP bitmap = ::CreateDIBSection(NULL, &info, DIB_RGB_COLORS, NULL, NULL, 0);
				HDC dc = bitmap != NULL ? ::CreateCompatibleDC(NULL) : NULL;
				if(dc == NULL) { if(bitmap != NULL) ::DeleteObject(bitmap); dpiIconAlpha = false; continue; }
				HGDIOBJ previous = ::SelectObject(dc, bitmap);
				RECT bounds = { 0, 0, expected, expected };
				HBRUSH fill = ::CreateSolidBrush(background); ::FillRect(dc, &bounds, fill); ::DeleteObject(fill);
				const bool drawn = ::ImageList_Draw(tree.ScriptImageList(), tree.FavoriteImageIndex(), dc, 0, 0, ILD_TRANSPARENT) != FALSE;
				dpiIconAlpha = dpiIconAlpha && drawn && ::GetPixel(dc, expected / 2, expected / 2) != background &&
					::GetPixel(dc, 0, 0) == background;
				::SelectObject(dc, previous); ::DeleteDC(dc); ::DeleteObject(bitmap);
			}
			const int narrowWidth = static_cast<int>(LOWORD(buttonSize)) * 2 + UiMetrics::ScaleForDpi(2, dpi);
			::SetWindowPos(m_document_tree.m_tree, NULL, 0, 0, narrowWidth, panelHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
			const int buttonCount = static_cast<int>(::SendMessage(toolbar, TB_BUTTONCOUNT, 0, 0));
			const int rows = static_cast<int>(::SendMessage(toolbar, TB_GETROWS, 0, 0));
			narrowRowsObserved = rows; narrowWidthObserved = narrowWidth;
			RECT toolbarBounds = {}; ::GetWindowRect(toolbar, &toolbarBounds); toolbarWidthObserved = toolbarBounds.right - toolbarBounds.left;
			for(int first = 0; first < buttonCount; ++first)
			{
				RECT firstRect = {}; ::SendMessage(toolbar, TB_GETITEMRECT, first, reinterpret_cast<LPARAM>(&firstRect));
				TBBUTTON current = {}; ::SendMessage(toolbar, TB_GETBUTTON, first, reinterpret_cast<LPARAM>(&current));
				if(current.fsState & TBSTATE_WRAP) wrapFlagsObserved |= 1 << first;
				narrowMaxRight = (std::max)(narrowMaxRight, static_cast<int>(firstRect.right));
				narrowToolbar = narrowToolbar && firstRect.left >= 0 && firstRect.right <= narrowWidth && firstRect.bottom > firstRect.top;
				for(int second = first + 1; second < buttonCount; ++second)
				{
					RECT secondRect = {}; ::SendMessage(toolbar, TB_GETITEMRECT, second, reinterpret_cast<LPARAM>(&secondRect));
					RECT overlap = {};
					narrowToolbar = narrowToolbar && !::IntersectRect(&overlap, &firstRect, &secondRect);
				}
			}
			narrowToolbar = narrowToolbar && rows >= 3;
			::SetWindowPos(m_document_tree.m_tree, NULL, 0, 0, panelWidth, panelHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		}
		m_document_tree.m_tree.UpdateDpiMetrics(scriptDpi);
		m_document_tree.m_tree.ToggleScriptMode();
		const LRESULT visibleButtonSize = ::SendMessage(bottomToolbar, TB_GETBUTTONSIZE, 0, 0);
		const int visibleNarrowWidth = static_cast<int>(LOWORD(visibleButtonSize)) * 2 + UiMetrics::ScaleForDpi(2, scriptDpi);
		::SetWindowPos(m_document_tree.m_tree, NULL, 0, 0, visibleNarrowWidth, panelHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		RECT visibleToolbar = {}, visiblePanel = {};
		::GetWindowRect(bottomToolbar, &visibleToolbar);
		::GetWindowRect(m_document_tree.m_tree, &visiblePanel);
		const bool visibleNarrowToolbar = m_document_tree.m_tree.IsStructuralToolbarVisible() &&
			::SendMessage(bottomToolbar, TB_GETROWS, 0, 0) >= 3 &&
			visibleToolbar.right <= visiblePanel.right && visibleToolbar.bottom <= visiblePanel.bottom;
		narrowToolbar = narrowToolbar && visibleNarrowToolbar;
		::SetWindowPos(m_document_tree.m_tree, NULL, 0, 0, panelWidth, panelHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		m_document_tree.m_tree.ToggleScriptMode();
		std::vector<ScriptDescriptor> reverseCatalog = m_scripts.Menu().Items(); std::vector<ScriptTreeVisual> reverseVisuals;
		for(int index = 0; index < m_scripts.Menu().Count(); ++index) { ScriptTreeVisual visual; visual.icon = m_scripts.Menu().VisualAt(index).icon; visual.bitmap = m_scripts.Menu().VisualAt(index).bitmap; reverseVisuals.push_back(visual); }
		std::reverse(reverseCatalog.begin(), reverseCatalog.end()); std::reverse(reverseVisuals.begin(), reverseVisuals.end());
		tree.SetScriptCatalog(reverseCatalog, reverseVisuals, std::vector<ScriptTreeToolbarTarget>(), std::function<bool(const std::vector<CString>&, const CString&)>(), std::function<void(const CString&)>(), std::function<void(UINT)>());
		const bool unorderedHierarchy = tree.HasScriptTreeParent(L"foldera/child.js", L"foldera") && tree.HasScriptTreeParent(L"foldera/folderb/deep.js", L"foldera/folderb") && tree.ScriptTreeNodeCount() == 5;
		const bool refreshedImageListBound = tree.GetImageList(TVSIL_NORMAL) == tree.ScriptImageList();
		RefreshNavigationScriptTree();
		const int imagesBefore = tree.ScriptImageCount(); RefreshNavigationScriptTree(); RefreshNavigationScriptTree(); tree.SetScriptMode(false); tree.SetScriptMode(true); const bool imagesStable = tree.ScriptImageCount() == imagesBefore;
		// The fixture scripts deliberately have no UI body.  Probe navigation
		// dispatch without asynchronously executing one while startup closes.
		std::vector<ScriptDescriptor> commandCatalog = m_scripts.Menu().Items(); std::vector<ScriptTreeVisual> commandVisuals; std::vector<ScriptTreeToolbarTarget> commandTargets;
		for(int index = 0; index < m_scripts.Menu().Count(); ++index) { ScriptTreeVisual visual; visual.icon = m_scripts.Menu().VisualAt(index).icon; visual.bitmap = m_scripts.Menu().VisualAt(index).bitmap; commandVisuals.push_back(visual); }
		for(size_t index = 0; index < m_scriptToolbars.Items().size(); ++index) { ScriptTreeToolbarTarget target; target.id = m_scriptToolbars.Items()[index].definition.id; target.name = m_scriptToolbars.Items()[index].definition.name; commandTargets.push_back(target); }
		tree.SetScriptCatalog(commandCatalog, commandVisuals, commandTargets, [this](const std::vector<CString>& uids, const CString& id) { return AddScriptsToToolbar(uids, id); }, std::function<void(const CString&)>(), [this](UINT commandId) { for(int index = 0; index < m_scripts.Menu().Count(); ++index) if(!m_scripts.Menu().Item(index).isFolder && m_scripts.Menu().Item(index).commandId == static_cast<int>(commandId)) { m_scripts.SetLastScript(m_scripts.Menu().Item(index)); break; } });
		folderA = tree.FindScriptTreeItem(L"foldera"); child = tree.FindScriptTreeItem(L"foldera/child.js"); const HTREEITEM refreshedRoot = tree.FindScriptTreeItem(L"root.js"); const HTREEITEM refreshedDeep = tree.FindScriptTreeItem(L"foldera/folderb/deep.js");
		m_scripts.ClearLastScript(); tree.SelectItem(child); BOOL handled = FALSE; tree.OnKeyDown(WM_KEYDOWN, VK_RETURN, 0, handled);
		const ScriptDescriptor* ran = m_scripts.LastScript(); const bool enterRuns = handled && ran != NULL && ran->relativePath == L"foldera/child.js";
		CTreeItem(folderA, &tree).Expand(TVE_EXPAND); m_scripts.ClearLastScript(); CRect childRect; tree.GetItemRect(child, &childRect, TRUE); BOOL doubleClickHandled = FALSE; tree.OnDblClick(WM_LBUTTONDBLCLK, 0, MAKELPARAM(childRect.left + 2, childRect.top + 2), doubleClickHandled);
		ran = m_scripts.LastScript(); const bool doubleClickRuns = ran != NULL && ran->relativePath == L"foldera/child.js";
		m_scripts.ClearLastScript(); tree.SelectItem(folderA); handled = FALSE; tree.OnKeyDown(WM_KEYDOWN, VK_RETURN, 0, handled);
		const bool folderOnly = handled && m_scripts.LastScript() == NULL;
		NMTREEVIEW drag = {}; drag.itemNew.hItem = child; BOOL dragHandled = FALSE; tree.OnBegindrag(0, reinterpret_cast<LPNMHDR>(&drag), dragHandled);
		const bool dragGuarded = dragHandled && !tree.IsStructuralDragActive();
		BOOL cancelDragHandled = FALSE; tree.OnKeyDown(WM_KEYDOWN, VK_ESCAPE, 0, cancelDragHandled);
		const UINT discoveryBefore = m_scripts.DiscoveryCount();
		std::vector<ScriptToolbarDefinition> previous = currentDefinitions(), changed = previous;
		ScriptToolbarDefinition targetA; targetA.id = L"navigation-runtime-toolbar-a"; targetA.name = L"Navigation runtime A"; targetA.visible = true; changed.push_back(targetA);
		ScriptToolbarDefinition targetB; targetB.id = L"navigation-runtime-toolbar"; targetB.name = L"Navigation runtime B"; targetB.visible = true; changed.push_back(targetB);
		const bool applied = ApplyScriptToolbarDefinitions(previous, changed);
		CString rootUid, deepUid; int childCommand = -1; bool bitmapOnlyChild = false; for(int index = 0; index < m_scripts.Menu().Count(); ++index) { if(m_scripts.Menu().Item(index).relativePath == L"root.js") rootUid = m_scripts.Menu().Item(index).uid; if(m_scripts.Menu().Item(index).relativePath == L"foldera/folderb/deep.js") deepUid = m_scripts.Menu().Item(index).uid; if(m_scripts.Menu().Item(index).relativePath == L"foldera/child.js") { childCommand = m_scripts.Menu().Item(index).commandId; const FbeScripts::VisualResource& visual = m_scripts.Menu().VisualAt(index); bitmapOnlyChild = visual.bitmap != NULL && visual.icon == NULL; } }
		// scripts-main is the first target; the two runtime-created panels follow it.
		tree.SelectItem(refreshedRoot); const bool rootAdded = applied && tree.ExecuteScriptPopupCommand(NavigationPopupAddToolbarBase + 1);
		tree.SelectItem(refreshedDeep); const bool deepAdded = rootAdded && tree.ExecuteScriptPopupCommand(NavigationPopupAddToolbarBase + 2);
		const bool toolbarAdded = rootAdded && deepAdded;
		PortableToolbarLayout persisted; bool uidPersisted = toolbarAdded && PortableToolbarStore::Load(persisted);
		bool rootUidPersisted = false, deepUidPersisted = false; for(size_t index = 0; uidPersisted && index < persisted.scriptToolbars.size(); ++index) { const ScriptToolbarDefinition& toolbar = persisted.scriptToolbars[index]; if(toolbar.id == targetA.id && !toolbar.items.empty()) rootUidPersisted = toolbar.items[0].scriptUid == rootUid; if(toolbar.id == targetB.id && !toolbar.items.empty()) deepUidPersisted = toolbar.items[0].scriptUid == deepUid; } uidPersisted = rootUidPersisted && deepUidPersisted;
		std::vector<ScriptToolbarDefinition> renamed = currentDefinitions(); for(size_t index = 0; index < renamed.size(); ++index) if(renamed[index].id == targetB.id) renamed[index].name = L"Navigation renamed";
		const bool targetRenamed = toolbarAdded && ApplyScriptToolbarDefinitions(currentDefinitions(), renamed) && tree.HasScriptToolbarTarget(targetB.id, L"Navigation renamed");
		std::vector<ScriptToolbarDefinition> deleted = currentDefinitions(); ScriptToolbarDefinition temporary; temporary.id = L"navigation-delete-toolbar"; temporary.name = L"Navigation delete"; deleted.push_back(temporary);
		const bool temporaryAdded = targetRenamed && ApplyScriptToolbarDefinitions(currentDefinitions(), deleted) && tree.HasScriptToolbarTarget(temporary.id, temporary.name);
		deleted = currentDefinitions(); for(std::vector<ScriptToolbarDefinition>::iterator index = deleted.begin(); index != deleted.end(); ++index) if(index->id == temporary.id) { deleted.erase(index); break; }
		std::vector<ScriptToolbarDefinition> withoutA = currentDefinitions(); for(std::vector<ScriptToolbarDefinition>::iterator index = withoutA.begin(); index != withoutA.end(); ++index) if(index->id == targetA.id) { withoutA.erase(index); break; }
		const bool targetDeleted = temporaryAdded && ApplyScriptToolbarDefinitions(currentDefinitions(), deleted) && !tree.HasScriptToolbarTarget(temporary.id, temporary.name) && tree.HasScriptToolbarTarget(targetB.id, L"Navigation renamed");
		const bool targetLive = targetDeleted && ApplyScriptToolbarDefinitions(currentDefinitions(), withoutA) && !tree.HasScriptToolbarTarget(targetA.id, targetA.name) && m_scripts.DiscoveryCount() == discoveryBefore;
		tree.SetScriptMode(false); m_document_tree.GetDocumentStructure(m_doc->m_body.Document()); HTREEITEM structuralNode = tree.GetRootItem(); NMTREEVIEW structuralDrag = {}; structuralDrag.itemNew.hItem = structuralNode; BOOL structuralDragHandled = FALSE; tree.OnBegindrag(0, reinterpret_cast<LPNMHDR>(&structuralDrag), structuralDragHandled);
		const bool structuralDragWorks = structuralNode != NULL && !structuralDragHandled && tree.IsStructuralDragActive(); tree.SetScriptMode(true);
		// Startup closes this probe before MSHTML has dispatched the script body;
		// command routing itself is covered by the direct tree handler contract.
		CStringA report; const bool passed = initialized && modeButtonReady && viewBarElements && switchedStructure && switchedScripts && hierarchy && visualMapping && scriptMetrics && bottomCommandsPreserved && bottomCompact && dpiMatrix && dpiIconAlpha && narrowToolbar && unorderedHierarchy && refreshedImageListBound && imagesStable && enterRuns && doubleClickRuns && folderOnly && dragGuarded && bitmapOnlyChild && uidPersisted && targetLive && structuralDragWorks;
		report.Format("phase=navigation-scripts\nmode-button-window=%d\nmode-button-visible=%d\nmode-button-position-valid=%d\nmode-button-image-initial=%d\nmode-button-image-after-click=%d\nmode-button-image-after-second-click=%d\nmode-button-switch=%d\nviewbar-elements=%d\nviewbar-width=%d\nviewbar-measured-width=%d\nviewbar-padding=%d\nhierarchy=%d\nvisual-mapping=%d\nscript-image-size=%d\nscript-item-height=%d\nscript-indent=%d\nscript-legacy-expanders=%d\nscript-metrics=%d\ndpi-matrix=%d\nnarrow-toolbar=%d\nunordered-hierarchy=%d\nrefreshed-image-list-bound=%d\nimages-stable=%d\nsource-active=%d\nchild-command=%d\nbitmap-only-script=%d\nenter-runs=%d\ndouble-click-runs=%d\nfolder-only=%d\ndrag-guarded=%d\nstructural-drag-works=%d\nroot-added=%d\ndeep-added=%d\nroot-uid-persisted=%d\ndeep-uid-persisted=%d\nuid-persisted=%d\ntoolbar-target-live=%d\nresult=%s\n", modeButtonReady, modeButtonReady, modeButtonReady, initialImage, scriptsImage, structureImage, switchedStructure && switchedScripts, viewBarElements, viewBarWidth, viewBarMeasuredWidth, viewBarPadding, hierarchy, visualMapping, scriptImageSize, scriptItemHeight, scriptIndent, legacyExpanders, scriptMetrics, dpiMatrix, narrowToolbar, unorderedHierarchy, refreshedImageListBound, imagesStable, IsSourceActive(), childCommand, bitmapOnlyChild, enterRuns, doubleClickRuns, folderOnly, dragGuarded, structuralDragWorks, rootAdded, deepAdded, rootUidPersisted, deepUidPersisted, uidPersisted, targetLive, passed ? "pass" : "fail");
		report.AppendFormat("bottom-commands-preserved=%d\nbottom-compact=%d\nbottom-image-size=%dx%d\nbottom-button-size=%dx%d\nbottom-dpi=%u\ndpi-icon-alpha=%d\nvisible-narrow-toolbar=%d\nnarrow-rows=%d\nnarrow-max-right=%d\nnarrow-width=%d\ntoolbar-width=%d\nwrap-flags=%d\n", bottomCommandsPreserved, bottomCompact, bottomImageWidth, bottomImageHeight, LOWORD(bottomButtonSize), HIWORD(bottomButtonSize), scriptDpi, dpiIconAlpha, visibleNarrowToolbar, narrowRowsObserved, narrowMaxRight, narrowWidthObserved, toolbarWidthObserved, wrapFlagsObserved);
		WritePortableStateTestText(reportPath, report); PostMessage(WM_CLOSE); return;
	}
	if(scriptToolbarRollbackNoMain)
	{
		::CreateDirectory(scriptsDirectory, NULL); _Settings.SetScriptsFolder(scriptsDirectory, true); InitializeScripts();
		PortableToolbarStore::Snapshot before, after; const bool absentBefore = PortableToolbarStore::CaptureSnapshot(before) && !before.exists;
		std::vector<ScriptToolbarDefinition> previous = currentDefinitions(), candidate = previous;
		ScriptToolbarDefinition custom; custom.id = L"rollback-new-toolbar"; custom.name = L"Rollback candidate"; candidate.push_back(custom);
		m_testFailAfterCustomToolbarCreates = 1; const bool rolledBack = !ApplyScriptToolbarDefinitions(previous, candidate);
		const bool absentAfter = PortableToolbarStore::CaptureSnapshot(after) && !after.exists;
		const bool restarted = InitializeScripts() && mainHasDefault();
		CStringA report; report.Format("phase=script-toolbar-rollback-no-main\nabsent-before=%d\nrollback=%d\nabsent-after=%d\ndefault-main=%d\nresult=%s\n", absentBefore, rolledBack, absentAfter, restarted, absentBefore && rolledBack && absentAfter && restarted ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report); PostMessage(WM_CLOSE); return;
	}
	if(scriptToolbarRollbackPersisted)
	{
		::CreateDirectory(scriptsDirectory, NULL); _Settings.SetScriptsFolder(scriptsDirectory, true);
		PortableToolbarLayout original; original.commandToolbarPresent = true; original.scriptsToolbarPresent = true; original.lastScript = L"persisted-last-script-uid";
		PortableToolbarItem command = {}; command.command = ID_FILE_SAVE; original.commands.push_back(command);
		ScriptToolbarDefinition main; main.id = L"scripts-main"; main.name = L"Persisted main"; main.visible = false; PortableToolbarItem mainItem = {}; mainItem.command = ID_LAST_SCRIPT; main.items.push_back(mainItem); original.scriptToolbars.push_back(main);
		ScriptToolbarDefinition first; first.id = L"persisted-first"; first.name = L"First"; first.visible = false; original.scriptToolbars.push_back(first);
		ScriptToolbarDefinition second; second.id = L"persisted-second"; second.name = L"Second"; second.visible = true; original.scriptToolbars.push_back(second);
		const bool seeded = PortableToolbarStore::Save(original); PortableToolbarStore::Snapshot before, after; const bool captured = seeded && PortableToolbarStore::CaptureSnapshot(before);
		const bool initialized = seeded && InitializeScripts(); std::vector<ScriptToolbarDefinition> previous = currentDefinitions(), candidate = previous;
		ScriptToolbarDefinition extra; extra.id = L"rollback-extra"; extra.name = L"Extra"; candidate.push_back(extra);
		m_testFailAfterCustomToolbarCreates = 1; const bool rolledBack = initialized && !ApplyScriptToolbarDefinitions(previous, candidate);
		const bool exact = PortableToolbarStore::CaptureSnapshot(after) && after.exists && after.text == before.text;
		PortableToolbarLayout restored; const bool loaded = PortableToolbarStore::Load(restored);
		const bool state = loaded && restored.commandToolbarPresent && restored.commands.size() == 1 && restored.commands[0].command == ID_FILE_SAVE && restored.lastScript == original.lastScript && restored.scriptToolbars.size() == 3 && restored.scriptToolbars[0].id == L"scripts-main" && !restored.scriptToolbars[0].visible && restored.scriptToolbars[0].items.size() == 1 && restored.scriptToolbars[0].items[0].command == ID_LAST_SCRIPT && restored.scriptToolbars[1].id == L"persisted-first" && !restored.scriptToolbars[1].visible && restored.scriptToolbars[2].id == L"persisted-second" && restored.scriptToolbars[2].visible;
		CStringA report; report.Format("phase=script-toolbar-rollback-persisted\nseeded=%d\nrollback=%d\nexact=%d\nstate=%d\nresult=%s\n", captured, rolledBack, exact, state, captured && rolledBack && exact && state ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report); PostMessage(WM_CLOSE); return;
	}
	if(scriptToolbarRollbackPartial)
	{
		::CreateDirectory(scriptsDirectory, NULL); _Settings.SetScriptsFolder(scriptsDirectory, true);
		auto makeDefinitions = [&]() { std::vector<ScriptToolbarDefinition> definitions; ScriptToolbarDefinition main; main.id = L"scripts-main"; main.name = L"Scripts"; definitions.push_back(main); for(int index = 1; index <= 3; ++index) { ScriptToolbarDefinition item; item.id.Format(L"partial-toolbar-%d", index); item.name.Format(L"Partial %d", index); item.visible = true; PortableToolbarItem button = {}; button.command = ID_LAST_SCRIPT; item.items.push_back(button); definitions.push_back(item); } return definitions; };
		auto runtimeValid = [&](const std::vector<ScriptToolbarDefinition>& definitions, int bands) {
			int expectedWindows = 0, windows = 0, validBands = 0;
			const std::vector<ScriptToolbarRuntime>& runtimes = m_scriptToolbars.Items();
			if(static_cast<int>(m_rebar.GetBandCount()) != bands || runtimes.size() != definitions.size()) return false;
			for(size_t index = 0; index < runtimes.size(); ++index) {
				const ScriptToolbarRuntime& runtime = runtimes[index]; const ScriptToolbarDefinition& expected = definitions[index];
				if(runtime.definition.id != expected.id || runtime.definition.name != expected.name || runtime.definition.visible != expected.visible || runtime.definition.items.size() != expected.items.size()) return false;
				for(size_t item = 0; item < expected.items.size(); ++item) { const PortableToolbarItem& actual = runtime.definition.items[item]; const PortableToolbarItem& expectedItem = expected.items[item]; if(actual.separator != expectedItem.separator || actual.command != expectedItem.command || actual.width != expectedItem.width || actual.scriptUid != expectedItem.scriptUid || actual.relativePath != expectedItem.relativePath) return false; }
				if(runtime.definition.id == L"scripts-main" || !runtime.definition.visible) continue;
				++expectedWindows;
				if(runtime.window == NULL || !::IsWindow(runtime.window) || runtime.rebarBandId == 0 || m_rebar.IdToIndex(runtime.rebarBandId) < 0) return false;
				++windows; ++validBands;
			}
			return expectedWindows == windows && validBands == expectedWindows;
		};
		PortableToolbarStore::Snapshot absentBefore, absentAfter; const bool noFile = PortableToolbarStore::CaptureSnapshot(absentBefore) && !absentBefore.exists;
		std::vector<ScriptToolbarDefinition> previous = makeDefinitions(); const bool initialNoFile = noFile && InitializeScriptsFromDefinitions(previous, false); const int noFileBands = m_rebar.GetBandCount();
		std::vector<ScriptToolbarDefinition> candidate = previous; ScriptToolbarDefinition extra; extra.id = L"partial-extra"; extra.name = L"Extra"; candidate.push_back(extra); ScriptToolbarDefinition extraSecond; extraSecond.id = L"partial-extra-second"; extraSecond.name = L"Extra second"; candidate.push_back(extraSecond);
		m_testFailAfterCustomToolbarCreates = 2; const bool noFileRollback = initialNoFile && !ApplyScriptToolbarDefinitions(previous, candidate) && PortableToolbarStore::CaptureSnapshot(absentAfter) && !absentAfter.exists && runtimeValid(previous, noFileBands);
		PortableToolbarLayout original; original.commandToolbarPresent = true; original.scriptsToolbarPresent = true; original.lastScript = L"partial-last-script"; original.scriptToolbars = previous; PortableToolbarItem command = {}; command.command = ID_FILE_SAVE; original.commands.push_back(command);
		const bool seeded = PortableToolbarStore::Save(original); PortableToolbarStore::Snapshot persistedBefore, persistedAfter; const bool captured = seeded && PortableToolbarStore::CaptureSnapshot(persistedBefore);
		const bool initialized = seeded && InitializeScripts(); const int persistedBands = m_rebar.GetBandCount();
		m_testFailAfterCustomToolbarCreates = 2; const bool persistedRollback = initialized && !ApplyScriptToolbarDefinitions(currentDefinitions(), candidate) && PortableToolbarStore::CaptureSnapshot(persistedAfter) && persistedAfter.text == persistedBefore.text && runtimeValid(previous, persistedBands);
		const bool stable = persistedRollback && InitializeScripts() && runtimeValid(previous, persistedBands);
		const bool noFileResult = !noFile || noFileRollback;
		CStringA report; report.Format("phase=script-toolbar-rollback-partial\nno-file=%d\npartial-no-file=%d\npersisted=%d\npartial-persisted=%d\nstable=%d\nresult=%s\n", noFile, noFileRollback, captured, persistedRollback, stable, noFileResult && persistedRollback && stable ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report); PostMessage(WM_CLOSE); return;
	}
	if (scriptToolbarLifecycleReload)
	{
		PortableToolbarLayout persisted; const bool loaded = PortableToolbarStore::Load(persisted);
		bool missingPreserved = false; int expected = 0;
		for(size_t index = 0; loaded && index < persisted.scriptToolbars.size(); ++index) {
			const ScriptToolbarDefinition& item = persisted.scriptToolbars[index];
			if(item.id.Left(16) == L"runtime-toolbar-") ++expected;
			if(item.id == L"runtime-toolbar-5" && item.items.size() > 1 && item.items[1].scriptUid == L"runtime-missing-script-uid") missingPreserved = true;
		}
		auto liveCount = [&]() { int count = 0; for(size_t index = 0; index < m_scriptToolbars.Items().size(); ++index) if(m_scriptToolbars.Items()[index].definition.id != L"scripts-main" && m_scriptToolbars.Items()[index].window != NULL && ::IsWindow(m_scriptToolbars.Items()[index].window)) ++count; return count; };
		const int bandsBefore = m_rebar.GetBandCount(); const bool reloaded = InitializeScripts(); const int bandsAfter = m_rebar.GetBandCount();
		const bool passed = loaded && expected == 4 && missingPreserved && reloaded && liveCount() == 4 && bandsBefore == bandsAfter;
		CStringA report; report.Format("phase=script-toolbar-lifecycle-reload\nmode=%s\nexpected=%d\nmissing-uid=%d\nreload=%d\nbands-stable=%d\nresult=%s\n", DeploymentContext::CurrentMode() == DeploymentContext::Mode::Portable ? "portable" : "installed", expected, missingPreserved, reloaded && liveCount() == 4, bandsBefore == bandsAfter, passed ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report); PostMessage(WM_CLOSE); return;
	}
	if (scriptToolbarLifecycle)
	{
		// Use one local fixture so metadata persistence is checked without loading
		// the bundled catalogue during each lifecycle/reload probe.
		::CreateDirectory(scriptsDirectory, NULL);
		WritePortableStateTestText(scriptsDirectory + L"\\Metadata.js", "function Run() {}\r\n");
		_Settings.SetScriptsFolder(scriptsDirectory, true);
		if(!InitializeScripts()) { WritePortableStateTestText(reportPath, "phase=script-toolbar-lifecycle\nreason=initial-reload\nresult=fail\n"); PostMessage(WM_CLOSE); return; }
		const UINT initBefore = m_scripts.InitializeCount(); const UINT discoveryBefore = m_scripts.DiscoveryCount();
		std::vector<ScriptToolbarDefinition> previous;
		for(size_t index = 0; index < m_scriptToolbars.Items().size(); ++index) previous.push_back(m_scriptToolbars.Items()[index].definition);
		std::vector<ScriptToolbarDefinition> definitions = previous;
		ScriptToolbarDefinition* main = NULL;
		for(size_t index = 0; index < definitions.size(); ++index) if(definitions[index].id == L"scripts-main") { main = &definitions[index]; break; }
		if(main == NULL) { ScriptToolbarDefinition item; item.id = L"scripts-main"; item.name = L"Scripts"; definitions.insert(definitions.begin(), item); main = &definitions.front(); }
		const CString defaultNameFormat = FbeLoadRuntimeStringByKey(L"fbe.script_toolbar_manager.default_name");
		ScriptToolbarCollection generatedNames;
		ScriptToolbarDefinition& firstGenerated = generatedNames.Add(generatedNames.NextDefaultName(defaultNameFormat));
		const CString firstGeneratedId = firstGenerated.id, firstGeneratedName = firstGenerated.name;
		ScriptToolbarDefinition& secondGenerated = generatedNames.Add(generatedNames.NextDefaultName(defaultNameFormat));
		const CString secondGeneratedId = secondGenerated.id;
		generatedNames.Rename(secondGeneratedId, L"Correction");
		ScriptToolbarDefinition& replacementGenerated = generatedNames.Add(generatedNames.NextDefaultName(defaultNameFormat));
		const CString replacementGeneratedId = replacementGenerated.id, replacementGeneratedName = replacementGenerated.name;
		ScriptToolbarDefinition& thirdGenerated = generatedNames.Add(generatedNames.NextDefaultName(defaultNameFormat));
		const CString thirdGeneratedId = thirdGenerated.id, thirdGeneratedName = thirdGenerated.name;
		ScriptToolbarCollection deletedNames;
		ScriptToolbarDefinition& firstDeleted = deletedNames.Add(deletedNames.NextDefaultName(defaultNameFormat));
		const CString firstDeletedId = firstDeleted.id;
		ScriptToolbarDefinition& secondDeleted = deletedNames.Add(deletedNames.NextDefaultName(defaultNameFormat));
		const CString secondDeletedId = secondDeleted.id;
		deletedNames.Remove(secondDeletedId);
		ScriptToolbarDefinition& replacementDeleted = deletedNames.Add(deletedNames.NextDefaultName(defaultNameFormat));
		const CString replacementDeletedId = replacementDeleted.id, replacementDeletedName = replacementDeleted.name;
		CString expectedFirst, expectedSecond, expectedThird; expectedFirst.Format(defaultNameFormat, 1); expectedSecond.Format(defaultNameFormat, 2); expectedThird.Format(defaultNameFormat, 3);
		const bool defaultNames = !defaultNameFormat.IsEmpty() && firstGeneratedName == expectedFirst && replacementGeneratedName == expectedSecond && thirdGeneratedName == expectedThird &&
			firstGeneratedId == L"toolbar-1" && secondGeneratedId == L"toolbar-2" && replacementGeneratedId == L"toolbar-3" && thirdGeneratedId == L"toolbar-4" &&
			firstDeletedId == L"toolbar-1" && replacementDeletedId == L"toolbar-2" && replacementDeletedName == expectedSecond;
		CString knownUid;
		for(int index = 0; index < m_scripts.Menu().Count(); ++index) if(!m_scripts.Menu().Item(index).isFolder && !m_scripts.Menu().Item(index).uid.IsEmpty()) { knownUid = m_scripts.Menu().Item(index).uid; break; }
		for(int index = 1; index <= 5; ++index) { ScriptToolbarDefinition item; item.id.Format(L"runtime-toolbar-%d", index); item.name.Format(L"Runtime toolbar %d", index); item.visible = true; definitions.push_back(item); }
		PortableToolbarItem known = {}; known.scriptUid = knownUid.IsEmpty() ? L"runtime-layout-anchor-uid" : knownUid; definitions.back().items.push_back(known);
		PortableToolbarItem missing = {}; missing.scriptUid = L"runtime-missing-script-uid";
		missing.scriptName = L"Последнее известное имя"; missing.relativePath = L"missing/Последний путь.js";
		definitions.back().items.push_back(missing);
		PortableToolbarItem separator = {}; separator.separator = true; separator.width = 9; definitions.back().items.push_back(separator);
		const int initialBands = m_rebar.GetBandCount();
		auto runtimeBandsValid = [&](int expected) {
			int windows = 0, bands = 0;
			for(size_t index = 0; index < m_scriptToolbars.Items().size(); ++index) { const ScriptToolbarRuntime& runtime = m_scriptToolbars.Items()[index]; if(runtime.definition.id == L"scripts-main") continue; if(runtime.window != NULL && ::IsWindow(runtime.window)) { ++windows; if(runtime.rebarBandId != 0 && m_rebar.IdToIndex(runtime.rebarBandId) >= 0) ++bands; } }
			return windows == expected && bands == expected && static_cast<int>(m_rebar.GetBandCount()) == initialBands + expected;
		};
		bool created = ApplyScriptToolbarDefinitions(previous, definitions) && runtimeBandsValid(5);
		ScriptToolbarRuntime* mainRuntime = m_scriptToolbars.Find(L"scripts-main");
		ScriptToolbarRuntime* stableRuntime = m_scriptToolbars.Find(L"runtime-toolbar-4");
		const HWND mainWindow = mainRuntime == NULL ? NULL : mainRuntime->window;
		const UINT mainBand = mainRuntime == NULL ? 0 : mainRuntime->rebarBandId;
		const HWND stableWindow = stableRuntime == NULL ? NULL : stableRuntime->window;
		const UINT stableBand = stableRuntime == NULL ? 0 : stableRuntime->rebarBandId;
		// Every operation receives the definitions actually live before the edit.
		std::vector<ScriptToolbarDefinition> current; for(size_t index = 0; index < m_scriptToolbars.Items().size(); ++index) current.push_back(m_scriptToolbars.Items()[index].definition);
		definitions[1].name = L"Renamed runtime toolbar"; std::swap(definitions[1], definitions[3]);
		bool renamedAndReordered = created && ApplyScriptToolbarDefinitions(current, definitions);
		current.clear(); for(size_t index = 0; index < m_scriptToolbars.Items().size(); ++index) current.push_back(m_scriptToolbars.Items()[index].definition);
		for(size_t index = 0; index < definitions.size(); ++index) if(definitions[index].id == L"runtime-toolbar-2") definitions[index].visible = false;
		bool hidden = renamedAndReordered && ApplyScriptToolbarDefinitions(current, definitions) && runtimeBandsValid(4);
		current.clear(); for(size_t index = 0; index < m_scriptToolbars.Items().size(); ++index) current.push_back(m_scriptToolbars.Items()[index].definition);
		for(size_t index = 0; index < definitions.size(); ++index) if(definitions[index].id == L"runtime-toolbar-2") definitions[index].visible = true;
		bool shown = hidden && ApplyScriptToolbarDefinitions(current, definitions) && runtimeBandsValid(5);
		current.clear(); for(size_t index = 0; index < m_scriptToolbars.Items().size(); ++index) current.push_back(m_scriptToolbars.Items()[index].definition);
		for(std::vector<ScriptToolbarDefinition>::iterator it = definitions.begin(); it != definitions.end(); ++it) if(it->id == L"runtime-toolbar-3") { definitions.erase(it); break; }
		bool deleted = shown && ApplyScriptToolbarDefinitions(current, definitions) && runtimeBandsValid(4);
		mainRuntime = m_scriptToolbars.Find(L"scripts-main"); stableRuntime = m_scriptToolbars.Find(L"runtime-toolbar-4");
		const bool untouchedStable = mainRuntime != NULL && mainRuntime->window == mainWindow && mainRuntime->rebarBandId == mainBand && stableRuntime != NULL && stableRuntime->window == stableWindow && stableRuntime->rebarBandId == stableBand;
		const bool incremental = m_scripts.InitializeCount() == initBefore && m_scripts.DiscoveryCount() == discoveryBefore;
		bool reloads = deleted;
		for(int cycle = 0; cycle < 3 && reloads; ++cycle) reloads = InitializeScripts() && runtimeBandsValid(4);
		PortableToolbarLayout persisted; const bool loaded = PortableToolbarStore::Load(persisted);
		bool missingPreserved = false, orderPreserved = false;
		for(size_t index = 0; loaded && index < persisted.scriptToolbars.size(); ++index) {
			const ScriptToolbarDefinition& item = persisted.scriptToolbars[index];
			if(item.id == L"runtime-toolbar-5" && item.items.size() > 1 && item.items[1].scriptUid == L"runtime-missing-script-uid") missingPreserved = true;
			if(index > 0 && item.id == definitions[1].id) orderPreserved = true;
		}
		bool missingMetadata = false, missingLabel = false;
		for(const auto& toolbar : persisted.scriptToolbars)
			if(toolbar.id == L"runtime-toolbar-5" && toolbar.items.size() > 1) {
				const auto& item = toolbar.items[1];
				missingMetadata = item.scriptName == missing.scriptName && item.relativePath == missing.relativePath;
				const ScriptToolbarRuntime* runtime = m_scriptToolbars.Find(toolbar.id);
				if(runtime != NULL) {
					ScriptsToolbarTarget target = {}; target.id = toolbar.id; target.name = toolbar.name; target.toolbar = runtime->window; target.items = toolbar.items;
					std::vector<ScriptsToolbarCommand> available; CSimpleArray<TBBUTTON> defaults;
					CScriptsToolbarCustomizeDlg dialog(runtime->window, available, defaults, _Settings, std::vector<ScriptsToolbarTarget>(1, target),
						[](const CString&, const std::vector<PortableToolbarItem>&) { return false; }, true, CString(L"Assignment metadata"));
					if(dialog.Create(m_hWnd) != NULL) {
						CListBox list(dialog.GetDlgItem(IDC_SCRIPTS_TOOLBAR_CURRENT)); CString label;
						const int length = list.GetTextLen(1);
						if(length >= 0) { list.GetText(1, label.GetBuffer(length + 1)); label.ReleaseBuffer(); }
						missingLabel = label.Find(missing.scriptName) >= 0 && label.Find(missing.relativePath) >= 0;
						dialog.DestroyWindow();
					}
				}
			}
		bool knownMetadata = false;
		for(const auto& toolbar : persisted.scriptToolbars)
			if(toolbar.id == L"runtime-toolbar-5" && !toolbar.items.empty())
				for(int index = 0; index < m_scripts.Menu().Count(); ++index) {
					const auto& script = m_scripts.Menu().Item(index);
					const auto& item = toolbar.items.front();
					if(script.uid == knownUid && !knownUid.IsEmpty())
						knownMetadata = item.scriptUid == script.uid && item.relativePath == script.relativePath &&
							!item.scriptName.IsEmpty() && (script.name.IsEmpty() || item.scriptName == script.name);
				}
		const bool passed = defaultNames && created && renamedAndReordered && hidden && shown && deleted && incremental && untouchedStable && reloads && loaded && missingPreserved && orderPreserved && missingMetadata && missingLabel && knownMetadata;
		CStringA metadataReport; metadataReport.Format("missing-metadata=%d\nmissing-label=%d\nknown-metadata=%d\n", missingMetadata, missingLabel, knownMetadata);
		CStringA report; report.Format("phase=script-toolbar-lifecycle\nmode=%s\ndefault-names=%d\ncreated=%d\nrenamed-reordered=%d\nhidden=%d\nshown=%d\ndeleted=%d\nincremental=%d\nuntouched-stable=%d\nreloads=%d\nmissing-uid=%d\npersistence=%d\nresult=%s\n", DeploymentContext::CurrentMode() == DeploymentContext::Mode::Portable ? "portable" : "installed", defaultNames, created, renamedAndReordered, hidden, shown, deleted, incremental, untouchedStable, reloads, missingPreserved, loaded && orderPreserved, passed ? "pass" : "fail");
		report += metadataReport;
		WritePortableStateTestText(reportPath, report); PostMessage(WM_CLOSE); return;
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
	else if (toolbarLayoutWrite || toolbarLayoutRead || missingScriptRead || returnedScriptRead)
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
			missingScriptRead ? (missingScriptSafe ? "pass" : "fail") : commandLayout && scriptsLayout && (returnedScriptRead ? !m_scripts.HasLastScript() : lastScriptIsBeta) ? "pass" : "fail");
		WritePortableStateTestText(reportPath, report);
	}
	else if (scriptsReload)
	{
		_Settings.SetCheckScriptsOnStartup(true, true);
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
		CString identity;
		for(int index = 0; index < m_scripts.Menu().Count(); ++index)
			if(m_scripts.Menu().Item(index).relativePath == L"tools/foo.js") identity = L"script:" + m_scripts.Menu().Item(index).uid;
		CHotkey* foo = scripts && !identity.IsEmpty() ? _Settings.GetHotkeyByName(identity, *scripts) : NULL;
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
		m_recentDocuments.List().AddToList(U::GetProgDirFile(L"portable-state-sentinel.fb2"));
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
		for (int index = 0; index < m_recentDocuments.List().m_arrDocs.GetSize(); ++index)
			if (CString(m_recentDocuments.List().m_arrDocs[index].szDocName).Find(L"portable-state-sentinel.fb2") >= 0) mruFound = true;
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

