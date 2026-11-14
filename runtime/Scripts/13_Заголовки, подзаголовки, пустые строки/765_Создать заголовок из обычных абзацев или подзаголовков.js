// Скрипт "Создать заголовок из обычных абзацев или подзаголовков" для редактора FBE
// version 2.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для создания заголовка (DIV class="title") 
// из одного или нескольких выделенных обычных абзацев или подзаголовков в fb2 документах.
// Из нескольких выделенных обычных абзацев и (или) подзаголовков создается один общий заголовок.
// Допускается полное или частичное выделение, а также просто курсор внутри абзаца.
// Заголовок помещается в новую секцию, создаваемую на месте первого выделенного абзаца.
// При создании заголовка сразу после существующего - вставляется пустая строка для валидности FB2.
// Защищённые блочные элементы (history, annotation, epigraph, title, poem, cite, table, image) не обрабатываются.

// Известный баг:
// При ПОЛНОМ ВЫДЕЛЕНИИ ПОСЛЕДНЕГО АБЗАЦА В СЕКЦИИ пустая строка после созданного заголовка не добавляется.
// Вследствие этого в данном случае получается невалидная конструкция.

// Решение: ИСПОЛЬЗОВАТЬ ЧАСТИЧНОЕ ВЫДЕЛЕНИЕ ПОСЛЕДНЕГО АБЗАЦА ИЛИ КУРСОР внутри такого последнего абзаца в секции.
// Или же можно воспользоваться скриптом "13_Почистить структуру.js." 
// для восстановления валидности структуры.

