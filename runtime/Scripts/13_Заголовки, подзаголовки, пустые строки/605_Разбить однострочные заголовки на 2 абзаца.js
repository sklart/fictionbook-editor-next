// Скрипт "Разбить однострочные заголовки на 2 абзаца" для редактора FBE
// version 3.8
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для автоматического разделения однострочных заголовков на 2 строки в fb2 документах.
// Скрипт обрабатывает заголовки форматов: "Глава 1 Название", "Часть первая Текст".
// Также обрабатывает числительные: "1 Название", "XX Название", "Первая глава Текст".
// Скрипт обнаруживает и корректно разбивает на 2 строки заголовки с точками: "Глава 1. Название".
// Позволяет настраивать удаление/сохранение точек в конце 1 абзаца заголовков после разбивки.
// Обрабатывается только основной раздел документа.
// Тихий режим (без окон) или обычный режим с подробной статистикой.
// Поддержка отмены действий (Ctrl+Z)

// version 3.8, 21.12.2025
//======================================

function Run() {
    // Название и версия скрипта
    var scriptName = "Разбить однострочные заголовки на 2 абзаца";
    var scriptVersion = "3.8";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима
    
    // Обработка точек в конце первого абзаца при разбивке
    // 1 - оставлять как есть, 0 - удалять точку
    var keepDots = 0; // По умолчанию - 0 - удаляем точку
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    // Получаем неразрывный пробел из настроек FBE
    var nbspChar, nbspEntity;
    try {
        nbspChar = window.external.GetNBSP();
        if (nbspChar.charCodeAt(0) == 160) {
            nbspEntity = "&nbsp;";
        } else {
            nbspEntity = nbspChar;
        }
    } catch(e) {
        nbspChar = String.fromCharCode(160);
        nbspEntity = "&nbsp;";
    }

    // Маркеры заголовков (в нижнем регистре для сравнения)
    var markers = ['глава', 'часть', 'книга', 'том', 'раздел', 'chapter', 'part', 'book', 'volume', 'stave'];
    
    // Русские числительные (включаем различные формы в нижнем регистре)
    var russianNumerals = [
        // женский род
        'первая', 'вторая', 'третья', 'четвертая', 'пятая', 'шестая', 'седьмая', 
        'восьмая', 'девятая', 'десятая', 'одиннадцатая', 'двенадцатая',
        // мужской род
        'первый', 'второй', 'третий', 'четвертый', 'пятый', 'шестой', 'седьмой', 
        'восьмой', 'девятый', 'десятый', 'одиннадцатый', 'двенадцатый',
        // без окончания
        'один', 'два', 'три', 'четыре', 'пять', 'шесть', 'семь', 'восемь', 'девять', 'десять',
        'одиннадцать', 'двенадцать', 'тринадцать', 'четырнадцать', 'пятнадцать',
        'шестнадцать', 'семнадцать', 'восемнадцать', 'девятнадцать', 'двадцать'
    ];
    
    // Английские числительные (в нижнем регистре)
    var englishNumerals = [
        'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine', 'ten',
        'eleven', 'twelve', 'thirteen', 'fourteen', 'fifteen', 'sixteen', 'seventeen',
        'eighteen', 'nineteen', 'twenty', 
        'first', 'second', 'third', 'fourth', 'fifth', 'sixth', 'seventh', 'eighth', 'ninth', 'tenth'
    ];

    // Статистика
    var stats = {
        totalTitles: 0,
        titlesInNotes: 0,      // Заголовки в примечаниях (исключаем)
        canBeSplit: 0,         // Можно разбить
        withDotOrComma: 0,     // С точкой/запятой (но разрешено для разбивки)
        tooShort: 0,           // Слишком короткие
        noTextAfterNumeral: 0, // Нет текста после числительного
        processed: 0,          // Успешно обработано
        failed: 0              // Не удалось обработать
    };

    var allTitlesInfo = [];

    // Этап 1: Поиск и анализ (без учета времени)
    window.external.BeginUndoUnit(document, scriptName + " - поиск");

    var allDivs = document.getElementsByTagName('div');
    
    for (var i = 0; i < allDivs.length; i++) {
        var div = allDivs[i];
        
        // Проверяем, находится ли div в разделе примечаний
        var inNotesSection = false;
        var parent = div.parentNode;
        while (parent) {
            if (parent.className == 'body' && parent.nodeName == 'DIV') {
                var fbname = parent.getAttribute('fbname');
                if (fbname == 'notes' || fbname == 'comments') {
                    inNotesSection = true;
                    break;
                }
            }
            parent = parent.parentNode;
        }
        
        if (inNotesSection) {
            // Пропускаем заголовки в примечаниях
            continue;
        }
        
        // Проверяем, является ли div заголовком
        if (div.className && typeof div.className == 'string') {
            var classes = div.className.split(' ');
            var isTitle = false;
            for (var j = 0; j < classes.length; j++) {
                if (classes[j] == 'title') {
                    isTitle = true;
                    break;
                }
            }

            if (isTitle) {
                stats.totalTitles++;

                var pElements = div.getElementsByTagName('p');
                if (pElements.length > 0) {
                    var firstP = pElements[0];
                    
                    // Анализируем заголовок
                    var titleInfo = analyzeTitleElement(firstP, markers, russianNumerals, englishNumerals, nbspEntity);
                    
                    // ЗАРУБКА НА ПАМЯТЬ: записываем ВСЕ данные анализа
                    // даже те, которые не будут обрабатываться
                    allTitlesInfo.push({
                        div: div,
                        pElement: firstP,
                        info: titleInfo,
                        originalText: getTextFromElement(firstP) // сохраняем оригинальный текст для отладки
                    });
                }
            }
        }
    }

    window.external.EndUndoUnit(document);

    // Теперь подсчитываем статистику ПОСЛЕ анализа всех заголовков
    // Это гарантирует, что все данные собраны
    for (var k = 0; k < allTitlesInfo.length; k++) {
        var title = allTitlesInfo[k];
        var info = title.info;
        
        if (info.isSuitable) {
            stats.canBeSplit++;
        }
        
        if (info.hasDotOrComma) {
            stats.withDotOrComma++;
        }
        
        if (info.tooShort) {
            stats.tooShort++;
        }
        
        if (info.noTextAfterNumeral) {
            stats.noTextAfterNumeral++;
        }
    }

    // Подсчитываем заголовки в примечаниях отдельно
    for (i = 0; i < allDivs.length; i++) {
        var div = allDivs[i];
        if (div.className && typeof div.className == 'string') {
            var classes = div.className.split(' ');
            var isTitle = false;
            for (j = 0; j < classes.length; j++) {
                if (classes[j] == 'title') {
                    isTitle = true;
                    break;
                }
            }
            
            if (isTitle) {
                var inNotes = false;
                parent = div.parentNode;
                while (parent) {
                    if (parent.className == 'body' && parent.nodeName == 'DIV') {
                        var fbname = parent.getAttribute('fbname');
                        if (fbname == 'notes' || fbname == 'comments') {
                            inNotes = true;
                            break;
                        }
                    }
                    parent = parent.parentNode;
                }
                
                if (inNotes) {
                    stats.titlesInNotes++;
                }
            }
        }
    }

    // Проверяем тихий режим - показываем только если есть ошибка или нет заголовков для обработки
    if (showStatistics == 1) {
        if (stats.canBeSplit == 0) {
            var message = "---------------------------\n" +
                         scriptName + "\n" +
                         "ver. " + scriptVersion + "\n" +
                         "---------------------------\n\n" +
                         "Статистика анализа:\n\n" +
                         "Всего заголовков: " + (stats.totalTitles + stats.titlesInNotes) + "\n" +
                         "  • в основном разделе: " + stats.totalTitles + "\n" +
                         "  • в примечаниях: " + stats.titlesInNotes + " (исключены)\n\n" +
                         "В основном разделе:\n" +
                         "  • можно разбить: 0\n" +
                         "  • с точкой/запятой: " + stats.withDotOrComma + "\n" +
                         "  • слишком короткие: " + stats.tooShort + "\n" +
                         "  • без текста после числит.: " + stats.noTextAfterNumeral + "\n\n" +
                         "Нет заголовков для обработки.";
            MsgBox(message, "FBE скрипт");
            return;
        }

        var confirmMessage = "---------------------------\n" +
                            scriptName + "\n" +
                            "ver. " + scriptVersion + "\n" +
                            "---------------------------\n\n" +
                            "Статистика анализа:\n\n" +
                            "Всего заголовков: " + (stats.totalTitles + stats.titlesInNotes) + "\n" +
                            "  • в основном разделе: " + stats.totalTitles + "\n" +
                            "  • в примечаниях: " + stats.titlesInNotes + " (исключены)\n\n" +
                            "В основном разделе:\n" +
                            "  • можно разбить: " + stats.canBeSplit + "\n" +
                            "  • с точкой/запятой: " + stats.withDotOrComma + " (но разрешено для разбивки)\n" +
                            "  • слишком короткие: " + stats.tooShort + "\n" +
                            "  • без текста после числит.: " + stats.noTextAfterNumeral + "\n\n" +
                            "Обработать " + stats.canBeSplit + " заголовков?";
        
        if (!AskYesNo(confirmMessage)) {
            MsgBox(scriptName + "\nver. " + scriptVersion + "\n\nОбработка отменена пользователем.", "FBE скрипт");
            return;
        }
    } else {
        // Тихий режим - проверяем только если нет заголовков для обработки
        if (stats.canBeSplit == 0) {
            MsgBox(scriptName + "\nver. " + scriptVersion + "\n\nНет заголовков для обработки.", "FBE скрипт");
            return;
        }
        // В тихом режиме сразу переходим к обработке без подтверждения
    }

    // Обработка (запускаем таймер здесь)
    var startTime = new Date();
    window.external.BeginUndoUnit(document, scriptName + " - обработка");

    for (k = 0; k < allTitlesInfo.length; k++) {
        var title = allTitlesInfo[k];
        if (title.info.isSuitable) {
            if (processTitle(title.div, title.pElement, title.info, nbspEntity, keepDots)) {
                stats.processed++;
            } else {
                stats.failed++;
            }
        }
    }

    window.external.EndUndoUnit(document);

    var totalTime = new Date() - startTime;
    var timeSeconds = (totalTime / 1000).toFixed(2).replace('.', ',') + " сек";

    // Показываем результат только в обычном режиме
    if (showStatistics == 1) {
        var resultMessage = "---------------------------\n" +
                           scriptName + "\n" +
                           "ver. " + scriptVersion + "\n" +
                           "---------------------------\n\n";
        
        if (stats.processed == stats.canBeSplit && stats.failed == 0) {
            resultMessage += "✓ Успешно обработано: " + stats.processed + " заголовков\n\n";
        } else if (stats.failed > 0) {
            resultMessage += "✓ Обработано: " + stats.processed + " заголовков\n";
            resultMessage += "✗ Не удалось обработать: " + stats.failed + " заголовков\n\n";
        } else {
            resultMessage += "Обработано: " + stats.processed + " из " + stats.canBeSplit + " заголовков\n\n";
        }
        
        resultMessage += "Время обработки: " + timeSeconds + "\n" +
                        "---------------------------";

        MsgBox(resultMessage, "FBE скрипт");
    } else if (stats.failed > 0) {
        // В тихом режиме показываем только ошибки
        var errorMessage = "---------------------------\n" +
                          scriptName + "\n" +
                          "ver. " + scriptVersion + "\n" +
                          "---------------------------\n\n" +
                          "✗ Ошибки при обработке:\n\n" +
                          "Не удалось обработать: " + stats.failed + " заголовков из " + stats.canBeSplit + "\n\n" +
                          "Время обработки: " + timeSeconds + "\n" +
                          "---------------------------";
        MsgBox(errorMessage, "FBE скрипт");
    }
}

