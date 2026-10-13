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
  "summary": "Снимает разметку: все аннотации разделов.",
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
    "sha256": "49abebd178d118a1494ef0fc0c9d50bb50e9d5ccc49e12753e3dce55aa32d5d8",
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
