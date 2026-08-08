// Скрипт "Выделить следующий любой структурный элемент" для редактора FBE
// version 2.8
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для поиска и выделения уже размеченных элементов в fb2 документах.
// Поиск ведется от текущего положения курсора вниз по тексту до конца документа.
// Скрипт ищет: аннотации, заголовки, эпиграфы, цитаты, стихи, таблицы,
// подзаголовки, авторов текста и "блочные" (отдельностоящие) иллюстрации.
// Найденный элемент выделяется целиком и позиционируется в центре окна документа для удобства.
// Индивидуальная настройка включения в поиск для каждого типа элемента.
// По умолчанию обрабатывается только основной раздел документа, без разделов сносок и комментариев.
// Каких-либо изменений в документе скрипт не делает.

// На основе серии скриптов:
// Выделить следующий ... элемент (заголовок, цитату, стих), уважаемого тов. Sclex.

// version 2.8, 06.03.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Выделить следующий любой структурный элемент";
    var version = "2.8";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // 1 - искать элемент, 0 - пропускать (игнорировать)
    var findAnnotation = 1;      // аннотация (DIV class=annotation)
    var findTitle = 1;           // заголовок (DIV class=title)
    var findEpigraph = 1;        // эпиграф (DIV class=epigraph)
    var findCite = 1;            // цитата (DIV class=cite)
    var findPoem = 1;            // стих (DIV class=poem)
    var findStanza = 1;          // строфа (DIV class=stanza)
    var findTable = 1;           // таблица (DIV class=table или TABLE)
    var findTextAuthor = 1;      // автор текста (P class=text-author)
    var findSubtitle = 1;        // подзаголовок (P class=subtitle)
    var findImage = 1;           // блочная иллюстрация (DIV class=image)
    
    // Обрабатывать раздел сносок (примечаний) - 0 нет, 1 да
    var processNotesSection = 0;
    
    // Обрабатывать раздел комментариев - 0 нет, 1 да
    var processCommentsSection = 0;
    
    // Настройка: 1 - показывать сообщения, 0 - тихий режим (только ошибки)
    var showMessages = 0;
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    // Получаем символ неразрывного пробела из настроек FBE
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
    
    // Функция проверки - находится ли элемент в разделе сносок или комментариев
    function isInSpecialSection(element) {
        while (element && element.nodeName != "BODY") {
            if (element.nodeName == "DIV" && element.className == "body") {
                var fbname = element.getAttribute("fbname") || "";
                if (fbname == "notes" && !processNotesSection) return true;
                if (fbname == "comments" && !processCommentsSection) return true;
                return false;
            }
            element = element.parentNode;
        }
        return false;
    }
    
    // Функция проверки DIV-элемента (блочного)
    function isDivTarget(element) {
        if (!element || element.nodeName != "DIV" || isInSpecialSection(element)) return false;
        
        var className = element.className || "";
        
        if (className == "annotation" && findAnnotation) return true;
        if (className == "title" && findTitle) return true;
        if (className == "epigraph" && findEpigraph) return true;
        if (className == "cite" && findCite) return true;
        if (className == "poem" && findPoem) return true;
        if (className == "stanza" && findStanza) return true;
        if (className == "image" && findImage) return true;
        if (className == "table" && findTable) return true;
        
        return false;
    }
    
    // Функция проверки TABLE-элемента
    function isTableTarget(element) {
        if (!element || element.nodeName != "TABLE" || isInSpecialSection(element)) return false;
        return (findTable);
    }
    
    // Функция проверки P-элемента (абзацного)
    function isPTarget(element) {
        if (!element || element.nodeName != "P" || isInSpecialSection(element)) return false;
        
        var className = element.className || "";
        
        if (className == "text-author" && findTextAuthor) return true;
        if (className == "subtitle" && findSubtitle) return true;
        
        return false;
    }
    
    // Функция поиска родительского DIV с определенным классом
    function getParentWithClass(elem3, nameOfClass) {
        while (elem3 && elem3.nodeName != "BODY") {
            if (elem3.nodeName == "DIV" && elem3.className == nameOfClass) return elem3;
            elem3 = elem3.parentNode;
        }
        return null;
    }
    
    // Функции навигации из примеров
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
    
    // Функция для получения следующего любого элемента (для картинок и таблиц)
    function getNextAnyElement(el) {
        if (el.firstChild)
            el = el.firstChild;
        else {
            while (el && !el.nextSibling)
                el = el.parentNode;
            if (el && el.nextSibling) 
                el = el.nextSibling;
        }
        return el;
    }
    
    // Функции прокрутки
    function scrollElementToCenter(element) {
        if (!element) return;
        
        try {
            var rect = element.getBoundingClientRect();
            if (!rect) return;
            
            var windowHeight = document.documentElement.clientHeight;
            var elementCenter = rect.top + (rect.bottom - rect.top) / 2;
            var scrollOffset = elementCenter - windowHeight / 2;
            
            window.scrollBy(0, scrollOffset);
        } catch(e) {
            try {
                element.scrollIntoView(true);
                window.scrollBy(0, -document.documentElement.clientHeight / 3);
            } catch(e2) {}
        }
    }
    
    function scrollIfItNeeds() { 
        var selection = document.selection;
        if (!selection) return;
        
        try {
            var range = selection.createRange();
            if (!range) return;
            
            var rect = range.getBoundingClientRect();
            if (rect) {
                var windowHeight = document.documentElement.clientHeight;
                var elementCenter = rect.top + (rect.bottom - rect.top) / 2;
                var scrollOffset = elementCenter - windowHeight / 2;
                window.scrollBy(0, scrollOffset);
            }
        } catch(e) {}
    }
    
    function selectElement(element) {
        if (!element) return false;
        
        try {
            if (element.nodeName == "DIV" && element.className == "image") {
                var controlRange = document.body.createControlRange();
                controlRange.add(element);
                controlRange.select();
                scrollElementToCenter(element);
            } else {
                var range = document.body.createTextRange();
                range.moveToElementText(element);
                if (range.moveStart("character", 1) == 1)
                    range.moveStart("character", -1);
                range.moveEnd("character", -1);
                range.select();
                scrollIfItNeeds();
            }
            return true;
        } catch(e) {
            return false;
        }
    }
    
    // === Получение текущего элемента ===
    
    var sel = document.selection;
    if (!sel) {
        if (showMessages) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Ошибка. Не удалось получить выделение.");
        }
        return;
    }
    
    var range = sel.createRange();
    if (!range) {
        if (showMessages) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Ошибка. Не удалось получить диапазон выделения.");
        }
        return;
    }
    
    var currentElement = null;
    
    if (sel.type && sel.type == "Control") {
        var controlRange = sel.createRange();
        if (controlRange && controlRange.length > 0) {
            currentElement = controlRange.item(0);
        }
    } else {
        try {
            currentElement = range.parentElement();
        } catch(e) {}
    }
    
    if (!currentElement) {
        if (showMessages) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Ошибка. Не удалось определить текущий элемент.\n\n" +
                   "Попробуйте поставить курсор внутрь текста или кликнуть на картинку.");
        }
        return;
    }
    
    if (currentElement.nodeName == "TEXTAREA" || currentElement.nodeName == "INPUT") {
        if (showMessages) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Ошибка. Должно быть выделение в тексте книги, а не в поле ввода.");
        }
        return;
    }
    
    window.external.BeginUndoUnit(document, "Поиск следующего элемента");
    try { window.external.SetStatusBarText("Ищем следующий структурный элемент..."); } catch(e) {}
    
    var startTime = new Date();
    
    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) {
        window.external.EndUndoUnit(document);
        if (showMessages) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Не найден контейнер fbw_body");
        }
        return;
    }
    
    // === АЛГОРИТМ ПОИСКА ВНИЗ ===
    
    // Получаем текущий элемент (может быть не P)
    var currentPtr = currentElement;
    
    // Запоминаем текущий элемент как точку старта
    var blockElementAtStart = null;
    
    // Для блочных элементов запоминаем родительский блок
    var tempPtr = currentPtr;
    while (tempPtr && tempPtr.nodeName != "BODY") {
        if (tempPtr.nodeName == "DIV" && isDivTarget(tempPtr)) {
            blockElementAtStart = tempPtr;
            break;
        }
        if (tempPtr.nodeName == "TABLE" && isTableTarget(tempPtr)) {
            blockElementAtStart = tempPtr;
            break;
        }
        tempPtr = tempPtr.parentNode;
    }
    
    // Начинаем поиск со следующего элемента
    var searchPtr = getNextAnyElement(currentPtr);
    while (searchPtr && searchPtr.nodeType != 1) {
        searchPtr = getNextAnyElement(searchPtr);
    }
    
    var foundElement = null;
    
    // Поиск вниз по документу
    while (searchPtr && fbwBody.contains(searchPtr)) {
        
        // Проверяем сам элемент (для картинок, таблиц и DIV-таблиц)
        if (searchPtr.nodeName == "DIV" && searchPtr.className == "image" && findImage && !isInSpecialSection(searchPtr)) {
            if (!blockElementAtStart || searchPtr != blockElementAtStart) {
                foundElement = searchPtr;
                break;
            }
        }
        
        if (searchPtr.nodeName == "DIV" && searchPtr.className == "table" && findTable && !isInSpecialSection(searchPtr)) {
            if (!blockElementAtStart || searchPtr != blockElementAtStart) {
                foundElement = searchPtr;
                break;
            }
        }
        
        if (searchPtr.nodeName == "TABLE" && findTable && !isInSpecialSection(searchPtr)) {
            if (!blockElementAtStart || searchPtr != blockElementAtStart) {
                foundElement = searchPtr;
                break;
            }
        }
        
        // Проверяем родительские DIV
        if (searchPtr.nodeName == "DIV" && isDivTarget(searchPtr) && !isInSpecialSection(searchPtr)) {
            if (!blockElementAtStart || searchPtr != blockElementAtStart) {
                foundElement = searchPtr;
                break;
            }
        }
        
        // Проверяем P (только если не внутри включенного DIV)
        if (searchPtr.nodeName == "P" && isPTarget(searchPtr)) {
            var insideDiv = false;
            var checkDiv = searchPtr.parentNode;
            while (checkDiv && checkDiv.nodeName != "BODY" && checkDiv != fbwBody) {
                if (checkDiv.nodeName == "DIV" && isDivTarget(checkDiv)) {
                    insideDiv = true;
                    break;
                }
                checkDiv = checkDiv.parentNode;
            }
            
            if (!insideDiv) {
                if (!blockElementAtStart || searchPtr != blockElementAtStart) {
                    foundElement = searchPtr;
                    break;
                }
            }
        }
        
        searchPtr = getNextAnyElement(searchPtr);
        while (searchPtr && searchPtr.nodeType != 1) {
            searchPtr = getNextAnyElement(searchPtr);
        }
    }
    
    // Вычисляем время выполнения
    var endTime = new Date();
    var timeDiff = (endTime - startTime) / 1000;
    
    window.external.EndUndoUnit(document);
    
    // Если нашли элемент, выделяем его
    if (foundElement) {
        var elementToSelect = foundElement;
        
        if (selectElement(elementToSelect)) {
            if (showMessages) {
                var elementType = "";
                if (elementToSelect.nodeName == "DIV") {
                    if (elementToSelect.className == "image") 
                        elementType = "DIV class=image (иллюстрация)";
                    else if (elementToSelect.className == "table")
                        elementType = "DIV class=table (таблица)";
                    else
                        elementType = "DIV class=" + elementToSelect.className;
                } else if (elementToSelect.nodeName == "TABLE") {
                    elementType = "TABLE";
                } else if (elementToSelect.nodeName == "P") {
                    elementType = "P class=" + elementToSelect.className;
                }
                
                MsgBox(scriptName + "\nver. " + version + "\n\n" +
                       "Найден и выделен элемент:\n" + elementType + 
                       "\n\nВремя обработки: " + timeDiff.toFixed(3).replace(".", ",") + " сек");
            }
        }
    } else {
        // ВСЕГДА показываем сообщение о достижении конца, независимо от режима!
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "До конца документа не нашлось ни одного подходящего элемента.");
    }
    
    try { window.external.SetStatusBarText("ОК"); } catch(e) {} 
    
    return;
}