// ======================================
// КЛЮЧЕВАЯ ФУНКЦИЯ: АНАЛИЗ ЗАГОЛОВКА (исправленная для обработки точек)
// ======================================

function analyzeTitleElement(pElement, markers, russianNumerals, englishNumerals, nbspEntity) {
    var result = {
        isSuitable: false,
        hasDotOrComma: false,
        tooShort: false,
        noTextAfterNumeral: false,
        splitPosition: 0,
        childNodes: [],
        text: "",
        isMarkerNumeralPattern: false,  // Новый флаг для паттерна "маркер + числительное"
        dotAfterNumeral: false          // Точка после числительного
    };
    
    if (!pElement) return result;
    
    // 1. Получаем текст
    var text = getTextFromElement(pElement);
    var normalizedText = normalizeSpaces(text, nbspEntity);
    result.text = normalizedText;
    
    // Проверяем длину текста
    if (!normalizedText || normalizedText.replace(/\s/g, '').length < 3) {
        result.tooShort = true;
        return result;
    }
    
    // 2. Сохраняем узлы
    var childNodes = pElement.childNodes;
    for (var i = 0; i < childNodes.length; i++) {
        result.childNodes.push(childNodes[i].cloneNode(true));
    }
    
    // 3. Разбиваем на слова
    var words = splitIntoWords(normalizedText);
    if (words.length < 2) {
        result.tooShort = true;
        return result;
    }
    
    // Приводим слова к нижнему регистру для сравнения
    var firstWord = words[0];
    var firstWordLower = firstWord.toLowerCase();
    var secondWord = words[1];
    var secondWordLower = secondWord.toLowerCase();
    
    // 4. Проверяем первый случай: маркер + числительное
    var isMarker = false;
    for (i = 0; i < markers.length; i++) {
        if (firstWordLower == markers[i]) {
            isMarker = true;
            break;
        }
    }
    
    if (isMarker) {
        // Проверяем второе слово на числительное (удаляем знаки препинания для проверки)
        var secondWordClean = secondWord.replace(/[.,:;!?]/g, '');
        var isValidNumeral = checkIfNumeral(secondWordClean, russianNumerals, englishNumerals);
        
        if (!isValidNumeral) {
            return result;
        }
        
        // Устанавливаем флаг, что это паттерн "маркер + числительное"
        result.isMarkerNumeralPattern = true;
        
        // Ищем точку после числительного
        var secondWordPos = normalizedText.indexOf(secondWord);
        if (secondWordPos !== -1) {
            var posAfterWord = secondWordPos + secondWord.length;
            // Пропускаем пробелы
            while (posAfterWord < normalizedText.length && normalizedText.charAt(posAfterWord) == ' ') {
                posAfterWord++;
            }
            
            // Проверяем, есть ли точка сразу после числительного (с пробелом или без)
            if (posAfterWord < normalizedText.length) {
                var nextChar = normalizedText.charAt(posAfterWord);
                if (nextChar == '.') {
                    result.dotAfterNumeral = true;
                    result.hasDotOrComma = true; // Для статистики
                    
                    // Для паттерна "маркер + числительное + точка" разбиваем ПОСЛЕ точки
                    // Переходим за точку
                    posAfterWord++;
                    // Пропускаем пробелы после точки
                    while (posAfterWord < normalizedText.length && normalizedText.charAt(posAfterWord) == ' ') {
                        posAfterWord++;
                    }
                    
                    // Проверяем, есть ли текст после точки
                    if (posAfterWord >= normalizedText.length || normalizedText.substring(posAfterWord).replace(/\s/g, '').length === 0) {
                        result.noTextAfterNumeral = true;
                        return result;
                    }
                    
                    result.isSuitable = true;
                    result.splitPosition = posAfterWord;
                    return result;
                } else if (nextChar == ',') {
                    // Запятую не обрабатываем как допустимый разделитель
                    result.hasDotOrComma = true;
                    return result;
                }
            }
        }
        
        // Если точки нет, проверяем старый паттерн (без точки)
        // Проверяем, есть ли текст после числительного
        var posAfterNumeral = findPositionAfterSecondWord(normalizedText, words);
        if (posAfterNumeral >= normalizedText.length || normalizedText.substring(posAfterNumeral).replace(/\s/g, '').length === 0) {
            result.noTextAfterNumeral = true;
            return result;
        }
        
        // Проверяем, что после числительного нет точки/запятой (старый паттерн)
        var checkPos = posAfterNumeral;
        while (checkPos < normalizedText.length && normalizedText.charAt(checkPos) == ' ') {
            checkPos++;
        }
        if (checkPos < normalizedText.length) {
            var charAfterSpace = normalizedText.charAt(checkPos);
            if (charAfterSpace == '.' || charAfterSpace == ',') {
                result.hasDotOrComma = true;
                return result;
            }
        }
        
        result.isSuitable = true;
        result.splitPosition = posAfterNumeral;
        return result;
    }
    
    // 5. Проверяем второй случай: числительное в начале (ВКЛЮЧАЯ РИМСКИЕ С ТОЧКОЙ)
    // Сначала проверяем слово без знаков препинания
    var firstWordClean = firstWord.replace(/[.,:;!?]/g, '');
    
    if (isNumberWord(firstWordClean)) {  // Проверяем ЧИСТОЕ слово без точки/запятой
        // Проверяем второе слово - если это маркер, то это паттерн "XVIII том"
        var isSecondWordMarker = false;
        for (i = 0; i < markers.length; i++) {
            if (secondWordLower == markers[i]) {
                isSecondWordMarker = true;
                break;
            }
        }
        
        // Ищем точку в первом слове (для римских цифр с точкой)
        var hasDotInFirstWord = (firstWord.indexOf('.') !== -1);
        var hasCommaInFirstWord = (firstWord.indexOf(',') !== -1);
        
        if (hasCommaInFirstWord) {
            // Запятую в первом слове не обрабатываем
            result.hasDotOrComma = true;
            return result;
        }
        
        // Если точка в первом слове (например, "XX.") - это допустимо для числительных
        if (hasDotInFirstWord) {
            result.dotAfterNumeral = true;
            result.hasDotOrComma = true;
            
            // Находим позицию после точки в первом слове
            var dotPosInWord = firstWord.indexOf('.');
            var firstWordPos = normalizedText.indexOf(firstWord);
            var posAfterDot = firstWordPos + dotPosInWord + 1;
            
            // Пропускаем пробелы после точки
            while (posAfterDot < normalizedText.length && normalizedText.charAt(posAfterDot) == ' ') {
                posAfterDot++;
            }
            
            // Проверяем, есть ли текст после точки
            if (posAfterDot >= normalizedText.length || normalizedText.substring(posAfterDot).replace(/\s/g, '').length === 0) {
                result.noTextAfterNumeral = true;
                return result;
            }
            
            result.isSuitable = true;
            result.splitPosition = posAfterDot;
            return result;
        }
        
        // Если второе слово - маркер (паттерн "XVIII том Текст")
        if (isSecondWordMarker) {
            // Разбиваем после маркера (после второго слова)
            var posAfterSecondWord = findPositionAfterSecondWord(normalizedText, words);
            if (posAfterSecondWord >= normalizedText.length || normalizedText.substring(posAfterSecondWord).replace(/\s/g, '').length === 0) {
                result.noTextAfterNumeral = true;
                return result;
            }
            
            result.isSuitable = true;
            result.splitPosition = posAfterSecondWord;
            return result;
        }
        // Если второе слово НЕ маркер (паттерн "15 Текст")
        else {
            // Проверяем точку после первого слова
            var firstWordPos = normalizedText.indexOf(firstWord);
            if (firstWordPos !== -1) {
                var posAfterWord = firstWordPos + firstWord.length;
                while (posAfterWord < normalizedText.length && normalizedText.charAt(posAfterWord) == ' ') {
                    posAfterWord++;
                }
                if (posAfterWord < normalizedText.length) {
                    var nextChar = normalizedText.charAt(posAfterWord);
                    if (nextChar == ',') {
                        result.hasDotOrComma = true;
                        return result;
                    }
                }
            }
            
            // Разбиваем после числительного (после первого слова)
            var posAfterFirstWord = findPositionAfterFirstWord(normalizedText, words);
            if (posAfterFirstWord >= normalizedText.length || normalizedText.substring(posAfterFirstWord).replace(/\s/g, '').length === 0) {
                result.noTextAfterNumeral = true;
                return result;
            }
            
            result.isSuitable = true;
            result.splitPosition = posAfterFirstWord;
            return result;
        }
    }
    
    return result;
}

