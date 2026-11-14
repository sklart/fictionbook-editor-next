// Скрипт "Удалить точки в конце подзаголовков" для редактора FBE
// version 1.7
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для удаления лишних точек в конце подзаголовков (P class=subtitle) в fb2 документах.
// Удаляет одиночную точку после букв, цифр, скобок и кавычек. Оставляет 1-2 точки после ! и ?.
// Заменяет три точки подряд на символ многоточия (…). Удаляет ошибочные двойные точки в конце.
// Скрипт работает сразу со всем документом.
// По умолчанию обрабатывается только основной раздел документа, без сносок и комментариев.
// Отдельная настройка обработки разделов сносок, комментариев и блочных элементов.
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.7, 02.03.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Удалить точки в конце подзаголовков";
    var version = "1.7";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    // Настройка режима отображения:
    // 0 - не показывать ничего (только ошибки)
    // 1 - показывать анализ и статистику
    // 2 - показывать только статистику в конце
    var showStatistics = 1;

    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0;     // 0 - нет, 1 - да

    // Обрабатывать раздел комментариев
    var processCommentsSection = 0;     // 0 - нет, 1 - да

    // Обрабатывать ли уже размеченные "блочные" элементы
    // (poem, stanza, cite, annotation)
    var processBlockElements = 0;     // 0 - нет, 1 - да

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    // Получаем символ неразрывного пробела из настроек FBE
    var nbspChar;
    try {
        nbspChar = window.external.GetNBSP();
    } catch(e) {
        nbspChar = String.fromCharCode(160);
    }

    // Счётчики для статистики
    var totalSubtitles = 0;                // всего найдено подзаголовков
    var mainSubtitles = 0;                 // в основном разделе
    var notesSubtitles = 0;                // в сносках
    var commentsSubtitles = 0;              // в комментариях
    
    var willChangeCount = 0;                // сколько подзаголовков будут изменены (предварительный подсчёт)
    var willChangeMain = 0;                  // из них в основном
    var willChangeNotes = 0;                  // в сносках
    var willChangeComments = 0;               // в комментариях
    
    var processedSubtitles = 0;             // фактически изменено
    var processedMain = 0;                  // изменено в основном
    var processedNotes = 0;                  // изменено в сносках
    var processedComments = 0;               // изменено в комментариях
    
    var dotsRemoved = 0;                     // удалено одиночных точек
    var doubleDotsRemoved = 0;                // удалено двойных точек
    var ellipsisReplaced = 0;                // заменено на многоточие (3 → …)
    
    var skippedBySettings = 0;                // пропущено по настройкам
    var skippedByBlock = 0;                   // пропущено из-за блочных элементов

    // Массив для сбора подзаголовков к обработке
    var subtitlesToProcess = [];

    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================

    // Функция для получения типа раздела (основной, сноски, комментарии)
    function getSectionType(element) {
        var body = findParentBody(element);
        if (!body) return "main";
        
        var fbname = body.getAttribute("fbname") || "";
        if (fbname == "notes") return "notes";
        if (fbname == "comments") return "comments";
        return "main";
    }

    // Функция для поиска родительского body
    function findParentBody(element) {
        while (element) {
            if (element.nodeName == "DIV" && element.className == "body") {
                return element;
            }
            element = element.parentNode;
        }
        return null;
    }

    // Функция для проверки, находится ли элемент в блочном элементе
    function isInBlockElement(element) {
        if (!processBlockElements) return false;

        var parent = element.parentNode;
        while (parent) {
            if (parent.nodeName == "DIV") {
                var className = parent.className || "";
                if (className == "poem" || className == "stanza" || 
                    className == "cite" || className == "annotation") {
                    return true;
                }
            }
            parent = parent.parentNode;
        }
        return false;
    }

    // Функция для проверки, можно ли обрабатывать элемент
    function shouldProcessElement(element) {
        var sectionType = getSectionType(element);
        
        // Проверяем разделы
        if (sectionType == "notes" && !processNotesSection) return false;
        if (sectionType == "comments" && !processCommentsSection) return false;
        
        // Проверяем блочные элементы
        if (!processBlockElements && isInBlockElement(element)) return false;
        
        return true;
    }

    // Функция для предварительной проверки, будут ли изменения в подзаголовке
    function willHaveChanges(element) {
        var text = element.innerText || "";
        if (text.length == 0) return false;
        
        // Проверка на 3 точки
        if (text.length >= 3 && 
            text.charAt(text.length - 3) == '.' && 
            text.charAt(text.length - 2) == '.' && 
            text.charAt(text.length - 1) == '.') {
            
            // Проверяем исключения (!.. или ?..)
            if (text.length >= 4) {
                var beforeThree = text.charAt(text.length - 4);
                if (beforeThree == '!' || beforeThree == '?') return false;
            }
            if (text.length >= 4) {
                var char1 = text.charAt(text.length - 4);
                var char2 = text.charAt(text.length - 3);
                if ((char1 == '!' || char1 == '?') && (char2 == '!' || char2 == '?')) return false;
            }
            return true;
        }
        
        // Проверка на 2 точки
        if (text.length >= 2 && 
            text.charAt(text.length - 2) == '.' && 
            text.charAt(text.length - 1) == '.') {
            
            if (text.length >= 3) {
                var beforeTwo = text.charAt(text.length - 3);
                if (beforeTwo == '!' || beforeTwo == '?') return false;
            }
            if (text.length >= 4) {
                var char1 = text.charAt(text.length - 4);
                var char2 = text.charAt(text.length - 3);
                if ((char1 == '!' || char1 == '?') && (char2 == '!' || char2 == '?')) return false;
            }
            return true;
        }
        
        // Проверка на 1 точку
        if (text.length >= 1 && text.charAt(text.length - 1) == '.') {
            if (text.length >= 2) {
                var beforeOne = text.charAt(text.length - 2);
                if (beforeOne == '!' || beforeOne == '?') return false;
            }
            if (text.length >= 3) {
                var char1 = text.charAt(text.length - 3);
                var char2 = text.charAt(text.length - 2);
                if ((char1 == '!' || char1 == '?') && (char2 == '!' || char2 == '?')) return false;
            }
            return true;
        }
        
        return false;
    }

    // Функция обработки текстового узла
    function processTextNode(textNode) {
        var text = textNode.nodeValue;
        if (text.length == 0) return false;

        var processed = false;

        // СЛУЧАЙ 1: Три точки подряд (обычное многоточие) → заменяем на символ …
        if (text.length >= 3 && 
            text.charAt(text.length - 3) == '.' && 
            text.charAt(text.length - 2) == '.' && 
            text.charAt(text.length - 1) == '.') {
            
            // Проверяем, не идёт ли перед ними ! или ? (случай !.. или ?..)
            if (text.length >= 4) {
                var beforeThree = text.charAt(text.length - 4);
                if (beforeThree == '!' || beforeThree == '?') {
                    return false;
                }
            }
            
            // Проверяем случай двух знаков перед точкой (?!. или !!. или ??.)
            if (text.length >= 4) {
                var char1 = text.charAt(text.length - 4);
                var char2 = text.charAt(text.length - 3);
                if ((char1 == '!' || char1 == '?') && (char2 == '!' || char2 == '?')) {
                    return false;
                }
            }
            
            textNode.nodeValue = text.substring(0, text.length - 3) + "…";
            processed = true;
            ellipsisReplaced++;
        }
        
        // СЛУЧАЙ 2: Две точки подряд
        else if (text.length >= 2 && 
                 text.charAt(text.length - 2) == '.' && 
                 text.charAt(text.length - 1) == '.') {
            
            if (text.length >= 3) {
                var beforeTwo = text.charAt(text.length - 3);
                if (beforeTwo == '!' || beforeTwo == '?') {
                    return false;
                }
            }
            
            if (text.length >= 4) {
                var char1 = text.charAt(text.length - 4);
                var char2 = text.charAt(text.length - 3);
                if ((char1 == '!' || char1 == '?') && (char2 == '!' || char2 == '?')) {
                    return false;
                }
            }
            
            textNode.nodeValue = text.substring(0, text.length - 2);
            processed = true;
            doubleDotsRemoved++;
        }
        
        // СЛУЧАЙ 3: Одна точка в конце
        else if (text.length >= 1 && text.charAt(text.length - 1) == '.') {
            
            if (text.length >= 2) {
                var beforeOne = text.charAt(text.length - 2);
                if (beforeOne == '!' || beforeOne == '?') {
                    return false;
                }
            }
            
            if (text.length >= 3) {
                var char1 = text.charAt(text.length - 3);
                var char2 = text.charAt(text.length - 2);
                if ((char1 == '!' || char1 == '?') && (char2 == '!' || char2 == '?')) {
                    return false;
                }
            }
            
            textNode.nodeValue = text.substring(0, text.length - 1);
            processed = true;
            dotsRemoved++;
        }

        return processed;
    }

    // Рекурсивная функция обхода всех текстовых узлов внутри элемента
    function processElement(element) {
        var processed = false;
        var children = element.childNodes;
        
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            
            if (child.nodeType == 3) {
                if (processTextNode(child)) {
                    processed = true;
                }
            }
            else if (child.nodeType == 1) {
                // Не заходим внутрь сносок
                if (child.nodeName == "A" && child.className == "note") {
                    continue;
                }
                if (processElement(child)) {
                    processed = true;
                }
            }
        }
        
        return processed;
    }

    // ==================================================
    // ОСНОВНАЯ ЛОГИКА СКРИПТА
    // ==================================================

    // Находим все абзацы в документе
    var allParagraphs = document.getElementsByTagName("P");

    // Фаза 1: СБОР ДАННЫХ И СТАТИСТИКА
    for (var i = 0; i < allParagraphs.length; i++) {
        var p = allParagraphs[i];
        
        // Проверяем, что это подзаголовок
        if (p.className == "subtitle") {
            totalSubtitles++;
            
            // Определяем раздел
            var sectionType = getSectionType(p);
            switch(sectionType) {
                case "main": mainSubtitles++; break;
                case "notes": notesSubtitles++; break;
                case "comments": commentsSubtitles++; break;
            }

            // Проверяем, можно ли обрабатывать
            var canProcess = true;

            // Проверка разделов
            if (sectionType == "notes" && !processNotesSection) canProcess = false;
            else if (sectionType == "comments" && !processCommentsSection) canProcess = false;
            
            // Проверка блочных элементов
            if (canProcess && !processBlockElements && isInBlockElement(p)) canProcess = false;

            if (canProcess) {
                // Сохраняем для обработки
                subtitlesToProcess[subtitlesToProcess.length] = p;
                
                // Предварительно проверяем, будут ли изменения
                if (willHaveChanges(p)) {
                    willChangeCount++;
                    switch(sectionType) {
                        case "main": willChangeMain++; break;
                        case "notes": willChangeNotes++; break;
                        case "comments": willChangeComments++; break;
                    }
                }
            } else {
                skippedBySettings++;
                if (!processBlockElements && isInBlockElement(p)) {
                    skippedByBlock++;
                }
            }
        }
    }

    // Если подходящих подзаголовков не найдено
    if (subtitlesToProcess.length == 0) {
        if (showStatistics >= 1) {
            var msg = "";
            msg = "Подзаголовков с точкой в конце (согласно настройкам) не найдено.\n\n";
            msg += "Всего найдено подзаголовков: " + totalSubtitles + "\n";
            msg += "  • в основном разделе: " + mainSubtitles + "\n";
            if (notesSubtitles > 0) msg += "  • в сносках-примечаниях: " + notesSubtitles + "\n";
            if (commentsSubtitles > 0) msg += "  • в комментариях: " + commentsSubtitles + "\n";
            
            if (skippedBySettings > 0) {
                msg += "\nПропущено по настройкам: " + skippedBySettings + "\n";
                if (skippedByBlock > 0) {
                    msg += "  • в блочных элементах: " + skippedByBlock + "\n";
                }
            }
            
            MsgBox(msg, scriptName + " ver. " + version);
        }
        return;
    }

    // Формируем статистику для подтверждения (ПЕРВОЕ ОКНО)
    var statsMessage = "";
    statsMessage = scriptName + "\n";
    statsMessage += "ver. " + version + "\n";
    statsMessage += "---------------------------\n\n";
    
    statsMessage += "Всего найдено подзаголовков: " + totalSubtitles + "\n";
    statsMessage += "Из них:\n";
    statsMessage += "  • в основном разделе: " + mainSubtitles + "\n";
    if (notesSubtitles > 0) statsMessage += "  • в сносках-примечаниях: " + notesSubtitles + "\n";
    if (commentsSubtitles > 0) statsMessage += "  • в комментариях: " + commentsSubtitles + "\n\n";

    statsMessage += "Текущие настройки скрипта:\n";
    statsMessage += "  • Обработка раздела сносок: " + (processNotesSection ? "ДА" : "НЕТ") + "\n";
    statsMessage += "  • Обработка раздела комментариев: " + (processCommentsSection ? "ДА" : "НЕТ") + "\n";
    statsMessage += "  • Обработка блочных элементов (цитаты, стихи, аннотации): " + (processBlockElements ? "ДА" : "НЕТ") + "\n\n";

    statsMessage += "Будет изменено подзаголовков: " + willChangeCount + "\n";
    statsMessage += "Из них:\n";
    statsMessage += "  • В основном разделе: " + willChangeMain + "\n";
    if (willChangeNotes > 0) statsMessage += "  • В разделе сносок: " + willChangeNotes + "\n";
    if (willChangeComments > 0) statsMessage += "  • В разделе комментариев: " + willChangeComments + "\n";

    // Запрос подтверждения (только в режиме 1)
    if (showStatistics == 1) {
        if (willChangeCount == 0) {
            MsgBox(scriptName + "\nver. " + version + "\n---------------------------\n\n" + 
                   "Подзаголовки найдены, но точки в конце (подлежащие удалению) отсутствуют.\n\n" +
                   "Работа скрипта завершена.", 
                   scriptName + " ver. " + version);
            return;
        }
        
        if (!AskYesNo(statsMessage + "\n\nПродолжить?", scriptName + " ver. " + version)) {
            return;
        }
    } else if (showStatistics == 2) {
        MsgBox(statsMessage, scriptName + " ver. " + version);
        if (willChangeCount == 0) return;
    }

    // ==================================================
    // ОБРАБОТКА (с поддержкой Undo)
    // ==================================================
    
    // Засекаем время начала обработки (после всех диалогов)
    var startTime = new Date();

    // Начинаем операцию для отмены Ctrl+Z
    window.external.BeginUndoUnit(document, "Удаление точек в подзаголовках");

    // Обрабатываем собранные подзаголовки
    for (var j = 0; j < subtitlesToProcess.length; j++) {
        var subtitle = subtitlesToProcess[j];
        
        if (processElement(subtitle)) {
            processedSubtitles++;
            
            // Подсчитываем по разделам
            var sectionType = getSectionType(subtitle);
            switch(sectionType) {
                case "main": processedMain++; break;
                case "notes": processedNotes++; break;
                case "comments": processedComments++; break;
            }
        }
    }

    // Завершаем операцию для отмены
    window.external.EndUndoUnit(document);

    // Вычисляем время выполнения
    var endTime = new Date();
    var timeDiff = (endTime - startTime) / 1000;
    var timeStr = timeDiff.toFixed(3).replace(".", ",");

    // ==================================================
    // ВТОРОЕ ОКНО (ИТОГОВАЯ СТАТИСТИКА)
    // ==================================================

    if (showStatistics >= 1 || (showStatistics == 0 && processedSubtitles > 0)) {
        var resultMessage = "";
        
        resultMessage = scriptName + "\n";
        resultMessage += "ver. " + version + "\n";
        resultMessage += "---------------------------\n\n";

        resultMessage += "Всего найдено подзаголовков: " + totalSubtitles + "\n";
        resultMessage += "Из них изменено:\n";
        resultMessage += "  • в основном разделе: " + processedMain + "\n";
        if (processedNotes > 0) resultMessage += "  • в сносках-примечаниях: " + processedNotes + "\n";
        if (processedComments > 0) resultMessage += "  • в комментариях: " + processedComments + "\n\n";

        resultMessage += "Выполненные замены:\n";
        if (dotsRemoved > 0) resultMessage += "  • удалено одиночных точек: " + dotsRemoved + "\n";
        if (doubleDotsRemoved > 0) resultMessage += "  • удалено двойных точек: " + doubleDotsRemoved + "\n";
        if (ellipsisReplaced > 0) resultMessage += "  • заменено на многоточие (3 → …): " + ellipsisReplaced + "\n";
        
        if (dotsRemoved == 0 && doubleDotsRemoved == 0 && ellipsisReplaced == 0) {
            resultMessage += "  • замен не требуется (точки в конце отсутствуют)\n";
        }
        
        resultMessage += "\n";
        resultMessage += "Время обработки: " + timeStr + " сек";

        MsgBox(resultMessage, scriptName + " ver. " + version);
    }
}

// ==================================================
// КОНЕЦ СКРИПТА
// ==================================================
