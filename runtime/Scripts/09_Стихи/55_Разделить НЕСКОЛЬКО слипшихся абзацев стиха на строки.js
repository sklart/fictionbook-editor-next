// Название: «Разделить несколько слипшихся абзацев стиха на строки» для редактора FBE
// version 3.4
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для разбивки нескольких длинных выделенных абзацев, 
// на короткие строки по типичным признакам похожести строк на стихи.
// Учитываются заглавные буквы, знаки препинания внутри исходного абзаца,
// а также определенный набор слов и частей речи, написанных с заглавной буквы.

// При выделении всего документа (CTRL+A) - тоже разбивает (удобно для сборников стихов без прозы)

//  Учитывается комплекс признаков "похожести" на стихи:
// Длина строк
// Небольшой перечень слов, которые бывают с заглавной буквы (обычно) только в начале фразы
// Слова из словаря (POETIC_BREAK_WORDS) после пробела
// Знак препинания + пробел + Заглавная буква
// Строчная буква + пробел + Заглавная буква
// Длины строк не должны сильно отличаться (максимально - 2,5-4 раз)
// Исключаются из обработки:
// Пустые строки
// Очень короткие абзацы (< 10 символов без форматирования)
// Недлинные абзацы, набранные КАПСом
// Поддержка отмены действий (Ctrl+Z)

// version 3.4, 15.12.2025
//======================================

// Получаем символ неразрывного пробела из настроек FBE
var nbspEntity;
try {
    var nbspChar = window.external.GetNBSP();
    if (nbspChar.charCodeAt(0) == 160) {
        nbspEntity = "&nbsp;";
    } else {
        nbspEntity = nbspChar;
    }
} catch(e) {
    nbspEntity = "&nbsp;";
}

// Словарь коротких слов и паттернов для разбивки - ПОЛНАЯ ВЕРСИЯ
var POETIC_BREAK_WORDS = [
    // Местоимения
    "Я", "Ты", "Он", "Она", "Оно", "Они", "Мы", "Вы",
    
    // Притяжательные местоимения
    "Мой", "Моя", "Моё", "Мои",
    "Твой", "Твоя", "Твоё", "Твои",
    "Наш", "Наша", "Наше", "Наши",
    "Свой", "Своя", "Своё", "Свои",
    "Ваш", "Ваша", "Ваше", "Ваши",
    "Его", "Её", "Их",
    
    // Дательные местоимения
    "Мне", "Тебе", "Ему", "Ей", "Нам", "Вам", "Им",
    
    // Предлоги и союзы (короткие)
    "С", "Со", "В", "Во", "К", "Ко", "На", "За", "Из", "От", "До", "По", 
    "О", "Об", "Обо", "У", "При", "Про", "Без", "Сквозь", "Через", "Меж", "Между",
    
    // Союзы
    "Да", "Но", "А", "И", "Или", "Ли", "Не", "Ни", "То", "Же",
    "Чтоб", "Чтобы", "Как", "Что", "Чем", "Бы", "Вот",
    
    // Вопросительные и указательные слова
    "Кто", "Что", "Чей", "Чья", "Чьё", "Чьи",
    "Какой", "Какая", "Какое", "Какие",
    "Который", "Которая", "Которое", "Которые",
    "Сколько", "Столько",
    
    // Наречия места
    "Тут", "Там", "Здесь", "Везде", "Нигде", "Всюду",
    "Далеко", "Близко", "Высоко", "Низко", "Глубоко",
    "Справа", "Слева", "Сверху", "Снизу", "Внутри", "Снаружи",
    "Впереди", "Позади", "Вокруг", "Вблизи", "Вдали",
    
    // Наречия времени
    "Сейчас", "Теперь", "Тогда", "Всегда", "Никогда", "Иногда",
    "Часто", "Редко", "Скоро", "Поздно", "Рано",
    "Сегодня", "Завтра", "Вчера", "Утром", "Вечером", "Ночью",
    "Сразу", "Скоро", "Долго", "Быстро", "Медленно",
    
    // Наречия меры и степени
    "Мало", "Много", "Совсем", "Слишком", "Довольно", "Весьма",
    "Совершенно", "Абсолютно", "Полностью", "Чуть", "Еле", "Едва",
    "Сильно", "Слабо", "Тихо", "Громко", "Ясно", "Темно",
    "Холодно", "Жарко", "Тепло", "Сыро", "Сухо",
    
    // Наречия образа действия
    "Быстро", "Медленно", "Тихо", "Громко", "Ярко", "Тускло",
    "Легко", "Тяжело", "Просто", "Сложно", "Прямо", "Криво",
    "Ровно", "Косо", "Смело", "Трусливо", "Умно", "Глупо",
    "Красиво", "Некрасиво", "Весело", "Грустно", "Серьезно", "Шутя",
    
    // Неопределенные местоимения и наречия (с дефисами)
    "Кто-то", "Что-то", "Чей-то", "Какой-то", "Который-то",
    "Кто-нибудь", "Что-нибудь", "Чей-нибудь", "Какой-нибудь",
    "Кто-либо", "Что-либо", "Чей-либо", "Какой-либо",
    "Кое-кто", "Кое-что", "Кое-какой",
    "Некто", "Нечто", "Некий",
    
    // Наречия с дефисами
    "Куда-то", "Куда-нибудь", "Куда-либо",
    "Где-то", "Где-нибудь", "Где-либо",
    "Когда-то", "Когда-нибудь", "Когда-либо",
    "Как-то", "Как-нибудь", "Как-либо",
    "Откуда-то", "Откуда-нибудь", "Откуда-либо",
    "Почему-то", "Почему-нибудь", "Почему-либо",
    "Зачем-то", "Зачем-нибудь", "Зачем-либо",
    "Отчего-то", "Отчего-нибудь", "Отчего-либо",
    
    // Слова с большой буквы в начале предложения
    "Раз", "Пусть", "Хоть", "Если", "Когда", "Где", "Куда", "Откуда", "Почему", "Зачем",
    "Вдруг", "Внезапно", "Словно", "Будто", "Точно", "Прямо",
    "Ведь", "Хоть", "Даже", "Только", "Лишь", "Едва",
    
    // Частицы
    "Неужели", "Разве", "Вряд", "Едва", "Еле",
    "Пожалуй", "Пускай", "Будто", "Мол", "Дескать",
    
    // Числительные (прописные)
    "Один", "Два", "Три", "Четыре", "Пять",
    "Первым", "Вторым", "Третьим",
    "Однажды", "Дважды", "Трижды",
    
    // Глаголы в повелительном наклонении
    "Смотри", "Слушай", "Иди", "Беги", "Пой", "Плачь", "Смейся",
    "Дай", "Возьми", "Брось", "Оставь", "Забудь", "Помни",
    "Жди", "Зови", "Лети", "Плыви", "Стой", "Лежи",
    
    // Отрицания
    "Нет", "Никак", "Ничуть", "Нисколько", "Нимало",
    
    // Восклицания и междометия
    "О", "Ах", "Ох", "Эх", "Ух", "Ай", "Ой", "Эй",
    "Господи", "Боже", "Черт", "Черт возьми",
    
    // Слова состояния
    "Жаль", "Стыдно", "Страшно", "Весело", "Грустно",
    "Трудно", "Легко", "Можно", "Нельзя", "Надо", "Нужно",
    
    // Сравнения
    "Будто", "Словно", "Точно", "Прямо", "Ровно",
    
    // Вводные слова
    "Кажется", "Пожалуй", "Верно", "Наверно", "Наверное",
    "Конечно", "Безусловно", "Возможно", "Вероятно"
];

// Вспомогательные функции
function isWhitespace(char) {
    return char == ' ' || char == '\r' || char == '\n';
}

function trimString(str) {
    if (!str) return "";
    return str.replace(/^\s+|\s+$/g, '');
}

function isUpperCase(char) {
    return (char >= 'А' && char <= 'Я') || (char >= 'A' && char <= 'Z');
}

// IE6-совместимая замена indexOf для строк
function stringContains(str, search) {
    if (!str || !search) return -1;
    
    for (var i = 0; i <= str.length - search.length; i++) {
        var found = true;
        for (var j = 0; j < search.length; j++) {
            if (str.charAt(i + j) != search.charAt(j)) {
                found = false;
                break;
            }
        }
        if (found) return i;
    }
    return -1;
}

// IE6-совместимая замена indexOf для массива
function arrayContains(arr, search) {
    if (!arr) return -1;
    
    for (var i = 0; i < arr.length; i++) {
        if (arr[i] == search) {
            return i;
        }
    }
    return -1;
}

