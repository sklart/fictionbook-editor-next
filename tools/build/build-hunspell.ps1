<#
.SYNOPSIS
Собирает статическую библиотеку Hunspell из локального проекта libhunspell.vcxproj.
#>

[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [string]$PlatformToolset
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "ThirdPartySources.ps1")

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$projectPath = Join-Path $repoRoot "build\hunspell\libhunspell.vcxproj"
$includeDir = Join-Path $repoRoot "build\hunspell\include"
$hunvisapiPath = Join-Path $includeDir "hunvisapi.h"
$solutionDir = $repoRoot
if (-not $solutionDir.EndsWith("\")) {
    $solutionDir += "\"
}

function New-HunspellBuildProject {
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$Toolset
    )

    New-Item -ItemType Directory -Path (Split-Path -Parent $Path) -Force | Out-Null
    New-Item -ItemType Directory -Path $includeDir -Force | Out-Null

    @"
#ifndef _HUNSPELL_VISIBILITY_H_
#define _HUNSPELL_VISIBILITY_H_

#if defined(HUNSPELL_STATIC)
#define LIBHUNSPELL_DLL_EXPORTED
#elif defined(BUILDING_LIBHUNSPELL)
#define LIBHUNSPELL_DLL_EXPORTED __declspec(dllexport)
#else
#define LIBHUNSPELL_DLL_EXPORTED __declspec(dllimport)
#endif

#endif
"@ | Set-Content -LiteralPath $hunvisapiPath -Encoding ascii

    @'
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|Win32">
      <Configuration>Debug</Configuration>
      <Platform>Win32</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|Win32">
      <Configuration>Release</Configuration>
      <Platform>Win32</Platform>
    </ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <ProjectGuid>{A4E47388-15F1-4B6E-930E-E631BC69D441}</ProjectGuid>
    <RootNamespace>libhunspell</RootNamespace>
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|Win32'" Label="Configuration">
    <ConfigurationType>StaticLibrary</ConfigurationType>
    <PlatformToolset>__PLATFORM_TOOLSET__</PlatformToolset>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|Win32'" Label="Configuration">
    <ConfigurationType>StaticLibrary</ConfigurationType>
    <PlatformToolset>__PLATFORM_TOOLSET__</PlatformToolset>
    <CharacterSet>Unicode</CharacterSet>
    <WholeProgramOptimization>true</WholeProgramOptimization>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
  <ImportGroup Label="ExtensionSettings" />
  <ImportGroup Label="Shared" />
  <ImportGroup Label="PropertySheets">
    <Import Project="$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props"
            Condition="exists('$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props')"
            Label="LocalAppDataPlatform" />
  </ImportGroup>
  <PropertyGroup Label="UserMacros" />
  <PropertyGroup>
    <OutDir>$(SolutionDir)build\hunspell\lib\$(Configuration)\</OutDir>
    <IntDir>$(SolutionDir)build\hunspell\obj\$(Configuration)\</IntDir>
    <TargetName>libhunspell</TargetName>
  </PropertyGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|Win32'">
    <ClCompile>
      <AdditionalIncludeDirectories>$(SolutionDir)build\hunspell\include;$(SolutionDir)third_party\hunspell\src\hunspell;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <PreprocessorDefinitions>WIN32;_DEBUG;HUNSPELL_STATIC;_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>
      <WarningLevel>Level3</WarningLevel>
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
    </ClCompile>
  </ItemDefinitionGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|Win32'">
    <ClCompile>
      <AdditionalIncludeDirectories>$(SolutionDir)build\hunspell\include;$(SolutionDir)third_party\hunspell\src\hunspell;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <PreprocessorDefinitions>WIN32;NDEBUG;HUNSPELL_STATIC;_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <RuntimeLibrary>MultiThreaded</RuntimeLibrary>
      <WarningLevel>Level3</WarningLevel>
      <PrecompiledHeader>NotUsing</PrecompiledHeader>
      <Optimization>MaxSpeed</Optimization>
      <FunctionLevelLinking>true</FunctionLevelLinking>
      <IntrinsicFunctions>true</IntrinsicFunctions>
    </ClCompile>
    <Lib>
      <LinkTimeCodeGeneration>true</LinkTimeCodeGeneration>
    </Lib>
  </ItemDefinitionGroup>
  <ItemGroup>
    <ClCompile Include="$(SolutionDir)third_party\hunspell\src\hunspell\affentry.cxx" />
    <ClCompile Include="$(SolutionDir)third_party\hunspell\src\hunspell\affixmgr.cxx" />
    <ClCompile Include="$(SolutionDir)third_party\hunspell\src\hunspell\csutil.cxx" />
    <ClCompile Include="$(SolutionDir)third_party\hunspell\src\hunspell\filemgr.cxx" />
    <ClCompile Include="$(SolutionDir)third_party\hunspell\src\hunspell\hashmgr.cxx" />
    <ClCompile Include="$(SolutionDir)third_party\hunspell\src\hunspell\hunspell.cxx" />
    <ClCompile Include="$(SolutionDir)third_party\hunspell\src\hunspell\hunzip.cxx" />
    <ClCompile Include="$(SolutionDir)third_party\hunspell\src\hunspell\phonet.cxx" />
    <ClCompile Include="$(SolutionDir)third_party\hunspell\src\hunspell\replist.cxx" />
    <ClCompile Include="$(SolutionDir)third_party\hunspell\src\hunspell\suggestmgr.cxx" />
  </ItemGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
'@.Replace("__PLATFORM_TOOLSET__", $Toolset) | Set-Content -LiteralPath $Path -Encoding utf8
}

if (-not $PlatformToolset) {
    $PlatformToolset = "v145"
}

if (-not (Test-Path -LiteralPath $projectPath)) {
    Write-Host "Проект сборки Hunspell не найден, создаю: $projectPath"
    New-HunspellBuildProject -Path $projectPath -Toolset $PlatformToolset
}
elseif (-not (Test-Path -LiteralPath $hunvisapiPath)) {
    Write-Host "Заголовок Hunspell visibility не найден, обновляю служебные файлы сборки."
    New-HunspellBuildProject -Path $projectPath -Toolset $PlatformToolset
}

if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "Не найден vswhere.exe. Установите Visual Studio с инструментами сборки C++."
}

$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild `
    -find "MSBuild\Current\Bin\MSBuild.exe" | Select-Object -First 1

if (-not $msbuild) {
    throw "Не найден MSBuild.exe."
}

& $msbuild $projectPath /m /t:Build `
    "/p:Configuration=$Configuration" `
    "/p:Platform=Win32" `
    "/p:PlatformToolset=$PlatformToolset" `
    "/p:SolutionDir=$solutionDir" `
    /v:minimal /nologo

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host (Format-ThirdPartyText "SHVuc3BlbGwg0L/QvtC00LPQvtGC0L7QstC70LXQvSDQsiBidWlsZFxcaHVuc3BlbGxcXGxpYlxcezB9" $Configuration)
