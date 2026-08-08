// Скрипт "Заменить пустые строки на подзаголовки" для редактора FBE
// version 2.3
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для замены пустых строк на подзаголовки в fb2 документах.
// При наличии выделения скрипт работает с выделенным фрагментом,
// в противном случае - обрабатывается сразу весь документ.
// Скрипт не затрагивает заголовки, эпиграфы, history, первую аннотацию и пустые строки после эпиграфов.
// Остальные структурные элементы (стихи, цитаты, аннотации внутри боди и иллюстрации)
// обрабатываются опционально.
// Отдельная настройка для замены пустых строк рядом с "блочными" иллюстрациями.
// По умолчанию обрабатывается только основной раздел документа, без сносок и комментариев.
// По умолчанию замена пустых строк возле иллюстраций и внутри структурных элементов не производится.
//  Обработка разделов сносок и комментариев (опционально).
// Можно выбирать вид подзаголовка: стандартный звёздочками "* * *" или любой пользовательский.
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 2.3, 01.04.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Заменить пустые строки на подзаголовки";
    var version = "2.3";
    
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
    
    // Заменять ли пустые строки внутри структурных элементов (аннотации, стихи, цитаты)
    var processBlockElements = 0;     // 0 - нет, 1 - да
    
    // Оформление подзаголовков
    var processSubtitle = 1;     // 0 - стандартное звёздочками, 1 - пользовательское
    
    // Пользовательский вид подзаголовка:
    var UserSubtitle = "=♦=♦=♦=";
    
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
    
    // Определяем содержимое подзаголовка
    var subtitleContent = "";
    if (processSubtitle == 0) {
        subtitleContent = "* * *";
    } else {
        subtitleContent = UserSubtitle;
    }
    
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
    
    // Функция для проверки, находится ли элемент в структурном элементе (обрабатывается только при processBlockElements == 1)
    function isInStructuralElement(element) {
        var parent = element.parentNode;
        while (parent) {
            if (parent.nodeName == "DIV" && parent.className) {
                // Эти элементы обрабатываются только если processBlockElements == 1
                if (parent.className == "annotation" || 
                    parent.className == "poem" || 
                    parent.className == "stanza" || 
                    parent.className == "cite") {
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
    
    // Функция для замены пустой строки на подзаголовок
    function replaceEmptyLine(pElement) {
        try {
            var newP = document.createElement("P");
            newP.className = "subtitle";
            newP.innerHTML = subtitleContent;
            pElement.parentNode.insertBefore(newP, pElement);
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
        
        // 4. Проверяем epigraph и title - НИКОГДА не обрабатываем
        if (isInProtectedElement(p)) return false;
        
        // 5. Проверяем структурные элементы (annotation, poem, stanza, cite)
        if (isInStructuralElement(p)) {
            // Если processBlockElements == 0, то пропускаем
            if (processBlockElements == 0) return false;
            // Если processBlockElements == 1, то продолжаем проверки
        }
        
        // 6. Проверяем, не идет ли строка сразу после эпиграфа
        if (isAfterEpigraph(p)) return false;
        
        // 7. Проверяем соседство с картинками
        if (hasImageNearby(p)) return false;
        
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
    
    // Выводим статистику
    if (showStatistics == 1) {
        var msg = scriptName + "\n";
        msg += "ver. " + version + "\n\n";
        
        if (totalCount == 0) {
            msg += "Пустых строк (согласно настройкам) в обработанной области не найдено.\n\n";
        } else {
            msg += "✓ Обработано разделов:\n";
            if (mainCount > 0) {
                msg += "  • Основной раздел: " + mainCount + " замен\n";
            } else if (mainCount == 0 && (notesCount > 0 || commentsCount > 0)) {
                msg += "  • Основной раздел: 0 замен\n";
            }
            if (processNotesSection == 1) {
                if (notesCount > 0) {
                    msg += "  • Примечания: " + notesCount + " замен\n";
                } else if (notesCount == 0 && (mainCount > 0 || commentsCount > 0)) {
                    msg += "  • Примечания: 0 замен\n";
                }
            }
            if (processCommentsSection == 1) {
                if (commentsCount > 0) {
                    msg += "  • Комментарии: " + commentsCount + " замен\n";
                } else if (commentsCount == 0 && (mainCount > 0 || notesCount > 0)) {
                    msg += "  • Комментарии: 0 замен\n";
                }
            }
            if (totalCount > 0) {
                msg += "\n  Всего замен: " + totalCount + "\n\n";
            }
        }
        
        msg += "- Настройки обработки:\n";
        msg += "  • Обычные абзацы: ДА\n";
        if (processBlockElements == 1) {
            msg += "  • Структурные элементы (аннотации, стихи, цитаты): ДА\n";
        } else {
            msg += "  • Структурные элементы (аннотации, стихи, цитаты): НЕТ\n";
        }
        msg += "  • Заголовки и эпиграфы: НИКОГДА\n";
        msg += "  • Пустые строки после эпиграфов: НИКОГДА\n";
        msg += "  • Раздел history: НИКОГДА\n";
        msg += "  • Первая аннотация (до body): НИКОГДА\n";
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
        
        msg += "\nРежим обработки: ";
        if (hasSelection) {
            msg += "Выделение\n";
        } else {
            msg += "Весь документ\n";
        }
        
        if (totalCount > 0) {
            msg += "\n✓ Замен произведено: " + totalCount + "\n";
            msg += "\nВид подзаголовка: " + subtitleContent + "\n";
        }
        
        msg += "\nВремя обработки: " + timeStr + " сек";
        
        MsgBox(msg, "FBE скрипт");
    } else {
        // Тихий режим — выводим только если ничего не найдено
        if (totalCount == 0) {
            MsgBox(scriptName + "\nver. " + version + "\n\nПустых строк (согласно настройкам) в обработанной области не найдено.", "FBE скрипт");
        }
    }
    
    try {
        if (totalCount > 0) {
            window.external.SetStatusBarText(scriptName + " ver. " + version + ": заменено " + totalCount + " строк. Время: " + timeStr + " сек.");
        } else {
            window.external.SetStatusBarText(scriptName + " ver. " + version + ": пустых строк не найдено. Время: " + timeStr + " сек.");
        }
    }
    catch(e) {}
}
