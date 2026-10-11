$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$source = Get-Content -Raw -LiteralPath (Join-Path $root 'runtime\main.js')
$first = $source.Substring($source.IndexOf('function GetCP(cp)'), $source.IndexOf('var imgcode=') - $source.IndexOf('function GetCP(cp)'))
$start = $source.IndexOf('function StructuralFailure(')
$second = $source.Substring($start, $source.IndexOf('function ImgSetURL(') - $start)
if(-not $first -or -not $second) { throw 'Structural command functions were not found.' }
$runner = Join-Path $env:TEMP ('fbe-structural-{0}.js' -f [guid]::NewGuid().ToString('N'))
try {
    $script = @'
function need(ok, message) { if(!ok) { WScript.Echo("FAIL "+message); WScript.Quit(1); } }
var messages=[], undo=0, failMove=false;
var window={external:{
 BeginUndoUnit:function(){++undo;}, EndUndoUnit:function(){--undo;},
 GetLocalizedString:function(key){return key;}, MsgBox:function(message){messages.push(message);}
}};
function refresh(parent) {
 parent.firstChild=parent.children.length ? parent.children[0] : null;
 for(var i=0;i<parent.children.length;++i) {
  parent.children[i].parentNode=parent;
  parent.children[i].nextSibling=i+1<parent.children.length ? parent.children[i+1] : null;
 }
}
function detach(child) {
 if(!child.parentNode) return;
 var parent=child.parentNode;
 for(var i=0;i<parent.children.length;++i) if(parent.children[i]===child) { parent.children.splice(i,1); break; }
 child.parentNode=null; child.nextSibling=null; refresh(parent);
}
function node(name, cls, id) {
 var self={nodeType:1,nodeName:name,tagName:name,className:cls||"",id:id||"",attributes:[],
   children:[],firstChild:null,nextSibling:null,parentNode:null,innerHTML:"",
   getAttribute:function(key){return this[key]||null;},
   setAttribute:function(key,value){this[key]=value;},
   appendChild:function(child){if(failMove) {failMove=false; throw new Error("injected move failure");} detach(child); this.children.push(child); refresh(this); return child;},
   insertAdjacentElement:function(where,child){
     if(failMove) {failMove=false; throw new Error("injected move failure");}
     var parent=where=="beforeBegin" || where=="afterEnd" ? this.parentNode : this;
     need(!!parent,"insertion parent");
     var index=0;
     if(where=="beforeBegin" || where=="afterEnd") {
       for(index=0;index<parent.children.length;++index) if(parent.children[index]===this) break;
       if(where=="afterEnd") ++index;
     } else if(where=="beforeEnd") index=parent.children.length;
     detach(child); parent.children.splice(index,0,child); refresh(parent);
   },
   removeNode:function(){detach(this);}
 };
 if(id) self.attributes.push({specified:true,nodeName:"id"});
 return self;
}
function add(parent, child) {parent.appendChild(child); return child;}
function p(cls) {return node("P",cls||"");}
function section() {return node("DIV","section");}
var document={createElement:function(name){return node(name,"");}};
function MsgBox(message){messages.push(message);}
function GetLocalizedStringOrDefault(key){return key;}
function InflateIt(){}
var gotoNode=null;
function GoTo(value){gotoNode=value;}
'@
    $script += $first + $second
    $script += @'
var body=node("DIV","body"), outer=add(body,section()), a=add(outer,section()), b=add(outer,section());
need(RemoveOuterContainer(body,true)===false,"body unavailable");
need(RemoveOuterContainer(body,false)===false && body.children.length==1,"body not removed");
need(RemoveOuterContainer(outer,true)===true,"pure outer available");
need(RemoveOuterContainer(outer,false)===true,"pure outer removed");
need(body.children.length==2 && body.children[0]===a && body.children[1]===b,"child order retained");
need(undo==0 && gotoNode===a,"remove undo closed and caret moved");

body=node("DIV","body"); outer=add(body,section()); add(outer,node("DIV","title")); add(outer,section());
need(RemoveOuterContainer(outer,true)===false && body.children[0]===outer,"title refusal");
need(RemoveOuterContainer(outer,false)===false && messages.length>0,"title diagnostic");
outer=add(body,section()); outer.id="outer-id"; outer.attributes.push({specified:true,nodeName:"id"}); add(outer,section());
need(RemoveOuterContainer(outer,false)===false && body.children[1]===outer,"outer ID refusal");

body=node("DIV","body"); a=add(body,section()); a.id="original"; a.attributes.push({specified:true,nodeName:"id"}); add(a,p());
need(CloneContainer(a,false)===true,"clone succeeds");
need(body.children.length==2 && body.children[1].id==="" && body.children[1].className=="section","clone does not copy ID");
need(undo==0,"clone undo closed");

body=node("DIV","body"); a=add(body,section()); b=add(body,section()); add(a,p()); add(b,p());
need(MergeContainers(a,true)===true,"text merge available");
need(MergeContainers(a,false)===true && body.children.length==1 && a.children.length==2,"text merge order");
need(undo==0 && gotoNode===a,"merge undo closed and caret moved");

body=node("DIV","body"); a=add(body,section()); b=add(body,section()); add(a,section()); add(b,section());
need(MergeContainers(a,false)===true && a.children.length==2,"nested merge");
body=node("DIV","body"); a=add(body,section()); b=add(body,section()); add(a,p()); add(b,section());
need(MergeContainers(a,false)===false && body.children.length==2,"mixed section refusal");
body=node("DIV","body"); a=add(body,section()); b=add(body,section()); add(a,p()); add(b,p()); b.id="target"; b.attributes.push({specified:true,nodeName:"id"});
need(MergeContainers(a,false)===false && body.children.length==2,"second ID refusal");
body=node("DIV","body"); a=add(body,section()); b=add(body,section()); add(a,p()); add(b,node("DIV","epigraph")); add(b,p());
need(MergeContainers(a,false)===false && b.children.length==2,"epigraph preserved on refusal");
body=node("DIV","body"); a=add(body,section()); b=add(body,section()); add(a,p()); add(b,node("DIV","image")); add(b,p());
need(MergeContainers(a,false)===false && b.children.length==2,"second section header image refused");
body=node("DIV","body"); a=add(body,section()); b=add(body,section()); add(a,p()); add(b,p()); add(b,node("DIV","image"));
need(MergeContainers(a,false)===true && a.children[2].className=="image","inline image retained during merge");
body=node("DIV","body"); a=add(body,section()); b=add(body,section()); add(a,p()); add(b,node("TABLE","table"));
need(MergeContainers(a,false)===true && a.children[1].tagName=="TABLE","native table retained during merge");
body=node("DIV","body"); a=add(body,section()); b=add(body,section()); add(a,node("DIV","image")); add(a,node("DIV","image")); add(a,p()); add(b,p());
need(MergeContainers(a,false)===false && body.children.length==2,"duplicate header image rejected");

body=node("DIV","body"); a=add(body,node("DIV","cite")); b=add(body,node("DIV","cite")); add(a,p()); add(a,p("text-author")); add(b,p());
need(MergeContainers(a,false)===false && a.children.length==2,"author-before-text refusal");
body=node("DIV","body"); a=add(body,node("DIV","stanza")); b=add(body,node("DIV","stanza")); add(a,p()); add(b,p());
need(MergeContainers(a,false)===true && a.children.length==2,"stanza lines retained");

body=node("DIV","body"); a=add(body,section()); b=add(body,section()); add(a,p()); add(b,p()); failMove=true;
need(MergeContainers(a,false)===false && a.children.length==1 && b.children.length==1 && undo==0,"failed move keeps DOM and closes undo");
WScript.Echo("structural command behavior passed");
'@
    [IO.File]::WriteAllText($runner, $script, [Text.Encoding]::Unicode)
    & cscript.exe //nologo $runner
    if($LASTEXITCODE -ne 0) { throw "Structural command behavior failed: $LASTEXITCODE" }
} finally {
    Remove-Item -LiteralPath $runner -ErrorAction SilentlyContinue
}
