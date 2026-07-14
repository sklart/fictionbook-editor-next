<#
.SYNOPSIS
Генерирует NSIS-секции языковых компонентов установщика.

.DESCRIPTION
Скрипт читает `localization/language-packs.json` и создаёт подключаемый
`packaging/nsis/Installer/Generated/LanguagePacks.generated.nsh`. Файл
описывает выборочную установку runtime JSON и MUI-ресурсов интерфейса.
Словари остаются отдельными компонентами установщика.
#>
[CmdletBinding()]
param(
    [string]$OutputPath
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $repoRoot "packaging\nsis\Installer\Generated\LanguagePacks.generated.nsh"
}

$inventoryPath = Join-Path $repoRoot "localization\language-packs.json"
$inventory = Get-Content -Raw -LiteralPath $inventoryPath | ConvertFrom-Json -Depth 30

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("; Языковые компоненты FictionBook Editor Next.")
$lines.Add("; Сгенерировано из localization/language-packs.json.")
$lines.Add("; Не редактируйте вручную: запускайте tools/localization/export-nsis-language-pack-plan.ps1.")
$lines.Add("")
$lines.Add("; Fallback-язык: $($inventory.fallbackLanguage)")
$lines.Add("; Текущие языки интерфейса установщика: $(@($inventory.currentInstallerLanguages) -join ', ')")
$lines.Add("")
$lines.Add('SectionGroup $(LanguagePacksGroup) LanguagePacksGroup_id')

foreach ($language in @($inventory.languages)) {
    $flags = @()
    if (-not $language.required -and -not $language.defaultInstall) { $flags += "/o" }

    $flagText = if ($flags.Count -gt 0) { ($flags -join " ") + " " } else { "" }
    $sectionName = "LanguagePack_$($language.language -replace '[^A-Za-z0-9]', '_')"
    $lines.Add("")
    $lines.Add(('  Section {0}"{1} ({2})" {3}' -f $flagText, $language.displayName, $language.language, $sectionName))
    $lines.Add("    ; required=$($language.required); defaultInstall=$($language.defaultInstall); installerLanguage=$($language.installerLanguage)")
    if ($language.required) {
        $lines.Add("    SectionIn RO")
    }

    foreach ($groupName in @("fbeResources", "fbvMui")) {
        $group = $language.assets.PSObject.Properties[$groupName]
        if ($null -eq $group) { continue }
        foreach ($asset in @($group.Value)) {
            $normalized = ([string]$asset).Replace('/', '\')
            $targetDir = '$INSTDIR'
            $fileName = [IO.Path]::GetFileName($normalized)
            $relativeDir = [IO.Path]::GetDirectoryName($normalized)
            if (-not [string]::IsNullOrWhiteSpace($relativeDir)) {
                $targetDir = '$INSTDIR\' + $relativeDir
            }
            $lines.Add(('    SetOutPath "{0}"' -f $targetDir))
            $lines.Add('    File /nonfatal "${INPUTDIR}\' + $normalized + '"')
        }
    }

    $lines.Add(('    SetOutPath "$INSTDIR\Lang\{0}"' -f $language.language))
    $lines.Add(('    File /nonfatal /r "${{INPUTDIR}}\Lang\{0}\*.*"' -f $language.language))

    $lines.Add("  SectionEnd")
}

$lines.Add("")
$lines.Add("SectionGroupEnd")

[IO.File]::WriteAllText($outputPath, ($lines -join "`n") + "`n", [Text.UTF8Encoding]::new($false))

Write-Host "NSIS-секции языковых пакетов подготовлены."
Write-Host "  Файл: $outputPath"
Write-Host "  Языков: $(@($inventory.languages).Count)"
