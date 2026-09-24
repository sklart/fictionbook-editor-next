param([string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path)
$ErrorActionPreference = 'Stop'
function Text([string]$path) { Get-Content -Raw -LiteralPath (Join-Path $RepoRoot $path) }
function Must([string]$text, [string]$pattern, [string]$name) { if($text -notmatch $pattern) { throw "Missing navigation scripts contract: $name" } }

$tree = Text 'src\fbe\TreeView.cpp'
$treeHeader = Text 'src\fbe\TreeView.h'
$documentTree = Text 'src\fbe\DocumentTree.cpp'
$frame = Text 'src\fbe\mainfrm.cpp'
$settings = Text 'src\fbe\settings\SettingsSerialization.cpp'
$localization = Text 'localization\app-ui\catalog.json'

Must $treeHeader 'SetScriptCatalog\(const std::vector<ScriptDescriptor>& items, const std::vector<HICON>& icons' 'tree receives catalog descriptors and discovered visual snapshot'
Must $frame 'm_document_tree\.SetScriptCatalog\(m_scripts\.Menu\(\)\.Items\(\)' 'navigation tree is fed from the same runnable menu catalog'
Must $tree 'BuildScriptChildren\(root, CString\(\)\)' 'tree recursively starts from the catalog root'
Must $tree 'if\(script\.parentId != parentId\) continue;' 'tree builds children by parentId without flat-order dependency'
Must $tree 'BuildScriptChildren\(item, script\.id\)' 'tree supports arbitrary folder nesting'
Must $tree 'ID_SCRIPT_BASE \+ script->commandId' 'script activation uses the existing command runtime path'
Must $tree 'kNavigationPopupAddToolbarBase \+ static_cast<UINT>\(index\)' 'toolbar submenu is built dynamically'
Must $tree 'static_assert\(kNavigationPopupFirst > ID_VIEW_SCRIPT_TOOLBAR_DYNAMIC_LAST' 'popup IDs do not overlap dynamic toolbar IDs'
Must $tree 'static_assert\(kNavigationPopupFirst > ID_SPELL_REPLACE_LAST' 'popup IDs do not overlap spell commands'
Must $tree 'static_assert\(kNavigationPopupFirst > ID_PLUGIN_EXPORT_LAST' 'popup IDs do not overlap plugin commands'
Must $tree 'static_assert\(kNavigationPopupFirst > ID_SCI_EXPAND9' 'popup IDs do not overlap Scintilla commands'
Must $tree 'if\(m_script_mode\) \{ bHandled = TRUE; return 0; \}' 'script mode fail-closes structural drag notifications'
foreach($handler in @('OnCut', 'OnPaste', 'OnDelete', 'OnRight', 'OnLeft', 'OnMerge')) {
    $handlerStart = $tree.IndexOf("LRESULT CTreeView::$handler")
    if($handlerStart -lt 0 -or $tree.IndexOf('if(m_script_mode) return 0;', $handlerStart) -lt 0) { throw "Missing navigation scripts contract: script mode blocks $handler" }
}
Must $frame 'bool CMainFrame::AddScriptToToolbar' 'toolbar add has a transactional owner'
Must $frame 'item\.scriptUid = scriptUid' 'toolbar persistence stores script UID'
Must $frame 'ApplyScriptToolbarDefinitions\(previous, current\)' 'toolbar add uses existing persistence and runtime delta'
if($frame -match 'bool CMainFrame::AddScriptToToolbar[\s\S]*?\n\}') { if($Matches[0] -match 'InitializeScripts\(') { throw 'Navigation toolbar add must not reinitialize scripts.' } }
Must $frame 'void CMainFrame::RefreshNavigationScriptToolbarTargets\(\)' 'toolbar targets have a lightweight refresh path'
if($frame -match 'bool CMainFrame::ApplyScriptToolbarDefinitions[\s\S]*?\n\}') { Must $Matches[0] 'RefreshNavigationScriptToolbarTargets\(\)' 'successful toolbar update refreshes navigation targets' }
Must $documentTree 'SetDocumentTreeScripts\(' 'mode selection is persisted'
Must $settings 'DOCUMENT_TREE_SCRIPTS_KEY' 'settings schema persists navigation mode'
foreach($key in @('fbe.document_tree.mode.caption', 'fbe.document_tree.scripts.run', 'fbe.document_tree.scripts.add_to_toolbar', 'fbe.document_tree.scripts.open_location')) {
    Must $localization ('"' + [regex]::Escape($key) + '"') "localized $key"
}
Write-Host 'Navigation scripts tree contract passed.'
