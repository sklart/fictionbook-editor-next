<# Requires a concrete translation for every production Find/Replace template
   and regex-help string in every supported runtime locale. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$catalog = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'localization\app-ui\fbe-small-dialogs.json') | ConvertFrom-Json
$keys = [System.Collections.Generic.HashSet[string]]::new()

foreach($path in @('src\fbe\SearchReplace.h', 'src\fbe\search\SearchPresetCatalog.cpp', 'src\fbe\search\RegexQuickReference.h', 'src\fbe\search\ui\RegexHelpLauncher.cpp', 'src\fbe\search\ui\RegexQuickReferencePopup.cpp')) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $root $path)
    foreach($match in [regex]::Matches($text, 'L"(fbe\.(?:search_preset|regex_quick|regex_help|tooltip\.find\.regex_help)[^"]*)"')) {
        $key = $match.Groups[1].Value
        if(-not $key.EndsWith('.')) { [void] $keys.Add($key) }
    }
}

foreach($key in @('fbe.tooltip.find.templates')) {
    [void] $keys.Add($key)
}

if($keys.Count -eq 0) { throw 'No active quick-reference localization keys were discovered.' }
foreach($obsolete in @('fbe.regex_help.design.text', 'fbe.regex_help.source.text')) {
    if($keys.Contains($obsolete)) { throw "Obsolete production key is still used: $obsolete" }
}

$failures = @()
foreach($language in $catalog.targetLanguages) {
    $missing = @()
    foreach($key in $keys) {
        $entry = $catalog.strings.$key
        if($null -eq $entry -or [string]::IsNullOrWhiteSpace([string]$entry.translations.$language)) { $missing += $key }
    }
    Write-Host "$language $($keys.Count - $missing.Count)/$($keys.Count)"
    if($missing.Count) { $failures += "${language}: $($missing -join ', ')" }
}
if($failures.Count) { throw "Missing active quick-reference translations:`n$($failures -join "`n")" }

function Assert-RuntimeValues([string]$langRoot, [string]$label) {
    foreach($language in $catalog.targetLanguages) {
        $path = Join-Path (Join-Path $langRoot $language) 'fbe.json'
        if(-not (Test-Path -LiteralPath $path)) { throw "$label does not contain $path." }
        $runtime = Get-Content -Raw -Encoding UTF8 -LiteralPath $path | ConvertFrom-Json -AsHashtable
        foreach($key in $keys) {
            $expected = [string]$catalog.strings.$key.translations.$language
            if(-not $runtime.strings.ContainsKey($key) -or [string]$runtime.strings[$key] -ne $expected) {
                throw "$label changed or omitted $key for $language."
            }
        }
    }
}

$temporaryLangRoot = Join-Path ([IO.Path]::GetTempPath()) ("fbe-regex-localization-" + $PID)
try {
    & (Join-Path $root 'tools\localization\export-runtime-lang.ps1') -RepositoryRoot $root -OutputDirectory $temporaryLangRoot -Clean | Out-Host
    Assert-RuntimeValues $temporaryLangRoot 'Exported runtime localization'
}
finally {
    if(Test-Path -LiteralPath $temporaryLangRoot) { Remove-Item -LiteralPath $temporaryLangRoot -Recurse -Force }
}

$packagedLangRoot = Join-Path $root 'out\Release\Lang'
if(Test-Path -LiteralPath $packagedLangRoot) {
    Assert-RuntimeValues $packagedLangRoot 'Packaged Release runtime localization'
}

Write-Host "Regex quick-reference localization is complete: $($keys.Count) active keys x $($catalog.targetLanguages.Count) languages."
