// Скрипт "Заменить Глава 1 на Первая глава (Часть, Книга, Том, Раздел)" для редактора FBE
// version 3.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для замены числовых обозначений Глав, Частей, Книг, Томов и Разделов
// на текстовые (порядковые числительные) в fb2 документах.
// Преобразуются стандартные Глава, Часть, Книга, Том, Раздел.
// Пример преобразования:
// Глава 35 ==> Тридцать пятая глава, Том 4 ==> Четвертый том
// Скрипт обрабатывает числа от 1 до 999 с правильным согласованием по роду и падежу.
// Настройка обработки только заголовков или во всем тексте, с возможностью ограничения
// замены только началом абзаца для обычного текста.
// Регистр исходного слова (заглавная/строчная буква) сохраняется.
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 3.1, 31.03.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Заменить Глава 1 на Первая глава (Часть, Книга, Том, Раздел)";
    var version = "3.1";
    
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
    
    // Заменять текст только в начале абзаца (для обычного текста, а не для заголовков)
    // 1 - да, 0 - нет (в любом месте абзаца)
    var replaceOnlyAtParagraphStart = 1;
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    var startTime = new Date().getTime(); // Начало замера времени
    
    // ==================================================
    // СЛОВАРИ ДЛЯ ЧИСЛИТЕЛЬНЫХ
    // ==================================================
    
    // Единицы (женский род)
    var unitsFemale = {
        '1': 'первая', '2': 'вторая', '3': 'третья',
        '4': 'четвёртая', '5': 'пятая', '6': 'шестая',
        '7': 'седьмая', '8': 'восьмая', '9': 'девятая'
    };
    
    // Единицы (мужской род)
    var unitsMale = {
        '1': 'первый', '2': 'второй', '3': 'третий',
        '4': 'четвёртый', '5': 'пятый', '6': 'шестой',
        '7': 'седьмой', '8': 'восьмой', '9': 'девятый'
    };
    
    // Десятки (для составных чисел, БЕЗ РОДА)
    var tensAny = {
        '20': 'двадцать', '30': 'тридцать', '40': 'сорок',
        '50': 'пятьдесят', '60': 'шестьдесят', '70': 'семьдесят',
        '80': 'восемьдесят', '90': 'девяносто'
    };
    
    // Круглые десятки (женский род)
    var tensFemale = {
        '20': 'двадцатая', '30': 'тридцатая', '40': 'сороковая',
        '50': 'пятидесятая', '60': 'шестидесятая', '70': 'семидесятая',
        '80': 'восьмидесятая', '90': 'девяностая'
    };
    
    // Круглые десятки (мужской род)
    var tensMale = {
        '20': 'двадцатый', '30': 'тридцатый', '40': 'сороковой',
        '50': 'пятидесятый', '60': 'шестидесятый', '70': 'семидесятый',
        '80': 'восьмидесятый', '90': 'девяностый'
    };
    
    // Числа 10-19 (женский род)
    var teensFemale = {
        '10': 'десятая', '11': 'одиннадцатая', '12': 'двенадцатая',
        '13': 'тринадцатая', '14': 'четырнадцатая', '15': 'пятнадцатая',
        '16': 'шестнадцатая', '17': 'семнадцатая', '18': 'восемнадцатая',
        '19': 'девятнадцатая'
    };
    
    // Числа 10-19 (мужской род)
    var teensMale = {
        '10': 'десятый', '11': 'одиннадцатый', '12': 'двенадцатый',
        '13': 'тринадцатый', '14': 'четырнадцатый', '15': 'пятнадцатый',
        '16': 'шестнадцатый', '17': 'семнадцатый', '18': 'восемнадцатый',
        '19': 'девятнадцатый'
    };
    
    // Сотни (круглые, женский род)
    var hundredsFemale = {
        '100': 'сотая', '200': 'двухсотая', '300': 'трёхсотая',
        '400': 'четырёхсотая', '500': 'пятисотая', '600': 'шестисотая',
        '700': 'семисотая', '800': 'восьмисотая', '900': 'девятисотая'
    };
    
    // Сотни (круглые, мужской род)
    var hundredsMale = {
        '100': 'сотый', '200': 'двухсотый', '300': 'трёхсотый',
        '400': 'четырёхсотый', '500': 'пятисотый', '600': 'шестисотый',
        '700': 'семисотый', '800': 'восьмисотый', '900': 'девятисотый'
    };
    
    // Сотни (для составных чисел, БЕЗ РОДА)
    var hundredsAny = {
        '100': 'сто', '200': 'двести', '300': 'триста',
        '400': 'четыреста', '500': 'пятьсот', '600': 'шестьсот',
        '700': 'семьсот', '800': 'восемьсот', '900': 'девятьсот'
    };
    
    // ==================================================
    // ФУНКЦИИ ПРЕОБРАЗОВАНИЯ ЧИСЕЛ В ТЕКСТ
    // ==================================================
    
    // Функция преобразования числа в текст (женский род)
    function getFemaleText(numStr) {
        var num = parseInt(numStr, 10);
        
        // 1-9
        if (num >= 1 && num <= 9) {
            return unitsFemale[numStr];
        }
        
        // 10-19
        if (num >= 10 && num <= 19) {
            return teensFemale[numStr];
        }
        
        // 100-900 (круглые)
        if (num >= 100 && num <= 900 && num % 100 == 0) {
            return hundredsFemale[numStr];
        }
        
        // Двузначные числа (20-99)
        if (num >= 20 && num <= 99) {
            // 20, 30, 40... 90 (круглые)
            if (numStr.charAt(1) == '0') {
                return tensFemale[numStr];
            }
            
            // Составные числа (21-99, кроме 10-19)
            var tens = numStr.charAt(0) + '0';
            var units = numStr.charAt(1);
            var tensText = tensAny[tens];
            var unitsText = unitsFemale[units];
            
            if (tensText && unitsText) {
                return tensText + ' ' + unitsText;
            }
        }
        
        // Трехзначные числа (101-999, кроме круглых сотен)
        if (num >= 101 && num <= 999) {
            var hundreds = Math.floor(num / 100) * 100;
            var remainder = num % 100;
            var hundredsText = hundredsAny[hundreds.toString()];
            
            if (remainder == 0) {
                return hundredsFemale[hundreds.toString()];
            } else if (remainder >= 1 && remainder <= 19) {
                var remainderText = getFemaleText(remainder.toString());
                return hundredsText + ' ' + remainderText;
            } else {
                var remainderText = getFemaleText(remainder.toString());
                return hundredsText + ' ' + remainderText;
            }
        }
        
        return numStr;
    }
    
    // Функция преобразования числа в текст (мужской род)
    function getMaleText(numStr) {
        var num = parseInt(numStr, 10);
        
        // 1-9
        if (num >= 1 && num <= 9) {
            return unitsMale[numStr];
        }
        
        // 10-19
        if (num >= 10 && num <= 19) {
            return teensMale[numStr];
        }
        
        // 100-900 (круглые)
        if (num >= 100 && num <= 900 && num % 100 == 0) {
            return hundredsMale[numStr];
        }
        
        // Двузначные числа (20-99)
        if (num >= 20 && num <= 99) {
            // 20, 30, 40... 90 (круглые)
            if (numStr.charAt(1) == '0') {
                return tensMale[numStr];
            }
            
            // Составные числа (21-99, кроме 10-19)
            var tens = numStr.charAt(0) + '0';
            var units = numStr.charAt(1);
            var tensText = tensAny[tens];
            var unitsText = unitsMale[units];
            
            if (tensText && unitsText) {
                return tensText + ' ' + unitsText;
            }
        }
        
        // Трехзначные числа (101-999, кроме круглых сотен)
        if (num >= 101 && num <= 999) {
            var hundreds = Math.floor(num / 100) * 100;
            var remainder = num % 100;
            var hundredsText = hundredsAny[hundreds.toString()];
            
            if (remainder == 0) {
                return hundredsMale[hundreds.toString()];
            } else if (remainder >= 1 && remainder <= 19) {
                var remainderText = getMaleText(remainder.toString());
                return hundredsText + ' ' + remainderText;
            } else {
                var remainderText = getMaleText(remainder.toString());
                return hundredsText + ' ' + remainderText;
            }
        }
        
        return numStr;
    }
    
    // Функция замены с сохранением регистра
    function replaceChapter(match, type, num) {
        var typeLower = type.toLowerCase();
        var result;
        
        if (typeLower == "глава") {
            result = getFemaleText(num) + ' глава';
        } else if (typeLower == "часть") {
            result = getFemaleText(num) + ' часть';
        } else if (typeLower == "книга") {
            result = getFemaleText(num) + ' книга';
        } else if (typeLower == "том") {
            result = getMaleText(num) + ' том';
        } else if (typeLower == "раздел") {
            result = getMaleText(num) + ' раздел';
        } else {
            return match;
        }
        
        // Сохраняем регистр: если исходное слово было с заглавной буквы
        if (type.charAt(0) == type.charAt(0).toUpperCase()) {
            result = result.charAt(0).toUpperCase() + result.slice(1);
        }
        
        return result;
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
        
        // Проверяем все возможные варианты: Глава, Часть, Книга, Том, Раздел
        var newText = text.replace(/(Глава|Часть|Книга|Том|Раздел)\s+(\d+)/gi, function(match, type, num) {
            changes++;
            return replaceChapter(match, type, num);
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
