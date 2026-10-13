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
  "scope": "Сеанс и интерфейс редактора.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Запустите скрипт в FBE Next и следуйте сообщениям; требования к документу указаны в preconditions.",
    "Оставьте курсор в обрабатываемом элементе или точке начала поиска; непустое выделение не требуется.",
    "Запустите команду из меню или дерева скриптов."
  ],
  "warnings": [],
  "dependencies": [],
  "dependenciesComplete": false,
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
    "description": "Явная группировка Undo в этом файле не найдена. Это не означает ни гарантированное отсутствие Undo, ни безопасное выполнение."
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
