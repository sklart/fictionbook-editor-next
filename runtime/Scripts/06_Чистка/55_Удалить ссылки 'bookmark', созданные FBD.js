/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Удалить ссылки 'bookmark', созданные FBD",
  "version": "Не указана",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Удаляет ссылки 'bookmark', созданные FBD.",
  "description": "Рекурсивно находит служебные ссылки bookmark, созданные FBD, очищает разметку/символы в соответствующих абзацах и сохраняет обновлённый HTML. Не является удалением всех обычных ссылок книги.",
  "scope": "Область определяется условиями и обходом DOM в данном скрипте.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Запустите команду из меню или дерева скриптов.",
    "Просмотрите результат до сохранения. Для структурной правки проверьте книгу и связанные ссылки."
  ],
  "warnings": [
    "Скрипт содержит операции изменения документа. Перед массовой обработкой сохраните отдельную копию книги."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "6caaae762f44f0568331a56c6fab130221671643700c51b16684a8f2e15f0278",
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
// Скрипт "Удалить ссылки "bookmark", созданные FBD"
// Автор скрипта Sclex
// Версия скрипта 1.0

function Run() {

 try { var nbspChar=window.external.GetNBSP(); var nbspEntity; if (nbspChar.charCodeAt(0)==160) nbspEntity="&nbsp;"; else nbspEntity=nbspChar;}
 catch(e) { var nbspChar=String.fromCharCode(160); var nbspEntity="&nbsp;";}
 
 var s,result,spacesCnt;
 var tagsRegExp=new RegExp("<(?!/?(IMG|A))[^>]+>","gi");
 var tagsRegExp_="";
 var checkRegExp=new RegExp("^((?: |\\xA0)*)(<A\\b[^>]+>.+?</A>)((?: |\\xA0)*)","i");
 var refRegExp=new RegExp("(<A\\b[^>]+>.+?</A>)","i");
 var refRegExp_="";
 var nbspCharRegExp=new RegExp(nbspChar,"g");
 
 
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
  
 function removeSymbolsBeyondTags(str,beginIndex,howManySymbolsToRemove,substrToInsert) { 
  var i=0; 
  var removedSymbolsCnt=0; 
  var insideTag=false; 
  var len=str.length; 
  var symbolBeyondTagsCnt=0; 
  var str2=""; 
  var ch; 
   while (i<len) { 
   ch=str.charAt(i); 
    if (ch=="<") { 
     insideTag=true; 
     str2+=ch; 
    } 
    if (!insideTag && ch!="<" && ch!=">") { 
     symbolBeyondTagsCnt++; 
     if (symbolBeyondTagsCnt<beginIndex || symbolBeyondTagsCnt+1>beginIndex+howManySymbolsToRemove) 
      str2+=ch; 
     else { 
      if (removedSymbolsCnt==0) str2+=substrToInsert; 
      removedSymbolsCnt++; 
     } 
    } 
    if (insideTag && ch!="<" && ch!=">") 
     str2+=ch; 
    if (ch==">") { 
     insideTag=false; 
     str2+=ch; 
    } 
    i++; 
  } 
  return str2; 
 }
    
 function processP(el2) {
  s=el2.innerHTML.replace(/&nbsp;/g,String.fromCharCode(160)).replace(nbspCharRegExp,String.fromCharCode(160));
  //alert(s);
  s2=s.replace(tagsRegExp,tagsRegExp_);
  //alert("s: \n\n"+s2);
  if (!checkRegExp.test(s2)) return;
  result=checkRegExp.exec(s2);
  spacesCnt=result[1].length+result[3].length;
  s=s.replace(refRegExp,refRegExp_);
  el2.innerHTML=removeSymbolsBeyondTags(s,1,spacesCnt,"").replace(String.fromCharCode(160),nbspChar);
  replacesCnt++;
  //alert("result: "+el2.innerHTML);
 }

 function recursive(el) {
  while (el) {
   if (el.nodeName=="DIV" && el.className=="body" && el.getAttribute("fbname")=="notes" && el.firstChild)
    recursive(el.firstChild);
   else if (el.nodeName=="DIV" && el.className!="body" && (el.className!="annotation" || el.parentNode.className=="section") &&
            el.className!="history" && el.firstChild)
    recursive(el.firstChild);
   if (el.nodeName=="P") processP(el);
   el=el.nextSibling;
  }
 }
 
 var replacesCnt=0;
 var fbwBody=document.getElementById("fbw_body");
 window.external.BeginUndoUnit(window.document,'удаление ссылок "bookmark", созданных FBD');
 recursive(fbwBody);
 try {
    window.external.SetStatusBarText(units(replacesCnt, {nom: 'Была удалена', gen: 'Были удалены', plu: 'Было удалено'})
      +" "+replacesCnt+" "+
          units(replacesCnt, {nom: 'ссылка', gen: 'ссылки', plu: 'ссылок'})+".");
 }
 catch(e)
 {}
 window.external.EndUndoUnit(window.document);

}
