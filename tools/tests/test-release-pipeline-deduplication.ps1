<# Contract for the single Windows 7+ release pipeline. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
function Text([string]$path) { Get-Content -Raw -LiteralPath (Join-Path $root $path) }
function Require([string]$text, [string]$needle, [string]$where) {
    if (-not $text.Contains($needle)) { throw "$where must contain '$needle'." }
}
function Forbid([string]$text, [string]$needle, [string]$where) {
    if ($text.Contains($needle)) { throw "$where contains obsolete '$needle'." }
}

$workflow = Text '.github\workflows\build.yml'
$build = Text 'tools\build\build.ps1'
$verify = Text 'tools\build\verify-release.ps1'
$release = Text 'tools\build\create-release.ps1'
$artifacts = Text 'tools\build\verify-artifacts.ps1'
$stage = Text 'tools\build\stage-core.ps1'
$fingerprint = Text 'tools\build\editor-runtime-helpers.ps1'

foreach ($needle in @('validate:', 'build:', 'package:', 'publish:', 'Restore universal editor runtime cache', 'Build Release Win32', 'Build ArchHandler', 'Write Runtime build provenance', 'Verify universal release binaries', 'Create release artifacts without compiling', 'Verify release archives')) { Require $workflow $needle 'workflow' }
foreach ($needle in @('CompatibilityTarget', 'EditorRuntimeOnly', 'BatchConvertersOnly', 'Restore Modern editor runtime cache', 'Restore Win7 editor runtime cache', 'Build Modern', 'Build Win7', 'target-batches/Modern', 'target-batches/Win7', 'archhandler/Modern', 'archhandler/Win7', 'artifacts/Modern', 'artifacts/Win7')) { Forbid $workflow $needle 'workflow' }
foreach ($needle in @('CompatibilityTarget', 'EditorRuntimeOnly', 'BatchConvertersOnly')) { Forbid $build $needle 'build.ps1' }
foreach ($needle in @('CompatibilityTarget', 'CommonCoreDirectory', 'deployment.ini')) { Forbid $stage $needle 'stage-core.ps1' }
foreach ($needle in @('CompatibilityTarget', 'Modern', 'Win7')) { Forbid $fingerprint $needle 'editor runtime fingerprint' }
foreach ($needle in @('CompatibilityTarget', '-win7-win32-')) { Forbid $release $needle 'create-release.ps1' }
foreach ($needle in @('CompatibilityTarget', 'artifacts\\Modern', 'artifacts\\Win7')) { Forbid $artifacts $needle 'verify-artifacts.ps1' }
Require $verify 'check-win7-imports.ps1' 'verify-release.ps1'
Require $verify 'out\editor-runtime' 'verify-release.ps1'
Require $verify 'out\archhandler\Win32' 'verify-release.ps1'
Write-Host 'Unified release pipeline contract passed.'
