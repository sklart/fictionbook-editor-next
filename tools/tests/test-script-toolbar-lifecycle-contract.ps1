<# Focused source contract for hidden-panel editing and lifecycle runtime coverage. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
function Text([string]$path) { Get-Content -Raw -LiteralPath (Join-Path $root $path) }
function Must([string]$text, [string]$pattern, [string]$what) { if($text -notmatch $pattern) { throw "${what}: '$pattern' is missing." } }

$dialog = Text 'src\fbe\ScriptsToolbarCustomizeDlg.cpp'
$frame = Text 'src\fbe\mainfrm.cpp'
$runtime = Text 'src\fbe\testing\RuntimeTestPortableState.inl'
$runner = Text 'tools\tests\test-script-toolbar-lifecycle-runtime.ps1'
$verify = Text 'tools\build\verify-release.ps1'
$workflow = Text '.github\workflows\build.yml'
Must $dialog 'std::vector<PortableToolbarItem>& CScriptsToolbarCustomizeDlg::CurrentItems' 'Customize dialog owns a definition-item editing model'
Must $dialog 'CurrentItems\(\)\.push_back' 'Hidden definition can receive scripts and separators'
Must $dialog 'ApplyCurrentItemsToRuntimeToolbar\(\)' 'Runtime toolbar is optional projection of definition changes'
Must $dialog 'if\(index >= 0 && index < static_cast<int>\(m_panels\.size\(\)\)\)' 'Selector accepts hidden panels without an HWND gate'
Must $frame 'target\.id = runtime\.definition\.id' 'Selector is populated from stable toolbar definitions'
Must $frame 'UpdateScriptToolbarItems' 'Definition-content edits use the persistence layer directly'
Must $frame 'const bool hadPersistedMainDefinition = before\.scriptsToolbarPresent' 'Rollback snapshots actual persisted main-toolbar state'
Must $frame 'InitializeScriptsFromDefinitions\(previous, hadPersistedMainDefinition\)' 'Rollback restores runtime from previous in-memory definitions'
Must $dialog 'm_panels\[CurrentPanelIndex\(\)\]\.id == L"scripts-main"' 'Reset keeps defaults only for the main script toolbar'
Must $dialog 'm_scriptImages = reinterpret_cast<HIMAGELIST>\(::SendMessage\(m_toolbar, TB_GETIMAGELIST' 'Hidden toolbar uses the stable main Scripts image list'
Must $dialog 'ImageList_Draw\(m_scriptImages' 'Current hidden toolbar items retain script icons'
Must $runtime 'runtime-toolbar-%d' 'Runtime regression creates unbounded additional panels'
Must $runtime 'for\(int cycle = 0; cycle < 3' 'Runtime regression repeats InitializeScripts lifecycle'
Must $runtime 'runtime-missing-script-uid' 'Runtime regression preserves an absent script UID position'
Must $runtime 'script-toolbar-lifecycle-reload-runtime' 'Runtime regression checks a fresh reload process'
Must $runner 'IncludeInstalled' 'Runtime runner covers installed mode on an isolated CI worker'
Must $runner 'FBE_CI_ISOLATED_PROFILE' 'Installed runner rejects non-isolated developer profiles'
Must $runner 'Remove-Item -LiteralPath \$portableData -Recurse -Force' 'Portable lifecycle data is recreated and cleaned up'
Must $runner "'--portable'" 'Runtime runner covers portable mode'
Must $verify 'test-script-toolbar-lifecycle-runtime\.ps1.*-FbeExe' 'Verify-release executes the real portable lifecycle test'
Must $workflow 'FBE_CI_ISOLATED_PROFILE' 'CI declares an isolated profile before installed lifecycle testing'
Must $workflow 'test-script-toolbar-lifecycle-runtime\.ps1.*-IncludeInstalled' 'CI executes the real installed lifecycle test'
Write-Host 'Script toolbar lifecycle/UI contract passed.'
