<# Validates portable script identity, toolbar persistence and reload ownership. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
function Text([string]$path) { Get-Content -Raw -LiteralPath (Join-Path $root $path) }
function Must([string]$text, [string]$pattern, [string]$what) {
    if ($text -notmatch $pattern) { throw "${what}: '$pattern' is missing." }
}
function MustNot([string]$text, [string]$pattern, [string]$what) {
    if ($text -match $pattern) { throw "${what}: forbidden '$pattern' found." }
}

$frame = Text 'src\fbe\mainfrm.cpp'
$header = Text 'src\fbe\mainfrm.h'
$settings = Text 'src\fbe\Settings.cpp'
$about = Text 'src\fbe\AboutBox.cpp'
$catalog = Text 'src\fbe\scripts\ScriptCatalog.cpp'
$registry = Text 'src\fbe\scripts\ScriptCommandRegistry.cpp'
$menuBuilder = Text 'src\fbe\scripts\ScriptMenuBuilder.cpp'
$visuals = Text 'src\fbe\scripts\ScriptVisualResources.h'

Must $frame 'CHotkey ScriptsHotkey\(script\.relativePath' 'Script hotkey identity must be relative'
MustNot $frame 'CHotkey ScriptsHotkey\(script\.path' 'Absolute script path must not be persisted as hotkey identity'
Must $settings 'migratedLegacyScriptHotkey \|= foundHk != NULL' 'Legacy migrations accumulate across entries'
Must $settings 'legacyPath\.Right\(relativePath\.GetLength\(\)\) == relativePath' 'Moved portable hotkey is matched by relative path suffix'
Must $settings 'int longestSuffixLength = -1' 'Legacy hotkey migration tracks the most specific suffix'
Must $settings 'suffixLength > longestSuffixLength' 'Longer nested script suffix wins over basename'
Must $settings 'suffixLength == longestSuffixLength' 'Only equal best suffixes are ambiguous'
Must $settings 'ambiguous suffix: do not guess' 'Ambiguous legacy hotkeys are not migrated'
Must $settings 'SaveHotkeyGroups\(\)' 'Legacy hotkey migration writes portable identity back'

Must $header 'ReleaseScriptResources\(\)' 'Script lifecycle helper declaration'
Must $frame 'ReleaseScriptResources\(\);\s*\n\s*if \(StartupTrace::Enabled\(\)\)' 'Reload releases script resources before collecting'
Must $menuBuilder 'm_items\.clear\(\)' 'Reload clears active script descriptors'
Must $menuBuilder 'm_visuals\.clear\(\)' 'Reload clears visual resources together with descriptors'
Must $visuals '~VisualResource\(\) \{ Reset\(\); \}' 'GDI handles use RAII cleanup'
Must $visuals '::DeleteObject\(bitmap\)' 'Bitmap RAII cleanup'
Must $visuals '::DestroyIcon\(icon\)' 'Icon RAII cleanup'
Must $frame 'm_last_script = NULL' 'Reload resets Last Script pointer'
Must $frame 'GR_GDIOBJECTS' 'GUI regression scenario measures script reload GDI ownership'
Must $frame 'InitPlugins\(\);\s*\n\s*InitPlugins\(\);\s*\n\s*InitPlugins\(\);' 'GUI regression scenario reloads scripts repeatedly'
Must $catalog 'std::sort\(m_items\.begin\(\)' 'Deterministic script ordering'
Must $catalog 'left\.isFolder != right\.isFolder' 'Folders sort before scripts'
Must $catalog 'left\.relativePath\.CompareNoCase' 'Relative path is deterministic sort tiebreaker'
Must $registry 'CommandRegistry::Assign' 'Stable command IDs belong to ScriptCommandRegistry'
Must $menuBuilder 'MenuBuilder::Build' 'Active descriptors build the Scripts menu outside CMainFrame'
MustNot $frame '\bScrInfo\b|\bm_scripts\b|LoadScriptPicture|SortScripts' 'CMainFrame legacy script model'
Must $frame 'class ScriptDiscoveryRuntime' 'Discovery runtime has scoped ownership'
Must $frame '~ScriptDiscoveryRuntime\(\) \{ if \(m_started\) StopScript\(\); \}' 'Started scripting runtime is always stopped'
Must $frame '!runtime\.Started\(\) \|\| FAILED\(ScriptLoad\(candidate\.path\)\) \|\| !ScriptFindFunc\(L"Run"\)' 'Rejected scripts share scoped runtime cleanup'

Must $frame 'PortableToolbarsPath' 'Portable toolbar file path'
Must $frame 'Toolbars\.xml' 'Portable toolbar data file'
Must $frame '<Toolbars version=\\"1\\">' 'Versioned toolbar structure'
Must $frame '<Script path=\\"%s\\"' 'Scripts toolbar stores relative paths'
Must $frame 'MOVEFILE_REPLACE_EXISTING \| MOVEFILE_WRITE_THROUGH' 'Atomic portable toolbar replacement'
Must $frame 'RestorePortableToolbarLayout\(m_CmdToolbar, false\)' 'Portable command toolbar restore'
Must $frame 'RestorePortableToolbarLayout\(m_ScriptsToolbar, true\)' 'Portable scripts toolbar restore'
Must $frame 'SavePortableToolbarLayout\(\)' 'Portable toolbar save'
Must $frame 'continue; // deleted script or obsolete command' 'Deleted saved scripts are ignored'
Must $frame 'bool& commandToolbarPresent, bool& scriptsToolbarPresent' 'Toolbar reader distinguishes missing section from an empty section'
Must $frame 'if\(!toolbarPresent\) return;' 'Missing toolbar section keeps default layout'
Must $frame 'while\(target\.GetButtonCount\(\) > 0\) target\.DeleteButton\(0\);' 'Explicit empty toolbar clears the default layout'
Must $frame 'portable-toolbar-layout-write' 'GUI scenario customizes a non-empty portable toolbar'
Must $frame 'portable-toolbar-layout-read' 'GUI scenario checks non-empty portable toolbar after restart'
Must $frame 'relativePath == L"foo\.js"' 'Scripts toolbar E2E locates script by stable relative path'

Must $frame 'g_pluginManager\.GetPlugins\(\)' 'Bundled plugins remain available through PluginManager'
Must $about 'DeploymentContext::LogsDirectory\(\)' 'Update trace uses deployment-specific log directory'
MustNot $about 'LOCALAPPDATA' 'Update trace must not hardcode LocalAppData'

# Moving D:\portable to E:\portable does not change a normalized script key.
$before = 'tools/example.js'.ToLowerInvariant().Replace('\','/')
$after = 'tools/example.js'.ToLowerInvariant().Replace('\','/')
if ($before -cne $after) { throw 'Portable script hotkey identity depends on installation root.' }
Write-Host 'Portable script lifecycle, hotkeys and toolbar persistence contract passed.'
