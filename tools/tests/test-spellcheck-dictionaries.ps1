<#
.SYNOPSIS
Проверяет словари орфографии в out\<Configuration>\dict и ожидаемые Hunspell-связанные инварианты в исходниках.
#>

[CmdletBinding()]
param(
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "..\build\ThirdPartySources.ps1")

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$outputDir = Join-Path $repoRoot "out\$Configuration"
$dictDir = Join-Path $outputDir "dict"
$singleByteEncoding = [System.Text.Encoding]::GetEncoding(1251)

function Read-SourceFile([string]$RelativePath) {
    $path = Join-Path $repoRoot $RelativePath
    $bytes = [System.IO.File]::ReadAllBytes($path)
    if ($bytes.Length -ge 2 -and $bytes[0] -eq 0xFF -and $bytes[1] -eq 0xFE) {
        return [System.Text.Encoding]::Unicode.GetString($bytes)
    }
    return $singleByteEncoding.GetString($bytes)
}

function Get-DictionaryEncoding([string]$AffPath) {
    foreach ($line in [System.IO.File]::ReadLines($AffPath)) {
        if ($line -match '^\s*SET\s+(?<encoding>\S+)\s*$') {
            return $matches["encoding"].ToUpperInvariant()
        }
    }
    throw (Format-ThirdPartyText "0JIg0YHQu9C+0LLQsNGA0LUgezB9INC90LUg0L3QsNC50LTQtdC9INC30LDQs9C+0LvQvtCy0L7QuiDRgSDQutC+0LTQuNGA0L7QstC60L7QuS4=" $AffPath)
}

if (-not (Test-Path -LiteralPath $dictDir -PathType Container)) {
    throw (Format-ThirdPartyText "0JrQsNGC0LDQu9C+0LMg0YHQu9C+0LLQsNGA0LXQuSDQvdC1INC90LDQudC00LXQvTogezB9" $dictDir)
}

$expectedDictionaryEncodings = @{
    "de_DE" = "ISO8859-1"
    "en_US" = "UTF-8"
    "ru_RU" = "KOI8-R"
    "uk_UA" = "UTF-8"
}

foreach ($dictionaryName in $expectedDictionaryEncodings.Keys) {
    $affPath = Join-Path $dictDir "$dictionaryName.aff"
    $dicPath = Join-Path $dictDir "$dictionaryName.dic"

    if (-not (Test-Path -LiteralPath $affPath -PathType Leaf)) {
        throw (Format-ThirdPartyText "0J7RgtGB0YPRgtGB0YLQstGD0LXRgiDQvtCx0Y/Qt9Cw0YLQtdC70YzQvdGL0Lkg0YHQu9C+0LLQsNGA0Yw6IHswfQ==" $affPath)
    }
    if (-not (Test-Path -LiteralPath $dicPath -PathType Leaf)) {
        throw (Format-ThirdPartyText "0J7RgtGB0YPRgtGB0YLQstGD0LXRgiDQvtCx0Y/Qt9Cw0YLQtdC70YzQvdGL0Lkg0YHQu9C+0LLQsNGA0Yw6IHswfQ==" $dicPath)
    }

    $actualEncoding = Get-DictionaryEncoding -AffPath $affPath
    $expectedEncoding = $expectedDictionaryEncodings[$dictionaryName]
    if ($actualEncoding -ne $expectedEncoding) {
        throw (Format-ThirdPartyText "0KHQu9C+0LLQsNGA0YwgezB9INC+0LHRitGP0LLQu9GP0LXRgiBTRVQgezF9LCDQvtC20LjQtNCw0LvQvtGB0YwgezJ9Lg==" $dictionaryName, $actualEncoding, $expectedEncoding)
    }
}

