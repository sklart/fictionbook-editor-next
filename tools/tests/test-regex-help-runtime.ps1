[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot "..\..\out\Release\FBE.exe"), [int]$TimeoutSeconds = 60)
$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe)) { throw "FBE не найден: $FbeExe" }
$root = Join-Path ([IO.Path]::GetTempPath()) ('fbe-regex-help-runtime-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
try {
    $fixture = Join-Path $root 'help.fb2'; $report = Join-Path $root 'report.txt'
    '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Help</book-title><lang>en</lang></title-info><document-info><id>regex-help-runtime</id><version>1.0</version></document-info></description><body><section><p>Help runtime smoke.</p></section></body></FictionBook>' | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode,$oldScenario = $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO
    $exitCode = $null
    try {
        $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='regex-help-runtime'
        $process = Start-Process -FilePath $FbeExe -ArgumentList '--portable','-b',("`"$report`""),("`"$fixture`"") -PassThru
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw 'Regex Help runtime smoke timed out.' }
        $exitCode = $process.ExitCode
    } finally { $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO=$oldMode,$oldScenario }
    $rows=@{}; Get-Content -LiteralPath $report | ForEach-Object { $pair=$_ -split '=',2; if($pair.Count -eq 2){$rows[$pair[0]]=$pair[1]} }
    foreach($key in 'design','source','ru_design','ru_source','fallback','content','cache','parser','format','long'){if($rows[$key] -ne '1'){throw "Regex Help runtime smoke failed: $key=$($rows[$key]); report=$($rows | Out-String)"}}
    foreach($key in 'en_design_length','en_source_length','ru_design_length','ru_source_length'){if([int]$rows[$key] -le 32767){throw "Regex Help runtime smoke did not render full document: $key=$($rows[$key])"}}
    foreach($locale in 'en_design','en_source','ru_design','ru_source') {
        $actual = [int]$rows["${locale}_length"]
        $expected = [int]$rows["${locale}_expected_length"]
        $terminalNewlineOmitted = $rows["${locale}_terminal_newline_omitted"] -eq '1'
        if($actual -ne $expected -and -not ($terminalNewlineOmitted -and $actual + 1 -eq $expected)) {
            throw "Regex Help render length contract failed: $locale actual=$actual expected=$expected terminal_newline_omitted=$terminalNewlineOmitted"
        }
    }
    if($exitCode -ne 0){throw "Regex Help runtime smoke failed with exit code $exitCode; report=$($rows | Out-String)"}
    if($rows['result'] -ne 'pass'){throw "Regex Help runtime smoke result=$($rows['result']); report=$($rows | Out-String)"}
    Write-Host "Rendered lengths: en design=$($rows['en_design_length']), en source=$($rows['en_source_length']), ru design=$($rows['ru_design_length']), ru source=$($rows['ru_source_length'])"

    # ExecutableDirectory() is the only help-root input. Run a copied runtime
    # without Help so both the requested locale and en-US are genuinely absent.
    $missingRoot = Join-Path $root 'without-help'; $missingApp = Join-Path $missingRoot 'app'; New-Item -ItemType Directory -Path $missingApp -Force | Out-Null
    Get-ChildItem -LiteralPath (Split-Path -Parent $FbeExe) -Force | Where-Object { $_.Name -ne 'Help' } | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $missingApp -Recurse -Force }
    $missingReport = Join-Path $missingRoot 'missing-report.txt'; $missingFixture = Join-Path $missingRoot 'missing.fb2'
    Copy-Item -LiteralPath $fixture -Destination $missingFixture -Force
    try {
        $env:FBE_NEXT_TEST_MODE='1'; $env:FBE_NEXT_TEST_SCENARIO='regex-help-missing-files-runtime'
        $missingProcess = Start-Process -FilePath (Join-Path $missingApp 'FBE.exe') -ArgumentList '--portable','-b',("`"$missingReport`""),("`"$missingFixture`"") -PassThru
        if (-not $missingProcess.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $missingProcess.Id -Force; throw 'Regex Help missing-files runtime smoke timed out.' }
        if ($missingProcess.ExitCode -ne 0) { throw "Regex Help missing-files runtime smoke failed with exit code $($missingProcess.ExitCode)." }
    } finally { $env:FBE_NEXT_TEST_MODE,$env:FBE_NEXT_TEST_SCENARIO=$oldMode,$oldScenario }
    $missingRows=@{}; Get-Content -LiteralPath $missingReport | ForEach-Object { $pair=$_ -split '=',2; if($pair.Count -eq 2){$missingRows[$pair[0]]=$pair[1]} }
    foreach($key in 'missing_files','fallback_content','fallback_render') { if($missingRows[$key] -ne '1') { throw "Regex Help missing-files runtime smoke failed: $key=$($missingRows[$key]); report=$($missingRows | Out-String)" } }
    if($missingRows['result'] -ne 'pass') { throw "Regex Help missing-files runtime result=$($missingRows['result']); report=$($missingRows | Out-String)" }
    Write-Host 'Regex Help native runtime smoke passed.'
} finally { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
