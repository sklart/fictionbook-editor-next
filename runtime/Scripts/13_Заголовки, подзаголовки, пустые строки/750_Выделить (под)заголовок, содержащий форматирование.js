/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Выделить (под)заголовок, содержащий форматирование",
  "version": "1.4",
  "authors": [
    {
      "name": "TaKir",
      "role": "author",
      "contribution": "идея и реализация"
    },
    {
      "name": "DeepSeek",
      "role": "ai_assistance",
      "contribution": "реализация по указанию в шапке"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Находит следующий заголовок или подзаголовок с выбранным видом форматирования и выделяет его.",
  "description": "Начиная со следующего абзаца после позиции курсора, ищет P в документе. Подзаголовок либо заголовок раздела считается подходящим, если внутри есть жирность, курсив, зачёркивание, верхний или нижний индекс либо фрагмент кода, включённый настройками. Выделяет найденный P или весь DIV заголовка и прокручивает окно. Текст книги не изменяет. Переключатели для сносок и комментариев в текущей реализации не ограничивают область поиска.",
  "scope": "Абзацы внутри fbw_body после текущей позиции до конца книги; настройки разделов сносок и комментариев фактически не фильтруют обход.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор перед нужным заголовком или подзаголовком и запустите поиск без выделения.",
    "Настройками в исходном JS можно выбрать типы заголовков и форматирования. Повторный запуск продолжит поиск после выделенного элемента."
  ],
  "warnings": [
    "Переключатели сносок и комментариев не действуют: поиск может выделить элемент в этих разделах даже при значении 0.",
    "Если fbw_body отсутствует, выполнение может прерваться после начала группы отмены."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "858d38796d8b638e8c0ab273b0c2aee7564ee9b539391bf513dfde78d3dff6cd",
    "reviewStatus": "generated"
  },
  "tags": [
    "Заголовки, подзаголовки, пустые строки",
    "параметры"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Скрипт ищет и выделяет элемент, не меняя текст книги. При обычном завершении служебная группа действий закрывается; если область книги отсутствует, завершение не гарантировано."
  },
  "settings": [
    {
      "name": "processNotesSection",
      "value": 1,
      "expression": "1",
      "description": "Включать поиск в сносках: 1 — да, 0 — нет. В текущем коде переключатель не работает.",
      "valueMeaning": "Исходное значение в коде скрипта."
    },
    {
      "name": "processCommentsSection",
      "value": 1,
      "expression": "1",
      "description": "Включать поиск в комментариях: 1 — да, 0 — нет. В текущем коде переключатель не работает.",
      "valueMeaning": "Исходное значение в коде скрипта."
    },
    {
      "name": "processTITLES",
      "value": 1,
      "expression": "1",
      "description": "Искать заголовки разделов: 1 — да, 0 — нет.",
      "valueMeaning": "Исходное значение в коде скрипта."
    },
    {
      "name": "processSUBTITLES",
      "value": 1,
      "expression": "1",
      "description": "Искать подзаголовки: 1 — да, 0 — нет.",
      "valueMeaning": "Исходное значение в коде скрипта."
    },
    {
      "name": "processSTRONG",
      "value": 1,
      "expression": "1",
      "description": "Учитывать жирное начертание: 1 — да, 0 — нет.",
      "valueMeaning": "Исходное значение в коде скрипта."
    },
    {
      "name": "processEM",
      "value": 1,
      "expression": "1",
      "description": "Учитывать курсив: 1 — да, 0 — нет.",
      "valueMeaning": "Исходное значение в коде скрипта."
    },
    {
      "name": "processSTRIKE",
      "value": 1,
      "expression": "1",
      "description": "Учитывать зачёркивание: 1 — да, 0 — нет.",
      "valueMeaning": "Исходное значение в коде скрипта."
    },
    {
      "name": "processSUP",
      "value": 1,
      "expression": "1",
      "description": "Учитывать верхний индекс: 1 — да, 0 — нет.",
      "valueMeaning": "Исходное значение в коде скрипта."
    },
    {
      "name": "processSUB",
      "value": 1,
      "expression": "1",
      "description": "Учитывать нижний индекс: 1 — да, 0 — нет.",
      "valueMeaning": "Исходное значение в коде скрипта."
    },
    {
      "name": "processCODE",
      "value": 1,
      "expression": "1",
      "description": "Учитывать фрагменты кода: 1 — да, 0 — нет.",
      "valueMeaning": "Исходное значение в коде скрипта."
    }
  ]
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Выделить (под)заголовок, содержащий форматирование" для редактора FBE
// version 1.4
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для обнаруженияв fb2 документах и выделения ближайшего найденного
// (вниз по тексту от места курсора) заголовка или подзаголовка с любым форматированием.
//  (жирность, курсив, зачеркивание, верхние или нижние индексы, код).
// Обнаружение каждого вида форматирования можно включать-выключать по необходимости.
// По умолчанию поиск производится во всем документе, включая разделы сносок и комментариев.

// На основе скриптов
// Выделить следующий заголовок.js, Выделить следующий подзаголовок (любой).js
// уважаемого тов. Sclex.

// version 1.4, 01.04.2026
//======================================

function Run() {

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 1;     // 0 - нет, 1 - да
    
    // Обрабатывать раздел комментариев
    var processCommentsSection = 1;     // 0 - нет, 1 - да
    
    // Искать в заголовках (DIV class=title)
    var processTITLES = 1;     // 0 - нет, 1 - да
    
    // Искать в подзаголовках (P class=subtitle)
    var processSUBTITLES = 1;     // 0 - нет, 1 - да
    
    // Искать жирность
    var processSTRONG = 1;     // 0 - нет, 1 - да
    
    // Искать курсив
    var processEM = 1;     // 0 - нет, 1 - да
    
    // Искать зачеркивание
    var processSTRIKE = 1;     // 0 - нет, 1 - да
    
    // Искать верхний индекс
    var processSUP = 1;     // 0 - нет, 1 - да
    
    // Искать нижний индекс
    var processSUB = 1;     // 0 - нет, 1 - да
    
    // Искать код
    var processCODE = 1;     // 0 - нет, 1 - да

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    var scriptName = "Выделить (под)заголовок, содержащий форматирование";
    var version = "1.4";
    var undoMsg = "поиск и выделение следующего (под)заголовка с форматированием";
    var notFoundMsg = "От позиции курсора до конца документа ничего не найдено.";

    try { var nbspChar=window.external.GetNBSP(); var nbspEntity; if (nbspChar.charCodeAt(0)==160) nbspEntity="&nbsp;"; else nbspEntity=nbspChar; }
    catch(e) { var nbspChar=String.fromCharCode(160); var nbspEntity="&nbsp;";};
    var re2=new RegExp(" |&nbsp;|"+nbspChar,"g");
    
    // Проверка, можно ли обрабатывать данный body
    function isBodyAllowed(bodyElement) {
        if (!bodyElement) return false;
        
        var fbname = bodyElement.getAttribute("fbname") || "";
        
        // Основной раздел
        if (fbname == "") return true;
        
        // Раздел сносок
        if (fbname == "notes" && processNotesSection == 1) return true;
        
        // Раздел комментариев
        if (fbname == "comments" && processCommentsSection == 1) return true;
        
        return false;
    }
    
    // Функция получения родительского body
    function getParentBody(element) {
        while (element && element.nodeName != "BODY") {
            if (element.nodeName == "DIV" && element.id == "fbw_body")
                return element;
            element = element.parentNode;
        }
        return null;
    }
    
    // Проверка наличия форматирования в элементе
    function hasFormatting(element) {
        if (!element) return false;
        
        var children = element.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 3) continue;
            if (child.nodeType == 1) {
                var nodeName = child.nodeName;
                var className = child.className || "";
                
                if (processSTRONG == 1 && nodeName == "STRONG") return true;
                if (processEM == 1 && nodeName == "EM") return true;
                if (processSTRIKE == 1 && nodeName == "STRIKE") return true;
                if (processSUP == 1 && nodeName == "SUP") return true;
                if (processSUB == 1 && nodeName == "SUB") return true;
                if (processCODE == 1 && nodeName == "SPAN" && className == "code") return true;
                
                if (hasFormatting(child)) return true;
            }
        }
        return false;
    }
    
    function checkP(elem1) {
        // Проверяем подзаголовки
        if (processSUBTITLES == 1 && elem1.className == "subtitle") {
            if (hasFormatting(elem1)) return true;
        }
        // Проверяем заголовки (через hasAmongParents)
        if (processTITLES == 1 && hasAmongParents(elem1, "title")) {
            // Находим сам DIV title
            var titleDiv = getParentWithClass(elem1, "title");
            if (titleDiv && hasFormatting(titleDiv)) return true;
        }
        return false;
    }

    function hasAmongParents(elem2, nameOfClass) {
        while (elem2 && elem2.nodeName != "BODY") {
            if (elem2.nodeName == "DIV" && elem2.className == nameOfClass) return true;
            elem2 = elem2.parentNode;
        }
        return false;
    }
    
    function getParentWithClass(elem3, nameOfClass) {
        while (elem3 && elem3.nodeName != "BODY") {
            if (elem3.nodeName == "DIV" && elem3.className == nameOfClass) return elem3;
            elem3 = elem3.parentNode;
        }
        return null;
    }
    
    var emptyLineRegExp = new RegExp("^( | |&nbsp;|"+nbspChar+")*?$","i");
    
    function isLineEmpty(ptr) {
        return emptyLineRegExp.test(ptr.innerHTML.replace(/<[^>]*?>/gi,""));
    }
    
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

    function scrollIfItNeeds() { 
        var selection = document.selection;
        if (selection) {
            var range = selection.createRange();
            var rect = range.getBoundingClientRect();
            var correction = (rect.bottom - document.documentElement.clientHeight/2);
            window.scrollBy(0, correction);
        }
    }
    
    function showMessage(msg) {
        var fullMsg = scriptName + "\nver. " + version + "\n\n" + msg;
        MsgBox(fullMsg);
    }
    
    var tr = document.selection.createRange();
    
    if (tr.parentElement().nodeName == "TEXTAREA" || tr.parentElement().nodeName == "INPUT") {
        showMessage("Ошибка. Должно быть выделение в тексте книги, а не в поле ввода.");
        return;
    }
    
    window.external.BeginUndoUnit(document, undoMsg);
    
    var fbwBody = document.getElementById("fbw_body");
    var tr3 = document.selection.createRange();
    tr3.collapse(false);
    var ptr = tr3.parentElement();
    ptr = getNextP(ptr);
    
    while (ptr && fbwBody.contains(ptr)) {
        // Проверяем, находится ли элемент в разрешенном разделе
        var body = getParentBody(ptr);
        if (isBodyAllowed(body)) {
            if (checkP(ptr)) break;
        }
        ptr = getNextP(ptr);
    }
    
    if (ptr && fbwBody.contains(ptr)) {
        var tr1 = document.body.createTextRange();
        
        // Определяем, что выделять: сам P или родительский DIV title
        var elementToSelect = ptr;
        if (processTITLES == 1 && hasAmongParents(ptr, "title")) {
            var titleDiv = getParentWithClass(ptr, "title");
            if (titleDiv) elementToSelect = titleDiv;
        }
        
        tr1.moveToElementText(elementToSelect);
        if (tr1.moveStart("character", 1) == 1)
            tr1.moveStart("character", -1);
        tr1.moveEnd("character", -1);
        tr1.select();
        scrollIfItNeeds();
    }
    else {
        showMessage(notFoundMsg);
    }
    
    window.external.EndUndoUnit(document);
}
