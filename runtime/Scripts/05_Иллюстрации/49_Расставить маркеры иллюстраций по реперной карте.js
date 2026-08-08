// Скрипт "Расставить маркеры иллюстраций по реперной карте" для редактора FBE
// version 2.5
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Второй скрипт из комплекта для переноса иллюстраций между fb2-документами.
// Работает в паре со Скриптом 1 "Снять реперную карту fb2 документа для переноса иллюстраций.js".
//
// Как работает эта пара скриптов:
// Создать реперную карту fb2 документа для переноса иллюстраций.js
// и
// Расставить маркеры иллюстраций по реперной карте.js

// Открываем исходный документ с иллюстрациями.
// Запускаем скрипт для создания реперной карты данного документа.
// Открываем целевой документ с отредактированным текстом и без иллюстраций.
// Запускаем второй скрипт Расставить маркеры иллюстраций по реперной карте.js
// Второй скрипт расставляет в целевом документе текстовые маркеры типа zzz_pic
// или сразу пустые картинки (в зависимости от включенных настроек во втором скрипте)
// на местах, максимально совпадающих с исходным документом.
// В случае наличия предполагаемых ошибок расстановки,
// скрипт создает файл отчета об ошибках в той же папке D:\\FBE_Compare,
// где первым скриптом создается реперная карта исходного документа.

// Далее можно:
// Проверить в целевом документе расстановку текстовых маркеров,
// при необходимости переставить отдельные маркеры вручную.
// Заменить текстовые маркеры zzz_pic на <image l:href="#undefined"/> глобальной заменой в режиме XML кода.
// Подцепить на места пустышек реальные иллюстрации скриптом "15_Расставить иллюстрации по заданным местам.js"
//
// Особенности данного скрипта:
// - Чтение реперной карты из TXT через FSO
// - Поиск мест вставки по текстовым реперам с учётом корректуры целевого текста
// - Вставка текстовых маркеров или сразу пустых картинок (на выбор в настройках ниже)
// - Учёт пустых строк вокруг иллюстраций (из карты)

// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 2.5, 01.07.2026
//======================================

// ==================================================
// ГЛОБАЛЬНЫЕ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ==================================================

function isEmptyText(text) {
    if (!text || text.length == 0) return true;
    var normalized = "";
    for (var i = 0; i < text.length; i++) {
        var ch = text.charAt(i);
        var code = ch.charCodeAt(0);
        if (code == 32 || code == 160 || code == 8194 || code == 8195 || code == 8196 ||
            code == 8197 || code == 8198 || code == 8201 || code == 8202 || code == 8239) {
            normalized += " ";
        } else {
            normalized += ch;
        }
    }
    var noSpaces = "";
    for (var i = 0; i < normalized.length; i++) {
        if (normalized.charAt(i) != " ") {
            noSpaces += normalized.charAt(i);
        }
    }
    noSpaces = noSpaces.replace(/&nbsp;/g, "");
    noSpaces = noSpaces.replace(new RegExp(String.fromCharCode(160), "g"), "");
    return noSpaces.length == 0;
}

