<# Keeps built-in NBSP template input independent from the user's display glyph. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$catalog = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\search\SearchPresetCatalog.cpp')
$normalizer = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\FBEview.cpp')
$settingsPage = Get-Content -Raw -LiteralPath (Join-Path $root 'src\fbe\settings\ui\SettingsEditorPage.cpp')

if($catalog -notmatch 'PRESET\(design_nbsp_to_space[^\r\n]*L"\\u00A0"') { throw 'The NBSP preset must retain literal U+00A0, not a configured display glyph.' }
if($normalizer -notmatch 'NormalizeSearchPatternNbsp[\s\S]{0,220}pattern\.Replace\(L"\\u00A0", _Settings\.GetNBSPChar\(\)\)') { throw 'Search must normalize literal U+00A0 to the configured NBSP character.' }
$nbspArrays = [regex]::Matches($settingsPage, 'const wchar_t (?:nbspCharacters|characters)\[\]\s*=\s*\{(?<values>[^}]+)\}')
if($nbspArrays.Count -ne 2) { throw 'The NBSP display and selection character lists must both exist.' }
$characterLists = foreach($array in $nbspArrays) {
    $characters = foreach($token in ($array.Groups['values'].Value -split ',')) {
        if($token -notmatch '^\s*L(''|")\\u(?<code>[0-9A-Fa-f]{4})\1\s*$') {
            throw "Unexpected NBSP character literal: $token"
        }
        $Matches['code'].ToUpperInvariant()
    }
    ,$characters
}
if($characterLists[0] -notcontains '25AB') { throw 'The custom NBSP glyph U+25AB must remain selectable.' }
if(($characterLists[0] -join ',') -cne ($characterLists[1] -join ',')) {
    throw 'The NBSP display and selection indices must refer to the same characters.'
}
if($settingsPage -notmatch 'if\(_Settings\.GetNBSPChar\(\) == CString\(nbspCharacters\[i\]\)\) nbspIndex = i;' -or
   $settingsPage -notmatch 'm_nbspCharacter\.SetCurSel\(nbspIndex\);' -or
   $settingsPage -notmatch 'index >= 0 && index < _countof\(characters\) \? CString\(characters\[index\]\)') {
    throw 'The selected NBSP index must map to the displayed character.'
}
Write-Host 'Search preset NBSP normalization contract passed.'
