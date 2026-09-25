param([string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path)
$ErrorActionPreference = 'Stop'
function Text([string]$path) { Get-Content -Raw -LiteralPath (Join-Path $RepoRoot $path) }
function Must([string]$text, [string]$pattern, [string]$name) { if($text -notmatch $pattern) { throw "Missing navigation scripts contract: $name" } }

$tree = Text 'src\fbe\TreeView.cpp'
$treeHeader = Text 'src\fbe\TreeView.h'; $resource = Text 'src\fbe\resource.h'
$documentTree = Text 'src\fbe\DocumentTree.cpp'
$documentTreeHeader = Text 'src\fbe\DocumentTree.h'
$frame = Text 'src\fbe\mainfrm.cpp'
$settings = Text 'src\fbe\settings\SettingsSerialization.cpp'
$localization = Text 'localization\app-ui\catalog.json'

Must $treeHeader 'struct ScriptTreeVisual' 'tree receives a lightweight discovered visual snapshot'
Must $treeHeader 'std::vector<int> m_script_images' 'tree caches catalog image indices'
Must $frame 'm_document_tree\.SetScriptCatalog\(m_scripts\.Menu\(\)\.Items\(\)' 'navigation tree is fed from the same runnable menu catalog'
Must $tree 'BuildScriptChildren\(root, CString\(\)\)' 'tree recursively starts from the catalog root'
Must $tree 'if\(script\.parentId != parentId\) continue;' 'tree builds children by parentId without flat-order dependency'
Must $tree 'BuildScriptChildren\(item, script\.id\)' 'tree supports arbitrary folder nesting'
Must $tree 'ID_SCRIPT_BASE \+ script->commandId' 'script activation uses the existing command runtime path'
Must $frame 'OnToolsScript\(0, static_cast<WORD>\(ID_SCRIPT_BASE \+ commandId\)' 'tree activation delegates to the existing main-frame script command runtime'
Must $tree 'NavigationPopupAddToolbarBase \+ static_cast<UINT>\(index\)' 'toolbar submenu is built dynamically'
Must $tree 'TPM_RETURNCMD' 'popup returns local commands without global routing'
Must $treeHeader 'enum NavigationPopupCommand' 'popup IDs are local enum values'
Must $tree 'ExecuteScriptPopupCommand\(command\)' 'popup dispatch uses the same production command handler exercised by runtime tests'
if($treeHeader -match '57600|kNavigationPopupFirst|kNavigationPopupLast') { throw 'Navigation popup must not reserve global resource IDs.' }
Must $tree 'PrepareScriptImages\(\)' 'visual mapping is prepared outside tree rebuild'
Must $tree 'const bool refreshImages = m_script_images\.size\(\) != items\.size\(\) \|\| !ScriptVisualsMatch\(m_script_visuals, visuals, items\.size\(\)\);' 'unchanged catalog refresh reuses image slots'
Must $treeHeader 'ScriptImageCount\(\)' 'runtime probe reads the native image-list count without owning it'
if($tree -match 'void CTreeView::BuildScriptChildren\([\s\S]*?\n\}') { if($Matches[0] -match 'AddIcon\(|AddImage\(') { throw 'Tree rebuild must not append image-list slots.' } }
Must $tree 'visual\.icon != NULL' 'ICO visual snapshot is supported'
Must $tree 'visual\.bitmap != NULL' 'BMP visual snapshot is supported'
Must $tree 'if\(m_script_mode\) \{ bHandled = TRUE; return 0; \}' 'script mode fail-closes structural drag notifications'
Must $tree 'if\(value && m_drag\) EndDrag\(\);' 'switching to script mode releases an in-flight structural drag safely'
foreach($handler in @('OnCut', 'OnPaste', 'OnDelete', 'OnRight', 'OnLeft', 'OnMerge')) {
    $handlerStart = $tree.IndexOf("LRESULT CTreeView::$handler")
    if($handlerStart -lt 0 -or $tree.IndexOf('if(m_script_mode) return 0;', $handlerStart) -lt 0) { throw "Missing navigation scripts contract: script mode blocks $handler" }
}
Must $frame 'bool CMainFrame::AddScriptToToolbar' 'toolbar add has a transactional owner'
Must $frame 'item\.scriptUid = scriptUid' 'toolbar persistence stores script UID'
Must $frame 'ApplyScriptToolbarDefinitions\(previous, current\)' 'toolbar add uses existing persistence and runtime delta'
Must $frame 'if\(captured\.empty\(\) && !runtime\.definition\.items\.empty\(\)\) captured = runtime\.definition\.items;' 'shutdown cannot erase a custom toolbar UID after an empty native capture'
if($frame -match 'bool CMainFrame::AddScriptToToolbar[\s\S]*?\n\}') { if($Matches[0] -match 'InitializeScripts\(') { throw 'Navigation toolbar add must not reinitialize scripts.' } }
Must $frame 'void CMainFrame::RefreshNavigationScriptToolbarTargets\(\)' 'toolbar targets have a lightweight refresh path'
if($frame -match 'bool CMainFrame::ApplyScriptToolbarDefinitions[\s\S]*?\n\}') { Must $Matches[0] 'RefreshNavigationScriptToolbarTargets\(\)' 'successful toolbar update refreshes navigation targets' }
Must $documentTree 'SetDocumentTreeScripts\(' 'mode selection is persisted'
Must $documentTree 'ID_DOCUMENT_TREE_MODE_STRUCTURE' 'mode structure command has a named resource ID'
Must $documentTree 'ID_DOCUMENT_TREE_MODE_SCRIPTS' 'mode scripts command has a named resource ID'
if($documentTree -match 'm_mode_bar|RefreshModeButtons') { throw 'Navigation mode must not create a separate mode toolbar row.' }
Must $documentTree 'm_mode_button\.Create' 'mode control is a child of the pane title area'
Must $documentTree 'TBSTYLE_TOOLTIPS' 'mode button provides a native tooltip'
Must $documentTree 'WS_TABSTOP' 'mode button participates in keyboard focus navigation'
Must $documentTree 'ToggleScriptMode\(\)' 'mode button uses the existing persisted mode transition'
Must $documentTree 'AddDocumentTreeModeImage\(m_mode_images, IDR_SCRIPTS\)' 'scripts target uses the historical scripts bitmap'
Must $documentTree 'AddDocumentTreeModeImage\(m_mode_images, IDB_STRUCTURE\)' 'structure target uses the historical structure bitmap cell'
Must $documentTree 'fbe\.document_tree\.mode\.show_scripts' 'scripts tooltip is runtime-localized'
Must $documentTree 'fbe\.document_tree\.mode\.show_structure' 'structure tooltip is runtime-localized'
Must $documentTree 'ImageList_GetIcon\(source, 0, ILD_NORMAL\)' 'mode image is extracted from the legacy strip before adding to the new list'
Must $documentTree 'ImageList_AddIcon\(images, icon\)' 'mode image is copied through an icon handle rather than cross-list ImageList_Copy'
if($documentTree -match 'ImageList_Copy\(images') { throw 'Mode button must not move bitmap slots across image lists.' }
Must $documentTree 'GetModeButtonProbe' 'runtime test can verify the visible title-area button and its rect'
Must $documentTree 'TBIF_COMMAND \| TBIF_IMAGE \| TBIF_BYINDEX' 'mode button updates and probes the first toolbar button by index'
Must $documentTree 'CPaneContainer::UpdateLayout\(GET_X_LPARAM\(lParam\), GET_Y_LPARAM\(lParam\)\);\s*LayoutModeButton\(\)' 'mode button follows the close button after a pane resize'
Must $documentTree 'm_view_bar\.HideButton\(0, scripts \? TRUE : FALSE\)' 'Elements selector is hidden only in Scripts mode'
Must $documentTree 'm_view_bar\.ShowWindow\(scripts \? SW_HIDE : SW_SHOW\)' 'mode selector bar is absent in Scripts mode'
Must $documentTree 'm_view_bar\.HideButton\(1, TRUE\)' 'legacy descriptor Scripts selector is always hidden'
Must $documentTreeHeader 'IsModeSelectorVisible' 'runtime test can verify selector visibility by mode'
Must $documentTree 'm_tree\.RefreshModeControls\(\)' 'startup synchronizes the visible selector with the persisted mode'
Must $documentTree 'UiMetrics::ScaleForDpi\(28, UiMetrics::DpiForWindow\(m_hWnd\)\)' 'mode selector has a DPI-aware initial height'
Must $documentTree 'm_view_bar\.AutoSize\(\)' 'mode selector measures its text with the menu font'
if($frame.IndexOf('m_splitter.SetSplitterPos(_Settings.GetSplitterPos());') -gt $frame.IndexOf('TryRestoreRecovery();')) { throw 'Persisted splitter width must be restored before the recovery prompt.' }
Must (Text 'src\fbe\testing\RuntimeTestPortableState.inl') 'initialImage == 0 && initialCommand == ID_DOCUMENT_TREE_MODE_SCRIPTS' 'structure mode exposes the scripts target image and command'
Must (Text 'src\fbe\testing\RuntimeTestPortableState.inl') 'scriptsImage == 1 && scriptsCommand == ID_DOCUMENT_TREE_MODE_STRUCTURE' 'scripts mode exposes the structure target image and command'
Must (Text 'src\fbe\testing\RuntimeTestPortableState.inl') 'structureImage == 0 && structureCommand == ID_DOCUMENT_TREE_MODE_SCRIPTS' 'returning to structure restores the scripts target image and command'
Must $documentTree 'SetModeChangedHandler' 'mode click updates the pane title'
Must $documentTree 'FbeLoadRuntimeStringByKey\(m_tree\.m_tree\.IsScriptMode\(\)' 'pane title follows the active localized mode'
if($documentTree -match 'm_navigation_menu|fbe\.document_tree\.mode\.caption') { throw 'Navigation mode must no longer be hidden behind a View popup.' }
Must $resource 'ID_DOCUMENT_TREE_MODE_STRUCTURE\s+57600' 'mode structure resource ID is allocated'
Must $resource 'ID_DOCUMENT_TREE_MODE_SCRIPTS\s+57601' 'mode scripts resource ID is allocated'
if($documentTree -match '57872|57873') { throw 'Navigation mode commands must not use magic numbers.' }
if($tree -match 'ID_SCRIPT_BASE \+ 999') { throw 'Navigation code must use ScriptCommandCount rather than a duplicated capacity.' }
Must $settings 'DOCUMENT_TREE_SCRIPTS_KEY' 'settings schema persists navigation mode'
Must $treeHeader 'm_scriptImageList' 'script mode owns a separate alpha-compatible image list'
Must $tree 'SetImageList\(m_scriptImageList,TVSIL_NORMAL\)' 'scripts select their own image list'
Must $tree 'SetImageList\(m_ImageList,TVSIL_NORMAL\)' 'structure restores the legacy structural image list'
Must (Text 'src\fbe\testing\RuntimeTestPortableState.inl') 'navigation-scripts-runtime' 'runtime scenario exercises the native navigation tree'
Must (Text 'tools\build\verify-release.ps1') 'test-fbe-navigation-scripts-runtime\.ps1' 'release gate runs navigation runtime regression'
Must (Text '.github\workflows\build.yml') 'test-fbe-navigation-scripts-runtime\.ps1' 'CI runs navigation runtime regression'
foreach($key in @('fbe.document_tree.mode.caption', 'fbe.document_tree.mode.show_scripts', 'fbe.document_tree.mode.show_structure', 'fbe.document_tree.scripts.run', 'fbe.document_tree.scripts.add_to_toolbar', 'fbe.document_tree.scripts.open_location')) {
    Must $localization ('"' + [regex]::Escape($key) + '"') "localized $key"
}
Write-Host 'Navigation scripts tree contract passed.'
