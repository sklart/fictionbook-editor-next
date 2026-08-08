// Скрипт "Разметить заголовки безымянных разделов из выделенного фрагмента" для редактора FBE
// version 2.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для автоматической расстановки заголовков в БЕЗЫМЯННЫЕ разделы (секции)
// fb2-документа из выделенного фрагмента с оглавлением.

// Фрагмент с оглавлением можно расположить в любой секции документа в основном боди.
// Удобнее это сделать где-то в начале или в конце документа.

// Порядок использования:
// 1. Выделите в документе фрагмент с текстовым оглавлением
// (список будущих заголовков, каждый с новой строки) и запустите скрипт.
// 2. Ответьте на вопросы скрипта в диалоговых окнах
// 3. Проверьте результат

// Как работает скрипт:
// 1. Скрипт извлекает из выделенного фрагмента абзацы-заголовки
// 2. Находит все безымянные секции (разделы без заголовков) в документе
// 3. Сопоставляет количество заголовков и секций (не обязательно должно совпадать)
// 4. Расставляет заголовки в безымянные секции по порядку
// 5. Если количество заголовков и секций не совпадает, скрипт предложит варианты
// 6. Скрипт работает только с безымянными секциями (без тега <title>)
// 7. Форматирование в создаваемых заголовках не сохраняется (используется plain text)

// Особенности:
// - Скрипт игнорирует пустые строки в выделенном фрагменте.
// - Скрипт нормализует все типы пробелов в выделенном фрагменте (с учетом особенностей FBE).
// - Скрипт умеет пропускать любое указанное кол-во "первых" безымянных секций от начала документа.
// - Скрипт умеет расставлять заголовки безымянных секций, даже если кол-во заголовков и секций не совпадает.
// - В таком слечае либо останутся лишние заголовки, либо лишние пустые секции.
// - По умолчанию - 1 секцию (настраивается внутри и есть в диалоге скрипта).
// - Автоматически исправляет точки без пробелов в заголовках (например: "1.Название" → "1. Название").
// - Если кол-во абзацев заголовков в выделенном фрагменте вдвое больше кол-ва безымянных секций,
//   скрипт предложит создать заголовки из 2 абзацев.
// - После расстановки скрипт предлагает удалить исходный фрагмент с оглавлением (можно отключить).
// - Показывает предварительную и окончательную статистику расстановки.

// Настройки (можно менять в теле скрипта):
// - processSectionWithSelection: 1/0 - включать ли в обработку и расстановку заголовков
//   секцию с выделенным фрагментом оглавления
// - skipFirstSectionsCount: N - сколько "первых" безымянных секций пропустить (по умолчанию: 1)
// - deleteOriginalSelection: 1/0 - запрашивать удаление исходного выделенного фрагмента

// version 2.2, 17.12.2025
// ======================================

