<#
.SYNOPSIS
Exercises production Save -> Reopen -> Save with many compact FB2 binaries.
#>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int]$BinaryCount = 500,
    [int]$BinarySizeKiB = 64,
    [int]$TimeoutSeconds = 600
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
if ($BinaryCount -lt 1 -or $BinarySizeKiB -lt 1) { throw 'Количество и размер binary должны быть положительными.' }
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$schemaPath = Join-Path $repoRoot 'runtime\FictionBook.xsd'

function Assert-ManyBinaries([string]$Path, $Expected) {
    $text = Get-Content -LiteralPath $Path -Raw
    if ($text -match 'dt:dt|urn:schemas-microsoft-com:datatypes') { throw 'FBE записал MSXML datatype metadata.' }
    $cache = New-Object -ComObject Msxml2.XMLSchemaCache.6.0
    $cache.add('http://www.gribuser.ru/xml/fictionbook/2.0', $schemaPath)
    $document = New-Object -ComObject Msxml2.DOMDocument.6.0
    $document.async = $false
    if (-not $document.load($Path)) { throw "MSXML не прочитал FB2: $($document.parseError.reason)" }
    $document.schemas = $cache
    if ($document.validate().errorCode -ne 0) { throw 'FictionBook.xsd validation failed.' }
    [xml]$xml = $text
    $namespaces = [Xml.XmlNamespaceManager]::new($xml.NameTable)
    $namespaces.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
    $binaries = @($xml.SelectNodes('/fb:FictionBook/fb:binary', $namespaces))
    if ($binaries.Count -ne $Expected.Count) { throw "Ожидалось $($Expected.Count) binary, получено $($binaries.Count)." }
    foreach ($record in $Expected) {
        $binary = $xml.SelectSingleNode(('/fb:FictionBook/fb:binary[@id="{0}"]' -f $record.Id), $namespaces)
        if ($null -eq $binary -or $binary.GetAttribute('content-type') -ne $record.ContentType) { throw "FBE изменил metadata $($record.Id)." }
        if ($binary.InnerText -match '[ \r\n\t]') { throw "FBE оставил whitespace в Base64 $($record.Id)." }
        $bytes = [Convert]::FromBase64String($binary.InnerText)
        $hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes))
        if ($bytes.Length -ne $record.Length -or $hash -ne $record.Hash) { throw "FBE изменил bytes $($record.Id)." }
    }
}

function Invoke-FbeMeasured([string[]]$Arguments, [string]$Phase) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process -FilePath $FbeExe -ArgumentList $Arguments -PassThru
    [int64]$peakPrivate = 0; [int64]$peakWorkingSet = 0
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        Start-Sleep -Milliseconds 100
        $process.Refresh()
        $peakPrivate = [Math]::Max($peakPrivate, $process.PrivateMemorySize64)
        $peakWorkingSet = [Math]::Max($peakWorkingSet, $process.WorkingSet64)
    } while (-not $process.HasExited -and [DateTime]::UtcNow -lt $deadline)
    if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force; throw "FBE не завершил $Phase." }
    $watch.Stop()
    if ($process.ExitCode -ne 0) { throw "FBE вернул код $($process.ExitCode): $Phase." }
    [pscustomobject]@{ Phase = $Phase; ElapsedMs = $watch.ElapsedMilliseconds; PeakPrivateBytes = $peakPrivate; PeakWorkingSetBytes = $peakWorkingSet }
}

$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-many-binaries-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
try {
    $previousMode = $env:FBE_NEXT_TEST_MODE; $previousScenario = $env:FBE_NEXT_TEST_SCENARIO
    $env:FBE_NEXT_TEST_MODE = '1'; $env:FBE_NEXT_TEST_SCENARIO = 'binary-roundtrip'
    $records = @(); $markup = [Text.StringBuilder]::new(); $random = [Random]::new(24024)
    for ($index = 0; $index -lt $BinaryCount; ++$index) {
        $bytes = [byte[]]::new($BinarySizeKiB * 1KB); $random.NextBytes($bytes)
        $id = 'stress-{0:D3}' -f $index; $contentType = if (($index % 2) -eq 0) { 'application/octet-stream' } else { 'application/x-fbe-stress' }
        $records += [pscustomobject]@{ Id = $id; ContentType = $contentType; Length = $bytes.Length; Hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)) }
        [void]$markup.AppendFormat('<binary id="{0}" content-type="{1}">{2}</binary>', $id, $contentType, [Convert]::ToBase64String($bytes))
    }
    $fixture = Join-Path $directory 'many-binaries.fb2'
    $xml = "<?xml version=`"1.0`" encoding=`"utf-8`"?><FictionBook xmlns=`"http://www.gribuser.ru/xml/fictionbook/2.0`"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>many binaries</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>many-binaries</id><version>1.0</version></document-info></description><body><section><p>Stress fixture.</p></section></body>$markup</FictionBook>"
    [IO.File]::WriteAllText($fixture, $xml, [Text.UTF8Encoding]::new($false))
    $measurements = @()
    $saveReport = Join-Path $directory 'save.tsv'; $reopenReport = Join-Path $directory 'reopen.tsv'; $resaveReport = Join-Path $directory 'resave.tsv'
    $measurements += Invoke-FbeMeasured @('-b', $saveReport, $fixture) 'Save'
    if (-not (Test-Path -LiteralPath $saveReport)) { throw 'FBE не записал Save report.' }
    Assert-ManyBinaries $fixture $records
    $measurements += Invoke-FbeMeasured @('-b', $reopenReport, $fixture) 'Reopen'
    if (-not (Test-Path -LiteralPath $reopenReport)) { throw 'FBE не записал Reopen report.' }
    $measurements += Invoke-FbeMeasured @('-b', $resaveReport, $fixture) 'Save #2'
    if (-not (Test-Path -LiteralPath $resaveReport)) { throw 'FBE не записал Save #2 report.' }
    Assert-ManyBinaries $fixture $records
    $measurements | Format-Table Phase,ElapsedMs,PeakPrivateBytes,PeakWorkingSetBytes -AutoSize | Out-Host
    Write-Host "Many-binary production Save -> Reopen -> Save stress passed ($BinaryCount x $BinarySizeKiB KiB)."
}
finally {
    if ($null -eq $previousMode) { Remove-Item Env:FBE_NEXT_TEST_MODE -ErrorAction SilentlyContinue } else { $env:FBE_NEXT_TEST_MODE = $previousMode }
    if ($null -eq $previousScenario) { Remove-Item Env:FBE_NEXT_TEST_SCENARIO -ErrorAction SilentlyContinue } else { $env:FBE_NEXT_TEST_SCENARIO = $previousScenario }
    Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue
}
