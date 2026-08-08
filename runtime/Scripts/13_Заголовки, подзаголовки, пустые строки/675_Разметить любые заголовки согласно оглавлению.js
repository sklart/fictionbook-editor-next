// Скрипт "Разметить любые заголовки согласно оглавлению" для редактора FBE
// version 2.0
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт автоматически размечает заголовки в fb2 документах согласно выделенному фрагменту оглавления.
// Ищет совпадения текста заголовков в основном тексте документа, игнорируя форматирование и сноски.
// Обрабатывает как отдельные абзацы-заголовки, так и прилипшие к концам обычных абзацев тексты.
// Объединяет разорванные заголовки (например, "Глава 1." на одной строке и "Название" на следующей).
// Обработка разделов сносок и комментариев (опционально).
// Режим работы скрипта: обычный или тихий.
// Поддержка отмены всех действий (Ctrl+Z).

// В отличие от скрипта 07_Разметить заголовки разделов согласно оглавлению документа....js
// данный скрипт более правильно находит совпадения оглавления и заголовков
// и умеет объединять разорванные строки заголовков.
// В данной версии скрипта нет настроечных диалоговых окон, все настройки задаются внутри скрипта.

// version 2.0, 05.02.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Разметить любые заголовки согласно оглавлению";
    var version = "2.0";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима
    
    // Показывать список успешно созданных заголовков
    var showCreatedTitlesList = 0; // 0 - нет, 1 - да
    
    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0; // 0 - нет, 1 - да
    
    // Обрабатывать раздел комментариев
    var processCommentsSection = 0; // 0 - нет, 1 - да
    
    // Если заголовок уже есть, создавать ли такой же, если найдется?
    var processTitleDuplicate = 0; // 0 - нет, 1 - да
    
    // Проверять совпадение текста без учета регистра
    var processAnyLetters = 0; // 0 - нет, 1 - да
    
    // Оставить регистр создаваемого заголовка без изменения
    var processLettersAsIs = 1; // 0 - нет, сделать как в оглавлении, 1 - да (оставить без изменений)
    
    // Удалить выделенный фрагмент после полной успешной расстановки
    var processDeleteSelection = 1; // 0 - нет, 1 - да
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    // Таймер для измерения времени выполнения
    var startTime = new Date().getTime();
    
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
    
    // Список необычных пробелов, которые должны обрабатываться наравне с обычными
    var unusualSpaces = String.fromCharCode(160) +  // неразрывный пробел
        String.fromCharCode(8194) +  // EN SPACE
        String.fromCharCode(8195) +  // EM SPACE
        String.fromCharCode(8196) +  // THREE-PER-EM SPACE
        String.fromCharCode(8197) +  // FOUR-PER-EM SPACE
        String.fromCharCode(8198) +  // SIX-PER-EM SPACE
        String.fromCharCode(8239) +  // NARROW NO-BREAK SPACE
        String.fromCharCode(8201) +  // THIN SPACE
        String.fromCharCode(8202) +  // HAIR SPACE
        nbspChar;
    
    // Проверяем, есть ли выделение
    if (!document.selection || document.selection.type != "Text") {
        MsgBox(scriptName + "\nver. " + version + "\n\n✗ Перед запуском скрипта выделите фрагмент оглавления!");
        return;
    }
    
    // Получаем выделенный текст (оглавление)
    var selectionRange = document.selection.createRange();
    var selectionText = selectionRange.text;
    
    if (!selectionText || selectionText.replace(/\s/g, '').length === 0) {
        MsgBox(scriptName + "\nver. " + version + "\n\n✗ Выделенный фрагмент пуст или содержит только пробелы!");
        return;
    }
    
    // Разбиваем выделенный текст на строки (заголовки оглавления)
    var tocLines = selectionText.split(/\r\n|\r|\n/);
    var cleanTocLines = [];
    var originalTocLines = [];
    
    // Объединяем строки, если они разорваны
    var combinedTocLines = [];
    var currentLine = "";
    
    for (var i = 0; i < tocLines.length; i++) {
        var line = tocLines[i];
        line = line.replace(new RegExp("^[\\s" + unusualSpaces + "]+"), "");
        line = line.replace(new RegExp("[\\s" + unusualSpaces + "]+$"), "");
        
        if (line.length === 0) {
            if (currentLine.length > 0) {
                combinedTocLines.push(currentLine);
                currentLine = "";
            }
            continue;
        }
        
        if (currentLine.length > 0) {
            var lastWord = currentLine.split(/\s+/).pop();
            if (lastWord.match(/^(глава|часть|раздел|параграф|приложение|прил\.|гл\.|ч\.|разд\.|§|п\.)\s*\d*\.?$/i) ||
                lastWord.match(/^\d+\.$/) ||
                lastWord.match(/[.:;]$/)) {
                currentLine += " " + line;
            } else {
                combinedTocLines.push(currentLine);
                currentLine = line;
            }
        } else {
            currentLine = line;
        }
    }
    
    if (currentLine.length > 0) {
        combinedTocLines.push(currentLine);
    }
    
    tocLines = combinedTocLines;
    
    // Очищаем строки
    for (var i = 0; i < tocLines.length; i++) {
        var line = tocLines[i];
        line = line.replace(new RegExp("^[\\s" + unusualSpaces + "]+"), "");
        line = line.replace(new RegExp("[\\s" + unusualSpaces + "]+$"), "");
        
        if (line.length > 0) {
            originalTocLines.push(tocLines[i]);
            cleanTocLines.push(line);
        }
    }
    
    if (cleanTocLines.length === 0) {
        MsgBox(scriptName + "\nver. " + version + "\n\n✗ В выделенном фрагменте не найдено ни одной непустой строки!");
        return;
    }
    
    // Функция для нормализации текста
    function normalizeText(text) {
        if (!text) return "";
        
        var spaceRegex = new RegExp("[" + unusualSpaces + "]", "g");
        text = text.replace(spaceRegex, " ");
        text = text.replace(/[–—]/g, "-");
        text = text.replace(/…/g, "...");
        text = text.replace(/^\s+/, "").replace(/\s+$/, "");
        text = text.replace(/\s+/g, " ");
        
        if (processAnyLetters) {
            text = text.toLowerCase();
        }
        
        return text;
    }
    
    // Нормализуем строки оглавления
    var normalizedTocLines = [];
    for (var i = 0; i < cleanTocLines.length; i++) {
        normalizedTocLines.push(normalizeText(cleanTocLines[i]));
    }
    
    // Получаем основной body документа
    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) {
        MsgBox(scriptName + "\nver. " + version + "\n\n✗ Не найден основной body документа!");
        return;
    }
    
    // Функция для получения текста без тегов (игнорируем только сноски)
    function getTextWithoutTags(element) {
        var result = "";
        
        function traverse(node) {
            if (node.nodeType == 3) {
                result += node.nodeValue;
            } else if (node.nodeType == 1) {
                if (node.nodeName == "A") {
                    var className = node.className || "";
                    if (className == "note") {
                        return;
                    }
                }
                
                for (var i = 0; i < node.childNodes.length; i++) {
                    traverse(node.childNodes[i]);
                }
            }
        }
        
        traverse(element);
        return result;
    }
    
    // Функция для проверки, находится ли элемент внутри выделенного фрагмента
    function isElementInSelection(element) {
        if (!element || !selectionRange) return false;
        
        try {
            var elementRange = document.body.createTextRange();
            elementRange.moveToElementText(element);
            
            var startComparison = selectionRange.compareEndPoints("StartToStart", elementRange);
            var endComparison = selectionRange.compareEndPoints("EndToEnd", elementRange);
            
            if (startComparison <= 0 && endComparison >= 0) {
                return true;
            }
        } catch(e) {
            return false;
        }
        
        return false;
    }
    
    // Собираем ВСЕ параграфы в документе (оптимизированный сбор как в 1.6)
    var allParagraphs = [];
    
    // Сначала собираем все DIV body которые нужно обрабатывать
    var allBodies = fbwBody.getElementsByTagName("DIV");
    var bodiesToProcess = [];
    
    for (var i = 0; i < allBodies.length; i++) {
        var div = allBodies[i];
        if (div.className != "body") continue;
        
        var fbname = div.getAttribute("fbname") || "";
        var shouldProcess = false;
        
        if (fbname == "" && processNotesSection == 0 && processCommentsSection == 0) {
            shouldProcess = true;
        } else if (fbname == "notes" && processNotesSection == 1) {
            shouldProcess = true;
        } else if (fbname == "comments" && processCommentsSection == 1) {
            shouldProcess = true;
        }
        
        if (shouldProcess) {
            bodiesToProcess.push(div);
        }
    }
    
    // Собираем параграфы из нужных body
    for (var b = 0; b < bodiesToProcess.length; b++) {
        var bodyDiv = bodiesToProcess[b];
        
        // Находим все секции внутри body
        var sections = [];
        var childNodes = bodyDiv.childNodes;
        
        for (var i = 0; i < childNodes.length; i++) {
            var node = childNodes[i];
            if (node.nodeType == 1 && node.nodeName == "DIV" && node.className == "section") {
                sections.push(node);
            }
        }
        
        // Если нет секций, считаем сам body как секцию
        if (sections.length === 0) {
            sections.push(bodyDiv);
        }
        
        // Обрабатываем каждую секцию
        for (var s = 0; s < sections.length; s++) {
            var section = sections[s];
            var paragraphs = section.getElementsByTagName("P");
            
            for (var j = 0; j < paragraphs.length; j++) {
                var p = paragraphs[j];
                
                // Пропускаем параграфы внутри блочных элементов
                var parent = p.parentNode;
                var skip = false;
                
                while (parent && parent != section) {
                    var parentClass = parent.className || "";
                    if (parentClass == "cite" || parentClass == "poem" || 
                        parentClass == "stanza" || parentClass == "epigraph" ||
                        parentClass == "text-author") {
                        skip = true;
                        break;
                    }
                    parent = parent.parentNode;
                }
                
                if (skip) continue;
                
                // Пропускаем уже оформленные заголовки
                if (!processTitleDuplicate && p.parentNode.className == "title") {
                    continue;
                }
                
                // Пропускаем параграфы внутри выделения
                if (isElementInSelection(p)) {
                    continue;
                }
                
                // Получаем текст
                var text = getTextWithoutTags(p);
                
                // Пропускаем пустые параграфы
                if (text.replace(/\s/g, '').length === 0) {
                    continue;
                }
                
                allParagraphs.push({
                    element: p,
                    text: text,
                    html: p.innerHTML,
                    parentDiv: bodyDiv,
                    parentSection: section,
                    position: allParagraphs.length,
                    normalizedText: normalizeText(text)
                });
            }
        }
    }
    
    if (allParagraphs.length === 0) {
        MsgBox(scriptName + "\nver. " + version + "\n\n✗ В документе не найдено подходящих абзацев для обработки!");
        return;
    }
    
    // Улучшенный поиск как в версии 1.6
    var operations = [];
    var processedIndices = {};
    var lastFoundPosition = -1;
    
    for (var tocIndex = 0; tocIndex < normalizedTocLines.length; tocIndex++) {
        var searchText = normalizedTocLines[tocIndex];
        var originalText = originalTocLines[tocIndex];
        var found = false;
        
        // Ищем совпадение, начиная с позиции после последнего найденного
        for (var pIndex = lastFoundPosition + 1; pIndex < allParagraphs.length; pIndex++) {
            if (processedIndices[pIndex]) continue;
            
            var paragraph = allParagraphs[pIndex];
            var normalizedParaText = paragraph.normalizedText;
            
            // Проверяем полное совпадение
            if (normalizedParaText === searchText) {
                operations.push({
                    type: "whole",
                    paragraph: paragraph,
                    tocIndex: tocIndex,
                    searchText: searchText,
                    originalText: originalText,
                    position: pIndex
                });
                processedIndices[pIndex] = true;
                lastFoundPosition = pIndex;
                found = true;
                break;
            }
            
            // Проверяем совпадение в конце
            if (normalizedParaText.length >= searchText.length) {
                var endPart = normalizedParaText.substring(normalizedParaText.length - searchText.length);
                if (endPart === searchText) {
                    operations.push({
                        type: "end",
                        paragraph: paragraph,
                        tocIndex: tocIndex,
                        searchText: searchText,
                        originalText: originalText,
                        position: pIndex
                    });
                    processedIndices[pIndex] = true;
                    lastFoundPosition = pIndex;
                    found = true;
                    break;
                }
            }
        }
        
        // Если не нашли с текущей позиции, ищем с начала документа
        if (!found) {
            for (var pIndex = 0; pIndex < allParagraphs.length; pIndex++) {
                if (processedIndices[pIndex]) continue;
                
                var paragraph = allParagraphs[pIndex];
                var normalizedParaText = paragraph.normalizedText;
                
                if (normalizedParaText === searchText) {
                    operations.push({
                        type: "whole",
                        paragraph: paragraph,
                        tocIndex: tocIndex,
                        searchText: searchText,
                        originalText: originalText,
                        position: pIndex
                    });
                    processedIndices[pIndex] = true;
                    lastFoundPosition = pIndex;
                    found = true;
                    break;
                }
                
                if (normalizedParaText.length >= searchText.length) {
                    var endPart = normalizedParaText.substring(normalizedParaText.length - searchText.length);
                    if (endPart === searchText) {
                        operations.push({
                            type: "end",
                            paragraph: paragraph,
                            tocIndex: tocIndex,
                            searchText: searchText,
                            originalText: originalText,
                            position: pIndex
                        });
                        processedIndices[pIndex] = true;
                        lastFoundPosition = pIndex;
                        found = true;
                        break;
                    }
                }
            }
        }
    }
    
    // Статистика
    var createdTitles = operations.length;
    var skippedTitles = normalizedTocLines.length - createdTitles;
    var notFoundLines = [];
    var createdTitlesList = [];
    var wholeCount = 0, endCount = 0;
    
    // Заполняем списки
    var foundTocIndices = {};
    for (var i = 0; i < operations.length; i++) {
        foundTocIndices[operations[i].tocIndex] = true;
        createdTitlesList.push(operations[i].originalText);
        if (operations[i].type == "whole") wholeCount++;
        else endCount++;
    }
    
    for (var tocIndex = 0; tocIndex < normalizedTocLines.length; tocIndex++) {
        if (!foundTocIndices[tocIndex]) {
            notFoundLines.push({
                index: tocIndex + 1,
                text: originalTocLines[tocIndex]
            });
        }
    }
    
    // Начинаем undo unit
    window.external.BeginUndoUnit(document, scriptName);
    
    // Сортируем операции по позиции в документе
    operations.sort(function(a, b) {
        return a.position - b.position;
    });
    
    // Выполняем операции (с конца, чтобы индексы не смещались)
    for (var i = operations.length - 1; i >= 0; i--) {
        var op = operations[i];
        var paragraph = op.paragraph;
        var paragraphElement = paragraph.element;
        var parentSection = paragraph.parentSection;
        var parentDiv = paragraph.parentDiv;
        
        // Определяем, нужно ли создавать новую секцию
        var needNewSection = (parentSection == parentDiv); // Если родитель - body, а не section
        
        if (op.type == "whole") {
            // Целый параграф
            if (needNewSection) {
                // Создаем новую секцию
                var newSection = document.createElement("DIV");
                newSection.className = "section";
                
                var titleDiv = document.createElement("DIV");
                titleDiv.className = "title";
                
                var titleParagraph = document.createElement("P");
                if (processLettersAsIs) {
                    titleParagraph.innerHTML = originalTocLines[op.tocIndex];
                } else {
                    titleParagraph.innerHTML = paragraph.html;
                }
                
                titleDiv.appendChild(titleParagraph);
                newSection.appendChild(titleDiv);
                
                // Заменяем параграф новой секцией
                paragraphElement.parentNode.replaceChild(newSection, paragraphElement);
            } else {
                // Уже в секции, создаем заголовок внутри нее
                var titleDiv = document.createElement("DIV");
                titleDiv.className = "title";
                
                var titleParagraph = document.createElement("P");
                if (processLettersAsIs) {
                    titleParagraph.innerHTML = originalTocLines[op.tocIndex];
                } else {
                    titleParagraph.innerHTML = paragraph.html;
                }
                
                titleDiv.appendChild(titleParagraph);
                
                // Вставляем заголовок перед параграфом
                paragraphElement.parentNode.insertBefore(titleDiv, paragraphElement);
                
                // Удаляем параграф
                paragraphElement.parentNode.removeChild(paragraphElement);
            }
            
        } else if (op.type == "end") {
            // Прилипший в конце
            var originalParaText = paragraph.text;
            var searchTextOriginal = cleanTocLines[op.tocIndex];
            
            // Ищем точную позицию совпадения
            var paraLength = originalParaText.length;
            var searchLength = searchTextOriginal.length;
            var foundPos = -1;
            
            // Ищем с конца, но сохраняем знаки препинания перед заголовком
            for (var pos = paraLength - searchLength; pos >= 0; pos--) {
                var substr = originalParaText.substring(pos, paraLength);
                if (normalizeText(substr) === op.searchText) {
                    foundPos = pos;
                    break;
                }
            }
            
            if (foundPos === -1) {
                foundPos = Math.max(0, paraLength - searchLength);
            }
            
            // Важно: сохраняем знаки препинания (точки, восклицательные и вопросительные знаки) перед заголовком
            var punctuationBeforeTitle = "";
            if (foundPos > 0) {
                var charBefore = originalParaText.charAt(foundPos - 1);
                // Если перед заголовком стоит точка, сохраняем ее в исходном тексте
                if (charBefore === '.' || charBefore === '!' || charBefore === '?' || charBefore === ';' || charBefore === ':') {
                    // Оставляем знак препинания в исходном тексте
                    // Не уменьшаем foundPos, чтобы знак остался в исходном тексте
                }
            }
            
            // Обрезаем исходный параграф (оставляем знаки препинания)
            var remainingText = originalParaText.substring(0, foundPos);
            
            // Убираем только пробелы и дефисы в конце, но НЕ точки и другие знаки препинания
            remainingText = remainingText.replace(/[\s—–\-]+$/, "");
            
            if (remainingText.length > 0) {
                // Сохраняем начало параграфа
                paragraphElement.innerHTML = remainingText.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
                
                if (needNewSection) {
                    // Создаем новую секцию с заголовком
                    var newSection = document.createElement("DIV");
                    newSection.className = "section";
                    
                    var titleDiv = document.createElement("DIV");
                    titleDiv.className = "title";
                    
                    var titleParagraph = document.createElement("P");
                    var titleText = originalParaText.substring(foundPos);
                    
                    if (processLettersAsIs) {
                        // Берем оригинальный текст из конца абзаца
                        titleParagraph.innerHTML = titleText.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
                    } else {
                        // Берем текст из оглавления
                        titleParagraph.innerHTML = originalTocLines[op.tocIndex];
                    }
                    
                    titleDiv.appendChild(titleParagraph);
                    newSection.appendChild(titleDiv);
                    
                    // Вставляем новую секцию после обрезанного параграфа
                    paragraphElement.parentNode.insertBefore(newSection, paragraphElement.nextSibling);
                } else {
                    // Создаем заголовок внутри существующей секции
                    var titleDiv = document.createElement("DIV");
                    titleDiv.className = "title";
                    
                    var titleParagraph = document.createElement("P");
                    var titleText = originalParaText.substring(foundPos);
                    
                    if (processLettersAsIs) {
                        titleParagraph.innerHTML = titleText.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
                    } else {
                        titleParagraph.innerHTML = originalTocLines[op.tocIndex];
                    }
                    
                    titleDiv.appendChild(titleParagraph);
                    
                    // Вставляем заголовок после обрезанного параграфа
                    paragraphElement.parentNode.insertBefore(titleDiv, paragraphElement.nextSibling);
                }
            } else {
                // Весь параграф стал заголовком
                if (needNewSection) {
                    var newSection = document.createElement("DIV");
                    newSection.className = "section";
                    
                    var titleDiv = document.createElement("DIV");
                    titleDiv.className = "title";
                    
                    var titleParagraph = document.createElement("P");
                    if (processLettersAsIs) {
                        titleParagraph.innerHTML = paragraph.html;
                    } else {
                        titleParagraph.innerHTML = originalTocLines[op.tocIndex];
                    }
                    
                    titleDiv.appendChild(titleParagraph);
                    newSection.appendChild(titleDiv);
                    paragraphElement.parentNode.replaceChild(newSection, paragraphElement);
                } else {
                    var titleDiv = document.createElement("DIV");
                    titleDiv.className = "title";
                    
                    var titleParagraph = document.createElement("P");
                    if (processLettersAsIs) {
                        titleParagraph.innerHTML = paragraph.html;
                    } else {
                        titleParagraph.innerHTML = originalTocLines[op.tocIndex];
                    }
                    
                    titleDiv.appendChild(titleParagraph);
                    
                    // Вставляем заголовок на место параграфа
                    paragraphElement.parentNode.replaceChild(titleDiv, paragraphElement);
                }
            }
        }
    }
    
    window.external.EndUndoUnit(document);
    
    // Вычисляем время выполнения
    var endTime = new Date().getTime();
    var executionTime = (endTime - startTime) / 1000;
    var timeStr = executionTime.toFixed(3).replace(".", ",");
    
    // Формируем статистику
    if (showStatistics) {
        var statsMessage = scriptName + "\nver. " + version + "\n\n";
        
        statsMessage += "✓ Обработка завершена\n";
        statsMessage += "────────────────────\n";
        statsMessage += "• Строк в оглавлении: " + normalizedTocLines.length + "\n";
        statsMessage += "• Создано заголовков: " + createdTitles + "\n";
        statsMessage += "• Не найдено совпадений: " + skippedTitles + "\n";
        
        // Статистика по типам
        statsMessage += "  • Целые абзацы: " + wholeCount + "\n";
        statsMessage += "  • Прилипшие в конце: " + endCount + "\n";
        
        if (showCreatedTitlesList && createdTitlesList.length > 0) {
            statsMessage += "\n✓ Успешно созданы заголовки:\n";
            for (var i = 0; i < Math.min(createdTitlesList.length, 10); i++) {
                var titleText = createdTitlesList[i];
                if (titleText.length > 60) {
                    titleText = titleText.substring(0, 57) + "...";
                }
                statsMessage += "  • " + titleText + "\n";
            }
            if (createdTitlesList.length > 10) {
                statsMessage += "  ... и еще " + (createdTitlesList.length - 10) + " заголовков\n";
            }
        }
        
        if (notFoundLines.length > 0) {
            statsMessage += "\n✗ Не найдены совпадения для строк:\n";
            var lineNumbers = [];
            for (var i = 0; i < notFoundLines.length; i++) {
                lineNumbers.push(notFoundLines[i].index);
            }
            statsMessage += "  • Номера: " + lineNumbers.join(", ") + "\n";
            
            var showCount = Math.min(notFoundLines.length, 5);
            statsMessage += "  • Примеры:\n";
            for (var i = 0; i < showCount; i++) {
                var titleText = notFoundLines[i].text;
                if (titleText.length > 60) {
                    titleText = titleText.substring(0, 57) + "...";
                }
                statsMessage += "    - " + titleText + "\n";
            }
            if (notFoundLines.length > showCount) {
                statsMessage += "    ... и еще " + (notFoundLines.length - showCount) + " строк\n";
            }
        }
        
        statsMessage += "\n────────────────────\n";
        statsMessage += "Время обработки: " + timeStr + " сек";
        
        MsgBox(statsMessage);
    }
    
    // Если все заголовки были успешно расставлены и нужно удалить выделение
    if (processDeleteSelection && skippedTitles === 0 && createdTitles > 0) {
        if (AskYesNo(scriptName + "\nver. " + version + 
                    "\n\nВсе заголовки успешно расставлены (" + createdTitles + " из " + 
                    normalizedTocLines.length + ").\n\nУдалить исходный выделенный фрагмент оглавления?")) {
            selectionRange.text = "";
        }
    }
}
