// Скрипт "Расформатировать 'буквицы' от жирности и курсива" для редактора FBE 
// version 1.8
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для расформатирования от жирности и курсива
// в fb2 документах так называемых "Буквиц" "т.е. первых" 1-6 символов абзацев,
// следующих сразу после заголовка или подзаголовка.
// Остальное форматирование обрабатываемого абзаца не затрагивается.
// Абзацы, следующие НЕ после заголовка или подзаголовка не затрагиваются.
// Сами заголовки и подзаголовки не затрагиваются.
// Поддержка отмены действий (Ctrl+Z).

// Примеры:
// Мама мыла раму. (простой случай, расформатируется только первая заглавная М)
// — … «Ёлочки» кавычки расставлены во всем документе (расформатируется — … «Ё - тире+пробел+многоточие+пробел+кавычка+первая заглавная Ё )

// version 1.8, 01.12.2025
// ============================================

function Run() {
    var scriptName = "Расформатировать 'буквицы'  от жирности и курсива";
    var scriptVersion = "1.8";
    
    window.external.BeginUndoUnit(document, scriptName + " v" + scriptVersion);
    
    var statsTotal = 0;
    var statsModified = 0;
    var statsSkipped = 0;
    
    // Получаем текущий символ неразрывного пробела из настроек FBE
    var nbspChar;
    try {
        nbspChar = window.external.GetNBSP();
    } catch(e) {
        nbspChar = String.fromCharCode(160); // стандартный &nbsp;
    }
    
    // Получаем все абзацы в документе
    var paragraphs = document.getElementsByTagName("P");
    
    // Флаг, что мы находимся после заголовка
    var afterTitle = false;
    
    for (var i = 0; i < paragraphs.length; i++) {
        var p = paragraphs[i];
        statsTotal++;
        
        // Пропускаем пустые абзацы
        if (isParagraphEmpty(p, nbspChar)) {
            statsSkipped++;
            continue;
        }
        
        // Проверяем, является ли абзац заголовком
        if (isTitleParagraph(p)) {
            afterTitle = true;
            statsSkipped++;
            continue;
        }
        
        // Если не после заголовка - пропускаем
        if (!afterTitle) {
            statsSkipped++;
            continue;
        }
        
        // Сбрасываем флаг после первого непустого абзаца после заголовка
        afterTitle = false;
        
        // Пропускаем списки
        if (isNumericList(p, nbspChar)) {
            statsSkipped++;
            continue;
        }
        
        // Пропускаем короткие абзацы, полностью отформатированные
        if (isFullyFormattedShortParagraph(p, nbspChar)) {
            statsSkipped++;
            continue;
        }
        
        // Обрабатываем буквицу
        if (processBoldFirstChars(p, nbspChar)) {
            statsModified++;
        } else {
            statsSkipped++;
        }
    }
    
    // Показываем результат
    var message = scriptName + "\n";
    message += "Version: " + scriptVersion + "\n\n";
    message += "Всего абзацев в документе: " + statsTotal + "\n";
    message += "Изменено абзацев (после заголовков, подзаголовков): " + statsModified + "\n";
    message += "Пропущено: " + statsSkipped;
    
    MsgBox(message, "FBE скрипт");
    
    window.external.EndUndoUnit(document);
    return "Done";
}

// Проверяем, является ли абзац заголовком
function isTitleParagraph(p) {
    // Проверяем родительский элемент
    var parent = p.parentNode;
    if (parent && parent.className && 
        (parent.className.indexOf('title') !== -1 || 
         parent.className.indexOf('subtitle') !== -1)) {
        return true;
    }
    
    // Проверяем непосредственные стили заголовка
    var styles = ["title", "subtitle", "h1", "h2", "h3", "h4", "h5", "h6"];
    for (var i = 0; i < styles.length; i++) {
        if (p.className && p.className.indexOf(styles[i]) !== -1) {
            return true;
        }
    }
    
    return false;
}

// Проверяем, является ли абзац коротким и полностью отформатированным
function isFullyFormattedShortParagraph(p, nbspChar) {
    var html = p.innerHTML;
    var text = getNormalizedText(p, nbspChar);
    
    // Считаем длину текста без пробелов
    var textWithoutSpaces = text.replace(/\s/g, '');
    
    // Если текст очень короткий (меньше 10 символов без пробелов)
    if (textWithoutSpaces.length < 10) {
        // Проверяем, полностью ли он в тегах форматирования
        var formatElements = [];
        findAllFormatElementsDeep(p, formatElements);
        
        if (formatElements.length > 0) {
            // Получаем весь текст из всех тегов форматирования
            var formattedText = "";
            for (var i = 0; i < formatElements.length; i++) {
                formattedText += getNormalizedText(formatElements[i], nbspChar);
            }
            
            // Сравниваем с общим текстом
            formattedText = formattedText.replace(/\s/g, '');
            textWithoutSpaces = textWithoutSpaces.replace(/\s/g, '');
            
            // Если весь текст отформатирован - это короткий отформатированный абзац
            return formattedText === textWithoutSpaces;
        }
    }
    
    return false;
}

