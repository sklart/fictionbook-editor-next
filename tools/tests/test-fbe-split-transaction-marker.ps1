[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [Parameter(Mandatory)][string]$EvidenceDirectory
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$evidence = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceDirectory)
if (Test-Path -LiteralPath $evidence) { throw "Evidence directory already exists: $evidence" }
New-Item -ItemType Directory -Path $evidence | Out-Null
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SPLIT_MARKER_KIND')
$saved = @{}; foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name) }
$failures = @()
try {
    foreach ($kind in @('native-only','no-dom','dom-add-first','dom-add-last','structure-add-first','structure-add-last')) {
        $iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'split-marker'; $passed = $false
        try {
            $null = New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory (Join-Path $iso.Root 'empty-scripts')
            $fixture = Join-Path $iso.Root 'marker.fb2'; $report = Join-Path $evidence ($kind + '.tsv')
            $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Test</nickname></author><book-title>Marker</book-title><lang>en</lang></title-info><document-info><id>marker</id><version>1</version></document-info></description><body><section id="probe-marker"><p>seed</p></section></body></FictionBook>'
            if ($kind -like 'structure-*') { $xml = $xml.Replace('<p>seed</p>', '<p>seed</p><p>tail</p>') }
            [IO.File]::WriteAllText($fixture, $xml, [Text.UTF8Encoding]::new($false))
            $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'split-transaction-marker'; $env:FBE_NEXT_TEST_SPLIT_MARKER_KIND = $kind
            $process = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -WindowStyle Hidden -ArgumentList @('--portable','-b',$report,$fixture) -PassThru
            $code = Wait-IsolatedFbeProcess -Process $process -Isolation $iso -Scenario "split-marker-$kind" -Report $report -TimeoutSeconds 120
            if ($code -ne 0 -or !(Test-Path -LiteralPath $report)) { throw "Marker probe failed to execute: $kind, exit=$code" }
            $result = @(Get-Content -LiteralPath $report | Where-Object { $_ -match '^result\t' })
            if ($result.Count -ne 1) { throw "Missing marker result: $report" }
            Write-Host "$kind $($result[0])"
            if ($result[0] -notmatch '^result\tPASS\t') { $failures += $kind }
            $passed = $true # A recorded FAIL is valid experimental evidence, not a passing candidate.
        } finally { Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed }
    }
    Get-FileHash -LiteralPath $FbeExe -Algorithm SHA256 | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence 'exe-sha256.json') -Encoding utf8
    if ($failures.Count) { throw "Transaction marker experiment failed: $($failures -join ', '); evidence: $evidence" }
} finally { foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $saved[$name]) } }
