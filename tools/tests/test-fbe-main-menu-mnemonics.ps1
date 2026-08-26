<# Проверяет локализованные мнемоники непосредственно в runtime JSON catalog. #>
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$catalog = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'localization\app-ui\fbe-idr-mainframe-menu.json') | ConvertFrom-Json
foreach ($entry in $catalog.strings.PSObject.Properties) {
    if ($entry.Value.kind -ne 'POPUP') { continue }
    foreach ($locale in $catalog.targetLanguages) {
        $text = [string]$entry.Value.translations.PSObject.Properties[$locale].Value
        if ($text -notmatch '(?<!&)&(?!&)') { throw "У POPUP $($entry.Name) нет mnemonic для $locale." }
    }
}
Write-Host 'Runtime menu mnemonic validation passed.'
