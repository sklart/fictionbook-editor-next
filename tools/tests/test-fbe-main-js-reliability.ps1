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
    'ImagesInfo.length=0;',
    'function UpdateBinaryReferences(oldId, newId)',
    'function BinaryIsReferenced(id)',
    'function LocalizedBinaryMessage(key)',
    'fbe.binary.id.empty',
    'fbe.binary.id.duplicate',
    'fbe.binary.delete.referenced'
)) {
    if(-not $source.Contains($required)) { throw "Missing main.js reliability fix: $required" }
}
foreach($hardcoded in @('Binary ID must not be empty.', 'A binary with this ID already exists.', 'This binary is still used by the book or its cover and cannot be deleted.')) {
    if($source.Contains($hardcoded)) { throw "Binary editor message must be localized, not hardcoded: $hardcoded" }
}
if($source -notmatch 'else\s+ShowPrevImage\("fbw-internal:"\+list\[0\]\.value\);') { throw 'ShowCoverImage must use preview mode only when requested.' }
if($source -match 'setTimeout\s*\(\s*[\x27\x22]') { throw 'Image preview must not use a string timer.' }
if($source -match 'for\(var i=0; i < divs\.length; i\+\+\)') { throw 'apiCleanUp must iterate a live collection backwards.' }
if($source -match 'imgs\[i\]\.src=""; imgs\[i\]\.src=pic_id; break') { throw 'Removing a binary must refresh every matching image.' }

