// Скрипт "Вставить пустую блочную иллюстрацию по месту курсора" для редактора FBE
// version 1.5
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для вставки пустой блочной иллюстрации (пустышки) в fb2 документах
// по месту текущего положения курсора или выделения.
// Обрабатываются все разделы документа, включая сноски и комментарии.
// Работает с обычными абзацами, а также с размеченными подзаголовками, заголовками, эпиграфами,
// цитатами, стихами, аннотациями, таблицами и существующими иллюстрациями.
// Не работает в первой аннотации документа и в истории (history).
// Если курсор или выделение находится внутри блочного DIV элемента,
// то вставка иллюстраций осуществляется до или после такого элемента, согласно настроек.
// Настраивается вставка до или после текущего элемента, а также добавление пустых строк.
// При вставке пустых иллюстраций, скрипт создаёт секции там, где это необходимо для валидности fb2.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.5, 29.06.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Вставить пустую блочную иллюстрацию по месту курсора";
    var version = "1.5";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать сообщения, 0 - не показывать (тихий режим)
    var showStatistics = 0; // По умолчанию тихий режим
    
    // Настройки вставки пустых иллюстраций:
    // Вставлять пустую иллюстрацию перед текущим элементом = 0
    // Вставлять пустую иллюстрацию после текущего элемента = 1
    var processParagraphs = 1;
    
    // Настройки добавления пустых строк при вставке пустых иллюстраций:
    // Не добавлять пустые строки = 0
    // Вставлять пустую строку только перед вставляемой иллюстрацией = 1
    // Вставлять пустую строку только после вставляемой иллюстрации = 2
    // Вставлять пустую строку перед и после вставляемой пустой иллюстрации = 3
    var processEmptyLines = 2;
    
    // Настройка вставки пустой строки между соседними иллюстрациями:
    // 0 - не вставлять пустую строку между соседними иллюстрациями
    // 1 - вставлять пустую строку между соседними иллюстрациями
    var insertLineBetweenImages = 1;
    
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
    } catch(e) {
        nbspChar = String.fromCharCode(160);
        nbspEntity = "&nbsp;";
    }
    
    // Регулярное выражение для проверки пустой строки (как у Sclex'а)
    var emptyLineRegExp = new RegExp("^[ " + nbspChar + "]*?$", "");
    
    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================
    
    // Проверка, является ли элемент пустой строкой
    function isLineEmpty(element) {
        if (!element || element.nodeName != "P") return false;
        return (element.innerText.search(emptyLineRegExp) >= 0);
    }
    
    // Создание и вставка пустой строки с правильным inflateBlock
    function insertEmptyLine(parent, beforeElement) {
        var p = document.createElement("P");
        if (beforeElement) {
            parent.insertBefore(p, beforeElement);
        } else {
            parent.appendChild(p);
        }
        window.external.inflateBlock(p) = true;
        return p;
    }
    
    // Создание пустой блочной иллюстрации (без ссылки на файл)
    function createEmptyImage() {
        var imageDiv = document.createElement("DIV");
        imageDiv.className = "image";
        imageDiv.setAttribute("onresizestart", "return false");
        imageDiv.setAttribute("contentEditable", "false");
        imageDiv.setAttribute("href", "#undefined");
        
        var img = document.createElement("IMG");
        img.src = "fbw-internal:#undefined";
        imageDiv.appendChild(img);
        
        return imageDiv;
    }
    
    // Получение предыдущего значащего элемента (пропуская текстовые узлы)
    function getPrevSibling(element) {
        if (!element) return null;
        var prev = element.previousSibling;
        while (prev && prev.nodeType == 3) {
            prev = prev.previousSibling;
        }
        return prev;
    }
    
    // Получение следующего значащего элемента (пропуская текстовые узлы)
    function getNextSibling(element) {
        if (!element) return null;
        var next = element.nextSibling;
        while (next && next.nodeType == 3) {
            next = next.nextSibling;
        }
        return next;
    }
    
    // Получение следующего узла (для обхода дерева)
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
    
    // Получение следующего абзаца P
    function getNextP(el) {
        var savedEl = el;
        while (el && (el.nodeName != "P" || el == savedEl))
            el = getNextNode(el);
        return el;
    }
    
    // Проверка: является ли элемент DIV с указанным классом
    function isDivClass(element, className) {
        return element && element.nodeName == "DIV" && element.className == className;
    }
    
    // Проверка: является ли элемент контейнерным DIV (cite, poem, epigraph и т.д.)
    function isContainerDiv(element) {
        if (!element || element.nodeName != "DIV") return false;
        var cls = element.className || "";
        return cls == "cite" || cls == "poem" || cls == "epigraph" || 
               cls == "image" || cls == "table" || cls == "title" || cls == "annotation";
    }
    
    // Проверка: является ли элемент картинкой
    function isImageElement(element) {
        return isDivClass(element, "image");
    }
    
    // Поиск ближайшего родительского контейнера (section, title, epigraph, cite и т.д.)
    function findParentContainer(element) {
        var current = element;
        while (current && current.nodeName != "BODY") {
            if (current.nodeName == "DIV") {
                var cls = current.className || "";
                if (cls == "section" || cls == "title" || cls == "epigraph" || 
                    cls == "cite" || cls == "poem" || cls == "stanza" || 
                    cls == "table" || cls == "annotation" || cls == "image") {
                    return current;
                }
            }
            current = current.parentNode;
        }
        return null;
    }
    
    // Поиск запретной зоны (аннотация или история до основного раздела)
    function findForbiddenZone(element) {
        var current = element;
        while (current && current.nodeName != "BODY") {
            if (current.nodeName == "DIV") {
                if (current.className == "annotation") return "annotation";
                if (current.className == "history") return "history";
                if (current.className == "body") return null;
            }
            current = current.parentNode;
        }
        return null;
    }
    
    // Проверка: содержит ли секция ТОЛЬКО картинки и пустые строки
    function isImageOnlySection(section) {
        if (!isDivClass(section, "section")) return false;
        
        var children = section.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 3) continue;
            if (child.nodeType != 1) return false;
            
            if (child.nodeName == "DIV" && child.className == "image") continue;
            if (child.nodeName == "P" && isLineEmpty(child)) continue;
            
            return false;
        }
        
        return true;
    }
    
    // Поиск самой глубокой вложенной секции (для правильной вставки в конец)
    function findDeepestSection(section) {
        if (!isDivClass(section, "section")) return section;
        
        var lastChild = section.lastChild;
        
        while (lastChild) {
            if (lastChild.nodeType == 3) {
                lastChild = lastChild.previousSibling;
                continue;
            }
            if (isDivClass(lastChild, "section")) {
                return findDeepestSection(lastChild);
            }
            return section;
        }
        
        return section;
    }
    
    // Поиск родительского cite или poem (учитывает stanza внутри poem)
    function findCiteOrPoemParent(element) {
        var current = element.parentNode;
        while (current && current.nodeName != "BODY") {
            if (current.nodeName == "DIV") {
                var cls = current.className || "";
                if (cls == "cite" || cls == "poem") return current;
                if (cls == "section") return null;
            }
            current = current.parentNode;
        }
        return null;
    }
    
    // Удаление всех пустых строк в заданном направлении от элемента
    function removeEmptyLines(ptr, directionFn) {
        var emptyPtr = directionFn(ptr);
        while (emptyPtr && emptyPtr.nodeName == "P" && isLineEmpty(emptyPtr)) {
            var nextEmpty = directionFn(emptyPtr);
            emptyPtr.removeNode(true);
            emptyPtr = nextEmpty;
        }
    }
    
    // Проверка: нужна ли пустая строка ПЕРЕД элементом
    function needsEmptyLineBefore(element) {
        var prev = getPrevSibling(element);
        if (!prev) {
            var parent = element.parentNode;
            if (isDivClass(parent, "body")) return false;
            return isDivClass(parent, "section");
        }
        if (isContainerDiv(prev)) return true;
        if (isDivClass(prev, "section")) return false;
        return true;
    }
    
    // Проверка: нужна ли пустая строка ПОСЛЕ элемента
    function needsEmptyLineAfter(element) {
        var next = getNextSibling(element);
        if (!next) {
            var parent = element.parentNode;
            if (isDivClass(parent, "body")) return false;
            return isDivClass(parent, "section");
        }
        if (isContainerDiv(next)) return true;
        if (isDivClass(next, "section")) return false;
        return true;
    }
    
    // Проверка: находится ли элемент прямо в body (не в section)
    function isInBody(element) {
        if (!element) return false;
        var parent = element.parentNode;
        return isDivClass(parent, "body");
    }
    
    // Прокрутка к элементу для удобного просмотра
    function scrollToElement(element) {
        if (!element) return;
        try {
            var rect = element.getBoundingClientRect();
            if (!rect) return;
            var correction = (rect.bottom - document.documentElement.clientHeight / 2);
            window.scrollBy(0, correction);
        } catch(e) {}
    }
    
    // ==================================================
    // ОСНОВНАЯ ЛОГИКА
    // ==================================================
    
    // Получаем текущее выделение
    var sel = document.selection;
    if (!sel) {
        if (showStatistics) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Ошибка. Не удалось получить выделение.");
        }
        return;
    }
    
    var currentElement = null;
    
    // Определяем текущий элемент (Control Range для картинок, иначе текстовое выделение)
    if (sel.type && sel.type == "Control") {
        var controlRange = sel.createRange();
        if (controlRange && controlRange.length > 0) {
            currentElement = controlRange.item(0);
        }
    } else {
        // Проверяем, не в поле ли ввода
        var tr = document.selection.createRange();
        if (tr.parentElement().nodeName == "TEXTAREA" || tr.parentElement().nodeName == "INPUT") {
            if (showStatistics) {
                MsgBox(scriptName + "\nver. " + version + "\n\n" +
                       "Ошибка. Выделение в поле ввода, а не в тексте книги.");
            }
            return;
        }
        
        // Вставляем маркеры для определения границ выделения
        var rndm = Math.round(Math.random() * 100000).toString();
        var startId = "BlockStart" + rndm;
        var endId = "BlockEnd" + rndm;
        var markerTagName = "I";
        
        var trStart = document.selection.createRange();
        var trEnd = trStart.duplicate();
        trStart.collapse(true);
        trStart.pasteHTML("<" + markerTagName + " id=" + startId + "></" + markerTagName + ">");
        trEnd.collapse(false);
        trEnd.pasteHTML("<" + markerTagName + " id=" + endId + "></" + markerTagName + ">");
        
        var blockStartNode = document.getElementById(startId);
        var blockEndNode = document.getElementById(endId);
        
        if (!blockStartNode || !blockEndNode) {
            return;
        }
        
        // Находим начальный абзац
        var blockStartEl = blockStartNode;
        while (blockStartEl && blockStartEl.nodeName != "BODY" && blockStartEl.nodeName != "P") {
            blockStartEl = blockStartEl.parentNode;
        }
        
        if (!blockStartEl || blockStartEl.nodeName == "BODY") {
            if (blockStartNode) blockStartNode.removeNode(true);
            if (blockEndNode) blockEndNode.removeNode(true);
            return;
        }
        
        // Находим конечный абзац (с учётом полного выделения)
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
            if (blockStartNode) blockStartNode.removeNode(true);
            if (blockEndNode) blockEndNode.removeNode(true);
            return;
        }
        
        // Проверка запретных зон (аннотация, история)
        var forbiddenZone = findForbiddenZone(blockStartEl);
        if (forbiddenZone == "annotation") {
            if (blockStartNode) blockStartNode.removeNode(true);
            if (blockEndNode) blockEndNode.removeNode(true);
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Вставка иллюстраций в аннотацию не производится.");
            return;
        }
        if (forbiddenZone == "history") {
            if (blockStartNode) blockStartNode.removeNode(true);
            if (blockEndNode) blockEndNode.removeNode(true);
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Вставка иллюстраций в хистори не производится.");
            return;
        }
        
        // Собираем все абзацы от начала до конца выделения
        var psArray = [];
        var ptr = blockStartEl;
        while (ptr) {
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
        
        // Проверка: если выделено больше одного абзаца
        if (psArray.length > 1) {
            var parentEl = psArray[0].parentNode;
            var sameParent = true;
            for (var i = 1; i < psArray.length; i++) {
                if (psArray[i].parentNode != parentEl) {
                    sameParent = false;
                    break;
                }
            }
            
            if (!sameParent) {
                if (blockStartNode) blockStartNode.removeNode(true);
                if (blockEndNode) blockEndNode.removeNode(true);
                
                MsgBox(scriptName + "\nver. " + version + "\n\n" +
                       "Должен быть выделен один блочный элемент или один обычный абзац.");
                return;
            }
            
            var firstContainer = findParentContainer(psArray[0]);
            var lastContainer = findParentContainer(psArray[psArray.length - 1]);
            
            if (firstContainer != lastContainer) {
                if (blockStartNode) blockStartNode.removeNode(true);
                if (blockEndNode) blockEndNode.removeNode(true);
                
                MsgBox(scriptName + "\nver. " + version + "\n\n" +
                       "Должен быть выделен один блочный элемент или один обычный абзац.");
                return;
            }
            
            // Если абзацы в section (не в контейнере) — ругаемся
            if (firstContainer && isDivClass(firstContainer, "section")) {
                if (blockStartNode) blockStartNode.removeNode(true);
                if (blockEndNode) blockEndNode.removeNode(true);
                
                MsgBox(scriptName + "\nver. " + version + "\n\n" +
                       "Должен быть выделен один блочный элемент или один обычный абзац.");
                return;
            }
            // Если в контейнере (cite, epigraph, poem) — разрешаем
        }
        
        currentElement = blockStartEl;
        
        // Удаляем маркеры
        if (blockStartNode) blockStartNode.removeNode(true);
        if (blockEndNode) blockEndNode.removeNode(true);
    }
    
    if (!currentElement) {
        if (showStatistics) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Ошибка. Не удалось определить текущий элемент.");
        }
        return;
    }
    
    // Проверка на поле ввода (для Control Range)
    if (currentElement.nodeName == "TEXTAREA" || currentElement.nodeName == "INPUT") {
        if (showStatistics) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Ошибка. Выделение в поле ввода, а не в тексте книги.");
        }
        return;
    }
    
    // Проверка: картинка в body перед заголовком (только одна допустима)
    if (isImageElement(currentElement) && isInBody(currentElement)) {
        var nextAfterImage = getNextSibling(currentElement);
        if (nextAfterImage && isDivClass(nextAfterImage, "title")) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Перед заголовком книги уже есть иллюстрация.\n" +
                   "Допустима только одна иллюстрация перед заголовком.");
            return;
        }
    }
    
    // ==================================================
    // ОПРЕДЕЛЯЕМ ЦЕЛЕВОЙ ЭЛЕМЕНТ ДЛЯ ВСТАВКИ
    // ==================================================
    
    var targetContainer = null;   // Элемент, относительно которого вставляем
    var targetType = "";          // Тип: paragraph, subtitle, title, epigraph, cite, poem, image, annotation, table
    
    // Проверяем, не картинка ли уже выделена
    if (isDivClass(currentElement, "image")) {
        targetContainer = currentElement;
        targetType = "image";
    }
    
    // Поднимаемся вверх по дереву в поисках контейнера
    if (!targetContainer) {
        var searchEl = currentElement;
        while (searchEl && searchEl.nodeName != "BODY") {
            if (searchEl.nodeName == "DIV") {
                var cls = searchEl.className || "";
                // stanza всегда внутри poem — поднимаем до poem
                if (cls == "stanza") {
                    var poemParent = searchEl.parentNode;
                    if (isDivClass(poemParent, "poem")) {
                        targetContainer = poemParent;
                        targetType = "poem";
                        break;
                    }
                }
                // Остальные контейнеры
                if (cls == "title" || cls == "epigraph" || cls == "cite" || 
                    cls == "poem" || cls == "table" || cls == "annotation" || cls == "image") {
                    targetContainer = searchEl;
                    targetType = cls;
                    break;
                }
            }
            // Подзаголовок
            if (searchEl.nodeName == "P" && searchEl.className == "subtitle") {
                targetContainer = searchEl;
                targetType = "subtitle";
                break;
            }
            // Обычный абзац
            if (searchEl.nodeName == "P" && !targetContainer) {
                targetContainer = searchEl;
                targetType = "paragraph";
            }
            searchEl = searchEl.parentNode;
        }
    }
    
    if (!targetContainer) {
        targetContainer = currentElement;
        targetType = "other";
    }
    
    // Подзаголовок или обычный абзац внутри цитаты/стиха → поднимаем до cite/poem
    if ((targetType == "subtitle" || targetType == "paragraph") && targetContainer.nodeName == "P") {
        var containerParent = findCiteOrPoemParent(targetContainer);
        if (containerParent) {
            targetContainer = containerParent;
            targetType = containerParent.className;
        }
    }
    
    // Заголовок body + эпиграф после → притворяемся что курсор в эпиграфе
    if (targetType == "title" && processParagraphs == 1) {
        var titleParent = targetContainer.parentNode;
        if (isDivClass(titleParent, "body")) {
            var nextAfterTitle = getNextSibling(targetContainer);
            if (nextAfterTitle && isDivClass(nextAfterTitle, "epigraph")) {
                targetContainer = nextAfterTitle;
                targetType = "epigraph";
            }
        }
    }
    
    // ==================================================
    // ЗАПРЕТ ВСТАВКИ ПЕРЕД ЭПИГРАФОМ
    // ==================================================
    
    if (targetType == "epigraph" && processParagraphs == 0) {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "Вставка перед эпиграфом не допускается.\n" +
               "Эпиграф всегда должен быть первым элементом после заголовка.");
        return;
    }
    
    // Начинаем блок отмены действий
    window.external.BeginUndoUnit(document, scriptName);
    
    try {
        window.external.SetStatusBarText("Вставляем пустую иллюстрацию...");
    } catch(e) {}
    
    var startTime = new Date().getTime();
    
    // ==================================================
    // ОПРЕДЕЛЯЕМ ТОЧКУ ВСТАВКИ
    // ==================================================
    
    var insertionPoint = null;      // Родительский элемент, куда вставляем
    var insertBeforeElement = null; // Элемент, перед которым вставляем (null = в конец)
    
    if (targetType == "epigraph") {
        // === ЭПИГРАФ ===
        var epiParent = targetContainer.parentNode;
        var epiInSection = isDivClass(epiParent, "section");
        
        if (!epiInSection && processParagraphs == 1) {
            // Эпиграф вне секции, вставка ПОСЛЕ
            
            // Проверяем, есть ли следующие эпиграфы подряд
            var nextEl = getNextSibling(targetContainer);
            var followingEpigraphs = [];
            while (nextEl && isDivClass(nextEl, "epigraph")) {
                followingEpigraphs.push(nextEl);
                nextEl = getNextSibling(nextEl);
            }
            
            var afterAllEpigraphs = nextEl;
            
            if (followingEpigraphs.length > 0) {
                // Несколько эпиграфов подряд — оборачиваем каждый в свою секцию
                var newSection = document.createElement("DIV");
                newSection.className = "section";
                
                targetContainer.parentNode.removeChild(targetContainer);
                newSection.appendChild(targetContainer);
                
                epiParent.insertBefore(newSection, followingEpigraphs[0]);
                
                insertionPoint = newSection;
                insertBeforeElement = null;
                
                // Оборачиваем остальные эпиграфы
                for (var i = 0; i < followingEpigraphs.length; i++) {
                    var epi = followingEpigraphs[i];
                    var wrapSection = document.createElement("DIV");
                    wrapSection.className = "section";
                    
                    epi.parentNode.removeChild(epi);
                    wrapSection.appendChild(epi);
                    
                    insertEmptyLine(wrapSection, null);
                    
                    if (afterAllEpigraphs) {
                        epiParent.insertBefore(wrapSection, afterAllEpigraphs);
                    } else {
                        epiParent.appendChild(wrapSection);
                    }
                }
                
            } else {
                // Один эпиграф — создаём секцию
                var newSection = document.createElement("DIV");
                newSection.className = "section";
                
                var epiNext = targetContainer.nextSibling;
                targetContainer.parentNode.removeChild(targetContainer);
                newSection.appendChild(targetContainer);
                
                if (epiNext) {
                    epiParent.insertBefore(newSection, epiNext);
                } else {
                    epiParent.appendChild(newSection);
                }
                
                insertionPoint = newSection;
                insertBeforeElement = targetContainer.nextSibling;
            }
            
        } else if (epiInSection && processParagraphs == 1) {
            // Эпиграф внутри секции — вставка после него
            insertionPoint = epiParent;
            insertBeforeElement = targetContainer.nextSibling;
        }
        
    } else if (targetType == "title") {
        // === ЗАГОЛОВОК ===
        var titleParent = targetContainer.parentNode;
        var titleInBody = isDivClass(titleParent, "body");
        var titleInSection = isDivClass(titleParent, "section");
        
        if (titleInBody && processParagraphs == 1) {
            // Заголовок body, вставка ПОСЛЕ
            var nextAfterTitle = getNextSibling(targetContainer);
            
            if (nextAfterTitle && isDivClass(nextAfterTitle, "section") && isImageOnlySection(nextAfterTitle)) {
                // Уже есть секция с картинками — вставляем в неё
                insertionPoint = nextAfterTitle;
                insertBeforeElement = null;
            } else {
                // Создаём новую секцию
                var newSection = document.createElement("DIV");
                newSection.className = "section";
                
                if (nextAfterTitle) {
                    titleParent.insertBefore(newSection, nextAfterTitle);
                } else {
                    titleParent.appendChild(newSection);
                }
                
                insertionPoint = newSection;
                insertBeforeElement = null;
            }
            
        } else if (titleInBody && processParagraphs == 0) {
            // Заголовок body, вставка ПЕРЕД — только одна картинка, без секции, без ПС
            var prevBeforeTitle = getPrevSibling(targetContainer);
            if (prevBeforeTitle && isImageElement(prevBeforeTitle)) {
                window.external.EndUndoUnit(document);
                MsgBox(scriptName + "\nver. " + version + "\n\n" +
                       "Перед заголовком книги уже есть иллюстрация.\n" +
                       "Допустима только одна иллюстрация перед заголовком.");
                return;
            }
            
            insertionPoint = titleParent;
            insertBeforeElement = targetContainer;
            
        } else if (titleInSection && processParagraphs == 1) {
            // Заголовок секции, вставка ПОСЛЕ
            var nextAfterTitle = getNextSibling(targetContainer);
            
            if (nextAfterTitle && isDivClass(nextAfterTitle, "epigraph")) {
                // После заголовка эпиграф — вставляем после эпиграфа
                insertionPoint = titleParent;
                insertBeforeElement = getNextSibling(nextAfterTitle);
            } else if (nextAfterTitle && isDivClass(nextAfterTitle, "section")) {
                // После заголовка вложенная секция — создаём новую секцию перед ней
                var newSection = document.createElement("DIV");
                newSection.className = "section";
                
                titleParent.insertBefore(newSection, nextAfterTitle);
                
                insertionPoint = newSection;
                insertBeforeElement = null;
            } else {
                // После заголовка обычное содержимое — вставляем в начало секции
                insertionPoint = titleParent;
                insertBeforeElement = nextAfterTitle;
            }
            
        } else if (titleInSection && processParagraphs == 0) {
            // Заголовок секции, вставка ПЕРЕД
            var prevBeforeTitle = getPrevSibling(targetContainer);
            
            if (prevBeforeTitle && isDivClass(prevBeforeTitle, "epigraph")) {
                // Перед заголовком эпиграф — создаём новую секцию перед всей секцией
                var newSection = document.createElement("DIV");
                newSection.className = "section";
                
                titleParent.parentNode.insertBefore(newSection, titleParent);
                
                insertionPoint = newSection;
                insertBeforeElement = null;
            } else if (prevBeforeTitle) {
                // Перед заголовком что-то есть — вставляем перед ним
                insertionPoint = titleParent;
                insertBeforeElement = targetContainer;
            } else {
                // Перед заголовком ничего нет — ищем предыдущую секцию
                var prevSection = getPrevSibling(titleParent);
                
                if (prevSection && isDivClass(prevSection, "section")) {
                    // Вставляем в конец самой глубокой секции предыдущей секции
                    var deepestSection = findDeepestSection(prevSection);
                    insertionPoint = deepestSection;
                    insertBeforeElement = null;
                } else {
                    // Предыдущей секции нет — создаём новую
                    var newSection = document.createElement("DIV");
                    newSection.className = "section";
                    
                    titleParent.parentNode.insertBefore(newSection, titleParent);
                    
                    insertionPoint = newSection;
                    insertBeforeElement = null;
                }
            }
            
        } else {
            insertionPoint = titleParent;
            insertBeforeElement = targetContainer;
        }
        
    } else if (targetType == "image") {
        // === КАРТИНКА ===
        insertionPoint = targetContainer.parentNode;
        if (processParagraphs == 0) {
            insertBeforeElement = targetContainer;
        } else {
            insertBeforeElement = targetContainer.nextSibling;
        }
        
    } else {
        // === ОБЫЧНЫЙ АБЗАЦ, ПОДЗАГОЛОВОК, ЦИТАТА, СТИХ, АННОТАЦИЯ, ТАБЛИЦА ===
        var contParent = targetContainer.parentNode;
        
        if (processParagraphs == 0) {
            insertionPoint = contParent;
            insertBeforeElement = targetContainer;
        } else {
            insertionPoint = contParent;
            insertBeforeElement = targetContainer.nextSibling;
        }
    }
    
    if (!insertionPoint) {
        window.external.EndUndoUnit(document);
        if (showStatistics) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Ошибка. Не удалось определить место для вставки.");
        }
        return;
    }
    
    // ==================================================
    // ПРОВЕРКА: ДВЕ КАРТИНКИ ВНЕ СЕКЦИИ — НУЖНА СЕКЦИЯ
    // ==================================================
    
    var needSectionForImages = false;
    if (targetType == "image") {
        var imgParent = targetContainer.parentNode;
        if (isDivClass(imgParent, "body")) {
            needSectionForImages = true;
        }
    }
    
    if (needSectionForImages) {
        var newSection = document.createElement("DIV");
        newSection.className = "section";
        
        var existingImage = targetContainer;
        var imageNext = existingImage.nextSibling;
        existingImage.parentNode.removeChild(existingImage);
        newSection.appendChild(existingImage);
        
        if (imageNext) {
            insertionPoint.insertBefore(newSection, imageNext);
        } else {
            insertionPoint.appendChild(newSection);
        }
        
        insertionPoint = newSection;
        targetContainer = existingImage;
        insertBeforeElement = existingImage.nextSibling;
    }
    
    // ==================================================
    // ВСТАВКА КАРТИНКИ
    // ==================================================
    
    var newImage = createEmptyImage();
    
    if (insertBeforeElement) {
        insertionPoint.insertBefore(newImage, insertBeforeElement);
    } else {
        insertionPoint.appendChild(newImage);
    }
    
    // ==================================================
    // ОБРАБОТКА ПУСТЫХ СТРОК
    // ==================================================
    
    var isInBodyFlag = isDivClass(insertionPoint, "body");
    
    // Сначала удаляем все лишние пустые строки с нужных сторон
    if (processEmptyLines == 1 || processEmptyLines == 3) {
        removeEmptyLines(newImage, getPrevSibling);
    }
    if (processEmptyLines == 2 || processEmptyLines == 3) {
        removeEmptyLines(newImage, getNextSibling);
    }
    if (processEmptyLines == 0 && needSectionForImages) {
        removeEmptyLines(newImage, getPrevSibling);
        removeEmptyLines(newImage, getNextSibling);
    }
    
    // Потом добавляем по одной, где нужно (в body не добавляем)
    if (processEmptyLines == 1) {
        if (!isInBodyFlag && needsEmptyLineBefore(newImage)) {
            insertEmptyLine(insertionPoint, newImage);
        }
    } else if (processEmptyLines == 2) {
        if (!isInBodyFlag && needsEmptyLineAfter(newImage)) {
            insertEmptyLine(insertionPoint, getNextSibling(newImage));
        }
    } else if (processEmptyLines == 3) {
        if (!isInBodyFlag) {
            if (needsEmptyLineBefore(newImage)) {
                insertEmptyLine(insertionPoint, newImage);
            }
            if (needsEmptyLineAfter(newImage)) {
                insertEmptyLine(insertionPoint, getNextSibling(newImage));
            }
        }
    } else if (processEmptyLines == 0 && needSectionForImages) {
        // В секции с картинками обязательна хотя бы одна пустая строка
        var hasLine = false;
        var child = insertionPoint.firstChild;
        while (child) {
            if (child.nodeName == "P" && isLineEmpty(child)) {
                hasLine = true;
                break;
            }
            child = child.nextSibling;
        }
        if (!hasLine) {
            insertEmptyLine(insertionPoint, null);
        }
    }
    
    // ==================================================
    // ПУСТАЯ СТРОКА МЕЖДУ СОСЕДНИМИ КАРТИНКАМИ
    // ==================================================
    
    if (insertLineBetweenImages == 1 && !isInBodyFlag) {
        var neighborImage = null;
        var direction = "";
        
        if (processParagraphs == 0) {
            neighborImage = getNextSibling(newImage);
            direction = "after";
        } else {
            neighborImage = getPrevSibling(newImage);
            direction = "before";
        }
        
        if (neighborImage && isImageElement(neighborImage)) {
            var hasLineBetween = false;
            
            if (direction == "after") {
                var between = getNextSibling(newImage);
                if (between && between == neighborImage) {
                    hasLineBetween = false;
                } else if (between && isLineEmpty(between) && getNextSibling(between) == neighborImage) {
                    hasLineBetween = true;
                }
            } else {
                var between = getPrevSibling(newImage);
                if (between && between == neighborImage) {
                    hasLineBetween = false;
                } else if (between && isLineEmpty(between) && getPrevSibling(between) == neighborImage) {
                    hasLineBetween = true;
                }
            }
            
            if (!hasLineBetween) {
                if (direction == "after") {
                    insertEmptyLine(insertionPoint, neighborImage);
                } else {
                    insertEmptyLine(insertionPoint, newImage);
                }
            }
        }
    }
    
    // Прокручиваем к вставленной картинке
    scrollToElement(newImage);
    
    // Завершаем блок отмены действий
    window.external.EndUndoUnit(document);
    
    // Вычисляем время выполнения
    var endTime = new Date().getTime();
    var timeDiff = (endTime - startTime) / 1000;
    var timeFormatted = timeDiff.toFixed(3).replace(".", ",");
    
    try {
        window.external.SetStatusBarText("ОК");
    } catch(e) {}
    
    if (showStatistics) {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "Пустая блочная иллюстрация вставлена.\n\n" +
               "Время выполнения: " + timeFormatted + " сек");
    }
}
