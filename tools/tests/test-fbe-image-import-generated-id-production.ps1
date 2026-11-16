<#
.SYNOPSIS
Exercises FBE's production image-import path and verifies its generated binary id.
#>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 180, [ValidateSet('0','1')][string]$Inline = '1', [switch]$UndoRedo, [switch]$ExistingIdCollision)

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

function Invoke-FbeScenario([string]$Scenario, [string]$Report, [string]$Fixture, [string]$WorkingDirectory) {
    $env:FBE_NEXT_TEST_MODE = '1'
    $env:FBE_NEXT_TEST_SCENARIO = $Scenario
    $process = Start-Process -FilePath $FbeExe -WorkingDirectory $WorkingDirectory -ArgumentList @('--portable', '-b', $Report, $Fixture) -WindowStyle Hidden -PassThru
    $code = Wait-IsolatedFbeProcess -Process $process -Isolation $isolation -Scenario $Scenario -Report $Report -TimeoutSeconds $TimeoutSeconds
    if ($code -ne 0) {
        $details = if (Test-Path -LiteralPath $Report) { Get-Content -LiteralPath $Report -Raw } else { 'report unavailable' }
        throw "FBE вернул код $code для сценария $Scenario. Report: $details"
    }
    if (-not (Test-Path -LiteralPath $Report -PathType Leaf)) { throw "FBE не записал отчёт сценария $Scenario." }
}