if(-not (Get-Command cscript.exe -ErrorAction SilentlyContinue)) { throw 'Windows Script Host cscript.exe is required for the main.js behavioral test.' }
$runner = Join-Path $env:TEMP ('fbe-main-js-reliability-{0}.js' -f [guid]::NewGuid().ToString('N'))
try {
    # main.js contains localized strings.  UTF-16 with BOM is the deterministic
    # input format for cscript.exe and avoids depending on the active ANSI page.
    $runnerScript = @'
var timerId=0, timers={}, clearedTimers=[], messages=[], previewCalls=[], fullCalls=[];
function assert(condition, message) { if(!condition) { WScript.Echo("FAIL: "+message); WScript.Quit(1); } }
function element(id) { return { id:id, style:{}, src:"", width:0, height:0, innerHTML:"", innerText:"", className:"", children:[], all:{}, appendChild:function(child){ this.children.push(child); child.parentElement=this; }, insertAdjacentElement:function(where, child){ this.inserted=child; child.parentElement=this; }, removeNode:function(){ if(this.collection) for(var n=0;n<this.collection.length;n++) if(this.collection[n]===this) { this.collection.splice(n,1); break; } } }; }
var elements={};
elements.prevImgPanel=element("prevImgPanel"); elements.prevImg=element("prevImg");
elements.fullImgPanel=element("fullImgPanel"); elements.fullImg=element("fullImg");
elements.css=element("css"); elements.css.href="main.css";
elements.fbw_body=element("fbw_body"); elements.fbw_body.innerHTML="BODY BEFORE XSL FAILURE";
elements.fbw_desc=element("fbw_desc"); elements.fbw_desc.innerHTML="DESC BEFORE XSL FAILURE";
var cleanupDivs=[];
var selectionRange={ text:"", htmlText:"", pasteHTML:function(value){this.pasted=value;}, parentElement:function(){return null;} };
var document={
  all:{ binobj:{getElementsByTagName:function(){return [];}}, tags:function(name){return name=="DIV" ? cleanupDivs : [];} },
  body:{scrollTop:0}, documentElement:{clientWidth:800,clientHeight:600,scrollTop:0,scrollLeft:0},
  selection:{type:"Text",empty:function(){},createRange:function(){return selectionRange;}},
  getElementById:function(id){return elements[id] || null;}, getElementsByTagName:function(){return [];},
  createElement:function(name){ return element(name); }
};
var window={ event:null, onerror:null, external:{
  TraceScript:function(){}, MsgBox:function(text){messages.push(text);}, GetNBSP:function(){return "\u00A0";},
  BeginUndoUnit:function(){}, EndUndoUnit:function(){}, inflateBlock:false, GetUUID:function(){return "test-id";},
  GetStylePath:function(){return "C:\\missing-xsl";}
}, setTimeout:function(callback){var id=++timerId; timers[id]=callback; return id;}, clearTimeout:function(id){clearedTimers.push(id); delete timers[id];} };
var event={srcElement:{offsetHeight:20},clientX:400,clientY:300};
function ActiveXObject(name) {
  if(name=="Msxml2.DOMDocument.6.0") return { async:false, preserveWhiteSpace:false, parseError:{errorCode:0}, firstChild:null, load:function(){return true;}, setProperty:function(){}, selectSingleNode:function(){return null;} };
  if(name=="Msxml2.XSLTemplate.6.0") return { stylesheet:null, createProcessor:function(){return null;} };
  if(name=="Msxml2.FreeThreadedDOMDocument.6.0") return { async:false, parseError:{errorCode:1,reason:"XSL unavailable",line:1,linepos:1}, documentElement:null, setProperty:function(){}, load:function(){return false;} };
  throw new Error("unexpected ActiveX class: "+name);
}
'@
    # These COM indexed-property writes are valid in MSHTML but not in the
    # standalone JScript parser.  They are irrelevant to the isolated DOM
    # checks below, so normalize only the temporary test copy.
    $runnerSource = $source -replace 'window\.external\.inflateBlock\([^\r\n]*?\)\s*=\s*true;', 'window.external.inflateBlock = true;'
    $runnerScript += $runnerSource
    $runnerScript += @'

// TextIntoHTML must escape every occurrence, not only the first special char.
assert(TextIntoHTML("&&<><>")=="&amp;&amp;&lt;&gt;&lt;&gt;", "TextIntoHTML escapes repeated ampersands and angle brackets");

// A blank cover does nothing; preview and full image take distinct paths.
var cover={getElementsByTagName:function(){return [{value:""}];}};
var realShowPrevImage=ShowPrevImage;
ShowPrevImage=function(source){previewCalls.push(source);}; ShowFullImage=function(source){fullCalls.push(source);};
ShowCoverImage(cover, false); assert(previewCalls.length==0 && fullCalls.length==0, "blank cover must not open an image");
cover={getElementsByTagName:function(){return [{value:"#cover.png"}];}};
ShowCoverImage(cover, false); ShowCoverImage(cover, true);
assert(previewCalls[0]=="fbw-internal:#cover.png" && fullCalls[0]=="fbw-internal:#cover.png", "cover preview and full image use their intended handlers");

// Restore real preview handlers and prove that leaving before the delay cancels it.
ShowPrevImage=realShowPrevImage;
ImagesInfo=[{src:"fbw-internal:#cover.png",width:100,height:80}];
ShowPrevImage("fbw-internal:#cover.png");
assert(prevImageShowTimer!=null && elements.prevImgPanel.style.visibility!="visible", "preview is delayed");
var scheduledPreview=prevImageShowTimer; HidePrevImage();
assert(prevImageShowTimer==null && clearedTimers[clearedTimers.length-1]==scheduledPreview && elements.prevImgPanel.style.visibility=="hidden", "preview timer is cancelled on mouse leave");

// apiCleanUp walks MSHTML's live collection backwards, so adjacent nodes survive no iteration skip.
cleanupDivs=[element("first"),element("second"),element("other")];
cleanupDivs[0].className="drop"; cleanupDivs[1].className="drop"; cleanupDivs[2].className="keep";
for(var cleanupIndex=0;cleanupIndex<cleanupDivs.length;cleanupIndex++) cleanupDivs[cleanupIndex].collection=cleanupDivs;
apiCleanUp("drop"); assert(cleanupDivs.length==1 && cleanupDivs[0].className=="keep", "apiCleanUp removes neighboring matching elements");

// AddTitle accepts an empty supported container and creates an empty title paragraph.
var emptyBody=element("empty-body"); emptyBody.tagName="DIV"; emptyBody.className="body"; emptyBody.firstChild=null; emptyBody.innerText="";
selectionRange.text=""; selectionRange.htmlText="";
GoTo=function(){};
AddTitle(emptyBody, false);
assert(emptyBody.inserted && emptyBody.inserted.className=="title" && emptyBody.inserted.children.length==1, "AddTitle inserts an empty title into an empty container");

// The author-tag loop must compare actual tag values, not array indexes.
var epigraphContainer=element("section"); epigraphContainer.tagName="DIV"; epigraphContainer.className="section"; epigraphContainer.firstChild=null;
var sourceParagraph={innerText:"quote",innerHTML:"quote",children:[{tagName:"EM"}],all:[]};
var authorParagraph={innerText:"author",innerHTML:"author",children:[],all:[{innerText:"author",tagName:"EM"}]};
var parsedParagraphs=[sourceParagraph,authorParagraph], createCount=0;
document.createElement=function(name){ var node=element(name); if(name=="DIV" && createCount++==0) node.getElementsByTagName=function(tag){return tag=="P" ? parsedParagraphs : [];}; return node; };
selectionRange.text="quote\r\nauthor"; selectionRange.htmlText="<P>quote</P><P>author</P>";
GoToEndOfElement=function(){}; InflateIt=function(){};
AddEpigraph(epigraphContainer, false);
assert(epigraphContainer.inserted && epigraphContainer.inserted.children.length==2 && epigraphContainer.inserted.children[1].className!="text-author", "AddEpigraph compares collected tag names rather than their indexes");

// Both a missing and a malformed XSL fail inside apiLoadFB2 without a second JS
// exception, restore CSS, and leave the already-rendered editor DOM untouched.
function assertXslFailure(label) {
  messages.length=0; elements.css.href="main.css"; elements.fbw_body.innerHTML="BODY BEFORE XSL FAILURE"; elements.fbw_desc.innerHTML="DESC BEFORE XSL FAILURE";
  var result=apiLoadFB2(label, "english");
  assert(result===false, label+" must report a controlled load failure");
  assert(messages.length==2, label+" reports the XSL problem and the controlled Body-mode failure only");
  assert(elements.css.href=="main.css", label+" restores CSS after failure");
  assert(elements.fbw_body.innerHTML=="BODY BEFORE XSL FAILURE" && elements.fbw_desc.innerHTML=="DESC BEFORE XSL FAILURE", label+" leaves the editor DOM intact");
}
assertXslFailure("missing.xsl"); assertXslFailure("broken.xsl");
WScript.Echo("main.js behavioral reliability test passed.");
'@
    [IO.File]::WriteAllText($runner, $runnerScript, [Text.Encoding]::Unicode)
    $output = & cscript.exe //nologo $runner
    $outputText = [string]($output -join "`n")
    if($LASTEXITCODE -ne 0) { throw "main.js behavioral JScript test failed: $outputText" }
    if($outputText -notmatch 'main\.js behavioral reliability test passed') { throw 'main.js behavioral JScript test produced no success marker.' }
}
finally {
    Remove-Item -LiteralPath $runner -Force -ErrorAction SilentlyContinue
}
Write-Host 'main.js reliability contracts passed.'
