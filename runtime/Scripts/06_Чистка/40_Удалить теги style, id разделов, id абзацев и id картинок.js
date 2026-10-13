/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Удалить теги style, id разделов, id абзацев и id картинок",
  "version": "1.2",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Удаляет теги style, id разделов, id абзацев и id картинок.",
  "description": "Обходит указанные типы элементов и удаляет их атрибуты id; комбинированный вариант также снимает style-оболочки. Сопоставление всех входящих ссылок перед удалением не следует считать гарантированным: проверьте структуру и ссылки после операции.",
  "scope": "Область определяется условиями и обходом DOM в данном скрипте.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже.",
    "Просмотрите результат до сохранения. Для структурной правки проверьте книгу и связанные ссылки."
  ],
  "warnings": [
    "Скрипт содержит операции изменения документа. Перед массовой обработкой сохраните отдельную копию книги.",
    "Удаление ID может разорвать ссылки на эти элементы. ID элемента image и ID binary — разные части структуры; не смешивайте их."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "dcfd8bd36e3f0f925361e16d6c103f29f98bfa475aa216c8888d54bd9e11ef3a",
    "reviewStatus": "generated"
  },
  "tags": [
    "Чистка",
    "изображения"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Удалить теги style, id разделов, id абзацев и id картинок v1.2" 
// Автор Sclex

function Run() {
 var fbwBody=document.getElementById("fbw_body");
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
    window.external.BeginUndoUnit(document,"удаление тегов style, id разделов, id абзацев и id картинок");
    firstRemoving=false;
   }
   ptr.removeNode(false);
   replaceCounter3+=1;
  } 
  ptr=saveNext;
 }

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
    if (elem2.getAttribute("id")!=null && elem2.getAttribute("id")!="") {
     if (firstRemoving) {
      window.external.BeginUndoUnit(document,"удаление тегов style, id разделов, id абзацев и id картинок");
      firstRemoving=false;
     }
     elem2.removeAttribute("id");
     replaceCounter1+=1;
    }
   }
  }
 }

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
    if (elem2.className && elem2.className=="section" && elem2.getAttribute("id")!=null && elem2.getAttribute("id")!="") {
     if (firstRemoving) {
      window.external.BeginUndoUnit(document,"удаление тегов style, id разделов, id абзацев и id картинок");
      firstRemoving=false;
     }
     elem2.removeAttribute("id");
     replaceCounter2+=1;
    }
   }
  }
 }

 var fbwBody=document.getElementById("fbw_body");
 var elems=fbwBody.getElementsByTagName("DIV");
 var firstRemoving=true;
 var elem, elem2, elems2, i, j;
 var replaceCounter4=0;
 for (var i=0; i<elems.length; i++) {
  elem=elems[i];
  if (elem.className && elem.className.toLowerCase()=="body") {
   elems2=elem.getElementsByTagName("DIV");
   for (var j=0; j<elems2.length; j++) {
    elem2=elems2[j];
    if (elem2.className && elem2.className=="image" && elem2.getAttribute("id")!=null && elem2.getAttribute("id")!="") {
     if (firstRemoving) {
      window.external.BeginUndoUnit(document,"удаление тегов style, id разделов, id абзацев и id картинок");
      firstRemoving=false;
     }
     elem2.removeAttribute("id");
     replaceCounter4+=1;
    }
   }
  }
 }
 if (!firstRemoving) window.external.EndUndoUnit(document);
 MsgBox("Всего удалено тегов style: "+replaceCounter3+"\n"+
                  "Всего удалено id абзацев: "+replaceCounter1+"\n"+
                  "Всего удалено id разделов: "+replaceCounter2+"\n"+
                  "Всего удалено id картинок: "+replaceCounter4);
}
