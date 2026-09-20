$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path $PSScriptRoot -Parent)
$header = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.h')
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
foreach($token in @('m_last_external_file_check', 'm_external_file_check_started', 'CheckFileTimeStampIfDue')) {
    if($header.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "Missing external file throttle state '$token'." }
}
foreach($token in @('DWORD>(now - m_last_external_file_check) < 1000', 'return CheckFileTimeStamp();')) {
    if($source.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "Missing external file throttle behavior '$token'." }
}
$idleStart = $source.IndexOf('BOOL CMainFrame::OnIdle()')
$idleEnd = $source.IndexOf('void CMainFrame::AddTbButton', $idleStart)
$idle = $source.Substring($idleStart, $idleEnd - $idleStart)
if($idle.IndexOf('CheckFileTimeStampIfDue()', [StringComparison]::Ordinal) -lt 0 -or $idle.IndexOf('CheckFileTimeStamp()', [StringComparison]::Ordinal) -ge 0) {
    throw 'OnIdle must use only the throttled external file check.'
}
Write-Host 'External file check throttle contract passed.'
