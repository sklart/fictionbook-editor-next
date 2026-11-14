// ==================================================
//Скрипт: «Разбить выделенные абзацы по регекспу» v.3.6
// Сборка — stokber + помощь DeepSeek (2026, октябрь).
// В сценарии использован алгоритм сохранения форматирования
// при разбивке абзацев из скрипта ув. TaKir-a
// «Разделить несколько слипшихся абзацев стиха на строки»
// (se 3.6)

// Описание: Разбивает выделенные абзацы в документе FB2
//           перед каждым совпадением с регулярным выражением.
//           Сохраняет форматирование, включая атрибуты тегов.
//           Учитывает теги A: разбивка в начале ссылки выполняется всегда,
//           в середине – по настройке SPLIT_INSIDE_LINKS.
//           Сноски распознаются по двум допустимым видам:
//             1) <a class="note" href="(file:...main.html)?#n_N">[N]</a>
//             2) <a href="(file:...main.html)?#c_N">{N}</a>
//           Их обработка регулируется настройкой IGNORE_FOOTNOTES.
//           Если совпадений нет — сообщение выводится сразу,
//           не задавая вопрос о сносках.
//           После выполнения курсор устанавливается в начало
//           первого обработанного абзаца с отступом сверху ~3 строки.
// Версия: 3.6 (разграничены области настроек SPLIT_INSIDE_LINKS
//              и IGNORE_FOOTNOTES: режимы SPLIT_INSIDE_LINKS
//              больше не применяются к допустимым сноскам)
//           Совместим с IE6.
// ==================================================

// ===================== НАСТРОЙКИ =====================
var SCRIPT_NAME = "Разбить выделенные абзацы по регекспу";
var SCRIPT_VERSION = "3.6";
var MARKER = "§%§"; // Уникальный маркер, временно вставляемый в HTML в местах будущего разбиения

// регулярное выражение для разбивки по умолчанию:
// перед числом в квадратных скобках:
var defaultRegex = "\\[\\d+\\]";
// перед числом в фигурных скобках:
// var defaultRegex = "\\{\\d+\\}";
// перед числом:
// var defaultRegex = "\\d+";
// перед числом с точкой:
// var defaultRegex = "\\d+\\.";
// перед латиницей со скобкой:
// var defaultRegex = "[a-z]+\\)";
// перед кириллицей со скобкой:
// var defaultRegex = "[а-я]+\\)";
// перед заголовком вида "Глава":
// var defaultRegex = "Глава|Часть";
// и др.

// Поведение при разбивке внутри ссылок (тег A):
// "0" - не разбивать (кроме начал ссылок)
// "1" - разбивать всегда (включая середины)
// "2" - спросить у пользователя (по умолчанию)
var SPLIT_INSIDE_LINKS = "0";

// Поведение при обработке сносок (тег <a> со ссылкой на сноску):
// "0" - не разбивать ВНУТРИ сносок (текст сносок игнорируется при анализе)
// "1" - разбивать (текст сносок учитывается как обычный текст)
// "2" - спросить у пользователя (по умолчанию)
var IGNORE_FOOTNOTES = "0";

// Отступ сверху для первого обработанного абзаца (в пикселях).
// Приблизительно 3 строки текста.
var TOP_OFFSET_PIXELS = 66;

// ===================== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ =====================

// Поиск элемента в массиве (IE6-совместимая замена Array.indexOf).
function arrayContains(arr, item) {
    if (!arr) return -1;
    for (var i = 0; i < arr.length; i++) {
        if (arr[i] == item) return i;
    }
    return -1;
}

// Проверка, является ли символ пробельным.
function isWhitespace(currentChar) {
    return currentChar == ' ' || currentChar == '\r' || currentChar == '\n';
}

// Обрезка ведущих и замыкающих пробельных символов строки.
function trimString(str) {
    if (!str) return "";
    return str.replace(/^\s+|\s+$/g, '');
}

