/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Cбросить структуру до разделов первого уровня",
  "version": "1.1",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Убирает вложенность разделов в основных частях книги, формируя разделы первого уровня.",
  "description": "Рекурсивно проходит содержимое основных body, разворачивает вложенные section и собирает HTML с разделами первого уровня. Разделы примечаний и комментариев не обрабатывает. Восстановленный HTML записывает обратно в каждый обработанный body.",
  "scope": "Основные части fbw_body; body с fbname=notes или comments исключены. Положение курсора и выделение не ограничивают обработку.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте книгу и запустите скрипт для выравнивания вложенных разделов основных частей до первого уровня.",
    "Проверьте полученную структуру, заголовки и ссылки перед сохранением."
  ],
  "warnings": [
    "Иерархия вложенных разделов удаляется; проверьте структуру и связи книги после обработки.",
    "Если fbw_body отсутствует, скрипт выходит после открытия группы отмены, не закрывая её."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "51deddbb9aab3c04006baf5653cd4f0d5ef39d5c8f6aaa325b765279be150a3c",
    "reviewStatus": "generated"
  },
  "tags": [
    "Структура разделов"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Обычный проход обрамлён группой отмены. При отсутствии области книги скрипт выходит после открытия группы и не закрывает её; полный откат через редактор не проверен."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "сброс структуры до разделов первого уровня"
// Версия 1.1

function Run() {

 var newSect, htmlStr;
 
 function rec(el) {
   var sectionOpened=false;
   var sectionClosed=false;
   var saveNext;
   while (el) {
     saveNext=el.nextSibling; 
     if (el.nodeName=="DIV" && el.className=="body" && el.firstChild) {
       if (el.getAttribute("fbname")!="notes" && el.getAttribute("fbname")!="comments") {
         htmlStr="";
         rec(el.firstChild);
         //alert("Результат:\n\n"+htmlStr);
         el.innerHTML=htmlStr;
       }
     }
     else if (el.nodeName=="DIV" && el.className=="section" && el.firstChild) {
       if (sectionOpened && !sectionClosed) {
         htmlStr+="</DIV>";
         sectionOpened=false;
       }
       rec(el.firstChild);
     }
     else if (el.nodeName=="DIV" || el.nodeName=="P") {
       if (!sectionOpened) {
         htmlStr+="<DIV class=section>";
         sectionOpened=true;
         sectionClosed=false;
       }
       htmlStr+=el.outerHTML;
     }
     el=saveNext;   
   }
   if (sectionOpened) htmlStr+="</DIV>";
 }

 window.external.BeginUndoUnit(document,"сброс структуры до разделов первого уровня");
 var fbwBody=document.getElementById("fbw_body");
 if (!fbwBody) return;
 htmlStr="";
 if (fbwBody.firstChild) rec(fbwBody.firstChild);
 window.external.EndUndoUnit(document);
}
