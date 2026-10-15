/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Создать подзаголовок(-ки) из обычных выделенных абзацев",
  "version": "1.2",
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
  "summary": "Преобразует обычные абзацы под курсором или в выделении в отдельные подзаголовки.",
  "description": "Вставляет временные маркеры в начало и конец диапазона, собирает пересечённые абзацы P и отклоняет обработку при непустых абзацах внутри заголовка, цитаты, стиха, таблицы, аннотации, эпиграфа или истории. Остальные абзацы заменяет на новые P с классом subtitle, сохраняя их HTML. При исходной настройке 0 пустые строки оставляет; при 1 также превращает их в подзаголовки, вопреки пояснению об удалении. Работает и с одним курсором, и с выделением.",
  "scope": "Абзац под курсором или полные абзацы, пересечённые выделением; обход может затронуть абзацы между маркерами вне ожидаемой области при нарушенной структуре.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор в обычный абзац либо выделите один или несколько абзацев и запустите команду.",
    "Проверьте, какие абзацы получили класс subtitle. При значении removeEmptyLines=1 пустые строки не удаляются, а становятся подзаголовками."
  ],
  "warnings": [
    "Параметр removeEmptyLines=1 не удаляет пустые строки: код размечает их как подзаголовки."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "2ed883795e8d102254443d5f2bb8b482798de50eafba893a9ab3903c1748928d",
    "reviewStatus": "generated"
  },
  "tags": [
    "Заголовки, подзаголовки, пустые строки",
    "параметры"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": ""
  },
  "settings": [
    {
      "name": "removeEmptyLines",
      "value": 0,
      "expression": "0",
      "description": "Обработка пустых строк: 0 — оставить, 1 — преобразовать в подзаголовки (ошибка текущей реализации, удаления нет).",
      "valueMeaning": "Исходное значение в коде скрипта.",
      "title": "Удалять пустые строки"
    }
  ]
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Создать подзаголовок(-ки) из обычных выделенных абзацев" для редактора FBE
// version 1.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для разметки подзаголовков в fb2 документе из обычных абзацев.
// Внутри уже размеченных структурных (блочных) DIV-элементов подзаголовки скриптом не создаются:
// (хистори, заголовки, аннотации, эпиграфы, цитаты, стихи, таблицы).
// Поддерживаются все варианты выделения:
//  курсор внутри абзаца, полное или неполное выделение одного или нескольких абзацев.
// Каждый выделенный абзац размечается как отдельный подзаголовок.
// Настройка для удаления или сохранения пустых строк при создании подзаголовков.

// version 1.2, 06.04.2026
//======================================

