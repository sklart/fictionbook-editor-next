// Скрипт "Создать реперную карту fb2 документа для переноса иллюстраций" для редактора FBE
// version 2.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Первый скрипт из комплекта для переноса иллюстраций между fb2-документами.
// Работает в паре со Скриптом 2 "Расставить маркеры иллюстраций по реперной карте.js".

// Данный скрипт предназначен для создания TXT-файла с реперной картой иллюстраций fb2 документа
// для последующего переноса иллюстраций в откорректированную версию fb2-документа
// (в другой документ, например, с отредактированным текстом).
// Скрипт создает реперную txt карту иллюстраций в папке с текущим fb2 документом
// или в жестко заданной в настройках папке, например D:\\FBE_Compare.
// В данную папку скрипт помещает созданный txt файл fb2_reper_map.txt
// с реперной картой расположения иллюстраций в исходном файле.
// Также в данную папку помещается файл с отчетом об ошибках (при их наличии)
// после работы второго скрипта - расстановки маркеров иллюстраций в целевом документе.
// Скрипт учитывает наличие пустых строк вокруг иллюстраций.
// Сохраняет реперную карту в TXT через FileSystemObject.
// Настройка перезаписи/копирования файла карты в соответствующую папку.
// Скрипт не вносит никаких изменений в fb2 документ.
// Режим работы: обычный или тихий.

// Как работает эта пара скриптов:
// Создать реперную карту fb2 документа для переноса иллюстраций.js
// и
// Расставить маркеры иллюстраций по реперной карте.js

// Открываем исходный fb2 документ с иллюстрациями.
// Запускаем этот скрипт для создания реперной карты данного документа.
// Открываем целевой документ с отредактированным текстом и без иллюстраций.
// Запускаем второй скрипт Расставить маркеры иллюстраций по реперной карте.js
// Второй скрипт расставляет в целевом документе текстовые маркеры типа zzz_pic
// или сразу пустые картинки (в зависимости от включенных настроек во втором скрипте)
// на местах, максимально совпадающих с исходным документом.
// При наличии ошибок расстановки второй скрипт создает файл отчета в папке с текущим fb2 документом
// или в жестко заданной в настройках папке, например D:\\FBE_Compare.

// Далее можно:
// Проверить в целевом документе расстановку текстовых маркеров,
// при необходимости переставить отдельные маркеры вручную.
// Заменить текстовые маркеры zzz_pic на <image l:href="#undefined"/> глобальной заменой в режиме XML кода.
// Подцепить на места пустышек реальные иллюстрации скриптом "15_Расставить иллюстрации по заданным местам.js"

// version 2.1, 30.06.2026
//======================================

// ==================================================
// ГЛОБАЛЬНЫЕ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ==================================================

