<# Exercises the non-COM XML scripting backend through the real editor. #>
[CmdletBinding()]
param(
    [string]$FbeExe,
    [int]$TimeoutSeconds = 90
)

$ErrorActionPreference = 'Stop'
if(-not $FbeExe){$FbeExe=Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'}
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
$isolation=New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'xml-backend'
$directory=$isolation.Root
$FbeExe=$isolation.Exe
$passed=$false
try {
    $null=New-IsolatedFbeSettingsSeed -Isolation $isolation
    $fixture = Join-Path $directory 'xml-script-backend.fb2'
    $report = Join-Path $directory 'xml-script-backend.txt'
    @'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0">
  <description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>XML API</book-title><lang>en</lang></title-info><document-info><id>xml-script-backend-test</id><version>1.0</version></document-info></description>
  <body><section><p id="xml-prior">XML_API_BEFORE</p></section></body><binary id="runtime.bin" content-type="application/octet-stream">AQID</binary>
</FictionBook>
'@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $savedMode, $savedScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'xml-script-backend-runtime'
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('--portable','-b', $report, $fixture) -WorkingDirectory (Split-Path $FbeExe) -WindowStyle Hidden -PassThru
        if(-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'FBE не завершил XML scripting backend scenario.' }
        if($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) {
            $diagnostics = if(Test-Path -LiteralPath $report) { Get-Content -LiteralPath $report -Raw } else { '<report missing>' }
            throw "FBE XML scripting backend scenario failed: exit $($process.ExitCode).`n$diagnostics"
        }
    } finally { $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO = $savedMode, $savedScenario }
    $result = @{}; foreach($line in Get-Content -LiteralPath $report) { $parts = $line -split '=', 2; if($parts.Count -eq 2) { $result[$parts[0]] = $parts[1] } }
    foreach($key in @('read', 'validate', 'invalid_rejected', 'no_op', 'apply', 'dirty', 'undo', 'undo_dirty_restored', 'redo', 'undo_second', 'redo_second', 'body', 'tree', 'history_chain', 'payload_lifetime', 'detached_history')) {
        if($result[$key] -ne '1') { throw "XML scripting backend runtime check failed: $key (value '$($result[$key])')." }
    }
    if($result['result'] -ne 'pass') { throw "XML scripting backend runtime result: $($result['result'])" }
    $exeHash=(Get-FileHash -LiteralPath $FbeExe -Algorithm SHA256).Hash
    $evidence=Join-Path ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../out/tests/binary-store/xml-undo'))) $exeHash.Substring(0,12)
    New-Item -ItemType Directory -Force $evidence | Out-Null
    Copy-Item -LiteralPath $report -Destination (Join-Path $evidence 'runtime-report.txt') -Force
    @{status='PASS';executableSha256=$exeHash;mainJsSha256=(Get-FileHash (Join-Path $directory 'main.js')).Hash;checks=$result} | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $evidence 'summary.json') -Encoding utf8
    Write-Host 'FBE XML scripting backend runtime passed.'
    $passed=$true
} finally { Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed }
