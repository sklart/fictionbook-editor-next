// Скрипт "Статистика звёздочек" для редактора FBE
// version 3.9
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для отображения максимально подробной статистики
// по звёздочкам * в fb2 документе.
// Ниже в начале настроек можно переключаться с полного отчета на адаптивный
// (отображение только строк с наличием 1 и более * в конкретных элементах)

//  Скрипт помогает быстрее разобраться с маркерами сносок в виде звёздочек.
//  В идеале кол-во звёздочек в пунктах
//  Абзацев с * в начале:
//  должно совпадать с общей суммой звёздочек в
//  Абзацев с * внутри:
//  Абзацев с * в конце:

// Никаких изменений в документе скрипт не производит.

// version 3.9, 18.12.2025
//======================================

// ==================== НАСТРОЙКИ ====================
// Спрашивать о режиме отображения статистики:
// 1 - спрашивать каждый раз (Да - полная, Нет - адаптивная)
// 0 - использовать значение SHOW_FULL_STATISTICS ниже
var ASK_STAT_MODE = 0; // <- ИЗМЕНИТЕ ЗДЕСЬ 1 или 0

// Режим отображения статистики (если ASK_STAT_MODE = 0):
// 1 - всегда полная статистика (все разделы, даже если 0)
// 0 - адаптивная статистика (только найденное)
var SHOW_FULL_STATISTICS = 0; // <- ИЗМЕНИТЕ ЗДЕСЬ 1 или 0
// ===================================================

