// Скрипт "ДИАГНОСТИКА: Анализ секций с картинками" для редактора FBE
// version 1.1
// Идея - TaKir
// Реализация - DeepSeek

// version 1.1, 10.01.2025

function Run() {
    // Находим основной body с контентом
    var mainBody = findMainBody();
    
    if (!mainBody) {
        window.external.MsgBox("Ошибка: не найден основной body с контентом!");
        return;
    }
    
    // Получаем все секции в документе
    var allSections = findAllSections(mainBody);
    var diagnosticInfo = "ДИАГНОСТИКА секций с картинками\n\n";
    diagnosticInfo += "Всего секций: " + allSections.length + "\n\n";
    
    var sectionsToShow = Math.min(5, allSections.length);
    
    for (var i = 0; i < sectionsToShow; i++) {
        var section = allSections[i];
        diagnosticInfo += "=== СЕКЦИЯ " + (i + 1) + " ===\n";
        
        // Проверяем, является ли секция содержащей только иллюстрацией
        var isImgSection = isImageSection(section);
        diagnosticInfo += "Секция-иллюстрация: " + (isImgSection ? "ДА" : "НЕТ") + "\n";
        
        // Считаем картинки
        var imageCount = countImagesInSection(section);
        diagnosticInfo += "Количество картинок: " + imageCount + "\n";
        
        if (imageCount > 0) {
            // Находим последнюю картинку (старый способ)
            var lastImageOld = findLastImageOldWay(section);
            // Находим последнюю картинку (новый способ)
            var lastImageNew = findLastImageNewWay(section);
            
            diagnosticInfo += "Последняя картинка (старый способ): " + (lastImageOld ? getImageName(lastImageOld) : "НЕТ") + "\n";
            diagnosticInfo += "Последняя картинка (новый способ): " + (lastImageNew ? getImageName(lastImageNew) : "НЕТ") + "\n";
            
            // Детальный анализ детей секции
            diagnosticInfo += "\nДЕТАЛЬНЫЙ АНАЛИЗ ДЕТЕЙ СЕКЦИИ:\n";
            var children = section.childNodes;
            for (var j = 0; j < children.length; j++) {
                var child = children[j];
                diagnosticInfo += "[" + j + "] ";
                
                if (child.nodeType == 1) { // Элемент
                    diagnosticInfo += "ЭЛЕМЕНТ: " + child.tagName;
                    
                    if (child.className) {
                        diagnosticInfo += " class='" + child.className + "'";
                    }
                    
                    if (isImageElement(child)) {
                        diagnosticInfo += " (КАРТИНКА: " + getImageName(child) + ")";
                    } else if (isEmptyParagraph(child)) {
                        diagnosticInfo += " (ПУСТОЙ АБЗАЦ)";
                    } else if (child.tagName.toLowerCase() == 'p') {
                        var text = getElementText(child);
                        diagnosticInfo += " (АБЗАЦ: '" + truncateText(text, 30) + "')";
                    }
                } else if (child.nodeType == 3) { // Текст
                    var text = child.nodeValue;
                    if (text && text.replace(/^\s+|\s+$/g, '') !== '') {
                        diagnosticInfo += "ТЕКСТ: '" + truncateText(text, 30) + "'";
                    } else {
                        diagnosticInfo += "ПУСТОЙ ТЕКСТ";
                    }
                }
                
                diagnosticInfo += "\n";
            }
            
            // Анализ функции findImageAndEmptyLineAtSectionEnd
            var result = findImageAndEmptyLineAtSectionEnd(section);
            diagnosticInfo += "\nРЕЗУЛЬТАТ findImageAndEmptyLineAtSectionEnd:\n";
            diagnosticInfo += "• Найдена картинка: " + (result.image ? getImageName(result.image) : "НЕТ") + "\n";
            diagnosticInfo += "• Найден пустой абзац: " + (result.emptyLine ? "ДА" : "НЕТ") + "\n";
        }
        
        // Ручное создание разделителя
        diagnosticInfo += "\n========================================\n\n";
    }
    
    if (allSections.length > 5) {
        diagnosticInfo += "... и еще " + (allSections.length - 5) + " секций\n";
    }
    
    window.external.MsgBox(diagnosticInfo);
}

// Вспомогательные функции для диагностики

function findMainBody() {
    var fbwBody = document.getElementById('fbw_body');
    if (!fbwBody) {
        var allDivs = document.getElementsByTagName('div');
        for (var i = 0; i < allDivs.length; i++) {
            if (allDivs[i].getAttribute('contenteditable') === 'true') {
                fbwBody = allDivs[i];
                break;
            }
        }
    }
    
    if (!fbwBody) return null;
    
    var allDivsInFbw = fbwBody.getElementsByTagName('div');
    for (var j = 0; j < allDivsInFbw.length; j++) {
        var div = allDivsInFbw[j];
        var className = div.className || '';
        if (className.indexOf('body') !== -1) {
            var fbname = div.getAttribute('fbname');
            if (!fbname || fbname !== 'notes') {
                return div;
            }
        }
    }
    
    return null;
}

function findAllSections(element) {
    var sections = [];
    
    function findSectionsRecursive(node) {
        if (!node) return;
        
        if (node.nodeType == 1) {
            if (isSectionElement(node)) {
                sections.push(node);
            } else {
                var children = node.childNodes;
                for (var i = 0; i < children.length; i++) {
                    findSectionsRecursive(children[i]);
                }
            }
        }
    }
    
    findSectionsRecursive(element);
    return sections;
}

function isSectionElement(element) {
    if (element.nodeType != 1) return false;
    if (element.tagName.toLowerCase() != 'div') return false;
    if (!element.className) return false;
    
    var className = element.className;
    if (className.indexOf('section') === -1) return false;
    
    return true;
}