$manifestPath = Join-Path $repoRoot "runtime\dict\sources.json"
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw "Не найден manifest происхождения словарей: $manifestPath" }
$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
foreach ($dictionaryName in @("de_DE", "en_US", "ru_RU", "uk_UA")) {
    $entry = $manifest.$dictionaryName
    if (-not $entry) { throw "В sources.json отсутствует $dictionaryName" }
    $runtimeAff = Join-Path $repoRoot "runtime\dict\$dictionaryName.aff"
    $runtimeDic = Join-Path $repoRoot "runtime\dict\$dictionaryName.dic"
    if ((Get-FileHash -LiteralPath $runtimeAff -Algorithm SHA256).Hash -ne $entry.affSha256) { throw "SHA-256 aff не совпадает с sources.json: $dictionaryName" }
    if ((Get-FileHash -LiteralPath $runtimeDic -Algorithm SHA256).Hash -ne $entry.dicSha256) { throw "SHA-256 dic не совпадает с sources.json: $dictionaryName" }
    $stagedAff = Join-Path $dictDir "$dictionaryName.aff"
    $stagedDic = Join-Path $dictDir "$dictionaryName.dic"
    if ((Get-FileHash -LiteralPath $stagedAff -Algorithm SHA256).Hash -ne $entry.affSha256) { throw "SHA-256 staged aff не совпадает с sources.json: $dictionaryName" }
    if ((Get-FileHash -LiteralPath $stagedDic -Algorithm SHA256).Hash -ne $entry.dicSha256) { throw "SHA-256 staged dic не совпадает с sources.json: $dictionaryName" }
    if ((Get-DictionaryEncoding -AffPath $runtimeAff) -ne $entry.encoding) { throw "SET aff не совпадает с sources.json: $dictionaryName" }
    $firstLine = [System.IO.File]::ReadLines($runtimeDic) | Select-Object -First 1
    if ($firstLine -ne [string]$entry.dicEntries) { throw "Count в dic не совпадает с sources.json: $dictionaryName" }
}

$spellerHeader = Read-SourceFile "src\fbe\Speller.h"
$spellerSource = Read-SourceFile "src\fbe\Speller.cpp"
$mainFrameSource = Read-SourceFile "src\fbe\mainfrm.h"
$mainFrameImplementation = Read-SourceFile "src\fbe\mainfrm.cpp"
$resourceHeader = Read-SourceFile "src\fbe\resource.h"
$res1Header = Read-SourceFile "src\fbe\res1.h"
$resourceScript = Read-SourceFile "src\fbe\FBE.rc"
$spellDialogLocalization = Get-Content -LiteralPath (Join-Path $repoRoot "localization\app-ui\fbe-small-dialogs.json") -Raw -Encoding UTF8 | ConvertFrom-Json
$expectedMoreSuggestionsRu = [System.Text.Encoding]::UTF8.GetString([Convert]::FromBase64String("0JXRidGRINCy0LDRgNC40LDQvdGC0Ys="))

if (-not $spellerHeader.Contains("DetectDictionaryCodePage(Hunhandle* dict, UINT fallbackCodePage);")) {
    throw (Get-ThirdPartyText -Base64 "0JIgU3BlbGxlci5oINC+0YLRgdGD0YLRgdGC0LLRg9C10YIg0L7QsdGK0Y/QstC70LXQvdC40LUgaGVscGVyLdGE0YPQvdC60YbQuNC4INC00LvRjyDQvtC/0YDQtdC00LXQu9C10L3QuNGPINC60L7QtNC+0LLQvtC5INGB0YLRgNCw0L3QuNGG0Ysg0YHQu9C+0LLQsNGA0Y8u")
}

if (-not $spellerSource.Contains("Hunspell_get_dic_encoding(dict)")) {
    throw (Get-ThirdPartyText -Base64 "0JIgU3BlbGxlci5jcHAg0L7RgtGB0YPRgtGB0YLQstGD0LXRgiDQt9Cw0L/RgNC+0YEg0YDQtdCw0LvRjNC90L7QuSDQutC+0LTQuNGA0L7QstC60Lgg0YHQu9C+0LLQsNGA0Y8g0YfQtdGA0LXQtyBIdW5zcGVsbC4=")
}

$suggestionsStart = $spellerSource.IndexOf("CStrings* CSpeller::GetSuggestions(CString word)")
$spellCheckStart = $spellerSource.IndexOf("SPELL_RESULT CSpeller::SpellCheck(CString word)")
if ($suggestionsStart -lt 0 -or $spellCheckStart -lt 0) {
    throw "Не найдены production пути spellcheck/suggestions."
}
$suggestionsSource = $spellerSource.Substring($suggestionsStart, $spellCheckStart - $suggestionsStart)
if (-not $suggestionsSource.Contains("word = FbePrepareDictionaryWord(word);") -or
    -not $suggestionsSource.Contains("FbeEncodeDictionaryWord(word, m_codePage)")) {
    throw "GetSuggestions должен использовать общую подготовку и кодирование словаря."
}
$spellCheckSource = $spellerSource.Substring($spellCheckStart)
if (-not $spellCheckSource.Contains("checkWord = FbePrepareDictionaryWord(checkWord);")) {
    throw "SpellCheck должен использовать общую подготовку словаря."
}

