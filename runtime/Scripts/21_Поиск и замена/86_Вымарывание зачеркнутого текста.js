// Скрипт "Вымарывание зачеркнутого текста" для редактора FBE
// version 1.9
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для замены зачёркнутого текста (strike) на квадратики (■) в fb2 документе.
// Скрипт работает либо со всем документом либо с выделенным фрагментом.
// Зачеркнутые участки текста скрипт заменяет на квадратики (■) и удаляет исходное форматирование
// (зачеркнутость, жирность, курсив, индексы и их совмещение) на местах этих квадратиков.
//  Возможные сноски в "вымаранных участках" сохраняются!

// Символ для замены по умолчанию - ■
// Любой другой удобный символ для замены (кроме пробелов) пользователь может ввести в настройках ниже.
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.9, 21.01.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Вымарывание зачеркнутого текста";
    var version = "1.9";
    
    // ========== НАСТРОЙКИ СКРИПТА ==========
    // Можно менять значения (0 - нет, 1 - да)
    
    // Настройка: Символ для замены по умолчанию - ■
    var replacesign = "■"; // Укажите любой символ (кроме пробелов) при необходимости
    
    // РАЗДЕЛЬНЫЕ НАСТРОЙКИ ДЛЯ РАЗНЫХ РЕЖИМОВ:
    
    // Настройка для работы со ВСЕМ ДОКУМЕНТОМ:
    // 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatisticsForDocument = 1; // По умолчанию: обычный режим
    
    // Настройка для работы с ВЫДЕЛЕННЫМ ФРАГМЕНТОМ:
    // 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatisticsForSelection = 0; // По умолчанию: тихий режим
    
    // ========================================
    
    // Начало замера времени
    var startTime = new Date().getTime();
    
    // Получаем неразрывный пробел из настроек FBE
    var nbspEntity = "&nbsp;";
    var nbspChar = String.fromCharCode(160);
    try {
        var nbspCharTemp = window.external.GetNBSP();
        if (nbspCharTemp.charCodeAt(0) != 160) {
            nbspChar = nbspCharTemp;
            nbspEntity = nbspChar;
        }
    } catch(e) {
        nbspChar = String.fromCharCode(160);
        nbspEntity = "&nbsp;";
    }
    
    // Определяем рабочую область
    var selection = document.selection;
    var range = null;
    var workArea = null;
    var isSelection = false;
    
    // Определяем, есть ли выделение
