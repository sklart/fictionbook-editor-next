// Скрипт «Удалить подзаголовки-звёздочки рядом с заголовками или подзаголовками» для редактора FBE
// version 3.4
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для удаления стандартных подзаголовков-"звёздочек" (* * *),
// непосредстенно примыкающих к размеченными заголовками или подзаголовками (до или после них).
// Количество звёздочек и пробелов в подзаголоках может быть любым.

// version 3.4, 18.01.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Удалить подзаголовки-звёздочки рядом с заголовками или подзаголовками";
    var version = "3.4";
    
    // Переменные для статистики
    var deletedTotal = 0;
    var deletedNearTitles = 0;
    var deletedNearSubtitles = 0;
    var deletedAboveTitles = 0;
    var deletedBelowTitles = 0;
    var deletedAboveSubtitles = 0;
    var deletedBelowSubtitles = 0;
    
    // Переменные для неразрывного пробела
    var nbspChar = String.fromCharCode(160);
    var nbspEntity = "&nbsp;";
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
    
    // Таймер (запустим после всех подтверждений)
    var startTime;
    
    try {
        // Спрашиваем настройки у пользователя
        
        // Вопрос 1: Удалять ли рядом с заголовками?
        var removeNearTitles = AskYesNo(scriptName + "\nver. " + version + "\n\n" + 
                                       "Удалять подзаголовки-звёздочки,\n" +
                                       "примыкающие к ЗАГОЛОВКАМ (выше или ниже)?");
        
        // Вопрос 2: Удалять ли рядом с подзаголовками?
        var removeNearSubtitles = AskYesNo(scriptName + "\nver. " + version + "\n\n" + 
                                          "Удалять подзаголовки-звёздочки,\n" +
                                          "примыкающие к ПОДЗАГОЛОВКАМ (выше или ниже)?");
        
        // Если ничего не выбрано - выходим
        if (!removeNearTitles && !removeNearSubtitles) {
            MsgBox("Ни один из вариантов не выбран.\nСкрипт завершен без изменений.", "FBE скрипт - Отмена");
            return;
        }
        
        // Запускаем таймер
        startTime = new Date();
        
        var undoMsg = "удаление подзаголовков-звёздочек";
        var statusBarMsg = "Удаляем подзаголовки-звёздочки…";
        
        window.external.BeginUndoUnit(document, undoMsg);
        try { window.external.SetStatusBarText(statusBarMsg); }
        catch(e) {}
        
        // Получаем корневой элемент
        var fbwBody = document.getElementById("fbw_body");
        if (!fbwBody) {
            MsgBox("Не найден элемент fbw_body", scriptName + " - Ошибка");
            return;
        }
        
        // Ищем все подзаголовки (элементы P с классом subtitle)
        var allElements = fbwBody.getElementsByTagName("P");
        var subtitlesToRemove = [];
        var processedElements = {}; // Хэш для отслеживания обработанных элементов
        
        for (var i = 0; i < allElements.length; i++) {
            var element = allElements[i];
            
            if (element.className == "subtitle") {
                // Проверяем, не обрабатывали ли мы уже этот элемент
                if (processedElements[element.uniqueID || i]) continue;
                
                var text = getTextContent(element);
                text = trimText(text);
                
                // Проверяем, является ли текст звёздочками
                if (isStarSubtitle(text)) {
                    processedElements[element.uniqueID || i] = true;
                    
                    // Проверяем соседство с заголовками (если включено)
                    if (removeNearTitles) {
                        var positionNearTitle = checkSubtitlePositionNearTitle(element);
                        if (positionNearTitle !== 'none') {
                            subtitlesToRemove.push({
                                element: element,
                                position: positionNearTitle,
                                type: 'title'
                            });
                            continue; // Элемент уже добавлен для удаления
                        }
                    }
                    
                    // Проверяем соседство с подзаголовками (если включено)
                    if (removeNearSubtitles) {
                        var positionNearSubtitle = checkSubtitlePositionNearSubtitle(element);
                        if (positionNearSubtitle !== 'none') {
                            subtitlesToRemove.push({
                                element: element,
                                position: positionNearSubtitle,
                                type: 'subtitle'
                            });
                        }
                    }
                }
            }
        }
        
        // Удаляем в обратном порядке
        for (var i = subtitlesToRemove.length - 1; i >= 0; i--) {
            var item = subtitlesToRemove[i];
            
            // Проверяем, что элемент еще существует в DOM
            if (item.element.parentNode) {
                item.element.parentNode.removeChild(item.element);
                deletedTotal++;
                
                // Обновляем статистику
                if (item.type == 'title') {
                    deletedNearTitles++;
                    if (item.position == 'above') deletedAboveTitles++;
                    if (item.position == 'below') deletedBelowTitles++;
                } else if (item.type == 'subtitle') {
                    deletedNearSubtitles++;
                    if (item.position == 'above') deletedAboveSubtitles++;
                    if (item.position == 'below') deletedBelowSubtitles++;
                }
            }
        }
        
        // Вычисляем время выполнения
        var endTime = new Date();
        var execTime = (endTime - startTime) / 1000;
        var timeStr = execTime.toFixed(2).replace('.', ',');
        
        // Финальная статистика
        var finalMessage = scriptName + "\n" +
                          "ver. " + version + "\n\n" +
                          "ВЫБРАНЫ ДЕЙСТВИЯ:\n";
        
        if (removeNearTitles && removeNearSubtitles) {
            finalMessage += "Удаление у ЗАГОЛОВКОВ и ПОДЗАГОЛОВКОВ\n\n";
        } else if (removeNearTitles) {
            finalMessage += "Удаление только у ЗАГОЛОВКОВ\n\n";
        } else if (removeNearSubtitles) {
            finalMessage += "Удаление только у ПОДЗАГОЛОВКОВ\n\n";
        }
        
        finalMessage += "РЕЗУЛЬТАТ:\n" +
                       "Всего удалено: " + deletedTotal + "\n\n";
        
        if (removeNearTitles && deletedNearTitles > 0) {
            finalMessage += "У заголовков: " + deletedNearTitles + "\n";
            if (deletedAboveTitles > 0) finalMessage += "  - выше заголовков: " + deletedAboveTitles + "\n";
            if (deletedBelowTitles > 0) finalMessage += "  - ниже заголовков: " + deletedBelowTitles + "\n";
            if (removeNearSubtitles && deletedNearSubtitles > 0) finalMessage += "\n";
        }
        
        if (removeNearSubtitles && deletedNearSubtitles > 0) {
            finalMessage += "У подзаголовков: " + deletedNearSubtitles + "\n";
            if (deletedAboveSubtitles > 0) finalMessage += "  - выше подзаголовков: " + deletedAboveSubtitles + "\n";
            if (deletedBelowSubtitles > 0) finalMessage += "  - ниже подзаголовков: " + deletedBelowSubtitles + "\n";
        }
        
        if (deletedTotal === 0) {
            finalMessage += "\nПодзаголовки-звёздочки для удаления не найдены.";
        }
        
        finalMessage += "\n\nВремя выполнения: " + timeStr + " сек.";
        
        MsgBox(finalMessage, "FBE скрипт - Результат");
        
        try { window.external.SetStatusBarText("ОК"); }
        catch(e) {} 
        window.external.EndUndoUnit(document);
        
    } catch (error) {
        MsgBox("Ошибка: " + error.message, "FBE скрипт - Ошибка");
    }
}

