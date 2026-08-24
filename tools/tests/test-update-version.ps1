[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Не найдены C++ build tools.' }
$vsDevCmd = Join-Path $vs 'Common7\Tools\VsDevCmd.bat'
$msvc = Get-ChildItem -LiteralPath (Join-Path $vs 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
if (-not $msvc) { throw 'Не найден каталог MSVC.' }
$out = Join-Path ([IO.Path]::GetTempPath()) 'fbe-update-version-test.exe'
$testSource = Join-Path $PSScriptRoot 'test-update-version.cpp'
$implementation = Join-Path $root 'src\fbe\UpdateVersion.cpp'
$command = '"{0}" -arch=x86 -host_arch=x64 >nul && cl /nologo /EHsc /DUNICODE /D_UNICODE /I "{1}\src\fbe" /I "{1}\third_party\wtl" /I "{2}\ATLMFC\include" "{3}" "{4}" /Fe"{5}"' -f $vsDevCmd, $root, $msvc.FullName, $testSource, $implementation, $out
& cmd.exe /d /s /c $command
if ($LASTEXITCODE -ne 0) { throw 'Не удалось собрать SemVer regression test.' }
& $out
if ($LASTEXITCODE -ne 0) { throw 'SemVer regression test failed.' }
Write-Host 'SemVer regression test passed.'
