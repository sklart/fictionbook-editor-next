// Скрипт «Работа с секциями и очистка форматирования»
// Для FictionBook Editor Next
// Автор: MiFodiyHOMEStudio

var sectionsTool_versionNum = "0.5.26β";

function Run() {
    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) {
        alert("Ошибка: не найден контейнер fbw_body.");
        return;
    }

    if (window.external && window.external.IsXML && window.external.IsXML()) {
        alert("Скрипт работает только в режиме редактирования HTML.");
        return;
    }

    // Вычисляем точный путь к HTML-файлу
    var scriptPath = document.location.href;

    var dialogWidth = "490px";
    var dialogHeight = "250px";

    // Передаем параметры родительского окна
    var coll = new Object();
    coll["fbwBody"] = fbwBody;
    coll["mainDocument"] = document;
    coll["window"] = window;
    coll["versionNum"] = sectionsTool_versionNum;

    // showModelessDialog держит окно поверх основного окна FBE
    window.showModelessDialog(
        "HTML/Работа с секциями.html",
        coll,
        "dialogHeight: " + dialogHeight + "; dialogWidth: " + dialogWidth + "; center: Yes; help: No; resizable: No; status: No;"
    );
}