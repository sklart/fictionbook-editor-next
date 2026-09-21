// =============================================
// Скрипт: «Разбить выделенные абзацы по регекспу»
// Версия: 1.0 (2026, сентябрь)
// Сборка — stokber + помощь DeepSeek.
// В сценарии использован алгоритм сохранения форматирования при разбивке абзацев из скрипта ув. TaKir-a «Разделить несколько слипшихся абзацев стиха на строки»
// Описание: Разбивает выделенные абзацы в документе FB2
//           перед каждым совпадением с регулярным выражением.
//           Сохраняет форматирование, включая атрибуты тегов.
//           Учитывает теги A: разбивка в начале ссылки выполняется всегда,
//           в середине – по настройке SPLIT_INSIDE_LINKS.
//           После выполнения курсор устанавливается в начало
//           первого обработанного абзаца, а сам абзац располагается
//           примерно на три строки ниже верха окна редактора.

// ===========================================

// ===================== НАСТРОЙКИ =====================
var SCRIPT_NAME = "Разбить выделенные абзацы по регекспу";
var SCRIPT_VERSION = "1.0";
var MARKER = "§%§";

// Поведение при разбивке внутри ссылок/сносок (тег A):
// "0" - не разбивать (кроме начал ссылок)
// "1" - разбивать всегда (включая середины)
// "2" - спросить у пользователя 
var SPLIT_INSIDE_LINKS = "1";

// Отступ сверху для первого обработанного абзаца (в пикселях).
// Приблизительно 3 строки текста.
var TOP_OFFSET_PIXELS = 66;

// ===================== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ =====================

// Поиск элемента в массиве (IE6-совместимый)
function arrayContains(arr, item) {
    if (!arr) return -1;
    for (var i = 0; i < arr.length; i++) {
        if (arr[i] == item) return i;
    }
    return -1;
}

function isWhitespace(currentChar) {
    return currentChar == ' ' || currentChar == '\r' || currentChar == '\n';
}

function trimString(str) {
    if (!str) return "";
    return str.replace(/^\s+|\s+$/g, '');
}

// Получаем символ неразрывного пробела из настроек FBE
var nbspEntity = "&nbsp;";
try {
    var nbspChar = window.external.GetNBSP();
    if (nbspChar.charCodeAt(0) != 160)
        nbspEntity = nbspChar;
} catch (e) {
    nbspChar = String.fromCharCode(160);
}

// Нормализация текста для анализа
function normalizeTextForAnalysis(text) {
    if (!text) return "";
    var normalized = text;
    normalized = normalized.replace(/&nbsp;/g, " ");
    normalized = normalized.replace(/&lt;/g, " ");
    normalized = normalized.replace(/&gt;/g, " ");
    normalized = normalized.replace(/&amp;/g, " ");
    normalized = normalized.replace(/&quot;/g, " ");
    normalized = normalized.replace(/&apos;/g, " ");
    normalized = normalized.replace(/&shy;/g, "");
    if (nbspEntity.length == 1) {
        var regex = new RegExp(nbspEntity, "g");
        normalized = normalized.replace(regex, " ");
    } else {
        regex = new RegExp(nbspEntity, "g");
        normalized = normalized.replace(regex, " ");
    }
    normalized = normalized.replace(/□/g, " ");
    normalized = normalized.replace(/▫/g, " ");
    normalized = normalized.replace(/◦/g, " ");
    normalized = normalized.replace(/\xA0/g, " ");
    return normalized;
}

// Получение чистого текста из элемента
function getPlainTextFromElement(element) {
    var text = "";

    function walk(node) {
        if (node.nodeType == 3) {
            text += normalizeTextForAnalysis(node.nodeValue);
        } else if (node.nodeType == 1) {
            if (node.nodeName == "A") {
                var href = node.getAttribute("l:href") || node.getAttribute("href") || "";
                if (href.indexOf("#n_") == 0 || href.indexOf("#_") == 0) {
                    return;
                }
            }
            for (var i = 0; i < node.childNodes.length; i++) {
                walk(node.childNodes[i]);
            }
        }
    }
    walk(element);
    return text;
}

