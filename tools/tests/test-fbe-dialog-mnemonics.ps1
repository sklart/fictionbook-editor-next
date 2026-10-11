[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$catalogPath = Join-Path $PSScriptRoot '..\..\localization\app-ui\fbe-small-dialogs.json'
$catalog = Get-Content -Raw -Encoding UTF8 -LiteralPath $catalogPath | ConvertFrom-Json
$seen = @{}
foreach ($entry in $catalog.strings.PSObject.Properties) {
    $item = $entry.Value
    if (-not $entry.Name.StartsWith('fbe.dialog.idd_', [StringComparison]::Ordinal) -or
        -not $item.targetId -or $item.kind -in @('TOOLTIP', 'VALUE', 'CAPTION')) { continue }
    foreach ($language in $catalog.targetLanguages) {
        $value = [string]$item.translations.$language
        $mnemonics = [Collections.Generic.List[string]]::new()
        for ($index = 0; $index -lt $value.Length; ++$index) {
            if ($value[$index] -ne '&') { continue }
            if ($index + 1 -ge $value.Length) { throw "Trailing &: $($entry.Name), $language" }
            ++$index
            if ($value[$index] -eq '&') { continue }
            if (-not [char]::IsLetterOrDigit($value[$index])) { throw "Invalid mnemonic: $($entry.Name), $language" }
            $mnemonics.Add(([string]$value[$index]).ToLowerInvariant())
        }
        if ($mnemonics.Count -gt 1) { throw "Multiple mnemonics: $($entry.Name), $language" }
        if ($mnemonics.Count -eq 0) { continue }
        $id = "$($item.resource)|$language|$($mnemonics[0])"
        if ($seen.ContainsKey($id)) { throw "Duplicate mnemonic $($mnemonics[0]) in $($item.resource)/$language`: $($seen[$id]) and $($entry.Name)" }
        $seen[$id] = $entry.Name
    }
}
Write-Host "Dialog mnemonics checked in $(@($catalog.targetLanguages).Count) locales."
