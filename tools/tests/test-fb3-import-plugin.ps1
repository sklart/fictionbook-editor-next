[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$InputPackage,
    [string]$PluginPath,
    [string]$ExportPluginPath
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (!$PluginPath) { $PluginPath = Join-Path $root 'out/Release/Plugins/ImportFB3.dll' }
$source = (Resolve-Path -LiteralPath $InputPackage).Path
$plugin = (Resolve-Path -LiteralPath $PluginPath).Path
& (Join-Path $root 'tools/build/Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$scratch = New-IsolatedFbeScratchDirectory -Name 'fb3-import-plugin'
$passed = $false
try {
    $plugins = New-Item -Path (Join-Path $scratch.Root 'Plugins') -ItemType Directory
    $stagedPlugin = Join-Path $plugins 'ImportFB3.dll'
    Copy-Item -LiteralPath $plugin -Destination $stagedPlugin
    $arguments = @()
    if ($ExportPluginPath) {
        $stagedExport = Join-Path $plugins 'ExportFB3.dll'
        Copy-Item -LiteralPath (Resolve-Path -LiteralPath $ExportPluginPath).Path -Destination $stagedExport
        Copy-Item -LiteralPath (Join-Path $root 'runtime/FB3Schemas') -Destination $scratch.Root -Recurse
        $arguments += $stagedExport
    }
    foreach ($schema in 'FictionBook.xsd', 'FictionBookGenres.xsd', 'FictionBookLang.xsd', 'FictionBookLinks.xsd') {
        Copy-Item -LiteralPath (Join-Path $root "runtime/$schema") -Destination $scratch.Root
    }
    $valid = Join-Path $scratch.Root 'Книга (FB3).fb3'
    Copy-Item -LiteralPath $source -Destination $valid
    $before = (Get-FileHash -LiteralPath $valid).Hash
    $broken = Join-Path $scratch.Root 'broken.fb3'
    [IO.File]::WriteAllBytes($broken, [byte[]]@(0x50, 0x4b, 3, 4))
    $exe = Join-Path $scratch.Root 'fb3-import-plugin-test.exe'
    & cl.exe /nologo /EHsc /std:c++17 /utf-8 /W4 /WX /Zc:wchar_t- /DUNICODE /D_UNICODE `
        ('/I' + (Join-Path $root 'build/generated/Win32/Release/fbe-api')) `
        "/Fo$($scratch.Root)\\" "/Fe$exe" (Join-Path $PSScriptRoot 'fb3-import-plugin-test.cpp') /link ole32.lib oleaut32.lib
    if ($LASTEXITCODE -ne 0) { throw 'FB3 plugin harness did not compile.' }
    & $exe $stagedPlugin $valid $broken @arguments
    if ($LASTEXITCODE -ne 0) { throw "FB3 plugin harness failed with exit code $LASTEXITCODE." }
    if ((Get-FileHash -LiteralPath $valid).Hash -ne $before) { throw 'FB3 plugin modified its input.' }
    $passed = $true
} finally {
    Complete-IsolatedFbeRuntime -Isolation $scratch -Passed $passed -PreserveContents
}