// ======================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ======================================

function checkIfNumeral(word, russianNumerals, englishNumerals) {
    var wordLower = word.toLowerCase();
    
    // Арабские цифры
    var allDigits = true;
    for (var i = 0; i < word.length; i++) {
        var ch = word.charAt(i);
        if (ch < '0' || ch > '9') {
            allDigits = false;
            break;
        }
    }
    if (allDigits) return true;
    
    // Римские цифры
    var romanChars = "IVXLCDM";
    var isRoman = true;
    for (i = 0; i < word.length; i++) {
        var found = false;
        for (var j = 0; j < romanChars.length; j++) {
            if (word.charAt(i).toUpperCase() == romanChars.charAt(j)) {
                found = true;
                break;
            }
        }
        if (!found) {
            isRoman = false;
            break;
        }
    }
    if (isRoman) return true;
    
    // Русские числительные (регистронезависимо)
    for (i = 0; i < russianNumerals.length; i++) {
        if (wordLower == russianNumerals[i]) {
            return true;
        }
    }
    
    // Английские числительные (регистронезависимо)
    for (i = 0; i < englishNumerals.length; i++) {
        if (wordLower == englishNumerals[i]) {
            return true;
        }
    }
    
    return false;
}

function findPositionAfterSecondWord(text, words) {
    if (words.length < 2) return text.length;
    
    // Находим позицию после второго слова
    var pos = 0;
    var wordCount = 0;
    var inWord = false;
    
    for (var i = 0; i < text.length; i++) {
        var ch = text.charAt(i);
        if (ch == ' ' || ch == '\t' || ch == '\n') {
            if (inWord) {
                wordCount++;
                inWord = false;
                if (wordCount == 2) {
                    pos = i;
                    break;
                }
            }
        } else {
            inWord = true;
        }
    }
    
    if (pos == 0) pos = text.length;
    
    // Пропускаем пробелы
    while (pos < text.length && text.charAt(pos) == ' ') {
        pos++;
    }
    
    return pos;
}

