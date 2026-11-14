// Скрипт "Сформатировать эпиграф стихом" для редактора FBE
// version 1.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для форматирования эпиграфа как стиха (с сохранением авторства текста) в fb2 документах .
// Скрипт обрабатывает эпиграф под курсором или при частичном выделении эпиграфа.
// Текст стиха разделяется на строфы по пустым строкам.
// Сохраняются тэги автора текста text-author с их форматированием.
// Соблюдается правильная вкладка тегов: эпиграф → цитата → стих → строфы.

// version 1.1, 10.02.2024
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Сформатировать эпиграф стихом";
    var version = "1.1";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 0; // Измените на 1 для показа статистики
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    var startTime = new Date().getTime();
    
    try { var nbspChar=window.external.GetNBSP(); var nbspEntity; if (nbspChar.charCodeAt(0)==160) nbspEntity="&nbsp;"; else nbspEntity=nbspChar;}
    catch(e) { var nbspChar=String.fromCharCode(160); var nbspEntity="&nbsp;";}
    
    // Функция для проверки, является ли строка пустой (только пробелы, неразрывные пробелы или вообще без текста)
    function isEmptyParagraph(pElement) {
        if (!pElement || pElement.nodeName != "P") return false;
        
        var textContent = "";
        // Собираем весь текст из абзаца
        function collectText(node) {
            if (node.nodeType == 3) { // TEXT_NODE
                textContent += node.nodeValue;
            } else if (node.nodeType == 1) { // ELEMENT_NODE
                // Проверяем, не является ли это сноской или комментарием
                if (node.nodeName == "A") {
                    var className = node.className || "";
                    var href = node.getAttribute("href") || "";
                    if (className == "note" || (href.length > 0 && href.charAt(0) == "#")) {
                        // Это сноска - не считаем ее содержимое при проверке пустоты
                        return;
                    }
                }
                // Рекурсивно проверяем детей
                for (var i = 0; i < node.childNodes.length; i++) {
                    collectText(node.childNodes[i]);
                }
            }
        }
        
        collectText(pElement);
        
        // Удаляем все пробелы, неразрывные пробелы и спецсимволы
        var cleanText = textContent.replace(new RegExp("[\\s" + nbspChar + "]+", "g"), "");
        return cleanText.length == 0;
    }
    
    // Функция для проверки, является ли элемент тегом автора
    function isTextAuthorElement(element) {
        if (element.nodeName != "P") return false;
        var className = element.className || "";
        return className == "text-author";
    }
    
    // Функция для поиска эпиграфа, содержащего элемент
    function findEpigraphContainingElement(element) {
        var current = element;
        while (current && current.nodeName != "BODY") {
            if (current.nodeName == "DIV" && current.className == "epigraph") {
                return current;
            }
            current = current.parentNode;
        }
        return null;
    }
    
    // Функция для проверки, содержит ли эпиграф уже тег poem
    function containsPoem(epigraph) {
        if (!epigraph) return false;
        
        function checkChildren(node) {
            if (node.nodeType == 1) { // ELEMENT_NODE
                if (node.nodeName == "DIV" && node.className == "poem") {
                    return true;
                }
                for (var i = 0; i < node.childNodes.length; i++) {
                    if (checkChildren(node.childNodes[i])) {
                        return true;
                    }
                }
            }
            return false;
        }
        
        return checkChildren(epigraph);
    }
    
    // Функция для проверки, содержит ли эпиграф уже тег cite
    function containsCite(epigraph) {
        if (!epigraph) return false;
        
        function checkChildren(node) {
            if (node.nodeType == 1) { // ELEMENT_NODE
                if (node.nodeName == "DIV" && node.className == "cite") {
                    return node; // Возвращаем сам элемент cite
                }
                for (var i = 0; i < node.childNodes.length; i++) {
                    var result = checkChildren(node.childNodes[i]);
                    if (result) return result;
                }
            }
            return null;
        }
        
        return checkChildren(epigraph);
    }
    
    // Основная функция форматирования эпиграфа
    function formatEpigraphAsPoem(epigraph) {
        if (!epigraph) return false;
        
        // Проверяем, не содержит ли уже эпиграф тег poem
        if (containsPoem(epigraph)) {
            MsgBox("Этот эпиграф уже содержит стих (тег <poem>).\n\n" + 
                   "Скрипт не будет выполнять преобразование.", "FBE скрипт");
            return false;
        }
        
        // Проверяем, содержит ли эпиграф тег cite
        var existingCite = containsCite(epigraph);
        var hasCite = (existingCite != null);
        
        // Собираем все дочерние элементы эпиграфа
        // Если есть cite, собираем элементы из него
        var sourceElement = hasCite ? existingCite : epigraph;
        var children = sourceElement.childNodes;
        var paragraphs = [];
        var authorParagraphs = [];
        var currentStanza = [];
        var allStanzas = [];
        var foundAuthorSection = false;
        
        // Проходим по всем элементам
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            
            if (child.nodeType == 1) { // ELEMENT_NODE
                // Проверяем, является ли это тегом автора
                if (isTextAuthorElement(child)) {
                    foundAuthorSection = true;
                    authorParagraphs.push(child);
                    continue;
                }
                
                // Если мы уже нашли автора, но встретили другой элемент - это ошибка структуры
                if (foundAuthorSection) {
                    // Элементы после автора - пропускаем их
                    continue;
                }
                
                // Для тега P (не автора)
                if (child.nodeName == "P") {
                    // Проверяем, пустой ли абзац (разделитель строф)
                    if (isEmptyParagraph(child)) {
                        // Сохраняем текущую строфу, если в ней есть строки
                        if (currentStanza.length > 0) {
                            allStanzas.push(currentStanza);
                            currentStanza = [];
                        }
                        // Пустой абзац-разделитель не добавляем никуда
                    } else {
                        currentStanza.push(child);
                    }
                }
            }
        }
        
        // Добавляем последнюю строфу, если она не пустая
        if (currentStanza.length > 0) {
            allStanzas.push(currentStanza);
        }
        
        // Если нет ни одной строфы - значит, в эпиграфе только автор или ничего нет
        if (allStanzas.length == 0) {
            MsgBox("В эпиграфе не найдено текста для преобразования в стих.\n\n" +
                   "Возможно, эпиграф содержит только автора или пустые абзацы.", "FBE скрипт");
            return false;
        }
        
        // Создаем poem
        var poemDiv = document.createElement("DIV");
        poemDiv.className = "poem";
        
        // Создаем строфы
        for (var i = 0; i < allStanzas.length; i++) {
            var stanza = allStanzas[i];
            
            var stanzaDiv = document.createElement("DIV");
            stanzaDiv.className = "stanza";
            
            for (var j = 0; j < stanza.length; j++) {
                stanzaDiv.appendChild(stanza[j]);
            }
            
            poemDiv.appendChild(stanzaDiv);
        }
        
        // Добавляем авторов
        for (var i = 0; i < authorParagraphs.length; i++) {
            poemDiv.appendChild(authorParagraphs[i]);
        }
        
        if (hasCite) {
            // Если был cite, очищаем его и добавляем poem внутрь
            while (existingCite.firstChild) {
                existingCite.removeChild(existingCite.firstChild);
            }
            existingCite.appendChild(poemDiv);
            // Сам cite уже находится внутри epigraph, ничего больше не делаем
        } else {
            // Если не было cite, очищаем эпиграф и создаем структуру
            while (epigraph.firstChild) {
                epigraph.removeChild(epigraph.firstChild);
            }
            epigraph.appendChild(poemDiv);
        }
        
        return true;
    }
    
    // Основная логика скрипта
    try {
        var statusBarMsg = "Форматируем эпиграф стихом...";
        var undoMsg = "Форматирование эпиграфа стихом";
        
        window.external.BeginUndoUnit(document, undoMsg);
        try { window.external.SetStatusBarText(statusBarMsg); }
        catch(e) {}
        
        // Получаем текущее выделение
        var selection = document.selection;
        var range = selection.createRange();
        
        // Проверяем, что выделение в теле книги
        if (range.parentElement().nodeName == "TEXTAREA" || range.parentElement().nodeName == "INPUT") {
            MsgBox("Ошибка: выделение должно быть в тексте книги, а не в поле ввода.", "FBE скрипт");
            window.external.EndUndoUnit(document);
            return;
        }
        
        // Находим элемент, с которого начинается выделение
        var startElement = range.parentElement();
        
        // Ищем эпиграф, содержащий этот элемент
        var epigraph = findEpigraphContainingElement(startElement);
        
        if (!epigraph) {
            MsgBox("Курсор или выделение не находится внутри эпиграфа.\n\n" +
                   "Пожалуйста, поместите курсор внутрь эпиграфа или выделите его часть.", "FBE скрипт");
            window.external.EndUndoUnit(document);
            return;
        }
        
        // Форматируем эпиграф
        var success = formatEpigraphAsPoem(epigraph);
        
        var endTime = new Date().getTime();
        var executionTime = (endTime - startTime) / 1000;
        
        if (success && showStatistics) {
            var statsMessage = scriptName + "\n" +
                               "ver. " + version + "\n" +
                               "---------------------------\n" +
                               "✓ Эпиграф успешно оформлен как стих.\n\n" +
                               "Время обработки: " + executionTime.toFixed(3).replace(".", ",") + " сек";
            
            MsgBox(statsMessage, "FBE скрипт");
        }
        
        try { window.external.SetStatusBarText("Готово"); }
        catch(e) {}
        
        window.external.EndUndoUnit(document);
        
    } catch(err) {
        try { window.external.SetStatusBarText("Ошибка"); }
        catch(e) {}
        
        if (window.external.EndUndoUnit) {
            window.external.EndUndoUnit(document);
        }
        
        MsgBox("Произошла ошибка при выполнении скрипта:\n\n" + err.toString(), "FBE скрипт - Ошибка");
    }
}
