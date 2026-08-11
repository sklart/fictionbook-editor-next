<#
.SYNOPSIS
Runs FBE production Save for an FB2 binary and verifies the persisted bytes.
#>
[CmdletBinding()]
param([string]$FbeExe = (Join-Path $PSScriptRoot '..\..\out\Release\FBE.exe'), [int]$TimeoutSeconds = 180)

$ErrorActionPreference = 'Stop'
$FbeExe = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FbeExe)
if (-not (Test-Path -LiteralPath $FbeExe -PathType Leaf)) { throw "Не найден FBE: $FbeExe" }
$directory = Join-Path ([IO.Path]::GetTempPath()) ('fbe-binary-roundtrip-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $directory)
$fixture = Join-Path $directory 'binary.fb2'
$saveReport = Join-Path $directory 'save.tsv'
$reopenReport = Join-Path $directory 'reopen.tsv'
$bytes = [byte[]](0..255)
$base64 = [Convert]::ToBase64String($bytes)
$beforeHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes))
try {
    @"
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>binary</book-title><lang>en</lang></title-info><document-info><program-used>test</program-used><id>binary-test</id><version>1.0</version></document-info></description><body><section><p>Binary fixture.</p></section></body><binary id="payload" content-type="application/octet-stream">$base64</binary></FictionBook>
"@ | Set-Content -LiteralPath $fixture -Encoding utf8

    $save = Start-Process -FilePath $FbeExe -ArgumentList @('-s', '-b', $saveReport, $fixture) -PassThru
    if (-not $save.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $save.Id -Force; throw 'FBE не завершил production Save.' }
    if (-not (Test-Path -LiteralPath $saveReport)) { throw 'FBE не записал отчёт production Save.' }

    $xmlText = Get-Content -LiteralPath $fixture -Raw
    if ($xmlText -match 'dt:dt|urn:schemas-microsoft-com:datatypes') { throw 'Production Save записал MSXML datatype metadata.' }
    [xml]$xml = $xmlText
    $namespaces = [Xml.XmlNamespaceManager]::new($xml.NameTable)
    $namespaces.AddNamespace('fb', 'http://www.gribuser.ru/xml/fictionbook/2.0')
    $binary = $xml.SelectSingleNode('/fb:FictionBook/fb:binary[@id="payload"]', $namespaces)
    if ($null -eq $binary -or $binary.GetAttribute('content-type') -ne 'application/octet-stream') { throw 'Production Save изменил binary metadata.' }
    $afterBytes = [Convert]::FromBase64String(($binary.InnerText -replace '\s', ''))
    $afterHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($afterBytes))
    if ($afterHash -ne $beforeHash) { throw "Production Save изменил decoded binary: $afterHash." }

    $reopen = Start-Process -FilePath $FbeExe -ArgumentList @('-b', $reopenReport, $fixture) -PassThru
    if (-not $reopen.WaitForExit($TimeoutSeconds * 1000)) { Stop-Process -Id $reopen.Id -Force; throw 'FBE не завершил повторное открытие saved binary.' }
    if (-not (Test-Path -LiteralPath $reopenReport)) { throw 'FBE не записал отчёт повторного открытия binary.' }
    Write-Host 'Production binary Save -> reopen round-trip passed.'
}
finally { Remove-Item -LiteralPath $directory -Recurse -Force -ErrorAction SilentlyContinue }
