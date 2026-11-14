// Скрипт "Карта структуры FB2" для редактора FBE
// version 3.8
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// version 3.8, 12.12.2025
//======================================

function Run() {
    var startTime = new Date().getTime();
    var scriptName = "Карта структуры FB2";
    var scriptVersion = "3.8";
    
    // Функция для удаления пробелов с начала и конца строки
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
    
    // Функция для проверки наличия подстроки
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
        return prefix + "_structure_map_" + baseName + ".txt";
    }
    
    var reportFileName = getReportName(originalFileName, 1);
    
    // 3. ЗАПРОС ПОДТВЕРЖДЕНИЯ
    var msg = "-----------------------------------\n";
    msg += scriptName + " v" + scriptVersion + "\n";
    msg += "-----------------------------------\n\n";
    msg += "Исходный файл: " + originalFileName + ".fb2\n\n";
    msg += "Создать отчет с полной статистикой?\n" + reportFileName + "\n\n";
    
    if (!window.confirm(msg)) return;
    
    // 4. ПЕРЕМЕННЫЕ ДЛЯ СТАТИСТИКИ
    var totalTags = 0;
    var lastSimpleTag = "";
    var simpleTagCount = 0;
    
    // Для проверки соответствия сносок
    var footnoteLinks = []; // Массив ID сносок из ссылок
    var footnoteSections = []; // Массив ID сносок из разделов
    
    // ОБЪЕКТ ДЛЯ ХРАНЕНИЯ НАЙДЕННЫХ ССЫЛОК
    var foundLinksInfo = [];
    var allLinksCount = 0;
    
    // Словари для статистики
    var structureStats = {
        "BODY": {count: 0, display: "Основной раздел - BODY"},
        "SECTION": {count: 0, display: "Разделы - DIV class='section'"},
        "TITLE": {count: 0, display: "Заголовки - DIV class='title'"},
        "SUBTITLE": {count: 0, display: "Подзаголовки - P class='subtitle'"},
        "CITE": {count: 0, display: "Цитаты - DIV class='cite'"},
        "POEM": {count: 0, display: "Стихи - DIV class='poem'"},
        "STANZA": {count: 0, display: "Строфы - DIV class='stanza'"},
        "EPIGRAPH": {count: 0, display: "Эпиграфы - DIV class='epigraph'"},
        "ANNOTATION": {count: 0, display: "Аннотации - DIV class='annotation'"},
        "IMAGE_BLOCK": {count: 0, display: "Иллюстрации - DIV class='image'"},
        "IMAGE_INLINE": {count: 0, display: "Иллюстрации в тексте - SPAN class='image'"},
        "FOOTNOTE_LINK": {count: 0, display: "Сноски в тексте - <a>[n]</a>"},
        "FOOTNOTE_SECTION": {count: 0, display: "Разделы сносок - section id='n_...'"},
        "LINK": {count: 0, display: "Внешние ссылки - A"}
    };
    
    var formatStats = {
        "EM": {name: "Курсив <EM>", count: 0},
        "STRONG": {name: "Жирный <STRONG>", count: 0},
        "EM_STRONG": {name: "Жирный курсив <EM><STRONG>", count: 0},
        "U": {name: "Подчеркивание <U>", count: 0},
        "STRIKE": {name: "Зачеркивание <STRIKE>", count: 0},
        "SUP": {name: "Верхний индекс <SUP>", count: 0},
        "SUB": {name: "Нижний индекс <SUB>", count: 0},
        "CODE": {name: "Код <CODE>", count: 0},
        "SPAN_CODE": {name: "Код <SPAN class='code'>", count: 0}
    };
    
    var structureLines = [];
    
    // 5. ФУНКЦИЯ ДЛЯ СОБИРАНИЯ СТРУКТУРЫ И СТАТИСТИКИ
    function collectDocumentStructure() {
        structureLines.push("--- HTML СТРУКТУРА (DOM FBE) ---");
        structureLines.push("");
        
        // Функция для получения содержимого P с форматированием
        function getFormattedPContent(pElement) {
            var result = "";
            var currentFormatStack = [];
            
            function extractContent(node) {
                if (node.nodeType === 3) { // Текстовый узел
                    var text = node.nodeValue;
                    if (text.length > 20) {
                        text = text.substring(0, 20) + "...";
                    }
                    result += text;
                } else if (node.nodeType === 1) { // Элемент
                    var tag = node.nodeName.toUpperCase();
                    var parentTags = currentFormatStack.slice();
                    currentFormatStack.push(tag);
                    
                    // Проверяем комбинации (жирный курсив)
                    var hasEm = false;
                    var hasStrong = false;
                    for (var k = 0; k < parentTags.length; k++) {
                        if (parentTags[k] === 'EM' || parentTags[k] === 'I') hasEm = true;
                        if (parentTags[k] === 'STRONG' || parentTags[k] === 'B') hasStrong = true;
                    }
                    
                    if ((tag === 'EM' || tag === 'I') && hasStrong) {
                        formatStats["EM_STRONG"].count++;
                    }
                    if ((tag === 'STRONG' || tag === 'B') && hasEm) {
                        formatStats["EM_STRONG"].count++;
                    }
                    
                    // Увеличиваем счетчики форматирования
                    if (tag === 'EM' || tag === 'I') {
                        formatStats["EM"].count++;
                    } else if (tag === 'STRONG' || tag === 'B') {
                        formatStats["STRONG"].count++;
                    } else if (tag === 'U') {
                        formatStats["U"].count++;
                    } else if (tag === 'STRIKE') {
                        formatStats["STRIKE"].count++;
                    } else if (tag === 'SUP') {
                        formatStats["SUP"].count++;
                    } else if (tag === 'SUB') {
                        formatStats["SUB"].count++;
                    } else if (tag === 'CODE') {
                        formatStats["CODE"].count++;
                    } else if (tag === 'SPAN') {
                        var className = node.className || "";
                        if (contains(className, 'code')) {
                            formatStats["SPAN_CODE"].count++;
                        }
                    }
                    
                    // Открывающий тег
                    if (tag === 'STRONG' || tag === 'B') {
                        result += "<strong>";
                    } else if (tag === 'EM' || tag === 'I') {
                        result += "<em>";
                    } else if (tag === 'U') {
                        result += "<u>";
                    } else if (tag === 'STRIKE') {
                        result += "<strike>";
                    } else if (tag === 'SUP') {
                        result += "<sup>";
                    } else if (tag === 'SUB') {
                        result += "<sub>";
                    } else if (tag === 'CODE') {
                        result += "<code>";
                    } else if (tag === 'SPAN') {
                        var className = node.className || "";
                        if (contains(className, 'code')) {
                            result += "<span class='code'>";
                        } else {
                            result += "<span>";
                        }
                    } else {
                        result += "<" + tag.toLowerCase() + ">";
                    }
                    
                    // Рекурсивно обрабатываем детей
                    for (var i = 0; i < node.childNodes.length; i++) {
                        extractContent(node.childNodes[i]);
                    }
                    
                    // Закрывающий тег
                    if (tag === 'STRONG' || tag === 'B') {
                        result += "</strong>";
                    } else if (tag === 'EM' || tag === 'I') {
                        result += "</em>";
                    } else if (tag === 'U') {
                        result += "</u>";
                    } else if (tag === 'STRIKE') {
                        result += "</strike>";
                    } else if (tag === 'SUP') {
                        result += "</sup>";
                    } else if (tag === 'SUB') {
                        result += "</sub>";
                    } else if (tag === 'CODE') {
                        result += "</code>";
                    } else if (tag === 'SPAN') {
                        var className = node.className || "";
                        if (contains(className, 'code')) {
                            result += "</span>";
                        } else {
                            result += "</span>";
                        }
                    } else {
                        result += "</" + tag.toLowerCase() + ">";
                    }
                    
                    currentFormatStack.pop();
                }
            }
            
            for (var i = 0; i < pElement.childNodes.length; i++) {
                extractContent(pElement.childNodes[i]);
            }
            
            return result;
        }
        
        function walk(node) {
            if (!node) return;
            
            if (node.nodeType === 1) { // Element
                var tag = node.nodeName.toUpperCase();
                var className = node.className || "";
                var isSpecial = false;
                var displayLine = "";
                var statKey = "";
                
                // Функция проверки класса
                function hasClass(classStr) {
                    if (className === classStr) return true;
                    if (className === "") return false;
                    return contains(className, classStr);
                }
                
                // 1. СТРУКТУРНЫЕ ЭЛЕМЕНТЫ
                if (tag === 'BODY') {
                    displayLine = '<BODY>';
                    statKey = "BODY";
                    isSpecial = true;
                } else if (tag === 'DIV' && hasClass('section')) {
                    displayLine = '<DIV class="section">';
                    if (node.id) {
                        displayLine = '<DIV class="section" id="' + node.id + '">';
                        // Проверяем, является ли это разделом сноски
                        if (node.id.length > 0 && (node.id.charAt(0) === 'n' || node.id.charAt(0) === '_')) {
                            structureStats["FOOTNOTE_SECTION"].count++;
                            footnoteSections.push(node.id);
                        }
                    }
                    statKey = "SECTION";
                    isSpecial = true;
                } 
                // 2. ЗАГОЛОВКИ И ПОДЗАГОЛОВКИ
                else if (tag === 'DIV' && hasClass('title')) {
                    displayLine = '<DIV class="title">';
                    statKey = "TITLE";
                    isSpecial = true;
                } else if (tag === 'P' && hasClass('subtitle')) {
                    displayLine = '<P class="subtitle">';
                    statKey = "SUBTITLE";
                    isSpecial = true;
                }
                // 3. СПЕЦИАЛЬНЫЕ БЛОКИ
                else if (tag === 'DIV' && hasClass('cite')) {
                    displayLine = '<DIV class="cite">';
                    statKey = "CITE";
                    isSpecial = true;
                } else if (tag === 'DIV' && hasClass('poem')) {
                    displayLine = '<DIV class="poem">';
                    statKey = "POEM";
                    isSpecial = true;
                } else if (tag === 'DIV' && hasClass('stanza')) {
                    displayLine = '<DIV class="stanza">';
                    statKey = "STANZA";
                    isSpecial = true;
                } else if (tag === 'DIV' && hasClass('epigraph')) {
                    displayLine = '<DIV class="epigraph">';
                    statKey = "EPIGRAPH";
                    isSpecial = true;
                } else if (tag === 'DIV' && hasClass('annotation')) {
                    displayLine = '<DIV class="annotation">';
                    statKey = "ANNOTATION";
                    isSpecial = true;
                }
                // 4. ИЗОБРАЖЕНИЯ
                else if (tag === 'SPAN' && hasClass('image')) {
                    var href = node.getAttribute('href') || '';
                    displayLine = '<SPAN class="image"' + (href ? ' href="' + href + '"' : '') + '>';
                    statKey = "IMAGE_INLINE";
                    isSpecial = true;
                } else if (tag === 'DIV' && hasClass('image')) {
                    var href = node.getAttribute('href') || '';
                    displayLine = '<DIV class="image"' + (href ? ' href="' + href + '"' : '') + '>';
                    statKey = "IMAGE_BLOCK";
                    isSpecial = true;
                }
                // 5. ССЫЛКИ - ТОЛЬКО ДЛЯ ОТОБРАЖЕНИЯ В СТРУКТУРЕ (статистику считаем отдельно через document.links)
                else if (tag === 'A') {
                    var href = node.getAttribute('href') || node.getAttribute('l:href') || '';
                    var typeAttr = node.getAttribute('type') || '';
                    
                    displayLine = '<A' + (href ? ' href="' + href + '"' : '') + 
                                 (typeAttr ? ' type="' + typeAttr + '"' : '') + '>';
                    
                    // В структуре показываем как есть, статистику посчитаем отдельно
                    isSpecial = true;
                }
                // 6. КОД (SPAN class='code')
                else if (tag === 'SPAN' && hasClass('code')) {
                    displayLine = '<SPAN class="code">';
                    formatStats["SPAN_CODE"].count++;
                    isSpecial = true;
                }
                // 7. АБЗАЦЫ P
                else if (tag === 'P') {
                    // Проверяем форматирование
                    var hasFormatting = false;
                    for (var c = 0; c < node.childNodes.length; c++) {
                        var child = node.childNodes[c];
                        if (child.nodeType === 1) {
                            var childTag = child.nodeName.toUpperCase();
                            var childClass = child.className || "";
                            if (childTag === 'STRONG' || childTag === 'B' || 
                                childTag === 'EM' || childTag === 'I' ||
                                childTag === 'U' || childTag === 'STRIKE' ||
                                childTag === 'SUP' || childTag === 'SUB' ||
                                childTag === 'CODE' ||
                                (childTag === 'SPAN' && contains(childClass, 'code'))) {
                                hasFormatting = true;
                                break;
                            }
                        }
                    }
                    
                    if (hasFormatting) {
                        // P с форматированием: показываем на одной строке
                        var content = getFormattedPContent(node);
                        displayLine = '<P>' + content + '</P>';
                        isSpecial = true;
                    } else {
                        // Простой P без форматирования
                        displayLine = '<P></P>';
                    }
                }
                // 8. ОТДЕЛЬНЫЕ ТЕГИ ФОРМАТИРОВАНИЯ (не внутри P)
                else if (tag === 'STRONG' || tag === 'B' || tag === 'EM' || tag === 'I' ||
                         tag === 'U' || tag === 'STRIKE' || tag === 'SUP' || tag === 'SUB' || tag === 'CODE') {
                    displayLine = '<' + tag.toLowerCase() + '></' + tag.toLowerCase() + '>';
                    
                    // Увеличиваем счетчик
                    if (tag === 'B') tag = 'STRONG';
                    if (tag === 'I') tag = 'EM';
                    if (formatStats[tag]) {
                        formatStats[tag].count++;
                    }
                    
                    isSpecial = true;
                }
                
                if (displayLine !== "") {
                    // Увеличиваем счетчик структурных элементов (ссылкам считаем отдельно)
                    if (statKey && structureStats[statKey] && tag !== 'A') {
                        structureStats[statKey].count++;
                    }
                    
                    // ЛОГИКА СОКРАЩЕНИЯ: только для простых P без форматирования
                    if (tag === 'P' && !isSpecial) {
                        if (lastSimpleTag === 'P') {
                            simpleTagCount++;
                            if (simpleTagCount <= 2) {
                                structureLines.push(displayLine);
                            } else if (simpleTagCount === 3) {
                                structureLines.push("... [простые P пропущены]");
                            }
                        } else {
                            lastSimpleTag = 'P';
                            simpleTagCount = 1;
                            structureLines.push(displayLine);
                        }
                    } else {
                        // Все особые элементы и P с форматированием - всегда
                        if (simpleTagCount > 2 && lastSimpleTag === 'P') {
                            // Уже показали сообщение о пропуске
                        }
                        lastSimpleTag = "";
                        simpleTagCount = 0;
                        structureLines.push(displayLine);
                    }
                    totalTags++;
                }
                
                // Обрабатываем детей (но не для P с форматированием - уже обработали)
                var lineStart = "";
                if (displayLine.length >= 3) lineStart = displayLine.substring(0, 3);
                var lineEnd = "";
                if (displayLine.length >= 4) lineEnd = displayLine.substring(displayLine.length - 4);
                
                if (!(tag === 'P' && lineStart === '<P>' && lineEnd === '</P>')) {
                    for (var i = 0; i < node.childNodes.length; i++) {
                        walk(node.childNodes[i]);
                    }
                }
                
                // Закрывающие теги для структурных элементов (кроме P)
                if (displayLine !== "" && isSpecial && tag !== 'P') {
                    var closeTag = '</' + tag + '>';
                    structureLines.push(closeTag);
                    totalTags++;
                }
            }
        }
        
        walk(document.body);
    }
    
    collectDocumentStructure();
    
    // 6. АНАЛИЗ ВСЕХ ССЫЛОК В ДОКУМЕНТЕ ЧЕРЕЗ document.links
    function analyzeAllLinks() {
        allLinksCount = document.links.length;
        
        for (var i = 0; i < document.links.length; i++) {
            var link = document.links[i];
            var linkType = link.getAttribute("type") || "";
            var lhref = link.getAttribute("l:href") || "";
            var href = link.getAttribute("href") || "";
            var innerHTML = link.innerHTML || "";
            
            // Записываем информацию о ссылке
            var linkInfo = "Ссылка " + (i+1) + ": ";
            linkInfo += "href='" + href + "', ";
            linkInfo += "l:href='" + lhref + "', ";
            linkInfo += "type='" + linkType + "', ";
            linkInfo += "text='" + trimStr(innerHTML) + "'";
            foundLinksInfo.push(linkInfo);
            
            // ОПРЕДЕЛЯЕМ ТИП ССЫЛКИ (по логике из скрипта статистики)
            var isFootnote = (
                linkType === "note" ||                    // type="note"
                lhref.indexOf("#n_") === 0 ||             // l:href="#n_..."
                lhref.indexOf("#_") === 0 ||              // l:href="#_..."
                /^#n_/.test(href) ||                      // href="#n_..."
                /^#_/.test(href) ||                       // href="#_..."
                /^\[\d+\]$/.test(innerHTML) ||           // текст: [1], [2]
                /^\d+$/.test(innerHTML)                  // текст: 1, 2
            );
            
            // ДОПОЛНИТЕЛЬНАЯ ПРОВЕРКА: ищем #n_ или #_ в любом месте href (для FBE формата)
            if (!isFootnote && href && href.length > 0) {
                // Проверяем наличие #n_ или #_ в любом месте строки
                for (var h = 0; h < href.length - 2; h++) {
                    if (href.charAt(h) === '#') {
                        var nextChar = href.charAt(h + 1);
                        if (nextChar === 'n' || nextChar === '_') {
                            isFootnote = true;
                            break;
                        }
                    }
                }
            }
            
            // ДОПОЛНИТЕЛЬНАЯ ПРОВЕРКА: класс "note"
            if (!isFootnote) {
                var className = link.className || "";
                if (className === "note" || contains(className, "note")) {
                    isFootnote = true;
                }
            }
            
            if (isFootnote) {
                structureStats["FOOTNOTE_LINK"].count++;
                
                // Извлекаем ID сноски
                var footnoteId = "";
                if (lhref && lhref.length > 0) {
                    footnoteId = lhref;
                } else if (href && href.length > 0) {
                    // Ищем # в href
                    var hashPos = -1;
                    for (var p = 0; p < href.length; p++) {
                        if (href.charAt(p) === '#') {
                            hashPos = p;
                            break;
                        }
                    }
                    if (hashPos !== -1) {
                        footnoteId = href.substring(hashPos);
                    }
                }
                
                // Очищаем ID: оставляем только часть после #
                if (footnoteId) {
                    var cleanId = footnoteId;
                    for (var c = 0; c < cleanId.length; c++) {
                        if (cleanId.charAt(c) === '#') {
                            cleanId = cleanId.substring(c);
                            break;
                        }
                    }
                    footnoteLinks.push(cleanId);
                }
            } else {
                structureStats["LINK"].count++;
            }
        }
    }
    
    analyzeAllLinks();
    
    // 7. ФОРМИРУЕМ ПОЛНЫЙ ОТЧЕТ
    var reportLines = [];
    
    // Шапка
    reportLines.push("==================================================");
    reportLines.push("FBE STRUCTURE MAP");
    reportLines.push("Скрипт: \"" + scriptName + "\" v" + scriptVersion);
    reportLines.push("Исходный файл: " + originalFileName + ".fb2");
    
    // Дата
    var now = new Date();
    var dateStr = now.getDate() + "." + (now.getMonth()+1) + "." + now.getFullYear() + " " +
                  now.getHours() + ":" + (now.getMinutes()<10?"0":"") + now.getMinutes();
    reportLines.push("Создан: " + dateStr);
    reportLines.push("==================================================");
    reportLines.push("");
    
    // Добавляем структуру
    for (var i = 0; i < structureLines.length; i++) {
        reportLines.push(structureLines[i]);
    }
    
    reportLines.push("");
    reportLines.push("==================================================");
    
    // 8. СТАТИСТИКА
    reportLines.push("СТАТИСТИКА ДОКУМЕНТА");
    reportLines.push("--------------------------------------------------");
    
    // Общая статистика
    reportLines.push("Всего элементов: " + totalTags);
    
    // Подсчет уникальных структурных элементов
    var uniqueStructureCount = 0;
    var structureList = [];
    for (var key in structureStats) {
        if (structureStats[key].count > 0) {
            uniqueStructureCount++;
            // Берем только русские названия для списка
            var display = structureStats[key].display;
            var rusName = "";
            var foundDash = false;
            for (var p = 0; p < display.length; p++) {
                if (display.charAt(p) === '-') {
                    foundDash = true;
                    rusName = display.substring(0, p);
                    // Убираем пробелы в конце
                    while (rusName.length > 0 && 
                           (rusName.charAt(rusName.length - 1) === ' ' || 
                            rusName.charAt(rusName.length - 1) === '\t')) {
                        rusName = rusName.substring(0, rusName.length - 1);
                    }
                    break;
                }
            }
            if (!foundDash) {
                rusName = display;
            }
            structureList.push(rusName);
        }
    }
    reportLines.push("Уникальных структурных элементов - " + uniqueStructureCount);
    if (structureList.length > 0) {
        reportLines.push("(" + structureList.join(", ") + ")");
    }
    
    // Подсчет уникальных элементов форматирования
    var uniqueFormatCount = 0;
    var formatList = [];
    for (var key in formatStats) {
        if (formatStats[key].count > 0) {
            uniqueFormatCount++;
            // Убираем HTML-теги из названия для списка
            var cleanName = formatStats[key].name;
            var resultName = "";
            var inTag = false;
            for (var n = 0; n < cleanName.length; n++) {
                var ch = cleanName.charAt(n);
                if (ch === '<') {
                    inTag = true;
                } else if (ch === '>') {
                    inTag = false;
                } else if (!inTag) {
                    resultName += ch;
                }
            }
            // Убираем лишние пробелы
            var finalName = "";
            var lastWasSpace = false;
            for (var n = 0; n < resultName.length; n++) {
                var ch = resultName.charAt(n);
                if (ch === ' ' || ch === '\t' || ch === '\n' || ch === '\r') {
                    if (!lastWasSpace && finalName.length > 0) {
                        finalName += ' ';
                        lastWasSpace = true;
                    }
                } else {
                    finalName += ch;
                    lastWasSpace = false;
                }
            }
            // Убираем пробел в конце если есть
            if (finalName.length > 0 && (finalName.charAt(finalName.length - 1) === ' ' || 
                finalName.charAt(finalName.length - 1) === '\t')) {
                finalName = finalName.substring(0, finalName.length - 1);
            }
            formatList.push(finalName);
        }
    }
    reportLines.push("Уникальных элементов форматирования - " + uniqueFormatCount);
    if (formatList.length > 0) {
        reportLines.push("(" + formatList.join(", ") + ")");
    }
    
    reportLines.push("");
    reportLines.push("ДЕТАЛЬНЫЙ ПОДСЧЕТ ПО СТРУКТУРЕ:");
    reportLines.push("------------------------------");
    
    // Вывод структурной статистики с уточненными названиями
    var order = ["BODY", "SECTION", "TITLE", "SUBTITLE", "CITE", "POEM", "STANZA", 
                 "EPIGRAPH", "ANNOTATION", "IMAGE_BLOCK", "IMAGE_INLINE", 
                 "FOOTNOTE_LINK", "FOOTNOTE_SECTION", "LINK"];
    
    for (var i = 0; i < order.length; i++) {
        var key = order[i];
        if (structureStats[key] && structureStats[key].count > 0) {
            reportLines.push(structureStats[key].display + " - " + structureStats[key].count);
        }
    }
    
    // ОТЛАДОЧНАЯ ИНФОРМАЦИЯ О ССЫЛКАХ
    reportLines.push("");
    reportLines.push("=== ИНФОРМАЦИЯ О ССЫЛКАХ ===");
    reportLines.push("Всего ссылок в документе (document.links): " + allLinksCount);
    if (foundLinksInfo.length > 0) {
        reportLines.push("");
        reportLines.push("Детальная информация о ссылках:");
        for (var i = 0; i < foundLinksInfo.length; i++) {
            reportLines.push(foundLinksInfo[i]);
        }
    }
    
    // Общее количество сносок
    var totalFootnotes = structureStats["FOOTNOTE_LINK"].count + structureStats["FOOTNOTE_SECTION"].count;
    var matchedFootnotes = 0;
    
    // Проверяем соответствие ссылок и разделов
    if (footnoteLinks.length > 0 || footnoteSections.length > 0) {
        reportLines.push("");
        reportLines.push("АНАЛИЗ СНОСОК:");
        reportLines.push("-------------");
        reportLines.push("Всего ссылок в документе: " + allLinksCount);
        reportLines.push("Ссылок-сноок в тексте: " + structureStats["FOOTNOTE_LINK"].count);
        reportLines.push("Разделов сносок: " + structureStats["FOOTNOTE_SECTION"].count);
        
        if (footnoteLinks.length > 0) {
            reportLines.push("Найденные ссылки (очищенные ID): " + footnoteLinks.join(", "));
        }
        if (footnoteSections.length > 0) {
            reportLines.push("Найденные разделы: " + footnoteSections.join(", "));
        }
        
        // Проверяем соответствие
        if (structureStats["FOOTNOTE_LINK"].count !== structureStats["FOOTNOTE_SECTION"].count) {
            reportLines.push("⚠ ВНИМАНИЕ: Количество ссылок и разделов не совпадает!");
            
            // Ищем несоответствия
            var missingLinks = [];
            var missingSections = [];
            
            // Сравниваем ID (уже очищенные)
            for (var i = 0; i < footnoteSections.length; i++) {
                var sectionId = footnoteSections[i];
                // Добавляем # для сравнения
                var sectionIdWithHash = "#" + sectionId;
                
                var found = false;
                for (var j = 0; j < footnoteLinks.length; j++) {
                    if (footnoteLinks[j] === sectionIdWithHash) {
                        found = true;
                        matchedFootnotes++;
                        break;
                    }
                }
                if (!found) {
                    missingLinks.push(sectionId);
                }
            }
            
            if (missingLinks.length > 0) {
                reportLines.push("Разделы без ссылок: " + missingLinks.join(", "));
            }
        } else {
            matchedFootnotes = Math.min(structureStats["FOOTNOTE_LINK"].count, structureStats["FOOTNOTE_SECTION"].count);
        }
        
        reportLines.push("Совпадающих пар: " + matchedFootnotes);
    }
    
    if (totalFootnotes > 0) {
        reportLines.push("");
        reportLines.push("ВСЕГО СНОСОК: " + totalFootnotes + " (в тексте: " + 
                        structureStats["FOOTNOTE_LINK"].count + ", разделов: " + 
                        structureStats["FOOTNOTE_SECTION"].count + ")");
        
        // Проверяем, совпадает ли количество
        if (structureStats["FOOTNOTE_LINK"].count === structureStats["FOOTNOTE_SECTION"].count) {
            reportLines.push("✓ Баланс сносок соблюден!");
        } else {
            reportLines.push("⚠ Дисбаланс: проверьте соответствие ссылок и разделов.");
        }
    }
    
    reportLines.push("");
    reportLines.push("ДЕТАЛЬНЫЙ ПОДСЧЕТ ПО ФОРМАТИРОВАНИЮ:");
    reportLines.push("------------------------------------");
    
    // Вывод статистики форматирования
    var formatOrder = ["EM", "STRONG", "EM_STRONG", "U", "STRIKE", "SUP", "SUB", "CODE", "SPAN_CODE"];
    
    for (var i = 0; i < formatOrder.length; i++) {
        var key = formatOrder[i];
        if (formatStats[key] && formatStats[key].count > 0) {
            reportLines.push(formatStats[key].name + " - " + formatStats[key].count);
        }
    }
    
    reportLines.push("");
    reportLines.push("==================================================");
    
    var reportText = reportLines.join('\r\n');
    
    // 9. СОХРАНЕНИЕ
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
    
    // 10. ИТОГ
    var endTime = new Date().getTime();
    var execTime = ((endTime - startTime) / 1000).toFixed(2);
    
    var finalMsg = "-----------------------------------\n";
    finalMsg += scriptName + " v" + scriptVersion + "\n";
    finalMsg += "-----------------------------------\n";
    
    if (saved) {
        finalMsg += "✓ Отчет создан!\n\n";
        finalMsg += "Файл: " + originalFileName + ".fb2\n";
        finalMsg += "Отчет: " + finalName + "\n";
        if (attempts > 1) finalMsg += "(версия " + attempts + ")\n";
        finalMsg += "Всего элементов: " + totalTags + "\n";
        finalMsg += "Структурных элементов: " + uniqueStructureCount + " типов\n";
        finalMsg += "Форматирования: " + uniqueFormatCount + " типов\n";
        finalMsg += "Всего ссылок в документе: " + allLinksCount + "\n";
        var totalFootnotes = structureStats["FOOTNOTE_LINK"].count + structureStats["FOOTNOTE_SECTION"].count;
        if (totalFootnotes > 0) {
            finalMsg += "Сносок: " + totalFootnotes + " (ссылок: " + structureStats["FOOTNOTE_LINK"].count + 
                       ", разделов: " + structureStats["FOOTNOTE_SECTION"].count + ")\n";
            if (structureStats["FOOTNOTE_LINK"].count === structureStats["FOOTNOTE_SECTION"].count) {
                finalMsg += "✓ Баланс сносок соблюден!\n";
            } else {
                finalMsg += "⚠ Дисбаланс сносок!\n";
            }
        }
        finalMsg += "Время: " + execTime + " сек";
    } else {
        finalMsg += "✗ Ошибка!\n\n";
        finalMsg += "Не удалось создать файл.\n";
        finalMsg += "Проверьте права на запись.";
    }
    
    finalMsg += "\n-----------------------------------";
    
    MsgBox(finalMsg, "FBE скрипт");
}
