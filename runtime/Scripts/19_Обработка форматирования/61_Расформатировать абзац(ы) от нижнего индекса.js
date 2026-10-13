/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Расформатировать абзац(ы) от нижнего индекса",
  "version": "1.5",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Снимает разметку: абзац(ы) от нижнего индекса.",
  "description": "Обрабатывает полные абзацы выбранного диапазона. Функция processPs добавляет либо удаляет конкретную inline-разметку данного варианта; границы выделения временно помечает. Например, выделение части строки может повлиять на весь её абзац, а не только на символы диапазона.",
  "scope": "Выделенный диапазон либо полные абзацы, которые он пересекает; см. описание варианта.",
  "modifiesDocument": true,
  "requiresSelection": true,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Подготовьте выделение, описанное в scope и условиях запуска; некоторые команды работают с целыми абзацами.",
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
    "sha256": "f9410ce83e431177732599c7b7ae5c18f88ad84e4b0e9db0466bdc529ff8cc83",
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
// Скрипт «Сформатировать абзац(ы) от нижнего индекса»
// Автор Sclex
// Версия 1.5

function Run() {

 var undoMsg="расформатирование абзаца(-ев) от нижнего индекса";
 var statusBarMsg="Расформатируем абзац(ы) от нижнего индекса…";
 //имя тэга, который будет использован для маркеров начала и конца выделения
 var markerTagName="B";

 function processPs() {
  for (var i=0; i<ps.length; i++)
   ps[i].innerHTML=ps[i].innerHTML.replace(/<\/?(SUB)( [^>]*)?>/gi,"");
 }
 
 function getNextNode(el) {
  //alert("Вошли в getNextNode.");
  if (el.firstChild && el.nodeName!="P")
   el=el.firstChild;
  else {
   while (!el.nextSibling)
    el=el.parentNode;
   el=el.nextSibling; 
  }
  return el;
 }
 
 function getNextP(el) {
  var savedEl=el;
  while (el && (el.nodeName!="P" || el==savedEl))
   el=getNextNode(el);
  return el;
 }

 var rndm, startId,endId;

 function insertSelectionMarkers() {
  rndm=Math.round(Math.random()*100000).toString();
  startId="BlockStart"+rndm;
  endId="BlockEnd"+rndm;
  var tr=document.selection.createRange();
  var tr2=tr.duplicate();
  tr.collapse();
  tr.pasteHTML("<"+markerTagName+" id="+startId+"></"+markerTagName+">");
  tr2.collapse(false);
  tr2.pasteHTML("<"+markerTagName+" id="+endId+"></"+markerTagName+">");
 }
 
 try { var nbspChar=window.external.GetNBSP(); var nbspEntity; if (nbspChar.charCodeAt(0)==160) nbspEntity="&nbsp;"; else nbspEntity=nbspChar;}
 catch(e) { var nbspChar=String.fromCharCode(160); var nbspEntity="&nbsp;";}

 var s;
 var ps=[];
 var tr,el,prv,pm,saveNext,saveFirstEmpty,nextPtr;
 var errMsg="Нет выделения.\n\nПеред запуском скрипта нужно выделить абзацы, которые будут обработаны.";
 tr=document.selection.createRange();
 /* if (!tr || tr.compareEndPoints("StartToEnd",tr)==0) {
  MsgBox(errMsg);
  return;
 } */
 if (tr.parentElement().nodeName=="TEXTAREA" || tr.parentElement().nodeName=="INPUT") {
  MsgBox("Ошибка. Должно быть выделение в тексте книги, а не в поле ввода.");
  return;
 }
 window.external.BeginUndoUnit(document,undoMsg);
 try { window.external.SetStatusBarText(statusBarMsg); }
 catch(e) {}
 var fbwBody=document.getElementById("fbw_body");

 insertSelectionMarkers();
 var blockStartNode=document.getElementById(startId);
 var blockEndNode=document.getElementById(endId);
 var blockStartEl=blockStartNode;
 while (blockStartEl && blockStartEl.nodeName!="BODY" && blockStartEl.nodeName!="P")
  blockStartEl=blockStartEl.parentNode;
 if (!blockStartEl || blockStartEl.nodeName=="BODY") return;
 //alert("blockStartEl: "+blockStartEl.outerHTML); 
 var blockEndEl=blockEndNode;
 while (blockEndEl && blockEndEl.nodeName!="BODY" && blockEndEl.nodeName!="P")
  blockEndEl=blockEndEl.parentNode;
 if (blockEndEl && blockEndEl.nodeName=="BODY") {
  blockEndEl=blockEndNode;
  if (blockEndEl.previousSibling && blockEndEl.previousSibling.nodeName=="P")
   blockEndEl=blockEndEl.previousSibling;
  if (!blockEndEl || blockEndEl.nodeName!="P") return;
 }
 //alert("blockEndEl: "+blockEndEl.outerHTML);
 var ptr=blockStartEl;
 while (ptr) {
  //alert("Добавляем в массив: "+ptr.outerHTML);
  ps.push(ptr);
  if (ptr===blockEndEl) break;
  ptr=getNextP(ptr);
 }
 
 processPs();

 // удаляем маркеры блока
 blockStartNode=document.getElementById(startId);
 blockEndNode=document.getElementById(endId);
 if (blockStartNode) blockStartNode.removeNode(true);
 if (blockEndNode) blockEndNode.removeNode(true);
 
 var tr1=document.body.createTextRange();
 tr1.moveToElementText(blockStartEl);
 if (tr1.moveStart("character",1)==1)
  tr1.moveStart("character",-1);
 var tr2=document.body.createTextRange();
 tr2.moveToElementText(blockEndEl);
 tr1.setEndPoint("EndToEnd",tr2);
 tr1.select();

 //if (processFlag) addPToArray(pp);
 try { window.external.SetStatusBarText("ОК"); }
 catch(e) {} 
 window.external.EndUndoUnit(document);
}
