[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
function Require([string]$Path, [string]$Pattern, [string]$Message) {
    $text = Get-Content -LiteralPath (Join-Path $root $Path) -Raw
    if ($text -notmatch $Pattern) { throw $Message }
}

Require 'tools\tests\RuntimeTestIsolation.ps1' 'fr-.*NewGuid' 'Isolated runtime must use a short unique disposable root.'
Require 'tools\tests\RuntimeTestIsolation.ps1' 'Copy-Item.*-Recurse' 'Isolated runtime must copy the complete Release runtime.'
Require 'tools\tests\RuntimeTestIsolation.ps1' 'DataPath=\$dataName' 'Each isolated runtime must use a unique DataPath.'
Require 'tools\tests\RuntimeTestIsolation.ps1' 'WaitForExit\(\)' 'Child-process bookkeeping must complete before reading ExitCode.'
Require 'tools\tests\RuntimeTestIsolation.ps1' 'Runtime failure artifacts' 'Failures must retain and report disposable-runtime artifacts.'
Require 'tools\tests\test-fbe-archive-mru-runtime.ps1' 'Assert-MruRestart' 'MRU restart result must be checked field by field.'
Require 'tools\tests\test-fbe-archive-mru-runtime.ps1' 'empty' 'MRU diagnostics must retain the informational empty field.'
Require 'tools\tests\test-fbe-archive-runtime.ps1' 'Set-IsolatedRuntimeStage' 'Recovery scenarios must write a progress marker.'
Require 'tools\tests\test-fbe-archive-runtime.ps1' 'Recovery\.fb2' 'Recovery runtime must assert snapshot creation and cleanup.'
Require 'src\fbe\mainfrm.cpp' 'archive-recovery-verify.*archive-recovery-external-verify.*normal-recovery-verify.*&& U::MessageBox' 'Recovery verification scenarios must bypass the modal recovery prompt.'
Require 'tools\tests\test-fbe-document-tree-window-runtime.ps1' 'tree-window\.log' 'Tree failure report must use a CI-exported diagnostics extension.'
Require 'tools\tests\test-fbe-document-tree-window-runtime.ps1' 'Set-IsolatedRuntimeStage' 'Tree runtime must identify the failed deployment mode.'
$gate = Get-Content -LiteralPath (Join-Path $root 'tools\build\verify-release.ps1') -Raw
if($gate -notmatch 'test-fbe-document-tree-selection-contract\.ps1' -or
   $gate -notmatch '(?s)foreach \(\$treeMode in @\(''portable'', ''installed''\)\).*?test-fbe-document-tree-window-runtime\.ps1.*?-Mode \$treeMode') {
    throw 'Release verification must run the tree contract and both deployment modes.'
}
Require 'tools\tests\test-fbe-editor-view-lifecycle-runtime.ps1' 'Wait-IsolatedFbeProcess' 'View lifecycle must reap its timed-out process before cleanup.'
Require 'tools\tests\test-fbe-editor-view-lifecycle-runtime.ps1' 'New-IsolatedFbeSettingsSeed' 'View lifecycle must use a fresh settings seed.'
Require 'tools\tests\test-fbe-editor-view-lifecycle-runtime.ps1' 'FBE_NEXT_TEST_STARTUP_BREADCRUMB' 'View lifecycle must retain startup phase diagnostics.'

. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
# Exercise the real process bookkeeping without opening a GUI or modifying FBE.
$waitRoot = Join-Path ([IO.Path]::GetTempPath()) ('fr-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $waitRoot | Out-Null
$pwsh = (Get-Process -Id $PID).Path
$waitIsolation = [pscustomobject]@{Root=$waitRoot;Runtime=$waitRoot;Exe=$pwsh;DataPath=$waitRoot;StagePath=(Join-Path $waitRoot 'stage.txt')}
$waitReport = Join-Path $waitRoot 'report.txt'
$child = $null
try {
    $child = Start-Process $pwsh -ArgumentList '-NoProfile -Command "Start-Sleep -Seconds 30"' -WindowStyle Hidden -PassThru
    $timeoutObserved = $false
    try {
        $null = Wait-IsolatedFbeProcess -Process $child -Isolation $waitIsolation -Scenario 'controlled-wait-timeout' -Report $waitReport -TimeoutSeconds 1
    } catch {
        if ($_.Exception.Message -notlike "FBE runtime scenario 'controlled-wait-timeout' timed out after 1 seconds.*") { throw }
        if ($_.Exception.Message -notlike '*report_exists=False*') { throw 'Timeout omitted report diagnostics.' }
        $timeoutObserved = $true
    }
    $child.Refresh()
    if (!$timeoutObserved -or !$child.HasExited) { throw 'Timeout must fail and reap the child before runtime cleanup.' }
    Complete-IsolatedFbeRuntime -Isolation $waitIsolation -Passed $true
    if (Test-Path $waitRoot) { throw 'Reaped child prevented disposable cleanup.' }

    $child = Start-Process $pwsh -ArgumentList '-NoProfile -Command "exit 7"' -WindowStyle Hidden -PassThru
    $code = Wait-IsolatedFbeProcess -Process $child -Isolation $waitIsolation -Scenario 'controlled-nonzero-exit' -Report $waitReport -TimeoutSeconds 10
    if ($code -ne 7) { throw 'Nonzero child exit was hidden.' }
} finally {
    if ($child -and !$child.HasExited) { Stop-Process -Id $child.Id -Force -ErrorAction SilentlyContinue; $child.WaitForExit() }
}
$savedArtifacts = $env:FBE_RUNTIME_FAILURE_ARTIFACTS
$testArtifacts = Join-Path $root ('build\isolation-test-' + [guid]::NewGuid().ToString('N'))
try {
    $fixtureSource = Join-Path $testArtifacts 'source'
    New-Item -ItemType Directory -Path $fixtureSource -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $fixtureSource 'FBE.exe') -Value 'fixture'
    $before = @(Get-ChildItem -LiteralPath ([IO.Path]::GetTempPath()) -Directory -Filter 'fr-*' | Select-Object -ExpandProperty FullName)
    $creationFailed = $false
    try { New-IsolatedFbeRuntime -FbeExe (Join-Path $fixtureSource 'FBE.exe') -Name 'invalid|name' | Out-Null } catch { $creationFailed = $true }
    $newRoots = @(Get-ChildItem -LiteralPath ([IO.Path]::GetTempPath()) -Directory -Filter 'fr-*' | Where-Object { $_.FullName -notin $before })
    if (!$creationFailed -or $newRoots.Count) { throw 'A failed runtime creation must remove its partial copy.' }
    $env:FBE_RUNTIME_FAILURE_ARTIFACTS = $testArtifacts
    $owned = New-IsolatedFbeScratchDirectory -Name 'owned-process'
    $ownedExe = Join-Path $owned.Root 'owned-cmd.exe'
    Copy-Item -LiteralPath $env:ComSpec -Destination $ownedExe
    $ownedChild = Start-Process -FilePath $ownedExe -ArgumentList '/c ping -n 30 127.0.0.1 >nul' -WindowStyle Hidden -PassThru
    try {
        $detected = $false
        for ($i = 0; $i -lt 30 -and -not $detected; $i++) { $detected = @(Get-IsolatedRuntimeProcesses $owned | Where-Object ProcessId -eq $ownedChild.Id).Count -gt 0; if (-not $detected) { Start-Sleep -Milliseconds 100 } }
        if (-not $detected) { throw 'The isolated non-FBE child was not detected.' }
        $reported = $false
        try { Complete-IsolatedFbeRuntime -Isolation $owned -Passed $true } catch { $reported = $_.Exception.Message -like 'Disposable runtime leaked process(es):*' }
        $ownedChild.Refresh()
        if (-not $reported -or -not $ownedChild.HasExited -or (Test-Path -LiteralPath $owned.Root)) { throw 'The isolated child must be stopped and the leaked-process failure reported.' }
    } finally {
        if (-not $ownedChild.HasExited) { Stop-Process -Id $ownedChild.Id -Force -ErrorAction SilentlyContinue; $ownedChild.WaitForExit() }
        if (Test-Path -LiteralPath $owned.Root) { Complete-IsolatedFbeRuntime -Isolation $owned -Passed $true }
    }
    $bounded = New-IsolatedFbeScratchDirectory -Name 'bounded'
    New-Item -ItemType Directory -Path (Join-Path $bounded.Root 'nested') | Out-Null
    Set-Content -LiteralPath (Join-Path $bounded.Root 'nested\evidence.txt') -Value 'diagnostic'
    Set-Content -LiteralPath (Join-Path $bounded.Root 'nested\runtime.dll') -Value 'binary'
    $large = [IO.File]::OpenWrite((Join-Path $bounded.Root 'nested\huge.xml'))
    try { $large.SetLength(6MB) } finally { $large.Dispose() }
    for ($i = 0; $i -lt 5; $i++) { $part = [IO.File]::OpenWrite((Join-Path $bounded.Root "nested\part-$i.xml")); try { $part.SetLength(5MB) } finally { $part.Dispose() } }
    Complete-IsolatedFbeRuntime -Isolation $bounded -Passed $false -PreserveContents
    $boundedArtifacts = Join-Path $testArtifacts (Split-Path -Leaf $bounded.Root)
    if ((Test-Path $bounded.Root) -or !(Test-Path (Join-Path $boundedArtifacts 'nested\evidence.txt')) -or (Test-Path (Join-Path $boundedArtifacts 'nested\runtime.dll')) -or (Test-Path (Join-Path $boundedArtifacts 'nested\huge.xml'))) { throw 'Failure evidence must retain small diagnostics without binaries or oversized files.' }
    $summary = Get-Content -LiteralPath (Join-Path $boundedArtifacts 'artifact-summary.txt') -Raw
    if ($summary -notmatch '(?m)^skipped=([2-9]|[1-9][0-9]+)\r?$' -or $summary -notmatch '(?m)^bytes=(\d+)\r?$' -or [long]$Matches[1] -gt 20MB) { throw 'The artifact summary must report omitted files and enforce the total size limit.' }
    $kept = New-IsolatedFbeRuntime -FbeExe (Join-Path $fixtureSource 'FBE.exe') -Name 'kept'
    try {
        Complete-IsolatedFbeRuntime -Isolation $kept -Passed $true -KeepArtifacts
        if (Test-Path -LiteralPath (Join-Path $kept.Root '.fbe-runtime-owner.json')) { throw 'Explicitly retained artifacts must not be marked for cleanup.' }
        Remove-AbandonedIsolatedFbeRuntimes
        if (!(Test-Path -LiteralPath $kept.Root)) { throw 'Explicitly retained artifacts were removed.' }
    } finally { if (Test-Path -LiteralPath $kept.Root) { Remove-Item -LiteralPath $kept.Root -Recurse -Force } }
    $ready = Join-Path $testArtifacts 'child-ready.txt'
    $helper = (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1').Replace("'", "''")
    $exe = (Join-Path $fixtureSource 'FBE.exe').Replace("'", "''")
    $readyQuoted = $ready.Replace("'", "''")
    $command = ". '$helper'; `$iso = New-IsolatedFbeRuntime -FbeExe '$exe' -Name interrupted; Set-Content -LiteralPath (Join-Path `$iso.Root 'test-stage.txt') -Value 'interrupted'; [IO.File]::WriteAllText('$readyQuoted', `$iso.Root); Start-Sleep -Seconds 60"
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
    $child = Start-Process $pwsh -ArgumentList "-NoProfile -EncodedCommand $encoded" -WindowStyle Hidden -PassThru
    try {
        for ($i = 0; $i -lt 100 -and !(Test-Path -LiteralPath $ready) -and !$child.HasExited; $i++) { Start-Sleep -Milliseconds 100 }
        if (!(Test-Path -LiteralPath $ready)) { throw 'Child did not create its test runtime.' }
        $orphan = [IO.File]::ReadAllText($ready)
        Remove-AbandonedIsolatedFbeRuntimes
        if (!(Test-Path -LiteralPath $orphan)) { throw 'A live owner must protect its runtime.' }
        Stop-Process -Id $child.Id -Force
        $child.WaitForExit()
        Remove-AbandonedIsolatedFbeRuntimes
        $orphanArtifacts = Join-Path $testArtifacts (Split-Path -Leaf $orphan)
        if ((Test-Path -LiteralPath $orphan) -or !(Test-Path -LiteralPath (Join-Path $orphanArtifacts 'test-stage.txt'))) { throw 'The next cleanup must preserve diagnostics and remove the killed child runtime.' }
    } finally {
        if (!$child.HasExited) { Stop-Process -Id $child.Id -Force -ErrorAction SilentlyContinue; $child.WaitForExit() }
    }
    Remove-Item -LiteralPath $ready -Force
    $command = ". '$helper'; `$iso = New-IsolatedFbeScratchDirectory -Name interrupted-scratch; New-Item -ItemType Directory -Path (Join-Path `$iso.Root 'nested') | Out-Null; Set-Content -LiteralPath (Join-Path `$iso.Root 'nested\evidence.txt') -Value 'evidence'; [IO.File]::WriteAllText('$readyQuoted', `$iso.Root); Start-Sleep -Seconds 60"
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
    $child = Start-Process $pwsh -ArgumentList "-NoProfile -EncodedCommand $encoded" -WindowStyle Hidden -PassThru
    try {
        for ($i = 0; $i -lt 100 -and !(Test-Path -LiteralPath $ready) -and !$child.HasExited; $i++) { Start-Sleep -Milliseconds 100 }
        if (!(Test-Path -LiteralPath $ready)) { throw 'Child did not create its scratch directory.' }
        $orphan = [IO.File]::ReadAllText($ready)
        Stop-Process -Id $child.Id -Force; $child.WaitForExit()
        Remove-AbandonedIsolatedFbeRuntimes
        $orphanArtifacts = Join-Path $testArtifacts (Split-Path -Leaf $orphan)
        if ((Test-Path -LiteralPath $orphan) -or !(Test-Path -LiteralPath (Join-Path $orphanArtifacts 'nested\evidence.txt'))) { throw 'Interrupted scratch cleanup must preserve nested evidence.' }
    } finally {
        if (!$child.HasExited) { Stop-Process -Id $child.Id -Force -ErrorAction SilentlyContinue; $child.WaitForExit() }
    }
    Remove-Item -LiteralPath $ready -Force
    $command = ". '$helper'; `$iso = New-IsolatedFbeScratchDirectory -Name interrupted-runtime -RuntimePayload; New-Item -ItemType Directory -Path (Join-Path `$iso.Root 'Resources') | Out-Null; Set-Content -LiteralPath (Join-Path `$iso.Root 'Resources\payload.xml') -Value 'bundled'; Set-Content -LiteralPath (Join-Path `$iso.Root 'report.txt') -Value 'diagnostic'; [IO.File]::WriteAllText('$readyQuoted', `$iso.Root); Start-Sleep -Seconds 60"
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
    $child = Start-Process $pwsh -ArgumentList "-NoProfile -EncodedCommand $encoded" -WindowStyle Hidden -PassThru
    try {
        for ($i = 0; $i -lt 100 -and !(Test-Path -LiteralPath $ready) -and !$child.HasExited; $i++) { Start-Sleep -Milliseconds 100 }
        if (!(Test-Path -LiteralPath $ready)) { throw 'Child did not create its runtime payload.' }
        $orphan = [IO.File]::ReadAllText($ready)
        Stop-Process -Id $child.Id -Force; $child.WaitForExit()
        Remove-AbandonedIsolatedFbeRuntimes
        $orphanArtifacts = Join-Path $testArtifacts (Split-Path -Leaf $orphan)
        if ((Test-Path -LiteralPath $orphan) -or !(Test-Path -LiteralPath (Join-Path $orphanArtifacts 'report.txt')) -or (Test-Path -LiteralPath (Join-Path $orphanArtifacts 'Resources\payload.xml'))) { throw 'Interrupted runtime payload must preserve its report without copying bundled resources.' }
    } finally {
        if (!$child.HasExited) { Stop-Process -Id $child.Id -Force -ErrorAction SilentlyContinue; $child.WaitForExit() }
    }
    New-Item -ItemType Directory -Path $waitRoot | Out-Null
    Set-Content -LiteralPath (Join-Path $waitRoot 'test-stage.txt') -Value 'failed stage'
    New-Item -ItemType Directory -Path (Join-Path $waitRoot 'Recovery') | Out-Null
    Set-Content -LiteralPath (Join-Path $waitRoot 'Recovery\fixture.fb2') -Value 'recovery'
    New-Item -ItemType Directory -Path (Join-Path $waitRoot 'Diagnostics') | Out-Null
    Set-Content -LiteralPath (Join-Path $waitRoot 'Diagnostics\startup.log') -Value 'startup'
    Complete-IsolatedFbeRuntime -Isolation $waitIsolation -Passed $false
    $saved = Join-Path $testArtifacts (Split-Path -Leaf $waitRoot)
    if ((Test-Path $waitRoot) -or !(Test-Path (Join-Path $saved 'test-stage.txt')) -or !(Test-Path (Join-Path $saved 'Recovery\fixture.fb2')) -or !(Test-Path (Join-Path $saved 'Diagnostics\startup.log'))) { throw 'Failure must save diagnostics and remove the disposable runtime.' }
    New-Item -ItemType Directory -Path $waitRoot | Out-Null
    Complete-IsolatedFbeRuntime -Isolation $waitIsolation -Passed $false -KeepArtifacts
    if (!(Test-Path $waitRoot)) { throw 'Explicit KeepArtifacts must retain the runtime.' }
    $env:FBE_RUNTIME_FAILURE_ARTIFACTS = Join-Path $waitRoot 'nested'
    $rejected = $false
    try { Complete-IsolatedFbeRuntime -Isolation $waitIsolation -Passed $false } catch { $rejected = $true }
    if (!$rejected -or !(Test-Path $waitRoot)) { throw 'Invalid diagnostics location must leave the runtime intact.' }
    Complete-IsolatedFbeRuntime -Isolation $waitIsolation -Passed $true
} finally {
    $env:FBE_RUNTIME_FAILURE_ARTIFACTS = $savedArtifacts
    if (Test-Path $waitRoot) { Remove-Item -LiteralPath $waitRoot -Recurse -Force }
    if (Test-Path $testArtifacts) { Remove-Item -LiteralPath $testArtifacts -Recurse -Force }
}
Write-Host 'Runtime verification isolation contract and controlled timeout/nonzero-exit regressions passed.'
