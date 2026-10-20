<# Exercises real portable trace retention and explicit cleanup while verifying
   that the existing user diagnostics tree remains byte-for-byte unchanged. #>
[CmdletBinding()]
param([string]$FbeExecutable, [int]$TimeoutSeconds = 90)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $FbeExecutable) { $FbeExecutable = Join-Path $root 'out\Release\FBE.exe' }
$FbeExecutable = (Resolve-Path -LiteralPath $FbeExecutable).Path
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExecutable -Name 'diagnostic-cleanup'
$testRoot = $isolation.Root
$portableDiagnostics = Join-Path $isolation.DataPath 'Diagnostics'
$passed = $false
$userDiagnostics = Join-Path $env:LOCALAPPDATA 'FBE Next\Diagnostics'

function Get-FileTreeSnapshot([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) { return '<absent>' }
    return (Get-ChildItem -LiteralPath $Path -Recurse -File | Sort-Object FullName | ForEach-Object {
        "$($_.FullName)|$($_.Length)|$((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash)"
    }) -join "`n"
}

try {
    $null = New-IsolatedFbeSettingsSeed $isolation
    New-Item -ItemType Directory -Force -Path $portableDiagnostics | Out-Null

    # Twelve well-formed completed sessions are deliberately older than the new
    # process trace. Background retention and explicit ClearOldLogSessions() may
    # overlap; the serialized cleanup must preserve the active portable trace.
    foreach ($number in 1..12) {
        $name = 'fbe-trace-20200101-0000{0:D2}-{0:D3}-pid{1}.log' -f $number, (600000 + $number)
        Set-Content -LiteralPath (Join-Path $portableDiagnostics $name) -Value "portable-session-$number" -Encoding ascii
    }

    # Local FULL runs never seed or create user diagnostics.  The complete
    # existing tree is compared after the real portable cleanup scenario.
    $userBefore = Get-FileTreeSnapshot $userDiagnostics

    $oldMode, $oldScenario, $oldTrace = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TRACE
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'portable-diagnostic-cleanup'; $env:FBE_NEXT_TRACE = '1'
        Set-IsolatedRuntimeStage $isolation 'portable-diagnostic-cleanup'
        $process = Start-Process -FilePath $isolation.Exe -ArgumentList '--portable' -WorkingDirectory $testRoot -WindowStyle Hidden -PassThru
        $exitCode = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario 'portable-diagnostic-cleanup' -Report (Join-Path $portableDiagnostics 'portable-state-report.txt') -TimeoutSeconds $TimeoutSeconds
        if ($exitCode -ne 0) { throw "Portable diagnostic cleanup scenario exited with $exitCode." }
    } finally {
        $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TRACE = $oldMode, $oldScenario, $oldTrace
    }

    $reportPath = Join-Path $portableDiagnostics 'portable-state-report.txt'
    $report = Get-Content -LiteralPath $reportPath -Raw
    if ($report -notmatch '(?m)^portable=1$' -or $report -notmatch '(?m)^result=pass$') { throw "Portable diagnostic cleanup failed:`n$report" }
    if (@(Get-ChildItem -LiteralPath $portableDiagnostics -Filter 'fbe-trace-20200101-*.log' -File).Count -ne 0) { throw 'Completed portable diagnostic sessions were not removed.' }
    if (@(Get-ChildItem -LiteralPath $portableDiagnostics -Filter 'fbe-trace-*.log' -File).Count -ne 1) { throw 'Portable cleanup did not preserve exactly the active trace session.' }
    if ((Get-FileTreeSnapshot $userDiagnostics) -cne $userBefore) { throw 'Portable cleanup changed user diagnostic traces.' }
    Write-Host 'Portable diagnostic cleanup isolation behavior passed.'
    $passed = $true
}
finally {
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