// Получаем первый текст элемента
function getFirstText(element) {
    for (var i = 0; i < element.childNodes.length; i++) {
        var child = element.childNodes[i];
        if (child.nodeType == 3) {
            return child.nodeValue;
        } else if (child.nodeType == 1) {
            var text = getFirstText(child);
            if (text) return text;
        }
    }
    return "";
}

// Получаем весь текст элемента с учетом неразрывных пробелов
function getAllText(element, nbspChar) {
    var result = "";
    for (var i = 0; i < element.childNodes.length; i++) {
        var child = element.childNodes[i];
        if (child.nodeType == 3) {
            result += child.nodeValue;
        } else if (child.nodeType == 1) {
            result += getAllText(child, nbspChar);
        }
    }
    return result;
}

// Проверка пустого абзаца с учетом неразрывных пробелов
function isParagraphEmpty(p, nbspChar) {
    var html = p.innerHTML;
    var text = html.replace(/<[^>]*>/g, '');
    text = text.replace(/&nbsp;/g, ' ');
    text = text.replace(new RegExp(nbspChar, 'g'), ' ');
    return text.replace(/\s/g, '').length === 0;
}

// Проверка на нумерованный список
function isNumericList(p, nbspChar) {
    var text = getFirstText(p);
    if (!text) return false;
    
    // Нормализуем пробелы
    text = text.replace(new RegExp(nbspChar, 'g'), ' ');
    text = text.replace(/&nbsp;/g, ' ');
    
    var patterns = [
        /^[1-9]\d*\.\s+/,
        /^[1-9]\d*\)\s+/,
        /^[1-9]\d*\.\)\s+/,
        /^[IVXLCDM]+\.\s+/,
        /^[ivxlcdm]+\.\s+/,
        /^[а-я]\.\s+/
    ];
    
    for (var j = 0; j < patterns.length; j++) {
        if (patterns[j].test(text)) {
            return true;
        }
    }
    
    return false;
}

// Получаем текст элемента, заменяя неразрывные пробелы на обычные для сравнения
function getNormalizedText(element, nbspChar) {
    var text = getAllText(element, nbspChar);
    // Заменяем неразрывные пробелы на обычные для сравнения
    text = text.replace(new RegExp(nbspChar, 'g'), ' ');
    text = text.replace(/&nbsp;/g, ' ');
    return text;
}

// Проверяем, является ли тег жирным/курсивом
function isFormatTag(node) {
    var tag = node.nodeName.toUpperCase();
    return (tag == "STRONG" || tag == "EM" || tag == "B" || tag == "I" || tag == "EMPHASIS");
}

// Основная обработка буквицы
function processBoldFirstChars(p, nbspChar) {
    // Получаем весь текст абзаца (нормализованный)
    var allText = getNormalizedText(p, nbspChar);
    
    // Ищем первый тег форматирования в абзаце
    var firstFormatElement = findFirstFormatElement(p);
    if (!firstFormatElement) return false;
    
    var boldText = getNormalizedText(firstFormatElement, nbspChar);
    
    // Пропускаем слишком длинные элементы
    if (boldText.length < 1 || boldText.length > 8) return false;
    
    // Находим позицию этого текста в абзаце
    var pos = findTextPositionInElement(p, firstFormatElement, allText, nbspChar);
    if (pos === -1) return false;
    
    // Проверяем, что форматирование в начале абзаца (первые 10 символов)
    if (pos > 10) return false;
    
    // Проверяем, что форматированный текст содержит букву в начале или после допустимых префиксов
    if (!containsLetterAfterValidPrefix(boldText)) return false;
    
    // Проверяем, что весь текст от начала абзаца до конца форматирования - это валидная буквица
    var textBefore = allText.substring(0, pos);
    var combined = textBefore + boldText;
    
    if (!isValidBoldFirstCharFull(combined)) return false;
    
    // Проверяем, что после буквицы в абзаце есть еще обычный текст
    if (!hasPlainTextAfter(p, firstFormatElement, nbspChar)) return false;
    
    // Убираем форматирование
    unwrapElementCompletely(firstFormatElement);
    return true;
}

// Ищем первый тег форматирования в абзаце (самый глубокий вложенный)
function findFirstFormatElement(element) {
    // Сначала ищем во всех дочерних элементах
    for (var i = 0; i < element.childNodes.length; i++) {
        var child = element.childNodes[i];
        if (child.nodeType == 1) {
            if (isFormatTag(child)) {
                // Нашли тег форматирования, теперь ищем самый глубокий вложенный
                return findDeepestFormatElement(child);
            } else {
                // Рекурсивно ищем в других элементах
                var found = findFirstFormatElement(child);
                if (found) return found;
            }
        }
    }
    return null;
}