function findPositionAfterFirstWord(text, words) {
    if (words.length < 1) return text.length;
    
    // Находим позицию после первого слова
    var pos = 0;
    while (pos < text.length && text.charAt(pos) != ' ') {
        pos++;
    }
    
    // Пропускаем пробелы
    while (pos < text.length && text.charAt(pos) == ' ') {
        pos++;
    }
    
    return pos;
}

// ======================================
// ПРОСТЫЕ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ======================================

function getTextFromElement(element) {
    if (!element) return "";
    var text = "";
    
    function getText(node) {
        if (node.nodeType == 3) {
            text += node.nodeValue || "";
        } else if (node.nodeType == 1) {
            for (var i = 0; i < node.childNodes.length; i++) {
                getText(node.childNodes[i]);
            }
        }
    }
    
    getText(element);
    return text;
}

function normalizeSpaces(text, nbspEntity) {
    if (!text) return "";
    
    var result = "";
    for (var i = 0; i < text.length; i++) {
        if (i + nbspEntity.length <= text.length) {
            var substr = text.substring(i, i + nbspEntity.length);
            if (substr == nbspEntity) {
                result += ' ';
                i += nbspEntity.length - 1;
                continue;
            }
        }
        
        if (i + 6 <= text.length) {
            var substr = text.substring(i, i + 6);
            if (substr == '&nbsp;') {
                result += ' ';
                i += 5;
                continue;
            }
        }
        
        if (text.charCodeAt(i) == 160) {
            result += ' ';
            continue;
        }
        
        result += text.charAt(i);
    }
    
    result = result.replace(/\s+/g, ' ');
    result = result.replace(/^\s+|\s+$/g, '');
    
    return result;
}

