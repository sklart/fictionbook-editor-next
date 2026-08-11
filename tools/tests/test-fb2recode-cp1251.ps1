$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'runtime\Utilities\fb2recode\fb2recode.js')
$accepted = 'ЂЃѓЉљЊњЋћЌќЏџЎўЄєЇїІіҐґЁё«»„“”—–…№ €'
foreach($character in $accepted.ToCharArray()) {
    if($source.IndexOf($character) -lt 0) { throw "В CP1251 mapping отсутствует U+$([int][char]$character).ToString('X4'): $character" }
}
if($source.IndexOf('˜') -ge 0) { throw 'Undefined CP1251 byte 0x98 не должен отображаться как U+02DC.' }
if($source -notmatch 'isCp1251Character') { throw 'Не найдена точная проверка CP1251 mapping.' }
Write-Host 'FB2Recode exact CP1251 mapping passed.'
