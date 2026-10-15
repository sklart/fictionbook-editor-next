/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Удалить все цитаты и их содержимое",
  "version": "1.0",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Удаляет из книги все блоки цитат вместе с их содержимым.",
  "description": "Обходит все DIV внутри корня книги и удаляет каждый DIV.cite вместе с дочерними узлами. Результат выводит в строку состояния; если цитат нет, показывает сообщение.",
  "scope": "Все DIV.cite внутри корня fbw_body, включая вложенные блоки.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте книгу и запустите скрипт для удаления всех цитат с текстом.",
    "Проверьте число удалённых цитат в строке состояния и связанные ссылки."
  ],
  "warnings": [
    "Удаляются и контейнеры цитат, и весь их текст; ссылки на удалённые элементы могут перестать работать. Сохраните копию книги перед запуском."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "6ab2f4e9965b79efca6b1d980d815442c6811d4ef83dd89d2600eea4fbece9c6",
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
// Скрипт "Удалить все цитаты и их содержимое" v1.0
// Автор Sclex

function Run() {
 var fbwBody, allDivs, i, removedCitesCnt, itsFirstRemoving;
 fbwBody=document.getElementById("fbw_body");
 if (!fbwBody) return;
 removedCitesCnt=0;
 allDivs=fbwBody.getElementsByTagName("DIV");
 itsFirstRemoving=true;
 for (i=allDivs.length-1; i>=0; i--) {
  if (allDivs[i].className=="cite") {
   if (itsFirstRemoving) {
    window.external.BeginUndoUnit(document,"удаление всех цитат и их содержимого");
    itsFirstRemoving=false;
   }
   allDivs[i].removeNode(true);
   removedCitesCnt++;
  }
 }
 if (removedCitesCnt==0) {
  MsgBox("Цитат в документе не нашлось.");
  return;
 }
 try { window.external.SetStatusBarText("Удалено цитат с их содержимым: "+removedCitesCnt+"."); }
 catch(e) {} 
 window.external.EndUndoUnit(document);
}
