/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Раздел под курсором - обрамить другим (новым) разделом",
  "version": "1.0",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Обрамляет раздел под курсором новым вложенным разделом, оставляя начальные заголовок, эпиграф, аннотацию и иллюстрации на прежнем уровне.",
  "description": "Находит раздел, содержащий курсор. Начальные блоки title, epigraph, annotation и image оставляет перед новым вложенным разделом; остальные узлы переносит внутрь него. При отсутствии раздела сообщает об ошибке. Формирует HTML строкой и заменяет содержимое исходного раздела.",
  "scope": "Один охватывающий курсор раздел; начальные служебные блоки и остальное содержимое этого раздела перестраиваются. Выделение как диапазон не обрабатывается.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор внутри раздела, который нужно обрамить, и запустите скрипт.",
    "Проверьте полученную вложенность, заголовок, эпиграф, аннотацию, иллюстрации и ссылки до сохранения книги."
  ],
  "warnings": [
    "Скрипт заменяет HTML раздела; проверьте структуру и ссылки после запуска.",
    "В строке вставляемого HTML отсутствует закрывающий символ > у последнего тега DIV; результат зависит от разбора HTML редактором.",
    "Проверка поддержки DOM Range показывает сообщение при положительном результате, но не останавливает выполнение."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "37e3a94f12148ea70c54c3268c2d8d2914dac043d14c27ae7375c35d4358ad52",
    "reviewStatus": "generated"
  },
  "tags": [
    "Структура разделов",
    "параметры"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": ""
  },
  "settings": []
}
</FBE-SCRIPT-CARD>*/
// СКрипт "Раздел под курсором - обрамить другим (новым) разделом
// Автор  Sclex
// версия 1.0

function Run() {
 var supportsDOMRanges = document.implementation.hasFeature("Range", "2.0");
 if (supportsDOMRanges) {MsgBox("Ошибка! Нет поддержки DOM Ranges!");}
 var MyTagName="B";
 var body=document.getElementById("fbw_body");
// var coll=body.document.selection.createRange().getClientRects();
 var coll=body.document.selection.createRange()
 var ttr1 = body.document.selection.createRange();
// var el=body.document.elementFromPoint(coll[0].left, coll[0].top);
 var tr=ttr1.duplicate();
 tr.collapse();
 tr.pasteHTML("<"+MyTagName+" id=SectionDownCursorPosition></"+MyTagName+">");
 var ptr=body;
 var AfterCursorPosition=false;
 var NeedExit=false;
 var ImageFound=false;
 while (ptr&&!NeedExit) {
  if (ptr.nodeType==1 && ptr.nodeName==MyTagName &&
      ptr.getAttribute("id")=="SectionDownCursorPosition") {
   AfterCursorPosition=true;
   var CursorPosition=ptr;
   NeedExit=true;
  }
  if (!NeedExit) {
   if (ptr.firstChild!=null) {
    ptr=ptr.firstChild;
   } else {
    while (ptr&&ptr.nextSibling==null) {
     ptr=ptr.parentNode;
     if (ptr && ptr.nodeType==1 && ptr.nodeName=="P") {InsideP=false}
    }
    if (ptr) {ptr=ptr.nextSibling}
   }
  }
 }
 var while_flag=true;
 while (while_flag) {
  if (ptr) {
   if (ptr.nodeName!="DIV" || ptr.className!="section") {ptr=ptr.parentNode;}
   else while_flag=false;
  } else {while_flag=false;}
 }
 CursorPosition.parentNode.removeChild(CursorPosition);
 if (ptr==null) {
  MsgBox("Ошибка. Курсор не внутри раздела.");
  return;
 }
 var ptr2=ptr.firstChild;
 var before_text_contents=true;
 //в myhtml1 будем собирать содержимое раздела до начала текстово-секционного содержимого
 //т.е. тэги title, epigraph, annotation, image
 var myhtml1="";
 //а в myhtml2 будем собирать текстово-секционное содержимое
 var myhtml2="";
 while (ptr2) {
  if (ptr2.nodeName=="DIV" && (ptr2.className=="image" || ptr2.className=="epigraph" ||
                               ptr2.className=="title" || ptr2.className=="annotation" )){
  }
  else {
   before_text_contents=false;
  }
  if (before_text_contents) {myhtml1=myhtml1+ptr2.outerHTML;}
  else {
   if (ptr2.innerHTML!="") {myhtml2=myhtml2+ptr2.outerHTML;}
   else {myhtml2=myhtml2+"<P> </P>";}
  }
  ptr2=ptr2.nextSibling;
 }
 window.external.beginUndoUnit(document,"section down");
 ptr.innerHTML=myhtml1+"<DIV class=section>"+myhtml2+"</DIV";
 window.external.endUndoUnit(document);
 MsgBox("Раздел под курсором был успешно обрамлен другим (новым) разделом.");
}