// Нормализация текста для анализа
function normalizeTextForAnalysis(text) {
    if (!text) return "";
    
    var normalized = text;
    
    normalized = normalized.replace(/&nbsp;/g, " ");
    normalized = normalized.replace(/&lt;/g, " ");
    normalized = normalized.replace(/&gt;/g, " ");
    normalized = normalized.replace(/&amp;/g, " ");
    normalized = normalized.replace(/&quot;/g, " ");
    normalized = normalized.replace(/&apos;/g, " ");
    normalized = normalized.replace(/&shy;/g, "");
    
    if (nbspEntity.length == 1) {
        var regex = new RegExp(nbspEntity, "g");
        normalized = normalized.replace(regex, " ");
    } else {
        var regex = new RegExp(nbspEntity, "g");
        normalized = normalized.replace(regex, " ");
    }
    
    normalized = normalized.replace(/□/g, " ");
    normalized = normalized.replace(/▫/g, " ");
    normalized = normalized.replace(/◦/g, " ");
    normalized = normalized.replace(/\xA0/g, " ");
    
    return normalized;
}

// Получает маппинг позиций с учетом ВСЕХ символов FBE (IE6-совместимая)
function getPositionMapping(html, normalizedText) {
    var mapping = [];
    var textIndex = 0;
    var htmlIndex = 0;
    
    var htmlEntities = ["&nbsp;", "&lt;", "&gt;", "&amp;", "&quot;", "&apos;", "&shy;"];
    var entityLengths = [1, 1, 1, 1, 1, 1, 0];
    
    var fbeSpaces = ["□", "▫", "◦"];
    if (nbspEntity.length == 1 && arrayContains(fbeSpaces, nbspEntity) == -1) {
        fbeSpaces[fbeSpaces.length] = nbspEntity;
    }
    
    while (htmlIndex < html.length && textIndex <= normalizedText.length) {
        var char = html.charAt(htmlIndex);
        
        if (char == '<') {
            var tagEnd = -1;
            for (var k = htmlIndex; k < html.length; k++) {
                if (html.charAt(k) == '>') {
                    tagEnd = k;
                    break;
                }
            }
            if (tagEnd != -1) {
                htmlIndex = tagEnd + 1;
                continue;
            } else {
                htmlIndex++;
                continue;
            }
        }
        
        if (char == '&') {
            var foundEntity = false;
            
            for (var e = 0; e < htmlEntities.length; e++) {
                var entity = htmlEntities[e];
                var isEntity = true;
                
                for (var m = 0; m < entity.length; m++) {
                    if (htmlIndex + m >= html.length || html.charAt(htmlIndex + m) != entity.charAt(m)) {
                        isEntity = false;
                        break;
                    }
                }
                
                if (isEntity) {
                    if (entityLengths[e] > 0) {
                        mapping[mapping.length] = {
                            htmlPos: htmlIndex,
                            textPos: textIndex,
                            entityLength: entity.length,
                            normalizedLength: entityLengths[e]
                        };
                        textIndex += entityLengths[e];
                    }
                    
                    htmlIndex += entity.length;
                    foundEntity = true;
                    break;
                }
            }
            
            if (foundEntity) continue;
            
            if (html.charAt(htmlIndex + 1) == '#') {
                var semicolonPos = -1;
                for (var k = htmlIndex; k < html.length; k++) {
                    if (html.charAt(k) == ';') {
                        semicolonPos = k;
                        break;
                    }
                }
                if (semicolonPos != -1) {
                    mapping[mapping.length] = {
                        htmlPos: htmlIndex,
                        textPos: textIndex,
                        entityLength: semicolonPos - htmlIndex + 1,
                        normalizedLength: 1
                    };
                    textIndex++;
                    htmlIndex = semicolonPos + 1;
                    continue;
                }
            }
        }
        
        var isFbeSpace = false;
        for (var s = 0; s < fbeSpaces.length; s++) {
            if (char == fbeSpaces[s]) {
                isFbeSpace = true;
                break;
            }
        }
        
        if (isFbeSpace) {
            mapping[mapping.length] = {
                htmlPos: htmlIndex,
                textPos: textIndex,
                entityLength: 1,
                normalizedLength: 1
            };
            textIndex++;
            htmlIndex++;
            continue;
        }
        
        if (char.charCodeAt(0) == 160) {
            mapping[mapping.length] = {
                htmlPos: htmlIndex,
                textPos: textIndex,
                entityLength: 1,
                normalizedLength: 1
            };
            textIndex++;
            htmlIndex++;
            continue;
        }
        
        if (textIndex < normalizedText.length) {
            mapping[mapping.length] = {
                htmlPos: htmlIndex,
                textPos: textIndex,
                entityLength: 1,
                normalizedLength: 1
            };
            textIndex++;
        }
        
        htmlIndex++;
    }
    
    return mapping;
}

