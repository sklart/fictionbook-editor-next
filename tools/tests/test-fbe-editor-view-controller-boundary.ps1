<# Guards the narrow coordinator boundary for editor view changes. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$controllerHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\view\EditorViewController.h')
$controllerSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\view\EditorViewController.cpp')
$transitionHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\view\EditorViewTransition.h')
$viewState = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\view\EditorViewState.h')
$selectionState = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\view\EditorSelectionState.h')
$transferHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\SourceDocumentTransfer.h')
$mainSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')

foreach($required in @('class\s+EditorViewController', 'EditorViewChangeResult', 'Request\s*\(', 'EditorViewTransitionPlan', 'Succeeded\s*\(')) {
    if(($controllerHeader + $controllerSource) -notmatch $required) { throw "Editor view controller is missing: $required" }
}
if($controllerSource -notmatch 'MakeEditorViewTransitionPlan\s*\(') { throw 'Controller must delegate transition policy to EditorViewTransition.' }
foreach($unit in @($controllerHeader, $controllerSource)) {
    foreach($forbidden in @('mainfrm\.h', 'CMainFrame', 'FB::Doc', 'MSHTML', 'Scintilla', 'SCI_', 'HWND', 'MessageBox', 'RecoveryController', 'DocumentSaveController', 'RecentDocumentsController', 'Plugin', 'RuntimeTests', 'FBE_NEXT_TEST')) {
        if($unit -match $forbidden) { throw "EditorViewController must not depend on $forbidden." }
    }
}
foreach($required in @('class\s+EditorViewState', 'class\s+EditorSelectionState', 'PrepareSerializedSource', 'ApplySourceDocument')) {
    if(($viewState + $selectionState + $transferHeader) -notmatch $required) { throw "Existing view boundary is missing: $required" }
}
$showView = [regex]::Match($mainSource, '(?s)void\s+CMainFrame::ShowView\(.*\z').Value
if([string]::IsNullOrEmpty($showView)) { throw 'Unable to locate CMainFrame::ShowView.' }
foreach($required in @('m_editor_view_controller\.Request\s*\(', 'm_editor_view_state\.CommitTransition\s*\(', 'SourceToHTML\s*\(', 'ShowSource\s*\(')) {
    if($showView -notmatch $required) { throw "ShowView no longer coordinates: $required" }
}
Write-Host 'Editor view controller boundary contract passed.'
