// Скрипт "Определить структурный элемент под курсором" для редактора FBE
// version 2.4
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для показа html кода элементов в месте установки курсора или в выделенном фрагменте.

// version 2.4, 21.01.2026
//======================================
/*
СПРАВКА ПО СТРУКТУРНЫМ ЭЛЕМЕНТАМ FB2:

1. Подзаголовок <P class=subtitle>:
   - Может быть сам по себе среди обычных абзацев
   - Может быть внутри блочных элементов: аннотация, стих, цитата
   - Внутри других блочных элементов подзаголовков НЕ БЫВАЕТ!

2. Автор текста <P class=text-author>:
   - НЕ может быть сам по себе!
   - Может быть только внутри (в самом конце) блочных элементов:
     * Стих (poem)
     * Цитата (cite) 
     * Эпиграф (epigraph)
   - Внутри других блочных элементов автор текста НЕ БЫВАЕТ!

3. Стих (poem) и строфа (stanza):
   - Строфа ВСЕГДА внутри стиха
   - Не может быть строфы без стиха
   - Приоритет при определении: poem > stanza

4. Основные блочные элементы DIV:
   - body (основной раздел, сноски, комментарии)
   - section (секция)
   - title (заголовок)
   - annotation (аннотация) - может быть основная, ко всей книге или к любой секции
   - history (история изменений документа) видимая часть в режиме html
   - epigraph (эпиграф)
   - poem (стих)
   - stanza (строфа, только внутри poem)
   - cite (цитата)
   - image (изображение)
   - table (таблица)

5. Ссылки:
   - class="note" - ссылка на примечание
   - href="#n_X" - ссылка на примечание X
   - href="#c_X" - ссылка на комментарий X
*/

