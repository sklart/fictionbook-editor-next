<#
.SYNOPSIS
Генерирует черновой NSIS-план будущих языковых компонентов установщика.

.DESCRIPTION
Скрипт читает `localization/language-packs.json` и создаёт в
`out/localization/nsis-language-packs` справочный `.nsh`-файл с будущими
секциями языковых пакетов. Файл намеренно не подключается к `MakeInstaller.nsi`:
это промежуточный артефакт для ревизии структуры перед изменением поведения
установщика.
#>
[CmdletBinding()]
param(
    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot "out\localization\nsis-language-packs"
}

$inventoryPath = Join-Path $repoRoot "localization\language-packs.json"
$inventory = Get-Content -Raw -LiteralPath $inventoryPath | ConvertFrom-Json -Depth 30

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("; Черновой план языковых компонентов FictionBook Editor Next.")
$lines.Add("; Сгенерировано из localization/language-packs.json.")
$lines.Add("; Этот файл пока НЕ подключается к MakeInstaller.nsi.")
$lines.Add("; Он нужен для ревизии будущего разбиения языков на компоненты.")
$lines.Add("")
$lines.Add("; Fallback-язык: $($inventory.fallbackLanguage)")
$lines.Add("; Текущие языки интерфейса установщика: $(@($inventory.currentInstallerLanguages) -join ', ')")
$lines.Add("")
$lines.Add("; Language-neutral ресурсы, которые нужны нескольким языкам.")
if ($inventory.languageNeutralAssets) {
    foreach ($group in $inventory.languageNeutralAssets.PSObject.Properties) {
        foreach ($asset in @($group.Value)) {
            $normalized = ([string]$asset).Replace('/', '\')
            $targetDir = '$INSTDIR'
            $relativeDir = [IO.Path]::GetDirectoryName($normalized)
            if (-not [string]::IsNullOrWhiteSpace($relativeDir)) {
                $targetDir = '$INSTDIR\' + $relativeDir
            }
            $lines.Add(('SetOutPath "{0}"' -f $targetDir))
            $lines.Add('File /nonfatal "${INPUTDIR}\' + $normalized + '"')
        }
    }
}

$lines.Add("")
$lines.Add(('SectionGroup /e "{0}" LanguagePacksGroup_id' -f "Языки интерфейса и проверки FB2"))

foreach ($language in @($inventory.languages)) {
    $flags = @()
    if ($language.required) { $flags += "RO" }
    elseif (-not $language.defaultInstall) { $flags += "/o" }

    $flagText = if ($flags.Count -gt 0) { ($flags -join " ") + " " } else { "" }
    $sectionName = "LanguagePack_$($language.language -replace '[^A-Za-z0-9]', '_')"
    $lines.Add("")
    $lines.Add(('  Section {0}"{1} ({2})" {3}' -f $flagText, $language.displayName, $language.language, $sectionName))
    $lines.Add("    ; required=$($language.required); defaultInstall=$($language.defaultInstall); installerLanguage=$($language.installerLanguage)")

    foreach ($group in $language.assets.PSObject.Properties) {
        if ($group.Name -eq "futureTemplates") {
            foreach ($asset in @($group.Value)) {
                $lines.Add("    ; TODO futureTemplates: $asset")
            }
            continue
        }

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

    $lines.Add("  SectionEnd")
}

$lines.Add("")
$lines.Add("SectionGroupEnd")

$outputPath = Join-Path $OutputDirectory "FictionBookEditorNext.LanguagePacks.draft.nsh"
[IO.File]::WriteAllText($outputPath, ($lines -join "`n") + "`n", [Text.UTF8Encoding]::new($false))

Write-Host "Черновой NSIS-план языковых пакетов подготовлен."
Write-Host "  Файл: $outputPath"
Write-Host "  Языков: $(@($inventory.languages).Count)"
