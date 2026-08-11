<# Returns CMake and the generator selected from a supported Visual Studio installation. #>
[CmdletBinding()]
param([string]$PlatformToolset)
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Не найден vswhere.exe.' }
$args = @('-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64')
if ($PlatformToolset -eq 'v143') { $args += @('-version', '[17.0,18.0)') }
$installationPath = & $vswhere @args -property installationPath | Select-Object -First 1
if (-not $installationPath) { throw 'Не найдена подходящая установка Visual Studio.' }
$version = & $vswhere @args -property catalog_productLineVersion | Select-Object -First 1
$cmake = Get-ChildItem (Join-Path $installationPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin') -Filter cmake.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
if (-not $cmake) { $cmake = Get-Command cmake.exe -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source }
if (-not $cmake) { throw 'Не найден cmake.exe.' }
# Current VS-bundled CMake understands the stable VS2022 generator even when
# it is hosted by a newer Visual Studio installation.  Keep this explicit
# until CMake publishes a VS18 generator instead of assuming one exists.
[PSCustomObject]@{ CMake = $cmake; Generator = 'Visual Studio 17 2022' }
