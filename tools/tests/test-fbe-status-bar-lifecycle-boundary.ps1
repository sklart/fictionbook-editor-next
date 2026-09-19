<# Guards logical status-bar state ownership independently of WTL presentation. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$stateHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\StatusBarState.h')
$stateSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\StatusBarState.cpp')
$mainHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.h')
$mainSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')
$externalHelper = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\ExternalHelper.h')
$appUtils = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\apputils.h')

foreach($required in @('namespace\s+FBEStatusBar', 'class\s+State', 'QueueMessage', 'PromoteQueuedMessage', 'SetContext', 'SetTransient', 'ClearTransientIfExpired', 'SetValidation', 'EffectiveMainText', 'ResetForDocument')) {
    if(($stateHeader + $stateSource) -notmatch $required) { throw "Status state is missing: $required" }
}
foreach($forbidden in @('CMainFrame', 'HWND', 'CMultiPaneStatusBarCtrl', 'DocumentSaveController', 'RecoveryController', 'PluginExecutionController', 'SetPaneText', 'RuntimeLocalization')) {
    if(($stateHeader + $stateSource) -match $forbidden) { throw "Status state must not depend on $forbidden." }
}
foreach($legacy in @('m_status_msg', 'm_status_context', 'm_status_transient', 'm_status_transient_expiration', 'm_validation_status')) {
    if(($mainHeader + $mainSource) -match $legacy) { throw "CMainFrame retains logical status state: $legacy" }
}
foreach($required in @('m_status_state\.EffectiveMainText', 'FBEStatusBar::TogglePaneVisibility', 'FBEStatusBar::ApplyPaneVisibility', 'FBEStatusBar::ClickAction', 'FBEStatusBar::DoubleClickAction', 'FBEStatusBar::DecimalXmlReference')) {
    if($mainSource -notmatch $required) { throw "Status presentation no longer uses required owner/helper: $required" }
}
foreach($required in @('StatusTextImmediate', 'UpdateWindow\(m_status\)', 'm_status_state\.SetTransient')) {
    if(($appUtils + $mainHeader + $externalHelper) -notmatch $required) { throw "Immediate status-bar progress is missing: $required" }
}
if($externalHelper -notmatch 'WM_SETSTATUSTEXT, AU::StatusTextImmediate') { throw 'Скриптовый status update должен запрашивать немедленное отображение.' }
if($mainHeader -notmatch 'wParam == AU::StatusTextImmediate' -or $mainHeader -match 'PeekMessage|DispatchMessage|DoEvents') { throw 'Немедленный status update не должен прокачивать очередь или реентерить сериализацию.' }
Write-Host 'Status bar lifecycle boundary contract passed.'
