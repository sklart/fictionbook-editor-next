// Скрипт "Сформатировать все целиком курсивные абзацы цитатами" для редактора FBE
// version 1.7
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для автоматического превращения в цитаты полностью курсивных абзацев в fb2 документах.
// Скрипт преобразует найденные подходящие абзацы в цитаты, опционально удаляя внешние теги курсива.
// Абзацы внутри уже размеченных элементов (заголовки, аннотации, эпиграфы, стихи,
// строфы, цитаты, таблицы, подзаголовки), не обрабатываются.
// Можно настроить создание отдельных или единых цитат из соседних курсивных абзацев.
// По умолчанию обрабатывается только основной раздел документа, без разделов сносок и комментариев.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.7, 08.03.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Сформатировать все целиком курсивные абзацы цитатами";
    var version = "1.7";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка режима отображения:
    // 0 - показывать диалоговые окна, анализ и статистику
    // 1 - показывать только статистику в конце (без предварительного анализа и запросов)
    var showStatistics = 0;
    
    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 1;     // 0 - нет, 1 - да
    
    // Обрабатывать раздел комментариев
    var processCommentsSection = 1;     // 0 - нет, 1 - да
    
    // Удалять ли внешние тэги курсива (EM) после преобразования
    var processItalic = 1;     // 0 - нет, 1 - да
    
    // Объединять соседние курсивные абзацы в единую цитату (или создавать отдельные цитаты)
    var processJoinItalic = 1;     // 0 - нет, 1 - да
    
    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================
    
    // Получение неразрывного пробела из настроек FBE
    try { 
        var nbspChar = window.external.GetNBSP(); 
        var nbspEntity; 
        if (nbspChar.charCodeAt(0) == 160) 
            nbspEntity = "&nbsp;"; 
        else 
            nbspEntity = nbspChar; 
    } catch(e) { 
        var nbspChar = String.fromCharCode(160); 
        var nbspEntity = "&nbsp;";
    }
    
    // Список структурных DIV классов (в которых НЕ нужно обрабатывать абзацы)
    var structuralDivClasses = " annotation title epigraph cite poem stanza table ";
    
    // Список структурных P классов
    var structuralPClasses = " text-author subtitle ";
    
    // Функция проверки, находится ли абзац внутри структурного DIV
    function isInsideStructuralDiv(pElement) {
        var parent = pElement.parentNode;
        while (parent && parent.nodeName != "BODY") {
            if (parent.nodeName == "DIV") {
                var className = parent.className || "";
                if (structuralDivClasses.indexOf(" " + className + " ") != -1) {
                    return true;
                }
            }
            parent = parent.parentNode;
        }
        return false;
    }
    
    // Функция проверки, является ли сам абзац структурным
    function isStructuralParagraph(pElement) {
        if (!pElement || pElement.nodeName != "P") return false;
        
        var className = pElement.className || "";
        if (structuralPClasses.indexOf(" " + className + " ") != -1) {
            return true;
        }
        
        return false;
    }
    
    // Функция проверки, находится ли элемент внутри сноски
    function isInsideNoteOrComment(node) {
        var current = node;
        while (current && current.nodeName != "BODY") {
            if (current.nodeName == "A") {
                var className = current.className || "";
                if (className == "note") return true;
                
                var href = current.getAttribute("href") || "";
                if (href.length > 0 && href.charAt(0) == "#") return true;
            }
            current = current.parentNode;
        }
        return false;
    }
    
    // Функция определения раздела (основной, сноски, комментарии)
    function getSectionType(element) {
        var body = element;
        while (body && body.nodeName != "BODY") {
            if (body.nodeName == "DIV" && body.className == "body") {
                var fbname = body.getAttribute("fbname") || "";
                if (fbname == "notes") return "notes";
                if (fbname == "comments") return "comments";
                return "main";
            }
            body = body.parentNode;
        }
        return "main";
    }
    
    // Функция проверки, является ли элемент сноской
    function isFootnoteElement(node) {
        if (!node || node.nodeType != 1) return false;
        
        if (node.nodeName == "A") {
            var className = node.className || "";
            var href = node.getAttribute("href") || "";
            if (className == "note" || (href.length > 0 && href.charAt(0) == "#")) {
                return true;
            }
        }
        return false;
    }
    
    // Функция удаления курсива внутри сноски
    function removeItalicFromFootnote(footnote) {
        if (!footnote) return;
        
        // Находим все EM внутри сноски
        var emElements = footnote.getElementsByTagName("EM");
        var iElements = footnote.getElementsByTagName("I");
        
        // Удаляем EM (переносим их содержимое на уровень выше)
        for (var i = emElements.length - 1; i >= 0; i--) {
            var em = emElements[i];
            var parent = em.parentNode;
            while (em.firstChild) {
                parent.insertBefore(em.firstChild, em);
            }
            parent.removeChild(em);
        }
        
        // Удаляем I
        for (var i = iElements.length - 1; i >= 0; i--) {
            var iTag = iElements[i];
            var parent = iTag.parentNode;
            while (iTag.firstChild) {
                parent.insertBefore(iTag.firstChild, iTag);
            }
            parent.removeChild(iTag);
        }
    }
    
    // Функция проверки, является ли абзац полностью курсивным (текст, игнорируя сноски)
    function isParagraphFullyItalic(pElement) {
        if (!pElement || pElement.nodeName != "P") return false;
        
        // Проверяем, не структурный ли это абзац сам по себе
        if (isStructuralParagraph(pElement)) return false;
        
        // Проверяем, не внутри ли структурного DIV
        if (isInsideStructuralDiv(pElement)) return false;
        
        // Проверяем, не внутри ли сноски (сам абзац)
        if (isInsideNoteOrComment(pElement)) return false;
        
        var hasText = false;
        var allTextInItalic = true;
        
        // Рекурсивная проверка всех узлов, игнорируя сноски
        function checkNode(node) {
            if (!node) return;
            
            // Если это сноска - полностью игнорируем её содержимое
            if (node.nodeType == 1 && isFootnoteElement(node)) {
                return;
            }
            
            if (node.nodeType == 3) { // TEXT_NODE
                var text = node.nodeValue;
                var trimmed = text.replace(/\s/g, "");
                if (trimmed.length > 0) {
                    hasText = true;
                    
                    // Проверяем, находится ли этот текст внутри EM
                    var parent = node.parentNode;
                    var inItalic = false;
                    while (parent && parent != pElement) {
                        if (parent.nodeName == "EM" || parent.nodeName == "I") {
                            inItalic = true;
                            break;
                        }
                        parent = parent.parentNode;
                    }
                    
                    if (!inItalic) {
                        allTextInItalic = false;
                    }
                }
            } else if (node.nodeType == 1) { // ELEMENT_NODE
                // Рекурсивно проверяем дочерние узлы
                for (var i = 0; i < node.childNodes.length; i++) {
                    checkNode(node.childNodes[i]);
                    if (!allTextInItalic) break;
                }
            }
        }
        
        for (var i = 0; i < pElement.childNodes.length; i++) {
            checkNode(pElement.childNodes[i]);
            if (!allTextInItalic) break;
        }
        
        return hasText && allTextInItalic;
    }
    
    // Функция удаления внешних тегов курсива из абзаца (с очисткой сносок)
    function removeOuterItalic(pElement) {
        if (!pElement) return;
        
        // Сначала очищаем курсив внутри сносок
        var footnotes = [];
        function collectFootnotes(node) {
            if (!node) return;
            if (node.nodeType == 1 && isFootnoteElement(node)) {
                footnotes.push(node);
            }
            for (var i = 0; i < node.childNodes.length; i++) {
                collectFootnotes(node.childNodes[i]);
            }
        }
        
        collectFootnotes(pElement);
        
        // Удаляем курсив внутри каждой сноски
        for (var i = 0; i < footnotes.length; i++) {
            removeItalicFromFootnote(footnotes[i]);
        }
        
        // Создаём временный контейнер для нового содержимого
        var tempDiv = document.createElement("DIV");
        
        // Функция копирования узлов, удаляя внешние EM
        function copyNode(node, target) {
            if (!node) return;
            
            if (node.nodeType == 3) { // TEXT_NODE
                target.appendChild(node.cloneNode(false));
            } else if (node.nodeType == 1) { // ELEMENT_NODE
                // Если это сноска - копируем как есть (она уже очищена от курсива)
                if (isFootnoteElement(node)) {
                    target.appendChild(node.cloneNode(true));
                    return;
                }
                
                // Если это EM или I - пропускаем, копируем детей
                if (node.nodeName == "EM" || node.nodeName == "I") {
                    for (var i = 0; i < node.childNodes.length; i++) {
                        copyNode(node.childNodes[i], target);
                    }
                } else {
                    // Для остальных тегов - клонируем сам тег и его детей
                    var newNode = node.cloneNode(false);
                    target.appendChild(newNode);
                    for (var i = 0; i < node.childNodes.length; i++) {
                        copyNode(node.childNodes[i], newNode);
                    }
                }
            }
        }
        
        // Копируем все дочерние узлы
        for (var i = 0; i < pElement.childNodes.length; i++) {
            copyNode(pElement.childNodes[i], tempDiv);
        }
        
        // Заменяем содержимое абзаца
        while (pElement.firstChild) {
            pElement.removeChild(pElement.firstChild);
        }
        while (tempDiv.firstChild) {
            pElement.appendChild(tempDiv.firstChild);
        }
    }
    
    // Функция создания цитаты из группы абзацев (с сохранением пустых строк)
    function createCiteFromParagraphs(paragraphs, sectionType) {
        if (!paragraphs || paragraphs.length == 0) return null;
        
        var firstP = paragraphs[0];
        var parent = firstP.parentNode;
        
        // Создаём DIV цитаты
        var citeDiv = document.createElement("DIV");
        citeDiv.className = "cite";
        
        // Переносим абзацы в цитату
        for (var i = 0; i < paragraphs.length; i++) {
            var p = paragraphs[i];
            
            // Если нужно удалить внешний курсив (и курсив в сносках)
            if (processItalic == 1) {
                removeOuterItalic(p);
            }
            
            // Клонируем абзац
            var pCopy = p.cloneNode(true);
            citeDiv.appendChild(pCopy);
            
            // Если это не последний абзац в группе, проверяем пустые строки между ними
            if (i < paragraphs.length - 1) {
                var nextP = paragraphs[i + 1];
                
                // Проверяем элементы между текущим и следующим абзацем
                var current = p.nextSibling;
                while (current && current != nextP) {
                    if (current.nodeName == "P") {
                        var pText = current.innerText || "";
                        if (pText.replace(/\s/g, "").length == 0) {
                            var emptyPCopy = current.cloneNode(true);
                            citeDiv.appendChild(emptyPCopy);
                        }
                    }
                    current = current.nextSibling;
                }
            }
        }
        
        // Вставляем цитату перед первым абзацем
        parent.insertBefore(citeDiv, firstP);
        
        // Собираем элементы для удаления
        var elementsToRemove = [];
        
        // Добавляем все абзацы группы
        for (var i = 0; i < paragraphs.length; i++) {
            elementsToRemove.push(paragraphs[i]);
        }
        
        // Добавляем пустые строки между ними
        for (var i = 0; i < paragraphs.length - 1; i++) {
            var current = paragraphs[i].nextSibling;
            while (current && current != paragraphs[i + 1]) {
                if (current.nodeName == "P") {
                    var pText = current.innerText || "";
                    if (pText.replace(/\s/g, "").length == 0) {
                        elementsToRemove.push(current);
                    }
                }
                current = current.nextSibling;
            }
        }
        
        // Удаляем в обратном порядке
        for (var i = 0; i < elementsToRemove.length - 1; i++) {
            for (var j = i + 1; j < elementsToRemove.length; j++) {
                if (elementsToRemove[i].sourceIndex < elementsToRemove[j].sourceIndex) {
                    var temp = elementsToRemove[i];
                    elementsToRemove[i] = elementsToRemove[j];
                    elementsToRemove[j] = temp;
                }
            }
        }
        
        for (var i = 0; i < elementsToRemove.length; i++) {
            elementsToRemove[i].removeNode(true);
        }
        
        return citeDiv;
    }
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    // Получаем тело документа
    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) {
        MsgBox("Ошибка: не найден элемент fbw_body", "FBE скрипт");
        return;
    }
    
    // ==================================================
    // ФАЗА 1: АНАЛИЗ
    // ==================================================
    
    var allP = fbwBody.getElementsByTagName("P");
    var italicParagraphs = [];
    
    // Статистика
    var stats = {
        total: 0,
        main: 0,
        notes: 0,
        comments: 0,
        structuralDiv: 0,
        structuralP: 0,
        inNotes: 0,
        notItalic: 0
    };
    
    // Проходим по всем абзацам
    for (var i = 0; i < allP.length; i++) {
        var p = allP[i];
        
        // Проверяем структурные абзацы
        if (isStructuralParagraph(p)) {
            stats.structuralP++;
            continue;
        }
        
        // Проверяем, не внутри ли структурного DIV
        if (isInsideStructuralDiv(p)) {
            stats.structuralDiv++;
            continue;
        }
        
        // Проверяем, не внутри ли сноски
        if (isInsideNoteOrComment(p)) {
            stats.inNotes++;
            continue;
        }
        
        // Проверяем курсивность
        if (isParagraphFullyItalic(p)) {
            italicParagraphs.push(p);
            
            var sectionType = getSectionType(p);
            if (sectionType == "main") stats.main++;
            else if (sectionType == "notes") stats.notes++;
            else if (sectionType == "comments") stats.comments++;
        } else {
            stats.notItalic++;
        }
    }
    
    stats.total = italicParagraphs.length;
    
    // Если нет курсивных абзацев - завершаем (ВСЕГДА показываем сообщение)
    if (stats.total == 0) {
        MsgBox(scriptName + "\nver. " + version + 
               "\n---------------------------\n" +
               "✓ Полностью курсивных абзацев не найдено", "FBE скрипт");
        return;
    }
    
    // Определяем, какие разделы обрабатывать
    var processMain = 1; // основной раздел обрабатываем всегда
    var processNotes = (processNotesSection == 1);
    var processComments = (processCommentsSection == 1);
    
    // Формируем строки настроек для отчётов
    var mainSetting = "ДА";
    var notesSetting = (processNotesSection == 1) ? "ДА" : "НЕТ";
    var commentsSetting = (processCommentsSection == 1) ? "ДА" : "НЕТ";
    var italicSetting = (processItalic == 1) ? "ДА" : "НЕТ";
    var joinSetting = (processJoinItalic == 1) ? "ДА (в одну цитату)" : "НЕТ (отдельные цитаты)";
    
    // Фильтруем абзацы по настройкам разделов
    var paragraphsToProcess = [];
    for (var i = 0; i < italicParagraphs.length; i++) {
        var p = italicParagraphs[i];
        var sectionType = getSectionType(p);
        
        if (sectionType == "main" && processMain) {
            paragraphsToProcess.push(p);
        } else if (sectionType == "notes" && processNotes) {
            paragraphsToProcess.push(p);
        } else if (sectionType == "comments" && processComments) {
            paragraphsToProcess.push(p);
        }
    }
    
    // Если после фильтрации ничего не осталось - завершаем (ВСЕГДА показываем)
    if (paragraphsToProcess.length == 0) {
        var msg = scriptName + "\nver. " + version + 
                 "\n---------------------------\n" +
                 "✓ Найдено курсивных абзацев: " + stats.total + "\n" +
                 "  • в основном разделе: " + stats.main + "\n" +
                 "  • в разделе сносок (примечаний): " + stats.notes + "\n" +
                 "  • в разделе комментариев: " + stats.comments + "\n\n" +
                 "Текущие настройки скрипта:\n" +
                 "• Обработка основного раздела: " + mainSetting + "\n" +
                 "• Обработка раздела сносок (примечаний): " + notesSetting + "\n" +
                 "• Обработка раздела комментариев: " + commentsSetting + "\n\n" +
                 "✓ После фильтрации по разделам не осталось абзацев для обработки";
        
        MsgBox(msg, "FBE скрипт");
        return;
    }
    
    // Показываем анализ и запрашиваем подтверждение (если не тихий режим)
    if (showStatistics == 0) {
        var analysisMsg = scriptName + "\nver. " + version + 
                         "\n---------------------------\n" +
                         "Анализ документа:\n" +
                         "✓ Полностью курсивных обычных абзацев: " + stats.total + "\n" +
                         "  • в основном разделе: " + stats.main + "\n" +
                         "  • в разделе сносок (примечаний): " + stats.notes + "\n" +
                         "  • в разделе комментариев: " + stats.comments + "\n\n" +
                         "Текущие настройки скрипта:\n" +
                         "• Обработка основного раздела: " + mainSetting + "\n" +
                         "• Обработка раздела сносок (примечаний): " + notesSetting + "\n" +
                         "• Обработка раздела комментариев: " + commentsSetting + "\n" +
                         "• Удалять внешние тэги курсива: " + italicSetting + "\n" +
                         "• Объединять соседние курсивные абзацы: " + joinSetting + "\n\n" +
                         "Будет обработано абзацев: " + paragraphsToProcess.length + "\n\n" +
                         "Преобразовать найденные абзацы в цитаты?";
        
        if (!AskYesNo(analysisMsg)) {
            return; // пользователь отказался
        }
    }
    
    // ==================================================
    // ТАЙМЕР
    // ==================================================
    
    var startTime = new Date().getTime();
    
    // ==================================================
    // ФАЗА 2: ОБРАБОТКА
    // ==================================================
    
    window.external.BeginUndoUnit(document, scriptName + " " + version);
    
    var citesCreated = {
        total: 0,
        main: 0,
        notes: 0,
        comments: 0
    };
    
    if (processJoinItalic == 1) {
        // Режим объединения
        
        // Сортируем по позиции
        for (var i = 0; i < paragraphsToProcess.length - 1; i++) {
            for (var j = i + 1; j < paragraphsToProcess.length; j++) {
                if (paragraphsToProcess[i].sourceIndex > paragraphsToProcess[j].sourceIndex) {
                    var temp = paragraphsToProcess[i];
                    paragraphsToProcess[i] = paragraphsToProcess[j];
                    paragraphsToProcess[j] = temp;
                }
            }
        }
        
        // Группируем
        var groups = [];
        var currentGroup = [];
        var lastParent = null;
        var lastIndex = -1;
        
        for (var i = 0; i < paragraphsToProcess.length; i++) {
            var p = paragraphsToProcess[i];
            var parent = p.parentNode;
            
            var siblings = parent.childNodes;
            var index = -1;
            for (var j = 0; j < siblings.length; j++) {
                if (siblings[j] == p) {
                    index = j;
                    break;
                }
            }
            
            if (currentGroup.length == 0) {
                currentGroup.push(p);
                lastParent = parent;
                lastIndex = index;
            } else if (parent == lastParent && index == lastIndex + 1) {
                currentGroup.push(p);
                lastIndex = index;
            } else {
                groups.push(currentGroup);
                currentGroup = [p];
                lastParent = parent;
                lastIndex = index;
            }
        }
        
        if (currentGroup.length > 0) {
            groups.push(currentGroup);
        }
        
        // Создаём цитаты
        for (var i = groups.length - 1; i >= 0; i--) {
            var group = groups[i];
            if (group.length > 0) {
                var sectionType = getSectionType(group[0]);
                var cite = createCiteFromParagraphs(group, sectionType);
                
                if (cite) {
                    citesCreated.total++;
                    if (sectionType == "main") citesCreated.main++;
                    else if (sectionType == "notes") citesCreated.notes++;
                    else if (sectionType == "comments") citesCreated.comments++;
                }
            }
        }
        
    } else {
        // Режим отдельных цитат
        
        // Сортируем в обратном порядке
        for (var i = 0; i < paragraphsToProcess.length - 1; i++) {
            for (var j = i + 1; j < paragraphsToProcess.length; j++) {
                if (paragraphsToProcess[i].sourceIndex < paragraphsToProcess[j].sourceIndex) {
                    var temp = paragraphsToProcess[i];
                    paragraphsToProcess[i] = paragraphsToProcess[j];
                    paragraphsToProcess[j] = temp;
                }
            }
        }
        
        // Создаём отдельные цитаты
        for (var i = 0; i < paragraphsToProcess.length; i++) {
            var p = paragraphsToProcess[i];
            var sectionType = getSectionType(p);
            var cite = createCiteFromParagraphs([p], sectionType);
            
            if (cite) {
                citesCreated.total++;
                if (sectionType == "main") citesCreated.main++;
                else if (sectionType == "notes") citesCreated.notes++;
                else if (sectionType == "comments") citesCreated.comments++;
            }
        }
    }
    
    window.external.EndUndoUnit(document);
    
    // ==================================================
    // ФАЗА 3: СТАТИСТИКА
    // ==================================================
    
    var endTime = new Date().getTime();
    var elapsed = (endTime - startTime) / 1000;
    var elapsedStr = elapsed.toFixed(3).replace(".", ",");
    
    var statsMsg = scriptName + "\nver. " + version + 
                  "\n---------------------------\n" +
                  "✓ Полностью курсивных обычных абзацев: " + stats.total + "\n" +
                  "✓ Размечено цитат: " + citesCreated.total + "\n" +
                  "  • в основном разделе: " + citesCreated.main + "\n" +
                  "  • в разделе сносок (примечаний): " + citesCreated.notes + "\n" +
                  "  • в разделе комментариев: " + citesCreated.comments + "\n";
    
    if (processItalic == 1) {
        statsMsg += "• Внешние тэги курсива удалены.\n";
    }
    
    statsMsg += "\nТекущие настройки скрипта:\n" +
                "• Обработка основного раздела: " + mainSetting + "\n" +
                "• Обработка раздела сносок (примечаний): " + notesSetting + "\n" +
                "• Обработка раздела комментариев: " + commentsSetting + "\n" +
                "• Удалять внешние тэги курсива: " + italicSetting + "\n" +
                "• Объединять соседние курсивные абзацы: " + joinSetting + "\n\n" +
                "Время обработки: " + elapsedStr + " сек.";
    
    MsgBox(statsMsg, "FBE скрипт");
}