function Run() {
    var scriptName = "Статистика звёздочек";
    var scriptVersion = "3.9";
    
    // Определяем режим отображения
    var showFullStats;
    
    if (ASK_STAT_MODE) {
        // Спрашиваем пользователя
        var choice = AskYesNo("Режим отображения статистики:\n\nДа - полная статистика (все разделы)\nНет - адаптивная (только найденное)");
        showFullStats = choice; // Да = true (Полная статистика), Нет = false (Адаптивная статистика)
    } else {
        // Используем настройку
        showFullStats = SHOW_FULL_STATISTICS;
    }
    
    // Получаем символ неразрывного пробела из настроек FBE
    var nbspChar;
    try {
        nbspChar = window.external.GetNBSP();
    } catch(e) {
        nbspChar = String.fromCharCode(160); // стандартный &nbsp;
    }
    
    // Регулярное выражение для поиска 3 звёздочек в КОНЦЕ текста
    var reTarget = new RegExp("(\\*\\s*\\*\\s*\\*|\\*\\s*\\*\\s*" + nbspChar + "\\s*\\*|\\*\\s*" + nbspChar + "\\s*\\*\\s*\\*|" + nbspChar + "\\s*\\*\\s*\\*\\s*\\*)(\\s|" + nbspChar + ")*$", "i");
    
    // ==================== ОБЩИЕ ФУНКЦИИ ====================
    
    // Проверяем, находится ли элемент в разделе сносок
    function isInNotesSection(element) {
        var parent = element;
        while (parent) {
            if (parent.nodeName && parent.nodeName.toUpperCase() == "DIV") {
                var className = parent.className || "";
                if (className.indexOf("body") != -1) {
                    var fbname = parent.getAttribute("fbname");
                    if (fbname && fbname == "notes") {
                        return true;
                    }
                }
            }
            parent = parent.parentNode;
        }
        return false;
    }
    
    // Получаем текст из узла
    function getTextFromNode(node) {
        if (!node) return "";
        
        if (node.nodeType == 3) { // TEXT_NODE
            return node.nodeValue || "";
        }
        
        if (node.nodeType != 1) { // не ELEMENT_NODE
            return "";
        }
        
        var text = "";
        var children = node.childNodes;
        for (var i = 0; i < children.length; i++) {
            text += getTextFromNode(children[i]);
        }
        return text;
    }
    
    // Проверяем, содержит ли текст буквы или цифры
    function hasLettersOrDigits(text) {
        for (var i = 0; i < text.length; i++) {
            var c = text.charAt(i);
            // Русские и английские буквы
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
                (c >= 'а' && c <= 'я') || (c >= 'А' && c <= 'Я') || c == 'Ё' || c == 'ё') {
                return true;
            }
            // Цифры
            if (c >= '0' && c <= '9') {
                return true;
            }
        }
        return false;
    }
    
    // Считаем звёздочки в тексте
    function countStars(text) {
        var count = 0;
        for (var i = 0; i < text.length; i++) {
            if (text.charAt(i) == '*') count++;
        }
        return count;
    }
    
    // Проверяем, является ли символ пробельным
    function isWhitespaceChar(c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == nbspChar;
    }
    
    // ==================== ФУНКЦИИ АНАЛИЗА ====================
    
    function analyzeDocument() {
        var startTime = new Date();
        
        // Общая статистика
        var totalStarsInDocument = 0;
        
        // Статистика чистых звёздочек
        var totalCleanStars = 0;
        var mainCleanTitles = 0;
        var mainCleanTitlesStars = 0;
        var mainCleanSubtitles = 0;
        var mainCleanSubtitlesStars = 0;
        var notesCleanTitles = 0;
        var notesCleanTitlesStars = 0;
        var notesCleanSubtitles = 0;
        var notesCleanSubtitlesStars = 0;
        
        // Статистика подозрительных звёздочек
        var totalSuspiciousStars = 0;
        var totalSuspiciousEntries = 0;
        
        // Основной раздел
        var mainTitlesWithStars = 0;
        var mainTitlesStarsCount = 0;
        var mainSubtitlesWithStars = 0;
        var mainSubtitlesStarsCount = 0;
        var mainStartParagraphs = 0;
        var mainStartStarsCount = 0;
        var mainInsideParagraphs = 0;
        var mainInsideStarsCount = 0;
        var mainEndParagraphs = 0;
        var mainEndStarsCount = 0;
        
        // Раздел сносок
        var notesTitlesWithStars = 0;
        var notesTitlesStarsCount = 0;
        var notesSubtitlesWithStars = 0;
        var notesSubtitlesStarsCount = 0;
        var notesStartParagraphs = 0;
        var notesStartStarsCount = 0;
        var notesInsideParagraphs = 0;
        var notesInsideStarsCount = 0;
        var notesEndParagraphs = 0;
        var notesEndStarsCount = 0;
        
        // Абзацы с *** в конце
        var mainTargetCount = 0;
        
        // Определяем расположение звёздочек в тексте
        function analyzeStarPositions(text) {
            var result = {
                hasStars: false,
                starsAtStart: 0,
                starsInside: 0,
                starsAtEnd: 0,
                starsCount: 0
            };
            
            if (!text) return result;
            
            // Считаем все звёздочки
            var starsCount = 0;
            for (var i = 0; i < text.length; i++) {
                if (text.charAt(i) == '*') starsCount++;
            }
            result.starsCount = starsCount;
            result.hasStars = starsCount > 0;
            
            if (starsCount == 0) return result;
            
            // Находим позиции всех звёздочек
            var starPositions = [];
            for (var i = 0; i < text.length; i++) {
                if (text.charAt(i) == '*') {
                    starPositions.push(i);
                }
            }
            
            // Для каждой звёздочки определяем положение
            for (var i = 0; i < starPositions.length; i++) {
                var pos = starPositions[i];
                var isStart = true;
                var isEnd = true;
                
                // Проверяем, что перед звёздочкой только пробелы
                for (var j = 0; j < pos; j++) {
                    if (!isWhitespaceChar(text.charAt(j))) {
                        isStart = false;
                        break;
                    }
                }
                
                // Проверяем, что после звёздочки только пробелы
                for (var j = pos + 1; j < text.length; j++) {
                    if (!isWhitespaceChar(text.charAt(j))) {
                        isEnd = false;
                        break;
                    }
                }
                
                if (isStart) {
                    result.starsAtStart++;
                } else if (isEnd) {
                    result.starsAtEnd++;
                } else {
                    result.starsInside++;
                }
            }
            
            return result;
        }
        
        // Обработка одного абзаца для анализа
        function analyzeParagraph(p, inNotes) {
            var stats = {
                // Чистые звёздочки
                cleanTitles: 0, cleanTitlesStars: 0,
                cleanSubtitles: 0, cleanSubtitlesStars: 0,
                
                // Подозрительные звёздочки
                titlesWithStars: 0, titlesStarsCount: 0,
                subtitlesWithStars: 0, subtitlesStarsCount: 0,
                startParagraphs: 0, startStarsCount: 0,
                insideParagraphs: 0, insideStarsCount: 0,
                endParagraphs: 0, endStarsCount: 0,
                
                targetCount: 0,
                totalStars: 0
            };
            
            var className = p.className || "";
            var isSubtitle = (className.indexOf("subtitle") != -1);
            
            // Проверяем, находится ли абзац внутри <div class="title">
            var insideTitle = false;
            var parent = p.parentNode;
            while (parent) {
                if (parent.nodeName && parent.nodeName.toUpperCase() == "DIV") {
                    var parentClass = parent.className || "";
                    if (parentClass.indexOf("title") != -1) {
                        insideTitle = true;
                        break;
                    }
                }
                parent = parent.parentNode;
            }
            
            // Получаем весь текст абзаца
            var fullText = getTextFromNode(p);
            
            // Считаем все звёздочки
            var starsInText = countStars(fullText);
            stats.totalStars = starsInText;
            
            if (starsInText == 0) {
                return stats; // Нет звёздочек
            }
            
            // Проверяем, есть ли в тексте буквы/цифры
            var hasText = hasLettersOrDigits(fullText);
            
            // Анализируем расположение звёздочек
            var starAnalysis = analyzeStarPositions(fullText);
            
            // Проверяем целевой паттерн (*** в конце)
            var isTarget = reTarget.test(fullText);
            
            if (isSubtitle) {
                // Подзаголовок
                if (!hasText) {
                    // "Чистые" звёздочки в подзаголовке
                    stats.cleanSubtitles = 1;
                    stats.cleanSubtitlesStars = starsInText;
                } else {
                    // Подозрительные звёздочки в подзаголовке
                    stats.subtitlesWithStars = 1;
                    stats.subtitlesStarsCount = starsInText;
                }
            } 
            else if (insideTitle) {
                // Заголовок
                if (!hasText) {
                    // "Чистые" звёздочки в заголовке
                    stats.cleanTitles = 1;
                    stats.cleanTitlesStars = starsInText;
                } else {
                    // Подозрительные звёздочки в заголовке
                    stats.titlesWithStars = 1;
                    stats.titlesStarsCount = starsInText;
                }
            }
            else {
                // Обычный абзац
                if (hasText) {
                    // Есть текст
                    if (isTarget && starsInText >= 3) {
                        // Абзац с *** в конце
                        stats.targetCount = 1;
                        
                        if (starsInText > 3) {
                            // Лишние звёздочки (кроме 3 целевых)
                            var extraStars = starsInText - 3;
                            var remainingStars = extraStars;
                            
                            // Звёздочки в начале
                            if (starAnalysis.starsAtStart > 0) {
                                stats.startParagraphs = 1;
                                stats.startStarsCount = starAnalysis.starsAtStart;
                                remainingStars -= starAnalysis.starsAtStart;
                            }
                            
                            // Звёздочки внутри
                            if (remainingStars > 0) {
                                stats.insideParagraphs = 1;
                                stats.insideStarsCount = remainingStars;
                            }
                        }
                    } else {
                        // Не целевой абзац - все звёздочки подозрительные
                        var hasStart = starAnalysis.starsAtStart > 0;
                        var hasInside = starAnalysis.starsInside > 0;
                        var hasEnd = starAnalysis.starsAtEnd > 0;
                        
                        if (hasStart) {
                            stats.startParagraphs = 1;
                            stats.startStarsCount = starAnalysis.starsAtStart;
                        }
                        
                        if (hasInside) {
                            stats.insideParagraphs = 1;
                            stats.insideStarsCount = starAnalysis.starsInside;
                        }
                        
                        if (hasEnd) {
                            stats.endParagraphs = 1;
                            stats.endStarsCount = starAnalysis.starsAtEnd;
                        }
                    }
                } else {
                    // Нет текста (только звёздочки)
                    if (isTarget && starsInText >= 3) {
                        // Абзац с *** в конце
                        stats.targetCount = 1;
                    }
                }
            }
            
            return stats;
        }
        
        // Получаем все абзацы в документе
        var paragraphs = document.getElementsByTagName("P");
        
        // Проходим по всем абзацам
        for (var i = 0; i < paragraphs.length; i++) {
            var p = paragraphs[i];
            
            // Определяем, в каком разделе находится абзац
            var inNotes = isInNotesSection(p);
            
            // Анализируем абзац
            var stats = analyzeParagraph(p, inNotes);
            
            // Добавляем к общей статистике
            totalStarsInDocument += stats.totalStars;
            
            if (inNotes) {
                // Раздел сносок
                notesCleanTitles += stats.cleanTitles;
                notesCleanTitlesStars += stats.cleanTitlesStars;
                notesCleanSubtitles += stats.cleanSubtitles;
                notesCleanSubtitlesStars += stats.cleanSubtitlesStars;
                
                notesTitlesWithStars += stats.titlesWithStars;
                notesTitlesStarsCount += stats.titlesStarsCount;
                notesSubtitlesWithStars += stats.subtitlesWithStars;
                notesSubtitlesStarsCount += stats.subtitlesStarsCount;
                notesStartParagraphs += stats.startParagraphs;
                notesStartStarsCount += stats.startStarsCount;
                notesInsideParagraphs += stats.insideParagraphs;
                notesInsideStarsCount += stats.insideStarsCount;
                notesEndParagraphs += stats.endParagraphs;
                notesEndStarsCount += stats.endStarsCount;
            } else {
                // Основной раздел
                mainCleanTitles += stats.cleanTitles;
                mainCleanTitlesStars += stats.cleanTitlesStars;
                mainCleanSubtitles += stats.cleanSubtitles;
                mainCleanSubtitlesStars += stats.cleanSubtitlesStars;
                
                mainTitlesWithStars += stats.titlesWithStars;
                mainTitlesStarsCount += stats.titlesStarsCount;
                mainSubtitlesWithStars += stats.subtitlesWithStars;
                mainSubtitlesStarsCount += stats.subtitlesStarsCount;
                mainStartParagraphs += stats.startParagraphs;
                mainStartStarsCount += stats.startStarsCount;
                mainInsideParagraphs += stats.insideParagraphs;
                mainInsideStarsCount += stats.insideStarsCount;
                mainEndParagraphs += stats.endParagraphs;
                mainEndStarsCount += stats.endStarsCount;
                
                mainTargetCount += stats.targetCount;
            }
        }
        
        // Считаем итоги
        totalCleanStars = mainCleanTitlesStars + mainCleanSubtitlesStars + 
                         notesCleanTitlesStars + notesCleanSubtitlesStars;
        
        var mainSuspiciousTotal = mainTitlesStarsCount + mainSubtitlesStarsCount + 
                                 mainStartStarsCount + mainInsideStarsCount + mainEndStarsCount;
        var notesSuspiciousTotal = notesTitlesStarsCount + notesSubtitlesStarsCount + 
                                  notesStartStarsCount + notesInsideStarsCount + notesEndStarsCount;
        totalSuspiciousStars = mainSuspiciousTotal + notesSuspiciousTotal;
        
        // Считаем вхождения (элементы со звёздочками)
        var mainSuspiciousEntries = mainTitlesWithStars + mainSubtitlesWithStars + 
                                   mainStartParagraphs + mainInsideParagraphs + mainEndParagraphs;
        var notesSuspiciousEntries = notesTitlesWithStars + notesSubtitlesWithStars + 
                                    notesStartParagraphs + notesInsideParagraphs + notesEndParagraphs;
        totalSuspiciousEntries = mainSuspiciousEntries + notesSuspiciousEntries;
        
        // Время окончания
        var endTime = new Date();
        var executionTime = (endTime - startTime) / 1000;
        
        // Формируем сообщение
        var message = scriptName + "\n";
        message += "ver. " + scriptVersion + "\n\n";
        
        // Режим отображения
        message += "Режим: " + (showFullStats ? "полная статистика" : "адаптивная статистика") + "\n\n";
        
        // Проверяем разные случаи
        if (totalStarsInDocument == 0) {
            message += "В документе звёздочек не найдено!\n\n";
            message += "Время выполнения: " + executionTime.toFixed(2) + " сек.";
            
            return {
                message: message,
                mainTargetCount: mainTargetCount,
                hasStars: false,
                hasSuspicious: false
            };
        }
        
        message += "ВСЕГО звёздочек в документе: " + totalStarsInDocument + " шт.\n\n";
        
        // Раздел "Чистые звёздочки"
        if (showFullStats || totalCleanStars > 0) {
            message += "• \"ЧИСТЫЕ\" ЗВЁЗДОЧКИ (без букв/цифр), всего: " + totalCleanStars + "\n";
            
            var mainCleanTotal = mainCleanTitlesStars + mainCleanSubtitlesStars;
            if (showFullStats || mainCleanTotal > 0) {
                message += "Основной раздел: " + mainCleanTotal + "\n";
                if (showFullStats || mainCleanTitles > 0) {
                    message += "-   Заголовки: " + mainCleanTitles + " (" + mainCleanTitlesStars + " зв.)\n";
                }
                if (showFullStats || mainCleanSubtitles > 0) {
                    message += "-   Подзаголовки: " + mainCleanSubtitles + " (" + mainCleanSubtitlesStars + " зв.)\n";
                }
            }
            
            var notesCleanTotal = notesCleanTitlesStars + notesCleanSubtitlesStars;
            if (showFullStats || notesCleanTotal > 0) {
                message += "Раздел сносок: " + notesCleanTotal + "\n";
                if (showFullStats || notesCleanTitles > 0) {
                    message += "-   Заголовки в сносках: " + notesCleanTitles + " (" + notesCleanTitlesStars + " зв.)\n";
                }
                if (showFullStats || notesCleanSubtitles > 0) {
                    message += "-   Подзаголовки в сносках: " + notesCleanSubtitles + " (" + notesCleanSubtitlesStars + " зв.)\n";
                }
            }
            message += "---------------------------\n";
        }
        
        // Раздел "Подозрительные звёздочки"
        if (showFullStats || totalSuspiciousStars > 0) {
            if (totalSuspiciousStars > 0 || showFullStats) {
                message += "• ПОДОЗРИТЕЛЬНЫЕ ЗВЁЗДОЧКИ\n";
                message += "(возможные маркеры сносок):\n\n";
                
                if (mainSuspiciousTotal > 0 || showFullStats) {
                    var mainEntriesStr = "";
                    if (mainSuspiciousEntries > 0) {
                        mainEntriesStr = " (" + mainSuspiciousEntries + " вхождений)";
                    }
                    message += "Основной раздел, всего *: " + mainSuspiciousTotal + " шт" + mainEntriesStr + ".\n";
                    
                    if (showFullStats || mainTitlesWithStars > 0) {
                        message += "-   Заголовков с *: " + mainTitlesWithStars;
                        if (mainTitlesStarsCount > 0) {
                            message += " (" + mainTitlesStarsCount + " звёздочек)";
                        }
                        message += "\n";
                    }
                    
                    if (showFullStats || mainSubtitlesWithStars > 0) {
                        message += "-   Подзаголовков с *: " + mainSubtitlesWithStars;
                        if (mainSubtitlesStarsCount > 0) {
                            message += " (" + mainSubtitlesStarsCount + " звёздочек)";
                        }
                        message += "\n";
                    }
                    
                    if (showFullStats || mainStartParagraphs > 0 || mainInsideParagraphs > 0 || mainEndParagraphs > 0) {
                        message += "ОБЫЧНЫЕ АБЗАЦЫ:\n";
                        if (showFullStats || mainStartParagraphs > 0) {
                            message += "-   Абзацев с * в начале: " + mainStartParagraphs;
                            if (mainStartStarsCount > 0) {
                                message += " (" + mainStartStarsCount + " звёздочек)";
                            }
                            message += "\n";
                        }
                        if (showFullStats || mainInsideParagraphs > 0) {
                            message += "-   Абзацев с * внутри: " + mainInsideParagraphs;
                            if (mainInsideStarsCount > 0) {
                                message += " (" + mainInsideStarsCount + " звёздочек)";
                            }
                            message += "\n";
                        }
                        if (showFullStats || mainEndParagraphs > 0) {
                            message += "-   Абзацев с * в конце: " + mainEndParagraphs;
                            if (mainEndStarsCount > 0) {
                                message += " (" + mainEndStarsCount + " звёздочек)";
                            }
                            message += "\n";
                        }
                    }
                    message += "---------------------------\n";
                }
                
                if (notesSuspiciousTotal > 0 || showFullStats) {
                    var notesEntriesStr = "";
                    if (notesSuspiciousEntries > 0) {
                        notesEntriesStr = " (" + notesSuspiciousEntries + " вхождений)";
                    }
                    message += "Раздел сносок, всего: " + notesSuspiciousTotal + " шт" + notesEntriesStr + ".\n";
                    
                    if (showFullStats || notesTitlesWithStars > 0) {
                        message += "-   Заголовков сносок с *: " + notesTitlesWithStars;
                        if (notesTitlesStarsCount > 0) {
                            message += " (" + notesTitlesStarsCount + " звёздочек)";
                        }
                        message += "\n";
                    }
                    
                    if (showFullStats || notesSubtitlesWithStars > 0) {
                        message += "-   Подзаголовков сносок с *: " + notesSubtitlesWithStars;
                        if (notesSubtitlesStarsCount > 0) {
                            message += " (" + notesSubtitlesStarsCount + " звёздочек)";
                        }
                        message += "\n";
                    }
                    
                    if (showFullStats || notesStartParagraphs > 0 || notesInsideParagraphs > 0 || notesEndParagraphs > 0) {
                        message += "ОБЫЧНЫЕ АБЗАЦЫ В СНОСКАХ:\n";
                        if (showFullStats || notesStartParagraphs > 0) {
                            message += "-   Абзацев сносок с * в начале: " + notesStartParagraphs;
                            if (notesStartStarsCount > 0) {
                                message += " (" + notesStartStarsCount + " звёздочек)";
                            }
                            message += "\n";
                        }
                        if (showFullStats || notesInsideParagraphs > 0) {
                            message += "-   Абзацев сносок с * внутри: " + notesInsideParagraphs;
                            if (notesInsideStarsCount > 0) {
                                message += " (" + notesInsideStarsCount + " звёздочек)";
                            }
                            message += "\n";
                        }
                        if (showFullStats || notesEndParagraphs > 0) {
                            message += "-   Абзацев сносок с * в конце: " + notesEndParagraphs;
                            if (notesEndStarsCount > 0) {
                                message += " (" + notesEndStarsCount + " звёздочек)";
                            }
                            message += "\n";
                        }
                    }
                    message += "---------------------------\n";
                }
                
                // Предупреждение (только если есть подозрительные звёздочки)
                if (totalSuspiciousStars > 0) {
                    message += "• ВНИМАНИЕ: В тексте обнаружены звёздочки,\n";
                    message += "которые могут быть маркерами сносок.\n";
                    message += "- Особое внимание на:\n";
                    
                    var mainWarnings = [];
                    var notesWarnings = [];
                    
                    if (mainStartStarsCount > 0) mainWarnings.push("в начале");
                    if (mainInsideStarsCount > 0) mainWarnings.push("внутри");
                    if (mainEndStarsCount > 0) mainWarnings.push("в конце");
                    
                    if (notesStartStarsCount > 0) notesWarnings.push("в начале");
                    if (notesInsideStarsCount > 0) notesWarnings.push("внутри");
                    if (notesEndStarsCount > 0) notesWarnings.push("в конце");
                    
                    if (mainWarnings.length > 0) {
                        message += "• Основной текст: Звёздочки ";
                        message += mainWarnings.join(", ");
                        message += " абзацев!\n";
                    }
                    
                    if (notesWarnings.length > 0) {
                        message += "• Сноски: Звёздочки ";
                        message += notesWarnings.join(", ");
                        message += " абзацев!\n";
                    }
                    
                    message += "Рекомендуется проверить их вручную!\n";
                    message += "---------------------------\n";
                }
            }
        }
        
        // Абзацы с *** в конце
        if (mainTargetCount > 0 || showFullStats) {
            message += "• Абзацев с *** в конце: " + mainTargetCount + "\n";
            if (mainTargetCount > 0) {
                message += "(*** рекомендуется преобразовать в подзаголовки)\n";
            }
            message += "---------------------------\n";
        }
        
        message += "Время выполнения: " + executionTime.toFixed(2) + " сек.";
        
        return {
            message: message,
            mainTargetCount: mainTargetCount,
            hasStars: totalStarsInDocument > 0,
            hasSuspicious: totalSuspiciousStars > 0
        };
    }
    
    // ==================== ОСНОВНОЙ КОД ====================
    
    try {
        // Выполняем анализ
        var startTime = new Date();
        var analysisResult = analyzeDocument();
        var endTime = new Date();
        var executionTime = (endTime - startTime) / 1000;
        
        // Показываем результаты
        MsgBox(analysisResult.message, "FBE скрипт");
        
    } catch(e) {
        MsgBox("Ошибка при выполнении скрипта:\n" + e.toString(), "FBE скрипт: ошибка");
    }
    
    return "Done";
}
