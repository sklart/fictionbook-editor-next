/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Выделить следующую аннотацию",
  "version": "1.2",
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
  "summary": "Выделяет следующую аннотацию целиком.",
  "description": "От конца текущего диапазона ищет DIV.annotation с абзацем P. Обходит абзацы вперёд, пропускает контейнер, в котором находится исходная позиция, и выделяет найденный блок аннотации целиком. Прокручивает окно к результату; содержимое книги не меняет.",
  "scope": "Поиск вперёд от конца текущего диапазона среди DIV.annotation внутри fbw_body.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор в книге или выделите фрагмент, от конца которого нужно начать поиск.",
    "Запустите команду; при успехе редактор выделит аннотацию целиком и прокрутит окно к результату.",
    "Если подходящего блока дальше нет, скрипт покажет сообщение; текст книги он не меняет."
  ],
  "warnings": [
    "Блок без абзацев P не попадёт в поиск."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "69481601152e5356fdf9d5d3b1a9fd5c6b0c51e64619f1c03123394535a34754",
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
// Скрипт «Выделить следующую аннотацию» (annotation)
// Автор Sclex, сборка TaKir
// Версия 1.2

function Run() {

 var blockElementClass="annotation"; // класс блочного элемента, который (элемент) нужно искать
 var undoMsg="переход на следующую аннотацию";
 var statusBarMsg="Переходим на следующую аннотацию…";
 var notFoundMsg="До конца документа не нашлось ни одной аннотации.";
 
try { var nbspChar=window.external.GetNBSP(); var nbspEntity; if (nbspChar.charCodeAt(0)==160) nbspEntity="&nbsp;"; else nbspEntity=nbspChar; }
 catch(e) { var nbspChar=String.fromCharCode(160); var nbspEntity="&nbsp;";};
 var re2=new RegExp(" |&nbsp;|"+nbspChar,"g");
 
 function checkP(elem1) {
  //if (elem1.className=="text-author") return true;
  if (hasAmongParents(elem1,"annotation")) return true;
  return false;
 }

 function hasAmongParents(elem2,nameOfClass) {
  while (elem2 && elem2.nodeName!="BODY") {
    if (elem2.nodeName=="DIV" && elem2.className==nameOfClass) return true;
    elem2=elem2.parentNode;
  }
  return false;
 }
 
 function getParentWithClass(elem3,nameOfClass) {
  while (elem3 && elem3.nodeName!="BODY") {
    if (elem3.nodeName=="DIV" && elem3.className==nameOfClass) return elem3;
    elem3=elem3.parentNode;
  }
  return null;
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
 var blockElementAtStart=getParentWithClass(ptr,blockElementClass);
 //alert("blockEndNode: "+ptr.outerHTML);
 ptr=getNextP(ptr);
 
 while (ptr && fbwBody.contains(ptr)) {
  if (checkP(ptr)&& getParentWithClass(ptr,blockElementClass)!=blockElementAtStart) break;
  ptr=getNextP(ptr);
 }
 
 if (ptr && fbwBody.contains(ptr)) {
  var tr1=document.body.createTextRange();
  var parentElem=getParentWithClass(ptr,blockElementClass);
  if (parentElem) {
   tr1.moveToElementText(parentElem);
   if (tr1.moveStart("character",1)==1)
    tr1.moveStart("character",-1);
   tr1.moveEnd("character",-1);
   tr1.select();
   scrollIfItNeeds();
   var scriptResult="Found";
  }
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
