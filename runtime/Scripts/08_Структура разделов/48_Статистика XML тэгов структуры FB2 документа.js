// Скрипт "Извлечение тегов FB2 и описания" для редактора FBE
// version 1.3
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// version 1.3, 15.12.2025
//======================================

function Run() {
    var startTime = new Date().getTime();
    var scriptName = "Извлечение тегов FB2 и описания";
    var scriptVersion = "1.3";
    
    // Словарь переводов тегов FB2 (расширенный)
    var tagTranslations = {
        // Структурные (блочные) теги
        "body": "Тело документа",
        "section": "Раздел",
        "title": "Заголовок",
        "subtitle": "Подзаголовок",
        "cite": "Цитата",
        "poem": "Стихотворение",
        "stanza": "Строфа",
        "epigraph": "Эпиграф",
        "annotation": "Аннотация",
        "table": "Таблица",
        "tr": "Строка таблицы",
        "th": "Заголовок таблицы",
        "td": "Ячейка таблицы",
        
        // Строчные теги
        "p": "Абзац",
        "v": "Строка стиха",
        "text-author": "Автор текста",
        "empty-line": "Пустая строка",
        "image": "Изображение",
        "a": "Ссылка",
        
        // Теги форматирования
        "strong": "Жирный",
        "emphasis": "Курсив",
        "strikethrough": "Зачеркивание",
        "code": "Код",
        "sup": "Верхний индекс",
        "sub": "Нижний индекс",
        "style": "Стиль"
    };
    
    // Категории тегов (по классам HTML в FBE)
    var structuralClasses = ["body", "section", "title", "subtitle", "cite", "poem", "stanza", 
                           "epigraph", "annotation", "table", "tr", "th", "td"];
    var inlineClasses = ["image"];
    var formattingTags = ["strong", "emphasis", "strikethrough", "code", "sup", "sub", "style", "b", "i", "u"];
    
    // Функция для удаления пробелов с начала и конца строки (IE6 compatible)
    function trimStr(str) {
        if (!str) return "";
        var start = 0;
        var end = str.length - 1;
        
        while (start <= end && (str.charAt(start) === ' ' || str.charAt(start) === '\t' || 
               str.charAt(start) === '\n' || str.charAt(start) === '\r')) {
            start++;
        }
        
        while (end >= start && (str.charAt(end) === ' ' || str.charAt(end) === '\t' || 
               str.charAt(end) === '\n' || str.charAt(end) === '\r')) {
            end--;
        }
        
        return str.substring(start, end + 1);
    }
    
    // Функция для проверки наличия подстроки (IE6 compatible)
    function contains(str, searchStr) {
        if (!str || !searchStr) return false;
        for (var i = 0; i <= str.length - searchStr.length; i++) {
            var found = true;
            for (var j = 0; j < searchStr.length; j++) {
                if (str.charAt(i + j) !== searchStr.charAt(j)) {
                    found = false;
                    break;
                }
            }
            if (found) return true;
        }
        return false;
    }
    
    // 1. ПОЛУЧАЕМ ИМЯ ФАЙЛА
    var originalFileName = "document";
    var userInput = prompt("Введите имя исходного FB2 файла (без .fb2):", "document");
    
    if (userInput === null) return;
    
    if (userInput !== "") {
        originalFileName = trimStr(userInput);
        if (originalFileName.length > 4) {
            var last4 = "";
            for (var k = originalFileName.length - 4; k < originalFileName.length; k++) {
                last4 += originalFileName.charAt(k);
            }
            last4 = last4.toLowerCase();
            if (last4 === ".fb2" || last4 === ".txt") {
                originalFileName = originalFileName.substring(0, originalFileName.length - 4);
            }
        }
    }
    
    // 2. ГЕНЕРИРУЕМ ИМЯ ОТЧЕТА
    function getReportName(baseName, num) {
        var prefix = "";
        if (num < 10) prefix = "0" + num;
        else prefix = num + "";
        return prefix + "_fb2_tags_" + baseName + ".txt";
    }
    
    var reportFileName = getReportName(originalFileName, 1);
    
    // 3. ЗАПРОС ПОДТВЕРЖДЕНИЯ
    var msg = "-----------------------------------\n";
    msg += scriptName + " v" + scriptVersion + "\n";
    msg += "-----------------------------------\n\n";
    msg += "Исходный файл: " + originalFileName + ".fb2\n\n";
    msg += "Извлечь все XML-теги и описание?\n" + reportFileName + "\n\n";
    
    if (!window.confirm(msg)) return;
    
    // 4. ПОЛУЧАЕМ ДОСТУП К СТРУКТУРАМ
    var fbwBody = document.getElementById("fbw_body");
    var fbwDesc = document.getElementById("fbw_desc");
    
    if (!fbwBody) {
        MsgBox("Ошибка: не найден элемент fbw_body\n(структура текста документа)", "FBE скрипт");
        return;
    }
    
    if (!fbwDesc) {
        MsgBox("Ошибка: не найден элемент fbw_desc\n(раздел description)", "FBE скрипт");
        return;
    }
    
    var mDesc = fbwDesc.all;
    
    // === НАЧАЛО ОТЛАДОЧНОГО БЛОКА ===
    // Исследуем структуру fbw_desc для поиска недостающих полей
    var debugInfo = "ОТЛАДКА: Структура fbw_desc\n";
    debugInfo += "===========================\n\n";
    
    // Счетчики найденных элементов
    var foundDiAuthor = 0;
    var foundStiElements = 0;
    var foundElementsWithNickname = 0;
    var foundProgramUsed = 0;
    
    // 1. Поиск по ID и NAME во всех элементах fbw_desc
    debugInfo += "1. ЭЛЕМЕНТЫ С ID И NAME:\n";
    debugInfo += "-----------------------\n";
    
    function collectElementsByTagName(tagName) {
        var elements = fbwDesc.getElementsByTagName(tagName);
        for (var i = 0; i < elements.length; i++) {
            var elem = elements[i];
            var id = elem.id || "";
            var name = elem.getAttribute("name") || "";
            var className = elem.className || "";
            
            if (id || name) {
                debugInfo += tagName + ": id='" + id + "', name='" + name + "', class='" + className + "'\n";
                
                // Проверяем на интересующие нас элементы
                if (id.indexOf("di") === 0 || name.indexOf("di") === 0) {
                    if (id.indexOf("Author") >= 0 || name.indexOf("Author") >= 0) {
                        foundDiAuthor++;
                        debugInfo += "  ^-- ВОЗМОЖНО АВТОР ДОКУМЕНТА!\n";
                        
                        // Ищем nickname внутри
                        var inputs = elem.getElementsByTagName("INPUT");
                        for (var j = 0; j < inputs.length; j++) {
                            var inputName = inputs[j].name || "";
                            if (inputName.indexOf("nick") >= 0) {
                                foundElementsWithNickname++;
                                debugInfo += "      Поле nickname: '" + inputs[j].value + "'\n";
                            }
                        }
                    }
                    
                    if (id.indexOf("Prog") >= 0 || name.indexOf("Prog") >= 0) {
                        foundProgramUsed++;
                        // Ищем значение program-used
                        var valueElem = elem.getElementsByTagName("INPUT")[0] || elem.getElementsByTagName("TEXTAREA")[0];
                        if (valueElem) {
                            debugInfo += "  ^-- PROGRAM-USED: '" + (valueElem.value || "") + "'\n";
                        }
                    }
                }
                
                // Ищем src-title-info
                if (id.indexOf("sti") === 0 || name.indexOf("sti") === 0 || 
                    id.indexOf("src") === 0 || name.indexOf("src") === 0) {
                    foundStiElements++;
                    debugInfo += "  ^-- ВОЗМОЖНО SRC-TITLE-INFO!\n";
                    
                    // Выводим внутренние поля
                    var children = elem.getElementsByTagName("INPUT");
                    for (var j = 0; j < children.length; j++) {
                        var childName = children[j].name || "";
                        if (childName) {
                            debugInfo += "      Поле: '" + childName + "' = '" + children[j].value + "'\n";
                        }
                    }
                }
            }
        }
    }
    
    // Ищем в разных типах элементов
    collectElementsByTagName("DIV");
    collectElementsByTagName("SPAN");
    collectElementsByTagName("FIELDSET");
    collectElementsByTagName("SELECT");
    
    // 2. Поиск прямых свойств в mDesc
    debugInfo += "\n\n2. ПРЯМЫЕ СВОЙСТВА mDesc:\n";
    debugInfo += "------------------------\n";
    
    var propCount = 0;
    for (var prop in mDesc) {
        if (propCount < 50) { // Ограничим вывод
            var value = mDesc[prop];
            
            // Интересуют только объекты и строки
            if (value && (typeof value === "object" || typeof value === "string")) {
                debugInfo += prop + " (" + typeof value + ")";
                
                // Проверяем, содержит ли свойство искомые подстроки
                var propLower = prop.toLowerCase();
                if (propLower.indexOf("di") >= 0 || propLower.indexOf("src") >= 0 || 
                    propLower.indexOf("sti") >= 0 || propLower.indexOf("nick") >= 0) {
                    debugInfo += " <-- ВНИМАНИЕ!";
                    
                    if (propLower.indexOf("author") >= 0 && propLower.indexOf("di") >= 0) {
                        foundDiAuthor++;
                    }
                    if (propLower.indexOf("sti") >= 0 || propLower.indexOf("src") >= 0) {
                        foundStiElements++;
                    }
                    if (propLower.indexOf("nick") >= 0) {
                        foundElementsWithNickname++;
                    }
                }
                
                debugInfo += "\n";
                propCount++;
            }
        }
    }
    
    // 3. Ищем program-used в других местах
    debugInfo += "\n\n3. ПОИСК PROGRAM-USED:\n";
    debugInfo += "---------------------\n";
    
    // Ищем во всех INPUT и TEXTAREA
    var allInputs = fbwDesc.getElementsByTagName("INPUT");
    var allTextareas = fbwDesc.getElementsByTagName("TEXTAREA");
    
    for (var i = 0; i < allInputs.length; i++) {
        var input = allInputs[i];
        var inputName = input.name || "";
        var inputId = input.id || "";
        
        if (inputName.indexOf("prog") >= 0 || inputId.indexOf("prog") >= 0 || 
            inputName.indexOf("program") >= 0 || inputId.indexOf("program") >= 0) {
            foundProgramUsed++;
            debugInfo += "INPUT: name='" + inputName + "', id='" + inputId + "', value='" + input.value + "'\n";
        }
    }
    
    for (var i = 0; i < allTextareas.length; i++) {
        var textarea = allTextareas[i];
        var textareaName = textarea.name || "";
        var textareaId = textarea.id || "";
        
        if (textareaName.indexOf("prog") >= 0 || textareaId.indexOf("prog") >= 0 || 
            textareaName.indexOf("program") >= 0 || textareaId.indexOf("program") >= 0) {
            foundProgramUsed++;
            debugInfo += "TEXTAREA: name='" + textareaName + "', id='" + textareaId + "', value='" + textarea.value + "'\n";
        }
    }
    
    debugInfo += "\n\nИТОГО НАЙДЕНО:\n";
    debugInfo += "• Элементов document-info author: " + foundDiAuthor + "\n";
    debugInfo += "• Элементов src-title-info: " + foundStiElements + "\n";
    debugInfo += "• Элементов с nickname: " + foundElementsWithNickname + "\n";
    debugInfo += "• Элементов program-used: " + foundProgramUsed + "\n";
    
    // Показываем отладочную информацию
    MsgBox(debugInfo, "Отладка fbw_desc v" + scriptVersion);
    
    // === КОНЕЦ ОТЛАДОЧНОГО БЛОКА ===
    
    // 5. ИЗВЛЕКАЕМ ТЕГИ ИЗ ДОКУМЕНТА (fbw_body)
    var foundTags = {
        structural: {},
        inline: {},  
        formatting: {}
    };
    
    var tagOrder = [];
    var allTagsInOrder = [];
    
    // Функция для добавления тега в коллекцию
    function addTag(tagName, element, className) {
        var category = null;
        var fb2TagName = tagName;
        var translation = tagTranslations[tagName] || "";
        
        if (className) {
            var classToFB2 = {
                "body": "body", "section": "section", "title": "title", "subtitle": "subtitle",
                "cite": "cite", "poem": "poem", "stanza": "stanza", "epigraph": "epigraph",
                "annotation": "annotation", "table": "table", "tr": "tr", "th": "th",
                "td": "td", "image": "image"
            };
            
            if (classToFB2[className]) {
                fb2TagName = classToFB2[className];
                translation = tagTranslations[fb2TagName] || className;
                
                for (var i = 0; i < structuralClasses.length; i++) {
                    if (structuralClasses[i] === className) {
                        category = "structural";
                        break;
                    }
                }
                
                if (!category) {
                    for (var i = 0; i < inlineClasses.length; i++) {
                        if (inlineClasses[i] === className) {
                            category = "inline";
                            break;
                        }
                    }
                }
            }
        }
        
        if (!category) {
            tagName = tagName.toLowerCase();
            for (var i = 0; i < formattingTags.length; i++) {
                if (formattingTags[i] === tagName) {
                    category = "formatting";
                    fb2TagName = tagName;
                    translation = tagTranslations[fb2TagName] || tagName;
                    break;
                }
            }
        }
        
        if (!category) {
            category = "structural";
            fb2TagName = tagName;
            translation = tagTranslations[fb2TagName] || tagName;
        }
        
        if (!foundTags[category][fb2TagName]) {
            foundTags[category][fb2TagName] = {
                name: fb2TagName,
                translation: translation,
                count: 1
            };
            tagOrder.push({
                name: fb2TagName,
                category: category,
                translation: translation
            });
        } else {
            foundTags[category][fb2TagName].count++;
        }
        
        allTagsInOrder.push({
            name: fb2TagName,
            category: category,
            translation: translation
        });
    }
    
    // Рекурсивная функция обхода DOM
    function collectTagsFromDOM(node) {
        if (!node) return;
        
        if (node.nodeType === 1) {
            var tagName = node.nodeName.toLowerCase();
            var className = node.className || "";
            
            if (tagName === "div" || tagName === "span" || tagName === "p") {
                var classes = [];
                if (className) {
                    var currentClass = "";
                    for (var c = 0; c < className.length; c++) {
                        var ch = className.charAt(c);
                        if (ch === ' ') {
                            if (currentClass) {
                                classes.push(currentClass);
                                currentClass = "";
                            }
                        } else {
                            currentClass += ch;
                        }
                    }
                    if (currentClass) {
                        classes.push(currentClass);
                    }
                }
                
                for (var i = 0; i < classes.length; i++) {
                    addTag(tagName, node, classes[i]);
                }
                
                if (tagName === "p" && !className) {
                    addTag("p", node, null);
                }
            } else if (tagName === "a") {
                addTag("a", node, null);
            } else if (tagName === "img") {
                addTag("image", node, null);
            } else {
                addTag(tagName, node, null);
            }
            
            for (var i = 0; i < node.childNodes.length; i++) {
                collectTagsFromDOM(node.childNodes[i]);
            }
        }
    }
    
    collectTagsFromDOM(fbwBody);
    
    // 6. ИЗВЛЕКАЕМ РАЗДЕЛ DESCRIPTION (улучшенная версия с учетом отладки)
    var descriptionXML = '<?xml version="1.0" encoding="utf-8"?>\n<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" xmlns:l="http://www.w3.org/1999/xlink">\n <description>\n';
    
    // Вспомогательная функция для создания XML элемента
    function createXMLElement(tagName, content, attributes, indentLevel) {
        if (!indentLevel) indentLevel = 2;
        var indent = "";
        for (var i = 0; i < indentLevel; i++) indent += " ";
        
        var attrStr = "";
        if (attributes) {
            for (var attr in attributes) {
                if (attributes.hasOwnProperty(attr) && attributes[attr] !== null && attributes[attr] !== undefined) {
                    attrStr += ' ' + attr + '="' + escapeXML(attributes[attr] + "") + '"';
                }
            }
        }
        
        if (content === undefined || content === null || content === "") {
            return indent + '<' + tagName + attrStr + '/>\n';
        } else {
            return indent + '<' + tagName + attrStr + '>' + escapeXML(content + "") + '</' + tagName + '>\n';
        }
    }
    
    // Вспомогательная функция для экранирования XML
    function escapeXML(text) {
        if (!text) return "";
        return text.replace(/&/g, '&amp;')
                   .replace(/</g, '&lt;')
                   .replace(/>/g, '&gt;')
                   .replace(/"/g, '&quot;')
                   .replace(/'/g, '&apos;');
    }
    
    // 6.1. TITLE-INFO
    descriptionXML += '  <title-info>\n';
    
    // Жанры
    var mDiv = mDesc.tiGenre ? mDesc.tiGenre.getElementsByTagName("DIV") : [];
    for (var j = 0; j < mDiv.length; j++) {
        var genre = mDiv[j].all.genre ? mDiv[j].all.genre.value : "";
        if (genre) {
            descriptionXML += createXMLElement("genre", genre, null, 3);
        }
    }
    
    // Авторы
    mDiv = mDesc.tiAuthor ? mDesc.tiAuthor.getElementsByTagName("DIV") : [];
    for (var j = 0; j < mDiv.length; j++) {
        var div = mDiv[j];
        var firstName = div.all.first ? div.all.first.value : "";
        var middleName = div.all.middle ? div.all.middle.value : "";
        var lastName = div.all.last ? div.all.last.value : "";
        
        // Пытаемся найти nickname (на основе отладки)
        var nickname = "";
        var inputs = div.getElementsByTagName("INPUT");
        for (var k = 0; k < inputs.length; k++) {
            var inputName = inputs[k].name || "";
            if (inputName.toLowerCase().indexOf("nick") >= 0) {
                nickname = inputs[k].value || "";
                break;
            }
        }
        
        if (firstName || middleName || lastName || nickname) {
            descriptionXML += '   <author>\n';
            if (firstName) descriptionXML += createXMLElement("first-name", firstName, null, 4);
            if (middleName) descriptionXML += createXMLElement("middle-name", middleName, null, 4);
            if (lastName) descriptionXML += createXMLElement("last-name", lastName, null, 4);
            if (nickname) descriptionXML += createXMLElement("nickname", nickname, null, 4);
            descriptionXML += '   </author>\n';
        }
    }
    
    // Название книги
    var bookTitle = mDesc.tiTitle ? mDesc.tiTitle.value : "";
    if (bookTitle) {
        descriptionXML += createXMLElement("book-title", bookTitle, null, 3);
    }
    
    // Аннотация
    var annotationText = "";
    if (fbwBody.firstChild && fbwBody.firstChild.className == "annotation") {
        var mP = fbwBody.firstChild.getElementsByTagName("P");
        for (var k = 0; k < mP.length; k++) {
            var pContent = mP[k].innerHTML || "";
            pContent = pContent.replace(/<[^>]*>/g, "");
            pContent = pContent.replace(/&nbsp;/g, " ");
            pContent = pContent.replace(/&lt;/g, "<");
            pContent = pContent.replace(/&gt;/g, ">");
            pContent = pContent.replace(/&amp;/g, "&");
            pContent = trimStr(pContent);
            
            if (pContent) {
                annotationText += '      <p>' + escapeXML(pContent) + '</p>\n';
            }
        }
    }
    
    if (annotationText) {
        descriptionXML += '   <annotation>\n' + annotationText + '   </annotation>\n';
    } else {
        descriptionXML += '   <annotation/>\n';
    }
    
    // Дата
    var bookDate = mDesc.tiDate ? mDesc.tiDate.value : "";
    if (bookDate) {
        descriptionXML += createXMLElement("date", bookDate, null, 3);
    }
    
    // Язык книги
    var bookLang = mDesc.tiLang ? mDesc.tiLang.value : "";
    if (bookLang) {
        descriptionXML += createXMLElement("lang", bookLang, null, 3);
    }
    
    // Язык оригинала
    var srcLang = mDesc.tiSrcLang ? mDesc.tiSrcLang.value : "";
    if (srcLang) {
        descriptionXML += createXMLElement("src-lang", srcLang, null, 3);
    }
    
    // Переводчики
    mDiv = mDesc.tiTrans ? mDesc.tiTrans.getElementsByTagName("DIV") : [];
    for (var j = 0; j < mDiv.length; j++) {
        var div = mDiv[j];
        var firstName = div.all.first ? div.all.first.value : "";
        var middleName = div.all.middle ? div.all.middle.value : "";
        var lastName = div.all.last ? div.all.last.value : "";
        
        // Ищем nickname для переводчиков
        var nickname = "";
        var inputs = div.getElementsByTagName("INPUT");
        for (var k = 0; k < inputs.length; k++) {
            var inputName = inputs[k].name || "";
            if (inputName.toLowerCase().indexOf("nick") >= 0) {
                nickname = inputs[k].value || "";
                break;
            }
        }
        
        if (firstName || middleName || lastName || nickname) {
            descriptionXML += '   <translator>\n';
            if (firstName) descriptionXML += createXMLElement("first-name", firstName, null, 4);
            if (middleName) descriptionXML += createXMLElement("middle-name", middleName, null, 4);
            if (lastName) descriptionXML += createXMLElement("last-name", lastName, null, 4);
            if (nickname) descriptionXML += createXMLElement("nickname", nickname, null, 4);
            descriptionXML += '   </translator>\n';
        }
    }
    
    // Серии
    mDiv = mDesc.tiSeq ? mDesc.tiSeq.getElementsByTagName("DIV") : [];
    for (var j = 0; j < mDiv.length; j++) {
        var div = mDiv[j];
        var seqName = div.children && div.children.name ? div.children.name.value : "";
        var seqNumber = div.children && div.children.number ? div.children.number.value : "";
        
        if (seqName || seqNumber) {
            var attrs = {};
            if (seqName) attrs.name = seqName;
            if (seqNumber) attrs.number = seqNumber;
            descriptionXML += createXMLElement("sequence", "", attrs, 3);
        }
    }
    
    descriptionXML += '  </title-info>\n';
    
    // 6.2. DOCUMENT-INFO (с улучшениями из отладки)
    descriptionXML += '  <document-info>\n';
    
    // Автор документа - ищем через различные возможные пути
    var docAuthorFound = false;
    
    // Способ 1: через diAuthor (если существует)
    if (mDesc.diAuthor) {
        mDiv = mDesc.diAuthor.getElementsByTagName("DIV");
        for (var j = 0; j < mDiv.length; j++) {
            var div = mDiv[j];
            var firstName = div.all.first ? div.all.first.value : "";
            var middleName = div.all.middle ? div.all.middle.value : "";
            var lastName = div.all.last ? div.all.last.value : "";
            var nickname = "";
            
            // Ищем nickname в полях ввода
            var inputs = div.getElementsByTagName("INPUT");
            for (var k = 0; k < inputs.length; k++) {
                var inputName = inputs[k].name || "";
                if (inputName.toLowerCase().indexOf("nick") >= 0) {
                    nickname = inputs[k].value || "";
                    break;
                }
            }
            
            if (firstName || middleName || lastName || nickname) {
                docAuthorFound = true;
                descriptionXML += '   <author>\n';
                if (firstName) descriptionXML += createXMLElement("first-name", firstName, null, 4);
                if (middleName) descriptionXML += createXMLElement("middle-name", middleName, null, 4);
                if (lastName) descriptionXML += createXMLElement("last-name", lastName, null, 4);
                if (nickname) descriptionXML += createXMLElement("nickname", nickname, null, 4);
                descriptionXML += '   </author>\n';
            }
        }
    }
    
    // Способ 2: поиск по всем элементам с "di" и "author"
    if (!docAuthorFound) {
        var allDivs = fbwDesc.getElementsByTagName("DIV");
        for (var i = 0; i < allDivs.length; i++) {
            var div = allDivs[i];
            var id = div.id || "";
            var name = div.getAttribute("name") || "";
            
            if ((id.indexOf("di") === 0 && id.indexOf("author") >= 0) || 
                (name.indexOf("di") === 0 && name.indexOf("author") >= 0)) {
                
                var firstName = "";
                var middleName = "";
                var lastName = "";
                var nickname = "";
                
                // Ищем поля внутри
                var inputs = div.getElementsByTagName("INPUT");
                for (var k = 0; k < inputs.length; k++) {
                    var inputName = inputs[k].name || "";
                    var inputValue = inputs[k].value || "";
                    
                    if (inputName.indexOf("first") >= 0) firstName = inputValue;
                    else if (inputName.indexOf("middle") >= 0) middleName = inputValue;
                    else if (inputName.indexOf("last") >= 0) lastName = inputValue;
                    else if (inputName.indexOf("nick") >= 0) nickname = inputValue;
                }
                
                if (firstName || middleName || lastName || nickname) {
                    descriptionXML += '   <author>\n';
                    if (firstName) descriptionXML += createXMLElement("first-name", firstName, null, 4);
                    if (middleName) descriptionXML += createXMLElement("middle-name", middleName, null, 4);
                    if (lastName) descriptionXML += createXMLElement("last-name", lastName, null, 4);
                    if (nickname) descriptionXML += createXMLElement("nickname", nickname, null, 4);
                    descriptionXML += '   </author>\n';
                    break;
                }
            }
        }
    }
    
    // Program-used - улучшенный поиск
    var programUsed = "Fiction Book Editor";
    
    // Ищем в разных местах
    if (mDesc.diProg && mDesc.diProg.value) {
        programUsed = mDesc.diProg.value;
    } else {
        // Поиск во всех INPUT и TEXTAREA
        var allInputs = fbwDesc.getElementsByTagName("INPUT");
        for (var i = 0; i < allInputs.length; i++) {
            var inputName = allInputs[i].name || "";
            var inputId = allInputs[i].id || "";
            if (inputName.indexOf("prog") >= 0 || inputId.indexOf("prog") >= 0 || 
                inputName.indexOf("program") >= 0 || inputId.indexOf("program") >= 0) {
                programUsed = allInputs[i].value || programUsed;
                break;
            }
        }
    }
    
    descriptionXML += createXMLElement("program-used", programUsed, null, 3);
    
    // Дата документа
    var docDate = "";
    var docDateValue = "";
    
    if (mDesc.diDate) docDate = mDesc.diDate.value;
    if (mDesc.diDateValue) docDateValue = mDesc.diDateValue.value;
    
    if (docDate || docDateValue) {
        var attrs = {};
        if (docDateValue) attrs.value = docDateValue;
        descriptionXML += createXMLElement("date", docDate, attrs, 3);
    }
    
    // ID
    var docId = mDesc.diId ? mDesc.diId.value : "";
    if (docId) {
        descriptionXML += createXMLElement("id", docId, null, 3);
    }
    
    // Версия
    var version = mDesc.diVer ? mDesc.diVer.value : "1.0";
    if (version) {
        descriptionXML += createXMLElement("version", version, null, 3);
    }
    
    // История
    if (mDesc.diHist) {
        var historyText = mDesc.diHist.value || "";
        if (historyText) {
            descriptionXML += '   <history>\n';
            descriptionXML += '    <p>' + escapeXML(historyText) + '</p>\n';
            descriptionXML += '   </history>\n';
        }
    }
    
    descriptionXML += '  </document-info>\n';
    
    // 6.3. SRC-TITLE-INFO (попытка найти через отладочную информацию)
    descriptionXML += '  <src-title-info>\n';
    
    var srcInfoFound = false;
    
    // Пытаемся найти элементы с "sti" или "src"
    var allElements = fbwDesc.getElementsByTagName("*");
    for (var i = 0; i < allElements.length; i++) {
        var elem = allElements[i];
        var id = elem.id || "";
        var name = elem.getAttribute("name") || "";
        
        if ((id.indexOf("sti") === 0 || name.indexOf("sti") === 0) && 
            (id.indexOf("genre") >= 0 || name.indexOf("genre") >= 0)) {
            
            // Нашли элемент с жанром оригинала
            var inputs = elem.getElementsByTagName("INPUT");
            for (var j = 0; j < inputs.length; j++) {
                var value = inputs[j].value || "";
                if (value) {
                    descriptionXML += createXMLElement("genre", value, null, 3);
                    srcInfoFound = true;
                    break;
                }
            }
        }
    }
    
    // Если не нашли через отладку, добавляем заглушку
    if (!srcInfoFound) {
        descriptionXML += '   <!-- Раздел src-title-info не найден в интерфейсе FBE -->\n';
        descriptionXML += '   <!-- Данные будут добавлены в следующей версии скрипта -->\n';
    }
    
    descriptionXML += '  </src-title-info>\n';
    
    // 6.4. PUBLISH-INFO
    descriptionXML += '  <publish-info>\n';
    
    var printName = mDesc.piName ? mDesc.piName.value : "";
    if (printName) {
        descriptionXML += createXMLElement("book-name", printName, null, 3);
    }
    
    var publisher = mDesc.piPub ? mDesc.piPub.value : "";
    if (publisher) {
        descriptionXML += createXMLElement("publisher", publisher, null, 3);
    }
    
    var city = mDesc.piCity ? mDesc.piCity.value : "";
    if (city) {
        descriptionXML += createXMLElement("city", city, null, 3);
    }
    
    var year = mDesc.piYear ? mDesc.piYear.value : "";
    if (year) {
        descriptionXML += createXMLElement("year", year, null, 3);
    }
    
    var isbn = mDesc.piISBN ? mDesc.piISBN.value : "";
    if (isbn) {
        descriptionXML += createXMLElement("isbn", isbn, null, 3);
    }
    
    mDiv = mDesc.piSeq ? mDesc.piSeq.getElementsByTagName("DIV") : [];
    for (var j = 0; j < mDiv.length; j++) {
        var div = mDiv[j];
        var seqName = div.children && div.children.name ? div.children.name.value : "";
        var seqNumber = div.children && div.children.number ? div.children.number.value : "";
        
        if (seqName || seqNumber) {
            var attrs = {};
            if (seqName) attrs.name = seqName;
            if (seqNumber) attrs.number = seqNumber;
            descriptionXML += createXMLElement("sequence", "", attrs, 3);
        }
    }
    
    descriptionXML += '  </publish-info>\n';
    
    // 6.5. CUSTOM-INFO
    mDiv = mDesc.ci ? mDesc.ci.getElementsByTagName("DIV") : [];
    for (var j = 0; j < mDiv.length; j++) {
        var div = mDiv[j];
        var infoType = div.all.type ? div.all.type.value : "";
        var infoValue = div.all.val ? (div.all.val.innerHTML || div.all.val.value || "") : "";
        
        if (infoValue) {
            infoValue = infoValue.replace(/<[^>]*>/g, "");
            infoValue = infoValue.replace(/&nbsp;/g, " ");
            infoValue = trimStr(infoValue);
            
            if (infoValue) {
                descriptionXML += createXMLElement("custom-info", infoValue, { "info-type": infoType }, 2);
            }
        }
    }
    
    descriptionXML += ' </description>\n</FictionBook>';
    
    // 7. ФОРМИРУЕМ ОТЧЕТ
    var reportLines = [];
    
    // Шапка
    reportLines.push("==================================================");
    reportLines.push("ИЗВЛЕЧЕНИЕ ТЕГОВ FB2 И ОПИСАНИЯ");
    reportLines.push("Скрипт: \"" + scriptName + "\" v" + scriptVersion);
    reportLines.push("Исходный файл: " + originalFileName + ".fb2");
    
    // Дата
    var now = new Date();
    var dateStr = now.getDate() + "." + (now.getMonth()+1) + "." + now.getFullYear() + " " +
                  now.getHours() + ":" + (now.getMinutes()<10?"0":"") + now.getMinutes();
    reportLines.push("Создан: " + dateStr);
    reportLines.push("==================================================");
    reportLines.push("");
    
    // УНИКАЛЬНЫЕ XML-ТЕГИ (в порядке первого появления)
    reportLines.push("УНИКАЛЬНЫЕ XML-ТЕГИ FB2 (в порядке первого появления)");
    reportLines.push("");
    
    // Структурные теги
    reportLines.push("СТРУКТУРНЫЕ (БЛОЧНЫЕ) ТЕГИ:");
    reportLines.push("---------------------------");
    var hasStructural = false;
    for (var i = 0; i < tagOrder.length; i++) {
        if (tagOrder[i].category === "structural") {
            var tagInfo = tagOrder[i];
            var displayName = tagInfo.translation ? tagInfo.translation : tagInfo.name;
            reportLines.push(displayName + " <" + tagInfo.name + "></" + tagInfo.name + ">");
            hasStructural = true;
        }
    }
    if (!hasStructural) {
        reportLines.push("(не найдено)");
    }
    reportLines.push("");
    
    // Строчные теги
    reportLines.push("СТРОЧНЫЕ ТЕГИ (абзацы, строки и т.д.):");
    reportLines.push("--------------------------------------");
    var hasInline = false;
    for (var i = 0; i < tagOrder.length; i++) {
        if (tagOrder[i].category === "inline") {
            var tagInfo = tagOrder[i];
            var displayName = tagInfo.translation ? tagInfo.translation : tagInfo.name;
            reportLines.push(displayName + " <" + tagInfo.name + "></" + tagInfo.name + ">");
            hasInline = true;
        }
    }
    if (!hasInline) {
        reportLines.push("(не найдено)");
    }
    reportLines.push("");
    
    // Теги форматирования
    reportLines.push("ТЕГИ ФОРМАТИРОВАНИЯ:");
    reportLines.push("-------------------");
    var hasFormatting = false;
    for (var i = 0; i < tagOrder.length; i++) {
        if (tagOrder[i].category === "formatting") {
            var tagInfo = tagOrder[i];
            var displayName = tagInfo.translation ? tagInfo.translation : tagInfo.name;
            reportLines.push(displayName + " <" + tagInfo.name + "></" + tagInfo.name + ">");
            hasFormatting = true;
        }
    }
    if (!hasFormatting) {
        reportLines.push("(не найдено)");
    }
    
    // Вывод ВСЕХ тегов в порядке появления (дополнительно)
    reportLines.push("");
    reportLines.push("ВСЕ ТЕГИ В ПОРЯДКЕ ПОЯВЛЕНИЯ:");
    reportLines.push("-----------------------------");
    if (allTagsInOrder.length > 0) {
        var uniqueInOrder = [];
        var seen = {};
        for (var i = 0; i < allTagsInOrder.length; i++) {
            var tag = allTagsInOrder[i];
            if (!seen[tag.name]) {
                seen[tag.name] = true;
                uniqueInOrder.push(tag);
            }
        }
        
        for (var i = 0; i < uniqueInOrder.length; i++) {
            var tag = uniqueInOrder[i];
            var displayName = tag.translation ? tag.translation : tag.name;
            reportLines.push(displayName + " <" + tag.name + "></" + tag.name + ">");
        }
    } else {
        reportLines.push("(теги не найдены)");
    }
    
    reportLines.push("");
    reportLines.push("==================================================");
    reportLines.push("");
    
    // РАЗДЕЛ DESCRIPTION
    reportLines.push("СОДЕРЖИМОЕ РАЗДЕЛА DESCRIPTION:");
    reportLines.push("================================");
    reportLines.push("");
    reportLines.push(descriptionXML);
    
    reportLines.push("");
    reportLines.push("==================================================");
    
    var reportText = reportLines.join('\r\n');
    
    // 8. СОХРАНЕНИЕ ФАЙЛА
    var saved = false;
    var finalName = "";
    var attempts = 0;
    
    for (var attempt = 1; attempt <= 10; attempt++) {
        var testName = getReportName(originalFileName, attempt);
        try {
            saved = window.external.SaveBinary(testName, reportText, 0);
            if (saved) {
                finalName = testName;
                attempts = attempt;
                break;
            }
        } catch(e) {
            // Продолжаем
        }
    }
    
    // 9. ИТОГ
    var endTime = new Date().getTime();
    var execTime = ((endTime - startTime) / 1000).toFixed(2);
    
    var totalTagsCount = 0;
    var structuralCount = 0;
    var inlineCount = 0;
    var formattingCount = 0;
    
    for (var category in foundTags) {
        for (var tag in foundTags[category]) {
            if (foundTags[category].hasOwnProperty(tag)) {
                totalTagsCount++;
                if (category === "structural") structuralCount++;
                else if (category === "inline") inlineCount++;
                else if (category === "formatting") formattingCount++;
            }
        }
    }
    
    var finalMsg = "-----------------------------------\n";
    finalMsg += scriptName + " v" + scriptVersion + "\n";
    finalMsg += "-----------------------------------\n";
    
    if (saved) {
        finalMsg += "✓ Отчет создан!\n\n";
        finalMsg += "Файл: " + originalFileName + ".fb2\n";
        finalMsg += "Отчет: " + finalName + "\n";
        if (attempts > 1) finalMsg += "(версия " + attempts + ")\n\n";
        finalMsg += "СТАТИСТИКА:\n";
        finalMsg += "• Всего уникальных тегов: " + totalTagsCount + "\n";
        if (totalTagsCount > 0) {
            finalMsg += "  - Структурных: " + structuralCount + "\n";
            finalMsg += "  - Строчных: " + inlineCount + "\n";
            finalMsg += "  - Форматирования: " + formattingCount + "\n";
        }
        finalMsg += "• Раздел description: извлечен с улучшениями\n";
        finalMsg += "\nОТЛАДКА (смотри предыдущее окно):\n";
        finalMsg += "- Найдено document-info: " + (docAuthorFound ? "да" : "нет") + "\n";
        finalMsg += "- Найдено src-title-info: " + (srcInfoFound ? "да" : "нет") + "\n";
        finalMsg += "\nВремя выполнения: " + execTime + " сек";
    } else {
        finalMsg += "✗ Ошибка!\n\n";
        finalMsg += "Не удалось создать файл отчета.\n";
        finalMsg += "Проверьте права на запись в папку.";
    }
    
    finalMsg += "\n-----------------------------------";
    
    MsgBox(finalMsg, "FBE скрипт");
}
