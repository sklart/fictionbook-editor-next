/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Установить курсор в начало документа",
  "version": "1.0",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Переносит курсор в начало визуального документа.",
  "description": "Создаёт текстовый диапазон для fbw_body, сворачивает его к началу и выделяет. Постоянных изменений текста не выполняет.",
  "scope": "Начало визуального текста fbw_body и прокрутка текущего окна.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Откройте книгу и запустите скрипт: курсор перейдёт к началу визуального текста, окно прокрутится вверх.",
    "Если область книги fbw_body не найдена, скрипт завершится без сообщения."
  ],
  "warnings": [],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "605cb9150cdbbaebabfd6974d423c69d839c7e235ec9427b62953f62ee26bb85",
    "reviewStatus": "generated"
  },
  "tags": [
    "Управление курсором либо выделением"
  ],
  "undo": {
    "status": "no_explicit_grouping",
    "description": ""
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Установить курсор в начало документа"
// Автор Sclex
// Версия 1.0

function Run() {
 var fbwBody=document.getElementById("fbw_body");
 if (!fbwBody) return;
 var tr=document.body.createTextRange();
 tr.moveToElementText(fbwBody);
 tr.collapse(true);
 if (tr.move("character",1)==1)
  tr.move("character",-1);
 tr.select();
 window.scroll(0, 0);
}
