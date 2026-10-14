/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Расформатировать абзац(ы) от программного кода",
  "version": "1.5",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Снимает оболочки программного кода с выбранных абзацев.",
  "description": "Удаляет оболочки SPAN.code с сохранением содержимого и дополнительно удаляет теги SUB из HTML каждого выбранного абзаца. Границы абзацев определяются временными маркерами в начале и конце текущего текстового диапазона; после обработки скрипт пытается восстановить выделение.",
  "scope": "Полные абзацы P между началом и концом текущего диапазона; при одном курсоре — содержащий его абзац. Принадлежность диапазона fbw_body явно не проверяется.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор в нужный абзац или выделите текст, пересекающий несколько абзацев книги.",
    "Запустите скрипт и проверьте оформление целых абзацев, включая текст за пределами частичного выделения.",
    "Если обработка прервалась, проверьте оставшиеся временные маркеры и состояние отмены перед повторным запуском."
  ],
  "warnings": [
    "Скрипт обрабатывает целые абзацы, даже если выделена только часть текста или установлен один курсор.",
    "При неподходящем диапазоне возможен досрочный выход после установки временных маркеров и открытия операции отмены; завершение и полный откат не гарантированы.",
    "Помимо разметки программного кода скрипт удаляет теги нижнего индекса SUB в обрабатываемых абзацах."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "afb701004e081434142c658a863d17d70771ba4b4d39d24993e7427b7045d302",
    "reviewStatus": "generated"
  },
  "tags": [
    "Обработка форматирования"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "На штатном пути изменения абзацев и временные маркеры входят в одну операцию отмены редактора. Досрочные выходы после её начала и ошибки не гарантируют завершение группы и полный откат."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт «Расформатировать абзац(ы) от программного кода»
// Автор Sclex
// Версия 1.5

function Run() {

 var undoMsg="расформатирование абзаца(-ев) от программного кода";
 var statusBarMsg="Расформатируем абзац(ы) от программного кода…";
 //имя тэга, который будет использован для маркеров начала и конца выделения
 var markerTagName="B";

 function processPs() {
  var spans,j;
  for (var i=0; i<ps.length; i++) {
   spans=ps[i].getElementsByTagName("SPAN");
   for (j=spans.length-1; j>=0; j--)
    if (spans[j].className=="code") spans[j].removeNode(false);
   ps[i].innerHTML=ps[i].innerHTML.replace(/<\/?SUB( [^>]*)?>/gi,"");
  }
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
