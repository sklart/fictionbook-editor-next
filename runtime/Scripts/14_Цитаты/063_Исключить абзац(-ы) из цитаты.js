// Скрипт "Исключить абзац(-ы) из цитаты" для редактора FBE
// version 2.0
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для исключения одного или нескольких абзацев из цитаты
// (DIV class="cite") в fb2 документах.
// Для работы скрипта курсор или выделение (частичное и полное) должны быть строго внутри одной цитаты.
// Учитывается наличие text-author в цитате по трём правилам: выделен только text-author, 
// выделен абзац перед text-author, выделены абзацы до text-author.
// При выделении text-author вместе с другими абзацами, все они становятся обычными.
// Сохраняются subtitle (при наличии) у вынесенных из цитаты абзацев.
// Скрипт проверяет наличие вложенных стихов (poem) и не обрабатывает такие цитаты.
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 2.0, 13.02.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Исключить абзац(-ы) из цитаты";
    var version = "2.0";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Работаем сразу в тихом режиме (окна только при ошибках)
    var showStatistics = 0; // 0 - тихий режим
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    // Проверяем, что находимся в теле документа
    var fbw_body = document.getElementById("fbw_body");
    if (!fbw_body) {
        if (showStatistics) MsgBox("Ошибка: не найден элемент fbw_body", "FBE скрипт\n" + scriptName + "\nver. " + version);
        return;
    }
    
    // Получаем текущее выделение
    var range = document.selection.createRange();
    if (!range) {
        if (showStatistics) MsgBox("Ошибка: не удалось получить выделение", "FBE скрипт\n" + scriptName + "\nver. " + version);
        return;
    }
    
    // Создаем уникальные ID для временных маркеров
    var randomNum = Math.floor(Math.random() * 900000 + 100000).toString();
    var markerStartId = "ExcludeCiteStart_" + randomNum;
    var markerEndId = "ExcludeCiteEnd_" + randomNum;
    
    // Начинаем группу отмены действий
    window.external.BeginUndoUnit(document, scriptName + " v" + version);
    
    // Вставляем маркер начала
    var rangeCopy = range.duplicate();
    rangeCopy.collapse(true);
    rangeCopy.pasteHTML("<B id='" + markerStartId + "'></B>");
    
    // Вставляем маркер конца
    rangeCopy = range.duplicate();
    rangeCopy.collapse(false);
    rangeCopy.pasteHTML("<B id='" + markerEndId + "'></B>");
    
    // Получаем элементы-маркеры
    var markerStart = document.getElementById(markerStartId);
    var markerEnd = document.getElementById(markerEndId);
    
    if (!markerStart || !markerEnd) {
        // Ошибка - удаляем маркеры, если они есть
        if (markerStart) markerStart.removeNode(true);
        if (markerEnd) markerEnd.removeNode(true);
        window.external.EndUndoUnit(document);
        if (showStatistics) MsgBox("Ошибка: не удалось создать маркеры выделения", "FBE скрипт\n" + scriptName + "\nver. " + version);
        return;
    }
    
    // ==================================================
    // ПРОВЕРКА ГРАНИЦ ЦИТАТЫ
    // ==================================================
    
    // Функция для поиска цитаты, содержащей элемент
    function findContainingCite(element) {
        while (element && element.nodeName != "BODY" && !(element.nodeName == "DIV" && element.className == "cite")) {
            element = element.parentNode;
        }
        return (element && element.nodeName == "DIV" && element.className == "cite") ? element : null;
    }
    
    // Проверяем, где находятся начало и конец исходного выделения
    var testRange = range.duplicate();
    
    // Проверяем начало выделения
    testRange.collapse(true);
    var startParent = testRange.parentElement();
    var startCite = findContainingCite(startParent);
    
    // Проверяем конец выделения
    testRange = range.duplicate();
    testRange.collapse(false);
    var endParent = testRange.parentElement();
    var endCite = findContainingCite(endParent);
    
    // Случай 1: начало вне цитаты, конец внутри
    if (!startCite && endCite) {
        markerStart.removeNode(true);
        markerEnd.removeNode(true);
        window.external.EndUndoUnit(document);
        MsgBox("Ошибка: выделение начинается ДО цитаты и заходит в неё.\n\nКурсор или выделение должны находиться строго внутри одной цитаты!", "FBE скрипт\n" + scriptName + "\nver. " + version);
        return;
    }
    
    // Случай 2: начало внутри цитаты, конец вне
    if (startCite && !endCite) {
        markerStart.removeNode(true);
        markerEnd.removeNode(true);
        window.external.EndUndoUnit(document);
        MsgBox("Ошибка: выделение начинается внутри цитаты и выходит за её пределы.\n\nКурсор или выделение должны находиться строго внутри одной цитаты!", "FBE скрипт\n" + scriptName + "\nver. " + version);
        return;
    }
    
    // Случай 3: обе точки в разных цитатах
    if (startCite && endCite && startCite != endCite) {
        markerStart.removeNode(true);
        markerEnd.removeNode(true);
        window.external.EndUndoUnit(document);
        MsgBox("Ошибка: выделение захватывает две разные цитаты.\n\nКурсор или выделение должны находиться строго внутри одной цитаты!", "FBE скрипт\n" + scriptName + "\nver. " + version);
        return;
    }
    
    // Случай 4: обе точки вне цитат
    if (!startCite && !endCite) {
        markerStart.removeNode(true);
        markerEnd.removeNode(true);
        window.external.EndUndoUnit(document);
        MsgBox("Ошибка: выделение находится вне цитаты.\n\nКурсор или выделение должны находиться строго внутри одной цитаты!", "FBE скрипт\n" + scriptName + "\nver. " + version);
        return;
    }
    
    // Если дошли сюда, значит обе точки в одной цитате
    var citeElement = startCite; // или endCite, они одинаковые
    
    // ==================================================
    // ПРОВЕРКА НА НАЛИЧИЕ ВЛОЖЕННЫХ СТИХОВ (poem)
    // ==================================================
    
    // Ищем внутри цитаты элементы DIV с классом poem
    var poems = citeElement.getElementsByTagName("DIV");
    var hasPoem = false;
    for (var i = 0; i < poems.length; i++) {
        if (poems[i].className == "poem") {
            hasPoem = true;
            break;
        }
    }
    
    if (hasPoem) {
        markerStart.removeNode(true);
        markerEnd.removeNode(true);
        window.external.EndUndoUnit(document);
        MsgBox("Цитата содержит вложенные стихи (poem).\n\nСначала расформатируйте стихи отдельным скриптом, затем повторите операцию.", "FBE скрипт\n" + scriptName + "\nver. " + version);
        return;
    }
    
    // ==================================================
    // СБОР ВСЕХ АБЗАЦЕВ ЦИТАТЫ
    // ==================================================
    
    // Собираем все дочерние абзацы цитаты в массив
    var allParagraphs = [];
    var child = citeElement.firstChild;
    
    while (child) {
        if (child.nodeName == "P") {
            allParagraphs.push(child);
        }
        child = child.nextSibling;
    }
    
    if (allParagraphs.length == 0) {
        markerStart.removeNode(true);
        markerEnd.removeNode(true);
        window.external.EndUndoUnit(document);
        if (showStatistics) MsgBox("Ошибка: в цитате нет абзацев", "FBE скрипт\n" + scriptName + "\nver. " + version);
        return;
    }
    
    // ==================================================
    // ОПРЕДЕЛЕНИЕ АБЗАЦЕВ С УЧЕТОМ ПОЛНОГО ВЫДЕЛЕНИЯ
    // ==================================================
    
    // Функция для поиска абзаца, содержащего элемент
    function findContainingParagraph(element) {
        while (element && element.nodeName != "P" && element.nodeName != "BODY") {
            element = element.parentNode;
        }
        return (element && element.nodeName == "P") ? element : null;
    }
    
    // Находим абзац для START маркера
    var startParagraph = findContainingParagraph(markerStart);
    if (!startParagraph) {
        markerStart.removeNode(true);
        markerEnd.removeNode(true);
        window.external.EndUndoUnit(document);
        if (showStatistics) MsgBox("Ошибка: не удалось определить начальный абзац", "FBE скрипт\n" + scriptName + "\nver. " + version);
        return;
    }
    
    // Находим индекс START абзаца
    var startIndex = -1;
    for (i = 0; i < allParagraphs.length; i++) {
        if (allParagraphs[i] == startParagraph) {
            startIndex = i;
            break;
        }
    }
    
    if (startIndex == -1) {
        markerStart.removeNode(true);
        markerEnd.removeNode(true);
        window.external.EndUndoUnit(document);
        if (showStatistics) MsgBox("Ошибка: не удалось определить позицию начального абзаца", "FBE скрипт\n" + scriptName + "\nver. " + version);
        return;
    }
    
    // Проверяем, есть ли выделение
    var selectionIsNone = (range.compareEndPoints("StartToEnd", range) == 0);
    
    // ОПРЕДЕЛЯЕМ КОНЕЧНЫЙ АБЗАЦ
    var endIndex = startIndex; // По умолчанию - тот же, что и начало
    
    if (!selectionIsNone) {
        // Пытаемся найти абзац для END маркера стандартным способом
        var endParagraph = findContainingParagraph(markerEnd);
        
        if (endParagraph) {
            // Если END маркер внутри абзаца - находим его индекс
            for (i = 0; i < allParagraphs.length; i++) {
                if (allParagraphs[i] == endParagraph) {
                    endIndex = i;
                    break;
                }
            }
        } else {
            // ОСОБЫЙ СЛУЧАЙ: END маркер вне абзаца (полное выделение)
            // Нужно определить, после какого абзаца он находится
            
            // Получаем позицию END маркера в DOM
            var endMarkerPos = markerEnd;
            
            // Ищем предыдущий абзац перед маркером
            var prev = endMarkerPos.previousSibling;
            while (prev && prev.nodeName != "P") {
                if (prev.nodeType == 1 && prev.nodeName == "B" && prev.id == markerEndId) {
                    // Это сам маркер, идем дальше
                    prev = prev.previousSibling;
                } else if (prev.lastChild) {
                    // Заглядываем вглубь последнего ребенка
                    var deep = prev.lastChild;
                    while (deep && deep.nodeType == 1 && deep.lastChild) {
                        deep = deep.lastChild;
                    }
                    if (deep && deep.nodeName == "P") {
                        prev = deep;
                        break;
                    } else {
                        prev = prev.previousSibling;
                    }
                } else {
                    prev = prev.previousSibling;
                }
            }
            
            // Также проверяем, может быть маркер внутри какого-то элемента, который внутри абзаца
            if (!prev || prev.nodeName != "P") {
                // Поднимаемся от маркера вверх до первого абзаца
                var temp = markerEnd.parentNode;
                while (temp && temp.nodeName != "P" && temp.nodeName != "BODY" && temp != citeElement) {
                    temp = temp.parentNode;
                }
                if (temp && temp.nodeName == "P") {
                    prev = temp;
                }
            }
            
            if (prev && prev.nodeName == "P") {
                // Нашли абзац перед маркером
                for (i = 0; i < allParagraphs.length; i++) {
                    if (allParagraphs[i] == prev) {
                        endIndex = i;
                        break;
                    }
                }
            } else {
                // Если ничего не нашли, возможно выделение захватило до конца цитаты
                endIndex = allParagraphs.length - 1;
            }
        }
    }
    
    // Проверяем корректность индексов
    if (endIndex < startIndex) {
        var temp = startIndex;
        startIndex = endIndex;
        endIndex = temp;
    }
    
    // Убеждаемся, что endIndex в пределах массива
    if (endIndex >= allParagraphs.length) {
        endIndex = allParagraphs.length - 1;
    }
    
    // ==================================================
    // АНАЛИЗ ПОЗИЦИИ TEXT-AUTHOR
    // ==================================================
    
    // Ищем text-author в цитате
    var textAuthorIndex = -1;
    for (i = 0; i < allParagraphs.length; i++) {
        if (allParagraphs[i].className == "text-author") {
            textAuthorIndex = i;
            break;
        }
    }
    
    // Флаг: выделен ли text-author
    var isTextAuthorSelected = (textAuthorIndex >= startIndex && textAuthorIndex <= endIndex);
    
    // Флаг: выделена ли вся цитата целиком
    var isFullCiteSelected = (startIndex == 0 && endIndex == allParagraphs.length - 1);
    
    // ==================================================
    // ПРИМЕНЕНИЕ ЛОГИКИ С TEXT-AUTHOR
    // ==================================================
    
    // Функция для полного расформатирования text-author (удаление класса)
    function unformatTextAuthor(p) {
        p.removeAttribute("class");
        p.removeAttribute("className");
    }
    
    // ОСОБЫЙ СЛУЧАЙ: выделена вся цитата целиком (включая text-author)
    if (isFullCiteSelected && textAuthorIndex != -1) {
        if (showStatistics && showStatistics == 1) {
            MsgBox("Выделена вся цитата целиком, text-author расформатируется", "FBE скрипт\n" + scriptName + "\nver. " + version);
        }
        
        // Выносим все абзацы за пределы цитаты (сохраняя порядок)
        var parentNode = citeElement.parentNode;
        for (i = 0; i < allParagraphs.length; i++) {
            var p = allParagraphs[i];
            
            // Удаляем класс только для text-author
            if (p.className == "text-author") {
                unformatTextAuthor(p);
            }
            // Для остальных (включая subtitle) класс сохраняется автоматически
            
            parentNode.insertBefore(p, citeElement);
        }
        
        // Удаляем пустую цитату
        citeElement.removeNode(true);
    }
    
    // Правило А: выделен ТОЛЬКО text-author
    else if (textAuthorIndex != -1 && isTextAuthorSelected && startIndex == endIndex && startIndex == textAuthorIndex) {
        // Случай: курсор на text-author или выделен только он
        if (showStatistics && showStatistics == 1) {
            MsgBox("Применяется правило А: text-author выносится из цитаты", "FBE скрипт\n" + scriptName + "\nver. " + version);
        }
        
        // Создаем новую цитату для абзацев до text-author
        if (textAuthorIndex > 0) {
            var newCiteBefore = document.createElement("DIV");
            newCiteBefore.className = "cite";
            
            // Вставляем новую цитату перед старой
            citeElement.parentNode.insertBefore(newCiteBefore, citeElement);
            
            // Перемещаем абзацы до text-author в новую цитату (с сохранением классов)
            for (i = 0; i < textAuthorIndex; i++) {
                newCiteBefore.appendChild(allParagraphs[i]);
            }
        }
        
        // Выносим text-author из цитаты (становится обычным абзацем - удаляем класс)
        var taParagraph = allParagraphs[textAuthorIndex];
        unformatTextAuthor(taParagraph);
        
        // Вставляем его на место старой цитаты
        citeElement.parentNode.insertBefore(taParagraph, citeElement);
        
        // Создаем новую цитату для абзацев после text-author (с сохранением классов)
        if (textAuthorIndex < allParagraphs.length - 1) {
            var newCiteAfter = document.createElement("DIV");
            newCiteAfter.className = "cite";
            
            // Вставляем после taParagraph или после старой цитаты
            if (textAuthorIndex == 0) {
                citeElement.parentNode.insertBefore(newCiteAfter, citeElement);
            } else {
                citeElement.parentNode.insertBefore(newCiteAfter, citeElement);
            }
            
            // Перемещаем абзацы после text-author в новую цитату
            for (i = textAuthorIndex + 1; i < allParagraphs.length; i++) {
                newCiteAfter.appendChild(allParagraphs[i]);
            }
        }
        
        // Удаляем старую пустую цитату
        citeElement.removeNode(true);
    }
    
    // СЛУЧАЙ: text-author выделен вместе с другими абзацами (не вся цитата, но несколько абзацев включая text-author)
    else if (textAuthorIndex != -1 && isTextAuthorSelected && !(startIndex == endIndex && startIndex == textAuthorIndex) && !isFullCiteSelected) {
        if (showStatistics && showStatistics == 1) {
            MsgBox("Выделены абзацы вместе с text-author, text-author расформатируется", "FBE скрипт\n" + scriptName + "\nver. " + version);
        }
        
        // Создаем первую цитату для абзацев до выделенных
        if (startIndex > 0) {
            var newCiteFirst = document.createElement("DIV");
            newCiteFirst.className = "cite";
            citeElement.parentNode.insertBefore(newCiteFirst, citeElement);
            
            for (i = 0; i < startIndex; i++) {
                newCiteFirst.appendChild(allParagraphs[i]);
            }
        }
        
        // Выносим выделенные абзацы, удаляя класс у text-author
        for (i = startIndex; i <= endIndex; i++) {
            var p = allParagraphs[i];
            
            // Если это text-author - удаляем класс
            if (p.className == "text-author") {
                unformatTextAuthor(p);
            }
            // Для остальных класс сохраняется
            
            citeElement.parentNode.insertBefore(p, citeElement);
        }
        
        // Создаем последнюю цитату для оставшихся абзацев
        if (endIndex < allParagraphs.length - 1) {
            var newCiteLast = document.createElement("DIV");
            newCiteLast.className = "cite";
            
            if (endIndex == allParagraphs.length - 1) {
                citeElement.parentNode.insertBefore(newCiteLast, citeElement.nextSibling);
            } else {
                citeElement.parentNode.insertBefore(newCiteLast, citeElement);
            }
            
            for (i = endIndex + 1; i < allParagraphs.length; i++) {
                newCiteLast.appendChild(allParagraphs[i]);
            }
        }
        
        // Удаляем старую цитату
        citeElement.removeNode(true);
    }
    
    // Правило Б: выделен последний обычный абзац ПЕРЕД text-author (сам text-author не выделен)
    else if (textAuthorIndex != -1 && !isTextAuthorSelected && endIndex == textAuthorIndex - 1) {
        if (showStatistics && showStatistics == 1) {
            MsgBox("Применяется правило Б: выделенный абзац выносится, text-author остается в цитате", "FBE скрипт\n" + scriptName + "\nver. " + version);
        }
        
        // Создаем первую цитату для абзацев до выделенных (с сохранением классов)
        if (startIndex > 0) {
            var newCiteFirst = document.createElement("DIV");
            newCiteFirst.className = "cite";
            citeElement.parentNode.insertBefore(newCiteFirst, citeElement);
            
            for (i = 0; i < startIndex; i++) {
                newCiteFirst.appendChild(allParagraphs[i]);
            }
        }
        
        // Выносим выделенные абзацы (классы сохраняются)
        for (i = startIndex; i <= endIndex; i++) {
            var p = allParagraphs[i];
            if (startIndex > 0) {
                citeElement.parentNode.insertBefore(p, citeElement);
            } else {
                citeElement.parentNode.insertBefore(p, citeElement);
            }
        }
        
        // Создаем последнюю цитату для оставшихся абзацев (включая text-author, с сохранением классов)
        var newCiteLast = document.createElement("DIV");
        newCiteLast.className = "cite";
        
        if (endIndex == allParagraphs.length - 1) {
            citeElement.parentNode.insertBefore(newCiteLast, citeElement.nextSibling);
        } else {
            citeElement.parentNode.insertBefore(newCiteLast, citeElement);
        }
        
        for (i = endIndex + 1; i < allParagraphs.length; i++) {
            newCiteLast.appendChild(allParagraphs[i]);
        }
        
        // Удаляем старую цитату
        citeElement.removeNode(true);
    }
    
    // Правило В: выделены абзацы внутри цитаты, text-author в конце и НЕ ВЫДЕЛЕН
    else if (textAuthorIndex != -1 && !isTextAuthorSelected && endIndex < textAuthorIndex) {
        if (showStatistics && showStatistics == 1) {
            MsgBox("Применяется правило В: выделенные абзацы выносятся, text-author остается в последней цитате", "FBE скрипт\n" + scriptName + "\nver. " + version);
        }
        
        // Создаем первую цитату для абзацев до выделенных (с сохранением классов)
        if (startIndex > 0) {
            var newCiteFirst = document.createElement("DIV");
            newCiteFirst.className = "cite";
            citeElement.parentNode.insertBefore(newCiteFirst, citeElement);
            
            for (i = 0; i < startIndex; i++) {
                newCiteFirst.appendChild(allParagraphs[i]);
            }
        }
        
        // Выносим выделенные абзацы (классы сохраняются)
        for (i = startIndex; i <= endIndex; i++) {
            var p = allParagraphs[i];
            citeElement.parentNode.insertBefore(p, citeElement);
        }
        
        // Создаем последнюю цитату для оставшихся абзацев (включая text-author, с сохранением классов)
        var newCiteLast = document.createElement("DIV");
        newCiteLast.className = "cite";
        citeElement.parentNode.insertBefore(newCiteLast, citeElement);
        
        for (i = endIndex + 1; i < allParagraphs.length; i++) {
            newCiteLast.appendChild(allParagraphs[i]);
        }
        
        // Удаляем старую цитату
        citeElement.removeNode(true);
    }
    
    // Общий случай: нет text-author
    else {
        if (showStatistics && showStatistics == 1) {
            MsgBox("Применяется общий случай разделения цитаты", "FBE скрипт\n" + scriptName + "\nver. " + version);
        }
        
        // Определяем, где находятся выделенные абзацы
        
        // Случай 1: выделены первые абзацы
        if (startIndex == 0) {
            // Выносим выделенные абзацы (классы сохраняются)
            for (i = 0; i <= endIndex; i++) {
                var p = allParagraphs[i];
                citeElement.parentNode.insertBefore(p, citeElement);
            }
            
            // Оставшиеся абзацы остаются в цитате (с сохранением классов)
            if (endIndex < allParagraphs.length - 1) {
                // Ничего не делаем, они уже в citeElement
            } else {
                // Все абзацы были выделены - удаляем пустую цитату
                citeElement.removeNode(true);
            }
        }
        
        // Случай 2: выделены последние абзацы
        else if (endIndex == allParagraphs.length - 1) {
            // Создаем новую цитату для первых абзацев (с сохранением классов)
            var newCiteFirst = document.createElement("DIV");
            newCiteFirst.className = "cite";
            citeElement.parentNode.insertBefore(newCiteFirst, citeElement);
            
            for (i = 0; i < startIndex; i++) {
                newCiteFirst.appendChild(allParagraphs[i]);
            }
            
            // Выносим выделенные абзацы (классы сохраняются)
            for (i = startIndex; i <= endIndex; i++) {
                var p = allParagraphs[i];
                citeElement.parentNode.insertBefore(p, citeElement);
            }
            
            // Удаляем старую цитату
            citeElement.removeNode(true);
        }
        
        // Случай 3: выделены абзацы в середине
        else {
            // Создаем первую цитату для абзацев до выделенных (с сохранением классов)
            var newCiteFirst = document.createElement("DIV");
            newCiteFirst.className = "cite";
            citeElement.parentNode.insertBefore(newCiteFirst, citeElement);
            
            for (i = 0; i < startIndex; i++) {
                newCiteFirst.appendChild(allParagraphs[i]);
            }
            
            // Выносим выделенные абзацы (классы сохраняются)
            for (i = startIndex; i <= endIndex; i++) {
                var p = allParagraphs[i];
                citeElement.parentNode.insertBefore(p, citeElement);
            }
            
            // Создаем последнюю цитату для абзацев после выделенных (с сохранением классов)
            var newCiteLast = document.createElement("DIV");
            newCiteLast.className = "cite";
            citeElement.parentNode.insertBefore(newCiteLast, citeElement);
            
            for (i = endIndex + 1; i < allParagraphs.length; i++) {
                newCiteLast.appendChild(allParagraphs[i]);
            }
            
            // Удаляем старую цитату
            citeElement.removeNode(true);
        }
    }
    
    // ==================================================
    // ОЧИСТКА И ЗАВЕРШЕНИЕ
    // ==================================================
    
    // Удаляем временные маркеры
    if (markerStart) markerStart.removeNode(true);
    if (markerEnd) markerEnd.removeNode(true);
    
    // Завершаем группу отмены
    window.external.EndUndoUnit(document);
    
    // В тихом режиме ничего не показываем
}