function Run() {
    // Название и версия скрипта для сообщений
    var scriptName = "Разметить заголовки безымянных разделов из выделенного фрагмента";
    var scriptVersion = "2.2";
    
    // Настройки скрипта (можно менять перед запуском)
    var SETTINGS = {
        // Включать ли секцию с выделенным фрагментом в обработку?
        // 1 = да (включать в подсчет), 0 = нет (исключать из подсчета)
        processSectionWithSelection: 1,
        
        // Сколько первых безымянных секций пропустить? (0 - не пропускать, N - пропустить N секций)
        // По умолчанию: 1 (пропустить первую секцию)
        skipFirstSectionsCount: 1,
        
        // Удалить исходный выделенный фрагмент после расстановки? (0 - нет, 1 - запросить)
        deleteOriginalSelection: 1
    };
    
    // Замер времени начала РЕАЛЬНОЙ работы скрипта
    var scriptStartTime = null;
    
    // Получаем неразрывный пробел из настроек FBE
    var nbspChar, nbspEntity;
    try {
        nbspChar = window.external.GetNBSP();
        nbspEntity = (nbspChar.charCodeAt(0) == 160) ? "&nbsp;" : nbspChar;
    } catch(e) {
        nbspChar = String.fromCharCode(160);
        nbspEntity = "&nbsp;";
    }
    
    // Проверяем, есть ли выделение
    if (!document.selection || document.selection.type.toLowerCase() !== "text") {
        MsgBox("Вы ничего не выделили.\n\nПеред запуском скрипта выделите фрагмент с оглавлением.", 
               scriptName + " (" + scriptVersion + ")");
        return;
    }
    
    var selectionRange = document.selection.createRange();
    if (!selectionRange.text || selectionRange.text.replace(/^\s+|\s+$/g, '').replace(/\s+/g, '') === '') {
        MsgBox("Выделение пустое или содержит только пробелы!", scriptName + " (" + scriptVersion + ")");
        return;
    }
    
    // Сохраняем информацию о выделения для проверки секции
    var originalSelection = {
        range: selectionRange.duplicate(),
        html: selectionRange.htmlText,
        parentSection: null
    };
    
    // Находим секцию, в которой находится выделение
    originalSelection.parentSection = findParentSection(selectionRange.parentElement());
    
    // 1. Извлекаем абзацы-заголовки из выделенного фрагмента
    var headers = extractHeadersFromSelection(selectionRange.htmlText, nbspEntity);
    
    if (headers.length === 0) {
        MsgBox("В выделенном фрагменте не найдено абзацев для заголовков.", scriptName + " (" + scriptVersion + ")");
        return;
    }
    
    // 2. Находим все безымянные секции в документе
    var namelessSections = findAllNamelessSections();
    
    // 3. Проверяем секцию с выделением (исключаем если нужно)
    var sectionsToProcess = [];
    var skippedSections = 0;
    var skippedReasons = [];
    
    for (var i = 0; i < namelessSections.length; i++) {
        var section = namelessSections[i];
        var isSelectionSection = (section === originalSelection.parentSection);
        
        if (isSelectionSection && SETTINGS.processSectionWithSelection === 0) {
            // Пропускаем секцию с выделением
            skippedSections++;
            skippedReasons.push("секция с оглавлением");
            continue;
        }
        
        sectionsToProcess.push(section);
    }
    
    // 4. Пропускаем первые N секций если нужно - ФИНАЛЬНЫЙ ВАРИАНТ ДЛЯ IE6
    var sectionsSkippedCount = 0;
    if (SETTINGS.skipFirstSectionsCount > 0 && sectionsToProcess.length > 0) {
        // Запрашиваем у пользователя, сколько секций пропустить
        var userInput = prompt(
            "Исключить первых N безымянных секций:\n" +
            "0 = обработать все, 1 = исключить первую, N = исключить N первых",
            SETTINGS.skipFirstSectionsCount
        );
        
        // Если пользователь нажал Cancel - выходим
        if (userInput === null) {
            return;
        }
        
        // Парсим введенное число
        var skipCount = parseInt(userInput, 10);
        if (isNaN(skipCount) || skipCount < 0) {
            skipCount = 0;
        }
        
        // Пропускаем указанное количество секций
        if (skipCount > 0) {
            // Нельзя пропустить больше секций, чем есть
            if (skipCount > sectionsToProcess.length) {
                skipCount = sectionsToProcess.length;
            }
            
            // Удаляем первые skipCount секций
            for (var s = 0; s < skipCount; s++) {
                sectionsToProcess.shift();
            }
            
            sectionsSkippedCount = skipCount;
            skippedSections += sectionsSkippedCount;
            
            if (sectionsSkippedCount === 1) {
                skippedReasons.push("первая секция пропущена");
            } else {
                skippedReasons.push("первые " + sectionsSkippedCount + " секций пропущены");
            }
        }
    }
    
    // 5. Сравниваем количества
    var headerCount = headers.length;
    var sectionCount = sectionsToProcess.length;
    
    // Показываем первоначальную статистику С ШАПКОЙ
    var statsMessage = "---------------------------\n" +
                      scriptName + "\n" +
                      "version: " + scriptVersion + "\n" +
                      "---------------------------\n" +
                      "Найдено:\n" +
                      "Абзацев-заголовков в выделении: " + headerCount + "\n" +
                      "Безымянных секций: " + namelessSections.length + "\n";
    
    if (skippedSections > 0) {
        statsMessage += "Секций исключено из подсчета: " + skippedSections;
        if (skippedReasons.length > 0) {
            statsMessage += " (" + skippedReasons.join(", ") + ")";
        }
        statsMessage += "\n";
    }
    
    statsMessage += "Секций для обработки: " + sectionCount + "\n" +
                   "---------------------------";
    
    MsgBox(statsMessage, "FBE скрипт");
    
    // Проверяем, нужно ли продолжать
    if (sectionCount === 0) {
        MsgBox("Нет безымянных секций для обработки.", scriptName + " (" + scriptVersion + ")");
        return;
    }
    
    // 6. Проверяем соотношение заголовков и секций
    var doubleHeadersPossible = false;
    var userChoice = "";
    
    if (headerCount === sectionCount) {
        // Идеальное совпадение
        if (!confirm(scriptName + " v" + scriptVersion + "\n\n" +
                    "Количество заголовков и секций совпадает (" + headerCount + ").\n\nПродолжить расстановку?")) {
            return;
        }
        userChoice = "1:1";
        
    } else if (headerCount === sectionCount * 2) {
        // Возможны двойные заголовки
        doubleHeadersPossible = true;
        if (!confirm(scriptName + " v" + scriptVersion + "\n\n" +
                    "Количество заголовков вдвое больше количества секций (" + headerCount + " заголовков, " + sectionCount + " секций).\n\n" +
                    "Возможно, каждый заголовок состоит из двух абзацев.\n\n" +
                    "Расставить двойные заголовки?")) {
            return;
        }
        userChoice = "2:1";
        
    } else if (headerCount < sectionCount) {
        // Заголовков меньше чем секций
        if (!confirm(scriptName + " v" + scriptVersion + "\n\n" +
                    "Заголовков меньше чем секций (" + headerCount + " заголовков, " + sectionCount + " секций).\n\n" +
                    "Расставить заголовки с начала документа сколько получится?\n" +
                    "(Лишние секции останутся без заголовков)")) {
            return;
        }
        userChoice = "less";
        
    } else { // headerCount > sectionCount
        // Заголовков больше чем секций
        if (!confirm(scriptName + " v" + scriptVersion + "\n\n" +
                    "Заголовков больше чем секций (" + headerCount + " заголовков, " + sectionCount + " секций).\n\n" +
                    "Расставить заголовки с начала документа сколько получится?\n" +
                    "(Лишние заголовки не будут использованы)")) {
            return;
        }
        userChoice = "more";
    }
    
    // 7. Запрос на удаление исходного фрагмента (ДО начала замера времени!)
    var shouldDeleteOriginal = false;
    if (SETTINGS.deleteOriginalSelection === 1) {
        shouldDeleteOriginal = confirm(scriptName + " v" + scriptVersion + "\n\n" +
                                      "Удалить исходный выделенный фрагмент с оглавлением после расстановки заголовков?\n\n" +
                                      "Рекомендуется: ДА, чтобы избежать дублирования текста в документе.");
    }
    
    // 8. Начинаем транзакцию для отмены
    window.external.BeginUndoUnit(document, scriptName);
    
    // ВОТ ТУТ НАЧИНАЕМ ЗАМЕР ВРЕМЕНИ - после ВСЕХ confirm!
    scriptStartTime = new Date();
    
    // 9. Расставляем заголовки (с обработкой точек без пробелов)
    var processedCount = 0;
    var doubleHeadersCount = 0;
    var fixedDotsCount = 0;
    
    for (var j = 0; j < sectionsToProcess.length; j++) {
        if (j >= headers.length) break; // Закончились заголовки
        
        var currentSection = sectionsToProcess[j];
        
        if (userChoice === "2:1" && j * 2 + 1 < headers.length) {
            // Берем два заголовка для одной секции - отдельными абзацами
            var firstHeader = headers[j * 2];
            var secondHeader = headers[j * 2 + 1];
            
            // Проверяем и исправляем точки без пробелов
            var fixedFirstHeader = fixDotsWithoutSpaces(firstHeader);
            var fixedSecondHeader = fixDotsWithoutSpaces(secondHeader);
            
            if (fixedFirstHeader !== firstHeader || fixedSecondHeader !== secondHeader) {
                fixedDotsCount++;
            }
            
            if (insertDoubleTitleIntoSection(currentSection, fixedFirstHeader, fixedSecondHeader, nbspEntity)) {
                processedCount++;
                doubleHeadersCount++;
            }
        } else {
            // Один заголовок на одну секцию
            
            // Проверяем и исправляем точки без пробелов
            var headerText = headers[j];
            var fixedHeader = fixDotsWithoutSpaces(headerText);
            
            if (fixedHeader !== headerText) {
                fixedDotsCount++;
            }
            
            if (insertSingleTitleIntoSection(currentSection, fixedHeader, nbspEntity)) {
                processedCount++;
            }
        }
    }
    
    // 10. Удаляем исходный выделенный фрагмент если нужно
    var deleteOriginalResult = "";
    if (shouldDeleteOriginal && processedCount > 0) {
        try {
            originalSelection.range.select();
            originalSelection.range.text = "";
            deleteOriginalResult = "Исходный фрагмент удален.";
        } catch(e) {
            deleteOriginalResult = "Не удалось удалить исходный фрагмент.";
        }
    } else if (SETTINGS.deleteOriginalSelection === 1) {
        deleteOriginalResult = "Исходный фрагмент сохранен.";
    }
    
    window.external.EndUndoUnit(document);
    
    // 11. Показываем результат с правильным временем
    var scriptEndTime = new Date();
    var timeDiff = scriptStartTime ? (scriptEndTime - scriptStartTime) : 0;
    var timeSeconds = (timeDiff / 1000).toFixed(2).replace('.', ',') + " сек";
    
    var resultMessage = "---------------------------\n" +
                       scriptName + "\n" +
                       "version: " + scriptVersion + "\n" +
                       "---------------------------\n" +
                       "Результаты:\n" +
                       "Обработано секций: " + processedCount + " из " + sectionCount + "\n";
    
    if (doubleHeadersCount > 0) {
        resultMessage += "С двойными заголовками: " + doubleHeadersCount + "\n";
    }
    
    if (fixedDotsCount > 0) {
        resultMessage += "Исправлено точек без пробелов: " + fixedDotsCount + "\n";
    }
    
    if (skippedSections > 0) {
        resultMessage += "Секций исключено из обработки: " + skippedSections;
        if (skippedReasons.length > 0) {
            resultMessage += " (" + skippedReasons.join(", ") + ")";
        }
        resultMessage += "\n";
    }
    
    if (deleteOriginalResult) {
        resultMessage += deleteOriginalResult + "\n";
    }
    
    resultMessage += "Время выполнения скрипта: " + timeSeconds + "\n" +
                    "---------------------------";
    
    MsgBox(resultMessage, "FBE скрипт");
}