// Сопоставление позиций между исходным HTML и нормализованным текстом
function getPositionMapping(html, normalizedText) {
    var mapping = [];
    var textIndex = 0;
    var htmlIndex = 0;
    var htmlEntities = ["&nbsp;", "&lt;", "&gt;", "&amp;", "&quot;", "&apos;", "&shy;"];
    var entityLengths = [1, 1, 1, 1, 1, 1, 0];
    var fbeSpaces = ["□", "▫", "◦"];
    if (nbspEntity.length == 1 && arrayContains(fbeSpaces, nbspEntity) == -1) {
        fbeSpaces[fbeSpaces.length] = nbspEntity;
    }
    while (htmlIndex < html.length && textIndex <= normalizedText.length) {
        var currentChar = html.charAt(htmlIndex);
        if (currentChar == '<') {
            var tagEnd = -1;
            for (var k = htmlIndex; k < html.length; k++) {
                if (html.charAt(k) == '>') {
                    tagEnd = k;
                    break;
                }
            }
            if (tagEnd != -1) {
                htmlIndex = tagEnd + 1;
                continue;
            } else {
                htmlIndex++;
                continue;
            }
        }
        if (currentChar == '&') {
            var foundEntity = false;
            for (var e = 0; e < htmlEntities.length; e++) {
                var entity = htmlEntities[e];
                var isEntity = true;
                for (var m = 0; m < entity.length; m++) {
                    if (htmlIndex + m >= html.length || html.charAt(htmlIndex + m) != entity.charAt(m)) {
                        isEntity = false;
                        break;
                    }
                }
                if (isEntity) {
                    if (entityLengths[e] > 0) {
                        mapping[mapping.length] = {
                            htmlPos: htmlIndex,
                            textPos: textIndex,
                            entityLength: entity.length,
                            normalizedLength: entityLengths[e]
                        };
                        textIndex += entityLengths[e];
                    }
                    htmlIndex += entity.length;
                    foundEntity = true;
                    break;
                }
            }
            if (foundEntity) continue;
            if (html.charAt(htmlIndex + 1) == '#') {
                var semicolonPos = -1;
                for (k = htmlIndex; k < html.length; k++) {
                    if (html.charAt(k) == ';') {
                        semicolonPos = k;
                        break;
                    }
                }
                if (semicolonPos != -1) {
                    mapping[mapping.length] = {
                        htmlPos: htmlIndex,
                        textPos: textIndex,
                        entityLength: semicolonPos - htmlIndex + 1,
                        normalizedLength: 1
                    };
                    textIndex++;
                    htmlIndex = semicolonPos + 1;
                    continue;
                }
            }
        }
        var isFbeSpace = false;
        for (var s = 0; s < fbeSpaces.length; s++) {
            if (currentChar == fbeSpaces[s]) {
                isFbeSpace = true;
                break;
            }
        }
        if (isFbeSpace || currentChar.charCodeAt(0) == 160) {
            mapping[mapping.length] = {
                htmlPos: htmlIndex,
                textPos: textIndex,
                entityLength: 1,
                normalizedLength: 1
            };
            textIndex++;
            htmlIndex++;
            continue;
        }
        if (textIndex < normalizedText.length) {
            mapping[mapping.length] = {
                htmlPos: htmlIndex,
                textPos: textIndex,
                entityLength: 1,
                normalizedLength: 1
            };
            textIndex++;
        }
        htmlIndex++;
    }
    return mapping;
}

// Конвертация позиции из нормализованного текста в HTML
function convertTextPosToHtmlPos(textPos, mapping) {
    if (!mapping || mapping.length == 0) return textPos;
    for (i = 0; i < mapping.length; i++) {
        if (mapping[i].textPos == textPos) return mapping[i].htmlPos;
    }
    var bestMatch = -1;
    var bestMatchIndex = -1;
    for (var i = 0; i < mapping.length; i++) {
        if (mapping[i].textPos <= textPos && mapping[i].textPos > bestMatch) {
            bestMatch = mapping[i].textPos;
            bestMatchIndex = i;
        }
    }
    if (bestMatchIndex != -1) {
        var diff = textPos - mapping[bestMatchIndex].textPos;
        if (diff <= 5) return mapping[bestMatchIndex].htmlPos + diff;
    }
    return textPos;
}

