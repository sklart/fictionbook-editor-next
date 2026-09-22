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
$lifecycleHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\document\DocumentLifecycleController.h')
$loaderSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\document\DocumentLoader.cpp')
$pendingSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\document\PendingDocument.cpp')
$recentOwner = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\document\recent\RecentDocumentsController.h')
$saveHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\document\DocumentSaveController.h')
$saveSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\document\DocumentSaveController.cpp')
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
	'phase-b-cache-runtime',
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

foreach($required in @('enum class DocumentLifecycleStatus', 'struct DocumentLifecycleResult', 'Succeeded\s*\(', 'DocumentLifecycleResult NewDocument\s*\(', 'DocumentLifecycleResult Open\s*\(', 'DocumentLifecycleResult ReloadNormal\s*\(')) {
    if($lifecycleHeader -notmatch $required) { throw "Document lifecycle result contract is missing: $required" }
}
if($loaderSource -match 'mainfrm\.h') { throw 'DocumentLoader must not depend on CMainFrame.' }
foreach($required in @('FbeRecentDocuments::Controller\s+m_recentDocuments')) {
    if($mainHeader -notmatch $required) { throw "CMainFrame recent-document boundary is missing: $required" }
}
foreach($required in @('bool Resolve\s*\(', 'void OnOpened\s*\(', 'void OnFailed\s*\(', 'void OnCancelledArchive\s*\(')) {
    if($recentOwner -notmatch $required) { throw "Recent-document controller is missing: $required" }
}
$mruHandler = [regex]::Match($mainSource, 'LRESULT CMainFrame::OnFileOpenMRU[\s\S]*?(?=LRESULT CMainFrame::OnFileSave\()').Value
if(-not $mruHandler) { throw 'OnFileOpenMRU handler is missing.' }
foreach($required in @('m_recentDocuments\.Resolve', 'm_recentDocuments\.OnOpened', 'm_recentDocuments\.OnFailed')) {
    if($mruHandler -notmatch $required) { throw "OnFileOpenMRU must delegate MRU ownership: $required" }
}
foreach($forbidden in @('\.MoveToTop\(', '\.RemoveFromList\(', 'FbeRecentDocuments::TouchMruOrder', 'FbeRecentDocuments::RebuildMruMenu')) {
    if($mruHandler -match $forbidden) { throw "OnFileOpenMRU retained MRU mutation: $forbidden" }
}
foreach($required in @('m_document\.reset\(\);\s*FB::Doc::m_active_doc = m_previous;', 'm_document\.release\(\);\s*FB::Doc::m_active_doc = committed;')) {
    if($pendingSource -notmatch $required) { throw "PendingDocument transactional rollback contract is missing: $required" }
}
$newHandler = [regex]::Match($mainSource, 'LRESULT CMainFrame::OnFileNew[\s\S]*?(?=LRESULT CMainFrame::OnFileOpen\()').Value
$loadHandler = [regex]::Match($mainSource, 'CMainFrame::FILE_OP_STATUS\s+CMainFrame::LoadFile[\s\S]*?(?=void\s+CMainFrame::GetDocumentStructure)').Value
$reloadHandler = [regex]::Match($mainSource, 'bool CMainFrame::ReloadFile[\s\S]*?(?=void CMainFrame::GoTo\(int)').Value
foreach($handler in @($newHandler, $loadHandler, $reloadHandler)) {
    if(-not $handler -or $handler -notmatch 'DocumentLifecycleController') { throw 'New/Open/Reload must delegate document orchestration to DocumentLifecycleController.' }
    if($handler -match 'PendingDocument\s+pending') { throw 'CMainFrame retained transactional document orchestration.' }
}
foreach($required in @('enum class DocumentSaveStatus', 'struct DocumentSaveResult', 'SaveCurrent\s*\(', 'SaveAsNormal\s*\(')) {
    if($saveHeader -notmatch $required) { throw "Document save boundary is missing: $required" }
}
if($saveSource -match 'mainfrm\.h') { throw 'DocumentSaveController must not depend on CMainFrame.' }
foreach($required in @('DocumentSaveController', 'DocumentSavePlan::Create', 'CommitSuccessfulSave')) {
    if($mainSource -notmatch $required) { throw "SaveFile boundary is missing: $required" }
}
if($saveSource -notmatch 'SaveDocument' -or $saveSource -notmatch 'session\.SavedArchive' -or $saveSource -notmatch 'session\.Saved\(\)') {
    throw 'DocumentSaveController must commit session only after persistence succeeds.'
}

Write-Host 'Runtime test scenario boundary contract passed.'
