<# Guards source benchmark and diagnostic command ownership outside mainfrm. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sourceHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\SourceViewDiagnostics.h')
$sourceImplementation = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\source\SourceViewDiagnostics.cpp')
$serviceHeader = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\diagnostics\DiagnosticCommandService.h')
$serviceImplementation = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\diagnostics\DiagnosticCommandService.cpp')
$main = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\mainfrm.cpp')

foreach($required in @('namespace\s+FbeSourceDiagnostics', 'SourceViewPhaseProfiler', 'ProfileSamples', 'ProcessMemorySnapshot', 'GetProcessMemorySnapshot')) {
    if(($sourceHeader + $sourceImplementation) -notmatch $required) { throw "Source diagnostics is missing: $required" }
}
foreach($required in @('class\s+DiagnosticCommandService', 'OpenCurrentLog', 'OpenLogFolder', 'CurrentLogPath', 'ClearOldLogSessions', 'CreatePackage', 'IsEnabledForNextLaunch', 'SetEnabledForNextLaunch')) {
    if(($serviceHeader + $serviceImplementation) -notmatch $required) { throw "Diagnostic command service is missing: $required" }
}
foreach($unit in @($sourceHeader, $sourceImplementation, $serviceHeader, $serviceImplementation)) {
    foreach($forbidden in @('mainfrm\.h', 'CMainFrame', 'RecoveryController', 'PluginExecutionController', 'FB::Doc')) {
        if($unit -match $forbidden) { throw "Diagnostic subsystem must not depend on $forbidden." }
    }
}
foreach($forbidden in @('struct\s+SourceProfileSample', 'class\s+ShowSourcePhaseProfiler', 'struct\s+ProcessMemorySnapshot', 'static\s+bool\s+OpenDiagnosticLog', 'static\s+bool\s+OpenDiagnosticLogFolder', 'CopyDiagnosticLogPathToClipboard')) {
    if($main -match $forbidden) { throw "mainfrm retains diagnostic ownership: $forbidden" }
}
foreach($required in @('m_diagnostic_commands\.OpenCurrentLog', 'm_diagnostic_commands\.OpenLogFolder', 'm_diagnostic_commands\.ClearOldLogSessions', 'm_diagnostic_commands\.CreatePackage')) {
    if($main -notmatch $required) { throw "Frame no longer delegates diagnostic operation: $required" }
}
Write-Host 'Diagnostic command boundary contract passed.'
