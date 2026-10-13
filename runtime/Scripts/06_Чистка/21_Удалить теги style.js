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
  "summary": "Удаляет теги style.",
  "description": "Обходит DOM и снимает соответствующие style-оболочки через removeNode(false). Содержимое узла оставляет на месте; оформление может измениться.",
  "scope": "Область определяется условиями и обходом DOM в данном скрипте.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Запустите команду из меню или дерева скриптов.",
    "Просмотрите результат до сохранения. Для структурной правки проверьте книгу и связанные ссылки."
  ],
  "warnings": [
    "Скрипт содержит операции изменения документа. Перед массовой обработкой сохраните отдельную копию книги."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
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
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
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
