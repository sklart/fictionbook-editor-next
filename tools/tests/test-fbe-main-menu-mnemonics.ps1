<# Проверяет локализованные мнемоники в том же дереве, из которого генерируется MENU. #>
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
& (Join-Path $repoRoot 'tools\localization\update-fbe-main-menu-resource.ps1') -ValidateMnemonicsOnly
if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) {
    throw "Проверка мнемоник генератором завершилась с кодом $LASTEXITCODE."
}