function getLocalHref(name) {
    var name1 = name;
    if (name1.charAt(0) != "#") {
        return '"';
    }
    var thg = new RegExp("main\\.html\\#", "i");
    var srch10 = name1.search(thg);
    if (srch10 == -1) {
        name1 = name1.substring(1, name1.length);
    } else {
        name1 = name1.substring(srch10 + 10, name1.length);
    }
    return name1;
}

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
    var scriptName = "Создать реперную карту fb2 документа для переноса иллюстраций";
    var version = "2.1";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    // --- Настройка путей ---
    // Запасная папка (используется, если автоопределение не сработало)
    var workFolder = "D:\\FBE_Compare";
    var mapFileName = "fb2_reper_map.txt";        // Имя файла реперной карты

    // --- Настройка сохранения ---
    var overwriteFile = 0; // 0 — перезаписывать, 1 — создавать копию с нумерацией

    var shortParagraphThreshold = 50;              // Порог короткого абзаца в символах (меньше — короткий)
    var wordsPerAnchor = 3;                        // Сколько слов брать из начала, середины и конца абзаца
    var showStatistics = 1;                        // 1 — показывать статистику, 0 — тихий режим (только ошибки)
    var processNotesSection = 0;                   // Обрабатывать раздел сносок (примечаний): 0 — нет, 1 — да
    var processCommentsSection = 0;                // Обрабатывать раздел комментариев: 0 — нет, 1 — да

    // ==================================================
    // ОПРЕДЕЛЕНИЕ ПАПКИ С ДОКУМЕНТОМ
    // ==================================================

    var documentFolder = "";
    var fb2FileName = "";

    // --- Метод 1: Штатное API FBE ---
    try {
        documentFolder = window.external.GetDocumentDirectory();
        fb2FileName = window.external.GetDocumentFileName();
    } catch(e) {}

    // --- Метод 2: PowerShell (заголовок окна) ---
    if (documentFolder == "") {
        var shell, fso;
        try {
            shell = new ActiveXObject("WScript.Shell");
            fso = new ActiveXObject("Scripting.FileSystemObject");
        } catch(e) {}

        if (shell && fso) {
            var tempFile = "C:\\__fbe_path_result.txt";
            try {
                var psCmd = "powershell -ExecutionPolicy Bypass -command \"(Get-Process -Name 'fbe' -ErrorAction SilentlyContinue).MainWindowTitle\" > \"" + tempFile + "\" 2>&1";
                shell.Run(psCmd, 0, true);

                if (fso.FileExists(tempFile)) {
                    var file = fso.OpenTextFile(tempFile, 1, false, -1);
                    var title = "";
                    if (!file.AtEndOfStream) {
                        title = file.ReadLine();
                        title = title.replace(/^\s+|\s+$/g, "");
                    }
                    file.Close();
                    try { fso.DeleteFile(tempFile); } catch(e) {}

                    if (title.length > 0) {
                        var pathMatch = title.match(/[A-Za-z]:\\.+?\.fb2/i);
                        if (pathMatch) {
                            var fb2Path = pathMatch[0];
                            var lastSlash = fb2Path.lastIndexOf("\\");
                            if (lastSlash != -1) {
                                documentFolder = fb2Path.substring(0, lastSlash);
                                fb2FileName = fb2Path.substring(lastSlash + 1);
                            }
                        }
                    }
                }
            } catch(e) {}
        }
    }

    // --- Метод 3: Временный файл + CMD-поиск ---
    if (documentFolder == "") {
        try {
            if (!shell) shell = new ActiveXObject("WScript.Shell");
            if (!fso) fso = new ActiveXObject("Scripting.FileSystemObject");
        } catch(e) {}

        if (shell && fso) {
            var binObjects = document.all.binobj.getElementsByTagName("DIV");
            var binData = "";
            if (binObjects.length > 0) {
                binData = binObjects[0].base64data;
            } else {
                binData = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPj/HwADBwIAMCbHYQAAAABJRU5ErkJggg==";
            }

            var testId = "__fbe_path_test_" + new Date().getTime();
            try {
                window.external.SaveBinary(testId, binData, 0);
            } catch(e) {}

            var tempFile = "C:\\__fbe_path_result.txt";
            var drivesToCheck = ["C:"];
            try {
                var allDrives = fso.Drives;
                var dcEnum = new Enumerator(allDrives);
                drivesToCheck = [];
                for (; !dcEnum.atEnd(); dcEnum.moveNext()) {
                    var drv = dcEnum.item();
                    if (drv.IsReady) drivesToCheck.push(drv.DriveLetter + ":");
                }
            } catch(e) {}

            var foundPath = "";
            for (var d = 0; d < drivesToCheck.length; d++) {
                var disk = drivesToCheck[d];
                var cmd = "cmd /u /c dir /s /b \"" + disk + "\\" + testId + "*\" > \"" + tempFile + "\" 2>nul";
                try {
                    shell.Run(cmd, 0, true);
                    if (fso.FileExists(tempFile)) {
                        var resultFile = fso.OpenTextFile(tempFile, 1, false, -1);
                        var line = "";
                        if (!resultFile.AtEndOfStream) {
                            line = resultFile.ReadLine();
                            line = line.replace(/^\s+|\s+$/g, "");
                            if (line.length > 0 && line.indexOf(testId) != -1) {
                                foundPath = line;
                            }
                        }
                        resultFile.Close();
                        try { fso.DeleteFile(tempFile); } catch(e) {}
                        if (foundPath != "") break;
                    }
                } catch(ec) {}
            }

            if (foundPath != "") {
                var lastSlash2 = foundPath.lastIndexOf("\\");
                if (lastSlash2 != -1) {
                    documentFolder = foundPath.substring(0, lastSlash2);
                    fb2FileName = "(неизвестно)";
                }
                try { fso.DeleteFile(foundPath); } catch(e) {}
            }
        }
    }

    // --- Используем запасную папку, если автоопределение не сработало ---
    if (documentFolder == "") {
        documentFolder = workFolder;
    }

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

    function isEmptyParagraph(paragraph) {
        if (!paragraph || paragraph.nodeType != 1 || paragraph.nodeName != "P") return false;
        var text = getElementText(paragraph);
        return isEmptyText(text);
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

    function extractWordsFromOriginal(text) {
        var words = [];
        if (!text || text.length == 0) return words;
        var currentWord = "";
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            var code = ch.charCodeAt(0);
            var isAlphaNum = (code >= 48 && code <= 57) ||
                             (code >= 65 && code <= 90) ||
                             (code >= 97 && code <= 122) ||
                             (code >= 1040 && code <= 1103) ||
                             code == 1025 || code == 1105;
            if (isAlphaNum) {
                currentWord += ch.toLowerCase();
            } else {
                if (currentWord.length > 0) {
                    words.push(currentWord);
                    currentWord = "";
                }
            }
        }
        if (currentWord.length > 0) {
            words.push(currentWord);
        }
        return words;
    }

    function isTitle(node) {
        if (!node || node.nodeType != 1 || node.nodeName != "DIV") return false;
        var cls = node.className || "";
        return cls == "title" || cls == "subtitle";
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

    function isSection(node) {
        if (!node || node.nodeType != 1 || node.nodeName != "DIV") return false;
        return (node.className || "") == "section";
    }

    function isBody(node) {
        if (!node || node.nodeType != 1 || node.nodeName != "DIV") return false;
        return (node.className || "") == "body";
    }

    function getParagraphLength(paragraph) {
        var text = getElementText(paragraph);
        return text.length;
    }

    function getAnchorsFromText(text) {
        var words = extractWordsFromOriginal(text);
        var anchors = [];

        if (words.length == 0) return anchors;

        var n = wordsPerAnchor;
        if (n > words.length) n = words.length;

        var anchorBegin = "";
        for (var i = 0; i < n; i++) {
            if (i > 0) anchorBegin += " ";
            anchorBegin += words[i];
        }
        anchors.push(anchorBegin);

        if (words.length >= n * 2) {
            var midStart = Math.floor((words.length - n) / 2);
            var anchorMid = "";
            for (var i = midStart; i < midStart + n; i++) {
                if (i > midStart) anchorMid += " ";
                anchorMid += words[i];
            }
            anchors.push(anchorMid);
        } else if (words.length > n) {
            var anchorMid = "";
            var count = 0;
            for (var i = n; i < words.length - n; i++) {
                if (count > 0) anchorMid += " ";
                anchorMid += words[i];
                count++;
            }
            if (count > 0) anchors.push(anchorMid);
        }

        if (words.length > n) {
            var anchorEnd = "";
            for (var i = words.length - n; i < words.length; i++) {
                if (i > words.length - n) anchorEnd += " ";
                anchorEnd += words[i];
            }
            anchors.push(anchorEnd);
        }

        return anchors;
    }

    function getAnchorsFromParagraph(paragraph) {
        var text = getElementText(paragraph);
        return getAnchorsFromText(text);
    }

    function getSectionTitle(sectionElement) {
        if (!sectionElement) return "";
        var children = sectionElement.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (isTitle(child)) {
                return getElementText(child);
            }
        }
        return "";
    }

    function getBodyTitle(bodyElement) {
        if (!bodyElement) return "";
        var children = bodyElement.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (isTitle(child)) {
                return getElementText(child);
            }
        }
        return "";
    }

    function countEmptyLinesAround(element, direction) {
        var count = 0;
        var current = element;
        var parent = element.parentNode;

        while (true) {
            if (direction == -1) {
                current = current.previousSibling;
            } else {
                current = current.nextSibling;
            }

            if (!current) break;
            if (current.parentNode != parent) break;

            if (current.nodeType == 3) {
                if (isEmptyText(current.nodeValue || "")) {
                    continue;
                } else {
                    break;
                }
            }

            if (current.nodeType != 1) continue;

            if (current.nodeName == "P") {
                if (isEmptyParagraph(current)) {
                    count++;
                    continue;
                } else {
                    break;
                }
            }

            break;
        }

        return count;
    }

    function collectImagesRecursive(container, sectionIndex, bodyType, resultArray) {
        if (!container) return;
        var children = container.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType != 1) continue;

            if (child.nodeName == "DIV") {
                var cls = child.className || "";
                if (cls == "image") {
                    var href = child.getAttribute("href") || "";
                    if (href.length > 0 && href.charAt(0) == "#") {
                        var localName = getLocalHref(href);
                        resultArray.push({
                            name: localName,
                            sectionIndex: sectionIndex,
                            bodyType: bodyType,
                            element: child
                        });
                    }
                } else if (cls == "section") {
                    collectImagesRecursive(child, sectionIndex, bodyType, resultArray);
                } else {
                    collectImagesRecursive(child, sectionIndex, bodyType, resultArray);
                }
            }
        }
    }

    function collectSectionsRecursive(container, parentIndex, level, bodyType, sectionsArr, imagesArr) {
        if (!container) return;
        var children = container.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType != 1) continue;

            if (isSection(child)) {
                sectionsArr.totalCount++;
                var secIndex = sectionsArr.totalCount;
                var secTitle = getSectionTitle(child);
                var secTitleClean = cleanText(secTitle);

                var directImages = [];
                collectImagesFromSectionDirect(child, secIndex, bodyType, directImages);
                for (var di = 0; di < directImages.length; di++) {
                    imagesArr.push(directImages[di]);
                }

                sectionsArr.push({
                    index: secIndex,
                    parent: parentIndex,
                    level: level,
                    title: secTitleClean,
                    titleOriginal: secTitle,
                    imageCount: directImages.length,
                    bodyType: bodyType,
                    element: child
                });

                collectSectionsRecursive(child, secIndex, level + 1, bodyType, sectionsArr, imagesArr);
            }
        }
    }

    function collectImagesFromSectionDirect(sectionElement, sectionIndex, bodyType, resultArray) {
        if (!sectionElement) return;
        var children = sectionElement.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType != 1) continue;

            if (child.nodeName == "DIV") {
                var cls = child.className || "";
                if (cls == "image") {
                    var href = child.getAttribute("href") || "";
                    if (href.length > 0 && href.charAt(0) == "#") {
                        var localName = getLocalHref(href);
                        resultArray.push({
                            name: localName,
                            sectionIndex: sectionIndex,
                            bodyType: bodyType,
                            element: child
                        });
                    }
                } else if (cls != "section") {
                    collectImagesFromSectionDirect(child, sectionIndex, bodyType, resultArray);
                }
            }
        }
    }

    function areConsecutiveImages(img1, img2) {
        if (!img1 || !img2) return false;
        var elem1 = img1.element;
        var elem2 = img2.element;

        if (elem1.parentNode != elem2.parentNode) return false;

        var current = elem1.nextSibling;
        while (current) {
            if (current == elem2) return true;
            if (current.nodeType == 3) {
                if (!isEmptyText(current.nodeValue || "")) return false;
                current = current.nextSibling;
                continue;
            }
            if (current.nodeType != 1) {
                current = current.nextSibling;
                continue;
            }
            if (isImage(current)) {
                current = current.nextSibling;
                continue;
            }
            if (current.nodeName == "P") {
                var pText = "";
                var pChildren = current.childNodes;
                for (var pc = 0; pc < pChildren.length; pc++) {
                    var pChild = pChildren[pc];
                    if (pChild.nodeType == 3) {
                        pText += pChild.nodeValue;
                    } else if (pChild.nodeType == 1) {
                        var subChildren = pChild.childNodes;
                        for (var sc = 0; sc < subChildren.length; sc++) {
                            if (subChildren[sc].nodeType == 3) {
                                pText += subChildren[sc].nodeValue;
                            }
                        }
                    }
                }
                if (!isEmptyText(pText)) return false;
                current = current.nextSibling;
                continue;
            }
            return false;
        }
        return false;
    }

    function collectAnchorsDirection(imageElement, direction, ignoreImages) {
        var anchors = [];
        var shortParagraphs = [];
        var shortParagraphsTotalLength = 0;
        var current = imageElement;
        var container = imageElement.parentNode;

        while (true) {
            if (direction == -1) {
                current = current.previousSibling;
            } else {
                current = current.nextSibling;
            }

            if (!current) break;
            if (current.parentNode != container) break;

            if (current.nodeType == 3) {
                if (isEmptyText(current.nodeValue || "")) continue;
                continue;
            }

            if (current.nodeType != 1) continue;

            if (isSection(current) || isBody(current)) break;

            if (isImage(current)) {
                if (ignoreImages) continue;
                else break;
            }

            if (isTitle(current)) {
                var titleText = getElementText(current);
                if (!isEmptyText(titleText)) {
                    var titleAnchors = getAnchorsFromText(titleText);
                    for (var ta = 0; ta < titleAnchors.length; ta++) {
                        anchors.push(titleAnchors[ta]);
                    }
                }
                break;
            }

            if (current.nodeName == "DIV") {
                var cls2 = current.className || "";
                if (cls2 == "epigraph" || cls2 == "cite" || cls2 == "annotation") {
                    var blockText = getElementText(current);
                    if (!isEmptyText(blockText)) {
                        var blockAnchors = getAnchorsFromText(blockText);
                        for (var ba = 0; ba < blockAnchors.length; ba++) {
                            anchors.push(blockAnchors[ba]);
                        }
                    }
                    continue;
                }
            }

            if (current.nodeName == "P") {
                if (isEmptyParagraph(current)) continue;

                var pLength = getParagraphLength(current);
                if (pLength >= shortParagraphThreshold) {
                    var pAnchors = getAnchorsFromParagraph(current);
                    for (var a = 0; a < pAnchors.length; a++) {
                        anchors.push(pAnchors[a]);
                    }
                    break;
                } else {
                    shortParagraphs.push(current);
                    shortParagraphsTotalLength += pLength;
                    if (shortParagraphs.length >= 6 || shortParagraphsTotalLength >= shortParagraphThreshold * 2) {
                        var combinedText = "";
                        for (var s = 0; s < shortParagraphs.length; s++) {
                            if (s > 0) combinedText += " ";
                            combinedText += getElementText(shortParagraphs[s]);
                        }
                        var combinedAnchors = getAnchorsFromText(combinedText);
                        for (var ca = 0; ca < combinedAnchors.length; ca++) {
                            anchors.push(combinedAnchors[ca]);
                        }
                        break;
                    }
                }
            }
        }

        if (shortParagraphs.length > 0 && anchors.length == 0) {
            var combinedText = "";
            for (var s = 0; s < shortParagraphs.length; s++) {
                if (s > 0) combinedText += " ";
                combinedText += getElementText(shortParagraphs[s]);
            }
            var combinedAnchors = getAnchorsFromText(combinedText);
            for (var ca = 0; ca < combinedAnchors.length; ca++) {
                anchors.push(combinedAnchors[ca]);
            }
        }

        if (anchors.length == 0) {
            anchors = climbLevelsForAnchors(imageElement, direction, ignoreImages);
        }

        return anchors;
    }

    function climbLevelsForAnchors(imageElement, direction, ignoreImages) {
        var currentElement = imageElement;

        while (currentElement) {
            var container = currentElement.parentNode;
            if (!container) break;

            var sibling = findNextSiblingSkipEmpty(currentElement, direction, container);

            if (sibling) {
                if (isSection(sibling)) {
                    var textElement = findFirstTextInSection(sibling, direction);
                    if (textElement) {
                        return extractAnchorsFromElement(textElement);
                    }
                } else {
                    var textElement = findTextInElement(sibling, direction);
                    if (textElement) {
                        return extractAnchorsFromElement(textElement);
                    }
                }
            }

            if (isSection(container)) {
                var innerSection = findInnerSection(container, currentElement, direction);
                if (innerSection) {
                    var textElement = findFirstTextInSection(innerSection, direction);
                    if (textElement) {
                        return extractAnchorsFromElement(textElement);
                    }
                }
            }

            if (isBody(container)) break;

            currentElement = container;
        }

        return [];
    }

    function findNextSiblingSkipEmpty(element, direction, parentContainer) {
        var current = element;

        while (current && current.parentNode != parentContainer) {
            current = current.parentNode;
        }

        if (!current || current.parentNode != parentContainer) return null;

        var sibling = (direction == 1) ? current.nextSibling : current.previousSibling;

        while (sibling) {
            if (sibling.nodeType == 3) {
                if (!isEmptyText(sibling.nodeValue || "")) return sibling;
                sibling = (direction == 1) ? sibling.nextSibling : sibling.previousSibling;
                continue;
            }
            if (sibling.nodeType == 1) {
                if (sibling.nodeName == "P" && isEmptyParagraph(sibling)) {
                    sibling = (direction == 1) ? sibling.nextSibling : sibling.previousSibling;
                    continue;
                }
                return sibling;
            }
            sibling = (direction == 1) ? sibling.nextSibling : sibling.previousSibling;
        }

        return null;
    }

    function findInnerSection(container, element, direction) {
        if (!container) return null;
        var children = container.childNodes;

        var pos = -1;
        for (var i = 0; i < children.length; i++) {
            if (children[i] == element) {
                pos = i;
                break;
            }
        }
        if (pos == -1) {
            var ancestor = element;
            while (ancestor && ancestor.parentNode != container) {
                ancestor = ancestor.parentNode;
            }
            if (ancestor && ancestor.parentNode == container) {
                for (var i = 0; i < children.length; i++) {
                    if (children[i] == ancestor) {
                        pos = i;
                        break;
                    }
                }
            }
        }
        if (pos == -1) return null;

        if (direction == 1) {
            for (var i = pos + 1; i < children.length; i++) {
                if (isSection(children[i])) return children[i];
            }
        } else {
            for (var i = pos - 1; i >= 0; i--) {
                if (isSection(children[i])) return children[i];
            }
        }

        return null;
    }

    function extractAnchorsFromElement(element) {
        if (!element) return [];
        if (element.nodeName == "P") {
            return getAnchorsFromParagraph(element);
        }
        if (isTitle(element)) {
            return getAnchorsFromText(getElementText(element));
        }
        return getAnchorsFromText(getElementText(element));
    }

    function findFirstTextInSection(section, direction) {
        if (!section) return null;

        var children = section.childNodes;
        if (direction == 1) {
            for (var i = 0; i < children.length; i++) {
                var found = findTextInElement(children[i], direction);
                if (found) return found;
            }
        } else {
            for (var i = children.length - 1; i >= 0; i--) {
                var found = findTextInElement(children[i], direction);
                if (found) return found;
            }
        }

        return null;
    }

    function findTextInElement(element, direction) {
        if (!element || element.nodeType != 1) return null;

        if (element.nodeName == "P" && !isEmptyParagraph(element)) {
            return element;
        }

        if (isTitle(element)) {
            var titleText = getElementText(element);
            if (!isEmptyText(titleText)) return element;
        }

        if (element.nodeName == "DIV") {
            var cls = element.className || "";
            if (cls == "image") return null;
            if (cls == "section") {
                return findFirstTextInSection(element, direction);
            }
            if (cls == "epigraph" || cls == "cite" || cls == "annotation") {
                var blockText = getElementText(element);
                if (!isEmptyText(blockText)) return element;
            }
        }

        var children = element.childNodes;
        if (direction == 1) {
            for (var i = 0; i < children.length; i++) {
                if (children[i].nodeType == 1) {
                    var found = findTextInElement(children[i], direction);
                    if (found) return found;
                }
            }
        } else {
            for (var i = children.length - 1; i >= 0; i--) {
                if (children[i].nodeType == 1) {
                    var found = findTextInElement(children[i], direction);
                    if (found) return found;
                }
            }
        }

        return null;
    }

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    var startTime = new Date().getTime();

    try {
        var fbwBody = document.getElementById("fbw_body");
        if (!fbwBody) {
            if (showStatistics == 1) {
                MsgBox(scriptName + "\n" + "ver. " + version + "\n----------------------------------------\n" +
                       "✗ Ошибка: не найден fbw_body.\n");
            }
            return;
        }

        var bodyElements = [];
        var allDivs = document.getElementsByTagName("DIV");
        for (var d = 0; d < allDivs.length; d++) {
            var div = allDivs[d];
            if (div.className == "body") {
                var fbname = div.getAttribute("fbname") || "";
                if (fbname == "") {
                    bodyElements.push({ element: div, type: "main" });
                } else if (fbname == "notes" && processNotesSection == 1) {
                    bodyElements.push({ element: div, type: "notes" });
                } else if (fbname == "comments" && processCommentsSection == 1) {
                    bodyElements.push({ element: div, type: "comments" });
                }
            }
        }

        if (bodyElements.length == 0) {
            if (showStatistics == 1) {
                MsgBox(scriptName + "\n" + "ver. " + version + "\n----------------------------------------\n" +
                       "✗ Ошибка: не найдены разделы body.\n");
            }
            return;
        }

        // ==================================================
        // ФАЗА 1: СБОР ДАННЫХ (только чтение)
        // ==================================================

        var bookTitle = "";
        var bookTitleClean = "";
        var allSections = [];
        allSections.totalCount = 0;
        var allImages = [];

        for (var b = 0; b < bodyElements.length; b++) {
            var bodyInfo = bodyElements[b];
            var bodyElement = bodyInfo.element;
            var bodyType = bodyInfo.type;

            if (bodyType == "main") {
                bookTitle = getBodyTitle(bodyElement);
                bookTitleClean = cleanText(bookTitle);
            }

            var bodyChildren = bodyElement.childNodes;
            for (var c = 0; c < bodyChildren.length; c++) {
                var child = bodyChildren[c];
                if (child.nodeType != 1) continue;
                if (child.nodeName == "DIV") {
                    var cls = child.className || "";
                    if (cls == "image") {
                        var href = child.getAttribute("href") || "";
                        if (href.length > 0 && href.charAt(0) == "#") {
                            var localName = getLocalHref(href);
                            allImages.push({
                                name: localName,
                                sectionIndex: 0,
                                bodyType: bodyType,
                                element: child
                            });
                        }
                    } else if (cls != "section") {
                        collectImagesRecursive(child, 0, bodyType, allImages);
                    }
                }
            }

            collectSectionsRecursive(bodyElement, 0, 1, bodyType, allSections, allImages);
        }

        var totalSections = allSections.totalCount;
        var totalImages = allImages.length;

        if (totalImages == 0) {
            if (showStatistics == 1) {
                MsgBox(scriptName + "\n" + "ver. " + version + "\n----------------------------------------\n" +
                       "✗ В документе не найдено блочных картинок.\n");
            }
            return;
        }

        var imageGroups = [];
        var currentGroup = [];

        for (var ai = 0; ai < allImages.length; ai++) {
            var img = allImages[ai];

            if (currentGroup.length == 0) {
                currentGroup.push(img);
            } else {
                var prevImg = currentGroup[currentGroup.length - 1];
                if (areConsecutiveImages(prevImg, img)) {
                    currentGroup.push(img);
                } else {
                    imageGroups.push(currentGroup);
                    currentGroup = [img];
                }
            }
        }
        if (currentGroup.length > 0) {
            imageGroups.push(currentGroup);
        }

        var imagesInfo = [];

        for (var gi = 0; gi < imageGroups.length; gi++) {
            var group = imageGroups[gi];
            var firstImage = group[0];
            var lastImage = group[group.length - 1];

            var anchorsAbove = collectAnchorsDirection(firstImage.element, -1, true);
            var anchorsBelow = collectAnchorsDirection(lastImage.element, 1, true);

            var emptyAbove = countEmptyLinesAround(firstImage.element, -1);
            var emptyBelow = countEmptyLinesAround(lastImage.element, 1);

            for (var gi2 = 0; gi2 < group.length; gi2++) {
                imagesInfo.push({
                    name: group[gi2].name,
                    sectionIndex: group[gi2].sectionIndex,
                    bodyType: group[gi2].bodyType,
                    anchorsAbove: anchorsAbove,
                    anchorsBelow: anchorsBelow,
                    emptyAbove: emptyAbove,
                    emptyBelow: emptyBelow
                });
            }
        }

        // ==================================================
        // ФАЗА 2: ЗАПИСЬ В TXT
        // ==================================================

        var fileContent = "";
        fileContent += "FBE_REPER_MAP|1.0\r\n";
        fileContent += "TOTAL_IMAGES|" + totalImages + "\r\n";
        fileContent += "TOTAL_SECTIONS|" + totalSections + "\r\n";
        if (bookTitle.length > 0) {
            fileContent += "BOOK_TITLE|" + bookTitle + "|" + bookTitleClean + "\r\n";
        } else {
            fileContent += "BOOK_TITLE|NO_TITLE|NO_TITLE\r\n";
        }
        fileContent += "\r\n";

        for (var si = 0; si < allSections.length; si++) {
            var sec = allSections[si];
            if (sec && sec.index) {
                fileContent += "SECTION|" + sec.index + "|" + sec.parent + "|" + sec.level + "|" + sec.title + "|" + sec.imageCount + "\r\n";
            }
        }
        fileContent += "\r\n";

        for (var ii = 0; ii < imagesInfo.length; ii++) {
            var imgInfo = imagesInfo[ii];
            fileContent += "IMAGE|" + (ii + 1) + "|" + imgInfo.name + "|" + imgInfo.sectionIndex + "\r\n";
            fileContent += "EMPTY_ABOVE|" + (ii + 1) + "|" + imgInfo.emptyAbove + "\r\n";
            fileContent += "EMPTY_BELOW|" + (ii + 1) + "|" + imgInfo.emptyBelow + "\r\n";

            if (imgInfo.anchorsAbove.length > 0) {
                fileContent += "ANCHOR_ABOVE|" + (ii + 1);
                for (var aa = 0; aa < imgInfo.anchorsAbove.length; aa++) {
                    fileContent += "|" + imgInfo.anchorsAbove[aa];
                }
                fileContent += "\r\n";
            } else {
                fileContent += "ANCHOR_ABOVE|" + (ii + 1) + "|NO_TEXT\r\n";
            }

            if (imgInfo.anchorsBelow.length > 0) {
                fileContent += "ANCHOR_BELOW|" + (ii + 1);
                for (var ab = 0; ab < imgInfo.anchorsBelow.length; ab++) {
                    fileContent += "|" + imgInfo.anchorsBelow[ab];
                }
                fileContent += "\r\n";
            } else {
                fileContent += "ANCHOR_BELOW|" + (ii + 1) + "|NO_TEXT\r\n";
            }
        }

        // Запись файла
        var fullPath = documentFolder + "\\" + mapFileName;
        var writeSuccess = false;
        var writeError = "";

        try {
            var fsoWriter = new ActiveXObject("Scripting.FileSystemObject");
            try {
                fsoWriter.CreateFolder(documentFolder);
            } catch (e) {}

            // Проверяем, существует ли файл, и создаём копию если нужно
            if (overwriteFile == 1 && fsoWriter.FileExists(fullPath)) {
                var baseName = mapFileName;
                var dotPos = baseName.lastIndexOf(".");
                var nameWithoutExt = baseName;
                var ext = "";
                if (dotPos != -1) {
                    nameWithoutExt = baseName.substring(0, dotPos);
                    ext = baseName.substring(dotPos);
                }

                var counter = 1;
                var newPath;
                do {
                    newPath = documentFolder + "\\" + nameWithoutExt + "_" + counter + ext;
                    counter++;
                } while (fsoWriter.FileExists(newPath));

                fullPath = newPath;
            }

            var file = fsoWriter.CreateTextFile(fullPath, true, true);
            file.Write(fileContent);
            file.Close();
            writeSuccess = true;
        } catch (e) {
            writeError = e.message;
        }

        // ==================================================
        // СТАТИСТИКА
        // ==================================================

        var endTime = new Date().getTime();
        var elapsed = (endTime - startTime) / 1000;
        var elapsedStr = Math.round(elapsed * 1000) / 1000;
        elapsedStr = "" + elapsedStr;

        var level1Count = 0;
        var level2Count = 0;
        var level3Count = 0;
        var level4PlusCount = 0;
        for (var si3 = 0; si3 < allSections.length; si3++) {
            var sec3 = allSections[si3];
            if (sec3 && sec3.level) {
                if (sec3.level == 1) level1Count++;
                else if (sec3.level == 2) level2Count++;
                else if (sec3.level == 3) level3Count++;
                else level4PlusCount++;
            }
        }

        if (showStatistics == 1) {
            var msg = "";
            msg += scriptName + "\n";
            msg += "ver. " + version + "\n";
            msg += "----------------------------------------\n";
            msg += "\n";
            if (documentFolder != "") {
                msg += "Папка: " + documentFolder + "\n\n";
            }
            if (bookTitle.length > 0) {
                msg += "✓ Заголовок книги: " + bookTitle + "\n";
                msg += "\n";
            }
            msg += "✓ Всего секций: " + totalSections + "\n";
            msg += "  • 1-го уровня: " + level1Count + "\n";
            if (level2Count > 0) msg += "  • 2-го уровня: " + level2Count + "\n";
            if (level3Count > 0) msg += "  • 3-го уровня: " + level3Count + "\n";
            if (level4PlusCount > 0) msg += "  • 4+ уровня: " + level4PlusCount + "\n";
            msg += "\n";
            msg += "✓ Всего картинок: " + totalImages + "\n";
            msg += "✓ Групп картинок: " + imageGroups.length + "\n";
            msg += "\n";
            var withAbove = 0;
            var withBelow = 0;
            for (var si2 = 0; si2 < imagesInfo.length; si2++) {
                if (imagesInfo[si2].anchorsAbove.length > 0) withAbove++;
                if (imagesInfo[si2].anchorsBelow.length > 0) withBelow++;
            }
            msg += "  • С реперами сверху: " + withAbove + " из " + totalImages + "\n";
            msg += "  • С реперами снизу: " + withBelow + " из " + totalImages + "\n";
            msg += "\n";

            if (writeSuccess) {
                msg += "✓ Файл сохранён:\n";
                msg += "  " + fullPath + "\n";
            } else {
                msg += "✗ Ошибка записи файла:\n";
                msg += "  " + writeError + "\n";
            }

            msg += "\n";
            msg += "Время выполнения: " + elapsedStr + " сек.\n";

            MsgBox(msg);
        } else {
            if (!writeSuccess) {
                MsgBox(scriptName + "\n" + "ver. " + version + "\n----------------------------------------\n" +
                       "✗ Ошибка записи файла: " + writeError + "\n");
            }
        }
    } catch (e) {
        if (showStatistics == 1) {
            MsgBox(scriptName + "\n" + "ver. " + version + "\n----------------------------------------\n" +
                   "✗ Критическая ошибка: " + e.message + "\n");
        }
    }
}