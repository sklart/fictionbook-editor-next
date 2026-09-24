param([string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path)
$ErrorActionPreference = 'Stop'
function Text([string]$path) { Get-Content -Raw -LiteralPath (Join-Path $RepoRoot $path) }
function Must([string]$text, [string]$pattern, [string]$name) { if($text -notmatch $pattern) { throw "Missing navigation scripts contract: $name" } }

$tree = Text 'src\fbe\TreeView.cpp'
$treeHeader = Text 'src\fbe\TreeView.h'; $resource = Text 'src\fbe\resource.h'
$documentTree = Text 'src\fbe\DocumentTree.cpp'
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
Must $resource 'ID_DOCUMENT_TREE_MODE_STRUCTURE\s+57600' 'mode structure resource ID is allocated'
Must $resource 'ID_DOCUMENT_TREE_MODE_SCRIPTS\s+57601' 'mode scripts resource ID is allocated'
if($documentTree -match '57872|57873') { throw 'Navigation mode commands must not use magic numbers.' }
if($tree -match 'ID_SCRIPT_BASE \+ 999') { throw 'Navigation code must use ScriptCommandCount rather than a duplicated capacity.' }
Must $settings 'DOCUMENT_TREE_SCRIPTS_KEY' 'settings schema persists navigation mode'
Must (Text 'src\fbe\testing\RuntimeTestPortableState.inl') 'navigation-scripts-runtime' 'runtime scenario exercises the native navigation tree'
Must (Text 'tools\build\verify-release.ps1') 'test-fbe-navigation-scripts-runtime\.ps1' 'release gate runs navigation runtime regression'
Must (Text '.github\workflows\build.yml') 'test-fbe-navigation-scripts-runtime\.ps1' 'CI runs navigation runtime regression'
foreach($key in @('fbe.document_tree.mode.caption', 'fbe.document_tree.scripts.run', 'fbe.document_tree.scripts.add_to_toolbar', 'fbe.document_tree.scripts.open_location')) {
    Must $localization ('"' + [regex]::Escape($key) + '"') "localized $key"
}
Write-Host 'Navigation scripts tree contract passed.'
