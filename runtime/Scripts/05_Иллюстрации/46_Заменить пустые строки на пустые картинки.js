// Скрипт "Заменить пустые строки на пустые картинки" для редактора FBE
// version 1.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для замены пустых строк на пустые блочные иллюстрации "пустышки" в fb2 документах.
// При наличии выделения, скрипт работает с выделенным фрагментом,
// в противном случае - обрабатывается сразу весь документ.
// Скрипт не делает замен внутри блочных элементов: в history, аннотациях, заголовках, эпиграфах, цитатах, стихах.
// Также не заменяются пустые строки после эпиграфов.
// Замена пустых строк на картинки-пустышки рядом с другими иллюстрациями, заголовками, подзаголовками - настраивается.
// По умолчанию обрабатывается только основной раздел документа, без сносок и комментариев.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.2, 14.05.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Заменить пустые строки на пустые картинки";
    var version = "1.2";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1;
    
    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0;     // 0 - нет, 1 - да
    
    // Обрабатывать раздел комментариев
    var processCommentsSection = 0;     // 0 - нет, 1 - да
    
    // Заменять пустые строки рядом с иллюстрациями
    var processImages = 0;     // 0 - нет, 1 - да
    
    // Заменять пустые строки ПОСЛЕ заголовков разделов-секций (section)
    var processTitles = 0;     // 0 - нет, 1 - да
    
    // Заменять пустые строки рядом с подзаголовками
    var processSubtitles = 0;     // 0 - нет, 1 - да
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
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
    
    // Регулярное выражение для проверки пустой строки
    var re0 = new RegExp("^( | |&nbsp;|"+nbspChar+")*?$","");
    var re2 = new RegExp("<(?!img)[^>]*?>", "gi");
    
    var Ts = new Date().getTime();
    var mainCount = 0;
    var notesCount = 0;
    var commentsCount = 0;
    
    // Функция для определения раздела элемента
    function getSectionType(element) {
        var parent = element.parentNode;
        while (parent) {
            if (parent.nodeName == "DIV" && parent.className == "body") {
                var fbname = parent.getAttribute("fbname") || "";
                if (fbname == "") return "main";
                if (fbname == "notes") return "notes";
                if (fbname == "comments") return "comments";
                return "other";
            }
            parent = parent.parentNode;
        }
        return "main";
    }
    
    // Функция для проверки, находится ли элемент в истории (НИКОГДА не обрабатываем)
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
    
    // Функция для проверки, находится ли элемент в первой аннотации (НИКОГДА не обрабатываем)
    function isInFirstAnnotation(element) {
        var parent = element.parentNode;
        while (parent) {
            if (parent.nodeName == "DIV" && parent.className == "annotation") {
                // Проверяем, что этот annotation находится до первого body
                var bodyDivs = document.getElementsByTagName("DIV");
                var firstBodyFound = false;
                for (var i = 0; i < bodyDivs.length; i++) {
                    var div = bodyDivs[i];
                    if (div.className == "body" && div.getAttribute("fbname") == "") {
                        firstBodyFound = true;
                        break;
                    }
                    if (div == parent) {
                        // Нашли наш annotation до первого body
                        return true;
                    }
                }
                return false;
            }
            parent = parent.parentNode;
        }
        return false;
    }
    
    // Функция для проверки, находится ли элемент в epigraph или title (НИКОГДА не обрабатываем)
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
    
    // Функция для проверки, находится ли элемент в любом структурном элементе (НИКОГДА не обрабатываем)
    function isInStructuralElement(element) {
        var parent = element.parentNode;
        while (parent) {
            if (parent.nodeName == "DIV" && parent.className) {
                if (parent.className == "annotation" || 
                    parent.className == "poem" || 
                    parent.className == "stanza" || 
                    parent.className == "cite" ||
                    parent.className == "history") {
                    return true;
                }
            }
            parent = parent.parentNode;
        }
        return false;
    }
    
    // Функция для проверки, находится ли элемент сразу после эпиграфа
    function isAfterEpigraph(element) {
        var prev = element.previousSibling;
        // Пропускаем текстовые узлы и комментарии
        while (prev && prev.nodeType != 1) {
            prev = prev.previousSibling;
        }
        if (prev && prev.nodeName == "DIV" && prev.className == "epigraph") {
            return true;
        }
        return false;
    }
    
    // Функция для проверки соседства с картинками
    function hasImageNearby(element) {
        if (processImages == 1) return false;
        
        var prev = element.previousSibling;
        while (prev && prev.nodeType != 1) {
            prev = prev.previousSibling;
        }
        if (prev && prev.nodeName == "DIV" && prev.className == "image") {
            return true;
        }
        
        var next = element.nextSibling;
        while (next && next.nodeType != 1) {
            next = next.nextSibling;
        }
        if (next && next.nodeName == "DIV" && next.className == "image") {
            return true;
        }
        
        return false;
    }
    
    // Функция для проверки соседства с history
    function hasHistoryNearby(element) {
        var prev = element.previousSibling;
        while (prev && prev.nodeType != 1) {
            prev = prev.previousSibling;
        }
        if (prev && prev.nodeName == "DIV" && prev.className == "history") {
            return true;
        }
        
        var next = element.nextSibling;
        while (next && next.nodeType != 1) {
            next = next.nextSibling;
        }
        if (next && next.nodeName == "DIV" && next.className == "history") {
            return true;
        }
        
        return false;
    }
    
    // Функция для проверки соседства с аннотациями
    function hasAnnotationNearby(element) {
        var prev = element.previousSibling;
        while (prev && prev.nodeType != 1) {
            prev = prev.previousSibling;
        }
        if (prev && prev.nodeName == "DIV" && prev.className == "annotation") {
            return true;
        }
        
        var next = element.nextSibling;
        while (next && next.nodeType != 1) {
            next = next.nextSibling;
        }
        if (next && next.nodeName == "DIV" && next.className == "annotation") {
            return true;
        }
        
        return false;
    }
    
    // Функция для проверки, является ли title заголовком body (а не section)
    function isBodyTitle(titleElement) {
        var parent = titleElement.parentNode;
        if (parent && parent.nodeName == "DIV" && parent.className == "body") {
            return true;
        }
        return false;
    }
    
    // Функция для проверки соседства с заголовками
    function hasTitleNearby(element) {
        if (processTitles == 1) return false;
        
        var prev = element.previousSibling;
        while (prev && prev.nodeType != 1) {
            prev = prev.previousSibling;
        }
        if (prev && prev.nodeName == "DIV" && prev.className == "title") {
            return true;
        }
        
        var next = element.nextSibling;
        while (next && next.nodeType != 1) {
            next = next.nextSibling;
        }
        if (next && next.nodeName == "DIV" && next.className == "title") {
            return true;
        }
        
        // Дополнительная проверка: если элемент находится в body,
        // а следующий за ним элемент - section с title внутри
        if (!prev || prev.nodeType != 1) {
            var parent = element.parentNode;
            // Если родитель - section, проверяем предыдущий элемент на уровне section
            if (parent && parent.nodeName == "DIV" && parent.className == "section") {
                var prevSectionSibling = parent.previousSibling;
                while (prevSectionSibling && prevSectionSibling.nodeType != 1) {
                    prevSectionSibling = prevSectionSibling.previousSibling;
                }
                // Если перед section находится title body - это соседство с заголовком
                if (prevSectionSibling && prevSectionSibling.nodeName == "DIV" && 
                    prevSectionSibling.className == "title" && isBodyTitle(prevSectionSibling)) {
                    return true;
                }
            }
            // Если родитель - body, проверяем предыдущий элемент на уровне body
            if (parent && parent.nodeName == "DIV" && parent.className == "body") {
                var prevBodySibling = element.previousSibling;
                while (prevBodySibling && prevBodySibling.nodeType != 1) {
                    prevBodySibling = prevBodySibling.previousSibling;
                }
                if (prevBodySibling && prevBodySibling.nodeName == "DIV" && 
                    prevBodySibling.className == "title" && isBodyTitle(prevBodySibling)) {
                    return true;
                }
            }
        }
        
        return false;
    }
    
    // Функция для проверки соседства с эпиграфами
    function hasEpigraphNearby(element) {
        var prev = element.previousSibling;
        while (prev && prev.nodeType != 1) {
            prev = prev.previousSibling;
        }
        if (prev && prev.nodeName == "DIV" && prev.className == "epigraph") {
            return true;
        }
        
        var next = element.nextSibling;
        while (next && next.nodeType != 1) {
            next = next.nextSibling;
        }
        if (next && next.nodeName == "DIV" && next.className == "epigraph") {
            return true;
        }
        
        return false;
    }
    
    // Функция для проверки соседства с подзаголовками
    function hasSubtitleNearby(element) {
        if (processSubtitles == 1) return false;
        
        var prev = element.previousSibling;
        while (prev && prev.nodeType != 1) {
            prev = prev.previousSibling;
        }
        if (prev && prev.nodeName == "P" && prev.className == "subtitle") {
            return true;
        }
        
        var next = element.nextSibling;
        while (next && next.nodeType != 1) {
            next = next.nextSibling;
        }
        if (next && next.nodeName == "P" && next.className == "subtitle") {
            return true;
        }
        
        return false;
    }
    
    // Функция для замены пустой строки на пустую картинку
    function replaceEmptyLine(pElement) {
        try {
            var newDiv = document.createElement("DIV");
            newDiv.setAttribute("onresizestart", "return false");
            newDiv.className = "image";
            newDiv.setAttribute("contentEditable", "false");
            newDiv.setAttribute("href", "#undefined");
            
            var newImg = document.createElement("IMG");
            newImg.setAttribute("src", "fbw-internal:#undefined");
            
            newDiv.appendChild(newImg);
            pElement.parentNode.insertBefore(newDiv, pElement);
            pElement.removeNode(true);
            return true;
        }
        catch(e) {
            return false;
        }
    }
    
    // Функция для проверки, можно ли обрабатывать абзац
    function canProcessParagraph(p) {
        // 1. Проверяем пустая ли строка
        if (!re0.test(p.innerHTML.replace(re2, ""))) return false;
        
        // 2. Проверяем history - НИКОГДА не обрабатываем
        if (isInHistory(p)) return false;
        
        // 3. Проверяем первую аннотацию - НИКОГДА не обрабатываем
        if (isInFirstAnnotation(p)) return false;
        
        // 4. Проверяем epigraph и title - НИКОГДА не обрабатываем внутри
        if (isInProtectedElement(p)) return false;
        
        // 5. Проверяем любые структурные элементы - НИКОГДА не обрабатываем внутри
        if (isInStructuralElement(p)) return false;
        
        // 6. Проверяем, не идет ли строка сразу после эпиграфа
        if (isAfterEpigraph(p)) return false;
        
        // 7. Проверяем соседство с картинками
        if (hasImageNearby(p)) return false;
        
        // 8. Проверяем соседство с history
        if (hasHistoryNearby(p)) return false;
        
        // 9. Проверяем соседство с аннотациями (внутри body)
        if (hasAnnotationNearby(p)) return false;
        
        // 10. Проверяем соседство с заголовками
        if (hasTitleNearby(p)) return false;
        
        // 11. Проверяем соседство с эпиграфами
        if (hasEpigraphNearby(p)) return false;
        
        // 12. Проверяем соседство с подзаголовками
        if (hasSubtitleNearby(p)) return false;
        
        return true;
    }
    
    // Функция для обработки всех абзацев внутри указанного контейнера
    function processContainer(container) {
        var count = 0;
        var ptr = container;
        var endProcessing = false;
        var saveNode = null;
        var saveNext;
        
        while (!endProcessing && ptr) {
            saveNext = ptr;
            if (saveNext.firstChild && saveNext.nodeName != "P" && 
                !(saveNext.nodeName == "DIV" && 
                  (
                   (saveNext.className == "history" && false) ||
                   (saveNext.className == "annotation" && false) ||
                   saveNext.className == "table"
                  )
                 )
               ) {
                saveNext = saveNext.firstChild;
                saveNode = null;
            }
            else {
                saveNode = saveNext.parentNode;
                while (!saveNext.nextSibling) {
                    saveNext = saveNext.parentNode;
                    if (saveNext == container) endProcessing = true;
                }
                saveNext = saveNext.nextSibling;
            }
            
            if (ptr.nodeName == "P") {
                if (canProcessParagraph(ptr)) {
                    if (replaceEmptyLine(ptr)) count++;
                }
            }
            ptr = saveNext;
        }
        
        return count;
    }
    
    window.external.BeginUndoUnit(document, scriptName);
    
    // Проверяем наличие выделения
    var selRange = document.selection.createRange();
    var hasSelection = false;
    
    if (selRange && selRange.compareEndPoints("StartToEnd", selRange) != 0) {
        if (selRange.parentElement().nodeName != "TEXTAREA" && selRange.parentElement().nodeName != "INPUT") {
            hasSelection = true;
        }
    }
    
    if (hasSelection) {
        // ==================================================
        // РАБОТА С ВЫДЕЛЕНИЕМ
        // ==================================================
        
        function getNextNode(el) {
            if (el.firstChild && el.nodeName != "P")
                el = el.firstChild;
            else {
                while (el && !el.nextSibling)
                    el = el.parentNode;
                if (el && el.nextSibling)
                    el = el.nextSibling;
            }
            return el;
        }
        
        function getNextP(el) {
            var savedEl = el;
            while (el && (el.nodeName != "P" || el == savedEl)) {
                el = getNextNode(el);
            }
            return el;
        }
        
        var trStart = document.selection.createRange();
        trStart.collapse(true);
        var blockStartEl = trStart.parentElement();
        
        var trEnd = document.selection.createRange();
        trEnd.collapse(false);
        var blockEndEl = trEnd.parentElement();
        
        var psArray = [];
        var ptr = blockStartEl;
        
        while (ptr && ptr != null) {
            if (ptr.nodeName == "P") {
                var alreadyExists = false;
                for (var k = 0; k < psArray.length; k++) {
                    if (psArray[k] == ptr) {
                        alreadyExists = true;
                        break;
                    }
                }
                if (!alreadyExists) {
                    psArray.push(ptr);
                }
            }
            if (ptr === blockEndEl) break;
            ptr = getNextP(ptr);
            if (!ptr) break;
        }
        
        // Обрабатываем в обратном порядке с учетом разделов
        for (var i = psArray.length - 1; i >= 0; i--) {
            var p = psArray[i];
            if (p.nodeName != "P") continue;
            
            // Определяем раздел
            var sectionType = getSectionType(p);
            
            // Проверяем, можно ли обрабатывать этот абзац
            if (!canProcessParagraph(p)) continue;
            
            // Проверяем, можно ли обрабатывать в этом разделе
            if (sectionType == "main") {
                if (replaceEmptyLine(p)) mainCount++;
            } else if (sectionType == "notes" && processNotesSection == 1) {
                if (replaceEmptyLine(p)) notesCount++;
            } else if (sectionType == "comments" && processCommentsSection == 1) {
                if (replaceEmptyLine(p)) commentsCount++;
            }
        }
        
    } else {
        // ==================================================
        // РАБОТА СО ВСЕМ ДОКУМЕНТОМ
        // ==================================================
        
        // Находим все DIV с классом body
        var allBodies = [];
        var bodyDivs = document.getElementsByTagName("DIV");
        for (var i = 0; i < bodyDivs.length; i++) {
            var div = bodyDivs[i];
            if (div.className == "body") {
                allBodies[allBodies.length] = div;
            }
        }
        
        // Обрабатываем каждый найденный body
        for (var i = 0; i < allBodies.length; i++) {
            var body = allBodies[i];
            var fbname = body.getAttribute("fbname") || "";
            
            // Обрабатываем только нужные разделы
            if (fbname == "") {
                // Основной раздел
                mainCount = processContainer(body);
            } else if (fbname == "notes" && processNotesSection == 1) {
                // Раздел примечаний, если включен
                notesCount = processContainer(body);
            } else if (fbname == "comments" && processCommentsSection == 1) {
                // Раздел комментариев, если включен
                commentsCount = processContainer(body);
            }
            // Остальные разделы игнорируем
        }
    }
    
    window.external.EndUndoUnit(document);
    
    var Tf = new Date().getTime();
    var timeSec = (Tf - Ts) / 1000;
    var timeStr = timeSec.toFixed(3).replace(".", ",");
    
    var totalCount = mainCount + notesCount + commentsCount;
    
    // Формируем сообщение статистики
    var msg = scriptName + "\n";
    msg += "ver. " + version + "\n\n";
    
    msg += "Режим обработки: ";
    if (hasSelection) {
        msg += "ВЫДЕЛЕНИЕ\n\n";
    } else {
        msg += "ВЕСЬ ДОКУМЕНТ\n\n";
    }
    
    if (totalCount == 0) {
        msg += "Пустых строк (согласно настройкам) в обработанной области не найдено.\n\n";
    } else {
        msg += "✓ Обработано разделов:\n";
        if (mainCount > 0 || (notesCount > 0 || commentsCount > 0)) {
            msg += "  • Основной раздел, замен: " + mainCount + "\n";
        }
        if (processNotesSection == 1) {
            if (notesCount > 0 || mainCount > 0 || commentsCount > 0) {
                msg += "  • Примечания, замен: " + notesCount + "\n";
            }
        }
        if (processCommentsSection == 1) {
            if (commentsCount > 0 || mainCount > 0 || notesCount > 0) {
                msg += "  • Комментарии, замен: " + commentsCount + "\n";
            }
        }
        if (totalCount > 0) {
            msg += "\n✓ Всего произведено замен: " + totalCount + "\n\n";
        }
    }
    
    msg += "- Настройки обработки:\n";
    msg += "  • Рядом с обычными абзацами: ДА\n";
    if (processTitles == 1) {
        msg += "  • После заголовков: ДА\n";
    } else {
        msg += "  • После заголовков: НЕТ\n";
    }
    if (processSubtitles == 1) {
        msg += "  • Рядом с подзаголовками: ДА\n";
    } else {
        msg += "  • Рядом с подзаголовками: НЕТ\n";
    }
    msg += "  • Внутри структурных элементов: НИКОГДА\n";
    msg += "  • Рядом с эпиграфами: НИКОГДА\n";
    if (processImages == 1) {
        msg += "  • Рядом с иллюстрациями: ДА\n";
    } else {
        msg += "  • Рядом с иллюстрациями: НЕТ\n";
    }
    
    msg += "\n- Обработка разделов:\n";
    msg += "  • Основной раздел: ДА\n";
    if (processNotesSection == 1) {
        msg += "  • Раздел сносок (примечаний): ДА\n";
    } else {
        msg += "  • Раздел сносок (примечаний): НЕТ\n";
    }
    if (processCommentsSection == 1) {
        msg += "  • Раздел комментариев: ДА\n";
    } else {
        msg += "  • Раздел комментариев: НЕТ\n";
    }
    
    msg += "\nВремя обработки: " + timeStr + " сек";
    
    // Выводим сообщение всегда (и в тихом, и в обычном режиме)
    MsgBox(msg, "FBE скрипт");
    
    try {
        if (totalCount > 0) {
            window.external.SetStatusBarText(scriptName + " ver. " + version + ": заменено " + totalCount + " строк. Время: " + timeStr + " сек.");
        } else {
            window.external.SetStatusBarText(scriptName + " ver. " + version + ": пустых строк не найдено. Время: " + timeStr + " сек.");
        }
    }
    catch(e) {}
}
