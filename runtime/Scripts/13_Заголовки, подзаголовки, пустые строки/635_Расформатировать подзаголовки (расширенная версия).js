// Скрипт "Расформатировать подзаголовки" для редактора FBE
// version 2.9
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для расформатирования подзаголовков в основном разделе fb2 документа,
// не затрагивая разделы сносок-примечаний и комментариев.
// Можно обрабатывать весь документ, или от курсора и до конца, или выделенный фрагмент (по умолчанию - весь).
// Скрипт может расформатировать сразу все, либо только текстовые,
// либо только "символьные" подзаголовки, (по умолчанию - все).
// "Символьные подзаголовки" - не содержат букв, цифр (обычно - звездочки или решетки).
// Скрипт может добавлять вокруг расформатированных подзаголовков пустые строки, (по умолчанию - добавляет).

// version 2.9, 10.01.2026
//======================================

function Run() {
    // ========== НАСТРОЙКИ РЕЖИМОВ ОБРАБОТКИ (можно менять) ==========
    
    // Режим обработки:
    // 0 = от курсора и до конца
    // 1 = весь документ  
    // 2 = в выделенном фрагменте
    var PROCESS_MODE = 1;
    
    // Типы подзаголовков для обработки, (можно настроить):
    // 0 = все подзаголовки
    // 1 = только текстовые (с буквами/цифрами)
    // 2 = только символьные (без букв/цифр - звездочки и проч.)
    var PROCESS_TYPE = 0;
    
    // Добавлять пустые строки перед и после подзаголовка, (можно настроить)
    var ADD_EMPTY_LINES = 1; // 0 = не добавлять, 1 = добавлять
    // ======================================
    
    var versionNum = "2.9";
    
    try { 
        var nbspChar = window.external.GetNBSP(); 
        var nbspEntity; 
        if (nbspChar.charCodeAt(0) == 160) 
            nbspEntity = "&nbsp;"; 
        else 
            nbspEntity = nbspChar;
    }
    catch(e) { 
        nbspChar = String.fromCharCode(160); 
        nbspEntity = "&nbsp;";
    }
    
    var fbw_body = document.getElementById("fbw_body");
    if (!fbw_body) {
        MsgBox("Ошибка: не найден элемент fbw_body\nВозможно, документ не загружен");
        return;
    }
    
    // Функции для навигации по DOM
    function getNextNode(el) {
        if (el.firstChild && el.nodeName != "P")
            el = el.firstChild;
        else {
            while (el && !el.nextSibling)
                el = el.parentNode;
            if (el && el.nextSibling) el = el.nextSibling; 
        }
        return el;
    }
    
    function getNextP(el) {
        var savedEl = el;
        while (el && (el.nodeName != "P" || el == savedEl))
            el = getNextNode(el);
        return el;
    }
    
    // Функция для получения параграфов в выделенном фрагменте (УПРОЩЁННАЯ версия)
    function getParagraphsInSelection() {
        var selection = document.selection;
        if (!selection) {
            MsgBox("Ошибка: не удалось получить объект выделения");
            return null;
        }
        
        var tr = selection.createRange();
        if (!tr) {
            MsgBox("Ошибка: не удалось создать Range из выделения");
            return null;
        }
        
        // Проверяем, что выделение в тексте книги
        if (tr.parentElement().nodeName == "TEXTAREA" || tr.parentElement().nodeName == "INPUT") {
            MsgBox("Ошибка. Должно быть выделение в тексте книги, а не в поле ввода.");
            return null;
        }
        
        // Проверяем, есть ли выделение (как в скрипте "Удалить пустые строки в выделении")
        if (tr.compareEndPoints("StartToEnd", tr) == 0) {
            MsgBox("Нет выделения.\n\nПеред запуском скрипта нужно выделить абзацы, которые будут обработаны.");
            return null;
        }
        
        // Получаем элементы начала и конца выделения (как в примере скрипта)
        var trStart = selection.createRange();
        trStart.collapse(true); // Начало выделения
        var blockStartEl = trStart.parentElement();
        
        var trEnd = selection.createRange();
        trEnd.collapse(false); // Конец выделения
        var blockEndEl = trEnd.parentElement();
        
        // Проверяем, что оба элемента существуют и находятся в fbw_body
        if (!blockStartEl || !blockEndEl) {
            MsgBox("Ошибка: не удалось определить границы выделения");
            return null;
        }
        
        if (!fbw_body.contains(blockStartEl) || !fbw_body.contains(blockEndEl)) {
            MsgBox("Ошибка: выделение находится вне текста книги");
            return null;
        }
        
        // Находим параграфы для начала и конца
        var startParagraph = blockStartEl;
        while (startParagraph && startParagraph.nodeName != "P" && startParagraph.nodeName != "BODY")
            startParagraph = startParagraph.parentNode;
        
        var endParagraph = blockEndEl;
        while (endParagraph && endParagraph.nodeName != "P" && endParagraph.nodeName != "BODY")
            endParagraph = endParagraph.parentNode;
        
        if (!startParagraph || startParagraph.nodeName == "BODY" || 
            !endParagraph || endParagraph.nodeName == "BODY") {
            MsgBox("Ошибка: выделение должно начинаться и заканчиваться в абзацах (P)");
            return null;
        }
        
        // Собираем все параграфы между начальным и конечным
        var paragraphs = [];
        var ptr = startParagraph;
        var foundEnd = false;
        
        while (ptr && !foundEnd && fbw_body.contains(ptr)) {
            if (ptr.nodeName == "P") {
                paragraphs.push(ptr);
                if (ptr === endParagraph) {
                    foundEnd = true;
                }
            }
            
            if (!foundEnd) {
                ptr = getNextP(ptr);
            } else {
                break;
            }
        }
        
        // Если не нашли конечный параграф, возвращаем только начальный
        if (!foundEnd) {
            MsgBox("Ошибка: не удалось найти конечный параграф выделения");
            return [startParagraph];
        }
        
        return paragraphs;
    }
    
    // Функция для получения параграфов от курсора до конца
    function getParagraphsFromCursor() {
        var selection = document.selection;
        if (!selection) return null;
        
        var tr = selection.createRange();
        if (!tr) return null;
        
        if (tr.parentElement().nodeName == "TEXTAREA" || tr.parentElement().nodeName == "INPUT") {
            return null;
        }
        
        // Если есть выделение, используем его начало
        if (tr.compareEndPoints("StartToEnd", tr) != 0) {
            tr.collapse(true); // Используем начало выделения
        }
        
        // Получаем элемент курсора
        var cursorElement = tr.parentElement();
        if (!cursorElement) return null;
        
        // Находим параграф курсора
        var cursorParagraph = cursorElement;
        while (cursorParagraph && cursorParagraph.nodeName != "P" && cursorParagraph.nodeName != "BODY")
            cursorParagraph = cursorParagraph.parentNode;
        
        if (!cursorParagraph || cursorParagraph.nodeName == "BODY") {
            return null;
        }
        
        // Собираем параграфы от курсора до конца
        var paragraphs = [];
        var ptr = cursorParagraph;
        
        while (ptr && fbw_body.contains(ptr)) {
            if (ptr.nodeName == "P") {
                paragraphs.push(ptr);
            }
            ptr = getNextP(ptr);
        }
        
        return paragraphs;
    }
    
    // Функция проверки, находится ли элемент в основном разделе
    function isInMainSection(element) {
        var parent = element.parentNode;
        while (parent) {
            if (parent.nodeName == "DIV" && parent.className == "body") {
                var fbname = parent.getAttribute("fbname");
                if (!fbname || (fbname != "notes" && fbname != "comments")) {
                    return true;
                }
                return false;
            }
            parent = parent.parentNode;
        }
        return false;
    }
    
    // Функция проверки наличия текста
    function hasTextContent(element) {
        function checkNode(node) {
            if (node.nodeType == 3) {
                var text = node.nodeValue || "";
                for (var i = 0; i < text.length; i++) {
                    var ch = text.charCodeAt(i);
                    if ((ch >= 48 && ch <= 57) ||
                        (ch >= 65 && ch <= 90) ||
                        (ch >= 97 && ch <= 122) ||
                        (ch >= 1040 && ch <= 1103) ||
                        ch == 46 || ch == 44 || ch == 33 || ch == 63 ||
                        ch == 58 || ch == 59 || ch == 8212 ||
                        ch == 40 || ch == 41) {
                        return true;
                    }
                }
            } else if (node.nodeType == 1) {
                var children = node.childNodes;
                for (var j = 0; j < children.length; j++) {
                    if (checkNode(children[j])) {
                        return true;
                    }
                }
            }
            return false;
        }
        return checkNode(element);
    }
    
    // Определяем, какие параграфы проверять в зависимости от режима
    var paragraphsToCheck = [];
    var modeDescription = "";
    
    if (PROCESS_MODE == 0) {
        // Режим "от курсора до конца"
        modeDescription = "от курсора до конца";
        var cursorParagraphs = getParagraphsFromCursor();
        if (!cursorParagraphs || cursorParagraphs.length == 0) {
            MsgBox("Не удалось определить позицию курсора.\n\nУстановите курсор в нужное место в тексте книги\n(кликните мышкой в любом абзаце) и попробуйте снова.");
            return;
        }
        paragraphsToCheck = cursorParagraphs;
        
    } else if (PROCESS_MODE == 1) {
        // Режим "весь документ"
        modeDescription = "весь документ";
        var allParagraphs = fbw_body.getElementsByTagName('P');
        for (var i = 0; i < allParagraphs.length; i++) {
            paragraphsToCheck.push(allParagraphs[i]);
        }
        
    } else if (PROCESS_MODE == 2) {
        // Режим "в выделенном фрагменте"
        modeDescription = "в выделенном фрагменте";
        var selectionParagraphs = getParagraphsInSelection();
        if (!selectionParagraphs || selectionParagraphs.length == 0) {
            // Сообщение уже показано в getParagraphsInSelection()
            return;
        }
        paragraphsToCheck = selectionParagraphs;
    }
    
    // Сбор статистики и элементов
    var textSubtitles = [];
    var symbolSubtitles = [];
    
    for (i = 0; i < paragraphsToCheck.length; i++) {
        var p = paragraphsToCheck[i];
        if (p.className == 'subtitle') {
            if (isInMainSection(p)) {
                if (hasTextContent(p)) {
                    textSubtitles.push(p);
                } else {
                    symbolSubtitles.push(p);
                }
            }
        }
    }
    
    var totalSubtitles = textSubtitles.length + symbolSubtitles.length;
    
    if (totalSubtitles == 0) {
        MsgBox("Нет подзаголовков для обработки " + modeDescription + " в основном разделе");
        return;
    }
    
    // Определяем, какие подзаголовки будем обрабатывать
    var typeDescription = "ВСЕ ПОДЗАГОЛОВКИ";
    var processTypeText = "";
    if (PROCESS_TYPE == 1) {
        typeDescription = "ТОЛЬКО ТЕКСТОВЫЕ";
        processTypeText = " текстовых";
        if (textSubtitles.length == 0) {
            MsgBox("Текстовых подзаголовков не найдено в выбранной области");
            return;
        }
    } else if (PROCESS_TYPE == 2) {
        typeDescription = "ТОЛЬКО СИМВОЛЬНЫЕ";
        processTypeText = " символьных";
        if (symbolSubtitles.length == 0) {
            MsgBox("Символьных подзаголовков не найдено в выбранной области");
            return;
        }
    }
    
    var targetSubtitles = [];
    if (PROCESS_TYPE == 0) {
        targetSubtitles = textSubtitles.concat(symbolSubtitles);
    } else if (PROCESS_TYPE == 1) {
        targetSubtitles = textSubtitles;
    } else if (PROCESS_TYPE == 2) {
        targetSubtitles = symbolSubtitles;
    }
    
    var statsMsg = "\"Расформатировать подзаголовки\"\n";
    statsMsg += "ver. " + versionNum + "\n";
    statsMsg += "====================\n\n";
    statsMsg += "РЕЖИМ ОБРАБОТКИ: " + modeDescription + "\n";
    statsMsg += "НАЙДЕНО ПОДЗАГОЛОВКОВ:\n";
    statsMsg += "Всего: " + totalSubtitles + "\n";
    statsMsg += "Текстовых: " + textSubtitles.length + "\n";
    statsMsg += "Символьных: " + symbolSubtitles.length + "\n\n";
    statsMsg += "НАСТРОЙКИ:\n";
    statsMsg += "Тип: " + typeDescription + "\n";
    statsMsg += "Пустые строки: " + (ADD_EMPTY_LINES == 1 ? "добавлять" : "не добавлять") + "\n\n";
    statsMsg += "Обработать " + targetSubtitles.length + " подзаголовков?";
    
    if (!confirm(statsMsg)) {
        return;
    }
    
    // ТОЛЬКО ТЕПЕРЬ запускаем таймер!
    var Ts = new Date().getTime();
    
    // Обработка
    window.external.BeginUndoUnit(document, "Расформатировать подзаголовки (v" + versionNum + ")");
    
    var processedCount = 0;
    var emptyLinesAdded = 0;
    
    // Подготавливаем пустые строки заранее
    var emptyLinesCache = [];
    if (ADD_EMPTY_LINES == 1) {
        for (i = 0; i < targetSubtitles.length * 2; i++) {
            var emptyLine = document.createElement("P");
            emptyLine.innerHTML = nbspEntity;
            emptyLinesCache.push(emptyLine);
        }
    }
    
    // Обрабатываем в обратном порядке
    for (i = targetSubtitles.length - 1; i >= 0; i--) {
        var p = targetSubtitles[i];
        
        // Расформатируем подзаголовок
        p.className = "";
        p.style.fontWeight = "";
        p.style.fontStyle = "";
        p.style.textDecoration = "";
        
        if (ADD_EMPTY_LINES == 1) {
            var parent = p.parentNode;
            if (parent) {
                var emptyBefore = emptyLinesCache.pop();
                var emptyAfter = emptyLinesCache.pop();
                
                parent.insertBefore(emptyAfter, p.nextSibling);
                parent.insertBefore(emptyBefore, p);
                
                emptyLinesAdded += 2;
            }
        }
        
        processedCount++;
    }
    
    window.external.EndUndoUnit(document);
    
    // Выводим итоговую статистику
    var Tf = new Date().getTime();
    var Tsek = Math.ceil(100 * (Tf - Ts) / 1000) / 100;
    
    var resultMsg = "\"Расформатировать подзаголовки\"\n";
    resultMsg += "ver. " + versionNum + "\n";
    resultMsg += "====================\n\n";
    resultMsg += "ОБРАБОТКА ЗАВЕРШЕНА\n";
    resultMsg += "Режим: " + modeDescription + "\n\n";
    
    if (PROCESS_TYPE != 0) {
        resultMsg += "Расформатировано: " + processedCount + processTypeText + " подзаголовков\n";
    } else {
        resultMsg += "Расформатировано: " + processedCount + " подзаголовков\n";
    }
    
    if (ADD_EMPTY_LINES == 1) {
        resultMsg += "Добавлено пустых строк: " + emptyLinesAdded + "\n";
    }
    
    resultMsg += "\nВремя работы: " + Tsek + " сек.";
    
    MsgBox(resultMsg);
}