// version 2.1, 14.04.2026
// Исправлено: принудительная проверка наличия элементов после последнего выделенного абзаца
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Создать заголовок из обычных абзацев или подзаголовков";
    var version = "2.1";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1;
    
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
    // ПРОВЕРКА НА БЛОЧНУЮ КАРТИНКУ В ВЫДЕЛЕНИИ
    // ==================================================
    
    var sel = document.selection;
    if (sel && sel.type == "Control") {
        var controlRange = sel.createRange();
        if (controlRange && controlRange.length > 0) {
            var controlElement = controlRange.item(0);
            if (controlElement && controlElement.nodeName == "DIV" && controlElement.className == "image") {
                var invalidMsg = scriptName + "\nver. " + version + "\n\nДолжны быть выделены только обычные абзацы или подзаголовки.";
                MsgBox(invalidMsg, "FBE скрипт");
                return;
            }
        }
    }
    
    // ==================================================
    // ФУНКЦИИ ПРОВЕРКИ ЗАЩИЩЁННЫХ ЭЛЕМЕНТОВ (из скрипта 745)
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
    // ФУНКЦИИ НАВИГАЦИИ (из скрипта Sclex)
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
    // ФУНКЦИЯ ВСТАВКИ МАРКЕРОВ (из скрипта Sclex)
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
    // ФУНКЦИЯ СОЗДАНИЯ ПУСТОГО АБЗАЦА (из скрипта 695)
    // ==================================================
    
    function createEmptyParagraph() {
        var p = document.createElement("P");
        p.innerHTML = nbspChar;
        return p;
    }
    
    // ==================================================
    // ФУНКЦИЯ ПРОВЕРКИ ВАЛИДНОСТИ АБЗАЦА
    // ==================================================
    
    function isValidParagraph(p) {
        if (!p || p.nodeName != "P") return false;
        
        var className = p.className || "";
        if (className != "" && className != "subtitle") return false;
        
        if (isInHistory(p)) return false;
        if (isInFirstAnnotation(p)) return false;
        if (isInProtectedElement(p)) return false;
        if (isInsideBlockElement(p)) return false;
        
        return true;
    }
    
    // ==================================================
    // ФУНКЦИЯ ПРОВЕРКИ, ЧТО ВСЕ АБЗАЦЫ В ОДНОЙ СЕКЦИИ
    // ==================================================
    
    function areAllInSameSection(ps) {
        if (ps.length == 0) return false;
        
        var parentEl = ps[0].parentNode;
        if (!parentEl || parentEl.className != "section") return false;
        
        for (var i = 1; i < ps.length; i++) {
            if (ps[i].parentNode != parentEl) return false;
        }
        
        return true;
    }
    
    // ==================================================
    // ФУНКЦИЯ ПРОВЕРКИ, НАХОДИТСЯ ЛИ ЭЛЕМЕНТ СРАЗУ ПОСЛЕ ЗАГОЛОВКА
    // ==================================================
    
    function isImmediatelyAfterTitle(element) {
        var prev = element.previousSibling;
        while (prev && prev.nodeType != 1) {
            prev = prev.previousSibling;
        }
        
        if (prev && prev.nodeName == "DIV" && prev.className == "title") {
            return true;
        }
        
        return false;
    }
    
    // ==================================================
    // ОСНОВНАЯ ЛОГИКА (из версии 2.0 + проверка на последний элемент)
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
        
        // Проверка на картинку между начальным и конечным абзацами
        var tempNode = blockStartEl;
        while (tempNode && tempNode != blockEndEl) {
            if (tempNode.nodeName == "DIV" && tempNode.className == "image") {
                var invalidMsg = scriptName + "\nver. " + version + "\n\nДолжны быть выделены только обычные абзацы или подзаголовки.";
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
                var invalidMsg = scriptName + "\nver. " + version + "\n\nДолжны быть выделены только обычные абзацы или подзаголовки.";
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
        
        if (!areAllInSameSection(ps)) {
            throw new Error("Выделенные абзацы должны находиться в одной секции");
        }
        
        var parentSection = ps[0].parentNode;
        var firstP = ps[0];
        var lastP = ps[ps.length - 1];
        
        var needEmptyLineBefore = isImmediatelyAfterTitle(firstP);
        
        // Проверяем, есть ли в секции элементы ПОСЛЕ последнего выделенного абзаца
        var hasElementsAfter = false;
        var foundLast = false;
        for (var i = 0; i < parentSection.childNodes.length; i++) {
            var child = parentSection.childNodes[i];
            if (child == lastP) {
                foundLast = true;
                continue;
            }
            if (foundLast && child.nodeType == 1) {
                hasElementsAfter = true;
                break;
            }
        }
        
        var afterContent = [];
        var foundLastP = false;
        
        for (var i = 0; i < parentSection.childNodes.length; i++) {
            var child = parentSection.childNodes[i];
            if (child == lastP) {
                foundLastP = true;
                continue;
            }
            if (foundLastP && child.nodeType == 1) {
                afterContent.push(child.cloneNode(true));
            }
        }
        
        var newSection = document.createElement("DIV");
        newSection.className = "section";
        
        var newTitle = document.createElement("DIV");
        newTitle.className = "title";
        
        for (var i = 0; i < ps.length; i++) {
            var pCopy = ps[i].cloneNode(true);
            pCopy.className = "";
            newTitle.appendChild(pCopy);
        }
        
        newSection.appendChild(newTitle);
        
        for (var i = 0; i < afterContent.length; i++) {
            newSection.appendChild(afterContent[i]);
        }
        
        // Если элементов после выделения нет — гарантированно добавляем пустую строку
        if (!hasElementsAfter) {
            var emptyP = createEmptyParagraph();
            newSection.appendChild(emptyP);
        }
        
        var nodesToRemove = [];
        foundLastP = false;
        
        for (var i = 0; i < parentSection.childNodes.length; i++) {
            var child = parentSection.childNodes[i];
            if (child == firstP) {
                foundLastP = true;
            }
            if (foundLastP && child.nodeType == 1) {
                nodesToRemove.push(child);
            }
        }
        
        for (var i = 0; i < nodesToRemove.length; i++) {
            nodesToRemove[i].removeNode(true);
        }
        
        if (needEmptyLineBefore) {
            var emptyPBefore = createEmptyParagraph();
            parentSection.appendChild(emptyPBefore);
        }
        
        parentSection.parentNode.insertBefore(newSection, parentSection.nextSibling);
        
        var sn = document.getElementById(startId);
        var en = document.getElementById(endId);
        if (sn) sn.removeNode(true);
        if (en) en.removeNode(true);
        
        var endTime = new Date().getTime();
        var timeDiff = (endTime - startTime) / 1000;
        var timeStr = timeDiff.toFixed(3).replace(".", ",");
        
        if (showStatistics == 1) {
            var msg = scriptName + "\nver. " + version + "\n\n";
            msg += "✓ Заголовок успешно создан\n";
            msg += "  • Кол-во абзацев: " + ps.length + "\n";
            if (needEmptyLineBefore) {
                msg += "  • Вставлена пустая строка перед заголовком\n";
            }
            if (!hasElementsAfter) {
                msg += "  • Вставлена пустая строка после заголовка\n";
            }
            msg += "\nВремя выполнения: " + timeStr + " сек";
            
            MsgBox(msg, "FBE скрипт");
        }
        
        try {
            window.external.SetStatusBarText(scriptName + " ver. " + version + ": создан заголовок из " + ps.length + " абзацев. Время: " + timeStr + " сек.");
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
