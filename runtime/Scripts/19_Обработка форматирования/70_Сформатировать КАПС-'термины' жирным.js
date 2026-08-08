// Скрипт "Сформатировать КАПС-термины жирным" для редактора FBE
// version 3.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для форматирования тэгами жирности ЗАГЛАВНЫХ "терминов" в fb2 документах.
// "Термины" должны быть расположены в началах абзацев, как в словарях.
// "Термины" с цифрами в начале абзаца: (9 МАЯ 1945 ГОДА - день Победы) также обрабатываются.
// "Термины" всегда состоят из ЗАГЛАВНЫХ слов и могут содержать в себе цифры и знаки препинания.
// Разделителем термина и его пояснения служит пробел после правой границы КАПС.
// На правой границе КАПС термина могут быть любые "прилипшие" к КАПС знаки препинания.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// Скрипт может также иногда применяться для оформления имен персонажей в пьесах, драмах,
// когда имена персонажей набраны ЗАГЛАВНЫМИ буквами.

// version 3.1, 19.05.2026
// ======================================

function Run() {
    var scriptName = "Сформатировать КАПС-термины жирным";
    var version = "3.1";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА
    // ==================================================

    // 0 - нет или выключено, 1 - да или включено

    // Показывать статистику: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима

    // Обрабатывать текст внутри цитат: 0 - нет, 1 - да
    var processCites = 1;

    // Минимальное кол-во заглавных букв подряд в "термине":
    var MinCapitalLetters = 2;

    // Минимальное кол-во обычных строчных букв в "пояснении термина":
    var MinSmallLetters = 5;

    // Допустимые знаки препинания внутри или на границе "термина":
    var capsTrailingPunctuation = ",.!?;…'\"»'`" + String.fromCharCode(0x60);

    // Максимальное кол-во заглавных букв подряд в "термине":
    var maxCapsTermLength = 50;

    // Обрабатывать текст в диалогах (тире или дефис с пробелами в начале абзаца): 0 - нет, 1 - да 
    var processDialogs = 0;

    // ==================================================
    // НАЧАЛО СКРИПТА
    // ==================================================

    var nbspEntity = "&nbsp;";
    var nbspChar = " ";
    try { nbspChar = window.external.GetNBSP(); if (nbspChar.charCodeAt(0) == 160) { nbspEntity = "&nbsp;"; } else { nbspEntity = nbspChar; } } catch(e) { nbspChar = String.fromCharCode(160); nbspEntity = "&nbsp;"; }

    var unusualSpaces = String.fromCharCode(160) + String.fromCharCode(8194) + String.fromCharCode(8195) +
        String.fromCharCode(8196) + String.fromCharCode(8197) + String.fromCharCode(8198) +
        String.fromCharCode(8239) + String.fromCharCode(8201) + String.fromCharCode(8202) + nbspChar;

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

    function isLetter(ch) {
        if (ch >= 'а' && ch <= 'я') return true;
        if (ch >= 'А' && ch <= 'Я') return true;
        if (ch == 'Ё' || ch == 'ё') return true;
        if (ch >= 'a' && ch <= 'z') return true;
        if (ch >= 'A' && ch <= 'Z') return true;
        var code = ch.charCodeAt(0);
        if (code >= 0x0100 && code <= 0x017F) return true;
        if (code >= 0x0180 && code <= 0x024F) return true;
        if (code == 0x00DF) return true;
        if (code == 0x00C4 || code == 0x00D6 || code == 0x00DC) return true;
        if (code == 0x00E4 || code == 0x00F6 || code == 0x00FC) return true;
        if (code == 0x04AE || code == 0x04AF) return true;
        if (code == 0x04D8 || code == 0x04D9) return true;
        if (code == 0x04E8 || code == 0x04E9) return true;
        if (code == 0x04A2 || code == 0x04A3) return true;
        if (code == 0x0496 || code == 0x0497) return true;
        if (code == 0x049A || code == 0x049B) return true;
        if (code == 0x0492 || code == 0x0493) return true;
        if (code == 0x04B0 || code == 0x04B1) return true;
        if (code == 0x04BA || code == 0x04BB) return true;
        if (code == 0x0406 || code == 0x0456) return true;
        if (code == 0x040E || code == 0x045E) return true;
        if (code == 0x0490 || code == 0x0491) return true;
        if (code == 0x0404 || code == 0x0454) return true;
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

    function isDigit(ch) {
        return (ch >= '0' && ch <= '9');
    }

    function isSpace(ch) {
        return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || isUnusualSpace(ch);
    }

    function isDash(ch) {
        var dashes = "-–—―−";
        for (var i = 0; i < dashes.length; i++) {
            if (ch == dashes.charAt(i)) return true;
        }
        return false;
    }

    function isQuote(ch) {
        if (ch == '"' || ch == '\u00AB' || ch == '\u00BB' || ch == '\u201C' ||
            ch == '\u201D' || ch == '\u201E' || ch == '\u2039' || ch == '\u203A') return true;
        return false;
    }

    function isAllowedPunctuationInCaps(ch) {
        if (ch == ',' || ch == '.' || ch == ';') return true;
        if (ch == "'" || ch == "`" || ch == String.fromCharCode(0x60)) return true;
        if (isQuote(ch)) return true;
        if (ch == '(' || ch == ')' || ch == '[' || ch == ']') return true;
        if (ch == '!' || ch == '?') return true;
        if (ch == '…') return true;
        if (ch == '\u2116' || ch == '%' || ch == '&') return true;
        return false;
    }

    function isTrailingPunctuation(ch) {
        for (var i = 0; i < capsTrailingPunctuation.length; i++) {
            if (ch == capsTrailingPunctuation.charAt(i)) return true;
        }
        return false;
    }

    function isAllCaps(text) {
        var hasLetters = false;
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            if (isLowercase(ch)) return false;
            if (isLetter(ch)) hasLetters = true;
        }
        return hasLetters;
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

    function countLetters(text) {
        var count = 0;
        for (var i = 0; i < text.length; i++) {
            if (isLetter(text.charAt(i))) count++;
        }
        return count;
    }

    function countLowercase(text) {
        var count = 0;
        for (var i = 0; i < text.length; i++) {
            if (isLowercase(text.charAt(i))) count++;
        }
        return count;
    }

    function isFullCapsWord(word) {
        if (word.length == 0) return false;
        var hasLetter = false;
        for (var i = 0; i < word.length; i++) {
            var ch = word.charAt(i);
            if (isLowercase(ch)) return false;
            if (isUppercase(ch)) hasLetter = true;
        }
        return hasLetter;
    }

    function isNextWordFullCaps(text, startPos) {
        var len = text.length;
        var pos = startPos;
        var hasLetter = false;
        while (pos < len) {
            var ch = text.charAt(pos);
            if (isLowercase(ch)) return false;
            if (isUppercase(ch)) hasLetter = true;
            if (isSpace(ch) || isDash(ch) || ch == ',' || ch == '.' || ch == ';' ||
                ch == '!' || ch == '?' || ch == ':' || ch == ')' || ch == '(' ||
                isQuote(ch) || isTrailingPunctuation(ch)) {
                break;
            }
            pos++;
        }
        return hasLetter;
    }

    // Считает количество заглавных букв в слове
    function countCapsInWord(word) {
        var count = 0;
        for (var i = 0; i < word.length; i++) {
            if (isUppercase(word.charAt(i))) count++;
        }
        return count;
    }

    function checkCapsParens(text, parenStart) {
        var len = text.length;
        var ch = text.charAt(parenStart);
        var quoteBefore = false;
        var actualParenStart = parenStart;

        if (isQuote(ch)) {
            if (parenStart + 1 < len && text.charAt(parenStart + 1) == '(') {
                quoteBefore = true;
                actualParenStart = parenStart + 1;
            } else {
                return -1;
            }
        } else if (ch != '(') {
            return -1;
        }

        var closePos = -1;
        var hasClosingQuote = false;
        for (var j = actualParenStart + 1; j < len; j++) {
            if (text.charAt(j) == ')') {
                closePos = j;
                if (quoteBefore && j + 1 < len && isQuote(text.charAt(j + 1))) {
                    hasClosingQuote = true;
                    closePos = j + 1;
                }
                break;
            }
        }
        if (closePos == -1) return -1;
        if (closePos <= actualParenStart + 1) return -1;

        var insideStart = actualParenStart + 1;
        var insideEnd = hasClosingQuote ? closePos - 1 : closePos;
        var inside = "";
        for (var k = insideStart; k < insideEnd; k++) {
            inside += text.charAt(k);
        }

        var words = [];
        var currentWord = "";
        for (var w = 0; w < inside.length; w++) {
            var wch = inside.charAt(w);
            if (isSpace(wch) || wch == ',' || wch == '.' || wch == ';' || wch == '!' || wch == '?') {
                if (currentWord.length > 0) {
                    words.push(currentWord);
                    currentWord = "";
                }
            } else if (isLetter(wch) || isDigit(wch) || wch == "'" || wch == "`" ||
                       wch == String.fromCharCode(0x60) || isDash(wch) || isQuote(wch)) {
                currentWord += wch;
            } else {
                return -1;
            }
        }
        if (currentWord.length > 0) {
            words.push(currentWord);
        }

        var hasCapsWord = false;
        var totalCaps = 0;
        for (var wi = 0; wi < words.length; wi++) {
            var word = words[wi];
            var cleanWord = word;
            if (cleanWord.length >= 2 && isQuote(cleanWord.charAt(0)) &&
                isQuote(cleanWord.charAt(cleanWord.length - 1))) {
                cleanWord = cleanWord.substring(1, cleanWord.length - 1);
            }
            var hasLetter = false;
            for (var ci = 0; ci < cleanWord.length; ci++) {
                if (isLetter(cleanWord.charAt(ci))) hasLetter = true;
            }
            if (!hasLetter) continue;
            if (!isFullCapsWord(cleanWord)) return -1;
            hasCapsWord = true;
            totalCaps += countCapsInWord(cleanWord);
        }

        if (!hasCapsWord) return -1;

        // Возвращаем объект с позицией и количеством заглавных букв
        return { endPos: closePos + 1, capsCount: totalCaps };
    }

    function trimStrRight(str) {
        var end = str.length;
        while (end > 0 && isSpace(str.charAt(end - 1))) {
            end--;
        }
        var result = "";
        for (var i = 0; i < end; i++) {
            result += str.charAt(i);
        }
        return result;
    }

    // ==================================================
    // ПОИСК КАПС-ТЕРМИНА
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
            if (isDash(firstCh)) {
                if (startPos + 1 < len && isSpace(text.charAt(startPos + 1))) {
                    return { found: false, termEndPos: 0, type: "" };
                }
            }
        }

        var capsCount = 0;
        var capsEnd = startPos;
        var lastCapsWordEnd = startPos;

        while (capsEnd < len) {
            var ch = text.charAt(capsEnd);

            if (isUppercase(ch)) {
                capsCount++;
                lastCapsWordEnd = capsEnd;
                capsEnd++;
            } else if (isDigit(ch)) {
                // Цифра — продолжаем, не увеличиваем capsCount
                lastCapsWordEnd = capsEnd;
                capsEnd++;
            } else if (isSpace(ch)) {
                var spaceStart = capsEnd;
                capsEnd++;
                while (capsEnd < len && isSpace(text.charAt(capsEnd))) {
                    capsEnd++;
                }

                if (capsEnd < len) {
                    var nextCh = text.charAt(capsEnd);

                    if (isUppercase(nextCh)) {
                        if (isNextWordFullCaps(text, capsEnd)) {
                            // Считаем заглавные в этом слове
                            var wordStart = capsEnd;
                            while (capsEnd < len && isUppercase(text.charAt(capsEnd))) {
                                capsCount++;
                                lastCapsWordEnd = capsEnd;
                                capsEnd++;
                            }
                        } else {
                            capsEnd = lastCapsWordEnd + 1;
                            while (capsEnd < len && isTrailingPunctuation(text.charAt(capsEnd)) &&
                                   !isSpace(text.charAt(capsEnd))) {
                                capsEnd++;
                            }
                            if (capsEnd - 1 > lastCapsWordEnd) {
                                lastCapsWordEnd = capsEnd - 1;
                            }
                            break;
                        }
                    } else if (isDigit(nextCh)) {
                        var savedCapsEnd = capsEnd;
                        lastCapsWordEnd = capsEnd;
                        capsEnd++;
                        while (capsEnd < len && isDigit(text.charAt(capsEnd))) {
                            lastCapsWordEnd = capsEnd;
                            capsEnd++;
                        }
                        // После цифр должен быть пробел и КАПС-слово
                        if (capsEnd < len && isSpace(text.charAt(capsEnd))) {
                            var afterDigitsSpace = capsEnd;
                            while (afterDigitsSpace < len && isSpace(text.charAt(afterDigitsSpace))) {
                                afterDigitsSpace++;
                            }
                            if (afterDigitsSpace < len && isUppercase(text.charAt(afterDigitsSpace)) &&
                                isNextWordFullCaps(text, afterDigitsSpace)) {
                                capsEnd = afterDigitsSpace;
                            } else {
                                capsEnd = savedCapsEnd;
                                break;
                            }
                        } else {
                            capsEnd = savedCapsEnd;
                            break;
                        }
                    } else if (nextCh == '(' || (isQuote(nextCh) && capsEnd + 1 < len &&
                                                 text.charAt(capsEnd + 1) == '(')) {
                        var parenResult = checkCapsParens(text, capsEnd);
                        if (parenResult && parenResult.endPos > 0) {
                            capsCount += parenResult.capsCount;
                            capsEnd = parenResult.endPos;
                            lastCapsWordEnd = capsEnd - 1;

                            while (capsEnd < len && isTrailingPunctuation(text.charAt(capsEnd)) &&
                                   !isSpace(text.charAt(capsEnd))) {
                                capsEnd++;
                                lastCapsWordEnd = capsEnd - 1;
                            }

                            // После скобок может быть пробел и ещё КАПС-слова
                            if (capsEnd < len && isSpace(text.charAt(capsEnd))) {
                                var afterParenSpace = capsEnd;
                                while (afterParenSpace < len && isSpace(text.charAt(afterParenSpace))) {
                                    afterParenSpace++;
                                }
                                if (afterParenSpace < len &&
                                    (isUppercase(text.charAt(afterParenSpace)) ||
                                     isDigit(text.charAt(afterParenSpace)))) {
                                    capsEnd = afterParenSpace;
                                } else {
                                    break;
                                }
                            } else if (capsEnd < len && !isUppercase(text.charAt(capsEnd)) &&
                                       !isDigit(text.charAt(capsEnd))) {
                                break;
                            }
                        } else {
                            capsEnd = spaceStart;
                            break;
                        }
                    } else {
                        capsEnd = lastCapsWordEnd + 1;
                        while (capsEnd < len && isTrailingPunctuation(text.charAt(capsEnd)) &&
                               !isSpace(text.charAt(capsEnd))) {
                            capsEnd++;
                        }
                        if (capsEnd - 1 > lastCapsWordEnd) {
                            lastCapsWordEnd = capsEnd - 1;
                        }
                        break;
                    }
                }
            } else if (isDash(ch)) {
                if (capsEnd + 1 < len && (isUppercase(text.charAt(capsEnd + 1)) ||
                                          isDigit(text.charAt(capsEnd + 1)))) {
                    capsEnd++;
                } else {
                    break;
                }
            } else if (isAllowedPunctuationInCaps(ch)) {
                capsEnd++;
            } else if (isTrailingPunctuation(ch)) {
                if (capsEnd > 0 && (isUppercase(text.charAt(capsEnd - 1)) ||
                                    isDigit(text.charAt(capsEnd - 1)) ||
                                    isTrailingPunctuation(text.charAt(capsEnd - 1)))) {
                    capsEnd++;
                    lastCapsWordEnd = capsEnd - 1;
                }
                break;
            } else if (isLowercase(ch)) {
                break;
            } else {
                break;
            }
        }

        var finalEnd = capsEnd;
        var trailingStart = lastCapsWordEnd + 1;
        while (trailingStart < len && isTrailingPunctuation(text.charAt(trailingStart)) &&
               !isSpace(text.charAt(trailingStart))) {
            trailingStart++;
        }
        if (trailingStart > finalEnd) {
            finalEnd = trailingStart;
        }

        if (capsCount >= MinCapitalLetters) {
            var remainingText = "";
            for (var i = finalEnd; i < len; i++) {
                remainingText += text.charAt(i);
            }
            var lowerInRemaining = countLowercase(remainingText);

            if (lowerInRemaining >= MinSmallLetters) {
                if (!isAllCaps(text)) {
                    var cleanEnd = finalEnd;
                    while (cleanEnd > startPos && isSpace(text.charAt(cleanEnd - 1))) {
                        cleanEnd--;
                    }
                    var capsTermLength = cleanEnd - startPos;
                    if (capsTermLength <= maxCapsTermLength) {
                        return { found: true, termEndPos: cleanEnd, type: "caps" };
                    }
                }
            }
        }

        return { found: false, termEndPos: 0, type: "" };
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

    if (blockStartEl && blockEndEl) {
        var tr1 = document.body.createTextRange();
        tr1.moveToElementText(blockStartEl);
        if (tr1.moveStart("character", 1) == 1) tr1.moveStart("character", -1);
        var tr2b = document.body.createTextRange();
        tr2b.moveToElementText(blockEndEl);
        tr1.setEndPoint("EndToEnd", tr2b);
        tr1.select();
    }

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
