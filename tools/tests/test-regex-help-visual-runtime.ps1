[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot "..\..\out\Release\FBE.exe"), [string]$ArtifactDirectory, [int]$TimeoutSeconds = 120)
$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe)) { throw "FBE не найден: $FbeExe" }
if (-not $ArtifactDirectory) { throw 'Укажите -ArtifactDirectory для visual smoke Full Help.' }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$ArtifactDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ArtifactDirectory)
New-Item -ItemType Directory -Path $ArtifactDirectory -Force | Out-Null
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-regex-help-visual-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
$captureDirectory = Join-Path $root 'screens'
New-Item -ItemType Directory -Path $captureDirectory | Out-Null
$isolation = $null; $passed = $false
try {
    $isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'RegexHelpVisualData'
    $fixture = Join-Path $root 'help.fb2'; $report = Join-Path $root 'report.txt'
    '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Help</book-title><lang>en</lang></title-info><document-info><id>regex-help-visual</id><version>1.0</version></document-info></description><body><section><p>Help visual smoke.</p></section></body></FictionBook>' | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode,$oldScenario,$oldArtifacts=$env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_ARTIFACT_DIR
    try {
        $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='regex-help-visual-runtime'; $env:FBE_NEXT_TEST_ARTIFACT_DIR=$captureDirectory
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList '--portable','-b',("`"$report`""),("`"$fixture`"") -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'regex-help-visual-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
        if ($exitCode -ne 0) { throw "Regex Help visual smoke failed with exit code $exitCode; report=$(if(Test-Path -LiteralPath $report){Get-Content -LiteralPath $report -Raw}else{'missing'})." }
    } finally { $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO,$env:FBE_NEXT_TEST_ARTIFACT_DIR=$oldMode,$oldScenario,$oldArtifacts }
    $rows=@{}; Get-Content -LiteralPath $report | ForEach-Object { $pair=$_ -split '=',2; if($pair.Count -eq 2){$rows[$pair[0]]=$pair[1]} }
    foreach($key in 'initial','design_table','character_classes','design_code','quantifiers','warning','narrow','restored','regex_example','note','code_table','distinct_design','distinct_code','metrics') { if($rows[$key] -ne '1') { throw "Regex Help visual smoke failed: $key=$($rows[$key])" } }
    foreach($context in 'design','code') {
        foreach($key in 'markdown_read_ms','parse_ms','render_ms','dialog_first_visible_ms','total_ready_ms','block_count') { if($null -eq $rows["${context}_${key}"]) { throw "Regex Help visual metrics missing: ${context}_${key}" } }
        if([UInt64]$rows["${context}_block_count"] -le 0 -or [UInt64]$rows["${context}_dialog_first_visible_ms"] -gt [UInt64]$rows["${context}_total_ready_ms"]) { throw "Regex Help deferred-render ordering failed for $context." }
    }
    foreach($name in 'full-help-design-start.bmp','full-help-design-table.bmp','full-help-design-character-classes.bmp','full-help-design-code.bmp','full-help-design-quantifiers.bmp','full-help-design-warning.bmp','full-help-design-narrow.bmp','full-help-code-start.bmp','full-help-code-regex-example.bmp','full-help-code-note.bmp','full-help-code-table.bmp') {
        $source = Join-Path $captureDirectory $name
        if(-not (Test-Path -LiteralPath $source)) { throw "Full Help screenshot missing: $name" }
        Copy-Item -LiteralPath $source -Destination (Join-Path $ArtifactDirectory $name) -Force
    }
    foreach($group in @(
        @('full-help-design-table.bmp','full-help-design-character-classes.bmp','full-help-design-code.bmp','full-help-design-quantifiers.bmp','full-help-design-warning.bmp'),
        @('full-help-code-regex-example.bmp','full-help-code-note.bmp','full-help-code-table.bmp')
    )) {
        $hashes = @($group | ForEach-Object { (Get-FileHash -LiteralPath (Join-Path $captureDirectory $_) -Algorithm SHA256).Hash })
        if (($hashes | Select-Object -Unique).Count -ne $group.Count) { throw "Full Help marker screenshots are not distinct: $($group -join ', ')" }
    }
    Copy-Item -LiteralPath $report -Destination (Join-Path $ArtifactDirectory 'full-help-visual-report.txt') -Force
    Write-Host 'Regex Help visual placement and rendering smoke passed.'
    $passed = $true
} finally { if ($isolation) { Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed }; if ($passed) { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue } else { Write-Host "Visual smoke failure artifacts: $root" } }
