$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path $PSScriptRoot)
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'runtime\main.js')
if ($source -notmatch '(?s)function GetBinaries\(doc\).*?LocalizedBinaryMessage\("fbe\.binary\.processing_images"\).*?bo\.length>=50.*?\(i\+1\)%progressStep.*?SetStatusBarText\(progressText\.replace\("\{0\}",i\+1\)\.replace\("\{1\}",bo\.length\)\)') {
    throw 'GetBinaries должен показывать throttled progress при сохранении большого числа изображений.'
}

if(-not (Get-Command cscript.exe -ErrorAction SilentlyContinue)) { throw 'Windows Script Host cscript.exe is required for the main.js binary behavioral test.' }
$renameHandler = [regex]::Match($source, '(?s)function UpdateBinaryReferences\(oldId, newId\).*?(?=function RestoreBinaryReferenceSelects)').Value
if ($renameHandler -notmatch 'var elements=document\.all;') {
    throw 'Переименование binary должно обходить document.all, совместимый с MSHTML document mode.'
}
if ($renameHandler -match 'var\s+elements\s*=\s*document\.getElementsByTagName\("\*"\)') {
    throw 'Переименование binary не должно использовать неподдерживаемый MSHTML getElementsByTagName("*").'
}
$helpers = [regex]::Match($source, '(?s)function IsImageBinaryType\(type\).*?\r?\n}\r?\n\r?\n//--------------------------------------\r?\n// Adds a binary object').Value
$controls = [regex]::Match($source, '(?s)function BuildBinaryControls\(div, fullpath, id, type, data\).*?\r?\n}\r?\n\r?\nfunction SaveBinary\(binary\).*?\r?\n}').Value
$addBinary = [regex]::Match($source, '(?s)function apiAddBinary\(fullpath, id, type, data\).*?\r?\n}\r?\n\r?\nfunction GetImageData').Value
$remove = [regex]::Match($source, '(?s)function Remove\(obj\).*?\r?\n}\r?\n//-------------------').Value
if($helpers.Length -eq 0 -or $controls.Length -eq 0 -or $addBinary.Length -eq 0 -or $remove.Length -eq 0) { throw 'Could not extract binary reliability handlers from canonical runtime/main.js.' }
$helpers = $helpers -replace '\r?\n//--------------------------------------\r?\n// Adds a binary object$', ''
$addBinary = $addBinary -replace '\r?\n\r?\nfunction GetImageData$', ''
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
var binaries=[]; var references=[]; var covers=[]; var allElements=[];
var binobj={getElementsByTagName:function(){return binaries;},appendChild:function(binary){binaries.push(binary);}};
allElements.binobj=binobj;
var document={all:allElements,createElement:function(){return Binary('','');},getElementsByTagName:function(name){if(name=='*') throw new Error('Operation is not supported'); if(name=='SELECT') return covers; return [];}};
var localized={"fbe.binary.id.empty":'empty',"fbe.binary.id.duplicate":'duplicate',"fbe.binary.delete.referenced":'referenced',"fbe.binary.delete.check_failed":'check failed',"fbe.binary.processing_images":'Processing images: {0} / {1}'};
var window={event:null,external:{GetImageDimsByData:function(){return '2x3';},GetBinarySize:function(){return 4;},GetImageDimsByPath:function(){return '';},GetLocalizedString:function(key){return localized[key];}}};
function Binary(id,type){var b={base64data:'AQID',all:{},parentNode:binobj,innerHTML:''}; b.all.id=new Input(id); b.all.type=new Input(type); b.all.id.setAttribute('oldId',id); b.all.id.parentNode=b; b.all.type.parentNode=b; b.getElementsByTagName=function(){return [b.all.id,b.all.type];}; b.removeNode=function(){for(var i=0;i<binaries.length;i++)if(binaries[i]===b)binaries.splice(i,1);}; return b;}
function Ref(href,src){this.attrs={href:href,src:src}; this.href=href; this.src=src; this.tagName='IMG'; this.getAttribute=function(name){return name=="href" ? this.href : name=="src" ? this.src : this.attrs[name];}; this.setAttribute=function(name,value){this.attrs[name]=value; if(name=='href')this.href=value; if(name=='src')this.src=value;}; allElements.push(this);}
function Cover(value){this.value=value; this.options=[]; this.tagName='SELECT'; this.getElementsByTagName=function(name){return name=='OPTION' ? this.options : [];}; allElements.push(this);}
$helpers
$controls
$addBinary
$remove
function RebuildImagesInfo(){ImagesInfo.length=0; rebuilds++; for(var i=0;i<binaries.length;i++){var b=binaries[i]; if(IsImageBinaryType(b.all.type.value)) ImagesInfo.push({id:b.all.id.value,src:'fbw-internal:#'+b.all.id.value,width:'2',height:'3'});}}
function FillLists(){fills++; for(var i=0;i<covers.length;i++){covers[i].options=[]; for(var j=0;j<binaries.length;j++){if(IsImageBinaryType(binaries[j].all.type.value)) covers[i].options.push({value:'#'+binaries[j].all.id.value});}}}

