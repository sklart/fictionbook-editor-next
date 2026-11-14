// Скрипт "Объединить секцию, состоящую из иллюстраций, с вышестоящей секцией" для редактора FBE
// version 1.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для автоматического объединения безымянных секций с иллюстрациями
// с вышерасположенными секциями в fb2 документах.
// Скрипт находит безымянные секции, содержащие только изображения и пустые строки
// и присоединяет такие секции к вышестоящей секции, сохраняя порядок элементов.
// Между текстом или картинкой верхней секции и присоединенными картинками
// при необходимости вставляется разделитель (пустая строка).
// Лишние пустые строки после последней иллюстрации в получившейся объединенной секции удаляются.
// Обрабатывается только основной раздел документа (сноски и комментарии исключены из обработки).
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.1, 25.01.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Объединить секцию, состоящую из картинок, с вышестоящей секцией";
    var version = "1.1";
    
// ==================================================
// НАСТРОЙКИ СКРИПТА
// ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима
    
    // Настройка: обрабатывать первую секцию в body? 1 - да, 0 - нет
    var processFirstSection = 0; // По умолчанию - не обрабатываем
    
// ==================================================
// НАЧАЛО СКРИПТА
// ==================================================
    
    // Получаем символ неразрывного пробела из настроек FBE
    var nbspEntity = "&nbsp;";
    try {
        var nbspChar = window.external.GetNBSP();
        if (nbspChar.charCodeAt(0) != 160) {
            nbspEntity = nbspChar;
        }
    } catch(e) {
        var nbspChar = String.fromCharCode(160);
        var nbspEntity = "&nbsp;";
    }
    
    // Необычные пробелы для проверки
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
    
    // 1. Находим основной body (исключая notes/comments)
    var mainBody = null;
    var allBodies = document.getElementsByTagName("DIV");
    for (var i = 0; i < allBodies.length; i++) {
        var body = allBodies[i];
        if (body.className == "body") {
            var fbname = body.getAttribute("fbname") || "";
            if (fbname == "") {  // Это основной body
                mainBody = body;
                break;
            }
        }
    }
    
    if (!mainBody) {
        MsgBox(scriptName + "\nver. " + version + "\n\nНе найден основной body документа!");
        return;
    }
    
    // 2. Собираем все секции в основном body (прямые потомки)
    var allSections = [];
    var childNodes = mainBody.childNodes;
    for (var i = 0; i < childNodes.length; i++) {
        var node = childNodes[i];
        if (node.nodeType == 1 && node.nodeName == "DIV" && node.className == "section") {
            allSections.push({
                element: node,
                index: allSections.length,
                isCandidate: false,
                aboveSection: null
            });
        }
    }
    
    var initialSectionCount = allSections.length;
    
    // 3. Функция для проверки, является ли абзац пустым
    function isEmptyParagraph(pElement) {
        if (!pElement || pElement.nodeName != "P") return false;
        
        var isEmpty = true;
        var children = pElement.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 3) { // TEXT_NODE
                var text = child.nodeValue;
                for (var j = 0; j < text.length; j++) {
                    var charCode = text.charCodeAt(j);
                    var isSpace = false;
                    
                    // Проверяем обычный пробел
                    if (charCode == 32) {
                        isSpace = true;
                    } else {
                        // Проверяем необычные пробелы
                        for (var k = 0; k < unusualSpaces.length; k++) {
                            if (charCode == unusualSpaces.charCodeAt(k)) {
                                isSpace = true;
                                break;
                            }
                        }
                    }
                    
                    if (!isSpace) {
                        isEmpty = false;
                        break;
                    }
                }
                if (!isEmpty) break;
            } else if (child.nodeType == 1) {
                // Есть дочерние элементы - не пустой
                isEmpty = false;
                break;
            }
        }
        
        return isEmpty;
    }
    
    // 4. Анализируем секции на предмет кандидатов
    var candidates = [];
    
    for (var i = 0; i < allSections.length; i++) {
        var sectionInfo = allSections[i];
        var section = sectionInfo.element;
        
        // Пропускаем первую секцию, если настроено
        if (i == 0 && !processFirstSection) {
            continue;
        }
        
        // Проверяем, есть ли заголовок
        var hasTitle = false;
        var titleDivs = section.getElementsByTagName("DIV");
        for (var j = 0; j < titleDivs.length; j++) {
            if (titleDivs[j].className == "title") {
                hasTitle = true;
                break;
            }
        }
        
        // Если есть заголовок - не кандидат
        if (hasTitle) continue;
        
        // Анализируем содержимое секции
        var imageCount = 0;
        var emptyPCount = 0;
        var hasText = false;
        var hasOtherElements = false;
        
        var childNodes = section.childNodes;
        for (var j = 0; j < childNodes.length; j++) {
            var node = childNodes[j];
            if (node.nodeType == 1) { // ELEMENT_NODE
                if (node.nodeName == "DIV") {
                    if (node.className == "image") {
                        imageCount++;
                    } else {
                        hasOtherElements = true;
                    }
                } else if (node.nodeName == "P") {
                    if (isEmptyParagraph(node)) {
                        emptyPCount++;
                    } else {
                        hasText = true;
                    }
                } else {
                    hasOtherElements = true;
                }
            }
        }
        
        // Проверяем критерии кандидата
        if (!hasTitle && !hasText && !hasOtherElements && imageCount > 0) {
            sectionInfo.isCandidate = true;
            sectionInfo.imageCount = imageCount;
            sectionInfo.emptyPCount = emptyPCount;
            
            // Находим вышестоящую секцию (если есть)
            if (i > 0) {
                sectionInfo.aboveSection = allSections[i-1];
                candidates.push(sectionInfo);
            }
        }
    }
    
    // 5. В тихом режиме сразу выполняем без вопросов
    if (showStatistics == 0) {
        if (candidates.length == 0) {
            // В тихом режиме просто выходим без сообщений
            return;
        }
        
        // Запускаем таймер
        var startTime = new Date();
        
        // Выполняем объединение
        window.external.BeginUndoUnit(document, scriptName);
        var mergedCount = performMerging(candidates, nbspEntity, isEmptyParagraph);
        window.external.EndUndoUnit(document);
        
        // В тихом режиме не показываем статистику
        return;
    }
    
    // 6. Обычный режим: показываем анализ и спрашиваем подтверждение
    var analysisText = scriptName + "\nver. " + version + "\n\n";
    analysisText += "Всего секций в основном body: " + initialSectionCount + "\n";
    analysisText += "Подходящих случаев для объединения секций: " + candidates.length + "\n\n";
    
    if (candidates.length == 0) {
        MsgBox(analysisText + "Подходящих секций для объединения не найдено.");
        return;
    }
    
    // Показываем первые 15 примеров
    analysisText += "Будут объединены (первые 15 примеров):\n";
    analysisText += "------------------------------------------\n";
    
    var limit = Math.min(15, candidates.length);
    for (var i = 0; i < limit; i++) {
        var cand = candidates[i];
        analysisText += "Секция #" + (cand.index+1) + 
            " (картинок: " + cand.imageCount + 
            ", пустых строк: " + cand.emptyPCount + 
            ") → с секцией #" + (cand.aboveSection.index+1) + "\n";
    }
    
    if (candidates.length > 15) {
        analysisText += "... и еще " + (candidates.length - 15) + " случаев\n";
    }
    
    analysisText += "\nОбъединить?";
    
    // Включаем таймер после подтверждения
    var userConfirmed = AskYesNo(analysisText);
    if (!userConfirmed) {
        return;
    }
    
    var startTime = new Date();
    
    // 7. Выполняем объединение
    window.external.BeginUndoUnit(document, scriptName);
    var mergedCount = performMerging(candidates, nbspEntity, isEmptyParagraph);
    window.external.EndUndoUnit(document);
    
    // 8. Считаем оставшиеся секции
    var finalSectionCount = 0;
    childNodes = mainBody.childNodes;
    for (var i = 0; i < childNodes.length; i++) {
        var node = childNodes[i];
        if (node.nodeType == 1 && node.nodeName == "DIV" && node.className == "section") {
            finalSectionCount++;
        }
    }
    
    // 9. Выводим статистику
    var endTime = new Date();
    var executionTime = (endTime - startTime) / 1000;
    
    var statsText = scriptName + "\nver. " + version + "\n\n";
    statsText += "ОБЪЕДИНЕНИЕ ЗАВЕРШЕНО\n\n";
    statsText += "Секций в основном body БЫЛО: " + initialSectionCount + "\n";
    statsText += "Объединено секций: " + mergedCount + "\n";
    statsText += "Секций в основном body СТАЛО: " + finalSectionCount + "\n\n";
    statsText += "Время выполнения: " + executionTime.toFixed(2) + " сек\n\n";
    
    if (mergedCount > 0) {
        statsText += "✓ Секции успешно объединены\n";
        statsText += "✓ Изменения можно отменить (Ctrl+Z)\n";
    }
    
    MsgBox(statsText);
}

