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

Write-Host 'Runtime test scenario boundary contract passed.'
