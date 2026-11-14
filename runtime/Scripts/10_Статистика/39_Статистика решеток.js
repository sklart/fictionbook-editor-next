// Скрипт "Статистика решёток" для редактора FBE
// version 4.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// На основе скрипта "Статистика звёздочек", вер. 3.9.

// Скрипт предназначен для отображения максимально подробной статистики
// по символам решётки # в fb2 документе.
// Ниже в начале настроек можно переключаться с полного отчета на адаптивный
// (отображение только строк с наличием 1 и более # в конкретных элементах)

//  Скрипт помогает быстрее разобраться с маркерами сносок в виде решёток:
//  В идеале кол-во решёток в пунктах
//  Абзацев с # в начале:
//  должно совпадать с общей суммой решёток в
//  Абзацев с # внутри:
//  Абзацев с # в конце:

// Никаких изменений в документе скрипт не производит.

// version 4.1, 11.01.2026
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

// ============ НАСТРОЙКИ ДЛЯ ОПРЕДЕЛЕНИЯ ХЭШТЕГОВ ============
// Что считать хэштегами:

// 1. #слово (со строчной буквы) - считать хэштегом?
var HASHTAG_LOWERCASE = 1; // 1 - да, 0 - нет

// 2. #Слово (с заглавной буквы) - считать хэштегом?
var HASHTAG_CAPITAL_WORD = 0; // 1 - да, 0 - нет

// 3. Число+пробел+слово в начале абзаца - считать потенциальным хэштегом?
// Примеры: "1 слово", "367 текст", "44 Пример"
var HASHTAG_NUMBER_START = 0; // 1 - да, 0 - нет (по умолчанию НЕ считать)

// 4. Допускать знаки препинания внутри или в конце хэштега?
// Если 0 - хэштеги с точками, запятыми и т.д. будут игнорироваться
// Примеры: #слово, #слово! #слово? - будут отбракованы если установлено 0
var ALLOW_PUNCTUATION_IN_HASHTAG = 0; // 1 - да, 0 - нет (по умолчанию НЕ допускать)

// 5. Минимальная длина слова после # (без учета #)
// Применяется только если слово прошло проверку по пунктам 1,2 или 3
// И если соответствует настройке пунктуации (пункт 4)
var HASHTAG_MIN_WORD_LENGTH = 2; // минимальная длина слова

// 6. Максимальная длина слова после # (без учета #)
// Применяется только если слово прошло проверку по пунктам 1,2 или 3
// И если соответствует настройке пунктуации (пункт 4)
var HASHTAG_MAX_WORD_LENGTH = 50; // максимальная длина слова
// ===================================================

function Run() {
    var scriptName = "Статистика решёток";
    var scriptVersion = "4.1";
    
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
    
    // Регулярное выражение для поиска 3 решёток в КОНЦЕ текста
    var reTarget = new RegExp("(\\#\\s*\\#\\s*\\#|\\#\\s*\\#\\s*" + nbspChar + "\\s*\\#|\\#\\s*" + nbspChar + "\\s*\\#\\s*\\#|" + nbspChar + "\\s*\\#\\s*\\#\\s*\\#)(\\s|" + nbspChar + ")*$", "i");
    
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
    
    // Считаем решётки в тексте
    function countHashes(text) {
        var count = 0;
        for (var i = 0; i < text.length; i++) {
            if (text.charAt(i) == '#') count++;
        }
        return count;
    }
    
    // Проверяем, является ли символ пробельным
    function isWhitespaceChar(c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == nbspChar;
    }
    
    // Проверяем, является ли символ буквой (русской или английской)
    function isLetter(c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
               (c >= 'а' && c <= 'я') || (c >= 'А' && c <= 'Я') || c == 'Ё' || c == 'ё';
    }
    
    // Проверяем, является ли символ заглавной буквой
    function isCapitalLetter(c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'А' && c <= 'Я') || c == 'Ё';
    }
    
    // Проверяем, является ли символ строчной буквой
    function isLowercaseLetter(c) {
        return (c >= 'a' && c <= 'z') || (c >= 'а' && c <= 'я') || c == 'ё';
    }
    
    // Проверяем, является ли символ цифрой
    function isDigit(c) {
        return c >= '0' && c <= '9';
    }
    
    // Проверяем, является ли символ знаком препинания
    function isPunctuation(c) {
        return c == '.' || c == ',' || c == '!' || c == '?' || c == ':' || 
               c == ';' || c == '-' || c == '–' || c == '—' || c == '(' || 
               c == ')' || c == '[' || c == ']' || c == '{' || c == '}' || 
               c == '"' || c == "'" || c == '«' || c == '»' || c == '„' || 
               c == '“' || c == '‘' || c == '’' || c == '…' || c == '¿' || 
               c == '¡' || c == '‹' || c == '›' || c == '‚' || c == '‛';
    }
    
    // Проверяем, является ли символ допустимым в хэштеге (после #)
    function isValidHashtagChar(c, allowPunctuation) {
        if (allowPunctuation) {
            return isLetter(c) || isDigit(c) || c == '_' || c == '-';
        } else {
            // Не допускаем знаки препинания
            return isLetter(c) || isDigit(c) || c == '_';
        }
    }
    
    // Проверяем, содержит ли слово знаки препинания
    function hasPunctuationInWord(word) {
        for (var i = 0; i < word.length; i++) {
            if (isPunctuation(word.charAt(i))) {
                return true;
            }
        }
        return false;
    }
    
    // Находим все хэштеги в тексте
    function findHashtags(text) {
        var hashtags = [];
        
        if (!text || text.length < 2) {
            return hashtags;
        }
        
        // Ищем стандартные хэштеги вида #слово
        for (var i = 0; i < text.length - 1; i++) {
            // Находим символ #
            if (text.charAt(i) == '#') {
                // Проверяем, что перед # пробел или начало текста
                var isStartValid = (i == 0) || isWhitespaceChar(text.charAt(i - 1));
                
                if (isStartValid) {
                    var j = i + 1;
                    var wordStart = j;
                    
                    // Пропускаем дефис, если разрешено
                    if (ALLOW_PUNCTUATION_IN_HASHTAG && j < text.length && text.charAt(j) == '-') {
                        wordStart = j + 1;
                        j = wordStart;
                    }
                    
                    // Проверяем первый символ слова
                    if (j < text.length) {
                        var firstChar = text.charAt(j);
                        var isValid = false;
                        
                        // Проверяем в зависимости от настроек
                        if (HASHTAG_LOWERCASE && isLowercaseLetter(firstChar)) {
                            isValid = true;
                        } else if (HASHTAG_CAPITAL_WORD && isCapitalLetter(firstChar)) {
                            isValid = true;
                        }
                        
                        if (isValid) {
                            // Собираем всё слово
                            var wordLength = 0;
                            var wordHasPunctuation = false;
                            
                            while (j < text.length && isValidHashtagChar(text.charAt(j), ALLOW_PUNCTUATION_IN_HASHTAG)) {
                                // Проверяем на знаки препинания внутри слова
                                if (!ALLOW_PUNCTUATION_IN_HASHTAG && isPunctuation(text.charAt(j))) {
                                    wordHasPunctuation = true;
                                    break; // Прерываем если нашли знак препинания
                                }
                                j++;
                                wordLength++;
                            }
                            
                            // Проверяем, есть ли знаки препинания в конце слова
                            if (!ALLOW_PUNCTUATION_IN_HASHTAG && !wordHasPunctuation && j < text.length) {
                                if (isPunctuation(text.charAt(j))) {
                                    wordHasPunctuation = true;
                                }
                            }
                            
                            // Проверяем минимальную и максимальную длину (только если нет пунктуации или она разрешена)
                            if ((ALLOW_PUNCTUATION_IN_HASHTAG || !wordHasPunctuation) &&
                                wordLength >= HASHTAG_MIN_WORD_LENGTH && 
                                wordLength <= HASHTAG_MAX_WORD_LENGTH) {
                                
                                // Проверяем, что после слова пробел или конец текста или знак препинания
                                var isEndValid = (j >= text.length) || 
                                                 isWhitespaceChar(text.charAt(j)) || 
                                                 isPunctuation(text.charAt(j));
                                
                                if (isEndValid) {
                                    // Если пунктуация не разрешена и есть знак препинания в конце, не включаем его
                                    var endPos = j;
                                    if (!ALLOW_PUNCTUATION_IN_HASHTAG && wordHasPunctuation) {
                                        // Не добавляем этот хэштег
                                        continue;
                                    }
                                    
                                    // Извлекаем хэштег
                                    var hashtag = text.substring(i, endPos);
                                    hashtags.push(hashtag);
                                    
                                    // Переходим к позиции после хэштега
                                    i = endPos - 1;
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Ищем конструкции "число+пробел+слово" в начале абзаца (если включено)
        if (HASHTAG_NUMBER_START) {
            // Проверяем, начинается ли текст с числа
            var trimmedText = text.replace(/^\s+/, ''); // Убираем пробелы в начале
            
            if (trimmedText.length > 0 && isDigit(trimmedText.charAt(0))) {
                // Ищем конец числа
                var numEnd = 0;
                while (numEnd < trimmedText.length && isDigit(trimmedText.charAt(numEnd))) {
                    numEnd++;
                }
                
                // Проверяем, что после числа идет пробел
                if (numEnd < trimmedText.length && isWhitespaceChar(trimmedText.charAt(numEnd))) {
                    // Пропускаем пробелы
                    var wordStart = numEnd;
                    while (wordStart < trimmedText.length && isWhitespaceChar(trimmedText.charAt(wordStart))) {
                        wordStart++;
                    }
                    
                    // Проверяем, что после пробела идет слово
                    if (wordStart < trimmedText.length && isLetter(trimmedText.charAt(wordStart))) {
                        // Находим конец слова
                        var wordEnd = wordStart;
                        var wordHasPunctuation = false;
                        
                        while (wordEnd < trimmedText.length) {
                            var currentChar = trimmedText.charAt(wordEnd);
                            if (isLetter(currentChar) || isDigit(currentChar) || currentChar == '-') {
                                wordEnd++;
                            } else if (isPunctuation(currentChar)) {
                                wordHasPunctuation = true;
                                break;
                            } else {
                                break;
                            }
                        }
                        
                        var wordLength = wordEnd - wordStart;
                        
                        // Проверяем длину слова (только если нет пунктуации или она разрешена)
                        if ((ALLOW_PUNCTUATION_IN_HASHTAG || !wordHasPunctuation) &&
                            wordLength >= HASHTAG_MIN_WORD_LENGTH && 
                            wordLength <= HASHTAG_MAX_WORD_LENGTH) {
                            
                            // Проверяем, что после слова пробел или конец текста
                            var isEndValid = (wordEnd >= trimmedText.length) || 
                                             isWhitespaceChar(trimmedText.charAt(wordEnd)) || 
                                             isPunctuation(trimmedText.charAt(wordEnd));
                            
                            if (isEndValid) {
                                // Если пунктуация не разрешена и есть знак препинания, пропускаем
                                if (!ALLOW_PUNCTUATION_IN_HASHTAG && wordHasPunctuation) {
                                    return hashtags;
                                }
                                
                                // Формируем хэштег в виде #число_слово
                                var numberPart = trimmedText.substring(0, numEnd);
                                var wordPart = trimmedText.substring(wordStart, wordEnd);
                                var hashtag = "#" + numberPart + "_" + wordPart;
                                hashtags.push(hashtag);
                            }
                        }
                    }
                }
            }
        }
        
        return hashtags;
    }
    
    // ==================== ФУНКЦИИ АНАЛИЗА ====================
    
    function analyzeDocument() {
        var startTime = new Date();
        
        // Общая статистика
        var totalHashesInDocument = 0;
        
        // Статистика чистых решёток
        var totalCleanHashes = 0;
        var mainCleanTitles = 0;
        var mainCleanTitlesHashes = 0;
        var mainCleanSubtitles = 0;
        var mainCleanSubtitlesHashes = 0;
        var notesCleanTitles = 0;
        var notesCleanTitlesHashes = 0;
        var notesCleanSubtitles = 0;
        var notesCleanSubtitlesHashes = 0;
        
        // Статистика подозрительных решёток
        var totalSuspiciousHashes = 0;
        var totalSuspiciousEntries = 0;
        
        // Статистика хэштегов
        var totalHashtags = 0;
        var mainHashtags = 0;
        var notesHashtags = 0;
        var uniqueHashtags = {};
        var uniqueHashtagsCount = 0;
        
        // Основной раздел
        var mainTitlesWithHashes = 0;
        var mainTitlesHashesCount = 0;
        var mainSubtitlesWithHashes = 0;
        var mainSubtitlesHashesCount = 0;
        var mainStartParagraphs = 0;
        var mainStartHashesCount = 0;
        var mainInsideParagraphs = 0;
        var mainInsideHashesCount = 0;
        var mainEndParagraphs = 0;
        var mainEndHashesCount = 0;
        
        // Раздел сносок
        var notesTitlesWithHashes = 0;
        var notesTitlesHashesCount = 0;
        var notesSubtitlesWithHashes = 0;
        var notesSubtitlesHashesCount = 0;
        var notesStartParagraphs = 0;
        var notesStartHashesCount = 0;
        var notesInsideParagraphs = 0;
        var notesInsideHashesCount = 0;
        var notesEndParagraphs = 0;
        var notesEndHashesCount = 0;
        
        // Абзацы с ### в конце
        var mainTargetCount = 0;
        
        // Определяем расположение решёток в тексте
        function analyzeHashPositions(text) {
            var result = {
                hasHashes: false,
                hashesAtStart: 0,
                hashesInside: 0,
                hashesAtEnd: 0,
                hashesCount: 0
            };
            
            if (!text) return result;
            
            // Считаем все решётки
            var hashesCount = 0;
            for (var i = 0; i < text.length; i++) {
                if (text.charAt(i) == '#') hashesCount++;
            }
            result.hashesCount = hashesCount;
            result.hasHashes = hashesCount > 0;
            
            if (hashesCount == 0) return result;
            
            // Находим позиции всех решёток
            var hashPositions = [];
            for (var i = 0; i < text.length; i++) {
                if (text.charAt(i) == '#') {
                    hashPositions.push(i);
                }
            }
            
            // Для каждой решётки определяем положение
            for (var i = 0; i < hashPositions.length; i++) {
                var pos = hashPositions[i];
                var isStart = true;
                var isEnd = true;
                
                // Проверяем, что перед решёткой только пробелы
                for (var j = 0; j < pos; j++) {
                    if (!isWhitespaceChar(text.charAt(j))) {
                        isStart = false;
                        break;
                    }
                }
                
                // Проверяем, что после решётки только пробелы
                for (var j = pos + 1; j < text.length; j++) {
                    if (!isWhitespaceChar(text.charAt(j))) {
                        isEnd = false;
                        break;
                    }
                }
                
                if (isStart) {
                    result.hashesAtStart++;
                } else if (isEnd) {
                    result.hashesAtEnd++;
                } else {
                    result.hashesInside++;
                }
            }
            
            return result;
        }
        
        // Обработка одного абзаца для анализа
        function analyzeParagraph(p, inNotes) {
            var stats = {
                // Чистые решётки
                cleanTitles: 0, cleanTitlesHashes: 0,
                cleanSubtitles: 0, cleanSubtitlesHashes: 0,
                
                // Подозрительные решётки
                titlesWithHashes: 0, titlesHashesCount: 0,
                subtitlesWithHashes: 0, subtitlesHashesCount: 0,
                startParagraphs: 0, startHashesCount: 0,
                insideParagraphs: 0, insideHashesCount: 0,
                endParagraphs: 0, endHashesCount: 0,
                
                targetCount: 0,
                totalHashes: 0,
                hashtags: 0,
                hashtagList: []
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
            
            // Считаем все решётки
            var hashesInText = countHashes(fullText);
            stats.totalHashes = hashesInText;
            
            if (hashesInText == 0) {
                return stats; // Нет решёток
            }
            
            // Ищем хэштеги
            var hashtagsInParagraph = findHashtags(fullText);
            stats.hashtags = hashtagsInParagraph.length;
            stats.hashtagList = hashtagsInParagraph;
            
            // Проверяем, есть ли в тексте буквы/цифры (не считая хэштеги)
            var textWithoutHashtags = fullText;
            for (var h = 0; h < hashtagsInParagraph.length; h++) {
                // Заменяем хэштеги на пробелы для проверки
                var hashtag = hashtagsInParagraph[h];
                textWithoutHashtags = textWithoutHashtags.replace(hashtag, ' ');
            }
            var hasText = hasLettersOrDigits(textWithoutHashtags);
            
            // Анализируем расположение решёток (не считая хэштеги)
            var hashAnalysis = analyzeHashPositions(textWithoutHashtags);
            
            // Проверяем целевой паттерн (### в конце)
            var isTarget = reTarget.test(fullText);
            
            if (isSubtitle) {
                // Подзаголовок
                if (!hasText) {
                    // "Чистые" решётки в подзаголовке
                    stats.cleanSubtitles = 1;
                    stats.cleanSubtitlesHashes = hashesInText;
                } else {
                    // Подозрительные решётки в подзаголовке
                    stats.subtitlesWithHashes = 1;
                    stats.subtitlesHashesCount = hashesInText;
                }
            } 
            else if (insideTitle) {
                // Заголовок
                if (!hasText) {
                    // "Чистые" решётки в заголовке
                    stats.cleanTitles = 1;
                    stats.cleanTitlesHashes = hashesInText;
                } else {
                    // Подозрительные решётки в заголовке
                    stats.titlesWithHashes = 1;
                    stats.titlesHashesCount = hashesInText;
                }
            }
            else {
                // Обычный абзац
                if (hasText) {
                    // Есть текст
                    if (isTarget && hashesInText >= 3) {
                        // Абзац с ### в конце
                        stats.targetCount = 1;
                        
                        if (hashesInText > 3) {
                            // Лишние решётки (кроме 3 целевых)
                            var extraHashes = hashesInText - 3;
                            var remainingHashes = extraHashes;
                            
                            // Решётки в начале
                            if (hashAnalysis.hashesAtStart > 0) {
                                stats.startParagraphs = 1;
                                stats.startHashesCount = hashAnalysis.hashesAtStart;
                                remainingHashes -= hashAnalysis.hashesAtStart;
                            }
                            
                            // Решётки внутри
                            if (remainingHashes > 0) {
                                stats.insideParagraphs = 1;
                                stats.insideHashesCount = remainingHashes;
                            }
                        }
                    } else {
                        // Не целевой абзац - все решётки подозрительные
                        var hasStart = hashAnalysis.hashesAtStart > 0;
                        var hasInside = hashAnalysis.hashesInside > 0;
                        var hasEnd = hashAnalysis.hashesAtEnd > 0;
                        
                        if (hasStart) {
                            stats.startParagraphs = 1;
                            stats.startHashesCount = hashAnalysis.hashesAtStart;
                        }
                        
                        if (hasInside) {
                            stats.insideParagraphs = 1;
                            stats.insideHashesCount = hashAnalysis.hashesInside;
                        }
                        
                        if (hasEnd) {
                            stats.endParagraphs = 1;
                            stats.endHashesCount = hashAnalysis.hashesAtEnd;
                        }
                    }
                } else {
                    // Нет текста (только решётки)
                    if (isTarget && hashesInText >= 3) {
                        // Абзац с ### в конце
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
            totalHashesInDocument += stats.totalHashes;
            
            // Добавляем хэштеги
            if (stats.hashtags > 0) {
                if (inNotes) {
                    notesHashtags += stats.hashtags;
                } else {
                    mainHashtags += stats.hashtags;
                }
                totalHashtags += stats.hashtags;
                
                // Собираем уникальные хэштеги
                for (var h = 0; h < stats.hashtagList.length; h++) {
                    var hashtag = stats.hashtagList[h];
                    if (!uniqueHashtags[hashtag]) {
                        uniqueHashtags[hashtag] = true;
                    }
                }
            }
            
            if (inNotes) {
                // Раздел сносок
                notesCleanTitles += stats.cleanTitles;
                notesCleanTitlesHashes += stats.cleanTitlesHashes;
                notesCleanSubtitles += stats.cleanSubtitles;
                notesCleanSubtitlesHashes += stats.cleanSubtitlesHashes;
                
                notesTitlesWithHashes += stats.titlesWithHashes;
                notesTitlesHashesCount += stats.titlesHashesCount;
                notesSubtitlesWithHashes += stats.subtitlesWithHashes;
                notesSubtitlesHashesCount += stats.subtitlesHashesCount;
                notesStartParagraphs += stats.startParagraphs;
                notesStartHashesCount += stats.startHashesCount;
                notesInsideParagraphs += stats.insideParagraphs;
                notesInsideHashesCount += stats.insideHashesCount;
                notesEndParagraphs += stats.endParagraphs;
                notesEndHashesCount += stats.endHashesCount;
            } else {
                // Основной раздел
                mainCleanTitles += stats.cleanTitles;
                mainCleanTitlesHashes += stats.cleanTitlesHashes;
                mainCleanSubtitles += stats.cleanSubtitles;
                mainCleanSubtitlesHashes += stats.cleanSubtitlesHashes;
                
                mainTitlesWithHashes += stats.titlesWithHashes;
                mainTitlesHashesCount += stats.titlesHashesCount;
                mainSubtitlesWithHashes += stats.subtitlesWithHashes;
                mainSubtitlesHashesCount += stats.subtitlesHashesCount;
                mainStartParagraphs += stats.startParagraphs;
                mainStartHashesCount += stats.startHashesCount;
                mainInsideParagraphs += stats.insideParagraphs;
                mainInsideHashesCount += stats.insideHashesCount;
                mainEndParagraphs += stats.endParagraphs;
                mainEndHashesCount += stats.endHashesCount;
                
                mainTargetCount += stats.targetCount;
            }
        }
        
        // Считаем уникальные хэштеги
        for (var key in uniqueHashtags) {
            if (uniqueHashtags.hasOwnProperty(key)) {
                uniqueHashtagsCount++;
            }
        }
        
        // Считаем итоги
        totalCleanHashes = mainCleanTitlesHashes + mainCleanSubtitlesHashes + 
                         notesCleanTitlesHashes + notesCleanSubtitlesHashes;
        
        var mainSuspiciousTotal = mainTitlesHashesCount + mainSubtitlesHashesCount + 
                                 mainStartHashesCount + mainInsideHashesCount + mainEndHashesCount;
        var notesSuspiciousTotal = notesTitlesHashesCount + notesSubtitlesHashesCount + 
                                  notesStartHashesCount + notesInsideHashesCount + notesEndHashesCount;
        totalSuspiciousHashes = mainSuspiciousTotal + notesSuspiciousTotal;
        
        // Считаем вхождения (элементы с решётками)
        var mainSuspiciousEntries = mainTitlesWithHashes + mainSubtitlesWithHashes + 
                                   mainStartParagraphs + mainInsideParagraphs + mainEndParagraphs;
        var notesSuspiciousEntries = notesTitlesWithHashes + notesSubtitlesWithHashes + 
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
        if (totalHashesInDocument == 0) {
            message += "В документе символов решётки (#) не найдено!\n\n";
            message += "Время выполнения: " + executionTime.toFixed(2) + " сек.";
            
            return {
                message: message,
                mainTargetCount: mainTargetCount,
                hasHashes: false,
                hasSuspicious: false,
                totalHashtags: totalHashtags
            };
        }
        
        message += "ВСЕГО символов решётки (#) в документе: " + totalHashesInDocument + " шт.\n\n";
        
        // Статистика хэштегов (новый раздел)
        if (showFullStats || totalHashtags > 0) {
            message += "• ВОЗМОЖНЫЕ ХЭШТЕГИ:\n";
            
            if (totalHashtags > 0) {
                message += "Всего возможных хэштегов: " + totalHashtags + " шт.\n";
                message += "Уникальных хэштегов: " + uniqueHashtagsCount + " шт.\n";
                
                if (mainHashtags > 0) {
                    message += "Основной раздел: " + mainHashtags + " шт.\n";
                }
                if (notesHashtags > 0) {
                    message += "Раздел сносок: " + notesHashtags + " шт.\n";
                }
                
                // Выводим несколько примеров хэштегов (первые 25 символов)
                message += "Примеры: ";
                var examples = [];
                var exampleCount = 0;
                for (var key in uniqueHashtags) {
                    if (uniqueHashtags.hasOwnProperty(key)) {
                        var example = key;
                        if (example.length > 25) {
                            example = example.substring(0, 22) + "...";
                        }
                        examples.push(example);
                        exampleCount++;
                        if (exampleCount >= 5) break; // максимум 5 примеров
                    }
                }
                if (examples.length > 0) {
                    message += examples.join(", ");
                } else {
                    message += "нет";
                }
                message += "\n";
            } else {
                message += "Хэштегов не найдено\n";
            }
            message += "---------------------------\n";
        }
        
        // Раздел "Чистые решётки"
        if (showFullStats || totalCleanHashes > 0) {
            message += "• \"ЧИСТЫЕ\" РЕШЁТКИ # (без букв/цифр), всего: " + totalCleanHashes + "\n";
            
            var mainCleanTotal = mainCleanTitlesHashes + mainCleanSubtitlesHashes;
            if (showFullStats || mainCleanTotal > 0) {
                message += "Основной раздел: " + mainCleanTotal + "\n";
                if (showFullStats || mainCleanTitles > 0) {
                    message += "-   Заголовки: " + mainCleanTitles + " (" + mainCleanTitlesHashes + " #)\n";
                }
                if (showFullStats || mainCleanSubtitles > 0) {
                    message += "-   Подзаголовки: " + mainCleanSubtitles + " (" + mainCleanSubtitlesHashes + " #)\n";
                }
            }
            
            var notesCleanTotal = notesCleanTitlesHashes + notesCleanSubtitlesHashes;
            if (showFullStats || notesCleanTotal > 0) {
                message += "Раздел сносок: " + notesCleanTotal + "\n";
                if (showFullStats || notesCleanTitles > 0) {
                    message += "-   Заголовки в сносках: " + notesCleanTitles + " (" + notesCleanTitlesHashes + " #)\n";
                }
                if (showFullStats || notesCleanSubtitles > 0) {
                    message += "-   Подзаголовки в сносках: " + notesCleanSubtitles + " (" + notesCleanSubtitlesHashes + " #)\n";
                }
            }
            message += "---------------------------\n";
        }
        
        // Раздел "Подозрительные решётки"
        if (showFullStats || totalSuspiciousHashes > 0) {
            if (totalSuspiciousHashes > 0 || showFullStats) {
                message += "• ПОДОЗРИТЕЛЬНЫЕ РЕШЁТКИ #\n";
                message += "(возможные маркеры сносок):\n\n";
                
                if (mainSuspiciousTotal > 0 || showFullStats) {
                    var mainEntriesStr = "";
                    if (mainSuspiciousEntries > 0) {
                        mainEntriesStr = " (" + mainSuspiciousEntries + " вхождений)";
                    }
                    message += "Основной раздел, всего #: " + mainSuspiciousTotal + " шт" + mainEntriesStr + ".\n";
                    
                    if (showFullStats || mainTitlesWithHashes > 0) {
                        message += "-   Заголовков с #: " + mainTitlesWithHashes;
                        if (mainTitlesHashesCount > 0) {
                            message += " (" + mainTitlesHashesCount + " решёток)";
                        }
                        message += "\n";
                    }
                    
                    if (showFullStats || mainSubtitlesWithHashes > 0) {
                        message += "-   Подзаголовков с #: " + mainSubtitlesWithHashes;
                        if (mainSubtitlesHashesCount > 0) {
                            message += " (" + mainSubtitlesHashesCount + " решёток)";
                        }
                        message += "\n";
                    }
                    
                    if (showFullStats || mainStartParagraphs > 0 || mainInsideParagraphs > 0 || mainEndParagraphs > 0) {
                        message += "ОБЫЧНЫЕ АБЗАЦЫ:\n";
                        if (showFullStats || mainStartParagraphs > 0) {
                            message += "-   Абзацев с # в начале: " + mainStartParagraphs;
                            if (mainStartHashesCount > 0) {
                                message += " (" + mainStartHashesCount + " решёток)";
                            }
                            message += "\n";
                        }
                        if (showFullStats || mainInsideParagraphs > 0) {
                            message += "-   Абзацев с # внутри: " + mainInsideParagraphs;
                            if (mainInsideHashesCount > 0) {
                                message += " (" + mainInsideHashesCount + " решёток)";
                            }
                            message += "\n";
                        }
                        if (showFullStats || mainEndParagraphs > 0) {
                            message += "-   Абзацев с # в конце: " + mainEndParagraphs;
                            if (mainEndHashesCount > 0) {
                                message += " (" + mainEndHashesCount + " решёток)";
                            }
                        }
                    }
                    message += "\n---------------------------\n";
                }
                
                if (notesSuspiciousTotal > 0 || showFullStats) {
                    var notesEntriesStr = "";
                    if (notesSuspiciousEntries > 0) {
                        notesEntriesStr = " (" + notesSuspiciousEntries + " вхождений)";
                    }
                    message += "Раздел сносок, всего: " + notesSuspiciousTotal + " шт" + notesEntriesStr + ".\n";
                    
                    if (showFullStats || notesTitlesWithHashes > 0) {
                        message += "-   Заголовков сносок с #: " + notesTitlesWithHashes;
                        if (notesTitlesHashesCount > 0) {
                            message += " (" + notesTitlesHashesCount + " решёток)";
                        }
                        message += "\n";
                    }
                    
                    if (showFullStats || notesSubtitlesWithHashes > 0) {
                        message += "-   Подзаголовков сносок с #: " + notesSubtitlesWithHashes;
                        if (notesSubtitlesHashesCount > 0) {
                            message += " (" + notesSubtitlesHashesCount + " решёток)";
                        }
                        message += "\n";
                    }
                    
                    if (showFullStats || notesStartParagraphs > 0 || notesInsideParagraphs > 0 || notesEndParagraphs > 0) {
                        message += "ОБЫЧНЫЕ АБЗАЦЫ В СНОСКАХ:\n";
                        if (showFullStats || notesStartParagraphs > 0) {
                            message += "-   Абзацев сносок с # в начале: " + notesStartParagraphs;
                            if (notesStartHashesCount > 0) {
                                message += " (" + notesStartHashesCount + " решёток)";
                            }
                            message += "\n";
                        }
                        if (showFullStats || notesInsideParagraphs > 0) {
                            message += "-   Абзацев сносок с # внутри: " + notesInsideParagraphs;
                            if (notesInsideHashesCount > 0) {
                                message += " (" + notesInsideHashesCount + " решёток)";
                            }
                            message += "\n";
                        }
                        if (showFullStats || notesEndParagraphs > 0) {
                            message += "-   Абзацев сносок с # в конце: " + notesEndParagraphs;
                            if (notesEndHashesCount > 0) {
                                message += " (" + notesEndHashesCount + " решёток)";
                            }
                        }
                    }
                    message += "\n---------------------------\n";
                }
                
                // Предупреждение (только если есть подозрительные решётки)
                if (totalSuspiciousHashes > 0) {
                    message += "• ВНИМАНИЕ: В тексте обнаружены решётки #,\n";
                    message += "которые могут быть маркерами сносок.\n";
                    message += "- Особое внимание на:\n";
                    
                    var mainWarnings = [];
                    var notesWarnings = [];
                    
                    if (mainStartHashesCount > 0) mainWarnings.push("в начале");
                    if (mainInsideHashesCount > 0) mainWarnings.push("внутри");
                    if (mainEndHashesCount > 0) mainWarnings.push("в конце");
                    
                    if (notesStartHashesCount > 0) notesWarnings.push("в начале");
                    if (notesInsideHashesCount > 0) notesWarnings.push("внутри");
                    if (notesEndHashesCount > 0) notesWarnings.push("в конце");
                    
                    if (mainWarnings.length > 0) {
                        message += "• Основной текст: Решётки # ";
                        message += mainWarnings.join(", ");
                        message += " абзацев!\n";
                    }
                    
                    if (notesWarnings.length > 0) {
                        message += "• Сноски: Решётки # ";
                        message += notesWarnings.join(", ");
                        message += " абзацев!\n";
                    }
                    
                    message += "Рекомендуется проверить их вручную!\n";
                    message += "---------------------------\n";
                }
            }
        }
        
        // Абзацы с ### в конце
        if (mainTargetCount > 0 || showFullStats) {
            message += "• Абзацев с ### в конце: " + mainTargetCount + "\n";
            if (mainTargetCount > 0) {
                message += "(### рекомендуется преобразовать в подзаголовки)\n";
            }
            message += "---------------------------\n";
        }
        
        message += "Время выполнения: " + executionTime.toFixed(2) + " сек.";
        
        return {
            message: message,
            mainTargetCount: mainTargetCount,
            hasHashes: totalHashesInDocument > 0,
            hasSuspicious: totalSuspiciousHashes > 0,
            totalHashtags: totalHashtags,
            uniqueHashtagsCount: uniqueHashtagsCount
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
