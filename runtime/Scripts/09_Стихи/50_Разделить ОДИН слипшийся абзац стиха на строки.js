// Название: «Разделить один слипшийся абзац стиха на строки» для редактора FBE
// version 4.5
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для разбивки одного длинного абзаца, на котором стоит курсор,
// (или выделенного абзаца) на короткие строки по типичным признакам похожести строк на стихи.

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

// version 4.5, 10.12.2025
//======================================

// Словарь коротких слов и паттернов для разбивки
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

// Вспомогательные функции (должны быть объявлены до использования)
function isWhitespace(char) {
    return char == ' ' || char == '\r' || char == '\n' || char == '\t';
}

function trimString(str) {
    if (!str) return "";
    return str.replace(/^\s+|\s+$/g, '');
}

function isUpperCase(char) {
    return (char >= 'А' && char <= 'Я') || (char >= 'A' && char <= 'Z');
}

// Получает вертикальное смещение элемента относительно документа
function getElementOffsetTop(element) {
    var offsetTop = 0;
    while (element) {
        offsetTop += element.offsetTop || 0;
        element = element.offsetParent;
    }
    return offsetTop;
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

// Дополнительная функция для проверки слов с дефисами
function isPoeticBreakWord(word) {
    // Приводим к верхнему регистру для сравнения
    var upperWord = word.toUpperCase();
    
    // Сначала проверяем точное совпадение
    for (var i = 0; i < POETIC_BREAK_WORDS.length; i++) {
        if (upperWord == POETIC_BREAK_WORDS[i].toUpperCase()) {
            return true;
        }
    }
    
    // Затем проверяем паттерны для слов с дефисами
    if (word.indexOf('-') != -1) {
        var parts = word.split('-');
        if (parts.length == 2) {
            // Проверяем первую часть
            var firstPart = parts[0];
            var secondPart = parts[1];
            
            // Паттерны для первой части
            var validFirstParts = ["Кто", "Что", "Чей", "Какой", "Который", 
                                  "Куда", "Где", "Когда", "Как", "Откуда", 
                                  "Почему", "Зачем", "Отчего", "Кое"];
            
            // Паттерны для второй части
            var validSecondParts = ["то", "нибудь", "либо", "какой", "чего"];
            
            // Проверяем первую часть
            var firstPartValid = false;
            for (var j = 0; j < validFirstParts.length; j++) {
                if (firstPart.toUpperCase() == validFirstParts[j].toUpperCase()) {
                    firstPartValid = true;
                    break;
                }
            }
            
            // Проверяем вторую часть
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
    
    // Разбивка после знаков препинания
    if ((char == ',' || char == '.' || char == '!' || char == '?' || char == ';' || char == '…') &&
        (nextChar == ' ' || nextChar == '\r' || nextChar == '\n') &&
        isUpperCase(nextNextChar)) {
        return true;
    }
    
    // Разбивка перед заглавными буквами
    if (char == ' ' && isUpperCase(nextChar)) {
        return true;
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
                    lines.push(lineSoFar);
                    currentLine = "";
                }
            }
        }
    }
    
    var lastLine = trimString(currentLine);
    if (lastLine.length > 0) {
        lines.push(lastLine);
    }
    
    return lines;
}

function findContainingParagraph(range) {
    var element = range.parentElement();
    while (element && element.nodeName != "P" && element.nodeName != "BODY") {
        element = element.parentNode;
    }
    return (element && element.nodeName == "P") ? element : null;
}

function getPlainTextWithoutFootnotes(element) {
    var text = "";
    
    function walk(node) {
        if (node.nodeType == 3) {
            text += node.nodeValue;
        } else if (node.nodeType == 1) {
            // Пропускаем сноски
            if (node.nodeName == "A") {
                var href = node.getAttribute("l:href") || node.getAttribute("href") || "";
                if (href.indexOf("#n_") == 0 || href.indexOf("#_") == 0) {
                    return; // Это сноска - пропускаем
                }
            }
            
            // Обрабатываем остальные элементы
            for (var i = 0; i < node.childNodes.length; i++) {
                walk(node.childNodes[i]);
            }
        }
    }
    
    walk(element);
    return text;
}

