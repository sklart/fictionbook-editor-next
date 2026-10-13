/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Удалить разделы, внутри которых только заголовок и, возможно, пустые строки",
  "version": "1.0",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Удаляет разделы, внутри которых только заголовок и, возможно, пустые строки.",
  "description": "Проверяет содержимое секций и удаляет целиком контейнеры, где остались только заголовок и допустимые пустые строки. Использует removeNode(true), поэтому исчезает и текст удаляемого заголовка.",
  "scope": "Область определяется условиями и обходом DOM в данном скрипте.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Запустите команду из меню или дерева скриптов.",
    "Просмотрите результат до сохранения. Для структурной правки проверьте книгу и связанные ссылки."
  ],
  "warnings": [
    "Скрипт содержит операции изменения документа. Перед массовой обработкой сохраните отдельную копию книги.",
    "Удаляет текст заголовков вместе с признанными пустыми разделами; это не просто снятие внешнего тега."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "34414a438026dda57449734d6688eeb223ab27d1a1b7ba96d6ee2ca55b6a8106",
    "reviewStatus": "generated"
  },
  "tags": [
    "Чистка"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Удалить разделы, внутри которых только заголовок и, возможно, пустые строки"
// Версия 1.0
// Автор скрипта – Sclex

function Run() {

 function isEmptyLine(ptr) {
  if (ptr)
   if (ptr.nodeType)
    if (ptr.nodeType==1)
     if (ptr.nodeName)
      if (ptr.nodeName.toUpperCase()=="P")
       if (emptyLineRE.test(ptr.innerHTML.replace(new RegExp(nbspChar+"|<(?!img)[^>]*?>","gi"),""))) return true;
  return false;
 }

 function units(num, cases) {
  num = Math.abs(num);

  var word = '';

  if (num.toString().indexOf('.') > -1) {
   word = cases.gen;
  } else { 
   word = (
    num % 10 == 1 && num % 100 != 11 
     ? cases.nom
     : num % 10 >= 2 && num % 10 <= 4 && (num % 100 < 10 || num % 100 >= 20) 
      ? cases.gen
      : cases.plu
   );
  }
  return word;
 }
  
 function sectionNeedsRemoving(sect) {
  var elInSect=sect.firstChild;
  var wasTitle=false;
  var nextElInSect;
  while (elInSect) {
    if (elInSect.nodeName=="DIV" && elInSect.className=="title") wasTitle=true;
    nextElInSect=elInSect.nextSibling;
    if (elInSect.nodeName=="DIV" && elInSect.className=="section")
     if (sectionNeedsRemoving(elInSect)) {
      elInSect.removeNode(true);
      removesCnt++;
     }
     else return false;
    if (elInSect.nodeName!="P" && (elInSect.nodeName!="DIV" || elInSect.className!="title")) return false;
    if (elInSect.nodeName=="P" && !isEmptyLine(elInSect)) return false;
    elInSect=nextElInSect;
  }
  if (wasTitle) return true;
  return false;
 }
 
 function recursive(el) {
  //alert("recursive: "+el.outerHTML);
  var nextNode;
  while (el) {
   nextNode=el.nextSibling;
   if (el.nodeName=="DIV" && el.className=="body" && el.firstChild)
    recursive(el.firstChild);
   else if (el.nodeName=="DIV" && el.className=="section")
    if (sectionNeedsRemoving(el)) {
      el.removeNode(true);
      removesCnt++;
    }
   el=nextNode;
  }
 }
 
 try { var nbspChar=window.external.GetNBSP(); var nbspEntity; if (nbspChar.charCodeAt(0)==160) nbspEntity="&nbsp;"; else nbspEntity=nbspChar;}
 catch(e) { var nbspChar=String.fromCharCode(160); var nbspEntity="&nbsp;";}
 
 var emptyLineRE=new RegExp("^( | |&nbsp;|"+nbspChar+")*?$","i");
 var removesCnt=0;

 var fbwBody=document.getElementById("fbw_body");
 
 window.external.BeginUndoUnit(window.document,'удаление разделов, внутри которых только заголовок и, возможно, пустые строки');
 recursive(fbwBody.firstChild);
 
 try {
    window.external.SetStatusBarText(units(removesCnt, {nom: 'Был удален', gen: 'Были удалены', plu: 'Было удалено'})
      +" "+removesCnt+" "+
      units(removesCnt, {nom: 'раздел', gen: 'раздела', plu: 'разделов'})+".");
 }
 catch(e)
 {}
 
 window.external.EndUndoUnit(window.document);
}
