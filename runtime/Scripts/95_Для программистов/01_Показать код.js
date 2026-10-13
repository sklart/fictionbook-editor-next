/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Показать код",
  "version": "Не указана",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Показывает HTML ближайшего найденного контейнера около курсора.",
  "description": "Поднимается от parentElement текущего диапазона по DOM и выводит outerHTML. Это HTML визуального редактора, не полный исходный XML FB2.",
  "scope": "Область определяется условиями и обходом DOM в данном скрипте.",
  "modifiesDocument": false,
  "requiresSelection": null,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже."
  ],
  "warnings": [],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "b473e8690582632f654ca9d052129ecb645b7c2cb86ad3a8b47c5b0446fb36a3",
    "reviewStatus": "generated"
  },
  "tags": [
    "Для программистов"
  ],
  "undo": {
    "status": "no_explicit_grouping",
    "description": "Явная группировка Undo в этом файле не найдена. Это не означает ни гарантированное отсутствие Undo, ни безопасное выполнение."
  }
}
</FBE-SCRIPT-CARD>*/
//======================================
// Displays HTML code for the current
// section (only for script debugging)
//======================================

function Run()
{
 var rng=document.selection.createRange(); if(!rng) return false;

 var pe=rng.parentElement();

 while(pe && pe.tagName!="DIV" && pe.className!="section") pe=pe.parentElement;

 if(pe) MsgBox(pe.outerHTML);
}
