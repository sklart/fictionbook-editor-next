/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Выделить предыдущий подзаголовок (текст)",
  "version": "1.4",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    },
    {
      "name": "TaKir",
      "role": "contributor",
      "contribution": "rules_compiler"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Выделяет предыдущий подзаголовок (текст).",
  "description": "Выделяет предыдущий подзаголовок (текст). Поиск выполняется по условиям именно этого файла, с установкой диапазона и прокруткой; найденное не исправляется автоматически.",
  "scope": "От текущей позиции; направление поиска определяется конкретным скриптом.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Оставьте курсор в обрабатываемом элементе или точке начала поиска; непустое выделение не требуется.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже."
  ],
  "warnings": [],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "902561b1fd4364777bf6c406ef8c7477987f92d37782a5c57d613bf459362852",
    "reviewStatus": "generated"
  },
  "tags": [
    "Поиск форматирования",
    "поиск",
    "навигация"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт «Выделить предыдущий текстовый подзаголовок» 
// Автор Sclex, сборка TaKir
// Версия 1.4

function Run() {

 var undoMsg="переход на предыдущий текстовый подзаголовок";
 var statusBarMsg="Переходим на предыдущий текстовый подзаголовок…";
 var notFoundMsg="До начала документа не нашлось ни одного текстового подзаголовка.";
 try { var nbspChar=window.external.GetNBSP(); var nbspEntity; if (nbspChar.charCodeAt(0)==160) nbspEntity="&nbsp;"; else nbspEntity=nbspChar; }
 catch(e) { var nbspChar=String.fromCharCode(160); var nbspEntity="&nbsp;";};
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

 function getPreviousNode(el) {
  //alert("Вошли в getPreviousNode.");
  if (el.lastChild && el.nodeName!="P")
   el=el.lastChild;
  else {
   while (el && !el.previousSibling)
    el=el.parentNode;
   if (el && el.previousSibling) el=el.previousSibling; 
  }
  return el;
 }
 
 function getPreviousP(el) {
  var savedEl=el;
  while (el && (el.nodeName!="P" || el==savedEl))
   el=getPreviousNode(el);
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
 var tr,el,prv,pm,savePrevious,saveFirstEmpty,previousPtr;
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
 tr3.collapse(true);
 var ptr=tr3.parentElement();
 //alert("blockEndNode: "+ptr.outerHTML);
 ptr=getPreviousP(ptr);
 
 while (ptr && fbwBody.contains(ptr)) {
  if (checkP(ptr)) break;
  ptr=getPreviousP(ptr);
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
