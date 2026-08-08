// Скрипт "Объединить выделенные подряд идущие цитаты" для редактора FBE
// version 2.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для объединения отдельных выделенных подряд идущих цитат в одну в fb2 документах.
// Соседствующие размеченные цитаты должны идти одна за другой, без каких-либо промежуточных элементов.
// Авторы текста в НЕПОСЛЕДНИХ выделенных цитатах расформатируются от тэгов автора текста
// и форматируются согласно настроек жирным, или курсивом или жирным курсивом или обычным текстом.
// Можно настроить, чтобы цитаты, содержащие авторов текста, вообще не объединялись.
// Предусмотрена настройка вставки пустых строк между бывшими отдельными цитатами при объединении.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 2.1, 09.03.2026
// ======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Объединить выделенные подряд идущие цитаты";
    var version = "2.1";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка режима отображения:
    // 0 - показывать окна сообщений
    // 1 - тихий режим, только сообщения об ошибках
    var showStatistics = 0;
    
    // Вставлять ли пустые строки между бывшими цитатами при преобразовании в единую цитату
    var processEmptyLines = 0;     // 0 - нет, 1 - да
    
    // Объединять ли цитаты с размеченными тэгами авторов текста
    var processTextAuthor = 1;     // 0 - нет, 1 - да
    
    // При объединении цитат с размеченными тэгами авторов текста, 
    // авторов текста всех НЕПОСЛЕДНИХ цитат сделать обычными абзацами
    // 0 - курсивом, 1 - жирным, 3 - жирным курсивом, 4 - обычным текстом
    var processTextAuthorFormat = 3;
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    // Функция для проверки, является ли элемент посторонним (не цитата и не пустая строка)
    function isForeignElement(element) {
        if (!element || element.nodeType != 1) return false;
        
        // Цитата - не посторонний
        if (element.nodeName == "DIV" && element.className == "cite") {
            return false;
        }
        
        // Пустая строка - не посторонний (пропускаем)
        if (element.nodeName == "P") {
            var text = element.innerText || "";
            text = text.replace(/\s/g, "");
            if (text == "" || text == "&nbsp;") {
                return false;
            }
        }
        
        // Всё остальное (включая картинки) - постороннее
        return true;
    }
    
    // Функция для показа сообщений (всегда показываем ошибки, даже в тихом режиме)
    function showMessage(text) {
        MsgBox(scriptName + "\nver. " + version + "\n\n" + text);
    }
    
    // Функция для преобразования числовых настроек в текст
    function getEmptyLinesText(val) {
        return (val == 1) ? "ДА" : "НЕТ";
    }
    
    function getTextAuthorText(val) {
        return (val == 1) ? "ДА" : "НЕТ";
    }
    
    function getFormatText(val) {
        switch (val) {
            case 0: return "КУРСИВ";
            case 1: return "ЖИРНЫЙ";
            case 3: return "ЖИРНЫЙ КУРСИВ";
            case 4: return "ОБЫЧНЫЙ ТЕКСТ";
            default: return "неизвестно";
        }
    }
    
    // Засекаем время начала
    var startTime = new Date();
    
    // Получаем fbw_body
    var fbw_body = document.getElementById("fbw_body");
    if (!fbw_body) {
        showMessage("Ошибка: не найден fbw_body!");
        return;
    }
    
    // Проверяем тип выделения
    if (document.selection.type.toLowerCase() != "none" && 
        document.selection.type.toLowerCase() != "text" && 
        document.selection.type.toLowerCase() != "control") {
        showMessage("Ошибка: неподдерживаемый тип выделения!");
        return;
    }
    
    // Создаём уникальные ID для маркеров
    var randomNum = Math.floor((Math.random()*9)).toString() + 
                    Math.floor((Math.random()*9)).toString() + 
                    Math.floor((Math.random()*9)).toString() + 
                    Math.floor((Math.random()*9)).toString() + 
                    Math.floor((Math.random()*9)).toString() + 
                    Math.floor((Math.random()*9)).toString();
    
    var startMarkerId = "mergeCitesStart_" + randomNum;
    var endMarkerId = "mergeCitesEnd_" + randomNum;
    
    // Получаем выделение
    var range = document.selection.createRange();
    
    // Если выделение пустое
    if (range.compareEndPoints("StartToEnd", range) == 0) {
        showMessage("Ошибка: ничего не выделено!\n\nДля объединения должно быть выделено не меньше 2 цитат.");
        return;
    }
    
    // Начинаем операцию отмены
    window.external.BeginUndoUnit(document, "Объединение цитат");
    
    // Вставляем маркер в начало выделения
    var startRange = range.duplicate();
    startRange.collapse(true);
    startRange.pasteHTML("<B id='" + startMarkerId + "'></B>");
    
    // Вставляем маркер в конец выделения
    var endRange = range.duplicate();
    endRange.collapse(false);
    endRange.pasteHTML("<B id='" + endMarkerId + "'></B>");
    
    // Находим маркеры
    var startMarker = document.getElementById(startMarkerId);
    var endMarker = document.getElementById(endMarkerId);
    
    if (!startMarker || !endMarker) {
        if (startMarker) startMarker.removeNode(true);
        if (endMarker) endMarker.removeNode(true);
        window.external.EndUndoUnit(document);
        showMessage("Ошибка: не удалось вставить маркеры!");
        return;
    }
    
    // Поднимаемся от стартового маркера до первой цитаты
    var firstCite = startMarker;
    while (firstCite && firstCite != fbw_body) {
        if (firstCite.nodeName == "DIV" && firstCite.className == "cite") {
            break;
        }
        firstCite = firstCite.parentNode;
    }
    
    // Поднимаемся от конечного маркера до последней цитаты
    var lastCite = endMarker;
    while (lastCite && lastCite != fbw_body) {
        if (lastCite.nodeName == "DIV" && lastCite.className == "cite") {
            break;
        }
        lastCite = lastCite.parentNode;
    }
    
    // Проверяем, есть ли цитаты в выделении
    var hasCites = (firstCite && firstCite != fbw_body && firstCite.nodeName == "DIV" && firstCite.className == "cite") ||
                   (lastCite && lastCite != fbw_body && lastCite.nodeName == "DIV" && lastCite.className == "cite");
    
    // Проверяем, есть ли в выделении что-то кроме цитат
    var startInCite = false;
    var endInCite = false;
    
    if (firstCite && firstCite != fbw_body && firstCite.nodeName == "DIV" && firstCite.className == "cite") {
        startInCite = firstCite.contains(startMarker);
    }
    
    if (lastCite && lastCite != fbw_body && lastCite.nodeName == "DIV" && lastCite.className == "cite") {
        endInCite = lastCite.contains(endMarker);
    }
    
    // Если есть цитаты, но маркеры не в них — значит выделены и цитаты, и что-то ещё
    if (hasCites && (!startInCite || !endInCite)) {
        startMarker.removeNode(true);
        endMarker.removeNode(true);
        window.external.EndUndoUnit(document);
        showMessage("Ошибка: должны быть выделены только размеченные цитаты!");
        return;
    }
    
    // Если нет цитат вообще
    if (!hasCites) {
        startMarker.removeNode(true);
        endMarker.removeNode(true);
        window.external.EndUndoUnit(document);
        showMessage("Ошибка: выделение не содержит цитат!\n\nДолжны быть выделены только размеченные цитаты.");
        return;
    }
    
    // Если не нашли первую цитату
    if (!firstCite || firstCite == fbw_body || firstCite.nodeName != "DIV" || firstCite.className != "cite") {
        startMarker.removeNode(true);
        endMarker.removeNode(true);
        window.external.EndUndoUnit(document);
        showMessage("Ошибка: выделение не содержит цитат!\n\nДолжны быть выделены только размеченные цитаты.");
        return;
    }
    
    // Если не нашли последнюю цитату
    if (!lastCite || lastCite == fbw_body || lastCite.nodeName != "DIV" || lastCite.className != "cite") {
        startMarker.removeNode(true);
        endMarker.removeNode(true);
        window.external.EndUndoUnit(document);
        showMessage("Ошибка: выделение не содержит цитат!\n\nДолжны быть выделены только размеченные цитаты.");
        return;
    }
    
    // Проверяем, что маркеры находятся в правильном порядке
    var current = firstCite;
    var foundLast = false;
    var citesInOrder = [];
    
    while (current) {
        if (current.nodeName == "DIV" && current.className == "cite") {
            citesInOrder[citesInOrder.length] = current;
            
            if (current == lastCite) {
                foundLast = true;
                break;
            }
            
            current = current.nextSibling;
            
            // Пропускаем пустые строки между цитатами
            while (current && current.nodeType == 1 && 
                   current.nodeName == "P" && 
                   (!current.innerText || current.innerText.replace(/\s/g, "") == "")) {
                current = current.nextSibling;
            }
        } else {
            // Если встретили посторонний элемент (включая картинки) - это ошибка
            if (isForeignElement(current)) {
                startMarker.removeNode(true);
                endMarker.removeNode(true);
                window.external.EndUndoUnit(document);
                showMessage("Ошибка: должны быть выделены только размеченные цитаты!");
                return;
            }
            current = current.nextSibling;
        }
    }
    
    // Проверяем, что дошли до последней цитаты и нашли не менее 2
    if (!foundLast || citesInOrder.length < 2) {
        startMarker.removeNode(true);
        endMarker.removeNode(true);
        window.external.EndUndoUnit(document);
        
        if (citesInOrder.length < 2) {
            showMessage("Ошибка: выделено меньше 2 цитат!\n\nДля объединения должно быть выделено не меньше 2 цитат.");
        } else {
            showMessage("Ошибка: выделены не все подряд идущие цитаты!\n\nМежду первой и последней выделенной цитатой есть посторонние элементы.");
        }
        return;
    }
    
    // Проверяем, что маркеры действительно находятся внутри своих цитат
    var startInFirst = firstCite.contains(startMarker);
    var endInLast = lastCite.contains(endMarker);
    
    if (!startInFirst || !endInLast) {
        startMarker.removeNode(true);
        endMarker.removeNode(true);
        window.external.EndUndoUnit(document);
        showMessage("Ошибка: выделение выходит за границы цитат!\n\nВыделите часть текста внутри цитат, которые нужно объединить.");
        return;
    }
    
    // ==================================================
    // ПРОВЕРКА НАЛИЧИЯ АВТОРОВ ТЕКСТА (ЕСЛИ ОБЪЕДИНЕНИЕ ОТКЛЮЧЕНО)
    // ==================================================
    
    // Если объединение цитат с авторами отключено
    if (processTextAuthor == 0) {
        // Проверяем каждую цитату на наличие text-author
        for (var checkIdx = 0; checkIdx < citesInOrder.length; checkIdx++) {
            var citeToCheck = citesInOrder[checkIdx];
            var paragraphs = citeToCheck.getElementsByTagName("P");
            
            for (var pIdx = 0; pIdx < paragraphs.length; pIdx++) {
                if (paragraphs[pIdx].className == "text-author") {
                    // Нашли автора - отменяем операцию
                    startMarker.removeNode(true);
                    endMarker.removeNode(true);
                    window.external.EndUndoUnit(document);
                    showMessage("Объединение цитат с тэгами автор текста отключено в настройках.\n\nИзмените настройку processTextAuthor = 1 или уберите авторов из цитат.");
                    return;
                }
            }
        }
    }
    
    // Удаляем маркеры - они больше не нужны
    startMarker.removeNode(true);
    endMarker.removeNode(true);
    
    // ==================================================
    // СБОР ДАННЫХ ИЗ ВСЕХ ЦИТАТ
    // ==================================================
    
    var allContent = [];           // Массив для хранения всего содержимого
    var lastCiteIndex = citesInOrder.length - 1;  // Индекс последней цитаты
    
    // Счётчики для статистики
    var totalParagraphs = 0;
    var emptyLinesCount = 0;
    
    // Проходим по всем цитатам
    for (var i = 0; i < citesInOrder.length; i++) {
        var currentCiteDiv = citesInOrder[i];
        var children = currentCiteDiv.childNodes;
        
        // Проходим по всем дочерним элементам текущей цитаты
        for (var j = 0; j < children.length; j++) {
            var child = children[j];
            
            // Пропускаем не элементы (текстовые узлы и т.п.)
            if (child.nodeType != 1) continue;
            
            // Клонируем элемент, чтобы сохранить всё форматирование
            var clonedElement = child.cloneNode(true);
            
            // Если это не последняя цитата и включена обработка авторов
            if (processTextAuthor == 1 && i != lastCiteIndex) {
                // Если это параграф с классом text-author
                if (clonedElement.nodeName == "P" && clonedElement.className == "text-author") {
                    
                    // Преобразуем согласно настройкам
                    switch (processTextAuthorFormat) {
                        case 0: // курсивом
                            clonedElement.innerHTML = "<EM>" + clonedElement.innerHTML + "</EM>";
                            break;
                        case 1: // жирным
                            clonedElement.innerHTML = "<STRONG>" + clonedElement.innerHTML + "</STRONG>";
                            break;
                        case 3: // жирным курсивом
                            clonedElement.innerHTML = "<STRONG><EM>" + clonedElement.innerHTML + "</EM></STRONG>";
                            break;
                        case 4: // обычным текстом
                            // ничего не добавляем
                            break;
                    }
                    
                    // Убираем класс text-author
                    clonedElement.removeAttribute("class");
                    clonedElement.removeAttribute("className");
                }
            }
            
            // Добавляем в общий массив
            allContent[allContent.length] = clonedElement;
        }
        
        // Если это не последняя цитата и включены пустые строки между цитатами
        if (processEmptyLines == 1 && i != lastCiteIndex) {
            // Создаём пустой параграф
            var emptyP = document.createElement("P");
            emptyP.innerHTML = "&nbsp;";
            window.external.inflateBlock(emptyP) = true;
            allContent[allContent.length] = emptyP;
        }
    }
    
    // Подсчитываем реальное количество абзацев (тегов P) в итоговом массиве
    // и отдельно считаем пустые строки
    function countParagraphs(elements) {
        var total = 0;
        var empty = 0;
        
        for (var n = 0; n < elements.length; n++) {
            var element = elements[n];
            countInElement(element);
        }
        
        function countInElement(element) {
            // Если сам элемент - это P
            if (element.nodeName == "P") {
                total++;
                
                // Проверяем, пустой ли это параграф
                var text = element.innerText || "";
                text = text.replace(/\s/g, "");
                if (text == "" || text == "&nbsp;") {
                    empty++;
                }
            }
            
            // Рекурсивно проверяем всех детей
            var children = element.childNodes;
            for (var c = 0; c < children.length; c++) {
                var child = children[c];
                if (child.nodeType == 1) { // ELEMENT_NODE
                    countInElement(child);
                }
            }
        }
        
        return { total: total, empty: empty };
    }
    
    // Подсчитываем абзацы
    var counts = countParagraphs(allContent);
    totalParagraphs = counts.total;
    emptyLinesCount = counts.empty;
    
    // ==================================================
    // СОЗДАНИЕ НОВОЙ ЦИТАТЫ
    // ==================================================
    
    // Создаём новый DIV.cite
    var newCite = document.createElement("DIV");
    newCite.className = "cite";
    
    // Добавляем всё содержимое в новую цитату
    for (var k = 0; k < allContent.length; k++) {
        newCite.appendChild(allContent[k]);
    }
    
    // Вставляем новую цитату перед первой старой
    firstCite.parentNode.insertBefore(newCite, firstCite);
    
    // ==================================================
    // УДАЛЕНИЕ СТАРЫХ ЦИТАТ
    // ==================================================
    
    // Удаляем все старые цитаты (в обратном порядке, чтобы не сбить индексы)
    for (var m = citesInOrder.length - 1; m >= 0; m--) {
        citesInOrder[m].removeNode(true);
    }
    
    // Выделяем новую цитату
    try {
        var newRange = document.body.createTextRange();
        if (newRange && "moveToElementText" in newRange) {
            newRange.moveToElementText(newCite);
            newRange.select();
        }
    } catch (e) {
        // Игнорируем ошибки выделения
    }
    
    // Завершаем операцию отмены
    window.external.EndUndoUnit(document);
    
    // ==================================================
    // СТАТИСТИКА
    // ==================================================
    
    // Вычисляем время выполнения
    var endTime = new Date();
    var timeDiff = (endTime - startTime) / 1000; // в секундах
    
    // Форматируем время с 3 знаками после запятой
    var timeStr = timeDiff.toFixed(3);
    timeStr = timeStr.replace(".", ","); // заменяем точку на запятую
    
    // Если включён показ статистики (тихий режим - не показываем)
    if (showStatistics == 0) {
        var message = "✓ Объединено цитат: " + citesInOrder.length + "\n";
        message += "✓ Всего абзацев: " + totalParagraphs + "\n";
        
        if (emptyLinesCount > 0) {
            message += "  • из них пустых строк: " + emptyLinesCount + "\n";
        }
        
        message += "\nТекущие настройки скрипта:\n";
        message += "- Добавлять пустые строки между бывшими цитатами: " + getEmptyLinesText(processEmptyLines) + "\n";
        message += "- Объединять цитаты с авторами текста: " + getTextAuthorText(processTextAuthor) + "\n";
        
        if (processTextAuthor == 1) {
            message += "- Непоследних авторов текста преобразовывать: " + getFormatText(processTextAuthorFormat) + "\n";
        }
        
        message += "\nВремя обработки: " + timeStr + " сек";
        
        showMessage(message);
    }
    
    // Обновляем статус-бар (всегда показываем)
    try {
        window.external.SetStatusBarText("Объединено цитат: " + citesInOrder.length);
    } catch (e) {}
}
