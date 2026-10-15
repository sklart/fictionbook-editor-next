/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Удалить id картинок",
  "version": "1.2",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Удаляет непустые ID блочных иллюстраций в обрабатываемых разделах книги.",
  "description": "Удаляет непустые атрибуты id у блочных DIV.image во всех body книги. Внутриабзацные SPAN.image и имена бинарных вложений не меняет.",
  "scope": "Все DIV.image внутри всех body, включая notes и comments; SPAN.image не обрабатываются.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте книгу и запустите скрипт для удаления ID блочных иллюстраций в указанной области.",
    "После выполнения проверьте число удалений в сообщении и ссылки на обработанные элементы."
  ],
  "warnings": [
    "Удаляются ID оболочек блочных изображений, а не имена бинарных вложений; ссылки на удалённые ID оболочек могут перестать работать."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "769ff8dc89d7235651d6b7a721c3fefe174500036c21c6b60de87cfc88a5dcdb",
    "reviewStatus": "generated"
  },
  "tags": [
    "Чистка",
    "изображения"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": ""
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Удалить id картинок v1.2" 
// Автор Sclex

function Run() {
 var fbwBody=document.getElementById("fbw_body");
 var elems=fbwBody.getElementsByTagName("DIV");
 var firstRemoving=true;
 var elem, elem2, elems2, i, j;
 var replaceCounter=0;
 for (var i=0; i<elems.length; i++) {
  elem=elems[i];
  if (elem.className && elem.className.toLowerCase()=="body") {
   elems2=elem.getElementsByTagName("DIV");
   for (var j=0; j<elems2.length; j++) {
    elem2=elems2[j];
    if (elem2.className && elem2.className=="image" && elem2.getAttribute("id")!=null && elem2.getAttribute("id")!="") {
     if (firstRemoving) {
      window.external.BeginUndoUnit(document,"удаление id картинок");
      firstRemoving=false;
     }
     elem2.removeAttribute("id");
     replaceCounter+=1;
    }
   }
  }
 }
 if (!firstRemoving) window.external.EndUndoUnit(document);
 MsgBox("Всего удалено id картинок: "+replaceCounter);
}