// Конвертирует позицию из нормализованного текста в HTML
function convertTextPosToHtmlPos(textPos, mapping) {
    if (!mapping || mapping.length == 0) return textPos;
    
    for (var i = 0; i < mapping.length; i++) {
        if (mapping[i].textPos == textPos) {
            return mapping[i].htmlPos;
        }
    }
    
    var bestMatch = -1;
    var bestMatchIndex = -1;
    
    for (var i = 0; i < mapping.length; i++) {
        if (mapping[i].textPos <= textPos && mapping[i].textPos > bestMatch) {
            bestMatch = mapping[i].textPos;
            bestMatchIndex = i;
        }
    }
    
    if (bestMatchIndex != -1) {
        var diff = textPos - mapping[bestMatchIndex].textPos;
        if (diff <= 5) {
            return mapping[bestMatchIndex].htmlPos + diff;
        }
    }
    
    return textPos;
}

// Получает чистый текст из элемента
function getPlainTextFromElement(element) {
    var text = "";
    
    function walk(node) {
        if (node.nodeType == 3) {
            text += normalizeTextForAnalysis(node.nodeValue);
        } else if (node.nodeType == 1) {
            if (node.nodeName == "A") {
                var href = node.getAttribute("l:href") || node.getAttribute("href") || "";
                if (stringContains(href, "#n_") == 0 || stringContains(href, "#_") == 0) {
                    return;
                }
            }
            
            for (var i = 0; i < node.childNodes.length; i++) {
                walk(node.childNodes[i]);
            }
        }
    }
    
    walk(element);
    return text;
}

function getNextWord(text, startPos) {
    var word = "";
    for (var i = startPos; i < text.length; i++) {
        var char = text.charAt(i);
        if (char == ' ' || char == ',' || char == '.' || char == '!' || char == '?') {
            break;
        }
        word += char;
    }
    return word;
}

function isPoeticBreakWord(word) {
    var upperWord = word.toUpperCase();
    
    for (var i = 0; i < POETIC_BREAK_WORDS.length; i++) {
        if (upperWord == POETIC_BREAK_WORDS[i].toUpperCase()) {
            return true;
        }
    }
    
    var hasDash = false;
    for (var d = 0; d < word.length; d++) {
        if (word.charAt(d) == '-') {
            hasDash = true;
            break;
        }
    }
    
    if (hasDash) {
        var parts = [];
        var currentPart = "";
        for (var p = 0; p < word.length; p++) {
            if (word.charAt(p) == '-') {
                if (currentPart) {
                    parts[parts.length] = currentPart;
                    currentPart = "";
                }
            } else {
                currentPart += word.charAt(p);
            }
        }
        if (currentPart) {
            parts[parts.length] = currentPart;
        }
        
        if (parts.length == 2) {
            var firstPart = parts[0];
            var secondPart = parts[1];
            
            var validFirstParts = ["Кто", "Что", "Чей", "Какой", "Который", 
                                  "Куда", "Где", "Когда", "Как", "Откуда", 
                                  "Почему", "Зачем", "Отчего", "Кое"];
            
            var validSecondParts = ["то", "нибудь", "либо", "какой", "чего"];
            
            var firstPartValid = false;
            for (var j = 0; j < validFirstParts.length; j++) {
                if (firstPart.toUpperCase() == validFirstParts[j].toUpperCase()) {
                    firstPartValid = true;
                    break;
                }
            }
            
            var secondPartValid = false;
            for (var k = 0; k < validSecondParts.length; k++) {
                if (secondPart.toLowerCase() == validSecondParts[k].toLowerCase()) {
                    secondPartValid = true;
                    break;
                }
            }
            
            if (firstPartValid && secondPartValid) {
                return true;
            }
        }
    }
    
    return false;
}

function isPotentialBreakPosition(text, pos) {
    if (pos >= text.length - 2) return false;
    
    var char = text.charAt(pos);
    var nextChar = text.charAt(pos + 1);
    var nextNextChar = text.charAt(pos + 2);
    
    if ((char == ',' || char == '.' || char == '!' || char == '?' || char == ';' || char == '…') &&
        (nextChar == ' ' || nextChar == '\r' || nextChar == '\n') &&
        isUpperCase(nextNextChar)) {
        return true;
    }
    
    if (char == ' ' && isUpperCase(nextChar)) {
        return true;
    }
    
    return false;
}