// Проверяет, находится ли позиция внутри тега A (гиперссылки или сноски)
function isInsideATag(html, pos) {
    if (pos <= 0 || pos >= html.length) return false;
    var stack = [];
    var i = 0;
    while (i < pos) {
        var ch = html.charAt(i);
        if (ch === '<') {
            var tagEnd = -1;
            for (var k = i; k < html.length; k++) {
                if (html.charAt(k) === '>') {
                    tagEnd = k;
                    break;
                }
            }
            if (tagEnd === -1) break;
            var fullTag = html.substring(i, tagEnd + 1);
            var isClosing = (fullTag.length >= 2 && fullTag.charAt(1) === '/');
            var tagName = '';
            var startName = i + (isClosing ? 2 : 1);
            var endName = startName;
            while (endName < tagEnd && html.charAt(endName) !== ' ' && html.charAt(endName) !== '>' && html.charAt(endName) !== '/') {
                endName++;
            }
            tagName = html.substring(startName, endName).toLowerCase();
            var isSelfClosing = (fullTag.length >= 2 && fullTag.charAt(fullTag.length - 2) === '/');
            if (!isSelfClosing && tagName === 'a') {
                if (!isClosing) {
                    stack[stack.length] = 'a';
                } else {
                    if (stack.length > 0 && stack[stack.length - 1] === 'a') {
                        stack.pop();
                    }
                }
            }
            i = tagEnd + 1;
        } else {
            i++;
        }
    }
    for (var j = stack.length - 1; j >= 0; j--) {
        if (stack[j] === 'a') return true;
    }
    return false;
}

// Возвращает позицию открывающего тега A, в котором находится pos, или -1
function getOpenATagPosition(html, pos) {
    var i = pos;
    while (i > 0) {
        var lt = html.lastIndexOf('<', i - 1);
        if (lt === -1) break;
        var gt = html.indexOf('>', lt);
        if (gt === -1 || gt >= pos) break;
        var fullTag = html.substring(lt, gt + 1);
        var isClosing = (fullTag.length >= 2 && fullTag.charAt(1) === '/');
        var tagName = '';
        var startName = lt + (isClosing ? 2 : 1);
        var endName = startName;
        while (endName < gt && html.charAt(endName) !== ' ' && html.charAt(endName) !== '>' && html.charAt(endName) !== '/') {
            endName++;
        }
        tagName = html.substring(startName, endName).toLowerCase();
        var isSelfClosing = (fullTag.length >= 2 && fullTag.charAt(fullTag.length - 2) === '/');
        if (tagName === 'a' && !isSelfClosing) {
            if (!isClosing) {
                return lt;
            }
        }
        i = lt;
    }
    return -1;
}

// Проверяет, является ли позиция началом содержимого тега A (сразу после открывающего тега, без текста)
function isStartOfATagContent(html, pos) {
    if (!isInsideATag(html, pos)) return false;
    var openPos = getOpenATagPosition(html, pos);
    if (openPos === -1) return false;
    var gt = html.indexOf('>', openPos);
    if (gt === -1) return false;
    var content = html.substring(gt + 1, pos);
    // Удаляем все теги из content
    var text = content.replace(/<[^>]*>/g, '');
    // Проверяем, есть ли буквы/цифры
    var hasText = false;
    for (var i = 0; i < text.length; i++) {
        var ch = text.charAt(i);
        if (ch !== ' ' && ch !== '\r' && ch !== '\n' && ch !== '\t') {
            var code = ch.charCodeAt(0);
            if ((code >= 48 && code <= 57) || (code >= 65 && code <= 90) ||
                (code >= 97 && code <= 122) || (code >= 1040 && code <= 1103)) {
                hasText = true;
                break;
            }
        }
    }
    return !hasText;
}

// Проверяет, можно ли безопасно вставить маркер в позицию htmlPos
function isSafeInsertPosition(html, htmlPos) {
    if (htmlPos <= 0 || htmlPos >= html.length) return true;

    var charBefore = html.charAt(htmlPos - 1);
    var charAfter = html.charAt(htmlPos);

    var beforeTag = false;
    for (var i = htmlPos - 1; i >= 0; i--) {
        var ch = html.charAt(i);
        if (ch == '<') {
            beforeTag = true;
            break;
        }
        if (ch == '>') break;
    }
    var afterTag = false;
    for (i = htmlPos; i < html.length; i++) {
        ch = html.charAt(i);
        if (ch == '>') {
            afterTag = true;
            break;
        }
        if (ch == '<') break;
    }
    if (beforeTag && afterTag) return false;

    var isLetterOrDigit = function(ch) {
        var code = ch.charCodeAt(0);
        return (code >= 48 && code <= 57) ||
            (code >= 65 && code <= 90) ||
            (code >= 97 && code <= 122) ||
            (code >= 1040 && code <= 1103);
    };
    if (isLetterOrDigit(charBefore) && isLetterOrDigit(charAfter)) {
        return false;
    }
    return true;
}

