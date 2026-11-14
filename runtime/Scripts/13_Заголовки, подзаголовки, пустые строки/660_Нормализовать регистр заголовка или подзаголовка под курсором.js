// Скрипт "Нормализовать регистр заголовка или подзаголовка под курсором" для редактора FBE
// version 2.0
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Основано на скрипте "Нормализовать регистр заголовков и подзаголовков.js" (версии 2.4)

// Скрипт предназначен для автоматической нормализации регистра заголовка и подзаголовка
// в документах fb2, на котором находится курсор или в котором есть выделение.

// ОСНОВНЫЕ ВОЗМОЖНОСТИ СКРИПТА:
// 1. Определение заголовка или подзаголовка под курсором/выделением
// 2. Преобразование регистра с учетом:
//    - Начала предложений (после .!?…)
//    - Римских цифр
//    - Аббревиатур из встроенного списка
//    - Сохранения тэгов форматирования, сносок внутри заголовков
// 3. Настройки обработки:
//    - Обработка римских цифр и аббревиатур без изменений (опционально)
//    - Тихий режим (без окон) или обычный режим с подробной статистикой
// 4. Поддержка отмены действий (Ctrl+Z)

// version 2.0, 20.01.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Нормализовать регистр заголовка или подзаголовка под курсором";
    var version = "2.0";
    
    // ========== НАСТРОЙКИ СКРИПТА ==========
    // Можно менять значения (0 - нет, 1 - да)
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 0; // Измените на 1 для показа статистики
    
    // Обработка римских цифр
    var keepRomanNumerals = 1; // 0 - преобразовывать в нижний регистр, 1 - оставить как есть
    
    // Обработка аббревиатур из списка
    var keepAbbreviations = 1; // 0 - преобразовывать в нижний регистр, 1 - оставить как есть
    
    // Знаки препинания, после которых начинается новое предложение (заглавная буква)
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
        "LXXXI","LXXXII","LXXXIII","LXXXIV","LXXXV","LXXXVI","LXVII","LXXXVIII","LXXXIX","XC",
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
    
    // Функция для получения чистого текста из элемента (без тегов)
    function getCleanText(element) {
        var text = "";
        if (element.nodeType == 3) { // Текстовый узел
            text = element.nodeValue;
        } else if (element.nodeType == 1) { // Элемент
            for (var i = 0; i < element.childNodes.length; i++) {
                text += getCleanText(element.childNodes[i]);
            }
        }
        return text;
    }
    
    // Функция для получения текста с сохранением структуры абзацев
    function getTextWithStructure(element) {
        var result = [];
        
        function processNode(node) {
            if (node.nodeType == 1) { // Элемент
                // Если это абзац P, начинаем новый параграф
                if (node.nodeName == "P") {
                    var paragraphText = "";
                    // Собираем текст из всех дочерних узлов
                    for (var i = 0; i < node.childNodes.length; i++) {
                        paragraphText += getCleanText(node.childNodes[i]);
                    }
                    
                    // Добавляем только если текст не пустой
                    if (paragraphText && trimStr(paragraphText) !== "") {
                        result.push(paragraphText);
                    }
                } else {
                    // Обрабатываем содержимое других элементов
                    for (var i = 0; i < node.childNodes.length; i++) {
                        processNode(node.childNodes[i]);
                    }
                }
            }
        }
        
        processNode(element);
        
        // Если не нашли абзацев P, берем весь текст как один
        if (result.length === 0) {
            var flatText = getCleanText(element);
            if (flatText && trimStr(flatText) !== "") {
                result = [flatText];
            }
        }
        
        return result;
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
    
    // Функция для расчета процента заглавных букв в текста
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
                    var wordIsRoman = isRomanNumeral(currentWord);
                    var wordIsAbbrev = isAbbreviation(currentWord);
                    
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
                        // Обычное слово
                        if (sentenceStart) {
                            processedWord = currentWord.charAt(0).toUpperCase() + 
                                            currentWord.substr(1).toLowerCase();
                        } else {
                            processedWord = currentWord.toLowerCase();
                        }
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
            var wordIsRoman = isRomanNumeral(currentWord);
            var wordIsAbbrev = isAbbreviation(currentWord);
            
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
                
                // Проверяем только непосредственного предыдущего соседа
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
                    } else if (prevSibling.nodeType == 1) {
                        // Если предыдущий элемент - не текстовый узел,
                        // проверяем, не содержит ли он не-пробельный текст
                        var prevElementText = getCleanText(prevSibling);
                        if (prevElementText && trimStr(prevElementText) !== "") {
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
    
    // Функция для поиска элемента под курсором/выделением
    function findElementUnderCursorOrSelection() {
        // Получаем текущий выбор
        var sel = document.selection;
        if (!sel) {
            return null;
        }
        
        var range = null;
        try {
            range = sel.createRange();
        } catch(e) {
            return null;
        }
        
        if (!range) {
            return null;
        }
        
        // Получаем родительский элемент
        var parentElement = range.parentElement();
        if (!parentElement) {
            return null;
        }
        
        // Функция для проверки, является ли элемент заголовком или подзаголовком
        function isTitleOrSubtitle(element) {
            if (!element || element.nodeType != 1) return false;
            
            // Проверяем заголовки (div class="title")
            if (element.nodeName == "DIV" && element.className == "title") {
                return true;
            }
            
            // Проверяем подзаголовки (p class="subtitle")
            if (element.nodeName == "P" && element.className == "subtitle") {
                return true;
            }
            
            return false;
        }
        
        // Проверяем сам элемент и всех его родителей
        var currentElement = parentElement;
        while (currentElement && currentElement != document.body) {
            if (isTitleOrSubtitle(currentElement)) {
                var isBodyTitle = false;
                if (currentElement.nodeName == "DIV" && currentElement.className == "title") {
                    var parentNode = currentElement.parentNode;
                    if (parentNode && parentNode.nodeName == "DIV" && parentNode.className == "body") {
                        isBodyTitle = true;
                    }
                }
                return {
                    element: currentElement,
                    type: (currentElement.className == "title" ? "title" : "subtitle"),
                    isBodyTitle: isBodyTitle
                };
            }
            currentElement = currentElement.parentNode;
        }
        
        return null;
    }
    
    // Функция для определения типа body (основной, примечания, комментарии)
    function getBodyType(element) {
        var currentElement = element;
        while (currentElement && currentElement != document.body) {
            if (currentElement.nodeName == "DIV" && currentElement.className == "body") {
                var fbname = currentElement.getAttribute("fbname") || "";
                return (fbname == "" ? "main" : (fbname == "notes" ? "notes" : (fbname == "comments" ? "comments" : "other")));
            }
            currentElement = currentElement.parentNode;
        }
        return "unknown";
    }
    
    // Функция для форматирования текста для отображения (без нумерации)
    function formatTextSimple(textArray, maxLength) {
        if (!textArray || textArray.length === 0) return "";
        
        var result = "";
        for (var i = 0; i < textArray.length; i++) {
            var line = textArray[i];
            if (line.length > maxLength) {
                line = line.substring(0, maxLength) + "...";
            }
            result += line + "\n";
        }
        return result;
    }
    
    // ========== ОСНОВНАЯ ЛОГИКА ==========
    
    // Статистика
    var stats = {
        foundElement: null,
        elementType: "", // "title" или "subtitle"
        isBodyTitle: false,
        bodyType: "",
        text: "",
        textArray: [], // Текст с сохранением структуры абзацев
        percentUpperCase: 0,
        changed: false
    };
    
    // 1. Находим элемент под курсором/выделением
    stats.foundElement = findElementUnderCursorOrSelection();
    
    if (!stats.foundElement) {
        // ВСЕГДА показываем сообщение об ошибке, даже в тихом режиме
        MsgBox(scriptName + "\nver. " + version + "\n\nКурсор/выделение не находится в заголовке или подзаголовке!\n\n" +
               "Поместите курсор в заголовок (DIV class='title') или подзаголовок (P class='subtitle'), " +
               "или сделайте в них выделение, и запустите скрипт снова.");
        return;
    }
    
    // Определяем тип body
    stats.bodyType = getBodyType(stats.foundElement.element);
    stats.elementType = stats.foundElement.type;
    stats.isBodyTitle = stats.foundElement.isBodyTitle;
    
    // Получаем текст элемента с сохранением структуры
    stats.textArray = getTextWithStructure(stats.foundElement.element);
    if (stats.textArray.length === 0) {
        if (showStatistics == 1) {
            MsgBox(scriptName + "\nver. " + version + "\n\nЗаголовок или подзаголовок пуст!");
        } else {
            MsgBox(scriptName + "\nver. " + version + "\n\nЗаголовок или подзаголовок пуст!");
        }
        return;
    }
    
    // Получаем плоский текст для анализа
    stats.text = "";
    for (var i = 0; i < stats.textArray.length; i++) {
        stats.text += stats.textArray[i];
    }
    
    // Рассчитываем процент заглавных букв
    stats.percentUpperCase = calculateUpperCasePercent(stats.text);
    
    // Если 0% заглавных букв - сразу показываем сообщение и выходим
    if (stats.percentUpperCase === 0) {
        var noChangesMsg = scriptName + "\nver. " + version + "\n\n" +
                          "ИЗМЕНЕНИЙ НЕ ТРЕБУЕТСЯ!\n\n" +
                          (stats.elementType == "title" ? "Заголовок" : "Подзаголовок") + 
                          " уже содержит правильный регистр.";
        
        if (showStatistics == 1) {
            noChangesMsg += "\n\nЗаглавных букв: 0%";
        }
        
        noChangesMsg += "\n==============================";
        
        MsgBox(noChangesMsg);
        return;
    }
    
    // В тихом режиме пропускаем ВСЕ окна и сразу обрабатываем
    if (showStatistics == 1) {
        // Выводим информацию о найденном элементе
        var elementInfo = scriptName + "\nver. " + version + "\n\n" +
                         "НАЙДЕН ЭЛЕМЕНТ:\n" +
                         "=========================\n";
        
        if (stats.elementType == "title") {
            elementInfo += "Тип: ЗАГОЛОВОК";
            if (stats.isBodyTitle) {
                elementInfo += " (body)";
            }
            elementInfo += "\n";
        } else {
            elementInfo += "Тип: ПОДЗАГОЛОВОК\n";
        }
        
        elementInfo += "Раздел: ";
        if (stats.bodyType == "main") {
            elementInfo += "основной текст";
        } else if (stats.bodyType == "notes") {
            elementInfo += "сноски (примечания)";
        } else if (stats.bodyType == "comments") {
            elementInfo += "комментарии";
        } else {
            elementInfo += "другой";
        }
        
        elementInfo += "\n";
        elementInfo += "Заглавных букв: " + stats.percentUpperCase + "%\n\n";
        
        // Отображаем текст с сохранением структуры (для заголовков с несколькими абзацами)
        if (stats.elementType == "title" && stats.textArray.length > 1) {
            elementInfo += "ТЕКСТ (с сохранением структуры):\n\n" +  // Добавляем пустую строку
                          formatTextSimple(stats.textArray, 150) + "\n";
        } else {
            // Для подзаголовков или одноабзацных заголовков - просто текст с пустой строкой
            elementInfo += "ТЕКСТ:\n\n" + formatTextSimple(stats.textArray, 150) + "\n";
        }
        
        elementInfo += "Настройки:\n" +
                      "- Римские цифры: " + (keepRomanNumerals == 1 ? "не изменять" : "изменять") + "\n" +
                      "- Аббревиатуры: " + (keepAbbreviations == 1 ? "не изменять" : "изменять") + "\n\n";
        
        // Запрос на подтверждение
        elementInfo += "Применить нормализацию регистра к этому элементу?";
        
        if (!AskYesNo(elementInfo)) {
            MsgBox(scriptName + "\nver. " + version + "\n\nОбработка отменена пользователем.");
            return;
        }
    }
    
    // Запускаем таймер после подтверждения (или сразу в тихом режиме)
    var Ts = new Date().getTime();
    
    // Применяем изменения
    window.external.BeginUndoUnit(document, scriptName);
    
    stats.changed = replaceTextInElementRecursive(stats.foundElement.element);
    
    window.external.EndUndoUnit(document);
    
    // В тихом режиме ВООБЩЕ ничего не выводим
    if (showStatistics == 0) {
        return; // Просто выходим без сообщений
    }
    
    // Только в режиме с статистикой показываем результат
    var Tf = new Date().getTime();
    var timeSec = Math.round((Tf - Ts) / 10) / 100;
    
    // Получаем новый текст с сохранением структуры
    var newTextArray = getTextWithStructure(stats.foundElement.element);
    
    if (stats.changed) {
        var resultMsg = scriptName + "\nver. " + version + "\n\n" +
                       "РЕГИСТР ЗАГОЛОВКА УСПЕШНО НОРМАЛИЗОВАН!\n\n" +
                       "СРАВНЕНИЕ (до / после):\n" +
                       "=========================\n";
        
        if (stats.elementType == "title" && stats.textArray.length > 1) {
            // Для многоабзацных заголовков
            resultMsg += "• БЫЛО: (абзацев - " + stats.textArray.length + "):\n\n" +
                        formatTextSimple(stats.textArray, 150) + "\n";
            
            resultMsg += "• СТАЛО: (абзацев - " + newTextArray.length + "):\n\n" +
                        formatTextSimple(newTextArray, 150);
        } else {
            // Для подзаголовков или одноабзацных заголовков
            resultMsg += "БЫЛО:\n" + 
                        (stats.textArray[0].length > 150 ? stats.textArray[0].substring(0, 150) + "..." : stats.textArray[0]) + 
                        "\n\n";
            
            resultMsg += "СТАЛО:\n" + 
                        (newTextArray[0].length > 150 ? newTextArray[0].substring(0, 150) + "..." : newTextArray[0]) +
                        "\n";
        }
        
        resultMsg += "==============================\n\n" +
                    "Время выполнения: " + timeSec + " сек\n\n" +
                    "Примечание: Проверьте написание названий и имен собственных после нормализации!";
        
        MsgBox(resultMsg);
    } else {
        var resultMsg = scriptName + "\nver. " + version + "\n\n" +
                       "ИЗМЕНЕНИЙ НЕ ТРЕБУЕТСЯ!\n\n" +
                       "Заголовок уже содержит правильный регистр.\n\n" +
                       "Время выполнения: " + timeSec + " сек\n" +
                       "==============================";
        
        MsgBox(resultMsg);
    }
}