function isSpecialWordBreakPosition(text, pos) {
    if (pos >= text.length - 2) return false;
    
    var char = text.charAt(pos);
    var nextChar = text.charAt(pos + 1);
    
    if (char == ' ' && isUpperCase(nextChar)) {
        var word = getNextWord(text, pos + 1);
        return isPoeticBreakWord(word);
    }
    
    return false;
}

function findNextBreakPosition(text) {
    for (var i = 0; i < text.length; i++) {
        if (isPotentialBreakPosition(text, i)) {
            return i + 1;
        }
    }
    return text.length;
}

function estimateNextLineLength(text) {
    var nextBreak = findNextBreakPosition(text);
    if (nextBreak > 0) {
        return nextBreak;
    }
    return text.length;
}

function isBalancedSplit(line1, remainingText, isSpecialWordBreak) {
    var nextLineLength = estimateNextLineLength(remainingText);
    var currentLength = line1.length;
    
    if (isSpecialWordBreak) {
        if (currentLength < 3 || nextLineLength < 3) return false;
        var ratio = currentLength > nextLineLength ? currentLength / nextLineLength : nextLineLength / currentLength;
        if (ratio > 4) return false;
        return true;
    }
    
    if (currentLength < 10 || nextLineLength < 10) return false;
    var ratio = currentLength > nextLineLength ? currentLength / nextLineLength : nextLineLength / currentLength;
    if (ratio > 2.5) return false;
    
    return true;
}

// Разбивает текст на поэтические строки
function splitIntoPoeticLines(text) {
    var lines = [];
    var currentLine = "";
    
    for (var i = 0; i < text.length; i++) {
        var char = text.charAt(i);
        currentLine += char;
        
        if (isPotentialBreakPosition(text, i)) {
            var lineSoFar = trimString(currentLine);
            var remainingText = text.substring(i + 1);
            
            var isSpecialWordBreak = isSpecialWordBreakPosition(text, i);
            
            if (isSpecialWordBreak || isBalancedSplit(lineSoFar, remainingText, isSpecialWordBreak)) {
                if (lineSoFar.length > 0) {
                    lines[lines.length] = lineSoFar;
                    currentLine = "";
                }
            }
        }
    }
    
    var lastLine = trimString(currentLine);
    if (lastLine.length > 0) {
        lines[lines.length] = lastLine;
    }
    
    return lines;
}

// Находит позиции разбивки
function findBreakPositions(plainText, textLines) {
    var positions = [];
    var currentPos = 0;
    
    for (var i = 0; i < textLines.length - 1; i++) {
        currentPos += textLines[i].length;
        
        while (currentPos < plainText.length && isWhitespace(plainText.charAt(currentPos))) {
            currentPos++;
        }
        
        positions[positions.length] = currentPos;
    }
    
    return positions;
}

// Вставляет маркеры с учетом символов FBE
function insertMarkersWithCorrection(html, normalizedText, positions) {
    if (!html || !normalizedText || positions.length == 0) {
        return html;
    }
    
    var mapping = getPositionMapping(html, normalizedText);
    
    if (mapping.length == 0) {
        return html;
    }
    
    var result = html;
    
    for (var x = 0; x < positions.length; x++) {
        for (var y = x + 1; y < positions.length; y++) {
            if (positions[x] < positions[y]) {
                var temp = positions[x];
                positions[x] = positions[y];
                positions[y] = temp;
            }
        }
    }
    
    for (var i = 0; i < positions.length; i++) {
        var textPos = positions[i];
        
        if (textPos < 0 || textPos > normalizedText.length) {
            continue;
        }
        
        var htmlPos = convertTextPosToHtmlPos(textPos, mapping);
        
        if (htmlPos != -1 && htmlPos < result.length) {
            var isMidWord = false;
            if (htmlPos > 0 && htmlPos < result.length) {
                var charBefore = result.charAt(htmlPos - 1);
                var charAfter = result.charAt(htmlPos);
                
                if (!isWhitespace(charBefore) && charBefore != ',' && charBefore != '.' && 
                    !isWhitespace(charAfter) && charAfter != ',' && charAfter != '.') {
                    isMidWord = true;
                }
            }
            
            if (!isMidWord) {
                result = result.substring(0, htmlPos) + '§§' + result.substring(htmlPos);
            }
        }
    }
    
    return result;
}

