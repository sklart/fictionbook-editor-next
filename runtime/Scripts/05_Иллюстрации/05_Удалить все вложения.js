/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Удалить все вложения",
  "version": "1.4",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Удаляет бинарные вложения из описания книги.",
  "description": "После подтверждения перестраивает binobj, исключая контейнеры вложений, обновляет разделители и списки обложек. Сами ссылки на изображения могут потребовать отдельной проверки.",
  "scope": "Текущая книга или определённые кодом её разделы.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже.",
    "Просмотрите результат до сохранения. Для структурной правки проверьте книгу и связанные ссылки."
  ],
  "warnings": [
    "Скрипт содержит операции изменения документа. Перед массовой обработкой сохраните отдельную копию книги."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "dae072c905e2fc841842324a748de6e977bb5854483963955bd63b391e917768",
    "reviewStatus": "generated"
  },
  "tags": [
    "Иллюстрации"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Удалить все вложения"
// Версия 1.4

function Run() {
 if (!AskYesNo("Вы действительно хотите удалить все вложения из документа?")) return;
 window.external.BeginUndoUnit(document,"удаление всех вложений");
 var bins=document.all.binobj.getElementsByTagName("DIV");
 var ptr=document.all.binobj.firstChild;
 var GoMore=true;
 var HtmlStr="";
 while (GoMore) {
  if (ptr) {
   if (ptr.nodeName!="DIV") { 
    HtmlStr+=ptr.outerHTML; 
    ptr=ptr.nextSibling; 
   }
   else GoMore=false;
  } else GoMore=false;
 }
 document.all.binobj.innerHTML=HtmlStr;
 PutSpacers(document.all.binobj);
 var imgs=document.getElementsByTagName("IMG");
 for (var i=imgs.length-1; i>=0; i--) {
  var MyImg=imgs[i];
  var pic_id=MyImg.src; 
  MyImg.src=""; 
  MyImg.src=pic_id;
 }
 FillCoverList();
 window.external.EndUndoUnit(document);
 MsgBox("Работа скрипта завершена, все вложения удалены из документа.");
}
