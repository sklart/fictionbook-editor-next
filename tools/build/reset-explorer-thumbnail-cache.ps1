[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = "Medium")]
param(
    [switch]$NoRestartExplorer
)

$ErrorActionPreference = "Stop"

function Test-IsAdministrator {
    $currentIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $currentPrincipal = New-Object Security.Principal.WindowsPrincipal($currentIdentity)
    return $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Get-ThumbnailCacheFiles {
    $explorerCacheDirectory = Join-Path $env:LOCALAPPDATA "Microsoft\Windows\Explorer"
    if (-not (Test-Path -LiteralPath $explorerCacheDirectory -PathType Container)) {
        return @()
    }

    return @(Get-ChildItem -LiteralPath $explorerCacheDirectory -Filter "thumbcache*" -File -ErrorAction SilentlyContinue)
}

function Get-IconCacheFiles {
    $localAppDataDirectory = $env:LOCALAPPDATA
    if ([string]::IsNullOrWhiteSpace($localAppDataDirectory) -or -not (Test-Path -LiteralPath $localAppDataDirectory -PathType Container)) {
        return @()
    }

    $iconCacheFiles = @()

    $rootIconCacheFile = Join-Path $localAppDataDirectory "IconCache.db"
    if (Test-Path -LiteralPath $rootIconCacheFile -PathType Leaf) {
        $iconCacheFiles += Get-Item -LiteralPath $rootIconCacheFile
    }

    $explorerCacheDirectory = Join-Path $localAppDataDirectory "Microsoft\Windows\Explorer"
    if (Test-Path -LiteralPath $explorerCacheDirectory -PathType Container) {
        $iconCacheFiles += Get-ChildItem -LiteralPath $explorerCacheDirectory -Filter "iconcache*" -File -ErrorAction SilentlyContinue
    }

    return @($iconCacheFiles | Sort-Object FullName -Unique)
}

if (-not (Test-IsAdministrator)) {
    throw "Для сброса thumbnail cache Проводника нужны права администратора."
}

$thumbnailCacheFiles = Get-ThumbnailCacheFiles
$iconCacheFiles = Get-IconCacheFiles

if ($thumbnailCacheFiles.Count -eq 0 -and $iconCacheFiles.Count -eq 0) {
    Write-Host "Файлы кэша миниатюр и значков не найдены. Сбрасывать нечего."
    return
}

if ($thumbnailCacheFiles.Count -gt 0) {
    Write-Host "Будут удалены файлы thumbnail cache Проводника:"
    $thumbnailCacheFiles | Select-Object FullName, Length, LastWriteTime | Format-Table -AutoSize
}

if ($iconCacheFiles.Count -gt 0) {
    Write-Host "Будут удалены файлы icon cache Проводника:"
    $iconCacheFiles | Select-Object FullName, Length, LastWriteTime | Format-Table -AutoSize
}

if ($PSCmdlet.ShouldProcess("Explorer shell cache", "Удалить thumbcache/iconcache и перезапустить explorer.exe")) {
    Write-Host "Останавливаем explorer.exe..."
    Get-Process explorer -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 2

    foreach ($file in $thumbnailCacheFiles) {
        Remove-Item -LiteralPath $file.FullName -Force -ErrorAction Stop
    }

    foreach ($file in $iconCacheFiles) {
        Remove-Item -LiteralPath $file.FullName -Force -ErrorAction Stop
    }

    Write-Host "Кэш миниатюр и значков удалён."

    if (-not $NoRestartExplorer) {
        Write-Host "Перезапускаем explorer.exe..."
        Start-Process explorer.exe
    }

    Write-Host "Сброс кэша миниатюр и значков завершён."
}
