// Скрипт "Вставить пользовательский подзаголовок по месту курсора" для редактора FBE
// version 1.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для вставки пользовательского подзаголовка по месту курсора.
// Пользователь может задать любой вариант подзаголовка.
// По умолчанию используется вариант =♦=♦=♦=
// Варианты вставки:
// 1) На место текущей пустой строки (вместо <P>&nbsp;</P>)
// 2) ПЕРЕД текущим обычным непустым абзацем
// 3) ПЕРЕД текущим DIV-контейнером (цитата, стих, таблица, блочная картинка)
// 4) ПЕРЕД текущим подзаголовком
// По умолчанию скрипт работает в тихом режиме.

// version 1.2, 14.05.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Вставить пользовательский подзаголовок по месту курсора";
    var version = "1.2";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать сообщения, 0 - не показывать (тихий режим)
    var showMessages = 0;
    
    // Пользовательский вид подзаголовка:
    var UserSubtitle = "=♦=♦=♦=";
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    // Получаем неразрывный пробел из настроек FBE
    var nbspEntity = "&nbsp;";
    var nbspChar = String.fromCharCode(160);
    
    try {
        var tempChar = window.external.GetNBSP();
        if (tempChar.charCodeAt(0) != 160) {
            nbspEntity = tempChar;
            nbspChar = tempChar;
        }
    } catch(e) {
        // Оставляем стандартный &nbsp;
    }
    
    // Функция проверки, является ли элемент пустой строкой
    function isEmptyLine(elem) {
        if (!elem || elem.nodeType != 1 || elem.tagName != "P") return false;
        
        var text = elem.innerText || "";
        var emptyRegExp = new RegExp("^[ " + nbspChar + "]*$");
        return (text.search(emptyRegExp) >= 0);
    }
    
    // Получаем текущее выделение
    var sel = document.selection;
    if (!sel) {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "✗ Не удалось получить выделение!\n" +
               "Установите курсор в нужное место документа.");
        return;
    }
    
    var range = sel.createRange();
    if (!range) {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "✗ Не удалось получить диапазон выделения!\n" +
               "Установите курсор в нужное место документа.");
        return;
    }
    
    // Получаем элемент, в котором находится курсор
    var currentElement = null;
    try {
        currentElement = range.parentElement();
    } catch(e) {
        // Если не получилось через parentElement, пробуем Control Range
        if (sel.type && sel.type == "Control") {
            var controlRange = sel.createRange();
            if (controlRange && controlRange.length > 0) {
                currentElement = controlRange.item(0);
            }
        }
    }
    
    if (!currentElement) {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "✗ Не удалось определить текущий элемент!\n" +
               "Попробуйте поставить курсор внутрь абзаца или кликнуть на картинку.");
        return;
    }
    
    // Начинаем запись действий для возможности отмены Ctrl+Z
    window.external.BeginUndoUnit(document, scriptName);
    
    var startTime = new Date();
    var inserted = false;
    var resultMessage = "";
    
    // Функция проверки, является ли DIV разрешённым контейнером
    function isAllowedDivContainer(elem) {
        if (!elem || elem.nodeType != 1 || elem.tagName != "DIV") return false;
        var className = elem.className || "";
        return (className == "cite" || className == "poem" || className == "table" || className == "image");
    }
    
    // Функция проверки, является ли DIV запрещённым (annotation, title, epigraph)
    function isForbiddenDiv(elem) {
        if (!elem || elem.nodeType != 1 || elem.tagName != "DIV") return false;
        var className = elem.className || "";
        return (className == "annotation" || className == "title" || className == "epigraph");
    }
    
    // Ищем целевой элемент
    // Проходим вверх от текущего элемента, собирая всех родителей
    // Сначала проверяем, не находимся ли мы внутри запрещённого DIV
    // Если находимся внутри разрешённого DIV-контейнера — целью становится сам этот DIV
    // Иначе целью становится первый найденный P
    
    var elem = currentElement;
    var targetElement = null;
    var targetType = ""; // "emptyP", "normalP", "divContainer", "subtitleP"
    var foundP = null;
    var foundPType = "";
    var foundAllowedDiv = null;
    var foundForbiddenDiv = null;
    
    while (elem) {
        // Проверяем, не запрещённый ли это DIV
        if (elem.nodeType == 1 && elem.tagName == "DIV") {
            if (isForbiddenDiv(elem)) {
                foundForbiddenDiv = elem;
                // Не прерываем цикл, продолжаем искать — 
                // может быть, мы внутри разрешённого DIV, который внутри запрещённого
                // Но запрещённый имеет приоритет
            }
            if (isAllowedDivContainer(elem) && !foundAllowedDiv) {
                // Запоминаем первый (самый глубокий) разрешённый DIV
                foundAllowedDiv = elem;
            }
        }
        
        // Ищем первый P (если ещё не нашли)
        if (!foundP && elem.nodeType == 1 && elem.tagName == "P") {
            foundP = elem;
            if (isEmptyLine(elem)) {
                foundPType = "emptyP";
            } else if (elem.className == "subtitle") {
                foundPType = "subtitleP";
            } else {
                foundPType = "normalP";
            }
        }
        
        // Проверяем TABLE
        if (elem.nodeType == 1 && elem.tagName == "TABLE" && !foundAllowedDiv) {
            foundAllowedDiv = elem; // TABLE обрабатываем как разрешённый контейнер
        }
        
        elem = elem.parentElement;
    }
    
    // Принимаем решение о цели
    
    // 1. Если нашли запрещённый DIV (annotation, title, epigraph) — ошибка
    if (foundForbiddenDiv) {
        window.external.EndUndoUnit(document);
        var forbiddenName = foundForbiddenDiv.className || "";
        var forbiddenNameRu = "";
        if (forbiddenName == "annotation") forbiddenNameRu = "аннотации";
        else if (forbiddenName == "title") forbiddenNameRu = "заголовка";
        else if (forbiddenName == "epigraph") forbiddenNameRu = "эпиграфа";
        else forbiddenNameRu = forbiddenName;
        
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "Курсор находится внутри " + forbiddenNameRu + ".\n\n" +
               "✗ Перед данным элементом подзаголовок не вставляется.\n\n" +
               "Подзаголовки перед annotation, title и epigraph не допускаются.");
        return;
    }
    
    // 2. Если нашли разрешённый DIV-контейнер — вставляем перед ним
    if (foundAllowedDiv) {
        targetElement = foundAllowedDiv;
        targetType = "divContainer";
    }
    // 3. Иначе если нашли P — работаем с ним
    else if (foundP) {
        targetElement = foundP;
        targetType = foundPType;
    }
    // 4. Ничего не нашли
    else {
        window.external.EndUndoUnit(document);
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "✗ Не удалось определить место для вставки подзаголовка.\n" +
               "Курсор должен находиться внутри абзаца, пустой строки,\n" +
               "подзаголовка, цитаты, стиха, таблицы или картинки.");
        return;
    }
    
    // Создаём новый подзаголовок
    var newSubtitle = document.createElement("P");
    newSubtitle.className = "subtitle";
    newSubtitle.innerHTML = UserSubtitle;
    
    // Вставляем в зависимости от типа целевого элемента
    if (targetType == "emptyP") {
        // Заменяем пустую строку на подзаголовок
        targetElement.parentNode.insertBefore(newSubtitle, targetElement);
        targetElement.removeNode(true);
        inserted = true;
        resultMessage = "✓ Подзаголовок вставлен на место пустой строки";
    } else if (targetType == "normalP") {
        // Вставляем ПЕРЕД непустым абзацем
        targetElement.parentNode.insertBefore(newSubtitle, targetElement);
        inserted = true;
        resultMessage = "✓ Подзаголовок вставлен перед абзацем";
    } else if (targetType == "subtitleP") {
        // Вставляем ПЕРЕД существующим подзаголовком
        targetElement.parentNode.insertBefore(newSubtitle, targetElement);
        inserted = true;
        resultMessage = "✓ Подзаголовок вставлен перед существующим подзаголовком";
    } else if (targetType == "divContainer") {
        // Вставляем ПЕРЕД DIV-контейнером (вне контейнера)
        targetElement.parentNode.insertBefore(newSubtitle, targetElement);
        inserted = true;
        resultMessage = "✓ Подзаголовок вставлен перед структурным элементом";
    }
    
    // Завершаем запись действий
    window.external.EndUndoUnit(document);
    
    // Вычисляем время выполнения
    var endTime = new Date();
    var execTime = (endTime - startTime) / 1000;
    var execTimeStr = execTime.toFixed(3).replace(".", ",");
    
    // Выводим результат
    if (inserted) {
        if (showMessages == 1) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   resultMessage + "\n" +
                   "Содержимое: " + UserSubtitle + "\n\n" +
                   "Время обработки: " + execTimeStr + " сек");
        }
    } else {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "✗ Не удалось вставить подзаголовок.\n" +
               "Возможно, структура документа в этом месте не подходит.");
    }
    
    try {
        window.external.SetStatusBarText(scriptName + " ver. " + version + ": подзаголовок вставлен. Время: " + execTimeStr + " сек.");
    }
    catch(e) {}
}
