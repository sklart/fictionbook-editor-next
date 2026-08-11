<# Returns CMake and the generator selected from a supported Visual Studio installation. #>
[CmdletBinding()]
param([string]$PlatformToolset)
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Не найден vswhere.exe.' }
$args = @('-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-version', '[17.0,18.0)')
# CMake's stable Visual Studio generator is "Visual Studio 17 2022".  Select
# that instance itself (also for v143), rather than taking CMake from a newer
# instance and assuming that VS2022 is installed alongside it.
$installationPath = & $vswhere @args -property installationPath | Select-Object -First 1
if (-not $installationPath) {
	if ($PlatformToolset -eq 'v143') { throw 'Для PlatformToolset v143 требуется установленная Visual Studio 2022 с компонентом C++.' }
	throw 'Не найдена Visual Studio 2022 с компонентом C++, совместимая с генератором Visual Studio 17 2022.'
}
$cmake = Get-ChildItem (Join-Path $installationPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin') -Filter cmake.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
if (-not $cmake) { $cmake = Get-Command cmake.exe -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source }
if (-not $cmake) { throw 'Не найден cmake.exe.' }
[PSCustomObject]@{ CMake = $cmake; Generator = 'Visual Studio 17 2022' }
