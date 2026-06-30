[CmdletBinding()]
param()

# .SYNOPSIS
# Максимально очищает тестовое shell-состояние FBE: registry, shared shell dir
# и локальные build/output-каталоги, не падая на уже удалённых или временно
# занятых файлах.

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

$pathsToRemove = @(
    (Join-Path $repoRoot "out\installer-smoke-app"),
    (Join-Path $repoRoot "out\property-handler"),
    (Join-Path $repoRoot "out\property-handler-alt"),
    (Join-Path $repoRoot "out\property-handler-alt2"),
    (Join-Path $repoRoot "out\package\shell-build"),
    "C:\ProgramData\FictionBook Editor\Shell"
)

$registryPaths = @(
    "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.fb2",
    "HKLM:\SOFTWARE\Classes\.fb2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}",
    "HKLM:\SOFTWARE\Classes\FictionBook.2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}",
    "HKLM:\SOFTWARE\Classes\CLSID\{D4A47F38-1E5A-4F0D-B1C9-6D2A4A6B1F42}",
    "HKLM:\SOFTWARE\Classes\CLSID\{4F99D1F0-5D76-4B9C-9D3D-9E6B8B4C7E31}"
)

foreach ($path in $registryPaths) {
    if (Test-Path -LiteralPath $path) {
        try {
            Remove-Item -LiteralPath $path -Recurse -Force
            Write-Host "Удален ключ реестра: $path"
        }
        catch {
            Write-Warning "Не удалось удалить ключ реестра: $path"
            Write-Warning $_.Exception.Message
        }
    }
}

foreach ($path in $pathsToRemove) {
    if (Test-Path -LiteralPath $path) {
        try {
            Remove-Item -LiteralPath $path -Recurse -Force
            Write-Host "Удален путь: $path"
        }
        catch {
            Write-Warning "Не удалось полностью удалить путь: $path"
            Write-Warning $_.Exception.Message
        }
    }
}

Write-Host "Очистка тестового shell-состояния завершена."
