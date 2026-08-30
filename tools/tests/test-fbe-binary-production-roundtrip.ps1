<#
.SYNOPSIS
Runs FBE production Save for an FB2 binary and verifies the persisted bytes.
#>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 180)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
$schemaPath = Join-Path $PSScriptRoot '..\..\runtime\FictionBook.xsd'
function Assert-Fb2Schema([string]$Path) {
    $cache = New-Object -ComObject Msxml2.XMLSchemaCache.6.0
    $cache.add('http://www.gribuser.ru/xml/fictionbook/2.0', $schemaPath)
    $document = New-Object -ComObject Msxml2.DOMDocument.6.0
    $document.async = $false
    if (-not $document.load($Path)) { throw "MSXML не прочитал сохранённый FB2: $($document.parseError.reason)" }
    $document.schemas = $cache
    $validation = $document.validate()
    if ($validation.errorCode -ne 0) { throw "FictionBook.xsd validation failed: $($validation.reason)" }
}
function Assert-Binaries([string]$Path, $ExpectedRecords) {
    $xmlText = Get-Content -LiteralPath $Path -Raw
    if ($xmlText -match 'dt:dt|urn:schemas-microsoft-com:datatypes') { throw 'Production Save записал MSXML datatype metadata.' }
    Assert-Fb2Schema $Path
    [xml]$xml = $xmlText
    $namespaces = [Xml.XmlNamespaceManager]::new($xml.NameTable)
    $namespaces.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
    if ($null -eq $xml.SelectSingleNode('//fb:image[@*[local-name()="href"]="#cover-part-01.jpg"]', $namespaces)) {
        throw 'Production Save изменил xlink:href для binary id с тире.'
    }
    foreach ($expected in $ExpectedRecords) {
        $binary = $xml.SelectSingleNode(('/fb:FictionBook/fb:binary[@id="{0}"]' -f $expected.Id), $namespaces)
        if ($null -eq $binary -or $binary.GetAttribute('content-type') -ne $expected.ContentType) { throw "Production Save изменил metadata $($expected.Id)." }
        if ($binary.InnerText -match '[ \r\n\t]') { throw "Production Save оставил whitespace в Base64 $($expected.Id)." }
        $afterBytes = [Convert]::FromBase64String($binary.InnerText)
        $afterHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($afterBytes))
        if ($afterBytes.Length -ne $expected.Length -or $afterHash -ne $expected.Hash) { throw "Production Save изменил decoded binary $($expected.Id)." }
    }
}
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-binary-roundtrip-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
$fixture = Join-Path $directory 'binary.fb2'
$saveReport = Join-Path $directory 'save.tsv'
$reopenReport = Join-Path $directory 'reopen.tsv'
$resaveReport = Join-Path $directory 'resave.tsv'
$binaryRecords = @()
$sizes = @(3, 57, 256, 1024, 4097, 8192, 12288, 14336, 16384)
$contentTypes = @('image/jpeg', 'application/x-fbe-1', 'application/x-fbe-2', 'application/x-fbe-3', 'application/x-fbe-4', 'application/x-fbe-5', 'application/x-fbe-6', 'application/x-fbe-7', 'application/x-fbe-large')
for ($index = 0; $index -lt $sizes.Count; ++$index) {
    $bytes = [byte[]]::new($sizes[$index])
    for ($offset = 0; $offset -lt $bytes.Length; ++$offset) { $bytes[$offset] = [byte](($index * 29 + $offset) % 256) }
    $base64 = [Convert]::ToBase64String($bytes)
    if ($index -eq 1) { $base64 = [regex]::Replace($base64, '.{64}', '$0' + "`r`n") }
    $id = if ($index -eq 0) { 'cover-part-01.jpg' } else { "payload-$index" }
    $binaryRecords += [pscustomobject]@{ Id = $id; ContentType = $contentTypes[$index]; Length = $bytes.Length; Hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)); Base64 = $base64 }
}
$binaryMarkup = [string]::Join('', @($binaryRecords | ForEach-Object { '<binary id="{0}" content-type="{1}">{2}</binary>' -f $_.Id, $_.ContentType, $_.Base64 }))
try {
    @"
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>binary</book-title><lang>en</lang><coverpage><image l:href="#cover-part-01.jpg"/></coverpage></title-info><document-info><program-used>test</program-used><id>binary-test</id><version>1.0</version></document-info></description><body><section><p>Binary fixture.</p></section></body>$binaryMarkup</FictionBook>
"@ | Set-Content -LiteralPath $fixture -Encoding utf8

    $save = Start-Process -FilePath $FbeExe -ArgumentList @('-s', '-b', $saveReport, $fixture) -PassThru
    if (-not $save.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $save.Id -Force; throw 'FBE не завершил production Save.' }
    if ($save.ExitCode -ne 0) { throw "FBE вернул код $($save.ExitCode) для production binary Save." }
    if (-not (Test-Path -LiteralPath $saveReport)) { throw 'FBE не записал отчёт production Save.' }

    Assert-Binaries $fixture $binaryRecords

    $reopen = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $reopenReport, $fixture) -PassThru
    if (-not $reopen.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $reopen.Id -Force; throw 'FBE не завершил повторное открытие saved binary.' }
    if ($reopen.ExitCode -ne 0) { throw "FBE вернул код $($reopen.ExitCode) при повторном открытии binary." }
    if (-not (Test-Path -LiteralPath $reopenReport)) { throw 'FBE не записал отчёт повторного открытия binary.' }
    $resave = Start-Process -FilePath $FbeExe -ArgumentList @('-s', '-b', $resaveReport, $fixture) -PassThru
    if (-not $resave.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $resave.Id -Force; throw 'FBE не завершил повторный production Save binary.' }
    if ($resave.ExitCode -ne 0) { throw "FBE вернул код $($resave.ExitCode) для повторного production Save binary." }
    if (-not (Test-Path -LiteralPath $resaveReport)) { throw 'FBE не записал отчёт повторного production Save binary.' }
    Assert-Binaries $fixture $binaryRecords
    Write-Host 'Production binary Save -> reopen -> Save round-trip passed.'
}
finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