// Вставка маркеров разбивки в HTML по заданным позициям
function insertMarkersWithCorrection(html, normalizedText, positions, marker, allowInsideLinks) {
    if (!html || !normalizedText || positions.length == 0) return {
        html: html,
        skippedInsideA: 0
    };
    var mapping = getPositionMapping(html, normalizedText);
    if (mapping.length == 0) return {
        html: html,
        skippedInsideA: 0
    };
    var result = html;
    var skipped = 0;
    // Сортируем позиции по убыванию
    for (var x = 0; x < positions.length; x++) {
        for (var y = x + 1; y < positions.length; y++) {
            if (positions[x] < positions[y]) {
                var temp = positions[x];
                positions[x] = positions[y];
                positions[y] = temp;
            }
        }
    }
    for (var i = 0; i < positions.length; i++) {
        var textPos = positions[i];
        if (textPos < 0 || textPos > normalizedText.length) continue;
        var htmlPos = convertTextPosToHtmlPos(textPos, mapping);
        if (htmlPos != -1 && htmlPos < result.length) {
            var insideA = isInsideATag(result, htmlPos);
            var isStart = false;
            if (insideA) {
                isStart = isStartOfATagContent(result, htmlPos);
            }
            if (insideA && !allowInsideLinks && !isStart) {
                skipped++;
                continue;
            }
            if (isSafeInsertPosition(result, htmlPos)) {
                result = result.substring(0, htmlPos) + marker + result.substring(htmlPos);
            } else {
                var newPos = htmlPos;
                while (newPos < result.length && !isSafeInsertPosition(result, newPos)) {
                    newPos++;
                }
                if (newPos < result.length) {
                    result = result.substring(0, newPos) + marker + result.substring(newPos);
                }
            }
        }
    }
    return {
        html: result,
        skippedInsideA: skipped
    };
}

// Разбиение HTML по маркеру с сохранением структуры тегов
function splitHTMLByMarkers(html, marker) {
    if (!html || !marker) return [html];
    var parts = [];
    var stack = [];
    var currentPart = '';
    var i = 0;
    var len = html.length;
    var inTag = false;
    var tagName = '';
    var isClosing = false;
    var fullTag = '';

    while (i < len) {
        var currentChar = html.charAt(i);
        if (currentChar === '<') {
            inTag = true;
            tagName = '';
            isClosing = false;
            fullTag = '<';
            i++;
            while (i < len && html.charAt(i) !== '>') {
                fullTag += html.charAt(i);
                if (html.charAt(i) !== ' ' && html.charAt(i) !== '/' && tagName === '') {
                    var ch = html.charAt(i);
                    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch === '-' || ch === ':') {
                        tagName += ch;
                    }
                }
                i++;
            }
            if (i < len && html.charAt(i) === '>') {
                fullTag += '>';
                i++;
                isClosing = (fullTag.length >= 2 && fullTag.charAt(1) === '/');
                tagName = tagName.toLowerCase();
                var isSelfClosing = (fullTag.length >= 2 && fullTag.charAt(fullTag.length - 2) === '/');
                if (!isSelfClosing && tagName !== 'br' && tagName !== 'img' && tagName !== 'hr') {
                    if (isClosing) {
                        for (var j = stack.length - 1; j >= 0; j--) {
                            if (stack[j].tagName === tagName) {
                                stack.splice(j, 1);
                                break;
                            }
                        }
                    } else {
                        stack[stack.length] = {
                            tagName: tagName,
                            fullOpen: fullTag
                        };
                    }
                }
                inTag = false;
                currentPart += fullTag;
                continue;
            } else {
                currentPart += fullTag;
                i++;
                continue;
            }
        } else if (!inTag && currentChar === marker.charAt(0)) {
            var isFullMarker = true;
            for (var m = 0; m < marker.length; m++) {
                if (i + m >= len || html.charAt(i + m) !== marker.charAt(m)) {
                    isFullMarker = false;
                    break;
                }
            }
            if (isFullMarker) {
                if (currentPart.replace(/^\s+|\s+$/g, '').replace(/\s+/g, '') !== '') {
                    var closedPart = currentPart;
                    for (var s = stack.length - 1; s >= 0; s--) {
                        closedPart += '</' + stack[s].tagName + '>';
                    }
                    parts[parts.length] = closedPart;
                    currentPart = '';
                    for (var s2 = 0; s2 < stack.length; s2++) {
                        currentPart += stack[s2].fullOpen;
                    }
                }
                i += marker.length;
                continue;
            } else {
                currentPart += currentChar;
                i++;
            }
        } else {
            currentPart += currentChar;
            i++;
        }
    }
    if (currentPart.replace(/^\s+|\s+$/g, '').replace(/\s+/g, '') !== '') {
        parts[parts.length] = currentPart;
    }
    return parts.length > 0 ? parts : [html];
}

