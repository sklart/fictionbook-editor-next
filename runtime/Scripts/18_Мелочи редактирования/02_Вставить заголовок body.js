/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Вставить заголовок body",
  "version": "1.0",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Создаёт пустой заголовок внутри текущего body, если его ещё нет.",
  "description": "При курсоре внутри контейнера body проверяет его первый дочерний элемент. Если там нет блока title, вставляет в начало пустой заголовок с абзацем и пытается перевести курсор к нему. Если title уже первый, переходит к существующему заголовку; вне body новый блок не создаёт.",
  "scope": "Текущий контейнер body, найденный от позиции курсора вверх по DOM. Непустое или графическое выделение запрещено.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор в текст нужного body; предварительно снимите выделение.",
    "Запустите команду и проверьте заголовок в начале контейнера.",
    "Если заголовок уже был первым элементом, команда только переводит к нему курсор."
  ],
  "warnings": [
    "Проверяется только первый дочерний элемент контейнера. Заголовок в другой позиции может не распознаться, и тогда появится ещё один пустой заголовок.",
    "Позиционирование курсора зависит от общей функции редактора; при её ошибке группа отмены может остаться незакрытой."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "43d3a593aea451f9f147be7f73d8089903386ed9cfddc0748952bfd8bf79e53f",
    "reviewStatus": "generated"
  },
  "tags": [
    "Мелочи редактирования"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "На обычном пути создание заголовка охвачено группой отмены. Ошибки при вставке или переходе к заголовку могут прервать закрытие группы; полный откат не проверен."
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
 var selectionBeginId="addBodyTitleBeginId_"+randomNum;
 var fbw_body=document.getElementById("fbw_body");
 if (!fbw_body) return;
 if (document.selection.type.toLowerCase()!="none") {
  alert("При запуске скрипта не должно быть выделения. Должен быть просто курсор.");
  return;
 } 
 var tr=document.selection.createRange();
 window.external.BeginUndoUnit(document,"вставку заголовка body (v"+versionNum+")"); 
 tr.pasteHTML("<B id="+selectionBeginId+"></B>");
 el=document.getElementById(selectionBeginId);
 var selectionIsNone=tr.compareEndPoints("StartToEnd",tr)==0;
 if (selectionIsNone && fbw_body.contains(el)) {
  while (!(el.nodeName=="BODY" ||
           (el.nodeName=="DIV" &&
            (el.className=="body")
           )
         )) el=el.parentNode;
  if (el.nodeName!="BODY") {
   elParent=el.parentNode;
   if (el.className=="body")
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
