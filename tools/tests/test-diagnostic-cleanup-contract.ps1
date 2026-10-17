$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$source=Get-Content -Raw (Join-Path $root 'src/fbe/DiagnosticPackage.cpp')
$dialog=Get-Content -Raw (Join-Path $root 'src/fbe/diagnostics/DiagnosticDialog.h')
foreach($token in 'CleanupDiagnosticSessions','session.id == SessionId()','session.crashAmbiguous','OpenProcess(SYNCHRONIZE','FILE_ATTRIBUTE_REPARSE_POINT','bytesDeleted','sessionsPartiallyDeleted','filesFailed') {
 if(!$source.Contains($token)){throw "Missing cleanup boundary: $token"}
}
if($source -match '(?i)DeleteFile\([^\r\n]*(?:\.fb2|Settings.xml|Recovery)'){throw 'Cleanup crosses diagnostic file scope'}
if($dialog -notmatch 'CleanupDiagnosticSessions\(0,true\)' -or $dialog -notmatch 'cleanupPending'){throw 'Cleanup must show an estimate before confirmation'}
Write-Host 'Diagnostic cleanup preservation and two-step UI contract passed.'
