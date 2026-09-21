<#
.SYNOPSIS
Guards structural block-image insertion against reintroducing pasteHTML of a DIV.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$main = Get-Content -Raw -LiteralPath (Join-Path $root 'runtime\main.js')
$insImage = [regex]::Match($main, '(?s)function InsImage\(check, id\).*?}\r?\n//-+').Value
if ([string]::IsNullOrWhiteSpace($insImage)) { throw 'Не найдена функция InsImage.' }
if ($insImage -match 'pasteHTML\s*\(\s*["'']<DIV') { throw 'InsImage не должен вставлять block DIV через pasteHTML.' }
foreach ($required in @('fbe-block-image-marker-', 'InsertBlockImageAtMarker(marker,id)', 'return inserted;')) {
    if ($insImage.IndexOf($required, [StringComparison]::Ordinal) -lt 0) { throw "В InsImage отсутствует structural insertion contract: $required" }
}
foreach ($required in @('MoveFollowingSiblings(source,copy)', 'parent.cloneNode(false)', 'RemoveEmptyInlineFormatting', 'paragraph.insertAdjacentElement("afterEnd",block)', 'paragraph.insertAdjacentElement("beforeBegin",block)')) {
    if ($main.IndexOf($required, [StringComparison]::Ordinal) -lt 0) { throw "Отсутствует защита форматирования block image: $required" }
}
if ($main -notmatch 'function InsInlineImage\(check, id\)[\s\S]*?rng\.pasteHTML\(') { throw 'Inline image должен сохранить отдельный pasteHTML-путь.' }
Write-Host 'Block-image structural insertion contract passed.'
