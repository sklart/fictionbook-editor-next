// Скрипт "Расставить подписи к иллюстрациям из выделенного фрагмента текста" для редактора FBE 
// version 3.3
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для расстановки подписей к иллюстрациям в fb2 документах
// из выделенного фрагмента текста, содержащего все подписи сразу ко всем иллюстрациям.
// Абзацы из выделенного фрагмента переносятся к имеющимся в документе иллюстрациями.
// В скрипте реализован самый простой случай - кол-во иллюстраций = кол-ву абзацев подписей к ним.
// Перед запуском данного скрипта рекомендуется произвести унификацию иллюстраций
// и удаление неиспользуемых вложений  соответствующими скриптами из папки скриптов 05_Иллюстрации.
// Также не помешает произвести удаление дублей прикрепленных бинарников картинок
// скриптом 17_Исключение копий вложенных файлов.js из папки скриптов 05_Иллюстрации.
// ============================================

// Для успешного расставления подписей к иллюстрациям,
// количество расставленных в тексте картинок
// и кол-во исходных абзацев будущих подписей к ним должны совпадать.

// Исходные подписи должны быть каждая - одним абзацем.
// Пустых строк между исходными абзацами подписей не должно быть.
// Допустимые маркеры для обозначения вторых, третьих и тд.
// абзацев подписей могут быть ~ или ~~ или ++

// Исходное форматирование абзацев будущих подписей (болд, курсив)
// при переносе их к картинкам - сохраняется.

// Возможная нумерация абзацев будущих подписей (в формате 1. или 1))
// может автоматически удаляться по запросу скрипта.

// Исходные тексты подписей также могут автоматически удаляться
// по запросу скрипта.

// version 3.3, 03.12.2025
// ============================================


