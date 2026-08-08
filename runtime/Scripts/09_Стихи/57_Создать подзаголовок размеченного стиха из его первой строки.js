// Скрипт "Создать подзаголовок размеченного стиха из его первой строки" для редактора FBE
// version 1.0
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для создания подзаголовка перед уже размеченным (одним) стихом
// из первой строки этого стиха в fb2 документах.

// Размеченный (тэгами poem) стих может быть выделен полностью или частично
// или в нем может быть установлен курсор.
// Если перед стихом есть подзаголовок из звездочек или простой абзац со звездочками
// - такой абзац или подзаголовок удаляются.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// Переработанный скрипт "Создать заголовок размеченного стиха из его первой строки" (версия 2.9)
// Все правила форматирования сохранены без изменений.

// Основано на скрипте "Создать (под)заголовки из первых строк стихов, ориентируясь по абзацам-звездочкам"
// уважаемого тов. stokber

// ---------------------------------------------------------------------------
// ПРАВИЛА ОФОРМЛЕНИЯ ПОДЗАГОЛОВКА ИЗ ПЕРВОЙ СТРОКИ СТИХА (сохранены без изменений):
// 1. Удаляются все теги, сноски [N], {N}, <sup>...</sup>, звездочки *
// 2. Неразрывные пробелы заменяются на обычные
// 3. Удаляются внешние кавычки, если вся строка в них, вложенные кавычки сохраняются
// 4. Удаляются конечные знаки препинания: двоеточие (:), точка с запятой (;), запятая (,), точка (.), тире/дефисы с пробелами (— – -)
// 5. Многоточие в начале строки сохраняется
// 6. Если последний символ ! или ? (включая !! ?? !? ?!), добавляется ..»
// 7. Во всех остальных случаях (буквы, цифры), добавляется …»
// 8. В начало всегда добавляется «
// 9. Три точки в конце строки (...) заменяются на символ многоточия (…)

// version 1.0, 15.01.2026
//======================================

