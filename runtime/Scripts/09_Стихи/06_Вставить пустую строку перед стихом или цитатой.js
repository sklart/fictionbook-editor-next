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
  "summary": "Вставить пустую строку перед стихом или цитатой.",
  "description": "От parentElement текущего диапазона поднимается к стихотворению/цитате/эпиграфу и вставляет пустой абзац перед либо после контейнера через insertAdjacentElement. Не предназначен для массовой обработки всей книги.",
  "scope": "Элемент около курсора или выделенный графический объект.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
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
    "sha256": "c7899164d2f9e7afae9b6c1c44bb5169f579dac46f5334f63375e9bbe0e8332f",
    "reviewStatus": "generated"
  },
  "tags": [
    "Стихи"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
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