// Получаем символ неразрывного пробела из настроек FBE.
var nbspEntity = "&nbsp;";
try {
    var nbspChar = window.external.GetNBSP();
    if (nbspChar.charCodeAt(0) != 160)
        nbspEntity = nbspChar;
} catch (e) {
    nbspChar = String.fromCharCode(160);
}

// Экранированная версия nbspEntity для безопасного использования в RegExp.
var nbspEntityEscaped = nbspEntity.replace(/([.*+?^${}()|[\]\\])/g, "\\$1");

// Приведение текста к удобному для анализа виду.
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
    normalized = normalized.replace(new RegExp(nbspEntityEscaped, "g"), " ");
    normalized = normalized.replace(/□/g, " ");
    normalized = normalized.replace(/▫/g, " ");
    normalized = normalized.replace(/◦/g, " ");
    normalized = normalized.replace(/\xA0/g, " ");
    return normalized;
}

// Извлечение значения указанного атрибута из строки открывающего тега.
// Поддерживает значения в двойных, одинарных кавычках и без кавычек.
function extractAttributeFromTag(fullTag, attrName) {
    var re = new RegExp('(?:^|[\\s<])' + attrName + '\\s*=\\s*("[^"]*"|\'[^\']*\'|[^\\s>]+)', 'i');
    var m = fullTag.match(re);
    if (!m) return '';
    var val = m[1];
    if (val.length >= 2 && (val.charAt(0) === '"' || val.charAt(0) === "'")) {
        return val.substring(1, val.length - 1);
    }
    return val;
}

// Извлечение href (приоритет — l:href, затем обычный href).
function extractHrefFromTag(fullTag) {
    var v = extractAttributeFromTag(fullTag, 'l:href');
    if (v !== '') return v;
    return extractAttributeFromTag(fullTag, 'href');
}

// Извлечение значения атрибута class.
function extractClassFromTag(fullTag) {
    return extractAttributeFromTag(fullTag, 'class');
}

// Рекурсивный сбор чистого текста DOM-узла (без HTML-тегов).
function getInnerTextOfNode(node) {
    var t = "";
    for (var i = 0; i < node.childNodes.length; i++) {
        var child = node.childNodes[i];
        if (child.nodeType == 3) {
            t += child.nodeValue;
        } else if (child.nodeType == 1) {
            t += getInnerTextOfNode(child);
        }
    }
    return t;
}

// Проверка, является ли DOM-узел <a> сноской одного из двух допустимых видов:
//   1) class="note", href = (file:...main.html)?#n_N, текст = [N]
//   2) href = (file:...main.html)?#c_N, текст = {N}
function isFootnoteANode(aNode) {
    var href = aNode.getAttribute("l:href") || aNode.getAttribute("href") || "";
    var cls = aNode.getAttribute("class") || aNode.className || "";
    var inner = getInnerTextOfNode(aNode);
    if (cls == "note") {
        if (/^(file:.+?main\.html)?#n_\d+$/.test(href) && /^\[\d+\]$/.test(inner)) {
            return true;
        }
    }
    if (/^(file:.+?main\.html)?#c_\d+$/.test(href) && /^\{\d+\}$/.test(inner)) {
        return true;
    }
    return false;
}

// Проверка, является ли фрагмент <a ...>...</a> сноской (строковый вариант).
// fullTag — открывающий тег, innerText — чистый текст внутри <a>.
function isFootnoteAByStrings(fullTag, innerText) {
    var href = extractHrefFromTag(fullTag);
    var cls = extractClassFromTag(fullTag);
    if (cls == "note") {
        if (/^(file:.+?main\.html)?#n_\d+$/.test(href) && /^\[\d+\]$/.test(innerText)) {
            return true;
        }
    }
    if (/^(file:.+?main\.html)?#c_\d+$/.test(href) && /^\{\d+\}$/.test(innerText)) {
        return true;
    }
    return false;
}

