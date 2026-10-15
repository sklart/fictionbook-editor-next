/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Удалить пустые строки в выделении",
  "version": "1.3",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Удаляет пустые абзацы в области между началом и концом текущего выделения.",
  "description": "Берёт начальный и конечный родительские элементы текстового диапазона, обходит между ними абзацы и удаляет те, что состоят только из пробелов или неразрывных пробелов. Абзац без соседей не удаляет. Код не проверяет, что выделение непустое; при одном курсоре может проверить текущий элемент. Результат показывает в строке состояния.",
  "scope": "Элементы между началом и концом текущего диапазона внутри fbw_body; без непустого выделения возможна обработка элемента под курсором.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Выделите нужные абзацы внутри книги и запустите команду. Не запускайте её с одним курсором, если не хотите затронуть текущий пустой абзац.",
    "Проверьте крайние абзацы и число удалённых строк в строке состояния."
  ],
  "warnings": [
    "Проверка непустого выделения в коде отключена; при одном курсоре пустой текущий абзац тоже может быть удалён.",
    "Первый элемент берётся как parentElement начала диапазона без подъёма к P; при начале внутри встроенного форматирования возможна обработка не того узла."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "99f760eae741d1c5f20309dfe28a3d463a76baefebad0f9f383ad4e0c8619bb9",
    "reviewStatus": "generated"
  },
  "tags": [
    "Заголовки, подзаголовки, пустые строки"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Если основной текст книги недоступен, скрипт останавливается, не закрыв начатую группу отмены."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт «Удалить пустые строки в выделении»
// Автор Sclex
// Версия 1.3

function Run() {

 var undoMsg="Удаление пустых строк в выделени";
 var statusBarMsg="Удаляем пустые строки в выделении…";
 //имя тэга, который будет использован для маркеров начала и конца выделения
 var markerTagName="I";

 var removesCnt=0;
 
 function processPs() {
  for (var i=ps.length-1; i>=0; i--)
   if (isLineEmpty(ps[i]) && (ps[i].nextSibling || ps[i].previousSibling)) {
    ps[i].removeNode(true); 
    removesCnt++;
   }
 }

 try { var nbspChar=window.external.GetNBSP(); var nbspEntity; if (nbspChar.charCodeAt(0)==160) nbspEntity="&nbsp;"; else nbspEntity=nbspChar;}
 catch(e) { var nbspChar=String.fromCharCode(160); var nbspEntity="&nbsp;";} 
 var emptyLineRegExp=new RegExp("^( | |&nbsp;|"+nbspChar+")*?$","i");
 
 function isLineEmpty(ptr) {
  return emptyLineRegExp.test(ptr.innerHTML.replace(/<(?!img)[^>]*?>/gi,""));
 }
 
 function getNextNode(el) {
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

 var tr3=document.selection.createRange();
 tr3.collapse(true);
 var blockStartEl=tr3.parentElement();
 tr3=document.selection.createRange();
 tr3.collapse(false);
 var blockEndEl=tr3.parentElement();
 //alert("blockStartEl: "+blockStartEl.outerHTML);
 //alert("blockEndEl: "+blockEndEl.outerHTML);
 var ptr=blockStartEl;
 while (ptr && fbwBody.contains(ptr)) {
  //alert("Добавляем в массив: "+ptr.outerHTML);
  ps.push(ptr);
  if (ptr===blockEndEl) break;
  ptr=getNextP(ptr);
 }
 
 processPs();

 /* 
 var tr1=document.body.createTextRange();
 tr1.moveToElementText(blockStartEl);
 if (tr1.moveStart("character",1)==1)
  tr1.moveStart("character",-1);
 var tr2=document.body.createTextRange();
 tr2.moveToElementText(blockEndEl);
 tr1.setEndPoint("EndToEnd",tr2);
 tr1.select();
 */

 //if (processFlag) addPToArray(pp);
 try { window.external.SetStatusBarText("Было удалено пустых строк в выделении: "+removesCnt+"."); }
 catch(e) {} 
 window.external.EndUndoUnit(document);
}
