// Скрипт "Вставить пустую строку перед структурным элементом или картинкой" для редактора FBE
// version 1.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для вставки пустой строки перед текущим структурным элементом в fb2 документах.
// Скрипт работает с текущим положением курсора или выделением.
// Обрабатывает заголовки, эпиграфы, цитаты, стихи, аннотации, таблицы, иллюстрации и подзаголовки.
// Для заголовка вставляет пустую строку в конце ПРЕДЫДУЩЕЙ секции (с учетом максимальной вложенности).
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.2, 04.03.2025
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Вставить пустую строку перед структурным элементом или картинкой";
    var version = "1.2";
    
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
    
    // Функция для поиска самой глубокой вложенной секции
    function findDeepestSection(section) {
        var deepest = section;
        var lastChild = section.lastChild;
        
        // Ищем последнюю дочернюю секцию (самую глубокую)
        while (lastChild) {
            if (lastChild.nodeType == 1 && lastChild.tagName == "DIV" && lastChild.className == "section") {
                deepest = lastChild;
                // Рекурсивно проверяем внутри найденной секции
                var deeper = findDeepestSection(lastChild);
                if (deeper != lastChild) {
                    deepest = deeper;
                }
                break;
            }
            lastChild = lastChild.previousSibling;
        }
        
        return deepest;
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
    var targetTypeRuBefore = ""; // Для падежа "перед чем?"
    var isTitle = false; // Флаг для заголовка (особый случай)
    
    // Функция для проверки, является ли элемент целевым
    function isTargetElement(elem) {
        if (!elem || elem.nodeType != 1) return false;
        
        var tagName = elem.tagName;
        var className = elem.className || "";
        
        // Проверяем все возможные целевые элементы
        if (tagName == "DIV") {
            if (className == "title") {
                targetTypeRu = "заголовка";
                targetTypeRuBefore = "заголовком";
                isTitle = true;
                return true;
            }
            if (className == "epigraph") {
                targetTypeRu = "эпиграфа";
                targetTypeRuBefore = "эпиграфом";
                return true;
            }
            if (className == "poem") {
                targetTypeRu = "стиха";
                targetTypeRuBefore = "стихом";
                return true;
            }
            if (className == "cite") {
                targetTypeRu = "цитаты";
                targetTypeRuBefore = "цитатой";
                return true;
            }
            if (className == "annotation") {
                targetTypeRu = "аннотации";
                targetTypeRuBefore = "аннотацией";
                return true;
            }
            if (className == "image") {
                targetTypeRu = "иллюстрации";
                targetTypeRuBefore = "иллюстрацией";
                return true;
            }
            if (className == "table") {
                targetTypeRu = "таблицы";
                targetTypeRuBefore = "таблицей";
                return true;
            }
        }
        
        if (tagName == "P" && className == "subtitle") {
            targetTypeRu = "подзаголовка";
            targetTypeRuBefore = "подзаголовком";
            return true;
        }
        
        if (tagName == "TABLE") {
            targetTypeRu = "таблицы";
            targetTypeRuBefore = "таблицей";
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
    
    // Для заголовка нужна особая логика - вставляем в предыдущей секции
    if (isTitle) {
        // Находим родительскую секцию заголовка
        var parentSection = targetElement.parentNode;
        while (parentSection && (parentSection.tagName != "DIV" || parentSection.className != "section")) {
            parentSection = parentSection.parentNode;
        }
        
        if (!parentSection) {
            window.external.EndUndoUnit(document);
            if (showMessages) {
                MsgBox(scriptName + "\nver. " + version + "\n\n" +
                       "Не удалось найти родительскую секцию для заголовка!");
            }
            return;
        }
        
        // Ищем предыдущую секцию на том же уровне вложенности
        var prevSection = parentSection.previousSibling;
        while (prevSection && prevSection.nodeType == 3) {
            prevSection = prevSection.previousSibling;
        }
        
        if (!prevSection || prevSection.tagName != "DIV" || prevSection.className != "section") {
            window.external.EndUndoUnit(document);
            if (showMessages) {
                MsgBox(scriptName + "\nver. " + version + "\n\n" +
                       "Перед заголовком нет предыдущей секции для вставки пустой строки!");
            }
            return;
        }
        
        // НАХОДИМ САМУЮ ГЛУБОКУЮ ВЛОЖЕННУЮ СЕКЦИЮ ВНУТРИ prevSection
        var deepestSection = findDeepestSection(prevSection);
        
        // Проверяем, есть ли уже пустая строка в конце самой глубокой секции
        var lastChild = deepestSection.lastChild;
        while (lastChild && lastChild.nodeType == 3) {
            lastChild = lastChild.previousSibling;
        }
        
        if (lastChild && isEmptyLine(lastChild)) {
            window.external.EndUndoUnit(document);
            if (showMessages) {
                MsgBox(scriptName + "\nver. " + version + "\n\n" +
                       "✓ Пустая строка перед " + targetTypeRuBefore + " уже существует\n" +
                       "  (в конце предыдущей секции)");
            }
            return;
        }
        
        // Засекаем время
        var startTime = new Date();
        
        // Создаем и вставляем пустую строку в конец самой глубокой секции
        var emptyLine = document.createElement("P");
        emptyLine.innerHTML = nbspEntity;
        window.external.inflateBlock(emptyLine) = true;
        
        deepestSection.appendChild(emptyLine);
        
        // Завершаем запись действий
        window.external.EndUndoUnit(document);
        
        // Вычисляем время выполнения
        var endTime = new Date();
        var execTime = (endTime - startTime) / 1000;
        var execTimeStr = execTime.toFixed(3).replace(".", ",");
        
        if (showMessages) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "✓ Пустая строка вставлена перед " + targetTypeRuBefore + "\n" +
                   "  (в конце предыдущей секции)\n\n" +
                   "Время обработки: " + execTimeStr + " сек");
        }
        
        return;
    }
    
    // Для остальных элементов - обычная логика (вставка ПЕРЕД элементом)
    
    // Проверяем, есть ли уже пустая строка перед целевым элементом
    var prevElement = targetElement.previousSibling;
    
    // Пропускаем пустые текстовые узлы (переносы строк и пробелы)
    while (prevElement && prevElement.nodeType == 3) {
        prevElement = prevElement.previousSibling;
    }
    
    // Если предыдущий элемент - пустая строка, ничего не делаем
    if (prevElement && isEmptyLine(prevElement)) {
        window.external.EndUndoUnit(document);
        
        if (showMessages) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "✓ Пустая строка перед " + targetTypeRuBefore + " уже существует");
        }
        
        return;
    }
    
    // Засекаем время начала выполнения
    var startTime = new Date();
    
    // Создаем пустую строку
    var emptyLine = document.createElement("P");
    emptyLine.innerHTML = nbspEntity;
    window.external.inflateBlock(emptyLine) = true;
    
    // Вставляем пустую строку перед целевым элементом
    targetElement.insertAdjacentElement("beforeBegin", emptyLine);
    
    // Завершаем запись действий
    window.external.EndUndoUnit(document);
    
    // Вычисляем время выполнения
    var endTime = new Date();
    var execTime = (endTime - startTime) / 1000;
    var execTimeStr = execTime.toFixed(3).replace(".", ",");
    
    // Показываем результат
    if (showMessages) {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "✓ Пустая строка вставлена перед " + targetTypeRuBefore + "\n\n" +
               "Время обработки: " + execTimeStr + " сек");
    }
}