// Поиск позиции закрывающего </a>, соответствующего открывающему <a ...>
// на позиции openTagStart. Учитывает вложенность.
// Возвращает индекс '<' закрывающего тега или -1, если не найден.
function findMatchingCloseA(html, openTagStart) {
    var depth = 0;
    var i = openTagStart;
    while (i < html.length) {
        var lt = html.indexOf('<', i);
        if (lt === -1) return -1;
        var gt = html.indexOf('>', lt);
        if (gt === -1) return -1;
        var tag = html.substring(lt, gt + 1);
        var isClosing = (tag.length >= 2 && tag.charAt(1) === '/');
        var startName = lt + (isClosing ? 2 : 1);
        var endName = startName;
        while (endName < gt && html.charAt(endName) !== ' ' && html.charAt(endName) !== '>' && html.charAt(endName) !== '/') {
            endName++;
        }
        var name = html.substring(startName, endName).toLowerCase();
        if (name === 'a') {
            var isSelfClosing = (tag.length >= 2 && tag.charAt(tag.length - 2) === '/');
            if (!isSelfClosing) {
                if (isClosing) {
                    depth--;
                    if (depth === 0) return lt;
                } else {
                    depth++;
                }
            }
        }
        i = gt + 1;
    }
    return -1;
}

// Рекурсивный обход DOM-узла для сбора всего видимого текста.
// Если ignoreFootnotes == true, содержимое тегов <a>-сносок
// (по правилам isFootnoteANode) пропускается.
function getPlainTextFromElement(element, ignoreFootnotes) {
    var text = "";

    function walk(node) {
        if (node.nodeType == 3) {
            text += normalizeTextForAnalysis(node.nodeValue);
        } else if (node.nodeType == 1) {
            if (ignoreFootnotes && node.nodeName == "A" && isFootnoteANode(node)) {
                return;
            }
            for (var i = 0; i < node.childNodes.length; i++) {
                walk(node.childNodes[i]);
            }
        }
    }
    walk(element);
    return text;
}

