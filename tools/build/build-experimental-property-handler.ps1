[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("Win32", "x64")]
    [string]$Platform = "Win32",

    [string]$PlatformToolset,

    [string]$OutputDirectory = "",

    [string]$IntermediateDirectory = "",

    [switch]$SkipUpx
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$fbshellProject = Join-Path $repoRoot "src\fbshell\FBShell.vcxproj"
$experimentalOutDir = if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    Join-Path $repoRoot "out\package\shell-build\$Platform\$Configuration"
} else {
    $OutputDirectory
}
$experimentalIntDir = if ([string]::IsNullOrWhiteSpace($IntermediateDirectory)) {
    Join-Path $repoRoot "build\obj\fbshell\property-handler\$Platform\$Configuration"
} else {
    $IntermediateDirectory
}

if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "Не найден vswhere.exe. Установите Visual Studio с инструментами сборки C++."
}

. (Join-Path $PSScriptRoot "Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64 -PlatformToolset $PlatformToolset -VcVarsVersion '14.44'

$vswhereArguments = @("-latest", "-products", "*", "-requires", "Microsoft.Component.MSBuild")
if ($PlatformToolset -eq "v143") {
    $vswhereArguments += @("-version", "[17.0,18.0)")
}
$vswhereArguments += @("-find", "MSBuild\Current\Bin\MSBuild.exe")
$msbuild = & $vswhere @vswhereArguments |
    Select-Object -First 1

if (-not $msbuild) {
    throw "Не найден MSBuild.exe."
}

$properties = @(
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform",
    "/p:SolutionDir=$repoRoot\",
    "/p:ExperimentalPropertyHandler=true",
    "/p:OutDir=$experimentalOutDir\",
    "/p:IntDir=$experimentalIntDir\"
)

$properties += "/p:EnableUpx=false"

if ($PlatformToolset) {
    $properties += "/p:PlatformToolset=$PlatformToolset"
}

& $msbuild $fbshellProject /m /t:Build `
    $properties /v:minimal /nologo

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Штатная shell-сборка подготовлена в отдельной FBShell.dll."
Write-Host "В неё сейчас входят:"
Write-Host "  - обработчик свойств для .fb2;"
Write-Host "  - обработчик миниатюр для обложек .fb2."
Write-Host "Собирался только проект src\fbshell\FBShell.vcxproj, без полного FBE.sln."
Write-Host "Результат сборки: $experimentalOutDir"