// ======================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ======================================

// Найти родительскую секцию для элемента
function findParentSection(element) {
    if (!element) return null;
    
    var current = element;
    while (current) {
        if (current.nodeType === 1 && current.className && typeof current.className === 'string') {
            if (current.className.indexOf('section') !== -1) {
                return current;
            }
        }
        current = current.parentNode;
    }
    return null;
}

// Извлечь заголовки из выделенного HTML (игнорируя пустые строки)
function extractHeadersFromSelection(html, nbspEntity) {
    var headers = [];
    if (!html) return headers;
    
    try {
        // Создаем временный контейнер для парсинга HTML
        var tempDiv = document.createElement('div');
        tempDiv.style.position = 'absolute';
        tempDiv.style.left = '-9999px';
        tempDiv.innerHTML = html;
        document.body.appendChild(tempDiv);
        
        // Ищем все абзацы
        var pElements = tempDiv.getElementsByTagName('p');
        
        for (var i = 0; i < pElements.length; i++) {
            var p = pElements[i];
            var text = getPlainTextFromElement(p);
            
            // Нормализуем текст
            text = normalizeText(text, nbspEntity);
            
            // Проверяем, не пустая ли строка (после нормализации)
            if (text && text.replace(/\s/g, '') !== '') {
                // Дополнительная проверка: если текст содержит ТОЛЬКО пробельные символы
                var hasNonSpace = false;
                for (var j = 0; j < text.length; j++) {
                    var charCode = text.charCodeAt(j);
                    // Проверяем, что символ НЕ пробел, не неразрывный пробел, не табуляция, не перевод строки
                    if (charCode !== 32 && charCode !== 160 && charCode !== 9 && charCode !== 10 && charCode !== 13 && 
                        charCode !== 8194 && charCode !== 8195 && charCode !== 8201 && charCode !== 8287) { // разные виды пробелов
                        hasNonSpace = true;
                        break;
                    }
                }
                
                if (hasNonSpace) {
                    headers.push(text);
                }
            }
        }
        
        document.body.removeChild(tempDiv);
        
    } catch(e) {
        // В случае ошибки пробуем простой метод
        var tempDiv2 = document.createElement('div');
        tempDiv2.innerHTML = html;
        var plainText = getPlainTextFromElement(tempDiv2);
        plainText = normalizeText(plainText, nbspEntity);
        
        if (plainText && plainText.replace(/\s/g, '') !== '') {
            // Разделяем текст на строки по переводам строк
            var lines = plainText.split(/\r\n|\r|\n/);
            for (var k = 0; k < lines.length; k++) {
                var line = lines[k];
                line = line.replace(/^\s+|\s+$/g, '');
                
                if (line && line.replace(/\s/g, '') !== '') {
                    // Проверяем, не пустая ли строка
                    var hasNonSpace = false;
                    for (var l = 0; l < line.length; l++) {
                        var charCode = line.charCodeAt(l);
                        if (charCode !== 32 && charCode !== 160 && charCode !== 9 && charCode !== 10 && charCode !== 13 && 
                            charCode !== 8194 && charCode !== 8195 && charCode !== 8201 && charCode !== 8287) {
                            hasNonSpace = true;
                            break;
                        }
                    }
                    
                    if (hasNonSpace) {
                        headers.push(line);
                    }
                }
            }
        }
    }
    
    return headers;
}