function findBreakPositions(plainText, textLines) {
    var positions = [];
    var currentPos = 0;
    
    for (var i = 0; i < textLines.length - 1; i++) {
        currentPos += textLines[i].length;
        
        // Пропускаем пробелы после конца строки
        while (currentPos < plainText.length && isWhitespace(plainText.charAt(currentPos))) {
            currentPos++;
        }
        
        positions.push(currentPos);
    }
    
    return positions;
}

function getPlainTextFromHTML(html) {
    if (!html) return "";
    
    // Временный элемент для парсинга
    var tempDiv = document.createElement("div");
    tempDiv.innerHTML = html;
    
    // Рекурсивно извлекаем текст
    function extractText(node) {
        var text = "";
        
        if (node.nodeType == 3) {
            text += node.nodeValue;
        } else if (node.nodeType == 1) {
            // Пропускаем комментарии, скрипты, стили
            var tagName = node.nodeName.toLowerCase();
            if (tagName != "script" && tagName != "style" && tagName != "!--") {
                for (var i = 0; i < node.childNodes.length; i++) {
                    text += extractText(node.childNodes[i]);
                }
            }
        }
        
        return text;
    }
    
    return extractText(tempDiv);
}

function findFootnoteRangesInText(html, plainText) {
    var ranges = [];
    
    // Простой поиск теги <a> с атрибутами сносок
    var inTag = false;
    var tagStart = -1;
    var currentTextIndex = 0;
    
    for (var i = 0; i < html.length; i++) {
        if (html.charAt(i) == '<' && html.substr(i, 2).toLowerCase() == '<a') {
            // Нашли начало тега <a>
            tagStart = i;
            inTag = true;
            
            // Ищем конец тега
            var tagEnd = html.indexOf('</a>', tagStart);
            if (tagEnd == -1) {
                tagEnd = html.indexOf('/>', tagStart);
            }
            
            if (tagEnd != -1) {
                // Полный тег
                var fullTag = html.substring(tagStart, tagEnd + (html.charAt(tagEnd + 1) == '>' ? 2 : 4));
                
                // Проверяем, это сноска?
                if (fullTag.indexOf('#n_') != -1 || fullTag.indexOf('#_') != -1) {
                    // Текст до тега в HTML
                    var htmlBefore = html.substring(0, tagStart);
                    var textBefore = getPlainTextFromHTML(htmlBefore);
                    
                    // Текст сноски
                    var footnoteText = getPlainTextFromHTML(fullTag);
                    var footnoteLength = footnoteText.length;
                    
                    ranges.push({
                        start: textBefore.length,
                        end: textBefore.length + footnoteLength - 1,
                        html: fullTag,
                        text: footnoteText
                    });
                }
                
                i = tagEnd + (html.charAt(tagEnd + 1) == '>' ? 1 : 3);
                inTag = false;
            }
        }
    }
    
    return ranges;
}

function adjustBreakPositionsForFootnotes(html, plainText, positions) {
    if (!html || !plainText || positions.length == 0) {
        return positions;
    }
    
    // Находим все теги сносок и их позиции в чистом тексте
    var footnoteRanges = findFootnoteRangesInText(html, plainText);
    
    if (footnoteRanges.length == 0) {
        return positions; // Сносок нет - возвращаем как есть
    }
    
    var adjustedPositions = [];
    
    for (var i = 0; i < positions.length; i++) {
        var pos = positions[i];
        var shouldAdjust = false;
        var adjustTo = pos;
        
        // Проверяем, попадает ли позиция в диапазон сноски
        for (var j = 0; j < footnoteRanges.length; j++) {
            var range = footnoteRanges[j];
            if (pos >= range.start && pos <= range.end) {
                // Позиция внутри сноски - смещаем после сноски
                shouldAdjust = true;
                adjustTo = range.end + 1;
                break;
            }
        }
        
        adjustedPositions.push(adjustTo);
    }
    
    return adjustedPositions;
}

