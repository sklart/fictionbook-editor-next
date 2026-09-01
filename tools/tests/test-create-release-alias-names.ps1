<# Ensures create-release itself keeps the v3.0.8-rc.1 compatibility names. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$version = ([regex]::Match((Get-Content -Raw (Join-Path $root 'src\version.h')), 'FBE_VERSION_STRING\s+"(?<v>\d+\.\d+\.\d+)"')).Groups['v'].Value
if ($version -ne '3.0.8') {
    Write-Host "create-release 3.0.8 alias fixture skipped for version $version."
    return
}

$plan = @(& (Join-Path $root 'tools\build\create-release.ps1') -Platform Win32 -ReleaseTag 'v3.0.8-rc.2' -Prerelease -PrintArtifactPlan)
$expected = @(
    'Setup=FictionBookEditorNext-3.0.8-rc.2-win32-setup.exe',
    'Portable=FictionBookEditorNext-3.0.8-rc.2-win32-portable.zip',
    'Symbols=FictionBookEditorNext-3.0.8-rc.2-win32-symbols.zip',
    'LegacySetup=FictionBookEditorNext-3.0.8-win32-setup.exe',
    'LegacyPortable=FictionBookEditorNext-3.0.8-win32-portable.zip',
    'LegacyWin7Setup=FictionBookEditorNext-3.0.8-win7-win32-setup.exe',
    'LegacyWin7Portable=FictionBookEditorNext-3.0.8-win7-win32-portable.zip'
)
if (($plan -join '|') -ne ($expected -join '|')) {
    throw "create-release produced an invalid v3.0.8-rc.2 artifact plan: $($plan -join ', ')"
}
Write-Host 'create-release v3.0.8 prerelease compatibility alias plan passed.'
