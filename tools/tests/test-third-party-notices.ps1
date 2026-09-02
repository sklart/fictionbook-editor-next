[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

foreach ($path in @("LICENSE", "NOTICE", "THIRD-PARTY-NOTICES.md", "THIRD-PARTY-LICENSES\README.md", "THIRD-PARTY-LICENSES\WTL-MS-PL.txt", "THIRD-PARTY-LICENSES\Dictionary-de_DE.txt", "THIRD-PARTY-LICENSES\Dictionary-en_US.txt", "THIRD-PARTY-LICENSES\Dictionary-ru_RU.txt", "THIRD-PARTY-LICENSES\Dictionary-uk_UA.txt")) {
    if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $path) -PathType Leaf)) {
        throw "Отсутствует обязательный лицензионный документ: $path"
    }
}

$notices = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "THIRD-PARTY-NOTICES.md")
foreach ($component in @("Scintilla | 5.6.6", "Lexilla | 5.5.3", "PCRE2 | 10.48", "Hunspell | 1.7.3", "English Speller Database / SCOWL | 2026.02.25", "Goudron Russian Hunspell Dictionary | 1.0.8", "VESUM / dict_uk | 6.8.5", "libwebp | 1.6.0", "OpenJPEG | 2.5.4", "libheif | 1.23.3", "libde265 | 1.1.1", "libaom | 3.15.0", "Windows Template Library (WTL) | 10.01", "LunaSVG | 3.5.0", "PlutoVG | 1.3.3")) {
    if (-not $notices.Contains($component)) {
        throw "В THIRD-PARTY-NOTICES.md отсутствует актуальная запись: $component"
    }
}

$stageCoreScript = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "tools\build\stage-core.ps1")
$manifest = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "packaging\package-manifest.json")
foreach ($path in @("THIRD-PARTY-NOTICES.md", "Dictionary-de_DE.txt", "Dictionary-en_US.txt", "Dictionary-ru_RU.txt", "Dictionary-uk_UA.txt")) {
    if (-not $stageCoreScript.Contains($path) -and -not $manifest.Contains($path)) {
        throw "Core contract не требует лицензионный файл: $path"
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
Write-Host "Проверка реестра лицензий и состава portable-пакета прошла успешно."
