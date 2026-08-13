<#!
.SYNOPSIS
Verifies that production ImageImport output survives FBE Save -> Reopen -> Save.
#>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [string]$SmokeExe = (Join-Path $PSScriptRoot '..\..\out\tests\image-import\image-import-smoke.exe'),
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
$SmokeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($SmokeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
if (-not (Test-Path -LiteralPath $SmokeExe -PathType Leaf)) { throw "Не найден ImageImport smoke bridge: $SmokeExe" }
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$schemaPath = Join-Path $repoRoot 'runtime\FictionBook.xsd'

function Assert-Fb2Schema([string]$Path) {
    $cache = New-Object -ComObject Msxml2.XMLSchemaCache.6.0
    $cache.add('http://www.gribuser.ru/xml/fictionbook/2.0', $schemaPath)
    $document = New-Object -ComObject Msxml2.DOMDocument.6.0
    $document.async = $false
    if (-not $document.load($Path)) { throw "MSXML не прочитал FB2: $($document.parseError.reason)" }
    $document.schemas = $cache
    $validation = $document.validate()
    if ($validation.errorCode -ne 0) { throw "FictionBook.xsd validation failed: $($validation.reason)" }
}

function Assert-ImportedBinaries([string]$Path, $Expected) {
    $xmlText = Get-Content -LiteralPath $Path -Raw
    if ($xmlText -match 'dt:dt|urn:schemas-microsoft-com:datatypes') { throw 'FBE записал временные MSXML datatype metadata.' }
    Assert-Fb2Schema $Path
    [xml]$xml = $xmlText
    $namespaces = [Xml.XmlNamespaceManager]::new($xml.NameTable)
    $namespaces.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
    foreach ($record in $Expected) {
        $binary = $xml.SelectSingleNode(('/fb:FictionBook/fb:binary[@id="{0}"]' -f $record.Id), $namespaces)
        if ($null -eq $binary) { throw "Не найден binary $($record.Id)." }
        if ($binary.GetAttribute('content-type') -ne $record.ContentType) { throw "FBE изменил MIME $($record.Id)." }
        $bytes = [Convert]::FromBase64String(($binary.InnerText -replace '\s', ''))
        $hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes))
        if ($bytes.Length -ne $record.Length -or $hash -ne $record.Hash) { throw "FBE изменил imported bytes $($record.Id)." }
    }
}

function Invoke-Fbe([string[]]$Arguments, [string]$Description) {
    $process = Start-Process -FilePath $FbeExe -ArgumentList $Arguments -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $process.Id -Force; throw "FBE не завершил $Description." }
    if ($process.ExitCode -ne 0) { throw "FBE вернул код $($process.ExitCode): $Description." }
}

$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-image-import-roundtrip-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
try {
    $jp2 = Join-Path $directory 'fixture.jp2'
    & $SmokeExe '--make-jp2' $jp2
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $jp2)) { throw 'Smoke bridge не подготовил JPEG 2000 fixture.' }
    $cases = @(
        @{ Id = 'jpeg'; Source = (Join-Path $repoRoot 'out\tests\image-import\original.jpeg'); Converted = $false },
        @{ Id = 'png'; Source = (Join-Path $repoRoot 'src\fbe\res\imgph.png'); Converted = $false },
        @{ Id = 'avif'; Source = (Join-Path $repoRoot 'third_party\libheif\examples\example.avif'); Converted = $true },
        @{ Id = 'jpeg2000'; Source = $jp2; Converted = $true }
    )
    $records = @()
    foreach ($case in $cases) {
        if (-not (Test-Path -LiteralPath $case.Source -PathType Leaf)) { throw "Не найден fixture $($case.Source)." }
        $output = Join-Path $directory ($case.Id + '.bin')
        $metadata = Join-Path $directory ($case.Id + '.txt')
        & $SmokeExe '--export' $case.Source $output $metadata
        if ($LASTEXITCODE -ne 0) { throw "ImageImport bridge завершился с кодом $LASTEXITCODE для $($case.Id)." }
        $values = @{}
        foreach ($line in Get-Content -LiteralPath $metadata) { $pair = $line.Split('=', 2); if ($pair.Count -eq 2) { $values[$pair[0]] = $pair[1] } }
        $bytes = [IO.File]::ReadAllBytes($output)
        if ([string]::IsNullOrWhiteSpace($values.mime) -or [int64]$values.length -ne $bytes.Length -or ([int]$values.converted -ne [int]$case.Converted)) { throw "Некорректные metadata ImageImport для $($case.Id)." }
        $records += [pscustomobject]@{ Id = $case.Id; ContentType = $values.mime; Length = $bytes.Length; Hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)); Base64 = [Convert]::ToBase64String($bytes) }
    }
    $binaryMarkup = [string]::Join('', @($records | ForEach-Object { '<binary id="{0}" content-type="{1}">{2}</binary>' -f $_.Id, $_.ContentType, $_.Base64 }))
    $fixture = Join-Path $directory 'images.fb2'
    @"
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>ImageImport</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>image-import-fbe-roundtrip</id><version>1.0</version></document-info></description><body><section><p>ImageImport fixture.</p></section></body>$binaryMarkup</FictionBook>
"@ | Set-Content -LiteralPath $fixture -Encoding utf8
    $saveReport = Join-Path $directory 'save.tsv'
    $reopenReport = Join-Path $directory 'reopen.tsv'
    $resaveReport = Join-Path $directory 'resave.tsv'
    Invoke-Fbe @('-s', '-b', $saveReport, $fixture) 'первичное сохранение imported images'
    if (-not (Test-Path -LiteralPath $saveReport)) { throw 'FBE не записал отчёт первого сохранения image fixture.' }
    Assert-ImportedBinaries $fixture $records
    Invoke-Fbe @('-b', $reopenReport, $fixture) 'повторное открытие imported images'
    if (-not (Test-Path -LiteralPath $reopenReport)) { throw 'FBE не записал отчёт повторного открытия image fixture.' }
    Invoke-Fbe @('-s', '-b', $resaveReport, $fixture) 'повторное сохранение imported images'
    if (-not (Test-Path -LiteralPath $resaveReport)) { throw 'FBE не записал отчёт повторного сохранения image fixture.' }
    Assert-ImportedBinaries $fixture $records
    Write-Host 'ImageImport -> FBE Save -> reopen -> Save round-trip passed.'
}
finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
