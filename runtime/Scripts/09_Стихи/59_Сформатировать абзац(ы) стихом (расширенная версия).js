// Скрипт "Сформатировать абзац(ы) стихом (расширенная версия)" для редактора FBE
// version 1.5
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для форматирования выделенных абзацев стихом в fb2 документах.
// Обрабатываются только обычные абзацы в выделении.
// Абзац или абзацы могут быть выделены полностью или частично или в абзаце может быть установлен курсор.
// Преобразование выделенных абзацев в стих (<div class="poem">).
// Автоматическое разбиение на строфы (stanza) по пустым строкам.
// Автоматическое создание строки "автор текста" (<p class="text-author">) 
// для последнего абзаца с настраиваемыми условиями (дата, кавычки, ФИО).
// Если выделен только один абзац, строка "автор текста" не создаётся (согласно схеме fb2).
// Расформатирование стиха от полного форматирования (удаление внешних тегов <strong>/<em>)
// с тремя режимами настройки.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.5, 15.04.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Сформатировать абзац(ы) стихом (расширенная версия)";
    var version = "1.5";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 0;
    
    // Настройка расформатирования создаваемого стиха от исходных жирности или курсива:
    // 0 - Не расформатировать
    // 1 - Только от полного форматирования (весь стих в одинаковых тегах)
    // 2 - Расформатировать всегда (даже при частичном форматировании)
    var reformatMode = 1; // По умолчанию: только полное форматирование
    
    // Настройка создания "авторского" абзаца (text-author) для стиха:
    // 0 - Никогда не создавать
    // 1 - Создавать, если последний абзац содержит дату
    // 2 - Создавать, если последний абзац содержит кавычки или ФИО
    // 3 - Всегда создавать
    var authorParagraphMode = 2; // По умолчанию: условие 1 ИЛИ 2
    
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
    
    function isInFirstAnnotation(element) {
        var parent = element.parentNode;
        while (parent) {
            if (parent.nodeName == "DIV" && parent.className == "annotation") {
                var bodyDivs = document.getElementsByTagName("DIV");
                var firstBodyFound = false;
                for (var i = 0; i < bodyDivs.length; i++) {
                    var div = bodyDivs[i];
                    if (div.className == "body" && div.getAttribute("fbname") == "") {
                        firstBodyFound = true;
                        break;
                    }
                    if (div == parent) {
                        return true;
                    }
                }
                return false;
            }
            parent = parent.parentNode;
        }
        return false;
    }
    
    function isInProtectedElement(element) {
        var parent = element.parentNode;
        while (parent) {
            if (parent.nodeName == "DIV" && parent.className) {
                if (parent.className == "epigraph" || parent.className == "title") {
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
                if (className == "poem" || className == "stanza" || className == "epigraph" || 
                    className == "cite" || className == "annotation" || className == "table" || 
                    className == "title" || className == "subtitle" || className == "history" ||
                    className == "image") {
                    return true;
                }
            }
            parent = parent.parentNode;
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
        if (className != "" && className != "normal") return false;
        
        if (isInHistory(p)) return false;
        if (isInFirstAnnotation(p)) return false;
        if (isInProtectedElement(p)) return false;
        if (isInsideBlockElement(p)) return false;
        
        return true;
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
        
        // Очищаем от сносок
        var cleanText = normalized.replace(/\[\d+\]/g, '');
        cleanText = trimStr(cleanText);
        
        // Проверка на год (17xx-20xx)
        var yearPattern = /(?:[1][789][0-9][0-9]|[2][0][0-2][0-9])/;
        if (yearPattern.test(cleanText)) return true;
        
        // Проверка на год с "г."
        var yearWithGPattern = /(?:[1][789][0-9][0-9]|[2][0][0-2][0-9])[\x20\xA0]г[\.\)]?/i;
        if (yearWithGPattern.test(cleanText)) return true;
        
        // Месяцы (все формы)
        var months = [
            // Полные названия
            "январь", "февраль", "март", "апрель", "май", "июнь", "июль", 
            "август", "сентябрь", "октябрь", "ноябрь", "декабрь",
            // Родительный падеж
            "января", "февраля", "марта", "апреля", "мая", "июня", "июля", 
            "августа", "сентября", "октября", "ноября", "декабря",
            // Сокращения без точки
            "янв", "фев", "февр", "мар", "апр", "июн", "июл", "авг", 
            "сен", "сент", "окт", "ноя", "нояб", "дек",
            // Сокращения с точкой
            "янв.", "фев.", "февр.", "мар.", "апр.", "июн.", "июл.", 
            "авг.", "сен.", "сент.", "окт.", "ноя.", "нояб.", "дек."
        ];
        
        var lowerText = cleanText.toLowerCase();
        for (var i = 0; i < months.length; i++) {
            if (lowerText.indexOf(months[i]) !== -1) {
                return true;
            }
        }
        
        // Проверка на дату вида "15 мая" или "15-16 мая"
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
    // ФУНКЦИЯ ПОЛУЧЕНИЯ ТИПА ФОРМАТИРОВАНИЯ АБЗАЦА
    // Возвращает: "none", "strong", "em", "strong-em", "em-strong", "mixed"
    // ==================================================
    
    function getFormattingType(elem) {
        if (!elem) return "none";
        
        var html = elem.innerHTML || '';
        if (!html) return "none";
        
        var htmlWithoutNotes = html.replace(/<a\s+[^>]*class\s*=\s*["']?note["']?[^>]*>.*?<\/a>/gi, '');
        var trimmed = trimStr(htmlWithoutNotes);
        
        // Проверка на <strong><em>...</em></strong>
        if (/^\s*<strong(\s+[^>]*)?>\s*<em(\s+[^>]*)?>.*<\/em>\s*<\/strong>\s*$/i.test(trimmed)) {
            return "strong-em";
        }
        
        // Проверка на <em><strong>...</strong></em>
        if (/^\s*<em(\s+[^>]*)?>\s*<strong(\s+[^>]*)?>.*<\/strong>\s*<\/em>\s*$/i.test(trimmed)) {
            return "em-strong";
        }
        
        // Проверка на <strong>...</strong>
        if (/^\s*<strong(\s+[^>]*)?>.*<\/strong>\s*$/i.test(trimmed)) {
            return "strong";
        }
        
        // Проверка на <em>...</em>
        if (/^\s*<em(\s+[^>]*)?>.*<\/em>\s*$/i.test(trimmed)) {
            return "em";
        }
        
        // Проверка на отсутствие форматирования
        if (trimmed.indexOf('<') === -1 || (trimmed.indexOf('<strong') === -1 && trimmed.indexOf('<em') === -1)) {
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
        
        // Если первый абзац не имеет полного форматирования
        if (firstType === "none" || firstType === "mixed") {
            return false;
        }
        
        // Проверяем, что все остальные абзацы имеют такой же тип
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
        
        // Многопроходное удаление тегов форматирования
        while (changed && pass < maxPasses) {
            changed = false;
            pass++;
            
            // Паттерн для одиночного тега
            var singlePattern = /^\s*<(strong|em)(\s+[^>]*)?>(.*)<\/\1>\s*$/i;
            var match = newHTML.match(singlePattern);
            if (match) {
                newHTML = match[3];
                changed = true;
                continue;
            }
            
            // Паттерны для двойного форматирования
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
        
        // Восстанавливаем сноски
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
            newHTML = newHTML.replace(/<(em|strong)(\s+[^>]*)?>([^<]*)<\/\1>/gi, '$3');
            newHTML = newHTML.replace(/<(em|strong)(\s+[^>]*)?>([^<]*(?:<(?!\/?\1)[^>]*>[^<]*)*)<\/\1>/gi, '$3');
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
    
    function shouldMakeAuthor(currentText, totalParagraphs) {
        if (!currentText) return false;
        
        // Если всего один абзац - никогда не делаем авторскую строку
        if (totalParagraphs <= 1) return false;
        
        var current = normalizeText(currentText);
        if (current.length === 0) return false;
        
        switch(authorParagraphMode) {
            case 0:
                return false;
            case 1:
                return hasDate(current);
            case 2:
                if (hasDate(current)) return true;
                if (hasQuotes(current)) return true;
                if (isFIO(current)) return true;
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
                var invalidMsg = scriptName + "\nver. " + version + "\n\nДолжны быть выделены только обычные абзацы.";
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
                 tempNode.className == "epigraph" || tempNode.className == "poem" || 
                 tempNode.className == "stanza" || tempNode.className == "cite" || 
                 tempNode.className == "table" || tempNode.className == "annotation" || 
                 tempNode.className == "history")) {
                var invalidMsg = scriptName + "\nver. " + version + "\n\nДолжны быть выделены только обычные абзацы.";
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
                var invalidMsg = scriptName + "\nver. " + version + "\n\nДолжны быть выделены только обычные абзацы.";
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
        
        // Проверяем, что все абзацы в одной секции
        var parentEl = ps[0].parentNode;
        if (!parentEl || parentEl.className != "section") {
            throw new Error("Абзацы должны находиться в секции (section)");
        }
        
        for (var i = 1; i < ps.length; i++) {
            if (ps[i].parentNode != parentEl) {
                throw new Error("Выделенные абзацы должны находиться в одной секции");
            }
        }
        
        // Проверяем, нужно ли расформатировать ВЕСЬ стих
        var shouldReformatWholePoem = false;
        if (reformatMode === 1) {
            shouldReformatWholePoem = allParagraphsHaveSameFormatting(ps);
        }
        
        // Создаём poem
        var poem = document.createElement("DIV");
        poem.className = "poem";
        
        // Разбиваем абзацы на строфы по пустым строкам
        var stanzas = [];
        var currentStanza = [];
        var processedCount = 0;
        var reformattedCount = 0;
        var hasAuthor = false;
        
        // Количество непустых абзацев (для проверки авторской строки)
        var nonEmptyCount = 0;
        for (var i = 0; i < ps.length; i++) {
            var pText = ps[i].innerText || ps[i].textContent || "";
            var isEmpty = trimStr(pText.replace(/\u00A0/g, ' ')) === "";
            if (!isEmpty) nonEmptyCount++;
        }
        
        for (var i = 0; i < ps.length; i++) {
            var p = ps[i];
            var pText = p.innerText || p.textContent || "";
            var isEmpty = trimStr(pText.replace(/\u00A0/g, ' ')) === "";
            
            if (isEmpty && currentStanza.length > 0) {
                // Пустая строка — завершаем текущую строфу
                stanzas.push(currentStanza);
                currentStanza = [];
            } else if (!isEmpty) {
                // Не пустая строка — добавляем в текущую строфу
                currentStanza.push(p);
            }
        }
        
        // Проверяем последний абзац на авторство ДО создания строф
        // Только если всего абзацев больше одного (непустых)
        if (ps.length > 0 && nonEmptyCount > 1) {
            var lastP = ps[ps.length - 1];
            var lastPText = lastP.innerText || lastP.textContent || "";
            var lastPIsEmpty = trimStr(lastPText.replace(/\u00A0/g, ' ')) === "";
            
            if (!lastPIsEmpty && shouldMakeAuthor(lastPText, nonEmptyCount)) {
                hasAuthor = true;
                // Если последний абзац должен стать автором, убираем его из currentStanza
                // если он там есть
                if (currentStanza.length > 0 && currentStanza[currentStanza.length - 1] === lastP) {
                    currentStanza.pop();
                }
            }
        }
        
        // Добавляем последнюю строфу, если она не пустая
        if (currentStanza.length > 0) {
            stanzas.push(currentStanza);
        }
        
        // Обрабатываем каждую строфу
        for (var sIdx = 0; sIdx < stanzas.length; sIdx++) {
            var stanza = document.createElement("DIV");
            stanza.className = "stanza";
            
            var stanzaPs = stanzas[sIdx];
            
            for (var pIdx = 0; pIdx < stanzaPs.length; pIdx++) {
                var originalP = stanzaPs[pIdx];
                var pCopy = document.createElement("P");
                pCopy.innerHTML = originalP.innerHTML;
                
                // Применяем расформатирование
                if (reformatMode > 0) {
                    var shouldReformat = false;
                    
                    if (reformatMode === 2) {
                        shouldReformat = true;
                    } else if (reformatMode === 1) {
                        // В режиме 1 расформатируем только если ВЕСЬ стих имеет одинаковое форматирование
                        shouldReformat = shouldReformatWholePoem && isFullyFormatted(pCopy);
                    }
                    
                    if (shouldReformat) {
                        try {
                            reformatElement(pCopy, reformatMode);
                            reformattedCount++;
                        } catch(e) {}
                    }
                }
                
                stanza.appendChild(pCopy);
                processedCount++;
            }
            
            poem.appendChild(stanza);
        }
        
        // Если есть авторская строка, создаём её как <P class="text-author">
        if (hasAuthor) {
            var lastP = ps[ps.length - 1];
            var authorP = document.createElement("P");
            authorP.className = "text-author";
            authorP.innerHTML = lastP.innerHTML;
            
            // Принудительно удаляем всё форматирование из авторской строки
            try {
                forceReformatAuthorElement(authorP);
            } catch(e) {}
            
            poem.appendChild(authorP);
            processedCount++;
        }
        
        // Если по какой-то причине poem пустой, добавляем пустую строфу
        if (poem.childNodes.length === 0) {
            var emptyStanza = document.createElement("DIV");
            emptyStanza.className = "stanza";
            var emptyP = document.createElement("P");
            emptyP.innerHTML = nbspChar;
            emptyStanza.appendChild(emptyP);
            poem.appendChild(emptyStanza);
        }
        
        // Вставляем poem перед первым обработанным абзацем
        parentEl.insertBefore(poem, ps[0]);
        
        // Удаляем оригинальные абзацы
        for (var i = 0; i < ps.length; i++) {
            ps[i].removeNode(true);
        }
        
        // Удаляем маркеры
        var sn = document.getElementById(startId);
        var en = document.getElementById(endId);
        if (sn) sn.removeNode(true);
        if (en) en.removeNode(true);
        
        // Переходим в конец созданного poem
        GoToEndOfElement(poem);
        
        var endTime = new Date().getTime();
        var timeDiff = (endTime - startTime) / 1000;
        var timeStr = timeDiff.toFixed(3).replace(".", ",");
        
        if (showStatistics == 1) {
            var msg = scriptName + "\nver. " + version + "\n\n";
            msg += "✓ Стих успешно создан\n";
            msg += "  • Обработано абзацев: " + processedCount + "\n";
            msg += "  • Создано строф: " + stanzas.length + "\n";
            
            if (hasAuthor) {
                msg += "  • Создана строка text-author\n";
            }
            
            if (reformatMode > 0) {
                var reformatModeText = reformatMode === 1 ? "только полное (весь стих)" : "всегда";
                msg += "  • Режим расформатирования: " + reformatModeText + "\n";
                if (reformatMode === 1 && !shouldReformatWholePoem) {
                    msg += "  • Стих не расформатирован (возможно разное форматирование строк)\n";
                } else {
                    msg += "  • Расформатировано абзацев: " + reformattedCount + "\n";
                }
            }
            
            var authorModeText = "";
            switch(authorParagraphMode) {
                case 0: authorModeText = "никогда"; break;
                case 1: authorModeText = "при наличии даты"; break;
                case 2: authorModeText = "при наличии даты, кавычек или ФИО"; break;
                case 3: authorModeText = "всегда"; break;
            }
            msg += "  • Создание строки text-author: " + authorModeText + "\n";
            
            msg += "\nВремя выполнения: " + timeStr + " сек";
            
            MsgBox(msg, "FBE скрипт");
        }
        
        try {
            window.external.SetStatusBarText(scriptName + " ver. " + version + ": создан стих из " + processedCount + " абзацев. Время: " + timeStr + " сек.");
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
