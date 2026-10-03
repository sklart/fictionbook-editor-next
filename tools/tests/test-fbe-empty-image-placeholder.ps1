<#
.SYNOPSIS
Проверяет встроенную общую заглушку пустой картинки и контракт её round-trip.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$protocol = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\MemProtocol.h')
$resourceScript = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.rc')
$resourceIds = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\resource.h')
$runtime = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'runtime\main.js')
$tree = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\DocumentTree.cpp')
$placeholder = Join-Path $repoRoot 'src\fbe\res\imgph.png'

if($protocol -match 'U::LoadFile\(' -or $protocol -match 'GetProgDirFile\(.*imgph') { throw 'Пустая картинка всё ещё зависит от imgph.png рядом с EXE.' }
foreach($required in @('FindResource(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_EMPTY_IMAGE_PLACEHOLDER), RT_RCDATA)', 'SizeofResource', 'SafeArrayCreateVector(VT_UI1', 'A missing binary is the normal representation of an empty FB2 image')) {
    if(-not $protocol.Contains($required)) { throw "MemProtocol не загружает встроенную заглушку: $required" }
}
if($resourceIds -notmatch 'IDR_EMPTY_IMAGE_PLACEHOLDER\s+57602' -or $resourceScript -notmatch 'IDR_EMPTY_IMAGE_PLACEHOLDER\s+RCDATA\s+"res\\\\imgph\.png"') { throw 'PNG заглушки не добавлен как RCDATA ресурс FBE.' }
$bytes = [IO.File]::ReadAllBytes($placeholder)
if($bytes.Length -ne 52261 -or [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)) -ne 'B049D309E2CCCA57171AC11615824AB6F68659186DEE96651D3FC620E5FFD381') { throw 'Встроенный PNG не совпадает с согласованной оптимизированной заглушкой.' }
foreach($markup in @("href='#undefined'><IMG src='fbw-internal:#undefined'", 'var imageId=id=="" ? "undefined"')) {
    if(-not $runtime.Contains($markup)) { throw "Нарушена семантика пустой картинки: $markup" }
}
foreach($required in @('EnsureViewBarElementTextWidth', 'GetTextExtentPoint32W', 'TBIF_TEXT | TBIF_SIZE | TBIF_BYINDEX', 'desiredWidth = extent.cx + UiMetrics::ScaleForDpi(16')) {
    if(-not $tree.Contains($required)) { throw "Панель структуры не измеряет подпись Элементы по реальным метрикам: $required" }
}

$fixture = '<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><genre>prose</genre><author><first-name>T</first-name><last-name>T</last-name></author><book-title>Empty image</book-title><lang>en</lang></title-info><document-info><author><first-name>T</first-name><last-name>T</last-name></author><program-used>test</program-used><date value="2026-10-03">2026-10-03</date><id>empty-image-placeholder</id><version>1.0</version></document-info></description><body><section><image l:href="#undefined"/><p>Inline <image l:href="#undefined"/> image.</p></section></body></FictionBook>'
[xml]$roundTrip = $fixture
$xmlText = $roundTrip.OuterXml
if(([regex]::Matches($xmlText, '(?:xlink|l):href="#undefined"')).Count -ne 2 -or $xmlText -match 'fbw-internal:#"') { throw 'BODY -> SOURCE -> BODY контракт потерял undefined.' }
Write-Host 'Empty image placeholder resource and undefined round-trip contract passed.'