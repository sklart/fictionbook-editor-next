/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Диагностика позиции окна документа",
  "version": "1.2",
  "authors": [
    {
      "name": "TaKir",
      "role": "author"
    },
    {
      "name": "DeepSeek",
      "role": "ai_assistance"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Скрипт предназначен для определения реального положения и размеров окна открытого fb2 документа.",
  "description": "Скрипт предназначен для определения реального положения и размеров окна открытого fb2 документа.",
  "scope": "Текущая книга или определённые кодом её разделы.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже."
  ],
  "warnings": [],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "0432fc4d86adbcf9dbf3236df7a1d3b72625d57ec46cc84b610ac023df4f583a",
    "reviewStatus": "generated"
  },
  "tags": [
    "Для программистов"
  ],
  "undo": {
    "status": "no_explicit_grouping",
    "description": "Явная группировка Undo в этом файле не найдена. Это не означает ни гарантированное отсутствие Undo, ни безопасное выполнение."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Диагностика позиции и размеров окна документа" для редактора FBE
// version 1.2
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для определения реального положения и размеров окна открытого fb2 документа.
// Скрипт не вносит никаких изменений в документ.

// version 1.2, 20.09.2026
//======================================

function Run() {
    var s = "";

    s += "=== ОКНО ===\n";
    s += "screenLeft  = " + window.screenLeft + "\n";
    s += "screenTop   = " + window.screenTop + "\n";
    s += "innerHeight = " + window.innerHeight + "\n\n";

    s += "=== DOCUMENT.BODY ===\n";
    s += "clientHeight = " + document.body.clientHeight + "\n";
    s += "offsetHeight = " + document.body.offsetHeight + "\n";
    s += "scrollHeight = " + document.body.scrollHeight + "\n\n";

    s += "=== DOCUMENT.DOCUMENTELEMENT ===\n";
    s += "clientHeight = " + document.documentElement.clientHeight + "\n";
    s += "offsetHeight = " + document.documentElement.offsetHeight + "\n";
    s += "scrollHeight = " + document.documentElement.scrollHeight + "\n\n";

    var fbw = document.getElementById("fbw_body");
    if (fbw) {
        s += "=== FBW_BODY ===\n";
        s += "clientHeight = " + fbw.clientHeight + "\n";
        s += "offsetHeight = " + fbw.offsetHeight + "\n";
        s += "scrollHeight = " + fbw.scrollHeight + "\n\n";
    }

    s += "=== SCREEN ===\n";
    s += "availHeight = " + screen.availHeight + "\n";
    s += "height      = " + screen.height + "\n";

    MsgBox(
        "Диагностика позиции и размеров окна документа\n" +
        "ver. 1.2\n" +
        "---------------------------------------\n\n" +
        s
    );
}