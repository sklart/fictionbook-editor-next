// Скрипт "Звёздочки на концах абзацев - в абзацы или подзаголовки" для редактора FBE
// version 3.9
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для поиска в основном разделе (без боди сносок)
// потенциальных подзаголовков из 3 звёздочек в конце обычных абзацев и выделения их
// в отдельные простые абзацы или сразу в стандартные подзаголовки <subtitle>* * *</subtitle>

// Также скрипт отображает максимально подробную статистику по звёздочкам * в документе.
// Ниже в начале настроек можно переключаться с полного отчета на адаптивный
// (отображение только строк с наличием 1 и более * в конкретных элементах)

// version 3.9, 18.12.2025
//======================================

// ==================== НАСТРОЙКИ ====================
// Отображать полную статистику или только при наличии найденных элементов:
// 1 - всегда показывать ВСЕ разделы статистики (даже если 0)
// 0 - показывать только разделы с найденными элементами (адаптивно)
var SHOW_FULL_STATISTICS = 1; // <- ИЗМЕНИТЕ ЗДЕСЬ 1 или 0
// ===================================================

function Run() {
    var scriptName = "Звёздочки на концах абзацев - анализ";
    var scriptVersion = "3.9";
    
    // Показываем текущие настройки
    var settingsInfo = "Настройки скрипта:\n";
    settingsInfo += "Полная статистика: " + (SHOW_FULL_STATISTICS ? "ВКЛ" : "ВЫКЛ") + "\n";
    settingsInfo += "(1 - все разделы, 0 - только найденное)";
    
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
        
        // Для преобразования
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
                        // Целевой паттерн (конец абзаца с ***)
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
                        // Целевой паттерн
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
        
        // Проверяем разные случаи
        if (totalStarsInDocument == 0) {
            message += "В документе звёздочек не найдено!\n\n";
            message += "Время выполнения анализа: " + executionTime.toFixed(2) + " сек.";
            
            return {
                message: message,
                mainTargetCount: 0,
                hasStars: false,
                hasSuspicious: false
            };
        }
        
        message += "ВСЕГО звёздочек в документе: " + totalStarsInDocument + " шт.\n\n";
        
        // Раздел "Чистые звёздочки"
        if (SHOW_FULL_STATISTICS || totalCleanStars > 0) {
            message += "• \"ЧИСТЫЕ\" ЗВЁЗДОЧКИ (без букв/цифр), всего: " + totalCleanStars + "\n";
            
            var mainCleanTotal = mainCleanTitlesStars + mainCleanSubtitlesStars;
            if (SHOW_FULL_STATISTICS || mainCleanTotal > 0) {
                message += "Основной раздел: " + mainCleanTotal + "\n";
                if (SHOW_FULL_STATISTICS || mainCleanTitles > 0) {
                    message += "-   Заголовки: " + mainCleanTitles + " (" + mainCleanTitlesStars + " зв.)\n";
                }
                if (SHOW_FULL_STATISTICS || mainCleanSubtitles > 0) {
                    message += "-   Подзаголовки: " + mainCleanSubtitles + " (" + mainCleanSubtitlesStars + " зв.)\n";
                }
            }
            
            var notesCleanTotal = notesCleanTitlesStars + notesCleanSubtitlesStars;
            if (SHOW_FULL_STATISTICS || notesCleanTotal > 0) {
                message += "Раздел сносок: " + notesCleanTotal + "\n";
                if (SHOW_FULL_STATISTICS || notesCleanTitles > 0) {
                    message += "-   Заголовки в сносках: " + notesCleanTitles + " (" + notesCleanTitlesStars + " зв.)\n";
                }
                if (SHOW_FULL_STATISTICS || notesCleanSubtitles > 0) {
                    message += "-   Подзаголовки в сносках: " + notesCleanSubtitles + " (" + notesCleanSubtitlesStars + " зв.)\n";
                }
            }
            message += "---------------------------\n";
        }
        
        // Раздел "Подозрительные звёздочки"
        if (SHOW_FULL_STATISTICS || totalSuspiciousStars > 0) {
            if (totalSuspiciousStars > 0 || SHOW_FULL_STATISTICS) {
                message += "• ПОДОЗРИТЕЛЬНЫЕ ЗВЁЗДОЧКИ\n";
                message += "(возможные маркеры сносок):\n\n";
                
                if (mainSuspiciousTotal > 0 || SHOW_FULL_STATISTICS) {
                    var mainEntriesStr = "";
                    if (mainSuspiciousEntries > 0) {
                        mainEntriesStr = " (" + mainSuspiciousEntries + " вхождений)";
                    }
                    message += "Основной раздел, всего *: " + mainSuspiciousTotal + " шт" + mainEntriesStr + ".\n";
                    
                    if (SHOW_FULL_STATISTICS || mainTitlesWithStars > 0) {
                        message += "-   Заголовков с *: " + mainTitlesWithStars;
                        if (mainTitlesStarsCount > 0) {
                            message += " (" + mainTitlesStarsCount + " звёздочек)";
                        }
                        message += "\n";
                    }
                    
                    if (SHOW_FULL_STATISTICS || mainSubtitlesWithStars > 0) {
                        message += "-   Подзаголовков с *: " + mainSubtitlesWithStars;
                        if (mainSubtitlesStarsCount > 0) {
                            message += " (" + mainSubtitlesStarsCount + " звёздочек)";
                        }
                        message += "\n";
                    }
                    
                    if (SHOW_FULL_STATISTICS || mainStartParagraphs > 0 || mainInsideParagraphs > 0 || mainEndParagraphs > 0) {
                        message += "ОБЫЧНЫЕ АБЗАЦЫ:\n";
                        if (SHOW_FULL_STATISTICS || mainStartParagraphs > 0) {
                            message += "-   Абзацев с * в начале: " + mainStartParagraphs;
                            if (mainStartStarsCount > 0) {
                                message += " (" + mainStartStarsCount + " звёздочек)";
                            }
                            message += "\n";
                        }
                        if (SHOW_FULL_STATISTICS || mainInsideParagraphs > 0) {
                            message += "-   Абзацев с * внутри: " + mainInsideParagraphs;
                            if (mainInsideStarsCount > 0) {
                                message += " (" + mainInsideStarsCount + " звёздочек)";
                            }
                            message += "\n";
                        }
                        if (SHOW_FULL_STATISTICS || mainEndParagraphs > 0) {
                            message += "-   Абзацев с * в конце: " + mainEndParagraphs;
                            if (mainEndStarsCount > 0) {
                                message += " (" + mainEndStarsCount + " звёздочек)";
                            }
                            message += "\n";
                        }
                    }
                    message += "---------------------------\n";
                }
                
                if (notesSuspiciousTotal > 0 || SHOW_FULL_STATISTICS) {
                    var notesEntriesStr = "";
                    if (notesSuspiciousEntries > 0) {
                        notesEntriesStr = " (" + notesSuspiciousEntries + " вхождений)";
                    }
                    message += "Раздел сносок, всего: " + notesSuspiciousTotal + " шт" + notesEntriesStr + ".\n";
                    
                    if (SHOW_FULL_STATISTICS || notesTitlesWithStars > 0) {
                        message += "-   Заголовков сносок с *: " + notesTitlesWithStars;
                        if (notesTitlesStarsCount > 0) {
                            message += " (" + notesTitlesStarsCount + " звёздочек)";
                        }
                        message += "\n";
                    }
                    
                    if (SHOW_FULL_STATISTICS || notesSubtitlesWithStars > 0) {
                        message += "-   Подзаголовков сносок с *: " + notesSubtitlesWithStars;
                        if (notesSubtitlesStarsCount > 0) {
                            message += " (" + notesSubtitlesStarsCount + " звёздочек)";
                        }
                        message += "\n";
                    }
                    
                    if (SHOW_FULL_STATISTICS || notesStartParagraphs > 0 || notesInsideParagraphs > 0 || notesEndParagraphs > 0) {
                        message += "ОБЫЧНЫЕ АБЗАЦЫ В СНОСКАХ:\n";
                        if (SHOW_FULL_STATISTICS || notesStartParagraphs > 0) {
                            message += "-   Абзацев сносок с * в начале: " + notesStartParagraphs;
                            if (notesStartStarsCount > 0) {
                                message += " (" + notesStartStarsCount + " звёздочек)";
                            }
                            message += "\n";
                        }
                        if (SHOW_FULL_STATISTICS || notesInsideParagraphs > 0) {
                            message += "-   Абзацев сносок с * внутри: " + notesInsideParagraphs;
                            if (notesInsideStarsCount > 0) {
                                message += " (" + notesInsideStarsCount + " звёздочек)";
                            }
                            message += "\n";
                        }
                        if (SHOW_FULL_STATISTICS || notesEndParagraphs > 0) {
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
                    
                    if (mainStartStarsCount > 0 || mainEndStarsCount > 0 || mainInsideStarsCount > 0) {
                        mainWarnings.push("Основной текст: Звёздочки");
                        if (mainStartStarsCount > 0) mainWarnings.push("в начале");
                        if (mainInsideStarsCount > 0) mainWarnings.push("внутри");
                        if (mainEndStarsCount > 0) mainWarnings.push("в конце");
                        mainWarnings.push("абзацев!");
                    }
                    
                    if (notesStartStarsCount > 0 || notesEndStarsCount > 0 || notesInsideStarsCount > 0) {
                        notesWarnings.push("Сноски: Звёздочки");
                        if (notesStartStarsCount > 0) notesWarnings.push("в начале");
                        if (notesInsideStarsCount > 0) notesWarnings.push("внутри");
                        if (notesEndStarsCount > 0) notesWarnings.push("в конце");
                        notesWarnings.push("абзацев!");
                    }
                    
                    if (mainWarnings.length > 0) {
                        message += "• " + mainWarnings.join(" ") + "\n";
                    }
                    if (notesWarnings.length > 0) {
                        message += "• " + notesWarnings.join(" ") + "\n";
                    }
                    
                    message += "Рекомендуется проверить их вручную!\n";
                    message += "---------------------------\n";
                }
            }
        }
        
        // Раздел для преобразования
        if (mainTargetCount > 0 || SHOW_FULL_STATISTICS) {
            message += "• НАЙДЕНО ДЛЯ ПРЕОБРАЗОВАНИЯ (только основной раздел, без сносок):\n";
            if (mainTargetCount > 0) {
                message += "-   Абзацев с *** в конце: " + mainTargetCount + "\n";
            } else if (SHOW_FULL_STATISTICS) {
                message += "-   Абзацев с *** в конце: 0\n";
            }
            message += "---------------------------\n";
        }
        
        message += "Время выполнения анализа: " + executionTime.toFixed(2) + " сек.";
        
        return {
            message: message,
            mainTargetCount: mainTargetCount,
            hasStars: totalStarsInDocument > 0,
            hasSuspicious: totalSuspiciousStars > 0
        };
    }
    
    // ==================== ФУНКЦИИ ПРЕОБРАЗОВАНИЯ ====================
    
    function transformDocument() {
        var startTime = new Date();
        
        // Находим целевые абзацы для преобразования
        function findTargetParagraphs() {
            var targets = [];
            var paragraphs = document.getElementsByTagName("P");
            
            for (var i = 0; i < paragraphs.length; i++) {
                var p = paragraphs[i];
                
                // Пропускаем сноски
                if (isInNotesSection(p)) {
                    continue;
                }
                
                // Пропускаем подзаголовки
                var className = p.className || "";
                if (className.indexOf("subtitle") != -1) {
                    continue;
                }
                
                // Проверяем, находится ли в заголовке
                var inTitle = false;
                var parent = p.parentNode;
                while (parent) {
                    if (parent.nodeName && parent.nodeName.toUpperCase() == "DIV") {
                        var parentClass = parent.className || "";
                        if (parentClass.indexOf("title") != -1) {
                            inTitle = true;
                            break;
                        }
                    }
                    parent = parent.parentNode;
                }
                
                if (inTitle) {
                    continue;
                }
                
                // Получаем текст
                var text = getTextFromNode(p);
                
                // Проверяем целевой паттерн (*** в конце)
                if (reTarget.test(text)) {
                    targets.push(p);
                }
            }
            
            return targets;
        }
        
        // Удаляем звёздочки из конца абзаца
        function removeStarsFromEnd(paragraph) {
            var text = getTextFromNode(paragraph);
            if (!text) return false;
            
            // Находим последний текстовый узел с звёздочками
            var lastNodeWithStars = findLastNodeWithStars(paragraph);
            if (!lastNodeWithStars) {
                return false;
            }
            
            // Обрабатываем в зависимости от типа узла
            if (lastNodeWithStars.nodeType == 3) { // TEXT_NODE
                return processTextNode(lastNodeWithStars);
            } else if (lastNodeWithStars.nodeType == 1) { // ELEMENT_NODE
                return processElementNode(lastNodeWithStars);
            }
            
            return false;
        }
        
        // Находим последний узел, содержащий звёздочки
        function findLastNodeWithStars(element) {
            // Ищем с конца
            for (var i = element.childNodes.length - 1; i >= 0; i--) {
                var child = element.childNodes[i];
                
                if (child.nodeType == 3) { // TEXT_NODE
                    var text = child.nodeValue || "";
                    if (text.indexOf('*') != -1) {
                        return child;
                    }
                } else if (child.nodeType == 1) { // ELEMENT_NODE
                    // Проверяем, есть ли звёздочки в этом элементе
                    var found = findLastNodeWithStars(child);
                    if (found) {
                        return found;
                    }
                }
            }
            return null;
        }
        
        // Обрабатываем текстовый узел
        function processTextNode(textNode) {
            var text = textNode.nodeValue || "";
            if (!text) return false;
            
            var originalText = text;
            
            // Удаляем звёздочки и пробелы с конца
            var newText = removeTrailingStarsAndSpaces(text);
            
            if (newText !== text) {
                textNode.nodeValue = newText;
                return true;
            }
            
            return false;
        }
        
        // Обрабатываем элемент (например, <strong>***</strong>)
        function processElementNode(element) {
            var text = getTextFromNode(element);
            if (!text) return false;
            
            // Если весь элемент состоит только из звёздочек и пробелы
            var hasOnlyStars = true;
            for (var i = 0; i < text.length; i++) {
                var c = text.charAt(i);
                if (c != '*' && c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != nbspChar) {
                    hasOnlyStars = false;
                    break;
                }
            }
            
            if (hasOnlyStars) {
                // Удаляем весь элемент
                if (element.parentNode) {
                    element.parentNode.removeChild(element);
                    return true;
                }
            }
            
            return false;
        }
        
        // Удаляет звёздочки и пробелы с конца строки
        function removeTrailingStarsAndSpaces(text) {
            if (!text) return text;
            
            var result = text;
            var changed = true;
            
            while (changed && result.length > 0) {
                changed = false;
                var len = result.length;
                var lastChar = result.charAt(len - 1);
                
                // Удаляем пробелы
                if (lastChar == ' ' || lastChar == '\t' || lastChar == '\n' || lastChar == '\r' || lastChar == nbspChar) {
                    result = result.substring(0, len - 1);
                    changed = true;
                    continue;
                }
                
                // Удаляем звёздочки
                if (lastChar == '*') {
                    result = result.substring(0, len - 1);
                    changed = true;
                }
            }
            
            return result;
        }
        
        // Создаём новый абзац со звёздочками
        function createStarParagraph(isSubtitle) {
            var newP = document.createElement("P");
            if (isSubtitle) {
                newP.className = "subtitle";
            }
            newP.innerHTML = "* * *";
            return newP;
        }
        
        // Основная функция преобразования
        function transformParagraphs(targets, makeSubtitles) {
            var transformed = 0;
            var errors = 0;
            
            for (var i = 0; i < targets.length; i++) {
                var p = targets[i];
                
                try {
                    // Сохраняем ссылку на родителя и следующего соседа
                    var parent = p.parentNode;
                    if (!parent) {
                        errors++;
                        continue;
                    }
                    
                    var nextSibling = p.nextSibling;
                    
                    // Удаляем звёздочки из конца
                    if (removeStarsFromEnd(p)) {
                        // Создаём новый абзац
                        var newP = createStarParagraph(makeSubtitles);
                        
                        // Вставляем после исходного абзаца
                        if (nextSibling) {
                            parent.insertBefore(newP, nextSibling);
                        } else {
                            parent.appendChild(newP);
                        }
                        transformed++;
                    }
                } catch(e) {
                    errors++;
                }
            }
            
            return { transformed: transformed, errors: errors };
        }
        
        // Находим целевые абзацы
        var targetParagraphs = findTargetParagraphs();
        
        if (targetParagraphs.length == 0) {
            return {
                success: false,
                message: "Абзацев для преобразования не найдено!"
            };
        }
        
        // Показываем сколько найдено
        MsgBox("Найдено " + targetParagraphs.length + " абзацев с *** в конце.", "FBE скрипт");
        
        // Спрашиваем, что делать
        var makeSubtitles = AskYesNo("Преобразовать найденные *** в подзаголовки?\n\nДа - сделать подзаголовками (<p class=\"subtitle\">)\nНет - сделать обычными абзацами (<p>)");
        
        // Начинаем блок отмены
        window.external.BeginUndoUnit(document, scriptName + " v" + scriptVersion);
        
        // Выполняем преобразование
        var result = transformParagraphs(targetParagraphs, makeSubtitles);
        
        // Завершаем блок отмены
        window.external.EndUndoUnit(document);
        
        // Время выполнения
        var endTime = new Date();
        var executionTime = (endTime - startTime) / 1000;
        
        // Формируем отчёт
        var message = "Звёздочки на концах абзацев - преобразование\n";
        message += "ver. " + scriptVersion + "\n\n";
        message += "ОБРАБОТАНО:\n";
        message += "Найдено целевых абзацев: " + targetParagraphs.length + "\n";
        message += "Успешно преобразовано: " + result.transformed + "\n";
        
        if (makeSubtitles) {
            message += "Преобразовано в подзаголовки (<p class=\"subtitle\">)\n";
        } else {
            message += "Преобразовано в обычные абзацы (<p>)\n";
        }
        
        if (result.errors > 0) {
            message += "Ошибок при преобразовании: " + result.errors + "\n";
        }
        
        message += "\nВремя выполнения: " + executionTime.toFixed(2) + " сек.";
        
        return {
            success: true,
            message: message
        };
    }
    
    // ==================== ОСНОВНОЙ КОД ====================
    
    try {
        // Показываем настройки
        MsgBox(settingsInfo, "Настройки скрипта " + scriptVersion);
        
        // Спрашиваем пользователя, что делать
        var choice = AskYesNo("Звёздочки на концах абзацев\nВерсия: " + scriptVersion + "\n\nЧто выполнить?\n\nДа - только анализ (статистика)\nНет - анализ и преобразование");
        
        if (choice) {
            // Только анализ
            var analysisResult = analyzeDocument();
            MsgBox(analysisResult.message, "FBE скрипт");
        } else {
            // Анализ и преобразование
            var analysisResult = analyzeDocument();
            
            // Если нет звёздочек вообще
            if (!analysisResult.hasStars) {
                MsgBox(analysisResult.message, "FBE скрипт");
                return "Done";
            }
            
            // Показываем анализ
            if (analysisResult.mainTargetCount > 0) {
                var showTransform = AskYesNo(analysisResult.message + "\n\nВыполнить преобразование найденных " + analysisResult.mainTargetCount + " абзацев?");
                
                if (showTransform) {
                    var transformResult = transformDocument();
                    if (transformResult.success) {
                        MsgBox(transformResult.message, "FBE скрипт");
                    } else {
                        MsgBox(transformResult.message, "FBE скрипт");
                    }
                } else {
                    MsgBox("Преобразование отменено.", "FBE скрипт");
                }
            } else {
                // Нет абзацев для преобразования
                MsgBox(analysisResult.message, "FBE скрипт");
            }
        }
        
    } catch(e) {
        MsgBox("Ошибка при выполнении скрипта:\n" + e.toString(), "FBE скрипт: ошибка");
    }
    
    return "Done";
}
