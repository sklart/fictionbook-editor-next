<#
Measures the test-only idle harness of a Release FBE executable.  Invoke it
twice with the same fixture parameters: once against the preserved baseline
(`idle-performance-baseline`) and once against the current editor
(`idle-performance`).  The resulting TSV files are deliberately comparable.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$FbeExe,
    [ValidateSet('idle-performance', 'idle-performance-baseline')]
    [string]$Scenario = 'idle-performance',
    [Parameter(Mandatory = $true)]
    [string]$ResultPath,
    [int]$SmallParagraphCount = 1000,
    [int]$MediumParagraphCount = 10000,
    [int]$MediumParagraphTextLength = 220,
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
$ResultPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ResultPath)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "FBE not found: $FbeExe" }
if ($SmallParagraphCount -lt 1 -or $MediumParagraphCount -lt 1 -or $MediumParagraphTextLength -lt 1) {
    throw 'Paragraph counts and text length must be positive.'
}

function New-Fixture([string]$Path, [int]$ParagraphCount, [int]$TextLength) {
    $body = [Text.StringBuilder]::new()
    $text = 'x' * $TextLength
    for ($index = 1; $index -le $ParagraphCount; ++$index) {
        [void]$body.Append('<p>')
        [void]$body.Append($text)
        [void]$body.Append($index)
        [void]$body.Append('</p>')
    }
    $fixture = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>idle-performance</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>idle-performance</id><version>1.0</version></document-info></description><body><section>' + $body + '</section></body></FictionBook>'
    [IO.File]::WriteAllText($Path, $fixture, [Text.UTF8Encoding]::new($false))
}

function Read-Report([string]$Path) {
    $result = @{}
    Get-Content -LiteralPath $Path | ForEach-Object {
        $parts = $_ -split "`t", 2
        if ($parts.Count -eq 2) { $result[$parts[0]] = $parts[1] }
    }
    return $result
}

$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-idle-measure-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory | Out-Null
$oldMode, $oldScenario, $oldTrace, $oldIdleView = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TRACE, $env:FBE_NEXT_TEST_IDLE_VIEW
try {
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = $Scenario; $env:FBE_NEXT_TRACE = '1'
    $rows = foreach ($case in @(
            @{ Name = 'empty-body'; Paragraphs = 0; TextLength = 1; View = 'body' },
            @{ Name = 'small-body'; Paragraphs = $SmallParagraphCount; TextLength = $MediumParagraphTextLength; View = 'body' },
            @{ Name = 'medium-body'; Paragraphs = $MediumParagraphCount; TextLength = $MediumParagraphTextLength; View = 'body' },
            @{ Name = 'medium-source'; Paragraphs = $MediumParagraphCount; TextLength = $MediumParagraphTextLength; View = 'source' })) {
        $fixture = Join-Path $directory ($case.Name + '.fb2')
        $reportPath = Join-Path $directory ($case.Name + '.tsv')
        New-Fixture $fixture $case.Paragraphs $case.TextLength
        $env:FBE_NEXT_TEST_IDLE_VIEW = $case.View
        $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $reportPath, $fixture) -PassThru
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "$($case.Name): FBE did not finish." }
        if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $reportPath)) { throw "$($case.Name): FBE scenario failed: exit $($process.ExitCode)." }
        $report = Read-Report $reportPath
        if ($report['idle_cycles'] -ne '1000') {
            throw "$($case.Name): invalid idle report (cycles='$($report['idle_cycles'])')."
        }
        # The pre-Phase-A harness wrote an empty legacy `view` field because
        # CStringA and %s disagree under the ANSI build.  Its requested view
        # is nevertheless authoritative; current harnesses must report it.
        if ($report['view'] -and $report['view'] -ne $case.View) {
            throw "$($case.Name): unexpected view '$($report['view'])'."
        }
        [pscustomobject]@{
            scenario = $Scenario; case = $case.Name; fixture_bytes = (Get-Item -LiteralPath $fixture).Length
            view = $(if ($report['view']) { $report['view'] } else { $case.View }); idle_cycles = $report['idle_cycles']; elapsed_ms = $report['elapsed_ms']
            command_state_updates = $report['command_state_updates']; selection_context_builds = $report['selection_context_builds']
            toolbar_updates = $report['toolbar_updates']; file_fingerprint_checks = $report['file_fingerprint_checks']
            clipboard_checks = $report['clipboard_checks']; check_command_calls = $report['check_command_calls']
            js_com_calls = $report['js_com_calls']
        }
    }
    $parent = Split-Path -Parent $ResultPath
    if ($parent) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
    $rows | Export-Csv -LiteralPath $ResultPath -Delimiter "`t" -NoTypeInformation -Encoding utf8
    $rows | Format-Table -AutoSize | Out-Host
} finally {
    $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TRACE = $oldMode, $oldScenario, $oldTrace
    $env:FBE_NEXT_TEST_IDLE_VIEW = $oldIdleView
    Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue
}
