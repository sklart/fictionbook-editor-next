[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$FbeExe,
    [Parameter(Mandatory)][string]$EvidenceDirectory,
    [switch]$DirtyRange
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$evidence = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceDirectory)
if (Test-Path -LiteralPath $evidence) { throw "Evidence already exists: $evidence" }
New-Item -ItemType Directory -Path $evidence | Out-Null
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO')
$saved = @{}; foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name) }
$iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'history-observer'; $passed = $false
try {
    $null = New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory (Join-Path $iso.Root 'empty-scripts')
    $fixture = Join-Path $iso.Root 'observer.fb2'; $report = Join-Path $evidence 'observer.tsv'
    $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Test</nickname></author><book-title>Observer</book-title><lang>en</lang></title-info><document-info><id>observer</id><version>1</version></document-info></description><body><section id="oracle-section"><p>tail</p></section></body></FictionBook>'
    if ($DirtyRange) { $xml = $xml.Replace('<p>tail</p>', '<p id="probe-text">tail</p><p><a id="probe-link" l:href="#oracle-section">link</a></p>').Replace('xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"', 'xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"') }
    [IO.File]::WriteAllText($fixture, $xml, [Text.UTF8Encoding]::new($false))
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = if ($DirtyRange) { 'history-dirty-range-boundary' } else { 'history-observer-boundary' }
    $p = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -WindowStyle Hidden -ArgumentList @('--portable','-b',$report,$fixture) -PassThru
    $code = Wait-IsolatedFbeProcess -Process $p -Isolation $iso -Scenario 'history-observer-boundary' -Report $report -TimeoutSeconds 120
    if ($code -ne 0 -or !(Test-Path -LiteralPath $report)) { throw "Observer failed: exit=$code" }
    Get-FileHash -LiteralPath $FbeExe -Algorithm SHA256 | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence 'exe-sha256.json') -Encoding utf8
    Get-Content -LiteralPath $report
    $passed = $true
} finally {
    Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $saved[$name]) }
}
