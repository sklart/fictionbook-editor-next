<# Guards the presentation adapter boundary for editor view changes. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$hostHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\view\ui\EditorViewPresentationHost.h')
$hostSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\view\ui\EditorViewPresentationHost.cpp')
$controllerHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\view\EditorViewController.h')
$mainHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.h')
$mainSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')

foreach($required in @('struct\s+EditorViewPresentationContext', 'class\s+EditorViewPresentationHost', 'IEditorViewPresentationHost', 'CContainerWnd', 'CSplitterWindow', 'SourceEditorControl', 'EditorSelectionState')) {
    if(($hostHeader + $hostSource) -notmatch $required) { throw "Presentation host is missing: $required" }
}
foreach($required in @('ActivateWnd\s*\(', 'HideActiveWnd\s*\(', 'SetSinglePaneMode\s*\(', 'SaveBodyScroll', 'SCI_ENSUREVISIBLEENFORCEPOLICY')) {
    if($hostSource -notmatch $required) { throw "Presentation mechanics are missing: $required" }
}
foreach($unit in @($hostHeader, $hostSource)) {
    foreach($forbidden in @('mainfrm\.h', 'CMainFrame', 'DocumentSession', 'DocumentSave', 'Recovery', 'RecentDocuments', 'PluginExecution', 'PluginUi')) {
        if($unit -match $forbidden) { throw "Presentation host must not own $forbidden." }
    }
}
foreach($forbidden in @('IEditorViewHost', 'PrepareEditorViewPresentation', 'CompleteEditorViewPresentation', 'SaveEditorViewSelection', 'RestoreEditorViewSelection')) {
    if(($mainHeader + $mainSource) -match $forbidden) { throw "Main frame must not retain presentation method: $forbidden" }
}
$showView = [regex]::Match($mainSource, '(?s)void\s+CMainFrame::ShowView\(.*?(?=void\s+CMainFrame::ApplyEditorViewCommandUi)').Value
if([string]::IsNullOrEmpty($showView)) { throw 'Unable to locate CMainFrame::ShowView.' }
foreach($required in @('EditorViewPresentationHost', 'm_editor_view_controller\.ChangeView\s*\(')) {
    if($showView -notmatch $required) { throw "ShowView presentation adapter is missing: $required" }
}
foreach($forbidden in @('ActivateWnd\s*\(', 'HideActiveWnd\s*\(', 'SetSinglePaneMode\s*\(', 'SCI_')) {
    if($showView -match $forbidden) { throw "ShowView must not retain presentation mechanics: $forbidden" }
}
if($controllerHeader -notmatch 'IEditorSourceExchange&\s+source\s*,\s*IEditorViewPresentationHost&\s+presentation') {
    throw 'Controller must accept separate source and presentation ports.'
}
Write-Host 'Editor view presentation boundary contract passed.'