function cleanHTMLPart(html) {
    if (!html) return '';
    var trimmed = html;
    trimmed = trimmed.replace(/^(\s*)([^<])/, '$2');
    trimmed = trimmed.replace(/([^>])(\s*)$/, '$1');
    return trimmed;
}

// Поиск всех абзацев <p> в выделении
function getParagraphsInSelection() {
    var paragraphs = [];
    var selection = document.selection;
    if (!selection) return paragraphs;
    var range = selection.createRange();
    var startRange = range.duplicate();
    startRange.collapse(true);
    var startElement = startRange.parentElement();
    var endRange = range.duplicate();
    endRange.collapse(false);
    var endElement = endRange.parentElement();
    while (startElement && startElement.nodeName != "P" && startElement.nodeName != "BODY") {
        startElement = startElement.parentNode;
    }
    if (!startElement || startElement.nodeName != "P") return paragraphs;
    while (endElement && endElement.nodeName != "P" && endElement.nodeName != "BODY") {
        endElement = endElement.parentNode;
    }
    if (!endElement || endElement.nodeName != "P") endElement = startElement;
    var currentElement = startElement;
    while (currentElement) {
        paragraphs[paragraphs.length] = currentElement;
        if (currentElement == endElement) break;
        var foundNext = false;
        var nextSibling = currentElement.nextSibling;
        while (nextSibling) {
            if (nextSibling.nodeName == "P") {
                currentElement = nextSibling;
                foundNext = true;
                break;
            }
            if (nextSibling.nodeType == 1) {
                var firstParagraph = findFirstParagraphInElement(nextSibling);
                if (firstParagraph) {
                    currentElement = firstParagraph;
                    foundNext = true;
                    break;
                }
            }
            nextSibling = nextSibling.nextSibling;
        }
        if (!foundNext) {
            var parent = currentElement.parentNode;
            var nextInParent = null;
            while (parent && parent.nodeName != "BODY") {
                var tempElement = currentElement;
                nextInParent = null;
                var foundCurrent = false;
                for (var i = 0; i < parent.childNodes.length; i++) {
                    var child = parent.childNodes[i];
                    if (child == tempElement) {
                        foundCurrent = true;
                        continue;
                    }
                    if (foundCurrent) {
                        if (child.nodeName == "P") {
                            nextInParent = child;
                            break;
                        }
                        if (child.nodeType == 1) {
                            firstParagraph = findFirstParagraphInElement(child);
                            if (firstParagraph) {
                                nextInParent = firstParagraph;
                                break;
                            }
                        }
                    }
                }
                if (nextInParent) {
                    currentElement = nextInParent;
                    foundNext = true;
                    break;
                }
                currentElement = parent;
                parent = parent.parentNode;
            }
        }
        if (!foundNext) break;
    }
    return paragraphs;
}

function findFirstParagraphInElement(element) {
    if (element.nodeName == "P") return element;
    for (var i = 0; i < element.childNodes.length; i++) {
        var child = element.childNodes[i];
        if (child.nodeType == 1) {
            var result = findFirstParagraphInElement(child);
            if (result) return result;
        }
    }
    return null;
}

// ===================== ОСНОВНАЯ ЛОГИКА РАЗБИВКИ ПО РЕГУЛЯРНОМУ ВЫРАЖЕНИЮ =====================

