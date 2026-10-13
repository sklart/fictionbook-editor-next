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
  "summary": "Удаляет id абзацев.",
  "description": "Обходит указанные типы элементов и удаляет их атрибуты id; комбинированный вариант также снимает style-оболочки. Сопоставление всех входящих ссылок перед удалением не следует считать гарантированным: проверьте структуру и ссылки после операции.",
  "scope": "Область определяется условиями и обходом DOM в данном скрипте.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже.",
    "Просмотрите результат до сохранения. Для структурной правки проверьте книгу и связанные ссылки."
  ],
  "warnings": [
    "Скрипт содержит операции изменения документа. Перед массовой обработкой сохраните отдельную копию книги.",
    "Удаление ID может разорвать ссылки на эти элементы. ID элемента image и ID binary — разные части структуры; не смешивайте их."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
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
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
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
