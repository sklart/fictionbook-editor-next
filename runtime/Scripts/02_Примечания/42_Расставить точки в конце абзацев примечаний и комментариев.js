// Скрипт "Расставить точки в конце абзацев примечаний и комментариев" для редактора FBE
// version 1.8
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для расстановки точек в конце "обычных" абзацев
// в разделах сносок (примечаний) и комментариев,
// если такой абзац не заканчивается на типичный для конца абзаца знак препинания .?!,;:…

// Заголовки, эпиграфы, цитаты, стихи, аннотации, подзаголовки в данных разделах
// не обрабатываются (точки в них не ставятся).
// В статистике указываются номера конкретных разделов примечаний, где были расставлены точки.
// Поддержка отмены действий (Ctrl+Z).

// version 1.8, 06.01.2026
//======================================

function Run() {
    // НАСТРОЙКА ФОРМАТИРОВАНИЯ ТОЧКИ
    // 0 - точка добавляется без форматирования (после всех тегов)
    // 1 - точка добавляется с форматированием последнего текстового фрагмента
    var FORMAT_DOT = 1; // По умолчанию 1
    
    // НАСТРОЙКА ОГРАНИЧЕНИЯ ВЫВОДА НОМЕРОВ
    // Максимальное количество номеров примечаний/комментариев для вывода в статистике
    // Если номеров больше этого значения, выводится "..." и количество оставшихся
    var MAX_NUMBERS_TO_SHOW = 50; // По умолчанию 50
    
    // Запускаем таймер для замера времени выполнения
    var startTime = new Date().getTime();
    
    // Получаем неразрывный пробел из настроек FBE
    try { 
        var nbspChar = window.external.GetNBSP(); 
        var nbspEntity; 
        if (nbspChar.charCodeAt(0) == 160) 
            nbspEntity = "&nbsp;"; 
        else 
            nbspEntity = nbspChar; 
    }
    catch(e) { 
        var nbspChar = String.fromCharCode(160); 
        var nbspEntity = "&nbsp;";
    };
    
    // Список необычных пробелов, которые должны обрабатываться как обычные
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
    
    // Знаки препинания, после которых НЕ ставим точку
    // Круглые скобки ) и кавычки » " НЕ входят в этот список - после них точку СТАВИМ!
    var punctuationMarks = ".?!,;:…";
    
    // Функция для обрезки пробелов в конце строки (аналог trimRight)
    function trimRight(str) {
        var result = str;
        while (result.length > 0) {
            var lastChar = result.charAt(result.length - 1);
            var isSpace = false;
            
            // Проверяем обычный пробел
            if (lastChar == " " || lastChar == "\t" || lastChar == "\r" || lastChar == "\n") {
                isSpace = true;
            }
            // Проверяем необычные пробелы
            else {
                for (var i = 0; i < unusualSpaces.length; i++) {
                    if (lastChar == unusualSpaces.charAt(i)) {
                        isSpace = true;
                        break;
                    }
                }
            }
            
            if (isSpace) {
                result = result.substring(0, result.length - 1);
            } else {
                break;
            }
        }
        return result;
    }
    
    // Функция для получения текстового содержимого элемента (без HTML тегов)
    // Совместима с IE6
    function getElementText(element) {
        if (!element) return "";
        
        // Пробуем использовать innerText (поддерживается в IE и старых браузерах)
        if (typeof element.innerText !== "undefined") {
            return element.innerText;
        }
        
        // Если innerText нет, рекурсивно собираем текст из текстовых узлов
        var text = "";
        function collectText(node) {
            if (node.nodeType == 3) { // TEXT_NODE
                text += node.nodeValue;
            } else if (node.nodeType == 1) { // ELEMENT_NODE
                for (var i = 0; i < node.childNodes.length; i++) {
                    collectText(node.childNodes[i]);
                }
            }
        }
        collectText(element);
        return text;
    }
    
    // Функция для добавления точки с форматированием
    function addFormattedDot(element) {
        if (!element || !element.lastChild) {
            // Если нет дочерних элементов, просто добавляем точку
            element.innerHTML = element.innerHTML + ".";
            return;
        }
        
        // Ищем последний текстовый узел или элемент
        var lastNode = element.lastChild;
        
        // Поднимаемся по дереву, пока не найдем текстовый узел или элемент без детей
        while (lastNode && lastNode.nodeType != 3 && lastNode.lastChild) {
            lastNode = lastNode.lastChild;
        }
        
        if (lastNode.nodeType == 3) { // Текстовый узел
            // Добавляем точку в конец текстового узла
            lastNode.nodeValue = lastNode.nodeValue + ".";
        } else if (lastNode.nodeType == 1) { // Элемент
            // Добавляем текстовый узел с точкой в конец элемента
            var dotNode = document.createTextNode(".");
            lastNode.appendChild(dotNode);
        } else {
            // Запасной вариант
            element.innerHTML = element.innerHTML + ".";
        }
    }
    
    // Функция для проверки комбинаций знаков препинания в конце строки
    function endsWithPunctuation(str) {
        if (str.length == 0) return false;
        
        // Проверяем каждый знак препинания из списка
        var lastChar = str.charAt(str.length - 1);
        for (var i = 0; i < punctuationMarks.length; i++) {
            if (lastChar == punctuationMarks.charAt(i)) {
                return true;
            }
        }
        
        // Также проверяем символ многоточия как отдельный случай
        if (lastChar == "…") {
            return true;
        }
        
        // Проверяем комбинации из двух символов
        if (str.length >= 2) {
            var lastTwo = str.substring(str.length - 2);
            if (lastTwo == "?!" || lastTwo == "!?" || 
                lastTwo == "?." || lastTwo == "!." ||
                lastTwo == "?," || lastTwo == "!," ||
                lastTwo == "?:" || lastTwo == "!:" ||
                lastTwo == "?;" || lastTwo == "!;" ||
                lastTwo == "..") {
                return true;
            }
            
            // Проверяем комбинации из трех символов
            if (str.length >= 3) {
                var lastThree = str.substring(str.length - 3);
                if (lastThree == "..." || lastThree == "!.." || lastThree == "?.." ||
                    lastThree == "?!!" || lastThree == "!!!" || lastThree == "???" ||
                    lastThree == "?!." || lastThree == "!?.") {
                    return true;
                }
            }
        }
        
        return false;
    }
    
    // Ищем разделы примечаний и комментариев
    var bodyDivs = document.getElementsByTagName("DIV");
    var notesSection = null;
    var commentsSection = null;
    
    for (var i = 0; i < bodyDivs.length; i++) {
        var div = bodyDivs[i];
        if (div.className == "body" && div.getAttribute("fbname")) {
            var fbname = div.getAttribute("fbname");
            if (fbname == "notes") {
                notesSection = div;
            } else if (fbname == "comments") {
                commentsSection = div;
            }
        }
    }
    
    // Если разделов не найдено
    if (!notesSection && !commentsSection) {
        var endTime = new Date().getTime();
        var elapsed = (endTime - startTime) / 1000;
        
        MsgBox("\"Расставить точки в конце абзацев примечаний и комментариев\"\n" +
               "ver. 1.8\n" +
               "---------------------------------------\n\n" +
               "В документе не найдено разделов примечаний (fbname=\"notes\") и комментариев (fbname=\"comments\").\n\n" +
               "Время выполнения: " + elapsed.toFixed(2) + " сек.");
        return;
    }
    
    var totalPointsAdded = 0;
    var notesPointsAdded = 0;
    var commentsPointsAdded = 0;
    
    // Массивы для хранения номеров примечаний и комментариев, куда добавлены точки
    var notesWithDots = [];
    var commentsWithDots = [];
    
    // Статистика по пропущенным элементам - отдельно для примечаний и комментариев
    var notesSkippedStats = {
        titles: 0,
        subtitles: 0,
        epigraphs: 0,
        annotations: 0,
        cites: 0,
        poems: 0,
        stanzas: 0,
        history: 0
    };
    
    var commentsSkippedStats = {
        titles: 0,
        subtitles: 0,
        epigraphs: 0,
        annotations: 0,
        cites: 0,
        poems: 0,
        stanzas: 0,
        history: 0
    };
    
    // Начинаем блок отмены действий
    window.external.BeginUndoUnit(document, "Расставить точки в примечаниях и комментариях");
    
    // Функция для получения номера примечания/комментария
    function getSectionNumber(section) {
        if (!section) return "";
        
        // Ищем заголовок с номером внутри секции
        var titles = section.getElementsByTagName("DIV");
        for (var i = 0; i < titles.length; i++) {
            if (titles[i].className == "title") {
                var paragraphs = titles[i].getElementsByTagName("P");
                for (var j = 0; j < paragraphs.length; j++) {
                    var text = getElementText(paragraphs[j]);
                    if (text && text.length > 0) {
                        return text;
                    }
                }
            }
        }
        
        return "";
    }
    
    // Функция для обработки одного раздела
    function processSection(section, sectionName, skippedStats, dotsArray) {
        if (!section) return 0;
        
        var pointsInSection = 0;
        
        // Ищем все секции внутри раздела
        var sections = section.getElementsByTagName("DIV");
        for (var i = 0; i < sections.length; i++) {
            var subSection = sections[i];
            
            // Проверяем, что это секция примечания/комментария (class="section")
            if (subSection.className == "section") {
                // Получаем номер этой секции
                var sectionNumber = getSectionNumber(subSection);
                var sectionHasDot = false;
                
                // Ищем все абзацы P внутри этой секции
                var paragraphs = subSection.getElementsByTagName("P");
                for (var j = 0; j < paragraphs.length; j++) {
                    var paragraph = paragraphs[j];
                    
                    // Проверяем, что этот P не находится внутри исключенных структур
                    var parent = paragraph.parentNode;
                    var skipParagraph = false;
                    var skipReason = "";
                    
                    while (parent && parent != subSection) {
                        if (parent.nodeType == 1) { // ELEMENT_NODE
                            var className = parent.className;
                            if (className == "poem") {
                                skipParagraph = true;
                                skipReason = "poems";
                                break;
                            } else if (className == "cite") {
                                skipParagraph = true;
                                skipReason = "cites";
                                break;
                            } else if (className == "epigraph") {
                                skipParagraph = true;
                                skipReason = "epigraphs";
                                break;
                            } else if (className == "annotation") {
                                skipParagraph = true;
                                skipReason = "annotations";
                                break;
                            } else if (className == "history") {
                                skipParagraph = true;
                                skipReason = "history";
                                break;
                            } else if (className == "stanza") {
                                skipParagraph = true;
                                skipReason = "stanzas";
                                break;
                            }
                        }
                        parent = parent.parentNode;
                    }
                    
                    // Также пропускаем абзацы внутри заголовков и подзаголовков
                    if (!skipParagraph) {
                        var parentElement = paragraph.parentNode;
                        if (parentElement && parentElement.className == "title") {
                            skipParagraph = true;
                            skipReason = "titles";
                        } else if (paragraph.className == "subtitle") {
                            skipParagraph = true;
                            skipReason = "subtitles";
                        }
                    }
                    
                    if (skipParagraph && skipReason && skippedStats) {
                        // Увеличиваем счетчик пропущенных элементов для данного раздела
                        if (skippedStats[skipReason] !== undefined) {
                            skippedStats[skipReason]++;
                        }
                    } else if (!skipParagraph) {
                        // Получаем ТЕКСТОВОЕ содержимое абзаца (без HTML тегов)
                        var text = getElementText(paragraph);
                        
                        // Обрезаем пробелы в конце
                        var trimmedText = trimRight(text);
                        
                        // Если после обрезки текст не пустой
                        if (trimmedText.length > 0) {
                            // Проверяем, заканчивается ли текст знаком препинания
                            if (!endsWithPunctuation(trimmedText)) {
                                // Добавляем точку в зависимости от настройки форматирования
                                if (FORMAT_DOT == 1) {
                                    addFormattedDot(paragraph);
                                } else {
                                    paragraph.innerHTML = paragraph.innerHTML + ".";
                                }
                                pointsInSection++;
                                totalPointsAdded++;
                                sectionHasDot = true;
                            }
                        }
                    }
                }
                
                // Если в этой секции была добавлена точка, сохраняем номер
                if (sectionHasDot && sectionNumber && dotsArray) {
                    dotsArray[dotsArray.length] = sectionNumber;
                }
            }
        }
        
        return pointsInSection;
    }
    
    // Обрабатываем разделы
    if (notesSection) {
        notesPointsAdded = processSection(notesSection, "примечаний", notesSkippedStats, notesWithDots);
    }
    if (commentsSection) {
        commentsPointsAdded = processSection(commentsSection, "комментариев", commentsSkippedStats, commentsWithDots);
    }
    
    // Заканчиваем блок отмены действий
    window.external.EndUndoUnit(document);
    
    // Формируем сообщение с результатами
    var endTime = new Date().getTime();
    var elapsed = (endTime - startTime) / 1000;
    
    var message = "\"Расставить точки в конце абзацев примечаний и комментариев\"\n" +
                  "ver. 1.8\n" +
                  "---------------------------------------\n\n";
    
    // Формируем информацию о найденных разделах
    var foundSections = [];
    if (notesSection) foundSections.push("примечаний");
    if (commentsSection) foundSections.push("комментариев");
    
    if (foundSections.length == 2) {
        message += "Обработаны разделы примечаний и комментариев.\n\n";
    } else if (foundSections.length == 1) {
        message += "Обработан раздел " + foundSections[0] + ".\n";
        if (!notesSection) {
            message += "Раздел примечаний не найден.\n\n";
        } else if (!commentsSection) {
            message += "Раздел комментариев не найден.\n\n";
        }
    }
    
    message += "Результаты:\n";
    
    // Выводим только те результаты, которые реально есть
    if (notesSection) {
        message += "• Примечания: " + notesPointsAdded + " точек добавлено";
        if (notesWithDots.length > 0) {
            message += " в номера примечаний:\n  ";
            // Формируем список номеров (ограничено MAX_NUMBERS_TO_SHOW)
            for (var i = 0; i < notesWithDots.length && i < MAX_NUMBERS_TO_SHOW; i++) {
                if (i > 0) message += ", ";
                message += notesWithDots[i];
            }
            if (notesWithDots.length > MAX_NUMBERS_TO_SHOW) {
                message += ", ... (еще " + (notesWithDots.length - MAX_NUMBERS_TO_SHOW) + ")";
            }
            message += "\n";
        } else {
            message += "\n";
        }
    }
    if (commentsSection) {
        message += "• Комментарии: " + commentsPointsAdded + " точек добавлено";
        if (commentsWithDots.length > 0) {
            message += " в номера комментариев:\n  ";
            // Формируем список номеров (ограничено MAX_NUMBERS_TO_SHOW)
            for (var i = 0; i < commentsWithDots.length && i < MAX_NUMBERS_TO_SHOW; i++) {
                if (i > 0) message += ", ";
                message += commentsWithDots[i];
            }
            if (commentsWithDots.length > MAX_NUMBERS_TO_SHOW) {
                message += ", ... (еще " + (commentsWithDots.length - MAX_NUMBERS_TO_SHOW) + ")";
            }
            message += "\n";
        } else {
            message += "\n";
        }
    }
    
    message += "• Всего: " + totalPointsAdded + " точек добавлено\n\n";
    
    // Добавляем информацию о настройке форматирования
    message += "Настройка форматирования точки: " + FORMAT_DOT + "\n";
    message += "(0 - без форматирования, 1 - с форматированием)\n\n";
    
    // Функция для форматирования статистики пропущенных элементов
    function formatSkippedStats(skippedStats) {
        var result = "";
        var hasItems = false;
        
        if (skippedStats.titles > 0) {
            result += "• Заголовки: " + skippedStats.titles + " пропущено\n";
            hasItems = true;
        }
        if (skippedStats.subtitles > 0) {
            result += "• Подзаголовки: " + skippedStats.subtitles + " пропущено\n";
            hasItems = true;
        }
        if (skippedStats.epigraphs > 0) {
            result += "• Эпиграфы: " + skippedStats.epigraphs + " пропущено\n";
            hasItems = true;
        }
        if (skippedStats.annotations > 0) {
            result += "• Аннотации: " + skippedStats.annotations + " пропущено\n";
            hasItems = true;
        }
        if (skippedStats.cites > 0) {
            result += "• Цитаты: " + skippedStats.cites + " пропущено\n";
            hasItems = true;
        }
        if (skippedStats.poems > 0) {
            result += "• Стихи: " + skippedStats.poems + " пропущено\n";
            hasItems = true;
        }
        if (skippedStats.stanzas > 0) {
            result += "• Строфы: " + skippedStats.stanzas + " пропущено\n";
            hasItems = true;
        }
        if (skippedStats.history > 0) {
            result += "• History: " + skippedStats.history + " пропущено\n";
            hasItems = true;
        }
        
        if (!hasItems) {
            result = "  (нет пропущенных элементов)\n";
        }
        
        return result;
    }
    
    // Добавляем статистику по пропущенным элементам только если есть хотя бы один раздел
    var hasSkippedStats = false;
    var notesHasSkipped = false;
    var commentsHasSkipped = false;
    
    // Проверяем, есть ли пропущенные элементы в примечаниях
    if (notesSection) {
        for (var key in notesSkippedStats) {
            if (notesSkippedStats[key] > 0) {
                notesHasSkipped = true;
                break;
            }
        }
    }
    
    // Проверяем, есть ли пропущенные элементы в комментариях
    if (commentsSection) {
        for (var key in commentsSkippedStats) {
            if (commentsSkippedStats[key] > 0) {
                commentsHasSkipped = true;
                break;
            }
        }
    }
    
    // Если есть пропущенные элементы хотя бы в одном разделе
    if (notesHasSkipped || commentsHasSkipped) {
        message += "Пропущенные элементы:\n\n";
        
        // Статистика для примечаний (если раздел найден и есть пропущенные элементы)
        if (notesSection && notesHasSkipped) {
            message += "В примечаниях:\n";
            message += formatSkippedStats(notesSkippedStats) + "\n";
        }
        
        // Статистика для комментариев (если раздел найден и есть пропущенные элементы)
        if (commentsSection && commentsHasSkipped) {
            message += "В комментариях:\n";
            message += formatSkippedStats(commentsSkippedStats) + "\n";
        }
    }
    
    message += "Время выполнения: " + elapsed.toFixed(2) + " сек.";
    
    MsgBox(message);
}