// Получить простой текст из элемента (с учетом дочерных элементов)
function getPlainTextFromElement(element) {
    if (!element) return "";
    
    var text = "";
    
    function collectText(node) {
        if (node.nodeType === 3) { // Текстовый узел
            text += node.nodeValue || "";
        } else if (node.nodeType === 1) { // Элемент
            // Для некоторых элементов добавляем пробелы
            var tagName = node.tagName ? node.tagName.toLowerCase() : "";
            if (tagName === 'br' || tagName === 'div' || tagName === 'p') {
                text += " ";
            }
            
            // Рекурсивно обрабатываем детей
            for (var i = 0; i < node.childNodes.length; i++) {
                collectText(node.childNodes[i]);
            }
        }
    }
    
    collectText(element);
    return text;
}

// Нормализовать текст (убрать лишние пробелы, заменить неразрывные)
function normalizeText(text, nbspEntity) {
    if (!text) return "";
    
    // Заменяем неразрывные пробелы на обычные
    text = text.replace(new RegExp(nbspEntity, 'g'), ' ');
    
    // Заменяем все HTML-сущности пробелов
    text = text.replace(/&nbsp;/g, ' ');
    text = text.replace(/\s+/g, ' '); // Множественные пробелы в один
    
    // Убираем пробелы в начале и конце
    text = text.replace(/^\s+|\s+$/g, '');
    
    return text;
}

