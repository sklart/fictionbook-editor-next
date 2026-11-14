// Скрипт "Исправить римские цифры" для редактора FBE
// version 3.4
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для замены кириллицы на латиницу в римских цифрах в fb2 документах.
// Заменяются кириллические символы С, М, Х на аналогичные латинские C, M, X.
// Также есть настройка изменения регистра для римских цифр:
// - делать строчные римские заглавными или оставлять как есть.
// Аналогично римские числа в смешанном регистре (XvI) можно делать заглавными.
// Есть редактируемый "игнор-лист" для отдельных слов или символов, вызывающих лишние замены.
// Скрипт работает сразу со всем документом.
// По умолчанию обрабатываются все разделы документа, включая разделы сносок и комментариев.
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 3.4, 23.03.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Исправить римские цифры";
    var version = "3.4";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1;
    
    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 1;
    
    // Обрабатывать раздел комментариев
    var processCommentsSection = 1;
    
    // Исправлять ли кириллицу в строчных римских числах
    // 1 = исправлять также, как и в заглавных, 0 = оставлять как есть
    var processSmallCyrRoman = 1;
    
    // Исправлять ли строчные римские на ЗАГЛАВНЫЕ (исправляем целиком все римское "слово")
    // 1 = делать заглавными, 0 = оставлять как есть
    var processSmallRoman = 1;
    
    // Исправлять ли смешанный регистр (XvI, MiX и т.п.) на ЗАГЛАВНЫЕ
    // 1 = делать заглавными, 0 = игнорировать
    var processMixedRoman = 1;
    
    // ==================================================
    // СПИСОК ИГНОРИРУЕМЫХ СЛОВ И СИМВОЛОВ
    // ==================================================
    // Здесь можно перечислить отдельные символы или слова,
    // которые НЕ нужно обрабатывать, даже если они состоят из похожих "римских" символов
    
    var ignoreList = [
        "М", "м",     // заглавная и строчная русская М
        "С", "с",     // заглавная и строчная русская С (предлоги, сокращения)
        "C", "c",     // заглавная и строчная латинская С (отдельностоящие, вместо возможных русских)
        "ССР",     // известная аббревиатура русскими буквами
        "СС",      // известная аббревиатура русскими буквами
        "МММ",     // известная аббревиатура русскими буквами
        "т-с-с", "т-с-с!"      // варианты с дефисами и знаками
    ];
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    // Получаем неразрывный пробел из настроек FBE
    var nbspChar = String.fromCharCode(160);
    try {
        nbspChar = window.external.GetNBSP();
    } catch(e) {}
    
    // ==== ИЩЕМ ТОЛЬКО В ТЕЛЕ ДОКУМЕНТА (fbw_body) ====
    var bodyElement = document.getElementById("fbw_body");
    
    if (!bodyElement) {
        MsgBox(scriptName + "\nver. " + version + "\n---------------------------\n\n✗ ОШИБКА: Не найден элемент fbw_body");
        return;
    }
    
    // Получаем все DIV с классом body внутри fbw_body
    var bodies = [];
    var allDivs = bodyElement.getElementsByTagName("div");
    for (var i = 0; i < allDivs.length; i++) {
        if (allDivs[i].className == "body") {
            bodies[bodies.length] = allDivs[i];
        }
    }
    
    // Если не нашли DIV с классом body, используем сам fbw_body
    if (bodies.length == 0) {
        bodies = [bodyElement];
    }
    
    // ==================================================
    // ПРЕДВАРИТЕЛЬНЫЙ АНАЛИЗ (без таймера!)
    // ==================================================
    
    // Статистика для предварительного анализа
    var previewStats = {
        cyrillicErrors: { total: 0, main: 0, notes: 0, comments: 0 },
        caseChanges: { total: 0, main: 0, notes: 0, comments: 0 },
        hasNotes: false,
        hasComments: false
    };
    
    // Проверяем наличие разделов сносок и комментариев
    for (var b = 0; b < bodies.length; b++) {
        var body = bodies[b];
        var fbname = body.getAttribute("fbname");
        if (fbname == null) fbname = "";
        if (fbname == "notes") previewStats.hasNotes = true;
        if (fbname == "comments") previewStats.hasComments = true;
    }
    
    // Функция определения типа раздела по элементу
    function getSectionType(element) {
        var current = element;
        while (current) {
            if (current.nodeType == 1 && current.className == "body") {
                var fbname = current.getAttribute("fbname");
                if (fbname == null) fbname = "";
                if (fbname == "notes") return "notes";
                if (fbname == "comments") return "comments";
                return "main";
            }
            current = current.parentNode;
        }
        return "unknown";
    }
    
    // Функция проверки, является ли символ разделителем (для предпросмотра)
    function isSeparatorPreview(c) {
        return (c == " " || c == "\t" || c == "\n" || c == "\r" || 
               c == "." || c == "," || c == "!" || c == "?" || 
               c == ";" || c == ":" || c == "-" || c == "—" ||
               c == "(" || c == ")" || c == "[" || c == "]" ||
               c == "{" || c == "}" || c == "\"" || c == "'" ||
               c == "«" || c == "»" || c == "…" || c == "•" ||
               c == "·" || c == "∗" || c == "‒" || c == "–" ||
               c == "—" || c == "―");
    }
    
    // Функция проверки, является ли слово "х" частью конструкции "цифры-х"
    function isYearWithXPreview(word, fullText, position) {
        if (word != "х" && word != "x" && word != "Х" && word != "X") return false;
        if (position <= 0) return false;
        var prevChar = fullText.charAt(position - 1);
        if (prevChar != "-") return false;
        var searchPos = position - 2;
        var hasDigit = false;
        while (searchPos >= 0) {
            var c = fullText.charAt(searchPos);
            if (c >= '0' && c <= '9') {
                hasDigit = true;
                break;
            }
            if (c != "-" && c != " " && c != "\t" && c != "\n") break;
            searchPos--;
        }
        return hasDigit;
    }
    
    // Функция проверки, состоит ли слово только из римских символов (для предпросмотра)
    function isOnlyRomanCharsPreview(word) {
        var allRomanChars = "IVXLCDMivxlcdmСХМсхм";
        for (var i = 0; i < word.length; i++) {
            var c = word.charAt(i);
            var found = false;
            for (var j = 0; j < allRomanChars.length; j++) {
                if (c == allRomanChars.charAt(j)) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
        return true;
    }
    
    // Функция проверки, является ли слово потенциальным римским числом (для предпросмотра)
    function isRomanCandidatePreview(word, fullText, position) {
        if (!word || word.length == 0) return false;
        if (isYearWithXPreview(word, fullText, position)) return false;
        return isOnlyRomanCharsPreview(word);
    }
    
    // Функция проверки, есть ли в слове кириллические символы (для предпросмотра)
    function hasCyrillicPreview(word) {
        for (var i = 0; i < word.length; i++) {
            var c = word.charAt(i);
            if (c == 'С' || c == 'М' || c == 'Х' || c == 'с' || c == 'м' || c == 'х') return true;
        }
        return false;
    }
    
    // Функция определения типа регистра слова (для предпросмотра)
    function getCaseTypePreview(word) {
        var hasUpper = false, hasLower = false;
        for (var i = 0; i < word.length; i++) {
            var c = word.charAt(i);
            var code = word.charCodeAt(i);
            if (code >= 65 && code <= 90) hasUpper = true;
            if (code >= 97 && code <= 122) hasLower = true;
            if (c == 'С' || c == 'М' || c == 'Х') hasUpper = true;
            if (c == 'с' || c == 'м' || c == 'х') hasLower = true;
        }
        if (hasUpper && hasLower) return "mixed";
        if (hasLower) return "lower";
        return "upper";
    }
    
    // Функция проверки, находится ли слово в игнор-листе (для предпросмотра)
    function isInIgnoreListPreview(word) {
        if (!word || word.length == 0) return false;
        var cleanWord = word;
        while (cleanWord.length > 0) {
            var lastChar = cleanWord.charAt(cleanWord.length - 1);
            if (lastChar == '?' || lastChar == '!' || lastChar == '.' || 
                lastChar == ',' || lastChar == ';' || lastChar == ':' ||
                lastChar == ')' || lastChar == ']' || lastChar == '}') {
                cleanWord = cleanWord.substring(0, cleanWord.length - 1);
            } else break;
        }
        if (cleanWord.length == 0) return false;
        for (var i = 0; i < ignoreList.length; i++) {
            if (cleanWord == ignoreList[i]) return true;
        }
        return false;
    }
    
    // Функция для предварительного подсчета
    function previewCount(textNodes) {
        for (var n = 0; n < textNodes.length; n++) {
            var node = textNodes[n];
            var text = node.nodeValue;
            if (!text || text.length == 0) continue;
            
            var currentWord = "";
            var wordStart = 0;
            
            for (var i = 0; i <= text.length; i++) {
                var c = (i < text.length) ? text.charAt(i) : "";
                var isSep = (i == text.length) ? true : isSeparatorPreview(c);
                
                if (!isSep) {
                    if (currentWord.length == 0) wordStart = i;
                    currentWord += c;
                } else {
                    if (currentWord.length > 0) {
                        var sectionType = getSectionType(node);
                        
                        if (!isInIgnoreListPreview(currentWord)) {
                            if (isRomanCandidatePreview(currentWord, text, wordStart)) {
                                var caseType = getCaseTypePreview(currentWord);
                                var hasCyr = hasCyrillicPreview(currentWord);
                                
                                if (hasCyr) {
                                    if (!(caseType == "lower" && !processSmallCyrRoman)) {
                                        previewStats.cyrillicErrors.total++;
                                        if (sectionType == "main") previewStats.cyrillicErrors.main++;
                                        else if (sectionType == "notes") previewStats.cyrillicErrors.notes++;
                                        else if (sectionType == "comments") previewStats.cyrillicErrors.comments++;
                                    }
                                }
                                
                                var needsCaseChange = false;
                                if (!hasCyr) {
                                    if (caseType == "lower" && processSmallRoman) needsCaseChange = true;
                                    if (caseType == "mixed" && processMixedRoman) needsCaseChange = true;
                                } else {
                                    if (caseType == "lower" && processSmallRoman) needsCaseChange = true;
                                    if (caseType == "mixed" && processMixedRoman) needsCaseChange = true;
                                }
                                
                                if (needsCaseChange) {
                                    previewStats.caseChanges.total++;
                                    if (sectionType == "main") previewStats.caseChanges.main++;
                                    else if (sectionType == "notes") previewStats.caseChanges.notes++;
                                    else if (sectionType == "comments") previewStats.caseChanges.comments++;
                                }
                            }
                        }
                        currentWord = "";
                    }
                }
            }
        }
    }
    
    // Собираем текстовые узлы для предварительного анализа
    var allTextNodes = [];
    
    for (var b = 0; b < bodies.length; b++) {
        var body = bodies[b];
        
        function collectNodes(node) {
            if (!node) return;
            
            if (node.nodeType == 3) {
                allTextNodes[allTextNodes.length] = node;
            } else if (node.nodeType == 1) {
                var tagName = node.nodeName ? node.nodeName.toLowerCase() : "";
                if (tagName == "script" || tagName == "style") return;
                for (var i = 0; i < node.childNodes.length; i++) {
                    collectNodes(node.childNodes[i]);
                }
            }
        }
        
        collectNodes(body);
    }
    
    // Запускаем предварительный подсчет
    previewCount(allTextNodes);
    
    // ==== ПРЕДВАРИТЕЛЬНОЕ СООБЩЕНИЕ (только если есть что исправлять) ====
    var hasSomethingToFix = (previewStats.cyrillicErrors.total > 0 || previewStats.caseChanges.total > 0);
    
    if (hasSomethingToFix && showStatistics) {
        var previewMessage = "";
        previewMessage += scriptName + "\n";
        previewMessage += "ver. " + version + "\n";
        previewMessage += "---------------------------\n\n";
        previewMessage += "Будут исправлены кириллические символы С, М, Х\n";
        previewMessage += "в римских числах на латинские C, M, X.\n\n";
        
        if (previewStats.cyrillicErrors.total > 0) {
            previewMessage += "Обнаружено всего ошибок рус-лат в римских числах - " + previewStats.cyrillicErrors.total + "\n";
            previewMessage += "- В основном разделе - " + previewStats.cyrillicErrors.main + "\n";
            if (processNotesSection && previewStats.hasNotes) {
                previewMessage += "- В разделе сносок (примечаний) - " + previewStats.cyrillicErrors.notes + "\n";
            }
            if (processCommentsSection && previewStats.hasComments) {
                previewMessage += "- В разделе комментариев - " + previewStats.cyrillicErrors.comments + "\n";
            }
            previewMessage += "\n";
        }
        
        if (previewStats.caseChanges.total > 0) {
            previewMessage += "Мест для изменения регистра в римских числах - " + previewStats.caseChanges.total + "\n";
            previewMessage += "- В основном разделе - " + previewStats.caseChanges.main + "\n";
            if (processNotesSection && previewStats.hasNotes) {
                previewMessage += "- В разделе сносок (примечаний) - " + previewStats.caseChanges.notes + "\n";
            }
            if (processCommentsSection && previewStats.hasComments) {
                previewMessage += "- В разделе комментариев - " + previewStats.caseChanges.comments + "\n";
            }
            previewMessage += "\n";
        }
        
        // Сообщаем об отсутствии разделов
        if (!previewStats.hasNotes && !previewStats.hasComments) {
            previewMessage += "Разделов сносок и комментариев в документе нет.\n\n";
        } else {
            if (!previewStats.hasNotes && previewStats.hasComments) {
                previewMessage += "Раздела сносок (примечаний) в документе нет.\n\n";
            }
            if (previewStats.hasNotes && !previewStats.hasComments) {
                previewMessage += "Раздела комментариев в документе нет.\n\n";
            }
        }
        
        previewMessage += "---------------------------\n";
        previewMessage += "Настройки обработки:\n";
        previewMessage += "Обработка раздела сносок (примечаний): " + (processNotesSection ? "ДА" : "НЕТ") + "\n";
        previewMessage += "Обработка раздела комментариев: " + (processCommentsSection ? "ДА" : "НЕТ") + "\n\n";
        previewMessage += "Исправлять строчные кириллические: " + (processSmallCyrRoman ? "ДА" : "НЕТ") + "\n";
        previewMessage += "Делать все римские ЗАГЛАВНЫМИ: " + (processSmallRoman ? "ДА" : "НЕТ") + "\n";
        previewMessage += "Исправлять смешанный регистр: " + (processMixedRoman ? "ДА" : "НЕТ") + "\n\n";
        previewMessage += "Продолжить?";
        
        if (AskYesNo(previewMessage) == 0) {
            return;
        }
    }
    
    // ==== ОСНОВНАЯ ОБРАБОТКА (таймер только сейчас!) ====
    var startTime = new Date();
    
    // Начинаем операцию для возможности отмены Ctrl+Z
    window.external.BeginUndoUnit(document, scriptName + " ver." + version);
    
    try {
        // Статистика (реальная)
        var stats = {
            cyrillicReplacements: { total: 0, main: 0, notes: 0, comments: 0 },
            caseChanges: { total: 0, main: 0, notes: 0, comments: 0 },
            changedParagraphs: { total: 0, main: 0, notes: 0, comments: 0 },
            mainSections: 0,
            notesSections: 0,
            commentsSections: 0,
            otherSections: 0,
            skippedSections: 0,
            ignoredWords: 0,
            hasNotes: false,
            hasComments: false
        };
        
        // Проверяем наличие разделов
        for (var b = 0; b < bodies.length; b++) {
            var body = bodies[b];
            var fbname = body.getAttribute("fbname");
            if (fbname == null) fbname = "";
            if (fbname == "notes") stats.hasNotes = true;
            if (fbname == "comments") stats.hasComments = true;
        }
        
        // Все символы римских цифр (для проверки)
        var allRomanChars = "IVXLCDMivxlcdmСХМсхм";
        
        // Функция проверки, состоит ли слово только из символов римских цифр
        function isOnlyRomanChars(word) {
            for (var i = 0; i < word.length; i++) {
                var c = word.charAt(i);
                var found = false;
                for (var j = 0; j < allRomanChars.length; j++) {
                    if (c == allRomanChars.charAt(j)) {
                        found = true;
                        break;
                    }
                }
                if (!found) return false;
            }
            return true;
        }
        
        // Функция проверки, является ли слово "х" частью конструкции "цифры-х"
        function isYearWithX(word, fullText, position) {
            if (word != "х" && word != "x" && word != "Х" && word != "X") return false;
            if (position <= 0) return false;
            var prevChar = fullText.charAt(position - 1);
            if (prevChar != "-") return false;
            var searchPos = position - 2;
            var hasDigit = false;
            while (searchPos >= 0) {
                var c = fullText.charAt(searchPos);
                if (c >= '0' && c <= '9') {
                    hasDigit = true;
                    break;
                }
                if (c != "-" && c != " " && c != "\t" && c != "\n") break;
                searchPos--;
            }
            return hasDigit;
        }
        
        // Функция проверки, является ли слово потенциальным римским числом
        function isRomanCandidate(word, fullText, position) {
            if (!word || word.length == 0) return false;
            if (isYearWithX(word, fullText, position)) return false;
            return isOnlyRomanChars(word);
        }
        
        // Функция проверки, есть ли в слове кириллические символы
        function hasCyrillic(word) {
            for (var i = 0; i < word.length; i++) {
                var c = word.charAt(i);
                if (c == 'С' || c == 'М' || c == 'Х' || c == 'с' || c == 'м' || c == 'х') return true;
            }
            return false;
        }
        
        // Функция определения типа регистра слова
        function getCaseType(word) {
            var hasUpper = false, hasLower = false;
            for (var i = 0; i < word.length; i++) {
                var c = word.charAt(i);
                var code = word.charCodeAt(i);
                if (code >= 65 && code <= 90) hasUpper = true;
                if (code >= 97 && code <= 122) hasLower = true;
                if (c == 'С' || c == 'М' || c == 'Х') hasUpper = true;
                if (c == 'с' || c == 'м' || c == 'х') hasLower = true;
            }
            if (hasUpper && hasLower) return "mixed";
            if (hasLower) return "lower";
            return "upper";
        }
        
        // Функция проверки, находится ли слово в игнор-листе
        function isInIgnoreList(word) {
            if (!word || word.length == 0) return false;
            var cleanWord = word;
            while (cleanWord.length > 0) {
                var lastChar = cleanWord.charAt(cleanWord.length - 1);
                if (lastChar == '?' || lastChar == '!' || lastChar == '.' || 
                    lastChar == ',' || lastChar == ';' || lastChar == ':' ||
                    lastChar == ')' || lastChar == ']' || lastChar == '}') {
                    cleanWord = cleanWord.substring(0, cleanWord.length - 1);
                } else break;
            }
            if (cleanWord.length == 0) return false;
            for (var i = 0; i < ignoreList.length; i++) {
                if (cleanWord == ignoreList[i]) return true;
            }
            return false;
        }
        
        // Функция определения типа раздела (ищем DIV с class="body")
        function getSectionType(element) {
            var current = element;
            while (current) {
                if (current.nodeType == 1 && current.className == "body") {
                    var fbname = current.getAttribute("fbname");
                    if (fbname == null) fbname = "";
                    if (fbname == "notes") return "notes";
                    if (fbname == "comments") return "comments";
                    return "main";
                }
                current = current.parentNode;
            }
            return "unknown";
        }
        
        // Функция замены кириллицы на латиницу и изменения регистра
        function fixRomanString(str, makeUppercase) {
            var result = "";
            var cyrillicCount = 0;
            var caseChangeCount = 0;
            
            for (var i = 0; i < str.length; i++) {
                var c = str.charAt(i);
                var replaced = false;
                
                if (c == 'С') {
                    result += 'C';
                    cyrillicCount++;
                    replaced = true;
                } else if (c == 'М') {
                    result += 'M';
                    cyrillicCount++;
                    replaced = true;
                } else if (c == 'Х') {
                    result += 'X';
                    cyrillicCount++;
                    replaced = true;
                } else if (processSmallCyrRoman) {
                    if (c == 'с') {
                        result += makeUppercase ? 'C' : 'c';
                        cyrillicCount++;
                        replaced = true;
                        if (makeUppercase) caseChangeCount++;
                    } else if (c == 'м') {
                        result += makeUppercase ? 'M' : 'm';
                        cyrillicCount++;
                        replaced = true;
                        if (makeUppercase) caseChangeCount++;
                    } else if (c == 'х') {
                        result += makeUppercase ? 'X' : 'x';
                        cyrillicCount++;
                        replaced = true;
                        if (makeUppercase) caseChangeCount++;
                    }
                }
                
                if (!replaced) {
                    if (makeUppercase) {
                        var code = c.charCodeAt(0);
                        if (code >= 97 && code <= 122) {
                            result += String.fromCharCode(code - 32);
                            caseChangeCount++;
                        } else {
                            result += c;
                        }
                    } else {
                        result += c;
                    }
                }
            }
            
            return {
                text: result,
                cyrillicCount: cyrillicCount,
                caseChangeCount: caseChangeCount
            };
        }
        
        // Функция для проверки, является ли символ разделителем
        function isSeparator(c) {
            return (c == " " || c == "\t" || c == "\n" || c == "\r" || 
                   c == "." || c == "," || c == "!" || c == "?" || 
                   c == ";" || c == ":" || c == "-" || c == "—" ||
                   c == "(" || c == ")" || c == "[" || c == "]" ||
                   c == "{" || c == "}" || c == "\"" || c == "'" ||
                   c == "«" || c == "»" || c == "…" || c == "•" ||
                   c == "·" || c == "∗" || c == "‒" || c == "–" ||
                   c == "—" || c == "―");
        }
        
        // Считаем разделы
        for (var b = 0; b < bodies.length; b++) {
            var body = bodies[b];
            var fbname = body.getAttribute("fbname");
            if (fbname == null) fbname = "";
            
            if (fbname == "") {
                stats.mainSections++;
            } else if (fbname == "notes") {
                stats.notesSections++;
            } else if (fbname == "comments") {
                stats.commentsSections++;
            } else {
                stats.otherSections++;
            }
        }
        
        // Собираем текстовые узлы для обработки
        var textNodes = [];
        
        for (var b = 0; b < bodies.length; b++) {
            var body = bodies[b];
            var fbname = body.getAttribute("fbname");
            if (fbname == null) fbname = "";
            
            var processThisBody = true;
            
            if (fbname == "notes" && !processNotesSection) {
                processThisBody = false;
                stats.skippedSections++;
            }
            if (fbname == "comments" && !processCommentsSection) {
                processThisBody = false;
                stats.skippedSections++;
            }
            
            if (!processThisBody) continue;
            
            function collectTextNodes(node) {
                if (!node) return;
                
                if (node.nodeType == 3) {
                    textNodes[textNodes.length] = node;
                } else if (node.nodeType == 1) {
                    var tagName = node.nodeName ? node.nodeName.toLowerCase() : "";
                    if (tagName == "script" || tagName == "style") return;
                    for (var i = 0; i < node.childNodes.length; i++) {
                        collectTextNodes(node.childNodes[i]);
                    }
                }
            }
            
            collectTextNodes(body);
        }
        
        // Обрабатываем текстовые узлы
        var changedParagraphsMap = [];
        
        for (var n = 0; n < textNodes.length; n++) {
            var node = textNodes[n];
            var text = node.nodeValue;
            if (!text || text.length == 0) continue;
            
            var newText = "";
            var currentWord = "";
            var wordStart = -1;
            var nodeChanged = false;
            
            for (var i = 0; i <= text.length; i++) {
                var c = (i < text.length) ? text.charAt(i) : "";
                var isSep = (i == text.length) ? true : isSeparator(c);
                
                if (!isSep) {
                    if (wordStart == -1) {
                        wordStart = i;
                        currentWord = c;
                    } else {
                        currentWord += c;
                    }
                } else {
                    if (wordStart != -1) {
                        var sectionType = getSectionType(node);
                        
                        if (isInIgnoreList(currentWord)) {
                            newText += currentWord;
                            stats.ignoredWords++;
                        }
                        else if (isRomanCandidate(currentWord, text, wordStart)) {
                            var caseType = getCaseType(currentWord);
                            var hasCyr = hasCyrillic(currentWord);
                            var needsFixing = false;
                            var makeUppercase = false;
                            
                            if (hasCyr) {
                                if (caseType == "lower" && !processSmallCyrRoman) {
                                    needsFixing = false;
                                } else {
                                    needsFixing = true;
                                    if (caseType == "lower" && processSmallRoman) makeUppercase = true;
                                    if (caseType == "mixed" && processMixedRoman) makeUppercase = true;
                                }
                            } else {
                                if (caseType == "lower" && processSmallRoman) { needsFixing = true; makeUppercase = true; }
                                if (caseType == "mixed" && processMixedRoman) { needsFixing = true; makeUppercase = true; }
                            }
                            
                            if (needsFixing) {
                                var fixed = fixRomanString(currentWord, makeUppercase);
                                if (fixed.cyrillicCount > 0 || fixed.caseChangeCount > 0) {
                                    newText += fixed.text;
                                    
                                    if (fixed.cyrillicCount > 0) {
                                        stats.cyrillicReplacements.total += fixed.cyrillicCount;
                                        if (sectionType == "main") stats.cyrillicReplacements.main += fixed.cyrillicCount;
                                        else if (sectionType == "notes") stats.cyrillicReplacements.notes += fixed.cyrillicCount;
                                        else if (sectionType == "comments") stats.cyrillicReplacements.comments += fixed.cyrillicCount;
                                    }
                                    
                                    if (fixed.caseChangeCount > 0) {
                                        stats.caseChanges.total += fixed.caseChangeCount;
                                        if (sectionType == "main") stats.caseChanges.main += fixed.caseChangeCount;
                                        else if (sectionType == "notes") stats.caseChanges.notes += fixed.caseChangeCount;
                                        else if (sectionType == "comments") stats.caseChanges.comments += fixed.caseChangeCount;
                                    }
                                    
                                    nodeChanged = true;
                                    
                                    var parent = node.parentNode;
                                    while (parent) {
                                        if (parent.nodeType == 1 && (parent.tagName == "P" || parent.tagName == "DIV")) {
                                            var id = parent.tagName + "_" + parent.sourceIndex;
                                            var found = false;
                                            for (var k = 0; k < changedParagraphsMap.length; k++) {
                                                if (changedParagraphsMap[k] == id) {
                                                    found = true;
                                                    break;
                                                }
                                            }
                                            if (!found) {
                                                changedParagraphsMap[changedParagraphsMap.length] = id;
                                                stats.changedParagraphs.total++;
                                                if (sectionType == "main") stats.changedParagraphs.main++;
                                                else if (sectionType == "notes") stats.changedParagraphs.notes++;
                                                else if (sectionType == "comments") stats.changedParagraphs.comments++;
                                            }
                                            break;
                                        }
                                        parent = parent.parentNode;
                                    }
                                } else {
                                    newText += currentWord;
                                }
                            } else {
                                newText += currentWord;
                            }
                        } else {
                            newText += currentWord;
                        }
                        
                        wordStart = -1;
                        currentWord = "";
                    }
                    
                    if (i < text.length) {
                        newText += c;
                    }
                }
            }
            
            if (nodeChanged) {
                node.nodeValue = newText;
            }
        }
        
        // Вычисляем время выполнения
        var endTime = new Date();
        var executionTime = (endTime - startTime) / 1000;
        
        // Показываем итоговую статистику
        if (showStatistics && (stats.cyrillicReplacements.total > 0 || stats.caseChanges.total > 0 || stats.ignoredWords > 0)) {
            var statMessage = "";
            statMessage += scriptName + "\n";
            statMessage += "ver. " + version + "\n";
            statMessage += "---------------------------\n\n";
            
            if (stats.cyrillicReplacements.total > 0) {
                statMessage += "Всего исправлено:\n";
                statMessage += "Ошибок рус-лат в римских числах - " + stats.cyrillicReplacements.total + "\n";
                statMessage += "Исправлено абзацев - " + stats.changedParagraphs.total + "\n\n";
                
                statMessage += "Из них:\n";
                statMessage += "- В основном разделе замен - " + stats.cyrillicReplacements.main + "\n";
                statMessage += "- В основном разделе исправлено абзацев - " + stats.changedParagraphs.main + "\n";
                
                if (processNotesSection && stats.hasNotes) {
                    statMessage += "- В разделе сносок (примечаний) замен - " + stats.cyrillicReplacements.notes + "\n";
                    if (stats.changedParagraphs.notes > 0) {
                        statMessage += "- В разделе сносок (примечаний) исправлено абзацев - " + stats.changedParagraphs.notes + "\n";
                    }
                }
                if (processCommentsSection && stats.hasComments) {
                    statMessage += "- В разделе комментариев замен - " + stats.cyrillicReplacements.comments + "\n";
                    if (stats.changedParagraphs.comments > 0) {
                        statMessage += "- В разделе комментариев исправлено абзацев - " + stats.changedParagraphs.comments + "\n";
                    }
                }
                statMessage += "\n";
            }
            
            if (stats.caseChanges.total > 0) {
                statMessage += "Изменений регистра в римских числах - " + stats.caseChanges.total + "\n";
                statMessage += "- В основном разделе - " + stats.caseChanges.main + "\n";
                if (processNotesSection && stats.hasNotes) {
                    statMessage += "- В разделе сносок (примечаний) - " + stats.caseChanges.notes + "\n";
                }
                if (processCommentsSection && stats.hasComments) {
                    statMessage += "- В разделе комментариев - " + stats.caseChanges.comments + "\n";
                }
                statMessage += "\n";
            }
            
            if (stats.ignoredWords > 0) {
                statMessage += "✓ Проигнорировано слов (по списку): " + stats.ignoredWords + "\n\n";
            }
            
            statMessage += "---------------------------\n";
            statMessage += "Настройки обработки:\n";
            statMessage += "Обработка раздела сносок (примечаний): " + (processNotesSection ? "ДА" : "НЕТ") + "\n";
            statMessage += "Обработка раздела комментариев: " + (processCommentsSection ? "ДА" : "НЕТ") + "\n\n";
            statMessage += "Исправлять строчные кириллические: " + (processSmallCyrRoman ? "ДА" : "НЕТ") + "\n";
            statMessage += "Делать все римские ЗАГЛАВНЫМИ: " + (processSmallRoman ? "ДА" : "НЕТ") + "\n";
            statMessage += "Исправлять смешанный регистр: " + (processMixedRoman ? "ДА" : "НЕТ") + "\n\n";
            
            statMessage += "✓ Время обработки: " + executionTime.toFixed(3) + " сек\n";
            
            MsgBox(statMessage);
        }
        
    } catch(e) {
        MsgBox(scriptName + "\nver. " + version + "\n---------------------------\n\n✗ ОШИБКА: " + e.message);
    }
    
    window.external.EndUndoUnit(document);
}

// Вспомогательные функции
if (typeof AskYesNo == "undefined") {
    function AskYesNo(message) {
        return window.external.AskYesNo(message);
    }
}

if (typeof MsgBox == "undefined") {
    function MsgBox(message) {
        window.external.MsgBox(message);
    }
}