function Run() {
    // Название и версия для сообщений
    var scriptName = "Определить структурный элемент под курсором";
    var version = "2.4";
    
    // Начало замера времени
    var startTime = new Date().getTime();
    
    // Получаем текущий выбор
    var sel = document.selection;
    if (!sel) {
        MsgBox("Ошибка: Не удалось получить объект выделения!");
        return;
    }
    
    var range = null;
    try {
        range = sel.createRange();
    } catch(e) {
        MsgBox("Ошибка получения диапазона выделения: " + e.message);
        return;
    }
    
    if (!range) {
        MsgBox("Не удалось создать диапазон выделения!");
        return;
    }
    
    // Получаем родительский элемент
    var parentElement = null;
    try {
        if (range.item) {
            parentElement = range.item(0);
        } else {
            parentElement = range.parentElement();
        }
    } catch(e) {
        try {
            parentElement = range.commonParentElement();
        } catch(e2) {
            MsgBox("Не удалось определить элемент под курсором!");
            return;
        }
    }
    
    if (!parentElement) {
        MsgBox("Не удалось определить элемент под курсором!");
        return;
    }
    
    // Функция для поиска подстроки
    function findString(haystack, needle) {
        for (var i = 0; i <= haystack.length - needle.length; i++) {
            var found = true;
            for (var j = 0; j < needle.length; j++) {
                if (haystack.charAt(i + j) != needle.charAt(j)) {
                    found = false;
                    break;
                }
            }
            if (found) return i;
        }
        return -1;
    }
    
    // Функция для создания строки с повторением
    function repeatString(str, count) {
        var result = "";
        for (var i = 0; i < count; i++) {
            result += str;
        }
        return result;
    }
    
    // Функция для получения текстового содержимого
    function getElementText(element) {
        var text = "";
        if (!element) return text;
        
        if (element.nodeType == 3) {
            text = element.nodeValue;
        } else if (element.nodeType == 1) {
            if (element.innerText) {
                text = element.innerText;
            } else if (element.textContent) {
                text = element.textContent;
            }
        }
        return text;
    }
    
    // Функция для получения номера из ссылки
    function getLinkNumber(element) {
        if (!element || element.nodeName != "A") return "";
        
        var href = element.getAttribute("href") || "";
        if (!href) return "";
        
        // Ищем номер после #n_ или #c_
        var nPos = findString(href, "#n_");
        var cPos = findString(href, "#c_");
        
        if (nPos != -1) {
            var start = nPos + 3;
            var end = href.length;
            for (var i = start; i < href.length; i++) {
                var ch = href.charAt(i);
                if (ch < '0' || ch > '9') {
                    end = i;
                    break;
                }
            }
            return href.substring(start, end);
        }
        
        if (cPos != -1) {
            var start = cPos + 3;
            var end = href.length;
            for (var i = start; i < href.length; i++) {
                var ch = href.charAt(i);
                if (ch < '0' || ch > '9') {
                    end = i;
                    break;
                }
            }
            return href.substring(start, end);
        }
        
        return "";
    }
    
    // Функция для поиска ближайшего блочного элемента с правильными приоритетами
    function findNearestBlockElement(startElement) {
        var current = startElement;
        var bestElement = null;
        var bestPriority = 0;
        
        // Приоритеты элементов (чем выше число, тем выше приоритет)
        function getElementPriority(el) {
            if (!el || el.nodeType != 1) return 0;
            if (el.nodeName != "DIV") return 0;
            
            var className = el.className || "";
            
            switch(className) {
                case "poem": return 100;    // poem важнее всех
                case "cite": return 90;
                case "epigraph": return 80;
                case "annotation": return 70;
                case "title": return 60;
                case "section": return 50;
                case "history": return 45;  // история
                case "stanza": return 40;   // stanza менее важен
                case "table": return 30;
                case "image": return 20;
                case "tr": return 10;
                case "td":
                case "th": return 5;
                case "body": return 1;
                default: return 0;
            }
        }
        
        // Проверяем всех родителей
        while (current && current != document.body) {
            if (current.nodeType == 1 && current.nodeName == "DIV" && current.className) {
                var priority = getElementPriority(current);
                if (priority > bestPriority) {
                    bestPriority = priority;
                    bestElement = current;
                }
            }
            current = current.parentNode;
        }
        
        return bestElement;
    }
    
    // Находим ближайший блочный элемент
    var blockElement = findNearestBlockElement(parentElement);
    
    // Определяем, что показывать как основной элемент
    var displayElement = blockElement ? blockElement : parentElement;
    
    // Определяем тип выбора
    var selectionType = "Курсор установлен в:";
    try {
        var hasText = range.text && range.text.length > 0;
        if (hasText) {
            selectionType = "Выделение в:";
        }
    } catch(e) {
        selectionType = "Выделение в:";
    }
    
    // Функция для определения типа элемента с учетом контекста
    function getElementType(element) {
        if (!element || element.nodeType != 1) return "Неизвестный";
        
        var tagName = element.nodeName.toUpperCase();
        var className = element.className || "";
        
        // Определяем родительский элемент для контекста
        var parent = element.parentNode;
        var parentClassName = "";
        if (parent && parent.nodeType == 1 && parent.nodeName == "DIV") {
            parentClassName = parent.className || "";
        }
        
        if (tagName == "DIV") {
            if (className == "body") {
                var fbname = element.getAttribute("fbname") || "";
                if (fbname == "notes") return "Раздел сносок";
                if (fbname == "comments") return "Раздел комментариев";
                return "Основной раздел";
            }
            if (className == "section") return "Секция";
            if (className == "title") return "Заголовок";
            if (className == "annotation") {
                // Определяем, основная это аннотация или к секции
                var parent = element.parentNode;
                if (parent && parent.nodeName == "DIV") {
                    if (parent.className == "section") {
                        return "Аннотация (к секции)";
                    }
                    // Проверяем, не находится ли аннотация прямо в body
                    var grandParent = parent.parentNode;
                    if (grandParent && grandParent.nodeName == "DIV" && 
                        (grandParent.className == "body" || grandParent.id == "fbw_body")) {
                        return "Аннотация (основная)";
                    }
                }
                return "Аннотация";
            }
            if (className == "history") return "История изменений";
            if (className == "epigraph") return "Эпиграф";
            if (className == "poem") return "Стих";
            if (className == "stanza") return "Строфа";
            if (className == "cite") return "Цитата";
            if (className == "image") return "Изображение";
            if (className == "table") return "Таблица";
            if (className == "tr") return "Строка таблицы";
            if (className == "td" || className == "th") return "Ячейка таблицы";
            return "Блочный элемент DIV";
        }
        
        if (tagName == "P") {
            if (className == "subtitle") {
                // Подзаголовок с контекстом
                if (parentClassName == "poem") return "Подзаголовок стиха";
                if (parentClassName == "cite") return "Подзаголовок цитаты";
                if (parentClassName == "annotation") {
                    // Уточняем тип аннотации
                    var annotationParent = element.parentNode;
                    if (annotationParent && annotationParent.parentNode) {
                        var annParent = annotationParent.parentNode;
                        if (annParent.className == "section") {
                            return "Подзаголовок аннотации (к секции)";
                        } else if (annParent.className == "body" || annParent.id == "fbw_body") {
                            return "Подзаголовок аннотации (основной)";
                        }
                    }
                    return "Подзаголовок аннотации";
                }
                if (parentClassName == "epigraph") return "Подзаголовок эпиграфа";
                if (parentClassName == "history") return "Подзаголовок истории";
                return "Подзаголовок";
            }
            if (className == "text-author") {
                // Автор текста с контекстом
                if (parentClassName == "poem") return "Автор текста стиха";
                if (parentClassName == "cite") return "Автор текста цитаты";
                if (parentClassName == "epigraph") return "Автор текста эпиграфа";
                return "Автор текста";
            }
            if (className == "") return "Обычный абзац";
            return "Абзац";
        }
        
        if (tagName == "IMG") return "Изображение";
        
        if (tagName == "A") {
            var href = element.getAttribute("href") || "";
            var cls = element.className || "";
            
            // Определяем тип ссылки
            if (cls == "note") {
                var num = getLinkNumber(element);
                if (num) return "Ссылка на примечание [" + num + "]";
                return "Ссылка на примечание";
            }
            
            if (findString(href, "#n_") != -1) {
                var num = getLinkNumber(element);
                if (num) return "Ссылка на примечание [" + num + "]";
                return "Ссылка на примечание";
            }
            if (findString(href, "#c_") != -1) {
                var num = getLinkNumber(element);
                if (num) return "Ссылка на комментарий {" + num + "}";
                return "Ссылка на комментарий";
            }
            if (href.charAt(0) == "#") return "Внутренняя ссылка";
            if (findString(href, "http") == 0) return "Внешняя ссылка";
            return "Ссылка";
        }
        
        if (tagName == "SPAN") {
            if (className == "image") return "Встроенное изображение";
            if (className == "code") return "Код";
            return "Встроенный элемент";
        }
        
        if (tagName == "SUP") {
            // Проверяем, является ли частью ссылки
            var parent = element.parentNode;
            if (parent && parent.nodeName == "A") {
                var href = parent.getAttribute("href") || "";
                if (findString(href, "#n_") != -1) {
                    var num = getLinkNumber(parent);
                    if (num) return "Верхний индекс (ссылка на примечание [" + num + "])";
                    return "Верхний индекс (ссылка на примечание)";
                }
                if (findString(href, "#c_") != -1) {
                    var num = getLinkNumber(parent);
                    if (num) return "Верхний индекс (ссылка на комментарий {" + num + "})";
                    return "Верхний индекс (ссылка на комментарий)";
                }
            }
            return "Верхний индекс";
        }
        
        if (tagName == "EM" || tagName == "I") {
            var text = getElementText(element);
            if (text && text.length > 20) text = text.substring(0, 20) + "...";
            if (text) return "Курсив: \"" + text + "\"";
            return "Курсив";
        }
        
        if (tagName == "STRONG" || tagName == "B") {
            var text = getElementText(element);
            if (text && text.length > 20) text = text.substring(0, 20) + "...";
            if (text) return "Жирный: \"" + text + "\"";
            return "Жирный";
        }
        
        return "Элемент " + tagName;
    }
    
    // Функция для получения HTML содержимого
    function getElementHTML(element) {
        if (!element || element.nodeType != 1) return "";
        
        var html = "";
        try {
            html = element.outerHTML;
        } catch(e) {
            return "";
        }
        
        if (!html) return "";
        
        // Заменяем неразрывные пробелы
        html = html.replace(/\u00A0/g, "&nbsp;");
        
        // Сокращаем длинный HTML
        if (html.length > 300) {
            html = html.substring(0, 300) + "...";
        }
        
        return html;
    }
    
    // Функция для определения категории элемента
    function getElementCategory(element) {
        if (!element || element.nodeType != 1) return "элементе";
        
        var tagName = element.nodeName.toUpperCase();
        var className = element.className || "";
        
        if (tagName == "DIV") {
            if (className == "poem" || className == "cite" || className == "epigraph" || 
                className == "annotation" || className == "title" || className == "section" ||
                className == "history" || className == "stanza" || className == "table" || 
                className == "image") {
                return "блочном элементе";
            }
            return "блочном элементе";
        }
        
        if (tagName == "P") {
            if (className == "subtitle" || className == "text-author") {
                return "абзацном элементе";
            }
            return "абзацном элементе";
        }
        
        if (tagName == "IMG") return "элементе изображения";
        if (tagName == "A") return "элементе ссылки";
        if (tagName == "SPAN") return "встроенном элементе";
        
        return "элементе";
    }
    
    // Функция для получения дополнительных деталей об элементе
    function getElementDetails(element) {
        var details = [];
        
        if (!element || element.nodeType != 1) return details;
        
        var tagName = element.nodeName;
        var className = element.className || "";
        
        // Для ссылок
        if (tagName == "A") {
            var href = element.getAttribute("href") || "";
            if (href) {
                details.push("Адрес: " + href);
                
                // Определяем тип ссылки
                if (findString(href, "#n_") != -1) {
                    details.push("Тип: ссылка на примечание");
                    var num = getLinkNumber(element);
                    if (num) details.push("Номер: " + num);
                } else if (findString(href, "#c_") != -1) {
                    details.push("Тип: ссылка на комментарий");
                    var num = getLinkNumber(element);
                    if (num) details.push("Номер: " + num);
                } else if (className == "note") {
                    details.push("Тип: сноска");
                    var num = getLinkNumber(element);
                    if (num) details.push("Номер примечания: " + num);
                }
            }
            
            if (element.id) details.push("ID: " + element.id);
            if (className && className != "") details.push("Класс: " + className);
        }
        
        // Для изображений
        if (tagName == "IMG" || className == "image") {
            var src = element.getAttribute("src") || "";
            if (src) {
                details.push("Источник: " + src);
                var hashPos = findString(src, "#");
                if (hashPos != -1) {
                    var imgId = src.substring(hashPos + 1);
                    details.push("ID изображения: " + imgId);
                }
            }
        }
        
        return details;
    }
    
    // Собираем информацию об элементе
    var elementInfo = [];
    elementInfo.push("Определить структурный элемент под курсором");
    elementInfo.push("ver. " + version);
    elementInfo.push("====================================");
    elementInfo.push("");
    
    // Информация о типе выбора
    elementInfo.push("● ТИП ВЫБОРА:");
    elementInfo.push("");
    elementInfo.push("  " + selectionType);
    
    var elementCategory = getElementCategory(displayElement);
    var elementType = getElementType(displayElement);
    
    elementInfo.push("  " + elementCategory);
    elementInfo.push("  (" + elementType + ")");
    elementInfo.push("");
    
    // Основная информация
    elementInfo.push("● ОСНОВНОЙ ЭЛЕМЕНТ:");
    elementInfo.push("");
    
    elementInfo.push("  Тип: " + elementType);
    elementInfo.push("  Тег: " + displayElement.nodeName);
    
    var className = displayElement.className || "(нет класса)";
    elementInfo.push("  Класс: " + className);
    
    var id = displayElement.id || "(нет ID)";
    elementInfo.push("  ID: " + id);
    
    // Текстовое содержимое
    var textContent = getElementText(displayElement);
    if (textContent && textContent.replace(/\s/g, '').length > 0) {
        if (textContent.length > 60) {
            textContent = textContent.substring(0, 60) + "...";
        }
        elementInfo.push("  Текст: \"" + textContent + "\"");
    }
    
    // HTML содержимое
    var htmlContent = getElementHTML(displayElement);
    if (htmlContent) {
        elementInfo.push("  HTML: " + htmlContent);
    }
    
    // Дополнительные детали (особенно для ссылок)
    var details = getElementDetails(displayElement);
    if (details.length > 0) {
        elementInfo.push("");
        elementInfo.push("  ДЕТАЛИ:");
        for (var i = 0; i < details.length; i++) {
            elementInfo.push("    - " + details[i]);
        }
    }
    
    // Если показываем блочный элемент, но курсор был на другом элементе
    if (displayElement != parentElement) {
        elementInfo.push("");
        elementInfo.push("● ЭЛЕМЕНТ ПОД КУРСОРОМ:");
        elementInfo.push("");
        
        var originalType = getElementType(parentElement);
        elementInfo.push("  Тип: " + originalType);
        elementInfo.push("  Тег: " + parentElement.nodeName);
        elementInfo.push("  Класс: " + (parentElement.className || "(нет класса)"));
        
        // Детали для элемента под курсором
        var originalDetails = getElementDetails(parentElement);
        if (originalDetails.length > 0) {
            for (var i = 0; i < originalDetails.length; i++) {
                elementInfo.push("  " + originalDetails[i]);
            }
        }
        
        // Для stanza показываем дополнительную информацию
        if (parentElement.nodeName == "DIV" && parentElement.className == "stanza") {
            var stanzaText = getElementText(parentElement);
            if (stanzaText && stanzaText.length > 40) {
                stanzaText = stanzaText.substring(0, 40) + "...";
                elementInfo.push("  Текст строфы: \"" + stanzaText + "\"");
            }
        }
    }
    
    // Иерархия элементов
    elementInfo.push("");
    elementInfo.push("● ИЕРАРХИЯ (от элемента к корню):");
    elementInfo.push("");
    
    var current = displayElement;
    var level = 0;
    var hierarchy = [];
    
    while (current && current != document.body) {
        var indent = repeatString("  ", level);
        var elementDesc = indent;
        
        if (current.nodeType == 1) {
            var type = getElementType(current);
            elementDesc += type;
        } else if (current.nodeType == 3) {
            elementDesc += "Текстовый узел";
        } else {
            elementDesc += "Узел типа " + current.nodeType;
        }
        
        // Добавляем в начало
        var tempArr = [elementDesc];
        for (var j = 0; j < hierarchy.length; j++) {
            tempArr[tempArr.length] = hierarchy[j];
        }
        hierarchy = tempArr;
        
        current = current.parentNode;
        level++;
    }
    
    // Добавляем элементы иерархии
    for (var i = 0; i < hierarchy.length; i++) {
        elementInfo.push(hierarchy[i]);
    }
    elementInfo.push("  body (корневой элемент)");
    
    // Дополнительная информация для структурных элементов
    if (displayElement.nodeName == "DIV" && displayElement.className) {
        var className = displayElement.className;
        
        if (className == "poem" || className == "cite" || className == "epigraph" || 
            className == "annotation" || className == "title" || className == "history") {
            
            elementInfo.push("");
            elementInfo.push("● СТРУКТУРА ЭЛЕМЕНТА:");
            elementInfo.push("");
            
            // Считаем абзацы
            var paragraphs = displayElement.getElementsByTagName("P");
            var totalParagraphs = paragraphs.length;
            
            if (totalParagraphs > 0) {
                elementInfo.push("  Всего абзацев: " + totalParagraphs);
                
                // Считаем специальные абзацы
                var subtitles = 0;
                var textAuthors = 0;
                for (var i = 0; i < paragraphs.length; i++) {
                    var pClass = paragraphs[i].className || "";
                    if (pClass == "subtitle") subtitles++;
                    if (pClass == "text-author") textAuthors++;
                }
                
                if (subtitles > 0) elementInfo.push("  Подзаголовков: " + subtitles);
                if (textAuthors > 0) elementInfo.push("  Авторов текста: " + textAuthors);
                
                if (className == "poem") {
                    // Считаем строфы
                    var stanzas = displayElement.getElementsByTagName("DIV");
                    var stanzaCount = 0;
                    for (var i = 0; i < stanzas.length; i++) {
                        if (stanzas[i].className == "stanza") stanzaCount++;
                    }
                    elementInfo.push("  Строф: " + stanzaCount);
                }
            }
        }
    }
    
    // Время выполнения
    var endTime = new Date().getTime();
    var execTime = (endTime - startTime) / 1000;
    
    elementInfo.push("");
    elementInfo.push("====================================");
    elementInfo.push("Время выполнения: " + execTime.toFixed(2) + " сек.");
    
    // Выводим результат
    var result = elementInfo.join("\n");
    MsgBox(result);
}
