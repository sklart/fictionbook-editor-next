# Экспортирует runtime-локализацию в JSON-файлы Lang/<язык>/<модуль>.json.
# Эти файлы являются будущим внешним слоем локализации поверх встроенных ресурсов.
[CmdletBinding()]
param(
    [string] $RepositoryRoot,
    [string] $OutputDirectory,
    [switch] $Clean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}
else {
    $RepositoryRoot = (Resolve-Path $RepositoryRoot).Path
}

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $RepositoryRoot 'out\localization\Lang'
}

function Read-JsonFile {
    param([Parameter(Mandatory = $true)][string] $Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "JSON-файл не найден: $Path"
    }
    return Get-Content -Raw -LiteralPath $Path -Encoding UTF8 | ConvertFrom-Json -AsHashtable
}

function Get-Translation {
    param(
        [Parameter(Mandatory = $true)][hashtable] $Entry,
        [Parameter(Mandatory = $true)][string] $Language,
        [Parameter(Mandatory = $true)][string] $FallbackLanguage
    )

    $translations = $Entry['translations']
    if ($translations -is [hashtable]) {
        if ($translations.ContainsKey($Language) -and -not [string]::IsNullOrWhiteSpace([string] $translations[$Language])) {
            return [string] $translations[$Language]
        }
        if ($translations.ContainsKey($FallbackLanguage) -and -not [string]::IsNullOrWhiteSpace([string] $translations[$FallbackLanguage])) {
            return [string] $translations[$FallbackLanguage]
        }
    }

    if ($Entry.ContainsKey('source') -and -not [string]::IsNullOrWhiteSpace([string] $Entry['source'])) {
        return [string] $Entry['source']
    }

    throw "У строки локализации нет перевода и fallback: $($Entry | ConvertTo-Json -Compress)"
}

function Get-ModuleName {
    param(
        [Parameter(Mandatory = $true)][string] $Key,
        [AllowNull()][object] $Component
    )

    $componentText = if ($null -ne $Component) { [string] $Component } else { '' }
    $probe = if (-not [string]::IsNullOrWhiteSpace($componentText)) { $componentText } else { $Key }

    if ($probe -like 'fbe.*' -or $probe -eq 'fbe') { return 'fbe' }
    if ($probe -like 'fbv.*' -or $probe -eq 'fbv') { return 'fbv' }
    if ($probe -like 'export-html*' -or $probe -like 'export_html*') { return 'export-html' }
    if ($probe -like 'export-docx*' -or $probe -like 'export_docx*') { return 'export-docx' }
    if ($probe -like 'export-epub*' -or $probe -like 'export_epub*') { return 'export-epub' }
    if ($probe -like 'import-epub*' -or $probe -like 'import_epub*') { return 'import-epub' }
    if ($probe -eq 'common' -or $Key -like 'common.*') { return 'common' }
    if ($probe -like 'installer.*') { return $null }

    throw "Не удалось определить runtime-модуль для ключа '$Key' (component='$componentText')."
}

$contractPath = Join-Path $RepositoryRoot 'localization\runtime\contract.json'
$appCatalogPath = Join-Path $RepositoryRoot 'localization\app-ui\catalog.json'
$appMainMenuCatalogPath = Join-Path $RepositoryRoot 'localization\app-ui\fbe-idr-mainframe-menu.json'
$appSecondaryMenuCatalogPath = Join-Path $RepositoryRoot 'localization\app-ui\fbe-secondary-menus.json'
$appSmallDialogsCatalogPath = Join-Path $RepositoryRoot 'localization\app-ui\fbe-small-dialogs.json'
$pluginCatalogPath = Join-Path $RepositoryRoot 'localization\plugin-ui\catalog.json'

$contract = Read-JsonFile $contractPath
$appCatalog = Read-JsonFile $appCatalogPath
$appMainMenuCatalog = Read-JsonFile $appMainMenuCatalogPath
$appSecondaryMenuCatalog = Read-JsonFile $appSecondaryMenuCatalogPath
$appSmallDialogsCatalog = Read-JsonFile $appSmallDialogsCatalogPath
$pluginCatalog = Read-JsonFile $pluginCatalogPath

$fallbackLanguage = [string] $contract['fallbackLanguage']
$moduleFiles = @{}
foreach ($moduleEntry in @($contract['modules'])) {
    $moduleFiles[[string] $moduleEntry['module']] = [string] $moduleEntry['file']
}
$modules = @($moduleFiles.Keys | Sort-Object)
$languages = @($appCatalog['targetLanguages'])
foreach ($language in @($pluginCatalog['targetLanguages'])) {
    if ($languages -notcontains $language) {
        $languages += $language
    }
}
$languages = @($languages | Sort-Object)

if ($languages -notcontains $fallbackLanguage) {
    throw "Fallback-язык '$fallbackLanguage' отсутствует в списке языков."
}

if ($Clean -and (Test-Path -LiteralPath $OutputDirectory)) {
    Remove-Item -LiteralPath $OutputDirectory -Recurse -Force
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

$moduleStrings = @{}
foreach ($module in $modules) {
    $moduleStrings[$module] = @{}
}
$commonStrings = @{}

foreach ($catalog in @($appCatalog, $appMainMenuCatalog, $appSecondaryMenuCatalog, $appSmallDialogsCatalog, $pluginCatalog)) {
    $strings = if ($catalog.ContainsKey('seedStrings')) { $catalog['seedStrings'] } else { $catalog['strings'] }
    foreach ($key in @($strings.Keys | Sort-Object)) {
        $entry = $strings[$key]
        $component = if ($entry.ContainsKey('component')) { $entry['component'] } else { $null }
        $module = Get-ModuleName -Key $key -Component $component
        if ($null -eq $module) {
            continue
        }
        if ($module -eq 'common') {
            $commonStrings[$key] = $entry
            continue
        }
        if (-not $moduleStrings.ContainsKey($module)) {
            throw "Ключ '$key' относится к модулю '$module', которого нет в contract.json."
        }
        $moduleStrings[$module][$key] = $entry
    }
}

foreach ($language in $languages) {
    $languageDir = Join-Path $OutputDirectory $language
    New-Item -ItemType Directory -Path $languageDir -Force | Out-Null

    foreach ($module in $modules) {
        $stringsForModule = [ordered] @{}
        foreach ($key in @($commonStrings.Keys | Sort-Object)) {
            if ($module -in @('export-html', 'export-docx', 'export-epub', 'import-epub')) {
                $stringsForModule[$key] = Get-Translation -Entry $commonStrings[$key] -Language $language -FallbackLanguage $fallbackLanguage
            }
        }
        foreach ($key in @($moduleStrings[$module].Keys | Sort-Object)) {
            $stringsForModule[$key] = Get-Translation -Entry $moduleStrings[$module][$key] -Language $language -FallbackLanguage $fallbackLanguage
        }

        $payload = [ordered] @{
            formatVersion = 1
            module = $module
            locale = $language
            fallbackLocale = $fallbackLanguage
            strings = $stringsForModule
        }

        $outputPath = Join-Path $languageDir $moduleFiles[$module]
        $payload | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $outputPath -Encoding UTF8
    }
}

Write-Host "Runtime JSON-локализация экспортирована."
Write-Host "  Каталог: $OutputDirectory"
Write-Host "  Языков: $($languages.Count)"
Write-Host "  Модулей: $($modules.Count)"
