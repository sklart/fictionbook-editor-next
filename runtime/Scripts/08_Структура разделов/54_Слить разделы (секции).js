// Скрипт "Слить разделы (секции)" для редактора FBE
// version 1.6
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для слияния соседних одноуровневых разделов (секций) в fb2 документах.
// Аналог штатной функции "Слить разделы", расположенной внизу окна структуры документа.
// Курсор должен быть установлен в вышестоящем разделе (секции).
// Скрипт заменяет в нижележащем разделе DIV class="title", на <P class=subtitle> и производит объединение разделов в один.
// Сохраняется валидная структура разделов (секций) при слиянии.
// Эпиграф(ы) соответствующего раздела расформатируются вместе с заголовком согласно настроек.

// Опциональные настройки:
// Множественные расформатированные эпиграфы могут отделяться друг от друга пустыми строками.
// Расформатированный эпиграф может отделяться от последующего абзаца пустой строкой.
// Расформатированный эпиграф может быть выделен жирностью или курсивом.
// Расформатированный автор текста эпиграфа может быть выделен жирностью, курсивом или жирностью+курсивом.

// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.6, 26.04.2026
//======================================

function Run() {
    var scriptName = "Слить разделы (секции)";
    var version = "1.6";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 0;
    
    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0; // 0 - нет, 1 - да
    
    // Обрабатывать раздел комментариев
    var processCommentsSection = 0; // 0 - нет, 1 - да
    
    // Добавлять пустые строки между расформатированными несколькими эпиграфами
    // 0 - не добавлять, 1 - добавлять
    var addEmptyLinesBetweenEpigraphs = 1;
    
    // Добавлять пустую строку между расформатированным эпиграфом и нижеследующим обычным абзацем
    // 0 - не добавлять, 1 - добавлять
    var addEmptyLinesAfterEpigraphs = 1;
    
    // Форматировать эпиграфы курсивом или жирностью при расформатировании их в абзацы
    // 0 - оставить как есть, 1 - форматировать курсивом, 2 - форматировать жирностью
    var formatEpigraphs = 1;
    
    // Форматировать автора текста исходного эпиграфа курсивом или жирностью при расформатировании их в абзацы
    // 0 - оставить как есть, 1 - форматировать курсивом, 2 - форматировать жирностью, 3 - форматировать и курсивом и жирностью
    var formatTextAuthor = 3;
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    var startTime = new Date();
    
    try {
        var nbspChar = window.external.GetNBSP();
        var nbspEntity;
        if (nbspChar.charCodeAt(0) == 160) {
            nbspEntity = "&nbsp;";
        } else {
            nbspEntity = nbspChar;
        }
    } catch(e) {
        var nbspChar = String.fromCharCode(160);
        var nbspEntity = "&nbsp;";
    }
    
    var fbw_body = document.getElementById("fbw_body");
    if (!fbw_body) {
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка: не найден fbw_body", "FBE скрипт");
        return;
    }
    
    if (document.selection.type.toLowerCase() != "none" && document.selection.type.toLowerCase() != "text") {
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка: неподдерживаемый тип выделения", "FBE скрипт");
        return;
    }
    
    // Проверяем, что не в режиме XML
    if (window.external && window.external.IsXML && window.external.IsXML()) {
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка: скрипт работает только в HTML-режиме", "FBE скрипт");
        return;
    }
    
    // Генерируем уникальный ID для временного маркера
    var randomNum = Math.floor((Math.random()*9)).toString() + 
                    Math.floor((Math.random()*9)).toString() +
                    Math.floor((Math.random()*9)).toString() + 
                    Math.floor((Math.random()*9)).toString() +
                    Math.floor((Math.random()*9)).toString() + 
                    Math.floor((Math.random()*9)).toString();
    var selectionBeginId = "mergeSectionBeginId_" + randomNum;
    
    var tr = document.selection.createRange();
    tr.collapse(true);
    
    window.external.BeginUndoUnit(document, "слить разделы (v" + version + ")");
    
    // Вставляем временный маркер для определения позиции курсора
    tr.pasteHTML("<B id=" + selectionBeginId + "></B>");
    var el = document.getElementById(selectionBeginId);
    
    if (!fbw_body.contains(el)) {
        var marker = document.getElementById(selectionBeginId);
        if (marker) {
            marker.removeNode(true);
        }
        window.external.EndUndoUnit(document);
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка: курсор не в теле документа", "FBE скрипт");
        return;
    }
    
    // Ищем секцию, в которой находится курсор
    var currentSection = null;
    var ptr = el;
    while (ptr && ptr.nodeName != "BODY") {
        if (ptr.nodeName == "DIV" && ptr.className == "section") {
            currentSection = ptr;
            break;
        }
        ptr = ptr.parentNode;
    }
    
    if (!currentSection) {
        var marker = document.getElementById(selectionBeginId);
        if (marker) {
            marker.removeNode(true);
        }
        window.external.EndUndoUnit(document);
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка: курсор не внутри секции (раздела)", "FBE скрипт");
        return;
    }
    
    // Проверяем, что курсор в основном разделе (или в разрешенных notes/comments)
    var bodyDiv = currentSection.parentNode;
    while (bodyDiv && bodyDiv.nodeName != "BODY" && !(bodyDiv.nodeName == "DIV" && bodyDiv.className == "body")) {
        bodyDiv = bodyDiv.parentNode;
    }
    
    var fbname = "";
    if (bodyDiv && bodyDiv.nodeName == "DIV" && bodyDiv.className == "body") {
        fbname = bodyDiv.getAttribute("fbname") || "";
    }
    
    if (fbname == "notes" && processNotesSection == 0) {
        var marker = document.getElementById(selectionBeginId);
        if (marker) {
            marker.removeNode(true);
        }
        window.external.EndUndoUnit(document);
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка: обработка сносок отключена в настройках", "FBE скрипт");
        return;
    }
    
    if (fbname == "comments" && processCommentsSection == 0) {
        var marker = document.getElementById(selectionBeginId);
        if (marker) {
            marker.removeNode(true);
        }
        window.external.EndUndoUnit(document);
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка: обработка комментариев отключена в настройках", "FBE скрипт");
        return;
    }
    
    // Находим следующую секцию (нижерасположенную)
    var nextSection = currentSection.nextSibling;
    while (nextSection && nextSection.nodeType != 1) {
        nextSection = nextSection.nextSibling;
    }
    
    if (!nextSection || nextSection.nodeName != "DIV" || nextSection.className != "section") {
        var marker = document.getElementById(selectionBeginId);
        if (marker) {
            marker.removeNode(true);
        }
        window.external.EndUndoUnit(document);
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка: ниже текущей секции нет другой секции для слияния", "FBE скрипт");
        return;
    }
    
    // Проверяем, что секции одноуровневые
    var currentParent = currentSection.parentNode;
    var nextParent = nextSection.parentNode;
    
    if (currentParent !== nextParent) {
        var marker = document.getElementById(selectionBeginId);
        if (marker) {
            marker.removeNode(true);
        }
        window.external.EndUndoUnit(document);
        MsgBox(scriptName + "\nver. " + version + "\n\nОбъединять можно только одноуровневые секции.", "FBE скрипт");
        return;
    }
    
    // Проверяем, что в нижележащей секции нет вложенных секций
    var nestedSections = nextSection.getElementsByTagName("DIV");
    var hasNestedSections = false;
    for (var ns = 0; ns < nestedSections.length; ns++) {
        if (nestedSections[ns].className == "section" && nestedSections[ns] !== nextSection) {
            hasNestedSections = true;
            break;
        }
    }
    
    if (hasNestedSections) {
        var marker = document.getElementById(selectionBeginId);
        if (marker) {
            marker.removeNode(true);
        }
        window.external.EndUndoUnit(document);
        MsgBox(scriptName + "\nver. " + version + "\n\nОбъединять можно только одноуровневые секции.\nВ нижележащей секции есть вложенные секции.", "FBE скрипт");
        return;
    }
    
    // Удаляем временный маркер
    var marker = document.getElementById(selectionBeginId);
    if (marker) {
        marker.removeNode(true);
    }
    
    // Статистика
    var titleCount = 0;
    var epigraphCount = 0;
    var textAuthorCount = 0;
    
    // ==================================================
    // ШАГ 1: Обрабатываем заголовок следующей секции
    // ==================================================
    var nextFirstChild = nextSection.firstChild;
    while (nextFirstChild && nextFirstChild.nodeType != 1) {
        nextFirstChild = nextFirstChild.nextSibling;
    }
    
    if (nextFirstChild && nextFirstChild.nodeName == "DIV" && nextFirstChild.className == "title") {
        var titleDiv = nextFirstChild;
        var titleChildren = titleDiv.childNodes;
        var titlePElements = [];
        
        // Собираем все P элементы из title
        for (var i = 0; i < titleChildren.length; i++) {
            if (titleChildren[i].nodeType == 1 && titleChildren[i].nodeName == "P") {
                titlePElements.push(titleChildren[i]);
            }
        }
        
        // Превращаем каждый P в подзаголовок и вставляем перед title div
        for (var j = titlePElements.length - 1; j >= 0; j--) {
            var pElement = titlePElements[j];
            pElement.className = "subtitle";
            titleDiv.removeChild(pElement);
            titleDiv.parentNode.insertBefore(pElement, titleDiv);
            titleCount++;
        }
        
        // Удаляем пустой title div
        titleDiv.removeNode(false);
    }
    
    // ==================================================
    // ШАГ 2: Обрабатываем эпиграфы и сразу отслеживаем границы
    // ==================================================
    // ВАЖНО: сохраняем границы каждого бывшего эпиграфа
    // Массив объектов: { elements: [P, P, ...], nextSiblingAfterEpigraph: элемент }
    var epigraphData = [];
    
    var child = nextSection.firstChild;
    while (child) {
        if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "epigraph") {
            var epigraphDiv = child;
            var nextSiblingAfterEpigraph = child.nextSibling;
            var nextChild = child.nextSibling;
            
            // Собираем содержимое эпиграфа
            var epigraphChildren = epigraphDiv.childNodes;
            var bodyPElements = [];
            var textAuthorPElements = [];
            
            for (var k = 0; k < epigraphChildren.length; k++) {
                if (epigraphChildren[k].nodeType == 1 && epigraphChildren[k].nodeName == "P") {
                    var element = epigraphChildren[k];
                    
                    if (element.className == "text-author") {
                        element.removeAttribute("class");
                        element.removeAttribute("className");
                        textAuthorPElements.push(element);
                    } else {
                        element.removeAttribute("class");
                        element.removeAttribute("className");
                        bodyPElements.push(element);
                    }
                }
            }
            
            var allElements = [];
            
            // Обрабатываем тело эпиграфа
            for (var b = 0; b < bodyPElements.length; b++) {
                var bodyP = bodyPElements[b];
                
                if (formatEpigraphs == 1) {
                    var emElement = document.createElement("EM");
                    var content = bodyP.innerHTML;
                    bodyP.innerHTML = "";
                    emElement.innerHTML = content;
                    bodyP.appendChild(emElement);
                } else if (formatEpigraphs == 2) {
                    var strongElement = document.createElement("STRONG");
                    var content = bodyP.innerHTML;
                    bodyP.innerHTML = "";
                    strongElement.innerHTML = content;
                    bodyP.appendChild(strongElement);
                }
                
                epigraphDiv.parentNode.insertBefore(bodyP, epigraphDiv);
                allElements.push(bodyP);
                epigraphCount++;
            }
            
            // Обрабатываем авторов текста
            for (var a = 0; a < textAuthorPElements.length; a++) {
                var authorP = textAuthorPElements[a];
                
                if (formatTextAuthor == 1) {
                    var emElement = document.createElement("EM");
                    var content = authorP.innerHTML;
                    authorP.innerHTML = "";
                    emElement.innerHTML = content;
                    authorP.appendChild(emElement);
                    textAuthorCount++;
                } else if (formatTextAuthor == 2) {
                    var strongElement = document.createElement("STRONG");
                    var content = authorP.innerHTML;
                    authorP.innerHTML = "";
                    strongElement.innerHTML = content;
                    authorP.appendChild(strongElement);
                    textAuthorCount++;
                } else if (formatTextAuthor == 3) {
                    var strongElement = document.createElement("STRONG");
                    var emElement = document.createElement("EM");
                    var content = authorP.innerHTML;
                    authorP.innerHTML = "";
                    emElement.innerHTML = content;
                    strongElement.appendChild(emElement);
                    authorP.appendChild(strongElement);
                    textAuthorCount++;
                }
                
                epigraphDiv.parentNode.insertBefore(authorP, epigraphDiv);
                allElements.push(authorP);
            }
            
            // Сохраняем данные об этом эпиграфе
            epigraphData.push({
                elements: allElements,
                nextSibling: nextSiblingAfterEpigraph
            });
            
            child = nextChild;
        } else {
            child = child.nextSibling;
        }
    }
    
    // Удаляем все DIV epigraph (после того как все P вставлены)
    var allEpigraphDivs = [];
    var scanChild = nextSection.firstChild;
    while (scanChild) {
        if (scanChild.nodeType == 1 && scanChild.nodeName == "DIV" && scanChild.className == "epigraph") {
            allEpigraphDivs.push(scanChild);
        }
        scanChild = scanChild.nextSibling;
    }
    for (var r = 0; r < allEpigraphDivs.length; r++) {
        allEpigraphDivs[r].removeNode(false);
    }
    
    // ==================================================
    // ШАГ 3: Вставляем ПС МЕЖДУ разными эпиграфами
    // ==================================================
    if (addEmptyLinesBetweenEpigraphs == 1 && epigraphData.length > 1) {
        // Между каждыми соседними эпиграфами вставляем ПС
        // ПС вставляется ПЕРЕД первым элементом следующего эпиграфа
        for (var ed = 1; ed < epigraphData.length; ed++) {
            var prevEpigraph = epigraphData[ed - 1];
            var currentEpigraph = epigraphData[ed];
            
            // Находим первый элемент текущего эпиграфа
            var firstElementOfCurrent = currentEpigraph.elements[0];
            
            // Проверяем, нет ли уже ПС перед ним
            var prevSibling = firstElementOfCurrent.previousSibling;
            var needEmptyLine = true;
            
            if (prevSibling && prevSibling.nodeType == 1 && prevSibling.nodeName == "P") {
                var html = prevSibling.innerHTML;
                if (html == nbspEntity || html == "&nbsp;" || html == String.fromCharCode(160)) {
                    needEmptyLine = false;
                }
            }
            
            if (needEmptyLine) {
                var emptyP = document.createElement("P");
                emptyP.innerHTML = nbspEntity;
                nextSection.insertBefore(emptyP, firstElementOfCurrent);
            }
        }
    }
    
    // ==================================================
    // ШАГ 4: Вставляем ПС ПОСЛЕ последнего эпиграфа
    // ==================================================
    if (addEmptyLinesAfterEpigraphs == 1 && epigraphData.length > 0) {
        var lastEpigraph = epigraphData[epigraphData.length - 1];
        var lastElementOfLastEpigraph = lastEpigraph.elements[lastEpigraph.elements.length - 1];
        var nextElemAfterBlock = lastEpigraph.nextSibling;
        
        // Пропускаем текстовые узлы
        while (nextElemAfterBlock && nextElemAfterBlock.nodeType != 1) {
            nextElemAfterBlock = nextElemAfterBlock.nextSibling;
        }
        
        if (nextElemAfterBlock) {
            // Проверяем, не является ли следующий элемент уже ПС (из предыдущей вставки)
            var alreadyEmpty = false;
            if (nextElemAfterBlock.nodeType == 1 && nextElemAfterBlock.nodeName == "P") {
                var html = nextElemAfterBlock.innerHTML;
                if (html == nbspEntity || html == "&nbsp;" || html == String.fromCharCode(160)) {
                    alreadyEmpty = true;
                }
            }
            
            if (!alreadyEmpty) {
                var emptyP = document.createElement("P");
                emptyP.innerHTML = nbspEntity;
                nextSection.insertBefore(emptyP, nextElemAfterBlock);
            }
        }
    }
    
    // ==================================================
    // ШАГ 5: Переносим содержимое и удаляем пустую секцию
    // ==================================================
    while (nextSection.firstChild) {
        var childToMove = nextSection.firstChild;
        nextSection.removeChild(childToMove);
        currentSection.appendChild(childToMove);
    }
    
    nextSection.removeNode(false);
    
    window.external.EndUndoUnit(document);
    
    // Вычисляем время выполнения
    var endTime = new Date();
    var executionTime = (endTime - startTime) / 1000;
    var timeStr = executionTime.toFixed(3);
    timeStr = timeStr.replace(".", ",");
    
    // Выводим статистику
    if (showStatistics == 1) {
        var statsMsg = scriptName + "\nver. " + version + "\n\n";
        statsMsg += "✓ Секции успешно объединены\n\n";
        statsMsg += "  • Преобразовано заголовков в подзаголовки: " + titleCount + "\n";
        statsMsg += "  • Расформатировано эпиграфов: " + epigraphCount + "\n";
        statsMsg += "  • Обработано авторов текста: " + textAuthorCount + "\n\n";
        statsMsg += "Время выполнения: " + timeStr + " сек.";
        
        MsgBox(statsMsg, "FBE скрипт");
    }
}
