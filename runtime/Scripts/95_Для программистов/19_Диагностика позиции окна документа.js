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