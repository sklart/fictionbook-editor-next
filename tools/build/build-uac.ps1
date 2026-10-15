<# Builds the vendored NSIS UAC plug-in with the supported Win32 toolchain. #>
[CmdletBinding()]
param([string]$VcToolsVersion)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sourceDir = Join-Path $repoRoot 'third_party\uac'
$buildDir = Join-Path $repoRoot 'build\uac'
$destination = Join-Path $sourceDir 'Plugins\x86-unicode\UAC.dll'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw "Visual Studio locator not found: $vswhere" }
$candidates = @(& $vswhere -products '*' -property installationPath | ForEach-Object {
    $root = $_
    $toolsDir = Join-Path $root 'VC\Tools\MSVC'
    if (Test-Path -LiteralPath (Join-Path $root 'VC\Auxiliary\Build\vcvarsall.bat')) {
        Get-ChildItem -LiteralPath $toolsDir -Directory -ErrorAction SilentlyContinue | ForEach-Object {
            $version = $null
            if ([Version]::TryParse($_.Name, [ref]$version) -and $version.Major -eq 14 -and
                (Test-Path -LiteralPath (Join-Path $_.FullName 'bin\Hostx64\x86\cl.exe'))) {
                [pscustomobject]@{ Root=$root; Version=$version; Name=$_.Name;
                    Preferred=[int]($version.Major -eq 14 -and $version.Minor -eq 44) }
            }
        }
    }
})
if ($VcToolsVersion) { $candidates = @($candidates | Where-Object Name -EQ $VcToolsVersion) }
$selected = $candidates | Sort-Object -Property @{Expression='Preferred';Descending=$true},
    @{Expression='Version';Descending=$true}, @{Expression='Root';Descending=$false} | Select-Object -First 1
if (-not $selected) { throw "No compatible x86 MSVC toolset found (requested: '$VcToolsVersion')." }
$vsRoot = $selected.Root
$vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path -LiteralPath $vcvars)) { throw "VC environment script not found: $vcvars" }
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

$commands = @(
    '@echo off',
    "call `"$vcvars`" x86 -vcvars_ver=$($selected.Name) >nul || exit /b 1",
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
Write-Host "Built Win32 Unicode UAC.dll with MSVC $($selected.Name): $destination"
