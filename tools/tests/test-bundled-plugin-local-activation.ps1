<#
.SYNOPSIS
Проверяет bundled plug-ins через DllGetClassObject, без CoCreateInstance,
regsvr32 и каких-либо записей COM в реестре.
#>
[CmdletBinding()]
param(
    [string]$RuntimeDirectory,
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $RuntimeDirectory) { $RuntimeDirectory = Join-Path $root "out\Release" }
$RuntimeDirectory = (Resolve-Path -LiteralPath $RuntimeDirectory).Path

& (Join-Path $root 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64
$testDirectory = Join-Path $root "out\tests\bundled-plugin-local-activation\Win32\$Configuration"
$testExecutable = Join-Path $testDirectory 'bundled-plugin-local-activation.exe'
New-Item -ItemType Directory -Force -Path $testDirectory | Out-Null

& cl.exe /nologo /EHsc /std:c++14 /utf-8 /DUNICODE /D_UNICODE /MD /W3 `
    "/Fo$testDirectory\\" (Join-Path $PSScriptRoot 'bundled-plugin-local-activation.cpp') `
    (Join-Path $root 'src\fbe\FBE_i.c') '/link' '/SUBSYSTEM:CONSOLE' 'ole32.lib' 'oleaut32.lib' "/OUT:$testExecutable"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$manifestPath = Join-Path $root 'runtime\Plugins\plugins.json'
$plugins = @((Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json).plugins)
foreach ($plugin in $plugins) {
    if ($plugin.activation -ne 'local-com') { continue }
    # Release payloads resolve module names relative to plugins.json.  Keep
    # the flat development output fallback because out\Release is deliberately
    # not reshaped by this packaging change.
    $path = Join-Path (Join-Path $RuntimeDirectory 'Plugins') $plugin.module
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { $path = Join-Path $RuntimeDirectory $plugin.module }
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Не найден bundled plug-in: $path" }
    & $testExecutable $path $plugin.clsid $plugin.id $plugin.type
    if ($LASTEXITCODE -ne 0) { throw "Локальная активация $($plugin.module) завершилась с кодом $LASTEXITCODE." }
}
Write-Host 'Bundled plug-ins активируются локально, без обращения к реестру.'
