/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Перейти на следующие елочки",
  "version": "2.9",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Находит следующие елочки и переносит туда позицию редактирования.",
  "description": "Выполняет направленный поиск указанного вида кавычек, устанавливает диапазон и прокручивает найденное место. В коде используются временные маркеры/диапазоны; перестановка или замена кавычек не является назначением команды.",
  "scope": "От текущей позиции; направление поиска определяется конкретным скриптом.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Оставьте курсор в обрабатываемом элементе или точке начала поиска; непустое выделение не требуется.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже."
  ],
  "warnings": [
    "Навигационный скрипт временно вмешивается в DOM/выделение. Отсутствие постоянных изменений — ожидаемое назначение, не результат runtime-проверки.",
    "Записи в DOM могут относиться к временным маркерам, интерфейсу или отчёту; отсутствие постоянных правок книги — результат статического разбора, не runtime-гарантия."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "8f9708033d82a72086adacfb0a266d0f8d697e7ed439c2401b9da3f3adfffd01",
    "reviewStatus": "generated"
  },
  "tags": [
    "Обработка кавычек",
    "поиск",
    "навигация"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
  }
}
</FBE-SCRIPT-CARD>*/
//Перейти на кавычки v2.9
//Автор Sclex
//Подробности тут: http://www.fictionbook.org/forum/viewtopic.php?f=7&t=4412
//И тут: http://scripts.fictionbook.org

function Run() {

 var scriptDirection="forward";
 var versionNum="v2.9";
 var versionStr="Скрипт «Перейти на кавычки "+versionNum+"».\nАвтор Sclex.\n\n";
 var chars=new Array('«','»');
 var otstupSverhu=60;
 window.external.BeginUndoUnit(document,"Переход на символы "+chars+" "+versionNum);
 var beginMarkerId,beginMarkerEl,endMarkerId,endMarkerEl,range,range1,range2,charsLength,k,myNodeValue,extremeIndex,tmpIndex,fbw_body,el;

 function searchSymbol() {
   if (scriptDirection=="forward") {
   if (el.hasChildNodes()) el=el.firstChild;
   else {
    while (el!=fbw_body && el.nextSibling==null) el=el.parentNode;
    if (el!=fbw_body) el=el.nextSibling;
    if (el==fbw_body) {
      if (confirm(versionStr+"Достигнут конец документа.\nПродолжить поиск с начала документа?")) el=fbw_body;
       else return;
    }
   }
  }
  else {
   if (el.hasChildNodes()) el=el.lastChild;
   else {
    while (el!=fbw_body && el.previousSibling==null) el=el.parentNode;
     if (el!=fbw_body) el=el.previousSibling;
    if (el==fbw_body) {
      if (confirm(versionStr+"Достигнуто начало документа.\nПродолжить поиск с конца документа?")) el=fbw_body;
      else return;
    }
   }
  }

  while (true) {
   //alert("сейчас будет el");
   //if (el.nodeType==1) alert(el.outerHTML);
   //if (el.nodeType==3) alert("el.nodeValue:\n"+el.nodeValue);
   if (el==beginMarkerEl || el==endMarkerEl) {
    alert(versionStr+"Ничего не найдено, поиск завершен.");
    return;
   }
   if (el.nodeType==3) {
    myNodeValue=el.nodeValue;
    extremeIndex=-1;
    for (k=0;k<charsLength;k++) {
     tmpIndex=scriptDirection=="forward"?myNodeValue.indexOf(chars[k]):myNodeValue.lastIndexOf(chars[k]);
     if (scriptDirection=="forward") if (tmpIndex!=-1 && (tmpIndex<extremeIndex || extremeIndex==-1)) extremeIndex=tmpIndex; else;
     else extremeIndex=Math.max(extremeIndex,tmpIndex);
    }
    if (extremeIndex!=-1) {
     setCursorIntoTextNode(el,extremeIndex);
     return;
    }
   }
   if (scriptDirection=="forward") {
    if (el.hasChildNodes()) el=el.firstChild;
    else {
     while (el!=fbw_body && el.nextSibling==null) el=el.parentNode;
     if (el!=fbw_body) el=el.nextSibling;
     if (el==fbw_body) {
       if (confirm(versionStr+"Достигнут конец документа.\nПродолжить поиск с начала документа?")) el=fbw_body;
       else return;
     }
    }
   }
   else {
    if (el.hasChildNodes()) el=el.lastChild;
    else {
     while (el!=beginMarkerEl && el!=endMarkerEl && el!=fbw_body && el.previousSibling==null) el=el.parentNode;
     if (el!=beginMarkerEl && el!=endMarkerEl && el!=fbw_body) el=el.previousSibling;
     if (el==fbw_body) {
       if (confirm(versionStr+"Достигнуто начало документа.\nПродолжить поиск с конца документа?")) el=fbw_body;
       else return;
     }
     if (el==beginMarkerEl || el==endMarkerEl) {
      alert(versionStr+"Ничего не найдено, поиск завершен.");
      return;
     } 
    }
   }
  }
 }

 function setCursorIntoTextNode(noteNode,offset) {
  if (offset!=0) {
   var s1=noteNode.nodeValue;
   var s2=s1.substr(offset);
   var s1=s1.substr(0,offset);
   var node2=document.createTextNode(s2);
   node2=noteNode.parentNode.insertBefore(node2,noteNode.nextSibling);
   noteNode.nodeValue=s1;
   noteNode=node2;
  }
  var tmpLabelNode=noteNode.parentNode.insertBefore(document.createElement("B"),noteNode);
  tmpLabelNode.scrollIntoView(true);
  window.scrollBy(0,-otstupSverhu);
  var range=document.body.createTextRange();
  range.moveToElementText(tmpLabelNode);
  range.moveEnd("character",1);
  range.select();
  tmpLabelNode.removeNode();
 }

 fbw_body=document.getElementById("fbw_body");
 charsLength=chars.length;
 if (!fbw_body) {
  alert(versionStr+"Ошибка.\n\nНеправильная структура документа.\n(Не обнаружен элемент fbw_body.)");
  return;
 }
 if (document.selection.type=="Control") {
  alert(versionStr+"Выделена картинка, а чтобы сработал скрипт,\n"+
        "надо, чтобы курсор был в тексте.\n"+
        "Так что перейти на кавычку не выйдет.");
  return;
 }
 randomNum=Math.floor((Math.random()*9)).toString()+Math.floor((Math.random()*9)).toString()+Math.floor((Math.random()*9)).toString()+Math.floor((Math.random()*9)).toString()+Math.floor((Math.random()*9)).toString()+Math.floor((Math.random()*9)).toString();
 beginMarkerId="sclexGoToQuotesBeginMarker"+randomNum;
 beginMarkerId="sclexGoToQuotesEndMarker"+randomNum;
 range=document.selection.createRange();
 range1=range.duplicate();
 range1.collapse(true);
 range1.pasteHTML("<B id="+beginMarkerId+"></B>");
 //alert(fbw_body.innerHTML);
 beginMarkerEl=document.getElementById(beginMarkerId);
 range2=range.duplicate();
 range2.collapse(false);
 range2.pasteHTML("<B id="+endMarkerId+"></B>");
 endMarkerEl=document.getElementById(endMarkerId);
 el=((scriptDirection=="forward")?endMarkerEl:beginMarkerEl);

 try {
  searchSymbol();
 }
 catch(e) {
  alert(versionStr+"Произошла какая-то ошибка.");
 }

 beginMarkerEl.removeNode(true);
 endMarkerEl.removeNode(true);
 window.external.EndUndoUnit(document);
}
