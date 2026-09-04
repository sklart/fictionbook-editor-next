[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    # Match the Win32 Release toolset used by the repository CI build.
    [string]$PlatformToolset = "v143"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$sourcePath = Join-Path $PSScriptRoot "native\hunspell_probe.cpp"
$libPath = Join-Path $repoRoot "build\hunspell\lib\$Configuration\libhunspell.lib"
$projectDir = Join-Path $repoRoot "build\hunspell-probe"
$projectPath = Join-Path $projectDir "hunspell-probe.vcxproj"
$outputDir = Join-Path $repoRoot "out\tools"
$solutionDir = $repoRoot.TrimEnd("\") + "\"

if (-not (Test-Path -LiteralPath $libPath -PathType Leaf)) {
    & (Join-Path $repoRoot "tools\build\build-hunspell.ps1") -Configuration $Configuration -PlatformToolset $PlatformToolset
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

New-Item -ItemType Directory -Path $projectDir -Force | Out-Null
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

$project = @'
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|Win32"><Configuration>Debug</Configuration><Platform>Win32</Platform></ProjectConfiguration>
    <ProjectConfiguration Include="Release|Win32"><Configuration>Release</Configuration><Platform>Win32</Platform></ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <ProjectGuid>{72A9FC43-E926-4888-84B7-1687BD2BAA51}</ProjectGuid>
    <RootNamespace>HunspellProbe</RootNamespace>
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|Win32'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType><PlatformToolset>__TOOLSET__</PlatformToolset><CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|Win32'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType><PlatformToolset>__TOOLSET__</PlatformToolset><CharacterSet>Unicode</CharacterSet><WholeProgramOptimization>true</WholeProgramOptimization>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
  <PropertyGroup>
    <OutDir>$(SolutionDir)out\tools\</OutDir>
    <IntDir>$(SolutionDir)build\hunspell-probe\obj\$(Configuration)\</IntDir>
    <TargetName>hunspell-probe</TargetName>
  </PropertyGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|Win32'">
    <ClCompile><AdditionalIncludeDirectories>$(SolutionDir)build\hunspell\include;$(SolutionDir)third_party\hunspell\src\hunspell;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories><PreprocessorDefinitions>WIN32;_DEBUG;HUNSPELL_STATIC;_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions)</PreprocessorDefinitions><RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary><LanguageStandard>stdcpp17</LanguageStandard></ClCompile>
    <Link><AdditionalDependencies>$(SolutionDir)build\hunspell\lib\Debug\libhunspell.lib;%(AdditionalDependencies)</AdditionalDependencies></Link>
  </ItemDefinitionGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|Win32'">
    <ClCompile><AdditionalIncludeDirectories>$(SolutionDir)build\hunspell\include;$(SolutionDir)third_party\hunspell\src\hunspell;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories><PreprocessorDefinitions>WIN32;NDEBUG;HUNSPELL_STATIC;_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions)</PreprocessorDefinitions><RuntimeLibrary>MultiThreaded</RuntimeLibrary><Optimization>MaxSpeed</Optimization><LanguageStandard>stdcpp17</LanguageStandard></ClCompile>
    <Link><AdditionalDependencies>$(SolutionDir)build\hunspell\lib\Release\libhunspell.lib;%(AdditionalDependencies)</AdditionalDependencies></Link>
  </ItemDefinitionGroup>
  <ItemGroup><ClCompile Include="__SOURCE__" /></ItemGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
'@
$project = $project.Replace("__TOOLSET__", $PlatformToolset).Replace("__SOURCE__", $sourcePath)
$project | Set-Content -LiteralPath $projectPath -Encoding utf8

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) { throw "vswhere.exe not found." }
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\Current\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) { throw "MSBuild.exe not found." }

& $msbuild $projectPath /m /t:Build "/p:Configuration=$Configuration" "/p:Platform=Win32" "/p:PlatformToolset=$PlatformToolset" "/p:SolutionDir=$solutionDir" /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Hunspell probe: $(Join-Path $outputDir 'hunspell-probe.exe')"
