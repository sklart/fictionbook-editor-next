// Скрипт "Вставить пустую строку после структурного элемента или картинки" для редактора FBE
// version 1.3
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для вставки пустой строки после текущего структурного элемента в fb2 документах.
// Скрипт работает с текущим положением курсора или выделением.
// Обрабатывает заголовки, эпиграфы, цитаты, стихи, аннотации, таблицы, иллюстрации и подзаголовки.
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.3, 04.03.2025
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Вставить пустую строку после структурного элемента или картинки";
    var version = "1.3";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать сообщения, 0 - не показывать (тихий режим)
    var showMessages = 0; // Измените на 0 для тихого режима или на 1 для обычного
    
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
        
        // Проверяем текст внутри (с учетом неразрывных пробелов)
        var text = elem.innerText || "";
        var emptyRegExp = new RegExp("^[ " + nbspChar + "]*$");
        return (text.search(emptyRegExp) >= 0);
    }
    
    // Получаем текущее выделение
    var sel = document.selection;
    if (!sel) {
        if (showMessages) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Не удалось получить выделение!");
        }
        return;
    }
    
    var range = sel.createRange();
    if (!range) {
        if (showMessages) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Не удалось получить диапазон выделения!");
        }
        return;
    }
    
    // Получаем элемент, в котором находится курсор или выделение
    var currentElement = null;
    try {
        currentElement = range.parentElement();
    } catch(e) {
        // Если не получилось через parentElement, пробуем другие способы
        if (sel.type && sel.type == "Control") {
            // Это выделение контрола (картинки)
            var controlRange = sel.createRange();
            if (controlRange && controlRange.length > 0) {
                currentElement = controlRange.item(0);
            }
        }
    }
    
    if (!currentElement) {
        if (showMessages) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Не удалось определить текущий элемент!\n" +
                   "Попробуйте поставить курсор внутрь элемента или кликнуть на картинку.");
        }
        return;
    }
    
    // Начинаем запись действий для возможности отмены Ctrl+Z
    window.external.BeginUndoUnit(document, scriptName);
    
    // Переменная для найденного блочного элемента
    var targetElement = null;
    var targetTypeRu = "";
    
    // Функция для проверки, является ли элемент целевым
    function isTargetElement(elem) {
        if (!elem || elem.nodeType != 1) return false;
        
        var tagName = elem.tagName;
        var className = elem.className || "";
        
        // Проверяем все возможные целевые элементы
        if (tagName == "DIV") {
            if (className == "title") {
                targetTypeRu = "заголовка";
                return true;
            }
            if (className == "epigraph") {
                targetTypeRu = "эпиграфа";
                return true;
            }
            if (className == "poem") {
                targetTypeRu = "стиха";
                return true;
            }
            if (className == "cite") {
                targetTypeRu = "цитаты";
                return true;
            }
            if (className == "annotation") {
                targetTypeRu = "аннотации";
                return true;
            }
            if (className == "image") {
                targetTypeRu = "иллюстрации";
                return true;
            }
            if (className == "table") {
                targetTypeRu = "таблицы";
                return true;
            }
        }
        
        if (tagName == "P" && className == "subtitle") {
            targetTypeRu = "подзаголовка";
            return true;
        }
        
        if (tagName == "TABLE") {
            targetTypeRu = "таблицы";
            return true;
        }
        
        return false;
    }
    
    // Ищем целевой элемент, поднимаясь по родителям
    var elem = currentElement;
    while (elem) {
        if (isTargetElement(elem)) {
            targetElement = elem;
            break;
        }
        elem = elem.parentElement;
    }
    
    // Если не нашли - сообщаем и выходим
    if (!targetElement) {
        window.external.EndUndoUnit(document);
        
        if (showMessages) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Курсор должен находиться внутри одного из элементов:\n" +
                   "• заголовок (DIV class=title)\n" +
                   "• подзаголовок (P class=subtitle)\n" +
                   "• эпиграф (DIV class=epigraph)\n" +
                   "• цитата (DIV class=cite)\n" +
                   "• стих (DIV class=poem)\n" +
                   "• аннотация (DIV class=annotation)\n" +
                   "• таблица (DIV class=table или TABLE)\n" +
                   "• иллюстрация (DIV class=image)");
        }
        return;
    }
    
    // Проверяем, есть ли уже пустая строка после целевого элемента
    var nextElement = targetElement.nextSibling;
    
    // Пропускаем пустые текстовые узлы (переносы строк и пробелы)
    while (nextElement && nextElement.nodeType == 3) {
        nextElement = nextElement.nextSibling;
    }
    
    // Если следующий элемент - пустая строка, ничего не делаем
    if (nextElement && isEmptyLine(nextElement)) {
        window.external.EndUndoUnit(document);
        
        if (showMessages) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "✓ Пустая строка после " + targetTypeRu + " уже существует");
        }
        
        return;
    }
    
    // Засекаем время начала выполнения (ТОЛЬКО после всех проверок и подтверждений!)
    var startTime = new Date();
    
    // Создаем пустую строку
    var emptyLine = document.createElement("P");
    emptyLine.innerHTML = nbspEntity;
    // Важно! Сообщаем FBE, что это блочный элемент
    window.external.inflateBlock(emptyLine) = true;
    
    // Вставляем пустую строку после целевого элемента
    targetElement.insertAdjacentElement("afterEnd", emptyLine);
    
    // Завершаем запись действий
    window.external.EndUndoUnit(document);
    
    // Вычисляем время выполнения
    var endTime = new Date();
    var execTime = (endTime - startTime) / 1000; // в секундах
    
    // Форматируем время с 3 знаками после запятой
    var execTimeStr = execTime.toFixed(3).replace(".", ",");
    
    // Показываем результат (если включен показ сообщений)
    if (showMessages) {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "✓ Пустая строка вставлена после " + targetTypeRu + "\n\n" +
               "Время обработки: " + execTimeStr + " сек");
    }
}
