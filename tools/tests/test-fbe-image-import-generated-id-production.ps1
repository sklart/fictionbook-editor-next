<#
.SYNOPSIS
Exercises FBE's production image-import path and verifies its generated binary id.
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

function Invoke-FbeScenario([string]$Scenario, [string]$Report, [string]$Fixture) {
    $env:FBE_NEXT_TEST_MODE = '1'
    $env:FBE_NEXT_TEST_SCENARIO = $Scenario
    $process = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $Report, $Fixture) -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force
        throw "FBE не завершил сценарий $Scenario."
    }
    if ($process.ExitCode -ne 0) {
        $details = if (Test-Path -LiteralPath $Report) { Get-Content -LiteralPath $Report -Raw } else { 'report unavailable' }
        throw "FBE вернул код $($process.ExitCode) для сценария $Scenario. Report: $details"
    }
    if (-not (Test-Path -LiteralPath $Report -PathType Leaf)) { throw "FBE не записал отчёт сценария $Scenario." }
}

function Assert-ImportedImage([string]$Path, [byte[]]$ExpectedBytes, [string]$ExpectedHash) {
    $xmlText = Get-Content -LiteralPath $Path -Raw
    if ($xmlText -match 'dt:dt|urn:schemas-microsoft-com:datatypes') { throw 'Production Save записал MSXML datatype metadata.' }
    Assert-Fb2Schema $Path
    [xml]$xml = $xmlText
    $namespaces = [Xml.XmlNamespaceManager]::new($xml.NameTable)
    $namespaces.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
    $binary = $xml.SelectSingleNode('/fb:FictionBook/fb:binary[@id="cover-part-01.jpg"]', $namespaces)
    if ($null -eq $binary) { throw 'Production import не создал binary id="cover-part-01.jpg".' }
    if ($binary.GetAttribute('content-type') -ne 'image/jpeg') { throw "Production import изменил MIME: $($binary.GetAttribute('content-type'))." }
    if ($binary.InnerText -match '[ \r\n\t]') { throw 'Production Save оставил whitespace в imported Base64.' }
    $actualBytes = [Convert]::FromBase64String($binary.InnerText)
    $actualHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($actualBytes))
    if ($actualBytes.Length -ne $ExpectedBytes.Length -or $actualHash -ne $ExpectedHash) { throw 'Production import или Save изменили bytes JPEG.' }
    $image = $xml.SelectSingleNode('//fb:image[@*[local-name()="href"]="#cover-part-01.jpg"]', $namespaces)
    if ($null -eq $image) { throw 'Imported image не ссылается на #cover-part-01.jpg.' }
}

$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-image-generated-id-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
$fixture = Join-Path $directory 'imported-image.fb2'
$sourceImage = Join-Path $directory 'cover-part-01.jpg'
$importReport = Join-Path $directory 'import.tsv'
$reopenReport = Join-Path $directory 'reopen.tsv'
$savedEnvironment = @{
    FBE_NEXT_TEST_MODE = $env:FBE_NEXT_TEST_MODE
    FBE_NEXT_TEST_SCENARIO = $env:FBE_NEXT_TEST_SCENARIO
    FBE_NEXT_TEST_IMAGE_PATH = $env:FBE_NEXT_TEST_IMAGE_PATH
}
try {
    Add-Type -AssemblyName System.Drawing
    $bitmap = [Drawing.Bitmap]::new(2, 2)
    try {
        $bitmap.SetPixel(0, 0, [Drawing.Color]::Red)
        $bitmap.SetPixel(1, 0, [Drawing.Color]::Green)
        $bitmap.SetPixel(0, 1, [Drawing.Color]::Blue)
        $bitmap.SetPixel(1, 1, [Drawing.Color]::White)
        $bitmap.Save($sourceImage, [Drawing.Imaging.ImageFormat]::Jpeg)
    }
    finally { $bitmap.Dispose() }
    $sourceBytes = [IO.File]::ReadAllBytes($sourceImage)
    $sourceHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($sourceBytes))
    @"
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Image import</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>image-import-test</id><version>1.0</version></document-info></description><body><section><p>Image import fixture.</p></section></body></FictionBook>
"@ | Set-Content -LiteralPath $fixture -Encoding utf8

    $env:FBE_NEXT_TEST_IMAGE_PATH = $sourceImage
    Invoke-FbeScenario 'binary-import-image' $importReport $fixture
    Assert-ImportedImage $fixture $sourceBytes $sourceHash

    Invoke-FbeScenario 'binary-roundtrip' $reopenReport $fixture
    Assert-ImportedImage $fixture $sourceBytes $sourceHash
    Write-Host 'Production image import generated-id -> Save -> Reopen -> Save passed.'
}
finally {
    foreach ($name in $savedEnvironment.Keys) {
        if ($null -eq $savedEnvironment[$name]) { Remove-Item -Path "Env:$name" -ErrorAction SilentlyContinue }
        else { Set-Item -Path "Env:$name" -Value $savedEnvironment[$name] }
    }
    Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue
}
