<# Ensures local release, wrapper, and CI use the same NSIS 3 resolver. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$resolver = Join-Path $root 'tools\build\resolve-nsis.ps1'
$release = Get-Content -Raw -LiteralPath (Join-Path $root 'tools\build\create-release.ps1')
$wrapper = Get-Content -Raw -LiteralPath (Join-Path $root 'packaging\nsis\Installer\MakeInstaller.bat')
$workflow = Get-Content -Raw -LiteralPath (Join-Path $root '.github\workflows\build.yml')
$installer = Get-Content -Raw -LiteralPath (Join-Path $root 'packaging\nsis\Installer\MakeInstaller.nsi')

if (-not (Test-Path -LiteralPath $resolver -PathType Leaf)) { throw 'Не найден общий NSIS resolver.' }
$resolverText = Get-Content -Raw -LiteralPath $resolver
foreach ($fragment in @("[version]'3.12'", 'FBE_MAKENSIS', 'NSIS\makensis.exe', 'Plugins\x86-unicode\UAC.dll')) {
    if ($resolverText.IndexOf($fragment, [StringComparison]::Ordinal) -lt 0) { throw "Resolver не содержит контракт: $fragment" }
}
if ($installer -notmatch '(?m)^Unicode true\s*$') { throw 'MakeInstaller.nsi обязан явно включать Unicode true.' }
foreach ($pair in @(@($release, 'resolve-nsis.ps1'), @($wrapper, 'resolve-nsis.ps1'), @($workflow, 'resolve-nsis.ps1'))) {
    if ($pair[0].IndexOf($pair[1], [StringComparison]::Ordinal) -lt 0) { throw "NSIS consumer не использует общий resolver: $($pair[1])" }
}
foreach ($obsolete in @('NSIS\Unicode\makensis.exe', 'Не найден Unicode-вариант NSIS')) {
    if ($release.IndexOf($obsolete, [StringComparison]::Ordinal) -ge 0 -or $wrapper.IndexOf($obsolete, [StringComparison]::Ordinal) -ge 0) { throw "Остался устаревший NSIS путь: $obsolete" }
}
& $resolver | Out-Null
Write-Host 'NSIS toolchain contract passed.'
