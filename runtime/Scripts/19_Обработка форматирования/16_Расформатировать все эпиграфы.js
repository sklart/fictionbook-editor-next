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
