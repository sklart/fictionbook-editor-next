$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path $PSScriptRoot)
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\DiagnosticPackage.cpp')
$header = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\StartupTrace.h')
$mainFrame = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
foreach($required in @('CreateDiagnosticPackage', 'ZipStoreWriter', 'ContainsUnsafeContent', 'Privacy scan rejected', 'FBE-Diagnostics-%04u%02u%02u-%02u%02u%02u.zip', 'package-manifest.txt', 'environment-report.txt', 'fbelib-report.txt', 'diagnostic-modules.txt', 'FindLatestCrashText', 'crash/latest-crash-report.txt', 'Crash dumps, books, settings, recovery files, scripts, images and clipboard data are excluded', 'FBE.exe', 'Scintilla.dll', 'Lexilla.dll')) {
    if($source.IndexOf($required, [StringComparison]::Ordinal) -lt 0) { throw "Missing diagnostic package contract: $required" }
}
foreach($forbidden in @('*.fb2', 'Settings.xml', '.dmp')) {
    if($source.IndexOf('Add("' + $forbidden, [StringComparison]::Ordinal) -ge 0) { throw "Forbidden package input added: $forbidden" }
}
if($header.IndexOf('bool CreateDiagnosticPackage(CString& packagePath, CString& error);', [StringComparison]::Ordinal) -lt 0) { throw 'Diagnostic package API is not exposed.' }
if($mainFrame.IndexOf('OnToolsCreateDiagnosticPackage', [StringComparison]::Ordinal) -lt 0) { throw 'Diagnostic package command handler is missing.' }
Write-Host 'Diagnostic package contract passed.'