function Run() {
    try {
        var scriptName = "Расставить подписи к иллюстрациям из выделенного фрагмента текста";
        var scriptVersion = "3.3";
        
        // Получаем неразрывный пробел
        var nbspChar, nbspEntity;
        try {
            nbspChar = window.external.GetNBSP();
            nbspEntity = (nbspChar.charCodeAt(0) == 160) ? "&nbsp;" : nbspChar;
        } catch(e) {
            nbspChar = String.fromCharCode(160);
            nbspEntity = "&nbsp;";
        }
        
        // Проверяем выделение
        if (!document.selection || document.selection.type.toLowerCase() !== "text") {
            MsgBox("Вы ничего не выделили.\n\nПеред запуском данного скрипта, пожалуйста, выделите абзацы с подписей.", 
                   scriptName + " (" + scriptVersion + ")");
            return;
        }
        
        var myRange = document.selection.createRange();
        if (!myRange.text || myRange.text.replace(/^\s+|\s+$/g, '').replace(/\s+/g, '') === '') {
            MsgBox("Выделение пустое или содержит только пробелы!", scriptName + " (" + scriptVersion + ")");
            return;
        }
        
        // Сохраняем диапазон выделения для возможного удаления
        var originalSelectionRange = myRange.duplicate();
        
        // Получаем HTML выделения
        var selectedHTML = myRange.htmlText;
        
        // Находим все блочные картинки
        var allBlockImages = findAllBlockImagesSimple();
        
        // Извлекаем абзацы из HTML
        var captions = extractHTMLParagraphs(selectedHTML);
        
        // Проверяем, есть ли нумерация в подписях
        var hasNumbering = checkForNumberingSimple(captions);
        
        // Форматированное сообщение о найденном
        var foundMessage = "---------------------------\n" +
                          scriptName + "\n" +
                          "Version: " + scriptVersion + "\n" +
                          "---------------------------\n" +
                          "Найдено:\n" +
                          "Блочных картинок: " + allBlockImages.length + "\n" +
                          "Абзацев подписей: " + captions.length + "\n" +
                          "---------------------------";
        
        MsgBox(foundMessage, "FBE скрипт");
        
        // Проверяем количество
        if (allBlockImages.length !== captions.length) {
            MsgBox("Количество не совпадает!\nКартинок: " + allBlockImages.length + "\nПодписей: " + captions.length,
                   "FBE скрипт");
            return;
        }
        
        // Подтверждение расстановки
        if (!confirm("Расставить " + captions.length + " подписей к " + allBlockImages.length + " картинкам?")) {
            return;
        }
        
        // Запрос на удаление нумерации (если она есть)
        var removeNumbering = false;
        if (hasNumbering) {
            removeNumbering = confirm("В подписях обнаружена нумерация (1., 2., 1), 2) и т.д.).\n\n" +
                                   "Удалить нумерацию из расставляемых подписей?\n\n" +
                                   "Рекомендуется: ДА, так как нумерация обычно нужна только в исходном списке.");
        }
        
        // Запрос на удаление исходного фрагмента
        var deleteOriginal = false;
        if (captions.length > 0) {
            deleteOriginal = confirm("После расстановки подписей удалить исходный выделенный фрагмент с текстами подписей?\n\n" +
                                    "Рекомендуется: ДА, чтобы избежать дублирования текста в документе.");
        }
        
        // Начинаем транзакцию
        window.external.BeginUndoUnit(document, scriptName);
        
        // Расставляем подписи
        var successCount = 0;
        var numberingRemovedCount = 0;
        
        for (var i = 0; i < allBlockImages.length; i++) {
            var captionHTML = captions[i].html;
            var originalHTML = captionHTML;
            
            // Удаляем нумерацию, если запрошено
            if (removeNumbering) {
                captionHTML = removeNumberingWithDotInTag(captionHTML);
                if (captionHTML !== originalHTML) {
                    numberingRemovedCount++;
                }
            }
            
            if (insertCaptionWithFormattingFixed(allBlockImages[i].element, captionHTML, nbspEntity)) {
                successCount++;
            }
        }
        
        // Удаляем исходный фрагмент, если запрошено
        var deleteResult = "";
        if (deleteOriginal && successCount > 0) {
            try {
                originalSelectionRange.select();
                originalSelectionRange.text = "";
                deleteResult = "Исходный фрагмент удален.";
            } catch(e) {
                deleteResult = "Не удалось удалить исходный фрагмент.";
            }
        } else if (deleteOriginal) {
            deleteResult = "Исходный фрагмент НЕ удален (подписи не были расставлены).";
        } else {
            deleteResult = "Исходный фрагмент сохранен.";
        }
        
        // Добавляем информацию об удалении нумерации
        var numberingResult = "";
        if (removeNumbering) {
            numberingResult = "Нумерация удалена из " + numberingRemovedCount + " подписей.";
        }
        
        // Форматированное сообщение о результате
        var resultMessage = "---------------------------\n" +
                          scriptName + "\n" +
                          "Version: " + scriptVersion + "\n" +
                          "---------------------------\n" +
                          "Успешно расставлено: " + successCount + " подписей\n" +
                          deleteResult;
        
        if (numberingResult) {
            resultMessage += "\n" + numberingResult;
        }
        
        resultMessage += "\n---------------------------";
        
        // Результат
        MsgBox(resultMessage, "FBE скрипт");
        
    } catch (error) {
        MsgBox("Ошибка: " + error.message, scriptName + " (ошибка)");
    }
}

// Проверить наличие нумерации в подписях (простая версия)
function checkForNumberingSimple(captions) {
    if (!captions || captions.length === 0) return false;
    
    var numberingPatterns = [
        /^\s*[0-9]{1,3}\.\s/,        // 1. текст
        /^\s*[0-9]{1,3}\)\s/,        // 1) текст
        /^\s*[0-9]{1,3}\s/,          // 1 текст
        /^\s*[0-9]{1,3}\.\s*$/,      // 1. (без текста после)
        /^\s*[0-9]{1,3}\)\s*$/       // 1) (без текста после)
    ];
    
    for (var i = 0; i < captions.length; i++) {
        var caption = captions[i];
        if (!caption || !caption.html) continue;
        
        // Получаем текстовое содержимое HTML
        var textContent = getPlainTextFromHTML(caption.html);
        
        // Проверяем каждый паттерн
        for (var p = 0; p < numberingPatterns.length; p++) {
            if (numberingPatterns[p].test(textContent)) {
                return true; // Нашли нумерацию хотя бы в одной подписи
            }
        }
    }
    
    return false;
}

// Получить plain text из HTML (простая версия)
function getPlainTextFromHTML(html) {
    if (!html) return "";
    
    // Удаляем теги
    var text = html.replace(/<[^>]*>/g, ' ');
    
    // Заменяем HTML-сущности
    text = text.replace(/&nbsp;/g, ' ');
    text = text.replace(/&amp;/g, '&');
    text = text.replace(/&lt;/g, '<');
    text = text.replace(/&gt;/g, '>');
    text = text.replace(/&quot;/g, '"');
    
    // Убираем лишние пробелы
    text = text.replace(/\s+/g, ' ');
    text = text.replace(/^\s+|\s+$/g, '');
    
    return text;
}