function Assert-ImportedImage([string]$Path, [byte[]]$ExpectedBytes, [string]$ExpectedHash, [string]$Inline, [string]$ExpectedId) {
    $xmlText = Get-Content -LiteralPath $Path -Raw
    if ($xmlText -match 'dt:dt|urn:schemas-microsoft-com:datatypes') { throw 'Production Save записал MSXML datatype metadata.' }
    Assert-Fb2Schema $Path
    [xml]$xml = $xmlText
    $namespaces = [Xml.XmlNamespaceManager]::new($xml.NameTable)
    $namespaces.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
    $binary = $xml.SelectSingleNode(('/fb:FictionBook/fb:binary[@id="{0}"]' -f $ExpectedId), $namespaces)
    if ($null -eq $binary) { throw "Production import не создал binary id=`"$ExpectedId`"." }
    if ($binary.GetAttribute('content-type') -ne 'image/jpeg') { throw "Production import изменил MIME: $($binary.GetAttribute('content-type'))." }
    if ($binary.InnerText -match '[ \r\n\t]') { throw 'Production Save оставил whitespace в imported Base64.' }
    $actualBytes = [Convert]::FromBase64String($binary.InnerText)
    $actualHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($actualBytes))
    if ($actualBytes.Length -ne $ExpectedBytes.Length -or $actualHash -ne $ExpectedHash) { throw 'Production import или Save изменили bytes JPEG.' }
    $image = $xml.SelectSingleNode(('//fb:image[@*[local-name()="href"]="#{0}"]' -f $ExpectedId), $namespaces)
    if ($null -eq $image) { throw "Imported image не ссылается на #$ExpectedId." }
    $inlineImage = $xml.SelectSingleNode(('/fb:FictionBook/fb:body/fb:section/fb:p/fb:image[@*[local-name()="href"]="#{0}"]' -f $ExpectedId), $namespaces)
    $blockImage = $xml.SelectSingleNode(('/fb:FictionBook/fb:body/fb:section/fb:image[@*[local-name()="href"]="#{0}"]' -f $ExpectedId), $namespaces)
    if ($Inline -eq '1' -and ($null -eq $inlineImage -or $null -ne $blockImage)) { throw 'Inline import не сохранил image внутри paragraph.' }
    if ($Inline -eq '0' -and ($null -eq $blockImage -or $null -ne $inlineImage)) { throw 'Block import не сохранил image как прямой дочерний section.' }
}

. (Join-Path $PSScriptRoot 'RuntimeTestIsolation.ps1')
$isolation = New-IsolatedFbeRuntime -FbeExe $FbeExe -Name 'image-generated-id'
$FbeExe = $isolation.Exe
$directory = $isolation.Root; $passed = $false
$sourceDirectory = Join-Path $directory 'source'
$cwdDirectory = Join-Path $directory 'cwd'
$savedEnvironment = @{
    FBE_NEXT_TEST_MODE = $env:FBE_NEXT_TEST_MODE
    FBE_NEXT_TEST_SCENARIO = $env:FBE_NEXT_TEST_SCENARIO
    FBE_NEXT_TEST_IMAGE_PATH = $env:FBE_NEXT_TEST_IMAGE_PATH
    FBE_NEXT_TEST_IMAGE_INLINE = $env:FBE_NEXT_TEST_IMAGE_INLINE
    FBE_NEXT_TEST_IMAGE_UNDO_REDO = $env:FBE_NEXT_TEST_IMAGE_UNDO_REDO
}
try {
    [void](New-Item -ItemType Directory -Path $sourceDirectory -ErrorAction Stop)
    [void](New-Item -ItemType Directory -Path $cwdDirectory -ErrorAction Stop)
    Add-Type -AssemblyName System.Drawing
    $imageCases = @('cover-part-01.jpg', 'Обложка-01.jpg')
    foreach ($imageName in $imageCases) {
    $fixture = Join-Path $sourceDirectory ("imported-image-$imageName.fb2")
    $sourceImage = Join-Path $sourceDirectory $imageName
    $cwdImage = Join-Path $cwdDirectory $imageName
    $importReport = Join-Path $directory ("import-$imageName.tsv")
    $reopenReport = Join-Path $directory ("reopen-$imageName.tsv")
    $bitmap = [Drawing.Bitmap]::new(2, 2)
    try {
        $bitmap.SetPixel(0, 0, [Drawing.Color]::Red)
        $bitmap.SetPixel(1, 0, [Drawing.Color]::Green)
        $bitmap.SetPixel(0, 1, [Drawing.Color]::Blue)
        $bitmap.SetPixel(1, 1, [Drawing.Color]::White)
        $bitmap.Save($sourceImage, [Drawing.Imaging.ImageFormat]::Jpeg)
    }
    finally { $bitmap.Dispose() }
    $collisionBitmap = [Drawing.Bitmap]::new(3, 1)
    try {
        $collisionBitmap.SetPixel(0, 0, [Drawing.Color]::Black)
        $collisionBitmap.SetPixel(1, 0, [Drawing.Color]::Yellow)
        $collisionBitmap.SetPixel(2, 0, [Drawing.Color]::Magenta)
        $collisionBitmap.Save($cwdImage, [Drawing.Imaging.ImageFormat]::Jpeg)
    }
    finally { $collisionBitmap.Dispose() }
    $sourceBytes = [IO.File]::ReadAllBytes($sourceImage)
    $sourceHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($sourceBytes))
    $cwdHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([IO.File]::ReadAllBytes($cwdImage)))
    if($cwdHash -eq $sourceHash) { throw 'CWD collision images must differ.' }
    $existingBinary = if ($ExistingIdCollision) { '<binary id="{0}" content-type="image/jpeg">{1}</binary>' -f $imageName, [Convert]::ToBase64String([IO.File]::ReadAllBytes($cwdImage)) } else { '' }
    $expectedId = if ($ExistingIdCollision) { "${imageName}_0" } else { $imageName }
    @"
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Image import</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>image-import-test</id><version>1.0</version></document-info></description><body><section><p>Image import fixture.</p></section></body>$existingBinary</FictionBook>
"@ | Set-Content -LiteralPath $fixture -Encoding utf8

    $env:FBE_NEXT_TEST_IMAGE_PATH = $sourceImage
    $env:FBE_NEXT_TEST_IMAGE_INLINE = $Inline
    $env:FBE_NEXT_TEST_IMAGE_UNDO_REDO = if ($UndoRedo) { '1' } else { $null }
    Invoke-FbeScenario 'binary-import-image' $importReport $fixture $cwdDirectory
    if ($UndoRedo) {
        $phases = Get-Content -LiteralPath $importReport -Raw
        foreach ($phase in @('undo-dom-complete','redo-dom-complete','reopen-complete')) {
            if ($phases -notmatch ('(?m)^' + [regex]::Escape($phase) + '\t')) { throw "Missing Undo/Redo phase: $phase" }
        }
    }
    Assert-ImportedImage $fixture $sourceBytes $sourceHash $Inline $expectedId
    if ($ExistingIdCollision) {
        [xml]$saved = Get-Content -LiteralPath $fixture -Raw
        $old = $saved.SelectSingleNode('//*[local-name()="binary" and @id="' + $imageName + '"]')
        if ($null -eq $old -or [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Convert]::FromBase64String($old.InnerText))) -ne $cwdHash) { throw 'Existing colliding binary was changed.' }
    }

    Invoke-FbeScenario 'binary-roundtrip' $reopenReport $fixture $cwdDirectory
    Assert-ImportedImage $fixture $sourceBytes $sourceHash $Inline $expectedId
    if ($ExistingIdCollision) {
        [xml]$saved = Get-Content -LiteralPath $fixture -Raw
        $old = $saved.SelectSingleNode('//*[local-name()="binary" and @id="' + $imageName + '"]')
        if ($null -eq $old -or [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Convert]::FromBase64String($old.InnerText))) -ne $cwdHash) { throw 'Existing colliding binary changed after reopen.' }
    }
    }
    $passed = $true
    Write-Host 'Production image import generated-id -> Save -> Reopen -> Save passed.'
}
finally {
    foreach ($name in $savedEnvironment.Keys) {
        if ($null -eq $savedEnvironment[$name]) { Remove-Item -Path "Env:$name" -ErrorAction SilentlyContinue }
        else { Set-Item -Path "Env:$name" -Value $savedEnvironment[$name] }
    }
    Complete-IsolatedFbeRuntime -Isolation $isolation -Passed $passed
}
