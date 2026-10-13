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
  "summary": "Cбросить структуру до разделов первого уровня.",
  "description": "Рекурсивно уплощает вложенную структуру до секций первого уровня, собирая их содержимое. Сохраняемый текст и сохранённая иерархия — не одно и то же: часть границ разделов исчезает.",
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
    "sha256": "51deddbb9aab3c04006baf5653cd4f0d5ef39d5c8f6aaa325b765279be150a3c",
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
