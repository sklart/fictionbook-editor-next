/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Удалить все теги иллюстраций - теги image",
  "version": "1.6",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Удаляет все элементы иллюстраций в тексте и связанные с ними лишние пустые абзацы.",
  "description": "Внутри fbw_body удаляет все SPAN.image, DIV.image и оставшиеся IMG, затем часть пустых абзацев возле удалённых блочных иллюстраций. По возможности сохраняет пустые абзацы, необходимые после заголовков и эпиграфов. Бинарные вложения binobj не удаляются; в конце выводится статистика.",
  "scope": "Все SPAN.image, DIV.image и IMG внутри fbw_body, а также соседние пустые абзацы блочных иллюстраций; выделение не используется.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Сохраните копию книги и запустите скрипт для удаления вхождений иллюстраций в тексте.",
    "Просмотрите отчёт о числе удалённых элементов и пустых абзацев. Бинарные вложения останутся в книге, проверьте их отдельно."
  ],
  "warnings": [
    "Удаляются все изображения в тексте, включая IMG вне контейнеров image; соответствующие бинарные вложения остаются.",
    "Проверка пустых строк создаёт регулярное выражение до получения пользовательского неразрывного пробела из редактора, поэтому нестандартный символ пробела может не распознаться."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "d2aaff2be265b4dfa11801a2ac52891f79e924365fe784c5fa5c97b5341a888d",
    "reviewStatus": "generated"
  },
  "tags": [
    "Иллюстрации",
    "изображения"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Операция отмены начинается только при первом удалении и завершается после прохода. Явных досрочных выходов внутри неё нет; полный откат при ошибке не проверен."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Удалить все теги иллюстраций - теги image -  v1.6"
// Автор Sclex

function Run() {

 var versionStr="1.6";
 
 var re0=new RegExp("^( | |&nbsp;|"+nbspChar+")*?$","i");
 
 try { var nbspChar=window.external.GetNBSP(); var nbspEntity; if (nbspChar.charCodeAt(0)==160) nbspEntity="&nbsp;"; else nbspEntity=nbspChar;}
 catch(e) { var nbspChar=String.fromCharCode(160); var nbspEntity="&nbsp;";}

 function isLineEmpty(ptr) {
  return re0.test(ptr.innerHTML.replace(/<(?!img)[^>]*?>/gi,""));
 }
 
 function emptyLineIsNeededForValidity(elem) {
  if (!elem.previousSibling || (elem.previousSibling.nodeName=="DIV"
    && (elem.previousSibling.className=="epigraph" || elem.previousSibling.className=="title"))
    && elem.nextSibling && elem.nextSibling.nodeName=="P" && isLineEmpty(elem.nextSibling)
    && elem.nextSibling.nextSibling==null)
   return true;
   else return false;
 }
 
 var emptyLines=[];
 var fbwBody=document.getElementById("fbw_body");
 var undoUnitName="удаление всех тегов иллюстраций — тегов image";
 var firstRemoving=true;
 var elem, elem2, elems2, i, j;
 var replaceCounter1=0;
 var replaceCounter2=0;
 var replaceCounter3=0;
 elems=fbwBody.getElementsByTagName("SPAN");
 for (var i=elems.length-1; i>=0; i--) {
  elem=elems[i];
  if (elem.className && elem.className.toLowerCase()=="image") {
   if (firstRemoving) {
    window.external.BeginUndoUnit(document, undoUnitName);
    firstRemoving=false;
   }
   elem.removeNode(true);
   replaceCounter2+=1;
  }
 }
 var elems=fbwBody.getElementsByTagName("DIV");
 for (var i=elems.length-1; i>=0; i--) {
  elem=elems[i];
  if (elem.className && elem.className.toLowerCase()=="image") {
   if (firstRemoving) {
    window.external.BeginUndoUnit(document, undoUnitName);
    firstRemoving=false;
   }
   if (elem.nextSibling && elem.nextSibling.nodeName=="P" && isLineEmpty(elem.nextSibling))
    if (!emptyLineIsNeededForValidity(elem))
     if (emptyLines[emptyLines.length-1]!==elem.nextSibling) emptyLines.push(elem.nextSibling);
   if (elem.previousSibling && elem.previousSibling.nodeName=="P" && isLineEmpty(elem.previousSibling))
    if (emptyLines[emptyLines.length-1]!=elem.previousSibling) emptyLines.push(elem.previousSibling);
   elem.removeNode(true);
   replaceCounter1+=1;
  }
 }
 elems=fbwBody.getElementsByTagName("IMG");
 for (var i=elems.length-1; i>=0; i--) {
  elem=elems[i];
  if (true) {
   if (firstRemoving) {
    window.external.BeginUndoUnit(document, undoUnitName);
    firstRemoving=false;
   }
   elem.removeNode(true);
   replaceCounter3+=1;
  }
 }
 var removedEmptyLinesCnt=emptyLines.length;
 for (i=0; i<removedEmptyLinesCnt; i++)
  emptyLines[i].removeNode(true);
 var s;
 s="Скрипт «Удалить все теги иллюстраций – теги image» v"+versionStr+" (автор Sclex).\n"+
   "\n"+
   "Удалено тегов «блочных» иллюстраций: "+replaceCounter1+".\n"+
   "Удалено тегов внутриабзацных иллюстраций: "+replaceCounter2+".\n"+
   (replaceCounter3==0?"":"Удалено тегов непонятных (вероятно, ошибочных) иллюстраций: "+replaceCounter3+".\n")+
   "\n"+
   "Всего удалено тегов иллюстраций (тегов image): "+(replaceCounter1+replaceCounter2+replaceCounter3)+".\n"+
   "\n"+
   "Удалено пустых строк (которые были рядом с иллюстрациями): "+removedEmptyLinesCnt+".";
 if (!firstRemoving) window.external.EndUndoUnit(document);
 MsgBox(s);
}