// Удалить нумерацию с учетом точки внутри тега
function removeNumberingWithDotInTag(html) {
    if (!html) return html;
    
    var originalHTML = html;
    
    try {
        // Специальная обработка для случая типа: <p>6<strong>. текст</strong></p>
        // Сначала попробуем простую замену для стандартных случаев
        var simplePatterns = [
            /^(<p[^>]*>\s*)([0-9]{1,3}\.\s+)/i,
            /^(<p[^>]*>\s*)([0-9]{1,3}\)\s+)/i,
            /^(<p[^>]*>\s*)([0-9]{1,3}\s+)/i
        ];
        
        for (var i = 0; i < simplePatterns.length; i++) {
            if (simplePatterns[i].test(html)) {
                var newHtml = html.replace(simplePatterns[i], '$1');
                if (getPlainTextFromHTML(newHtml).replace(/\s/g, '') !== '') {
                    return newHtml;
                }
            }
        }
        
        // Если простые замены не сработали, создаем DOM для анализа
        var tempDiv = document.createElement('div');
        tempDiv.innerHTML = html;
        
        // Собираем все текстовые узлы
        var textNodes = [];
        collectTextNodes(tempDiv, textNodes);
        
        if (textNodes.length === 0) {
            return html;
        }
        
        // Анализируем первые два узла для нашего конкретного случая
        // Случай: цифра в первом узле, точка во втором узле (который внутри тега)
        if (textNodes.length >= 2) {
            var firstText = textNodes[0].nodeValue || "";
            var secondText = textNodes[1].nodeValue || "";
            
            // Проверяем паттерн: "цифра" + "точка"
            var numberMatch = firstText.match(/^\s*([0-9]{1,3})\s*$/);
            var dotMatch = secondText.match(/^\s*(\.|\))\s*/);
            
            if (numberMatch && dotMatch) {
                // Нашли наш случай! Удаляем цифру из первого узла
                textNodes[0].nodeValue = "";
                
                // Удаляем точку/скобку из начала второго узла
                textNodes[1].nodeValue = secondText.substring(dotMatch[0].length);
                
                // Получаем обновленный HTML
                var newHtml = tempDiv.innerHTML;
                
                // Проверяем, что остался текст
                if (getPlainTextFromHTML(newHtml).replace(/\s/g, '') !== '') {
                    return newHtml;
                }
            }
        }
        
        // Проверяем другие возможные паттерны
        var allText = getPlainTextFromHTML(html);
        var numberingMatch = allText.match(/^\s*([0-9]{1,3})(\.|\)|\s+)/);
        
        if (numberingMatch) {
            // Пробуем удалить через полный анализ DOM
            var numberingLength = numberingMatch[0].length;
            
            // Проходим по всем узлам и удаляем нужное количество символов с начала
            var remaining = numberingLength;
            for (var j = 0; j < textNodes.length; j++) {
                if (remaining <= 0) break;
                
                var node = textNodes[j];
                var text = node.nodeValue || "";
                
                if (text.length <= remaining) {
                    node.nodeValue = "";
                    remaining -= text.length;
                } else {
                    node.nodeValue = text.substring(remaining);
                    remaining = 0;
                }
            }
            
            var newHtml = tempDiv.innerHTML;
            
            // Убираем пустые элементы
            newHtml = newHtml.replace(/<[^>]+>\s*<\/[^>]+>/g, '');
            
            if (getPlainTextFromHTML(newHtml).replace(/\s/g, '') !== '') {
                return newHtml;
            }
        }
        
        return originalHTML;
        
    } catch(e) {
        return originalHTML;
    }
}

// Собрать все текстовые узлы
function collectTextNodes(element, resultArray) {
    if (!element) return;
    
    for (var i = 0; i < element.childNodes.length; i++) {
        var child = element.childNodes[i];
        
        if (child.nodeType === 3) { // Текстовый узел
            var text = child.nodeValue || "";
            if (text.replace(/\s/g, '') !== '') {
                resultArray.push(child);
            }
        } else if (child.nodeType === 1) { // Элемент
            collectTextNodes(child, resultArray);
        }
    }
}

