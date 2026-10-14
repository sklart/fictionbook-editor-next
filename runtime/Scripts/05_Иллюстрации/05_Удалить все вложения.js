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
  "summary": "После подтверждения удаляет все бинарные вложения книги и обновляет отображение изображений.",
  "description": "После подтверждения заменяет содержимое binobj его служебными узлами без DIV вложений, обновляет разделители и список обложек. Временно очищает и восстанавливает src всех IMG для обновления отображения. Ссылки на изображения в тексте остаются и могут стать неработающими.",
  "scope": "Все бинарные вложения binobj и все IMG документа; выделение не используется.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Сохраните копию книги и подтвердите полное удаление вложений. Отказ завершает работу без изменений.",
    "После работы проверьте ссылки на иллюстрации и обложки: сами изображения удалены, а ссылки в тексте сохраняются."
  ],
  "warnings": [
    "Удаляются все бинарные вложения, включая используемые иллюстрации и обложки. Ссылки в книге остаются и могут стать неработающими.",
    "При ошибке до завершения операции отмены полный откат не проверен."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
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
    "description": "Удаление сгруппировано в одну операцию отмены. Отказ пользователя происходит до её начала; явных досрочных выходов внутри нет, но откат после ошибки отдельно не проверен."
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
