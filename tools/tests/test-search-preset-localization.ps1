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
Write-Host "Search preset localization contract passed ($($ids.Count) built-ins, $($keys.Count) strings)."