if ($resourceScript -match 'IDC_SPELL_SUGG_LIST[^\r\n]*LBS_SORT') {
    throw "IDC_SPELL_SUGG_LIST must preserve the raw Hunspell suggestion order."
}
if ($spellerSource -notmatch 'm_Suggestions\.ResetContent\(\);[\s\S]*m_Replacement\.SetWindowText\(L""\);' -or
    $spellerSource -notmatch 'm_Suggestions\.SetCurSel\(0\);[\s\S]*m_Replacement\.SetWindowText\(\(\*m_strSuggestions\)\[0\]\);') {
    throw "Spell dialog must clear replacement and select the first raw suggestion."
}
if ($mainFrameSource -notmatch 'COMMAND_RANGE_HANDLER\(ID_SPELL_REPLACE_FIRST, ID_SPELL_REPLACE_LAST, OnSpellReplace\)' -or
    $mainFrameSource -match 'COMMAND_ID_HANDLER\(IDC_SPELL_REPLACE\+') {
    throw "Spell popup suggestions must use one safe command-ID range handler."
}
if ($resourceHeader -notmatch '#define ID_SPELL_REPLACE_FIRST\s+33000' -or
    $resourceHeader -notmatch '#define ID_SPELL_REPLACE_LAST\s+33031' -or
    $resourceHeader -notmatch '#define _APS_NEXT_COMMAND_VALUE\s+33032') {
    throw "Spell suggestion command range collides with another resource ID."
}
function Get-NumericResourceMacros([string]$Text) {
    return @([regex]::Matches($Text, '(?m)^#define\s+(?<name>\w+)\s+(?<value>0x[0-9A-Fa-f]+|\d+)\b') |
        ForEach-Object {
            $valueText = $_.Groups['value'].Value
            [PSCustomObject]@{
                Name = $_.Groups['name'].Value
                Value = if ($valueText.StartsWith('0x', [StringComparison]::OrdinalIgnoreCase)) {
                    [Convert]::ToInt32($valueText.Substring(2), 16)
                } else {
                    [int]$valueText
                }
            }
        })
}
$allSpellResourceMacros = @(
    Get-NumericResourceMacros $resourceHeader
    Get-NumericResourceMacros $res1Header
)
$suggestionIdCollisions = @($allSpellResourceMacros | Where-Object {
    $_.Value -ge 33000 -and $_.Value -le 33031 -and
        $_.Name -ne 'ID_SPELL_REPLACE_FIRST' -and $_.Name -ne 'ID_SPELL_REPLACE_LAST'
})
$resourceMacros = Get-NumericResourceMacros $resourceHeader
$paste2 = @($resourceMacros | Where-Object { $_.Name -eq 'ID_EDIT_PASTE2' } | Select-Object -First 1)
if ($suggestionIdCollisions.Count -ne 0 -or
    $paste2.Count -ne 1 -or ($paste2[0].Value -ge 33000 -and $paste2[0].Value -le 33031) -or
    $res1Header -notmatch '#define ID_SCI_COLLAPSE_BASE\s+40000' -or 33031 -ge 40000 -or
    $mainFrameImplementation -notmatch 'ID_LAST_PLUGIN < ID_SPELL_REPLACE_FIRST' -or
    $mainFrameImplementation -notmatch 'ID_SCRIPT_BASE \+ 999 < ID_SPELL_REPLACE_FIRST' -or
    $mainFrameImplementation -notmatch 'ID_SPELL_REPLACE_LAST < ID_SCI_COLLAPSE_BASE' -or
    $mainFrameSource -notmatch 'm_Speller->Replace\s*\(wID - ID_SPELL_REPLACE_FIRST\)') {
    throw "Spell suggestion command range must not overlap FBE, plug-in or script commands."
}
if ($spellerSource -notmatch 'primarySuggestionCount = numSuggestions < 8 \? numSuggestions : 8' -or
    $spellerSource -notmatch 'const int numSuggestions = suggestionCount;' -or
    $spellerSource -notmatch 'if \(suggestionCount > commandCapacity\)' -or
    $spellerSource -notmatch 'if \(numSuggestions > primarySuggestionCount\)' -or
    $spellerSource -notmatch 'CreatePopupMenu\(\)' -or
    $spellerSource -notmatch 'for \(int i=primarySuggestionCount; i<numSuggestions; i\+\+\)' -or
    $spellerSource -notmatch 'FbeRestoreSourceApostropheStyle\(m_CurrentSpellWord, \(\*m_menuSuggestions\)\[nIndex\]\)') {
    throw "Spell popup must retain all post-eighth suggestions and restore apostrophe style."
}
if ($spellerSource -notmatch 'FbeLoadRuntimeStringByKey\(L"fbe\.spelling\.menu\.more_suggestions"' -or
    $spellerSource -notmatch 'FbeLoadRuntimeStringByKey\(L"fbe\.dialog\.idd_spell_check\.ignore_all"' -or
    $spellerSource -notmatch 'FbeLoadRuntimeStringByKey\(L"fbe\.spelling\.menu\.add_to_dictionary"' -or
    $spellDialogLocalization.strings.'fbe.spelling.menu.more_suggestions'.translations.'ru-RU' -ne $expectedMoreSuggestionsRu -or
    $null -eq $spellDialogLocalization.strings.'fbe.spelling.menu.add_to_dictionary' -or
    (@($spellDialogLocalization.targetLanguages | Where-Object { -not $spellDialogLocalization.strings.'fbe.spelling.menu.add_to_dictionary'.translations.$_ }).Count -ne 0)) {
    throw "Spell popup labels must use runtime localization, including Russian More suggestions."
}
if ($mainFrameImplementation -notmatch 'FindTopLevelMenuPositionByCommand\(helpMenu, ID_TOOLS_DIAGNOSTIC_TRACE\)') {
    throw "Nested Diagnostics popup must use its runtime localization key."
}
$hunspellHeader = Get-Content -LiteralPath (Join-Path $repoRoot 'third_party\hunspell\src\hunspell\hunspell.hxx') -Raw
if ($hunspellHeader -notmatch '#define MAXSUGGESTION\s+15' -or (33031 - 33000 + 1) -lt 15) {
    throw "Spell suggestion command range is too small for bundled Hunspell."
}

