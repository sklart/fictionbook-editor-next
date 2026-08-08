// Скрипт "Создать реперную карту fb2 документа для переноса иллюстраций" для редактора FBE
// version 2.0
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Первый скрипт из комплекта для переноса иллюстраций между fb2-документами.
// Работает в паре со Скриптом 2 "Расставить маркеры иллюстраций по реперной карте.js".

// Данный скрипт предназначен для создания TXT-файла с реперной картой иллюстраций fb2 документа
// для последующего переноса иллюстраций в откорректированную версию fb2-документа
// (в другой документ, например, с отредактированным текстом).
// Для работы скрипт создает служебную папку D:\\FBE_Compare (путь можно изменить в настройках ниже).
// В данную папку скрипт помещает созданный txt файл fb2_reper_map.txt
// с реперной картой расположения иллюстраций в исходном файле.
// Также в данную папку помещается файл с отчетом об ошибках (при их наличии)
// после работы второго скрипта - расстановки маркеров иллюстраций в целевом документе.
// Скрипт учитывает наличие пустых строк вокруг иллюстраций.
// Сохраняет реперную карту в TXT через FileSystemObject
// Скрипт не вносит никаких изменений в fb2 документ.
// Режим работы: обычный или тихий.

// Как работает эта пара скриптов:
// Создать реперную карту fb2 документа для переноса иллюстраций.js
// и
// Расставить маркеры иллюстраций по реперной карте.js

// Открываем исходный документ с иллюстрациями.
// Запускаем этот скрипт для создания реперной карты данного документа.
// Открываем целевой документ с отредактированным текстом и без иллюстраций.
// Запускаем второй скрипт Расставить маркеры иллюстраций по реперной карте.js
// Второй скрипт расставляет в целевом документе текстовые маркеры типа zzz_pic
// или сразу пустые картинки (в зависимости от включенных настроек во втором скрипте)
// на местах, максимально совпадающих с исходным документом.
// В случае наличия предполагаемых ошибок расстановки,
// скрип создает файл отчета об ошибках в той же папке D:\\FBE_Compare

// Далее можно:
// Проверить в целевом документе расстановку текстовых маркеров,
// при необходимости переставить отдельные маркеры вручную.
// Заменить текстовые маркеры zzz_pic на <image l:href="#undefined"/> глобальной заменой в режиме XML кода.
// Подцепить на места пустышек реальные иллюстрации скриптом "15_Расставить иллюстрации по заданным местам.js"

// version 2.0, 30.06.2026
//======================================

// ==================================================
// ГЛОБАЛЬНЫЕ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// (вынесены из Run для доступности из всех функций)
// ==================================================

// Получение локального имени бинарника из href
// Пример: "#i_001.jpg" → "i_001.jpg"
// Взято из скрипта Sclex без изменений
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

