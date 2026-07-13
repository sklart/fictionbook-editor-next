# Восстанавливает COM-регистрацию плагинов FBE для локальной сборки из out\<Configuration>.
# Полезно при ручном запуске out\Release\FBE.exe, когда Windows ещё помнит старые пути
# к установленным или Debug-версиям Export/Import DLL.
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Release',

    [string] $RepositoryRoot,

    [string] $PluginDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}
else {
    $RepositoryRoot = (Resolve-Path $RepositoryRoot).Path
}

if ([string]::IsNullOrWhiteSpace($PluginDirectory)) {
    $PluginDirectory = Join-Path $RepositoryRoot "out\$Configuration"
}
$PluginDirectory = (Resolve-Path $PluginDirectory).Path

$regsvr32 = Join-Path $env:WINDIR 'SysWOW64\regsvr32.exe'
if (-not (Test-Path -LiteralPath $regsvr32)) {
    $regsvr32 = Join-Path $env:WINDIR 'System32\regsvr32.exe'
}
if (-not (Test-Path -LiteralPath $regsvr32)) {
    throw "regsvr32.exe не найден."
}

$plugins = @(
    @{ Name = 'ExportHTML'; Dll = 'ExportHTML.dll'; Clsid = '{E242A6D3-84BF-4285-9FAA-160F95370668}' },
    @{ Name = 'ExportDOCX'; Dll = 'ExportDOCX.dll'; Clsid = '{41494D79-3346-4E8C-A432-51BCD3742FC1}' },
    @{ Name = 'ExportEPUB'; Dll = 'ExportEPUB.dll'; Clsid = '{A9406281-7F4A-4D4B-9D5B-BF1FC6BDF9EF}' },
    @{ Name = 'ImportEPUB'; Dll = 'ImportEPUB.dll'; Clsid = '{D4B1B165-4D93-4F2D-8C8A-2D0C649431A1}' }
)

function Get-RegisteredInprocPath {
    param([Parameter(Mandatory = $true)][string] $Clsid)

    $paths = @(
        "Registry::HKEY_CURRENT_USER\Software\Classes\CLSID\$Clsid\InprocServer32",
        "Registry::HKEY_CURRENT_USER\Software\Classes\Wow6432Node\CLSID\$Clsid\InprocServer32",
        "Registry::HKEY_LOCAL_MACHINE\Software\Classes\CLSID\$Clsid\InprocServer32",
        "Registry::HKEY_LOCAL_MACHINE\Software\Classes\Wow6432Node\CLSID\$Clsid\InprocServer32"
    )

    foreach ($path in $paths) {
        if (Test-Path -LiteralPath $path) {
            $value = (Get-Item -LiteralPath $path -ErrorAction Stop).GetValue('')
            if (-not [string]::IsNullOrWhiteSpace($value)) {
                return $value
            }
        }
    }

    return ''
}

Write-Host "Восстановление COM-регистрации локальных плагинов FBE"
Write-Host "  Каталог плагинов: $PluginDirectory"
Write-Host "  regsvr32: $regsvr32"

foreach ($plugin in $plugins) {
    $dllPath = Join-Path $PluginDirectory $plugin.Dll
    if (-not (Test-Path -LiteralPath $dllPath)) {
        Write-Warning "Пропускаю $($plugin.Name): DLL не найдена: $dllPath"
        continue
    }

    $before = Get-RegisteredInprocPath -Clsid $plugin.Clsid
    Write-Host ""
    Write-Host "$($plugin.Name)"
    if (-not [string]::IsNullOrWhiteSpace($before)) {
        Write-Host "  Было: $before"
    }
    else {
        Write-Host "  Было: <не зарегистрировано>"
    }

    $process = Start-Process -FilePath $regsvr32 -ArgumentList @('/s', $dllPath) -Wait -PassThru -WindowStyle Hidden
    if ($process.ExitCode -ne 0) {
        throw "regsvr32 завершился с кодом $($process.ExitCode) для $dllPath"
    }

    $after = Get-RegisteredInprocPath -Clsid $plugin.Clsid
    Write-Host "  Стало: $after"
}

Write-Host ""
Write-Host "Локальная регистрация плагинов восстановлена."
Write-Host "Перезапустите FBE.exe, если он был открыт."
