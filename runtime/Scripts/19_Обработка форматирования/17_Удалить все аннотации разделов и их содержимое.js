/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Удалить все аннотации разделов и их содержимое",
  "version": "1.0",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Удаляет вложенные в разделы аннотации вместе с их содержимым.",
  "description": "Обходит DIV.annotation внутри корня книги и удаляет контейнер вместе с дочерними узлами, если родитель аннотации не является самим корнем fbw_body. Аннотации, непосредственно вложенные в корень, пропускает.",
  "scope": "Все DIV.annotation внутри fbw_body, кроме его непосредственных дочерних элементов.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте книгу и запустите скрипт для удаления аннотаций разделов с текстом.",
    "Проверьте число удалённых аннотаций в строке состояния и связанные ссылки."
  ],
  "warnings": [
    "Удаляются аннотации вместе с текстом; ссылки на удалённые элементы могут перестать работать. Сохраните копию книги перед запуском.",
    "При ошибке удаления операция отмены может остаться открытой; полнота отката не проверена."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "83206da1589c0fea088395f0ed46709c5b2b023ee479ec6b230f95941eac47cf",
    "reviewStatus": "generated"
  },
  "tags": [
    "Обработка форматирования"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Если найдены подходящие элементы, изменения группируются в одну операцию отмены редактора. При отсутствии элементов операция не открывается; при исключении её завершение не гарантировано."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Удалить все аннотации разделов и их содержимое" v1.0
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
      window.external.BeginUndoUnit(document,"удаление всех аннотаций разделов и их содержимого"); 
      itsFirstRemoving=false;
    }
   allDivs[i].removeNode(true);
   removedAnnotationsCnt++;
  }
 }
 if (removedAnnotationsCnt==0) {
  MsgBox("Аннотаций разделов в документе не нашлось.");
  return;
 }
 try { window.external.SetStatusBarText("Удалено аннотаций разделов с их содержимым: "+removedAnnotationsCnt+"."); }
 catch(e) {}
 window.external.EndUndoUnit(document);
}
