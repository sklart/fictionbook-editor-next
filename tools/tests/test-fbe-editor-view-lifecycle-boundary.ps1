<# Guards ownership of editor view lifecycle state and transition decisions. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$viewState = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\view\EditorViewState.h')
$transitionHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\view\EditorViewTransition.h')
$transitionSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\view\EditorViewTransition.cpp')
$selectionState = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\view\EditorSelectionState.h')
$mainHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.h')
$mainSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')

foreach($unit in @($viewState, $transitionHeader, $transitionSource)) {
    foreach($forbidden in @('CMainFrame', 'FBDoc', 'FB::Doc', 'MSHTML', 'Scintilla', 'HWND')) {
        if($unit -match $forbidden) { throw "Pure editor view component must not depend on $forbidden." }
    }
}
foreach($required in @('enum class EditorView', 'Current\s*\(', 'Previous\s*\(', 'CommitTransition\s*\(', 'Reset\s*\(', 'CtrlTab')) {
    if($viewState -notmatch $required) { throw "EditorViewState is missing: $required" }
}
foreach($forbidden in @('CurrentRef', 'PreviousRef', 'LastCtrlTabViewRef', 'CtrlTabActiveRef')) {
    if($viewState -match $forbidden) { throw "EditorViewState retains mutable state backdoor: $forbidden" }
}
foreach($required in @('EditorViewTransitionPlan', 'commitSourceToDocument', 'prepareDocumentSource', 'saveCurrentSelection', 'restoreTargetSelection', 'enterDescriptionMode', 'leaveDescriptionMode')) {
    if(($transitionHeader + $transitionSource) -notmatch $required) { throw "Transition plan is missing: $required" }
}
foreach($required in @('IHTMLTxtRangePtr', 'BodySourceSelectionState', 'Reset\s*\(')) {
    if($selectionState -notmatch $required) { throw "EditorSelectionState is missing: $required" }
}
foreach($legacy in @('m_current_view', 'm_last_view', 'm_last_ctrl_tab_view', 'm_ctrl_tab', 'm_body_selection', 'm_desc_selection', 'm_body_source_selection')) {
    if($mainHeader -match $legacy) { throw "CMainFrame header retains lifecycle state: $legacy" }
}
foreach($forbidden in @('#define\s+m_current_view', '#define\s+m_last_view', '#define\s+m_last_ctrl_tab_view', '#define\s+m_ctrl_tab', '#define\s+m_body_selection', '#define\s+m_desc_selection', '#define\s+m_body_source_selection')) {
    if($mainSource -match $forbidden) { throw "CMainFrame retains lifecycle compatibility macro: $forbidden" }
}
foreach($required in @('MakeEditorViewTransitionPlan', 'CommitTransition', 'NextEditorView', 'NextCtrlTabEditorView', 'SetDescriptionMode')) {
    if($mainSource -notmatch $required) { throw "CMainFrame does not coordinate extracted lifecycle operation: $required" }
}
Write-Host 'Editor view lifecycle boundary contract passed.'
