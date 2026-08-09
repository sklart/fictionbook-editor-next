$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path $PSScriptRoot)
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'runtime\main.js')
foreach($required in @('var failureCount=0;', 'var additionalFailureCount = failureCount > 3 ? failureCount - 3 : 0;', 'binary-failures=" + failureCount')) {
    if(-not $source.Contains($required)) { throw "Missing binary summary contract: $required" }
}
if(-not (Get-Command cscript.exe -ErrorAction SilentlyContinue)) { throw 'Windows Script Host cscript.exe is required for the PutBinaries behavioral test.' }
$match = [regex]::Match($source, '(?s)function PutBinaries\(doc\).*?\r?\n\}\r?\n//// == BODY ==')
if(-not $match.Success) { throw 'Could not extract PutBinaries from canonical runtime/main.js.' }
$functionBody = $match.Value -replace '\r?\n//// == BODY ==$', ''
$temp = Join-Path $env:TEMP ('fbe-put-binaries-{0}.js' -f [guid]::NewGuid().ToString('N'))
try {
    foreach($failures in @(0, 1, 3, 4, 10)) {
        $total = [Math]::Max(3, $failures)
        $binaries = [string]::Join('', (0..($total - 1) | ForEach-Object { $value = if($_ -lt $failures) { 'not-base64-!' } else { 'AQID' }; '<binary id="b{0}" content-type="image/png">{1}</binary>' -f $_, $value }))
        $harness = @"
var messages=[]; var summary=''; var adds=0; var ImagesInfo=[];
function TraceScript(){} function TraceVerboseOperation(){} function DiagError(){} function FillLists(){}
function MsgBox(text){messages.push(text);} function apiAddBinary(){adds++;}
function TraceDiagnosticSummary(code,text){summary=code+';'+text;}
var fbNS='http://www.gribuser.ru/xml/fictionbook/2.0';
$functionBody
var doc=new ActiveXObject('Msxml2.DOMDocument.6.0'); doc.async=false;
doc.setProperty('SelectionLanguage','XPath'); doc.setProperty('SelectionNamespaces','xmlns:fb="'+fbNS+'"');
if(!doc.loadXML('<FictionBook xmlns="'+fbNS+'">$binaries</FictionBook>')) WScript.Quit(2);
PutBinaries(doc); WScript.Echo('summary='+summary); WScript.Echo('messages='+messages.length); WScript.Echo('message-text='+messages.join('|')); WScript.Echo('adds='+adds);
"@
        [IO.File]::WriteAllText($temp, $harness, [Text.Encoding]::ASCII)
        $output = & cscript.exe //nologo $temp
        if($LASTEXITCODE -ne 0) { throw "cscript failed for $failures invalid binaries." }
        $text = [string]::Join("`n", $output)
        if($text -notmatch [regex]::Escape("binary-failures=$failures")) { throw "J699 did not report $failures failures: $text" }
        $expectedMessages = [Math]::Min(3, $failures) + $(if($failures -gt 3) { 1 } else { 0 })
        if($text -notmatch [regex]::Escape("messages=$expectedMessages")) { throw "Unexpected user-message count for $failures failures: $text" }
        if($failures -gt 3 -and $text -notmatch [regex]::Escape(($failures - 3).ToString() + ' more invalid images ignored')) { throw "Missing condensed warning for $failures failures: $text" }
    }
}
finally {
    Remove-Item -LiteralPath $temp -Force -ErrorAction SilentlyContinue
}
Write-Host 'PutBinaries behavioral JScript summary test passed.'
