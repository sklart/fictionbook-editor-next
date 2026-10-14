/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Удалить id разделов",
  "version": "1.2",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Удаляет непустые ID разделов в обрабатываемых разделах книги.",
  "description": "Удаляет непустые атрибуты id у DIV.section в обычных body книги. Разделы notes и comments пропускает; входящие ссылки заранее не проверяет.",
  "scope": "Все DIV.section в body, кроме notes и comments; выделение не учитывается.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте книгу и запустите скрипт для удаления ID разделов в указанной области.",
    "После выполнения проверьте число удалений в сообщении и ссылки на обработанные элементы."
  ],
  "warnings": [
    "Ссылки на разделы с удалёнными ID могут перестать работать; примечания и комментарии пропускаются.",
    "При ошибке во время обхода операция отмены может остаться открытой; полнота отката не проверена."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "a2bc033ed540af3b44b419d9daccc9e67e706cc1f5e5f9135b9b188c2814efc6",
    "reviewStatus": "generated"
  },
  "tags": [
    "Чистка"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Удаление ID на штатном пути группируется в одну операцию отмены редактора. При исключении до завершения группа может остаться открытой; полнота отката не проверена."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Удалить id разделов v1.2" 
// Автор Sclex

function Run() {
 var fbwBody=document.getElementById("fbw_body");
 var elems=fbwBody.getElementsByTagName("DIV");
 var firstRemoving=true;
 var elem, elem2, elems2, i, j;
 var replaceCounter=0;
 for (var i=0; i<elems.length; i++) {
  elem=elems[i];
  if (elem.className && elem.className.toLowerCase()=="body" && elem.getAttribute("fbname")!="notes" && elem.getAttribute("fbname")!="comments") {
   elems2=elem.getElementsByTagName("DIV");
   for (var j=0; j<elems2.length; j++) {
    elem2=elems2[j];
    if (elem2.className && elem2.className=="section" && elem2.getAttribute("id")!=null && elem2.getAttribute("id")!="") {
     if (firstRemoving) {
      window.external.BeginUndoUnit(document,"удаление id разделов");
      firstRemoving=false;
     }
     elem2.removeAttribute("id");
     replaceCounter+=1;
    }
   }
  }
 }
 if (!firstRemoving) window.external.EndUndoUnit(document);
 MsgBox("Всего удалено id разделов: "+replaceCounter);
}
