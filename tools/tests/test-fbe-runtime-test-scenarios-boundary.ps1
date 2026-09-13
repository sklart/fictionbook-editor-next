<# Guards the narrow boundary between CMainFrame and test-only runtime scenarios. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$mainSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')
$scenarioSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\testing\RuntimeTestScenarios.inl')
$scenarioParts = @('RuntimeTestPortableState.inl', 'RuntimeTestArchiveAndLifecycle.inl', 'RuntimeTestUndoAndContainers.inl', 'RuntimeTestNavigationAndStructure.inl', 'RuntimeTestEditorAndExport.inl')
$scenarioText = $scenarioSource
foreach($part in $scenarioParts) {
    if($scenarioSource -notmatch [regex]::Escape($part)) { throw "Runtime scenario umbrella is missing: $part" }
    $scenarioText += "`n" + (Get-Content -Raw -LiteralPath (Join-Path $root "src\fbe\testing\$part"))
}
$modeHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\testing\RuntimeTestScenarioMode.h')
$mainHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.h')
$scriptOwner = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\scripts\ScriptUiController.h')
$pluginOwner = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\plugins\PluginUiController.h')
$project = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBE.vcxproj')

foreach($required in @(
    'testing\\RuntimeTestScenarios\.inl',
    'testing\\RuntimeTestScenarioMode\.h')) {
    if($mainSource -notmatch $required -or $project -notmatch $required) {
        throw "FBE test runtime boundary is missing: $required"
    }
}

foreach($required in @(
    'CMainFrame::RunPortableStateTestScenario',
    'CMainFrame::OnSourceMemoryBenchmark',
    'CSplitUndoProbeParent',
    'split-undo-probe',
    'body-source-transition-runtime',
    'settings-dialog-runtime')) {
    if($scenarioText -notmatch [regex]::Escape($required)) {
        throw "Runtime scenario harness is missing: $required"
    }
}

foreach($forbidden in @(
    'class\s+CSplitUndoProbeParent',
    'CMainFrame::RunPortableStateTestScenario\s*\(',
    'CMainFrame::OnSourceMemoryBenchmark\s*\(')) {
    if($mainSource -match $forbidden) {
        throw "CMainFrame production implementation retained test-only orchestration: $forbidden"
    }
}

if($modeHeader -notmatch 'namespace\s+RuntimeTests' -or $modeHeader -notmatch 'IsScenario\s*\(') {
    throw 'Runtime test scenario mode must remain a narrow testing helper.'
}

if($mainSource -match 'CMainFrame::InitPlugins\s*\(') { throw 'InitPlugins must not regroup scripts, plugins and MRU.' }
foreach($required in @('InitializeExtensionUi\s*\(', 'InitializeScripts\s*\(', 'InitializeBundledPlugins\s*\(', 'InitializeRecentDocumentsMenu\s*\(')) {
    if($mainSource -notmatch $required) { throw "MainFrame extension responsibilities are missing: $required" }
}
foreach($forbidden in @('m_script_menu', 'm_script_visuals', 'm_last_script', 'm_import_plugins', 'm_export_plugins', 'm_last_plugin')) {
    if($mainHeader -match $forbidden) { throw "CMainFrame must not own extension state: $forbidden" }
}
if($mainSource -match 'g_pluginManager') { throw 'Bundled plugin manager must not have process-global lifetime.' }
foreach($required in @('class\s+UiController', 'MenuBuilder\s+m_menu', 'VisualResources\s+m_visuals', 'CString\s+m_lastRelativePath')) {
    if($scriptOwner -notmatch $required) { throw "Script owner is missing: $required" }
}
foreach($required in @('PluginManager\s+m_manager', 'CSimpleArray<CLSID>\s+m_importPlugins', 'CSimpleArray<CLSID>\s+m_exportPlugins')) {
    if($pluginOwner -notmatch $required) { throw "Plugin owner is missing: $required" }
}

Write-Host 'Runtime test scenario boundary contract passed.'
