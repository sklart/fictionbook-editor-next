$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path $PSScriptRoot)
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'runtime\main.js')

if(-not (Get-Command cscript.exe -ErrorAction SilentlyContinue)) { throw 'Windows Script Host cscript.exe is required for the main.js binary behavioral test.' }
$helpers = [regex]::Match($source, '(?s)function IsImageBinaryType\(type\).*?\r?\n}\r?\n\r?\n//--------------------------------------\r?\n// Adds a binary object').Value
$controls = [regex]::Match($source, '(?s)function BuildBinaryControls\(div, fullpath, id, type, data\).*?\r?\n}\r?\n\r?\nfunction SaveBinary\(binary\).*?\r?\n}').Value
$remove = [regex]::Match($source, '(?s)function Remove\(obj\).*?\r?\n}\r?\n//-------------------').Value
if($helpers.Length -eq 0 -or $controls.Length -eq 0 -or $remove.Length -eq 0) { throw 'Could not extract binary reliability handlers from canonical runtime/main.js.' }
$helpers = $helpers -replace '\r?\n//--------------------------------------\r?\n// Adds a binary object$', ''
$remove = $remove -replace '\r?\n//-------------------$', ''

$runner = Join-Path $env:TEMP ('fbe-main-js-binary-{0}.js' -f [guid]::NewGuid().ToString('N'))
try {
    $script = @"
var messages=[]; var ImagesInfo=[]; var rebuilds=0; var fills=0; var saved='';
function MsgBox(text){messages.push(text);}
function SaveImage(source){saved=source;}
function PutSpacers(){}
function assert(condition, text){if(!condition){WScript.Echo('FAIL: '+text); WScript.Quit(1);}}
function Input(value){this.value=value; this.attrs={}; this.setAttribute=function(name,val){this.attrs[name]=val;}; this.getAttribute=function(name){return this.attrs[name];};}
var binaries=[]; var references=[]; var covers=[];
var binobj={getElementsByTagName:function(){return binaries;}};
var document={all:{binobj:binobj},getElementsByTagName:function(name){if(name=='*') return references; if(name=='SELECT') return covers; return [];}};
var localized={"fbe.binary.id.empty":'empty',"fbe.binary.id.duplicate":'duplicate',"fbe.binary.delete.referenced":'referenced'};
var window={event:null,external:{GetImageDimsByData:function(){return '2x3';},GetBinarySize:function(){return 4;},GetImageDimsByPath:function(){return '';},GetLocalizedString:function(key){return localized[key];}}};
function Binary(id,type){var b={base64data:'AQID',all:{},parentNode:binobj,innerHTML:''}; b.all.id=new Input(id); b.all.type=new Input(type); b.all.id.setAttribute('oldId',id); b.all.id.parentNode=b; b.all.type.parentNode=b; b.getElementsByTagName=function(){return [b.all.id,b.all.type];}; b.removeNode=function(){for(var i=0;i<binaries.length;i++)if(binaries[i]===b)binaries.splice(i,1);}; return b;}
function Ref(href,src){this.attrs={href:href,src:src}; this.href=href; this.src=src; this.getAttribute=function(name){return this.attrs[name];}; this.setAttribute=function(name,value){this.attrs[name]=value; if(name=='href')this.href=value; if(name=='src')this.src=value;};}
$helpers
$controls
$remove
function RebuildImagesInfo(){ImagesInfo.length=0; rebuilds++; for(var i=0;i<binaries.length;i++){var b=binaries[i]; if(IsImageBinaryType(b.all.type.value)) ImagesInfo.push({id:b.all.id.value,src:'fbw-internal:#'+b.all.id.value,width:'2',height:'3'});}}
function FillLists(){fills++;}

var first=Binary('cover-part-01.jpg','image/jpeg'); var other=Binary('other.jpg','image/jpeg'); binaries.push(first); binaries.push(other);
BuildBinaryControls(first,'','cover-part-01.jpg','image/jpeg',first.base64data);
assert(first.innerHTML.split('id="dims"').length-1==1, 'image binary must contain exactly one dimensions control');
assert(first.all.id.onchange==OnBinaryChange && first.all.type.onchange==OnBinaryChange, 'control build must retain ID and Type handlers');
references.push(new Ref('#cover-part-01.jpg',null)); references.push(new Ref(null,'fbw-internal:#cover-part-01.jpg')); references.push(new Ref('#cover-part-01.jpg','fbw-internal:#cover-part-01.jpg'));
covers.push({value:'#cover-part-01.jpg'}); RebuildImagesInfo();
first.all.id.value='renamed.jpg'; window.event={srcElement:first.all.id}; OnBinaryChange();
assert(first.all.id.value=='renamed.jpg', 'rename must keep new ID');
assert(references[0].href=='#renamed.jpg' && references[1].src=='fbw-internal:#renamed.jpg' && references[2].href=='#renamed.jpg' && references[2].src=='fbw-internal:#renamed.jpg', 'rename must update every href and IMG src');
assert(covers[0].value=='#renamed.jpg', 'rename must update selected cover');
assert(ImagesInfo.length==2 && ImagesInfo[0].id=='renamed.jpg', 'rename must rebuild ImagesInfo');
SaveBinary(first); assert(saved=='fbw-internal:#renamed.jpg', 'Save must use the renamed binary ID');
first.all.id.value='other.jpg'; window.event={srcElement:first.all.id}; OnBinaryChange();
assert(first.all.id.value=='renamed.jpg' && messages.length==1, 'duplicate ID must be rejected and restored');
Remove(first); assert(binaries.length==2 && messages.length==2, 'used binary must not be deleted');
first.all.type.value='application/octet-stream'; window.event={srcElement:first.all.type}; OnBinaryChange();
assert(first.innerHTML.indexOf('id="show"')==-1 && first.innerHTML.indexOf('id="save"')==-1 && first.innerHTML.indexOf('id="dims"')==-1 && first.all.id.onchange==OnBinaryChange && first.all.type.onchange==OnBinaryChange && ImagesInfo.length==1 && ImagesInfo[0].id=='other.jpg', 'image to non-image must remove image controls and retain handlers');
first.all.type.value='image/png'; window.event={srcElement:first.all.type}; OnBinaryChange();
assert(first.innerHTML.indexOf('id="show"')!=-1 && first.innerHTML.indexOf('id="save"')!=-1 && first.innerHTML.split('id="dims"').length-1==1 && first.all.id.onchange==OnBinaryChange && first.all.type.onchange==OnBinaryChange && ImagesInfo.length==2 && ImagesInfo[0].id=='renamed.jpg', 'non-image to image must restore image controls, dimensions and handlers');
binaries.length=0; binaries.push(Binary('second-document.png','image/png')); RebuildImagesInfo();
assert(ImagesInfo.length==1 && ImagesInfo[0].id=='second-document.png', 'opening another document must not retain old ImagesInfo');
WScript.Echo('main.js binary behavioral test passed.');
"@
    [IO.File]::WriteAllText($runner, $script, [Text.Encoding]::ASCII)
    $output = & cscript.exe //nologo $runner
    if($LASTEXITCODE -ne 0) { throw "main.js binary behavioral JScript test failed: $([string]::Join("`n", $output))" }
    if(([string]::Join("`n", $output)) -notmatch 'main\.js binary behavioral test passed') { throw 'main.js binary behavioral test produced no success marker.' }
}
finally {
    Remove-Item -LiteralPath $runner -Force -ErrorAction SilentlyContinue
}
Write-Host 'main.js binary behavioral test passed.'
