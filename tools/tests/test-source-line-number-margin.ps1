[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

function Get-LineNumberDigits([int]$lineCount) {
    if ($lineCount -lt 1) {
        $lineCount = 1
    }

    $digits = 1
    for ($value = $lineCount; $value -ge 10; $value = [int][Math]::Floor($value / 10)) {
        $digits++
    }

    return [Math]::Max(4, $digits)
}

function Needs-MarginUpdate([int]$previous, [int]$lineCount) {
    return $previous -ne (Get-LineNumberDigits $lineCount)
}

$expected = @{ 1 = 4; 9 = 4; 10 = 4; 99 = 4; 100 = 4; 999 = 4; 1000 = 4; 9999 = 4; 10000 = 5; 99999 = 5; 100000 = 6 }
foreach ($lineCount in $expected.Keys) {
    if ((Get-LineNumberDigits $lineCount) -ne $expected[$lineCount]) {
        throw "Unexpected digits for $lineCount"
    }
}

if (!(Needs-MarginUpdate 4 10000) -or (Needs-MarginUpdate 5 10001) -or !(Needs-MarginUpdate 5 9999)) {
    throw 'Digit threshold update contract failed.'
}

$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$source = Get-Content -Raw (Join-Path $root 'src\fbe\mainfrm.cpp')
$header = Get-Content -Raw (Join-Path $root 'src\fbe\mainfrm.h')

foreach ($required in @('GetLineNumberDigits', 'ShouldUpdateSourceLineNumberMargin', 'SCI_GETLINECOUNT', 'SCI_TEXTWIDTH', 'SCI_SETMARGINWIDTHN')) {
    if ($source -notlike "*$required*") {
        throw "Missing source margin behavior: $required"
    }
}

if ($header -notlike '*SC_UPDATE_LINE_COUNT*') {
    throw 'Missing SC_UPDATE_LINE_COUNT margin update behavior.'
}

$modifiedBlock = [regex]::Match($source, 'void\s+CMainFrame::SciModified\s*\([^)]*\)\s*\{[\s\S]*?(?=\n\nbool\s+CMainFrame::SciUpdateUI)')
if (!$modifiedBlock.Success) {
    throw 'SciModified implementation was not found.'
}

if ($modifiedBlock.Value -match 'SC_MOD_INSERTTEXT\s*\|\s*SC_MOD_DELETETEXT|UpdateSourceLineNumberMargin') {
    throw 'SciModified must not update the line-number margin for every text modification.'
}

foreach ($required in @('SC_MOD_CHANGEFOLD', 'SCI_SETFOLDEXPANDED')) {
    if ($modifiedBlock.Value -notlike "*$required*") {
        throw "SciModified folding behavior is missing: $required"
    }
}

$sampleBlock = [regex]::Match($source, 'CStringA sample;[\s\S]*?SCI_TEXTWIDTH,[\s\S]*?sample.GetString\(\)')
if (!$sampleBlock.Success) {
    throw 'SCI_TEXTWIDTH must receive the ANSI CStringA digit sample.'
}

if ($sampleBlock.Value -match 'CStringW|LPCWSTR|wchar_t|L''9''') {
    throw 'SCI_TEXTWIDTH digit sample must not be UTF-16.'
}

function Get-LineNumberSample([int]$lineCount) {
    return ('9' * (Get-LineNumberDigits $lineCount))
}

foreach ($pair in @(@(9999, 10000), @(99999, 100000))) {
    if ((Get-LineNumberSample $pair[0]).Length -eq (Get-LineNumberSample $pair[1]).Length) {
        throw "Digit samples must differ across $($pair[0]) -> $($pair[1])."
    }
}

if ((Get-LineNumberSample 9999) -ne '9999' -or (Get-LineNumberSample 10000) -ne '99999' -or (Get-LineNumberSample 100000) -ne '999999') {
    throw 'ANSI digit samples have incorrect lengths.'
}

Write-Host 'Source line-number margin thresholds passed.'
