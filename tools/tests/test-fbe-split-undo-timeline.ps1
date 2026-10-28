[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '../../out/Release/FBE.exe'),
    [Parameter(Mandatory)][string]$EvidenceDirectory
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$evidence = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceDirectory)
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$names = @('FBE_NEXT_TEST_MODE','FBE_NEXT_TEST_SCENARIO','FBE_NEXT_TEST_SPLIT_POSITION','FBE_NEXT_TEST_SPLIT_CONTAINER_ID','FBE_NEXT_TEST_STRUCTURE_TRACE','FBE_NEXT_TEST_STRUCTURE_CASE','FBE_NEXT_TEST_SPLIT_TIMELINE','FBE_NEXT_TEST_SPLIT_TIMELINE_INPUT')
$old = @{}; foreach ($name in $names) { $old[$name] = [Environment]::GetEnvironmentVariable($name) }
try {
    foreach ($mode in @('split','split-input')) {
        $iso = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name split-timeline; $passed = $false
        try {
            $null = New-IsolatedFbeSettingsSeed -Isolation $iso -StartupScriptsDirectory (Join-Path $iso.Root empty-scripts)
            $fixture = Join-Path $iso.Root 'timeline.fb2'; $report = Join-Path $evidence ($mode + '.tsv'); $trace = Join-Path $evidence ($mode + '-trace.tsv')
            $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><nickname>Fixture</nickname></author><book-title>Split timeline</book-title><lang>en</lang></title-info><document-info><author><nickname>Fixture</nickname></author><date value="2026-10-28">2026-10-28</date><id>split-timeline</id><version>1.0</version></document-info></description><body><section id="target-section-middle"><p>First <a l:href="#target-section-middle">Backlink</a></p><p>Middle</p><p>Last</p></section></body></FictionBook>'
            [IO.File]::WriteAllText($fixture, $xml, [Text.UTF8Encoding]::new($false))
            $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'split-container'; $env:FBE_NEXT_TEST_SPLIT_POSITION = 'middle'; $env:FBE_NEXT_TEST_SPLIT_CONTAINER_ID = 'target-section-middle'
            $env:FBE_NEXT_TEST_STRUCTURE_TRACE = $trace; $env:FBE_NEXT_TEST_STRUCTURE_CASE = $mode; $env:FBE_NEXT_TEST_SPLIT_TIMELINE = '1'
            $env:FBE_NEXT_TEST_SPLIT_TIMELINE_INPUT = if ($mode -eq 'split-input') { '1' } else { $null }
            $p = Start-Process -FilePath $iso.Exe -WorkingDirectory $iso.Runtime -WindowStyle Hidden -ArgumentList @('--portable','-b',$report,$fixture) -PassThru
            $code = Wait-IsolatedFbeProcess -Process $p -Isolation $iso -Scenario 'split-undo-timeline' -Report $report -TimeoutSeconds 90
            if ($code -ne 0 -or !(Test-Path -LiteralPath $trace)) { throw "Timeline probe failed: $mode, exit=$code" }
            $passed = $true
            Write-Host "Timeline observation: $mode; $(Get-Content -LiteralPath $report -Raw)"
        } finally { Complete-IsolatedFbeRuntime -Isolation $iso -Passed $passed }
    }
    Get-FileHash -LiteralPath $FbeExe | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence exe-sha256.json)
} finally { foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $old[$name]) } }
