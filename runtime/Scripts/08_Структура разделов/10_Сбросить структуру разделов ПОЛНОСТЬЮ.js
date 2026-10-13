/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Сбросить структуру разделов ПОЛНОСТЬЮ",
  "version": "1.4",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Сбросить структуру разделов ПОЛНОСТЬЮ.",
  "description": "Снимает секционную иерархию посредством рекурсивного переноса внутреннего HTML и удаления прежних section-контейнеров. Восстанавливает рабочее представление и позицию; исходная структура глав не сохраняется.",
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
    "sha256": "486eaf9ff97d3a6a44d59a17c9240cc4d75192544c9e53d3be9b758ad5272bb6",
    "reviewStatus": "generated"
  },
  "tags": [
    "Структура разделов"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Выполнить ПОЛНЫЙ сброс структуры разделов"
// version 1.4
// Автор Sclex

function Run() {

 var newSect, htmlStr, firstSection, wasClosingSection;

 function rec(el) {
  var saveNext,saveNext2,el2;
  while (el) {
   saveNext=el.nextSibling;
   if (el.nodeName=="DIV" && el.className=="image") {
     htmlStr+=el.outerHTML;
   }
   else if (el.nodeName=="DIV" && el.className!="section") {
     if ((el.className!="title" && el.className!="epigraph" && el.className!="annotation")
          ||
          ((el.className=="epigraph" || el.className=="title" || el.className=="annotation") && el.parentNode.className=="body")) {
       htmlStr+="<DIV class="+el.className+(el.getAttribute("id")?" id='"+el.getAttribute("id")+"'":"")+">";
       rec(el.firstChild);
       htmlStr+="</DIV>";
     }
    else rec(el.firstChild);
   }
   if (el.nodeName=="DIV" && el.className=="body" && el.firstChild) {
     if (el.getAttribute("fbname")!="notes" && el.getAttribute("fbname")!="comments") {
       htmlStr="<DIV class=body"+(el.getAttribute("id")?" id='"+el.getAttribute("id")+"'":"")+">";
       firstSection=true;
       wasClosingSection=false;
       rec(el.firstChild);
       if (wasClosingSection) htmlStr+="</DIV>";
       htmlStr+="</DIV>";
       //alert(htmlStr);
       el.innerHTML="<P>1</P>";
       newSect=el.insertAdjacentHTML("beforeBegin",htmlStr);
       InflateIt(newSect);
       el.innerHTML="";
       el.removeNode(true);
     }
   }
   if (el.nodeName=="DIV" && el.className=="section") {
     if (firstSection) {
       htmlStr+="<DIV class=section"+(el.getAttribute("id")?" id='"+el.getAttribute("id")+"'":"")+">";
       firstSection=false;
       wasClosingSection=true;
     }
     rec(el.firstChild);
   }
   if (el.nodeName=="P") {
     if (el.className=="text-author" && el.parentNode.className=="epigraph")
       htmlStr+="<P"+(el.getAttribute("id")?" id='"+el.getAttribute("id")+"'":"")+">";
     else
       htmlStr+="<P"+(el.getAttribute("id")?" id='"+el.getAttribute("id")+"'":"")+(el.className?" class='"+el.className+"'":"")+">";
     if (el.innerHTML=="")
       htmlStr+="&nbsp;";
     else  
       htmlStr+=el.innerHTML;
     htmlStr+="</P>";
   }
   el=saveNext;
  }
 }

 window.external.BeginUndoUnit(document,"полный сброс структуры разделов");
 var fbwBody=document.getElementById("fbw_body")
 if (!fbwBody) return;
 
 var tr=document.body.createTextRange();
 tr.moveToElementText(fbwBody);
 tr.collapse(true);
 tr.select();
 window.scrollTo(0,0);

 if (fbwBody.firstChild)
   rec(fbwBody.firstChild);
 window.external.EndUndoUnit(document);
 try {
   window.external.SetStatusBarText("Полный сброс структуры разделов успешно завершен.");
 }
 catch(e) 
 {}
}
