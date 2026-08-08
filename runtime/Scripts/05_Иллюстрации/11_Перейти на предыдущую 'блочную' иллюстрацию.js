// Скрипт "Перейти на предыдущую блочную иллюстрацию" для редактора FBE
// version 1.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для поиска и выделения предыдущей "блочной" (отдельностоящей) иллюстрации в fb2 документе.
// Поиск ведется от текущего положения курсора вниз по тексту до конца документа.
// Найденная иллюстрация выделяется целиком и прокручивается в центр окна для удобства.
// По умолчанию обрабатывается только основной раздел документа, без разделов сносок и комментариев.

// На основе скрипта "Выделить предыдущий любой структурный элемент" ver. 1.2

// version 1.2, 07.03.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Перейти на предыдущую блочную иллюстрацию";
    var version = "1.2";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
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
    
    // Функция для получения предыдущего любого элемента
    function getPreviousAnyElement(el) {
        if (el.lastChild)
            el = el.lastChild;
        else {
            while (el && !el.previousSibling)
                el = el.parentNode;
            if (el && el.previousSibling) 
                el = el.previousSibling;
        }
        return el;
    }
    
    // Функция прокрутки к центру
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
    
    // Функция выделения иллюстрации (Control Range)
    function selectImage(element) {
        if (!element) return false;
        
        try {
            var controlRange = document.body.createControlRange();
            controlRange.add(element);
            controlRange.select();
            scrollElementToCenter(element);
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
    
    try { window.external.SetStatusBarText("Ищем предыдущую иллюстрацию..."); } catch(e) {}
    
    var startTime = new Date();
    
    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) {
        // УБРАН EndUndoUnit
        if (showMessages) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Не найден контейнер fbw_body");
        }
        return;
    }
    
    // === АЛГОРИТМ ПОИСКА ИЛЛЮСТРАЦИЙ ВВЕРХ ===
    
    // Получаем текущий элемент
    var currentPtr = currentElement;
    
    // Запоминаем текущую иллюстрацию, если курсор внутри неё
    var currentImage = null;
    var tempPtr = currentPtr;
    while (tempPtr && tempPtr.nodeName != "BODY") {
        if (tempPtr.nodeName == "DIV" && tempPtr.className == "image") {
            currentImage = tempPtr;
            break;
        }
        tempPtr = tempPtr.parentNode;
    }
    
    // Начинаем поиск с предыдущего элемента
    var searchPtr = getPreviousAnyElement(currentPtr);
    while (searchPtr && searchPtr.nodeType != 1) {
        searchPtr = getPreviousAnyElement(searchPtr);
    }
    
    var foundImage = null;
    
    // Поиск предыдущей иллюстрации
    while (searchPtr && fbwBody.contains(searchPtr)) {
        
        if (searchPtr.nodeName == "DIV" && 
            searchPtr.className == "image" && 
            !isInSpecialSection(searchPtr)) {
            
            // Проверяем, не та ли это иллюстрация, с которой начали
            if (!currentImage || searchPtr != currentImage) {
                foundImage = searchPtr;
                break;
            }
        }
        
        searchPtr = getPreviousAnyElement(searchPtr);
        while (searchPtr && searchPtr.nodeType != 1) {
            searchPtr = getPreviousAnyElement(searchPtr);
        }
    }
    
    // Вычисляем время выполнения
    var endTime = new Date();
    var timeDiff = (endTime - startTime) / 1000;
    
   // Если нашли иллюстрацию, выделяем её
    if (foundImage) {
        if (selectImage(foundImage)) {
            if (showMessages) {
                MsgBox(scriptName + "\nver. " + version + "\n\n" +
                       "Найдена и выделена блочная иллюстрация\n\n" +
                       "Время обработки: " + timeDiff.toFixed(3).replace(".", ",") + " сек");
            }
        } else {
            if (showMessages) {
                MsgBox(scriptName + "\nver. " + version + "\n\n" +
                       "Иллюстрация найдена, но не удалось её выделить.");
            }
        }
    } else {
        // ВСЕГДА показываем сообщение о достижении начала
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "До начала документа не нашлось ни одной блочной иллюстрации.");
    }
    
    try { window.external.SetStatusBarText("ОК"); } catch(e) {} 
    
    return;
}
