/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Расформатировать все эпиграфы",
  "version": "1.3",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    },
    {
      "name": "TaKir",
      "role": "modifier"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Снимает разметку: все эпиграфы.",
  "description": "Снимает соответствующие блочные контейнеры через removeNode(false), оставляя дочерние узлы на месте. Дополнительно может снимать атрибуты абзацев; смысловое деление и оформление изменяются.",
  "scope": "Текущая книга или определённые кодом её разделы.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже.",
    "Просмотрите результат до сохранения. Для структурной правки проверьте книгу и связанные ссылки."
  ],
  "warnings": [
    "Скрипт содержит операции изменения документа. Перед массовой обработкой сохраните отдельную копию книги."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "36ba4736f8570422103ef126f90120a20cf02ee79c2bae46d2bad678c54dacd3",
    "reviewStatus": "generated"
  },
  "tags": [
    "Обработка форматирования"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Расформатировать все эпиграфы" v1.3
// Автор Sclex
// Добавлена статистика выполнения (TaKir)

// version 1.3, 11.02.2026
//======================================

function Run() {
 var fbwBody, allDivs, i, j, removedEpigraphsCnt, itsFirstRemoving, allPs;
 
 // Добавляем таймер
 var startTime = new Date().getTime();
 
 fbwBody=document.getElementById("fbw_body");
 if (!fbwBody) return;
 
 removedEpigraphsCnt=0;
 allDivs=fbwBody.getElementsByTagName("DIV");
 itsFirstRemoving=true;
 
 for (i=allDivs.length-1; i>=0; i--) {
  if (allDivs[i].className=="epigraph") {
   if (itsFirstRemoving) {
    window.external.BeginUndoUnit(document,"расформатирование всех эпиграфов");
    itsFirstRemoving=false;
   }
   allPs=allDivs[i].getElementsByTagName("P");
   for (j=0; j<allPs.length; j++)
     if (allPs[j].className=="text-author" && allPs[j].parentNode.className=="epigraph") {
      allPs[j].removeAttribute("class");
      allPs[j].removeAttribute("className");
     }
   allDivs[i].removeNode(false);
   removedEpigraphsCnt++;
  }
 }
 
 // Завершаем undo unit
 window.external.EndUndoUnit(document);
 
 // Рассчитываем время выполнения
 var endTime = new Date().getTime();
 var executionTime = (endTime - startTime) / 1000;
 
 // Выводим статистику
 if (removedEpigraphsCnt==0) {
  MsgBox("Расформатировать все эпиграфы\nver. 1.3\n\n" +
         "Эпиграфов в документе не нашлось.\n\n" +
         "Время выполнения: " + executionTime.toFixed(3) + " сек.");
  return;
 }
 
 try { 
  window.external.SetStatusBarText("Расформатировано эпиграфов: "+removedEpigraphsCnt+"."); 
 } catch(e) {}
 
 // Главное окно статистики
 MsgBox("Расформатировать все эпиграфы\nver. 1.3\n" +
        "---------------------------------------\n" +
        "✓ Обработано эпиграфов: " + removedEpigraphsCnt + "\n" +
        "---------------------------------------\n" +
        "Время выполнения: " + executionTime.toFixed(3) + " сек.");
}
