/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Перейти на предыдущую пустую 'блочную' иллюстрацию",
  "version": "1.0",
  "authors": [
    {
      "name": "TaKir",
      "role": "author",
      "contribution": "идея и реализация"
    },
    {
      "name": "DeepSeek",
      "role": "ai_assistance",
      "contribution": "реализация"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Находит предыдущую пустую блочную иллюстрацию и выделяет её без изменения книги.",
  "description": "Ищет предыдущую DIV.image, у которого href и src непосредственного IMG содержат строку undefined. Выделяет найденный элемент и прокручивает к центру окна. Это проверка текстового маркера, а не фактического отсутствия бинарника.",
  "scope": "От курсора или выделенного элемента к начала документа среди DIV.image внутри fbw_body; notes и comments по умолчанию исключены, другие body отдельно не фильтруются.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор в тексте или выделите иллюстрацию и запустите скрипт. Непустое текстовое выделение не требуется.",
    "Повторный запуск ищет предыдущую подходящую блочную иллюстрацию; при достижении начала документа появится сообщение даже в тихом режиме."
  ],
  "warnings": [
    "Пустота определяется по вхождению слова undefined одновременно в href и src; существующий файл с таким фрагментом имени может попасть в результаты."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "ed25f30ffa0ff190adeb15a6baf755b6c4443f482b40951dd60c7328d6c72b0a",
    "reviewStatus": "generated"
  },
  "tags": [
    "Иллюстрации",
    "поиск",
    "навигация",
    "изображения",
    "параметры"
  ],
  "undo": {
    "status": "no_explicit_grouping",
    "description": ""
  },
  "settings": [
    {
      "name": "processNotesSection",
      "title": "Искать в примечаниях",
      "value": 0,
      "expression": "0",
      "description": "0 — пропустить notes; 1 — включить этот раздел.",
      "source": "code"
    },
    {
      "name": "processCommentsSection",
      "title": "Искать в комментариях",
      "value": 0,
      "expression": "0",
      "description": "0 — пропустить comments; 1 — включить этот раздел.",
      "source": "code"
    },
    {
      "name": "showMessages",
      "title": "Сообщения о переходе",
      "value": 0,
      "expression": "0",
      "description": "0 — скрыть сообщения об успешном переходе и части ошибок; сообщение об отсутствии совпадения остаётся; 1 — показывать их.",
      "source": "code"
    }
  ]
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Перейти на предыдущую пустую блочную иллюстрацию" для редактора FBE
// version 1.0
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для поиска и выделения предыдущей "пустой" блочной (отдельностоящей) иллюстрации
// (без ссылки на прикреплённый бинарник) в fb2 документе.
// Поиск ведётся от текущего положения курсора вверх по тексту до начала документа.
// Обычные иллюстрации (со ссылкой на прикреплённый бинарник) пропускаются.
// Найденная пустая иллюстрация выделяется целиком и прокручивается в центр окна для удобства.
// По умолчанию обрабатывается только основной раздел документа, без разделов сносок и комментариев.
// Скрипт не вносит никаких изменений в fb2 документ.

// На основе скрипта "Перейти на предыдущую блочную иллюстрацию" ver. 1.2

// version 1.0, 21.09.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Перейти на предыдущую пустую блочную иллюстрацию";
    var version = "1.0";
    
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
    
    // Функция проверки - является ли блочная иллюстрация пустышкой
    // (у DIV.image href содержит "undefined" И у вложенного IMG src содержит "undefined")
    function isEmptyImage(imageDiv) {
        if (!imageDiv) return false;
        
        var href = imageDiv.getAttribute("href") || "";
        if (href.indexOf("undefined") == -1) return false;
        
        var img = null;
        var children = imageDiv.childNodes;
        for (var i = 0; i < children.length; i++) {
            if (children[i].nodeName == "IMG") {
                img = children[i];
                break;
            }
        }
        if (!img) return false;
        
        var src = img.getAttribute("src") || "";
        if (src.indexOf("undefined") == -1) return false;
        
        return true;
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
    
    try { window.external.SetStatusBarText("Ищем предыдущую пустую иллюстрацию..."); } catch(e) {}
    
    var startTime = new Date();
    
    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) {
        if (showMessages) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Не найден контейнер fbw_body");
        }
        return;
    }
    
    // === АЛГОРИТМ ПОИСКА ПУСТЫХ ИЛЛЮСТРАЦИЙ ВВЕРХ ===
    
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
    
    // Поиск предыдущей пустой иллюстрации
    while (searchPtr && fbwBody.contains(searchPtr)) {
        
        if (searchPtr.nodeName == "DIV" && 
            searchPtr.className == "image" && 
            !isInSpecialSection(searchPtr) &&
            isEmptyImage(searchPtr)) {
            
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
                       "Найдена и выделена пустая блочная иллюстрация\n\n" +
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
               "До начала документа не нашлось ни одной пустой блочной иллюстрации.");
    }
    
    try { window.external.SetStatusBarText("ОК"); } catch(e) {} 
    
    return;
}