// Скрипт "Сформатировать короткие абзацы в концах секций" для редактора FBE
// version 2.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для форматирования коротких абзацев в концах секций fb2 документа.
// Скрипт находит последний абзац в каждой секции и проверяет его по заданным условиям.
// Подходящие условия: даты, ФИО, КАПС, кавычки, 2+ слов с заглавной буквы, одно слово с заглавной.
// Пропускаются: диалоги, списки, абзацы с пунктуацией в конце и ключевыми словами.
// Игнорируются абзацы внутри размеченных элементов: cite, epigraph, poem, annotation, table.
// Область обработки: сразу весь документ / от позиции курсора / выделенный фрагмент.
// Поскольку скрипт обрабатывает всего один последний абзац в любой из секций,
// выделять сам нужный абзац нет необходимости, достаточно выделить любой фрагмент
// в нужной секции или в нескольких.
// Режимы - автомат, пошаговый с просмотром, "адаптивный" пошаговый.
// В "адаптивном" пошаговом режиме, если кол-во подходящих абзацев в документе или фрагменте
// более заданного значения, скрипт предлагает переключиться на автоматический режим.
// Найденные подходящие абзацы форматируются курсивом или жирностью.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 2.1, 27.04.2026
// ======================================

function Run() {
    var scriptName = "Сформатировать короткие абзацы в концах секций";
    var version = "2.1";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    // Настройка режима отображения:
    // 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1;

    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0;     // 0 - нет, 1 - да

    // Обрабатывать раздел комментариев
    var processCommentsSection = 0;     // 0 - нет, 1 - да

    // --------------------------------------------------
    // Режим обработки
    // --------------------------------------------------

    // Обрабатывать сразу весь документ или от позиции курсора
    // 0 - весь документ, 1 - от курсора вниз
    var processFromCursor = 0;

    // Автоматический режим или пошаговый
    // 0 - автоматический, 1 - пошаговый
    var stepByStepMode = 1;

    // --------------------------------------------------
    // Настройки форматирования
    // --------------------------------------------------

    // Форматировать найденные абзацы
    // 0 - нет (только поиск), 1 - да
    var formatFound = 1;

    // Форматировать найденные абзацы жирностью или курсивом
    // 0 - жирностью (STRONG), 1 - курсивом (EM)
    var formatStyle = 1;

    // --------------------------------------------------
    // Настройки фильтрации абзацев
    // --------------------------------------------------

    // Максимальная длина абзаца (без учета тэгов), символов
    var maxParagraphLength = 30;

    // Минимальное количество букв в абзаце
    // (не применяется к датам и ФИО)
    var minLettersCount = 2;

    // Пропускать абзацы полностью в тэгах жирности
    // 0 - нет, 1 - да
    var skipFullBold = 1;

    // Пропускать абзацы полностью в тэгах курсива
    // 0 - нет, 1 - да
    var skipFullItalic = 1;

    // Пропускать абзацы с знаками препинания в конце (.:;…?!)
    // 0 - нет, 1 - да
    var skipEndPunctuation = 1;

    // Пропускать абзацы-диалоги (с любым тире или дефисом в начале абзаца)
    // 0 - нет, 1 - да
    var skipDialog = 1;

    // Пропускать абзацы, содержащие ключевые слова из словарика
    // 0 - нет, 1 - да
    var skipKeywordList = 1;

    // Пропускать абзацы без заглавных букв
    // (не применяется к датам и ФИО)
    // 0 - нет, 1 - да
    var skipNoUppercase = 1;

    // Пропускать абзацы, где нет букв или цифр (только символы)
    // 0 - нет, 1 - да
    var skipOnlySymbols = 1;

    // --------------------------------------------------
    // Настройки отображения в пошаговом режиме
    // --------------------------------------------------

    // Выделение найденного абзаца в пошаговом режиме
    // 0 - только прокрутка к абзацу, 1 - прокрутка и выделение
    var highlightFound = 1;

    // Порог для предупреждения в пошаговом режиме
    // Если найдено больше этого числа кандидатов — будет предложено
    // переключиться на автоматический режим
    var stepByStepWarningThreshold = 20;

    // --------------------------------------------------
    // Словарик ключевых слов для пропуска
    // (можно пополнять по необходимости)
    // --------------------------------------------------
    var keywordList = [
        "да", "нет", "я", "ты", "он", "она", "оно", "мы", "вы", "они",
        "ах", "ох", "эй", "ух", "эх", "ого", "увы",
        "вот", "тут", "там", "здесь", "вместе",
        "кто", "что", "где", "куда", "откуда", "когда", "зачем", "почему",
        "как", "вдруг", "опять", "снова", "уже", "завтра", "сегодня", "утром", "вечером",
        "был", "была", "было", "были",
        "есть", "нету", "будет", "будут",
        "мой", "твой", "наш", "ваш", "свой",
        "один", "одна", "одно", "одни",
        "ладно", "хорошо", "конечно"
    ];

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
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

    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================

    function trimStr(str) {
        return str.replace(/^\s+|\s+$/g, '');
    }

    function normalizeText(text) {
        var result = text;
        for (var i = 0; i < unusualSpaces.length; i++) {
            var sp = unusualSpaces.charAt(i);
            var temp = "";
            for (var j = 0; j < result.length; j++) {
                if (result.charAt(j) == sp) {
                    temp += " ";
                } else {
                    temp += result.charAt(j);
                }
            }
            result = temp;
        }
        return result;
    }

    function isUnusualSpace(ch) {
        for (var i = 0; i < unusualSpaces.length; i++) {
            if (ch == unusualSpaces.charAt(i)) return true;
        }
        return false;
    }

    function findParentBody(element) {
        while (element) {
            if (element.nodeName == "DIV" && element.className == "body") {
                return element;
            }
            element = element.parentNode;
        }
        return null;
    }

    function getSectionType(element) {
        var body = findParentBody(element);
        if (!body) return "main";
        var fbname = body.getAttribute("fbname") || "";
        if (fbname == "notes") return "notes";
        if (fbname == "comments") return "comments";
        return "main";
    }

    function shouldProcessElement(element) {
        if (!element || element.nodeType != 1) return false;
        var body = findParentBody(element);
        if (!body) return true;
        var fbname = body.getAttribute("fbname") || "";
        if (fbname == "notes" && !processNotesSection) return false;
        if (fbname == "comments" && !processCommentsSection) return false;
        return true;
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
            var ch = text.charAt(i);
            if ((ch >= 'а' && ch <= 'я') || (ch >= 'А' && ch <= 'Я') ||
                (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
                count++;
            }
        }
        return count;
    }

    function isFullBold(pElement) {
        var childElements = [];
        for (var i = 0; i < pElement.childNodes.length; i++) {
            if (pElement.childNodes[i].nodeType == 1) {
                childElements.push(pElement.childNodes[i]);
            } else if (pElement.childNodes[i].nodeType == 3) {
                var txt = pElement.childNodes[i].nodeValue;
                var hasText = false;
                for (var k = 0; k < txt.length; k++) {
                    var ch = txt.charAt(k);
                    if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t' && !isUnusualSpace(ch)) {
                        hasText = true;
                        break;
                    }
                }
                if (hasText) return false;
            }
        }
        if (childElements.length == 1) {
            var tagName = childElements[0].nodeName.toUpperCase();
            if (tagName == "STRONG" || tagName == "B") return true;
        }
        return false;
    }

    function isFullItalic(pElement) {
        var childElements = [];
        for (var i = 0; i < pElement.childNodes.length; i++) {
            if (pElement.childNodes[i].nodeType == 1) {
                childElements.push(pElement.childNodes[i]);
            } else if (pElement.childNodes[i].nodeType == 3) {
                var txt = pElement.childNodes[i].nodeValue;
                var hasText = false;
                for (var k = 0; k < txt.length; k++) {
                    var ch = txt.charAt(k);
                    if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t' && !isUnusualSpace(ch)) {
                        hasText = true;
                        break;
                    }
                }
                if (hasText) return false;
            }
        }
        if (childElements.length == 1) {
            var tagName = childElements[0].nodeName.toUpperCase();
            if (tagName == "EM" || tagName == "I") return true;
        }
        return false;
    }

    function hasEndPunctuation(text) {
        if (text.length == 0) return false;
        var lastChar = text.charAt(text.length - 1);
        var punctChars = ".:;…?!";
        return punctChars.indexOf(lastChar) >= 0;
    }

    function startsWithDash(text) {
        if (text.length == 0) return false;
        var startPos = 0;
        while (startPos < text.length) {
            var ch = text.charAt(startPos);
            if (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t' || isUnusualSpace(ch)) {
                startPos++;
            } else {
                break;
            }
        }
        if (startPos >= text.length) return false;
        var firstChar = text.charAt(startPos);
        var dashes = "-–—―−";
        return dashes.indexOf(firstChar) >= 0;
    }

    function looksLikeList(text) {
        var trimmed = "";
        var started = false;
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            if (!started && (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t' || isUnusualSpace(ch))) {
                continue;
            }
            started = true;
            trimmed += ch;
        }
        if (trimmed.length >= 2) {
            var first = trimmed.charAt(0);
            var second = trimmed.charAt(1);
            if (first >= '0' && first <= '9') {
                if (second == ')' || second == '.') return true;
            }
            if (first >= 'а' && first <= 'я') {
                if (second == ')') return true;
            }
        }
        return false;
    }

    function containsKeyword(text) {
        var lowerText = text.toLowerCase();
        for (var i = 0; i < keywordList.length; i++) {
            var kw = keywordList[i].toLowerCase();
            if (lowerText.indexOf(kw) >= 0) {
                var idx = lowerText.indexOf(kw);
                var beforeOk = (idx == 0) || (lowerText.charAt(idx - 1) == ' ') || (isUnusualSpace(lowerText.charAt(idx - 1)));
                var afterIdx = idx + kw.length;
                var afterOk = (afterIdx >= lowerText.length) || (lowerText.charAt(afterIdx) == ' ') || (isUnusualSpace(lowerText.charAt(afterIdx))) || (lowerText.charAt(afterIdx) == '.' || lowerText.charAt(afterIdx) == ',' || lowerText.charAt(afterIdx) == '!' || lowerText.charAt(afterIdx) == '?');
                if (beforeOk && afterOk) return true;
            }
        }
        return false;
    }

    function hasUppercase(text) {
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            if (ch >= 'А' && ch <= 'Я') return true;
            if (ch >= 'A' && ch <= 'Z') return true;
        }
        return false;
    }

    function hasLettersOrDigits(text) {
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            if ((ch >= 'а' && ch <= 'я') || (ch >= 'А' && ch <= 'Я')) return true;
            if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) return true;
            if (ch >= '0' && ch <= '9') return true;
        }
        return false;
    }

    function isAllCaps(text) {
        var hasLetters = false;
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            if ((ch >= 'а' && ch <= 'я') || (ch >= 'a' && ch <= 'z')) {
                return false;
            }
            if ((ch >= 'А' && ch <= 'Я') || (ch >= 'A' && ch <= 'Z')) {
                hasLetters = true;
            }
        }
        return hasLetters;
    }

    function hasQuotes(text) {
        var quotes = "\"'«»„“”‘’‹›";
        for (var i = 0; i < text.length; i++) {
            if (quotes.indexOf(text.charAt(i)) >= 0) return true;
        }
        return false;
    }

    function hasDate(text) {
        if (!text) return false;

        var normalized = normalizeText(text);
        if (normalized.length == 0) return false;

        var cleanText = normalized.replace(/\[\d+\]/g, '');
        cleanText = trimStr(cleanText);

        var yearPattern = /(?:[1][789][0-9][0-9]|[2][0][0-2][0-9])/;
        if (yearPattern.test(cleanText)) return true;

        var yearWithGPattern = /(?:[1][789][0-9][0-9]|[2][0][0-2][0-9])[\x20\xA0]г[\.\)]?/i;
        if (yearWithGPattern.test(cleanText)) return true;

        var months = [
            "январь", "февраль", "март", "апрель", "май", "июнь", "июль",
            "август", "сентябрь", "октябрь", "ноябрь", "декабрь",
            "января", "февраля", "марта", "апреля", "мая", "июня", "июля",
            "августа", "сентября", "октября", "ноября", "декабря",
            "янв", "фев", "февр", "мар", "апр", "июн", "июл", "авг",
            "сен", "сент", "окт", "ноя", "нояб", "дек",
            "янв.", "фев.", "февр.", "мар.", "апр.", "июн.", "июл.",
            "авг.", "сен.", "сент.", "окт.", "ноя.", "нояб.", "дек."
        ];

        var lowerText = cleanText.toLowerCase();
        for (var i = 0; i < months.length; i++) {
            if (lowerText.indexOf(months[i]) >= 0) {
                return true;
            }
        }

        var datePattern = /(?:[1-9]|1[0-9]|2[0-9]|3[01])(?:[–—-](?:[1-9]|1[0-9]|2[0-9]|3[01]))?[\x20\xA0]{1,2}(января|февраля|марта|апреля|мая|июня|июля|августа|сентября|октября|ноября|декабря)/i;
        if (datePattern.test(cleanText)) return true;

        return false;
    }

    function looksLikeName(text) {
        for (var i = 0; i < text.length - 3; i++) {
            var ch1 = text.charAt(i);
            var ch2 = text.charAt(i + 1);
            var ch3 = text.charAt(i + 2);
            var ch4 = text.charAt(i + 3);
            if ((ch1 >= 'А' && ch1 <= 'Я') || (ch1 >= 'A' && ch1 <= 'Z')) {
                if (ch2 == '.') {
                    if ((ch3 >= 'А' && ch3 <= 'Я') || (ch3 >= 'A' && ch3 <= 'Z')) {
                        if (ch4 == '.') return true;
                    }
                }
            }
        }
        for (var i = 0; i < text.length - 1; i++) {
            var ch1 = text.charAt(i);
            var ch2 = text.charAt(i + 1);
            if ((ch1 >= 'А' && ch1 <= 'Я') || (ch1 >= 'A' && ch1 <= 'Z')) {
                if (ch2 == '.') return true;
            }
        }
        return false;
    }

    function countCapitalizedWords(text) {
        var count = 0;
        var inWord = false;
        var wordStartsWithCapital = false;
        var wordHasLength = false;
        
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            var isLetter = (ch >= 'а' && ch <= 'я') || (ch >= 'А' && ch <= 'Я') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
            var isDigit = (ch >= '0' && ch <= '9');
            
            if (isLetter || isDigit) {
                if (!inWord) {
                    inWord = true;
                    wordHasLength = true;
                    if ((ch >= 'А' && ch <= 'Я') || (ch >= 'A' && ch <= 'Z')) {
                        wordStartsWithCapital = true;
                    } else {
                        wordStartsWithCapital = false;
                    }
                }
            } else {
                if (inWord) {
                    if (wordStartsWithCapital && wordHasLength) {
                        count++;
                    }
                    inWord = false;
                    wordHasLength = false;
                }
            }
        }
        if (inWord && wordStartsWithCapital && wordHasLength) {
            count++;
        }
        return count;
    }

    function countWords(text) {
        var count = 0;
        var inWord = false;
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            var isLetter = (ch >= 'а' && ch <= 'я') || (ch >= 'А' && ch <= 'Я') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
            var isDigit = (ch >= '0' && ch <= '9');
            if (isLetter || isDigit) {
                if (!inWord) {
                    inWord = true;
                    count++;
                }
            } else {
                inWord = false;
            }
        }
        return count;
    }

    function isSuitableParagraph(pElement) {
        var plainText = getPlainText(pElement);
        var textLength = plainText.length;

        if (textLength > maxParagraphLength) return false;

        if (skipOnlySymbols && !hasLettersOrDigits(plainText)) return false;
        if (skipFullBold && isFullBold(pElement)) return false;
        if (skipFullItalic && isFullItalic(pElement)) return false;
        if (skipEndPunctuation && hasEndPunctuation(plainText)) return false;
        if (skipDialog && startsWithDash(plainText)) return false;
        if (looksLikeList(plainText)) return false;
        if (skipKeywordList && containsKeyword(plainText)) return false;

        var isDate = hasDate(plainText);
        var isName = looksLikeName(plainText);
        var isCaps = isAllCaps(plainText);
        var hasQt = hasQuotes(plainText);
        var capWords = countCapitalizedWords(plainText);
        var wordCount = countWords(plainText);

        if (isDate || isName) {
            return true;
        }

        if (countLetters(plainText) < minLettersCount) return false;
        if (skipNoUppercase && !hasUppercase(plainText)) return false;

        if (isCaps) return true;
        if (hasQt) return true;
        if (capWords >= 2) return true;
        if (wordCount == 1 && capWords == 1) return true;

        return false;
    }

    function findLastParagraph(sectionElement) {
        var children = sectionElement.childNodes;
        for (var i = children.length - 1; i >= 0; i--) {
            var child = children[i];
            if (child.nodeType == 1) {
                if (child.nodeName == "DIV" && child.className == "section") {
                    var innerP = findLastParagraph(child);
                    if (innerP) return innerP;
                }
                if (child.nodeName == "P") {
                    return child;
                }
                if (child.nodeName == "DIV" && child.className != "section" && child.className != "body") {
                    var blockP = findLastParagraph(child);
                    if (blockP) return blockP;
                }
            }
        }
        return null;
    }

    function isInsideBlockDiv(pElement) {
        var parent = pElement.parentNode;
        if (!parent) return false;
        if (parent.nodeName == "DIV" && parent.className != "section" && parent.className != "body") {
            return true;
        }
        return false;
    }

    function findParentSection(element) {
        while (element) {
            if (element.nodeName == "DIV" && element.className == "section") {
                return element;
            }
            if (element.nodeName == "DIV" && element.className == "body") {
                return null;
            }
            element = element.parentNode;
        }
        return null;
    }

    function findContainingSection(element) {
        if (element.nodeName == "DIV" && element.className == "section") {
            return element;
        }
        return findParentSection(element);
    }

    // Проверка: является ли element1 предком element2
    function isAncestorOf(element1, element2) {
        var current = element2.parentNode;
        while (current) {
            if (current === element1) return true;
            current = current.parentNode;
        }
        return false;
    }

    function formatParagraph(pElement) {
        var tagName = (formatStyle == 1) ? "EM" : "STRONG";
        pElement.innerHTML = "<" + tagName + ">" + pElement.innerHTML + "</" + tagName + ">";
    }

    function getNextP(el) {
        function getNextNode(node) {
            if (node.firstChild && node.nodeName != "P")
                return node.firstChild;
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

    function selectElement(elem) {
        if (!elem) return;
        var b = elem.getBoundingClientRect();
        if (b.bottom - b.top <= window.external.getViewHeight()) {
            window.scrollBy(0, (b.top + b.bottom - window.external.getViewHeight()) / 2);
        } else {
            window.scrollBy(0, b.top);
        }
        if (highlightFound) {
            var r = document.body.createTextRange();
            if (!r || !r.compareEndPoints) return;
            r.moveToElementText(elem);
            r.select();
        }
    }

    // ==================================================
    // ОСНОВНАЯ ЛОГИКА СКРИПТА (СБОР ДАННЫХ)
    // ==================================================

    var allSections = [];
    var sectionsFromCursor = [];
    var cursorElement = null;
    var hasSelection = false;
    var selectionParagraphs = [];
    var modeLabel = "";

    var sel = document.selection;
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

    if (hasSelection) {
        modeLabel = "В ВЫДЕЛЕННОМ ФРАГМЕНТЕ ИЛИ СЕКЦИИ::";
        
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
        while (blockStartEl && blockStartEl.nodeName != "BODY" && blockStartEl.nodeName != "P")
            blockStartEl = blockStartEl.parentNode;

        var blockEndEl = blockEndNode;
        while (blockEndEl && blockEndEl.nodeName != "BODY" && blockEndEl.nodeName != "P")
            blockEndEl = blockEndEl.parentNode;
        if (blockEndEl && blockEndEl.nodeName == "BODY") {
            blockEndEl = blockEndNode;
            if (blockEndEl.previousSibling && blockEndEl.previousSibling.nodeName == "P")
                blockEndEl = blockEndEl.previousSibling;
        }

        if (blockStartEl && blockStartEl.nodeName == "P" && blockEndEl && blockEndEl.nodeName == "P") {
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
            if (tr1.moveStart("character", 1) == 1)
                tr1.moveStart("character", -1);
            var tr2b = document.body.createTextRange();
            tr2b.moveToElementText(blockEndEl);
            tr1.setEndPoint("EndToEnd", tr2b);
            tr1.select();
        }

        if (selectionParagraphs.length == 0) {
            MsgBox(scriptName + "\n" +
                   "ver. " + version + "\n" +
                   "---------------------------\n\n" +
                   "В выделении не найдено абзацев.");
            return;
        }
    }

    if (!hasSelection && processFromCursor == 0) {
        modeLabel = "ВО ВСЕМ ДОКУМЕНТЕ:";
        
        var bodies = document.getElementsByTagName("DIV");
        for (var i = 0; i < bodies.length; i++) {
            if (bodies[i].className == "body") {
                if (shouldProcessElement(bodies[i])) {
                    var sections = bodies[i].getElementsByTagName("DIV");
                    for (var j = 0; j < sections.length; j++) {
                        if (sections[j].className == "section") {
                            allSections.push(sections[j]);
                        }
                    }
                }
            }
        }
    } else if (!hasSelection && processFromCursor == 1) {
        modeLabel = "ОТ ПОЗИЦИИ КУРСОРА ДО КОНЦА ДОКУМЕНТА:";
        
        var selRange = document.selection.createRange();
        if (selRange && selRange.parentElement) {
            cursorElement = selRange.parentElement();
        }

        if (!cursorElement) {
            MsgBox(scriptName + "\n" +
                   "ver. " + version + "\n" +
                   "---------------------------\n\n" +
                   "Не удалось определить позицию курсора.");
            return;
        }

        var currentSection = findContainingSection(cursorElement);
        if (!currentSection) {
            MsgBox(scriptName + "\n" +
                   "ver. " + version + "\n" +
                   "---------------------------\n\n" +
                   "Курсор находится вне секции.");
            return;
        }

        var allBodySections = [];
        var bodies = document.getElementsByTagName("DIV");
        for (var i = 0; i < bodies.length; i++) {
            if (bodies[i].className == "body") {
                if (shouldProcessElement(bodies[i])) {
                    var sections = bodies[i].getElementsByTagName("DIV");
                    for (var j = 0; j < sections.length; j++) {
                        if (sections[j].className == "section") {
                            allBodySections.push(sections[j]);
                        }
                    }
                }
            }
        }

        var foundCurrent = false;
        for (var i = 0; i < allBodySections.length; i++) {
            if (allBodySections[i] === currentSection) {
                foundCurrent = true;
            }
            if (foundCurrent) {
                sectionsFromCursor.push(allBodySections[i]);
            }
        }
        allSections = sectionsFromCursor;
    }

    var targetParagraphs = [];
    
    if (hasSelection) {
        // Собираем уникальные секции из выделения
        var processedSections = [];
        
        for (var i = 0; i < selectionParagraphs.length; i++) {
            var sec = findContainingSection(selectionParagraphs[i]);
            if (sec && shouldProcessElement(sec)) {
                var alreadyProcessed = false;
                for (var s = 0; s < processedSections.length; s++) {
                    if (processedSections[s] === sec) {
                        alreadyProcessed = true;
                        break;
                    }
                }
                if (!alreadyProcessed) {
                    processedSections.push(sec);
                }
            }
        }

        // Удаляем родительские секции, если их дочерние секции тоже в списке
        // Оставляем только "листовые" секции (самые глубокие из выделенных)
        var leafSections = [];
        for (var i = 0; i < processedSections.length; i++) {
            var isParent = false;
            for (var j = 0; j < processedSections.length; j++) {
                if (i != j && isAncestorOf(processedSections[i], processedSections[j])) {
                    isParent = true;
                    break;
                }
            }
            if (!isParent) {
                leafSections.push(processedSections[i]);
            }
        }

        // Для каждой листовой секции ищем последний абзац
        for (var i = 0; i < leafSections.length; i++) {
            var sec = leafSections[i];
            var lastP = findLastParagraph(sec);
            if (lastP && shouldProcessElement(lastP)) {
                if (!isInsideBlockDiv(lastP)) {
                    if (isSuitableParagraph(lastP)) {
                        var dupFound = false;
                        for (var d = 0; d < targetParagraphs.length; d++) {
                            if (targetParagraphs[d].paragraph === lastP) {
                                dupFound = true;
                                break;
                            }
                        }
                        if (!dupFound) {
                            targetParagraphs.push({
                                paragraph: lastP,
                                section: sec
                            });
                        }
                    }
                }
            }
        }
    } else {
        for (var i = 0; i < allSections.length; i++) {
            var lastP = findLastParagraph(allSections[i]);
            if (lastP && shouldProcessElement(lastP)) {
                if (isInsideBlockDiv(lastP)) continue;
                if (isSuitableParagraph(lastP)) {
                    var isDuplicate = false;
                    for (var k = 0; k < targetParagraphs.length; k++) {
                        if (targetParagraphs[k].paragraph === lastP) {
                            isDuplicate = true;
                            break;
                        }
                    }
                    if (!isDuplicate) {
                        targetParagraphs.push({
                            paragraph: lastP,
                            section: allSections[i]
                        });
                    }
                }
            }
        }
    }

    if (targetParagraphs.length == 0) {
        var noFoundMsg = "Не найдено коротких абзацев в концах секций,\n" +
                         "подходящих под условия форматирования.";

        if (showStatistics >= 1) {
            MsgBox(scriptName + "\n" +
                   "ver. " + version + "\n" +
                   "---------------------------\n\n" +
                   noFoundMsg);
        }
        return;
    }

    // ==================================================
    // ПРОВЕРКА ПОРОГА ДЛЯ ПОШАГОВОГО РЕЖИМА
    // ==================================================

    if (stepByStepMode == 1 && !hasSelection && targetParagraphs.length > stepByStepWarningThreshold) {
        var warningMsg = modeLabel + "\n" +
                         "Подходящих для обработки абзацев: " + targetParagraphs.length + " шт.\n\n" +
                         "Уверены, что хотите обработать их вручную?\n" +
                         "(прервать выполнение скрипта будет нельзя)\n\n" +
                         "Рекомендуется обработать в автоматическом режиме.\n\n" +
                         "Да — обработать вручную\n" +
                         "Нет — переключиться на автоматическую обработку";

        if (!AskYesNo(scriptName + "\n" +
                      "ver. " + version + "\n" +
                      "---------------------------\n\n" +
                      warningMsg)) {
            stepByStepMode = 0;
        }
    }

    // ==================================================
    // ОБРАБОТКА
    // ==================================================

    if (stepByStepMode == 1 && !hasSelection) {
        var totalCandidates = targetParagraphs.length;

        for (var i = 0; i < targetParagraphs.length; i++) {
            var p = targetParagraphs[i].paragraph;
            var plainText = getPlainText(p);
            
            selectElement(p);
            
            var displayText = plainText;
            if (displayText.length > 50) {
                displayText = displayText.substring(0, 50) + "...";
            }

            var sectionInfo = "";
            var sType = getSectionType(p);
            if (sType == "notes") sectionInfo = " [сноски]";
            if (sType == "comments") sectionInfo = " [комментарии]";

            var progressInfo = " (" + (i + 1) + " из " + totalCandidates + ")";

            var question = "Найден короткий абзац в конце секции" + sectionInfo + progressInfo + ":\n\n" +
                           "\"" + displayText + "\"\n\n" +
                           "Форматировать?\n" +
                           "(" + (formatStyle == 1 ? "курсив" : "жирность") + ")";

            if (AskYesNo(scriptName + "\n" +
                         "ver. " + version + "\n" +
                         "---------------------------\n\n" +
                         question)) {
                if (formatFound) {
                    formatParagraph(p);
                }
            }
        }
    } else {
        if (showStatistics == 1 && !hasSelection) {
            var foundCount = targetParagraphs.length;
            var confirmMsg = modeLabel + "\n" +
                             "Найдено коротких абзацев: " + foundCount + "\n\n" +
                             "Будет применено форматирование:\n" +
                             (formatStyle == 1 ? "курсив (EM)" : "жирность (STRONG)") + "\n\n" +
                             "Продолжить?";

            if (!AskYesNo(scriptName + "\n" +
                          "ver. " + version + "\n" +
                          "---------------------------\n\n" +
                          confirmMsg)) {
                return;
            }
        }

        var startTime = new Date();

        window.external.BeginUndoUnit(document, scriptName + " " + version);

        var formattedCount = 0;
        for (var i = 0; i < targetParagraphs.length; i++) {
            if (formatFound) {
                formatParagraph(targetParagraphs[i].paragraph);
            }
            formattedCount++;
        }

        window.external.EndUndoUnit(document);

        var endTime = new Date();
        var timeDiff = (endTime - startTime) / 1000;
        var timeStr = timeDiff.toFixed(3);
        timeStr = timeStr.replace(/\./g, ",");

        if (showStatistics >= 1) {
            var formatType = (formatStyle == 1) ? "курсивом" : "жирностью";
            var resultMessage = "✓ " + modeLabel.replace(":", "") + "\n" +
                               "  • Преобразовано абзацев: " + formattedCount + "\n" +
                               "  • Форматирование: " + formatType + "\n\n" +
                               "✓ Время выполнения: " + timeStr + " сек";

            MsgBox(scriptName + "\n" +
                   "ver. " + version + "\n" +
                   "---------------------------\n\n" +
                   resultMessage);
        }
    }
}

// ==================================================
// КОНЕЦ СКРИПТА
// ==================================================
