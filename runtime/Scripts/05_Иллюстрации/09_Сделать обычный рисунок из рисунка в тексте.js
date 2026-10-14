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
  "summary": "Преобразует одну выделенную внутриабзацную картинку в блочную.",
  "description": "Требует выделения одного SPAN графическим выделением. Если абзац не содержит текстовых узлов и у SPAN первым дочерним узлом является IMG, вставляет после абзаца DIV.image с тем же изображением и href, затем удаляет исходный абзац. Остальные внутриабзацные изображения не обрабатывает.",
  "scope": "Один выделенный SPAN с IMG внутри абзаца P; при наличии текстового узла в этом абзаце преобразование не выполняется.",
  "modifiesDocument": true,
  "requiresSelection": true,
  "usage": [
    "Выделите одну внутриабзацную иллюстрацию графическим выделением и запустите скрипт.",
    "Если скрипт отказался работать, проверьте тип выделения и наличие другого текста в абзаце.",
    "После преобразования проверьте новый блочный рисунок и сохранность соседней разметки: исходный абзац удаляется."
  ],
  "warnings": [
    "Скрипт удаляет весь исходный абзац после вставки блочного рисунка. Если в нём есть элементы без текстовых узлов помимо рисунка, они могут быть потеряны."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
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
    "description": "Изменение сгруппировано в одну операцию отмены. Все явные отказы и проверки происходят до её начала; полный откат при ошибке не проверен."
  },
  "settings": []
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
