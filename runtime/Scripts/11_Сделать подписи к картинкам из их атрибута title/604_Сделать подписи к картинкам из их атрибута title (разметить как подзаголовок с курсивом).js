/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Сделать подписи к картинкам из их атрибута title (разметить как подзаголовок с курсивом)",
  "version": "1.1",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Пытается перенести title блочных иллюстраций в подписи как подзаголовок с курсивом.",
  "description": "Обходит DIV.image с первым дочерним IMG внутри книги. Для непустого title создаёт сразу после изображения абзац подзаголовка с курсивным текстом с текстом title, затем удаляет исходный title. В некоторых случаях пытается добавить пустой абзац перед следующим содержимым. Показывает число найденных изображений и перенесённых заголовков.",
  "scope": "Все DIV.image с первым дочерним IMG внутри fbw_body, включая notes и comments; только элементы с непустым title получают подпись.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте книгу с блочными иллюстрациями, у которых заполнен атрибут title, и запустите нужный вариант оформления подписи.",
    "После выполнения сравните число найденных картинок с числом созданных подписей и проверьте абзацы около изображений.",
    "Если скрипт остановился с ошибкой, проверьте частичные изменения и состояние отмены редактора перед повторным запуском."
  ],
  "warnings": [
    "Когда после изображения есть непустой абзац, ветка вставки дополнительной пустой строки содержит ошибочное присваивание результату вызова. Обработка может прерваться до создания подписи.",
    "Операция отмены открывается до проверки наличия книги; при отсутствии корня или ошибке обхода она может остаться открытой. Полнота отката не проверена.",
    "Текст title вставляется как HTML: если в нём есть разметка, результат может отличаться от обычного текста."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "c597449000a1214a29923f15eeeeb1fc8530497faa9183f599f49418c6efe2c1",
    "reviewStatus": "generated"
  },
  "tags": [
    "Сделать подписи к картинкам из их атрибута title",
    "изображения"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Группа отмены открывается до обхода изображений и закрывается только после итогового сообщения. При досрочном выходе из-за отсутствия книги или при ошибке её закрытие не гарантировано; полный откат не проверен."
  }
}
</FBE-SCRIPT-CARD>*/
//author: Sclex
//version: 1.1
//Sclex, не забывай изменять номер версии в переменной versionStr

function Run() {
 var versionStr="1.1";
 var inscriptionClass="subtitle";
 var openingTag="<EM>";
 var closingTag="</EM>";
 window.external.BeginUndoUnit(document,"заголовки картинок в подписи"); 
 var cntImages=0,cntChanges=0; 
 var fbw_body=document.getElementById("fbw_body");
 if (!fbw_body) return;
 var el=fbw_body.firstChild;
 var saveNext,newEl1,newEl2,newEl3,inserted,realNext;
 while (el!=fbw_body) {
  //if (el.nodeType==1) alert(el.outerHTML);
  if (el.nodeName=="DIV" && el.className=="image" && el.firstChild!=null)
   if (el.firstChild.nodeName=="IMG") {
    cntImages++;
    if (el.title!="" && el.title!=null) {
     realNext=el.nextSibling;
     saveNext=el;
     if (saveNext.firstChild!=null && saveNext.nodeName!="P") {
      saveNext=saveNext.firstChild;
     } else {
      while (saveNext!=fbw_body && saveNext.nextSibling==null)
      saveNext=saveNext.parentNode;
      if (saveNext!=fbw_body) saveNext=saveNext.nextSibling;
     }
     if (realNext)
      if (realNext.nodeType!=1 || realNext.nodeName!="P" || realNext.innerHTML!="")
       {
        newEl1=document.createElement("P");
        newEl1.innerHTML="";
        inserted=el.parentNode.insertBefore(newEl1,el.nextSibling);
        window.external.inflateBlock(inserted)=true;
       }
     newEl2=document.createElement("P");
     if (inscriptionClass!="") newEl2.className=inscriptionClass;
     newEl2.innerHTML=openingTag+el.title+closingTag;
     el.parentNode.insertBefore(newEl2,el.nextSibling);
     //newEl3=document.createElement("P");
     //newEl3.innerHTML="&nbsp;";
     //newEl3.baseName="empty-line";
     //inserted=el.parentNode.insertBefore(newEl3,el.nextSibling);
     //window.external.inflateBlock(inserted)=true;
     el.removeAttribute("title");
     cntChanges++;
     el=saveNext;
    }
   } 
  if (el.firstChild!=null && el.nodeName!="P") {
   el=el.firstChild;
  } else {
   while (el!=fbw_body && el.nextSibling==null)
    el=el.parentNode;
   if (el!=fbw_body) el=el.nextSibling;
  }
 }
 MsgBox("--== Скрипт Sclex’а ==--\n"+
        "Заголовки картинок в подписи "+versionStr+"\n"+
        "\n"+
        "Встретилось картинок: "+cntImages+"\n"+
        "Заменено заголовков картинки на подпись: "+cntChanges);
 window.external.EndUndoUnit(document);
}