function Run() {
    var scriptName = "Расставить маркеры иллюстраций по реперной карте";
    var version = "2.5";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА
    // ==================================================

    var workFolder = "D:\\FBE_Compare";
    var mapFileName = "fb2_reper_map.txt";
    var reportFileName = "отчет_о_пропущенных_картинках.txt"; // Имя файла отчёта
    var insertType = "marker";
    var markerText = "zzz_pic";
    var addEmptyLines = 1;
    var showStatistics = 1;
    var minAnchorLength = 3;

    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ВНУТРИ RUN
    // ==================================================

    function getElementText(element) {
        var text = "";
        if (!element) return text;
        var children = element.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 3) {
                text += child.nodeValue;
            } else if (child.nodeType == 1) {
                text += getElementText(child);
            }
        }
        return text;
    }

    function cleanText(text) {
        if (!text) return "";
        var result = "";
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            var code = ch.charCodeAt(0);
            if ((code >= 48 && code <= 57) ||
                (code >= 65 && code <= 90) ||
                (code >= 97 && code <= 122) ||
                (code >= 1040 && code <= 1103) ||
                code == 1025 || code == 1105) {
                result += ch.toLowerCase();
            }
        }
        return result;
    }

    function isEmptyParagraph(paragraph) {
        if (!paragraph || paragraph.nodeType != 1 || paragraph.nodeName != "P") return false;
        var text = getElementText(paragraph);
        return isEmptyText(text);
    }

    function isImage(node) {
        if (!node || node.nodeType != 1) return false;
        if (node.nodeName == "DIV") {
            var cls = node.className || "";
            if (cls == "image") {
                var href = node.getAttribute("href") || "";
                if (href.length > 0 && href.charAt(0) == "#") {
                    return true;
                }
            }
        }
        return false;
    }

    function isTitle(node) {
        if (!node || node.nodeType != 1 || node.nodeName != "DIV") return false;
        var cls = node.className || "";
        return cls == "title" || cls == "subtitle";
    }

    function isSection(node) {
        if (!node || node.nodeType != 1 || node.nodeName != "DIV") return false;
        return (node.className || "") == "section";
    }

    function isBody(node) {
        if (!node || node.nodeType != 1 || node.nodeName != "DIV") return false;
        return (node.className || "") == "body";
    }

    function isTextBlock(node) {
        if (!node || node.nodeType != 1) return false;
        if (node.nodeName == "P") return true;
        if (node.nodeName == "DIV") {
            var cls = node.className || "";
            return cls == "title" || cls == "subtitle" || cls == "epigraph" || 
                   cls == "cite" || cls == "annotation" || cls == "poem";
        }
        return false;
    }

    function isContainer(node) {
        if (!node || node.nodeType != 1) return false;
        if (isSection(node) || isBody(node)) return true;
        if (node.nodeName == "DIV") {
            var cls = node.className || "";
            return cls == "stanza";
        }
        return false;
    }

    function filterShortAnchors(anchors) {
        if (anchors.length == 0) return anchors;
        var hasLongAnchor = false;
        for (var i = 0; i < anchors.length; i++) {
            if (anchors[i].length >= minAnchorLength) {
                hasLongAnchor = true;
                break;
            }
        }
        if (!hasLongAnchor) return anchors;
        var filtered = [];
        for (var i = 0; i < anchors.length; i++) {
            if (anchors[i].length >= minAnchorLength) {
                filtered.push(anchors[i]);
            }
        }
        return filtered;
    }

    function findFirstTextInContainer(container) {
        if (!container) return null;
        var children = container.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType != 1) continue;
            if (child.nodeName == "P" && !isEmptyParagraph(child)) return child;
            if (isTitle(child)) continue;
            if (isSection(child)) continue;
            if (child.nodeName == "DIV" && !isImage(child)) {
                var found = findFirstTextInContainer(child);
                if (found) return found;
            }
        }
        return null;
    }

    function nextSiblingIsSection(element) {
        var sibling = element.nextSibling;
        while (sibling) {
            if (sibling.nodeType == 3) {
                if (!isEmptyText(sibling.nodeValue || "")) return false;
                sibling = sibling.nextSibling;
                continue;
            }
            if (sibling.nodeType == 1) {
                if (sibling.nodeName == "P" && isEmptyParagraph(sibling)) {
                    sibling = sibling.nextSibling;
                    continue;
                }
                return isSection(sibling);
            }
            sibling = sibling.nextSibling;
        }
        return false;
    }

    var nbspChar, nbspEntity;
    try {
        nbspChar = window.external.GetNBSP();
        if (nbspChar.charCodeAt(0) == 160) nbspEntity = "&nbsp;";
        else nbspEntity = nbspChar;
    } catch (e) {
        nbspChar = String.fromCharCode(160);
        nbspEntity = "&nbsp;";
    }

    function createEmptyLine() {
        var p = document.createElement("P");
        return p;
    }

    function createInsertElement() {
        if (insertType == "undefined") {
            var div = document.createElement("DIV");
            div.className = "image";
            div.setAttribute("href", "#undefined");
            var img = document.createElement("IMG");
            img.setAttribute("src", "fbw-internal:#undefined");
            div.appendChild(img);
            return div;
        } else {
            var p = document.createElement("P");
            p.innerHTML = markerText;
            return p;
        }
    }

    function insertMarkerAfter(element, emptyAbove, emptyBelow) {
        if (nextSiblingIsSection(element)) {
            var nextSection = element.nextSibling;
            while (nextSection && nextSection.nodeType == 3) nextSection = nextSection.nextSibling;
            while (nextSection && nextSection.nodeType == 1 && nextSection.nodeName == "P" && isEmptyParagraph(nextSection)) nextSection = nextSection.nextSibling;
            if (nextSection && isSection(nextSection)) {
                var firstText = findFirstTextInContainer(nextSection);
                if (firstText) return insertMarkerBeforeInSection(firstText, emptyAbove, emptyBelow);
            }
        }

        var parent = element.parentNode;
        if (addEmptyLines && emptyAbove > 0) {
            for (var ea = 0; ea < emptyAbove; ea++) {
                var emptyP = createEmptyLine();
                if (element.nextSibling) parent.insertBefore(emptyP, element.nextSibling);
                else parent.appendChild(emptyP);
                window.external.inflateBlock(emptyP) = true;
                element = emptyP;
            }
        }

        var markerEl = createInsertElement();
        if (element.nextSibling) parent.insertBefore(markerEl, element.nextSibling);
        else parent.appendChild(markerEl);

        if (addEmptyLines && emptyBelow > 0) {
            var lastElement = markerEl;
            for (var eb = 0; eb < emptyBelow; eb++) {
                var emptyP = createEmptyLine();
                if (lastElement.nextSibling) parent.insertBefore(emptyP, lastElement.nextSibling);
                else parent.appendChild(emptyP);
                window.external.inflateBlock(emptyP) = true;
                lastElement = emptyP;
            }
        }
        return markerEl;
    }

    function insertMarkerBeforeInSection(element, emptyAbove, emptyBelow) {
        var parent = element.parentNode;
        var markerEl = createInsertElement();
        if (addEmptyLines && emptyBelow > 0) {
            var firstEmpty = null;
            for (var eb = emptyBelow - 1; eb >= 0; eb--) {
                var emptyP = createEmptyLine();
                parent.insertBefore(emptyP, element);
                window.external.inflateBlock(emptyP) = true;
                if (eb == 0) firstEmpty = emptyP;
            }
            if (firstEmpty) parent.insertBefore(markerEl, firstEmpty);
            else parent.insertBefore(markerEl, element);
        } else {
            parent.insertBefore(markerEl, element);
        }
        if (addEmptyLines && emptyAbove > 0) {
            for (var ea = 0; ea < emptyAbove; ea++) {
                var emptyP = createEmptyLine();
                parent.insertBefore(emptyP, markerEl);
                window.external.inflateBlock(emptyP) = true;
            }
        }
        return markerEl;
    }

    function insertMarkerBefore(element, emptyAbove, emptyBelow) {
        var parent = element.parentNode;
        var markerEl = createInsertElement();
        if (addEmptyLines && emptyBelow > 0) {
            var firstEmpty = null;
            for (var eb = emptyBelow - 1; eb >= 0; eb--) {
                var emptyP = createEmptyLine();
                parent.insertBefore(emptyP, element);
                window.external.inflateBlock(emptyP) = true;
                if (eb == 0) firstEmpty = emptyP;
            }
            if (firstEmpty) parent.insertBefore(markerEl, firstEmpty);
            else parent.insertBefore(markerEl, element);
        } else {
            parent.insertBefore(markerEl, element);
        }
        if (addEmptyLines && emptyAbove > 0) {
            for (var ea = 0; ea < emptyAbove; ea++) {
                var emptyP = createEmptyLine();
                parent.insertBefore(emptyP, markerEl);
                window.external.inflateBlock(emptyP) = true;
            }
        }
        return markerEl;
    }

    function collectAllTextElements(container, result) {
        if (!container) return;
        var children = container.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType != 1) continue;
            if (isImage(child)) continue;
            if (isTextBlock(child)) {
                var text = getElementText(child);
                if (!isEmptyText(text)) {
                    var cleaned = cleanText(text);
                    result.push({ element: child, text: text, cleaned: cleaned, type: child.className || child.nodeName });
                }
            } else if (isContainer(child)) {
                collectAllTextElements(child, result);
            } else if (child.nodeName == "DIV") {
                collectAllTextElements(child, result);
            }
        }
    }

    function countMatchingAnchors(cleanedText, anchors) {
        if (anchors.length == 0) return 0;
        var count = 0;
        for (var a = 0; a < anchors.length; a++) {
            if (cleanedText.indexOf(anchors[a]) != -1) count++;
        }
        return count;
    }

    function globalSearchAnchors(elements, anchors, startIdx) {
        var positions = [];
        var sIdx = startIdx || 0;
        for (var i = sIdx; i < elements.length; i++) {
            var count = countMatchingAnchors(elements[i].cleaned, anchors);
            if (count > 0) positions.push({ index: i, count: count, text: elements[i].text });
        }
        return positions;
    }

    // Строгий поиск
    function findStrict(elements, startIndex, anchorsAbove, anchorsBelow) {
        if (anchorsAbove.length == 0) {
            var bestIdx = -1, bestCount = 0;
            for (var i = startIndex; i < elements.length; i++) {
                var belowScore = countMatchingAnchors(elements[i].cleaned, anchorsBelow);
                if (belowScore >= bestCount && belowScore > 0) { bestCount = belowScore; bestIdx = i; }
            }
            return { index: bestIdx, isBefore: true, score: bestCount };
        }
        if (anchorsBelow.length == 0) {
            var bestIdx = -1, bestCount = 0;
            for (var i = startIndex; i < elements.length; i++) {
                var aboveScore = countMatchingAnchors(elements[i].cleaned, anchorsAbove);
                if (aboveScore >= bestCount && aboveScore > 0) { bestCount = aboveScore; bestIdx = i; }
            }
            return { index: bestIdx, isBefore: false, score: bestCount };
        }

        var allAbove = globalSearchAnchors(elements, anchorsAbove, startIndex);
        var bestPair = { aboveIdx: -1, score: 0 };

        for (var ap = 0; ap < allAbove.length; ap++) {
            var aIdx = allAbove[ap].index;
            var aCount = allAbove[ap].count;
            var bestBelow = { idx: -1, count: 0 };
            for (var j = aIdx + 1; j < Math.min(elements.length, aIdx + 200); j++) {
                var bCount = countMatchingAnchors(elements[j].cleaned, anchorsBelow);
                if (bCount > bestBelow.count) { bestBelow.idx = j; bestBelow.count = bCount; }
            }
            if (bestBelow.idx >= 0) {
                var pairScore = aCount + bestBelow.count;
                if (pairScore > bestPair.score) { bestPair.aboveIdx = aIdx; bestPair.score = pairScore; }
            }
        }

        if (bestPair.aboveIdx >= 0) return { index: bestPair.aboveIdx, isBefore: false, score: bestPair.score };

        var bestAboveOnly = -1, bestCountOnly = 0;
        for (var ap2 = 0; ap2 < allAbove.length; ap2++) {
            if (allAbove[ap2].count > bestCountOnly) { bestCountOnly = allAbove[ap2].count; bestAboveOnly = allAbove[ap2].index; }
        }
        if (bestAboveOnly >= 0) {
            var anyBelow = globalSearchAnchors(elements, anchorsBelow, 0);
            if (anyBelow.length == 0) return { index: bestAboveOnly, isBefore: false, score: bestCountOnly };
        }
        return { index: -1, isBefore: false, score: 0 };
    }

    // Мягкий поиск в окне
    function findSoftInWindow(elements, windowStart, windowEnd, anchorsAbove, anchorsBelow) {
        if (windowStart >= windowEnd) return { index: -1, isBefore: false, score: 0 };

        var bestScore = 0;
        var bestIdx = -1;
        var bestIsBefore = false;

        for (var i = windowStart; i <= windowEnd && i < elements.length; i++) {
            var aboveScore = countMatchingAnchors(elements[i].cleaned, anchorsAbove);
            if (aboveScore > 0) {
                var bestBelowAfter = 0;
                for (var j = i + 1; j < Math.min(elements.length, i + 200); j++) {
                    var bCount = countMatchingAnchors(elements[j].cleaned, anchorsBelow);
                    if (bCount > bestBelowAfter) bestBelowAfter = bCount;
                }
                var bestBelowBefore = 0;
                var bestBelowBeforeIdx = -1;
                for (var j = Math.max(0, i - 200); j < i; j++) {
                    var bCount = countMatchingAnchors(elements[j].cleaned, anchorsBelow);
                    if (bCount > bestBelowBefore) { bestBelowBefore = bCount; bestBelowBeforeIdx = j; }
                }

                if (bestBelowAfter > 0) {
                    var score = aboveScore + bestBelowAfter;
                    if (score > bestScore) { bestScore = score; bestIdx = i; bestIsBefore = false; }
                }
                if (bestBelowBefore > 0) {
                    var score = aboveScore + bestBelowBefore;
                    if (score > bestScore) { bestScore = score; bestIdx = bestBelowBeforeIdx; bestIsBefore = true; }
                }
                if (bestBelowAfter == 0 && bestBelowBefore == 0 && aboveScore > bestScore) {
                    bestScore = aboveScore; bestIdx = i; bestIsBefore = false;
                }
            }

            var belowScore = countMatchingAnchors(elements[i].cleaned, anchorsBelow);
            if (belowScore > 0 && belowScore > bestScore) {
                bestScore = belowScore; bestIdx = i; bestIsBefore = true;
            }
        }

        return { index: bestIdx, isBefore: bestIsBefore, score: bestScore };
    }

    // ==================================================
    // ЧТЕНИЕ РЕПЕРНОЙ КАРТЫ
    // ==================================================

    var fullPath = workFolder + "\\" + mapFileName;
    var fileContent = "";
    var readSuccess = false;
    var readError = "";

    try {
        var fso = new ActiveXObject("Scripting.FileSystemObject");
        if (fso.FileExists(fullPath)) {
            var file = fso.OpenTextFile(fullPath, 1, false, -1);
            fileContent = file.ReadAll();
            file.Close();
            readSuccess = true;
        } else {
            readError = "Файл не найден: " + fullPath;
        }
    } catch (e) {
        readError = e.message;
    }

    if (!readSuccess) {
        if (showStatistics == 1) MsgBox(scriptName + "\n" + "ver. " + version + "\n----------------------------------------\n✗ Ошибка чтения файла:\n" + readError);
        return;
    }

    var lines = fileContent.split("\r\n");
    var totalImages = 0, totalSections = 0, bookTitle = "";
    var imagesData = [], totalFilteredAnchors = 0;
    var currentImage = null;

    for (var li = 0; li < lines.length; li++) {
        var line = lines[li];
        if (line.length == 0) continue;
        var parts = line.split("|");
        var key = parts[0];

        if (key == "TOTAL_IMAGES") totalImages = parseInt(parts[1]) || 0;
        else if (key == "TOTAL_SECTIONS") totalSections = parseInt(parts[1]) || 0;
        else if (key == "BOOK_TITLE") bookTitle = parts[1] || "";
        else if (key == "IMAGE") {
            if (currentImage) {
                var ba = currentImage.anchorsAbove.length, bb = currentImage.anchorsBelow.length;
                currentImage.anchorsAbove = filterShortAnchors(currentImage.anchorsAbove);
                currentImage.anchorsBelow = filterShortAnchors(currentImage.anchorsBelow);
                totalFilteredAnchors += (ba - currentImage.anchorsAbove.length) + (bb - currentImage.anchorsBelow.length);
                imagesData.push(currentImage);
            }
            currentImage = { imageNumber: parseInt(parts[1]) || 0, name: parts[2] || "", sectionIndex: parseInt(parts[3]) || 0, emptyAbove: 0, emptyBelow: 0, anchorsAbove: [], anchorsBelow: [], foundIndex: -1, isBefore: false };
        }
        else if (key == "EMPTY_ABOVE" && currentImage) currentImage.emptyAbove = parseInt(parts[2]) || 0;
        else if (key == "EMPTY_BELOW" && currentImage) currentImage.emptyBelow = parseInt(parts[2]) || 0;
        else if (key == "ANCHOR_ABOVE" && currentImage) {
            for (var aa = 2; aa < parts.length; aa++) {
                if (parts[aa] != "NO_TEXT") { var ca = cleanText(parts[aa]); if (ca.length > 0) currentImage.anchorsAbove.push(ca); }
            }
        }
        else if (key == "ANCHOR_BELOW" && currentImage) {
            for (var ab = 2; ab < parts.length; ab++) {
                if (parts[ab] != "NO_TEXT") { var ca = cleanText(parts[ab]); if (ca.length > 0) currentImage.anchorsBelow.push(ca); }
            }
        }
    }
    if (currentImage) {
        var ba = currentImage.anchorsAbove.length, bb = currentImage.anchorsBelow.length;
        currentImage.anchorsAbove = filterShortAnchors(currentImage.anchorsAbove);
        currentImage.anchorsBelow = filterShortAnchors(currentImage.anchorsBelow);
        totalFilteredAnchors += (ba - currentImage.anchorsAbove.length) + (bb - currentImage.anchorsBelow.length);
        imagesData.push(currentImage);
    }

    if (imagesData.length == 0) {
        if (showStatistics == 1) MsgBox(scriptName + "\n" + "ver. " + version + "\n----------------------------------------\n✗ В реперной карте нет данных о картинках.");
        return;
    }

    // ==================================================
    // ПОДТВЕРЖДЕНИЕ
    // ==================================================

    if (showStatistics == 1) {
        var confirmMsg = scriptName + "\nver. " + version + "\n----------------------------------------\n\n";
        confirmMsg += "Найдено в карте:\n• Картинок: " + imagesData.length + "\n• Секций в исходнике: " + totalSections + "\n";
        if (bookTitle.length > 0 && bookTitle != "NO_TITLE") confirmMsg += "• Книга: " + bookTitle + "\n";
        if (totalFilteredAnchors > 0) confirmMsg += "\n• Отфильтровано коротких реперов: " + totalFilteredAnchors + "\n";
        confirmMsg += "\nНастройки:\n• Тип вставки: " + (insertType == "marker" ? "текстовый маркер \"" + markerText + "\"" : "пустая картинка #undefined") + "\n";
        confirmMsg += "• Пустые строки: " + (addEmptyLines ? "ДА" : "НЕТ") + "\n• Мин. длина репера: " + minAnchorLength + " симв.\n\n";
        confirmMsg += "Алгоритм: двухпроходный\n\nРасставить маркеры в текущем документе?";
        if (!AskYesNo(confirmMsg)) return;
    }

    var startTime = new Date().getTime();

    // ==================================================
    // ФАЗА 1: СБОР ВСЕХ ТЕКСТОВЫХ ЭЛЕМЕНТОВ
    // ==================================================

    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) { if (showStatistics == 1) MsgBox(scriptName + "\n" + "ver. " + version + "\n----------------------------------------\n✗ Ошибка: не найден fbw_body."); return; }

    var allElements = [];
    var bodyChildren = fbwBody.childNodes;
    for (var bc = 0; bc < bodyChildren.length; bc++) {
        var child = bodyChildren[bc];
        if (child.nodeType == 1 && child.nodeName == "DIV" && isBody(child)) collectAllTextElements(child, allElements);
    }
    if (allElements.length == 0) { if (showStatistics == 1) MsgBox(scriptName + "\n" + "ver. " + version + "\n----------------------------------------\n✗ В документе нет текстовых элементов."); return; }

    // ==================================================
    // ФАЗА 2: ПРОХОД 1 — СТРОГИЙ ПОИСК
    // ==================================================

    var searchIndex = 0;
    for (var im = 0; im < imagesData.length; im++) {
        var imgInfo = imagesData[im];
        var result = findStrict(allElements, searchIndex, imgInfo.anchorsAbove, imgInfo.anchorsBelow);
        if (result.index < 0) {
            var globalResult = findStrict(allElements, 0, imgInfo.anchorsAbove, imgInfo.anchorsBelow);
            if (globalResult.index >= 0) result = globalResult;
        }
        if (result.index >= 0 && result.score > 0) {
            imgInfo.foundIndex = result.index;
            imgInfo.isBefore = result.isBefore;
            searchIndex = result.index + 1;
        }
    }

    // ==================================================
    // ФАЗА 3: ПРОХОД 2 — МЯГКИЙ ПОИСК
    // ==================================================

    for (var im = 0; im < imagesData.length; im++) {
        var imgInfo = imagesData[im];
        if (imgInfo.foundIndex >= 0) continue;

        var windowStart = 0;
        var windowEnd = allElements.length - 1;

        for (var p = im - 1; p >= 0; p--) {
            if (imagesData[p].foundIndex >= 0) { windowStart = imagesData[p].foundIndex + 1; break; }
        }
        for (var n = im + 1; n < imagesData.length; n++) {
            if (imagesData[n].foundIndex >= 0) { windowEnd = imagesData[n].foundIndex - 1; break; }
        }

        if (windowEnd - windowStart > 1000) {
            windowStart = Math.max(0, searchIndex);
            windowEnd = Math.min(allElements.length - 1, windowStart + 1000);
        }
        if (windowStart > windowEnd) {
            windowStart = Math.max(0, searchIndex);
            windowEnd = Math.min(allElements.length - 1, windowStart + 500);
        }

        var result = findSoftInWindow(allElements, windowStart, windowEnd, imgInfo.anchorsAbove, imgInfo.anchorsBelow);
        if (result.index >= 0 && result.score > 0) {
            imgInfo.foundIndex = result.index;
            imgInfo.isBefore = result.isBefore;
            searchIndex = result.index + 1;
        }
    }

    // ==================================================
    // ФАЗА 4: ВСТАВКА МАРКЕРОВ
    // ==================================================

    var markersInserted = 0;
    var markersSkipped = 0;
    var skippedList = []; // Полный список пропущенных для отчёта

    window.external.BeginUndoUnit(document, "Расстановка маркеров картинок");

    for (var im = 0; im < imagesData.length; im++) {
        var imgInfo = imagesData[im];

        if (imgInfo.foundIndex >= 0) {
            var targetElement = allElements[imgInfo.foundIndex].element;
            if (imgInfo.isBefore) {
                insertMarkerBefore(targetElement, imgInfo.emptyAbove, imgInfo.emptyBelow);
            } else {
                insertMarkerAfter(targetElement, imgInfo.emptyAbove, imgInfo.emptyBelow);
            }
            markersInserted++;
        } else {
            markersSkipped++;
            // Собираем информацию для отчёта
            var skipInfo = "";
            skipInfo += "Картинка #" + imgInfo.imageNumber + " (" + imgInfo.name + ")\r\n";
            skipInfo += "  Секция в исходнике: " + imgInfo.sectionIndex + "\r\n";
            skipInfo += "  Реперы сверху (" + imgInfo.anchorsAbove.length + "): ";
            skipInfo += imgInfo.anchorsAbove.length > 0 ? imgInfo.anchorsAbove.join(" | ") : "нет";
            skipInfo += "\r\n";
            skipInfo += "  Реперы снизу (" + imgInfo.anchorsBelow.length + "): ";
            skipInfo += imgInfo.anchorsBelow.length > 0 ? imgInfo.anchorsBelow.join(" | ") : "нет";
            skipInfo += "\r\n";

            // Глобальный поиск реперов
            if (imgInfo.anchorsAbove.length > 0) {
                var abovePos = globalSearchAnchors(allElements, imgInfo.anchorsAbove, 0);
                if (abovePos.length > 0) {
                    skipInfo += "  ✓ Верхние реперы найдены в " + abovePos.length + " элементах:\r\n";
                    for (var ap = 0; ap < Math.min(5, abovePos.length); ap++) {
                        var preview = abovePos[ap].text.length > 80 ? abovePos[ap].text.substring(0, 80) + "..." : abovePos[ap].text;
                        skipInfo += "    #" + abovePos[ap].index + " (совп:" + abovePos[ap].count + ") \"" + preview + "\"\r\n";
                    }
                } else {
                    skipInfo += "  ✗ Верхние реперы НЕ НАЙДЕНЫ нигде\r\n";
                }
            }
            if (imgInfo.anchorsBelow.length > 0) {
                var belowPos = globalSearchAnchors(allElements, imgInfo.anchorsBelow, 0);
                if (belowPos.length > 0) {
                    skipInfo += "  ✓ Нижние реперы найдены в " + belowPos.length + " элементах:\r\n";
                    for (var bp = 0; bp < Math.min(5, belowPos.length); bp++) {
                        var preview = belowPos[bp].text.length > 80 ? belowPos[bp].text.substring(0, 80) + "..." : belowPos[bp].text;
                        skipInfo += "    #" + belowPos[bp].index + " (совп:" + belowPos[bp].count + ") \"" + preview + "\"\r\n";
                    }
                } else {
                    skipInfo += "  ✗ Нижние реперы НЕ НАЙДЕНЫ нигде\r\n";
                }
            }
            skipInfo += "\r\n";
            skippedList.push(skipInfo);
        }
    }

    window.external.EndUndoUnit(document);

    // ==================================================
    // СОХРАНЕНИЕ ОТЧЁТА О ПРОПУЩЕННЫХ
    // ==================================================

    var reportSaved = false;
    var reportError = "";

    if (skippedList.length > 0) {
        try {
            var reportFso = new ActiveXObject("Scripting.FileSystemObject");
            try { reportFso.CreateFolder(workFolder); } catch (e) {}
            var reportPath = workFolder + "\\" + reportFileName;
            var reportFile = reportFso.CreateTextFile(reportPath, true, true);
            
            reportFile.Write("========================================\r\n");
            reportFile.Write("ОТЧЁТ О ПРОПУЩЕННЫХ КАРТИНКАХ\r\n");
            reportFile.Write("========================================\r\n");
            reportFile.Write("\r\n");
            reportFile.Write("Скрипт: " + scriptName + " v" + version + "\r\n");
            reportFile.Write("Дата: " + new Date() + "\r\n");
            reportFile.Write("\r\n");
            reportFile.Write("Всего картинок в карте: " + imagesData.length + "\r\n");
            reportFile.Write("Успешно вставлено: " + markersInserted + "\r\n");
            reportFile.Write("Пропущено: " + markersSkipped + "\r\n");
            reportFile.Write("\r\n");
            reportFile.Write("========================================\r\n");
            reportFile.Write("СПИСОК ПРОПУЩЕННЫХ КАРТИНОК\r\n");
            reportFile.Write("========================================\r\n");
            reportFile.Write("\r\n");

            for (var si = 0; si < skippedList.length; si++) {
                reportFile.Write(skippedList[si]);
            }

            reportFile.Write("========================================\r\n");
            reportFile.Write("КОНЕЦ ОТЧЁТА\r\n");
            reportFile.Write("========================================\r\n");

            reportFile.Close();
            reportSaved = true;
        } catch (e) {
            reportError = e.message;
        }
    }

    // ==================================================
    // СТАТИСТИКА
    // ==================================================

    var endTime = new Date().getTime();
    var elapsed = (endTime - startTime) / 1000;
    var elapsedStr = Math.round(elapsed * 1000) / 1000 + "";

    if (showStatistics == 1) {
        var msg = scriptName + "\nver. " + version + "\n----------------------------------------\n\n";
        msg += "✓ Картинок в карте: " + imagesData.length + "\n";
        msg += "✓ Всего текстовых элементов: " + allElements.length + "\n";
        if (totalFilteredAnchors > 0) msg += "✓ Отфильтровано коротких реперов: " + totalFilteredAnchors + "\n";
        msg += "\n✓ Вставлено: " + markersInserted + "\n";
        if (markersSkipped > 0) {
            msg += "✗ Пропущено (не найдено): " + markersSkipped + "\n";
        }
        msg += "\nТип вставки: " + (insertType == "marker" ? "текстовый маркер \"" + markerText + "\"" : "пустая картинка #undefined") + "\n";
        msg += "Алгоритм: двухпроходный\n";
        msg += "\nВремя выполнения: " + elapsedStr + " сек.\n";

        if (reportSaved) {
            msg += "\n✓ Отчёт сохранён:\n  " + workFolder + "\\" + reportFileName + "\n";
        } else if (skippedList.length > 0) {
            msg += "\n✗ Не удалось сохранить отчёт: " + reportError + "\n";
        }

        // Краткая диагностика — только первые 3 в окне
        if (skippedList.length > 0) {
            msg += "\n----------------------------------------\n";
            msg += "ПЕРВЫЕ 3 ПРОПУЩЕННЫЕ (подробности в отчёте):\n";
            msg += "----------------------------------------\n";
            for (var sd = 0; sd < Math.min(3, skippedList.length); sd++) {
                msg += "\n" + skippedList[sd].replace(/\r\n/g, "\n");
            }
            if (skippedList.length > 3) {
                msg += "\n... и ещё " + (skippedList.length - 3) + " картинок (см. файл отчёта)";
            }
        }

        MsgBox(msg);
    } else {
        if (markersSkipped > 0) MsgBox(scriptName + "\n" + "ver. " + version + "\n----------------------------------------\n✗ Пропущено маркеров: " + markersSkipped + " из " + imagesData.length);
    }
}
