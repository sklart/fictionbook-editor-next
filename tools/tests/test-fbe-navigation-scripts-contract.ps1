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

Must $treeHeader 'SetScriptCatalog\(const std::vector<ScriptDescriptor>& items' 'tree receives ScriptCatalog descriptors, not paths'
Must $frame 'm_document_tree\.SetScriptCatalog\(m_scripts\.Menu\(\)\.Items\(\)' 'navigation tree is fed from the same runnable menu catalog'
Must $tree '!m_script_mode|m_script_mode' 'tree has a distinct scripts mode'
Must $tree 'ID_SCRIPT_BASE \+ script->commandId' 'script activation uses the existing command runtime path'
Must $tree 'ID_DOCUMENT_TREE_ADD_SCRIPT_TOOLBAR_BASE' 'toolbar submenu is built dynamically'
Must $frame 'bool CMainFrame::AddScriptToToolbar' 'toolbar add has a transactional owner'
Must $frame 'item\.scriptUid = scriptUid' 'toolbar persistence stores script UID'
Must $frame 'ApplyScriptToolbarDefinitions\(previous, current\)' 'toolbar add uses existing persistence and runtime delta'
if($frame -match 'bool CMainFrame::AddScriptToToolbar[\s\S]*?\n\}') { if($Matches[0] -match 'InitializeScripts\(') { throw 'Navigation toolbar add must not reinitialize scripts.' } }
Must $documentTree 'SetDocumentTreeScripts\(' 'mode selection is persisted'
Must $settings 'DOCUMENT_TREE_SCRIPTS_KEY' 'settings schema persists navigation mode'
foreach($key in @('fbe.document_tree.mode.caption', 'fbe.document_tree.scripts.run', 'fbe.document_tree.scripts.add_to_toolbar', 'fbe.document_tree.scripts.open_location')) {
    Must $localization ('"' + [regex]::Escape($key) + '"') "localized $key"
}
Write-Host 'Navigation scripts tree contract passed.'
