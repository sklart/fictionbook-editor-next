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
    "ru_RU" = "UTF-8"
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

$spellerHeader = Read-SourceFile "src\fbe\Speller.h"
$spellerSource = Read-SourceFile "src\fbe\Speller.cpp"

if (-not $spellerHeader.Contains("DetectDictionaryCodePage(Hunhandle* dict, UINT fallbackCodePage);")) {
    throw (Get-ThirdPartyText -Base64 "0JIgU3BlbGxlci5oINC+0YLRgdGD0YLRgdGC0LLRg9C10YIg0L7QsdGK0Y/QstC70LXQvdC40LUgaGVscGVyLdGE0YPQvdC60YbQuNC4INC00LvRjyDQvtC/0YDQtdC00LXQu9C10L3QuNGPINC60L7QtNC+0LLQvtC5INGB0YLRgNCw0L3QuNGG0Ysg0YHQu9C+0LLQsNGA0Y8u")
}

if (-not $spellerSource.Contains("Hunspell_get_dic_encoding(dict)")) {
    throw (Get-ThirdPartyText -Base64 "0JIgU3BlbGxlci5jcHAg0L7RgtGB0YPRgtGB0YLQstGD0LXRgiDQt9Cw0L/RgNC+0YEg0YDQtdCw0LvRjNC90L7QuSDQutC+0LTQuNGA0L7QstC60Lgg0YHQu9C+0LLQsNGA0Y8g0YfQtdGA0LXQtyBIdW5zcGVsbC4=")
}

$helperCalls = ([regex]::Matches(
    $spellerSource,
    "DetectDictionaryCodePage\(m_Dictionaries\[[^\]]+\]\.handle, m_Dictionaries\[[^\]]+\]\.codepage\)")).Count
if ($helperCalls -lt 3) {
    throw (Format-ThirdPartyText "0J7QttC40LTQsNC70L7RgdGMINC60LDQuiDQvNC40L3QuNC80YPQvCAzINCy0YvQt9C+0LLQsCDQvtC/0YDQtdC00LXQu9C10L3QuNGPINC60L7QtNC+0LLQvtC5INGB0YLRgNCw0L3QuNGG0Ysg0YHQu9C+0LLQsNGA0Y8sINC90LDQudC00LXQvdC+OiB7MH0u" $helperCalls)
}

Write-Host (Get-ThirdPartyText -Base64 "0J/RgNC+0LLQtdGA0LrQsCDRgNC10LPRgNC10YHRgdC40Lgg0YHQu9C+0LLQsNGA0LXQuSDQvtGA0YTQvtCz0YDQsNGE0LjQuCDQv9GA0L7RiNC70LAg0YPRgdC/0LXRiNC90L4u")
