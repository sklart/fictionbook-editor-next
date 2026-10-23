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
$waitRoot = Join-Path ([IO.Path]::GetTempPath()) ('fbe-wait-contract-' + [guid]::NewGuid().ToString('N'))
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
Write-Host 'Runtime verification isolation contract and controlled timeout/nonzero-exit regressions passed.'