// Функция для проверки звёздочек в подзаголовке
function isStarSubtitle(text) {
    if (!text) return false;
    
    // Удаляем все пробелы и неразрывные пробелы
    var cleanText = text.replace(/[\s\u00A0]+/g, '');
    
    // Проверяем, состоит ли текст только из звёздочек
    if (cleanText.length === 0) return false;
    
    for (var i = 0; i < cleanText.length; i++) {
        if (cleanText.charAt(i) !== '*') {
            return false;
        }
    }
    
    return true;
}

// Функция для получения текстового содержимого (совместимая с IE6)
function getTextContent(element) {
    if (element.textContent !== undefined) {
        return element.textContent;
    } else if (element.innerText !== undefined) {
        return element.innerText;
    } else {
        var text = "";
        var children = element.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType === 3) { // Текстовый узел
                text += child.nodeValue;
            }
        }
        return text;
    }
}

// Функция для обрезки пробелов (совместимая с IE6)
function trimText(text) {
    if (!text) return "";
    
    var nbspChar = String.fromCharCode(160);
    var unusualSpaces = nbspChar + String.fromCharCode(8194) + String.fromCharCode(8195) + 
                       String.fromCharCode(8196) + String.fromCharCode(8197) + 
                       String.fromCharCode(8198) + String.fromCharCode(8239) + 
                       String.fromCharCode(8201) + String.fromCharCode(8202);
    
    // Заменяем все специальные пробелы на обычные для тримминга
    var pattern = new RegExp("[" + unusualSpaces + "]", "g");
    var tempText = text.replace(pattern, " ");
    
    // Обрезаем начало
    var start = 0;
    while (start < tempText.length && tempText.charAt(start) === " ") {
        start++;
    }
    
    // Обрезаем конец
    var end = tempText.length - 1;
    while (end >= 0 && tempText.charAt(end) === " ") {
        end--;
    }
    
    if (end < start) return "";
    
    // Возвращаем оригинальный текст с правильными границами
    return text.substring(start, end + 1);
}

