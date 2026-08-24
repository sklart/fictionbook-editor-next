[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $root 'tools\build\UpdateVersion.ps1')
foreach ($version in @('3.2.0', '3.2.0-beta.1', '3.2.0-rc.10', '3.2.0-alpha.beta', '3.2.0-0.3.7', '3.2.0-x.7.z.92', '3.2.0+build.1', '3.2.0+build-foo', '3.2.0+build.foo-bar', '3.2.0-rc.2+build.42', '3.2.0-rc.1+build-foo')) {
    if (-not (Test-FbeSemVer $version)) { throw "PowerShell SemVer rejected valid version: $version" }
}
foreach ($version in @('03.2.0', '3.02.0', '3.2.00', '3.2', '3.2.0.1', '3.2.0-', '3.2.0+', '3.2.0.+build.1', '3.2.0-rc.+build.1', '3.2.0-rc..1+build.1', '3.2.0-rc.', '3.2.0-.rc', '3.2.0-rc..1', '3.2.0-rc.01', '3.2.0-01', '3.2.0-rc/1', '3.2.0-rc\1', '3.2.0-rc_1', '3.2.0+build..1', '3.2.0+build/')) {
    if (Test-FbeSemVer $version) { throw "PowerShell SemVer accepted invalid version: $version" }
}
foreach ($tag in @('v3.2.0', 'v3.2.0-rc.1', 'v3.2.0-beta.12')) { if (-not (Test-FbeReleaseTag $tag)) { throw "ReleaseTag rejected: $tag" } }
foreach ($tag in @('3.2.0', 'v', 'v3.2', 'v03.2.0', 'v3.2.0-rc.01', 'v3.2.0/rc.1', 'v3.2.0-rc..1')) { if (Test-FbeReleaseTag $tag) { throw "Invalid ReleaseTag accepted: $tag" } }
if ((Get-FbeBaseVersion '3.2.0-rc.2+build.4') -ne '3.2.0' -or (Get-FbeBaseVersion '3.2.0') -ne '3.2.0' -or (Get-FbeBaseVersion 'bad')) { throw 'Get-FbeBaseVersion contract failed.' }
if ((Test-FbePrereleaseVersion '3.2.0') -or (Test-FbePrereleaseVersion '3.2.0+build.1') -or (Test-FbePrereleaseVersion '3.2.0+build-foo') -or (Test-FbePrereleaseVersion '3.2.0+build.foo-bar') -or -not (Test-FbePrereleaseVersion '3.2.0-beta.1') -or -not (Test-FbePrereleaseVersion '3.2.0-rc.2+build.1') -or -not (Test-FbePrereleaseVersion '3.2.0-rc.1+build-foo')) { throw 'Test-FbePrereleaseVersion contract failed.' }
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
