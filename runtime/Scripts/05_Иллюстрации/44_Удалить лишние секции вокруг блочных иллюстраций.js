// Скрипт "Удалить лишние секции вокруг блочных иллюстраций" для редактора FBE
// version 1.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для удаления избыточных секций вокруг блочных иллюстраций в fb2 документах.
// Скрипт обрабатывает секции, которые содержат только одну блочную иллюстрацию (DIV class=image) и пустую строку,
// находятся в родительской секции, за которой после иллюстрации следует вложенная секция.
// Такие конструкции упрощаются: иллюстрация переносится на уровень родительской секции,
// а обрамлявшая иллюстрацию секция удаляется вместе с пустой строкой.
// Это улучшает структуру документа и убирает лишнюю вложенность.
// Скрипт работает с основным разделом книги (не затрагивает разделы примечаний и комментариев).
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.2, 21.04.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Удалить лишние секции вокруг блочных иллюстраций";
    var version = "1.2";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    // Получаем неразрывный пробел из настроек FBE
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
    
    // Ищем все DIV с классом "body" (основные разделы)
    var bodyElements = [];
    var allDivs = document.getElementsByTagName("DIV");
    
    // Собираем все body элементы
    for (var i = 0; i < allDivs.length; i++) {
        var div = allDivs[i];
        if (div.className == "body") {
            // Проверяем, основной ли это body (fbname="" или отсутствует)
            var fbname = div.getAttribute("fbname") || "";
            if (fbname == "") {
                bodyElements.push(div);
            }
        }
    }
    
    if (bodyElements.length == 0) {
        if (showStatistics) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" + "В документе не найдено основных разделов (DIV class='body').");
        }
        return;
    }
    
    // Функция для проверки, является ли элемент пустой строкой
    function isEmptyLine(element) {
        if (element.nodeName != "P") return false;
        
        var innerHTML = element.innerHTML || "";
        var innerText = element.innerText || "";
        
        // Проверяем на пустоту
        if (innerHTML == "" || innerHTML == nbspEntity || innerHTML == nbspChar) return true;
        
        // Проверяем на наличие только пробелов или nbsp
        var cleanHTML = innerHTML.replace(new RegExp(nbspChar, "g"), " ")
                                 .replace(/&nbsp;/g, " ")
                                 .replace(/\s/g, "");
        
        return cleanHTML == "";
    }
    
    // Функция для проверки, содержит ли секция только одну иллюстрацию и пустую строку
    function isImageSectionWithOnlyImageAndEmptyLine(section) {
        if (section.className != "section") return false;
        
        var children = section.childNodes;
        var hasImage = false;
        var imageCount = 0;
        var hasOnlyImageAndEmptyLines = true;
        
        for (var j = 0; j < children.length; j++) {
            var child = children[j];
            if (child.nodeType == 1) { // ELEMENT_NODE
                if (child.nodeName == "DIV" && child.className == "image") {
                    hasImage = true;
                    imageCount++;
                } else if (child.nodeName == "P" && isEmptyLine(child)) {
                    // Пустая строка - разрешена
                } else if (child.nodeName == "DIV" && 
                          (child.className == "title" || child.className == "epigraph" || 
                           child.className == "annotation" || child.className == "section")) {
                    // Разрешаем только одну иллюстрацию и пустые строки
                    hasOnlyImageAndEmptyLines = false;
                    break;
                } else if (child.nodeName != "P" || !isEmptyLine(child)) {
                    // Любой другой элемент кроме пустого P
                    hasOnlyImageAndEmptyLines = false;
                    break;
                }
            } else if (child.nodeType == 3) { // TEXT_NODE
                var text = child.nodeValue || "";
                var trimmedText = text.replace(/\s/g, "");
                if (trimmedText != "") {
                    hasOnlyImageAndEmptyLines = false;
                    break;
                }
            }
        }
        
        // Важно: должна быть РОВНО ОДНА иллюстрация
        return hasImage && imageCount == 1 && hasOnlyImageAndEmptyLines;
    }
    
    // Функция для проверки, является ли элемент непустым содержимым секции
    function isNonEmptyContent(element) {
        if (!element || element.nodeType != 1) return false;
        
        if (element.nodeName == "DIV") {
            if (element.className == "section") return true;
            if (element.className == "image") return true;
            if (element.className == "annotation") return true;
            if (element.className == "epigraph") return true;
        } else if (element.nodeName == "P") {
            return !isEmptyLine(element);
        }
        
        return false;
    }
    
    // Функция для проверки, можно ли обрабатывать секцию
    function canProcessSection(section) {
        var parent = section.parentNode;
        if (!parent || parent.nodeName != "DIV") return false;
        
        // Родитель должен быть section или body
        if (parent.className != "section" && parent.className != "body") return false;
        
        // ИСКЛЮЧЕНИЕ: если родитель - body, и перед секцией с иллюстрацией идёт заголовок body
        // (то есть иллюстрация сразу после title внутри body), то НЕ ОБРАБАТЫВАЕМ
        if (parent.className == "body") {
            var prevSibling = section.previousSibling;
            // Проверяем, что предыдущий элемент - это заголовок body
            if (prevSibling && prevSibling.nodeType == 1 && 
                prevSibling.nodeName == "DIV" && prevSibling.className == "title") {
                return false;
            }
        }
        
        // Проверяем, является ли эта секция ПЕРВЫМ непустым содержимым в родителе
        var children = parent.childNodes;
        var foundImageSection = false;
        
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            
            if (child === section) {
                // Нашли нашу секцию с иллюстрацией
                foundImageSection = true;
                // Продолжаем проверку следующих элементов
                continue;
            }
            
            if (child.nodeType != 1) continue; // Пропускаем текстовые узлы
            
            if (foundImageSection) {
                // После нашей секции проверяем следующий элемент
                if (child.nodeName == "DIV" && child.className == "section") {
                    // Следующий элемент - секция (например, глава), можно обрабатывать
                    return true;
                } else if (isNonEmptyContent(child)) {
                    // Следующий элемент - другое непустое содержимое, нельзя обрабатывать
                    return false;
                }
                // Пропускаем пустые элементы
            } else if (isNonEmptyContent(child) && child !== section) {
                // Если ДО нашей секции есть другое непустое содержимое - нельзя обрабатывать
                return false;
            }
        }
        
        // Если дошли до конца и не нашли следующую секцию
        return false;
    }
    
    // Фаза 1: сбор данных
    var sectionsToProcess = [];
    var totalImages = 0;
    var suitableImageSections = 0;
    var suitableImages = 0;
    
    // Считаем все блочные иллюстрации
    var allImages = document.getElementsByTagName("DIV");
    for (var i = 0; i < allImages.length; i++) {
        if (allImages[i].className == "image") {
            totalImages++;
        }
    }
    
    // Рекурсивная функция для поиска всех секций
    function findSectionsRecursive(element, results) {
        if (!element || element.nodeType != 1) return;
        
        if (element.nodeName == "DIV" && element.className == "section") {
            results.push(element);
        }
        
        // Рекурсивно проверяем детей
        var children = element.childNodes;
        for (var i = 0; i < children.length; i++) {
            findSectionsRecursive(children[i], results);
        }
    }
    
    // Находим подходящие секции
    var allSections = [];
    for (var i = 0; i < bodyElements.length; i++) {
        findSectionsRecursive(bodyElements[i], allSections);
    }
    
    for (var i = 0; i < allSections.length; i++) {
        var section = allSections[i];
        if (isImageSectionWithOnlyImageAndEmptyLine(section) && canProcessSection(section)) {
            // Считаем иллюстрации в этой секции (должна быть одна)
            var imagesInSection = 0;
            var children = section.childNodes;
            for (var j = 0; j < children.length; j++) {
                if (children[j].nodeType == 1 && 
                    children[j].nodeName == "DIV" && 
                    children[j].className == "image") {
                    imagesInSection++;
                }
            }
            suitableImageSections++;
            suitableImages += imagesInSection;
            sectionsToProcess.push({
                section: section,
                parent: section.parentNode
            });
        }
    }
    
    // Выводим анализ
    if (showStatistics) {
        if (suitableImages == 0) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" + "Нет элементов, подходящих для обработки.");
            return;
        }
        
        var analysisMsg = scriptName + "\n" +
                         "ver. " + version + "\n\n" +
                         "  •  Всего блочных иллюстраций в документе: " + totalImages + "\n\n" +
                         "✓ Подходящих для обработки:\n" +
                         "     • Секций с иллюстрациями: " + suitableImageSections + "\n" +
                         "          • иллюстраций: " + suitableImages + "\n\n" +
                         "Обработать найденные элементы?";
        
        if (!AskYesNo(analysisMsg)) {
            return;
        }
    } else if (suitableImages == 0) {
        // Тихий режим - сообщаем, что нет подходящих иллюстраций
        MsgBox(scriptName + "\nver. " + version + "\n\n" + "Нет элементов, подходящих для обработки.");
        return;
    }
    
    // ТАЙМЕР ВКЛЮЧАЕМ ЗДЕСЬ - после подтверждения пользователя
    var startTime = new Date().getTime();
    
    // Начинаем блок отмены действий
    window.external.BeginUndoUnit(document, scriptName);
    
    // Фаза 2: обработка в обратном порядке (чтобы не сломать индексы)
    var processedImageSections = 0;
    var processedImages = 0;
    
    for (var i = sectionsToProcess.length - 1; i >= 0; i--) {
        var sectionData = sectionsToProcess[i];
        var section = sectionData.section;
        var parentSection = sectionData.parent;
        
        if (!parentSection || parentSection.nodeName != "DIV" || 
            (parentSection.className != "section" && parentSection.className != "body")) {
            continue;
        }
        
        // Находим иллюстрацию и пустые строки в этой секции
        var images = [];
        var emptyLines = [];
        var children = section.childNodes;
        
        // Собираем иллюстрации и пустые строки
        for (var j = 0; j < children.length; j++) {
            var child = children[j];
            if (child.nodeType == 1) {
                if (child.nodeName == "DIV" && child.className == "image") {
                    images.push(child);
                } else if (child.nodeName == "P" && isEmptyLine(child)) {
                    emptyLines.push(child);
                }
            }
        }
        
        if (images.length == 0) continue;
        
        // Находим позицию для вставки - после текущей секции
        var nextSibling = section.nextSibling;
        
        // Удаляем пустые строки, если есть
        for (var k = 0; k < emptyLines.length; k++) {
            if (emptyLines[k].parentNode == section) {
                emptyLines[k].parentNode.removeChild(emptyLines[k]);
            }
        }
        
        // Извлекаем все иллюстрации из текущей секции
        // Важно: извлекаем в обратном порядке, чтобы сохранить порядок
        var extractedImages = [];
        for (var k = images.length - 1; k >= 0; k--) {
            var image = section.removeChild(images[k]);
            extractedImages.unshift(image); // Добавляем в начало для сохранения порядка
        }
        
        // Вставляем все иллюстрации на место секции (в правильном порядке)
        for (var k = 0; k < extractedImages.length; k++) {
            if (nextSibling) {
                parentSection.insertBefore(extractedImages[k], nextSibling);
            } else {
                parentSection.appendChild(extractedImages[k]);
            }
        }
        
        // Удаляем теперь уже пустую секцию
        if (section.parentNode) {
            section.parentNode.removeChild(section);
        }
        
        processedImageSections++;
        processedImages += extractedImages.length;
    }
    
    // Завершаем блок отмены действий
    window.external.EndUndoUnit(document);
    
    // Вычисляем время выполнения
    var endTime = new Date().getTime();
    var timeDiff = (endTime - startTime) / 1000;
    var timeFormatted = timeDiff.toFixed(3).replace(".", ",");
    
    // Выводим статистику
    if (showStatistics) {
        var statsMsg = scriptName + "\n" +
                      "ver. " + version + "\n\n" +
                      "✓ Успешно переоформлено:\n" +
                      "     • Секций с иллюстрациями: " + processedImageSections + "\n" +
                      "          • иллюстраций: " + processedImages + "\n\n" +
                      "Время обработки: " + timeFormatted + " сек";
        
        MsgBox(statsMsg);
    }
}