// Функция для проверки соседства звёздочки с заголовком (существующая логика)
function checkSubtitlePositionNearTitle(subtitle) {
    var parentSection = getParentSection(subtitle);
    if (!parentSection) return 'none';
    
    // Случай 1: Звёздочка ВЫШЕ заголовка
    // Звездочка находится в конце предыдущей секции, а текущая секция начинается с заголовка
    if (isLastElementInSection(subtitle, parentSection)) {
        var nextSection = getNextSection(parentSection);
        if (nextSection && hasTitleAtStart(nextSection)) {
            return 'above';
        }
    }
    
    // Случай 2: Звёздочка НИЖЕ заголовка
    // звёздочка находится в той же секции сразу после заголовка
    if (isFirstElementAfterTitle(subtitle, parentSection)) {
        return 'below';
    }
    
    return 'none';
}

// Функция для проверки соседства звёздочки с подзаголовком
function checkSubtitlePositionNearSubtitle(starSubtitle) {
    var parentSection = getParentSection(starSubtitle);
    if (!parentSection) return 'none';
    
    // Получаем все элементы секции
    var sectionElements = [];
    var children = parentSection.childNodes;
    
    for (var i = 0; i < children.length; i++) {
        var child = children[i];
        if (child.nodeType === 1) { // Только элементы
            sectionElements.push(child);
        }
    }
    
    // Находим индекс нашей звёздочки в секции
    var starIndex = -1;
    for (var i = 0; i < sectionElements.length; i++) {
        if (sectionElements[i] === starSubtitle) {
            starIndex = i;
            break;
        }
    }
    
    if (starIndex === -1) return 'none';
    
    // Проверяем элемент ВЫШЕ звёздочки (если он существует)
    if (starIndex > 0) {
        var elementAbove = sectionElements[starIndex - 1];
        if (elementAbove.className == "subtitle" && elementAbove !== starSubtitle) {
            var textAbove = getTextContent(elementAbove);
            textAbove = trimText(textAbove);
            
            // Если элемент выше - обычный подзаголовок (не звёздочки)
            if (!isStarSubtitle(textAbove)) {
                return 'above';
            }
        }
    }
    
    // Проверяем элемент НИЖЕ звёздочки (если он существует)
    if (starIndex < sectionElements.length - 1) {
        var elementBelow = sectionElements[starIndex + 1];
        if (elementBelow.className == "subtitle" && elementBelow !== starSubtitle) {
            var textBelow = getTextContent(elementBelow);
            textBelow = trimText(textBelow);
            
            // Если элемент ниже - обычный подзаголовок (не звёздочки)
            if (!isStarSubtitle(textBelow)) {
                return 'below';
            }
        }
    }
    
    return 'none';
}

// ========== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ==========

function getParentSection(element) {
    var parent = element.parentNode;
    while (parent && parent.nodeName !== "DIV") {
        parent = parent.parentNode;
    }
    return parent;
}

function isLastElementInSection(element, section) {
    var children = section.childNodes;
    var lastElement = null;
    
    // Находим последний элемент в секции
    for (var i = children.length - 1; i >= 0; i--) {
        if (children[i].nodeType === 1) {
            lastElement = children[i];
            break;
        }
    }
    
    return lastElement === element;
}

function isFirstElementAfterTitle(subtitle, section) {
    var children = section.childNodes;
    var foundTitle = false;
    
    for (var i = 0; i < children.length; i++) {
        var child = children[i];
        
        if (child.nodeType === 1) {
            if (child.nodeName === "DIV" && child.className === "title") {
                foundTitle = true;
                continue;
            }
            
            if (foundTitle) {
                // Первый элемент после заголовка - наш подзаголовок
                if (child === subtitle) {
                    return true;
                }
                // Если нашли другой элемент до подзаголовка - значит не непосредственно после заголовка
                return false;
            }
        }
    }
    
    return false;
}

function getNextSection(section) {
    var next = section.nextSibling;
    while (next && (next.nodeType !== 1 || next.nodeName !== "DIV")) {
        next = next.nextSibling;
    }
    return next;
}

function hasTitleAtStart(section) {
    var children = section.childNodes;
    
    for (var i = 0; i < children.length; i++) {
        var child = children[i];
        
        if (child.nodeType === 1) {
            // Если первый элемент - заголовок
            if (child.nodeName === "DIV" && child.className === "title") {
                return true;
            }
            // Если первый элемент НЕ заголовок - значит секция не начинается с заголовка
            return false;
        }
    }
    
    return false;
}
