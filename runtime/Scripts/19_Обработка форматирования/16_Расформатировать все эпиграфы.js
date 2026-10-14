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
  "summary": "Снимает оболочки всех эпиграфов, сохраняя их содержимое.",
  "description": "Обходит DIV.epigraph внутри корня книги, снимает класс text-author с непосредственно вложенных абзацев подписи и удаляет оболочку каждого эпиграфа, оставляя дочерние узлы. После обработки показывает число эпиграфов и время выполнения.",
  "scope": "Все DIV.epigraph внутри корня fbw_body; класс подписи снимается только у непосредственных P.text-author.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте книгу и запустите скрипт для снятия разметки эпиграфов.",
    "Проверьте абзацы бывших эпиграфов и итоговую статистику в сообщении."
  ],
  "warnings": [
    "Оформление эпиграфов и подписей авторов изменяется; текст остаётся в книге.",
    "Если эпиграфов нет, скрипт всё равно вызывает завершение не начатой операции отмены. При ошибке обхода начатая операция может остаться открытой; корректность отката не проверена."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
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
    "description": "При наличии эпиграфов снятие разметки выполняется в одной операции отмены редактора. Если эпиграфов нет, завершение вызывается без начала; при исключении завершение не гарантировано."
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
