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
Write-Host 'Runtime verification isolation contract passed.'
