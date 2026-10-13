/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Удалить все пустые строки во всех body, кроме body примечаний и комментариев",
  "version": "1.1",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Удаляет все пустые строки во всех body, кроме body примечаний и комментариев.",
  "description": "Удаляет пустые абзацы в body, пропуская body примечаний и комментариев по fbname. Критерий пустоты учитывает пробелы/NBSP; аннотация и история управляются отдельными параметрами.",
  "scope": "Текущая книга или определённые кодом её разделы.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Проверьте перечисленные параметры: часть задаётся в исходном JS, часть — через общий диалог настроек.",
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
    "sha256": "b87cb69ba6e175c33f4e902a9801e5853c9844e2783031ccfd1eda53bd203d54",
    "reviewStatus": "generated"
  },
  "tags": [
    "Заголовки, подзаголовки, пустые строки",
    "сноски",
    "параметры"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
  },
  "settings": [
    {
      "name": "processHistory",
      "value": false,
      "expression": "false",
      "description": "флаг - обрабатывать ли history",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "processAnnotation",
      "value": false,
      "expression": "false",
      "description": "флаг - обрабатывать ли annotation",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    }
  ]
}
</FBE-SCRIPT-CARD>*/
 // Скрипт "Удалить все пустые строки во всех body, кроме body примечаний и комментариев"

function Run() {

 // номер версии скрипта
 var versionNumber="1.1";

 // флаг - обрабатывать ли history
 var processHistory=false;

 // флаг - обрабатывать ли annotation
 var processAnnotation=false;

 try { var nbspChar=window.external.GetNBSP(); var nbspEntity; if (nbspChar.charCodeAt(0)==160) nbspEntity="&nbsp;"; else nbspEntity=nbspChar;}
 catch(e) { var nbspChar=String.fromCharCode(160); var nbspEntity="&nbsp;";}

 var re0=new RegExp("^( | |&nbsp;|"+nbspChar+")*?$","");
 var re2=new RegExp("<(?!img)[^>]*?>", "gi");
 var re1=new RegExp("^(title|section|poem|text-author|cite|epigraph|stanza|body)$","");

 var Ts=new Date().getTime();
 var TimeStr=0;

 window.external.BeginUndoUnit(document,"удаление всех пустых строк в элементах body"); // ОТКАТ (UNDO) начало

 var id, nodeToRemove;
 var saveNode=null;
 var s="";
 var count=0;
 var fbw_body=document.getElementById("fbw_body");
 var ptr=fbw_body;
 var endProcessing=false;
 var saveNext;

 while (!endProcessing && ptr) {
  saveNext=ptr;
  if (saveNext.firstChild && saveNext.nodeName!="P" && 
      !(saveNext.nodeName=="DIV" && 
        (
         (saveNext.className=="history" && !processHistory)
         || 
         (saveNext.className=="annotation" && !processAnnotation)
         ||
         (saveNext.className=="body"
          &&
          (
           saveNext.getAttribute("fbname")=="notes"
           ||
           saveNext.getAttribute("fbname")=="comments"
          )
         )
         ||
         saveNext.className=="table"
        )
       )
     ) {
   saveNext=saveNext.firstChild;
   saveNode=null;
  }
  // либо углубляемся...
  else {
    saveNode=saveNext.parentNode;
    while (!saveNext.nextSibling)  {
     saveNext=saveNext.parentNode;
     // ...либо поднимаемся (если уже сходили вглубь)
     if (saveNext==fbw_body) endProcessing=true;
    }
   saveNext=saveNext.nextSibling; // и переходим на соседний элемент
  }
  if (ptr.nodeName=="P") {
   if (re0.test(ptr.innerHTML.replace(re2,""))) {
    ptr.removeNode(true);
    count++;
   }
  }
  while (saveNode && saveNode.nodeName=="DIV" && re1.test(saveNode.className) && !saveNode.firstChild) {
   nodeToRemove=saveNode;
   saveNode=saveNode.parentNode;
   nodeToRemove.removeNode(true);
  }
  ptr=saveNext;
 }

 // undo конец
 window.external.EndUndoUnit(document);

 var Tf=new Date().getTime();
 var Tmin = Math.floor((Tf-Ts)/60000);
 var Tsek = Math.ceil(10*((Tf-Ts)/1000-Tmin*60))/10;
 if (Tmin>0) {var TimeStr=Tmin+" мин. "+Tsek+" с"}
 else {var TimeStr=Tsek+" с"}

 try {
  if (count!=0)
   window.external.SetStatusBarText("Удалено пустых строк: " +count+ ". Время работы скрипта: "+TimeStr+". Версия скрипта: "+versionNumber+".");
  else
   window.external.SetStatusBarText("Пустых строк не найдено. Время работы скрипта: "+TimeStr+". Версия скрипта: "+versionNumber+".");
 }
 catch (e)
 {}

}
