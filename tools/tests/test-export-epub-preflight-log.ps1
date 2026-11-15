[CmdletBinding()]
param([Parameter(Mandatory)][string]$BatchExecutable, [Parameter(Mandatory)][string]$DllPath, [switch]$KeepArtifacts)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$scratch = New-IsolatedFbeScratchDirectory -Name 'export-epub-preflight'; $passed = $false
try {
    $inputFile = Join-Path $scratch.Root 'input.fb2'; $outputFile = Join-Path $scratch.Root 'output.epub'
    $xml = '<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><nickname>T</nickname></author><book-title>Test</book-title><lang>en</lang></title-info><document-info><author><nickname>T</nickname></author><id>preflight</id><version>1</version></document-info></description><body><section><title><p>Chapter Alpha</p></title><p>Broken <a l:href="#missing-target">link</a>.<image l:href="#missing-image"/></p></section></body></FictionBook>'
    [IO.File]::WriteAllText($inputFile, $xml, [Text.UTF8Encoding]::new($false))
    & $BatchExecutable --input $inputFile --output $outputFile --version 3 --overwrite --log --dll $DllPath
    if($LASTEXITCODE -ne 0) { throw 'EPUB export failed.' }
    $log = Get-Content -LiteralPath ($outputFile + '.log') -Raw -Encoding utf8
    if($log -notmatch '(?m)^Warnings: [1-9]\d*\r?$' -or $log -notmatch '(?m)^\[WARNING\].*missing-target.*Chapter Alpha') { throw "The export log does not identify the missing target and source chapter:`n$log" }
    if($log -notmatch '(?m)^\[WARNING\].*missing-image.*Chapter Alpha') { throw "The export log does not identify the missing image and source chapter:`n$log" }
    $valid = $xml.Replace('#missing-target', '#existing').Replace('<p>Broken ', '<p id="existing">Broken ').Replace('<image l:href="#missing-image"/>', '')
    [IO.File]::WriteAllText($inputFile, $valid, [Text.UTF8Encoding]::new($false))
    & $BatchExecutable --input $inputFile --output $outputFile --version 3 --overwrite --log --dll $DllPath
    if($LASTEXITCODE -ne 0) { throw 'EPUB export with a valid target failed.' }
    $log = Get-Content -LiteralPath ($outputFile + '.log') -Raw -Encoding utf8
    if($log -notmatch '(?m)^Warnings: 0\r?$' -or $log -match '(?m)^\[WARNING\]') { throw "A valid link produced an export warning:`n$log" }
    $passed = $true; Write-Host 'EPUB export preflight warning and source chapter in log: PASS'
} finally { Complete-IsolatedFbeRuntime -Isolation $scratch -Passed $passed -KeepArtifacts:$KeepArtifacts }
