// Скрипт "Сформатировать термины жирным (по разделителю)" для редактора FBE
// version 1.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для форматирования тэгами жирности "терминов" в fb2 документах.
// "Термины" должны быть расположены в началах абзацев, как в словарях.
// Разделителями "термина" и его "пояснения" служит двоеточие, тире/дефис, точка или левая круглая скобка.
// "Термин" — весь текст от начала абзаца до разделителя, независимо от регистра.
// Разделители можно включать все сразу или по отдельности.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// Скрипт может также иногда применяться для оформления имен персонажей в пьесах, драмах.

// version 1.2, 19.05.2026
// ======================================

function Run() {
    var scriptName = "Сформатировать термины жирным (по разделителю)";
    var version = "1.2";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА
    // ==================================================

    // 0 - нет или выключено, 1 - да или включено

    // Показывать статистику: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима

    // Обрабатывать текст внутри цитат: 0 - нет, 1 - да
    var processCites = 1;

    // Минимальное кол-во строчных букв после разделителя:
    var MinSmallLetters = 3;

    // Максимальная длина термина (до разделителя):
    var maxTermLength = 25;

    // Обрабатывать текст в диалогах (тире или дефис с пробелами в начале абзаца): 0 - нет, 1 - да
    var processDialogs = 0;

    // ==================================================
    // НАСТРОЙКИ ПРИМЕНЯЕМЫХ ТИПОВ РАЗДЕЛИТЕЛЕЙ (можно использовать все или отдельные)
    // ==================================================

    // Двоеточие с пробелом после: 0 - нет, 1 - да
    var processColon = 1;

    // Длинное тире с пробелами вокруг: 0 - нет, 1 - да
    var processEmDash = 1;

    // Короткое тире с пробелами вокруг: 0 - нет, 1 - да
    var processEnDash = 1;

    // Дефис с пробелами вокруг: 0 - нет, 1 - да
    var processHyphen = 1;

    // Точка с пробелом после: 0 - нет, 1 - да
    var processDot = 1;

    // Левая круглая скобка (с пробелом перед): 0 - нет, 1 - да
    var processLeftRoundBracket = 1;

    // Регистр первой буквы после левой скобки-разделителя:
    // "any" - любой регистр
    // "lower" - только строчная
    // "upper" - только заглавная
    var bracketLetterCase = "any";

    // ==================================================
    // НАСТРОЙКА ПОИСКА РАЗДЕЛИТЕЛЯ
    // ==================================================

    // Искать правую границу "термина":
    // 0 - по первому разделителю (даже если он внутри скобок)
    // 1 - по разделителю после скобок (если в скобках есть разделитель)
    var searchSeparator = 0;

    // ==================================================
    // НАЧАЛО СКРИПТА
    // ==================================================

    var nbspEntity = "&nbsp;";
    var nbspChar = " ";
    try {
        nbspChar = window.external.GetNBSP();
        if (nbspChar.charCodeAt(0) == 160) {
            nbspEntity = "&nbsp;";
        } else {
            nbspEntity = nbspChar;
        }
    } catch(e) {
        nbspChar = String.fromCharCode(160);
        nbspEntity = "&nbsp;";
    }

    var unusualSpaces = String.fromCharCode(160) + String.fromCharCode(8194) +
        String.fromCharCode(8195) + String.fromCharCode(8196) + String.fromCharCode(8197) +
        String.fromCharCode(8198) + String.fromCharCode(8239) + String.fromCharCode(8201) +
        String.fromCharCode(8202) + nbspChar;

    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================

    function trimStr(str) {
        return str.replace(/^\s+|\s+$/g, '');
    }

    function isUnusualSpace(ch) {
        for (var i = 0; i < unusualSpaces.length; i++) {
            if (ch == unusualSpaces.charAt(i)) return true;
        }
        return false;
    }

    function isLowercase(ch) {
        if (ch >= 'а' && ch <= 'я') return true;
        if (ch == 'ё') return true;
        if (ch >= 'a' && ch <= 'z') return true;
        var code = ch.charCodeAt(0);
        if (code >= 0x0100 && code <= 0x017F) {
            if (code == 0x0138) return true;
            if (code == 0x017F) return true;
            if (code == 0x0104) return false;
            if (code == 0x0106) return false;
            if (code == 0x0118) return false;
            if (code == 0x011A) return false;
            if (code == 0x0141) return false;
            if (code == 0x0143) return false;
            if (code == 0x015A) return false;
            if (code == 0x015E) return false;
            if (code == 0x0160) return false;
            if (code == 0x0179) return false;
            if (code == 0x017B) return false;
            if ((code % 2) == 1) return true;
            return false;
        }
        if (code >= 0x0180 && code <= 0x024F) return true;
        if (code == 0x00E4 || code == 0x00F6 || code == 0x00FC || code == 0x00DF) return true;
        if (code == 0x04AF) return true;
        if (code == 0x04D9) return true;
        if (code == 0x04E9) return true;
        if (code == 0x04A3) return true;
        if (code == 0x0497) return true;
        if (code == 0x049B) return true;
        if (code == 0x0493) return true;
        if (code == 0x04B1) return true;
        if (code == 0x04BB) return true;
        if (code == 0x0456) return true;
        if (code == 0x045E) return true;
        if (code == 0x0491) return true;
        if (code == 0x0454) return true;
        return false;
    }

    function isUppercase(ch) {
        if (ch >= 'А' && ch <= 'Я') return true;
        if (ch == 'Ё') return true;
        if (ch >= 'A' && ch <= 'Z') return true;
        var code = ch.charCodeAt(0);
        if (code >= 0x0100 && code <= 0x017F) {
            if (code == 0x0138) return false;
            if (code == 0x017F) return false;
            return true;
        }
        if (code == 0x00C4 || code == 0x00D6 || code == 0x00DC) return true;
        if (code == 0x04AE) return true;
        if (code == 0x04D8) return true;
        if (code == 0x04E8) return true;
        if (code == 0x04A2) return true;
        if (code == 0x0496) return true;
        if (code == 0x049A) return true;
        if (code == 0x0492) return true;
        if (code == 0x04B0) return true;
        if (code == 0x04BA) return true;
        if (code == 0x0406) return true;
        if (code == 0x040E) return true;
        if (code == 0x0490) return true;
        if (code == 0x0404) return true;
        return false;
    }

    function isSpace(ch) {
        return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || isUnusualSpace(ch);
    }

    function isEmDash(ch) {
        return (ch == '—' || ch == '\u2014');
    }

    function isEnDash(ch) {
        return (ch == '–' || ch == '\u2013');
    }

    function isHyphenChar(ch) {
        return (ch == '-' || ch == '‐' || ch == '\u2010');
    }

    function isInsideExcludedDiv(pElement) {
        var parent = pElement.parentNode;
        if (!parent) return false;
        if (parent.nodeName != "DIV") return false;
        var cls = parent.className || "";
        var excludedClasses = ["title", "epigraph", "poem", "stanza", "table", "image", "annotation", "history"];
        for (var i = 0; i < excludedClasses.length; i++) {
            if (cls == excludedClasses[i]) return true;
        }
        if (cls == "cite" && processCites == 0) return true;
        return false;
    }

    function isSubtitle(pElement) {
        if (pElement.nodeName == "P") {
            var cls = pElement.className || "";
            if (cls == "subtitle") return true;
        }
        return false;
    }

    function hasStrongAncestor(node) {
        var current = node;
        while (current) {
            if (current.nodeName == "STRONG" || current.nodeName == "B") return true;
            current = current.parentNode;
        }
        return false;
    }

    function getPlainText(element) {
        if (!element) return "";
        var result = "";
        function collectText(node) {
            if (node.nodeType == 3) {
                result += node.nodeValue;
            } else if (node.nodeType == 1) {
                for (var i = 0; i < node.childNodes.length; i++) {
                    collectText(node.childNodes[i]);
                }
            }
        }
        collectText(element);
        return result;
    }

    function countLowercase(text) {
        var count = 0;
        for (var i = 0; i < text.length; i++) {
            if (isLowercase(text.charAt(i))) count++;
        }
        return count;
    }

    // Проверяет регистр первой буквы после скобки согласно настройке
    function checkBracketCase(ch) {
        if (bracketLetterCase == "any") return true;
        if (bracketLetterCase == "lower" && isLowercase(ch)) return true;
        if (bracketLetterCase == "upper" && isUppercase(ch)) return true;
        return false;
    }

    // Проверяет, является ли позиция разделителем
    function isSeparatorAt(text, pos) {
        var len = text.length;
        if (pos < 0 || pos >= len) return false;

        var ch = text.charAt(pos);

        // Двоеточие с пробелом после
        if (processColon && ch == ':') {
            if (pos + 1 < len && isSpace(text.charAt(pos + 1))) {
                return true;
            }
        }

        // Точка с пробелом после
        if (processDot && ch == '.') {
            if (pos + 1 < len && isSpace(text.charAt(pos + 1))) {
                return true;
            }
        }

        // Левая круглая скобка (пробел перед + проверка регистра после)
        if (processLeftRoundBracket && ch == '(') {
            var hasSpaceBefore = (pos > 0 && isSpace(text.charAt(pos - 1)));
            if (hasSpaceBefore && pos + 1 < len) {
                if (checkBracketCase(text.charAt(pos + 1))) {
                    return true;
                }
            }
        }

        // Тире/дефис с пробелами вокруг
        var isDashType = false;
        if (processEmDash && isEmDash(ch)) isDashType = true;
        else if (processEnDash && isEnDash(ch)) isDashType = true;
        else if (processHyphen && isHyphenChar(ch)) isDashType = true;

        if (isDashType) {
            var hasSpaceBefore2 = (pos > 0 && isSpace(text.charAt(pos - 1)));
            var hasSpaceAfter = (pos + 1 < len && isSpace(text.charAt(pos + 1)));
            if (hasSpaceBefore2 && hasSpaceAfter) {
                return true;
            }
        }

        return false;
    }

    // Проверяет, находится ли позиция внутри скобок
    function isInsideParens(text, pos) {
        var openCount = 0;
        for (var i = 0; i < pos; i++) {
            if (text.charAt(i) == '(') openCount++;
            if (text.charAt(i) == ')') openCount--;
        }
        return openCount > 0;
    }

    // Находит позицию после закрывающей скобки
    function findAfterParens(text, startPos) {
        var len = text.length;
        var openCount = 0;
        for (var i = startPos; i < len; i++) {
            if (text.charAt(i) == '(') openCount++;
            if (text.charAt(i) == ')') {
                openCount--;
                if (openCount == 0) return i + 1;
            }
        }
        return -1;
    }

    // ==================================================
    // ПОИСК ТЕРМИНА ПО РАЗДЕЛИТЕЛЮ
    // ==================================================

    function findTermCandidate(plainText) {
        if (!plainText || plainText.length == 0) {
            return { found: false, termEndPos: 0, type: "" };
        }

        var text = plainText;
        var len = text.length;
        var pos = 0;

        while (pos < len && isSpace(text.charAt(pos))) pos++;
        if (pos >= len) return { found: false, termEndPos: 0, type: "" };

        var startPos = pos;

        // Проверка диалогов
        if (!processDialogs) {
            var firstCh = text.charAt(startPos);
            if (firstCh == '-' || isEmDash(firstCh) || isEnDash(firstCh) || isHyphenChar(firstCh)) {
                if (startPos + 1 < len && isSpace(text.charAt(startPos + 1))) {
                    return { found: false, termEndPos: 0, type: "" };
                }
            }
        }

        // Ищем разделитель
        var separatorPos = -1;
        var searchPos = startPos;

        while (searchPos < len) {
            if (isSeparatorAt(text, searchPos)) {
                if (searchSeparator == 0) {
                    separatorPos = searchPos;
                    break;
                } else {
                    if (!isInsideParens(text, searchPos)) {
                        separatorPos = searchPos;
                        break;
                    } else {
                        var afterParens = findAfterParens(text, searchPos);
                        if (afterParens > 0 && afterParens < len) {
                            searchPos = afterParens;
                            continue;
                        }
                    }
                }
            }
            searchPos++;
        }

        if (separatorPos <= startPos) {
            return { found: false, termEndPos: 0, type: "" };
        }

        // Извлекаем текст термина
        var termText = "";
        for (var i = startPos; i < separatorPos; i++) {
            termText += text.charAt(i);
        }
        termText = trimStr(termText);

        if (termText.length == 0) {
            return { found: false, termEndPos: 0, type: "" };
        }

        var termLength = termText.length;
        if (termLength > maxTermLength) {
            return { found: false, termEndPos: 0, type: "" };
        }

        var afterSep = "";
        for (var i = separatorPos + 1; i < len; i++) {
            afterSep += text.charAt(i);
        }
        afterSep = trimStr(afterSep);

        if (countLowercase(afterSep) < MinSmallLetters) {
            return { found: false, termEndPos: 0, type: "" };
        }

        var sepCh = text.charAt(separatorPos);
        var sepType;
        if (sepCh == ':') sepType = "colon";
        else if (sepCh == '.') sepType = "dot";
        else if (sepCh == '(') sepType = "bracket";
        else sepType = "dash";

        var cleanEnd = separatorPos;
        while (cleanEnd > startPos && isSpace(text.charAt(cleanEnd - 1))) {
            cleanEnd--;
        }

        return { found: true, termEndPos: cleanEnd, type: sepType };
    }

    // ==================================================
    // ФОРМАТИРОВАНИЕ
    // ==================================================

    function formatTermInParagraph(pElement, termEndPos) {
        if (termEndPos <= 0) return false;
        var foundInfo = findTextNodeAtPosition(pElement, termEndPos);
        if (!foundInfo) return false;
        var termEndNode = foundInfo.node;
        var termEndOffset = foundInfo.offset;
        if (hasStrongAncestor(termEndNode)) return false;

        var termEndText = termEndNode.nodeValue;
        var beforeTerm = "";
        for (var i = 0; i < termEndOffset; i++) {
            beforeTerm += termEndText.charAt(i);
        }
        var afterTerm = "";
        for (var i = termEndOffset; i < termEndText.length; i++) {
            afterTerm += termEndText.charAt(i);
        }

        var strongEl = document.createElement("STRONG");
        moveNodesToStrong(pElement, termEndNode, beforeTerm, afterTerm, strongEl);
        return true;
    }

    function normalizeTextForCounting(text) {
        var result = "";
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            if (isUnusualSpace(ch)) {
                result += " ";
            } else {
                result += ch;
            }
        }
        return result;
    }

    function findTextNodeAtPosition(rootElement, targetPos) {
        var currentPos = 0;
        function traverse(node) {
            if (node.nodeType == 3) {
                var nodeText = node.nodeValue;
                var normalizedText = normalizeTextForCounting(nodeText);
                var nodeLen = normalizedText.length;
                if (currentPos + nodeLen >= targetPos) {
                    var offset = findRealOffset(nodeText, targetPos - currentPos);
                    return { node: node, offset: offset };
                }
                currentPos += nodeLen;
                return null;
            } else if (node.nodeType == 1) {
                for (var i = 0; i < node.childNodes.length; i++) {
                    var result = traverse(node.childNodes[i]);
                    if (result) return result;
                }
            }
            return null;
        }
        return traverse(rootElement);
    }

    function findRealOffset(originalText, normalizedPos) {
        var normPos = 0;
        for (var i = 0; i < originalText.length; i++) {
            if (normPos >= normalizedPos) return i;
            normPos++;
        }
        return originalText.length;
    }

    function moveNodesToStrong(pElement, termEndNode, beforeTerm, afterTerm, strongEl) {
        var reachedEnd = false;
        var nodesToRemove = [];

        function processNode(node) {
            if (reachedEnd) return;
            if (node.nodeType == 3) {
                if (node === termEndNode) {
                    reachedEnd = true;
                    if (beforeTerm.length > 0) {
                        strongEl.appendChild(document.createTextNode(beforeTerm));
                    }
                    node.nodeValue = afterTerm;
                    return;
                }
                strongEl.appendChild(document.createTextNode(node.nodeValue));
                nodesToRemove.push(node);
            } else if (node.nodeType == 1) {
                if (containsNodeDescendant(node, termEndNode)) {
                    var clone = node.cloneNode(false);
                    processChildNodes(node, clone);
                    if (clone.childNodes.length > 0) {
                        strongEl.appendChild(clone);
                    }
                    reachedEnd = true;
                } else {
                    strongEl.appendChild(node.cloneNode(true));
                    nodesToRemove.push(node);
                }
            }
        }

        function processChildNodes(originalParent, cloneParent) {
            for (var i = 0; i < originalParent.childNodes.length; i++) {
                if (reachedEnd) break;
                var child = originalParent.childNodes[i];
                if (child.nodeType == 3) {
                    if (child === termEndNode) {
                        reachedEnd = true;
                        if (beforeTerm.length > 0) {
                            cloneParent.appendChild(document.createTextNode(beforeTerm));
                        }
                        child.nodeValue = afterTerm;
                        return;
                    }
                    cloneParent.appendChild(document.createTextNode(child.nodeValue));
                    nodesToRemove.push(child);
                } else if (child.nodeType == 1) {
                    if (containsNodeDescendant(child, termEndNode)) {
                        var subClone = child.cloneNode(false);
                        processChildNodes(child, subClone);
                        if (subClone.childNodes.length > 0) {
                            cloneParent.appendChild(subClone);
                        }
                        reachedEnd = true;
                    } else {
                        cloneParent.appendChild(child.cloneNode(true));
                        nodesToRemove.push(child);
                    }
                }
            }
        }

        function containsNodeDescendant(container, target) {
            if (container === target) return true;
            if (container.nodeType == 1) {
                for (var i = 0; i < container.childNodes.length; i++) {
                    if (containsNodeDescendant(container.childNodes[i], target)) return true;
                }
            }
            return false;
        }

        for (var i = 0; i < pElement.childNodes.length; i++) {
            if (reachedEnd) break;
            processNode(pElement.childNodes[i]);
        }

        for (var i = nodesToRemove.length - 1; i >= 0; i--) {
            var node = nodesToRemove[i];
            if (node.parentNode) {
                node.parentNode.removeChild(node);
            }
        }

        if (strongEl.childNodes.length > 0) {
            var firstChild = pElement.firstChild;
            if (firstChild) {
                pElement.insertBefore(strongEl, firstChild);
            } else {
                pElement.appendChild(strongEl);
            }
        }
    }

    function getNextP(el) {
        function getNextNode(node) {
            if (node.firstChild && node.nodeName != "P") return node.firstChild;
            while (!node.nextSibling) {
                node = node.parentNode;
                if (!node) return null;
            }
            return node.nextSibling;
        }
        var savedEl = el;
        while (el && (el.nodeName != "P" || el === savedEl)) {
            el = getNextNode(el);
        }
        return el;
    }

    // ==================================================
    // СОХРАНЕНИЕ И ВОССТАНОВЛЕНИЕ ПОЗИЦИИ
    // ==================================================

    function saveCursorPosition() {
        var sel2 = document.selection;
        if (sel2 && sel2.type && sel2.type == "Text") {
            var rng = sel2.createRange();
            if (rng) {
                return { range: rng.duplicate(), saved: true };
            }
        }
        return { saved: false };
    }

    function restoreCursorPosition(saved) {
        if (saved && saved.saved && saved.range) {
            try {
                saved.range.select();
            } catch(e) {
                // Игнорируем
            }
        }
    }

    // ==================================================
    // ОСНОВНАЯ ЛОГИКА
    // ==================================================

    var sel = document.selection;
    var hasSelection = false;

    if (sel && sel.type && sel.type == "Text") {
        var selRange = sel.createRange();
        if (selRange && selRange.parentElement) {
            var parentEl = selRange.parentElement();
            if (parentEl && parentEl.nodeName != "TEXTAREA" && parentEl.nodeName != "INPUT") {
                if (selRange.htmlText && selRange.htmlText.length > 0) {
                    hasSelection = true;
                }
            }
        }
    }

    if (!hasSelection && sel && sel.type && sel.type == "Control") {
        var controlRange = sel.createRange();
        if (controlRange && controlRange.length > 0) {
            hasSelection = true;
        }
    }

    if (!hasSelection) {
        MsgBox(scriptName + "\nver. " + version +
               "\n---------------------------\n\n" +
               "Нет выделения.\n\n" +
               "Перед запуском скрипта нужно выделить фрагмент текста.");
        return;
    }

    // Сохраняем позицию курсора
    var savedCursor = saveCursorPosition();

    var markerTagName = "B";
    var rndm = Math.round(Math.random() * 100000).toString();
    var startId = "BlockStart" + rndm;
    var endId = "BlockEnd" + rndm;

    var tr = document.selection.createRange();
    var tr2 = tr.duplicate();
    tr.collapse();
    tr.pasteHTML("<" + markerTagName + " id=" + startId + "></" + markerTagName + ">");
    tr2.collapse(false);
    tr2.pasteHTML("<" + markerTagName + " id=" + endId + "></" + markerTagName + ">");

    var blockStartNode = document.getElementById(startId);
    var blockEndNode = document.getElementById(endId);

    var blockStartEl = blockStartNode;
    while (blockStartEl && blockStartEl.nodeName != "BODY" && blockStartEl.nodeName != "P") {
        blockStartEl = blockStartEl.parentNode;
    }

    var blockEndEl = blockEndNode;
    while (blockEndEl && blockEndEl.nodeName != "BODY" && blockEndEl.nodeName != "P") {
        blockEndEl = blockEndEl.parentNode;
    }
    if (blockEndEl && blockEndEl.nodeName == "BODY") {
        blockEndEl = blockEndNode;
        if (blockEndEl.previousSibling && blockEndEl.previousSibling.nodeName == "P") {
            blockEndEl = blockEndEl.previousSibling;
        }
    }

    var selectionParagraphs = [];
    if (blockStartEl && blockStartEl.nodeName == "P" &&
        blockEndEl && blockEndEl.nodeName == "P") {
        var ptr = blockStartEl;
        while (ptr) {
            if (ptr.nodeName == "P") {
                selectionParagraphs.push(ptr);
            }
            if (ptr === blockEndEl) break;
            ptr = getNextP(ptr);
        }
    }

    var startNode = document.getElementById(startId);
    var endNode = document.getElementById(endId);
    if (startNode) startNode.removeNode(true);
    if (endNode) endNode.removeNode(true);

    // Восстанавливаем позицию
    restoreCursorPosition(savedCursor);

    if (selectionParagraphs.length == 0) {
        MsgBox(scriptName + "\nver. " + version +
               "\n---------------------------\n\n" +
               "В выделении не найдено абзацев.");
        return;
    }

    // ==================================================
    // АНАЛИЗ АБЗАЦЕВ
    // ==================================================

    var candidates = [];
    for (var i = 0; i < selectionParagraphs.length; i++) {
        var p = selectionParagraphs[i];
        if (isSubtitle(p)) continue;
        if (isInsideExcludedDiv(p)) continue;

        var plainText = getPlainText(p);
        var termInfo = findTermCandidate(plainText);

        if (termInfo.found) {
            var firstTextNode = null;
            function findFirstText(node) {
                if (firstTextNode) return;
                if (node.nodeType == 3) {
                    firstTextNode = node;
                    return;
                }
                if (node.nodeType == 1) {
                    for (var j = 0; j < node.childNodes.length; j++) {
                        findFirstText(node.childNodes[j]);
                    }
                }
            }
            findFirstText(p);
            if (firstTextNode && hasStrongAncestor(firstTextNode)) {
                continue;
            }
            candidates.push({
                paragraph: p,
                plainText: plainText,
                termEndPos: termInfo.termEndPos,
                type: termInfo.type
            });
        }
    }

    // ==================================================
    // СТАТИСТИКА И ЗАПУСК
    // ==================================================

    if (candidates.length == 0) {
        if (showStatistics >= 1) {
            MsgBox(scriptName + "\nver. " + version +
                   "\n---------------------------\n\n" +
                   "В выделенном фрагменте не найдено подходящих терминов.");
        }
        return;
    }

    if (showStatistics >= 1) {
        var confirmMsg = "В выделенном фрагменте найдено терминов: " +
                         candidates.length + "\n\n" +
                         "Будет применено форматирование: жирность (STRONG)\n\n" +
                         "Продолжить?";
        if (!AskYesNo(scriptName + "\nver. " + version +
                      "\n---------------------------\n\n" + confirmMsg)) {
            return;
        }
    }

    var startTime = new Date();
    window.external.BeginUndoUnit(document, scriptName + " " + version);

    var formattedCount = 0;
    for (var i = 0; i < candidates.length; i++) {
        var result = formatTermInParagraph(candidates[i].paragraph, candidates[i].termEndPos);
        if (result) {
            formattedCount++;
        }
    }

    window.external.EndUndoUnit(document);

    var endTime = new Date();
    var timeDiff = (endTime - startTime) / 1000;
    var timeStr = timeDiff.toFixed(3);
    timeStr = timeStr.replace(/\./g, ",");

    if (showStatistics >= 1) {
        var resultMessage = "✓ Проанализировано абзацев: " +
                            selectionParagraphs.length + "\n" +
                            "  • Найдено терминов: " + candidates.length + "\n" +
                            "  • Отформатировано жирным: " + formattedCount + "\n\n" +
                            "✓ Время выполнения: " + timeStr + " сек";
        MsgBox(scriptName + "\nver. " + version +
               "\n---------------------------\n\n" + resultMessage);
    }
}
