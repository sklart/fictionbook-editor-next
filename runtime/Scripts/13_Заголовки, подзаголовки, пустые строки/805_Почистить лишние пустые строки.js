// Скрипт "Почистить лишние пустые строки" для редактора FBE
// version 2.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для удаления лишних пустых строк (абзацев <P>) в fb2 документах.
// Удаляются дубликаты идущих подряд, пустые строки по соседству с заголовками,
// подзаголовками, цитатами, стихами, эпиграфами (где валидно), в началах и концах секций.
// При наличии выделения, скрипт работает с выделенным фрагментом,
// в противном случае - обрабатывается сразу весь документ.
// По умолчанию обрабатываются все разделы документа, включая разделы сносок и комментариев.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// Алгоритм чистки пустых строк взят из скрипта уважаемого тов. Sclex (528_Разметка подзаголовков, чистка пустых строк, удаление жирности и курсива в заголовках, v4.7)

// version 2.2, 03.07.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Почистить лишние пустые строки";
    var version = "2.2";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1;
    
    // Обрабатывать аннотацию
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
    
    // Счётчики дублей пустых строк
    var mainCountDup = 0;
    var notesCountDup = 0;
    var commentsCountDup = 0;
    var annotationCountDup = 0;
    
    // Счётчики чистки пустых строк
    var mainCountClean = 0;
    var notesCountClean = 0;
    var commentsCountClean = 0;
    var annotationCountClean = 0;
    
    // Детальные счётчики чистки
    var EmptyClearedEmpty = 0;
    var EmptyClearedCite = 0;
    var EmptyClearedPoem = 0;
    var EmptyClearedSubtitle = 0;
    var EmptyClearedSectionBegin = 0;
    var EmptyClearedSectionEnd = 0;
    var EmptyClearedInPoem = 0;
    var EmptyClearedTitleBegin = 0;
    var EmptyClearedTitleEnd = 0;
    var EmptyClearedTitleInside = 0;
    var EmptyClearedEpigraphBegin = 0;
    var EmptyClearedEpigraphEnd = 0;
    var EmptyClearedCiteBegin = 0;
    var EmptyClearedCiteEnd = 0;
    
    // Функция проверки, является ли абзац пустой строкой
    function isEmptyLine(p) {
        return re0.test(p.innerHTML.replace(re2, ""));
    }
    
    // Функция для определения раздела элемента
    function getSectionType(element) {
        var parent = element.parentNode;
        while (parent) {
            if (parent.nodeName == "DIV") {
                if (parent.className == "body") {
                    var fbname = parent.getAttribute("fbname") || "";
                    if (fbname == "") return "main";
                    if (fbname == "notes") return "notes";
                    if (fbname == "comments") return "comments";
                    return "other";
                }
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
    
    // Функция-обёртка для removeNode с подсчётом статистики
    function removeAndCount(element, reason) {
        var sectionType = getSectionType(element);
        element.removeNode(true);
        
        if (sectionType == "main") {
            mainCountClean++;
        } else if (sectionType == "notes") {
            notesCountClean++;
        } else if (sectionType == "comments") {
            commentsCountClean++;
        } else if (sectionType == "annotation") {
            annotationCountClean++;
        }
        
        switch(reason) {
            case "EmptyClearedEmpty": EmptyClearedEmpty++; break;
            case "EmptyClearedCite": EmptyClearedCite++; break;
            case "EmptyClearedPoem": EmptyClearedPoem++; break;
            case "EmptyClearedSubtitle": EmptyClearedSubtitle++; break;
            case "EmptyClearedSectionBegin": EmptyClearedSectionBegin++; break;
            case "EmptyClearedSectionEnd": EmptyClearedSectionEnd++; break;
            case "EmptyClearedInPoem": EmptyClearedInPoem++; break;
            case "EmptyClearedTitleBegin": EmptyClearedTitleBegin++; break;
            case "EmptyClearedTitleEnd": EmptyClearedTitleEnd++; break;
            case "EmptyClearedTitleInside": EmptyClearedTitleInside++; break;
            case "EmptyClearedEpigraphBegin": EmptyClearedEpigraphBegin++; break;
            case "EmptyClearedEpigraphEnd": EmptyClearedEpigraphEnd++; break;
            case "EmptyClearedCiteBegin": EmptyClearedCiteBegin++; break;
            case "EmptyClearedCiteEnd": EmptyClearedCiteEnd++; break;
        }
    }
    
    // ==================================================
    // ФУНКЦИЯ ПОДСЧЁТА ДУБЛЕЙ (только считает, не удаляет)
    // ==================================================
    function countDuplicates(container) {
        var result = { main: 0, notes: 0, comments: 0, annotation: 0 };
        
        var ptr = container;
        var endProcessing = false;
        var saveNode = null;
        var saveNext;
        
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
        
        var i = 0;
        while (i < allP.length) {
            if (isEmptyLine(allP[i]) && canProcessElement(allP[i])) {
                var j = i + 1;
                while (j < allP.length && isEmptyLine(allP[j]) && canProcessElement(allP[j])) {
                    j++;
                }
                
                if (j - i > 1) {
                    var sectionType = getSectionType(allP[i]);
                    var dupCount = j - i - 1;
                    
                    if (sectionType == "main") {
                        result.main += dupCount;
                    } else if (sectionType == "notes") {
                        result.notes += dupCount;
                    } else if (sectionType == "comments") {
                        result.comments += dupCount;
                    } else if (sectionType == "annotation") {
                        result.annotation += dupCount;
                    }
                }
                
                i = j;
            } else {
                i++;
            }
        }
        
        return result;
    }
    
    // ==================================================
    // ФУНКЦИЯ УДАЛЕНИЯ ДУБЛЕЙ ПУСТЫХ СТРОК
    // ==================================================
    function removeDuplicateEmpties(container) {
        var ptr = container;
        var endProcessing = false;
        var saveNode = null;
        var saveNext;
        
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
        
        var i = 0;
        while (i < allP.length) {
            if (isEmptyLine(allP[i]) && canProcessElement(allP[i])) {
                var j = i + 1;
                while (j < allP.length && isEmptyLine(allP[j]) && canProcessElement(allP[j])) {
                    j++;
                }
                
                if (j - i > 1) {
                    for (var k = i + 1; k < j; k++) {
                        // Проверка: не удаляем пустую строку, если она после эпиграфа в секции
                        var prevSibling = allP[k].previousSibling;
                        if (prevSibling && prevSibling.nodeName == "DIV" && prevSibling.className == "epigraph" &&
                            allP[k].parentNode && allP[k].parentNode.className == "section") {
                            continue;
                        }
                        
                        allP[k].removeNode(true);
                    }
                }
                
                i = j;
            } else {
                i++;
            }
        }
    }
    
    // ==================================================
    // ФУНКЦИЯ ЧИСТКИ ПУСТЫХ СТРОК (алгоритм Sclex v4.7)
    // ==================================================
    function cleanEmptyLines(container) {
        
        function removeEmptiesAtEnd(elemName) {
            var a3 = a5.lastChild;
            var go_more = true;
            var SavePrevA3;
            while (a3 != null && go_more) {
                SavePrevA3 = a3.previousSibling;
                if (a3.nodeName == "P" && isEmptyLine(a3) && canProcessElement(a3)) {
                    removeAndCount(a3, "EmptyCleared" + elemName.charAt(0).toUpperCase() + elemName.slice(1) + "End");
                } else {
                    go_more = false;
                }
                a3 = SavePrevA3;
            }
        }
        
        function removeEmptiesAtBegin(elemName) {
            var go_more = true;
            var a4 = savedFirstEmpty;
            var SaveNextA4;
            if (elemName == "section" && a4 && a4.previousSibling && a4.previousSibling.className == "image") return;
            while (a4 && go_more) {
                SaveNextA4 = a4.nextSibling;
                if (a4.nodeName == "P" && isEmptyLine(a4) && a4.parentNode != null && canProcessElement(a4)) {
                    removeAndCount(a4, "EmptyCleared" + elemName.charAt(0).toUpperCase() + elemName.slice(1) + "Begin");
                } else {
                    go_more = false;
                }
                a4 = SaveNextA4;
            }
        }
        
        var savedPtr = container.firstChild;
        if (savedPtr == null) return;
        
        var ptr = savedPtr;
        var a5 = container;
        var after_title = false;
        var flag_of_begin = true;
        var firstEmptyMemorized = false;
        var image_flag = false;
        var image_flag_2 = false;
        var savedFirstEmpty = null;
        
        while (ptr != null) {
            var SaveNextPtr = ptr.nextSibling;
            
            if (ptr.nodeName == "DIV" &&
                (ptr.className == "section" || ptr.className == "body" || ptr.className == "poem" ||
                 ptr.className == "stanza" || ptr.className == "cite" || ptr.className == "epigraph")) {
                cleanEmptyLines(ptr);
            }
            
            if (ptr.nodeName == "DIV" && ptr.className == "title") {
                var while_flag = true;
                while (while_flag) {
                    if (ptr.firstChild) {
                        if (isEmptyLine(ptr.firstChild) && canProcessElement(ptr.firstChild)) {
                            removeAndCount(ptr.firstChild, "EmptyClearedTitleBegin");
                        } else {
                            while_flag = false;
                        }
                    } else {
                        while_flag = false;
                    }
                }
                while_flag = true;
                while (while_flag) {
                    if (ptr.lastChild) {
                        if (isEmptyLine(ptr.lastChild) && canProcessElement(ptr.lastChild)) {
                            removeAndCount(ptr.lastChild, "EmptyClearedTitleEnd");
                        } else {
                            while_flag = false;
                        }
                    } else {
                        while_flag = false;
                    }
                }
                var ptrInTitle = ptr.lastChild;
                var savePrev;
                while (ptrInTitle != null) {
                    savePrev = ptrInTitle.previousSibling;
                    if (isEmptyLine(ptrInTitle) && canProcessElement(ptrInTitle)) {
                        removeAndCount(ptrInTitle, "EmptyClearedTitleInside");
                    }
                    ptrInTitle = savePrev;
                }
            }
            
            if (ptr.nodeName == "DIV" && ptr.className == "image") {
                if (flag_of_begin && !image_flag) {
                    flag_of_begin = false;
                    image_flag = true;
                    image_flag_2 = true;
                } else if (image_flag) {
                    image_flag = false;
                }
            }
            
            if (ptr.nodeName == "P" && ptr.parentNode.className == "section") {
                if (!isEmptyLine(ptr)) {
                    flag_of_begin = false;
                    image_flag = false;
                } else if (firstEmptyMemorized == false && (flag_of_begin || image_flag)) {
                    firstEmptyMemorized = true;
                    savedFirstEmpty = ptr;
                    flag_of_begin = false;
                }
            }
            
            if (!firstEmptyMemorized && flag_of_begin && ptr.nodeName == "P" &&
                (ptr.parentNode.className == "epigraph" || ptr.parentNode.className == "poem" ||
                 ptr.parentNode.className == "cite")) {
                if (isEmptyLine(ptr)) {
                    firstEmptyMemorized = true;
                    savedFirstEmpty = ptr;
                } else {
                    flag_of_begin = false;
                }
            }
            
            if (ptr.nodeName == "DIV" &&
                (ptr.className == "table" || ptr.className == "cite" || ptr.className == "poem")) {
                flag_of_begin = false;
            }
            
            if ((ptr.nodeName == "DIV" && (ptr.className == "poem" || ptr.className == "cite")) ||
                (ptr.nodeName == "P" && isEmptyLine(ptr) && canProcessElement(ptr))) {
                
                var a1 = ptr.previousSibling;
                var flag = true;
                while (a1 != null && flag) {
                    var SavePrev = a1.previousSibling;
                    if (a1.nodeName == "P" && isEmptyLine(a1) && canProcessElement(a1)) {
                        if (ptr.nodeName == "P") {
                            if (ptr.parentNode.nodeName == "DIV" && ptr.parentNode.className == "stanza") {
                                removeAndCount(a1, "EmptyClearedInPoem");
                            } else {
                                removeAndCount(a1, "EmptyClearedEmpty");
                            }
                        }
                        if (ptr.nodeName == "DIV" && ptr.className == "cite") {
                            removeAndCount(a1, "EmptyClearedCite");
                        }
                        if (ptr.nodeName == "DIV" && ptr.className == "poem") {
                            removeAndCount(a1, "EmptyClearedPoem");
                        }
                    } else {
                        flag = false;
                    }
                    a1 = SavePrev;
                }
                
                flag = true;
                var a2 = ptr.nextSibling;
                while (a2 != null && flag) {
                    var SaveNext = a2.nextSibling;
                    if (a2.nodeName == "P" && isEmptyLine(a2) && canProcessElement(a2)) {
                        SaveNextPtr = a2.nextSibling;
                        if (ptr.nodeName == "P") {
                            if (ptr.parentNode.nodeName == "DIV" && ptr.parentNode.className == "stanza") {
                                removeAndCount(a2, "EmptyClearedInPoem");
                            } else {
                                removeAndCount(a2, "EmptyClearedEmpty");
                            }
                        }
                        if (ptr.nodeName == "DIV" && ptr.className == "cite") {
                            removeAndCount(a2, "EmptyClearedCite");
                        }
                        if (ptr.nodeName == "DIV" && ptr.className == "poem") {
                            removeAndCount(a2, "EmptyClearedPoem");
                        }
                    } else {
                        flag = false;
                    }
                    a2 = SaveNext;
                }
                
                if (ptr.parentNode.nodeName == "DIV" && ptr.parentNode.className == "stanza" &&
                    ptr.nodeName == "P" && isEmptyLine(ptr) && canProcessElement(ptr)) {
                    removeAndCount(ptr, "EmptyClearedInPoem");
                }
            }
            
            if (ptr.nodeName == "P" && ptr.className == "subtitle") {
                var a1 = ptr.previousSibling;
                var flag = true;
                while (a1 != null && flag) {
                    var SavePrev = a1.previousSibling;
                    if (a1.nodeName == "P" && isEmptyLine(a1) && canProcessElement(a1)) {
                        removeAndCount(a1, "EmptyClearedSubtitle");
                    } else {
                        flag = false;
                    }
                    a1 = SavePrev;
                }
                flag = true;
                var a2 = ptr.nextSibling;
                while (a2 != null && flag) {
                    var SaveNext = a2.nextSibling;
                    if (a2.nodeName == "P" && isEmptyLine(a2) && canProcessElement(a2)) {
                        SaveNextPtr = a2.nextSibling;
                        removeAndCount(a2, "EmptyClearedSubtitle");
                    } else {
                        flag = false;
                    }
                    a2 = SaveNext;
                }
            }
            
            ptr = SaveNextPtr;
        }
        
        if (savedPtr.parentNode && savedPtr.parentNode.nodeName == "DIV" && savedPtr.parentNode.className == "section") {
            var a3 = a5.lastChild;
            if (!firstEmptyMemorized || (a3 != savedFirstEmpty && (!savedFirstEmpty.previousSibling ||
                savedFirstEmpty.previousSibling.className != "image")) || savedFirstEmpty.nextSibling) {
                removeEmptiesAtEnd("section");
            }
            a3 = a5.lastChild;
            if (firstEmptyMemorized && savedFirstEmpty.nextSibling) {
                if (image_flag_2 ?
                    savedFirstEmpty.nextSibling.className != "image" :
                    !(savedFirstEmpty.nextSibling.className == "image" && savedFirstEmpty.nextSibling.nextSibling &&
                      savedFirstEmpty.nextSibling.nextSibling.className == "image"
                     ) && !(savedFirstEmpty.nextSibling.className == "image" && !savedFirstEmpty.nextSibling.nextSibling)
                   ) {
                    removeEmptiesAtBegin("section");
                }
            }
        } else if (savedPtr.parentNode && savedPtr.parentNode.nodeName == "DIV" &&
            (savedPtr.parentNode.className == "epigraph" || savedPtr.parentNode.className == "cite")) {
            removeEmptiesAtEnd(savedPtr.parentNode.className);
            removeEmptiesAtBegin(savedPtr.parentNode.className);
        }
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
        
        var container = blockStartEl;
        while (container) {
            if (container.nodeName == "DIV" && 
                (container.className == "body" || container.className == "section" || container.className == "annotation")) {
                break;
            }
            container = container.parentNode;
        }
        
        if (!container || (container.nodeName == "DIV" && 
            container.className != "body" && container.className != "section" && container.className != "annotation")) {
            container = document.getElementById("fbw_body");
        } else {
            if (!isInsideContainer(blockEndEl, container)) {
                container = document.getElementById("fbw_body");
            }
        }
        
        // Фаза 0: подсчёт дублей (без удаления)
        var dupCount = countDuplicates(container);
        mainCountDup = dupCount.main;
        notesCountDup = dupCount.notes;
        commentsCountDup = dupCount.comments;
        annotationCountDup = dupCount.annotation;
        
        // Фаза 1: удаление дублей
        removeDuplicateEmpties(container);
        
        // Фаза 2: чистка пустых строк
        cleanEmptyLines(container);
        
    } else {
        // ==================================================
        // РАБОТА СО ВСЕМ ДОКУМЕНТОМ
        // ==================================================
        
        if (processAnnotation == 1) {
            var fbwBody = document.getElementById("fbw_body");
            if (fbwBody) {
                var fbwChildren = fbwBody.childNodes;
                for (var c = 0; c < fbwChildren.length; c++) {
                    var child = fbwChildren[c];
                    if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "annotation") {
                        var dupCountAnn = countDuplicates(child);
                        annotationCountDup = dupCountAnn.annotation;
                        removeDuplicateEmpties(child);
                        cleanEmptyLines(child);
                        break;
                    }
                }
            }
        }
        
        var bodyDivs = document.getElementsByTagName("DIV");
        for (var d = 0; d < bodyDivs.length; d++) {
            var div = bodyDivs[d];
            if (div.className == "body") {
                var fbname = div.getAttribute("fbname") || "";
                
                if (fbname == "") {
                    var dupCountMain = countDuplicates(div);
                    mainCountDup = dupCountMain.main;
                    notesCountDup = dupCountMain.notes;
                    commentsCountDup = dupCountMain.comments;
                    removeDuplicateEmpties(div);
                    cleanEmptyLines(div);
                } else if (fbname == "notes" && processNotesSection == 1) {
                    var dupCountNotes = countDuplicates(div);
                    notesCountDup = dupCountNotes.notes;
                    removeDuplicateEmpties(div);
                    cleanEmptyLines(div);
                } else if (fbname == "comments" && processCommentsSection == 1) {
                    var dupCountComm = countDuplicates(div);
                    commentsCountDup = dupCountComm.comments;
                    removeDuplicateEmpties(div);
                    cleanEmptyLines(div);
                }
            }
        }
    }
    
    window.external.EndUndoUnit(document);
    
    var Tf = new Date().getTime();
    var timeSec = (Tf - Ts) / 1000;
    var timeStr = timeSec.toFixed(3).replace(".", ",");
    
    var totalDup = mainCountDup + notesCountDup + commentsCountDup + annotationCountDup;
    var totalClean = mainCountClean + notesCountClean + commentsCountClean + annotationCountClean;
    var totalAll = totalDup + totalClean;
    
    // Формируем сообщение
    var msg = scriptName + "\n";
    msg += "ver. " + version + "\n\n";
    
    if (hasSelection) {
        msg += "РЕЖИМ ВЫДЕЛЕНИЯ\n";
    } else {
        msg += "РЕЖИМ ВЕСЬ ДОКУМЕНТ\n";
    }
    
    if (totalAll == 0) {
        msg += "\nЛишних пустых строк не найдено.";
    } else {
        msg += "\nУдалено дублей пустых строк: " + totalDup;
        msg += "\n";
        msg += "\nУдалено при чистке:";
        if (EmptyClearedEmpty > 0) msg += "\n  – из-за соседства с пустой строкой: " + EmptyClearedEmpty;
        if (EmptyClearedCite > 0) msg += "\n  – из-за соседства с цитатой: " + EmptyClearedCite;
        if (EmptyClearedPoem > 0) msg += "\n  – из-за соседства со стихом: " + EmptyClearedPoem;
        if (EmptyClearedSubtitle > 0) msg += "\n  – из-за соседства с подзаголовком: " + EmptyClearedSubtitle;
        if (EmptyClearedInPoem > 0) msg += "\n  – внутри стихов: " + EmptyClearedInPoem;
        if (EmptyClearedSectionBegin > 0) msg += "\n  – в начале раздела: " + EmptyClearedSectionBegin;
        if (EmptyClearedSectionEnd > 0) msg += "\n  – в конце раздела: " + EmptyClearedSectionEnd;
        if (EmptyClearedEpigraphBegin > 0) msg += "\n  – в начале эпиграфа: " + EmptyClearedEpigraphBegin;
        if (EmptyClearedEpigraphEnd > 0) msg += "\n  – в конце эпиграфа: " + EmptyClearedEpigraphEnd;
        if (EmptyClearedCiteBegin > 0) msg += "\n  – в начале цитаты: " + EmptyClearedCiteBegin;
        if (EmptyClearedCiteEnd > 0) msg += "\n  – в конце цитаты: " + EmptyClearedCiteEnd;
        if (EmptyClearedTitleBegin > 0) msg += "\n  – в начале заголовка: " + EmptyClearedTitleBegin;
        if (EmptyClearedTitleEnd > 0) msg += "\n  – в конце заголовка: " + EmptyClearedTitleEnd;
        if (EmptyClearedTitleInside > 0) msg += "\n  – внутри заголовка: " + EmptyClearedTitleInside;
        msg += "\n  ----------------------------";
        msg += "\n  Всего при чистке: " + totalClean;
        msg += "\n";
        msg += "\n✓ Всего удалено: " + totalAll;
        
        msg += "\n\nПо разделам:";
        msg += "\n  • Основной раздел: " + (mainCountDup + mainCountClean);
        if (processAnnotation == 1) {
            msg += "\n  • Аннотация: " + (annotationCountDup + annotationCountClean);
        }
        if (processNotesSection == 1) {
            msg += "\n  • Примечания: " + (notesCountDup + notesCountClean);
        }
        if (processCommentsSection == 1) {
            msg += "\n  • Комментарии: " + (commentsCountDup + commentsCountClean);
        }
    }
    
    msg += "\n\nВремя выполнения: " + timeStr + " сек.";
    
    if (showStatistics == 1 || totalAll == 0) {
        MsgBox(msg, "FBE скрипт");
    }
    
    try {
        if (totalAll > 0) {
            window.external.SetStatusBarText(scriptName + ": удалено " + totalAll + " строк. Время: " + timeStr + " сек.");
        } else {
            window.external.SetStatusBarText(scriptName + ": лишних строк не найдено. Время: " + timeStr + " сек.");
        }
    }
    catch(e) {}
}
