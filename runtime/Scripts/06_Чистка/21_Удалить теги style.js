/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Удалить теги style",
  "version": "1.0",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Удаляет большинство оболочек SPAN в визуальном тексте книги, сохраняя их содержимое.",
  "description": "Обходит fbw_body и снимает все SPAN, кроме элементов с классом точно code или image. Дочернее содержимое сохраняется, но оформление и семантика удалённых оболочек могут измениться. Теги STYLE скрипт не ищет.",
  "scope": "Все вложенные элементы SPAN внутри визуальной области fbw_body; выделение не учитывается.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте книгу и запустите скрипт для удаления оболочек SPAN во всём визуальном тексте.",
    "Проверьте оформление и встроенные элементы после обработки: сохраняются только SPAN с классами code и image."
  ],
  "warnings": [
    "Скрипт удаляет не только стилевые SPAN, но и другие SPAN, кроме классов code и image; вложенный текст сохраняется, оформление и поведение могут измениться."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "73248ff1ba0981aaaa25f188de57bf0eaf8a747a8f724a61f9f1ddad6fbd89e0",
    "reviewStatus": "generated"
  },
  "tags": [
    "Чистка"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": ""
  }
}
</FBE-SCRIPT-CARD>*/
//Скрипт «Удаление тегов style» v1.0
//Автор Sclex

function Run() {
 var fbwBody=document.getElementById("fbw_body");
 var ptr=fbwBody.firstChild;
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
    window.external.BeginUndoUnit(document,"удаление тегов style");
    firstRemoving=false;
   }
   ptr.removeNode(false);
  } 
  ptr=saveNext;
 }
 if (!firstRemoving) window.external.EndUndoUnit(document);
}
