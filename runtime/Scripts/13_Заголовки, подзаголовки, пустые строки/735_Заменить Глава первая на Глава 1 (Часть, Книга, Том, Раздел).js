// Скрипт "Заменить Глава первая на Глава 1 (Часть, Книга, Том, Раздел)" для редактора FBE
// version 1.0
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для замены числовых обозначений Глав, Частей, Книг, Томов и Разделов
// на текстовые (порядковые числительные) в fb2 документах.
// Преобразуются стандартные Глава, Часть, Книга, Том, Раздел.
// Пример преобразования:
// Глава тридцать пятая глава ==> Глава 35, Том четвёртый ==> Том 4
// Скрипт обрабатывает числа от 1 до 999 с правильным согласованием по роду и падежу.
// Настройка обработки только заголовков или во всем тексте, с возможностью ограничения
// замены только началом абзаца для обычного текста.
// Регистр исходного слова (заглавная/строчная буква) сохраняется.
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.0, 31.03.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Заменить Глава первая на Глава 1 (Часть, Книга, Том, Раздел)";
    var version = "1.0";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1;
    
    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0; // 0 - нет, 1 - да
    
    // Обрабатывать раздел комментариев
    var processCommentsSection = 0; // 0 - нет, 1 - да
    
    // Режим работы: 0 - заменять во всем тексте, 1 - заменять только в заголовках DIV class="title"
    var processOnlyTitles = 1; // По умолчанию - 1 (только заголовки)
    
    // Заменять текст только в начале абзаца (для обычного текста, не заголовков)
    // 1 - да, 0 - нет (в любом месте абзаца)
    var replaceOnlyAtParagraphStart = 1;
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    var startTime = new Date().getTime(); // Начало замера времени
    
    // ==================================================
    // СЛОВАРИ ДЛЯ ОБРАТНОГО ПРЕОБРАЗОВАНИЯ ТЕКСТА В ЧИСЛА
    // ==================================================
    
    // Единицы (женский род)
    var unitsFemaleToNum = {
        'первая': '1', 'вторая': '2', 'третья': '3',
        'четвёртая': '4', 'пятая': '5', 'шестая': '6',
        'седьмая': '7', 'восьмая': '8', 'девятая': '9'
    };
    
    // Единицы (мужской род)
    var unitsMaleToNum = {
        'первый': '1', 'второй': '2', 'третий': '3',
        'четвёртый': '4', 'пятый': '5', 'шестой': '6',
        'седьмой': '7', 'восьмой': '8', 'девятый': '9'
    };
    
    // Десятки (для составных чисел, БЕЗ РОДА)
    var tensAnyToNum = {
        'двадцать': '20', 'тридцать': '30', 'сорок': '40',
        'пятьдесят': '50', 'шестьдесят': '60', 'семьдесят': '70',
        'восемьдесят': '80', 'девяносто': '90'
    };
    
    // Круглые десятки (женский род)
    var tensFemaleToNum = {
        'двадцатая': '20', 'тридцатая': '30', 'сороковая': '40',
        'пятидесятая': '50', 'шестидесятая': '60', 'семидесятая': '70',
        'восьмидесятая': '80', 'девяностая': '90'
    };
    
    // Круглые десятки (мужской род)
    var tensMaleToNum = {
        'двадцатый': '20', 'тридцатый': '30', 'сороковой': '40',
        'пятидесятый': '50', 'шестидесятый': '60', 'семидесятый': '70',
        'восьмидесятый': '80', 'девяностый': '90'
    };
    
    // Числа 10-19 (женский род)
    var teensFemaleToNum = {
        'десятая': '10', 'одиннадцатая': '11', 'двенадцатая': '12',
        'тринадцатая': '13', 'четырнадцатая': '14', 'пятнадцатая': '15',
        'шестнадцатая': '16', 'семнадцатая': '17', 'восемнадцатая': '18',
        'девятнадцатая': '19'
    };
    
    // Числа 10-19 (мужской род)
    var teensMaleToNum = {
        'десятый': '10', 'одиннадцатый': '11', 'двенадцатый': '12',
        'тринадцатый': '13', 'четырнадцатый': '14', 'пятнадцатый': '15',
        'шестнадцатый': '16', 'семнадцатый': '17', 'восемнадцатый': '18',
        'девятнадцатый': '19'
    };
    
    // Сотни (круглые, женский род)
    var hundredsFemaleToNum = {
        'сотая': '100', 'двухсотая': '200', 'трёхсотая': '300',
        'четырёхсотая': '400', 'пятисотая': '500', 'шестисотая': '600',
        'семисотая': '700', 'восьмисотая': '800', 'девятисотая': '900'
    };
    
    // Сотни (круглые, мужской род)
    var hundredsMaleToNum = {
        'сотый': '100', 'двухсотый': '200', 'трёхсотый': '300',
        'четырёхсотый': '400', 'пятисотый': '500', 'шестисотый': '600',
        'семисотый': '700', 'восьмисотый': '800', 'девятисотый': '900'
    };
    
    // Сотни (для составных чисел, БЕЗ РОДА)
    var hundredsAnyToNum = {
        'сто': '100', 'двести': '200', 'триста': '300',
        'четыреста': '400', 'пятьсот': '500', 'шестьсот': '600',
        'семьсот': '700', 'восемьсот': '800', 'девятьсот': '900'
    };
    
    // ==================================================
    // ФУНКЦИИ ОБРАТНОГО ПРЕОБРАЗОВАНИЯ ТЕКСТА В ЧИСЛО
    // ==================================================
    
    // Функция преобразования текста в число (женский род)
    function getFemaleNumber(text) {
        // Проверяем круглые сотни
        if (hundredsFemaleToNum[text]) {
            return hundredsFemaleToNum[text];
        }
        
        // Проверяем 10-19
        if (teensFemaleToNum[text]) {
            return teensFemaleToNum[text];
        }
        
        // Проверяем круглые десятки
        if (tensFemaleToNum[text]) {
            return tensFemaleToNum[text];
        }
        
        // Проверяем единицы
        if (unitsFemaleToNum[text]) {
            return unitsFemaleToNum[text];
        }
        
        // Составные числа: "сто двадцать третья" и т.д.
        var parts = text.split(' ');
        if (parts.length == 2) {
            var firstPart = parts[0];
            var secondPart = parts[1];
            
            // Сотни + остаток
            if (hundredsAnyToNum[firstPart]) {
                var hundreds = parseInt(hundredsAnyToNum[firstPart], 10);
                var remainder = 0;
                
                // Проверяем остаток (10-19, десятки, единицы)
                if (teensFemaleToNum[secondPart]) {
                    remainder = parseInt(teensFemaleToNum[secondPart], 10);
                } else if (tensFemaleToNum[secondPart]) {
                    remainder = parseInt(tensFemaleToNum[secondPart], 10);
                } else if (unitsFemaleToNum[secondPart]) {
                    remainder = parseInt(unitsFemaleToNum[secondPart], 10);
                } else {
                    // Составной остаток: "двадцать первая"
                    var subparts = secondPart.split(' ');
                    if (subparts.length == 2) {
                        var tensText = subparts[0];
                        var unitsText = subparts[1];
                        if (tensAnyToNum[tensText] && unitsFemaleToNum[unitsText]) {
                            remainder = parseInt(tensAnyToNum[tensText], 10) + parseInt(unitsFemaleToNum[unitsText], 10);
                        }
                    }
                }
                
                if (remainder > 0) {
                    return (hundreds + remainder).toString();
                }
            }
            
            // Десятки + единицы: "двадцать первая"
            if (tensAnyToNum[firstPart] && unitsFemaleToNum[secondPart]) {
                var tens = parseInt(tensAnyToNum[firstPart], 10);
                var units = parseInt(unitsFemaleToNum[secondPart], 10);
                return (tens + units).toString();
            }
        }
        
        return null;
    }
    
    // Функция преобразования текста в число (мужской род)
    function getMaleNumber(text) {
        // Проверяем круглые сотни
        if (hundredsMaleToNum[text]) {
            return hundredsMaleToNum[text];
        }
        
        // Проверяем 10-19
        if (teensMaleToNum[text]) {
            return teensMaleToNum[text];
        }
        
        // Проверяем круглые десятки
        if (tensMaleToNum[text]) {
            return tensMaleToNum[text];
        }
        
        // Проверяем единицы
        if (unitsMaleToNum[text]) {
            return unitsMaleToNum[text];
        }
        
        // Составные числа: "сто двадцать третий" и т.д.
        var parts = text.split(' ');
        if (parts.length == 2) {
            var firstPart = parts[0];
            var secondPart = parts[1];
            
            // Сотни + остаток
            if (hundredsAnyToNum[firstPart]) {
                var hundreds = parseInt(hundredsAnyToNum[firstPart], 10);
                var remainder = 0;
                
                // Проверяем остаток (10-19, десятки, единицы)
                if (teensMaleToNum[secondPart]) {
                    remainder = parseInt(teensMaleToNum[secondPart], 10);
                } else if (tensMaleToNum[secondPart]) {
                    remainder = parseInt(tensMaleToNum[secondPart], 10);
                } else if (unitsMaleToNum[secondPart]) {
                    remainder = parseInt(unitsMaleToNum[secondPart], 10);
                } else {
                    // Составной остаток: "двадцать первый"
                    var subparts = secondPart.split(' ');
                    if (subparts.length == 2) {
                        var tensText = subparts[0];
                        var unitsText = subparts[1];
                        if (tensAnyToNum[tensText] && unitsMaleToNum[unitsText]) {
                            remainder = parseInt(tensAnyToNum[tensText], 10) + parseInt(unitsMaleToNum[unitsText], 10);
                        }
                    }
                }
                
                if (remainder > 0) {
                    return (hundreds + remainder).toString();
                }
            }
            
            // Десятки + единицы: "двадцать первый"
            if (tensAnyToNum[firstPart] && unitsMaleToNum[secondPart]) {
                var tens = parseInt(tensAnyToNum[firstPart], 10);
                var units = parseInt(unitsMaleToNum[secondPart], 10);
                return (tens + units).toString();
            }
        }
        
        return null;
    }
    
    // Функция обратной замены: "Первая глава" -> "Глава 1"
    function replaceChapterBack(match, numberText, type) {
        var typeLower = type.toLowerCase();
        var number;
        
        if (typeLower == "глава" || typeLower == "часть" || typeLower == "книга") {
            number = getFemaleNumber(numberText);
        } else if (typeLower == "том" || typeLower == "раздел") {
            number = getMaleNumber(numberText);
        } else {
            return match;
        }
        
        if (!number) {
            return match;
        }
        
        // Сохраняем регистр: если исходное слово было с заглавной буквы
        if (type.charAt(0) == type.charAt(0).toUpperCase()) {
            return type + ' ' + number;
        } else {
            return typeLower + ' ' + number;
        }
    }
    
    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================
    
    // Получаем неразрывный пробел из настроек FBE
    var nbspChar = " ";
    try {
        nbspChar = window.external.GetNBSP();
    } catch(e) {
        nbspChar = String.fromCharCode(160);
    }
    var nbspEntity = (nbspChar.charCodeAt(0) == 160) ? "&nbsp;" : nbspChar;
    
    // Функция проверки, является ли элемент тегом форматирования
    function isFormattingTag(element) {
        if (!element || element.nodeType != 1) return false;
        var tagName = element.nodeName;
        return (tagName == "STRONG" || tagName == "EM" || tagName == "B" || 
                tagName == "I" || tagName == "SPAN" || tagName == "A");
    }
    
    // Функция получения первого значимого текста из элемента (без тегов)
    function getFirstSignificantText(element) {
        var children = element.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 3) { // текстовый узел
                var text = child.nodeValue;
                // Проверяем, есть ли в нем непробельные символы
                for (var j = 0; j < text.length; j++) {
                    var ch = text.charAt(j);
                    if (ch != ' ' && ch != nbspChar && ch != '\t' && ch != '\r' && ch != '\n') {
                        // Возвращаем текст начиная с первого непробельного символа
                        return text.substring(j);
                    }
                }
            } else if (child.nodeType == 1) { // элемент
                // Если это тег форматирования — ищем внутри
                if (isFormattingTag(child)) {
                    var innerText = getFirstSignificantText(child);
                    if (innerText && innerText.length > 0) {
                        return innerText;
                    }
                } else {
                    // Любой другой тег (не форматирования) — ищем внутри
                    var innerText = getFirstSignificantText(child);
                    if (innerText && innerText.length > 0) {
                        return innerText;
                    }
                }
            }
        }
        return null;
    }
    
    // Функция проверки, начинается ли абзац с ключевого слова
    function isParagraphStartWithKeyword(paragraphElement) {
        if (!paragraphElement) return false;
        
        // Получаем первый значимый текст в абзаце
        var firstText = getFirstSignificantText(paragraphElement);
        if (!firstText || firstText.length == 0) return false;
        
        // Проверяем, начинается ли текст с одного из ключевых слов
        var keywords = ["Глава", "Часть", "Книга", "Том", "Раздел"];
        for (var i = 0; i < keywords.length; i++) {
            var kw = keywords[i];
            if (firstText.indexOf(kw) == 0) {
                return true;
            }
        }
        return false;
    }
    
    // Функция проверки, находится ли текстовый узел в начале абзаца
    function isAtParagraphStart(textNode) {
        // Находим родительский абзац P
        var parent = textNode.parentNode;
        var paragraphElement = null;
        
        while (parent) {
            if (parent.nodeType == 1 && parent.nodeName == "P") {
                paragraphElement = parent;
                break;
            }
            parent = parent.parentNode;
        }
        
        // Если не нашли P — не ограничиваем (для безопасности)
        if (!paragraphElement) {
            return true;
        }
        
        // Проверяем, начинается ли абзац с ключевого слова
        return isParagraphStartWithKeyword(paragraphElement);
    }
    
    // Функция обработки текстового узла (возвращает количество замен)
    function processTextNode(node, isInTitle) {
        var text = node.nodeValue;
        var changes = 0;
        
        // Если это обычный текст (не заголовок) и включена проверка начала абзаца
        if (!isInTitle && replaceOnlyAtParagraphStart == 1) {
            // Проверяем, начинается ли абзац с ключевого слова
            if (!isAtParagraphStart(node)) {
                return 0; // Абзац не начинается с ключевого слова — не заменяем
            }
        }
        
        // Шаблон для поиска: "Первая глава", "двадцать пятая часть" и т.д.
        // Ищем: (глава|часть|книга|том|раздел) + пробел + текстовое числительное
        var regex = /(Глава|Часть|Книга|Том|Раздел)\s+([а-яё]+(?:\s+[а-яё]+)?)/gi;
        
        var newText = text.replace(regex, function(match, type, numberText) {
            var result = replaceChapterBack(match, numberText, type);
            if (result != match) {
                changes++;
            }
            return result;
        });
        
        if (newText != text) {
            node.nodeValue = newText;
            return changes;
        }
        return 0;
    }
    
    // Рекурсивный обход всех текстовых узлов
    function processNode(node, stats, isInTitle) {
        var changes = 0;
        if (node.nodeType == 3) { // текстовый узел
            changes = processTextNode(node, isInTitle);
            if (stats && changes > 0) {
                stats.regularChanges += changes;
            }
        } else if (node.nodeType == 1) { // элемент
            // Защита сносок — не обрабатываем внутри A class=note
            if (node.nodeName == "A" && node.className == "note") {
                return 0;
            }
            
            // Определяем, находимся ли мы внутри заголовка
            var insideTitle = isInTitle;
            if (node.nodeName == "DIV" && node.className == "title") {
                insideTitle = true;
            }
            
            var children = node.childNodes;
            for (var i = 0; i < children.length; i++) {
                changes += processNode(children[i], stats, insideTitle);
            }
        }
        return changes;
    }
    
    // Функция обработки элемента заголовка (DIV class="title")
    function processTitleElement(titleElement, stats) {
        var changes = 0;
        var children = titleElement.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 3) { // текстовый узел
                var cnt = processTextNode(child, true); // true - это заголовок
                changes += cnt;
                if (stats && cnt > 0) {
                    stats.titleChanges += cnt;
                }
            } else if (child.nodeType == 1) { // элемент (например, P внутри заголовка)
                // Защита сносок внутри заголовка
                if (child.nodeName == "A" && child.className == "note") {
                    continue;
                }
                // Рекурсивно обрабатываем содержимое
                var innerChildren = child.childNodes;
                for (var j = 0; j < innerChildren.length; j++) {
                    if (innerChildren[j].nodeType == 3) {
                        var cnt2 = processTextNode(innerChildren[j], true); // true - это заголовок
                        changes += cnt2;
                        if (stats && cnt2 > 0) {
                            stats.titleChanges += cnt2;
                        }
                    } else if (innerChildren[j].nodeType == 1) {
                        // Если внутри есть еще элементы, обрабатываем их
                        var cnt3 = processNode(innerChildren[j], stats, true);
                        changes += cnt3;
                    }
                }
            }
        }
        return changes;
    }
    
    // Функция поиска всех заголовков DIV class="title" внутри body
    function findAllTitles(element, titlesArray) {
        if (!element) return;
        
        var children = element.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 1) { // элемент
                // Защита сносок — не заходим внутрь A class=note
                if (child.nodeName == "A" && child.className == "note") {
                    continue;
                }
                // Если нашли DIV с классом title — добавляем в массив
                if (child.nodeName == "DIV" && child.className == "title") {
                    titlesArray.push(child);
                } else {
                    // Иначе продолжаем поиск рекурсивно
                    findAllTitles(child, titlesArray);
                }
            }
        }
    }
    
    // Поиск всех body (основной, примечания, комментарии)
    function findAllBodies() {
        var bodies = [];
        var allDivs = document.getElementsByTagName("DIV");
        for (var i = 0; i < allDivs.length; i++) {
            var div = allDivs[i];
            if (div.className == "body") {
                var fbname = div.getAttribute("fbname") || "";
                var isMain = (fbname == "");
                var isNotes = (fbname == "notes");
                var isComments = (fbname == "comments");
                
                if (isMain) {
                    bodies.push({element: div, name: "основной раздел"});
                } else if (isNotes && processNotesSection) {
                    bodies.push({element: div, name: "раздел сносок"});
                } else if (isComments && processCommentsSection) {
                    bodies.push({element: div, name: "раздел комментариев"});
                }
            }
        }
        return bodies;
    }
    
    // ==================================================
    // ОСНОВНАЯ ЛОГИКА
    // ==================================================
    
    window.external.BeginUndoUnit(document, scriptName);
    
    var bodies = findAllBodies();
    var stats = {
        titleChanges: 0,
        regularChanges: 0,
        titlesCount: 0
    };
    var totalChanges = 0;
    
    if (processOnlyTitles == 0) {
        // Режим: замена во всем тексте
        for (var i = 0; i < bodies.length; i++) {
            var body = bodies[i];
            var titlesArray = [];
            findAllTitles(body.element, titlesArray);
            stats.titlesCount += titlesArray.length;
            
            // Сначала обрабатываем заголовки (чтобы посчитать их отдельно)
            for (var j = 0; j < titlesArray.length; j++) {
                stats.titleChanges += processTitleElement(titlesArray[j], null);
            }
            
            // Функция обхода без заголовков (для обычного текста)
            function processNodeExcludingTitles(node, titlesArray, statsObj) {
                var changes = 0;
                if (node.nodeType == 3) {
                    // Для обычного текста передаем isInTitle = false
                    changes = processTextNode(node, false);
                    if (statsObj && changes > 0) {
                        statsObj.regularChanges += changes;
                    }
                } else if (node.nodeType == 1) {
                    // Проверяем, является ли этот элемент заголовком
                    var isTitle = false;
                    for (var k = 0; k < titlesArray.length; k++) {
                        if (titlesArray[k] == node) {
                            isTitle = true;
                            break;
                        }
                    }
                    if (isTitle) {
                        return 0;
                    }
                    // Защита сносок
                    if (node.nodeName == "A" && node.className == "note") {
                        return 0;
                    }
                    var children = node.childNodes;
                    for (var n = 0; n < children.length; n++) {
                        changes += processNodeExcludingTitles(children[n], titlesArray, statsObj);
                    }
                }
                return changes;
            }
            
            processNodeExcludingTitles(body.element, titlesArray, stats);
        }
        totalChanges = stats.titleChanges + stats.regularChanges;
        
    } else {
        // Режим: замена только в заголовках DIV class="title"
        for (var i = 0; i < bodies.length; i++) {
            var body = bodies[i];
            var titlesArray = [];
            findAllTitles(body.element, titlesArray);
            stats.titlesCount += titlesArray.length;
            
            for (var j = 0; j < titlesArray.length; j++) {
                stats.titleChanges += processTitleElement(titlesArray[j], stats);
            }
        }
        totalChanges = stats.titleChanges;
    }
    
    window.external.EndUndoUnit(document);
    
    // ==================================================
    // ВРЕМЯ ВЫПОЛНЕНИЯ
    // ==================================================
    
    var endTime = new Date().getTime();
    var elapsed = (endTime - startTime) / 1000;
    
    // ==================================================
    // ФОРМИРОВАНИЕ СООБЩЕНИЯ СТАТИСТИКИ
    // ==================================================
    
    // Если тихий режим, но замен нет — всё равно показываем сообщение
    if (showStatistics == 0 && totalChanges == 0) {
        var msgNoChanges = scriptName + "\n" + "ver. " + version + "\n\n";
        msgNoChanges += "✓ Заменять нечего!\n";
        msgNoChanges += "\nВремя обработки: " + elapsed.toFixed(3) + " сек";
        MsgBox(msgNoChanges, scriptName + " ver." + version);
        return;
    }
    
    // Если showStatistics = 0 и замены есть — не показываем
    if (showStatistics == 0) {
        return;
    }
    
    // Формируем полную статистику
    var msg = scriptName + "\n" + "ver. " + version + "\n\n";
    
    msg += "✓ Обработано разделов: " + bodies.length + "\n\n";
    
    msg += "- Настройки обработки:\n";
    if (processOnlyTitles == 0) {
        msg += "  • Режим работы: Во всем тексте\n";
        if (replaceOnlyAtParagraphStart == 1) {
            msg += "  • Замена в обычном тексте: только в начале абзаца\n";
        } else {
            msg += "  • Замена в обычном тексте: в любом месте\n";
        }
    } else {
        msg += "  • Режим работы: Только в заголовках\n";
    }
    
    msg += "\n- Обработка разделов:\n";
    msg += "  • Основной раздел: ДА\n";
    msg += "  • Раздел сносок (примечаний): " + (processNotesSection ? "ДА" : "НЕТ") + "\n";
    msg += "  • Раздел комментариев: " + (processCommentsSection ? "ДА" : "НЕТ") + "\n";
    
    msg += "\n✓ Замен произведено: " + totalChanges + "\n";
    
    if (processOnlyTitles == 0) {
        // В режиме "во всем тексте" показываем разделение
        msg += "Из них:\n";
        msg += "  • В заголовках: " + stats.titleChanges + "\n";
        msg += "  • В обычном тексте: " + stats.regularChanges + "\n";
    } else {
        // В режиме "только в заголовках" показываем количество найденных заголовков
        if (stats.titlesCount > 0) {
            msg += "  • Найдено заголовков: " + stats.titlesCount + "\n";
        }
    }
    
    msg += "\nВремя обработки: " + elapsed.toFixed(3) + " сек";
    MsgBox(msg, scriptName + " ver." + version);
}
