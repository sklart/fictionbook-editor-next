<# Guards recovery storage, service and lifecycle boundaries. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$controllerHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\recovery\RecoveryController.h')
$controllerSource = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\recovery\RecoveryController.cpp')
$serviceHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\recovery\RecoveryService.h')
$storeHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\recovery\RecoveryStore.h')
$frame = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')

foreach($required in @('struct SnapshotRequest', 'class RecoveryController', 'CommitRestoredIdentity', 'GetRestoreCandidate')) {
    if($controllerHeader -notmatch $required) { throw "Recovery controller contract is missing: $required" }
}
foreach($forbidden in @('mainfrm\.h', 'MessageBox', 'SetTimer', 'KillTimer', 'SCI_', 'RecentDocuments', 'Plugin', 'Script')) {
    if($controllerSource -match $forbidden) { throw "Recovery controller crossed its lifecycle boundary: $forbidden" }
}
if($serviceHeader -match 'mainfrm\.h') { throw 'RecoveryService must not depend on CMainFrame.' }
foreach($forbidden in @('MessageBox', 'SetTimer', 'KillTimer', 'FB::Doc')) {
    if($storeHeader -match $forbidden) { throw "RecoveryStore must remain storage-only: $forbidden" }
}
$restore = [regex]::Match($frame, 'void CMainFrame::TryRestoreRecovery[\s\S]*?(?=LRESULT CMainFrame::OnSettingChange)').Value
if(-not $restore -or $restore -notmatch 'm_recovery\.CommitRestoredIdentity') { throw 'TryRestoreRecovery must delegate recovered identity to RecoveryController.' }
if($restore -match 'm_document_session\.RestoreArchive') { throw 'TryRestoreRecovery retained archive session identity mutation.' }
$save = [regex]::Match($frame, 'bool CMainFrame::SaveRecoveryNow[\s\S]*?(?=void CMainFrame::TryRestoreRecovery)').Value
if(-not $save -or $save -notmatch 'FbeRecovery::SnapshotRequest' -or $save -notmatch 'm_recovery\.Save') { throw 'SaveRecoveryNow must adapt editor state into SnapshotRequest.' }
Write-Host 'Recovery lifecycle boundary contract passed.'
