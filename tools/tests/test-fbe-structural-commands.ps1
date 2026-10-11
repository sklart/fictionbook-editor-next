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
var messages=[], undo=0, fault=null, rollbackFault=false;
function failurePhase(kind) {
 if(!fault || fault.kind!=kind) return "";
 if(++fault.count!=fault.nth) return "";
 var phase=fault.phase; fault=null; return phase;
}
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
   appendChild:function(child){
     var phase=failurePhase("appendChild");
     if(phase=="before") throw new Error("injected append failure");
     detach(child); this.children.push(child); refresh(this);
     if(phase=="after") throw new Error("injected append failure");
     return child;
   },
   insertBefore:function(child,ref){
     if(rollbackFault) {rollbackFault=false; throw new Error("injected rollback failure");}
     need(ref.parentNode===this,"insertBefore reference");
     detach(child);
     var index=0; for(;index<this.children.length;++index) if(this.children[index]===ref) break;
     this.children.splice(index,0,child); refresh(this); return child;
   },
   removeChild:function(child){need(child.parentNode===this,"removeChild parent"); detach(child); return child;},
   insertAdjacentElement:function(where,child){
     var phase=failurePhase("insertAdjacentElement");
     if(phase=="before") throw new Error("injected insertion failure");
     var parent=where=="beforeBegin" || where=="afterEnd" ? this.parentNode : this;
     need(!!parent,"insertion parent");
     var index=0;
     if(where=="beforeBegin" || where=="afterEnd") {
       for(index=0;index<parent.children.length;++index) if(parent.children[index]===this) break;
       if(where=="afterEnd") ++index;
     } else if(where=="beforeEnd") index=parent.children.length;
     detach(child); parent.children.splice(index,0,child); refresh(parent);
     if(phase=="after") throw new Error("injected insertion failure");
   },
   removeNode:function(){
     var phase=failurePhase("removeNode");
     if(phase=="before") throw new Error("injected removal failure");
     detach(this);
     if(phase=="after") throw new Error("injected removal failure");
   }
 };
 if(id) self.attributes.push({specified:true,nodeName:"id"});
 return self;
}
function add(parent, child) {parent.appendChild(child); return child;}
function p(cls) {return node("P",cls||"");}
function section() {return node("DIV","section");}
function same(parent, expected) {
 if(parent.children.length!=expected.length) return false;
 for(var i=0;i<expected.length;++i)
  if(parent.children[i]!==expected[i] || expected[i].parentNode!==parent ||
     expected[i].nextSibling!==(i+1<expected.length ? expected[i+1] : null)) return false;
 return true;
}
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

body=node("DIV","body"); a=add(body,section()); b=add(body,section()); add(a,p()); add(b,p());
fault={kind:"appendChild",nth:1,count:0,phase:"before"};
need(MergeContainers(a,false)===false && a.children.length==1 && b.children.length==1 && undo==0,"failed move keeps DOM and closes undo");

for(var move=1;move<=2;++move) {
 body=node("DIV","body"); var before=add(body,section()), first=add(body,section()), second=add(body,section()), after=add(body,section());
 var firstLine=add(first,p()), one=add(second,p()), two=add(second,p()), three=add(second,p());
 fault={kind:"appendChild",nth:move,count:0,phase:"after"};
 need(MergeContainers(first,false)===false,"merge fails after moved node "+move);
 need(same(body,[before,first,second,after]) && same(first,[firstLine]) && same(second,[one,two,three]),"merge restores exact node order "+move);
 need(undo==0 && messages[messages.length-1]==="fbe.structure.operation.failed","merge closes undo and confirms restoration "+move);
}

for(var move=1;move<=2;++move) {
 body=node("DIV","body"); before=add(body,section()); outer=add(body,section()); after=add(body,section());
 one=add(outer,section()); two=add(outer,section()); three=add(outer,section());
 fault={kind:"insertAdjacentElement",nth:move,count:0,phase:"after"};
 need(RemoveOuterContainer(outer,false)===false,"remove outer fails after moved node "+move);
 need(same(body,[before,outer,after]) && same(outer,[one,two,three]),"remove outer restores exact node order "+move);
 need(undo==0 && messages[messages.length-1]==="fbe.structure.operation.failed","remove outer closes undo "+move);
}

body=node("DIV","body"); before=add(body,section()); outer=add(body,section()); after=add(body,section());
one=add(outer,section()); two=add(outer,section());
fault={kind:"removeNode",nth:1,count:0,phase:"after"};
need(RemoveOuterContainer(outer,false)===false,"remove outer fails after shell removal");
need(same(body,[before,outer,after]) && same(outer,[one,two]) && undo==0,"outer shell and children restored after removal failure");

body=node("DIV","body"); before=add(body,section()); a=add(body,section()); after=add(body,section()); add(a,p());
fault={kind:"insertAdjacentElement",nth:1,count:0,phase:"after"};
need(CloneContainer(a,false)===false && same(body,[before,a,after]) && undo==0,"clone insertion failure leaves no partial node");
need(messages[messages.length-1]==="fbe.structure.operation.failed","clone confirms restoration");

body=node("DIV","body"); before=add(body,section()); a=add(body,section()); b=add(body,section()); after=add(body,section());
one=add(a,p()); two=add(b,p()); three=add(b,p());
fault={kind:"appendChild",nth:1,count:0,phase:"after"}; rollbackFault=true;
need(MergeContainers(a,false)===false && undo==0,"failed rollback still closes undo");
need(messages[messages.length-1]==="fbe.structure.operation.rollback_failed","incomplete rollback is reported honestly");

body=node("DIV","body"); a=add(body,section()); b=add(body,section()); one=add(a,p()); two=add(b,section());
need(MergeContainers(a,false)===false && same(body,[a,b]) && same(a,[one]) && same(b,[two]) && undo==0,"invalid merge with check=false leaves DOM unchanged");
WScript.Echo("structural command behavior passed");
'@
    [IO.File]::WriteAllText($runner, $script, [Text.Encoding]::Unicode)
    & cscript.exe //nologo $runner
    if($LASTEXITCODE -ne 0) { throw "Structural command behavior failed: $LASTEXITCODE" }
} finally {
    Remove-Item -LiteralPath $runner -ErrorAction SilentlyContinue
}
