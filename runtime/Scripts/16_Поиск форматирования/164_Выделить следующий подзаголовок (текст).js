/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Выделить следующий подзаголовок (текст)",
  "version": "1.4",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    },
    {
      "name": "TaKir",
      "role": "contributor",
      "contribution": "assembly"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Выделяет следующий текстовый подзаголовок.",
  "description": "От конца текущего диапазона ищет P.subtitle, в котором есть латинская или русская буква, цифра либо вертикальная черта. Начинает со следующего абзаца, поэтому текущий подзаголовок пропускает; найденный абзац выделяет и прокручивает к нему окно. Книгу не меняет.",
  "scope": "Поиск вперёд от конца диапазона среди абзацев P внутри fbw_body.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор в книге или выделите фрагмент, от границы которого нужно начать поиск.",
    "Запустите команду; при успехе редактор выделит подходящий блок и прокрутит окно к нему.",
    "Если совпадений дальше в выбранном направлении нет, скрипт покажет сообщение; книгу он не изменяет."
  ],
  "warnings": [
    "Шаблон также принимает вертикальную черту как «текстовый» знак; такой подзаголовок может попасть в результат."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "a95bfcd24d59e597c76f0c8300a98417145a9d9431af345edc979f3f1d7544cc",
    "reviewStatus": "generated"
  },
  "tags": [
    "Поиск форматирования",
    "поиск",
    "навигация"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "На штатном пути меняются только выделение и положение прокрутки; содержимое книги не меняется. Формальная группа отмены открывается и закрывается, но откат содержимого здесь неприменим."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт «Выделить следующий текстовый подзаголовок» (subtitle)
// Автор Sclex, сборка TaKir
// Версия 1.4

function Run() {

 var undoMsg="переход на следующий текстовый подзаголовок";
 var statusBarMsg="Переходим на следующий текстовый подзаголовок…";
 var notFoundMsg="До конца документа не нашлось ни одного текстового подзаголовка.";
 try { var nbspChar=window.external.GetNBSP(); var nbspEntity; if (nbspChar.charCodeAt(0)==160) nbspEntity="&nbsp;"; else nbspEntity=nbspChar; }
 catch(e) { var nbspChar=String.fromCharCode(160); var nbspEntity="&nbsp;";};
 var re2=new RegExp(" |&nbsp;|"+nbspChar,"g");
 var subtitleRegExp=new RegExp("[A-Za-zА-яЁё|0-9](.*)");

 
 function checkP(elem1) {
  if (elem1.className=="subtitle" && subtitleRegExp.test(elem1.innerHTML.replace(/<[^>]*?>/gi,""))) return true;
  //if (hasAmongParents(elem1,"epigraph")) return true;
  return false;
 }

 function hasAmongParents(elem2,nameOfClass) {
  while (elem2 && elem2.nodeName!="BODY") {
    if (elem2.nodeName=="DIV" && elem2.className==nameOfClass) return true;
    elem2=elem2.parentNode;
  }
  return false;
 }
 
 var emptyLineRegExp=new RegExp("^( | |&nbsp;|"+nbspChar+")*?$","i");
 
 function isLineEmpty(ptr) {
  return emptyLineRegExp.test(ptr.innerHTML.replace(/<[^>]*?>/gi,""));
 }
    
 function getNextNode(el) {
  //alert("Вошли в getNextNode.");
  if (el.firstChild && el.nodeName!="P")
   el=el.firstChild;
  else {
   while (el && !el.nextSibling)
    el=el.parentNode;
   if (el && el.nextSibling) el=el.nextSibling; 
  }
  return el;
 }
 
 function getNextP(el) {
  var savedEl=el;
  while (el && (el.nodeName!="P" || el==savedEl))
   el=getNextNode(el);
  return el;
 }

 function scrollIfItNeeds() { 
  var selection = document.selection;
  if (selection) {
    var range = selection.createRange();
    var rect = range.getBoundingClientRect();
    // var correction = (rect.bottom - document.documentElement.clientHeight/2); // центр
   var correction = (rect.bottom - document.documentElement.clientHeight/2); // верх
   // var popravka = (rect.bottom - document.documentElement.clientHeight/8* 6); // низ
   window.scrollBy(0, correction);
  }
 }
 
 var s;
 var tr,el,prv,pm,saveNext,saveFirstEmpty,nextPtr;
 var errMsg="Нет выделения.\n\nПеред запуском скрипта нужно выделить абзацы, которые будут обработаны.";
 tr=document.selection.createRange();

 if (tr.parentElement().nodeName=="TEXTAREA" || tr.parentElement().nodeName=="INPUT") {
  MsgBox("Ошибка. Должно быть выделение в тексте книги, а не в поле ввода.");
  return;
 }
 window.external.BeginUndoUnit(document,undoMsg);
 try { window.external.SetStatusBarText(statusBarMsg); }
 catch(e) {}
 var fbwBody=document.getElementById("fbw_body");

 var tr3=document.selection.createRange();
 tr3.collapse(false);
 var ptr=tr3.parentElement();
 //alert("blockEndNode: "+ptr.outerHTML);
 ptr=getNextP(ptr);
 
 while (ptr && fbwBody.contains(ptr)) {
  if (checkP(ptr)) break;
  ptr=getNextP(ptr);
 }
 
 if (ptr && fbwBody.contains(ptr)) {
  var tr1=document.body.createTextRange();
  tr1.moveToElementText(ptr);
  if (tr1.moveStart("character",1)==1)
   tr1.moveStart("character",-1);
  tr1.moveEnd("character",-1);
  tr1.select();
  scrollIfItNeeds();
  var scriptResult="Found";
 }
 else {
  var scriptResult="NotFound";
  MsgBox(notFoundMsg);
 }

 try { window.external.SetStatusBarText("ОК"); }
 catch(e) {} 
 window.external.EndUndoUnit(document);
 return scriptResult;
}