// Находит все позиции в нормализованном тексте, перед которыми нужно разбить абзац
function findAllBreakPositionsByRegex(normalizedText, regex) {
    var positions = [];
    regex.lastIndex = 0;
    var match;
    while ((match = regex.exec(normalizedText)) !== null) {
        var pos = match.index;
        if (pos > 0) {
            positions[positions.length] = pos;
        }
        if (!regex.global) break;
        if (match[0].length == 0) regex.lastIndex++;
    }
    return positions;
}

// Обрабатывает один абзац: разбивает на несколько по найденным позициям
function splitParagraphByRegex(paragraph, regex, allowInsideLinks) {
    try {
        var originalHTML = paragraph.innerHTML;
        var normalizedText = getPlainTextFromElement(paragraph);
        var trimmedText = trimString(normalizedText);
        if (trimmedText.length == 0) {
            return {
                success: false,
                parts: null,
                stats: {
                    isEmpty: true
                }
            };
        }
        var breakPositions = findAllBreakPositionsByRegex(normalizedText, regex);
        if (breakPositions.length == 0) {
            return {
                success: false,
                parts: null,
                stats: {
                    noMatches: true
                }
            };
        }
        var result = insertMarkersWithCorrection(originalHTML, normalizedText, breakPositions, MARKER, allowInsideLinks);
        var htmlWithMarkers = result.html;
        var skippedInsideA = result.skippedInsideA;
        if (!htmlWithMarkers || htmlWithMarkers === originalHTML) {
            return {
                success: false,
                parts: null,
                stats: {
                    markerError: true,
                    skippedInsideA: skippedInsideA
                }
            };
        }
        var parts = splitHTMLByMarkers(htmlWithMarkers, MARKER);
        if (parts.length <= 1) {
            return {
                success: false,
                parts: null,
                stats: {
                    noSplit: true,
                    skippedInsideA: skippedInsideA
                }
            };
        }
        var cleanParts = [];
        for (var i = 0; i < parts.length; i++) {
            var part = parts[i].replace(new RegExp(MARKER, 'g'), '');
            var cleanPart = cleanHTMLPart(part);
            if (cleanPart.length > 0) {
                cleanParts[cleanParts.length] = cleanPart;
            }
        }
        if (cleanParts.length <= 1) {
            return {
                success: false,
                parts: null,
                stats: {
                    afterCleanNoSplit: true,
                    skippedInsideA: skippedInsideA
                }
            };
        }
        return {
            success: true,
            parts: cleanParts,
            stats: {
                partsCount: cleanParts.length,
                skippedInsideA: skippedInsideA
            }
        };
    } catch (e) {
        return {
            success: false,
            parts: null,
            stats: {
                error: e.message
            }
        };
    }
}

// Форматирование времени в MM:SS:mmm
function formatTime(milliseconds) {
    var mins = Math.floor(milliseconds / 60000);
    var secs = Math.floor((milliseconds % 60000) / 1000);
    var msecs = milliseconds % 1000;
    var msecStr = (msecs < 100 ? (msecs < 10 ? "00" : "0") : "") + msecs;
    return mins + ":" + secs + ":" + msecStr;
}

// Устанавливает курсор в начало указанного элемента (IE6-совместимо)
function setCursorToElementStart(element) {
    if (!element) return;
    try {
        var rng = document.body.createTextRange();
        rng.moveToElementText(element);
        rng.collapse(true);
        rng.select();
    } catch (e) {
        try {
            element.scrollIntoView(true);
        } catch (e2) {}
    }
}

// Прокручивает окно так, чтобы верх элемента оказался на заданном
// расстоянии (в пикселях) от верхней границы окна просмотра.
function scrollElementToTopWithOffset(element, offsetPixels) {
    if (!element) return;
    try {
        // Сначала прокрутим так, чтобы элемент оказался в видимой области
        element.scrollIntoView(true);

        // Вычисляем абсолютную вертикальную позицию элемента
        var elementTop = 0;
        var el = element;
        while (el) {
            elementTop += el.offsetTop || 0;
            el = el.offsetParent;
        }

        // Целевая позиция прокрутки: верх элемента минус отступ
        var targetScroll = elementTop - offsetPixels;
        if (targetScroll < 0) targetScroll = 0;

        if (document.documentElement && document.documentElement.scrollTop !== undefined) {
            document.documentElement.scrollTop = targetScroll;
        }
        if (document.body && document.body.scrollTop !== undefined) {
            document.body.scrollTop = targetScroll;
        }
    } catch (e) {
        try {
            element.scrollIntoView(true);
        } catch (e2) {}
    }
}

