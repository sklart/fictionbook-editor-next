// Скрипт "Перейти на следующую инлайн-иллюстрацию" для редактора FBE
// version 1.8
// Автор исходного скрипта - Sсlex, доработка - stokber
// Обновление алгоритма - DeepSeek, TaKir (ver. 1.8)

// Скрипт предназначен для поиска и выделения следующей "инлайн" (внутриабзацной) иллюстрации в fb2 документах.
// Поиск ведется от текущего положения курсора или выделенной картинки вниз по тексту.
// Найденная иллюстрация выделяется целиком и прокручивается в центр окна для удобства.
// По умолчанию обрабатывается только основной раздел документа, без разделов сносок и комментариев.

// Изменения по сравнению с версией 1.0:
// Переработан алгоритм поиска и выделения иллюстраций.
// Скрипт работает без вставки маркеров (т.е. никак не изменяет документ).

// version 1.8, 22.04.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Перейти на следующую инлайн-иллюстрацию";
    var version = "1.8";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Обрабатывать раздел сносок (примечаний) - 0 нет, 1 да
    var processNotesSection = 0;
    
    // Обрабатывать раздел комментариев - 0 нет, 1 да
    var processCommentsSection = 0;
    
    // Настройка: 1 - показывать статистику и сообщения, 0 - тихий режим (только когда картинки закончились)
    var showStatistics = 0;
    
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
    
    // Функция для получения следующего элемента
    function getNextElement(el) {
        if (el.firstChild)
            return el.firstChild;
        
        var current = el;
        while (current && current.nextSibling == null)
            current = current.parentNode;
        
        if (current)
            return current.nextSibling;
        
        return null;
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
    
    // Проверка, что картинка не пустая
    function isNonEmptyImage(img) {
        if (!img.hasChildNodes()) return false;
        
        var child = getNextElement(img);
        while (child && child != img) {
            if (child.nodeName == "IMG") return true;
            child = getNextElement(child);
        }
        return false;
    }
    
    // Собрать ВСЕ инлайн-картинки в документе (в правильном порядке обхода DOM)
    function collectAllImages(root) {
        var images = [];
        
        function traverse(el) {
            if (!el) return;
            
            if (el.nodeName == "SPAN" && el.className == "image") {
                if (!isInSpecialSection(el) && isNonEmptyImage(el)) {
                    images.push(el);
                }
            }
            
            // Обходим детей
            var child = el.firstChild;
            while (child) {
                traverse(child);
                child = child.nextSibling;
            }
        }
        
        traverse(root);
        return images;
    }
    
    // Создать TextRange для элемента (на начало элемента)
    function createRangeAtElementStart(el) {
        var range = document.body.createTextRange();
        range.moveToElementText(el);
        range.collapse(true);
        return range;
    }
    
    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) {
        if (showStatistics) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "✗ Не найден контейнер fbw_body");
        }
        return;
    }
    
    try { window.external.SetStatusBarText("Ищем следующую инлайн-иллюстрацию..."); } catch(e) {}
    
    var startTime = new Date();
    
    var sel = document.selection;
    var currentRange = null;
    var startElement = null;
    
    // Проверяем, выделена ли картинка (Control Range)
    if (sel.type && sel.type == "Control") {
        var controlRange = sel.createRange();
        if (controlRange && controlRange.length > 0) {
            startElement = controlRange.item(0);
            // Создаём TextRange для начала этой картинки
            currentRange = createRangeAtElementStart(startElement);
        }
    }
    
    if (!currentRange) {
        // Текстовое выделение или курсор
        currentRange = sel.createRange();
        if (!currentRange) {
            if (showStatistics) {
                MsgBox(scriptName + "\nver. " + version + "\n\n" +
                       "✗ Не удалось получить диапазон выделения.");
            }
            return;
        }
    }
    
    // Собираем все картинки в документе
    var allImages = collectAllImages(fbwBody);
    
    if (allImages.length == 0) {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "✗ В документе нет инлайн-иллюстраций.");
        return;
    }
    
    // Ищем первую картинку, которая идёт ПОСЛЕ текущей позиции
    var foundImage = null;
    
    for (var i = 0; i < allImages.length; i++) {
        var img = allImages[i];
        
        // Если это та же картинка, с которой начали (при Control Range) - пропускаем
        if (startElement && img === startElement) {
            continue;
        }
        
        // Создаём диапазон для начала картинки
        var imgRange = createRangeAtElementStart(img);
        
        // Сравниваем позиции: currentRange раньше imgRange?
        // compareEndPoints("StartToStart", ...) возвращает -1 если первый раньше
        var cmp = currentRange.compareEndPoints("StartToStart", imgRange);
        
        if (cmp <= 0) {
            // Картинка начинается после или в той же позиции, что и currentRange
            // Проверим, не находится ли currentRange внутри этой картинки
            var endRange = currentRange.duplicate();
            endRange.collapse(false);
            
            var cmpEnd = endRange.compareEndPoints("StartToStart", imgRange);
            
            // Если конец currentRange тоже <= начала картинки, значит мы перед картинкой
            // Или если currentRange внутри картинки - тоже берём следующую
            if (cmpEnd <= 0 || (startElement && startElement.contains)) {
                foundImage = img;
                break;
            }
        }
    }
    
    // Если не нашли после текущей позиции, может быть в начале документа?
    if (!foundImage && !startElement) {
        // Может, курсор в конце документа, и картинки есть только до него?
        // Тогда ищем первую картинку, которая ПОСЛЕ начала документа, но ДО курсора?
        // Это сложный случай, пока пропустим
    }
    
    // Вычисляем время выполнения
    var endTime = new Date();
    var timeDiff = (endTime - startTime) / 1000;
    
    // Если нашли иллюстрацию, выделяем её
    if (foundImage) {
        selectImage(foundImage);
        if (showStatistics) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "✓ Найдена и выделена инлайн-иллюстрация\n\n" +
                   "Всего инлайн иллюстраций в документе: " + allImages.length + "\n" +
                   "Время обработки: " + timeDiff.toFixed(3).replace(".", ",") + " сек");
        }
    } else {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "✗ До конца документа не нашлось ни одной инлайн-иллюстрации.\n\n" +
               "Всего инлайн иллюстраций в документе: " + allImages.length);
    }
    
    try { window.external.SetStatusBarText("ОК"); } catch(e) {}
    
    return;
}