// Разбивает HTML по маркерам с сохранением форматирования
function splitHTMLByMarkers(html, marker) {
    if (!html || !marker) return [html];
    
    var parts = [];
    var stack = [];
    var currentPart = '';
    var i = 0;
    var len = html.length;
    var inTag = false;
    var tagName = '';
    var isClosing = false;
    
    while (i < len) {
        var char = html.charAt(i);
        
        if (char === '<') {
            inTag = true;
            tagName = '';
            isClosing = false;
            currentPart += char;
            i++;
            
            while (i < len && (html.charAt(i) === ' ' || html.charAt(i) === '/' || html.charAt(i) === '>')) {
                if (html.charAt(i) === '/') {
                    isClosing = true;
                }
                currentPart += html.charAt(i);
                i++;
            }
            
            while (i < len && html.charAt(i) !== ' ' && html.charAt(i) !== '>' && html.charAt(i) !== '/') {
                tagName += html.charAt(i);
                currentPart += html.charAt(i);
                i++;
            }
            
            while (i < len && html.charAt(i) !== '>') {
                currentPart += html.charAt(i);
                i++;
            }
            
            if (i < len && html.charAt(i) === '>') {
                currentPart += '>';
                i++;
                
                tagName = tagName.toLowerCase();
                if (isClosing) {
                    for (var j = stack.length - 1; j >= 0; j--) {
                        if (stack[j] === tagName) {
                            stack.splice(j, 1);
                            break;
                        }
                    }
                } else if (tagName !== 'br' && tagName !== 'img' && tagName !== 'hr') {
                    if (html.charAt(i-2) !== '/') {
                        stack[stack.length] = tagName;
                    }
                }
                
                inTag = false;
            }
            
        } else if (!inTag && char === marker.charAt(0)) {
            var isFullMarker = true;
            for (var m = 0; m < marker.length; m++) {
                if (i + m >= len || html.charAt(i + m) !== marker.charAt(m)) {
                    isFullMarker = false;
                    break;
                }
            }
            
            if (isFullMarker) {
                if (currentPart.replace(/^\s+|\s+$/g, '').replace(/\s+/g, '') !== '') {
                    var closedPart = currentPart;
                    for (var s = stack.length - 1; s >= 0; s--) {
                        closedPart += '</' + stack[s] + '>';
                    }
                    parts[parts.length] = closedPart;
                    
                    currentPart = '';
                    for (var s2 = 0; s2 < stack.length; s2++) {
                        currentPart += '<' + stack[s2] + '>';
                    }
                }
                
                i += marker.length;
                continue;
            } else {
                currentPart += char;
                i++;
            }
            
        } else {
            currentPart += char;
            i++;
        }
    }
    
    if (currentPart.replace(/^\s+|\s+$/g, '').replace(/\s+/g, '') !== '') {
        parts[parts.length] = currentPart;
    }
    
    return parts.length > 0 ? parts : [html];
}

function cleanHTMLPart(html) {
    if (!html) return '';
    
    var trimmed = html;
    trimmed = trimmed.replace(/^(\s*)([^<])/, '$2');
    trimmed = trimmed.replace(/([^>])(\s*)$/, '$1');
    
    return trimmed;
}

// Находит все абзацы в выделении
function getParagraphsInSelection() {
    var paragraphs = [];
    var selection = document.selection;
    
    if (!selection) return paragraphs;
    
    var range = selection.createRange();
    
    var startRange = range.duplicate();
    startRange.collapse(true);
    var startElement = startRange.parentElement();
    
    var endRange = range.duplicate();
    endRange.collapse(false);
    var endElement = endRange.parentElement();
    
    while (startElement && startElement.nodeName != "P" && startElement.nodeName != "BODY") {
        startElement = startElement.parentNode;
    }
    
    if (!startElement || startElement.nodeName != "P") {
        return paragraphs;
    }
    
    while (endElement && endElement.nodeName != "P" && endElement.nodeName != "BODY") {
        endElement = endElement.parentNode;
    }
    
    if (!endElement || endElement.nodeName != "P") {
        endElement = startElement;
    }
    
    var currentElement = startElement;
    
    while (currentElement) {
        paragraphs[paragraphs.length] = currentElement;
        
        if (currentElement == endElement) {
            break;
        }
        
        var foundNext = false;
        
        var nextSibling = currentElement.nextSibling;
        while (nextSibling) {
            if (nextSibling.nodeName == "P") {
                currentElement = nextSibling;
                foundNext = true;
                break;
            }
            
            if (nextSibling.nodeType == 1) {
                var firstParagraph = findFirstParagraphInElement(nextSibling);
                if (firstParagraph) {
                    currentElement = firstParagraph;
                    foundNext = true;
                    break;
                }
            }
            
            nextSibling = nextSibling.nextSibling;
        }
        
        if (!foundNext) {
            var parent = currentElement.parentNode;
            var nextInParent = null;
            
            while (parent && parent.nodeName != "BODY") {
                var tempElement = currentElement;
                nextInParent = null;
                
                var foundCurrent = false;
                for (var i = 0; i < parent.childNodes.length; i++) {
                    var child = parent.childNodes[i];
                    
                    if (child == tempElement) {
                        foundCurrent = true;
                        continue;
                    }
                    
                    if (foundCurrent) {
                        if (child.nodeName == "P") {
                            nextInParent = child;
                            break;
                        }
                        
                        if (child.nodeType == 1) {
                            var firstParagraph = findFirstParagraphInElement(child);
                            if (firstParagraph) {
                                nextInParent = firstParagraph;
                                break;
                            }
                        }
                    }
                }
                
                if (nextInParent) {
                    currentElement = nextInParent;
                    foundNext = true;
                    break;
                }
                
                currentElement = parent;
                parent = parent.parentNode;
            }
        }
        
        if (!foundNext) {
            break;
        }
    }
    
    return paragraphs;
}

