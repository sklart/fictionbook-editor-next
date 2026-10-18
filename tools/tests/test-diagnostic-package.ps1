$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$package=Get-Content -Raw (Join-Path $root 'src/fbe/DiagnosticPackage.cpp')
$header=Get-Content -Raw (Join-Path $root 'src/fbe/StartupTrace.h')
foreach($token in 'PrepareDiagnosticPackage','SaveDiagnosticPackage','ZipStoreWriter','ContainsUnsafeContent','EnvironmentSchema','TraceSchema','FindCrashTextForSession','sessionWindow = 30ULL * 60ULL','manifest.json','package-manifest.txt','environment-report.txt','fbelib-report.txt','diagnostic-modules.txt') {
 if(!$package.Contains($token)){throw "Missing package integration: $token"}
}
if($header -notmatch 'private:\s*std::vector<DiagnosticComponent> components;'){throw 'Snapshot contents must be private'}
if($header -notmatch 'shared_ptr<const DiagnosticPackageSnapshot>'){throw 'Prepared snapshot must be immutable'}
$save=[regex]::Match($package,'(?s)bool StartupTrace::SaveDiagnosticPackage.*?(?=bool StartupTrace::CreateDiagnosticPackage)').Value
if($save -match 'ReadBytes|ReadText|EnumerateDiagnosticSessions'){throw 'Save must not reread source files'}
if($save -notmatch 'snapshot.Components\(\)' -or $save -notmatch 'ContainsUnsafeContent\(item.bytes\)'){throw 'Save must verify and write snapshot bytes'}
if($package -match 'zip.Add\([^\r\n]*\.dmp'){throw 'Memory dumps must remain excluded'}
Write-Host 'Diagnostic immutable snapshot / ZIP contract passed.'
