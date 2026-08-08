// Скрипт "Удалить дубли пустых строк" для редактора FBE
// version 1.5
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для удаления дубликатов пустых строк (абзацев <P>) в fb2 документах,
// оставляя только одну пустую строку в каждой группе идущих подряд.
// При наличии выделения, скрипт работает с выделенным фрагментом,
// в противном случае - обрабатывается сразу весь документ.
// По умолчанию обрабатываются все разделы документа, включая разделы сносок и комментариев.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.5, 03.07.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Удалить дубли пустых строк";
    var version = "1.5";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1;
    
    // Обрабатывать основную аннотацию
    var processAnnotation = 1;
    
    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 1;
    
    // Обрабатывать раздел комментариев
    var processCommentsSection = 1;
    
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
    var annotationCount = 0;
    
    // Функция проверки, является ли абзац пустой строкой
    function isEmptyLine(p) {
        return re0.test(p.innerHTML.replace(re2, ""));
    }
    
    // Функция для определения раздела элемента
    function getSectionType(element) {
        var parent = element.parentNode;
        while (parent) {
            if (parent.nodeName == "DIV") {
                // Сначала проверяем body — он должен иметь приоритет над annotation
                if (parent.className == "body") {
                    var fbname = parent.getAttribute("fbname") || "";
                    if (fbname == "") return "main";
                    if (fbname == "notes") return "notes";
                    if (fbname == "comments") return "comments";
                    return "other";
                }
                // annotation проверяем только если не нашли body выше
                if (parent.className == "annotation") {
                    return "annotation";
                }
            }
            parent = parent.parentNode;
        }
        return "main";
    }
    
    // Функция проверки, можно ли обрабатывать элемент в данном разделе
    function canProcessElement(element) {
        var sectionType = getSectionType(element);
        
        if (sectionType == "main") return true;
        if (sectionType == "annotation" && processAnnotation == 1) return true;
        if (sectionType == "notes" && processNotesSection == 1) return true;
        if (sectionType == "comments" && processCommentsSection == 1) return true;
        
        return false;
    }
    
    // Функция проверки, содержится ли элемент внутри контейнера
    function isInsideContainer(element, container) {
        var parent = element;
        while (parent) {
            if (parent == container) return true;
            parent = parent.parentNode;
        }
        return false;
    }
    
    // Функция обработки контейнера (body, annotation или выделения)
    // Возвращает объект с количеством удалённых дублей по разделам
    function processContainer(container) {
        var result = { main: 0, notes: 0, comments: 0, annotation: 0 };
        
        var ptr = container;
        var endProcessing = false;
        var saveNode = null;
        var saveNext;
        
        // Собираем все P в массив
        var allP = [];
        
        while (!endProcessing && ptr) {
            saveNext = ptr;
            if (saveNext.firstChild && saveNext.nodeName != "P" && 
                !(saveNext.nodeName == "DIV" && saveNext.className == "table")
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
                allP[allP.length] = ptr;
            }
            ptr = saveNext;
        }
        
        // Обрабатываем массив, ищем группы пустых строк подряд
        var i = 0;
        while (i < allP.length) {
            if (isEmptyLine(allP[i]) && canProcessElement(allP[i])) {
                // Нашли пустую строку в разрешённом разделе
                var j = i + 1;
                while (j < allP.length && isEmptyLine(allP[j]) && canProcessElement(allP[j])) {
                    j++;
                }
                
                // Если нашли больше одной пустой строки подряд
                if (j - i > 1) {
                    var sectionType = getSectionType(allP[i]);
                    
                    // Удаляем лишние, оставляем первую (индекс i)
                    for (var k = i + 1; k < j; k++) {
                        allP[k].removeNode(true);
                        
                        if (sectionType == "main") {
                            result.main++;
                        } else if (sectionType == "notes") {
                            result.notes++;
                        } else if (sectionType == "comments") {
                            result.comments++;
                        } else if (sectionType == "annotation") {
                            result.annotation++;
                        }
                    }
                }
                
                i = j;
            } else {
                i++;
            }
        }
        
        return result;
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
        
        var trStart = document.selection.createRange();
        trStart.collapse(true);
        var blockStartEl = trStart.parentElement();
        
        var trEnd = document.selection.createRange();
        trEnd.collapse(false);
        var blockEndEl = trEnd.parentElement();
        
        // Ищем контейнер от blockStartEl
        var container = blockStartEl;
        while (container) {
            if (container.nodeName == "DIV" && 
                (container.className == "body" || container.className == "section" || container.className == "annotation")) {
                break;
            }
            container = container.parentNode;
        }
        
        // Если контейнер не найден, берём fbw_body
        if (!container || (container.nodeName == "DIV" && 
            container.className != "body" && container.className != "section" && container.className != "annotation")) {
            container = document.getElementById("fbw_body");
        } else {
            // Проверяем, находится ли blockEndEl внутри того же контейнера
            // Если нет — значит выделение охватывает несколько разделов, берём fbw_body
            if (!isInsideContainer(blockEndEl, container)) {
                container = document.getElementById("fbw_body");
            }
        }
        
        // Собираем ВСЕ P в контейнере (быстрый обход)
        var allP = [];
        var ptr = container;
        var endProcessing = false;
        var saveNode = null;
        var saveNext;
        
        while (!endProcessing && ptr) {
            saveNext = ptr;
            if (saveNext.firstChild && saveNext.nodeName != "P" && 
                !(saveNext.nodeName == "DIV" && saveNext.className == "table")
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
                allP[allP.length] = ptr;
            }
            ptr = saveNext;
        }
        
        // Определяем, какие P входят в выделение
        // P входит в выделение, если он между blockStartEl и blockEndEl
        var inRange = false;
        var selectedP = [];
        
        for (var idx = 0; idx < allP.length; idx++) {
            if (allP[idx] == blockStartEl || containsElement(allP[idx], blockStartEl)) {
                inRange = true;
            }
            if (inRange) {
                selectedP[selectedP.length] = allP[idx];
            }
            if (allP[idx] == blockEndEl || containsElement(allP[idx], blockEndEl)) {
                if (inRange) {
                    // Уже добавили этот P, выходим
                }
                inRange = false;
                break;
            }
        }
        
        // Если не нашли через containsElement, используем позиции в массиве
        if (selectedP.length == 0) {
            var startIdx = -1;
            var endIdx = -1;
            
            // Ищем blockStartEl или его родительский P
            var tempEl = blockStartEl;
            while (tempEl && tempEl.nodeName != "P") {
                tempEl = tempEl.parentNode;
            }
            if (tempEl) {
                for (var idx2 = 0; idx2 < allP.length; idx2++) {
                    if (allP[idx2] == tempEl) {
                        startIdx = idx2;
                        break;
                    }
                }
            }
            
            // Ищем blockEndEl или его родительский P
            tempEl = blockEndEl;
            while (tempEl && tempEl.nodeName != "P") {
                tempEl = tempEl.parentNode;
            }
            if (tempEl) {
                for (var idx3 = 0; idx3 < allP.length; idx3++) {
                    if (allP[idx3] == tempEl) {
                        endIdx = idx3;
                        break;
                    }
                }
            }
            
            if (startIdx >= 0 && endIdx >= startIdx) {
                for (var idx4 = startIdx; idx4 <= endIdx; idx4++) {
                    selectedP[selectedP.length] = allP[idx4];
                }
            }
        }
        
        // Функция проверки, содержит ли элемент искомый узел
        function containsElement(parentEl, targetEl) {
            if (!parentEl || !targetEl) return false;
            if (parentEl == targetEl) return true;
            // Проверяем всех потомков
            var children = parentEl.childNodes;
            for (var ci = 0; ci < children.length; ci++) {
                if (children[ci] == targetEl) return true;
                if (children[ci].nodeType == 1 && containsElement(children[ci], targetEl)) return true;
            }
            return false;
        }
        
        // Обрабатываем выбранные P
        var i = 0;
        while (i < selectedP.length) {
            if (selectedP[i].nodeName == "P" && isEmptyLine(selectedP[i]) && canProcessElement(selectedP[i])) {
                var j = i + 1;
                while (j < selectedP.length && selectedP[j].nodeName == "P" && isEmptyLine(selectedP[j]) && canProcessElement(selectedP[j])) {
                    j++;
                }
                
                if (j - i > 1) {
                    var sectionType = getSectionType(selectedP[i]);
                    
                    for (var k = i + 1; k < j; k++) {
                        selectedP[k].removeNode(true);
                        
                        if (sectionType == "main") {
                            mainCount++;
                        } else if (sectionType == "notes") {
                            notesCount++;
                        } else if (sectionType == "comments") {
                            commentsCount++;
                        } else if (sectionType == "annotation") {
                            annotationCount++;
                        }
                    }
                }
                i = j;
            } else {
                i++;
            }
        }
        
    } else {
        // ==================================================
        // РАБОТА СО ВСЕМ ДОКУМЕНТОМ
        // ==================================================
        
        // Обрабатываем аннотацию, если включено
        if (processAnnotation == 1) {
            var fbwBody = document.getElementById("fbw_body");
            if (fbwBody) {
                var fbwChildren = fbwBody.childNodes;
                for (var c = 0; c < fbwChildren.length; c++) {
                    var child = fbwChildren[c];
                    if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "annotation") {
                        var resAnn = processContainer(child);
                        annotationCount = resAnn.annotation;
                        break;
                    }
                }
            }
        }
        
        // Обрабатываем разделы body
        var bodyDivs = document.getElementsByTagName("DIV");
        for (var d = 0; d < bodyDivs.length; d++) {
            var div = bodyDivs[d];
            if (div.className == "body") {
                var fbname = div.getAttribute("fbname") || "";
                
                if (fbname == "") {
                    var res = processContainer(div);
                    mainCount = res.main;
                    notesCount = res.notes;
                    commentsCount = res.comments;
                } else if (fbname == "notes" && processNotesSection == 1) {
                    var res2 = processContainer(div);
                    notesCount = res2.notes;
                } else if (fbname == "comments" && processCommentsSection == 1) {
                    var res3 = processContainer(div);
                    commentsCount = res3.comments;
                }
            }
        }
    }
    
    window.external.EndUndoUnit(document);
    
    var Tf = new Date().getTime();
    var timeSec = (Tf - Ts) / 1000;
    var timeStr = timeSec.toFixed(3).replace(".", ",");
    
    var totalCount = mainCount + notesCount + commentsCount + annotationCount;
    
    // Формируем сообщение
    var msg = scriptName + "\n";
    msg += "ver. " + version + "\n\n";
    
    if (hasSelection) {
        msg += "РЕЖИМ ВЫДЕЛЕНИЯ\n";
    } else {
        msg += "РЕЖИМ ВЕСЬ ДОКУМЕНТ\n";
    }
    
    if (totalCount == 0) {
        msg += "\nДублей пустых строк не найдено.";
    } else {
        msg += "\n✓ Удалено дублей пустых строк: " + totalCount;
        
        // Единообразный вывод статистики для обоих режимов
        msg += "\n\n  • Основной раздел: " + mainCount;
        if (processAnnotation == 1) {
            msg += "\n  • Аннотация: " + annotationCount;
        }
        if (processNotesSection == 1) {
            msg += "\n  • Примечания: " + notesCount;
        }
        if (processCommentsSection == 1) {
            msg += "\n  • Комментарии: " + commentsCount;
        }
    }
    
    msg += "\n\nВремя выполнения: " + timeStr + " сек.";
    
    // Выводим сообщение всегда (и в тихом тоже, если не найдено)
    if (showStatistics == 1 || totalCount == 0) {
        MsgBox(msg, "FBE скрипт");
    }
    
    try {
        if (totalCount > 0) {
            window.external.SetStatusBarText(scriptName + ": удалено " + totalCount + " дублей. Время: " + timeStr + " сек.");
        } else {
            window.external.SetStatusBarText(scriptName + ": дублей не найдено. Время: " + timeStr + " сек.");
        }
    }
    catch(e) {}
}
