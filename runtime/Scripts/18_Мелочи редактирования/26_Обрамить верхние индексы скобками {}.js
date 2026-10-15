/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Обрамить верхние индексы скобками {}",
  "version": "1.0",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Обрамляет содержимое верхних индексов скобками {} по всей книге.",
  "description": "Обходит элементы SUP во всей книге и добавляет { перед первым содержимым и } после последнего. Обрабатывает любые верхние индексы, включая изображения; пустые SUP пропускает. Показывает число обработанных элементов.",
  "scope": "Все элементы SUP внутри fbw_body, без учёта выделения и положения курсора.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте книгу и проверьте, какие верхние индексы должны получить скобки.",
    "Запустите команду один раз и проверьте показанное число обработанных элементов.",
    "Просмотрите верхние индексы с изображениями и сложной вложенной разметкой."
  ],
  "warnings": [
    "Повторный запуск добавляет ещё одну пару скобок; скрипт не проверяет, были ли они уже поставлены.",
    "Скрипт обрабатывает любой SUP, включая не связанные со сносками и содержащие изображение."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "933ce211a8849c5da0469f3627042476b7a75adb185d9c36b22fc7877c46b811",
    "reviewStatus": "generated"
  },
  "tags": [
    "Мелочи редактирования"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": ""
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт «Обрамить верхние индексы скобками {}» v1.0

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
     window.external.BeginUndoUnit(document,"обрамление верхних индексов скобками {}");
     foundSup=true;
    }
    cnt++;
    if (el2.nodeType==3) el2.nodeValue="{"+el2.nodeValue;
    else el2.parentNode.insertBefore(document.createTextNode("{"),el2);
   }
   el2=el.lastChild;
   while (el2 && el2.nodeType==1 && el2.nodeName!="IMG") el2=el2.lastChild;
   if (el2) {
    if (!foundSup) {
     window.external.BeginUndoUnit(document,"обрамление верхних индексов скобками {}");
     foundSup=true;
    }   
    if (el2.nodeType==3) el2.nodeValue=el2.nodeValue+"}";
    else el2.parentNode.appendChild(document.createTextNode("}"));
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
  alert("Обрамлено скобками {} содержимое\nследующего количества тегов SUP:\n   "+cnt);
 } 
 else alert("Верхних индексов в документе не обнаружено.");
}