function splitIntoWords(text) {
    var words = [];
    var current = "";
    
    for (var i = 0; i < text.length; i++) {
        var ch = text.charAt(i);
        if (ch == ' ' || ch == '\t' || ch == '\n') {
            if (current) {
                words.push(current);
                current = "";
            }
        } else {
            current += ch;
        }
    }
    
    if (current) words.push(current);
    return words;
}

function isNumberWord(word) {
    if (!word) return false;
    
    // Арабские цифры
    var allDigits = true;
    for (var i = 0; i < word.length; i++) {
        var ch = word.charAt(i);
        if (ch < '0' || ch > '9') {
            allDigits = false;
            break;
        }
    }
    if (allDigits) return true;
    
    // Римские цифры
    var romanChars = "IVXLCDM";
    for (i = 0; i < word.length; i++) {
        var found = false;
        for (var j = 0; j < romanChars.length; j++) {
            if (word.charAt(i).toUpperCase() == romanChars.charAt(j)) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    
    return word.length > 0;
}

// ======================================
// ОСНОВНАЯ ФУНКЦИЯ ОБРАБОТКИ С УЧЁТОМ НАСТРОЙКИ keepDots
// ======================================

function processTitle(div, pElement, titleInfo, nbspEntity, keepDots) {
    if (!titleInfo.isSuitable) return false;
    
    var text = titleInfo.text;
    var splitPos = titleInfo.splitPosition;
    var childNodes = titleInfo.childNodes;
    
    if (splitPos <= 0 || splitPos >= text.length) return false;
    
    // Нормализуем пробелы
    for (var i = 0; i < childNodes.length; i++) {
        normalizeNode(childNodes[i], nbspEntity);
    }
    
    // Находим узел для разбивки
    var currentPos = 0;
    var targetNode = -1;
    var targetOffset = 0;
    
    for (i = 0; i < childNodes.length; i++) {
        var nodeText = getNodeText(childNodes[i]);
        if (nodeText.length > 0) {
            if (currentPos + nodeText.length > splitPos) {
                targetNode = i;
                targetOffset = splitPos - currentPos;
                break;
            }
            currentPos += nodeText.length;
        }
    }
    
    if (targetNode == -1) return false;
    
    var p1 = document.createElement('p');
    var p2 = document.createElement('p');
    
    // Копируем узлы в первый абзац
    for (i = 0; i <= targetNode; i++) {
        if (i < targetNode) {
            p1.appendChild(childNodes[i].cloneNode(true));
        } else {
            var node = childNodes[i];
            if (node.nodeType == 3) {
                var nodeText = node.nodeValue || "";
                if (targetOffset > 0) {
                    var part1 = nodeText.substring(0, targetOffset);
                    var part2 = nodeText.substring(targetOffset);
                    
                    if (part1.replace(/\s/g, '').length > 0) {
                        p1.appendChild(document.createTextNode(part1));
                    }
                    if (part2.replace(/\s/g, '').length > 0) {
                        p2.appendChild(document.createTextNode(part2));
                    }
                } else {
                    p2.appendChild(document.createTextNode(nodeText));
                }
            } else {
                if (targetOffset == 0) {
                    p2.appendChild(node.cloneNode(true));
                } else {
                    p1.appendChild(node.cloneNode(true));
                }
            }
        }
    }
    
    // Копируем оставшиеся узлы во второй абзац
    for (i = targetNode + 1; i < childNodes.length; i++) {
        p2.appendChild(childNodes[i].cloneNode(true));
    }
    
    // Убираем пробелы
    trimParagraph(p1);
    trimParagraph(p2);
    
    if (p1.childNodes.length == 0 || p2.childNodes.length == 0) {
        return false;
    }
    
    // УДАЛЕНИЕ ТОЧКИ В КОНЦЕ ПЕРВОГО АБЗАЦА (если keepDots = 0)
    if (keepDots == 0) {
        removeTrailingDot(p1);
    }
    
    // Заменяем содержимое
    while (div.firstChild) {
        div.removeChild(div.firstChild);
    }
    
    div.appendChild(p1);
    div.appendChild(p2);
    
    return true;
}

// ======================================
// ФУНКЦИЯ ДЛЯ УДАЛЕНИЯ ТОЧКИ В КОНЦЕ ПЕРВОГО АБЗАЦА
// ======================================

function removeTrailingDot(paragraph) {
    if (!paragraph || paragraph.childNodes.length == 0) return;
    
    // Ищем последний текстовый узел
    var lastNode = paragraph.lastChild;
    while (lastNode && lastNode.nodeType != 3 && lastNode.lastChild) {
        lastNode = lastNode.lastChild;
    }
    
    // Если нашли текстовый узел
    if (lastNode && lastNode.nodeType == 3) {
        var text = lastNode.nodeValue || "";
        
        // Удаляем точку в конце
        text = text.replace(/\.\s*$/, '');
        
        // Также удаляем возможные несколько точек подряд
        text = text.replace(/\.+$/, '');
        
        // Если текст не пустой, обновляем узел
        if (text.length > 0) {
            lastNode.nodeValue = text;
        } else {
            // Если текст стал пустым, удаляем узел
            var parent = lastNode.parentNode;
            if (parent) {
                parent.removeChild(lastNode);
            }
        }
    }
    
    // После удаления точки, удаляем возможные пустые родительские элементы
    trimParagraph(paragraph);
}

// ======================================
// ДОПОЛНИТЕЛЬНЫЕ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ======================================

function getNodeText(node) {
    if (!node) return "";
    if (node.nodeType == 3) return node.nodeValue || "";
    if (node.nodeType == 1) {
        var text = "";
        for (var i = 0; i < node.childNodes.length; i++) {
            text += getNodeText(node.childNodes[i]);
        }
        return text;
    }
    return "";
}

function normalizeNode(node, nbspEntity) {
    if (!node) return;
    
    if (node.nodeType == 3) {
        var text = node.nodeValue || "";
        if (text) {
            var result = "";
            for (var i = 0; i < text.length; i++) {
                if (i + nbspEntity.length <= text.length) {
                    var substr = text.substring(i, i + nbspEntity.length);
                    if (substr == nbspEntity) {
                        result += ' ';
                        i += nbspEntity.length - 1;
                        continue;
                    }
                }
                
                if (i + 6 <= text.length) {
                    var substr = text.substring(i, i + 6);
                    if (substr == '&nbsp;') {
                        result += ' ';
                        i += 5;
                        continue;
                    }
                }
                
                if (text.charCodeAt(i) == 160) {
                    result += ' ';
                    continue;
                }
                
                result += text.charAt(i);
            }
            
            node.nodeValue = result.replace(/\s+/g, ' ');
        }
    } else if (node.nodeType == 1) {
        for (i = 0; i < node.childNodes.length; i++) {
            normalizeNode(node.childNodes[i], nbspEntity);
        }
    }
}

function trimParagraph(p) {
    if (!p || p.childNodes.length == 0) return;
    
    // Удаляем пробелы в начале первого текстового узла
    var first = p.firstChild;
    while (first && first.nodeType != 3) {
        if (first.firstChild) {
            trimChildNodes(first, true, false);
        }
        first = first.nextSibling;
    }
    
    if (first && first.nodeType == 3) {
        var text = first.nodeValue || "";
        text = text.replace(/^\s+/, '');
        if (text == '') {
            p.removeChild(first);
        } else {
            first.nodeValue = text;
        }
    }
    
    // Удаляем пробелы в конце последнего текстового узла
    var last = p.lastChild;
    while (last && last.nodeType != 3) {
        if (last.lastChild) {
            trimChildNodes(last, false, true);
        }
        last = last.previousSibling;
    }
    
    if (last && last.nodeType == 3) {
        var text = last.nodeValue || "";
        text = text.replace(/\s+$/, '');
        if (text == '') {
            p.removeChild(last);
        } else {
            last.nodeValue = text;
        }
    }
}

function trimChildNodes(node, trimStart, trimEnd) {
    if (!node || node.nodeType != 1) return;
    
    if (trimStart && node.firstChild) {
        var first = node.firstChild;
        if (first.nodeType == 3) {
            var text = first.nodeValue || "";
            text = text.replace(/^\s+/, '');
            if (text == '') {
                node.removeChild(first);
            } else {
                first.nodeValue = text;
            }
        }
    }
    
    if (trimEnd && node.lastChild) {
        var last = node.lastChild;
        if (last.nodeType == 3) {
            var text = last.nodeValue || "";
            text = text.replace(/\s+$/, '');
            if (text == '') {
                node.removeChild(last);
            } else {
                last.nodeValue = text;
            }
        }
    }
}
