<#
.SYNOPSIS
Early localization preflight with a ratchet baseline for permitted legacy debt.
#>
[CmdletBinding()]
param(
    [string]$RepositoryRoot,
    [switch]$SkipStaticTests
)

$ErrorActionPreference = 'Stop'
$repoRoot = if ($RepositoryRoot) { (Resolve-Path -LiteralPath $RepositoryRoot).Path } else { (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path }
$productionLanguages = @('en-US','ru-RU','uk-UA','de-DE','fr-FR','es-ES','it-IT','pl-PL','pt-PT','nl-NL','cs-CZ','bg-BG')
$productionCatalogs = @(
    'localization\app-ui\catalog.json',
    'localization\app-ui\fbe-idr-mainframe-menu.json',
    'localization\app-ui\fbe-secondary-menus.json',
    'localization\app-ui\fbe-small-dialogs.json',
    'localization\plugin-ui\catalog.json'
)
$baselineRelativePath = 'localization\incomplete-translations-baseline.json'
$baselineAllowedCatalogs = @('localization\app-ui\fbe-small-dialogs.json')

function Test-StrictJson([string]$Path) {
    $pythonLauncher = Get-Command py.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -First 1
    if (-not $pythonLauncher) { throw 'Для strict JSON preflight требуется Python launcher (py.exe).' }
    $program = 'import json,sys; json.load(open(sys.argv[1],encoding=''utf-8-sig''), object_pairs_hook=lambda p: (lambda d: d if len(d)==len(p) else (_ for _ in ()).throw(ValueError(''duplicate key'')))(dict(p)))'
    & $pythonLauncher -3 -c $program $Path 2>&1 | Out-String | ForEach-Object { if ($_ -match '\S') { Write-Host $_.TrimEnd() } }
    return $LASTEXITCODE -eq 0
}
function Get-Entries($Catalog) {
    if ($Catalog.strings) { return @($Catalog.strings.PSObject.Properties) }
    if ($Catalog.seedStrings) { return @($Catalog.seedStrings.PSObject.Properties) }
    return @()
}
function Add-Issue($List, [string]$Catalog, [string]$Key, [string]$Language, [string]$State) {
    $List.Add([pscustomobject]@{ Catalog=$Catalog; Key=$Key; Language=$Language; State=$State })
}
function Get-PairId([string]$Catalog, [string]$Key, [string]$Language) { return ($Catalog, $Key, $Language -join '|') }
function Get-FormatPlaceholders($Text) {
    if ($null -eq $Text) { return @() }
    # C/Win32 printf placeholders. %% is a literal percent and intentionally excluded.
    return @([regex]::Matches([string]$Text, '(?<!%)%(?:\d+\$)?[-+ #0]*\d*(?:\.\d+)?[diuoxXfFeEgGaAcCsSp]') | ForEach-Object Value | Sort-Object)
}
function Test-SameFormatPlaceholders($Source, $Translation) {
    return ((Get-FormatPlaceholders $Source) -join '|') -ceq ((Get-FormatPlaceholders $Translation) -join '|')
}
function Write-GroupedIssues([string]$Title, $Items) {
    Write-Host $Title
    if (-not $Items.Count) { Write-Host '  0'; return }
    foreach ($catalog in @($Items | Group-Object Catalog | Sort-Object Name)) {
        Write-Host "  $($catalog.Name):"
        foreach ($entry in @($catalog.Group | Group-Object Key | Sort-Object Name)) {
            Write-Host "    $($entry.Name):"
            foreach ($detail in @($entry.Group | Sort-Object Language, State)) { Write-Host "      $($detail.Language) — $($detail.State)" }
        }
    }
}

$structuralIssues = [Collections.Generic.List[object]]::new()
$actualDebt = [Collections.Generic.List[object]]::new()
$baselinePath = Join-Path $repoRoot $baselineRelativePath
$baselinePairs = @{}
if (-not (Test-Path -LiteralPath $baselinePath -PathType Leaf)) {
    Add-Issue $structuralIssues $baselineRelativePath '(baseline)' '(baseline)' 'missing file'
}
elseif (-not (Test-StrictJson $baselinePath)) {
    Add-Issue $structuralIssues $baselineRelativePath '(baseline)' '(baseline)' 'invalid JSON or duplicate key'
}
else {
    $baseline = Get-Content -Raw -Encoding UTF8 -LiteralPath $baselinePath | ConvertFrom-Json
    if ($baseline.formatVersion -ne 1 -or -not $baseline.catalogs) { Add-Issue $structuralIssues $baselineRelativePath '(baseline)' '(baseline)' 'invalid baseline schema' }
    else {
        foreach ($catalogProperty in @($baseline.catalogs.PSObject.Properties)) {
            $catalog = [string]$catalogProperty.Name
            if ($catalog -notin $baselineAllowedCatalogs) { Add-Issue $structuralIssues $baselineRelativePath $catalog '(catalog)' 'baseline catalog is not allowed' }
            foreach ($keyProperty in @($catalogProperty.Value.PSObject.Properties)) {
                $key = [string]$keyProperty.Name
                $languages = @($keyProperty.Value)
                if ($languages.Count -eq 0) { Add-Issue $structuralIssues $baselineRelativePath $key '(all)' 'baseline language list is empty' }
                foreach ($duplicate in @($languages | Group-Object | Where-Object Count -gt 1)) { Add-Issue $structuralIssues $baselineRelativePath $key $duplicate.Name 'baseline language duplicate' }
                foreach ($language in $languages) {
                    if ($language -notin $productionLanguages) { Add-Issue $structuralIssues $baselineRelativePath $key $language 'baseline language is unsupported'; continue }
                    $id = Get-PairId $catalog $key $language
                    if ($baselinePairs.ContainsKey($id)) { Add-Issue $structuralIssues $baselineRelativePath $key $language 'baseline pair duplicate' }
                    else { $baselinePairs[$id] = $true }
                }
            }
        }
    }
}

foreach ($relativePath in $productionCatalogs) {
    $path = Join-Path $repoRoot $relativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { Add-Issue $structuralIssues $relativePath '(catalog)' '(catalog)' 'missing file'; continue }
    if (-not (Test-StrictJson $path)) { Add-Issue $structuralIssues $relativePath '(catalog)' '(catalog)' 'invalid JSON or duplicate key'; continue }
    $catalog = Get-Content -Raw -Encoding UTF8 -LiteralPath $path | ConvertFrom-Json
    $languages = @($catalog.targetLanguages)
    if (-not $catalog.PSObject.Properties['targetLanguages']) { Add-Issue $structuralIssues $relativePath '(catalog)' '(catalog)' 'missing targetLanguages' }
    foreach ($language in $productionLanguages) { if ($languages -notcontains $language) { Add-Issue $structuralIssues $relativePath '(catalog)' $language 'target language missing' } }
    foreach ($duplicate in @($languages | Group-Object | Where-Object Count -gt 1)) { Add-Issue $structuralIssues $relativePath '(catalog)' $duplicate.Name 'target language duplicate' }
    $entries = Get-Entries $catalog
    if ($entries.Count -eq 0) { Add-Issue $structuralIssues $relativePath '(catalog)' '(catalog)' 'missing strings' }
    foreach ($entryProperty in $entries) {
        $key = [string]$entryProperty.Name; $entry = $entryProperty.Value
        if (-not $entry.PSObject.Properties['translations']) { Add-Issue $structuralIssues $relativePath $key '(all)' 'translations missing'; continue }
        if ($entry.needsTranslation -or $entry.fallback) { Add-Issue $structuralIssues $relativePath $key '(all)' 'production fallback/needsTranslation is forbidden' }
        $sourceTranslation = $entry.translations.PSObject.Properties['en-US']
        foreach ($language in $productionLanguages) {
            $translation = $entry.translations.PSObject.Properties[$language]
            if (-not $translation) { $actualDebt.Add([pscustomobject]@{ Catalog=$relativePath; Key=$key; Language=$language; State='missing' }) }
            elseif ($null -eq $translation.Value -or [string]::IsNullOrWhiteSpace([string]$translation.Value)) { $actualDebt.Add([pscustomobject]@{ Catalog=$relativePath; Key=$key; Language=$language; State='empty' }) }
            elseif ($sourceTranslation -and -not (Test-SameFormatPlaceholders $sourceTranslation.Value $translation.Value)) { Add-Issue $structuralIssues $relativePath $key $language 'format placeholder set differs from en-US' }
        }
    }
}

$installerPath = Join-Path $repoRoot 'localization\installer-ui\catalog.json'
if (-not (Test-StrictJson $installerPath)) { Add-Issue $structuralIssues 'localization\installer-ui\catalog.json' '(catalog)' '(catalog)' 'invalid JSON or duplicate key' }
else {
    $installer = Get-Content -Raw -Encoding UTF8 -LiteralPath $installerPath | ConvertFrom-Json
    $languages = @($installer.targetLanguages)
    foreach ($language in $productionLanguages) { if ($languages -notcontains $language) { Add-Issue $structuralIssues 'localization\installer-ui\catalog.json' '(catalog)' $language 'target language missing' } }
    foreach ($duplicate in @($languages | Group-Object | Where-Object Count -gt 1)) { Add-Issue $structuralIssues 'localization\installer-ui\catalog.json' '(catalog)' $duplicate.Name 'target language duplicate' }
    if (($languages -join '|') -ne ($productionLanguages -join '|')) { Add-Issue $structuralIssues 'localization\installer-ui\catalog.json' '(catalog)' '(catalog)' 'targetLanguages differ from production catalog list' }
    foreach ($entryProperty in Get-Entries $installer) {
        $key = [string]$entryProperty.Name; $entry = $entryProperty.Value
        if (-not $entry.PSObject.Properties['translations']) { Add-Issue $structuralIssues 'localization\installer-ui\catalog.json' $key '(all)' 'translations missing'; continue }
        if ($entry.fallback -and [string]$entry.fallback -ne 'en-US') { Add-Issue $structuralIssues 'localization\installer-ui\catalog.json' $key '(all)' 'unsupported fallback (only en-US is allowed)' }
        foreach ($language in @('en-US','ru-RU','uk-UA')) {
            $translation = $entry.translations.PSObject.Properties[$language]
            if (-not $translation) { Add-Issue $structuralIssues 'localization\installer-ui\catalog.json' $key $language 'missing' }
            elseif ($null -eq $translation.Value -or [string]::IsNullOrWhiteSpace([string]$translation.Value)) { Add-Issue $structuralIssues 'localization\installer-ui\catalog.json' $key $language 'empty' }
        }
    }
}

$knownDebt = [Collections.Generic.List[object]]::new()
$newMissing = [Collections.Generic.List[object]]::new()
$actualPairIds = @{}
foreach ($debt in $actualDebt) {
    $id = Get-PairId $debt.Catalog $debt.Key $debt.Language; $actualPairIds[$id] = $true
    if ($debt.State -eq 'missing' -and $baselinePairs.ContainsKey($id)) { $knownDebt.Add($debt) }
    else { $newMissing.Add($debt) }
}
$staleBaseline = [Collections.Generic.List[object]]::new()
foreach ($id in $baselinePairs.Keys) {
    if (-not $actualPairIds.ContainsKey($id)) {
        $parts = $id -split '\|', 3
        $staleBaseline.Add([pscustomobject]@{ Catalog=$parts[0]; Key=$parts[1]; Language=$parts[2]; State='translation is now present' })
    }
}
$knownKeyCount = @($knownDebt | ForEach-Object { ($_.Catalog, $_.Key -join '|') } | Sort-Object -Unique).Count
$failed = $structuralIssues.Count -gt 0 -or $newMissing.Count -gt 0 -or $staleBaseline.Count -gt 0
if ($failed) {
    Write-Host 'Localization preflight FAILED'; Write-Host ''
    Write-GroupedIssues 'STRUCTURAL localization errors:' $structuralIssues
    Write-GroupedIssues 'NEW missing translations:' $newMissing
    Write-GroupedIssues 'STALE baseline entries:' $staleBaseline
    Write-Host "Known translation debt: $knownKeyCount keys / $($knownDebt.Count) translations"
    exit 1
}
Write-Host 'Localization preflight PASS'
Write-Host "Known translation debt: $knownKeyCount keys / $($knownDebt.Count) translations"
Write-Host 'New missing translations: 0'
Write-Host 'Stale baseline entries: 0'

$staticTests = @('test-plugin-localization-catalog.ps1','test-fbv-localization-resources.ps1','test-fbe-property-schema-localization.ps1','test-export-html-localization-resources.ps1','test-export-docx-localization-resources.ps1','test-export-epub-localization-resources.ps1','test-import-epub-localization-resources.ps1','test-localization-export.ps1','test-localization-runtime-contract.ps1','test-runtime-lang-export.ps1','test-fbe-runtime-lang-overlay.ps1','test-fbv-runtime-lang-overlay.ps1','test-export-html-runtime-lang-overlay.ps1','test-export-docx-runtime-lang-overlay.ps1','test-export-epub-runtime-lang-overlay.ps1','test-import-epub-runtime-lang-overlay.ps1','test-search-preset-localization.ps1','test-regex-help-runtime-localization.ps1','test-fbe-binary-editor-localization.ps1','test-product-hardcoded-cyrillic-audit.ps1')
if (-not $SkipStaticTests) { foreach ($test in $staticTests) { & (Join-Path $PSScriptRoot $test); if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE } } }
