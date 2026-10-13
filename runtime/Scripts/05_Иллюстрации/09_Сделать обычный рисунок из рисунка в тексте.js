/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Сделать обычный рисунок из рисунка в тексте",
  "version": "1.1",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Преобразует: обычный рисунок из рисунка в тексте.",
  "description": "Для выделенной inline-картинки извлекает href и IMG, создаёт блочный DIV.image рядом с абзацем и удаляет прежнее представление. Требует Control-выделение картинки; не конвертирует все inline-изображения за один запуск.",
  "scope": "Элемент около курсора или выделенный графический объект.",
  "modifiesDocument": true,
  "requiresSelection": true,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Подготовьте выделение, описанное в scope и условиях запуска; некоторые команды работают с целыми абзацами.",
    "Выделите требуемый графический объект в режиме Control-выделения.",
    "Проверьте перечисленные параметры: часть задаётся в исходном JS, часть — через общий диалог настроек.",
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
    "sha256": "863bd29f09b727897ce73585bead44a630f94180419ce0c803008815b094731f",
    "reviewStatus": "generated"
  },
  "tags": [
    "Иллюстрации",
    "параметры"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
  },
  "settings": [
    {
      "name": "strstr",
      "value": "<DIV class=image contentEditable=false onresizestart=\"return false\" href=\"#undefined\">",
      "expression": "'<DIV class=image contentEditable=false onresizestart=\"return false\" href=\"#undefined\">'",
      "description": "alert(span.outerHTML);",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    }
  ]
}
</FBE-SCRIPT-CARD>*/
//Скрипт «Сделать обычный рисунок из рисунка в тексте»
//Версия 1.1

function Run() {
 var sel=document.selection;
 if (sel.type!="Control") {
  alert("Картинка не выделена, скрипту нечего делать.");
  return;
 }
 if (sel.createRange().length==1) {
  var span=sel.createRange().item(0);
  if (span.nodeName!="SPAN") {
   alert("Выделена не инлайн-картинка.");
   return;
  }
  //alert(span.outerHTML);
  var strstr='<DIV class=image contentEditable=false onresizestart="return false" href="#undefined">';
  var p=span;
  while (p.nodeName!="P" && p.nodeName!="BODY") p=p.parentNode;
  if (p.nodeName=="P") {
   //alert("p: "+p.outerHTML);
   var el=p.firstChild;
   while (el!=p) {
    if (el.nodeType==3) return;
    if (el.firstChild) 
     el=el.firstChild;
    else {
     while (el!=p && el.nextSibling==null) el=el.parentNode;
     if (el!=p) el=el.nextSibling;
    } 
   }
   if (span.firstChild && span.firstChild.nodeName=="IMG") {
    window.external.BeginUndoUnit(document,"превращение рисунка в тексте в обычный рисунок");
    var myHref=span.getAttribute("href");
    p.insertAdjacentHTML("afterEnd",strstr+span.firstChild.outerHTML+"</DIV>");
    var div=p.nextSibling;
    div.setAttribute("href",myHref);
    p.removeNode(true);
    window.external.EndUndoUnit(document);
   } 
  } 
 }
}