// Проверка, является ли текст пустым
// Учитывает обычные пробелы, неразрывные, &nbsp; и необычные пробелы
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
    // ==================================================
    // ШАПКА СКРИПТА
    // ==================================================
    var scriptName = "Создать реперную карту fb2 документа для переноса иллюстраций";
    var version = "2.0";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    var workFolder = "D:\\FBE_Compare";          // Путь к рабочей папке (создаётся автоматически)
    var mapFileName = "fb2_reper_map.txt";        // Имя файла реперной карты
    var shortParagraphThreshold = 50;              // Порог короткого абзаца в символах (меньше — короткий)
    var wordsPerAnchor = 3;                        // Сколько слов брать из начала, середины и конца абзаца
    var showStatistics = 1;                        // 1 — показывать статистику, 0 — тихий режим (только ошибки)
    var processNotesSection = 0;                   // Обрабатывать раздел сносок (примечаний): 0 — нет, 1 — да
    var processCommentsSection = 0;                // Обрабатывать раздел комментариев: 0 — нет, 1 — да

    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ВНУТРИ RUN
    // ==================================================

    // ----- Работа с текстом -----

    // Рекурсивное получение всего текста из элемента (только текстовые узлы)
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

    // Проверка, является ли абзац пустым (только пробелы, &nbsp;, неразрывные пробелы)
    function isEmptyParagraph(paragraph) {
        if (!paragraph || paragraph.nodeType != 1 || paragraph.nodeName != "P") return false;
        var text = getElementText(paragraph);
        return isEmptyText(text);
    }

    // Очистка текста: оставляем только буквы (русские и латинские) и цифры, в нижний регистр
    // Используется для сравнения реперов без учёта знаков препинания и пробелов
    function cleanText(text) {
        if (!text) return "";
        var result = "";
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            var code = ch.charCodeAt(0);
            if ((code >= 48 && code <= 57) ||       // 0-9
                (code >= 65 && code <= 90) ||        // A-Z
                (code >= 97 && code <= 122) ||       // a-z
                (code >= 1040 && code <= 1103) ||    // А-Я, а-я
                code == 1025 || code == 1105) {      // Ё, ё
                result += ch.toLowerCase();
            }
        }
        return result;
    }

    // Извлечение слов из оригинального текста
    // Слово — непрерывная последовательность букв/цифр
    // Разделители — любые другие символы (пробелы, знаки препинания и т.д.)
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

    // ----- Проверка типов элементов -----

    // Проверка: является ли узел заголовком (DIV class="title" или class="subtitle")
    function isTitle(node) {
        if (!node || node.nodeType != 1 || node.nodeName != "DIV") return false;
        var cls = node.className || "";
        return cls == "title" || cls == "subtitle";
    }

    // Проверка: является ли узел блочной картинкой (DIV class="image" с href)
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

    // Проверка: является ли узел секцией (DIV class="section")
    function isSection(node) {
        if (!node || node.nodeType != 1 || node.nodeName != "DIV") return false;
        return (node.className || "") == "section";
    }

    // Проверка: является ли узел телом документа (DIV class="body")
    function isBody(node) {
        if (!node || node.nodeType != 1 || node.nodeName != "DIV") return false;
        return (node.className || "") == "body";
    }

    // ----- Извлечение реперов из текста -----

    // Получение длины текста абзаца
    function getParagraphLength(paragraph) {
        var text = getElementText(paragraph);
        return text.length;
    }

    // Извлечение реперов из текста: N слов из начала, N из середины, N из конца
    // N задаётся в настройке wordsPerAnchor
    function getAnchorsFromText(text) {
        var words = extractWordsFromOriginal(text);
        var anchors = [];

        if (words.length == 0) return anchors;

        var n = wordsPerAnchor;
        if (n > words.length) n = words.length;

        // Начало: первые n слов
        var anchorBegin = "";
        for (var i = 0; i < n; i++) {
            if (i > 0) anchorBegin += " ";
            anchorBegin += words[i];
        }
        anchors.push(anchorBegin);

        // Середина: n слов из середины
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

        // Конец: последние n слов
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

    // Извлечение реперов из абзаца (получаем текст, затем извлекаем слова)
    function getAnchorsFromParagraph(paragraph) {
        var text = getElementText(paragraph);
        return getAnchorsFromText(text);
    }

    // ----- Работа с заголовками -----

    // Получение заголовка секции (текст из первого title внутри секции)
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

    // Получение заголовка книги (title внутри body)
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

    // ----- Подсчёт пустых строк вокруг картинки -----

    // Подсчёт количества пустых абзацев непосредственно рядом с элементом
    // direction: -1 = вверх, 1 = вниз
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

            // Пропускаем пустые текстовые узлы
            if (current.nodeType == 3) {
                if (isEmptyText(current.nodeValue || "")) {
                    continue;
                } else {
                    break;
                }
            }

            if (current.nodeType != 1) continue;

            // Считаем пустые абзацы
            if (current.nodeName == "P") {
                if (isEmptyParagraph(current)) {
                    count++;
                    continue;
                } else {
                    break;
                }
            }

            // Любой другой элемент — не пустая строка
            break;
        }

        return count;
    }

    // ----- Сбор картинок и секций -----

    // Рекурсивный сбор картинок из контейнера (без захода во вложенные секции)
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

    // Рекурсивный обход секций и сбор информации о них
    // Заходит во все уровни вложенности, нумерует секции сквозным индексом
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

                // Собираем картинки, непосредственно принадлежащие этой секции
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

                // Рекурсивно заходим во вложенные секции
                collectSectionsRecursive(child, secIndex, level + 1, bodyType, sectionsArr, imagesArr);
            }
        }
    }

    // Сбор картинок только из прямых потомков секции (без захода во вложенные секции)
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
                    // Заходим в другие DIV (epigraph и т.д.), но не в секции
                    collectImagesFromSectionDirect(child, sectionIndex, bodyType, resultArray);
                }
            }
        }
    }

    // ----- Определение групп картинок -----

    // Проверка, идут ли две картинки подряд (без значимого текста между ними)
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

    // ----- Пробивка границ секций для поиска реперов -----

    // Основная функция сбора реперов в одном направлении
    // Сначала ищет внутри текущего контейнера, затем пробивает границы
    function collectAnchorsDirection(imageElement, direction, ignoreImages) {
        var anchors = [];
        var shortParagraphs = [];
        var shortParagraphsTotalLength = 0;
        var current = imageElement;
        var container = imageElement.parentNode;

        // Первый проход — внутри текущего контейнера
        while (true) {
            if (direction == -1) {
                current = current.previousSibling;
            } else {
                current = current.nextSibling;
            }

            if (!current) break;
            if (current.parentNode != container) break;

            // Пропускаем пустые текстовые узлы
            if (current.nodeType == 3) {
                if (isEmptyText(current.nodeValue || "")) continue;
                continue;
            }

            if (current.nodeType != 1) continue;

            // Достигли границы секции или body — останавливаемся
            if (isSection(current) || isBody(current)) break;

            // Наткнулись на другую картинку
            if (isImage(current)) {
                if (ignoreImages) continue;
                else break;
            }

            // Наткнулись на заголовок — используем как репер
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

            // Эпиграф, цитата, аннотация — извлекаем текст как репер
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

            // Обычный абзац
            if (current.nodeName == "P") {
                if (isEmptyParagraph(current)) continue;

                var pLength = getParagraphLength(current);
                if (pLength >= shortParagraphThreshold) {
                    // Длинный абзац — извлекаем реперы и выходим
                    var pAnchors = getAnchorsFromParagraph(current);
                    for (var a = 0; a < pAnchors.length; a++) {
                        anchors.push(pAnchors[a]);
                    }
                    break;
                } else {
                    // Короткий абзац — накапливаем
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

        // Если накопили короткие абзацы, но не нашли длинный — используем их
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

        // Если не нашли — пробиваем границы секций
        if (anchors.length == 0) {
            anchors = climbLevelsForAnchors(imageElement, direction, ignoreImages);
        }

        return anchors;
    }

    // Циклический подъём по уровням секций для поиска текста за границами
    // Поднимается от текущей секции вплоть до body, пока не найдёт текст
    function climbLevelsForAnchors(imageElement, direction, ignoreImages) {
        var currentElement = imageElement;

        while (currentElement) {
            var container = currentElement.parentNode;
            if (!container) break;

            // Пробуем найти sibling на текущем уровне
            var sibling = findNextSiblingSkipEmpty(currentElement, direction, container);

            if (sibling) {
                if (isSection(sibling)) {
                    // Нашли секцию — ищем текст внутри неё
                    var textElement = findFirstTextInSection(sibling, direction);
                    if (textElement) {
                        return extractAnchorsFromElement(textElement);
                    }
                } else {
                    // Нашли другой элемент — пробуем извлечь из него текст
                    var textElement = findTextInElement(sibling, direction);
                    if (textElement) {
                        return extractAnchorsFromElement(textElement);
                    }
                }
            }

            // Не нашли sibling — проверяем вложенные секции внутри container
            if (isSection(container)) {
                var innerSection = findInnerSection(container, currentElement, direction);
                if (innerSection) {
                    var textElement = findFirstTextInSection(innerSection, direction);
                    if (textElement) {
                        return extractAnchorsFromElement(textElement);
                    }
                }
            }

            // Дошли до body — дальше некуда
            if (isBody(container)) break;

            // Поднимаемся на уровень выше
            currentElement = container;
        }

        return [];
    }

    // Найти следующий/предыдущий значимый sibling, пропуская пустые элементы
    function findNextSiblingSkipEmpty(element, direction, parentContainer) {
        var current = element;

        // Поднимаемся, пока не окажемся прямым потомком parentContainer
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
                // Пропускаем пустые абзацы
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

    // Найти вложенную секцию после/до элемента внутри того же контейнера
    function findInnerSection(container, element, direction) {
        if (!container) return null;
        var children = container.childNodes;

        // Находим позицию элемента среди детей контейнера
        var pos = -1;
        for (var i = 0; i < children.length; i++) {
            if (children[i] == element) {
                pos = i;
                break;
            }
        }
        if (pos == -1) {
            // Если элемент не прямой потомок — ищем его предка
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

        // Ищем вложенную секцию в нужном направлении
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

    // Извлечение реперов из найденного элемента (абзац, заголовок и т.д.)
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

    // Найти первый текстовый элемент внутри секции в заданном направлении
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

    // Рекурсивный поиск первого непустого текстового элемента
    function findTextInElement(element, direction) {
        if (!element || element.nodeType != 1) return null;

        // Проверяем сам элемент
        if (element.nodeName == "P" && !isEmptyParagraph(element)) {
            return element;
        }

        if (isTitle(element)) {
            var titleText = getElementText(element);
            if (!isEmptyText(titleText)) return element;
        }

        if (element.nodeName == "DIV") {
            var cls = element.className || "";
            if (cls == "image") return null;           // Картинки пропускаем
            if (cls == "section") {
                return findFirstTextInSection(element, direction);  // Заходим во вложенную секцию
            }
            if (cls == "epigraph" || cls == "cite" || cls == "annotation") {
                var blockText = getElementText(element);
                if (!isEmptyText(blockText)) return element;
            }
        }

        // Ищем в детях
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
        // Поиск тела документа
        var fbwBody = document.getElementById("fbw_body");
        if (!fbwBody) {
            if (showStatistics == 1) {
                MsgBox(scriptName + "\n" + "ver. " + version + "\n----------------------------------------\n" +
                       "✗ Ошибка: не найден fbw_body.\n");
            }
            return;
        }

        // Сбор всех разделов body (основной, сноски, комментарии)
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

            // Получаем заголовок книги из основного body
            if (bodyType == "main") {
                bookTitle = getBodyTitle(bodyElement);
                bookTitleClean = cleanText(bookTitle);
            }

            // Собираем картинки, которые напрямую в body (вне секций, до первой секции)
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

            // Рекурсивно собираем все секции и картинки в них
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

        // Определяем группы картинок (идущих подряд без текста)
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

        // Собираем реперы и пустые строки для каждой группы
        var imagesInfo = [];

        for (var gi = 0; gi < imageGroups.length; gi++) {
            var group = imageGroups[gi];
            var firstImage = group[0];
            var lastImage = group[group.length - 1];

            // Реперы сверху — от текста над первой картинкой группы
            var anchorsAbove = collectAnchorsDirection(firstImage.element, -1, true);
            // Реперы снизу — от текста под последней картинкой группы
            var anchorsBelow = collectAnchorsDirection(lastImage.element, 1, true);

            // Пустые строки вокруг
            var emptyAbove = countEmptyLinesAround(firstImage.element, -1);
            var emptyBelow = countEmptyLinesAround(lastImage.element, 1);

            // Присваиваем одинаковые реперы всем картинкам в группе
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
        // ФАЗА 2: ЗАПИСЬ В TXT (только запись)
        // ==================================================

        // Формируем содержимое файла
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

        // Секции
        for (var si = 0; si < allSections.length; si++) {
            var sec = allSections[si];
            if (sec && sec.index) {
                fileContent += "SECTION|" + sec.index + "|" + sec.parent + "|" + sec.level + "|" + sec.title + "|" + sec.imageCount + "\r\n";
            }
        }
        fileContent += "\r\n";

        // Картинки с реперами
        for (var ii = 0; ii < imagesInfo.length; ii++) {
            var imgInfo = imagesInfo[ii];
            fileContent += "IMAGE|" + (ii + 1) + "|" + imgInfo.name + "|" + imgInfo.sectionIndex + "\r\n";
            fileContent += "EMPTY_ABOVE|" + (ii + 1) + "|" + imgInfo.emptyAbove + "\r\n";
            fileContent += "EMPTY_BELOW|" + (ii + 1) + "|" + imgInfo.emptyBelow + "\r\n";

            // Реперы сверху
            if (imgInfo.anchorsAbove.length > 0) {
                fileContent += "ANCHOR_ABOVE|" + (ii + 1);
                for (var aa = 0; aa < imgInfo.anchorsAbove.length; aa++) {
                    fileContent += "|" + imgInfo.anchorsAbove[aa];
                }
                fileContent += "\r\n";
            } else {
                fileContent += "ANCHOR_ABOVE|" + (ii + 1) + "|NO_TEXT\r\n";
            }

            // Реперы снизу
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

        // Запись файла через FileSystemObject (ActiveX)
        var fullPath = workFolder + "\\" + mapFileName;
        var writeSuccess = false;
        var writeError = "";

        try {
            var fso = new ActiveXObject("Scripting.FileSystemObject");
            // Создаём папку, если её нет
            try {
                fso.CreateFolder(workFolder);
            } catch (e) {
                // Папка уже существует — это нормально
            }
            var file = fso.CreateTextFile(fullPath, true, true);  // true, true = перезаписать, Unicode
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

        // Подсчёт секций по уровням
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
            // Тихий режим — сообщаем только об ошибках
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
