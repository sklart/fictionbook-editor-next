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
  "summary": "Удаляет все эпиграфы и их содержимое.",
  "description": "Обходит соответствующие DIV-контейнеры и удаляет их через removeNode(true), то есть вместе с дочерним текстом. Это уничтожение содержимого выбранного типа, а не снятие оболочки при сохранении текста.",
  "scope": "Текущая книга или определённые кодом её разделы.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже.",
    "Просмотрите результат до сохранения. Для структурной правки проверьте книгу и связанные ссылки."
  ],
  "warnings": [
    "Скрипт содержит операции изменения документа. Перед массовой обработкой сохраните отдельную копию книги.",
    "Удаляет текст вместе с контейнерами. Перед запуском обязательно сохраните отдельную копию книги."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
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
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
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