var first=Binary('cover-part-01.jpg','image/jpeg'); var other=Binary('other.jpg','image/jpeg'); binaries.push(first); binaries.push(other);
BuildBinaryControls(first,'','cover-part-01.jpg','image/jpeg',first.base64data);
assert(first.innerHTML.split('id="dims"').length-1==1, 'image binary must contain exactly one dimensions control');
assert(first.all.id.onchange==OnBinaryChange && first.all.type.onchange==OnBinaryChange, 'control build must retain ID and Type handlers');
references.push(new Ref('#cover-part-01.jpg',null)); references.push(new Ref(null,'fbw-internal:#cover-part-01.jpg')); references.push(new Ref('#cover-part-01.jpg','fbw-internal:#cover-part-01.jpg'));
covers.push(new Cover('#cover-part-01.jpg')); RebuildImagesInfo();
first.all.id.value='renamed.jpg'; window.event={srcElement:first.all.id}; OnBinaryChange();
assert(first.all.id.value=='renamed.jpg', 'rename must keep new ID');
assert(references[0].href=='#renamed.jpg' && references[1].src=='fbw-internal:#renamed.jpg' && references[2].href=='#renamed.jpg' && references[2].src=='fbw-internal:#renamed.jpg', 'rename must update every href and IMG src');
assert(covers[0].value=='#renamed.jpg', 'rename must update selected cover');
assert(ImagesInfo.length==2 && ImagesInfo[0].id=='renamed.jpg', 'rename must rebuild ImagesInfo');
SaveBinary(first); assert(saved=='fbw-internal:#renamed.jpg', 'Save must use the renamed binary ID');
first.all.id.value='other.jpg'; window.event={srcElement:first.all.id}; OnBinaryChange();
assert(first.all.id.value=='renamed.jpg' && messages.length==1, 'duplicate ID must be rejected and restored');
var rejectedIds=['','1starts-digit','contains space','bad!symbol','-starts-dash','.starts-dot'];
for(var rejected=0;rejected<rejectedIds.length;rejected++){var previousCover=covers[0].value;var previousHref=references[0].href;first.all.id.value=rejectedIds[rejected]; window.event={srcElement:first.all.id}; OnBinaryChange(); assert(first.all.id.value=='renamed.jpg' && covers[0].value==previousCover && references[0].href==previousHref, 'invalid ID must restore ID, references and cover: '+rejectedIds[rejected]);}
first.all.id.value='_cover-1.2'; window.event={srcElement:first.all.id}; OnBinaryChange();
assert(first.all.id.value=='_cover-1.2' && covers[0].value=='#_cover-1.2', 'underscore-start XML ID with dash and dot must be accepted');
first.all.id.value='\u041e\u0431\u043b\u043e\u0436\u043a\u0430_1-2.jpg'; window.event={srcElement:first.all.id}; OnBinaryChange();
assert(first.all.id.value=='\u041e\u0431\u043b\u043e\u0436\u043a\u0430_1-2.jpg' && covers[0].value=='#\u041e\u0431\u043b\u043e\u0436\u043a\u0430_1-2.jpg', 'Unicode XML ID must be accepted and cover selection restored after list rebuild');
first.all.id.value='renamed.jpg'; window.event={srcElement:first.all.id}; OnBinaryChange();
Remove(first); assert(binaries.length==2 && messages.length==8, 'used binary must not be deleted');
var beforeImages=ImagesInfo.length, beforeFills=fills;
Remove(first); assert(binaries.length==2 && ImagesInfo.length==beforeImages && fills==beforeFills, 'rejected removal must not change image metadata or cover lists');
var unused=Binary('unused.png','image/png'); binaries.push(unused); RebuildImagesInfo();
Remove(unused); assert(binaries.length==2 && ImagesInfo.length==2 && fills==beforeFills+1, 'unused binary must be removed and cover lists updated');
var savedElements=allElements.slice(0); allElements.length=0;
allElements.push(covers[0]);
assert(BinaryIsReferenced('renamed.jpg'), 'selected cover alone must protect binary');
allElements.length=0; allElements.push(references[0]);
assert(BinaryIsReferenced('renamed.jpg'), 'href alone must protect binary');
allElements.length=0; allElements.push(references[1]);
assert(BinaryIsReferenced('renamed.jpg'), 'src alone must protect binary');
allElements.length=0; allElements.push({getAttribute:function(){throw new Error('DOM failure');}});
beforeFills=fills; var beforeMessages=messages.length;
Remove(other); assert(binaries.length==2 && ImagesInfo.length==2 && fills==beforeFills && messages.length==beforeMessages+1 && messages[messages.length-1]=='check failed', 'failed reference check must reject deletion without modifying document');
allElements.length=0; for(var restore=0;restore<savedElements.length;restore++)allElements.push(savedElements[restore]);
first.all.type.value='application/octet-stream'; window.event={srcElement:first.all.type}; OnBinaryChange();
assert(first.innerHTML.indexOf('id="show"')==-1 && first.innerHTML.indexOf('id="save"')==-1 && first.innerHTML.indexOf('id="dims"')==-1 && first.all.id.onchange==OnBinaryChange && first.all.type.onchange==OnBinaryChange && ImagesInfo.length==1 && ImagesInfo[0].id=='other.jpg', 'image to non-image must remove image controls and retain handlers');
first.all.type.value='image/png'; window.event={srcElement:first.all.type}; OnBinaryChange();
assert(first.innerHTML.indexOf('id="show"')!=-1 && first.innerHTML.indexOf('id="save"')!=-1 && first.innerHTML.split('id="dims"').length-1==1 && first.all.id.onchange==OnBinaryChange && first.all.type.onchange==OnBinaryChange && ImagesInfo.length==2 && ImagesInfo[0].id=='renamed.jpg', 'non-image to image must restore image controls, dimensions and handlers');
binaries.length=0; binaries.push(Binary('second-document.png','image/png')); RebuildImagesInfo();
assert(ImagesInfo.length==1 && ImagesInfo[0].id=='second-document.png', 'opening another document must not retain old ImagesInfo');
binaries.length=0; references.length=0;
var generated=[];
generated.push(apiAddBinary('', 'cover-part-01.jpg', 'image/jpeg', 'AQID'));
generated.push(apiAddBinary('', '_image.jpg', 'image/jpeg', 'AQID'));
generated.push(apiAddBinary('', '_01-image.jpg', 'image/jpeg', 'AQID'));
generated.push(apiAddBinary('', 'image_01.jpg', 'image/jpeg', 'AQID'));
generated.push(apiAddBinary('', '\u041e\u0431\u043b\u043e\u0436\u043a\u0430.jpg', 'image/jpeg', 'AQID'));
var collision=apiAddBinary('', 'image_01.jpg', 'image/jpeg', 'AQID');
assert(generated[0]=='cover-part-01.jpg' && generated[1]=='_image.jpg' && generated[2]=='_01-image.jpg' && generated[3]=='image_01.jpg' && generated[4]=='\u041e\u0431\u043b\u043e\u0436\u043a\u0430.jpg', 'generated valid IDs must be preserved');
assert(collision=='image_01.jpg_0', 'normalized binary ID collision must be unique');
references.push(new Ref('#'+collision, 'fbw-internal:#'+collision));
assert(references[0].href=='#image_01.jpg_0' && references[0].src=='fbw-internal:#image_01.jpg_0', 'generated binary ID must match href and src references');
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