function isImageSection(section) {
    var children = section.childNodes;
    var hasImage = false;
    var hasOtherContent = false;
    
    for (var i = 0; i < children.length; i++) {
        var child = children[i];
        if (child.nodeType == 1) {
            if (isImageElement(child)) {
                hasImage = true;
            } else if (child.className && child.className.indexOf('title') !== -1) {
                hasOtherContent = true;
                break;
            } else if (child.className && child.className.indexOf('epigraph') !== -1) {
                hasOtherContent = true;
                break;
            } else if (child.tagName.toLowerCase() == 'p') {
                var text = getElementText(child);
                if (text && text.replace(/^\s+|\s+$/g, '') !== '') {
                    hasOtherContent = true;
                    break;
                }
            } else {
                hasOtherContent = true;
                break;
            }
        } else if (child.nodeType == 3) {
            var text = child.nodeValue || '';
            if (text && text.replace(/^\s+|\s+$/g, '') !== '') {
                hasOtherContent = true;
                break;
            }
        }
    }
    
    return hasImage && !hasOtherContent;
}

function countImagesInSection(section) {
    var count = 0;
    var children = section.childNodes;
    
    for (var i = 0; i < children.length; i++) {
        if (children[i].nodeType == 1 && isImageElement(children[i])) {
            count++;
        }
    }
    
    return count;
}

function getElementText(element) {
    if (element.nodeType == 3) {
        return element.nodeValue || '';
    }
    
    if (element.nodeType == 1) {
        var text = '';
        var children = element.childNodes;
        for (var i = 0; i < children.length; i++) {
            text += getElementText(children[i]);
        }
        return text;
    }
    
    return '';
}

function truncateText(text, maxLength) {
    if (!text) return '';
    if (text.length <= maxLength) {
        // Заменяем переносы строк для читаемости
        return text.replace(/\n/g, '\\n');
    }
    return text.substring(0, maxLength - 3).replace(/\n/g, '\\n') + '...';
}

function isImageElement(element) {
    if (element.nodeType != 1) return false;
    if (element.tagName.toLowerCase() != 'div') return false;
    if (!element.className) return false;
    
    var className = element.className;
    if (className.indexOf('image') === -1) return false;
    
    return true;
}

function getImageName(imageElement) {
    var href = imageElement.getAttribute('href');
    return href ? href : 'без имени';
}

function isEmptyParagraph(element) {
    if (element.nodeType != 1) return false;
    if (element.tagName.toLowerCase() != 'p') return false;
    
    var text = getElementText(element);
    if (!text) return true;
    
    // Убираем все пробельные символы
    var trimmedText = text.replace(/^\s+|\s+$/g, '');
    
    // Проверяем на пустоту, &nbsp; и неразрывные пробелы
    return trimmedText === '' || 
           trimmedText === '&nbsp;' || 
           trimmedText === String.fromCharCode(160);
}

// Старый способ поиска последней картинки (как в 4.4)
function findLastImageOldWay(section) {
    var children = section.childNodes;
    
    // Ищем с конца
    for (var i = children.length - 1; i >= 0; i--) {
        if (children[i].nodeType == 1 && isImageElement(children[i])) {
            return children[i];
        }
    }
    
    return null;
}

// Новый способ поиска последней картинки (должен быть правильный)
function findLastImageNewWay(section) {
    var children = section.childNodes;
    var lastImageIndex = -1;
    
    // Находим индекс последней картинки
    for (var i = 0; i < children.length; i++) {
        if (children[i].nodeType == 1 && isImageElement(children[i])) {
            lastImageIndex = i;
        }
    }
    
    if (lastImageIndex !== -1) {
        return children[lastImageIndex];
    }
    
    return null;
}

// Текущая функция из скрипта 4.5
function findImageAndEmptyLineAtSectionEnd(section) {
    var result = {
        image: null,
        emptyLine: null
    };
    
    var children = section.childNodes;
    var childCount = children.length;
    
    if (childCount === 0) return result;
    
    // Шаг 1: Найти ВСЕ картинки в секции
    var imageIndices = [];
    for (var i = 0; i < childCount; i++) {
        if (children[i].nodeType == 1 && isImageElement(children[i])) {
            imageIndices.push(i);
        }
    }
    
    if (imageIndices.length === 0) return result;
    
    // Шаг 2: Берем САМУЮ ПОСЛЕДНЮЮ картинку (ближе всех к концу секции)
    var lastImageIndex = imageIndices[imageIndices.length - 1];
    var lastImage = children[lastImageIndex];
    
    // Шаг 3: Проверяем, что после этой картинки нет текста
    var hasTextAfter = false;
    
    for (var j = lastImageIndex + 1; j < childCount; j++) {
        var child = children[j];
        
        if (child.nodeType == 3) {
            // Текстовый узел
            var text = child.nodeValue || '';
            if (text.replace(/^\s+|\s+$/g, '') !== '') {
                hasTextAfter = true;
                break;
            }
        } else if (child.nodeType == 1) {
            if (isImageElement(child)) {
                // Нашли другую картинку - значит lastImage не самая последняя
                hasTextAfter = true;
                break;
            } else if (!isEmptyParagraph(child)) {
                // Любой другой непустой элемент
                hasTextAfter = true;
                break;
            }
        }
    }
    
    if (hasTextAfter) {
        // После картинки есть текст - не переносим
        return result;
    }
    
    // Шаг 4: Проверяем, есть ли пустой абзац сразу после картинки
    if (lastImageIndex + 1 < childCount) {
        var nextChild = children[lastImageIndex + 1];
        if (nextChild.nodeType == 1 && isEmptyParagraph(nextChild)) {
            result.emptyLine = nextChild;
        }
    }
    
    result.image = lastImage;
    return result;
}
