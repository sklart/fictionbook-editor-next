// Скрипт "Сформатировать абзац(ы) цитатой (расширенная версия)" для редактора FBE
// version 1.5
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для форматирования выделенных абзацев цитатой в fb2 документах.
// Обрабатываются обычные абзацы, подзаголовки и text-author (только внутри эпиграфов) в выделении.
// Абзац или абзацы могут быть выделены полностью или частично или в абзаце может быть установлен курсор.
// Преобразование выделенных абзацев в цитату (<div class="cite">).
// Автоматическое создание строки "автор текста" (<p class="text-author">) 
// для последнего абзаца с настраиваемыми условиями (дата, кавычки, ФИО, ключевые слова).
// Если выделен только один абзац, строка "автор текста" не создаётся (согласно схеме fb2).
// Если в выделении уже есть text-author (внутри эпиграфа), он сохраняется как есть.
// Расформатирование цитаты от полного форматирования (удаление внешних тегов <strong>/<em>/<sub>/<sup>)
// с тремя режимами настройки.
// Подзаголовки (subtitle) могут быть внутри цитаты, но никогда не становятся text-author.
// Цитаты разрешены внутри эпиграфов и аннотаций.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.5, 05.06.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Сформатировать абзац(ы) цитатой (расширенная версия)";
    var version = "1.5";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 0;
    
    // Настройка создания "авторского" абзаца (text-author) для цитаты:
    // 0 - Никогда не создавать
    // 1 - Создавать, если последний абзац содержит дату, кавычки или ФИО,
    //     или ключевые слова в предыдущем абзаце*
    // 2 - если последний абзац короче предыдущего
    // 3 - Всегда создавать
    var authorParagraphMode = 1; // По умолчанию: 1
    
    // Минимальный процент разницы длины для создания авторского абзаца (0-100)
    // Используется только при authorParagraphMode = 2
    var minLengthDiffPercent = 10;
    
    // Настройка расформатирования создаваемой цитаты от исходных жирности или курсива:
    // 0 - Не расформатировать
    // 1 - Только от полного форматирования (вся цитата в одинаковых тегах)
    // 2 - Расформатировать всегда (даже при частичном форматировании)
    var reformatMode = 1; // По умолчанию: только полное форматирование
    
    // Максимальная длина абзаца для проверки ФИО (символов)
    var maxNameLength = 100;
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    var startTime = new Date().getTime();
    
    // Получаем неразрывный пробел из настроек FBE
    var nbspChar;
    var nbspEntity;
    try {
        nbspChar = window.external.GetNBSP();
        if (nbspChar.charCodeAt(0) == 160)
            nbspEntity = "&nbsp;";
        else
            nbspEntity = nbspChar;
    }
    catch(e) {
        nbspChar = String.fromCharCode(160);
        nbspEntity = "&nbsp;";
    }
    
    // ==================================================
    // ФУНКЦИИ ПРОВЕРКИ ЗАЩИЩЁННЫХ ЭЛЕМЕНТОВ
    // ==================================================
    
    function isInHistory(element) {
        var parent = element.parentNode;
        while (parent) {
            if (parent.nodeName == "DIV" && parent.className == "history") {
                return true;
            }
            parent = parent.parentNode;
        }
        return false;
    }
    
    function isInProtectedElement(element) {
        var parent = element.parentNode;
        while (parent) {
            if (parent.nodeName == "DIV" && parent.className) {
                if (parent.className == "title") {
                    return true;
                }
            }
            parent = parent.parentNode;
        }
        return false;
    }
    
    function isInsideBlockElement(ptr) {
        if (!ptr) return false;
        
        var parent = ptr.parentNode;
        while (parent && parent.nodeName != "BODY") {
            if (parent.nodeName == "DIV") {
                var className = parent.className || "";
                if (className == "poem" || className == "stanza" || 
                    className == "cite" || className == "table" || 
                    className == "title" || className == "subtitle" || className == "history" ||
                    className == "image") {
                    return true;
                }
            }
            parent = parent.parentNode;
        }
        return false;
    }
    
    // Проверка, находится ли элемент внутри эпиграфа
    function isInsideEpigraph(element) {
        var parent = element.parentNode;
        while (parent) {
            if (parent.nodeName == "DIV" && parent.className == "epigraph") {
                return true;
            }
            parent = parent.parentNode;
        }
        return false;
    }
    
    // Проверка, есть ли уже text-author среди НЕвыделенных соседей внутри эпиграфа
    function epigraphHasTextAuthorOutsideSelection(element, selectedPs) {
        var epigraph = null;
        var parent = element.parentNode;
        while (parent) {
            if (parent.nodeName == "DIV" && parent.className == "epigraph") {
                epigraph = parent;
                break;
            }
            parent = parent.parentNode;
        }
        
        if (!epigraph) return false;
        
        var children = epigraph.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeName == "P" && child.className == "text-author") {
                var isSelected = false;
                for (var j = 0; j < selectedPs.length; j++) {
                    if (selectedPs[j] === child) {
                        isSelected = true;
                        break;
                    }
                }
                if (!isSelected) {
                    return true;
                }
            }
        }
        
        return false;
    }
    
    // ==================================================
    // ФУНКЦИИ НАВИГАЦИИ
    // ==================================================
    
    function getNextNode(el) {
        if (el.firstChild && el.nodeName != "P")
            el = el.firstChild;
        else {
            while (!el.nextSibling)
                el = el.parentNode;
            el = el.nextSibling;
        }
        return el;
    }
    
    function getNextP(el) {
        var savedEl = el;
        while (el && (el.nodeName != "P" || el == savedEl))
            el = getNextNode(el);
        return el;
    }
    
    // ==================================================
    // ФУНКЦИЯ ВСТАВКИ МАРКЕРОВ
    // ==================================================
    
    var rndm, startId, endId;
    var markerTagName = "I";
    
    function insertSelectionMarkers() {
        rndm = Math.round(Math.random() * 100000).toString();
        startId = "BlockStart" + rndm;
        endId = "BlockEnd" + rndm;
        var tr = document.selection.createRange();
        var tr2 = tr.duplicate();
        tr.collapse();
        tr.pasteHTML("<" + markerTagName + " id=" + startId + "></" + markerTagName + ">");
        tr2.collapse(false);
        tr2.pasteHTML("<" + markerTagName + " id=" + endId + "></" + markerTagName + ">");
    }
    
    // ==================================================
    // ФУНКЦИЯ ПРОВЕРКИ ВАЛИДНОСТИ АБЗАЦА
    // ==================================================
    
    function isValidParagraph(p) {
        if (!p || p.nodeName != "P") return false;
        
        var className = p.className || "";
        
        // text-author разрешён только внутри эпиграфа
        if (className == "text-author") {
            if (!isInsideEpigraph(p)) return false;
        } else {
            // Обычные абзацы: "" или "normal" или "subtitle"
            if (className != "" && className != "normal" && className != "subtitle") return false;
        }
        
        if (isInHistory(p)) return false;
        if (isInProtectedElement(p)) return false;
        if (isInsideBlockElement(p)) return false;
        
        return true;
    }
    
    // Проверка, является ли абзац подзаголовком
    function isSubtitle(p) {
        return p && p.nodeName == "P" && p.className == "subtitle";
    }
    
    // Проверка, является ли абзац text-author
    function isTextAuthor(p) {
        return p && p.nodeName == "P" && p.className == "text-author";
    }
    
    // ==================================================
    // ФУНКЦИЯ TRIM (СОВМЕСТИМОСТЬ С IE6)
    // ==================================================
    
    function trimStr(str) {
        if (!str) return str;
        return str.replace(/^\s+|\s+$/g, '');
    }
    
    // ==================================================
    // ФУНКЦИЯ НОРМАЛИЗАЦИИ ТЕКСТА
    // ==================================================
    
    function normalizeText(text) {
        if (!text) return text;
        return trimStr(text.replace(/\u00A0/g, ' '));
    }
    
    // ==================================================
    // ФУНКЦИЯ ПРОВЕРКИ НАЛИЧИЯ ДАТЫ В ТЕКСТЕ
    // ==================================================
    
    function hasDate(text) {
        if (!text) return false;
        
        var normalized = normalizeText(text);
        if (normalized.length === 0) return false;
        
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
            if (lowerText.indexOf(months[i]) !== -1) {
                return true;
            }
        }
        
        var datePattern = /(?:[1-9]|1[0-9]|2[0-9]|3[01])(?:[–—-](?:[1-9]|1[0-9]|2[0-9]|3[01]))?[\x20\xA0]{1,2}(января|февраля|марта|апреля|мая|июня|июля|августа|сентября|октября|ноября|декабря)/i;
        if (datePattern.test(cleanText)) return true;
        
        return false;
    }
    
    // ==================================================
    // ФУНКЦИЯ ПРОВЕРКИ НАЛИЧИЯ КАВЫЧЕК
    // ==================================================
    
    function hasQuotes(text) {
        if (!text) return false;
        
        var quotePairs = [
            { open: '«', close: '»' },
            { open: '"', close: '"' },
            { open: '„', close: '«' }
        ];
        
        for (var i = 0; i < quotePairs.length; i++) {
            var openChar = quotePairs[i].open;
            var closeChar = quotePairs[i].close;
            var openCount = 0;
            var closeCount = 0;
            
            for (var j = 0; j < text.length; j++) {
                var currentChar = text.charAt(j);
                if (currentChar === openChar) openCount++;
                if (currentChar === closeChar) closeCount++;
            }
            
            if (openChar === closeChar) {
                if (openCount >= 2) return true;
            } else {
                if (openCount >= 1 && closeCount >= 1) return true;
            }
        }
        
        return false;
    }
    
    // ==================================================
    // ФУНКЦИЯ ПРОВЕРКИ, ЯВЛЯЕТСЯ ЛИ СТРОКА ФИО
    // ==================================================
    
    function isFIO(text) {
        if (!text || text.length > maxNameLength) return false;
        
        var normalized = normalizeText(text);
        if (normalized.length === 0) return false;
        
        var cleanText = normalized.replace(/\[\d+\]/g, '');
        cleanText = trimStr(cleanText);
        if (cleanText.length === 0) return false;
        
        var hasLetterDot = false;
        var capitalCount = 0;
        var dotCount = 0;
        
        for (var i = 0; i < cleanText.length; i++) {
            var char = cleanText.charAt(i);
            var code = char.charCodeAt(0);
            
            var isUpper = (code >= 1040 && code <= 1071) || (code >= 65 && code <= 90);
            
            if (isUpper) {
                capitalCount++;
                if (i < cleanText.length - 1 && cleanText.charAt(i + 1) === '.') {
                    hasLetterDot = true;
                }
            }
            
            if (char === '.') dotCount++;
        }
        
        var words = cleanText.split(/\s+/);
        
        if (words.length > 4) return false;
        if (capitalCount === 0) return false;
        
        var variant1 = hasLetterDot;
        var variant2 = dotCount >= 2;
        var variant3 = capitalCount >= words.length * 0.7;
        
        return variant1 || variant2 || variant3;
    }
    
    // ==================================================
    // ФУНКЦИЯ ПРОВЕРКИ КЛЮЧЕВЫХ СЛОВ В ПРЕДЫДУЩЕМ АБЗАЦЕ
    // (РЕГИСТРОНЕЗАВИСИМАЯ)
    // ==================================================
    
    function hasKeyWords(text) {
        if (!text) return false;
        
        var keyWords = [
            "С уважением", "Искренне ваш", "С любовью", "С наилучшими пожеланиям",
            "Обнимаю", "Целую", "Ваш", "Ваша",
            "sincerely", "sincerely yours", "yours sincerely", "Yours faithfully",
            "Best regards", "Kind regards", "Best wishes", "Warm regards",
            "Hope to hear from you soon", "Respectfully", "Regards"
        ];
        
        var normalized = normalizeText(text);
        var normalizedLower = normalized.toLowerCase();
        
        for (var i = 0; i < keyWords.length; i++) {
            if (normalizedLower.indexOf(keyWords[i].toLowerCase()) !== -1) {
                return true;
            }
        }
        
        return false;
    }
    
    // ==================================================
    // ФУНКЦИЯ ПОЛУЧЕНИЯ ТИПА ФОРМАТИРОВАНИЯ АБЗАЦА
    // Возвращает: "none", "strong", "em", "sub", "sup", "strong-em", "em-strong", "mixed"
    // ==================================================
    
    function getFormattingType(elem) {
        if (!elem) return "none";
        
        var html = elem.innerHTML || '';
        if (!html) return "none";
        
        var htmlWithoutNotes = html.replace(/<a\s+[^>]*class\s*=\s*["']?note["']?[^>]*>.*?<\/a>/gi, '');
        var trimmed = trimStr(htmlWithoutNotes);
        
        if (/^\s*<strong(\s+[^>]*)?>\s*<em(\s+[^>]*)?>.*<\/em>\s*<\/strong>\s*$/i.test(trimmed)) {
            return "strong-em";
        }
        
        if (/^\s*<em(\s+[^>]*)?>\s*<strong(\s+[^>]*)?>.*<\/strong>\s*<\/em>\s*$/i.test(trimmed)) {
            return "em-strong";
        }
        
        if (/^\s*<strong(\s+[^>]*)?>.*<\/strong>\s*$/i.test(trimmed)) {
            return "strong";
        }
        
        if (/^\s*<em(\s+[^>]*)?>.*<\/em>\s*$/i.test(trimmed)) {
            return "em";
        }
        
        if (/^\s*<sub(\s+[^>]*)?>.*<\/sub>\s*$/i.test(trimmed)) {
            return "sub";
        }
        
        if (/^\s*<sup(\s+[^>]*)?>.*<\/sup>\s*$/i.test(trimmed)) {
            return "sup";
        }
        
        if (trimmed.indexOf('<') === -1 || (trimmed.indexOf('<strong') === -1 && trimmed.indexOf('<em') === -1 && trimmed.indexOf('<sub') === -1 && trimmed.indexOf('<sup') === -1)) {
            return "none";
        }
        
        return "mixed";
    }
    
    // ==================================================
    // ФУНКЦИЯ ПРОВЕРКИ ПОЛНОГО ФОРМАТИРОВАНИЯ
    // ==================================================
    
    function isFullyFormatted(elem) {
        var type = getFormattingType(elem);
        return type !== "none" && type !== "mixed";
    }
    
    // ==================================================
    // ФУНКЦИЯ ПРОВЕРКИ, ЧТО ВСЕ АБЗАЦЫ ИМЕЮТ ОДИНАКОВОЕ ПОЛНОЕ ФОРМАТИРОВАНИЕ
    // ==================================================
    
    function allParagraphsHaveSameFormatting(ps) {
        if (!ps || ps.length === 0) return false;
        
        var firstType = getFormattingType(ps[0]);
        
        if (firstType === "none" || firstType === "mixed") {
            return false;
        }
        
        for (var i = 1; i < ps.length; i++) {
            var currentType = getFormattingType(ps[i]);
            if (currentType !== firstType) {
                return false;
            }
        }
        
        return true;
    }
    
    // ==================================================
    // ФУНКЦИЯ УДАЛЕНИЯ ВНЕШНЕГО ФОРМАТИРОВАНИЯ
    // ==================================================
    
    function removeOuterFormatting(elem) {
        if (!elem) return;
        
        var originalHTML = elem.innerHTML;
        if (!originalHTML) return;
        
        var noteCounter = 0;
        var notesMap = {};
        var htmlWithMarkers = originalHTML.replace(/<a\s+[^>]*class\s*=\s*["']?note["']?[^>]*>.*?<\/a>/gi, function(match) {
            var marker = '<!--NOTE_' + (noteCounter++) + '-->';
            notesMap[marker] = match;
            return marker;
        });
        
        var newHTML = htmlWithMarkers;
        var changed = true;
        var maxPasses = 5;
        var pass = 0;
        
        while (changed && pass < maxPasses) {
            changed = false;
            pass++;
            
            var singlePattern = /^\s*<(strong|em|sub|sup)(\s+[^>]*)?>(.*)<\/\1>\s*$/i;
            var match = newHTML.match(singlePattern);
            if (match) {
                newHTML = match[3];
                changed = true;
                continue;
            }
            
            var doublePattern1 = /^\s*<strong(\s+[^>]*)?>\s*<em(\s+[^>]*)?>(.*)<\/em>\s*<\/strong>\s*$/i;
            var doubleMatch1 = newHTML.match(doublePattern1);
            if (doubleMatch1) {
                newHTML = doubleMatch1[3];
                changed = true;
                continue;
            }
            
            var doublePattern2 = /^\s*<em(\s+[^>]*)?>\s*<strong(\s+[^>]*)?>(.*)<\/strong>\s*<\/em>\s*$/i;
            var doubleMatch2 = newHTML.match(doublePattern2);
            if (doubleMatch2) {
                newHTML = doubleMatch2[3];
                changed = true;
                continue;
            }
        }
        
        for (var marker in notesMap) {
            newHTML = newHTML.replace(marker, notesMap[marker]);
        }
        
        try {
            elem.innerHTML = newHTML;
        } catch(e) {}
    }
    
    // ==================================================
    // ФУНКЦИЯ УДАЛЕНИЯ ВСЕХ ТЕГОВ ФОРМАТИРОВАНИЯ
    // ==================================================
    
    function removeAllFormatting(elem) {
        if (!elem) return;
        
        var originalHTML = elem.innerHTML;
        if (!originalHTML) return;
        
        var newHTML = originalHTML;
        
        for (var pass = 0; pass < 3; pass++) {
            newHTML = newHTML.replace(/<(strong|em|sub|sup)(\s+[^>]*)?>([^<]*)<\/\1>/gi, '$3');
            newHTML = newHTML.replace(/<(strong|em|sub|sup)(\s+[^>]*)?>([^<]*(?:<(?!\/?\1)[^>]*>[^<]*)*)<\/\1>/gi, '$3');
        }
        
        try {
            elem.innerHTML = newHTML;
        } catch(e) {}
    }
    
    // ==================================================
    // ФУНКЦИЯ РАСФОРМАТИРОВАНИЯ
    // ==================================================
    
    function reformatElement(elem, mode) {
        if (!elem || mode === 0) return;
        
        if (mode === 1) {
            if (isFullyFormatted(elem)) {
                removeOuterFormatting(elem);
            }
        } else if (mode === 2) {
            removeAllFormatting(elem);
        }
    }
    
    // ==================================================
    // ФУНКЦИЯ ПРИНУДИТЕЛЬНОГО РАСФОРМАТИРОВАНИЯ АВТОРСКОЙ СТРОКИ
    // ==================================================
    
    function forceReformatAuthorElement(elem) {
        removeAllFormatting(elem);
    }
    
    // ==================================================
    // ФУНКЦИЯ ОПРЕДЕЛЕНИЯ, НУЖНО ЛИ ДЕЛАТЬ АВТОРСКУЮ СТРОКУ
    // ==================================================
    
    function shouldMakeAuthor(lastText, prevText, totalParagraphs, lastIsSubtitle, lastIsTextAuthor, insideEpigraphWithOutsideAuthor) {
        if (!lastText) return false;
        
        if (totalParagraphs <= 1) return false;
        
        if (lastIsSubtitle) return false;
        
        if (lastIsTextAuthor) return false;
        
        if (insideEpigraphWithOutsideAuthor) return false;
        
        var last = normalizeText(lastText);
        if (last.length === 0) return false;
        
        switch(authorParagraphMode) {
            case 0:
                return false;
            case 1:
                if (hasDate(last)) return true;
                if (hasQuotes(last)) return true;
                if (isFIO(last)) return true;
                if (prevText && hasKeyWords(prevText)) return true;
                return false;
            case 2:
                if (prevText) {
                    var prev = normalizeText(prevText);
                    if (prev.length > 0) {
                        var lengthDiffPercent = ((prev.length - last.length) / prev.length) * 100;
                        if (lengthDiffPercent >= minLengthDiffPercent) return true;
                    }
                }
                return false;
            case 3:
                return true;
            default:
                return false;
        }
    }
    
    // ==================================================
    // ФУНКЦИЯ ПЕРЕХОДА В КОНЕЦ ЭЛЕМЕНТА
    // ==================================================
    
    function GoToEndOfElement(elem) {
        if(!elem) return;
        var b = elem.getBoundingClientRect();
        if (b.bottom - b.top <= window.external.getViewHeight())
            window.scrollBy(0, (b.top + b.bottom - window.external.getViewHeight()) / 2);
        else
            window.scrollBy(0, b.top);
        var r = document.selection.createRange();
        if (!r || !("compareEndPoints" in r)) return;
        r.moveToElementText(elem);
        r.collapse(false);
        r.select();
    }
    
    // ==================================================
    // ПРОВЕРКА НА БЛОЧНУЮ КАРТИНКУ В ВЫДЕЛЕНИИ
    // ==================================================
    
    var sel = document.selection;
    if (sel && sel.type == "Control") {
        var controlRange = sel.createRange();
        if (controlRange && controlRange.length > 0) {
            var controlElement = controlRange.item(0);
            if (controlElement && controlElement.nodeName == "DIV" && controlElement.className == "image") {
                var invalidMsg = scriptName + "\nver. " + version + "\n\nДолжны быть выделены только обычные абзацы, эпиграфы, аннотации или подзаголовки.";
                MsgBox(invalidMsg, "FBE скрипт");
                return;
            }
        }
    }
    
    // ==================================================
    // ОСНОВНАЯ ЛОГИКА
    // ==================================================
    
    var tr = document.selection.createRange();
    if (tr.parentElement().nodeName == "TEXTAREA" || tr.parentElement().nodeName == "INPUT") {
        var errMsg = "Ошибка. Должно быть выделение в тексте книги, а не в поле ввода.";
        MsgBox(scriptName + "\nver. " + version + "\n\n" + errMsg, "FBE скрипт");
        return;
    }
    
    window.external.BeginUndoUnit(document, scriptName + " ver. " + version);
    
    try {
        insertSelectionMarkers();
        
        var blockStartNode = document.getElementById(startId);
        var blockEndNode = document.getElementById(endId);
        
        if (!blockStartNode || !blockEndNode) {
            throw new Error("Не удалось вставить маркеры выделения");
        }
        
        var blockStartEl = blockStartNode;
        while (blockStartEl && blockStartEl.nodeName != "BODY" && blockStartEl.nodeName != "P") {
            blockStartEl = blockStartEl.parentNode;
        }
        
        if (!blockStartEl || blockStartEl.nodeName == "BODY") {
            throw new Error("Не удалось определить начальный абзац");
        }
        
        var blockEndEl = blockEndNode;
        while (blockEndEl && blockEndEl.nodeName != "BODY" && blockEndEl.nodeName != "P") {
            blockEndEl = blockEndEl.parentNode;
        }
        
        if (!blockEndEl || blockEndEl.nodeName == "BODY") {
            blockEndEl = blockEndNode;
            if (blockEndEl.previousSibling && blockEndEl.previousSibling.nodeName == "P") {
                blockEndEl = blockEndEl.previousSibling;
            }
        }
        
        if (!blockEndEl || blockEndEl.nodeName != "P") {
            throw new Error("Не удалось определить конечный абзац");
        }
        
        // Проверка на запрещённые элементы между начальным и конечным абзацами
        var tempNode = blockStartEl;
        while (tempNode && tempNode != blockEndEl) {
            if (tempNode.nodeName == "DIV" && 
                (tempNode.className == "image" || tempNode.className == "title" || 
                 tempNode.className == "poem" || 
                 tempNode.className == "stanza" || tempNode.className == "cite" || 
                 tempNode.className == "table" || 
                 tempNode.className == "history")) {
                var invalidMsg = scriptName + "\nver. " + version + "\n\nДолжны быть выделены только обычные абзацы, эпиграфы, аннотации или подзаголовки.";
                MsgBox(invalidMsg, "FBE скрипт");
                var sn = document.getElementById(startId);
                var en = document.getElementById(endId);
                if (sn) sn.removeNode(true);
                if (en) en.removeNode(true);
                window.external.EndUndoUnit(document);
                return;
            }
            tempNode = getNextNode(tempNode);
        }
        
        var ps = [];
        var ptr = blockStartEl;
        
        while (ptr) {
            if (!isValidParagraph(ptr)) {
                var invalidMsg = scriptName + "\nver. " + version + "\n\nДолжны быть выделены только обычные абзацы, эпиграфы, аннотации или подзаголовки.";
                MsgBox(invalidMsg, "FBE скрипт");
                var sn = document.getElementById(startId);
                var en = document.getElementById(endId);
                if (sn) sn.removeNode(true);
                if (en) en.removeNode(true);
                window.external.EndUndoUnit(document);
                return;
            }
            
            ps.push(ptr);
            if (ptr === blockEndEl) break;
            ptr = getNextP(ptr);
        }
        
        if (ps.length == 0) {
            throw new Error("Не найдено абзацев для обработки");
        }
        
        // Проверяем родительский элемент
        var parentEl = ps[0].parentNode;
        
        var parentClassName = parentEl.className || "";
        if (parentClassName != "section" && parentClassName != "annotation" && parentClassName != "epigraph") {
            throw new Error("Абзацы должны находиться в секции (section), аннотации (annotation) или эпиграфе (epigraph)");
        }
        
        for (var i = 1; i < ps.length; i++) {
            if (ps[i].parentNode != parentEl) {
                throw new Error("Выделенные абзацы должны находиться в одном разделе (одной секции, аннотации или эпиграфе)");
            }
        }
        
        var insideEpigraph = isInsideEpigraph(ps[0]);
        
        var insideEpigraphWithOutsideAuthor = insideEpigraph && epigraphHasTextAuthorOutsideSelection(ps[0], ps);
        
        var alreadyHasTextAuthor = false;
        for (var i = 0; i < ps.length; i++) {
            if (isTextAuthor(ps[i])) {
                alreadyHasTextAuthor = true;
                break;
            }
        }
        
        var shouldReformatWholeCite = false;
        if (reformatMode === 1) {
            shouldReformatWholeCite = allParagraphsHaveSameFormatting(ps);
        }
        
        // Создаём cite
        var cite = document.createElement("DIV");
        cite.className = "cite";
        
        var processedCount = 0;
        var reformattedCount = 0;
        var hasAuthor = false;
        
        var firstNonEmpty = -1;
        var lastNonEmpty = -1;
        var nonEmptyPs = [];
        
        for (var i = 0; i < ps.length; i++) {
            var pText = ps[i].innerText || ps[i].textContent || "";
            var isEmpty = trimStr(pText.replace(/\u00A0/g, ' ')) === "";
            if (!isEmpty) {
                nonEmptyPs.push(ps[i]);
                if (firstNonEmpty === -1) firstNonEmpty = i;
                lastNonEmpty = i;
            }
        }
        
        // Определяем, нужно ли создавать авторскую строку
        if (!alreadyHasTextAuthor && nonEmptyPs.length > 1) {
            var lastNonEmptyP = nonEmptyPs[nonEmptyPs.length - 1];
            var lastNonEmptyText = lastNonEmptyP.innerText || lastNonEmptyP.textContent || "";
            var lastIsSubtitle = isSubtitle(lastNonEmptyP);
            var lastIsTextAuthor = isTextAuthor(lastNonEmptyP);
            
            var prevNonEmptyText = "";
            if (nonEmptyPs.length > 1) {
                var prevNonEmptyP = nonEmptyPs[nonEmptyPs.length - 2];
                prevNonEmptyText = prevNonEmptyP.innerText || prevNonEmptyP.textContent || "";
            }
            
            if (shouldMakeAuthor(lastNonEmptyText, prevNonEmptyText, nonEmptyPs.length, lastIsSubtitle, lastIsTextAuthor, insideEpigraphWithOutsideAuthor)) {
                hasAuthor = true;
            }
        }
        
        // Формируем список абзацев для вставки в cite
        var endIdx = lastNonEmpty;
        if (hasAuthor) {
            endIdx = -1;
            if (nonEmptyPs.length > 1) {
                var prevNonEmptyP = nonEmptyPs[nonEmptyPs.length - 2];
                for (var i = ps.length - 1; i >= 0; i--) {
                    if (ps[i] === prevNonEmptyP) {
                        endIdx = i;
                        break;
                    }
                }
            }
        }
        
        if (alreadyHasTextAuthor) {
            endIdx = -1;
            for (var i = nonEmptyPs.length - 1; i >= 0; i--) {
                if (!isTextAuthor(nonEmptyPs[i])) {
                    var targetP = nonEmptyPs[i];
                    for (var j = ps.length - 1; j >= 0; j--) {
                        if (ps[j] === targetP) {
                            endIdx = j;
                            break;
                        }
                    }
                    break;
                }
            }
        }
        
        for (var i = 0; i < ps.length; i++) {
            var p = ps[i];
            var pText = p.innerText || p.textContent || "";
            var isEmpty = trimStr(pText.replace(/\u00A0/g, ' ')) === "";
            
            // Пропускаем пустые в начале
            if (isEmpty && processedCount === 0 && !hasAuthor && !alreadyHasTextAuthor) {
                continue;
            }
            
            // Пропускаем пустые в конце
            if (isEmpty && !hasAuthor && !alreadyHasTextAuthor) {
                var hasNonEmptyAfter = false;
                for (var j = i + 1; j < ps.length; j++) {
                    var nextText = ps[j].innerText || ps[j].textContent || "";
                    if (trimStr(nextText.replace(/\u00A0/g, ' ')) !== "") {
                        if (!hasAuthor || j < ps.length - 1) {
                            hasNonEmptyAfter = true;
                        }
                        break;
                    }
                }
                if (!hasNonEmptyAfter) {
                    continue;
                }
            }
            
            // Пропускаем последний непустой, если он стал автором
            if (hasAuthor && i === lastNonEmpty) {
                continue;
            }
            
            // Если дошли до конца — прекращаем
            if (!hasAuthor && !alreadyHasTextAuthor && endIdx >= 0 && i > endIdx) {
                break;
            }
            
            if (alreadyHasTextAuthor && endIdx >= 0 && i > endIdx && !isTextAuthor(p)) {
                continue;
            }
            
            // ИСПОЛЬЗУЕМ cloneNode ДЛЯ СОХРАНЕНИЯ &nbsp; В ПУСТЫХ СТРОКАХ
            var pCopy = p.cloneNode(true);
            
            // Корректируем класс при необходимости
            if (isSubtitle(p)) {
                pCopy.className = "subtitle";
            } else if (isTextAuthor(p)) {
                pCopy.className = "text-author";
                try {
                    forceReformatAuthorElement(pCopy);
                } catch(e) {}
            } else if (!isEmpty) {
                // Обычный непустой абзац — сбрасываем класс, если был "normal"
                if (pCopy.className == "normal") {
                    pCopy.className = "";
                }
            }
            
            // Применяем расформатирование
            if (!isEmpty && !isTextAuthor(p) && reformatMode > 0) {
                var shouldReformat = false;
                
                if (reformatMode === 2) {
                    shouldReformat = true;
                } else if (reformatMode === 1) {
                    shouldReformat = shouldReformatWholeCite && isFullyFormatted(pCopy);
                }
                
                if (shouldReformat) {
                    try {
                        reformatElement(pCopy, reformatMode);
                        reformattedCount++;
                    } catch(e) {}
                }
            }
            
            cite.appendChild(pCopy);
            processedCount++;
        }
        
        // Если нужно создать НОВУЮ авторскую строку
        if (hasAuthor && !alreadyHasTextAuthor) {
            var lastP = nonEmptyPs[nonEmptyPs.length - 1];
            var authorP = lastP.cloneNode(true);
            authorP.className = "text-author";
            
            try {
                forceReformatAuthorElement(authorP);
            } catch(e) {}
            
            cite.appendChild(authorP);
            processedCount++;
        }
        
        // Если cite пустой, добавляем пустую строку
        if (cite.childNodes.length === 0) {
            var emptyP = document.createElement("P");
            emptyP.innerHTML = nbspChar;
            cite.appendChild(emptyP);
        }
        
        // Вставляем cite перед первым обработанным абзацем
        parentEl.insertBefore(cite, ps[0]);
        
        // Удаляем оригинальные абзацы
        for (var i = 0; i < ps.length; i++) {
            ps[i].removeNode(true);
        }
        
        // Удаляем маркеры
        var sn = document.getElementById(startId);
        var en = document.getElementById(endId);
        if (sn) sn.removeNode(true);
        if (en) en.removeNode(true);
        
        // Переходим в конец созданного cite
        GoToEndOfElement(cite);
        
        var endTime = new Date().getTime();
        var timeDiff = (endTime - startTime) / 1000;
        var timeStr = timeDiff.toFixed(3).replace(".", ",");
        
        if (showStatistics == 1) {
            var msg = scriptName + "\nver. " + version + "\n\n";
            msg += "✓ Цитата успешно создана\n";
            msg += "  • Обработано абзацев: " + processedCount + "\n";
            
            if (alreadyHasTextAuthor) {
                msg += "  • Сохранён существующий text-author\n";
            } else if (hasAuthor) {
                msg += "  • Создана строка text-author\n";
            }
            
            if (insideEpigraph) {
                msg += "  • Цитата внутри эпиграфа\n";
            }
            
            if (reformatMode > 0) {
                var reformatModeText = reformatMode === 1 ? "только полное (вся цитата)" : "всегда";
                msg += "  • Режим расформатирования: " + reformatModeText + "\n";
                if (reformatMode === 1 && !shouldReformatWholeCite) {
                    msg += "  • Цитата не расформатирована (возможно разное форматирование строк)\n";
                } else {
                    msg += "  • Расформатировано абзацев: " + reformattedCount + "\n";
                }
            }
            
            var authorModeText = "";
            switch(authorParagraphMode) {
                case 0: authorModeText = "никогда"; break;
                case 1: authorModeText = "при наличии даты, кавычек, ФИО или ключевых слов"; break;
                case 2: authorModeText = "если последний абзац короче предыдущего"; break;
                case 3: authorModeText = "всегда"; break;
            }
            msg += "  • Создание строки text-author: " + authorModeText + "\n";
            
            msg += "\nВремя выполнения: " + timeStr + " сек";
            
            MsgBox(msg, "FBE скрипт");
        }
        
        try {
            window.external.SetStatusBarText(scriptName + " ver. " + version + ": создана цитата из " + processedCount + " абзацев. Время: " + timeStr + " сек.");
        }
        catch(e) {}
        
    } catch(e) {
        var sn = document.getElementById(startId);
        var en = document.getElementById(endId);
        if (sn) sn.removeNode(true);
        if (en) en.removeNode(true);
        
        var errMsg = e.message;
        MsgBox(scriptName + "\nver. " + version + "\n\n" + errMsg, "FBE скрипт");
    }
    
    window.external.EndUndoUnit(document);
}
