$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$rc=Get-Content -Raw (Join-Path $root 'src/fbe/FBE.rc')
$dialog=Get-Content -Raw (Join-Path $root 'src/fbe/diagnostics/DiagnosticDialog.h')
$template=[regex]::Match($rc,'(?ms)^IDD_DIAGNOSTICS DIALOGEX.*?^END').Value
if(!$template){throw 'Unified diagnostics dialog template missing'}
foreach($control in 'SESSIONS','COMPONENTS','PREVIEW_TEXT','STATUS','TOGGLE','OPEN_LOG','OPEN_FOLDER','CLEANUP','PREVIEW','SAVE','STATE','CLOSE','CANCEL','PROGRESS','PREVIOUS_PAGE','NEXT_PAGE','CRASH_LABEL','CRASH_REPORTS','REVIEW_CRASH','CLEAR_CRASHES','CLEAR_HANGS') {
 if(!$template.Contains('IDC_DIAGNOSTIC_'+$control)){throw "Dialog control missing: $control"}
}
if($template -notmatch 'ES_READONLY'){throw 'Preview must be read-only'}
foreach($token in 'commands.PreparePackage','commands.SavePackage','commands.Sessions()','WM_GETMINMAXINFO','0x02E0','CreateDialogFontForDpi','ThemeManager::ApplyToWindow','diagnostics-window.ini','LB_SETHORIZONTALEXTENT') {
 if(!$dialog.Contains($token)){throw "Diagnostic dialog integration missing: $token"}
}
if($dialog -match 'MessageBox'){throw 'Unified dialog must use inline state, not a chain of modal messages'}
$completion=[regex]::Match($dialog,'(?s)void CompleteJob\(\).*?void Prepare\(').Value
if(!$completion -or $completion -match 'StartupTrace::MarkDiagnosticCrash(?:Report)?Reviewed\('){throw 'UI completion must not perform crash review I/O'}
if($dialog -notmatch 'Begin\(Work::ReviewPreview,'){throw 'Preview acknowledgement must use the existing worker'}
Write-Host 'Diagnostic dialog controls, ownership, theme/DPI hooks and read-only preview contracts passed (not visual proof).'
