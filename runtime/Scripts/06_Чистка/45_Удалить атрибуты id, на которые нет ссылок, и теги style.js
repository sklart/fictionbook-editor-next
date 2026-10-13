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
  "summary": "Удаляет атрибуты id, на которые нет ссылок, и теги style.",
  "description": "Сначала собирает цели href, затем удаляет неиспользуемые ID и style-оболочки. В отличие от безусловной очистки ID, учитывает найденные ссылки; это всё же структурная правка, результат которой нужно проверить.",
  "scope": "Область определяется условиями и обходом DOM в данном скрипте.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже.",
    "Просмотрите результат до сохранения. Для структурной правки проверьте книгу и связанные ссылки."
  ],
  "warnings": [
    "Скрипт содержит операции изменения документа. Перед массовой обработкой сохраните отдельную копию книги."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
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
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
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
