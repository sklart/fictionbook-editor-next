/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Расформатировать все цитаты",
  "version": "1.2",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Снимает разметку: все цитаты.",
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
    "sha256": "15ab949417ca8bc766a786d5a3c9de7ef1c26d8614bb15953b4f306f1268e9f5",
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
// Скрипт "Расформатировать все цитаты" v1.2
// Автор Sclex

function Run() {
 var fbwBody, allDivs, i, j, removedCitesCnt, itsFirstRemoving, allPs;
 fbwBody=document.getElementById("fbw_body");
 if (!fbwBody) return;
 removedCitesCnt=0;
 allDivs=fbwBody.getElementsByTagName("DIV");
 itsFirstRemoving=true;
 for (i=allDivs.length-1; i>=0; i--) {
  if (allDivs[i].className=="cite") {
   if (itsFirstRemoving) {
    window.external.BeginUndoUnit(document,"расформатирование всех цитат");
    itsFirstRemoving=false;
   }
   allPs=allDivs[i].getElementsByTagName("P");
   for (j=0; j<allPs.length; j++)
     if (allPs[j].className=="text-author" && allPs[j].parentNode.className=="cite") {
      allPs[j].removeAttribute("class");
      allPs[j].removeAttribute("className");
     }
   allDivs[i].removeNode(false);
   removedCitesCnt++;
  }
 }
 if (removedCitesCnt==0) {
  MsgBox("Цитат в документе не нашлось.");
  return;
 }
 try { window.external.SetStatusBarText("Расформатировано цитат: "+removedCitesCnt+"."); }
 catch(e) {} 
 window.external.EndUndoUnit(document);
}
