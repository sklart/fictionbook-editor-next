param(
    [string]$InputDir = 'D:\epub_big_suite',
    [string]$OutputDir = 'D:\epub_big_suite_fb2',
    [string]$PackageDir = 'D:\epub_big_test_packages',
    [string]$ImportOptions = '--recursive --preserve-tree --stats --log --svg png --flush-report-each --quiet --isolate-crashes',
    [int]$ResumeMode = 1,
    [int]$MaxFiles = 0,
    [int]$BuildBeforeRun = 0,
    [int]$BuildLunaSvgBeforeRun = 0,
    [int]$CheckDependenciesBeforeRun = 1,
    [int]$PackageProblems = 1,
    [int]$MaxProblemRows = 250
)

$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'

function New-Dir([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path | Out-Null
    }
}

function Get-FirstExisting([string[]]$Paths) {
    foreach ($p in $Paths) {
        if (Test-Path -LiteralPath $p) { return $p }
    }
    return $null
}

function Split-CommandLineSimple([string]$Text) {
    if ([string]::IsNullOrWhiteSpace($Text)) { return @() }
    return @($Text -split '\s+' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
}

function Copy-KeepRelative([string]$File, [string]$Base, [string]$DestinationRoot) {
    if ([string]::IsNullOrWhiteSpace($File) -or -not (Test-Path -LiteralPath $File)) { return }
    $full = [IO.Path]::GetFullPath($File)
    $baseFull = [IO.Path]::GetFullPath($Base).TrimEnd('\','/') + '\'
    if ($full.StartsWith($baseFull, [StringComparison]::OrdinalIgnoreCase)) {
        $rel = $full.Substring($baseFull.Length)
    } else {
        $rel = Split-Path -Leaf $full
    }
    $dst = Join-Path $DestinationRoot $rel
    New-Dir (Split-Path -Parent $dst)
    Copy-Item -LiteralPath $full -Destination $dst -Force
}

if (-not (Test-Path -LiteralPath $InputDir)) {
    throw "InputDir does not exist: $InputDir"
}
New-Dir $OutputDir
New-Dir $PackageDir

$batchExe = Get-FirstExisting @(
    (Join-Path $scriptDir 'out\Release\ImportEPUBBatch.exe'),
    (Join-Path $scriptDir 'out\ImportEPUBBatch.exe'),
    (Join-Path $scriptDir 'ImportEPUBBatch.exe')
)

if ($BuildBeforeRun -ne 0) {
    & (Join-Path $scriptDir 'BUILD_BATCH_WIN32_RELEASE.cmd')
    if ($LASTEXITCODE -ne 0) { throw 'BUILD_BATCH_WIN32_RELEASE.cmd failed.' }
    $batchExe = Join-Path $scriptDir 'out\Release\ImportEPUBBatch.exe'
}

if ($BuildLunaSvgBeforeRun -ne 0) {
    & (Join-Path $scriptDir 'BUILD_LUNASVG_ALL_WIN32_RELEASE.cmd')
    if ($LASTEXITCODE -ne 0) { throw 'BUILD_LUNASVG_ALL_WIN32_RELEASE.cmd failed.' }
}

if (-not (Test-Path -LiteralPath $batchExe)) {
    throw "ImportEPUBBatch.exe not found. Build the project first."
}

if ($CheckDependenciesBeforeRun -ne 0) {
    $checkCmd = Join-Path $scriptDir 'CHECK_DEPENDENCIES.cmd'
    if (Test-Path -LiteralPath $checkCmd) {
        & $checkCmd | Out-Host
    }
}

$reportCsv = Join-Path $OutputDir 'ImportEPUBBatch_report.csv'
$reportHtml = [IO.Path]::ChangeExtension($reportCsv, '.html')
$consoleLog = Join-Path $OutputDir ("ImportEPUB_big_test_console_$stamp.log")
$summaryTxt = Join-Path $OutputDir ("ImportEPUB_big_test_summary_$stamp.txt")

$files = Get-ChildItem -LiteralPath $InputDir -Filter *.epub -Recurse -File
if ($MaxFiles -gt 0) { $files = $files | Select-Object -First $MaxFiles }
$totalBytes = ($files | Measure-Object -Property Length -Sum).Sum
$totalGb = [math]::Round(($totalBytes / 1GB), 3)

$argsList = @('--batch', $InputDir, $OutputDir)
$argsList += Split-CommandLineSimple $ImportOptions
if ($ResumeMode -ne 0) {
    if ($argsList -notcontains '--skip-existing') { $argsList += '--skip-existing' }
} else {
    if ($argsList -notcontains '--overwrite') { $argsList += '--overwrite' }
}
if ($MaxFiles -gt 0) { $argsList += @('--max-files', [string]$MaxFiles) }
$argsList += @('--report', $reportCsv)

$header = @()
$header += 'ImportEPUB large volume test'
$header += ('Started: ' + (Get-Date))
$header += ('InputDir: ' + $InputDir)
$header += ('OutputDir: ' + $OutputDir)
$header += ('EPUB files scheduled: ' + $files.Count)
$header += ('EPUB size, GB: ' + $totalGb)
$header += ('ResumeMode: ' + $ResumeMode)
$header += ('MaxFiles: ' + $MaxFiles)
$header += ('BatchExe: ' + $batchExe)
$header += ('Arguments: ' + ($argsList -join ' '))
$header += ''
$header | Tee-Object -FilePath $consoleLog -Encoding UTF8

$sw = [Diagnostics.Stopwatch]::StartNew()
& $batchExe @argsList 2>&1 | Tee-Object -FilePath $consoleLog -Append -Encoding UTF8
$exitCode = $LASTEXITCODE
$sw.Stop()

$rows = @()
if (Test-Path -LiteralPath $reportCsv) {
    $rows = Import-Csv -LiteralPath $reportCsv -Delimiter ';'
}

$ok = @($rows | Where-Object { $_.Status -eq 'OK' }).Count
$warn = @($rows | Where-Object { $_.Status -like '*WARNING*' }).Count
$failed = @($rows | Where-Object { $_.Status -eq 'FAILED' }).Count
$skipped = @($rows | Where-Object { $_.Status -eq 'SKIPPED' }).Count
$processed = $rows.Count

$summary = @()
$summary += 'ImportEPUB large volume test summary'
$summary += ('Finished: ' + (Get-Date))
$summary += ('Elapsed: ' + $sw.Elapsed)
$summary += ('ExitCode: ' + $exitCode)
$summary += ('InputDir: ' + $InputDir)
$summary += ('OutputDir: ' + $OutputDir)
$summary += ('EPUB files scheduled: ' + $files.Count)
$summary += ('EPUB size, GB: ' + $totalGb)
$summary += ('Rows in report: ' + $processed)
$summary += ('OK: ' + $ok)
$summary += ('OK_WITH_WARNINGS: ' + $warn)
$summary += ('FAILED: ' + $failed)
$summary += ('SKIPPED: ' + $skipped)
$summary += ('Report CSV: ' + $reportCsv)
$summary += ('Report HTML: ' + $reportHtml)
$summary += ('Console log: ' + $consoleLog)
$summary | Set-Content -LiteralPath $summaryTxt -Encoding UTF8
$summary | Out-Host

$packageWork = Join-Path $PackageDir "ImportEPUB_big_test_$stamp"
$packageZip = $packageWork + '.zip'
if (Test-Path -LiteralPath $packageWork) { Remove-Item -LiteralPath $packageWork -Recurse -Force }
New-Dir $packageWork
New-Dir (Join-Path $packageWork 'report')
New-Dir (Join-Path $packageWork 'binaries')
New-Dir (Join-Path $packageWork 'problem_source_epub')
New-Dir (Join-Path $packageWork 'problem_converted_fb2')
New-Dir (Join-Path $packageWork 'problem_logs')

foreach ($p in @($reportCsv, $reportHtml, $consoleLog, $summaryTxt, (Join-Path $scriptDir 'CHECK_DEPENDENCIES_REPORT.txt'))) {
    if (Test-Path -LiteralPath $p) { Copy-Item -LiteralPath $p -Destination (Join-Path $packageWork 'report') -Force }
}
foreach ($name in @('ImportEPUB.dll','ImportEPUBBatch.exe','ImportEPUBLunaSVG.dll')) {
    $p = Get-FirstExisting @((Join-Path $scriptDir "out\Release\$name"), (Join-Path $scriptDir "out\$name"), (Join-Path $scriptDir $name))
    if ($p) { Copy-Item -LiteralPath $p -Destination (Join-Path $packageWork 'binaries') -Force }
}

$selected = @()
if ($PackageProblems -ne 0 -and $rows.Count -gt 0) {
    $selected = @($rows | Where-Object { $_.Status -eq 'FAILED' -or $_.Status -like '*WARNING*' } | Select-Object -First $MaxProblemRows)
    foreach ($r in $selected) {
        Copy-KeepRelative $r.Source $InputDir (Join-Path $packageWork 'problem_source_epub')
        Copy-KeepRelative $r.Output $OutputDir (Join-Path $packageWork 'problem_converted_fb2')
        foreach ($f in @($r.Source, $r.Output)) {
            if (-not [string]::IsNullOrWhiteSpace($f)) {
                $dir = Split-Path -Parent $f
                $stem = [IO.Path]::GetFileNameWithoutExtension($f)
                foreach ($log in @((Join-Path $dir ($stem + '.ImportEPUB.log')), (Join-Path $dir ($stem + '.failed-import.log')))) {
                    if (Test-Path -LiteralPath $log) { Copy-KeepRelative $log $dir (Join-Path $packageWork 'problem_logs') }
                }
            }
        }
    }
}

$readme = @()
$readme += 'ImportEPUB big-test package'
$readme += ('Generated: ' + (Get-Date))
$readme += ('InputDir: ' + $InputDir)
$readme += ('OutputDir: ' + $OutputDir)
$readme += ('Selected problem rows copied: ' + $selected.Count)
$readme += ('MaxProblemRows: ' + $MaxProblemRows)
$readme += 'This package intentionally does not include all 4+ GB of source EPUB files.'
$readme += 'Send this ZIP first. If needed, send separate selected EPUB/FB2 pairs from problem_* folders.'
$readme | Set-Content -LiteralPath (Join-Path $packageWork 'README_BIG_TEST_PACKAGE.txt') -Encoding UTF8

if (Test-Path -LiteralPath $packageZip) { Remove-Item -LiteralPath $packageZip -Force }
Compress-Archive -Path (Join-Path $packageWork '*') -DestinationPath $packageZip -Force
Write-Host "Big-test package: $packageZip"

exit $exitCode
