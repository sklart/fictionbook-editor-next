<# Verifies that historical x64 export configurations fail before they can
   request the Win32-only FBE contract or produce an x64 plug-in. #>
[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration = 'Release')

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$null = & (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion '14.44'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -version '[17.0,18.0)' -find 'MSBuild\Current\Bin\MSBuild.exe' | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($msbuild)) { throw 'Visual Studio 2022 MSBuild was not found.' }

$testRoot = Join-Path $repoRoot 'out\tests\export-plugin-x64-rejection'
Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $testRoot | Out-Null
$contractX64 = Join-Path $repoRoot "build\generated\x64\$Configuration\fbe-api"
$contractBefore = if (Test-Path -LiteralPath $contractX64) { Get-ChildItem -LiteralPath $contractX64 -Recurse -File | ForEach-Object FullName } else { @() }

foreach ($case in @(
    @{ Name = 'ExportHTML'; Project = 'src\export-html\ExportHTML.vcxproj' },
    @{ Name = 'ExportDOCX'; Project = 'src\export-docx\ExportDOCX.vcxproj' },
    @{ Name = 'ExportEPUB'; Project = 'src\export-epub\ExportEPUB.vcxproj' }
)) {
    $output = Join-Path $testRoot $case.Name
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    $result = & $msbuild (Join-Path $repoRoot $case.Project) /t:Build "/p:Configuration=$Configuration" /p:Platform=x64 "/p:OutDir=$output\" /v:normal /nologo 2>&1
    $exitCode = $LASTEXITCODE; $text = ($result | ForEach-Object { [string]$_ }) -join "`n"
    if ($exitCode -eq 0) { throw "$($case.Name) x64 build unexpectedly succeeded." }
    if ($text -notmatch 'RejectUnsupportedExportPluginPlatform' -or $text -notmatch 'supports only Win32') { throw "$($case.Name) x64 build did not fail through RejectUnsupportedExportPluginPlatform:`n$text" }
    if ($text -match 'FBEContracts\.vcxproj|fbe\.idl') { throw "$($case.Name) x64 rejection attempted to build FBEContracts." }
    if (Get-ChildItem -LiteralPath $output -Recurse -Filter "$($case.Name).dll" -File -ErrorAction SilentlyContinue) { throw "$($case.Name) x64 rejection produced a DLL." }
}
$contractAfter = if (Test-Path -LiteralPath $contractX64) { Get-ChildItem -LiteralPath $contractX64 -Recurse -File | ForEach-Object FullName } else { @() }
if (@($contractAfter | Where-Object { $_ -notin $contractBefore }).Count -ne 0) { throw 'x64 export rejection generated FBEContracts artifacts.' }
Write-Host 'x64 export plug-in rejection behavior passed.'