// Исправить точки без пробелов в тексте (улучшенная версия)
function fixDotsWithoutSpaces(text) {
    if (!text) return text;
    
    // Сохраняем оригинал для сравнения
    var original = text;
    
    // Шаблоны для поиска точек без пробелов после них
    // 1. Число.Текст (например: "26.Тринадцатый день") и число.«Текст
    text = text.replace(/([0-9]{1,3})\.([А-ЯЁA-Z«])/g, '$1. $2');
    
    // 2. Глава/Часть/Том/Раздел число.Текст (например: "Глава 1.Название")
    text = text.replace(/(Глава|Часть|Том|Раздел|Книга|Глава|Chapter|Part|Book|Volume)\s+[0-9]{1,3}\.([А-ЯЁA-Z«])/gi, function(match, p1, p2) {
        return match.replace('.' + p2, '. ' + p2);
    });
    
    // 3. Римские цифры I.II.III.IV.V и т.д. (только заглавные буквы)
    text = text.replace(/([IVXLCDM]{1,6})\.([А-ЯЁA-Z«])/g, '$1. $2');
    
    // 4. Буква.Текст (например: "А.Название")
    text = text.replace(/([А-ЯЁA-Z])\.([А-ЯЁA-Z«])/g, '$1. $2');
    
    // 5. Любая точка перед заглавной буквой или « (общий случай)
    text = text.replace(/\.([А-ЯЁA-Z«])/g, '. $1');
    
    // Убираем возможные двойные пробелы после исправлений
    text = text.replace(/\.\s+/g, '. ');
    text = text.replace(/\s+/g, ' ');
    
    return text;
}

