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
if ($insImage -match 'pasteHTML\s*\(' -or $insImage -match 'fbe-block-image-marker-') { throw 'InsImage не должен использовать marker/pasteHTML-путь для block DIV.' }
foreach ($required in @('var cp=rng.parentElement()', 'var whole=document.body.createTextRange()', 'operationError=error', 'if(!operationError && cleanupError) throw cleanupError;', 'return inserted;')) {
    if ($insImage.IndexOf($required, [StringComparison]::Ordinal) -lt 0) { throw "В InsImage отсутствует structural insertion contract: $required" }
}
if ($insImage -notmatch 'if\(undoStarted\)\s*\{[\s\S]*?window\.external\.EndUndoUnit\(document\);') { throw 'EndUndoUnit должен быть гарантированно закрыт в finally.' }
foreach ($required in @('whole.moveToElementText(paragraph)', 'left.setEndPoint("EndToStart",rng)', 'right.setEndPoint("StartToEnd",rng)', 'var leftHTML=left.htmlText', 'var rightHTML=right.htmlText', 'paragraph.cloneNode(false)', 'rightPart.removeAttribute("id")', 'paragraph.innerHTML=leftHTML', 'rightPart.innerHTML=rightHTML', 'InflateIt(paragraph)', 'InflateIt(rightPart)', 'paragraph.insertAdjacentHTML("beforeBegin",imageHtml)', 'paragraph.insertAdjacentHTML("afterEnd",imageHtml)', 'MoveCaretToParagraphStart(rightPart)')) {
    if ($main.IndexOf($required, [StringComparison]::Ordinal) -lt 0) { throw "Отсутствует защита форматирования block image: $required" }
}
$imageHtml = [regex]::Match($main, '(?s)function BlockImageHTML\(id\).*?\r?\n}').Value
if ([string]::IsNullOrWhiteSpace($imageHtml)) { throw 'Не найдена генерация HTML для DIV.image.' }
foreach ($required in @("class='image'", "href='#")) {
    if ($imageHtml.IndexOf($required, [StringComparison]::Ordinal) -lt 0) { throw "HTML DIV.image не соответствует contract: $required" }
}
if ($main -notmatch 'function InsInlineImage\(check, id\)[\s\S]*?rng\.pasteHTML\(') { throw 'Inline image должен сохранить отдельный pasteHTML-путь.' }
Write-Host 'Block-image structural insertion contract passed.'
