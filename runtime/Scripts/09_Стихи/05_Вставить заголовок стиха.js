/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Вставить заголовок стиха",
  "version": "1.0",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Вставляет пустой заголовок в стих под курсором или переходит к существующему.",
  "description": "При одиночном курсоре в пределах poem проверяет первый дочерний блок. Если там нет title, создаёт пустой заголовок с абзацем; если заголовок уже есть, переходит к нему. Непустое выделение отклоняет.",
  "scope": "Только контейнер poem, содержащий курсор внутри fbw_body; при существующем заголовке документ не меняется. Непустое выделение не допускается.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Снимите выделение и поставьте курсор внутри нужного стиха.",
    "Запустите скрипт и введите текст в созданный или найденный заголовок."
  ],
  "warnings": [
    "При вызове вне стиха заголовок не создаётся; скрипт может лишь временно вставить и удалить маркер курсора."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "73842adbc393f88dad657e98c81e13ccc4bb4bb54029cac61ef3bc3e0607c636",
    "reviewStatus": "generated"
  },
  "tags": [
    "Стихи"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": ""
  }
}
</FBE-SCRIPT-CARD>*/
function Run() {
  
 function isTitle(gg) {
  if (!gg) return false;
  if (gg.nodeName!="DIV" || !gg.className || gg.className!="title") return false;
  return true;
 }
 
 function InflateIt(elem) {
  if (!elem || elem.nodeType!=1) return;
  if (elem.tagName=="P") {
   window.external.inflateBlock(elem)=true;
   return;
  }
  elem=elem.firstChild;
  while (elem) {
   InflateIt(elem);
   elem=elem.nextSibling;
  }
 }
 
 var versionNum="1.0";
 var range,el,el2,saveNextAfterEl,saveNextAfterEl2,elParent,newTitle,whileFlag;
 var randomNum=Math.floor((Math.random()*9)).toString()+Math.floor((Math.random()*9)).toString()+Math.floor((Math.random()*9)).toString()+Math.floor((Math.random()*9)).toString()+Math.floor((Math.random()*9)).toString()+Math.floor((Math.random()*9)).toString();
 var selectionBeginId="addPoemTitleBeginId_"+randomNum;
 var fbw_body=document.getElementById("fbw_body");
 if (!fbw_body) return;
 if (document.selection.type.toLowerCase()!="none") {
  alert("При запуске скрипта не должно быть выделения. Должен быть просто курсор.");
  return;
 } 
 var tr=document.selection.createRange();
 window.external.BeginUndoUnit(document,"вставку заголовка стиха (v"+versionNum+")"); 
 tr.pasteHTML("<B id="+selectionBeginId+"></B>");
 el=document.getElementById(selectionBeginId);
 var selectionIsNone=tr.compareEndPoints("StartToEnd",tr)==0;
 if (selectionIsNone && fbw_body.contains(el)) {
  while (!(el.nodeName=="BODY" ||
           (el.nodeName=="DIV" &&
            (el.className=="poem")
           )
         )) el=el.parentNode;
  if (el.nodeName!="BODY") {
   elParent=el.parentNode;
   if (el.className=="poem")
    if (!isTitle(el.firstChild)) {
     newTitle=document.createElement("DIV");
     newTitle.className="title";
     var pp=document.createElement("P");
     pp.innerText="";
     window.external.inflateBlock(pp) = true;
     newTitle.appendChild(pp);	  
     newTitle=el.insertAdjacentElement("afterBegin",newTitle);
     if (newTitle) GoTo(newTitle);
     // InflateIt(newTitle);
    } // if (!isTitle(el.firstChild))
    else if (el.firstChild) GoTo(el.firstChild)
     else GoTo(el);
   saveNextAfterEl=el.nextSibling;
  } // if (el.nodeName!="BODY")
  document.getElementById(selectionBeginId).removeNode(true);
 } // if (selectionIsNone && fbw_body.contains(el))
 else {
  alert("При запуске скрипта не должно быть выделения. Должен быть просто курсор.");
  el.removeNode(false);
 }
 window.external.EndUndoUnit(document);
}