// ===================== ГЛАВНАЯ ФУНКЦИЯ Run =====================

function Run() {
    var startTime = new Date();
    var statsTotal = {
        paragraphsFound: 0,
        processed: 0,
        totalSplits: 0,
        empty: 0,
        noMatches: 0,
        errors: 0,
        skippedInsideA: 0
    };

    // Ссылка на первый обработанный абзац (новый <p>)
    var firstProcessedPara = null;

    window.external.BeginUndoUnit(document, SCRIPT_NAME + " v" + SCRIPT_VERSION);

    try {
        // Запрашиваем регулярное выражение
        var defaultRegex = "\\[\\d+\\]";
        var userRegex = prompt("Введите регулярное выражение для поиска совпадений, перед которыми нужно разбить абзац:", defaultRegex);
        if (userRegex === null) {
            window.external.EndUndoUnit(document);
            return;
        }
        if (userRegex === "") {
            userRegex = defaultRegex;
        }
        var regex;
        try {
            regex = new RegExp(userRegex, "g");
        } catch (e) {
            alert((SCRIPT_NAME + " v" + SCRIPT_VERSION) + ":\nНекорректное регулярное выражение:\n" + e.message);
            window.external.EndUndoUnit(document);
            return;
        }

        // Получаем выделенные абзацы
        var paragraphs = getParagraphsInSelection();
        statsTotal.paragraphsFound = paragraphs.length;
        if (paragraphs.length == 0) {
            alert((SCRIPT_NAME + " v" + SCRIPT_VERSION) + ":\nНет выделенных абзацев <p>. Пожалуйста, выделите текст, содержащий абзацы.");
            window.external.EndUndoUnit(document);
            return;
        }

        // Проверяем, есть ли вообще совпадения в выделенных абзацах
        var anyMatch = false;
        for (var chk = 0; chk < paragraphs.length; chk++) {
            var chkText = getPlainTextFromElement(paragraphs[chk]);
            var chkPositions = findAllBreakPositionsByRegex(chkText, regex);
            if (chkPositions.length > 0) {
                anyMatch = true;
                break;
            }
        }
        if (!anyMatch) {
            alert("В выделенных абзацах совпадений по введённому вами регекспу не найдено.\n\nСкрипт «" +
            SCRIPT_NAME + "» v" + SCRIPT_VERSION + "\n");
            window.external.EndUndoUnit(document);
            return;
        }
		
        // Собираем все позиции разбивки и определяем, есть ли позиции внутри A, не являющиеся началами
        var hasInsideANotStart = false;
        var allowInsideLinks = null;

        for (var idx = 0; idx < paragraphs.length; idx++) {
            var p = paragraphs[idx];
            var normalizedText = getPlainTextFromElement(p);
            var breakPositions = findAllBreakPositionsByRegex(normalizedText, regex);
            if (breakPositions.length > 0) {
                var mapping = getPositionMapping(p.innerHTML, normalizedText);
                if (mapping.length > 0) {
                    for (var b = 0; b < breakPositions.length; b++) {
                        var textPos = breakPositions[b];
                        var htmlPos = convertTextPosToHtmlPos(textPos, mapping);
                        if (htmlPos != -1 && isInsideATag(p.innerHTML, htmlPos)) {
                            if (!isStartOfATagContent(p.innerHTML, htmlPos)) {
                                hasInsideANotStart = true;
                                break;
                            }
                        }
                    }
                }
            }
            if (hasInsideANotStart) break;
        }

        // Определяем allowInsideLinks
        if (hasInsideANotStart) {
            if (SPLIT_INSIDE_LINKS === "0") {
                allowInsideLinks = false;
            } else if (SPLIT_INSIDE_LINKS === "1") {
                allowInsideLinks = true;
            } else { // "2" - спросить
                var answer1 = confirm("Обнаружены места разбивки внутри гиперссылок/сносок (тег A).\nРазбивать их?\n\nНажмите 'ОК' - разбивать, 'Отмена' - не разбивать.");
                allowInsideLinks = answer1;

                var answer2 = confirm("Вы хотите продолжить разбивку или прервать для ручной правки ссылок?\n\nНажмите 'ОК' для продолжения, 'Отмена' для отмены скрипта.");
                if (!answer2) {
                    alert((SCRIPT_NAME + " v" + SCRIPT_VERSION) + ":\nВыполнение скрипта прервано пользователем.\nИзменения не были внесены.");
                    window.external.EndUndoUnit(document);
                    return;
                }
            }
        } else {
            allowInsideLinks = true;
        }

        // Обрабатываем каждый абзац
        for (var i = 0; i < paragraphs.length; i++) {
            p = paragraphs[i];
            var result = splitParagraphByRegex(p, regex, allowInsideLinks);
            if (result.success) {
                var parent = p.parentNode;
                var className = p.className || "";
                var referenceNode = p;
                for (var j = 0; j < result.parts.length; j++) {
                    var newP = document.createElement("P");
                    if (className) newP.className = className;
                    newP.innerHTML = result.parts[j];
                    if (j == 0) {
                        parent.replaceChild(newP, referenceNode);
                    } else {
                        parent.insertBefore(newP, referenceNode.nextSibling);
                    }
                    referenceNode = newP;
                }
                statsTotal.processed++;
                statsTotal.totalSplits += (result.parts.length - 1);
                statsTotal.skippedInsideA += (result.stats && result.stats.skippedInsideA) ? result.stats.skippedInsideA : 0;

                // Определяем первый обработанный абзац (голову первой группы)
                if (firstProcessedPara === null) {
                    var headP = referenceNode; // последний созданный <p> группы
                    for (var k = 1; k < result.parts.length; k++) {
                        if (headP.previousSibling) {
                            headP = headP.previousSibling;
                        } else {
                            break;
                        }
                    }
                    firstProcessedPara = headP;
                }
            } else {
                if (result.stats && result.stats.isEmpty) statsTotal.empty++;
                else if (result.stats && (result.stats.noMatches || result.stats.afterCleanNoSplit || result.stats.noSplit)) statsTotal.noMatches++;
                else statsTotal.errors++;
                if (result.stats && result.stats.skippedInsideA) {
                    statsTotal.skippedInsideA += result.stats.skippedInsideA;
                }
            }
        }

        // Устанавливаем курсор в начало первого обработанного абзаца,
        // а сам абзац располагаем примерно на три строки ниже верха окна.
        if (firstProcessedPara) {
            setCursorToElementStart(firstProcessedPara);
            scrollElementToTopWithOffset(firstProcessedPara, TOP_OFFSET_PIXELS);
        } else {
            // Если ничего не обработано - снимаем выделение
            try {
                var clearRange = document.body.createTextRange();
                clearRange.collapse();
                clearRange.select();
            } catch (e) {}
        }

        // Статистика
        var endTime = new Date();
        var elapsed = endTime - startTime;
        var timeStr = formatTime(elapsed);
        var msg = "\n\n" +
            "Найдено абзацев: " + statsTotal.paragraphsFound + "\n" +
            "Обработано (разбито): " + statsTotal.processed + "\n" +
            "Создано новых абзацев: " + statsTotal.totalSplits + "\n";
        if (statsTotal.skippedInsideA > 0) {
            msg += "Пропущено разбиений внутри ссылок/сносок: " + statsTotal.skippedInsideA + "\n";
        }
        msg += "\n";
        var ignored = statsTotal.empty + statsTotal.noMatches + statsTotal.errors;
        if (ignored > 0) {
            msg += "Пропущено абзацев (не разбиты): " + ignored + "\n";
            if (statsTotal.empty > 0) msg += "  • пустые: " + statsTotal.empty + "\n";
            if (statsTotal.noMatches > 0) msg += "  • нет совпадений: " + statsTotal.noMatches + "\n";
            if (statsTotal.errors > 0) msg += "  • ошибки: " + statsTotal.errors + "\n";
            msg += "\n";
        }
        msg += "Время выполнения: " + timeStr + "\n\n";
        msg += "==========================\n" +
            SCRIPT_NAME + " v" + SCRIPT_VERSION + "\n" +
            "==========================\n\n"

        alert(msg);

    } catch (e) {
        alert((SCRIPT_NAME + " v" + SCRIPT_VERSION) + ":\nКритическая ошибка:\n" + e.message);
    } finally {
        window.external.EndUndoUnit(document);
    }
}