$helperCalls = ([regex]::Matches(
    $spellerSource,
    "DetectDictionaryCodePage\(m_Dictionaries\[[^\]]+\]\.handle, m_Dictionaries\[[^\]]+\]\.codepage\)")).Count
if ($helperCalls -lt 3) {
    throw (Format-ThirdPartyText "0J7QttC40LTQsNC70L7RgdGMINC60LDQuiDQvNC40L3QuNC80YPQvCAzINCy0YvQt9C+0LLQsCDQvtC/0YDQtdC00LXQu9C10L3QuNGPINC60L7QtNC+0LLQvtC5INGB0YLRgNCw0L3QuNGG0Ysg0YHQu9C+0LLQsNGA0Y8sINC90LDQudC00LXQvdC+OiB7MH0u" $helperCalls)
}

& (Join-Path $repoRoot "tools\build\Import-VsDevEnvironment.ps1") -Arch x86 -HostArch x64
$testDir = Join-Path $repoRoot "out\tests"
New-Item -ItemType Directory -Path $testDir -Force | Out-Null
$smokeExe = Join-Path $testDir "spellcheck-dictionary-smoke.exe"
& cl.exe /nologo /EHsc /std:c++17 /DUNICODE /D_UNICODE /MT `
    "/I$(Join-Path $repoRoot 'third_party\wtl')" `
    "/I$(Join-Path $repoRoot 'src\fbe')" `
    "/Fo$(Join-Path $testDir 'spellcheck-splitter.obj')" /c `
    (Join-Path $repoRoot "src\fbe\Splitter.cpp")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& cl.exe /nologo /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE /MT /DHUNSPELL_STATIC `
    "/I$(Join-Path $repoRoot 'build\hunspell\include')" `
    "/I$(Join-Path $repoRoot 'third_party\hunspell\src\hunspell')" `
    "/I$(Join-Path $repoRoot 'src\fbe')" `
    (Join-Path $PSScriptRoot "spellcheck-dictionary-smoke.cpp") `
    (Join-Path $testDir "spellcheck-splitter.obj") `
    (Join-Path $repoRoot "build\hunspell\lib\$Configuration\libhunspell.lib") `
    /link "/OUT:$smokeExe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $smokeExe $dictDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host (Get-ThirdPartyText -Base64 "0J/RgNC+0LLQtdGA0LrQsCDRgNC10LPRgNC10YHRgdC40Lgg0YHQu9C+0LLQsNGA0LXQuSDQvtGA0YTQvtCz0YDQsNGE0LjQuCDQv9GA0L7RiNC70LAg0YPRgdC/0LXRiNC90L4u")
