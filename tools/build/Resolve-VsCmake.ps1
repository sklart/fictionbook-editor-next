<# Returns CMake and the generator selected from a supported Visual Studio installation. #>
[CmdletBinding()]
param([string]$PlatformToolset)
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Не найден vswhere.exe.' }
$args = @('-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-version', '[17.0,18.0)')
# CMake's stable Visual Studio generator is "Visual Studio 17 2022".  Select
# that instance itself (also for v143), rather than taking CMake from a newer
# instance and assuming that VS2022 is installed alongside it.
$instances = @(& $vswhere @args -format json | ConvertFrom-Json)
$activeInstallationPath = if ($env:VSINSTALLDIR) { $env:VSINSTALLDIR.TrimEnd('\', '/') } else { $null }
$instance = if ($activeInstallationPath -and (Test-Path -LiteralPath (Join-Path $activeInstallationPath 'VC\Tools\MSVC'))) {
    $instances | Where-Object { $_.installationPath.TrimEnd('\', '/') -ieq $activeInstallationPath } | Select-Object -First 1
}
if (-not $instance) {
    # Prefer a complete IDE instance when no initialized VS environment pins
    # the choice.  This avoids silently selecting an unrelated Build Tools
    # installation with the same v143 minor toolset.
    $instance = $instances | Where-Object {
        $_.productId -eq 'Microsoft.VisualStudio.Product.Enterprise' -and
        (Test-Path -LiteralPath (Join-Path $_.installationPath 'VC\Tools\MSVC'))
    } | Select-Object -First 1
}
if (-not $instance) {
    $instance = $instances | Where-Object { Test-Path -LiteralPath (Join-Path $_.installationPath 'VC\Tools\MSVC') } | Select-Object -First 1
}
$installationPath = $instance.installationPath
if (-not $installationPath) {
	if ($PlatformToolset -eq 'v143') { throw 'Для PlatformToolset v143 требуется установленная Visual Studio 2022 с компонентом C++.' }
	throw 'Не найдена Visual Studio 2022 с компонентом C++, совместимая с генератором Visual Studio 17 2022.'
}
$cmake = Get-ChildItem (Join-Path $installationPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin') -Filter cmake.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
if (-not $cmake) {
    # Community/Enterprise may have the compiler workload but not the optional
    # CMake component.  The executable may come from another VS2022 instance;
    # generator instance and toolset below still remain pinned to the selected
    # compiler instance.
    $cmake = $instances |
        ForEach-Object { Get-ChildItem (Join-Path $_.installationPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin') -Filter cmake.exe -Recurse -ErrorAction SilentlyContinue } |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $cmake) { $cmake = Get-Command cmake.exe -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source }
if (-not $cmake) { throw 'Не найден cmake.exe.' }
$installationVersion = $instance.installationVersion
$availableToolsets = @(Get-ChildItem -LiteralPath (Join-Path $installationPath 'VC\Tools\MSVC') -Directory -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty Name |
    Sort-Object -Descending)
if ($availableToolsets.Count -eq 0) {
    throw "В Visual Studio '$installationPath' не найдены MSVC toolset directories."
}

# The supported release toolchain is VC 14.44.  Prefer the toolset selected by
# vcvars when it is compatible, otherwise select the installed 14.44 version.
$effectivePlatformToolset = if ($PlatformToolset) { $PlatformToolset } else { 'v143' }
$requestedPrefix = if ($env:VCToolsVersion -match '^14\.44\.\d+$') {
    [regex]::Escape($env:VCToolsVersion)
} elseif ($effectivePlatformToolset -eq 'v143') {
    '^14\.44\.'
} else {
    '^'
}
$vcToolsVersion = $availableToolsets | Where-Object { $_ -match $requestedPrefix } | Select-Object -First 1
if (-not $vcToolsVersion) {
    throw "Для $PlatformToolset не найден совместимый VC 14.44 toolset в '$installationPath'. Доступны: $($availableToolsets -join ', ')."
}
$cmakeVersion = (& $cmake --version | Select-Object -First 1) -replace '^cmake version\s+', ''
$generatorToolset = if ($effectivePlatformToolset -eq 'v143') { "v143,version=$vcToolsVersion" } else { $effectivePlatformToolset }
[PSCustomObject]@{
    CMake = $cmake
    CMakeVersion = $cmakeVersion
    Generator = 'Visual Studio 17 2022'
    GeneratorToolset = $generatorToolset
    InstallationPath = (Resolve-Path -LiteralPath $installationPath).Path
    InstallationVersion = $installationVersion
    GeneratorInstance = (Resolve-Path -LiteralPath $installationPath).Path
    VCToolsVersion = $vcToolsVersion
}