// Извлечь абзацы из HTML
function extractHTMLParagraphs(html) {
    var paragraphs = [];
    if (!html) return paragraphs;
    
    try {
        var tempDiv = document.createElement('div');
        tempDiv.style.position = 'absolute';
        tempDiv.style.left = '-9999px';
        tempDiv.innerHTML = html;
        document.body.appendChild(tempDiv);
        
        var pTags = tempDiv.getElementsByTagName('P');
        
        for (var i = 0; i < pTags.length; i++) {
            var htmlContent = pTags[i].innerHTML;
            paragraphs.push({
                html: htmlContent
            });
        }
        
        if (paragraphs.length === 0) {
            paragraphs.push({
                html: html
            });
        }
        
        document.body.removeChild(tempDiv);
        
    } catch(e) {}
    
    return paragraphs;
}

// Найти все блочные картинки
function findAllBlockImagesSimple() {
    var images = [];
    
    try {
        var allElements = document.getElementsByTagName('*');
        
        for (var i = 0; i < allElements.length; i++) {
            var element = allElements[i];
            var className = element.className || '';
            
            if (typeof className === 'string' && className.indexOf('image') !== -1) {
                var tagName = element.tagName ? element.tagName.toUpperCase() : '';
                
                if (tagName === 'DIV') {
                    var href = element.getAttribute('href') || element.getAttribute('l:href') || '';
                    
                    var isEmpty = (href === '#undefined' || href.indexOf('undefined') !== -1 || href === '#');
                    if (!isEmpty && href && href !== '#') {
                        images.push({
                            element: element,
                            href: href
                        });
                    }
                }
            }
        }
        
    } catch (e) {}
    
    return images;
}

// Вставить подпись с исправленным сохранением форматирования
function insertCaptionWithFormattingFixed(imageElement, htmlCaption, nbspEntity) {
    try {
        var parent = imageElement.parentNode;
        if (!parent) return false;
        
        // 1. Определяем маркер
        var marker = determineMarker(htmlCaption);
        
        // 2. Если маркера нет - вставляем как есть
        if (!marker) {
            return insertSingleCaptionSimple(imageElement, htmlCaption, nbspEntity);
        }
        
        // 3. Разбиваем на части с сохранением форматирования
        var parts = splitHTMLByMarkerWithFormatting(htmlCaption, marker);
        
        // 4. Если только одна часть - вставляем как есть
        if (parts.length <= 1) {
            return insertSingleCaptionSimple(imageElement, htmlCaption, nbspEntity);
        }
        
        // 5. Вставляем все части
        var insertPoint = imageElement.nextSibling;
        
        for (var j = 0; j < parts.length; j++) {
            var part = cleanHTMLPart(parts[j]);
            if (part && part.replace(/^\s+|\s+$/g, '').replace(/\s+/g, '') !== '') {
                var p = document.createElement('p');
                p.innerHTML = part;
                parent.insertBefore(p, insertPoint);
            }
        }
        
        // Пустая строка после всех частей
        var emptyLine = document.createElement('p');
        emptyLine.innerHTML = nbspEntity;
        parent.insertBefore(emptyLine, insertPoint);
        
        return true;
        
    } catch (e) {
        // Fallback на простую вставку
        return insertSingleCaptionSimple(imageElement, htmlCaption, nbspEntity);
    }
}

// Разбить HTML по маркеру с сохранением форматирования
function splitHTMLByMarkerWithFormatting(html, marker) {
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

// Вставить одну подпись (без разбивки)
function insertSingleCaptionSimple(imageElement, htmlCaption, nbspEntity) {
    try {
        var parent = imageElement.parentNode;
        if (!parent) return false;
        
        var insertPoint = imageElement.nextSibling;
        
        var p = document.createElement('p');
        p.innerHTML = cleanHTMLPart(htmlCaption);
        parent.insertBefore(p, insertPoint);
        
        var emptyLine = document.createElement('p');
        emptyLine.innerHTML = nbspEntity;
        parent.insertBefore(emptyLine, insertPoint);
        
        return true;
        
    } catch (e) {
        return false;
    }
}

// Определить какой маркер используется
function determineMarker(html) {
    if (!html) return null;
    
    // Сначала проверяем двойные маркеры
    if (html.indexOf('~~') !== -1) return '~~';
    if (html.indexOf('++') !== -1) return '++';
    if (html.indexOf('~') !== -1) return '~';
    
    return null;
}

// Очистить часть HTML (убрать лишние пробелы, сохранить теги)
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
