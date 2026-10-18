$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$trace=Get-Content -Raw (Join-Path $root 'src/fbe/StartupTrace.cpp')
$package=Get-Content -Raw (Join-Path $root 'src/fbe/DiagnosticPackage.cpp')
if($trace -match 'FindLatestTrace'){throw 'Implicit latest-session fallback is forbidden'}
foreach($token in 'SessionPartNumber','std::sort(selected.begin()','left.part < right.part','SessionPartNumber(find.cFileName, sessionStem, segment)','ValidSessionId','LegacyTraceName') {
 if(!$package.Contains($token)){throw "Missing exact session / segment boundary: $token"}
}
if($trace -notmatch 'session_id=%s'){throw 'Every rotated record must carry the process session ID'}
Write-Host 'Diagnostic session and numeric segment boundary contract passed.'