// Найти все безымянные секции (без заголовков) - ИСПРАВЛЕННАЯ ВЕРСИЯ!
function findAllNamelessSections() {
    var namelessSections = [];
    
    // Ищем все div с классом section
    var allDivs = document.getElementsByTagName('div');
    
    for (var i = 0; i < allDivs.length; i++) {
        var div = allDivs[i];
        if (div.className && typeof div.className === 'string') {
            if (div.className.indexOf('section') !== -1) {
                // Проверяем, есть ли у секции заголовок (title, НЕ subtitle!)
                if (!hasTitleElement(div)) {
                    namelessSections.push(div);
                }
            }
        }
    }
    
    return namelessSections;
}

// Проверить, есть ли у секции элемент title (ИСПРАВЛЕННАЯ ВЕРСИЯ!)
function hasTitleElement(section) {
    if (!section) return false;
    
    var children = section.childNodes;
    for (var i = 0; i < children.length; i++) {
        var child = children[i];
        if (child.nodeType === 1 && child.className && typeof child.className === 'string') {
            // Разбиваем классы по пробелам и проверяем каждый
            var classes = child.className.split(' ');
            for (var j = 0; j < classes.length; j++) {
                // Ищем именно класс "title", а не подстроку "title"
                if (classes[j] === 'title') {
                    return true;
                }
            }
        }
    }
    
    return false;
}

// Вставить одиночный заголовок в секцию
function insertSingleTitleIntoSection(section, headerText, nbspEntity) {
    if (!section || !headerText) return false;
    
    try {
        // Создаем структуру title > p > текст
        var titleDiv = document.createElement('div');
        titleDiv.className = 'title';
        
        var pElement = document.createElement('p');
        
        // Экранируем HTML-сущности в тексте
        var safeText = headerText
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;');
        
        pElement.innerHTML = safeText;
        
        titleDiv.appendChild(pElement);
        
        // Вставляем title в начало секции (перед первым элементом)
        if (section.firstChild) {
            section.insertBefore(titleDiv, section.firstChild);
        } else {
            section.appendChild(titleDiv);
        }
        
        return true;
        
    } catch(e) {
        return false;
    }
}

// Вставить двойной заголовок в секцию (два отдельных абзаца)
function insertDoubleTitleIntoSection(section, firstHeader, secondHeader, nbspEntity) {
    if (!section || !firstHeader || !secondHeader) return false;
    
    try {
        // Создаем структуру title > p > текст + p > текст
        var titleDiv = document.createElement('div');
        titleDiv.className = 'title';
        
        // Первый абзац
        var pElement1 = document.createElement('p');
        var safeText1 = firstHeader
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;');
        pElement1.innerHTML = safeText1;
        titleDiv.appendChild(pElement1);
        
        // Второй абзац
        var pElement2 = document.createElement('p');
        var safeText2 = secondHeader
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;');
        pElement2.innerHTML = safeText2;
        titleDiv.appendChild(pElement2);
        
        // Вставляем title в начало секции (перед первым элементом)
        if (section.firstChild) {
            section.insertBefore(titleDiv, section.firstChild);
        } else {
            section.appendChild(titleDiv);
        }
        
        return true;
        
    } catch(e) {
        return false;
    }
}
