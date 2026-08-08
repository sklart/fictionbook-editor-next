// Скрипт "Детектор всех тегов и классов FB2" для редактора FBE
// version 1.3
// Идея - TaKir
// Реализация - DeepSeek

// version 1.3, 12.12.2025
//======================================

function Run() {
    var startTime = new Date().getTime();
    var scriptName = "Детектор всех тегов и классов FB2";
    var scriptVersion = "1.3";
    
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
    
    // Хранилища для найденных данных
    var allTags = {};
    var allClasses = {};
    var allAttributes = {};
    var tagClassCombinations = {};
    
    // Для метаданных description
    var descriptionData = {
        titleInfo: {},
        documentInfo: {},
        publishInfo: {},
        customInfo: {}
    };
    
    // Счётчики
    var totalElements = 0;
    var totalTextNodes = 0;
    
    // Функция рекурсивного обхода
    function walk(node, depth) {
        if (!node) return;
        
        if (node.nodeType === 1) { // Element node
            totalElements++;
            var tagName = node.nodeName.toUpperCase();
            
            // Учитываем тег
            if (!allTags[tagName]) {
                allTags[tagName] = {count: 1, examples: []};
            } else {
                allTags[tagName].count++;
            }
            
            // Сохраняем пример (первые 3 уникальных)
            var outerHTML = node.outerHTML;
            if (allTags[tagName].examples.length < 3) {
                var alreadyExists = false;
                for (var ex = 0; ex < allTags[tagName].examples.length; ex++) {
                    if (allTags[tagName].examples[ex] === outerHTML) {
                        alreadyExists = true;
                        break;
                    }
                }
                if (!alreadyExists) {
                    allTags[tagName].examples.push(outerHTML);
                }
            }
            
            // Пытаемся найти метаданные description
            if (tagName === 'META' || tagName === 'TITLE' || tagName === 'AUTHOR' || 
                tagName === 'BOOK-TITLE' || tagName === 'ANNOTATION' || tagName === 'DATE') {
                
                var content = "";
                for (var c = 0; c < node.childNodes.length; c++) {
                    if (node.childNodes[c].nodeType === 3) {
                        content += node.childNodes[c].nodeValue;
                    }
                }
                content = trimStr(content);
                
                if (content) {
                    if (tagName === 'TITLE' || tagName === 'BOOK-TITLE') {
                        if (!descriptionData.titleInfo['title']) {
                            descriptionData.titleInfo['title'] = [];
                        }
                        descriptionData.titleInfo['title'].push(content);
                    }
                    else if (tagName === 'AUTHOR') {
                        if (!descriptionData.titleInfo['author']) {
                            descriptionData.titleInfo['author'] = [];
                        }
                        descriptionData.titleInfo['author'].push(content);
                    }
                    else if (tagName === 'ANNOTATION') {
                        if (!descriptionData.titleInfo['annotation']) {
                            descriptionData.titleInfo['annotation'] = [];
                        }
                        descriptionData.titleInfo['annotation'].push(content);
                    }
                    else if (tagName === 'DATE') {
                        if (!descriptionData.documentInfo['date']) {
                            descriptionData.documentInfo['date'] = [];
                        }
                        descriptionData.documentInfo['date'].push(content);
                    }
                }
            }
            
            // Также проверяем элементы по ID или классам
            if (node.id) {
                var idValue = node.id;
                // Проверяем наличие подстроки
                var hasSubstring = false;
                if (idValue.toLowerCase().indexOf('title') !== -1 || 
                    idValue.toLowerCase().indexOf('author') !== -1 || 
                    idValue.toLowerCase().indexOf('annotation') !== -1 || 
                    idValue.toLowerCase().indexOf('date') !== -1) {
                    hasSubstring = true;
                }
                
                if (hasSubstring) {
                    var content = "";
                    for (var c = 0; c < node.childNodes.length; c++) {
                        if (node.childNodes[c].nodeType === 3) {
                            content += node.childNodes[c].nodeValue;
                        }
                    }
                    content = trimStr(content);
                    
                    if (content) {
                        if (!descriptionData.customInfo[idValue]) {
                            descriptionData.customInfo[idValue] = [];
                        }
                        descriptionData.customInfo[idValue].push(content);
                    }
                }
            }
            
            // Обрабатываем атрибуты
            if (node.attributes && node.attributes.length > 0) {
                for (var i = 0; i < node.attributes.length; i++) {
                    var attr = node.attributes[i];
                    var attrName = attr.name;
                    var attrValue = attr.value;
                    
                    if (!allAttributes[attrName]) {
                        allAttributes[attrName] = {count: 1, values: {}};
                    } else {
                        allAttributes[attrName].count++;
                    }
                    
                    if (!allAttributes[attrName].values[attrValue]) {
                        allAttributes[attrName].values[attrValue] = 1;
                    } else {
                        allAttributes[attrName].values[attrValue]++;
                    }
                    
                    if (attrName === 'class' || attrName === 'className') {
                        var classes = attrValue.split(' ');
                        for (var c = 0; c < classes.length; c++) {
                            var className = trimStr(classes[c]);
                            if (className) {
                                if (!allClasses[className]) {
                                    allClasses[className] = {count: 1, tags: {}};
                                } else {
                                    allClasses[className].count++;
                                }
                                
                                if (!allClasses[className].tags[tagName]) {
                                    allClasses[className].tags[tagName] = 1;
                                } else {
                                    allClasses[className].tags[tagName]++;
                                }
                                
                                var comboKey = tagName + '.' + className;
                                if (!tagClassCombinations[comboKey]) {
                                    tagClassCombinations[comboKey] = 1;
                                } else {
                                    tagClassCombinations[comboKey]++;
                                }
                            }
                        }
                    }
                }
            }
            
            // Рекурсивно обходим детей
            for (var i = 0; i < node.childNodes.length; i++) {
                walk(node.childNodes[i], depth + 1);
            }
            
        } else if (node.nodeType === 3) { // Text node
            totalTextNodes++;
        }
    }
    
    // Запускаем обход
    walk(document.body, 0);
    
    // Пробуем получить метаданные через document.title
    var documentTitle = document.title;
    if (documentTitle && documentTitle !== "" && documentTitle !== "Fiction Book Editor") {
        if (!descriptionData.titleInfo['documentTitle']) {
            descriptionData.titleInfo['documentTitle'] = [];
        }
        descriptionData.titleInfo['documentTitle'].push(documentTitle);
    }
    
    // Пробуем найти метаданные в HEAD
    try {
        var head = document.getElementsByTagName('head')[0];
        if (head) {
            var metaTags = head.getElementsByTagName('meta');
            for (var m = 0; m < metaTags.length; m++) {
                var meta = metaTags[m];
                var name = meta.getAttribute('name') || meta.getAttribute('property') || "";
                var content = meta.getAttribute('content') || "";
                
                if (name && content) {
                    name = name.toLowerCase();
                    if (name.indexOf('title') !== -1 || name.indexOf('author') !== -1 || 
                        name.indexOf('description') !== -1 || name.indexOf('date') !== -1) {
                        
                        if (!descriptionData.customInfo[name]) {
                            descriptionData.customInfo[name] = [];
                        }
                        descriptionData.customInfo[name].push(content);
                    }
                }
            }
            
            var titleTags = head.getElementsByTagName('title');
            if (titleTags.length > 0) {
                var headTitle = "";
                for (var c = 0; c < titleTags[0].childNodes.length; c++) {
                    if (titleTags[0].childNodes[c].nodeType === 3) {
                        headTitle += titleTags[0].childNodes[c].nodeValue;
                    }
                }
                headTitle = trimStr(headTitle);
                if (headTitle && headTitle !== "" && headTitle !== "Fiction Book Editor") {
                    if (!descriptionData.titleInfo['headTitle']) {
                        descriptionData.titleInfo['headTitle'] = [];
                    }
                    descriptionData.titleInfo['headTitle'].push(headTitle);
                }
            }
        }
    } catch(e) {
        // Игнорируем ошибки
    }
    
    // Подсчитываем уникальные значения
    var uniqueTagsCount = 0;
    for (var tag in allTags) {
        if (allTags.hasOwnProperty(tag)) {
            uniqueTagsCount++;
        }
    }
    
    var uniqueClassesCount = 0;
    for (var cls in allClasses) {
        if (allClasses.hasOwnProperty(cls)) {
            uniqueClassesCount++;
        }
    }
    
    var uniqueAttributesCount = 0;
    for (var attr in allAttributes) {
        if (allAttributes.hasOwnProperty(attr) && attr.charAt(0) !== '_') {
            uniqueAttributesCount++;
        }
    }
    
    var uniqueCombinationsCount = 0;
    for (var combo in tagClassCombinations) {
        if (tagClassCombinations.hasOwnProperty(combo)) {
            uniqueCombinationsCount++;
        }
    }
    
    // Проверяем, есть ли данные в customInfo
    var hasCustomInfo = false;
    for (var key in descriptionData.customInfo) {
        if (descriptionData.customInfo.hasOwnProperty(key)) {
            hasCustomInfo = true;
            break;
        }
    }
    
    // Формируем отчёт
    var reportLines = [];
    
    // Шапка
    reportLines.push("==================================================");
    reportLines.push("FBE TAG & CLASS DETECTOR + DESCRIPTION INFO");
    reportLines.push("Скрипт: \"" + scriptName + "\" v" + scriptVersion);
    reportLines.push("==================================================");
    reportLines.push("");
    
    // НОВЫЙ РАЗДЕЛ: МЕТАДАННЫЕ DESCRIPTION
    reportLines.push("МЕТАДАННЫЕ ДОКУМЕНТА (DESCRIPTION):");
    reportLines.push("====================================");
    
    var hasDescriptionData = false;
    
    // 1. Информация из document.title
    if (descriptionData.titleInfo['documentTitle'] && descriptionData.titleInfo['documentTitle'].length > 0) {
        reportLines.push("Заголовок документа (document.title):");
        for (var i = 0; i < descriptionData.titleInfo['documentTitle'].length; i++) {
            reportLines.push("  • " + descriptionData.titleInfo['documentTitle'][i]);
        }
        hasDescriptionData = true;
    }
    
    // 2. Информация из HEAD title
    if (descriptionData.titleInfo['headTitle'] && descriptionData.titleInfo['headTitle'].length > 0) {
        reportLines.push("Заголовок в HEAD:");
        for (var i = 0; i < descriptionData.titleInfo['headTitle'].length; i++) {
            reportLines.push("  • " + descriptionData.titleInfo['headTitle'][i]);
        }
        hasDescriptionData = true;
    }
    
    // 3. Найденные теги с метаданными
    if (descriptionData.titleInfo['title'] && descriptionData.titleInfo['title'].length > 0) {
        reportLines.push("Заголовки (теги TITLE/BOOK-TITLE):");
        for (var i = 0; i < descriptionData.titleInfo['title'].length; i++) {
            reportLines.push("  • " + descriptionData.titleInfo['title'][i]);
        }
        hasDescriptionData = true;
    }
    
    if (descriptionData.titleInfo['author'] && descriptionData.titleInfo['author'].length > 0) {
        reportLines.push("Авторы (теги AUTHOR):");
        for (var i = 0; i < descriptionData.titleInfo['author'].length; i++) {
            reportLines.push("  • " + descriptionData.titleInfo['author'][i]);
        }
        hasDescriptionData = true;
    }
    
    if (descriptionData.titleInfo['annotation'] && descriptionData.titleInfo['annotation'].length > 0) {
        reportLines.push("Аннотации:");
        for (var i = 0; i < descriptionData.titleInfo['annotation'].length; i++) {
            var ann = descriptionData.titleInfo['annotation'][i];
            if (ann.length > 100) ann = ann.substring(0, 100) + "...";
            reportLines.push("  • " + ann);
        }
        hasDescriptionData = true;
    }
    
    if (descriptionData.documentInfo['date'] && descriptionData.documentInfo['date'].length > 0) {
        reportLines.push("Даты:");
        for (var i = 0; i < descriptionData.documentInfo['date'].length; i++) {
            reportLines.push("  • " + descriptionData.documentInfo['date'][i]);
        }
        hasDescriptionData = true;
    }
    
    // 4. Кастомные метаданные (по ID или META-тегам)
    if (hasCustomInfo) {
        reportLines.push("Прочие метаданные:");
        for (var key in descriptionData.customInfo) {
            if (descriptionData.customInfo.hasOwnProperty(key)) {
                reportLines.push("  " + key + ":");
                for (var i = 0; i < descriptionData.customInfo[key].length; i++) {
                    var val = descriptionData.customInfo[key][i];
                    if (val.length > 80) val = val.substring(0, 80) + "...";
                    reportLines.push("    • " + val);
                }
            }
        }
        hasDescriptionData = true;
    }
    
    if (!hasDescriptionData) {
        reportLines.push("Метаданные description не найдены в HTML-представлении.");
        reportLines.push("ВНИМАНИЕ: Раздел <description> FB2 обычно скрыт в HTML-режиме FBE.");
        reportLines.push("Для доступа к полным метаданным нужен доступ к исходному XML FB2.");
    }
    
    reportLines.push("");
    reportLines.push("ОБЩАЯ СТАТИСТИКА ДОКУМЕНТА:");
    reportLines.push("----------------------------");
    reportLines.push("Всего элементов: " + totalElements);
    reportLines.push("Всего текстовых узлов: " + totalTextNodes);
    reportLines.push("Уникальных тегов: " + uniqueTagsCount);
    reportLines.push("Уникальных классов: " + uniqueClassesCount);
    reportLines.push("Уникальных атрибутов: " + uniqueAttributesCount);
    reportLines.push("Уникальных сочетаний тег+класс: " + uniqueCombinationsCount);
    reportLines.push("");
    
    reportLines.push("==================================================");
    reportLines.push("Полный отчет о тегах и классах сохранен в файле.");
    
    var reportText = reportLines.join('\r\n');
    
    // Сохранение отчета
    var saved = false;
    var finalName = "";
    
    for (var attempt = 1; attempt <= 10; attempt++) {
        var testName = "00_tag_detector_report_" + attempt + ".txt";
        try {
            saved = window.external.SaveBinary(testName, reportText, 0);
            if (saved) {
                finalName = testName;
                break;
            }
        } catch(e) {
            // Продолжаем
        }
    }
    
    // Итог
    var endTime = new Date().getTime();
    var execTime = ((endTime - startTime) / 1000).toFixed(2);
    
    var finalMsg = "-----------------------------------\n";
    finalMsg += scriptName + " v" + scriptVersion + "\n";
    finalMsg += "-----------------------------------\n";
    
    if (saved) {
        finalMsg += "✓ Отчет создан!\n\n";
        finalMsg += "Файл: " + finalName + "\n";
        finalMsg += "Всего элементов: " + totalElements + "\n";
        finalMsg += "Уникальных тегов: " + uniqueTagsCount + "\n";
        finalMsg += "Уникальных классов: " + uniqueClassesCount + "\n\n";
        
        if (hasDescriptionData) {
            finalMsg += "✓ Найдены метаданные документа\n";
        } else {
            finalMsg += "⚠ Метаданные не найдены (скрыты в XML)\n";
        }
        
        finalMsg += "Время анализа: " + execTime + " сек";
    } else {
        finalMsg += "✗ Ошибка!\n\n";
        finalMsg += "Не удалось создать файл отчета.";
    }
    
    finalMsg += "\n-----------------------------------";
    
    MsgBox(finalMsg, "FBE скрипт");
}