function Run() {
    var scriptName = "Создать подзаголовок размеченного стиха из его первой строки";
    var version = "1.0";
    
    // Настройка: 1 - показывать статистику, 0 - не показывать
    var showStatistics = 0; // Измените на 0 для "тихого" режима
    
    var nbspChar, nbspEntity;
    try {
        nbspChar = window.external.GetNBSP();
        if (nbspChar.charCodeAt(0) == 160) nbspEntity = "&nbsp;";
        else nbspEntity = nbspChar;
    } catch(e) {
        nbspChar = String.fromCharCode(160);
        nbspEntity = "&nbsp;";
    }
    
    // Нижеследующая команда задает список необычных пробелов,
    // которые должны обрабатываться наравне с обычными пробелами:
    var unusualSpaces = String.fromCharCode(160) +  // неразрывный пробел
        String.fromCharCode(8194) +  // EN SPACE
        String.fromCharCode(8195) +  // EM SPACE
        String.fromCharCode(8196) +  // THREE-PER-EM SPACE
        String.fromCharCode(8197) +  // FOUR-PER-EM SPACE
        String.fromCharCode(8198) +  // SIX-PER-EM SPACE
        String.fromCharCode(8239) +  // NARROW NO-BREAK SPACE
        String.fromCharCode(8201) +  // THIN SPACE
        String.fromCharCode(8202) +  // HAIR SPACE
        nbspChar;
    
    window.external.BeginUndoUnit(document, "Создание подзаголовка стиха из первой строки");
    
    try {
        var poemElement = findPoemUnderSelection();
        if (!poemElement) {
            MsgBox("Курсор или выделение не находится внутри размеченного стиха (poem).\n\nПожалуйста, поместите курсор внутрь стиха или выделите часть стиха.");
            window.external.EndUndoUnit(document);
            return;
        }
        
        var startTime = new Date().getTime();
        var result = processPoem(poemElement);
        
        if (!result) {
            window.external.EndUndoUnit(document);
            return;
        }
        
        var endTime = new Date().getTime();
        var executionTime = ((endTime - startTime) / 1000).toFixed(2);
        
        if (showStatistics) {
            MsgBox(scriptName + "\nver. " + version + "\n\nПодзаголовок успешно создан.\n\nВремя выполнения: " + executionTime + " сек");
        }
        
    } catch(e) {
        MsgBox("Произошла ошибка: " + e.message + "\n\nСкрипт: " + scriptName + " v" + version);
    }
    
    window.external.EndUndoUnit(document);
    
    function findPoemUnderSelection() {
        try {
            var range = document.selection.createRange();
            if (!range) return null;
            var parent = range.parentElement();
            if (!parent) return null;
            var element = parent;
            while (element && element.nodeName != "BODY") {
                if (element.nodeName == "DIV" && element.className == "poem") return element;
                element = element.parentElement;
            }
            return null;
        } catch(e) { return null; }
    }
    
    function processPoem(poem) {
        if (!poem || poem.nodeName != "DIV" || poem.className != "poem") {
            MsgBox("Ошибка: Неверный элемент poem");
            return false;
        }
        
        // Удаляем абзацы-звездочки перед стихом
        removeStarsBeforePoem(poem);
        
        var firstLine = getFirstLineFromPoem(poem);
        if (!firstLine) {
            MsgBox("Не удалось найти первую строку стиха.");
            return false;
        }
        
        var formattedSubtitle = formatTitleCorrect(firstLine);
        createSubtitleBeforePoem(poem, formattedSubtitle);
        return true;
    }
    
    function removeStarsBeforePoem(poem) {
        var previous = poem.previousSibling;
        var checkCount = 0;
        
        while (previous && checkCount < 5) {
            var shouldRemove = false;
            
            if (previous.nodeName == "P") {
                var content = getElementText(previous);
                var cleanContent = removeAllSpaces(content);
                
                if (/^\*+$/.test(cleanContent)) shouldRemove = true;
                else if (cleanContent.length === 0) shouldRemove = true;
            }
            else if (previous.nodeName == "DIV" && previous.className == "epigraph") {
                // Эпиграф не удаляем, просто прекращаем проверку
                break;
            }
            else {
                // Любой другой элемент (изображение, заголовок и т.д.) - прекращаем проверку
                break;
            }
            
            if (shouldRemove) {
                var toRemove = previous;
                previous = previous.previousSibling;
                toRemove.removeNode(true);
                checkCount++;
            } else break;
        }
    }
    
    function getFirstLineFromPoem(poem) {
        for (var i = 0; i < poem.childNodes.length; i++) {
            var child = poem.childNodes[i];
            if (child.nodeName == "DIV" && child.className == "stanza") {
                for (var j = 0; j < child.childNodes.length; j++) {
                    if (child.childNodes[j].nodeName == "P") return child.childNodes[j];
                }
            }
            else if (child.nodeName == "P") return child;
        }
        return null;
    }
    
    function formatTitleCorrect(firstLineElement) {
        var htmlContent = firstLineElement.innerHTML;
        var result = htmlContent;
        
        result = result.replace(/<sup>.+?<\/sup>|\[\d+\]|\{\d+\}|\*/gi, "");
        result = result.replace(/\r\n/gi, " ");
        result = result.replace(/<\/?[^>]+>/gi, "");
        
        // Заменяем все необычные пробелы на обычные
        for (var i = 0; i < unusualSpaces.length; i++) {
            var spaceChar = unusualSpaces.charAt(i);
            var regex = new RegExp(spaceChar, "g");
            result = result.replace(regex, " ");
        }
        
        // Убираем пробелы в начале и конце
        result = result.replace(/^\s+|\s+$/g, "");
        
        // Проверяем, вся ли строка в кавычках
        var isFullyQuoted = false;
        var quotePatterns = [/^«[^»«]*»$/, /^"[^"]*"$/, /^'[^']*'$/, /^„[^„"]*"$/, /^"[^”]*”$/, /^"[^"]*"$/];
        
        for (var i = 0; i < quotePatterns.length; i++) {
            if (quotePatterns[i].test(result)) {
                isFullyQuoted = true;
                result = result.substring(1, result.length - 1);
                break;
            }
        }
        
        // УДАЛЕНИЕ ВСЕХ ЗНАКОВ ПРЕПИНАНИЯ В КОНЦЕ (кроме ! и ?)
        // Повторяем несколько раз для надежности
        for (var j = 0; j < 3; j++) {
            // Удаляем тире/дефисы с пробелами
            result = result.replace(/\s*[-—–]\s*$/g, "");
            
            // Удаляем запятые, точки, двоеточия, точки с запятой
            result = result.replace(/\s*[.,:;]\s*$/g, "");
            
            // Удаляем многоточия (чтобы не дублировать)
            result = result.replace(/\s*…\s*$/g, "");
            result = result.replace(/\s*\.\.\.\s*$/g, "");
            
            // Удаляем оставшиеся пробелы в конце
            result = result.replace(/\s+$/g, "");
        }
        
        // Убираем пробелы в начале
        result = result.replace(/^\s+/, "");
        
        if (result.length > 0) {
            var lastChar = result.charAt(result.length - 1);
            
            // Если в конце ! или ? - добавляем ..»
            if (/[!?]/.test(lastChar)) {
                result = result + "..»";
            } 
            // Во всех остальных случаях добавляем …»
            else {
                result = result + "…»";
            }
            
            result = "«" + result;
        }
        
        result = result.replace(/\.\.\./gi, "…");
        return result;
    }
    
    function createSubtitleBeforePoem(poem, subtitleText) {
        // Создаем элемент подзаголовка (P class="subtitle")
        var subtitleP = document.createElement("P");
        subtitleP.className = "subtitle";
        subtitleP.innerHTML = subtitleText;
        window.external.inflateBlock(subtitleP) = true;
        
        // Вставляем подзаголовок перед стихом
        poem.parentNode.insertBefore(subtitleP, poem);
    }
    
    function getElementText(element) {
        var text = "";
        function collectText(node) {
            if (node.nodeType == 3) text += node.nodeValue;
            else if (node.nodeType == 1) {
                for (var i = 0; i < node.childNodes.length; i++) collectText(node.childNodes[i]);
            }
        }
        if (element) collectText(element);
        return text;
    }
    
    function removeAllSpaces(text) {
        var result = text;
        for (var i = 0; i < unusualSpaces.length; i++) {
            var spaceChar = unusualSpaces.charAt(i);
            var regex = new RegExp(spaceChar, "g");
            result = result.replace(regex, "");
        }
        result = result.replace(/\s/g, "");
        return result;
    }
}
