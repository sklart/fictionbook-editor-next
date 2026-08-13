<#
.SYNOPSIS
Exercises production FBE Save -> Reopen -> Save with realistic binary payloads.
#>
[CmdletBinding()]
param(
    [string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'),
    [int[]]$SizesMiB = @(1, 5),
    [switch]$Include25MiB,
    [int]$TimeoutSeconds = 300
)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
if ($Include25MiB) { $SizesMiB += 25 }
if (@($SizesMiB | Where-Object { $_ -lt 1 }).Count) { throw 'Размер binary должен быть не менее 1 MiB.' }
$schemaPath = Join-Path $PSScriptRoot '..\..\runtime\FictionBook.xsd'

function Assert-Fb2([string]$Path, $Expected) {
    $text = Get-Content -LiteralPath $Path -Raw
    if ($text -match 'dt:dt|urn:schemas-microsoft-com:datatypes') { throw 'FBE записал временный MSXML datatype metadata.' }
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
    $binary = $xml.SelectSingleNode('/fb:FictionBook/fb:binary[@id="large-payload"]', $namespaces)
    if ($null -eq $binary -or $binary.GetAttribute('content-type') -ne 'application/octet-stream') { throw 'FBE изменил metadata large binary.' }
    $bytes = [Convert]::FromBase64String(($binary.InnerText -replace '\s', ''))
    $hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes))
    if ($bytes.Length -ne $Expected.Length -or $hash -ne $Expected.Hash) { throw 'FBE изменил bytes large binary.' }
}

function Invoke-FbeMeasured([string[]]$Arguments, [string]$Phase) {
    $started = [Diagnostics.Stopwatch]::StartNew()
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
    $started.Stop()
    if ($process.ExitCode -ne 0) { throw "FBE вернул код $($process.ExitCode): $Phase." }
    return [pscustomobject]@{ Phase = $Phase; ElapsedMs = $started.ElapsedMilliseconds; PrivateBytes = $peakPrivate; WorkingSetBytes = $peakWorkingSet }
}

$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-large-binary-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
try {
    $previousTestMode = $env:FBE_NEXT_TEST_MODE
    $previousScenario = $env:FBE_NEXT_TEST_SCENARIO
    $env:FBE_NEXT_TEST_MODE = '1'
    $env:FBE_NEXT_TEST_SCENARIO = 'binary-roundtrip'
    $measurements = @()
    foreach ($sizeMiB in $SizesMiB | Sort-Object -Unique) {
        $bytes = [byte[]]::new($sizeMiB * 1MB)
        for ($index = 0; $index -lt $bytes.Length; ++$index) { $bytes[$index] = [byte](($index * 37 + $sizeMiB * 19) % 256) }
        $expected = [pscustomobject]@{ Length = $bytes.Length; Hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)) }
        $fixture = Join-Path $directory ("binary-{0}mib.fb2" -f $sizeMiB)
        $base64 = [Convert]::ToBase64String($bytes)
        $xml = "<?xml version=`"1.0`" encoding=`"utf-8`"?><FictionBook xmlns=`"http://www.gribuser.ru/xml/fictionbook/2.0`"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>large binary</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>large-binary-$sizeMiB</id><version>1.0</version></document-info></description><body><section><p>Large binary fixture.</p></section></body><binary id=`"large-payload`" content-type=`"application/octet-stream`">$base64</binary></FictionBook>"
        [IO.File]::WriteAllText($fixture, $xml, [Text.UTF8Encoding]::new($false))
        $saveReport = Join-Path $directory ("save-{0}.tsv" -f $sizeMiB)
        $reopenReport = Join-Path $directory ("reopen-{0}.tsv" -f $sizeMiB)
        $resaveReport = Join-Path $directory ("resave-{0}.tsv" -f $sizeMiB)
        $measurements += Invoke-FbeMeasured @('-b', $saveReport, $fixture) ("Save $sizeMiB MiB")
        if (-not (Test-Path -LiteralPath $saveReport)) { throw "FBE не записал Save report для $sizeMiB MiB." }
        Assert-Fb2 $fixture $expected
        $measurements += Invoke-FbeMeasured @('-b', $reopenReport, $fixture) ("Reopen $sizeMiB MiB")
        if (-not (Test-Path -LiteralPath $reopenReport)) { throw "FBE не записал Reopen report для $sizeMiB MiB." }
        $measurements += Invoke-FbeMeasured @('-b', $resaveReport, $fixture) ("Save #2 $sizeMiB MiB")
        if (-not (Test-Path -LiteralPath $resaveReport)) { throw "FBE не записал Save #2 report для $sizeMiB MiB." }
        Assert-Fb2 $fixture $expected
    }
    $measurements | Format-Table Phase,ElapsedMs,PrivateBytes,WorkingSetBytes -AutoSize | Out-Host
    Write-Host 'Large binary production Save -> reopen -> Save round-trip passed.'
}
finally {
    if ($null -eq $previousTestMode) { Remove-Item Env:FBE_NEXT_TEST_MODE -ErrorAction SilentlyContinue } else { $env:FBE_NEXT_TEST_MODE = $previousTestMode }
    if ($null -eq $previousScenario) { Remove-Item Env:FBE_NEXT_TEST_SCENARIO -ErrorAction SilentlyContinue } else { $env:FBE_NEXT_TEST_SCENARIO = $previousScenario }
    Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue
}
