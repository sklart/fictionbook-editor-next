<# Verifies timer-only debounce while forced recovery remains direct. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$header = Get-Content -Raw (Join-Path $root 'src\fbe\mainfrm.h')
$source = Get-Content -Raw (Join-Path $root 'src\fbe\mainfrm.cpp')
foreach($token in @('m_recovery_generation', 'm_recovery_saved_generation', 'm_recovery_last_edit_tick', 'MarkRecoveryDirty', 'TryAutoRecovery', 'RECOVERY_TYPING_DEBOUNCE_MS', 'recovery-skipped-clean', 'recovery-deferred-typing', 'recovery-written')) { if($header -notmatch [regex]::Escape($token) -and $source -notmatch [regex]::Escape($token)) { throw "Recovery debounce contract is missing: $token" } }
$save = [regex]::Match($source, 'bool CMainFrame::SaveRecoveryNow\(\)[\s\S]*?(?=void CMainFrame::MarkRecoveryDirty)').Value
if(-not $save -or $save -notmatch 'if \(!DocChanged\(\)\)' -or $save -notmatch 'SCI_GETTEXT' -or $save.IndexOf('if (!DocChanged())') -gt $save.IndexOf('SCI_GETTEXT')) { throw 'SaveRecoveryNow can still read source before its clean-state guard.' }
$automatic = [regex]::Match($source, 'bool CMainFrame::TryAutoRecovery\(\)[\s\S]*?(?=void CMainFrame::TryRestoreRecovery)').Value
if(-not $automatic -or $automatic -notmatch 'm_recovery_generation == m_recovery_saved_generation' -or $automatic -notmatch 'sinceEdit < RECOVERY_TYPING_DEBOUNCE_MS') { throw 'Automatic recovery lacks generation skip or typing debounce.' }
$timer = [regex]::Match($source, 'LRESULT CMainFrame::OnTimer[\s\S]*?(?=LRESULT CMainFrame::OnPostCreate)').Value
if($timer -notmatch 'TryAutoRecovery\(\)' -or $timer -match 'SaveRecoveryNow\(\)') { throw 'Recovery timer does not use the automatic path exclusively.' }
foreach($forced in @('OnQueryEndSession', 'OnEndSession')) { $body = [regex]::Match($source, "LRESULT CMainFrame::$forced[\s\S]*?(?=LRESULT|bool CMainFrame)").Value; if($body -notmatch 'SaveRecoveryNow\(\)') { throw "$forced no longer forces immediate recovery." } }
Write-Host 'Recovery debounce contract passed.'
