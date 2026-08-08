// Скрипт "Удалить лишние секции вокруг эпиграфов и блочных иллюстраций" для редактора FBE
// version 1.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для удаления избыточных секций вокруг эпиграфов и блочных иллюстраций в fb2 документах.
// Скрипт обрабатывает секции, которые содержат ЛИБО только эпиграфы (один или несколько), ЛИБО только одну блочную иллюстрацию,
// плюс пустую строку. Если в секции есть и эпиграфы, и иллюстрация - секция пропускается.
// Обрабатываемые секции являются либо первыми в боди, либо находятся в родительской секции, 
// за которой следует вложенная секция.
// Такие конструкции упрощаются: содержимое (эпиграфы или иллюстрация) переносится на уровень родительской секции,
// а обрамлявшая секция удаляется вместе с пустой строкой.
// Это улучшает структуру документа и убирает лишнюю вложенность.
// Скрипт работает с основным разделом книги (не затрагивает разделы примечаний и комментариев).
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.2, 21.04.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Удалить лишние секции вокруг эпиграфов и блочных иллюстраций";
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
    
    // Функция для анализа содержимого секции
    // Возвращает объект с информацией: что есть в секции и можно ли её обработать
    function analyzeSectionContent(section) {
        if (section.className != "section") return null;
        
        var children = section.childNodes;
        var hasEpigraph = false;
        var epigraphCount = 0;
        var hasImage = false;
        var imageCount = 0;
        var hasOnlyAllowedContent = true;
        
        for (var j = 0; j < children.length; j++) {
            var child = children[j];
            if (child.nodeType == 1) { // ELEMENT_NODE
                if (child.nodeName == "DIV" && child.className == "epigraph") {
                    hasEpigraph = true;
                    epigraphCount++;
                } else if (child.nodeName == "DIV" && child.className == "image") {
                    hasImage = true;
                    imageCount++;
                } else if (child.nodeName == "P" && isEmptyLine(child)) {
                    // Пустая строка - разрешена
                } else if (child.nodeName == "DIV" && 
                          (child.className == "title" || child.className == "annotation" || 
                           child.className == "section")) {
                    // Запрещённые элементы
                    hasOnlyAllowedContent = false;
                    break;
                } else if (child.nodeName != "P" || !isEmptyLine(child)) {
                    // Любой другой элемент кроме пустого P
                    hasOnlyAllowedContent = false;
                    break;
                }
            } else if (child.nodeType == 3) { // TEXT_NODE
                var text = child.nodeValue || "";
                var trimmedText = text.replace(/\s/g, "");
                if (trimmedText != "") {
                    hasOnlyAllowedContent = false;
                    break;
                }
            }
        }
        
        // Определяем тип содержимого
        var contentType = "none";
        var isValid = false;
        
        if (!hasOnlyAllowedContent) {
            contentType = "invalid";
        } else if (hasEpigraph && !hasImage) {
            // Только эпиграфы
            contentType = "epigraph";
            isValid = (epigraphCount >= 1);
        } else if (hasImage && !hasEpigraph) {
            // Только картинки (должна быть ровно одна)
            contentType = "image";
            isValid = (imageCount == 1);
        } else if (hasEpigraph && hasImage) {
            // Смешанное содержимое - НЕ ОБРАБАТЫВАЕМ
            contentType = "mixed";
            isValid = false;
        }
        
        return {
            contentType: contentType,
            isValid: isValid,
            epigraphCount: epigraphCount,
            imageCount: imageCount
        };
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
    function canProcessSection(section, contentType) {
        var parent = section.parentNode;
        if (!parent || parent.nodeName != "DIV") return false;
        
        // Родитель должен быть section или body
        if (parent.className != "section" && parent.className != "body") return false;
        
        // ИСКЛЮЧЕНИЕ ДЛЯ КАРТИНОК: если родитель - body, и перед секцией с иллюстрацией идёт заголовок body
        // (то есть иллюстрация сразу после title внутри body), то НЕ ОБРАБАТЫВАЕМ
        if (contentType == "image" && parent.className == "body") {
            var prevSibling = section.previousSibling;
            // Проверяем, что предыдущий элемент - это заголовок body
            if (prevSibling && prevSibling.nodeType == 1 && 
                prevSibling.nodeName == "DIV" && prevSibling.className == "title") {
                return false;
            }
        }
        
        // Проверяем, является ли эта секция ПЕРВЫМ непустым содержимым в родителе
        var children = parent.childNodes;
        var foundTargetSection = false;
        
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            
            if (child === section) {
                // Нашли нашу секцию
                foundTargetSection = true;
                // Продолжаем проверку следующих элементов
                continue;
            }
            
            if (child.nodeType != 1) continue; // Пропускаем текстовые узлы
            
            if (foundTargetSection) {
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
    var totalEpigraphs = 0;
    var totalImages = 0;
    var suitableEpigraphSections = 0;
    var suitableEpigraphs = 0;
    var suitableImageSections = 0;
    var suitableImages = 0;
    
    // Считаем все эпиграфы и картинки
    var allDivs2 = document.getElementsByTagName("DIV");
    for (var i = 0; i < allDivs2.length; i++) {
        if (allDivs2[i].className == "epigraph") {
            totalEpigraphs++;
        } else if (allDivs2[i].className == "image") {
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
        var analysis = analyzeSectionContent(section);
        
        if (analysis && analysis.isValid) {
            if (canProcessSection(section, analysis.contentType)) {
                if (analysis.contentType == "epigraph") {
                    suitableEpigraphSections++;
                    suitableEpigraphs += analysis.epigraphCount;
                } else if (analysis.contentType == "image") {
                    suitableImageSections++;
                    suitableImages += analysis.imageCount;
                }
                sectionsToProcess.push({
                    section: section,
                    parent: section.parentNode,
                    contentType: analysis.contentType
                });
            }
        }
    }
    
    // Считаем общее количество подходящих элементов
    var totalSuitable = suitableEpigraphs + suitableImages;
    
    // Выводим анализ
    if (showStatistics) {
        if (totalSuitable == 0) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" + "Нет элементов, подходящих для обработки.");
            return;
        }
        
        var analysisMsg = scriptName + "\n" +
                         "ver. " + version + "\n\n" +
                         "  •  Всего эпиграфов в документе: " + totalEpigraphs + "\n" +
                         "  •  Всего блочных иллюстраций в документе: " + totalImages + "\n\n" +
                         "✓ Подходящих для обработки:\n";
        
        if (suitableEpigraphSections > 0) {
            analysisMsg += "     • Секций с эпиграфами: " + suitableEpigraphSections + "\n";
            analysisMsg += "          • эпиграфов: " + suitableEpigraphs + "\n";
        }
        
        if (suitableImageSections > 0) {
            analysisMsg += "     • Секций с иллюстрациями: " + suitableImageSections + "\n";
            analysisMsg += "          • иллюстраций: " + suitableImages + "\n";
        }
        
        analysisMsg += "\nОбработать найденные элементы?";
        
        if (!AskYesNo(analysisMsg)) {
            return;
        }
    } else if (totalSuitable == 0) {
        // Тихий режим - сообщаем, что нет подходящих элементов
        MsgBox(scriptName + "\nver. " + version + "\n\n" + "Нет элементов, подходящих для обработки.");
        return;
    }
    
    // ТАЙМЕР ВКЛЮЧАЕМ ЗДЕСЬ - после подтверждения пользователя
    var startTime = new Date().getTime();
    
    // Начинаем блок отмены действий
    window.external.BeginUndoUnit(document, scriptName);
    
    // Фаза 2: обработка в обратном порядке (чтобы не сломать индексы)
    var processedEpigraphSections = 0;
    var processedEpigraphs = 0;
    var processedImageSections = 0;
    var processedImages = 0;
    
    for (var i = sectionsToProcess.length - 1; i >= 0; i--) {
        var sectionData = sectionsToProcess[i];
        var section = sectionData.section;
        var parentSection = sectionData.parent;
        var contentType = sectionData.contentType;
        
        if (!parentSection || parentSection.nodeName != "DIV" || 
            (parentSection.className != "section" && parentSection.className != "body")) {
            continue;
        }
        
        // Находим содержимое и пустые строки в этой секции
        var contentElements = [];
        var emptyLines = [];
        var children = section.childNodes;
        
        // Собираем нужные элементы и пустые строки
        for (var j = 0; j < children.length; j++) {
            var child = children[j];
            if (child.nodeType == 1) {
                if (contentType == "epigraph" && child.nodeName == "DIV" && child.className == "epigraph") {
                    contentElements.push(child);
                } else if (contentType == "image" && child.nodeName == "DIV" && child.className == "image") {
                    contentElements.push(child);
                } else if (child.nodeName == "P" && isEmptyLine(child)) {
                    emptyLines.push(child);
                }
            }
        }
        
        if (contentElements.length == 0) continue;
        
        // Находим позицию для вставки - после текущей секции
        var nextSibling = section.nextSibling;
        
        // Удаляем пустые строки, если есть
        for (var k = 0; k < emptyLines.length; k++) {
            if (emptyLines[k].parentNode == section) {
                emptyLines[k].parentNode.removeChild(emptyLines[k]);
            }
        }
        
        // Извлекаем все нужные элементы из текущей секции
        // Важно: извлекаем в обратном порядке, чтобы сохранить порядок
        var extractedElements = [];
        for (var k = contentElements.length - 1; k >= 0; k--) {
            var element = section.removeChild(contentElements[k]);
            extractedElements.unshift(element); // Добавляем в начало для сохранения порядка
        }
        
        // Вставляем все элементы на место секции (в правильном порядке)
        for (var k = 0; k < extractedElements.length; k++) {
            if (nextSibling) {
                parentSection.insertBefore(extractedElements[k], nextSibling);
            } else {
                parentSection.appendChild(extractedElements[k]);
            }
        }
        
        // Удаляем теперь уже пустую секцию
        if (section.parentNode) {
            section.parentNode.removeChild(section);
        }
        
        if (contentType == "epigraph") {
            processedEpigraphSections++;
            processedEpigraphs += extractedElements.length;
        } else if (contentType == "image") {
            processedImageSections++;
            processedImages += extractedElements.length;
        }
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
                      "✓ Успешно переоформлено:\n";
        
        if (processedEpigraphSections > 0) {
            statsMsg += "     • Секций с эпиграфами: " + processedEpigraphSections + "\n";
            statsMsg += "          • эпиграфов: " + processedEpigraphs + "\n";
        }
        
        if (processedImageSections > 0) {
            statsMsg += "     • Секций с иллюстрациями: " + processedImageSections + "\n";
            statsMsg += "          • иллюстраций: " + processedImages + "\n";
        }
        
        statsMsg += "\nВремя обработки: " + timeFormatted + " сек";
        
        MsgBox(statsMsg);
    }
}