function mapTextPositionsToHTML(html, plainText) {
    var mapping = [];
    var textIndex = 0;
    var htmlIndex = 0;
    var inTag = false;
    
    while (htmlIndex < html.length && textIndex <= plainText.length) {
        var char = html.charAt(htmlIndex);
        
        if (char == '<') {
            // Начало тега - пропускаем весь тег
            inTag = true;
            htmlIndex++;
            
            while (htmlIndex < html.length && html.charAt(htmlIndex) != '>') {
                htmlIndex++;
            }
            
            if (htmlIndex < html.length && html.charAt(htmlIndex) == '>') {
                htmlIndex++;
                inTag = false;
            }
            continue;
        }
        
        if (!inTag && char != '>') {
            // Это текстовый символ
            if (textIndex < plainText.length) {
                mapping.push({
                    textPos: textIndex,
                    htmlPos: htmlIndex
                });
                textIndex++;
            }
        }
        
        htmlIndex++;
    }
    
    return mapping;
}

function insertMarkersAtPositions(html, plainText, positions) {
    if (!html || !plainText || positions.length == 0) {
        return html;
    }
    
    // Находим соответствие между позициями в чистом тексте и позициями в HTML
    var mapping = mapTextPositionsToHTML(html, plainText);
    
    if (mapping.length == 0) {
        return html;
    }
    
    // Вставляем маркеры в обратном порядке (чтобы индексы не сдвигались)
    var result = html;
    var positionsSorted = positions.slice();
    
    // Сортируем по убыванию (для IE6 совместимости без sort с функцией)
    for (var x = 0; x < positionsSorted.length; x++) {
        for (var y = x + 1; y < positionsSorted.length; y++) {
            if (positionsSorted[x] < positionsSorted[y]) {
                var temp = positionsSorted[x];
                positionsSorted[x] = positionsSorted[y];
                positionsSorted[y] = temp;
            }
        }
    }
    
    for (var i = 0; i < positionsSorted.length; i++) {
        var textPos = positionsSorted[i];
        
        // Находим соответствующую позицию в HTML
        var htmlPos = -1;
        for (var j = 0; j < mapping.length; j++) {
            if (mapping[j].textPos == textPos) {
                htmlPos = mapping[j].htmlPos;
                break;
            }
        }
        
        if (htmlPos != -1 && htmlPos < result.length) {
            // Вставляем маркер
            result = result.substring(0, htmlPos) + '§§' + result.substring(htmlPos);
        }
    }
    
    return result;
}