// Построение таблицы соответствия между позициями в HTML и в тексте.
// Согласовано с getPlainTextFromElement: содержимое сносок
// (при ignoreFootnotes == true) не попадает в mapping.
function getPositionMapping(html, normalizedText, ignoreFootnotes) {
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
                var fullTag = html.substring(htmlIndex, tagEnd + 1);
                var isClosing = (fullTag.length >= 2 && fullTag.charAt(1) === '/');
                var tagName = '';
                var startName = htmlIndex + (isClosing ? 2 : 1);
                var endName = startName;
                while (endName < tagEnd && html.charAt(endName) !== ' ' && html.charAt(endName) !== '>' && html.charAt(endName) !== '/') {
                    endName++;
                }
                tagName = html.substring(startName, endName).toLowerCase();
                var isSelfClosing = (fullTag.length >= 2 && fullTag.charAt(fullTag.length - 2) === '/');

                // Если это открывающий <a> и мы игнорируем сноски — проверяем,
                // не является ли весь блок <a>...</a> сноской; если да, пропускаем его целиком.
                if (ignoreFootnotes && tagName === 'a' && !isClosing && !isSelfClosing) {
                    var closeLt = findMatchingCloseA(html, htmlIndex);
                    if (closeLt !== -1) {
                        var innerHtml = html.substring(tagEnd + 1, closeLt);
                        var innerText = innerHtml.replace(/<[^>]*>/g, '');
                        if (isFootnoteAByStrings(fullTag, innerText)) {
                            var closeGt = html.indexOf('>', closeLt);
                            htmlIndex = (closeGt !== -1) ? closeGt + 1 : closeLt + 4;
                            continue;
                        }
                    }
                }

                htmlIndex = tagEnd + 1;
                continue;
            } else {
                htmlIndex++;
                continue;
            }
        }

        // Обрабатываем HTML-сущности
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
                for (var numericEntityIndex = htmlIndex; numericEntityIndex < html.length; numericEntityIndex++) {
                    if (html.charAt(numericEntityIndex) == ';') {
                        semicolonPos = numericEntityIndex;
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

        // Обрабатываем спецсимволы FBE и неразрывный пробел
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

        // Обычный символ
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

// Преобразование позиции из нормализованного текста в позицию в HTML.
function convertTextPosToHtmlPos(textPos, mapping) {
    if (!mapping || mapping.length == 0) return textPos;
    for (var i = 0; i < mapping.length; i++) {
        if (mapping[i].textPos == textPos) return mapping[i].htmlPos;
    }
    var bestMatch = -1;
    var bestMatchIndex = -1;
    for (var candidateIndex = 0; candidateIndex < mapping.length; candidateIndex++) {
        if (mapping[candidateIndex].textPos <= textPos && mapping[candidateIndex].textPos > bestMatch) {
            bestMatch = mapping[candidateIndex].textPos;
            bestMatchIndex = candidateIndex;
        }
    }
    if (bestMatchIndex != -1) {
        var diff = textPos - mapping[bestMatchIndex].textPos;
        if (diff <= 5) return mapping[bestMatchIndex].htmlPos + diff;
    }
    return textPos;
}

// Проверка, находится ли указанная позиция внутри тега <A>...</A>.
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

// Поиск позиции открывающего тега <A>, внутри которого находится позиция.
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

// Проверка, приходится ли позиция на самое начало содержимого тега <A>,
// то есть перед ней внутри этого же <A> нет ни одного НЕПРОБЕЛЬНОГО символа.
// Значащим считается любой символ, кроме пробелов, табуляции, CR, LF
// и неразрывного пробела.
function isStartOfATagContent(html, pos) {
    if (!isInsideATag(html, pos)) return false;
    var openPos = getOpenATagPosition(html, pos);
    if (openPos === -1) return false;
    var gt = html.indexOf('>', openPos);
    if (gt === -1) return false;
    var content = html.substring(gt + 1, pos);
    var text = content.replace(/<[^>]*>/g, '');
    for (var i = 0; i < text.length; i++) {
        var ch = text.charAt(i);
        if (ch !== ' ' && ch !== '\r' && ch !== '\n' && ch !== '\t' && ch !== '\xA0') {
            return false; // перед позицией есть значащий символ → это не начало
        }
    }
    return true;
}

// Проверка, попадает ли указанная HTML-позиция внутрь ДОПУСТИМОЙ сноски
// (по тем же правилам, что isFootnoteAByStrings / isFootnoteANode).
// Возвращает true, если позиция находится внутри <a>, распознанного
// как сноска; иначе false.
// Используется для разграничения поведения SPLIT_INSIDE_LINKS
// (обычные ссылки) и IGNORE_FOOTNOTES (сноски).
function isFootnoteAtHtmlPosition(html, pos) {
    if (!isInsideATag(html, pos)) return false;
    var openLt = getOpenATagPosition(html, pos);
    if (openLt === -1) return false;
    var openGt = html.indexOf('>', openLt);
    if (openGt === -1) return false;
    var openTag = html.substring(openLt, openGt + 1);
    var closeLt = findMatchingCloseA(html, openLt);
    if (closeLt === -1) return false;
    var innerHtml = html.substring(openGt + 1, closeLt);
    var innerText = innerHtml.replace(/<[^>]*>/g, '');
    return isFootnoteAByStrings(openTag, innerText);
}

// Проверка, находится ли позиция внутри HTML-тега (между '<' и '>').
function isInsideHtmlTag(html, pos) {
    if (pos <= 0 || pos >= html.length) return false;
    var beforeTag = false;
    for (var i = pos - 1; i >= 0; i--) {
        var ch = html.charAt(i);
        if (ch == '<') {
            beforeTag = true;
            break;
        }
        if (ch == '>') break;
    }
    if (!beforeTag) return false;
    var afterTag = false;
    for (var afterIndex = pos; afterIndex < html.length; afterIndex++) {
        var afterChar = html.charAt(afterIndex);
        if (afterChar == '>') {
            afterTag = true;
            break;
        }
        if (afterChar == '<') break;
    }
    return afterTag;
}

// Вставка маркеров разбиения в HTML по заданным позициям.
// Разграничение поведения:
//   - Если позиция внутри ДОПУСТИМОЙ сноски → SPLIT_INSIDE_LINKS не применяется.
//   - Если позиция внутри ОБЫЧНОЙ ссылки:
//       * на самом начале содержимого ссылки → маркер сдвигается
//         в позицию ПЕРЕД открывающим <a ...> (чтобы не разрывать ссылку);
//       * в остальных позициях внутри ссылки → при !allowInsideLinks
//         позиция пропускается; иначе маркер вставляется как обычно.
function insertMarkersWithCorrection(html, normalizedText, positions, marker, allowInsideLinks, ignoreFootnotes) {
    if (!html || !normalizedText || positions.length == 0) return {
        html: html,
        skippedInsideA: 0
    };
    var mapping = getPositionMapping(html, normalizedText, ignoreFootnotes);
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
        if (htmlPos == -1 || htmlPos >= result.length) continue;

        var insideA = isInsideATag(result, htmlPos);
        var isFootnoteHere = false;
        if (insideA) {
            isFootnoteHere = isFootnoteAtHtmlPosition(result, htmlPos);
        }

        // Разграничение сносок и обычных ссылок
        if (insideA && !isFootnoteHere) {
            var isStart = isStartOfATagContent(result, htmlPos);
            if (isStart) {
                // Позиция в самом начале содержимого ссылки.
                // Сдвигаем маркер к позиции ПЕРЕД открывающим <a ...>.
                var openLt = getOpenATagPosition(result, htmlPos);
                if (openLt === -1) {
                    skipped++;
                    continue;
                }
                htmlPos = openLt;
                insideA = false; // теперь позиция вне <a>
            } else if (!allowInsideLinks) {
                // Середина содержимого ссылки, разбиение запрещено
                skipped++;
                continue;
            }
        }

        // Вставка маркера (с обязательной проверкой «не внутри HTML-тега»)
        if (!isInsideHtmlTag(result, htmlPos)) {
            result = result.substring(0, htmlPos) + marker + result.substring(htmlPos);
        } else {
            var newPos = htmlPos;
            while (newPos < result.length && isInsideHtmlTag(result, newPos)) {
                newPos++;
            }
            if (newPos < result.length) {
                result = result.substring(0, newPos) + marker + result.substring(newPos);
            }
        }
    }
    return {
        html: result,
        skippedInsideA: skipped
    };
}

// Разбиение HTML по маркеру с сохранением структуры тегов.
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

// Очистка крайних пробелов и пустых участков в куске HTML.
function cleanHTMLPart(html) {
    if (!html) return '';
    var trimmed = html;
    trimmed = trimmed.replace(/^(\s*)([^<])/, '$2');
    trimmed = trimmed.replace(/([^>])(\s*)$/, '$1');
    return trimmed;
}

// Обход DOM начиная с текущего выделения для сбора всех абзацев <p>.
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
                            var childFirstParagraph = findFirstParagraphInElement(child);
                            if (childFirstParagraph) {
                                nextInParent = childFirstParagraph;
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

// Рекурсивный поиск первого абзаца <p> внутри элемента.
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

// Проверка, есть ли в элементе хотя бы одна сноска допустимого вида.
function hasFootnoteTagsInElement(element) {
    var found = false;

    function walk(node) {
        if (found) return;
        if (node.nodeType == 1) {
            if (node.nodeName == "A" && isFootnoteANode(node)) {
                found = true;
                return;
            }
            for (var i = 0; i < node.childNodes.length; i++) {
                walk(node.childNodes[i]);
                if (found) return;
            }
        }
    }
    walk(element);
    return found;
}

// ===================== ОСНОВНАЯ ЛОГИКА РАЗБИВКИ ПО РЕГУЛЯРНОМУ ВЫРАЖЕНИЮ =====================

// Поиск всех позиций в нормализованном тексте, перед которыми
// должен быть разрыв абзаца. Позиция 0 пропускается.
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

// Обработка одного абзаца: поиск позиций, вставка маркеров, разбиение.
function splitParagraphByRegex(paragraph, regex, allowInsideLinks, ignoreFootnotes) {
    try {
        var originalHTML = paragraph.innerHTML;
        var normalizedText = getPlainTextFromElement(paragraph, ignoreFootnotes);
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
        var result = insertMarkersWithCorrection(originalHTML, normalizedText, breakPositions, MARKER, allowInsideLinks, ignoreFootnotes);
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

// Форматирование времени в формате MM:SS:mmm.
function formatTime(milliseconds) {
    var mins = Math.floor(milliseconds / 60000);
    var secs = Math.floor((milliseconds % 60000) / 1000);
    var msecs = milliseconds % 1000;
    var msecStr = (msecs < 100 ? (msecs < 10 ? "00" : "0") : "") + msecs;
    return mins + ":" + secs + ":" + msecStr;
}

// Установка курсора в начало указанного элемента (IE6-совместимо).
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

// Прокрутка окна так, чтобы верх элемента оказался на заданном
// расстоянии от верхней границы окна просмотра.
function scrollElementToTopWithOffset(element, offsetPixels) {
    if (!element) return;
    try {
        element.scrollIntoView(true);
        var elementTop = 0;
        var el = element;
        while (el) {
            elementTop += el.offsetTop || 0;
            el = el.offsetParent;
        }
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

    var firstProcessedPara = null;

    window.external.BeginUndoUnit(document, SCRIPT_NAME + " v" + SCRIPT_VERSION);

    try {
        // 1. Запрашиваем регулярное выражение
        // из prompt (настраивается в начале кода):
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

        // 2. Получаем выделенные абзацы
        var paragraphs = getParagraphsInSelection();
        statsTotal.paragraphsFound = paragraphs.length;
        if (paragraphs.length == 0) {
            alert((SCRIPT_NAME + " v" + SCRIPT_VERSION) + ":\nНет выделенных абзацев <p>. Пожалуйста, выделите текст, содержащий абзацы.");
            window.external.EndUndoUnit(document);
            return;
        }

        // 3. Предварительная проверка: есть ли вообще совпадения в тексте
        //    выделенных абзацев (по самому широкому критерию — с учётом
        //    всего текста, включая сноски). Если совпадений нет — сообщаем
        //    сразу, без дополнительных вопросов.
        var anyMatch = false;
        for (var chk = 0; chk < paragraphs.length; chk++) {
            var chkText = getPlainTextFromElement(paragraphs[chk], false);
            var chkPositions = findAllBreakPositionsByRegex(chkText, regex);
            if (chkPositions.length > 0) {
                anyMatch = true;
                break;
            }
        }
        if (!anyMatch) {
            alert("В выделенных абзацах совпадений по введённому вами регекспу не найдено");
            window.external.EndUndoUnit(document);
            return;
        }

        // 4. Определяем hasInsideANotStart по ВСЕМ позициям разбивки
        //    (эквивалент ignoreFootnotes = false) — это самая широкая
        //    картина. Нужна для вопроса про ссылки ДО вопроса про сноски.
        var hasInsideANotStart = false;
        for (var idx = 0; idx < paragraphs.length; idx++) {
            var pCheck = paragraphs[idx];
            var normalizedTextCheck = getPlainTextFromElement(pCheck, false);
            var breakPositionsCheck = findAllBreakPositionsByRegex(normalizedTextCheck, regex);
            if (breakPositionsCheck.length > 0) {
                var mappingCheck = getPositionMapping(pCheck.innerHTML, normalizedTextCheck, false);
                if (mappingCheck.length > 0) {
                    for (var b = 0; b < breakPositionsCheck.length; b++) {
                        var textPosCheck = breakPositionsCheck[b];
                        var htmlPosCheck = convertTextPosToHtmlPos(textPosCheck, mappingCheck);
                        if (htmlPosCheck != -1 && isInsideATag(pCheck.innerHTML, htmlPosCheck)) {
                            if (!isStartOfATagContent(pCheck.innerHTML, htmlPosCheck)) {
                                hasInsideANotStart = true;
                                break;
                            }
                        }
                    }
                }
            }
            if (hasInsideANotStart) break;
        }

        // 5. Определяем allowInsideLinks (ВОПРОС ПРО ССЫЛКИ — ДО ВОПРОСА ПРО СНОСКИ)
        var allowInsideLinks = null;
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

        // 6. Определяем ignoreFootnotes (ВОПРОС ПРО СНОСКИ — ПОСЛЕ ВОПРОСА ПРО ССЫЛКИ)
        var ignoreFootnotes = true;
        if (IGNORE_FOOTNOTES === "0") {
            ignoreFootnotes = true;
        } else if (IGNORE_FOOTNOTES === "1") {
            ignoreFootnotes = false;
        } else { // "2" - спросить
            var hasFootnotes = false;
            for (var fi = 0; fi < paragraphs.length; fi++) {
                if (hasFootnoteTagsInElement(paragraphs[fi])) {
                    hasFootnotes = true;
                    break;
                }
            }
            if (hasFootnotes) {
                var ansFoot = confirm("В выделенных абзацах обнаружены сноски, типичные для редактора FBE (вида '<a l:href=\"#n_1\" type=\"note\">[1]</a>' и '<a l:href=\"#c_1\">{1}</a>').\n\nУчитывать текст сносок при поиске совпадений и разбивать перед ними или внутри них?\n\n'ОК' - учитывать (разбивать), 'Отмена' - не учитывать (игнорировать).");
                ignoreFootnotes = !ansFoot;
            } else {
                ignoreFootnotes = true;
            }
        }

        // 7. Повторная проверка: если сноски игнорируются, возможно,
        //    что все совпадения были внутри сносок и теперь разбивать
        //    нечего. В этом случае сообщаем об этом и завершаем работу.
        if (ignoreFootnotes) {
            var anyMatchAfter = false;
            for (var chk2 = 0; chk2 < paragraphs.length; chk2++) {
                var chkText2 = getPlainTextFromElement(paragraphs[chk2], true);
                var chkPositions2 = findAllBreakPositionsByRegex(chkText2, regex);
                if (chkPositions2.length > 0) {
                    anyMatchAfter = true;
                    break;
                }
            }
            if (!anyMatchAfter) {
                alert("В выделенных абзацах совпадений по введённому вами регекспу не найдено");
                window.external.EndUndoUnit(document);
                return;
            }
        }

        // 8. Обрабатываем каждый абзац
        for (var i = 0; i < paragraphs.length; i++) {
            var p = paragraphs[i];
            var result = splitParagraphByRegex(p, regex, allowInsideLinks, ignoreFootnotes);
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

                if (firstProcessedPara === null) {
                    var headP = referenceNode;
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

        // 9. Устанавливаем курсор в начало первого обработанного абзаца
        if (firstProcessedPara) {
            setCursorToElementStart(firstProcessedPara);
            scrollElementToTopWithOffset(firstProcessedPara, TOP_OFFSET_PIXELS);
        } else {
            try {
                var clearRange = document.body.createTextRange();
                clearRange.collapse();
                clearRange.select();
            } catch (e) {}
        }

        // 10. Статистика
        var endTime = new Date();
        var elapsed = endTime - startTime;
        var timeStr = formatTime(elapsed);
        var msg = "==========================\n" +
            SCRIPT_NAME + " v" + SCRIPT_VERSION + "\n" +
            "==========================\n\n" +
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
        msg += "Время выполнения: " + timeStr;
        alert(msg);

    } catch (e) {
        alert((SCRIPT_NAME + " v" + SCRIPT_VERSION) + ":\nКритическая ошибка:\n" + e.message);
    } finally {
        window.external.EndUndoUnit(document);
    }
}