<# Builds the vendored NSIS UAC plug-in with the supported Win32 toolchain. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sourceDir = Join-Path $repoRoot 'third_party\uac'
$buildDir = Join-Path $repoRoot 'build\uac'
$destination = Join-Path $sourceDir 'Plugins\x86-unicode\UAC.dll'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw "Visual Studio locator not found: $vswhere" }
$vsRoot = & $vswhere -products '*' -property installationPath |
    Where-Object {
        $toolsDir = Join-Path $_ 'VC\Tools\MSVC'
        (Test-Path -LiteralPath $toolsDir) -and
            @(Get-ChildItem -LiteralPath $toolsDir -Directory -Filter '14.44.*').Count -gt 0
    } | Select-Object -First 1
if (-not $vsRoot) { throw 'Visual Studio with VC Tools 14.44 was not found.' }
$vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path -LiteralPath $vcvars)) { throw "VC environment script not found: $vcvars" }
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

$commands = @(
    '@echo off',
    "call `"$vcvars`" x86 -vcvars_ver=14.44 >nul || exit /b 1",
    "cl /nologo /std:c++17 /O2 /DNDEBUG /EHsc- /GR- /W3 /DUNICODE /c /Fo`"$buildDir/`" `"$sourceDir\uac.cpp`" `"$sourceDir\RunAs.cpp`" `"$sourceDir\util.cpp`" || exit /b 1",
    "rc /nologo /DUNICODE /fo`"$buildDir\resource.res`" `"$sourceDir\resource.rc`" || exit /b 1",
    "link /nologo /DLL /MACHINE:X86 /OPT:REF /OPT:ICF /OUT:`"$buildDir\UAC.dll`" `"$buildDir\uac.obj`" `"$buildDir\RunAs.obj`" `"$buildDir\util.obj`" `"$buildDir\resource.res`" kernel32.lib user32.lib shell32.lib advapi32.lib ole32.lib gdi32.lib || exit /b 1"
)
$commandFile = Join-Path $buildDir 'build.cmd'
[IO.File]::WriteAllLines($commandFile, $commands, [Text.Encoding]::ASCII)
Push-Location $repoRoot
try {
    & $env:ComSpec /d /c $commandFile
    if ($LASTEXITCODE -ne 0) { throw "UAC.dll build failed with exit code $LASTEXITCODE." }
}
finally { Pop-Location }
Copy-Item -LiteralPath (Join-Path $buildDir 'UAC.dll') -Destination $destination -Force
Write-Host "Built Win32 Unicode UAC.dll: $destination"
