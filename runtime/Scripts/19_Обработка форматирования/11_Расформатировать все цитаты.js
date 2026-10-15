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
  "summary": "Снимает оболочки всех цитат, сохраняя их содержимое.",
  "description": "Обходит DIV.cite внутри корня книги, снимает класс text-author с непосредственных абзацев авторской подписи и удаляет оболочку каждой цитаты, оставляя дочерние узлы.",
  "scope": "Все DIV.cite внутри корня fbw_body; класс подписи снимается только у P.text-author, непосредственно вложенных в цитату.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте книгу и запустите скрипт для снятия разметки цитат.",
    "Проверьте абзацы бывших цитат и число обработанных блоков в строке состояния."
  ],
  "warnings": [
    "Оформление цитат и подписей авторов изменяется; текст остаётся в книге."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
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
    "description": ""
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