function Run() {
    var scriptName = "Создать подзаголовок(-ки) из обычных выделенных абзацев";
    var version = "1.2";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Удалять пустые строки между исходными абзацами при создании подзаголовков
    // 0 - нет (оставляем как есть), 1 - да, удаляем
    var removeEmptyLines = 0;
    
    // Структурные элементы DIV, внутри которых скрипт не создает подзаголовки
    var forbiddenClasses = ["title", "epigraph", "cite", "annotation", "poem", "stanza", "table", "history"];
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    // Функция для вывода сообщения об ошибке
    function showErrorMsg() {
        var msg = scriptName + "\n";
        msg += "ver. " + version + "\n\n";
        msg += "Должны быть выделены только обычные абзацы!";
        MsgBox(msg, "FBE скрипт");
    }
    
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
    var reEmpty = new RegExp("^( | |&nbsp;|"+nbspChar+")*?$", "");
    var reTags = new RegExp("<[^>]*?>", "gi");
    
    // Функция проверки, является ли абзац пустой строкой
    function isEmptyParagraph(p) {
        var text = p.innerHTML.replace(reTags, "");
        return reEmpty.test(text);
    }
    
    // Функция проверки, находится ли элемент внутри запрещённого DIV
    function isInsideForbiddenElement(element) {
        var parent = element.parentNode;
        while (parent) {
            if (parent.nodeName == "DIV" && parent.className) {
                for (var i = 0; i < forbiddenClasses.length; i++) {
                    if (parent.className == forbiddenClasses[i]) {
                        return true;
                    }
                }
            }
            parent = parent.parentNode;
        }
        return false;
    }
    
    // Функция для получения следующего узла (из скрипта цитат)
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
    
    // Функция для получения следующего абзаца (из скрипта цитат)
    function getNextP(el) {
        var savedEl = el;
        while (el && (el.nodeName != "P" || el == savedEl))
            el = getNextNode(el);
        return el;
    }
    
    // Проверяем, что выделение не в поле ввода
    var tr = document.selection.createRange();
    if (tr.parentElement().nodeName == "TEXTAREA" || tr.parentElement().nodeName == "INPUT") {
        showErrorMsg();
        return;
    }
    
    window.external.BeginUndoUnit(document, scriptName);
    
    // Вставляем маркеры начала и конца выделения
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
    
    // Находим абзац начала выделения
    var blockStartNode = document.getElementById(startId);
    var blockStartEl = blockStartNode;
    while (blockStartEl && blockStartEl.nodeName != "BODY" && blockStartEl.nodeName != "P")
        blockStartEl = blockStartEl.parentNode;
    
    if (!blockStartEl || blockStartEl.nodeName == "BODY") {
        // Удаляем маркеры и выходим
        if (blockStartNode) blockStartNode.removeNode(true);
        var tmpNode = document.getElementById(endId);
        if (tmpNode) tmpNode.removeNode(true);
        window.external.EndUndoUnit(document);
        return;
    }
    
    // Находим абзац конца выделения
    var blockEndNode = document.getElementById(endId);
    var blockEndEl = blockEndNode;
    while (blockEndEl && blockEndEl.nodeName != "BODY" && blockEndEl.nodeName != "P")
        blockEndEl = blockEndEl.parentNode;
    
    if (blockEndEl && blockEndEl.nodeName == "BODY") {
        blockEndEl = blockEndNode;
        if (blockEndEl.previousSibling && blockEndEl.previousSibling.nodeName == "P")
            blockEndEl = blockEndEl.previousSibling;
        if (!blockEndEl || blockEndEl.nodeName != "P") {
            // Удаляем маркеры и выходим
            if (blockStartNode) blockStartNode.removeNode(true);
            if (blockEndNode) blockEndNode.removeNode(true);
            window.external.EndUndoUnit(document);
            return;
        }
    }
    
    // Собираем все абзацы от start до end
    var psArray = [];
    var ptr = blockStartEl;
    while (ptr) {
        if (ptr.nodeName == "P") {
            // Проверяем, не добавлен ли уже этот абзац
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
    
    // Проверяем все абзацы на наличие внутри запрещённых элементов
    var hasForbidden = false;
    for (var i = 0; i < psArray.length; i++) {
        // Проверяем только НЕ пустые строки (если removeEmptyLines == 0)
        // или все абзацы (если removeEmptyLines == 1)
        if (removeEmptyLines == 1 || !isEmptyParagraph(psArray[i])) {
            if (isInsideForbiddenElement(psArray[i])) {
                hasForbidden = true;
                break;
            }
        }
    }
    
    if (hasForbidden) {
        // Выводим сообщение об ошибке
        showErrorMsg();
        
        // Удаляем маркеры
        if (blockStartNode) blockStartNode.removeNode(true);
        if (blockEndNode) blockEndNode.removeNode(true);
        
        window.external.EndUndoUnit(document);
        return;
    }
    
    // Заменяем абзацы на подзаголовки (обрабатываем в обратном порядке)
    for (var i = psArray.length - 1; i >= 0; i--) {
        var p = psArray[i];
        
        // Если это пустая строка и настройка removeEmptyLines == 0 - пропускаем (не трогаем)
        if (removeEmptyLines == 0 && isEmptyParagraph(p)) {
            continue;
        }
        
        // Пропускаем, если уже подзаголовок
        if (p.className == "subtitle") continue;
        
        // Создаём новый абзац с классом subtitle
        var newP = document.createElement("P");
        newP.className = "subtitle";
        
        // Копируем всё содержимое
        newP.innerHTML = p.innerHTML;
        
        // Заменяем старый абзац на новый
        p.parentNode.insertBefore(newP, p);
        p.removeNode(true);
    }
    
    // Удаляем маркеры
    if (blockStartNode) blockStartNode.removeNode(true);
    if (blockEndNode) blockEndNode.removeNode(true);
    
    window.external.EndUndoUnit(document);
    
    // Статус-бар (без сообщения пользователю)
    try {
        window.external.SetStatusBarText(scriptName + " ver. " + version + ": выполнено");
    }
    catch(e) {}
}
