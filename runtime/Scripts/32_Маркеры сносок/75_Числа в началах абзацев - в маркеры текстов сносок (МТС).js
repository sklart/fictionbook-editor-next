// Скрипт "Числа в началах абзацев - в маркеры текстов сносок (МТС)" для редактора FBE
// version 2.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для оформления чисел (1-9999) в началах абзацев как маркеров текстов сносок в fb2 документах.
// Поддерживается 6 вариантов оформления: SUP, звёздочки, решётки, скобки и скобки с тильдами.
// Исключаются из обработки "голые" цифры, дроби, интервалы, мат. выражения и заголовки.
// При выборе в качестве маркеров звёздочек или решёток - скрипт предварительно проверяет документ
// на их наличие и может заменить на альтернативный маркер во избежание путаницы в маркерах.
// Гибкие настройки: можно включать/исключать блочные элементы, subtitle, точки, скобки после цифр,
// а также используется пополняемый словарик стоп-слов исключений.
// Скрипт выводит подробную статистику с разбивкой по причинам исключений.
// При выделении обрабатывается только текст внутри выделения.
// При отсутствии выделения скрипт обрабатывает весь документ.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// ВАЖНО: для быстрой работы в режиме выделения рекомендуется разбивать документ на секции (главы).
// В одной огромной секции без подсекций возможны замедления из-за перестройки DOM в IE6.
// В режиме без выделения такой проблемы нет.

// version 2.1, 12.07.2026
//======================================

function Run() {
    var scriptName = "Числа в началах абзацев - в маркеры текстов сносок (МТС)";
    var version = "2.1";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1;

    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0; // 0 - нет, 1 - да

    // Обрабатывать раздел комментариев
    var processCommentsSection = 0; // 0 - нет, 1 - да

    // Обрабатывать абзацы внутри DIV элементов (блочные элементы)
    var processDivContainers = 0; // 0 - нет, 1 - да

    // Обрабатывать абзацы внутри class=subtitle или class=text-author
    var processSubtitlesTextAuthor = 0; // 0 - нет, 1 - да

    // Выбор оформления: найденные цифры-числа в началах абзацев:
    // 0 - Сформатировать надстрочным текстом - верхним индексом (SUP)
    // 1 - Заменить на знак звездочки * (одна звездочка, без пробела)
    // 2 - Заменить на знак решетки # (одна решетка, без пробела)
    // 3 - Обрамить квадратными скобками [34]
    // 4 - Обрамить фигурными скобками {34}
    // 5 - Обрамить квадратными скобками с тильдами [~34~]
    // 6 - Обрамить фигурными скобками с тильдами {~34~}
    var formatMode = 3;

    // Использовать другие маркеры, если звёздочки или решётки уже есть в документе:
    // 0 - нет, только предупредить и завершить
    // 1 - да, использовать альтернативный маркер
    var useAlternativeMarker = 1;

    // Альтернативный маркер взамен звёздочек или решёток (если useAlternativeMarker = 1):
    // 3 - квадратные скобки [N]
    // 4 - фигурные скобки {N}
    // 5 - квадратные скобки с тильдами [~N~]
    // 6 - фигурные скобки с тильдами {~N~}
    var alternativeMarker = 6;

    // Обрабатывать абзацы с числами, начинающимися с нуля (01, 001, 0005 и т.д.)
    var processLeadingZeros = 1; // 0 - нет, 1 - да

    // Обрабатывать начала строк с точкой после цифры (числа), кроме исключений
    var processWithDot = 1; // 0 - нет, 1 - да

    // Обрабатывать абзацы с круглой скобкой после цифры (числа), кроме исключений
    var processWithBracket = 1; // 0 - нет, 1 - да

    // Обрабатывать абзацы, где после цифр идет любой дефис (с пробелами или без)
    var processWithDefis = 0; // 0 - нет, 1 - да

    // Обрабатывать абзацы, где после цифр идет любое тире (с пробелами или без)
    var processWithDash = 0; // 0 - нет, 1 - да

    // Обрабатывать абзацы, где после цифр идет месяц, или указание времени
    var processWithDateTime = 0; // 0 - нет, 1 - да

    // Обрабатывать абзацы, где после цифр идут типичные слова для заголовков (в любом регистре)
    var processWithHeadingWords = 0; // 0 - нет, 1 - да

    // Минимальная длина текста после числа (чтобы не срабатывать на голых цифрах)
    var minTextLengthAfterNumber = 2;

    // ==================================================
    // СЛОВАРИКИ исключений (можно пополнять через запятую)
    // ==================================================

    // Слова-месяцы и единицы времени (в любом регистре)
    var dateTimeWords = "мая,декабря,января,февраля,марта,апреля,июня,июля,августа,сентября,октября,ноября," +
        "год,года,лет,неделя,недели,недель,час,часа,часов,минут,минуты,секунд,секунды,г,гг,мин,сек";

    // Слова-заголовки (в любом регистре)
    var headingWords = "глава,часть,раздел,приложение";

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    var nbspEntity = "&nbsp;";
    try {
        var nbspChar = window.external.GetNBSP();
        if (nbspChar.charCodeAt(0) != 160) {
            nbspEntity = nbspChar;
        }
    } catch (e) {
        var nbspChar = String.fromCharCode(160);
    }

    var unusualSpaces = String.fromCharCode(160) +
        String.fromCharCode(8194) +
        String.fromCharCode(8195) +
        String.fromCharCode(8196) +
        String.fromCharCode(8197) +
        String.fromCharCode(8198) +
        String.fromCharCode(8239) +
        String.fromCharCode(8201) +
        String.fromCharCode(8202) +
        nbspChar;

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

    function isLetter(ch) {
        if (ch >= "a" && ch <= "z") return true;
        if (ch >= "A" && ch <= "Z") return true;
        if (ch >= "а" && ch <= "я") return true;
        if (ch >= "А" && ch <= "Я") return true;
        if (ch == "ё" || ch == "Ё") return true;
        return false;
    }

    function toLowerCaseStr(str) {
        var result = "";
        for (var i = 0; i < str.length; i++) {
            var ch = str.charAt(i);
            var code = ch.charCodeAt(0);
            if (code >= 65 && code <= 90) {
                result += String.fromCharCode(code + 32);
            }
            else if (code >= 1040 && code <= 1071) {
                result += String.fromCharCode(code + 32);
            }
            else if (code == 1025) {
                result += String.fromCharCode(1105);
            }
            else {
                result += ch;
            }
        }
        return result;
    }

    function isWordInDictionary(word, dictionary) {
        var wordLower = toLowerCaseStr(word);
        var dictLower = toLowerCaseStr(dictionary);
        var wordStart = 0;
        for (var i = 0; i <= dictLower.length; i++) {
            if (i == dictLower.length || dictLower.charAt(i) == ",") {
                var dictWord = "";
                for (var j = wordStart; j < i; j++) {
                    dictWord += dictLower.charAt(j);
                }
                if (dictWord == wordLower) return true;
                wordStart = i + 1;
            }
        }
        return false;
    }

    function isInsideFootnote(node) {
        var current = node;
        while (current) {
            if (current.nodeName == "A") {
                var className = current.className || "";
                var href = current.getAttribute("href") || "";
                if (className == "note" || (href.length > 0 && href.charAt(0) == "#")) {
                    return true;
                }
            }
            current = current.parentNode;
        }
        return false;
    }

    function collectStartPlainText(pElement, maxLength) {
        var result = "";

        function collectPlainFromNode(node) {
            if (!node) return false;
            if (result.length >= maxLength) return true;

            if (node.nodeType == 3) {
                result += node.nodeValue;
                if (result.length >= maxLength) return true;
            } else if (node.nodeType == 1) {
                var child = node.firstChild;
                while (child) {
                    if (collectPlainFromNode(child)) return true;
                    child = child.nextSibling;
                }
            }
            return false;
        }

        var child = pElement.firstChild;
        while (child) {
            if (collectPlainFromNode(child)) break;
            child = child.nextSibling;
        }

        return result;
    }

    function startsWithNumber(text) {
        if (text.length == 0) return { found: false, number: "", afterNumber: "", numberStartIndex: -1, numberEndIndex: -1 };

        var i = 0;
        var len = text.length;

        var spaceCount = 0;
        while (i < len && isSpace(text.charAt(i))) {
            i++;
            spaceCount++;
        }
        if (i >= len) return { found: false, number: "", afterNumber: "", numberStartIndex: -1, numberEndIndex: -1 };

        var numStart = i;

        if (!isDigit(text.charAt(i))) return { found: false, number: "", afterNumber: "", numberStartIndex: -1, numberEndIndex: -1 };

        // Если число начинается с нуля
        if (text.charAt(i) == "0") {
            if (!processLeadingZeros) {
                // Обработка чисел с ведущими нулями выключена
                return { found: false, number: "", afterNumber: "", numberStartIndex: -1, numberEndIndex: -1 };
            }
            // Проверяем, что после нуля есть ещё цифры (чтобы сам "0" не обрабатывался)
            if (i + 1 >= len || !isDigit(text.charAt(i + 1))) {
                return { found: false, number: "", afterNumber: "", numberStartIndex: -1, numberEndIndex: -1 };
            }
        }

        var number = "";
        while (i < len && isDigit(text.charAt(i))) {
            number += text.charAt(i);
            i++;
        }

        // Вычисляем числовое значение (даже для чисел с ведущими нулями)
        var numValue = 0;
        for (var d = 0; d < number.length; d++) {
            numValue = numValue * 10 + (number.charCodeAt(d) - 48);
        }
        if (numValue > 9999) return { found: false, number: "", afterNumber: "", numberStartIndex: -1, numberEndIndex: -1 };

        var afterNumber = "";
        for (var a = i; a < len; a++) {
            afterNumber += text.charAt(a);
        }

        return { found: true, number: number, afterNumber: afterNumber, numberStartIndex: spaceCount, numberEndIndex: i };
    }

    function isBareAfterNumber(afterNumber) {
        if (afterNumber.length == 0) return true;

        var i = 0;
        while (i < afterNumber.length && isSpace(afterNumber.charAt(i))) {
            i++;
        }
        if (i >= afterNumber.length) return true;

        var ch = afterNumber.charAt(i);

        if (ch == "." || ch == ")") {
            if (ch == "." && i + 2 < afterNumber.length && afterNumber.charAt(i + 1) == "." && afterNumber.charAt(i + 2) == ".") {
                var j = i + 3;
                while (j < afterNumber.length && isSpace(afterNumber.charAt(j))) {
                    j++;
                }
                if (j >= afterNumber.length) return true;
                while (j < afterNumber.length) {
                    if (isLetter(afterNumber.charAt(j))) return false;
                    j++;
                }
                return true;
            }

            var j = i + 1;
            while (j < afterNumber.length) {
                if (isLetter(afterNumber.charAt(j))) return false;
                if (isSpace(afterNumber.charAt(j))) {
                    j++;
                    continue;
                }
                j++;
            }
            return true;
        }

        if (ch == ":" || ch == "," || ch == ";" || ch == "!" || ch == "?") {
            var k = i + 1;
            while (k < afterNumber.length && isSpace(afterNumber.charAt(k))) {
                k++;
            }
            if (k >= afterNumber.length) return true;
            while (k < afterNumber.length) {
                if (isLetter(afterNumber.charAt(k))) return false;
                k++;
            }
            return true;
        }

        if (isLetter(ch) || isDigit(ch)) return false;

        var m = i + 1;
        while (m < afterNumber.length) {
            if (isLetter(afterNumber.charAt(m))) return false;
            m++;
        }
        return true;
    }

    function isFraction(afterNumber) {
        var i = 0;
        while (i < afterNumber.length && isSpace(afterNumber.charAt(i))) {
            i++;
        }
        if (i >= afterNumber.length) return false;
        return (afterNumber.charAt(i) == "/");
    }

    function isMathExpression(afterNumber) {
        var i = 0;
        while (i < afterNumber.length && isSpace(afterNumber.charAt(i))) {
            i++;
        }
        if (i >= afterNumber.length) return false;
        var ch = afterNumber.charAt(i);
        return (ch == "+" || ch == "*" || ch == "\u00D7" || ch == ":" || ch == "=");
    }

    function isInterval(afterNumber) {
        var i = 0;
        while (i < afterNumber.length && isSpace(afterNumber.charAt(i))) {
            i++;
        }
        if (i >= afterNumber.length) return false;
        var ch = afterNumber.charAt(i);
        if (ch == "-" || ch == "\u2013" || ch == "\u2014" || ch == "\u2212") {
            var j = i + 1;
            while (j < afterNumber.length && isSpace(afterNumber.charAt(j))) {
                j++;
            }
            if (j < afterNumber.length && isDigit(afterNumber.charAt(j))) {
                return true;
            }
        }
        return false;
    }

    function isTime(afterNumber) {
        var i = 0;
        while (i < afterNumber.length && isSpace(afterNumber.charAt(i))) {
            i++;
        }
        if (i >= afterNumber.length) return false;
        return (afterNumber.charAt(i) == ":");
    }

    function isOrdinal(afterNumber) {
        var i = 0;
        while (i < afterNumber.length && isSpace(afterNumber.charAt(i))) {
            i++;
        }
        if (i >= afterNumber.length) return false;
        var ch = afterNumber.charAt(i);
        if (ch == "-" || ch == "\u2013" || ch == "\u2014") {
            var j = i + 1;
            if (j < afterNumber.length) {
                var nextCh = afterNumber.charAt(j);
                if (isLetter(nextCh)) {
                    return true;
                }
            }
        }
        return false;
    }

    function startsWithDot(afterNumber) {
        var i = 0;
        while (i < afterNumber.length && isSpace(afterNumber.charAt(i))) {
            i++;
        }
        if (i >= afterNumber.length) return false;
        return (afterNumber.charAt(i) == ".");
    }

    function startsWithRoundBracket(afterNumber) {
        var i = 0;
        while (i < afterNumber.length && isSpace(afterNumber.charAt(i))) {
            i++;
        }
        if (i >= afterNumber.length) return false;
        return (afterNumber.charAt(i) == ")");
    }

    function startsWithDefis(afterNumber) {
        var i = 0;
        while (i < afterNumber.length && isSpace(afterNumber.charAt(i))) {
            i++;
        }
        if (i >= afterNumber.length) return false;
        var ch = afterNumber.charAt(i);
        return (ch == "-" && (i + 1 >= afterNumber.length || (afterNumber.charAt(i + 1) != "-" && !isDigit(afterNumber.charAt(i + 1)))));
    }

    function startsWithDash(afterNumber) {
        var i = 0;
        while (i < afterNumber.length && isSpace(afterNumber.charAt(i))) {
            i++;
        }
        if (i >= afterNumber.length) return false;
        var ch = afterNumber.charAt(i);
        return (ch == "\u2013" || ch == "\u2014" || ch == "\u2212");
    }

    function startsWithDateTimeWord(afterNumber) {
        var i = 0;
        while (i < afterNumber.length && isSpace(afterNumber.charAt(i))) {
            i++;
        }
        if (i >= afterNumber.length) return false;

        var word = "";
        while (i < afterNumber.length && (isLetter(afterNumber.charAt(i)) || afterNumber.charAt(i) == "-")) {
            word += afterNumber.charAt(i);
            i++;
        }

        if (word.length == 0) return false;
        return isWordInDictionary(word, dateTimeWords);
    }

    function startsWithHeadingWord(afterNumber) {
        var i = 0;
        while (i < afterNumber.length && isSpace(afterNumber.charAt(i))) {
            i++;
        }
        if (i >= afterNumber.length) return false;

        var word = "";
        while (i < afterNumber.length && isLetter(afterNumber.charAt(i))) {
            word += afterNumber.charAt(i);
            i++;
        }

        if (word.length == 0) return false;
        return isWordInDictionary(word, headingWords);
    }

    function hasEnoughTextAfterNumber(afterNumber, minLength) {
        var letterCount = 0;
        var i = 0;
        while (i < afterNumber.length) {
            var ch = afterNumber.charAt(i);
            if (isLetter(ch)) {
                letterCount++;
                if (letterCount >= minLength) return true;
            }
            i++;
        }
        return false;
    }

    function shouldRemoveTrailingChar(afterNumber) {
        var i = 0;
        while (i < afterNumber.length && isSpace(afterNumber.charAt(i))) {
            i++;
        }
        if (i >= afterNumber.length) return { remove: false, charsToSkip: 0 };
        var ch = afterNumber.charAt(i);
        if (ch == "." || ch == ")") {
            return { remove: true, charsToSkip: i + 1 };
        }
        return { remove: false, charsToSkip: 0 };
    }

    function replaceNumberInFirstTextNode(pElement, number, numberStartInPlain, numberEndInPlain, replacementText, removeTrailing) {
        var found = false;

        function replaceInNode(node, currentPos) {
            if (found) return currentPos;

            if (node.nodeType == 3) {
                var text = node.nodeValue;
                var nodeLen = text.length;

                var numStartInNode = numberStartInPlain - currentPos;
                var numEndInNode = numberEndInPlain - currentPos;

                if (numStartInNode >= 0 && numStartInNode < nodeLen) {
                    if (numEndInNode <= nodeLen) {
                        var before = "";
                        for (var b = 0; b < numStartInNode; b++) {
                            before += text.charAt(b);
                        }
                        var after = "";
                        for (var a = numEndInNode; a < nodeLen; a++) {
                            after += text.charAt(a);
                        }
                        node.nodeValue = before + replacementText + after;
                        found = true;
                    } else {
                        var before = "";
                        for (var b = 0; b < numStartInNode; b++) {
                            before += text.charAt(b);
                        }
                        node.nodeValue = before + replacementText;
                        found = true;
                    }
                }

                return currentPos + nodeLen;
            } else if (node.nodeType == 1) {
                var child = node.firstChild;
                var newPos = currentPos;
                while (child) {
                    newPos = replaceInNode(child, newPos);
                    if (found) return newPos;
                    child = child.nextSibling;
                }
                return newPos;
            }

            return currentPos;
        }

        replaceInNode(pElement, 0);
        return found;
    }

    function isSubtitleOfOnlySigns(pElement, signChar) {
        if (pElement.className != "subtitle") return false;
        var text = pElement.innerText || pElement.textContent || "";
        var hasSign = false;
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            if (ch == signChar) {
                hasSign = true;
            } else if (ch == " " || isSpace(ch)) {
            } else {
                return false;
            }
        }
        return hasSign;
    }

    var alternativeWasUsed = false;
    var originalFormatMode = formatMode;
    var foundSignCount = 0;
    var foundSignName = "";

    if (formatMode == 1 || formatMode == 2) {
        var signToCheck = (formatMode == 1) ? "*" : "#";
        var signName = (formatMode == 1) ? "звёздочки" : "решетки";
        var totalSignCount = 0;
        var paragraphsWithSign = 0;

        var allPElements = document.getElementsByTagName("P");
        for (var p = 0; p < allPElements.length; p++) {
            var pEl = allPElements[p];
            if (isSubtitleOfOnlySigns(pEl, signToCheck)) continue;

            var text = pEl.innerText || pEl.textContent || "";
            var hasSign = false;
            for (var s = 0; s < text.length; s++) {
                if (text.charAt(s) == signToCheck) {
                    totalSignCount++;
                    hasSign = true;
                }
            }
            if (hasSign) paragraphsWithSign++;
        }

        if (totalSignCount > 0) {
            foundSignCount = totalSignCount;
            foundSignName = signName;

            if (useAlternativeMarker == 1) {
                formatMode = alternativeMarker;
                alternativeWasUsed = true;
            } else {
                var warnMsg = scriptName + "\n";
                warnMsg += "ver. " + version + "\n\n";
                warnMsg += "В тексте найдено " + signName + ": " + totalSignCount + " в " + paragraphsWithSign + " абзацах.\n\n";
                warnMsg += "Рекомендуется использовать другой маркер.";
                MsgBox(warnMsg, "FBE скрипт");
                return;
            }
        }
    }

    var fbw_body = document.getElementById("fbw_body");
    if (!fbw_body) {
        MsgBox("Ошибка: не найден fbw_body", scriptName + " v" + version);
        return;
    }

    var hasSelection = false;
    var selectionElements = [];
    var blockStartEl = null;
    var blockEndEl = null;

    try {
        var tr = document.selection.createRange();
        if (tr && tr.compareEndPoints("StartToEnd", tr) != 0) {
            if (tr.parentElement().nodeName != "TEXTAREA" && tr.parentElement().nodeName != "INPUT") {
                hasSelection = true;

                var tr3 = document.selection.createRange();
                tr3.collapse(true);
                blockStartEl = tr3.parentElement();
                tr3 = document.selection.createRange();
                tr3.collapse(false);
                blockEndEl = tr3.parentElement();

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

                var ptr = blockStartEl;
                while (ptr && fbw_body.contains(ptr)) {
                    selectionElements.push(ptr);
                    if (ptr === blockEndEl) break;
                    ptr = getNextP(ptr);
                }
            }
        }
    } catch (e) {
        hasSelection = false;
    }

    var startTime = new Date();

    window.external.BeginUndoUnit(document, scriptName);

    var changedParagraphs = 0;
    var totalChecked = 0;
    var totalWithNumbers = 0;
    var totalExcluded = 0;

    var excludedByFootnote = 0;
    var excludedBySubtitleTextAuthor = 0;
    var excludedByDivContainers = 0;
    var excludedByFraction = 0;
    var excludedByMath = 0;
    var excludedByInterval = 0;
    var excludedByTime = 0;
    var excludedByOrdinal = 0;
    var excludedByDot = 0;
    var excludedByBracket = 0;
    var excludedByDefis = 0;
    var excludedByDash = 0;
    var excludedByDateTime = 0;
    var excludedByHeading = 0;
    var excludedByMinLength = 0;
    var excludedByLeadingZeros = 0;

    var paragraphsToProcess = [];

    if (hasSelection) {
        for (var e = 0; e < selectionElements.length; e++) {
            var el = selectionElements[e];
            if (el.nodeName == "P") {
                paragraphsToProcess.push(el);
            }
        }
    } else {
        var bodyDivs = document.getElementsByTagName("DIV");
        var targetBodies = [];

        for (var b = 0; b < bodyDivs.length; b++) {
            if (bodyDivs[b].className == "body") {
                var fbname = bodyDivs[b].getAttribute("fbname") || "";
                if (fbname == "") {
                    targetBodies.push(bodyDivs[b]);
                } else if (fbname == "notes" && processNotesSection) {
                    targetBodies.push(bodyDivs[b]);
                } else if (fbname == "comments" && processCommentsSection) {
                    targetBodies.push(bodyDivs[b]);
                }
            }
        }

        for (var tb = 0; tb < targetBodies.length; tb++) {
            var allP = targetBodies[tb].getElementsByTagName("P");
            for (var ap = 0; ap < allP.length; ap++) {
                paragraphsToProcess.push(allP[ap]);
            }
        }
    }

    for (var idx = paragraphsToProcess.length - 1; idx >= 0; idx--) {
        var pElement = paragraphsToProcess[idx];
        totalChecked++;

        var plainText = collectStartPlainText(pElement, 200);
        if (plainText.length == 0) continue;

        var numberInfo = startsWithNumber(plainText);
        if (!numberInfo.found) continue;

        var afterNumber = numberInfo.afterNumber;

        if (isBareAfterNumber(afterNumber)) continue;

        totalWithNumbers++;

        if (isInsideFootnote(pElement)) {
            excludedByFootnote++;
            totalExcluded++;
            continue;
        }

        var pClass = pElement.className || "";

        if (!processSubtitlesTextAuthor) {
            if (pClass == "subtitle" || pClass == "text-author") {
                excludedBySubtitleTextAuthor++;
                totalExcluded++;
                continue;
            }
        }

        if (!processDivContainers) {
            var parent = pElement.parentNode;
            if (parent && parent.nodeType == 1 && parent.nodeName == "DIV") {
                var parentClass = parent.className || "";
                if (parentClass == "title" || parentClass == "epigraph" || parentClass == "cite" ||
                    parentClass == "poem" || parentClass == "stanza" || parentClass == "table" ||
                    parentClass == "annotation" || parentClass == "history") {
                    excludedByDivContainers++;
                    totalExcluded++;
                    continue;
                }
            }
        }

        var number = numberInfo.number;
        var numberStartInPlain = numberInfo.numberStartIndex;
        var numberEndInPlain = numberInfo.numberEndIndex;

        if (isFraction(afterNumber)) { excludedByFraction++; totalExcluded++; continue; }
        if (isMathExpression(afterNumber)) { excludedByMath++; totalExcluded++; continue; }
        if (isInterval(afterNumber)) { excludedByInterval++; totalExcluded++; continue; }
        if (isTime(afterNumber)) { excludedByTime++; totalExcluded++; continue; }
        if (isOrdinal(afterNumber)) { excludedByOrdinal++; totalExcluded++; continue; }

        if (!processWithDot && startsWithDot(afterNumber)) { excludedByDot++; totalExcluded++; continue; }
        if (!processWithBracket && startsWithRoundBracket(afterNumber)) { excludedByBracket++; totalExcluded++; continue; }
        if (!processWithDefis && startsWithDefis(afterNumber)) { excludedByDefis++; totalExcluded++; continue; }
        if (!processWithDash && startsWithDash(afterNumber)) { excludedByDash++; totalExcluded++; continue; }
        if (!processWithDateTime && startsWithDateTimeWord(afterNumber)) { excludedByDateTime++; totalExcluded++; continue; }
        if (!processWithHeadingWords && startsWithHeadingWord(afterNumber)) { excludedByHeading++; totalExcluded++; continue; }

        if (!hasEnoughTextAfterNumber(afterNumber, minTextLengthAfterNumber)) { excludedByMinLength++; totalExcluded++; continue; }

        var trailingInfo = shouldRemoveTrailingChar(afterNumber);
        if (trailingInfo.remove) {
            numberEndInPlain += trailingInfo.charsToSkip;
        }

        if (formatMode == 0) {
            var html = pElement.innerHTML;

            var htmlStripped = "";
            var inTag = false;
            var htmlPositions = [];
            for (var hi = 0; hi < html.length; hi++) {
                if (html.charAt(hi) == "<") {
                    inTag = true;
                } else if (html.charAt(hi) == ">") {
                    inTag = false;
                } else if (!inTag) {
                    htmlPositions.push(hi);
                    htmlStripped += html.charAt(hi);
                }
            }

            var numStartInStripped = -1;
            var searchPos = 0;
            var skippedSpaces = 0;
            while (searchPos < htmlStripped.length && isSpace(htmlStripped.charAt(searchPos)) && skippedSpaces < numberStartInPlain) {
                searchPos++;
                skippedSpaces++;
            }
            if (searchPos < htmlStripped.length && isDigit(htmlStripped.charAt(searchPos))) {
                numStartInStripped = searchPos;
            }

            if (numStartInStripped >= 0) {
                var numEndInStripped = numStartInStripped;
                while (numEndInStripped < htmlStripped.length && isDigit(htmlStripped.charAt(numEndInStripped))) {
                    numEndInStripped++;
                }

                var htmlStartPos = htmlPositions[numStartInStripped];
                var htmlEndPos = htmlPositions[numEndInStripped - 1] + 1;

                var newHtml = "";
                for (var hi2 = 0; hi2 < htmlStartPos; hi2++) {
                    newHtml += html.charAt(hi2);
                }
                newHtml += "<SUP>";
                for (var hi3 = htmlStartPos; hi3 < htmlEndPos; hi3++) {
                    newHtml += html.charAt(hi3);
                }
                newHtml += "</SUP>";
                for (var hi4 = htmlEndPos; hi4 < html.length; hi4++) {
                    newHtml += html.charAt(hi4);
                }

                pElement.innerHTML = newHtml;
                changedParagraphs++;
            }
        } else {
            var replacement = "";
            switch (formatMode) {
                case 1: replacement = "*"; break;
                case 2: replacement = "#"; break;
                case 3: replacement = "[" + number + "]"; break;
                case 4: replacement = "{" + number + "}"; break;
                case 5: replacement = "[~" + number + "~]"; break;
                case 6: replacement = "{~" + number + "~}"; break;
            }

            if (replaceNumberInFirstTextNode(pElement, number, numberStartInPlain, numberEndInPlain, replacement, trailingInfo.remove)) {
                changedParagraphs++;
            }
        }
    }

    window.external.EndUndoUnit(document);

    var endTime = new Date();
    var executionTime = (endTime - startTime) / 1000;
    var timeStr = executionTime.toFixed(3);
    timeStr = timeStr.replace(".", ",");

    if (showStatistics == 1 || changedParagraphs == 0) {
        var modeNames = [
            "надстрочный текст (SUP)",
            "знак звёздочки *",
            "знак решётки #",
            "квадратные скобки [N]",
            "фигурные скобки {N}",
            "квадратные скобки с тильдами [~N~]",
            "фигурные скобки с тильдами {~N~}"
        ];
        var modeName = modeNames[formatMode];

        var originalModeNames = [
            "надстрочный текст (SUP)",
            "звёздочки",
            "решетки",
            "квадратные скобки [N]",
            "фигурные скобки {N}",
            "квадратные скобки с тильдами [~N~]",
            "фигурные скобки с тильдами {~N~}"
        ];
        var originalModeName = originalModeNames[originalFormatMode];

        var msg = "";
        msg += scriptName + "\n";
        msg += "ver. " + version + "\n\n";

        if (alternativeWasUsed) {
            msg += "Выбран маркер: " + originalModeName + " (уже есть в тексте: " + foundSignCount + ")\n";
            msg += "Использован маркер: " + modeName + "\n\n";
        }

        msg += "\u2713 Абзацев изменено: " + changedParagraphs;
        if (changedParagraphs == 0) {
            msg += " (подходящих абзацев не найдено)";
        }
        msg += "\n\n";

        msg += "  \u2022 Всего абзацев проверено: " + totalChecked + "\n";
        msg += "  \u2022 Всего абзацев с цифрами в начале: " + totalWithNumbers + "\n";
        msg += "  \u2022 Исключено согласно настроек: " + totalExcluded + "\n";

        if (totalExcluded > 0) {
            msg += "\nИз них исключено:\n";
            if (excludedByDivContainers > 0) msg += "  \u2022 В блочных элементах: " + excludedByDivContainers + "\n";
            if (excludedBySubtitleTextAuthor > 0) msg += "  \u2022 В subtitle/text-author: " + excludedBySubtitleTextAuthor + "\n";
            if (excludedByFootnote > 0) msg += "  \u2022 Внутри существующих сносок: " + excludedByFootnote + "\n";
            if (excludedByFraction > 0) msg += "  \u2022 Дроби: " + excludedByFraction + "\n";
            if (excludedByMath > 0) msg += "  \u2022 Мат. выражения: " + excludedByMath + "\n";
            if (excludedByInterval > 0) msg += "  \u2022 Интервалы: " + excludedByInterval + "\n";
            if (excludedByTime > 0) msg += "  \u2022 Время (часы:минуты): " + excludedByTime + "\n";
            if (excludedByOrdinal > 0) msg += "  \u2022 Порядковые (1-й, 2-я): " + excludedByOrdinal + "\n";
            if (excludedByDot > 0) msg += "  \u2022 С точкой: " + excludedByDot + "\n";
            if (excludedByBracket > 0) msg += "  \u2022 С круглой скобкой: " + excludedByBracket + "\n";
            if (excludedByDefis > 0) msg += "  \u2022 С дефисом: " + excludedByDefis + "\n";
            if (excludedByDash > 0) msg += "  \u2022 С тире: " + excludedByDash + "\n";
            if (excludedByDateTime > 0) msg += "  \u2022 С месяцем/временем: " + excludedByDateTime + "\n";
            if (excludedByHeading > 0) msg += "  \u2022 С заголовочными словами: " + excludedByHeading + "\n";
            if (excludedByMinLength > 0) msg += "  \u2022 Мин. длина текста: " + excludedByMinLength + "\n";
        }

        msg += "\nИспользуемые настройки:\n";
        msg += "  \u2022 Оформление: " + modeName + "\n";
        if (hasSelection) {
            msg += "  \u2022 Режим: выделенный фрагмент\n";
        } else {
            msg += "  \u2022 Режим: весь документ\n";
        }
        msg += "  \u2022 Обработка сносок: " + (processNotesSection ? "да" : "нет") + "\n";
        msg += "  \u2022 Обработка комментариев: " + (processCommentsSection ? "да" : "нет") + "\n";
        msg += "  \u2022 Обработка блочных элементов: " + (processDivContainers ? "да" : "нет") + "\n";
        msg += "  \u2022 Обработка subtitle/text-author: " + (processSubtitlesTextAuthor ? "да" : "нет") + "\n";
        msg += "  \u2022 Числа с ведущими нулями (01, 001): " + (processLeadingZeros ? "да" : "нет") + "\n";
        msg += "  \u2022 С точкой: " + (processWithDot ? "да" : "нет") + "\n";
        msg += "  \u2022 С круглой скобкой: " + (processWithBracket ? "да" : "нет") + "\n";
        msg += "  \u2022 С дефисом: " + (processWithDefis ? "да" : "нет") + "\n";
        msg += "  \u2022 С тире: " + (processWithDash ? "да" : "нет") + "\n";
        msg += "  \u2022 С месяцем/временем: " + (processWithDateTime ? "да" : "нет") + "\n";
        msg += "  \u2022 С заголовочными словами: " + (processWithHeadingWords ? "да" : "нет") + "\n";
        msg += "  \u2022 Мин. длина текста после числа: " + minTextLengthAfterNumber + "\n";

        msg += "\nВремя выполнения: " + timeStr + " сек.";

        MsgBox(msg, "FBE скрипт");
    }
}
