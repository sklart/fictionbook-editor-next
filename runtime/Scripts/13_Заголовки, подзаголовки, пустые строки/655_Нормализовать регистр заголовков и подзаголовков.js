// Скрипт "Нормализовать регистр заголовков и подзаголовков" для редактора FBE
// version 2.7
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для автоматической нормализации регистра заголовков и подзаголовков в документах fb2.

// ОСНОВНЫЕ ВОЗМОЖНОСТИ СКРИПТА:
// 1. Анализ документа на наличие заголовков и подзаголовков, написанных заглавными буквами.
// 2. Преобразование регистра с учетом:
//    - Начала предложений (после .!?…) - заглавные буквы после типичных концов предложений.
//    - Римских цифр (остаются без изменений).
//    - Аббревиатур из встроенного списка (остаются без изменений).
//    - Сохранения тэгов форматирования, сносок внутри заголовков.
//    - Сохранения смешанного регистра (не изменяет слова, которые не полностью заглавные).
// Каждый абзац в заголовке проверяется отдельно на превышение порога заглавных букв.
// Слова с обычным регистром (не полностью заглавные) сохраняются без изменений.
// Скрипт изменяет ТОЛЬКО полностью заглавные слова.
// 3. Настройки обработки:
//    - Режим работы: обычный (1) или тихий (0)
//    - Обработка заголовка основного body (опционально)
//    - Обработка разделов сносок и комментариев (опционально)
//    - Сохранение римских цифр и аббревиатур без изменений (опционально)
//    - Порог анализа (произвольный процент заглавных букв в заголовках и подзаголовках для обработки)
// 4. Поддержка отмены действий (Ctrl+Z)

