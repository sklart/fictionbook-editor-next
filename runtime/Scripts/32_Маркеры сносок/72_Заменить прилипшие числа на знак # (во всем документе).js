// Скрипт «Заменить прилипшие к тексту цифры на знак решетки (#) во всем документе» для редактора FBE 
// version 3.3
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для расстановки возможных маркеров сносок,
// выполненных «прилипшими» цифрами, в fb2 документах - сразу во всем документе.
// Чаще всего такое бывает при конвертации из epub в fb2 при помощи calibre.

// Скрипт имеет некоторую защиту (фильтр исключений), например, по умолчанию, пропускает:
// Короткие слова с заглавной буквы + цифры (А4, MI6, Т34)
// Возможные единицы измерения с квадратами/кубами (м2, м3, км2, мл3 и т.д.)
// Верхние/нижние индексы (<sup>, <sub>)
// При необходимости фильтр исключений можно отключить
// путем закомментаривания соответствующих строк.

// !!! Во избежание ложных замен, лучше не запускать данный скрипт
// на сложных технических текстах, списках литературы, библиографиях и подобном,
// а воспользоваться обычным поиском и заменой.

// version 3.3, 28.11.2025
//======================================

function Run() {
    try {
        var scriptName = "Заменить прилипшие к тексту цифры на # во всем документе";
        var scriptVersion = "3.3";
        var undoMsg = scriptName;
        var statusBarMsg = "Заменяем прилипшие цифры на #…";
        var replaceCount = 0;
        var skippedCount = 0;
        
        window.external.BeginUndoUnit(document, undoMsg);
        try { window.external.SetStatusBarText(statusBarMsg); }
        catch(e) {}
        
        // СОЗДАЕМ КАРТУ ССЫЛОК: запоминаем все текстовые узлы, которые находятся внутри ссылок
        var linkTextNodes = {};
        var footnoteLinks = 0;
        var webLinks = 0;
        
        for (var i = 0; i < document.links.length; i++) {
            var link = document.links[i];
            var linkType = link.getAttribute("type") || "";
            var lhref = link.getAttribute("l:href") || "";
            var href = link.getAttribute("href") || "";
            var innerHTML = link.innerHTML || "";
            
            // УЛУЧШЕННОЕ ОПРЕДЕЛЕНИЕ СНОСОК: проверяем множество признаков
            var isFootnote = (
                linkType === "note" ||                    // type="note"
                lhref.indexOf("#n_") === 0 ||             // l:href="#n_..."
                lhref.indexOf("#_") === 0 ||              // l:href="#_..."
                /^#n_/.test(href) ||                      // href="#n_..."
                /^#_/.test(href) ||                       // href="#_..."
                /^\[\d+\]$/.test(innerHTML) ||           // текст ссылки: [1], [2], etc.
                /^\d+$/.test(innerHTML)                  // текст ссылки: 1, 2, etc. (просто цифры)
            );
            
            if (isFootnote) {
                footnoteLinks++;
            } else {
                webLinks++;
            }
            
            collectTextNodesFromElement(link, linkTextNodes);
        }
        
        function collectTextNodesFromElement(element, map) {
            for (var j = 0; j < element.childNodes.length; j++) {
                var node = element.childNodes[j];
                if (node.nodeType === 3) { // TEXT_NODE
                    map[node] = true; // помечаем текстовый узел как принадлежащий ссылке
                } else if (node.nodeType === 1) { // ELEMENT_NODE
                    collectTextNodesFromElement(node, map);
                }
            }
        }
        
        // Теги, в которых производим замену
        var targetTags = ['p', 'v', 'subtitle', 'text-author'];
        
        // Функция для правильного склонения
        function pluralize(number, one, two, five) {
            number = Math.abs(number) % 100;
            if (number >= 5 && number <= 20) {
                return five;
            }
            number = number % 10;
            if (number === 1) {
                return one;
            }
            if (number >= 2 && number <= 4) {
                return two;
            }
            return five;
        }
        
        // Функция проверки исключений (ПРОВЕРЕННАЯ версия)
        function shouldExclude(textPart, digits) {
            var combined = textPart + digits;
            
            // Исключения для заглавных букв + цифр (химические формулы)
            if (/^[A-ZА-ЯЁ][a-zа-яё]?\d+$/.test(combined) || 
                /^[A-Z]{1,3}\d+$/.test(combined)) {
                return true;
            }
            
            // Исключения ТОЛЬКО для единиц измерения с квадратами/кубами (2, 3)
            var units = ["м", "км", "см", "мм", "дм", "гм", "кг", "г", "мг", "л", "мл"];
            for (var u = 0; u < units.length; u++) {
                if (textPart === units[u] && /^[23]$/.test(digits)) {
                    return true;
                }
            }
            
            return false;
        }
        
        // Функция проверки, является ли элемент индексом
        function isIndexElement(element) {
            if (element.nodeType != 1) return false;
            var tagName = element.nodeName.toLowerCase();
            return tagName == 'sup' || tagName == 'sub';
        }
        
        // Функция проверки, является ли это списком
        function isListElement(textNode) {
            var text = textNode.nodeValue;
            
            // Проверка на нумерованный список
            if (/^([1-9]\d{0,3})[\.\)]\s+/.test(text)) {
                var prevNode = textNode.previousSibling;
                while (prevNode) {
                    if (prevNode.nodeType == 3 && prevNode.nodeValue.replace(/[\s\xA0]/g, '').length > 0) {
                        return false; // Есть текст перед нами - не список
                    }
                    prevNode = prevNode.previousSibling;
                }
                return true;
            }
            
            return false;
        }
        
        // Функция получения всего текстового содержимого элемента
        function getElementTextContent(element) {
            var text = "";
            function traverse(el) {
                for (var i = 0; i < el.childNodes.length; i++) {
                    var child = el.childNodes[i];
                    if (child.nodeType === 3) {
                        text += child.nodeValue;
                    } else if (child.nodeType === 1) {
                        traverse(child);
                    }
                }
            }
            traverse(element);
            return text;
        }
        
        // Функция получения последнего текста из элемента
        function getLastTextFromElement(element) {
            if (element.nodeType == 3) {
                return element.nodeValue;
            }
            
            // Пропускаем индексы
            if (isIndexElement(element)) {
                return "";
            }
            
            // Идем с конца дочерних элементов
            for (var i = element.childNodes.length - 1; i >= 0; i--) {
                var child = element.childNodes[i];
                var result = getLastTextFromElement(child);
                if (result) return result;
            }
            return "";
        }
        
        // Функция поиска предыдущего текста с буквами (ИСПРАВЛЕННАЯ)
        function findPreviousTextWithLetters(currentNode) {
            var node = currentNode.previousSibling;
            
            while (node) {
                if (node.nodeType == 3) { // Текстовый узел
                    var text = node.nodeValue;
                    if (/([A-Za-zА-яЁё])/.test(text)) {
                        return text; // ВОЗВРАЩАЕМ ВЕСЬ ТЕКСТ
                    }
                } else if (node.nodeType == 1) { // Элемент
                    // Пропускаем индексы
                    if (!isIndexElement(node)) {
                        var elementText = getElementTextContent(node); // Используем функцию получения ВСЕГО текста
                        if (elementText && /([A-Za-zА-яЁё])/.test(elementText)) {
                            return elementText; // ВОЗВРАЩАЕМ ВЕСЬ ТЕКСТ
                        }
                    }
                }
                node = node.previousSibling;
            }
            
            return null;
        }
        
        // Функция обработки текстового узла
        function processTextNode(textNode) {
            var text = textNode.nodeValue;
            var parent = textNode.parentNode;
            var wasReplaced = false;
            
            // ПРОВЕРКА: пропускаем индексы (СЧИТАЕМ В СТАТИСТИКЕ ИСКЛЮЧЕНИЙ)
            if (isIndexElement(parent)) {
                skippedCount++;
                return false;
            }
            
            // ПРОВЕРКА: пропускаем списки (СЧИТАЕМ В СТАТИСТИКЕ ИСКЛЮЧЕНИЙ)
            if (isListElement(textNode)) {
                skippedCount++;
                return false;
            }
            
            // СЛУЧАЙ 1: цифры прилипли к буквам в том же текстовом узле
            var newText = text.replace(/([A-Za-zА-яЁё][…,;.»\"!?:%“”«»™©]*)([1-9]\d{0,4})(?![.,]?\d)/g, 
                function(match, textPart, digits) {
                    // НОВОЕ: Пропускаем если это часть ссылки (СЧИТАЕМ ТОЛЬКО ПРИ НАЛИЧИИ ЦИФР)
                    if (linkTextNodes[textNode]) {
                        skippedCount++;
                        return match;
                    }
                    
                    // НОВОЕ ИСКЛЮЧЕНИЕ: защищаем числа в скобках (1870–1938) или [1934...
                    var lastChar = textPart.substr(-1);
                    if (lastChar === '(' || lastChar === '[') {
                        skippedCount++;
                        return match;
                    }
                    
                    // Старая проверка исключений
                    if (shouldExclude(textPart, digits)) {
                        skippedCount++;
                        return match;
                    }
                    replaceCount++;
                    wasReplaced = true;
                    return textPart + '#';
                });
            
            if (newText !== text) {
                textNode.nodeValue = newText;
                return wasReplaced;
            }
            
            // СЛУЧАЙ 2: текстовый узел содержит только цифры (внутри форматирования)
            if (/^([1-9]\d{0,4}[.,!?]?)$/.test(text)) {
                // НОВОЕ: Пропускаем если это часть ссылки
                if (linkTextNodes[textNode]) {
                    skippedCount++;
                    return false;
                }
                
                // Ищем предыдущий текст с буквами
                var previousText = findPreviousTextWithLetters(textNode);
                
                if (previousText) {
                    var digitsOnly = text.replace(/[.,!?]/g, '');
                    if (/^([1-9]\d{0,4})$/.test(digitsOnly)) {
                        // НОВОЕ ИСКЛЮЧЕНИЕ ДЛЯ СЛУЧАЯ 2: защищаем числа после скобок
                        var lastChar = previousText.substr(-1);
                        if (lastChar === '(' || lastChar === '[') {
                            skippedCount++;
                            return false;
                        }
                        
                        if (!shouldExclude(previousText, digitsOnly)) {
                            replaceCount++;
                            textNode.nodeValue = '#';
                            return true;
                        } else {
                            skippedCount++;
                            return false;
                        }
                    }
                }
                return false;
            }
            
            // СЛУЧАЙ 3: цифры находятся в начале текстового узла после закрывающего тега
            if (/^([1-9]\d{0,4}[.,!?]?)/.test(text)) {
                // НОВОЕ: Пропускаем если это часть ссылки
                if (linkTextNodes[textNode]) {
                    skippedCount++;
                    return false;
                }
                
                var previousText = findPreviousTextWithLetters(textNode);
                
                if (previousText) {
                    var match = text.match(/^([1-9]\d{0,4})/);
                    if (match) {
                        var digits = match[1];
                        
                        // НОВОЕ ИСКЛЮЧЕНИЕ ДЛЯ СЛУЧАЯ 3: защищаем числа после скобок
                        var lastChar = previousText.substr(-1);
                        if (lastChar === '(' || lastChar === '[') {
                            skippedCount++;
                            return false;
                        }
                        
                        if (!shouldExclude(previousText, digits)) {
                            replaceCount++;
                            textNode.nodeValue = '#' + text.substr(digits.length);
                            return true;
                        } else {
                            skippedCount++;
                            return false;
                        }
                    }
                }
                return false;
            }
            
            return false;
        }
        
        // Функция обработки дочерних узлов
        function processChildNodes(element) {
            var nodes = element.childNodes;
            
            for (var j = 0; j < nodes.length; j++) {
                var currentNode = nodes[j];
                
                if (currentNode.nodeType === 3) { // TEXT_NODE
                    processTextNode(currentNode);
                    
                } else if (currentNode.nodeType === 1) { // ELEMENT_NODE
                    // Рекурсивно обрабатываем вложенные элементы
                    processChildNodes(currentNode);
                }
            }
        }
        
        // Обрабатываем каждый целевой тег
        for (var t = 0; t < targetTags.length; t++) {
            var tagName = targetTags[t];
            var elements = document.getElementsByTagName(tagName);
            
            // Обрабатываем каждый элемент этого тега
            for (var i = 0; i < elements.length; i++) {
                var element = elements[i];
                
                // Обрабатываем все дочерние узлы
                processChildNodes(element);
            }
        }
        
        // Формируем сообщение со статистикой
        var message = scriptName + "\n";
        message += "Version: " + scriptVersion + "\n\n";
        
        var replaceText = pluralize(replaceCount, "замена", "замены", "замен");
        message += "Произведено " + replaceCount + " " + replaceText + ".\n\n";
        
        if (skippedCount > 0) {
            var skippedText = pluralize(skippedCount, "потенциальная замена", "потенциальные замены", "потенциальных замен");
            message += "Пропущено " + skippedCount + " " + skippedText + " (фильтр исключений).\n";
        }
        
        // РАЗДЕЛЬНАЯ СТАТИСТИКА ПО ССЫЛКАМ
        if (footnoteLinks > 0) {
            var footnotesText = pluralize(footnoteLinks, "сноска", "сноски", "сносок");
            message += "Обнаружено " + footnoteLinks + " " + footnotesText + " (не обрабатываются).\n";
        }
        
        if (webLinks > 0) {
            var linksText = pluralize(webLinks, "внешняя ссылка", "внешние ссылки", "внешних ссылок");
            message += "Обнаружено " + webLinks + " " + linksText + " (не обрабатываются).\n";
        }
        
        message += "\nОбработаны только теги: <p>, <v>, <subtitle>, <text-author>";
        message += "\nЗаменяются только целые числа от 1 до 5 цифр подряд, прилипшие к тексту.";
        message += "\n\nФИЛЬТР ИСКЛЮЧЕНИЙ: не обрабатываются:";
        message += "\n• Короткие слова с заглавной буквы + цифры (А4, MI6, Т34)";
        message += "\n• Единицы измерения с квадратами/кубами (м2, км3, см2 и т.д.)";
        message += "\n• Верхние/нижние индексы (<sup>, <sub>)";
        message += "\n• Нумерованные списки";
        message += "\n• Числа в круглых и квадратных скобках";
        message += "\n• Текст внутри ссылок и сносок";
        
        try { window.external.SetStatusBarText("Заменено прилипших цифр: " + replaceCount + "."); }
        catch(e) {} 
        
        window.external.EndUndoUnit(document);
        MsgBox(message, "FBE скрипт");
        
    } catch (e) {
        try { window.external.EndUndoUnit(document); } catch(e2) {}
        MsgBox('Ошибка: ' + e.message, "FBE скрипт");
    }
}
