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
  "summary": "Удаляет пустые абзацы в основных телах книги, исключая сноски и комментарии.",
  "description": "Обходит fbw_body и удаляет абзацы, которые после удаления HTML-тегов содержат только обычные или неразрывные пробелы. Тела сносок и комментариев всегда пропускает; аннотацию и историю по исходным настройкам тоже не обрабатывает. После удаления строк удаляет опустевшие контейнеры заголовков, секций, стихов и других перечисленных структурных элементов. Показывает число удалений в строке состояния.",
  "scope": "Все основные body в fbw_body, кроме notes/comments; annotation и history по умолчанию исключены. Выделение не используется.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте книгу и запустите команду без выделения.",
    "Проверьте не только удалённые строки, но и структуру разделов: опустевшие контейнеры могут быть удалены."
  ],
  "warnings": [
    "Скрипт удаляет пустые абзацы и вслед за ними некоторые пустые структурные элементы; перед массовой обработкой сохраните копию книги.",
    "Обход DOM не останавливает подъём после достижения fbw_body; если у его предков нет следующего соседнего элемента, возможна ошибка и незавершённая группа отмены."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
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
    "description": "Удаления объединяются для отмены при штатном завершении. При ошибке обхода завершение группы не гарантировано; проверьте структуру книги до сохранения."
  },
  "settings": [
    {
      "name": "processHistory",
      "value": false,
      "expression": "false",
      "description": "Обрабатывать историю изменений книги: false — нет, true — да.",
      "valueMeaning": "Исходное значение в коде скрипта."
    },
    {
      "name": "processAnnotation",
      "value": false,
      "expression": "false",
      "description": "Обрабатывать аннотацию книги: false — нет, true — да.",
      "valueMeaning": "Исходное значение в коде скрипта."
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
