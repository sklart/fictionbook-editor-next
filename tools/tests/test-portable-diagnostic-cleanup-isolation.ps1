<# Exercises real portable trace retention and explicit cleanup without touching
   the user's diagnostic sessions. #>
[CmdletBinding()]
param([string]$FbeExecutable)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $FbeExecutable) { $FbeExecutable = Join-Path $root 'out\Release\FBE.exe' }
$FbeExecutable = (Resolve-Path -LiteralPath $FbeExecutable).Path
$sourceDirectory = Split-Path -Parent $FbeExecutable
$testRoot = Join-Path $root 'out\tests\portable-diagnostic-cleanup-isolation'
$portableDiagnostics = Join-Path $testRoot 'Data\Diagnostics'
$userDiagnostics = Join-Path $env:LOCALAPPDATA 'FBE Next\Diagnostics'
$token = Get-Random -Minimum 100000000 -Maximum 999999999
$userFiles = @(
    (Join-Path $userDiagnostics "fbe-trace-20000101-000001-001-pid$token.log")
    (Join-Path $userDiagnostics "fbe-trace-20000101-000002-002-pid$($token + 1).log")
)
$userDirectoryExisted = Test-Path -LiteralPath $userDiagnostics -PathType Container

function Get-FileHashSnapshot([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return '<absent>' }
    $item = Get-Item -LiteralPath $Path
    return "$($item.Length)|$((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash)"
}

try {
    Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
    Copy-Item -LiteralPath $sourceDirectory -Destination $testRoot -Recurse -Force
    New-Item -ItemType Directory -Force -Path $portableDiagnostics | Out-Null
    "[Portable]`r`nDataPath=Data`r`n" | Set-Content -LiteralPath (Join-Path $testRoot 'portable.ini') -Encoding utf8NoBOM

    # Twelve well-formed completed sessions are deliberately older than the new
    # process trace.  Start() retains ten sessions; ClearOldLogSessions() then
    # removes every completed portable session while preserving its active one.
    foreach ($number in 1..12) {
        $name = 'fbe-trace-20200101-0000{0:D2}-{0:D3}-pid{1}.log' -f $number, (600000 + $number)
        Set-Content -LiteralPath (Join-Path $portableDiagnostics $name) -Value "portable-session-$number" -Encoding ascii
    }

    New-Item -ItemType Directory -Force -Path $userDiagnostics | Out-Null
    Set-Content -LiteralPath $userFiles[0] -Value "user-session-one-$token" -Encoding ascii
    Set-Content -LiteralPath $userFiles[1] -Value "user-session-two-$token" -Encoding ascii
    $userBefore = @{}; foreach ($path in $userFiles) { $userBefore[$path] = Get-FileHashSnapshot $path }

    $oldMode, $oldScenario, $oldTrace = $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TRACE
    try {
        $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'portable-diagnostic-cleanup'; $env:FBE_NEXT_TRACE = '1'
        $process = Start-Process -FilePath (Join-Path $testRoot 'FBE.exe') -ArgumentList '--portable' -WorkingDirectory $testRoot -Wait -PassThru
        if ($process.ExitCode -ne 0) { throw "Portable diagnostic cleanup scenario exited with $($process.ExitCode)." }
    } finally {
        $env:FBE_NEXT_TEST_MODE, $env:FBE_NEXT_TEST_SCENARIO, $env:FBE_NEXT_TRACE = $oldMode, $oldScenario, $oldTrace
    }

    $reportPath = Join-Path $portableDiagnostics 'portable-state-report.txt'
    $report = Get-Content -LiteralPath $reportPath -Raw
    if ($report -notmatch '(?m)^portable=1$' -or $report -notmatch '(?m)^result=pass$') { throw "Portable diagnostic cleanup failed:`n$report" }
    if ((Get-ChildItem -LiteralPath $portableDiagnostics -Filter 'fbe-trace-20200101-*.log' -File).Count -ne 0) { throw 'Completed portable diagnostic sessions were not removed.' }
    if ((Get-ChildItem -LiteralPath $portableDiagnostics -Filter 'fbe-trace-*.log' -File).Count -ne 1) { throw 'Portable cleanup did not preserve exactly the active trace session.' }
    foreach ($path in $userFiles) {
        if ((Get-FileHashSnapshot $path) -cne $userBefore[$path]) { throw "Portable cleanup changed user diagnostic trace: $path" }
    }
    Write-Host 'Portable diagnostic cleanup isolation behavior passed.'
}
finally {
    foreach ($path in $userFiles) { Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue }
    if (-not $userDirectoryExisted -and (Test-Path -LiteralPath $userDiagnostics -PathType Container) -and -not (Get-ChildItem -LiteralPath $userDiagnostics -Force | Select-Object -First 1)) {
        Remove-Item -LiteralPath $userDiagnostics -Force -ErrorAction SilentlyContinue
    }
}
