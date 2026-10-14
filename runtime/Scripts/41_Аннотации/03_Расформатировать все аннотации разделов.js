/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Расформатировать все аннотации разделов",
  "version": "1.0",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Снимает оболочки аннотаций, вложенных в разделы книги, сохраняя их текст.",
  "description": "Обходит DIV.annotation внутри корня книги и удаляет оболочку с сохранением дочерних узлов, если родитель аннотации не является самим корнем fbw_body. Аннотации, непосредственно вложенные в корень, пропускает.",
  "scope": "Все DIV.annotation внутри fbw_body, кроме его непосредственных дочерних элементов.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте книгу и запустите скрипт для снятия разметки аннотаций разделов.",
    "Проверьте оставшийся текст и число обработанных аннотаций в строке состояния."
  ],
  "warnings": [
    "Текст остаётся, но разметка аннотаций исчезает. При ошибке обхода операция отмены может остаться открытой."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "49abebd178d118a1494ef0fc0c9d50bb50e9d5ccc49e12753e3dce55aa32d5d8",
    "reviewStatus": "generated"
  },
  "tags": [
    "Аннотации"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Если найдены подходящие элементы, изменения группируются в одну операцию отмены редактора. При отсутствии элементов операция не открывается; при исключении её завершение не гарантировано."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Расформатировать аннотации разделов" v1.0
// Автор Sclex

function Run() {
 var fbwBody, allDivs, i, removedAnnotationsCnt, itsFirstRemoving;
 fbwBody=document.getElementById("fbw_body");
 if (!fbwBody) return;
 removedAnnotationsCnt=0;
 allDivs=fbwBody.getElementsByTagName("DIV");
 itsFirstRemoving=true;
 for (i=allDivs.length-1; i>=0; i--) {
  if (allDivs[i].className=="annotation" && allDivs[i].parentNode!=fbwBody) {
    if (itsFirstRemoving) {
      window.external.BeginUndoUnit(document,"расформатирование всех аннотаций разделов"); 
      itsFirstRemoving=false;
    }
   allDivs[i].removeNode(false);
   removedAnnotationsCnt++;
  }
 }
 if (removedAnnotationsCnt==0) {
  MsgBox("Аннотаций разделов в документе не нашлось.");
  return;
 }
 try { window.external.SetStatusBarText("Расформатировано аннотаций разделов: "+removedAnnotationsCnt+"."); }
 catch(e) {}
 window.external.EndUndoUnit(document);
}