// Ищем самый глубокий тег форматирования внутри элемента
function findDeepestFormatElement(element) {
    // Проверяем, есть ли внутри другие теги форматирования
    var deepest = element;
    
    for (var i = 0; i < element.childNodes.length; i++) {
        var child = element.childNodes[i];
        if (child.nodeType == 1 && isFormatTag(child)) {
            var childDeepest = findDeepestFormatElement(child);
            if (childDeepest) {
                deepest = childDeepest;
            }
        }
    }
    
    return deepest;
}

// Проверяем, есть ли обычный текст после элемента
function hasPlainTextAfter(p, element, nbspChar) {
    var foundElement = false;
    var hasTextAfter = false;
    
    function traverse(node) {
        if (node === element) {
            foundElement = true;
            return;
        }
        
        if (foundElement && node.nodeType == 3) {
            var text = node.nodeValue;
            text = text.replace(new RegExp(nbspChar, 'g'), ' ');
            text = text.replace(/&nbsp;/g, ' ');
            text = text.replace(/\s/g, '');
            
            if (text.length > 0) {
                hasTextAfter = true;
            }
        }
        
        if (!hasTextAfter && node.nodeType == 1) {
            for (var i = 0; i < node.childNodes.length; i++) {
                traverse(node.childNodes[i]);
                if (hasTextAfter) break;
            }
        }
    }
    
    traverse(p);
    return hasTextAfter;
}

// Рекурсивно находим ВСЕ элементы форматирования (включая вложенные)
function findAllFormatElementsDeep(element, result) {
    for (var i = 0; i < element.childNodes.length; i++) {
        var child = element.childNodes[i];
        if (child.nodeType == 1) {
            if (isFormatTag(child)) {
                result.push(child);
            }
            findAllFormatElementsDeep(child, result);
        }
    }
}

// Проверяем, содержит ли текст букву в начале или после допустимых префиксов
function containsLetterAfterValidPrefix(text) {
    if (!text) return false;
    
    var letterMatch = /[A-ZА-ЯЁ]/i.exec(text);
    if (!letterMatch) return false;
    
    var letterPos = letterMatch.index;
    var prefix = text.substring(0, letterPos);
    
    var validPrefixPattern = /^[\s\u00A0—\-–\.…«"\'`]*$/;
    return validPrefixPattern.test(prefix);
}

// Полная проверка буквицы
function isValidBoldFirstCharFull(text) {
    if (!text) return false;
    
    var letterMatch = /[A-ZА-ЯЁ]/i.exec(text);
    if (!letterMatch) return false;
    
    var letterPos = letterMatch.index;
    var prefix = text.substring(0, letterPos);
    
    if (!/[A-ZА-ЯЁ]/.test(text.charAt(letterPos))) return false;
    if (text.length > 8) return false;
    
    var validPrefixPattern = /^[\s\u00A0—\-–\.…«"\'`]*$/;
    if (!validPrefixPattern.test(prefix)) return false;
    
    return true;
}

// Находим позицию текста элемента в общем тексте
function findTextPositionInElement(container, targetElement, allText, nbspChar) {
    var textBefore = "";
    var found = false;
    
    function traverse(node) {
        if (node === targetElement) {
            found = true;
            return;
        }
        
        if (!found && node.nodeType == 3) {
            textBefore += node.nodeValue;
        } else if (!found && node.nodeType == 1) {
            for (var i = 0; i < node.childNodes.length; i++) {
                traverse(node.childNodes[i]);
                if (found) break;
            }
        }
    }
    
    traverse(container);
    
    if (!found) return -1;
    
    textBefore = textBefore.replace(new RegExp(nbspChar, 'g'), ' ');
    textBefore = textBefore.replace(/&nbsp;/g, ' ');
    
    return textBefore.length;
}

// Убираем форматирование полностью (включая вложенные теги)
function unwrapElementCompletely(element) {
    var parent = element.parentNode;
    if (!parent) return;
    
    // Сначала рекурсивно разбираем всех детей-тегов форматирования
    var children = [];
    for (var i = 0; i < element.childNodes.length; i++) {
        children.push(element.childNodes[i]);
    }
    
    for (var i = 0; i < children.length; i++) {
        var child = children[i];
        if (child.nodeType == 1 && isFormatTag(child)) {
            unwrapElementCompletely(child);
        }
    }
    
    // Перемещаем всех оставшихся детей на уровень выше
    while (element.firstChild) {
        parent.insertBefore(element.firstChild, element);
    }
    
    // Удаляем пустой элемент
    parent.removeChild(element);
    
    // Если родитель тоже тег форматирования и теперь содержит только текст
    if (parent && isFormatTag(parent) && 
        parent.childNodes.length === 1 && 
        parent.firstChild.nodeType === 3) {
        unwrapElementCompletely(parent);
    }
}
