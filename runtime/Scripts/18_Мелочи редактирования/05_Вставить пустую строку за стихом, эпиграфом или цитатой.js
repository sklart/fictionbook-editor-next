/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Вставить пустую строку за стихом, эпиграфом или цитатой",
  "version": "1.0",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Вставляет пустой абзац непосредственно после стиха, цитаты или эпиграфа, в котором находится курсор.",
  "description": "По концу текущего текстового диапазона находит ближайший контейнер DIV.poem, DIV.cite или DIV.epigraph и вставляет после него пустой абзац. Если курсор вне этих контейнеров, ничего не меняет.",
  "scope": "Ближайший содержащий конец диапазона стих, цитата или эпиграф.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор внутри стиха, цитаты или эпиграфа (либо выделите текст внутри нужного контейнера) и запустите скрипт.",
    "При выделении ориентиром служит его конец.",
    "Проверьте появившийся пустой абзац после контейнера."
  ],
  "warnings": [
    "Скрипт не вставляет строку, если курсор находится вне поддерживаемого контейнера."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "cd24e28c3ad4c0fd7b6ee587724f912493c6b4eba1bedce72acec736d746216e",
    "reviewStatus": "generated"
  },
  "tags": [
    "Мелочи редактирования"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": ""
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Вставить пустую строку за стихом, эпиграфом или цитатой"
// Версия 1.0
// Автор Sclex

function Run() {
 tr=document.selection.createRange();
 if (!tr) return;
 if (tr.parentElement().nodeName=="TEXTAREA") {
  MsgBox("Ошибка. Должно быть выделение в тексте книги, а не в поле ввода.");
  return;
 }
 tr.collapse(false);
 var el=tr.parentElement();
 while (el && el.nodeName!="BODY" &&
       !(el.nodeName=="DIV" &&
         (el.className.search(/^(poem|cite|epigraph)$/i)>=0)
        )
       )
  el=el.parentNode;
 if (!el) return;
 if (el.nodeName=="BODY") return;
 var a={"POEM":"стихом", "CITE":"цитатой", "EPIGRAPH":"эпиграфом"};
 window.external.BeginUndoUnit(document,"вставку пустой строки за "+a[el.className.toUpperCase()]);
 var em=document.createElement("P");
 InflateIt(el.insertAdjacentElement("afterEnd",em));
 window.external.EndUndoUnit(document);
}
