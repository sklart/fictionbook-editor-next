[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [ValidateRange(10, 180)][int]$TimeoutSeconds = 45
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE not found: $FbeExe" }
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-tooltip-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
$oldMode, $oldScenario, $oldSettings = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY
try {
    $fixture = Join-Path $root 'fixture.fb2'
    $report = Join-Path $root 'report.txt'
@'
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>Test</first-name><last-name>Author</last-name></author><book-title>Tooltip smoke</book-title><lang>en</lang></title-info><document-info><author><first-name>Test</first-name><last-name>Author</last-name></author><id>tooltip-smoke</id><version>1.0</version></document-info></description><body><section><p>Toolbar tooltip test.</p></section></body></FictionBook>
'@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $env:FBE_NEXT_TEST_MODE = '1'
    $env:FBE_NEXT_TEST_SCENARIO = 'script-toolbar-tooltips-runtime'
    $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = Join-Path $root 'profile'
    $process = Start-Process -FilePath $FbeExe -WorkingDirectory (Split-Path $FbeExe) -ArgumentList @('--installed', '-b', $report, $fixture) -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'Script toolbar tooltip runtime timed out.' }
    if ($process.ExitCode -ne 0) { $detail = if (Test-Path $report) { Get-Content $report -Raw } else { '<report missing>' }; throw "Script toolbar tooltip runtime failed: $detail" }
    $rows = @{}
    Get-Content -LiteralPath $report | ForEach-Object { $pair = $_ -split '=', 2; if ($pair.Count -eq 2) { $rows[$pair[0]] = $pair[1] } }
    foreach ($key in 'unicode', 'ansi', 'missing', 'no-path', 'long-name') {
        if ($rows[$key] -ne '1') { throw "Script toolbar tooltip regression: $key=$($rows[$key])" }
    }
    if ($rows['result'] -ne 'pass') { throw "Script toolbar tooltip result=$($rows['result'])" }
    Write-Host 'Script toolbar tooltip runtime passed.'
} finally {
    $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TEST_SETTINGS_DIRECTORY = $oldMode, $oldScenario, $oldSettings
    $resolved = [IO.Path]::GetFullPath($root)
    $temp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolved.StartsWith($temp, [StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $resolved -Recurse -Force -ErrorAction SilentlyContinue
    }
}