// version 2.7, 25.01.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Нормализовать регистр заголовков и подзаголовков";
    var version = "2.7";
    
    // ========== НАСТРОЙКИ СКРИПТА ==========
    // Можно менять значения (0 - нет, 1 - да)
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 0; // Измените на 0 для тихого режима
    
    // Процент заглавных букв для анализа "частично заглавных" заголовков
    var percentThreshold = 80; // 80% по умолчанию
    
    // Обработка римских цифр
    var keepRomanNumerals = 1; // 0 - преобразовывать в нижний регистр, 1 - оставить как есть
    
    // Обработка аббревиатур из списка
    var keepAbbreviations = 1; // 0 - преобразовывать в нижний регистр, 1 - оставить как есть
    
    // Обрабатывать заголовки и подзаголовки в разделе сносок (примечаний)
    var processNotesSection = 0; // 0 - нет, 1 - да
    
    // Обрабатывать заголовки и подзаголовки в разделе комментариев
    var processCommentsSection = 0; // 0 - нет, 1 - да
    
    // Обрабатывать заголовок основного body
    var processFirstTitle = 1; // 0 - нет, 1 - да
    
    // Знаки препинания, после которых начинается новое предложение (заглавная буква)
    // Можно добавлять/убирать знаки при необходимости
    // U+2026 - многоточие как один символ, ... - три точки
    var sentenceEndChars = ".!?…";
    
    // ========== СПИСКИ ДЛЯ ПРОВЕРКИ ==========
    
    // Список римских цифр (I - MMMM) - делаем как массив для простой проверки
    var romanNumeralsArray = [
        "I","II","III","IV","V","VI","VII","VIII","IX","X",
        "XI","XII","XIII","XIV","XV","XVI","XVII","XVIII","XIX","XX",
        "XXI","XXII","XXIII","XXIV","XXV","XXVI","XXVII","XXVIII","XXIX","XXX",
        "XXXI","XXXII","XXXIII","XXXIV","XXXV","XXXVI","XXXVII","XXXVIII","XXXIX","XL",
        "XLI","XLII","XLIII","XLIV","XLV","XLVI","XLVII","XLVIII","XLIX","L",
        "LI","LII","LIII","LIV","LV","LVI","LVII","LVIII","LIX","LX",
        "LXI","LXII","LXIII","LXIV","LXV","LXVI","LXVII","LXVIII","LXIX","LXX",
        "LXXI","LXXII","LXXIII","LXXIV","LXXV","LXXVI","LXXVII","LXXVIII","LXXIX","LXXX",
        "LXXXI","LXXXII","LXXXIII","LXXXIV","LXXXV","LXXXVI","LXXXVII","LXXXVIII","LXXXIX","XC",
        "XCI","XCII","XCIII","XCIV","XCV","XCVI","XCVII","XCVIII","XCIX","C"
    ];
    
    // Список русских аббревиатур - отсортирован по алфавиту
    var russianAbbreviations = [
        "АБС","АЗЛК","АЗС","АКБ","АКМ","АНБ","АССР","АСТ","АЭС","БАД","БАМ","ББК","БИК","БМП","БП","БПЛА","БССР","БТР",
        "ВАЗ","ВАСХНИЛ","ВВП","ВВС","ВГИК","ВДВ","ВДНХ","ВИЧ","ВКС","ВЛКСМ","ВМС","ВМФ","ВНД","ВОЗ","ВПК","ВС","ВУЗ","ВЦИК","ВЦСПС","ВШЭ","ВЭД",
        "ГАЗ","ГАИ","ГДР","ГИБДД","ГК","ГЛОНАСС","ГОСТ","ГТО","ГУЛАГ","ГЭС",
        "ДВС","ДНК","ДОСААФ","ДЮСШ","ЕГЭ","ЕС","ЕЭС","ЖБИ","ЖК","ЖКХ","ЖКТ","ЖЭК",
        "ЗАГС","ЗАО","ЗИЛ","ЗКС","ЗРК",
        "ИБП","ИВЛ","ИГИЛ","ИЖС","ИИ","ИНН","ИП","ИФНС",
        "КАМАЗ","КАСКО","КГБ","КНДР","КНР","КоАП","КПД","КПП","КПРФ","КПСС","КТ",
        "ЛАТР","ЛГБТ","ЛДПР", "ЛПУ","ЛЭП",
        "МАГАТЭ","МАЗ","МБР","МВД","МВФ","МГИМО","МГУ","МДФ","МИД","МИСИ","МИФИ","МКАД","МКС","ММВБ","МО","МРТ","МТС","МФТИ","МФЦ","МЧС",
        "НАМИ","НАТО","НВП","НДС","НДФЛ","НИИ","НИОКР","НИИЦ","НКВД","НКО","НЛП","НПЗ","НПО","НШ","НЭП",
        "ОБСЕ","ОБХСС","ОБЭП","ОКАТО","ОКВЭД","ОКД","ОКП","ОКПО","ОМОН","ООН","ООО","ОПГ","ОСАГО","ОТК",
        "ПВО","ПВХ","ПДД","ПК","ПНД","ППС","ПТС","ПТУ","ПУЭ","ПФР",
        "РАМН","РАН","РБК","РВСН","РГБ","РЖД","РККА","РКН","РНК","РПГ","РПЦ","РСДРП","РСФСР","РУВД","РУДН","РФ","РЭБ","РЭР",
        "СВ","СВР","СВЧ","СЗ","СИЗ","СИЗО","СКА","СМЕРШ","СМИ","СНГ","СНиП","СНТ","СПбГУ","СС","ССО","ССР","СССР","СТС","США","СЭВ","СЭС",
        "ТАСС","ТВД","ТН","ТПУ","ТТН","ТТХ","ТЭЦ",
        "УАЗ","УВД","УДК","УЗИ","УЗО","УПК","УССР",
        "ФБР","ФМС","ФНС","ФРГ","ФСБ","ФСИН","ФСО","ФССП",
        "ХВ","ХЗ","ХЛ","ХО","ХР",
        "ЦЕРН","ЦК","ЦНС","ЦРУ","ЦСКА",
        "ЧК","ЧМ","ЧП",
        "ЭВМ","ЭКГ","ЭЭГ",
        "ЮАР","ЮВ","ЮЗ","ЮНЕСКО",
        "ЯБЧ","ЯВ","ЯМЗ","ЯО"
    ];
    
    // Список латинских аббревиатур - отсортирован по алфавиту
    var latinAbbreviations = [
        "BBC","BMW","CEO","CFO","CERN","CNN","COO","CTO","DOI","ESA","FIFA","GE","GM","GMC","HBO","HDMI","HP","IBM","ISBN","ISSN","KPI",
        "L","LAN","LCD","LED","LG","M","MTV","NASA","NATO","NBC","NEC","OLED","PC","R&D","S","SAP","TV","UAV","UN","UNESCO","USB","UEFA","VPN","VW","WAN","WHO","WI-FI","WTO","XL","XS","XXL","XXXL"
    ];
    
    // Объединяем оба списка в один для проверки
    var abbreviationsArray = russianAbbreviations.concat(latinAbbreviations);
    
    // Сортировка объединенного массива по алфавиту
    for (var i = 0; i < abbreviationsArray.length - 1; i++) {
        for (var j = i + 1; j < abbreviationsArray.length; j++) {
            if (abbreviationsArray[i] > abbreviationsArray[j]) {
                var temp = abbreviationsArray[i];
                abbreviationsArray[i] = abbreviationsArray[j];
                abbreviationsArray[j] = temp;
            }
        }
    }
    
    // Неразрывный пробел из настроек FBE
    var nbspEntity = "&nbsp;";
    var nbspChar = String.fromCharCode(160);
    try {
        nbspChar = window.external.GetNBSP();
        if (nbspChar.charCodeAt(0) != 160) {
            nbspEntity = nbspChar;
        }
    } catch(e) {
        nbspChar = String.fromCharCode(160);
        nbspEntity = "&nbsp;";
    }
    
    // ========== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ==========
    
    // Функция для проверки, является ли символ пробельным
    function isWhitespace(ch) {
        return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch.charCodeAt(0) == 160;
    }
    
    // Функция для ручного trim (аналог String.trim() для IE6)
    function trimStr(str) {
        var start = 0;
        var end = str.length - 1;
        
        // Находим первый не-пробельный символ
        while (start <= end && isWhitespace(str.charAt(start))) {
            start++;
        }
        
        // Находим последний не-пробельный символ
        while (end >= start && isWhitespace(str.charAt(end))) {
            end--;
        }
        
        if (start > end) {
            return "";
        }
        
        return str.substring(start, end + 1);
    }
    
    // Функция для получения чистого текста из элемента (без тегов) - возвращает массив текстов по абзацам
    function getCleanTextArray(element) {
        var paragraphs = [];
        var currentText = "";
        
        function collectText(node) {
            if (node.nodeType == 3) { // Текстовый узел
                currentText += node.nodeValue;
            } else if (node.nodeType == 1) { // Элемент
                // Если это элемент P внутри заголовка, сохраняем текущий текст и начинаем новый абзац
                if (node.nodeName == "P") {
                    if (currentText) {
                        paragraphs.push(currentText);
                        currentText = "";
                    }
                }
                
                // Рекурсивно обрабатываем дочерние узлы
                for (var i = 0; i < node.childNodes.length; i++) {
                    collectText(node.childNodes[i]);
                }
            }
        }
        
        collectText(element);
        
        // Добавляем последний собранный текст
        if (currentText) {
            paragraphs.push(currentText);
        }
        
        // Если нет абзацев P внутри заголовка, возвращаем весь текст как один абзац
        if (paragraphs.length === 0) {
            paragraphs.push(getCleanTextSimple(element));
        }
        
        return paragraphs;
    }
    
    // Функция для получения всего текста из элемента (без разбивки по абзацам)
    function getCleanTextSimple(element) {
        var text = "";
        if (element.nodeType == 3) { // Текстовый узел
            text = element.nodeValue;
        } else if (element.nodeType == 1) { // Элемент
            for (var i = 0; i < element.childNodes.length; i++) {
                text += getCleanTextSimple(element.childNodes[i]);
            }
        }
        return text;
    }
    
    // Функция для проверки, является ли символ буквой (кириллица или латиница)
    function isLetter(ch) {
        var code = ch.charCodeAt(0);
        return (code >= 1040 && code <= 1103) || // Кириллица
               (code == 1025 || code == 1105) || // Ёё
               (code >= 65 && code <= 90) ||     // Латинские A-Z
               (code >= 97 && code <= 122);      // Латинские a-z
    }
    
    // Функция для проверки, является ли символ заглавной буквой
    function isUpperCase(ch) {
        if (!isLetter(ch)) return false;
        var code = ch.charCodeAt(0);
        return (code >= 1040 && code <= 1071) || // Кириллица заглавные
               (code == 1025) ||                 // Ё заглавное
               (code >= 65 && code <= 90);       // Латинские заглавные
    }
    
    // Функция для проверки, является ли символ строчной буквой
    function isLowerCase(ch) {
        if (!isLetter(ch)) return false;
        var code = ch.charCodeAt(0);
        return (code >= 1072 && code <= 1103) || // Кириллица строчные
               (code == 1105) ||                 // ё строчное
               (code >= 97 && code <= 122);      // Латинские строчные
    }
    
    // Функция для проверки, является ли слово полностью заглавным
    function isAllUppercase(word) {
        if (!word) return false;
        
        var hasLetters = false;
        for (var i = 0; i < word.length; i++) {
            var ch = word.charAt(i);
            if (isLetter(ch)) {
                hasLetters = true;
                if (isLowerCase(ch)) {
                    return false; // Нашли строчную букву - слово не полностью заглавное
                }
            }
        }
        
        return hasLetters; // Возвращаем true только если есть хотя бы одна буква и все они заглавные
    }
    
    // Функция для проверки, является ли слово римской цифрой
    function isRomanNumeral(word) {
        if (!word) return false;
        var upperWord = word.toUpperCase();
        // Простой поиск в массиве
        for (var i = 0; i < romanNumeralsArray.length; i++) {
            if (romanNumeralsArray[i] === upperWord) {
                return true;
            }
        }
        return false;
    }
    
    // Функция для проверки, является ли слово аббревиатурой
    function isAbbreviation(word) {
        if (!word) return false;
        var upperWord = word.toUpperCase();
        // Простой поиск в массиве
        for (var i = 0; i < abbreviationsArray.length; i++) {
            if (abbreviationsArray[i] === upperWord) {
                return true;
            }
        }
        return false;
    }
    
    // Функция для расчета процента заглавных букв в тексте
    function calculateUpperCasePercent(text) {
        if (!text) return 0;
        
        var totalLetters = 0;
        var upperLetters = 0;
        
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            if (isLetter(ch)) {
                totalLetters++;
                if (isUpperCase(ch)) {
                    upperLetters++;
                }
            }
        }
        
        if (totalLetters == 0) return 0;
        return Math.round((upperLetters / totalLetters) * 100);
    }
    
    // Функция для проверки, нужно ли начинать новое предложение после этого символа
    function isSentenceEnd(ch, nextChars) {
        // Проверяем стандартные знаки
        if (sentenceEndChars.indexOf(ch) != -1) {
            return true;
        }
        
        // Проверяем многоточие из трех точек
        if (ch == '.' && nextChars && nextChars.length >= 2) {
            if (nextChars.charAt(0) == '.' && nextChars.charAt(1) == '.') {
                return true;
            }
        }
        
        return false;
    }
    
    // Функция для нормализации регистра текста
    function normalizeText(text, isFirstWord) {
        if (!text) return "";
        
        var result = "";
        var inWord = false;
        var currentWord = "";
        var sentenceStart = isFirstWord;
        var i = 0;
        
        while (i < text.length) {
            var ch = text.charAt(i);
            var nextChars = text.substr(i + 1, 2); // Следующие 2 символа для проверки многоточия
            
            if (isLetter(ch)) {
                if (!inWord) {
                    inWord = true;
                    currentWord = ch;
                } else {
                    currentWord += ch;
                }
                i++;
            } else {
                if (inWord) {
                    // Обрабатываем накопленное слово
                    var processedWord = "";
                    var wordIsAllUpper = isAllUppercase(currentWord);
                    var wordIsRoman = isRomanNumeral(currentWord);
                    var wordIsAbbrev = isAbbreviation(currentWord);
                    
                    if (wordIsAllUpper) {
                        // Слово полностью заглавное - обрабатываем его
                        if (wordIsRoman || wordIsAbbrev) {
                            // Проверяем настройки
                            if ((wordIsRoman && keepRomanNumerals == 1) || 
                                (wordIsAbbrev && keepAbbreviations == 1)) {
                                // Оставляем как есть (полностью заглавными)
                                processedWord = currentWord.toUpperCase();
                            } else {
                                // Приводим к нормальному регистру
                                if (sentenceStart) {
                                    // В начале предложения - первая буква заглавная, остальные строчные
                                    processedWord = currentWord.charAt(0).toUpperCase() + 
                                                    currentWord.substr(1).toLowerCase();
                                } else {
                                    // В середине предложения - все строчные
                                    processedWord = currentWord.toLowerCase();
                                }
                            }
                        } else {
                            // Обычное слово, полностью заглавное
                            if (sentenceStart) {
                                processedWord = currentWord.charAt(0).toUpperCase() + 
                                                currentWord.substr(1).toLowerCase();
                            } else {
                                processedWord = currentWord.toLowerCase();
                            }
                        }
                    } else {
                        // Слово не полностью заглавное - оставляем как есть
                        processedWord = currentWord;
                    }
                    
                    result += processedWord;
                    inWord = false;
                    currentWord = "";
                    sentenceStart = false;
                }
                
                // Добавляем не-буквенный символ
                result += ch;
                
                // Проверяем, не конец ли предложения
                if (isSentenceEnd(ch, nextChars)) {
                    sentenceStart = true;
                }
                
                i++;
            }
        }
        
        // Обрабатываем последнее слово, если есть
        if (inWord && currentWord) {
            var processedWord = "";
            var wordIsAllUpper = isAllUppercase(currentWord);
            var wordIsRoman = isRomanNumeral(currentWord);
            var wordIsAbbrev = isAbbreviation(currentWord);
            
            if (wordIsAllUpper) {
                if (wordIsRoman || wordIsAbbrev) {
                    if ((wordIsRoman && keepRomanNumerals == 1) || 
                        (wordIsAbbrev && keepAbbreviations == 1)) {
                        processedWord = currentWord.toUpperCase();
                    } else {
                        if (sentenceStart) {
                            processedWord = currentWord.charAt(0).toUpperCase() + 
                                            currentWord.substr(1).toLowerCase();
                        } else {
                            processedWord = currentWord.toLowerCase();
                        }
                    }
                } else {
                    if (sentenceStart) {
                        processedWord = currentWord.charAt(0).toUpperCase() + 
                                        currentWord.substr(1).toLowerCase();
                    } else {
                        processedWord = currentWord.toLowerCase();
                    }
                }
            } else {
                // Слово не полностью заглавное - оставляем как есть
                processedWord = currentWord;
            }
            
            result += processedWord;
        }
        
        return result;
    }
    
    // Функция для замены текста в элементе (обрабатывает каждый текстовый узел отдельно)
    function replaceTextInElementRecursive(element) {
        var changed = false;
        
        if (element.nodeType == 3) { // Текстовый узел
            var oldText = element.nodeValue;
            // Проверяем, есть ли в тексте буквы
            var hasLetters = false;
            for (var i = 0; i < oldText.length; i++) {
                if (isLetter(oldText.charAt(i))) {
                    hasLetters = true;
                    break;
                }
            }
            
            // Проверяем, не пустой ли текст (без trim)
            var textIsEmpty = true;
            for (var i = 0; i < oldText.length; i++) {
                if (!isWhitespace(oldText.charAt(i))) {
                    textIsEmpty = false;
                    break;
                }
            }
            
            if (hasLetters && !textIsEmpty) {
                // Определяем, нужно ли начинать с заглавной буквы
                // Для первого текстового узла в элементе - начинаем с заглавной
                var isFirstInElement = true;
                var prevSibling = element.previousSibling;
                while (prevSibling) {
                    if (prevSibling.nodeType == 3) {
                        // Проверяем, не пустой ли предыдущий текстовый узел
                        var prevTextIsEmpty = true;
                        for (var j = 0; j < prevSibling.nodeValue.length; j++) {
                            if (!isWhitespace(prevSibling.nodeValue.charAt(j))) {
                                prevTextIsEmpty = false;
                                break;
                            }
                        }
                        if (!prevTextIsEmpty) {
                            isFirstInElement = false;
                            break;
                        }
                    }
                    prevSibling = prevSibling.previousSibling;
                }
                
                var newText = normalizeText(oldText, isFirstInElement);
                if (oldText != newText) {
                    element.nodeValue = newText;
                    changed = true;
                }
            }
        } else if (element.nodeType == 1) { // Элемент
            // Рекурсивно обрабатываем все дочерние узлы
            for (var i = 0; i < element.childNodes.length; i++) {
                if (replaceTextInElementRecursive(element.childNodes[i])) {
                    changed = true;
                }
            }
        }
        
        return changed;
    }
    
    // Рекурсивная функция для поиска ВСЕХ элементов в DOM
    function findAllElements(parent, results) {
        if (!parent || !parent.childNodes) return;
        
        for (var i = 0; i < parent.childNodes.length; i++) {
            var node = parent.childNodes[i];
            
            if (node.nodeType == 1) { // Элемент
                // Проверяем заголовки (div class="title")
                if (node.nodeName == "DIV" && node.className == "title") {
                    results.push({element: node, type: "title", isBodyTitle: false});
                }
                // Проверяем подзаголовки (p class="subtitle")
                else if (node.nodeName == "P" && node.className == "subtitle") {
                    results.push({element: node, type: "subtitle", isBodyTitle: false});
                }
                
                // Рекурсивно ищем вложенные элементы
                findAllElements(node, results);
            }
        }
    }
    
    // Функция для поиска заголовков body
    function findBodyTitles(parent, results) {
        if (!parent || !parent.childNodes) return;
        
        // Ищем все body элементы
        for (var i = 0; i < parent.childNodes.length; i++) {
            var node = parent.childNodes[i];
            
            if (node.nodeType == 1 && node.nodeName == "DIV" && node.className == "body") {
                var fbname = node.getAttribute("fbname") || "";
                var bodyType = (fbname == "" ? "main" : (fbname == "notes" ? "notes" : (fbname == "comments" ? "comments" : "other")));
                
                // Ищем заголовок body (первый DIV с class="title" в body)
                var bodyChildren = node.childNodes;
                for (var j = 0; j < bodyChildren.length; j++) {
                    var child = bodyChildren[j];
                    if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "title") {
                        // Это заголовок body
                        results.push({
                            element: child,
                            type: "title",
                            isBodyTitle: true,
                            bodyType: bodyType,
                            fbname: fbname
                        });
                        break; // Нашли заголовок, выходим из цикла
                    }
                }
                
                // Рекурсивно ищем вложенные элементы (обычные заголовки и подзаголовки)
                findFilteredElements(node, results);
            }
        }
    }
    
    // Функция для поиска элементов с учетом настроек разделов
    function findFilteredElements(parent, results) {
        if (!parent || !parent.childNodes) return;
        
        for (var i = 0; i < parent.childNodes.length; i++) {
            var node = parent.childNodes[i];
            
            if (node.nodeType == 1) { // Элемент
                // Пропускаем заголовки body, так как они уже обработаны
                if (node.nodeName == "DIV" && node.className == "title") {
                    var parentNode = node.parentNode;
                    if (parentNode && parentNode.nodeName == "DIV" && parentNode.className == "body") {
                        continue; // Это заголовок body, пропускаем
                    }
                }
                
                // Проверяем, находится ли элемент в исключенных разделах
                var inExcludedSection = false;
                var parentBody = node;
                
                // Ищем родительский DIV class="body"
                while (parentBody && (parentBody.nodeName != "DIV" || parentBody.className != "body")) {
                    parentBody = parentBody.parentNode;
                }
                
                if (parentBody) {
                    var fbname = parentBody.getAttribute("fbname") || "";
                    
                    // Проверяем настройки для разделов сносок и комментариев
                    if (fbname == "notes" && processNotesSection == 0) {
                        inExcludedSection = true;
                    }
                    if (fbname == "comments" && processCommentsSection == 0) {
                        inExcludedSection = true;
                    }
                }
                
                if (!inExcludedSection) {
                    // Проверяем заголовки (div class="title")
                    if (node.nodeName == "DIV" && node.className == "title") {
                        results.push({element: node, type: "title", isBodyTitle: false});
                    }
                    // Проверяем подзаголовки (p class="subtitle")
                    else if (node.nodeName == "P" && node.className == "subtitle") {
                        results.push({element: node, type: "subtitle", isBodyTitle: false});
                    }
                }
                
                // Рекурсивно ищем вложенные элементы
                findFilteredElements(node, results);
            }
        }
    }
    
    // Функция для подсчета элементов по типо
    function countElementsByType(elements, type) {
        var count = 0;
        for (var i = 0; i < elements.length; i++) {
            if (elements[i].type == type) {
                count++;
            }
        }
        return count;
    }
    
    // Функция для анализа заголовка по абзацам
    function analyzeTitleByParagraphs(element) {
        var paragraphs = getCleanTextArray(element);
        var maxPercent = 0;
        var all100Percent = true;
        var hasHighPercentParagraph = false;
        
        for (var i = 0; i < paragraphs.length; i++) {
            var text = paragraphs[i];
            var percent = calculateUpperCasePercent(text);
            
            if (percent > maxPercent) {
                maxPercent = percent;
            }
            
            if (percent == 100) {
                hasHighPercentParagraph = true;
            } else {
                all100Percent = false;
            }
            
            if (percent >= percentThreshold) {
                hasHighPercentParagraph = true;
            }
        }
        
        return {
            maxPercent: maxPercent,
            all100Percent: all100Percent,
            hasHighPercentParagraph: hasHighPercentParagraph,
            paragraphCount: paragraphs.length
        };
    }
    
    // ========== ОСНОВНАЯ ЛОГИКА ==========
    
    // Статистика
    var stats = {
        allTitles: 0,
        allSubtitles: 0,
        allTitles100: 0,
        allSubtitles100: 0,
        allTitlesPartial: 0,
        allSubtitlesPartial: 0,
        
        // Новые статистики для многоабзацных заголовков
        multiParagraphTitles: 0,
        multiParagraphTitlesToProcess: 0,
        
        processTitles100: [],
        processTitlesPartial: [],
        processSubtitles100: [],
        processSubtitlesPartial: [],
        
        changedTitles100: 0,
        changedTitlesPartial: 0,
        changedSubtitles100: 0,
        changedSubtitlesPartial: 0,
        
        mainBodyTitleFound: false,
        mainBodyTitlePercent: 0
    };
    
    // Фаза 1: Анализ документа
    var body = document.getElementById("fbw_body");
    if (!body) {
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка: Не найден элемент body!");
        return;
    }
    
    // 1. Находим заголовки body
    var bodyTitleElements = [];
    findBodyTitles(body, bodyTitleElements);
    
    // 2. Находим все остальные элементы
    var allElements = [];
    findAllElements(body, allElements);
    
    // Объединяем результаты
    var allCombinedElements = bodyTitleElements.concat(allElements);
    
    // Удаляем дубликаты
    var uniqueElements = [];
    var seenElements = {};
    
    for (var i = 0; i < allCombinedElements.length; i++) {
        var item = allCombinedElements[i];
        var elementId = item.element.uniqueID || item.element.sourceIndex || i;
        
        if (!seenElements[elementId]) {
            seenElements[elementId] = true;
            uniqueElements.push(item);
        }
    }
    
    allElements = uniqueElements;
    
    stats.allTitles = countElementsByType(allElements, "title");
    stats.allSubtitles = countElementsByType(allElements, "subtitle");
    
    // Проверяем, есть ли вообще заголовки
    if (allElements.length === 0) {
        if (showStatistics == 1) {
            MsgBox(scriptName + "\nver. " + version + "\n\nЗаголовков или подзаголовков в документе не обнаружено!");
        }
        return;
    }
    
    // Расчет статистики для всех элементов
    for (var i = 0; i < allElements.length; i++) {
        var item = allElements[i];
        
        if (item.type == "title") {
            // Для заголовков анализируем по абзацам
            var analysis = analyzeTitleByParagraphs(item.element);
            
            if (analysis.paragraphCount > 1) {
                stats.multiParagraphTitles++;
            }
            
            if (analysis.all100Percent) {
                stats.allTitles100++;
            } else if (analysis.hasHighPercentParagraph) {
                stats.allTitlesPartial++;
            }
            
            // Для заголовков body отдельно запоминаем процент
            if (item.isBodyTitle && item.bodyType == "main") {
                stats.mainBodyTitleFound = true;
                stats.mainBodyTitlePercent = analysis.maxPercent;
            }
        } else {
            // Для подзаголовков - обычный анализ (всегда один абзац)
            var text = getCleanTextSimple(item.element);
            var percent = calculateUpperCasePercent(text);
            
            if (percent == 100) {
                stats.allSubtitles100++;
            } else if (percent >= percentThreshold) {
                stats.allSubtitlesPartial++;
            }
        }
    }
    
    // Фильтруем элементы согласно настройкам
    var filteredElements = [];
    for (var i = 0; i < allElements.length; i++) {
        var item = allElements[i];
        var shouldInclude = true;
        
        // Проверяем настройки
        if (item.isBodyTitle && item.bodyType == "main" && processFirstTitle == 0) {
            shouldInclude = false;
        }
        
        if (item.bodyType == "notes" && processNotesSection == 0) {
            shouldInclude = false;
        }
        
        if (item.bodyType == "comments" && processCommentsSection == 0) {
            shouldInclude = false;
        }
        
        if (shouldInclude) {
            filteredElements.push(item);
        }
    }
    
    // Анализ каждого элемента (только те, что подлежат обработке)
    for (var i = 0; i < filteredElements.length; i++) {
        var item = filteredElements[i];
        
        if (item.type == "title") {
            // Для заголовков анализируем по абзацам
            var analysis = analyzeTitleByParagraphs(item.element);
            
            if (analysis.all100Percent) {
                stats.processTitles100.push(item.element);
                if (analysis.paragraphCount > 1) {
                    stats.multiParagraphTitlesToProcess++;
                }
            } else if (analysis.hasHighPercentParagraph) {
                stats.processTitlesPartial.push(item.element);
                if (analysis.paragraphCount > 1) {
                    stats.multiParagraphTitlesToProcess++;
                }
            }
        } else {
            // Для подзаголовков - обычный анализ
            var text = getCleanTextSimple(item.element);
            var percent = calculateUpperCasePercent(text);
            
            if (percent == 100) {
                stats.processSubtitles100.push(item.element);
            } else if (percent >= percentThreshold) {
                stats.processSubtitlesPartial.push(item.element);
            }
        }
    }
    
    // Проверяем, есть ли вообще что обрабатывать
    var totalToProcess = stats.processTitles100.length + stats.processTitlesPartial.length + 
                         stats.processSubtitles100.length + stats.processSubtitlesPartial.length;
    
    if (totalToProcess === 0) {
        // В тихом режиме просто выходим, не показывая сообщений
        if (showStatistics == 1) {
            var noElementsMsg = scriptName + "\nver. " + version + "\n\n";
            
            if (stats.mainBodyTitleFound && processFirstTitle == 0) {
                noElementsMsg += "Заголовков или подзаголовков в основном разделе не обнаружено!\n\n";
                noElementsMsg += "Обнаружен заголовок основного body ";
                if (stats.mainBodyTitlePercent == 100) {
                    noElementsMsg += "(100% заглавных букв)";
                } else if (stats.mainBodyTitlePercent >= percentThreshold) {
                    noElementsMsg += "(>=" + percentThreshold + "% заглавных букв)";
                } else {
                    noElementsMsg += "(" + stats.mainBodyTitlePercent + "% заглавных букв)";
                }
                noElementsMsg += ", но обработка отключена в настройках.";
            } else if (stats.mainBodyTitleFound && processFirstTitle == 1) {
                noElementsMsg += "Заголовков или подзаголовков во всех разделах не обнаружено!\n\n";
                noElementsMsg += "Будет обработан только основной заголовок body.";
            } else {
                noElementsMsg += "Заголовков или подзаголовков в документе не обнаружено!";
            }
            
            MsgBox(noElementsMsg);
        }
        return;
    }
    
    // ========== РЕЖИМЫ РАБОТЫ: ОБЫЧНЫЙ ИЛИ ТИХИЙ ==========
    
    if (showStatistics == 1) {
        // ОБЫЧНЫЙ РЕЖИМ: показываем статистику и запрашиваем подтверждения
        
        // Вывод статистики анализа
        var analysisMsg = scriptName + "\nver. " + version + "\n\n" +
                         "АНАЛИЗ ДОКУМЕНТА:\n" +
                         "=========================\n" +
                         "• Всего заголовков (все разделы): " + stats.allTitles + "\n" +
                         "• Всего подзаголовков (все разделы): " + stats.allSubtitles + "\n";
        
        // Добавляем информацию о многоабзацных заголовках
        if (stats.multiParagraphTitles > 0) {
            analysisMsg += "• Заголовков из нескольких абзацев: " + stats.multiParagraphTitles + "\n";
        }
        
        analysisMsg += "=========================\n" +
                      "• Заголовков (100% заглавных букв): " + stats.allTitles100 + "\n" +
                      "• Подзаголовков (100% заглавных букв): " + stats.allSubtitles100 + "\n\n" +
                      "• Заголовков (>=" + percentThreshold + "% заглавных букв): " + stats.allTitlesPartial + "\n" +
                      "• Подзаголовков (>=" + percentThreshold + "% заглавных букв): " + stats.allSubtitlesPartial + "\n";
        
        // Добавляем информацию об основном заголовке body
        if (stats.mainBodyTitleFound) {
            analysisMsg += "=========================\n" +
                          "Основной заголовок body: ";
            if (stats.mainBodyTitlePercent == 100) {
                analysisMsg += "100% заглавных букв";
            } else if (stats.mainBodyTitlePercent >= percentThreshold) {
                analysisMsg += ">=" + percentThreshold + "% заглавных букв";
            } else {
                analysisMsg += stats.mainBodyTitlePercent + "% заглавных букв";
            }
            analysisMsg += "\n";
        }
        
        analysisMsg += "=========================\n\n" +
                      "Настройки:\n" +
                      "- Порог анализа: " + percentThreshold + "%\n" +
                      "- Римские цифры: " + (keepRomanNumerals == 1 ? "не изменять" : "изменять") + "\n" +
                      "- Аббревиатуры: " + (keepAbbreviations == 1 ? "не изменять" : "изменять") + "\n" +
                      "- Раздел сносок: " + (processNotesSection == 1 ? "обрабатывать" : "не обрабатывать") + "\n" +
                      "- Раздел комментариев: " + (processCommentsSection == 1 ? "обрабатывать" : "не обрабатывать") + "\n" +
                      "- Заголовок основного body: " + (processFirstTitle == 1 ? "обрабатывать" : "не обрабатывать") + "\n";
        
        // Добавляем информацию о том, что будет обработано
        analysisMsg += "=========================\n" +
                      "БУДУТ ОБРАБОТАНЫ (согласно настройкам):\n" +
                      "• Заголовков (100% заглавных букв): " + stats.processTitles100.length + "\n" +
                      "• Подзаголовков (100% заглавных букв): " + stats.processSubtitles100.length + "\n\n";
        
        if (stats.processTitlesPartial.length > 0 || stats.processSubtitlesPartial.length > 0) {
            analysisMsg += "• Заголовков (>=" + percentThreshold + "% заглавных букв): " + stats.processTitlesPartial.length + "\n" +
                          "• Подзаголовков (>=" + percentThreshold + "% заглавных букв): " + stats.processSubtitlesPartial.length + "\n";
        }
        
        // Добавляем информацию о многоабзацных заголовках для обработки
        if (stats.multiParagraphTitlesToProcess > 0) {
            analysisMsg += "• Из них заголовков из нескольких абзацев: " + stats.multiParagraphTitlesToProcess + "\n";
        }
        
        MsgBox(analysisMsg);
        
        // Запросы на подтверждение
        var confirmTitles100 = false;
        var confirmTitlesPartial = false;
        var confirmSubtitles100 = false;
        var confirmSubtitlesPartial = false;
        
        if (stats.processTitles100.length > 0) {
            var msg = "Изменить регистр ЗАГОЛОВКОВ где 100% заглавных букв?\n\n" +
                     "Количество: " + stats.processTitles100.length;
            if (stats.multiParagraphTitlesToProcess > 0) {
                msg += "\n(включая заголовки из нескольких абзацев)";
            }
            confirmTitles100 = AskYesNo(msg);
        }
        
        if (stats.processSubtitles100.length > 0) {
            confirmSubtitles100 = AskYesNo("Изменить регистр ПОДЗАГОЛОВКОВ где 100% заглавных букв?\n\nКоличество: " + stats.processSubtitles100.length);
        }
        
        if (stats.processTitlesPartial.length > 0) {
            var msg = "Изменить регистр ЗАГОЛОВКОВ где >=" + percentThreshold + "% заглавных букв?\n\n" +
                     "Количество: " + stats.processTitlesPartial.length;
            if (stats.multiParagraphTitlesToProcess > 0) {
                msg += "\n(включая заголовки из нескольких абзацев)";
            }
            confirmTitlesPartial = AskYesNo(msg);
        }
        
        if (stats.processSubtitlesPartial.length > 0) {
            confirmSubtitlesPartial = AskYesNo("Изменить регистр ПОДЗАГОЛОВКОВ где >=" + percentThreshold + "% заглавных букв?\n\nКоличество: " + stats.processSubtitlesPartial.length);
        }
        
        // Если ничего не выбрано, выходим
        if (!confirmTitles100 && !confirmSubtitles100 && !confirmTitlesPartial && !confirmSubtitlesPartial) {
            MsgBox(scriptName + "\nver. " + version + "\n\nОбработка отменена пользователем.");
            return;
        }
    } else {
        // ТИХИЙ РЕЖИМ: автоматически обрабатываем все найденные элементы без запросов
        var confirmTitles100 = (stats.processTitles100.length > 0);
        var confirmSubtitles100 = (stats.processSubtitles100.length > 0);
        var confirmTitlesPartial = (stats.processTitlesPartial.length > 0);
        var confirmSubtitlesPartial = (stats.processSubtitlesPartial.length > 0);
    }
    
    // Запускаем таймер после последнего подтверждения
    var Ts = new Date().getTime();
    
    // Фаза 2: Изменение документа
    window.external.BeginUndoUnit(document, scriptName);
    
    // Обработка заголовков (100%)
    if (confirmTitles100) {
        for (var i = 0; i < stats.processTitles100.length; i++) {
            var element = stats.processTitles100[i];
            if (replaceTextInElementRecursive(element)) {
                stats.changedTitles100++;
            }
        }
    }
    
    // Обработка подзаголовков (100%)
    if (confirmSubtitles100) {
        for (var i = 0; i < stats.processSubtitles100.length; i++) {
            var element = stats.processSubtitles100[i];
            if (replaceTextInElementRecursive(element)) {
                stats.changedSubtitles100++;
            }
        }
    }
    
    // Обработка заголовков (частичных)
    if (confirmTitlesPartial) {
        for (var i = 0; i < stats.processTitlesPartial.length; i++) {
            var element = stats.processTitlesPartial[i];
            if (replaceTextInElementRecursive(element)) {
                stats.changedTitlesPartial++;
            }
        }
    }
    
    // Обработка подзаголовков (частичных)
    if (confirmSubtitlesPartial) {
        for (var i = 0; i < stats.processSubtitlesPartial.length; i++) {
            var element = stats.processSubtitlesPartial[i];
            if (replaceTextInElementRecursive(element)) {
                stats.changedSubtitlesPartial++;
            }
        }
    }
    
    window.external.EndUndoUnit(document);
    
    // В тихом режиме просто завершаем работу без сообщений
    if (showStatistics == 1) {
        // Финальная статистика только для обычного режима
        var Tf = new Date().getTime();
        var timeSec = Math.round((Tf - Ts) / 10) / 100;
        
        var resultMsg = scriptName + "\nver. " + version + "\n\n" +
                       "РЕЗУЛЬТАТЫ ОБРАБОТКИ:\n" +
                       "=========================\n" +
                       "• Заголовков изменено (100% заглавных букв): " + stats.changedTitles100 + " из " + stats.processTitles100.length + "\n" +
                       "• Подзаголовков изменено (100% заглавных букв): " + stats.changedSubtitles100 + " из " + stats.processSubtitles100.length + "\n\n";
        
        if (stats.processTitlesPartial.length > 0 || stats.processSubtitlesPartial.length > 0) {
            resultMsg += "• Заголовков изменено (>=" + percentThreshold + "%): " + stats.changedTitlesPartial + " из " + stats.processTitlesPartial.length + "\n" +
                        "• Подзаголовков изменено (>=" + percentThreshold + "%): " + stats.changedSubtitlesPartial + " из " + stats.processSubtitlesPartial.length + "\n";
        }
        
        // Добавляем информацию о многоабзацных заголовках
        if (stats.multiParagraphTitlesToProcess > 0) {
            resultMsg += "• Из них обработано заголовков из нескольких абзацев: " + stats.multiParagraphTitlesToProcess + "\n";
        }
        
        resultMsg += "==============================\n\n" +
                    "Время выполнения: " + timeSec + " сек\n\n" +
                    "Проверьте написание названий и имен собственных после нормализации заголовков и подзаголовков!";
        
        MsgBox(resultMsg);
    }
}
