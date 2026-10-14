/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Удалить id абзацев",
  "version": "1.2",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Удаляет непустые ID абзацев в обрабатываемых разделах книги.",
  "description": "Удаляет непустые атрибуты id у абзацев во всех body книги, включая примечания и комментарии. Текст абзацев сохраняет, ссылки на удалённые ID заранее не проверяет.",
  "scope": "Все P внутри всех body, включая notes и comments; выделение не учитывается.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте книгу и запустите скрипт для удаления ID абзацев в указанной области.",
    "После выполнения проверьте число удалений в сообщении и ссылки на обработанные элементы."
  ],
  "warnings": [
    "Ссылки на абзацы с удалёнными ID могут перестать работать; скрипт не ищет входящие ссылки.",
    "При ошибке во время обхода операция отмены может остаться открытой; полнота отката не проверена."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "504ae4c2186077eeed8de585ae947b683ba503f6ee422c697b4ee426795e952d",
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
// Скрипт "Удалить id абзацев v1.2" 
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
   elems2=elem.getElementsByTagName("P");
   for (var j=0; j<elems2.length; j++) {
    elem2=elems2[j];
    if (elem2.getAttribute("id")!=null && elem2.getAttribute("id")!="") {
     if (firstRemoving) {
      window.external.BeginUndoUnit(document,"удаление id абзацев");
      firstRemoving=false;
     }
     elem2.removeAttribute("id");
     replaceCounter+=1;
    }
   }
  }
 }
 if (!firstRemoving) window.external.EndUndoUnit(document);
 MsgBox("Всего удалено id абзацев: "+replaceCounter);
}
