<# Ensures every built-in Search/Replace template and category can be rendered in every shipped UI language. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$catalog = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'localization\app-ui\fbe-small-dialogs.json') | ConvertFrom-Json
$source = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'src\fbe\search\SearchPresetCatalog.cpp')
$ids = [regex]::Matches($source, 'PRESET\(([a-z0-9_]+),') | ForEach-Object { $_.Groups[1].Value }
$categories = @('whitespace','punctuation','typography','dashes_numbers','ocr','proofreading','names','xml_formatting','fb2_structure','links_notes','import_artifacts','diagnostics')
$keys = @('fbe.search_preset.review_only')
$legacyLocalizationIds = @{
    design_normalize_spaces = 'normalize_spaces'
    design_trim_before_punctuation = 'trim_before_punctuation'
    design_trim_leading = 'trim_leading'
    design_trim_trailing = 'trim_trailing'
    design_tabs_to_spaces = 'tabs_to_spaces'
    design_nbsp_to_space = 'nbsp_to_space'
    design_duplicate_word = 'duplicate_word'
    design_repeated_punctuation = 'repeated_punctuation'
}
foreach($id in $ids) {
    $localizedId = if($legacyLocalizationIds.ContainsKey($id)) { $legacyLocalizationIds[$id] } else { $id }
    $keys += "fbe.search_preset.$localizedId.name", "fbe.search_preset.$localizedId.description"
}
foreach($category in $categories) { $keys += "fbe.search_preset.category.$category" }
foreach($key in $keys | Select-Object -Unique) {
    $entry = $catalog.strings.PSObject.Properties[$key]
    if($null -eq $entry) { throw "Missing preset localization key: $key" }
    foreach($language in $catalog.targetLanguages) {
        if([string]::IsNullOrWhiteSpace([string]$entry.Value.translations.$language)) { throw "Empty $language translation for $key" }
    }
}
$expectedRussianCategories = @{
    whitespace = 'Пробелы и отступы'; punctuation = 'Пунктуация'; typography = 'Типографика'; proofreading = 'Вычитка';
    ocr = 'OCR и распознавание'; dashes_numbers = 'Тире и числа'; names = 'Имена и сокращения'; xml_formatting = 'Форматирование XML';
    diagnostics = 'Диагностика'; fb2_structure = 'Структура FB2'; links_notes = 'Ссылки и сноски'; import_artifacts = 'Артефакты импорта'
}
foreach ($category in $categories) {
    $translations = $catalog.strings."fbe.search_preset.category.$category".translations
    if ($translations.'ru-RU' -ne $expectedRussianCategories[$category]) { throw "Russian category label is not canonical: $category" }
    $english = [string]$translations.'en-US'
    foreach ($language in $catalog.targetLanguages) {
        $label = [string]$translations.$language
        if ($label -match '^(Search template|Шаблон поиска|Шаблон пошуку|Suchvorlage|Modèle de recherche|Plantilla de búsqueda|Modello di ricerca|Szablon wyszukiwania|Modelo de pesquisa|Zoeksjabloon|Vyhledávací šablona|Шаблон за търсене)') { throw "Category label still has a template prefix: $language/$category" }
        if ($language -ne 'en-US' -and $label -eq $english) { throw "Category label was not translated: $language/$category" }
    }
}Write-Host "Search preset localization contract passed ($($ids.Count) built-ins, $($keys.Count) strings)."