// Находит первый абзац внутри элемента
function findFirstParagraphInElement(element) {
    if (element.nodeName == "P") {
        return element;
    }
    
    for (var i = 0; i < element.childNodes.length; i++) {
        var child = element.childNodes[i];
        if (child.nodeType == 1) {
            var result = findFirstParagraphInElement(child);
            if (result) {
                return result;
            }
        }
    }
    
    return null;
}

// Основная функция разбивки (с упрощенной проверкой коротких абзацев)
function splitParagraph(paragraph) {
    try {
        var originalHTML = paragraph.innerHTML;
        var normalizedText = getPlainTextFromElement(paragraph);
        var trimmedText = trimString(normalizedText);
        
        // Анализируем абзац для статистики
        var textLength = trimmedText.length;
        
        if (textLength == 0) {
            return { 
                success: false, 
                error: "Пустой абзац",
                stats: { isEmpty: true, textLength: 0 }
            };
        }
        
        var textLines = splitIntoPoeticLines(normalizedText);
        
        if (textLines.length <= 1) {
            return { 
                success: false, 
                error: "Не удалось разбить на стихотворные строки",
                stats: { isShort: true, textLength: textLength, linesFound: 1 }
            };
        }
        
        var breakPositions = findBreakPositions(normalizedText, textLines);
        
        var htmlWithMarkers = insertMarkersWithCorrection(originalHTML, normalizedText, breakPositions);
        
        if (!htmlWithMarkers || htmlWithMarkers === originalHTML) {
            return { 
                success: false, 
                error: "Не удалось вставить маркеры разбивки",
                stats: { isError: true, textLength: textLength }
            };
        }
        
        var parts = splitHTMLByMarkers(htmlWithMarkers, '§§');
        
        if (parts.length <= 1) {
            return { 
                success: false, 
                error: "Не удалось разбить HTML",
                stats: { isError: true, textLength: textLength }
            };
        }
        
        var cleanParts = [];
        for (var i = 0; i < parts.length; i++) {
            var part = parts[i].replace(/§§/g, '');
            var cleanPart = cleanHTMLPart(part);
            if (cleanPart.length > 0) {
                cleanParts[cleanParts.length] = cleanPart;
            }
        }
        
        if (cleanParts.length <= 1) {
            return { 
                success: false, 
                error: "После очистки осталась одна строка",
                stats: { isShort: true, textLength: textLength, linesFound: 1 }
            };
        }
        
        return {
            success: true,
            parts: cleanParts,
            lines: cleanParts.length,
            stats: { textLength: textLength, linesFound: cleanParts.length }
        };
        
    } catch (e) {
        return { 
            success: false, 
            error: "Ошибка: " + e.message,
            stats: { isError: true }
        };
    }
}

