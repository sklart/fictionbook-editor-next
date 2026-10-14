/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Вставить пустую строку перед стихом или цитатой",
  "version": "1.0",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Вставляет пустой абзац непосредственно перед стихом или цитатой, в котором находится курсор.",
  "description": "По началу текущего текстового диапазона находит ближайший контейнер DIV.poem или DIV.cite и вставляет перед ним пустой абзац. Если курсор вне этих контейнеров, ничего не меняет.",
  "scope": "Ближайший содержащий начало диапазона стих или цитата.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор внутри стиха или цитаты (либо выделите текст внутри нужного контейнера) и запустите скрипт.",
    "При выделении ориентиром служит его начало.",
    "Проверьте появившийся пустой абзац перед контейнером."
  ],
  "warnings": [
    "Скрипт не вставляет строку, если курсор находится вне поддерживаемого контейнера. При ошибке вставки операция отмены может остаться открытой; полнота отката не проверена."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "c7899164d2f9e7afae9b6c1c44bb5169f579dac46f5334f63375e9bbe0e8332f",
    "reviewStatus": "generated"
  },
  "tags": [
    "Стихи"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "На штатном пути вставка абзаца сгруппирована в одну операцию отмены редактора. Досрочные выходы происходят до открытия группы; при ошибке во время вставки её закрытие не гарантировано."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Вставить пустую строку перед стихом или цитатой"
// Версия 1.0
// Автор Sclex

function Run() {
 tr=document.selection.createRange();
 if (!tr) return;
 if (tr.parentElement().nodeName=="TEXTAREA") {
  MsgBox("Ошибка. Должно быть выделение в тексте книги, а не в поле ввода.");
  return;
 }
 tr.collapse(true);
 var el=tr.parentElement();
 while (el && el.nodeName!="BODY" &&
       !(el.nodeName=="DIV" &&
         (el.className.search(/^(poem|cite)$/i)>=0)
        )
       ) 
  el=el.parentNode;
 if (!el) return;
 if (el.nodeName=="BODY") return;
 var a={"POEM":"стихом", "CITE":"цитатой", "EPIGRAPH":"эпиграфом"};
 window.external.BeginUndoUnit(document,"вставку пустой строки перед "+a[el.className.toUpperCase()]);
 var em=document.createElement("P");
 InflateIt(el.insertAdjacentElement("beforeBegin",em));
 window.external.EndUndoUnit(document);
}
