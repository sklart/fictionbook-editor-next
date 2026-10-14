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
  "summary": "Показывает HTML ближайшего DIV у курсора в окне сообщения.",
  "description": "Получает элемент под курсором, поднимается к первому DIV либо элементу с классом section и показывает его outerHTML. Это внутренний HTML визуального редактора, а не исходный XML книги.",
  "scope": "Ближайший контейнер DIV у позиции курсора; весь документ не обходится.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор в нужный абзац или структурный блок и запустите скрипт.",
    "Просмотрите HTML ближайшего контейнера в сообщении; для полного XML книги используйте другие средства."
  ],
  "warnings": [],
  "dependencies": [],
  "dependenciesComplete": true,
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
    "description": "Скрипт только читает HTML и показывает сообщение; изменений книги и операции отмены в нём нет."
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
