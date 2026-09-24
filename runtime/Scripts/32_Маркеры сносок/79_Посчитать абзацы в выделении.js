// Скрипт "Посчитать абзацы в выделении" для редактора FBE
// version 1.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для подсчета любых абзацев в выделенном фрагменте fb2 документа.
// В статистике отображается общее количество абзацев в выделении, количество пустых строк,
// количество "нумерованных" абзацев (возможных маркеров текстов сносок) и количество абзацев без нумерации.
// "Нумерованными" считаются абзацы с любым числом в начале, числом в верхнем или нижнем индексе SUP/SUB,
// с точкой, скобкой, квадратными/фигурными скобками и тильдами:

// 34 Тут любой текст
// 34. Тут любой текст
// 34) Тут любой текст
// [34] Тут любой текст
// {34} Тут любой текст
// [~34~] Тут любой текст
// {~34~} Тут любой текст

// Подсчет "нумерованных" абзацев позволяет быстрее выполнить дальнейшую расстановку возможных сносок.
// Скрипт не вносит никаких изменений в fb2 документ.

// version 1.1, 20.07.2026
//======================================

function Run() {
    var scriptName = "Посчитать абзацы в выделении";
    var version = "1.1";

    // Неразрывный пробел из настроек FBE
    var nbspChar = String.fromCharCode(160);
    try {
        nbspChar = window.external.GetNBSP();
    } catch (e) {}

    // Список необычных пробелов
    var unusualSpaces = String.fromCharCode(160) +
        String.fromCharCode(8194) + String.fromCharCode(8195) + String.fromCharCode(8196) +
        String.fromCharCode(8197) + String.fromCharCode(8198) + String.fromCharCode(8239) +
        String.fromCharCode(8201) + String.fromCharCode(8202) + String.fromCharCode(9643) + nbspChar;

    function isSpace(ch) {
        if (ch == " " || ch == "\t" || ch == "\r" || ch == "\n") return true;
        for (var i = 0; i < unusualSpaces.length; i++) {
            if (ch == unusualSpaces.charAt(i)) return true;
        }
        return false;
    }

    function isDigit(ch) {
        return (ch >= "0" && ch <= "9");
    }

    // Проверка: является ли строка пустой (только пробелы)
    function isEmptyLine(text) {
        for (var i = 0; i < text.length; i++) {
            if (!isSpace(text.charAt(i))) return false;
        }
        return true;
    }

    // Проверка: начинается ли строка с числа (с учётом возможных маркеров)
    function startsWithNumber(text) {
        if (text.length == 0) return false;
        var i = 0;

        // Пропускаем начальные пробелы
        while (i < text.length && isSpace(text.charAt(i))) i++;
        if (i >= text.length) return false;

        var ch = text.charAt(i);

        // Проверка на квадратную скобку [N] или [~N~]
        if (ch == "[") {
            i++;
            if (i < text.length && text.charAt(i) == "~") i++; // [~
            var foundDigit = false;
            while (i < text.length && isDigit(text.charAt(i))) { foundDigit = true; i++; }
            if (!foundDigit) return false;
            if (i < text.length && text.charAt(i) == "~") i++; // ~
            if (i < text.length && text.charAt(i) == "]") return true; // ]
            return false;
        }

        // Проверка на фигурную скобку {N} или {~N~}
        if (ch == "{") {
            i++;
            if (i < text.length && text.charAt(i) == "~") i++; // {~
            var foundDigit = false;
            while (i < text.length && isDigit(text.charAt(i))) { foundDigit = true; i++; }
            if (!foundDigit) return false;
            if (i < text.length && text.charAt(i) == "~") i++; // ~
            if (i < text.length && text.charAt(i) == "}") return true; // }
            return false;
        }

        // Обычное число в начале (может быть с точкой или скобкой после)
        if (!isDigit(ch)) return false;

        // Собираем все цифры подряд
        while (i < text.length && isDigit(text.charAt(i))) i++;

        // Допустимые символы после числа: пробел, точка, скобка, или сразу буква
        if (i >= text.length) return true; // только число — считаем
        var nextCh = text.charAt(i);
        if (isSpace(nextCh) || nextCh == "." || nextCh == ")" || nextCh == ":" || 
            nextCh == "," || nextCh == ";" || nextCh == "!" || nextCh == "?") return true;

        // Если сразу буква — тоже считаем (число-заголовок)
        return true;
    }

    // ==================================================
    // ОСНОВНАЯ ЧАСТЬ
    // ==================================================

    var fbw_body = document.getElementById("fbw_body");
    if (!fbw_body) {
        MsgBox("Ошибка: не найден fbw_body", scriptName + " v" + version);
        return;
    }

    // Проверка выделения
    var hasSelection = false;
    var selectionRange = null;

    try {
        var sel = document.selection;
        if (sel && sel.type && sel.type == "Text") {
            var tr = sel.createRange();
            if (tr && tr.compareEndPoints("StartToEnd", tr) != 0) {
                var parentEl = tr.parentElement();
                if (parentEl && parentEl.nodeName != "TEXTAREA" && parentEl.nodeName != "INPUT") {
                    var checkEl = parentEl;
                    var insideFbwBody = false;
                    var depth = 0;
                    while (checkEl && depth < 100) {
                        depth++;
                        if (checkEl === fbw_body) { insideFbwBody = true; break; }
                        checkEl = checkEl.parentNode;
                    }
                    if (insideFbwBody) {
                        hasSelection = true;
                        selectionRange = tr.duplicate();
                    }
                }
            }
        }
    } catch (e) {
        hasSelection = false;
    }

    if (!hasSelection) {
        MsgBox("Нет выделения.\n\nПеред запуском скрипта выделите нужный фрагмент текста.", scriptName + " v" + version);
        return;
    }

    // Сбор абзацев в выделении
    var allPElements = fbw_body.getElementsByTagName("P");
    var collectedParagraphs = [];

    for (var ap = 0; ap < allPElements.length; ap++) {
        var pEl = allPElements[ap];
        try {
            if (!pEl || !fbw_body.contains(pEl)) continue;
            var pRange = document.body.createTextRange();
            pRange.moveToElementText(pEl);
            var comp1 = selectionRange.compareEndPoints("StartToEnd", pRange);
            var comp2 = selectionRange.compareEndPoints("EndToStart", pRange);
            if (comp1 < 0 && comp2 > 0) {
                collectedParagraphs.push(pEl);
            }
        } catch (e2) {}
    }

    var totalParagraphs = collectedParagraphs.length;

    if (totalParagraphs == 0) {
        MsgBox("В выделении не найдено абзацев.", scriptName + " v" + version);
        return;
    }

    // Подсчёт пустых строк и нумерованных абзацев
    var emptyCount = 0;
    var numberedCount = 0;

    for (var idx = 0; idx < collectedParagraphs.length; idx++) {
        var pElement = collectedParagraphs[idx];
        var text = pElement.innerText || pElement.textContent || "";

        if (isEmptyLine(text)) {
            emptyCount++;
        } else if (startsWithNumber(text)) {
            numberedCount++;
        }
    }

    // Ненумерованные = всего - пустые - нумерованные
    var unnumberedCount = totalParagraphs - emptyCount - numberedCount;

    // Статистика
    var msg = "";
    msg += scriptName + "\n";
    msg += "ver. " + version + "\n\n";

    msg += "✓ Всего выделено абзацев: " + totalParagraphs + "\n\n";
    msg += "Среди них:\n";
    msg += "  • Пустых строк: " + emptyCount + "\n";
    msg += "  • Нумерованных абзацев: " + numberedCount + "\n";
    msg += "  • Абзацев без нумерации: " + unnumberedCount + "\n";

    MsgBox(msg, "FBE скрипт");
}