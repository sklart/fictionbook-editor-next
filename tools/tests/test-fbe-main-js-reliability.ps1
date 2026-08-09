$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path $PSScriptRoot)
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'runtime\main.js')

if($source -match 'hasChildNodes(?!\()') { throw 'hasChildNodes must always be invoked.' }
foreach($required in @(
    'if (!list[0] || !list[0].value) return;',
    'var prevImageShowTimer = null;',
    'window.clearTimeout(prevImageShowTimer);',
    'window.setTimeout(function()',
    'for(var i=divs.length-1; i >= 0; i--)',
    'if(r.parentElement()!==elem',
    'return rng.parentElement();',
    'var re0=new RegExp("&","g");',
    'ImagesInfo.length=0;'
)) {
    if(-not $source.Contains($required)) { throw "Missing main.js reliability fix: $required" }
}
if($source -notmatch 'else\s+ShowPrevImage\("fbw-internal:"\+list\[0\]\.value\);') { throw 'ShowCoverImage must use preview mode only when requested.' }
if($source -match 'setTimeout\s*\(\s*[\x27\x22]') { throw 'Image preview must not use a string timer.' }
if($source -match 'for\(var i=0; i < divs\.length; i\+\+\)') { throw 'apiCleanUp must iterate a live collection backwards.' }
if($source -match 'imgs\[i\]\.src=""; imgs\[i\]\.src=pic_id; break') { throw 'Removing a binary must refresh every matching image.' }
Write-Host 'main.js reliability contracts passed.'
