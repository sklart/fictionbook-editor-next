
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
	const bool scriptToolbarLifecycle = IsFbeTestScenario(L"script-toolbar-lifecycle-runtime");
	const bool scriptToolbarLifecycleReload = IsFbeTestScenario(L"script-toolbar-lifecycle-reload-runtime");
	const bool scriptToolbarRollbackNoMain = IsFbeTestScenario(L"script-toolbar-rollback-no-main-runtime");
	const bool scriptToolbarRollbackPersisted = IsFbeTestScenario(L"script-toolbar-rollback-persisted-runtime");
	const bool scriptToolbarRollbackPartial = IsFbeTestScenario(L"script-toolbar-rollback-partial-runtime");
	const bool scriptToolbarRuntimeSize = IsFbeTestScenario(L"script-toolbar-runtime-size");
	const bool navigationScriptsRuntime = IsFbeTestScenario(L"navigation-scripts-runtime");
	const bool navigationScriptsReloadRuntime = IsFbeTestScenario(L"navigation-scripts-reload-runtime");
	if (!ordinaryWrite && !ordinaryRead && !emptyToolbarWrite && !emptyToolbarRead && !toolbarLayoutWrite && !toolbarLayoutRead && !missingScriptRead && !malformedToolbarRead && !scriptsReload && !legacyHotkeyRead && !diagnosticCleanup && !scriptToolbarLifecycle && !scriptToolbarLifecycleReload && !scriptToolbarRollbackNoMain && !scriptToolbarRollbackPersisted && !scriptToolbarRollbackPartial && !scriptToolbarRuntimeSize && !navigationScriptsRuntime && !navigationScriptsReloadRuntime)
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
	if (DeploymentContext::CurrentMode() != DeploymentContext::Mode::Portable && !scriptToolbarLifecycle && !scriptToolbarLifecycleReload && !scriptToolbarRollbackNoMain && !scriptToolbarRollbackPersisted && !scriptToolbarRollbackPartial && !scriptToolbarRuntimeSize && !navigationScriptsRuntime && !navigationScriptsReloadRuntime)
	{
		WritePortableStateTestText(reportPath, "phase=failed\nreason=not-portable\n");
		PostMessage(WM_CLOSE);
		return;
	}
	auto currentDefinitions = [&]() { std::vector<ScriptToolbarDefinition> result; for(size_t index = 0; index < m_scriptToolbars.Items().size(); ++index) result.push_back(m_scriptToolbars.Items()[index].definition); return result; };
	auto mainHasDefault = [&]() { TBBUTTON button = {}; return m_ScriptsToolbar.GetButtonCount() > 0 && m_ScriptsToolbar.GetButton(0, &button) && button.idCommand == ID_LAST_SCRIPT; };
	if(scriptToolbarRuntimeSize)
	{
		::CreateDirectory(scriptsDirectory, NULL);
		_Settings.SetScriptsFolder(scriptsDirectory, true);
		if(!InitializeScripts()) { WritePortableStateTestText(reportPath, "phase=script-toolbar-runtime-size\nreason=initial-reload\nresult=fail\n"); PostMessage(WM_CLOSE); return; }
		std::vector<ScriptToolbarDefinition> previous = currentDefinitions(), definitions = previous;
		for(int index = 1; index <= 8; ++index) { ScriptToolbarDefinition item; item.id.Format(L"runtime-size-%d", index); item.name.Format(L"Runtime size %d", index); item.visible = true; definitions.push_back(item); }
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
		std::vector<ScriptToolbarDefinition> hidden = current;
		for(size_t index = 0; index < hidden.size(); ++index) if(hidden[index].id == L"runtime-size-2") { hidden[index].visible = false; break; }
		const bool hiddenOk = added && ApplyScriptToolbarDefinitions(current, hidden);
		const bool shown = hiddenOk && ApplyScriptToolbarDefinitions(hidden, current) && oneRow();
		const bool restarted = shown && InitializeScripts() && oneRow();
		const bool passed = created && added && hiddenOk && shown && restarted;
		CStringA report; report.Format("phase=script-toolbar-runtime-size\ncreated=%d\nadd-script=%d\nhide-show=%d\nrestart=%d\nresult=%s\n", created, added, hiddenOk && shown, restarted, passed ? "pass" : "fail");
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
		const bool modeButtonReady = !tree.IsScriptMode() && !_Settings.DocumentTreeScripts() && m_document_tree.m_tree.IsModeSelectorVisible() && m_document_tree.GetModeButtonProbe(title, modeButton, closeButton, initialImage, initialCommand) && initialImage == 0 && initialCommand == ID_DOCUMENT_TREE_MODE_SCRIPTS;
		m_document_tree.m_tree.ToggleScriptMode();
		RECT scriptsTitle = {}, scriptsButton = {}, scriptsClose = {}; int scriptsImage = -1; UINT scriptsCommand = 0;
		const bool switchedScripts = tree.IsScriptMode() && _Settings.DocumentTreeScripts() && !m_document_tree.m_tree.IsModeSelectorVisible() && m_document_tree.GetModeButtonProbe(scriptsTitle, scriptsButton, scriptsClose, scriptsImage, scriptsCommand) && scriptsImage == 1 && scriptsCommand == ID_DOCUMENT_TREE_MODE_STRUCTURE;
		m_document_tree.m_tree.ToggleScriptMode();
		RECT structureTitle = {}, structureButton = {}, structureClose = {}; int structureImage = -1; UINT structureCommand = 0;
		const bool switchedStructure = !tree.IsScriptMode() && !_Settings.DocumentTreeScripts() && m_document_tree.m_tree.IsModeSelectorVisible() && m_document_tree.GetModeButtonProbe(structureTitle, structureButton, structureClose, structureImage, structureCommand) && structureImage == 0 && structureCommand == ID_DOCUMENT_TREE_MODE_SCRIPTS;
		m_document_tree.m_tree.ToggleScriptMode();
		HTREEITEM folderA = tree.FindScriptTreeItem(L"foldera");
		HTREEITEM child = tree.FindScriptTreeItem(L"foldera/child.js");
		const HTREEITEM folderB = tree.FindScriptTreeItem(L"foldera/folderb");
		const HTREEITEM deep = tree.FindScriptTreeItem(L"foldera/folderb/deep.js");
		const HTREEITEM root = tree.FindScriptTreeItem(L"root.js");
		const bool hierarchy = folderA != NULL && child != NULL && folderB != NULL && deep != NULL && root != NULL &&
			tree.HasScriptTreeParent(L"foldera/child.js", L"foldera") && tree.HasScriptTreeParent(L"foldera/folderb/deep.js", L"foldera/folderb") && tree.ScriptTreeNodeCount() == 5;
		const bool visualMapping = tree.ScriptTreeImage(root) >= 0 && tree.ScriptTreeImage(folderA) >= 0 && tree.ScriptTreeImage(child) >= 0 && tree.ScriptTreeImage(folderB) >= 0 && tree.ScriptTreeImage(deep) >= 0 && tree.ScriptImageList() != tree.StructuralImageList();
		std::vector<ScriptDescriptor> reverseCatalog = m_scripts.Menu().Items(); std::vector<ScriptTreeVisual> reverseVisuals;
		for(int index = 0; index < m_scripts.Menu().Count(); ++index) { ScriptTreeVisual visual; visual.icon = m_scripts.Menu().VisualAt(index).icon; visual.bitmap = m_scripts.Menu().VisualAt(index).bitmap; reverseVisuals.push_back(visual); }
		std::reverse(reverseCatalog.begin(), reverseCatalog.end()); std::reverse(reverseVisuals.begin(), reverseVisuals.end());
		tree.SetScriptCatalog(reverseCatalog, reverseVisuals, std::vector<ScriptTreeToolbarTarget>(), std::function<void(const CString&, const CString&)>(), std::function<void(const CString&)>(), std::function<void(UINT)>());
		const bool unorderedHierarchy = tree.HasScriptTreeParent(L"foldera/child.js", L"foldera") && tree.HasScriptTreeParent(L"foldera/folderb/deep.js", L"foldera/folderb") && tree.ScriptTreeNodeCount() == 5;
		RefreshNavigationScriptTree();
		const int imagesBefore = tree.ScriptImageCount(); RefreshNavigationScriptTree(); RefreshNavigationScriptTree(); tree.SetScriptMode(false); tree.SetScriptMode(true); const bool imagesStable = tree.ScriptImageCount() == imagesBefore;
		// The fixture scripts deliberately have no UI body.  Probe navigation
		// dispatch without asynchronously executing one while startup closes.
		std::vector<ScriptDescriptor> commandCatalog = m_scripts.Menu().Items(); std::vector<ScriptTreeVisual> commandVisuals; std::vector<ScriptTreeToolbarTarget> commandTargets;
		for(int index = 0; index < m_scripts.Menu().Count(); ++index) { ScriptTreeVisual visual; visual.icon = m_scripts.Menu().VisualAt(index).icon; visual.bitmap = m_scripts.Menu().VisualAt(index).bitmap; commandVisuals.push_back(visual); }
		for(size_t index = 0; index < m_scriptToolbars.Items().size(); ++index) { ScriptTreeToolbarTarget target; target.id = m_scriptToolbars.Items()[index].definition.id; target.name = m_scriptToolbars.Items()[index].definition.name; commandTargets.push_back(target); }
		tree.SetScriptCatalog(commandCatalog, commandVisuals, commandTargets, [this](const CString& uid, const CString& id) { AddScriptToToolbar(uid, id); }, std::function<void(const CString&)>(), [this](UINT commandId) { for(int index = 0; index < m_scripts.Menu().Count(); ++index) if(!m_scripts.Menu().Item(index).isFolder && m_scripts.Menu().Item(index).commandId == static_cast<int>(commandId)) { m_scripts.SetLastScript(m_scripts.Menu().Item(index)); break; } });
		folderA = tree.FindScriptTreeItem(L"foldera"); child = tree.FindScriptTreeItem(L"foldera/child.js"); const HTREEITEM refreshedRoot = tree.FindScriptTreeItem(L"root.js"); const HTREEITEM refreshedDeep = tree.FindScriptTreeItem(L"foldera/folderb/deep.js");
		m_scripts.ClearLastScript(); tree.SelectItem(child); BOOL handled = FALSE; tree.OnKeyDown(WM_KEYDOWN, VK_RETURN, 0, handled);
		const ScriptDescriptor* ran = m_scripts.LastScript(); const bool enterRuns = handled && ran != NULL && ran->relativePath == L"foldera/child.js";
		CTreeItem(folderA, &tree).Expand(TVE_EXPAND); m_scripts.ClearLastScript(); CRect childRect; tree.GetItemRect(child, &childRect, TRUE); BOOL doubleClickHandled = FALSE; tree.OnDblClick(WM_LBUTTONDBLCLK, 0, MAKELPARAM(childRect.left + 2, childRect.top + 2), doubleClickHandled);
		ran = m_scripts.LastScript(); const bool doubleClickRuns = ran != NULL && ran->relativePath == L"foldera/child.js";
		m_scripts.ClearLastScript(); tree.SelectItem(folderA); handled = FALSE; tree.OnKeyDown(WM_KEYDOWN, VK_RETURN, 0, handled);
		const bool folderOnly = handled && m_scripts.LastScript() == NULL;
		NMTREEVIEW drag = {}; drag.itemNew.hItem = child; BOOL dragHandled = FALSE; tree.OnBegindrag(0, reinterpret_cast<LPNMHDR>(&drag), dragHandled);
		const bool dragGuarded = dragHandled && !tree.IsStructuralDragActive();
		const UINT discoveryBefore = m_scripts.DiscoveryCount();
		std::vector<ScriptToolbarDefinition> previous = currentDefinitions(), changed = previous;
		ScriptToolbarDefinition targetA; targetA.id = L"navigation-runtime-toolbar-a"; targetA.name = L"Navigation runtime A"; targetA.visible = true; changed.push_back(targetA);
		ScriptToolbarDefinition targetB; targetB.id = L"navigation-runtime-toolbar"; targetB.name = L"Navigation runtime B"; targetB.visible = true; changed.push_back(targetB);
		const bool applied = ApplyScriptToolbarDefinitions(previous, changed);
		CString rootUid, deepUid; int childCommand = -1; for(int index = 0; index < m_scripts.Menu().Count(); ++index) { if(m_scripts.Menu().Item(index).relativePath == L"root.js") rootUid = m_scripts.Menu().Item(index).uid; if(m_scripts.Menu().Item(index).relativePath == L"foldera/folderb/deep.js") deepUid = m_scripts.Menu().Item(index).uid; if(m_scripts.Menu().Item(index).relativePath == L"foldera/child.js") childCommand = m_scripts.Menu().Item(index).commandId; }
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
		CStringA report; const bool passed = initialized && modeButtonReady && switchedStructure && switchedScripts && hierarchy && visualMapping && unorderedHierarchy && imagesStable && enterRuns && doubleClickRuns && folderOnly && dragGuarded && uidPersisted && targetLive && structuralDragWorks;
		report.Format("phase=navigation-scripts\nmode-button-window=%d\nmode-button-visible=%d\nmode-button-position-valid=%d\nmode-button-image-initial=%d\nmode-button-image-after-click=%d\nmode-button-image-after-second-click=%d\nmode-button-switch=%d\nhierarchy=%d\nvisual-mapping=%d\nunordered-hierarchy=%d\nimages-stable=%d\nsource-active=%d\nchild-command=%d\nenter-runs=%d\ndouble-click-runs=%d\nfolder-only=%d\ndrag-guarded=%d\nstructural-drag-works=%d\nroot-added=%d\ndeep-added=%d\nroot-uid-persisted=%d\ndeep-uid-persisted=%d\nuid-persisted=%d\ntoolbar-target-live=%d\nresult=%s\n", modeButtonReady, modeButtonReady, modeButtonReady, initialImage, scriptsImage, structureImage, switchedStructure && switchedScripts, hierarchy, visualMapping, unorderedHierarchy, imagesStable, IsSourceActive(), childCommand, enterRuns, doubleClickRuns, folderOnly, dragGuarded, structuralDragWorks, rootAdded, deepAdded, rootUidPersisted, deepUidPersisted, uidPersisted, targetLive, passed ? "pass" : "fail");
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
		// The lifecycle probe exercises toolbar ownership, not the bundled script
		// catalogue.  A fresh empty test folder makes its repeated reloads bounded
		// and deterministic on both portable and isolated installed workers.
		::CreateDirectory(scriptsDirectory, NULL);
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
		PortableToolbarItem missing = {}; missing.scriptUid = L"runtime-missing-script-uid"; definitions.back().items.push_back(missing);
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
		const bool passed = defaultNames && created && renamedAndReordered && hidden && shown && deleted && incremental && untouchedStable && reloads && loaded && missingPreserved && orderPreserved;
		CStringA report; report.Format("phase=script-toolbar-lifecycle\nmode=%s\ndefault-names=%d\ncreated=%d\nrenamed-reordered=%d\nhidden=%d\nshown=%d\ndeleted=%d\nincremental=%d\nuntouched-stable=%d\nreloads=%d\nmissing-uid=%d\npersistence=%d\nresult=%s\n", DeploymentContext::CurrentMode() == DeploymentContext::Mode::Portable ? "portable" : "installed", defaultNames, created, renamedAndReordered, hidden, shown, deleted, incremental, untouchedStable, reloads, missingPreserved, loaded && orderPreserved, passed ? "pass" : "fail");
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