// Разбивает HTML по маркерам с сохранением форматирования
function splitHTMLByMarkers(html, marker) {
    if (!html || !marker) return [html];
    
    var parts = [];
    var stack = []; // Стек открытых тегов
    var currentPart = '';
    var i = 0;
    var len = html.length;
    var inTag = false;
    var tagName = '';
    var isClosing = false;
    
    while (i < len) {
        var char = html.charAt(i);
        
        if (char === '<') {
            // Начинается тег
            inTag = true;
            tagName = '';
            isClosing = false;
            currentPart += char;
            i++;
            
            // Пропускаем пробелы и / в начале тега
            while (i < len && (html.charAt(i) === ' ' || html.charAt(i) === '/' || html.charAt(i) === '>')) {
                if (html.charAt(i) === '/') {
                    isClosing = true;
                }
                currentPart += html.charAt(i);
                i++;
            }
            
            // Собираем имя тега
            while (i < len && html.charAt(i) !== ' ' && html.charAt(i) !== '>' && html.charAt(i) !== '/') {
                tagName += html.charAt(i);
                currentPart += html.charAt(i);
                i++;
            }
            
            // Пропускаем остаток тега до >
            while (i < len && html.charAt(i) !== '>') {
                currentPart += html.charAt(i);
                i++;
            }
            
            if (i < len && html.charAt(i) === '>') {
                currentPart += '>';
                i++;
                
                // Обработка стека тегов
                tagName = tagName.toLowerCase();
                if (isClosing) {
                    // Закрывающий тег
                    for (var j = stack.length - 1; j >= 0; j--) {
                        if (stack[j] === tagName) {
                            stack.splice(j, 1);
                            break;
                        }
                    }
                } else if (tagName !== 'br' && tagName !== 'img' && tagName !== 'hr') {
                    // Открывающий тег (кроме одиночных)
                    if (html.charAt(i-2) !== '/') { // Проверяем, не самозакрывающийся ли тег
                        stack.push(tagName);
                    }
                }
                
                inTag = false;
            }
            
        } else if (!inTag && char === marker.charAt(0)) {
            // Проверяем, это маркер или часть текста
            var isFullMarker = true;
            for (var m = 0; m < marker.length; m++) {
                if (i + m >= len || html.charAt(i + m) !== marker.charAt(m)) {
                    isFullMarker = false;
                    break;
                }
            }
            
            if (isFullMarker) {
                // Нашли маркер - завершаем текущую часть
                if (currentPart.replace(/^\s+|\s+$/g, '').replace(/\s+/g, '') !== '') {
                    // Закрываем все открытые теги в текущей части
                    var closedPart = currentPart;
                    for (var s = stack.length - 1; s >= 0; s--) {
                        closedPart += '</' + stack[s] + '>';
                    }
                    parts.push(closedPart);
                    
                    // Начинаем новую часть с открытием тех же тегов
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
    
    // Добавляем последнюю часть
    if (currentPart.replace(/^\s+|\s+$/g, '').replace(/\s+/g, '') !== '') {
        parts.push(currentPart);
    }
    
    // Если что-то пошло не так - возвращаем оригинал
    return parts.length > 0 ? parts : [html];
}

// Очищает часть HTML (убирает лишние пробелы)
function cleanHTMLPart(html) {
    if (!html) return '';
    
    // Убираем пробелы в начале и конце, но сохраняем теги
    var trimmed = html;
    
    // Убираем начальные пробелы (но не внутри тегов!)
    trimmed = trimmed.replace(/^(\s*)([^<])/, '$2');
    
    // Убираем конечные пробелы
    trimmed = trimmed.replace(/([^>])(\s*)$/, '$1');
    
    return trimmed;
}

// Главная функция
function Run() {
    var version = "4.5";
    var scriptName = "Разделить один слипшийся абзац стиха на строки";
    var fullTitle = scriptName + " (версия " + version + ")";
    
    try {
        // Проверяем выделение
        var selection = document.selection;
        var range = selection.createRange();
        
        // Находим абзац
        var paragraph = findContainingParagraph(range);
        
        if (!paragraph || paragraph.nodeName != "P") {
            MsgBox(fullTitle + "\n\nКурсор должен быть в абзаце текста или должен быть выделен абзац", "FBE скрипт");
            return;
        }
        
        // Сохраняем информацию об абзаце ДО обработки
        var originalParagraphInfo = {
            element: paragraph,
            parent: paragraph.parentNode,
            nextSibling: paragraph.nextSibling,
            offsetTop: getElementOffsetTop(paragraph)
        };
        
        // Начинаем Undo-транзакцию
        window.external.BeginUndoUnit(document, scriptName);
        
        // Получаем HTML абзаца для сохранения форматирования
        var originalHTML = paragraph.innerHTML;
        
        // Получаем чистый текст без сносок для анализа разбивки
        var plainText = getPlainTextWithoutFootnotes(paragraph);
        
        if (!plainText || plainText.replace(/^\s+|\s+$/g, '').length == 0) {
            MsgBox(fullTitle + "\n\nАбзац пуст", "FBE скрипт");
            window.external.EndUndoUnit(document);
            return;
        }
        
        // Разбиваем на строки
        var textLines = splitIntoPoeticLines(plainText);
        
        if (textLines.length <= 1) {
            MsgBox(fullTitle + "\n\nНе удалось разбить на стихотворные строки. Проверьте текст.", "FBE скрипт");
            window.external.EndUndoUnit(document);
            return;
        }
        
        // Находим позиции разбивки в чистом тексте
        var breakPositions = findBreakPositions(plainText, textLines);
        
        // Корректируем позиции разбивки, чтобы не разрывать теги сносок
        var adjustedPositions = adjustBreakPositionsForFootnotes(originalHTML, plainText, breakPositions);
        
        // Подготавливаем текст с маркерами разбивки
        var textWithMarkers = insertMarkersAtPositions(originalHTML, plainText, adjustedPositions);
        
        if (!textWithMarkers || textWithMarkers == originalHTML) {
            MsgBox(fullTitle + "\n\nНе удалось вставить маркеры разбивки", "FBE скрипт");
            window.external.EndUndoUnit(document);
            return;
        }
        
        // Разбиваем HTML по маркерам с сохранением форматирования
        var htmlParts = splitHTMLByMarkers(textWithMarkers, '§§');
        
        // Удаляем маркеры из частей
        for (var i = 0; i < htmlParts.length; i++) {
            htmlParts[i] = htmlParts[i].replace(/§§/g, '');
        }
        
        // Создаем новые абзацы
        var parent = paragraph.parentNode;
        var className = paragraph.className || "";
        var firstNewParagraph = null;
        
        for (var i = 0; i < htmlParts.length; i++) {
            var partHTML = cleanHTMLPart(htmlParts[i]);
            if (!partHTML || partHTML.replace(/^\s+|\s+$/g, '').replace(/\s+/g, '') == '') {
                continue;
            }
            
            var newParagraph = document.createElement("P");
            if (className) {
                newParagraph.className = className;
            }
            newParagraph.innerHTML = partHTML;
            
            if (i == 0) {
                parent.replaceChild(newParagraph, paragraph);
                firstNewParagraph = newParagraph; // Сохраняем первый новый абзац
            } else {
                parent.insertBefore(newParagraph, paragraph.nextSibling);
            }
            paragraph = newParagraph;
        }
        
        // Снимаем выделение после обработки
        try {
            var clearRange = document.body.createTextRange();
            clearRange.collapse();
            clearRange.select();
        } catch (e) {
            // Игнорируем ошибки при снятии выделения
        }
        
        // Прокручиваем к месту разбивки
        try {
            var scrollElement = null;
            
            // Пробуем разные варианты элемента для прокрутки
            if (firstNewParagraph && firstNewParagraph.scrollIntoView) {
                scrollElement = firstNewParagraph;
            } else if (originalParagraphInfo.parent && originalParagraphInfo.parent.scrollIntoView) {
                // Прокручиваем к родительскому контейнеру
                scrollElement = originalParagraphInfo.parent;
            }
            
            if (scrollElement) {
                // Простая прокрутка к элементу
                scrollElement.scrollIntoView(false); // false = прокрутка к верху элемента
                
                // Дополнительная попытка центрирования
                try {
                    var elementTop = getElementOffsetTop(scrollElement);
                    var windowHeight = document.documentElement.clientHeight || document.body.clientHeight || 500;
                    
                    // Вычисляем позицию для центрирования
                    var targetScroll = elementTop - Math.floor(windowHeight / 3);
                    
                    // Устанавливаем позицию прокрутки
                    if (document.documentElement && document.documentElement.scrollTop !== undefined) {
                        document.documentElement.scrollTop = targetScroll;
                    } else if (document.body && document.body.scrollTop !== undefined) {
                        document.body.scrollTop = targetScroll;
                    }
                } catch (e) {
                    // Если не получилось центрировать, оставляем базовую прокрутку
                }
            }
        } catch (scrollError) {
            // Игнорируем ошибки прокрутки
        }
        
        MsgBox(fullTitle + "\n\nАбзац разбит на " + htmlParts.length + " строк\n\nФорматирование и сноски сохранены", "FBE скрипт");
        window.external.EndUndoUnit(document);
        
    } catch (e) {
        try { window.external.EndUndoUnit(document); } catch(e2) {}
        MsgBox(fullTitle + "\n\nОшибка: " + e.message, "FBE скрипт");
    }
}
