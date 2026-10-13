/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Обрамить верхние индексы скобками []",
  "version": "1.0",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Обрамить верхние индексы скобками [].",
  "description": "Обходит SUP в книге и обрамляет его содержимое указанными квадратными/фигурными скобками, перестраивая соответствующие узлы. Работает с верхними индексами вообще, а не только с подтверждёнными сносками.",
  "scope": "Область определяется условиями и обходом DOM в данном скрипте.",
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
    "sha256": "daac1acf9d52a02a9f7c100134f30957ffaeededf5da7340ec4ff944a34c1b2e",
    "reviewStatus": "generated"
  },
  "tags": [
    "Мелочи редактирования"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт «Обрамить верхние индексы скобками []» v1.0

function Run() {
 var fbw_body=document.getElementById("fbw_body");
 var el=fbw_body.firstChild;
 var el2;
 var foundSup=false;
 var cnt=0;
 while (el && el!=fbw_body) {
  if (el.nodeName=="SUP") {
   el2=el.firstChild;
   while (el2 && el2.nodeType==1 && el2.nodeName!="IMG") el2=el2.firstChild;
   if (el2) {
    if (!foundSup) {
     window.external.BeginUndoUnit(document,"обрамление верхних индексов скобками []");
     foundSup=true;
    }
    cnt++;
    if (el2.nodeType==3) el2.nodeValue="["+el2.nodeValue;
    else el2.parentNode.insertBefore(document.createTextNode("["),el2);
   }
   el2=el.lastChild;
   while (el2 && el2.nodeType==1 && el2.nodeName!="IMG") el2=el2.lastChild;
   if (el2) {
    if (!foundSup) {
     window.external.BeginUndoUnit(document,"обрамление верхних индексов скобками []");
     foundSup=true;
    }   
    if (el2.nodeType==3) el2.nodeValue=el2.nodeValue+"]";
    else el2.parentNode.appendChild(document.createTextNode("]"));
   }
  }
  if (el.firstChild && el.nodeName!="SUP")
   el=el.firstChild;
  else {
   while (el && el!=fbw_body && !el.nextSibling) el=el.parentNode;
   if (el!=fbw_body) el=el.nextSibling;
  }
 }
 if (foundSup) {
  window.external.EndUndoUnit(document);
  alert("Обрамлено скобками [] содержимое\nследующего количества тегов SUP:\n   "+cnt);
 } 
 else alert("Верхних индексов в документе не обнаружено.");
}
