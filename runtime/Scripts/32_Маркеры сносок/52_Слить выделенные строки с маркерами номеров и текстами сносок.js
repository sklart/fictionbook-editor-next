// Скрипт «Слить выделенные строки с маркерами номеров и текстами сносок»
// Версия 2.11 (real)
// В скрипте использованы фрагменты кода ув.Sclex-а «Удалить пустые строки в выделении» + помощь DeepSeek + stokber (сентябрь 2026). 

function Run() {
    var undoMsg = "Слить выделенные строки с маркерами номеров и текстами сносок";
    var statusBarMsg = "Объединяем номера и тексты сносок…";
    //имя тэга, который будет использован для маркеров начала и конца выделения
    var markerTagName = "I";

    var removesCnt = 0; // счетчик удаленных пустых строк.
    var mergedCnt = 0; // счетчик объединенных номеров
    var servicCnt = 0; // счетчик удалённых служебных строк.
    // Получение символа неразрывного пробела
    try {
        var nbspChar = window.external.GetNBSP();
        var nbspEntity;
        if (nbspChar.charCodeAt(0) == 160) nbspEntity = "&nbsp;";
        else nbspEntity = nbspChar;
    } catch (e) {
        var nbspChar = String.fromCharCode(160);
        var nbspEntity = "&nbsp;";
    }

    // ========== ПРОВЕРКА НАЛИЧИЯ ВЫДЕЛЕНИЯ ==========
    var errMsg = "Нет выделения.\n\nПеред запуском скрипта нужно выделить абзацы, которые будут обработаны.";
    if (document.selection.type == "None") {
        MsgBox(errMsg);
        return;
    }

    var tr = document.selection.createRange();
    if (tr.parentElement().nodeName == "TEXTAREA" || tr.parentElement().nodeName == "INPUT") {
        MsgBox("Ошибка. Должно быть выделение в тексте книги, а не в поле ввода.");
        return;
    }

    // ------------------------------------------------------------
    // НАСТРАИВАЕМЫЕ РЕГУЛЯРНЫЕ ВЫРАЖЕНИЯ (можно редактировать)
    // ------------------------------------------------------------
    // 1) Строки, которые должны быть полностью очищены (содержат "Вернуться" или "(обратно)")
    var delStr = new RegExp("^(?: | |&nbsp;|" + nbspChar + ")*?(Вернуться|\\\(?обратно\\\)?)(?: | |&nbsp;|" + nbspChar + ")*?$", "i");
    // 2) Строки, содержащие номер (цифры, возможно в [ ] или { })
    // var numStr = new RegExp("^(?: | |&nbsp;|" + nbspChar + ")*?((\\(|\\[|\\{)?\\d+(\\)|\\]|\\}|\\.)?)(?: | |&nbsp;|" + nbspChar + ")*?$", "i");
	
	var numStr = new RegExp("^(?: | |&nbsp;|" + nbspChar + ")*?((\\(|\\[|\\{)?\\d+(\\)|\\]|\\})?\.?)(?: | |&nbsp;|" + nbspChar + ")*?$", "i");

    // === Запрос скобок для номеров 2 ===
    var bracket = prompt("Введите новые откр. и закр. скобки через пробел (например: [ ], { }, ( ) или [~ ~])  или оставьте поле пустым для удаления скобок:", "[~ ~]");
    if (bracket == null) {
        return
    }
    var openBracket;
    var closeBracket;

    var parts = bracket.replace(/^\s+|\s+$/g, "").split(/\s+/); // скобки.
    var partsN = bracket.replace(/^\s+|\s+$/g, "");

    // разрешённая одинарная правая "скобка"...
    var rxBracket = partsN.match(/^[._)]$/);
    var rxNoBracket = partsN.match(/[^._)]/);

    // количество символов в скобках:
    var count = (partsN.match(/./g) || []).length;


    if (parts.length === 2) { // если 2 группы...
        openBracket = parts[0];
        closeBracket = parts[1];
    } else if ((parts.length == 1) && (partsN = rxBracket)) { // если символ одиночный...
        openBracket = "";
        closeBracket = rxBracket;

    } else if ((parts.length == 1) && (partsN = rxNoBracket) && (partsN != null) && (count == 1)) { // 
        alert("Неразрешённая одинарная скобка (Разрешены только точка '.', символ подчеркивания '_' или закрывающая круглая скобка ')'");
        return

    } else if ((parts.length == 1) && (count > 1)) { // 
        alert("Кажется, вы забыли оставить пробел между символами скобок!");
        return

    } else if ((parts.length == 1) && ((partsN == "") || (partsN == null))) { // без скобок...
        openBracket = "";
        closeBracket = "";

    } else if (parts.length > 2) { // 
        alert("В запросе может быть не больше одного пробела!");
        return

    } else {
        alert("Между открывающими и закрывающими символами скобок должен быть пробел!");
        return
    }
    // =================================

    // Регулярка для проверки пустой строки (учитывает пробелы и &nbsp;)
    var emptyLineRegExp = new RegExp("^( | |&nbsp;|" + nbspChar + ")*?$", "i");

    function isLineEmpty(ptr) {
        return emptyLineRegExp.test(ptr.innerHTML.replace(/<(?!img)[^>]*?>/gi, ""));
    }

    // Вспомогательные функции для обхода дерева (оставлены без изменений)
    function getNextNode(el) {
        if (el.firstChild && el.nodeName != "P")
            el = el.firstChild;
        else {
            while (el && !el.nextSibling)
                el = el.parentNode;
            if (el && el.nextSibling) el = el.nextSibling;
        }
        return el;
    }

    function getNextP(el) {
        var savedEl = el;
        while (el && (el.nodeName != "P" || el == savedEl))
            el = getNextNode(el);
        return el;
    }

    // Функция удаления всех пустых абзацев (теперь без проверки на наличие соседей)
    function processPs() {
        for (var i = ps.length - 1; i >= 0; i--)
            if (isLineEmpty(ps[i])) {
                ps[i].removeNode(true);
                removesCnt++;
            }
    }

    // ---- Сбор абзацев в выделении (без изменений) ----
    var s;
    var ps = [];
    var tr, el, prv, pm, saveNext, saveFirstEmpty, nextPtr;

    window.external.BeginUndoUnit(document, undoMsg);
    try {
        window.external.SetStatusBarText(statusBarMsg);
    } catch (e) {}
    var fbwBody = document.getElementById("fbw_body");

    var tr3 = document.selection.createRange();
    tr3.collapse(true);
    var blockStartEl = tr3.parentElement();
    tr3 = document.selection.createRange();
    tr3.collapse(false);
    var blockEndEl = tr3.parentElement();

    var ptr = blockStartEl;
    while (ptr && fbwBody.contains(ptr)) {
        ps.push(ptr);
        if (ptr === blockEndEl) break;
        ptr = getNextP(ptr);
    }

    // ========== НОВАЯ ОБРАБОТКА ==========
    // Шаг 1: Очистка строк, соответствующих delStr
    for (var i = 0; i < ps.length; i++) {
        var text = ps[i].innerHTML.replace(/<(?!img)[^>]*?>/gi, "");
        if (delStr.test(text)) {
            ps[i].innerHTML = "";
            servicCnt++ // счётчик.
        }
    }

    // Шаг 2: Обработка номеров и объединение с текстом
    for (var i = 0; i < ps.length; i++) {
        var text = ps[i].innerHTML.replace(/<(?!img)[^>]*?>/gi, "");
        var match = numStr.exec(text);
        if (match) {
            var numRaw = match[1]; // строка с цифрами и возможными скобками
            var num = numRaw.replace(/[^0-9]/g, ''); // оставляем только цифры
            // Поиск следующего непустого и не номерного абзаца
            var j = i + 1;
            while (j < ps.length) {
                var textJ = ps[j].innerHTML.replace(/<(?!img)[^>]*?>/gi, "");
                if (!isLineEmpty(ps[j]) && !numStr.test(textJ)) {
                    break;
                }
                j++;
            }
            if (j < ps.length) {
                // Вставляем номер в начало найденного абзаца
                var newContent = openBracket + num + closeBracket + " " + ps[j].innerHTML;
                ps[j].innerHTML = newContent;
                // Очищаем текущий абзац (бывший номер)
                ps[i].innerHTML = "";
                mergedCnt++; // счетчик.
            }
        }
    }

    // Шаг 3: Удаление всех пустых абзацев (включая те, что стали пустыми)

    processPs();

    removesCnt -= mergedCnt + servicCnt; // удалено в итоге пустых строк.
    try {
        window.external.SetStatusBarText("Объединено номеров: " + mergedCnt + ". Удалено пустых абзацев: " + removesCnt + ". Удалено служебных абзацев: " + servicCnt + ".");
    } catch (e) {}
    window.external.EndUndoUnit(document);
}