// Главная функция с ДЕТАЛЬНОЙ статистикой
function Run() {
    var version = "3.4";
    var scriptName = "Разделить несколько слипшихся абзацев стиха на строки";
    var fullTitle = scriptName + " (версия " + version + ")";
    
    try {
        var paragraphs = getParagraphsInSelection();
        
        if (paragraphs.length == 0) {
            MsgBox(fullTitle + "\n\nКурсор должен быть в абзаце текста или должно быть выделение абзацев", "FBE скрипт");
            return;
        }
        
        var confirmMsg = "Найдено абзацев для обработки: " + paragraphs.length + "\n\nПродолжить разбивку?";
        
        if (!confirm(confirmMsg)) {
            return;
        }
        
        window.external.BeginUndoUnit(document, scriptName);
        
        var startTime = new Date();
        var processed = 0;
        var totalLines = 0;
        
        // Статистика для детального отчета
        var emptyCount = 0;
        var shortCount = 0;
        var errorCount = 0;
        var successCount = 0;
        
        // Для сохранения успешно обработанных абзацев для прокрутки
        var firstProcessedPara = null;
        
        for (var i = 0; i < paragraphs.length; i++) {
            var para = paragraphs[i];
            var result = splitParagraph(para);
            
            if (result.success) {
                var parent = para.parentNode;
                var className = para.className || "";
                
                for (var j = 0; j < result.parts.length; j++) {
                    var newPara = document.createElement("P");
                    if (className) {
                        newPara.className = className;
                    }
                    newPara.innerHTML = result.parts[j];
                    
                    if (j == 0) {
                        parent.replaceChild(newPara, para);
                    } else {
                        parent.insertBefore(newPara, para.nextSibling);
                    }
                    para = newPara;
                }
                
                processed++;
                totalLines += result.lines;
                successCount++;
                
                if (!firstProcessedPara) {
                    firstProcessedPara = para;
                }
            } else {
                // Классифицируем тип ошибки
                if (result.error && result.error.indexOf("Пустой абзац") != -1) {
                    emptyCount++;
                } else if (result.error && (result.error.indexOf("Не удалось разбить") != -1 || 
                                           result.error.indexOf("осталась одна строка") != -1)) {
                    if (result.stats && result.stats.textLength && result.stats.textLength < 30) {
                        shortCount++;
                    } else {
                        errorCount++;
                    }
                } else {
                    errorCount++;
                }
            }
        }
        
        // Снимаем выделение
        try {
            var clearRange = document.body.createTextRange();
            clearRange.collapse();
            clearRange.select();
        } catch (e) {
            // Игнорируем ошибки при снятии выделения
        }
        
        // Прокручиваем к центру первого обработанного абзаца
        var elementToScroll = firstProcessedPara || (paragraphs.length > 0 ? paragraphs[0] : null);
        if (elementToScroll && elementToScroll.scrollIntoView) {
            try {
                elementToScroll.scrollIntoView(false);
                
                var elementTop = 0;
                var element = elementToScroll;
                while (element) {
                    elementTop += element.offsetTop || 0;
                    element = element.offsetParent;
                }
                
                var windowHeight = document.documentElement.clientHeight || document.body.clientHeight || 500;
                var targetScroll = elementTop - Math.floor(windowHeight / 3);
                
                if (document.documentElement && document.documentElement.scrollTop !== undefined) {
                    document.documentElement.scrollTop = targetScroll;
                } else if (document.body && document.body.scrollTop !== undefined) {
                    document.body.scrollTop = targetScroll;
                }
            } catch (scrollError) {
                elementToScroll.scrollIntoView(false);
            }
        }
        
        var endTime = new Date();
        var timeDiff = endTime - startTime;
        
        // Формируем детальную статистику
        var excludedTotal = emptyCount + shortCount + errorCount;
        
        var message = "---------------------------\n" +
                     fullTitle + "\n" +
                     "---------------------------\n\n" +
                     "Обработано абзацев: " + processed + " из " + paragraphs.length + "\n" +
                     "Получено строк: " + totalLines + "\n";
        
        // Показываем детальную статистику исключений только если есть исключения
        if (excludedTotal > 0) {
            message += "\nИсключено из обработки: " + excludedTotal + " абзацев\n";
            message += "Из них:\n";
            
            if (emptyCount > 0) {
                message += "• Пустые строки - " + emptyCount + "\n";
            }
            if (shortCount > 0) {
                message += "• Короткие строки - " + shortCount + "\n";
            }
            if (errorCount > 0) {
                message += "• Ошибки обработки - " + errorCount + "\n";
            }
        }
        
        message += "\n---------------------------\n" +
                  "Время выполнения: " + timeDiff + " мс";
        
        MsgBox(message, "FBE скрипт");
        window.external.EndUndoUnit(document);
        
    } catch (e) {
        try { window.external.EndUndoUnit(document); } catch(e2) {}
        MsgBox(fullTitle + "\n\nОшибка: " + e.message, "FBE скрипт");
    }
}
