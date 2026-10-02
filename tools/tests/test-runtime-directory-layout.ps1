<#
.SYNOPSIS
Verifies the canonical runtime directory names in source, staging, portable packaging, and NSIS input.
#>
[CmdletBinding()]
param(
    [string]$RuntimeDirectory
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$expectedRuntimeDirectories = @('Backgrounds', 'Dict', 'Help', 'HTML', 'Lang', 'Plugins', 'Resources', 'Scripts', 'Themes', 'TreeCmd', 'Utilities')
$sourceRuntimeDirectories = @('Backgrounds', 'Dict', 'Help', 'HTML', 'Plugins', 'Resources', 'Scripts', 'Themes', 'TreeCmd', 'Utilities')
$legacyDirectories = @('dict', 'EditorBackgrounds', 'ArchiveMruRuntimeData')
function Test-ExactDirectory([string]$Parent, [string]$Name) { return @((Get-ChildItem -LiteralPath $Parent -Directory -Force | Select-Object -ExpandProperty Name) -ccontains $Name) }

foreach($directory in $sourceRuntimeDirectories) {
    if(-not (Test-ExactDirectory (Join-Path $repoRoot "runtime") $directory)) {
        throw "Runtime source directory is missing: $directory"
    }
}
foreach($directory in $legacyDirectories) {
    if(Test-ExactDirectory (Join-Path $repoRoot "runtime") $directory) {
        throw "Legacy runtime source directory must not exist: $directory"
    }
}

$manifest = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'packaging\package-manifest.json') -Encoding UTF8 | ConvertFrom-Json
if(@($manifest.core.runtimeDirectories) -join '|' -ne ($expectedRuntimeDirectories -join '|')) {
    throw 'Package manifest must declare the canonical runtime directory list in canonical order.'
}
foreach($directory in $legacyDirectories) {
    if(@($manifest.core.runtimeDirectories) -ccontains $directory) { throw "Package manifest contains legacy runtime directory: $directory" }
    if(@($manifest.core.forbidden) -cnotcontains $directory) { throw "Package manifest must forbid legacy runtime directory: $directory" }
}

$installer = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'packaging\nsis\Installer\MakeInstaller.nsi') -Encoding UTF8
foreach($directory in @('Backgrounds', 'Dict', 'HTML')) {
    if($installer -notmatch [regex]::Escape("`$INSTDIR\$directory")) { throw "NSIS installer does not target canonical directory: $directory" }
}
if($installer -cmatch [regex]::Escape('${INPUTDIR}\dict\') -or $installer -cmatch [regex]::Escape('${INPUTDIR}\EditorBackgrounds\')) {
    throw 'NSIS installer still reads a legacy runtime directory.'
}

$archiveTest = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\tests\test-fbe-archive-mru-runtime.ps1') -Encoding UTF8
$isolation = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools\tests\RuntimeTestIsolation.ps1') -Encoding UTF8
if($archiveTest -notmatch [regex]::Escape("New-IsolatedFbeRuntime -FbeExe `$FbeExe -Name 'ArchiveMruRuntimeData'")) {
    throw 'Archive MRU data must be created only by the isolated runtime test.'
}
if($isolation -notmatch 'if \(\$Passed\) \{ Remove-Item -LiteralPath \$Isolation\.Root -Recurse -Force \}' -or
   $isolation -notmatch 'Runtime failure artifacts: \$\(\$Isolation\.Root\)') {
    throw 'Isolated runtime cleanup must remove successful data and retain failed diagnostics only in its isolated directory.'
}

if($RuntimeDirectory) {
    $stage = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($RuntimeDirectory)
    foreach($directory in $expectedRuntimeDirectories) {
        if(-not (Test-ExactDirectory $stage $directory)) { throw "Staged runtime directory is missing: $directory" }
    }
    foreach($directory in $legacyDirectories) {
        if(Test-ExactDirectory $stage $directory) { throw "Staged runtime contains forbidden legacy/test directory: $directory" }
    }
}

Write-Host 'Canonical runtime directory layout passed.'
