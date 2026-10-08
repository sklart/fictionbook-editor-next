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
$master = Join-Path $repoRoot 'src\fbe\res\imgph-master.png'

if($protocol -match 'U::LoadFile\(' -or $protocol -match 'GetProgDirFile\(.*imgph') { throw 'Пустая картинка всё ещё зависит от imgph.png рядом с EXE.' }
foreach($required in @('FindResource(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_EMPTY_IMAGE_PLACEHOLDER), RT_RCDATA)', 'SizeofResource', 'SafeArrayCreateVector(VT_UI1', 'A missing binary is the normal representation of an empty FB2 image')) {
    if(-not $protocol.Contains($required)) { throw "MemProtocol не загружает встроенную заглушку: $required" }
}
if($resourceIds -notmatch 'IDR_EMPTY_IMAGE_PLACEHOLDER\s+57602' -or $resourceScript -notmatch 'IDR_EMPTY_IMAGE_PLACEHOLDER\s+RCDATA\s+"res\\\\imgph\.png"') { throw 'PNG заглушки не добавлен как RCDATA ресурс FBE.' }
$bytes = [IO.File]::ReadAllBytes($placeholder)
$masterBytes = if(Test-Path -LiteralPath $master -PathType Leaf) { [IO.File]::ReadAllBytes($master) } else { @() }
if($bytes.Length -lt 8 -or $masterBytes.Length -lt 8 -or [Convert]::ToHexString($bytes[0..7]) -ne '89504E470D0A1A0A' -or [Convert]::ToHexString($masterBytes[0..7]) -ne '89504E470D0A1A0A') { throw 'Runtime placeholder и его master-версия должны быть корректными PNG-файлами.' }
foreach($markup in @("href='#undefined'><IMG src='fbw-internal:#undefined'", 'var imageId=id=="" ? "undefined"')) {
    if(-not $runtime.Contains($markup)) { throw "Нарушена семантика пустой картинки: $markup" }
}
foreach($required in @('EnsureViewBarElementMetrics', 'GetTextExtentPoint32W', 'TB_GETBUTTONSIZE', 'TB_SETBUTTONSIZE', 'TBIF_TEXT | TBIF_SIZE | TBIF_BYINDEX', 'const LRESULT result = ::SendMessage', "result == -1 || text[0] == L'\0'", 'desiredWidth = extent.cx + UiMetrics::ScaleForDpi(16')) {
    if(-not $tree.Contains($required)) { throw "Панель структуры не измеряет подпись Элементы по реальным метрикам: $required" }
}
if($tree -notmatch 'RefreshLocalizedMenuCaptions\(\)[\s\S]*?EnsureViewBarElementMetrics\(\);') { throw 'Панель структуры не пересчитывает размеры после локализации.' }
Write-Host 'Empty image placeholder resource and undefined round-trip contract passed.'