// Это надежнее, чем парсить htmlText
    if (selection && selection.type == "Text") {
        range = selection.createRange();
        if (range && range.text != "") {
            // Берем parentElement - это самый надежный способ в IE6
            workArea = range.parentElement();
            isSelection = true;
            
            // Если parentElement слишком высоко (например, BODY),
            // пытаемся найти более конкретный элемент
            if (workArea && workArea.tagName && 
                (workArea.tagName.toUpperCase() == "BODY" || 
                 workArea.tagName.toUpperCase() == "HTML")) {
                
                // Пробуем получить более конкретный элемент через commonParentElement
                try {
                    var textRange = range.duplicate();
                    textRange.expand("textedit");
                    var commonParent = textRange.parentElement();
                    if (commonParent && commonParent != workArea) {
                        workArea = commonParent;
                    }
                } catch(e) {
                    // Оставляем workArea как есть
                }
            }
        }
    }
    
    // Если нет выделения, работаем со всем документом
    if (!workArea) {
        workArea = document.body;
    }
    
    // Выбираем нужный режим показа статистики
    var showStatistics = isSelection ? showStatisticsForSelection : showStatisticsForDocument;
    
    // Начинаем блок отмены действий
    window.external.BeginUndoUnit(document, scriptName);
    
    // Счетчики статистики
    var strikeAreasCount = 0;
    var replacedCharsCount = 0;
    var processedElements = 0;
    
    // Функция проверки, является ли символ буквой или цифрой
    function isLetterOrDigit(charCode) {
        return (
            // Русские буквы
            (charCode >= 1040 && charCode <= 1103) || charCode == 1025 || charCode == 1105 ||
            // Латинские буквы
            (charCode >= 65 && charCode <= 90) || (charCode >= 97 && charCode <= 122) ||
            // Цифры
            (charCode >= 48 && charCode <= 57) ||
            // Украинские и другие кириллические буквы
            charCode == 1026 || charCode == 1027 || charCode == 1028 || charCode == 1029 ||
            charCode == 1030 || charCode == 1031 || charCode == 1032 || charCode == 1033 ||
            charCode == 1034 || charCode == 1035 || charCode == 1036 || charCode == 1038 ||
            charCode == 1039 || charCode == 1106 || charCode == 1107 || charCode == 1108 ||
            charCode == 1109 || charCode == 1110 || charCode == 1111 || charCode == 1112 ||
            charCode == 1113 || charCode == 1114 || charCode == 1115 || charCode == 1116 ||
            charCode == 1118 || charCode == 1119
        );
    }
    
    // Функция проверки, является ли узел частью сноски
    function isInsideFootnote(node) {
        while (node) {
            if (node.nodeType == 1) { // ELEMENT_NODE
                if (node.nodeName == "A") {
                    var className = node.className || "";
                    var href = node.getAttribute("href") || "";
                    if (className == "note" || (href.length > 0 && href.charAt(0) == "#")) {
                        return true;
                    }
                }
            }
            node = node.parentNode;
        }
        return false;
    }
    
    // Функция проверки, является ли элемент тегом форматирования
    function isFormattingTag(element) {
        if (!element || element.nodeType != 1) return false;
        
        var tagName = element.nodeName;
        var className = element.className || "";
        
        // Обычные теги форматирования
        if (tagName == "STRONG" || tagName == "EM" || tagName == "U" || 
            tagName == "SUB" || tagName == "SUP" || tagName == "CODE" ||
            tagName == "STRIKE") {
            
            // Исключаем сноски
            if (tagName == "A") {
                var href = element.getAttribute("href") || "";
                if (className == "note" || (href.length > 0 && href.charAt(0) == "#")) {
                    return false;
                }
            }
            return true;
        }
        
        // SPAN с классами форматирования (особенно class="code")
        if (tagName == "SPAN") {
            // Проверяем классы, связанные с форматированием
            if (className.indexOf("code") != -1 || 
                className.indexOf("strong") != -1 ||
                className.indexOf("em") != -1 ||
                className.indexOf("strike") != -1 ||
                className.indexOf("underline") != -1 ||
                className.indexOf("subscript") != -1 ||
                className.indexOf("superscript") != -1) {
                return true;
            }
        }
        
        return false;
    }
    
    // Функция для замены текста в узле
    function replaceTextInNode(node) {
        if (node.nodeType == 3) { // Текстовый узел
            var originalText = node.nodeValue;
            var replacedText = "";
            var changed = false;
            var isFootnote = isInsideFootnote(node);
            
            for (var i = 0; i < originalText.length; i++) {
                var charCode = originalText.charCodeAt(i);
                var char = originalText.charAt(i);
                
                // Если это цифра внутри сноски - НЕ заменяем!
                if (isFootnote && charCode >= 48 && charCode <= 57) {
                    replacedText += char;
                    continue;
                }
                
                // Проверяем, нужно ли заменять символ на квадратик
                // Сохраняем: пробелы, неразрывные пробелы
                if (
                    // Пробелы и неразрывные пробелы
                    charCode == 32 || charCode == 160 || 
                    charCode == 8194 || charCode == 8195 || charCode == 8196 || 
                    charCode == 8197 || charCode == 8198 || charCode == 8239 || 
                    charCode == 8201 || charCode == 8202
                ) {
                    replacedText += char;
                } 
                // Обработка дефисов/тире
                else if (char == "-" || charCode == 8211 || charCode == 8212 || charCode == 45) {
                    // Проверяем соседние символы
                    var prevChar = i > 0 ? originalText.charAt(i - 1) : "";
                    var nextChar = i < originalText.length - 1 ? originalText.charAt(i + 1) : "";
                    var prevIsSpace = prevChar == " " || prevChar.charCodeAt(0) == 160;
                    var nextIsSpace = nextChar == " " || nextChar.charCodeAt(0) == 160;
                    
                    // Если дефис между не-пробелами - заменяем
                    if (!prevIsSpace && !nextIsSpace) {
                        replacedText += replacesign;
                        replacedCharsCount++;
                        changed = true;
                    } else {
                        replacedText += char;
                    }
                }
                // Сохраняем только определенные знаки препинания (по ТЗ)
                else if (
                    char == "." || char == "," || char == "!" || char == "?" ||
                    char == "(" || char == ")" || char == "[" || char == "]" ||
                    char == "{" || char == "}" || char == "\"" || char == "'" ||
                    charCode == 171 || charCode == 187 || charCode == 8220 || 
                    charCode == 8221 || charCode == 8222 || char == ";" ||
                    charCode == 8230 // многоточие
                ) {
                    replacedText += char;
                }
                // Двоеточие заменяем!
                else if (char == ":") {
                    replacedText += replacesign;
                    replacedCharsCount++;
                    changed = true;
                }
                // Все остальное (буквы, цифры, другие символы) заменяем
                else {
                    replacedText += replacesign;
                    replacedCharsCount++;
                    changed = true;
                }
            }
            
            if (changed) {
                node.nodeValue = replacedText;
                return true;
            }
        }
        return false;
    }
    
    // Функция для получения ВСЕХ тегов форматирования, связанных с strike
    function getAllFormattingTags(strikeElement) {
        var tags = [];
        
        // 1. Ищем родительские теги (идут вверх)
        var current = strikeElement;
        while (current.parentNode) {
            current = current.parentNode;
            if (current.nodeType == 1 && isFormattingTag(current)) {
                var alreadyAdded = false;
                for (var i = 0; i < tags.length; i++) {
                    if (tags[i] === current) {
                        alreadyAdded = true;
                        break;
                    }
                }
                if (!alreadyAdded) {
                    tags.push(current);
                }
            }
        }
        
        // 2. Ищем дочерние теги (идут вниз, рекурсивно)
        function findChildFormattingTags(element) {
            if (!element || element.nodeType != 1) return;
            
            if (isFormattingTag(element) && element !== strikeElement) {
                var alreadyAdded = false;
                for (var i = 0; i < tags.length; i++) {
                    if (tags[i] === element) {
                        alreadyAdded = true;
                        break;
                    }
                }
                if (!alreadyAdded) {
                    tags.push(element);
                }
            }
            
            var children = element.childNodes;
            for (var i = 0; i < children.length; i++) {
                findChildFormattingTags(children[i]);
            }
        }
        
        findChildFormattingTags(strikeElement);
        
        return tags;
    }
    
    // Функция для удаления тега форматирования
    function removeFormattingTag(tag) {
        if (!tag || !tag.parentNode) return false;
        
        var parent = tag.parentNode;
        var children = [];
        
        // Собираем всех детей
        for (var i = 0; i < tag.childNodes.length; i++) {
            children.push(tag.childNodes[i]);
        }
        
        // Вставляем детей перед тегом
        for (var i = 0; i < children.length; i++) {
            parent.insertBefore(children[i], tag);
        }
        
        // Удаляем пустой тег
        parent.removeChild(tag);
        
        return true;
    }
    
    // ОСОБАЯ ФУНКЦИЯ ДЛЯ ОБРАБОТКИ SPAN С КЛАССОМ "code"
    function processCodeSpanWithStrike(spanElement, strikeElement) {
        if (spanElement && spanElement.nodeName == "SPAN" && 
            spanElement.className && spanElement.className.indexOf("code") != -1) {
            
            function replaceTextInCode(node) {
                if (node.nodeType == 3) {
                    return replaceTextInNode(node);
                } else if (node.nodeType == 1) {
                    var changed = false;
                    var children = node.childNodes;
                    for (var i = 0; i < children.length; i++) {
                        if (replaceTextInCode(children[i])) {
                            changed = true;
                        }
                    }
                    return changed;
                }
                return false;
            }
            
            replaceTextInCode(strikeElement);
            removeFormattingTag(strikeElement);
            
            return true;
        }
        return false;
    }
    
    // Функция для обработки элемента STRIKE
    function processStrikeElement(strikeElement) {
        strikeAreasCount++;
        
        // ПРОВЕРКА: может быть это SPAN с классом "code"?
        var parent = strikeElement.parentNode;
        var isInCodeSpan = false;
        
        if (parent && parent.nodeName == "SPAN" && 
            parent.className && parent.className.indexOf("code") != -1) {
            isInCodeSpan = true;
        }
        
        // Если strike внутри SPAN с классом "code" - используем специальную обработку
        if (isInCodeSpan) {
            if (processCodeSpanWithStrike(parent, strikeElement)) {
                processedElements++;
                return;
            }
        }
        
        // ОБЫЧНАЯ ОБРАБОТКА
        
        // ШАГ 1: Получаем ВСЕ теги форматирования
        var formattingTags = getAllFormattingTags(strikeElement);
        
        // ШАГ 2: Заменяем текст внутри STRIKE на квадратики
        function replaceTextInElement(element) {
            var changed = false;
            
            if (element.nodeType == 3) {
                if (replaceTextInNode(element)) {
                    changed = true;
                }
            } else if (element.nodeType == 1) {
                var children = element.childNodes;
                for (var i = 0; i < children.length; i++) {
                    if (replaceTextInElement(children[i])) {
                        changed = true;
                    }
                }
            }
            
            return changed;
        }
        
        replaceTextInElement(strikeElement);
        
        // ШАГ 3: Упорядочиваем теги для удаления
        var tagsToRemove = [];
        tagsToRemove.push(strikeElement);
        
        for (var i = 0; i < formattingTags.length; i++) {
            var alreadyAdded = false;
            for (var j = 0; j < tagsToRemove.length; j++) {
                if (tagsToRemove[j] === formattingTags[i]) {
                    alreadyAdded = true;
                    break;
                }
            }
            if (!alreadyAdded) {
                tagsToRemove.push(formattingTags[i]);
            }
        }
        
        // ШАГ 4: Удаляем теги (несколько проходов)
        var maxPasses = 5;
        var pass = 0;
        
        while (tagsToRemove.length > 0 && pass < maxPasses) {
            pass++;
            var removedInThisPass = false;
            
            for (var i = tagsToRemove.length - 1; i >= 0; i--) {
                var tag = tagsToRemove[i];
                
                if (tag && tag.parentNode) {
                    var isAncestor = false;
                    for (var j = 0; j < tagsToRemove.length; j++) {
                        if (i != j && tagsToRemove[j] && 
                            isAncestorOf(tag, tagsToRemove[j])) {
                            isAncestor = true;
                            break;
                        }
                    }
                    
                    if (!isAncestor) {
                        if (removeFormattingTag(tag)) {
                            tagsToRemove.splice(i, 1);
                            removedInThisPass = true;
                        }
                    }
                } else {
                    tagsToRemove.splice(i, 1);
                }
            }
            
            if (!removedInThisPass) {
                break;
            }
        }
        
        processedElements++;
    }
    
    // Вспомогательная функция: проверяет, является ли element1 предком element2
    function isAncestorOf(element1, element2) {
        if (!element1 || !element2) return false;
        
        var current = element2.parentNode;
        while (current) {
            if (current === element1) {
                return true;
            }
            current = current.parentNode;
        }
        return false;
    }
    
    // Поиск всех элементов STRIKE в рабочей области
    function findAllStrikeElements(element, results) {
        if (element.nodeType == 1) { // ELEMENT_NODE
            if (element.nodeName == "STRIKE") {
                results.push(element);
            } else {
                var children = element.childNodes;
                for (var i = 0; i < children.length; i++) {
                    findAllStrikeElements(children[i], results);
                }
            }
        }
    }
    
    // Собираем все STRIKE элементы
    var strikeElements = [];
    findAllStrikeElements(workArea, strikeElements);
    
    // Обрабатываем в обратном порядке
    for (var i = strikeElements.length - 1; i >= 0; i--) {
        processStrikeElement(strikeElements[i]);
    }
    
    // Завершаем блок отмены действий
    window.external.EndUndoUnit(document);
    
    // Конец замера времени
    var endTime = new Date().getTime();
    var executionTime = (endTime - startTime) / 1000;
    
    // Вывод статистики (если включен для текущего режима)
    if (showStatistics) {
        var areaInfo = isSelection ? "выделенный фрагмент" : "весь документ";
        var modeInfo = isSelection ? "(тихий режим по умолчанию)" : "(обычный режим по умолчанию)";
        
        var message = scriptName + "\n" +
                     "ver. " + version + "\n\n" +
                     "Область обработки: " + areaInfo + " " + modeInfo + "\n" +
                     "Участков текста заменено: " + strikeAreasCount + "\n" +
                     "Символов заменено: " + replacedCharsCount + "\n" +
                     "Обработано элементов: " + processedElements + "\n" +
                     "Время выполнения: " + executionTime.toFixed(2) + " сек.";
        
        MsgBox(message);
    }
}
