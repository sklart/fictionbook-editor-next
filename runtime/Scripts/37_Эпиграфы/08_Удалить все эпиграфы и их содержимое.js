/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Удалить все эпиграфы и их содержимое",
  "version": "1.0",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Удаляет из книги все эпиграфы вместе с их содержимым.",
  "description": "Обходит все DIV внутри корня книги и удаляет каждый DIV.epigraph вместе с дочерними узлами. Результат выводит в строку состояния; если эпиграфов нет, показывает сообщение.",
  "scope": "Все DIV.epigraph внутри корня fbw_body, включая вложенные блоки.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте книгу и запустите скрипт для удаления всех эпиграфов с текстом.",
    "Проверьте число удалённых эпиграфов в строке состояния и связанные ссылки."
  ],
  "warnings": [
    "Удаляются и контейнеры эпиграфов, и весь их текст; ссылки на удалённые элементы могут перестать работать. Сохраните копию книги перед запуском."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "462028cca300eb4cbd952e43d423986108124b878d8fd79ae21f85be18e609db",
    "reviewStatus": "generated"
  },
  "tags": [
    "Эпиграфы"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": ""
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Удалить все эпиграфы и их содержимое" v1.0
// Автор Sclex

function Run() {
 var fbwBody, allDivs, i, removedEpigraphsCnt, itsFirstRemoving;
 fbwBody=document.getElementById("fbw_body");
 if (!fbwBody) return;
 removedEpigraphsCnt=0;
 allDivs=fbwBody.getElementsByTagName("DIV");
 itsFirstRemoving=true;
 for (i=allDivs.length-1; i>=0; i--) {
  if (allDivs[i].className=="epigraph") {
   if (itsFirstRemoving) {
    window.external.BeginUndoUnit(document,"удаление всех эпиграфов и их содержимого");
    itsFirstRemoving=false;
   }
   allDivs[i].removeNode(true);
   removedEpigraphsCnt++;
  }
 }
 if (removedEpigraphsCnt==0) {
  MsgBox("Эпиграфов в документе не нашлось.");
  return;
 }
 try { window.external.SetStatusBarText("Удалено эпиграфов с их содержимым: "+removedEpigraphsCnt+"."); }
 catch(e) {} 
 window.external.EndUndoUnit(document);
}
