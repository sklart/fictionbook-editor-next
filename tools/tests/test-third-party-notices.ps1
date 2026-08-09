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
foreach ($component in @("Scintilla | 5.6.5", "Lexilla | 5.5.2", "PCRE2 | 10.47", "Hunspell | 1.7.3", "Windows Template Library (WTL) | 10.01", "LunaSVG | 3.5.0", "PlutoVG | 1.3.1")) {
    if (-not $notices.Contains($component)) {
        throw "В THIRD-PARTY-NOTICES.md отсутствует актуальная запись: $component"
    }
}

$packageScript = Get-Content -Raw -LiteralPath (Join-Path $repoRoot "tools\build\package-portable.ps1")
foreach ($path in @("THIRD-PARTY-NOTICES.md", "Scintilla-Lexilla.txt", "PCRE2.txt", "Hunspell.txt", "LunaSVG.txt", "PlutoVG.txt", "UAC.txt")) {
    if (-not $packageScript.Contains($path)) {
        throw "package-portable.ps1 не добавляет лицензионный файл: $path"
    }
}

Write-Host "Проверка реестра лицензий и состава portable-пакета прошла успешно."
