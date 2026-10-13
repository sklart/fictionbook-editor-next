<# Exercises SourceEditorControl through an unattended FBE process. #>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 90)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if(-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'source-editor-ui'
$directory = $isolation.DataPath
$passed = $false
try {
    $fixture = Join-Path $directory 'source.fb2'; $report = Join-Path $directory 'source.tsv'
    '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>source</book-title><lang>en</lang></title-info><document-info><id>source-test</id><version>1.0</version></document-info></description><body><section><p>source</p></section></body></FictionBook>' | Set-Content -LiteralPath $fixture -Encoding utf8
    $oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'source-editor-ui-runtime'
        $process = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $report, $fixture) -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'source-editor-ui-runtime' -Report $report -TimeoutSeconds $TimeoutSeconds
        if($exitCode -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw "FBE SourceEditorControl scenario failed: exit $exitCode." }
    } finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario }
    $rows = @{}; foreach($line in Get-Content -LiteralPath $report) { $parts = $line -split "`t"; if($parts.Count -eq 2) { $rows[$parts[0]] = $parts[1] } }
    foreach($key in @('created', 'utf8', 'eol', 'eol_visibility', 'wrapping', 'whitespace', 'line_numbers', 'folding', 'styles', 'tag_state', 'metrics', 'reapply', 'caret_enhanced', 'caret_custom', 'caret_dpi', 'caret_selection', 'caret_undo')) { if($rows[$key] -ne '1') { throw "SourceEditorControl runtime check failed: $key (value '$($rows[$key])')." } }
	$designReport = Join-Path $directory 'caret-design.tsv'
	$oldMode, $oldScenario = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO
	try {
		$env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'caret-design-runtime'
		$design = Start-Process -FilePath $isolation.Exe -WorkingDirectory $isolation.Runtime -ArgumentList @('-b', $designReport, $fixture) -PassThru
		$designExitCode = Wait-IsolatedFbeProcess -Process $design -Isolation $isolation -Scenario 'caret-design-runtime' -Report $designReport -TimeoutSeconds $TimeoutSeconds
		if($designExitCode -ne 0 -or -not (Test-Path -LiteralPath $designReport)) { throw "Design caret scenario failed: exit $designExitCode." }
	} finally { $env:FBE_NEXT_TEST_MODE=$oldMode; $env:FBE_NEXT_TEST_SCENARIO=$oldScenario }
	$designRows = @{}; foreach($line in Get-Content -LiteralPath $designReport) { $parts = $line -split "`t|="; if($parts.Count -eq 2) { $designRows[$parts[0]] = $parts[1] } }
	foreach($key in @('located','shown','unchanged','removed','source_ignored','focus_restored','shown_after_return')) { if($designRows[$key] -ne '1') { throw "Design caret runtime check failed: $key (value '$($designRows[$key])')." } }
    $passed = $true
    Write-Host 'FBE SourceEditorControl runtime passed.'
} finally { Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed }
