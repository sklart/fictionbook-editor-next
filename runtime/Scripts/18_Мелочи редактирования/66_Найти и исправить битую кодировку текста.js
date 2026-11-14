// Скрипт "Найти и исправить битую кодировку текста" для редактора FBE
// version 1.8
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для обнаружения и исправления битой кодировки текста в fb2 документах.
// При наличии выделения, скрипт работает с выделенным фрагментом,
// в противном случае - обрабатывается сразу весь документ.
// По умолчанию обрабатываются все разделы документа, включая разделы сносок и комментариев.
// Для каждого найденного битого участка определяется тип перекодировки
//   (cp1251→UTF-8 или UTF-8→cp1251).
// Три режима работы:
//   0 - только диагностика (без исправлений)
//   1 - спросить и исправить (с запросом подтверждения)
//   2 - автоисправление без запроса
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.8, 04.07.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Найти и исправить битую кодировку текста";
    var version = "1.8";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    // Режим обработки "битых кодировок" в документе:
    //   0 - только диагностика (без исправлений)
    //   1 - спросить и исправить (с запросом подтверждения)
    //   2 - автоисправление без запроса
    var fixMode = 1;

    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1;

    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 1;

    // Обрабатывать раздел комментариев
    var processCommentsSection = 1;

    // Минимальная длина битого фрагмента (в символах)
    var minFragmentLength = 5;

    // Максимальное количество показываемых примеров в окне подтверждения
    var maxConfirmExamples = 5;

    // Максимальное количество показываемых примеров в итоговом отчёте
    var maxReportExamples = 3;

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    // ==================================================
    // ФУНКЦИИ ПЕРЕКОДИРОВКИ
    // ==================================================

    var cp1251Table = [
        0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
        0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
        0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x0098, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
        0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
        0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
        0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
        0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457,
        0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
        0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F,
        0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
        0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F,
        0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437,
        0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
        0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447,
        0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F
    ];

    var unicodeToCp1251 = {};
    for (var i = 0; i < cp1251Table.length; i++) {
        unicodeToCp1251[cp1251Table[i]] = i + 128;
    }

    function cp1251ToUnicode(byte) {
        if (byte < 128) return String.fromCharCode(byte);
        var index = byte - 128;
        if (index >= 0 && index < cp1251Table.length) return String.fromCharCode(cp1251Table[index]);
        return "?";
    }

    function unicodeToCp1251Byte(ch) {
        var code = ch.charCodeAt(0);
        if (code < 128) return code;
        if (unicodeToCp1251[code] !== undefined) return unicodeToCp1251[code];
        return -1;
    }

    function tryFixCp1251asUtf8(str) {
        var result = "";
        for (var i = 0; i < str.length; i++) {
            var ch = str.charAt(i);
            var code = ch.charCodeAt(0);
            if (code >= 128 && code <= 255) {
                result += cp1251ToUnicode(code);
            } else {
                result += ch;
            }
        }
        return result;
    }

    function tryFixUtf8asCp1251(str) {
        var bytes = [];
        for (var i = 0; i < str.length; i++) {
            var ch = str.charAt(i);
            var code = ch.charCodeAt(0);
            var byte = unicodeToCp1251Byte(ch);
            if (byte >= 0 && byte <= 255) {
                bytes.push(byte);
            } else {
                return str;
            }
        }

        var result = "";
        var i = 0;
        while (i < bytes.length) {
            var b1 = bytes[i];
            if (b1 < 128) {
                result += String.fromCharCode(b1);
                i++;
            } else if ((b1 & 0xE0) == 0xC0 && i + 1 < bytes.length) {
                var b2 = bytes[i + 1];
                if ((b2 & 0xC0) == 0x80) {
                    var code = ((b1 & 0x1F) << 6) | (b2 & 0x3F);
                    result += String.fromCharCode(code);
                    i += 2;
                } else {
                    result += String.fromCharCode(b1);
                    i++;
                }
            } else {
                result += String.fromCharCode(b1);
                i++;
            }
        }
        return result;
    }

    function fixText(text, fixType) {
        if (fixType == "cp1251 → UTF-8") {
            return tryFixCp1251asUtf8(text);
        } else if (fixType == "UTF-8 → cp1251") {
            return tryFixUtf8asCp1251(text);
        }
        return text;
    }

    function looksLikeValidCyrillic(str) {
        if (str.length == 0) return false;
        var cyrillicCount = 0;
        var totalCount = 0;
        for (var i = 0; i < str.length; i++) {
            var code = str.charCodeAt(i);
            totalCount++;
            if ((code >= 0x0400 && code <= 0x04FF) || (code >= 0x0500 && code <= 0x052F)) {
                cyrillicCount++;
            }
        }
        return (cyrillicCount > 0 && (cyrillicCount / totalCount) >= 0.3);
    }

    function countWords(str) {
        var trimmed = str.replace(/^\s+|\s+$/g, '');
        if (trimmed.length == 0) return 0;
        var words = trimmed.split(/\s+/);
        return words.length;
    }

    function analyzeFragment(text) {
        if (text.length < minFragmentLength) return null;

        var highBytes = 0;
        var totalChars = 0;
        for (var i = 0; i < text.length; i++) {
            var code = text.charCodeAt(i);
            totalChars++;
            if (code >= 128 && code <= 255) highBytes++;
        }

        var cyrillicInOriginal = 0;
        for (var i2 = 0; i2 < text.length; i2++) {
            var code2 = text.charCodeAt(i2);
            if ((code2 >= 0x0400 && code2 <= 0x04FF) || (code2 >= 0x0500 && code2 <= 0x052F)) {
                cyrillicInOriginal++;
            }
        }

        if (looksLikeValidCyrillic(text)) return null;

        if (cyrillicInOriginal >= 3) {
            var fix1 = tryFixUtf8asCp1251(text);
            if (fix1 != text && fix1.length > 0 && looksLikeValidCyrillic(fix1)) {
                return { broken: text, fixed: fix1, type: "cp1251 → UTF-8", description: "Текст был в cp1251 (Windows-1251), но прочитан как UTF-8" };
            }
        }

        var ratio = totalChars > 0 ? highBytes / totalChars : 0;
        if (ratio >= 0.4) {
            var fix2 = tryFixCp1251asUtf8(text);
            if (fix2 != text && fix2.length > 0 && looksLikeValidCyrillic(fix2)) {
                return { broken: text, fixed: fix2, type: "cp1251 → UTF-8", description: "Текст был в cp1251 (Windows-1251), но прочитан как UTF-8" };
            }
        }

        var fix3 = tryFixUtf8asCp1251(text);
        if (fix3 != text && fix3.length > 0 && looksLikeValidCyrillic(fix3)) {
            return { broken: text, fixed: fix3, type: "cp1251 → UTF-8", description: "Текст был в cp1251 (Windows-1251), но прочитан как UTF-8" };
        }

        var fix4 = tryFixCp1251asUtf8(text);
        if (fix4 != text && fix4.length > 0 && looksLikeValidCyrillic(fix4)) {
            return { broken: text, fixed: fix4, type: "cp1251 → UTF-8", description: "Текст был в cp1251 (Windows-1251), но прочитан как UTF-8" };
        }

        return null;
    }

    function getParagraphText(para) {
        return para.innerText || para.textContent || "";
    }

    var nbspChar = String.fromCharCode(160);
    try { var nbspCharTemp = window.external.GetNBSP(); nbspChar = nbspCharTemp; } catch (e) {}

    var unusualSpaces = String.fromCharCode(160) +
        String.fromCharCode(8194) + String.fromCharCode(8195) + String.fromCharCode(8196) +
        String.fromCharCode(8197) + String.fromCharCode(8198) + String.fromCharCode(8239) +
        String.fromCharCode(8201) + String.fromCharCode(8202) + nbspChar;

    function normalizeSpaces(str) {
        var result = "";
        for (var i = 0; i < str.length; i++) {
            var ch = str.charAt(i);
            var isUnusual = false;
            for (var j = 0; j < unusualSpaces.length; j++) {
                if (ch == unusualSpaces.charAt(j)) { isUnusual = true; break; }
            }
            result += isUnusual ? " " : ch;
        }
        return result;
    }

    function getAllParagraphsInBody(bodyDiv) {
        var paragraphs = [];
        var allP = bodyDiv.getElementsByTagName("P");
        for (var i = 0; i < allP.length; i++) {
            paragraphs.push(allP[i]);
        }
        return paragraphs;
    }

    function truncateText(text, maxLen) {
        if (text.length <= maxLen) return text;
        return text.substring(0, maxLen) + "...";
    }

    function fixTextNodesInElement(element, fixType) {
        var childNodes = element.childNodes;
        for (var i = 0; i < childNodes.length; i++) {
            var node = childNodes[i];
            if (node.nodeType == 3) {
                var originalText = node.nodeValue;
                var fixedText = fixText(originalText, fixType);
                if (fixedText != originalText) {
                    node.nodeValue = fixedText;
                }
            } else if (node.nodeType == 1) {
                if (node.nodeName != "A") {
                    fixTextNodesInElement(node, fixType);
                }
            }
        }
    }

    function scrollToParagraph(para) {
        try {
            var tr = document.body.createTextRange();
            tr.moveToElementText(para);
            var trTop = tr.boundingTop;
            var trBottom = tr.boundingTop + tr.boundingHeight;
            var viewHeight = window.external.getViewHeight();

            if (trBottom - trTop <= viewHeight) {
                window.scrollBy(-1000, (trTop + trBottom - viewHeight) / 2);
            } else {
                window.scrollBy(-1000, trTop);
            }
        } catch(e) {}
    }

    function getParagraphsInSelection() {
        var selRange = document.selection.createRange();
        if (!selRange || selRange.compareEndPoints("StartToEnd", selRange) == 0) {
            return [];
        }

        var MyTagName = "B";
        var ttr1 = selRange.duplicate();

        var trStart = ttr1.duplicate();
        trStart.collapse(true);
        trStart.pasteHTML("<" + MyTagName + " id=BlockStart></" + MyTagName + ">");

        var trEnd = ttr1.duplicate();
        trEnd.collapse(false);
        trEnd.pasteHTML("<" + MyTagName + " id=BlockEnd></" + MyTagName + ">");

        var BlockStartNode = document.getElementById("BlockStart");
        var BlockEndNode = document.getElementById("BlockEnd");

        if (!BlockStartNode || !BlockEndNode) {
            return [];
        }

        var paragraphs = [];

        var firstP = BlockStartNode;
        while (firstP && firstP.nodeName != "P") {
            firstP = firstP.parentNode;
        }
        if (firstP && firstP.nodeName == "P") {
            paragraphs.push(firstP);
        }

        var el = firstP || BlockStartNode;
        while (el && el.nodeName != "DIV" && el.nodeName != "BODY") {
            el = el.parentNode;
        }

        var InsideSelection = false;
        var ProcessingEnded = false;
        var ptr = el;

        while (!ProcessingEnded && ptr) {
            if (ptr.nodeType == 1 && ptr.nodeName == MyTagName && ptr.getAttribute("id") == "BlockStart") {
                InsideSelection = true;
            }

            if (ptr.nodeType == 1 && ptr.nodeName == MyTagName && ptr.getAttribute("id") == "BlockEnd") {
                InsideSelection = false;
                ProcessingEnded = true;
            }

            if (ptr.nodeType == 1 && ptr.nodeName == "P" && InsideSelection && ptr !== firstP) {
                var alreadyAdded = false;
                for (var k = 0; k < paragraphs.length; k++) {
                    if (paragraphs[k] === ptr) { alreadyAdded = true; break; }
                }
                if (!alreadyAdded) {
                    paragraphs.push(ptr);
                }
            }

            if (ptr.firstChild) {
                ptr = ptr.firstChild;
            } else {
                while (ptr && ptr.nextSibling == null) {
                    ptr = ptr.parentNode;
                    if (ptr && ptr.nodeType == 1 && ptr.nodeName == MyTagName && ptr.getAttribute("id") == "BlockEnd") {
                        ProcessingEnded = true;
                    }
                }
                if (ptr && !ProcessingEnded) {
                    ptr = ptr.nextSibling;
                }
            }
        }

        if (BlockStartNode && BlockStartNode.parentNode) {
            BlockStartNode.parentNode.removeChild(BlockStartNode);
        }
        if (BlockEndNode && BlockEndNode.parentNode) {
            BlockEndNode.parentNode.removeChild(BlockEndNode);
        }

        return paragraphs;
    }

    // ==================================================
    // ОСНОВНАЯ ЛОГИКА
    // ==================================================

    var foundFragments = [];
    var hasSelection = false;

    var selRangeCheck = document.selection.createRange();
    if (selRangeCheck && selRangeCheck.compareEndPoints("StartToEnd", selRangeCheck) != 0) {
        if (selRangeCheck.parentElement().nodeName != "TEXTAREA" && selRangeCheck.parentElement().nodeName != "INPUT") {
            hasSelection = true;
        }
    }

    if (hasSelection) {
        var selectedParagraphs = getParagraphsInSelection();

        for (var sp = 0; sp < selectedParagraphs.length; sp++) {
            var para = selectedParagraphs[sp];
            var text = getParagraphText(para);
            var normalized = normalizeSpaces(text);

            if (normalized.length < minFragmentLength) continue;

            var result = analyzeFragment(normalized);
            if (result) {
                result.paragraphElement = para;
                result.section = "main";
                foundFragments.push(result);
            }
        }
    } else {
        var allDivs = document.getElementsByTagName("DIV");
        var bodyDivs = [];
        for (var i = 0; i < allDivs.length; i++) {
            if (allDivs[i].className == "body") bodyDivs.push(allDivs[i]);
        }

        for (var b = 0; b < bodyDivs.length; b++) {
            var bodyDiv = bodyDivs[b];
            var fbname = bodyDiv.getAttribute("fbname") || "";

            if (fbname == "") {
                // основной
            } else if (fbname == "notes" && processNotesSection) {
                // сноски
            } else if (fbname == "comments" && processCommentsSection) {
                // комментарии
            } else {
                continue;
            }

            var paragraphs = getAllParagraphsInBody(bodyDiv);
            for (var p = 0; p < paragraphs.length; p++) {
                var para = paragraphs[p];
                var text = getParagraphText(para);
                var normalized = normalizeSpaces(text);
                if (normalized.length < minFragmentLength) continue;
                var result = analyzeFragment(normalized);
                if (result) {
                    result.paragraphElement = para;
                    result.section = fbname || "main";
                    foundFragments.push(result);
                }
            }
        }
    }

    // ==================================================
    // ЕСЛИ НИЧЕГО НЕ НАЙДЕНО — ВСЕГДА показываем сообщение
    // ==================================================
    if (foundFragments.length == 0) {
        var noMsg = scriptName + "\n";
        noMsg += "ver. " + version + "\n\n";
        noMsg += (hasSelection ? "РЕЖИМ: ВЫДЕЛЕННЫЙ ФРАГМЕНТ\n\n" : "РЕЖИМ: ВЕСЬ ДОКУМЕНТ\n\n");
        noMsg += "\u2713 Битой кодировки не обнаружено.\n";
        noMsg += "Текст не содержит характерных признаков перекодировки.\n\n";
        noMsg += "Время выполнения: 0,000 сек.";
        MsgBox(noMsg, "FBE скрипт");
        return;
    }

    // Прокрутка к первому найденному абзацу (только "весь документ")
    if (!hasSelection && foundFragments.length > 0 && foundFragments[0].paragraphElement) {
        scrollToParagraph(foundFragments[0].paragraphElement);
    }

    // Подсчёт статистики
    var totalCharsBroken = 0;
    var totalWordsBroken = 0;
    var mainCount = 0;
    var notesCount = 0;
    var commentsCount = 0;

    for (var s = 0; s < foundFragments.length; s++) {
        var frag = foundFragments[s];
        totalCharsBroken += frag.broken.length;
        totalWordsBroken += countWords(frag.broken);
        if (frag.section == "main") mainCount++;
        else if (frag.section == "notes") notesCount++;
        else if (frag.section == "comments") commentsCount++;
    }

    // ==================================================
    // ЗАПРОС ПОДТВЕРЖДЕНИЯ (fixMode == 1)
    // ==================================================
    var doFix = false;

    if (fixMode == 2) {
        doFix = true;
    } else if (fixMode == 1) {
        var confirmMsg = scriptName + "\n";
        confirmMsg += "ver. " + version + "\n\n";
        confirmMsg += (hasSelection ? "РЕЖИМ: ВЫДЕЛЕННЫЙ ФРАГМЕНТ\n\n" : "РЕЖИМ: ВЕСЬ ДОКУМЕНТ\n\n");
        confirmMsg += "\u2713 Обнаружено фрагментов с битой кодировкой: " + foundFragments.length + "\n\n";

        var confirmExamples = foundFragments.length;
        if (confirmExamples > maxConfirmExamples) confirmExamples = maxConfirmExamples;

        for (var e = 0; e < confirmExamples; e++) {
            var ef = foundFragments[e];
            confirmMsg += "--- Пример " + (e + 1) + " ---\n";
            confirmMsg += "Тип: " + ef.type + "\n";
            confirmMsg += "Битый: " + truncateText(ef.broken, 60) + "\n";
            confirmMsg += "Испр.: " + truncateText(ef.fixed, 60) + "\n\n";
        }

        if (foundFragments.length > maxConfirmExamples) {
            confirmMsg += "... и ещё " + (foundFragments.length - maxConfirmExamples) + " фрагментов\n\n";
        }

        confirmMsg += "Исправить найденные фрагменты?";

        if (AskYesNo(confirmMsg)) {
            doFix = true;
        }
    }

    // ==================================================
    // ТАЙМЕР ЗАПУСКАЕМ ТОЛЬКО ПОСЛЕ ПОДТВЕРЖДЕНИЯ
    // ==================================================
    var timerStart = new Date();

    // ==================================================
    // ИСПРАВЛЕНИЕ (с сохранением форматирования)
    // ==================================================
    var fixedCount = 0;

    if (doFix) {
        window.external.BeginUndoUnit(document, scriptName);

        for (var f = 0; f < foundFragments.length; f++) {
            var frag = foundFragments[f];
            if (frag.paragraphElement) {
                fixTextNodesInElement(frag.paragraphElement, frag.type);
                fixedCount++;
            }
        }

        window.external.EndUndoUnit(document);
    }

    // ==================================================
    // ИТОГОВЫЙ ОТЧЁТ
    // ==================================================

    var endTime = new Date();
    var elapsed = (endTime.getTime() - timerStart.getTime()) / 1000;
    var elapsedStr = elapsed.toFixed(3).replace(".", ",");

    // Тихий режим + выделение + успешное исправление — молча выходим
    if (showStatistics == 0 && hasSelection && doFix && fixedCount > 0) {
        try {
            window.external.SetStatusBarText(scriptName + ": исправлено " + fixedCount + " фрагментов. Время: " + elapsedStr + " сек.");
        } catch(e) {}
        return;
    }

    // Тихий режим + весь документ + успешное исправление/диагностика — молча выходим
    if (showStatistics == 0 && !hasSelection && fixMode != 1) {
        try {
            if (doFix) {
                window.external.SetStatusBarText(scriptName + ": исправлено " + fixedCount + " фрагментов. Время: " + elapsedStr + " сек.");
            } else {
                window.external.SetStatusBarText(scriptName + ": найдено " + foundFragments.length + " фрагментов. Время: " + elapsedStr + " сек.");
            }
        } catch(e) {}
        return;
    }

    // Во всех остальных случаях показываем итоговое окно
    var msg = scriptName + "\n";
    msg += "ver. " + version + "\n\n";
    msg += (hasSelection ? "РЕЖИМ: ВЫДЕЛЕННЫЙ ФРАГМЕНТ\n\n" : "РЕЖИМ: ВЕСЬ ДОКУМЕНТ\n\n");
    msg += "\u2713 Обнаружено фрагментов: " + foundFragments.length + "\n";

    if (doFix) {
        msg += "\u2713 Исправлено фрагментов: " + fixedCount + "\n";
    } else if (fixMode == 1) {
        msg += "\u2713 Исправление отклонено пользователем\n";
    }

    msg += "\nСтатистика битого текста:\n";
    msg += "  \u2022 Абзацев: " + foundFragments.length + "\n";
    msg += "  \u2022 Символов (с пробелами): " + totalCharsBroken + "\n";
    msg += "  \u2022 Слов: " + totalWordsBroken + "\n";
    msg += "\nПо разделам:\n";
    msg += "  \u2022 Основной: " + mainCount + "\n";
    if (notesCount > 0) msg += "  \u2022 Сноски: " + notesCount + "\n";
    if (commentsCount > 0) msg += "  \u2022 Комментарии: " + commentsCount + "\n";

    if (!doFix || hasSelection) {
        var reportExamples = foundFragments.length;
        if (reportExamples > maxReportExamples) reportExamples = maxReportExamples;
        if (reportExamples > 0) {
            msg += "\n--- Примеры ---";
            for (var x = 0; x < reportExamples; x++) {
                var xf = foundFragments[x];
                msg += "\n" + (x + 1) + ". " + truncateText(xf.broken, 40) + " \u2192 " + truncateText(xf.fixed, 40);
            }
            if (foundFragments.length > maxReportExamples) {
                msg += "\n... и ещё " + (foundFragments.length - maxReportExamples) + " фрагментов";
            }
        }
    }

    msg += "\n\nВремя выполнения: " + elapsedStr + " сек.";
    MsgBox(msg, "FBE скрипт");

    try {
        if (doFix) {
            window.external.SetStatusBarText(scriptName + ": исправлено " + fixedCount + " фрагментов. Время: " + elapsedStr + " сек.");
        } else {
            window.external.SetStatusBarText(scriptName + ": найдено " + foundFragments.length + " фрагментов. Время: " + elapsedStr + " сек.");
        }
    } catch(e) {}
}
