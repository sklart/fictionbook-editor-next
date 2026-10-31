[CmdletBinding()]
param(
    [string]$FbeExe,
    [string]$EvidenceDirectory,
    [int[]]$ParagraphCounts = @(100, 3000, 12000),
    [string]$InputFb2,
    [switch]$AllowMissingStageMeasurements,
    [ValidateRange(1,600)][int]$TimeoutSeconds = 180
)
$ErrorActionPreference = 'Stop'
if (-not $FbeExe) { $FbeExe = Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe' }
if (-not $EvidenceDirectory) { $EvidenceDirectory = Join-Path $PSScriptRoot '..\..\build\settings-search-performance' }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$evidence = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($EvidenceDirectory)
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$exe = (Resolve-Path -LiteralPath $FbeExe).Path
$inputFixture = if ($InputFb2) { (Resolve-Path -LiteralPath $InputFb2).Path } else { $null }
if ($inputFixture) { $ParagraphCounts = @(1) }
$oldMode = $env:FBE_NEXT_TEST_MODE; $oldScenario = $env:FBE_NEXT_TEST_SCENARIO; $oldStage = $env:FBE_SETTINGS_PERF_STAGE_FILE
$summary = [Collections.Generic.List[object]]::new()
try {
    foreach ($count in $ParagraphCounts) {
        if ($count -lt 1) { throw 'Paragraph count must be positive.' }
        $isolation = New-IsolatedFbeRuntime -FbeExe $exe -Name settings-search-perf
        $passed = $false
        try {
            $null = New-IsolatedFbeSettingsSeed -Isolation $isolation -StartupScriptsDirectory (Join-Path $isolation.Root 'empty-scripts')
            $fixture = Join-Path $isolation.Root 'fixture.fb2'
            $xml = [Text.StringBuilder]::new()
            [void]$xml.Append('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><nickname>Fixture</nickname></author><book-title>Performance fixture</book-title><lang>en</lang></title-info><document-info><id>settings-search-perf</id><version>1.0</version></document-info></description><body><section>')
            for ($i = 0; $i -lt $count; ++$i) { [void]$xml.Append('<p>Repeated synthetic text for measuring editor settings and search. ').Append($i).Append(' No real book content is used.</p>') }
            [void]$xml.Append('</section></body></FictionBook>')
            if ($inputFixture) {
                Copy-Item -LiteralPath $inputFixture -Destination $fixture
            } else {
                [IO.File]::WriteAllText($fixture, $xml.ToString(), [Text.UTF8Encoding]::new($false))
            }
            $hash = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash
            $report = Join-Path $evidence "measurements-$count.tsv"
            $env:FBE_SETTINGS_PERF_STAGE_FILE = Join-Path $evidence "settings-stages-$count.tsv"
            [IO.File]::WriteAllText($env:FBE_SETTINGS_PERF_STAGE_FILE, "stage`telapsed_ms`r`n", [Text.Encoding]::ASCII)
            $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'community-settings-search-performance'
            $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('--portable','-b',$report,$fixture) -WindowStyle Hidden -PassThru
            $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario $env:FBE_NEXT_TEST_SCENARIO -Report $report -TimeoutSeconds $TimeoutSeconds
            if ($exitCode -ne 0) { throw "Runtime measurement failed, exit $exitCode; report $report" }
            $rows = @(Import-Csv -LiteralPath $report -Delimiter "`t")
            if ($rows.Count -ne 60 -or @($rows | Where-Object valid -ne '1').Count) { throw "Incomplete measurements: $report" }
            $stageCount = @(Import-Csv -LiteralPath $env:FBE_SETTINGS_PERF_STAGE_FILE -Delimiter "`t").Count
            if ($stageCount -ne 72 -and -not ($AllowMissingStageMeasurements -and $stageCount -eq 0)) { throw 'Incomplete settings stage measurements.' }
            foreach ($group in $rows | Group-Object operation,page) {
                $cold = @($group.Group | Where-Object repeat -eq '0')
                $warm = @($group.Group | Where-Object repeat -ne '0' | ForEach-Object { [double]::Parse($_.elapsed_ms,[Globalization.CultureInfo]::InvariantCulture) } | Sort-Object)
                if ($cold.Count -ne 1 -or $warm.Count -ne 5) { throw "Incomplete samples for $($group.Name)" }
                $summary.Add([pscustomobject]@{ Paragraphs=if ($inputFixture) { $null } else { $count }; FixtureSha256=$hash; Operation=$group.Group[0].operation; Page=$group.Group[0].page; ColdMs=$cold[0].elapsed_ms; WarmMedianMs=$warm[2]; WarmMaxMs=$warm[4] })
            }
            $passed = $true
            $fixtureLabel = if ($inputFixture) { 'supplied FB2 copy' } else { "$count paragraphs" }
            Write-Host "PASS: $fixtureLabel; report $report"
        } finally { Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed }
    }
    $summary | Export-Csv -LiteralPath (Join-Path $evidence 'summary.csv') -NoTypeInformation
    [ordered]@{ ExeSha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash; OS=[Environment]::OSVersion.VersionString; Processor=(Get-CimInstance Win32_Processor).Name; ProcessMode='isolated portable'; FirstSample='first invocation in process'; WarmSamples=5; FixtureType=if ($inputFixture) { 'supplied FB2 copy' } else { 'synthetic' }; Note='No cold OS cache guarantee.' } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence 'environment.json') -Encoding UTF8
} finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario; $env:FBE_SETTINGS_PERF_STAGE_FILE=$oldStage }
