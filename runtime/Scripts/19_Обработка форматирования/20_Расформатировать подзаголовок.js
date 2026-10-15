/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Расформатировать подзаголовок",
  "version": "1.2",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Снимает оформление подзаголовка в начале текущего выделения или около курсора.",
  "description": "Вставляет временный маркер в начало текущего текстового диапазона, находит содержащий его абзац в книге и меняет стиль, только если это P.subtitle. Затем удаляет маркер. Выделенный текст не обрабатывается целиком.",
  "scope": "Один абзац P.subtitle, содержащий начало текущего диапазона внутри fbw_body.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор в подзаголовок или выделите текст, начинающийся в нём, и запустите скрипт.",
    "Проверьте оформление этого абзаца; при другом положении курсора подзаголовки не изменятся."
  ],
  "warnings": [
    "Скрипт временно вставляет маркер в документ."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "aadf7fb5b7e86f678c36059c07c961e89c364b23c37f45ee42fd830fd0a0ea08",
    "reviewStatus": "generated"
  },
  "tags": [
    "Обработка форматирования"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": ""
  }
}
</FBE-SCRIPT-CARD>*/
function Run() {
 var versionNum="1.2";

 var range,el,el2,el3,saveNextAfterEl,saveNextAfterEl2;
 var elParent,i,j,divs,ps,saveClassName;
 var randomNum=Math.floor((Math.random()*9)).toString()+Math.floor((Math.random()*9)).toString()+
         Math.floor((Math.random()*9)).toString()+Math.floor((Math.random()*9)).toString()+
      Math.floor((Math.random()*9)).toString()+Math.floor((Math.random()*9)).toString();
 var selectionBeginId="removePoemOrCiteBeginId_"+randomNum;
 var selectionEndId="removePoemOrCiteEndId"+randomNum;
 var fbw_body=document.getElementById("fbw_body");
 if (!fbw_body) return;
 if (document.selection.type.toLowerCase()!="none" && document.selection.type.toLowerCase()!="text") return;
 var tr=document.selection.createRange();
 tr.collapse(true);
 window.external.BeginUndoUnit(document,"расформатирование подзаголовка (v"+versionNum+")");
 tr.pasteHTML("<B id="+selectionBeginId+"></B>");
 el=document.getElementById(selectionBeginId);
 if (fbw_body.contains(el)) {
  while (el.nodeName!="BODY" && el.nodeName!="P") el=el.parentNode;
  if (el.nodeName=="P" && el.className=="subtitle") {
   window.external.SetStyleEx(document, el, "normal");
   el.removeAttribute("className");
  }
 }
 if (document.getElementById(selectionBeginId))
  document.getElementById(selectionBeginId).removeNode(true);
 window.external.EndUndoUnit(document);
}
