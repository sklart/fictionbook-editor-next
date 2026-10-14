/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Перейти на следующее совмещение курсива с жирностью",
  "version": "1.0",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Переходит вперёд к вложенному сочетанию курсива и жирности.",
  "description": "Ищет вперёд место, где тег I или EM вложен в B либо STRONG или наоборот, и ставит курсор в найденном оформлении. При достижении границы документа спрашивает о продолжении с противоположного края. Цвет, CSS-стили и обычное соседство тегов не проверяет.",
  "scope": "Поиск вперёд от конца текущего выделения или позиции курсора внутри fbw_body; после подтверждения возможен обход с противоположного края.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор в тексте или выделите фрагмент, от границы которого нужен поиск.",
    "Запустите поиск вперёд по совмещению курсивного и полужирного тегов.",
    "При достижении границы книги подтвердите продолжение с другого края или остановите поиск."
  ],
  "warnings": [
    "Проверяются только вложенные I/EM и B/STRONG; оформление через CSS не учитывается.",
    "Из-за порядка логических операций проверка текстового содержимого применяется лишь к полужирной ветви. Пустой курсивный тег внутри жирного может быть принят за находку.",
    "При выделенной картинке группа отмены остаётся открытой. Ошибка во время поиска тоже может прервать очистку временных маркеров и закрытие группы."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "3c4ddec71475f04b9944de5adba0b24f3e3754562ed5d77bb28a51b7e66648e9",
    "reviewStatus": "generated"
  },
  "tags": [
    "Поиск форматирования",
    "поиск",
    "навигация"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "При штатном поиске временные маркеры удаляются, а группа отмены закрывается. При выделенной картинке или ошибке поиска закрытие не гарантировано; полный откат не проверен."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт «Выделить следующее совмещение курсива с жирностью»
//Автор Sclex
//Топик про скрипты FBE: http://www.fictionbook.org/forum/viewtopic.php?t=4412
//Мой сайт про мои скрипты: http://scripts.fictionbook.org

function Run() {
 var versionStr="переход на следующее совмещение курсива с жирностью v1.0";
 var scriptDirection="forward";
 var isItTagWeLookingFor1=function (t) { if (t.nodeName=="I" || t.nodeName=="EM") return true; else return false;}
 var isItTagWeLookingFor2=function (t) { if (t.nodeName=="B" || t.nodeName=="STRONG") return true; else return false;}
 var paragraphIndent=false;
 
 var randomNum,beginMarkerId,endMarkerId,beginMarkerEl,endMarkerEl,range1,range2,el,el2,el3,b,placeWhereToStop,fbw_body,nowInTagWeLookingFor,tag1WeLookingFor_count,tag2WeLookingFor_count;
 window.external.BeginUndoUnit(document,versionStr);
 
 function scrollIfItNeeds() {
  var selection = document.selection;
  if (selection) {
    var range = selection.createRange();
    var rect = range.getBoundingClientRect();
    
    // Проверяем, находится ли выделение менее чем в 20 пикселях от нижнего края окна
    if (document.documentElement.clientHeight - rect.bottom < 20) {
      // Прокручиваем документ на 50 пикселей вниз
      window.scrollBy(0, 50);
    }
  }
 }
 
 function getNextNodeInScriptDirection(a) {
  if (scriptDirection=="forward") {
   if (a.firstChild) {
    if (isItTagWeLookingFor1(a)) tag1WeLookingFor_count++;
    if (isItTagWeLookingFor2(a)) tag2WeLookingFor_count++;
    a=a.firstChild;
   } else {
    while (a!=fbw_body && a!=placeWhereToStop && a.nextSibling==null) {
     if (isItTagWeLookingFor1(a) && tag1WeLookingFor_count>0) tag1WeLookingFor_count--;
     if (isItTagWeLookingFor2(a) && tag2WeLookingFor_count>0) tag2WeLookingFor_count--;
     a=a.parentNode;
    } 
    if (a!=fbw_body && a!=placeWhereToStop) {
     if (isItTagWeLookingFor1(a) && tag1WeLookingFor_count>0) tag1WeLookingFor_count--;
     if (isItTagWeLookingFor2(a) && tag2WeLookingFor_count>0) tag2WeLookingFor_count--;
     a=a.nextSibling;
    } 
   }
  } else {
   if (a.lastChild) {
    if (isItTagWeLookingFor1(a)) tag1WeLookingFor_count++;
    if (isItTagWeLookingFor2(a)) tag2WeLookingFor_count++;
    a=a.lastChild;
   } else {
    while (a!=fbw_body && a!=placeWhereToStop && a.previousSibling==null) {
     if (isItTagWeLookingFor1(a) && tag1WeLookingFor_count>0) tag1WeLookingFor_count--;
     if (isItTagWeLookingFor2(a) && tag2WeLookingFor_count>0) tag2WeLookingFor_count--;
     a=a.parentNode;
    } 
    if (a!=fbw_body && a!=placeWhereToStop) {
     if (isItTagWeLookingFor1(a) && tag1WeLookingFor_count>0) tag1WeLookingFor_count--;
     if (isItTagWeLookingFor2(a) && tag2WeLookingFor_count>0) tag2WeLookingFor_count--;
     a=a.previousSibling;
    } 
   }
  }
  return a;
 }
 
 function nonEmptyTag(f) {
 
  function getNextNodeInScriptDirection2(t) {
   a=t;
   if (scriptDirection=="forward") {
    if (a.firstChild) a=a.firstChild;
    else {
     while (a!=f && a.nextSibling==null) a=a.parentNode;
     if (a!=f) a=a.nextSibling;
    }
   } else {
    if (a.lastChild) a=a.lastChild;
    else {
     while (a!=f&& a.previousSibling==null) a=a.parentNode;
     if (a!=f) a=a.previousSibling;
    }
   }
   return a;
  }
 
  if (!f.hasChildNodes()) return false;
  el3=getNextNodeInScriptDirection2(f);
  while (el3!=f && el3.nodeType!=3) el3=getNextNodeInScriptDirection2(el3);
  if (el3.nodeType==3) return true;
  else return false;
 }
 
 function mySearch() {
  //alert(fbw_body.outerHTML);
  //проверим, сколько раз находится маркер, от которого начинаем движение, в теге искомого типа
  tag1WeLookingFor_count=0;
  tag2WeLookingFor_count=0;
  el2=el;
  if (paragraphIndent) {
   while (el.parentNode.nodeName!="BODY" && el.parentNode.nodeName!="DIV") el=el.parentNode;
   if ((scriptDirection=="forward"?el.nextSibling:el.previousSibling)==null) {
    while (el!=fbw_body && (scriptDirection=="forward"?el.nextSibling:el.previousSibling)==null) el=el.parentNode;
    if (el!=fbw_body) el=(scriptDirection=="forward"?el.nextSibling:el.previousSibling);
   } else el=scriptDirection=="forward"?el.nextSibling:el.previousSibling;
   if (el==fbw_body)
    if (confirm("Достигнут"+(scriptDirection=="forward"?" конец":"о начало")+" документа.\nПродолжить поиск с "+(scriptDirection=="forward"?"начала":"конца")+" документа?")) el=fbw_body;
    else return;
  } else {
   while (el2!=fbw_body && el2.nodeName!="BODY") {
    if (isItTagWeLookingFor1(el2)) tag1WeLookingFor_count++;
    if (isItTagWeLookingFor2(el2)) tag2WeLookingFor_count++;
    el2=el2.parentNode;
   }
   //alert("tagWeLookingFor_count:"+tagWeLookingFor_count);
   //теперь пройдем курсив, на котором находимся, если надо
   while (!((el.nodeType==3 && (tag1WeLookingFor_count==0 || tag2WeLookingFor_count==0)) || (el.nodeName=="P" && window.external.inflateBlock(el)==true) || el.nodeName=="IMG")) {
    el=getNextNodeInScriptDirection(el);
    if (el==placeWhereToStop) {
     alert("Ничего не найдено, поиск завершен.");
     return;
    } 
    if (el==fbw_body)
     if (confirm("Достигнут"+(scriptDirection=="forward"?" конец":"о начало")+" документа.\nПродолжить поиск с "+(scriptDirection=="forward"?"начала":"конца")+" документа?")) el=fbw_body;
     else return;   
   }   
  } 
  //теперь найдем новый курсив
  while (true) {
   el=getNextNodeInScriptDirection(el);
   if (el==placeWhereToStop) {
    alert("Ничего не найдено, поиск завершен.");
    return;
   } 
   if (el==fbw_body)
    if (confirm("Достигнут"+(scriptDirection=="forward"?" конец":"о начало")+" документа.\nПродолжить поиск с "+(scriptDirection=="forward"?"начала":"конца")+" документа?")) el=fbw_body;
    else return;
   if ((isItTagWeLookingFor1(el) && tag2WeLookingFor_count>0) || (isItTagWeLookingFor2(el) && tag1WeLookingFor_count>0) && nonEmptyTag(el)) {
    el2=el;
    while (el2.hasChildNodes() && el2!=placeWhereToStop) el2=getNextNodeInScriptDirection(el2);
    if (el2==placeWhereToStop) {
     alert("Ничего не найдено, поиск завершен.");
     return;
    }     
    range1.moveToElementText(el);
    range1.collapse(scriptDirection=="forward"?true:false);
    if (scriptDirection=="forward")
     if (range1.parentElement!==el && range1.move("character",1)==1) range1.move("character",-1); else;
    else 
     if (range1.parentElement!==el && range1.move("character",-1)==1) range1.move("character",1);
    range1.select();
    scrollIfItNeeds();
    return "Found";
   }
  }
 }
 
 if (document.selection.type=="Control") {
  alert("Ошибка.\n\nВы используете тип выделения,\nкоторый не поддерживатся этим скриптом –\nвыделение картинок, а не текста.\nПожалуйста, используйте выделение текста.");
  return;
 }
 fbw_body=document.getElementById("fbw_body");
 randomNum=Math.floor((Math.random()*9)).toString()+Math.floor((Math.random()*9)).toString()+Math.floor((Math.random()*9)).toString()+Math.floor((Math.random()*9)).toString()+Math.floor((Math.random()*9)).toString()+Math.floor((Math.random()*9)).toString();
 beginMarkerId="sclexGoToQuotesBeginMarker"+randomNum;
 endMarkerId="sclexGoToQuotesEndMarker"+randomNum;
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
 el=(scriptDirection=="forward")?endMarkerEl:beginMarkerEl;
 placeWhereToStop=(scriptDirection=="forward")?beginMarkerEl:endMarkerEl;

 var scriptResult=mySearch();
 try {
//  var scriptResult=mySearch();
 }
 catch(e) {
  alert(versionStr+"\n\nПроизошла какая-то ошибка.");
 }

 beginMarkerEl.removeNode(true);
 endMarkerEl.removeNode(true);
 window.external.EndUndoUnit(document);
 if (scriptResult=="Found") return scriptResult;
 return "NotFound";
}
