[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

foreach ($path in @("LICENSE", "NOTICE", "THIRD-PARTY-NOTICES.md", "THIRD-PARTY-LICENSES\README.md", "THIRD-PARTY-LICENSES\WTL-MS-PL.txt")) {
    if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $path) -PathType Leaf)) {
        throw "Отсутствует обязательный лицензионный документ: $path"
    }
}

$notices = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "THIRD-PARTY-NOTICES.md")
foreach ($component in @("Scintilla | 5.6.6", "Lexilla | 5.5.3", "PCRE2 | 10.47", "Hunspell | 1.7.3", "libwebp | 1.6.0", "OpenJPEG | 2.5.4", "libheif | 1.23.1", "libde265 | 1.1.0", "libaom | 3.14.1", "Windows Template Library (WTL) | 10.01", "LunaSVG | 3.5.0", "PlutoVG | 1.3.1")) {
    if (-not $notices.Contains($component)) {
        throw "В THIRD-PARTY-NOTICES.md отсутствует актуальная запись: $component"
    }
}

$packageScript = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "tools\build\package-portable.ps1")
foreach ($path in @("THIRD-PARTY-NOTICES.md", "Scintilla-Lexilla.txt", "PCRE2.txt", "Hunspell.txt", "Hunspell-MySpell.txt", "libwebp.txt", "OpenJPEG.txt", "libheif.txt", "libde265.txt", "libaom.txt", "libaom-PATENTS.txt", "LunaSVG.txt", "PlutoVG.txt", "Theme-palettes-MIT.txt", "UAC.txt")) {
    if (-not $packageScript.Contains($path)) {
        throw "package-portable.ps1 не добавляет лицензионный файл: $path"
    }
}

foreach ($path in @("runtime\copying.txt", "runtime\Themes\licenses\MIT.txt", "runtime\Themes\licenses\THIRD_PARTY_NOTICES.txt")) {
    if (Test-Path -LiteralPath (Join-Path $repoRoot $path) -PathType Leaf) {
        throw "Устаревшая дублирующая лицензия должна быть удалена из runtime: $path"
    }
}
if (Test-Path -LiteralPath (Join-Path $repoRoot "runtime\Themes\licenses")) {
    throw "Устаревший каталог runtime\\Themes\\licenses должен быть удалён."
}

$fbeProject = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "src\fbe\FBE.vcxproj")
foreach ($obsoleteOutput in @("copying.txt", "ThirdPartyNotices.txt", "Themes\licenses")) {
    if (-not $fbeProject.Contains($obsoleteOutput)) {
        throw "FBE.vcxproj не очищает устаревший лицензионный артефакт: $obsoleteOutput"
    }
}

$installerScript = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "packaging\nsis\Installer\MakeInstaller.nsi")
foreach ($path in @("LICENSE", "NOTICE", "THIRD-PARTY-NOTICES.md", "THIRD-PARTY-LICENSES")) {
    if (-not $installerScript.Contains($path)) {
        throw "Установщик не включает единый лицензионный материал: $path"
    }
}
if (-not $packageScript.Contains('Remove-Item -LiteralPath $legacyEnglishGplPath -Force')) {
    throw "Portable-пакет должен оставлять LICENSE единственной английской копией GPL."
}

Write-Host "Проверка реестра лицензий и состава portable-пакета прошла успешно."
