/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Удалить атрибуты id, на которые нет ссылок, и теги style",
  "version": "1.1",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Снимает обычные оболочки SPAN и удаляет ID без найденных локальных ссылок.",
  "description": "Собирает используемые ID по адресам ссылок документа, если адрес содержит main.html# либо начинается с #. Снимает оболочки SPAN, кроме code и image. Затем удаляет не найденные в этом наборе ID абзацев и блоков изображений во всех body, а также вложенных DIV вне body примечаний и комментариев.",
  "scope": "Ссылки ищутся во всём document.links; изменения в fbw_body. ID абзацев и блоков изображений проверяются во всех body, ID вложенных DIV — вне body notes/comments; выделение не используется.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Запустите скрипт, чтобы убрать обычные оболочки SPAN и невостребованные ID.",
    "Проверьте внутренние переходы и оформление после очистки."
  ],
  "warnings": [
    "ID считается используемым только для распознанных форм локальных адресов; другие формы ссылок могут остаться без цели после очистки.",
    "Снимаются все обычные оболочки SPAN, даже если их оформление было нужно."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "05b9a08a1c7de18aff3eb541f4f035aaab0ea4202ff1229d6e4832b22447eb2a",
    "reviewStatus": "generated"
  },
  "tags": [
    "Чистка"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "При первом изменении открывается группа отмены и на штатном пути закрывается после всех проходов. Полнота отката при ошибке не проверена."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Удалить атрибуты id, на которые нет ссылок, и теги style v1.1" 
// Автор Sclex

function Run() {
 var undoUnitName="удаление атрибутов id, на которые нет ссылок, и тегов style";
 var fbwBody=document.getElementById("fbw_body");
 
 //соберем id-ы, на которые есть ссылки
 var usingIds={};
 var key, href, pos;
 for (key in usingIds) delete(usingIds[key]);
 var links = document.links;
 for(var i = 0; i < links.length; i++) {
  href=links[i].href;
  if (href.search("main.html#")>=0 || href.indexOf("#")==0) {
   pos=href.search("#");
   href=href.substr(pos+1);
   usingIds[href.toLowerCase()]=true;
  }
 }

 //удалим теги style
 var ptr=fbwBody.firstChild;
 var replaceCounter3=0;
 var saveNext, firstRemoving=true; 
 while (ptr && ptr!=fbwBody) {
  if (ptr.firstChild)
   saveNext=ptr.firstChild;
  else {
   saveNext=ptr;
   while (saveNext!=fbwBody && saveNext.nextSibling==null) saveNext=saveNext.parentNode;
   if (saveNext!=fbwBody) saveNext=saveNext.nextSibling;
  }
  if (ptr.nodeName.toUpperCase()=="SPAN" && ptr.className!="code" && ptr.className!="image") {
   if (firstRemoving) {
    window.external.BeginUndoUnit(document,undoUnitName);
    firstRemoving=false;
   }
   ptr.removeNode(false);
   replaceCounter3+=1;
  } 
  ptr=saveNext;
 }

 //удалим id абзацных тегов
 var fbwBody=document.getElementById("fbw_body");
 var elems=fbwBody.getElementsByTagName("DIV");
 var elem, elem2, elems2, i, j;
 var replaceCounter1=0;
 for (var i=0; i<elems.length; i++) {
  elem=elems[i];
  if (elem.className && elem.className.toLowerCase()=="body") {
   elems2=elem.getElementsByTagName("P");
   for (var j=0; j<elems2.length; j++) {
    elem2=elems2[j];
    if (elem2.getAttribute("id")!=null && elem2.getAttribute("id")!="" && usingIds[elem2.id.toLowerCase()]!=true) {
     if (firstRemoving) {
      window.external.BeginUndoUnit(document,undoUnitName);
      firstRemoving=false;
     }
     elem2.removeAttribute("id");
     replaceCounter1+=1;
    }
   }
  }
 }

 //удалим id картинок
 var fbwBody=document.getElementById("fbw_body");
 var elems=fbwBody.getElementsByTagName("DIV");
 var elem, elem2, elems2, i, j;
 var replaceCounter4=0;
 for (var i=0; i<elems.length; i++) {
  elem=elems[i];
  if (elem.className && elem.className.toLowerCase()=="body") {
   elems2=elem.getElementsByTagName("DIV");
   for (var j=0; j<elems2.length; j++) {
    elem2=elems2[j];
    if (elem2.className && elem2.className=="image" && elem2.getAttribute("id")!=null && elem2.getAttribute("id")!="" && usingIds[elem2.id.toLowerCase()]!=true) {
     if (firstRemoving) {
      window.external.BeginUndoUnit(document,undoUnitName);
      firstRemoving=false;
     }
     elem2.removeAttribute("id");
     replaceCounter4+=1;
    }
   }
  }
 }
 
 //удалим id блочных элементов
 var fbwBody=document.getElementById("fbw_body");
 var elems=fbwBody.getElementsByTagName("DIV");
 var elem, elem2, elems2, i, j;
 var replaceCounter2=0;
 for (var i=0; i<elems.length; i++) {
  elem=elems[i];
  if (elem.className && elem.className.toLowerCase()=="body" && elem.getAttribute("fbname")!="notes" && elem.getAttribute("fbname")!="comments") {
   elems2=elem.getElementsByTagName("DIV");
   for (var j=0; j<elems2.length; j++) {
    elem2=elems2[j];
    if (elem2.className && elem2.getAttribute("id")!=null && elem2.getAttribute("id")!="" && usingIds[elem2.id.toLowerCase()]!=true) {
     if (firstRemoving) {
      window.external.BeginUndoUnit(document,undoUnitName);
      firstRemoving=false;
     }
     elem2.removeAttribute("id");
     replaceCounter2+=1;
    }
   }
  }
 } 

 if (!firstRemoving) window.external.EndUndoUnit(document);
 MsgBox("Всего удалено тегов style: "+replaceCounter3+"\n"+
                  "Всего удалено атрибутов id абзацных тегов: "+replaceCounter1+"\n"+
                  "Всего удалено атрибутов id блочных элементов: "+replaceCounter2+"\n"+
                  "Всего удалено атрибутов id картинок: "+replaceCounter4);
}