// Функция для выполнения объединения
function performMerging(candidates, nbspEntity, isEmptyParagraph) {
    var mergedCount = 0;
    
    // Обрабатываем в обратном порядке, чтобы индексы не сбивались
    for (var i = candidates.length - 1; i >= 0; i--) {
        var cand = candidates[i];
        var candidateSection = cand.element;
        var aboveSection = cand.aboveSection.element;
        
        // 1. Проверяем, есть ли в конце вышестоящей секции пустая строка
        var hasEmptyLineAtEnd = false;
        var aboveChildren = aboveSection.childNodes;
        if (aboveChildren.length > 0) {
            var lastChild = aboveChildren[aboveChildren.length - 1];
            if (lastChild.nodeName == "P" && isEmptyParagraph(lastChild)) {
                hasEmptyLineAtEnd = true;
            }
        }
        
        // 2. Перемещаем элементы из кандидат-секции в вышестоящую
        var candChildren = candidateSection.childNodes;
        var elementsToMove = [];
        
        // Собираем элементы для перемещения (в правильном порядке)
        for (var j = 0; j < candChildren.length; j++) {
            var child = candChildren[j];
            if (child.nodeType == 1) {
                elementsToMove.push(child);
            }
        }
        
        // 3. Если в конце вышестоящей секции нет пустой строки, добавляем её
        if (!hasEmptyLineAtEnd && elementsToMove.length > 0) {
            var emptyP = document.createElement("P");
            emptyP.innerHTML = nbspEntity;
            aboveSection.appendChild(emptyP);
        }
        
        // 4. Перемещаем все элементы из кандидат-секции
        for (var j = 0; j < elementsToMove.length; j++) {
            var element = elementsToMove[j];
            aboveSection.appendChild(element);
        }
        
        // 5. Очищаем пустые строки после последней картинки в объединенной секции
        cleanEmptyLinesAfterLastImage(aboveSection, isEmptyParagraph);
        
        // 6. Удаляем пустую кандидат-секцию
        candidateSection.parentNode.removeChild(candidateSection);
        
        mergedCount++;
    }
    
    return mergedCount;
}

// Функция для очистки пустых строк после последней картинки
function cleanEmptyLinesAfterLastImage(section, isEmptyParagraph) {
    if (!section) return;
    
    // Находим индекс последней картинки
    var lastImageIndex = -1;
    var children = section.childNodes;
    
    for (var i = 0; i < children.length; i++) {
        var child = children[i];
        if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "image") {
            lastImageIndex = i;
        }
    }
    
    // Если нашли картинку
    if (lastImageIndex >= 0) {
        // Удаляем все пустые строки после последней картинки до следующего непустого элемента
        var i = lastImageIndex + 1;
        while (i < children.length) {
            var child = children[i];
            if (child.nodeType == 1 && child.nodeName == "P" && isEmptyParagraph(child)) {
                // Удаляем пустую строку
                section.removeChild(child);
                // Не увеличиваем i, так как массив children изменился
            } else {
                // Нашли непустой элемент - останавливаемся
                break;
            }
        }
    }
}
