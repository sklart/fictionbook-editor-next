// Скрипт "Исправить нумерацию сносок (примечаний)" для редактора FBE
// version 1.4
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для проверки и исправления нумерации примечаний (сносок)
// в fb2 документах после перемещения секций.
// Работает со сносками вида: <a class="note" href="file:///.../main.html#n_12">[12]</a>
// Скрипт исправляет некоторые неприятные ошибки нумерации сносок (примечаний),
// которые не исправляют другие имеющиеся скрипты.
// Поддержка отмены действий (Ctrl+Z).

// version 1.4, 28.01.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Исправить нумерацию сносок (примечаний)";
    var version = "1.4";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Режим работы: 1 - автоматически, 0 - с запросом пользователя
    var autoFix = 0; // По умолчанию: 0 - с запросом
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    try {
        // Начинаем блок отмены действий
        window.external.BeginUndoUnit(document, scriptName);
        
        // Получаем неразрывный пробел из настроек FBE
        var nbspChar, nbspEntity;
        try {
            nbspChar = window.external.GetNBSP();
            if (nbspChar.charCodeAt(0) == 160) nbspEntity = "&nbsp;";
            else nbspEntity = nbspChar;
        } catch(e) {
            nbspChar = String.fromCharCode(160);
            nbspEntity = "&nbsp;";
        }
        
        // 1. НАХОДИМ ВСЕ ССЫЛКИ НА ПРИМЕЧАНИЯ В ТЕКСТЕ
        var noteLinks = findAllNoteLinks();
        var totalLinks = noteLinks.length;
        
        // 2. НАХОДИМ ВСЕ РАЗДЕЛЫ ПРИМЕЧАНИЙ
        var noteSections = findAllNoteSections();
        var totalSections = noteSections.length;
        
        if (totalLinks == 0 && totalSections == 0) {
            MsgBox(scriptName + "\nver. " + version + 
                  "\n\nВ документе не найдены сноски (примечания).");
            window.external.EndUndoUnit(document);
            return;
        }
        
        if (totalLinks == 0 && totalSections > 0) {
            MsgBox(scriptName + "\nver. " + version + 
                  "\n\nНайдены разделы примечаний (" + totalSections + "), но нет ссылок на них в тексте.\n\n" +
                  "Возможно, скрипт не распознал формат ссылок.");
            window.external.EndUndoUnit(document);
            return;
        }
        
        if (totalLinks > 0 && totalSections == 0) {
            MsgBox(scriptName + "\nver. " + version + 
                  "\n\nНайдены ссылки на примечания (" + totalLinks + "), но нет разделов примечаний.");
            window.external.EndUndoUnit(document);
            return;
        }
        
        // 3. АНАЛИЗИРУЕМ СООТВЕТСТВИЕ
        var analysis = analyzeNotes(noteLinks, noteSections);
        
        if (analysis.totalMismatches == 0 && analysis.missingNotes == 0 && analysis.orderCorrect) {
            MsgBox(scriptName + "\nver. " + version + 
                  "\n\n✓ Нумерация примечаний в порядке!\n" +
                  "Всего ссылок на раздел примечаний в тексте: " + totalLinks + "\n" +
                  "Всего примечаний: " + totalSections);
            window.external.EndUndoUnit(document);
            return;
        }
        
        // 4. ПОКАЗЫВАЕМ СТАТИСТИКУ
        var statsMessage = scriptName + "\nver. " + version + "\n\n";
        statsMessage += "АНАЛИЗ СНОСОК (ПРИМЕЧАНИЙ):\n";
        statsMessage += "• Всего ссылок на раздел примечаний в тексте: " + totalLinks + "\n";
        statsMessage += "• Всего примечаний: " + totalSections + "\n";
        
        // Подсчитываем сколько примечаний с неверной нумерацией
        var wrongNumberedNotes = 0;
        for (var i = 0; i < noteLinks.length; i++) {
            if (noteLinks[i].noteNum != (i + 1)) {
                wrongNumberedNotes++;
            }
        }
        
        if (wrongNumberedNotes > 0) {
            statsMessage += "• Порядок нарушен: " + wrongNumberedNotes + " примечаний\n";
        }
        
        if (analysis.missingNotes > 0) {
            statsMessage += "• Пропущенных номеров: " + analysis.missingNotes + "\n";
        }
        
        if (analysis.duplicateNumbers.length > 0) {
            statsMessage += "• Дублирующихся номеров: " + analysis.duplicateNumbers.length + "\n";
        }
        
        if (analysis.mismatchExamples.length > 0) {
            statsMessage += "\nПРОБЛЕМЫ:\n";
            for (var i = 0; i < Math.min(5, analysis.mismatchExamples.length); i++) {
                statsMessage += "  " + analysis.mismatchExamples[i] + "\n";
            }
            if (analysis.mismatchExamples.length > 5) {
                statsMessage += "  ... и еще " + (analysis.mismatchExamples.length - 5) + "\n";
            }
        }
        
        // 5. ЗАПРАШИВАЕМ ПОДТВЕРЖДЕНИЕ (если не авторежим)
        var shouldFix = autoFix == 1;
        
        if (!shouldFix) {
            var confirmMessage = statsMessage + 
                "\nХотите исправить нумерацию сносок (примечаний)?";
            shouldFix = AskYesNo(confirmMessage);
        }
        
        if (!shouldFix) {
            window.external.EndUndoUnit(document);
            return;
        }
        
        // ТАЙМЕР ВКЛЮЧАЕМ ТОЛЬКО ПОСЛЕ ПОСЛЕДНЕГО CONFIRM!
        var startTime = new Date().getTime();
        
        // 6. ИСПРАВЛЯЕМ НУМЕРАЦИЮ
        var fixResults = fixNoteNumbering(noteLinks, noteSections, analysis);
        
        var endTime = new Date().getTime();
        var executionTime = ((endTime - startTime) / 1000).toFixed(3);
        
        // 7. РЕЗУЛЬТАТ
        var resultMessage = scriptName + "\nver. " + version + "\n\n";
        resultMessage += "✓ НУМЕРАЦИЯ СНОСОК ИСПРАВЛЕНА\n\n";
        resultMessage += "РЕЗУЛЬТАТЫ:\n";
        resultMessage += "• Обновлено ссылок на раздел примечаний в тексте: " + fixResults.fixedLinks + "\n";
        resultMessage += "• Переупорядочено примечаний: " + fixResults.fixedSections + "\n";
        resultMessage += "• Порядок примечаний исправлен согласно нумерации\n";
        resultMessage += "\nВремя выполнения: " + executionTime + " сек";
        
        MsgBox(resultMessage);
        
    } catch(e) {
        MsgBox(scriptName + "\nver. " + version + "\n\nПроизошла ошибка: " + e.message);
    }
    
    // Завершаем блок отмены действий
    window.external.EndUndoUnit(document);
    
    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================
    
    // Функция поиска всех ссылок на примечания в тексте
    function findAllNoteLinks() {
        var links = [];
        var allLinks = document.getElementsByTagName("A");
        
        for (var i = 0; i < allLinks.length; i++) {
            var link = allLinks[i];
            var href = link.getAttribute("href") || "";
            
            // Проверяем, является ли это ссылкой на примечание
            // Варианты: #n_13 или file:///.../main.html#n_13
            var noteMatch = href.match(/#n_(\d+)$/);
            if (noteMatch) {
                var noteNum = parseInt(noteMatch[1], 10);
                
                // Получаем текст ссылки (обычно [13])
                var linkText = link.innerHTML || "";
                var textMatch = linkText.match(/\[(\d+)\]/);
                var displayedNum = textMatch ? parseInt(textMatch[1], 10) : noteNum;
                
                // Также проверяем класс "note" для дополнительной уверенности
                var isNoteClass = (link.className == "note");
                
                links.push({
                    element: link,
                    href: href,
                    noteNum: noteNum,
                    displayedNum: displayedNum,
                    linkText: linkText,
                    isNoteClass: isNoteClass,
                    // Для определения порядка в DOM
                    domPosition: getElementPosition(link)
                });
            }
        }
        
        // СОРТИРУЕМ ссылки по их позиции в документе (порядок следования в тексте)
        links.sort(function(a, b) {
            return a.domPosition - b.domPosition;
        });
        
        return links;
    }
    
    // Функция для определения позиции элемента в DOM
    function getElementPosition(element) {
        // Простой метод: считаем все предыдущие элементы
        var pos = 0;
        var prev = element;
        
        while (prev = prev.previousSibling) {
            pos++;
        }
        
        return pos;
    }
    
    // Функция поиска всех разделов примечаний
    function findAllNoteSections() {
        var sections = [];
        var allBodies = document.getElementsByTagName("DIV");
        
        for (var i = 0; i < allBodies.length; i++) {
            var body = allBodies[i];
            if (body.className == "body") {
                var fbname = body.getAttribute("fbname") || "";
                if (fbname == "notes") {
                    // Нашли раздел примечаний, ищем все секции внутри
                    var noteSectionsInBody = body.getElementsByTagName("DIV");
                    for (var j = 0; j < noteSectionsInBody.length; j++) {
                        var section = noteSectionsInBody[j];
                        if (section.className == "section") {
                            // Извлекаем ID секции (вида n_13)
                            var id = section.getAttribute("id") || "";
                            var match = id.match(/^n_(\d+)$/);
                            if (match) {
                                var noteNum = parseInt(match[1], 10);
                                
                                // Получаем номер из заголовка
                                var titleNum = noteNum;
                                var titleDiv = getFirstChildByClass(section, "title");
                                if (titleDiv) {
                                    var titleP = getFirstChildByTag(titleDiv, "P");
                                    if (titleP) {
                                        var titleText = titleP.innerHTML || "";
                                        var titleMatch = titleText.match(/(\d+)/);
                                        if (titleMatch) {
                                            titleNum = parseInt(titleMatch[1], 10);
                                        }
                                    }
                                }
                                
                                sections.push({
                                    element: section,
                                    id: id,
                                    noteNum: noteNum,
                                    titleNum: titleNum,
                                    parentBody: body,
                                    // Для сортировки по текущему порядку
                                    currentPosition: j
                                });
                            }
                        }
                    }
                    break; // Предполагаем один раздел notes
                }
            }
        }
        
        // СОРТИРУЕМ примечания по текущему порядку в разделе notes
        sections.sort(function(a, b) {
            return a.currentPosition - b.currentPosition;
        });
        
        return sections;
    }
    
    // Вспомогательная функция: найти первый дочерный элемент по классу
    function getFirstChildByClass(element, className) {
        for (var i = 0; i < element.childNodes.length; i++) {
            var child = element.childNodes[i];
            if (child.nodeType == 1 && child.className == className) {
                return child;
            }
        }
        return null;
    }
    
    // Вспомогательная функция: найти первый дочерный элемент по тегу
    function getFirstChildByTag(element, tagName) {
        for (var i = 0; i < element.childNodes.length; i++) {
            var child = element.childNodes[i];
            if (child.nodeType == 1 && child.nodeName == tagName) {
                return child;
            }
        }
        return null;
    }
    
    // Функция анализа соответствия ссылок и примечаний
    function analyzeNotes(links, sections) {
        var result = {
            totalMismatches: 0,
            missingNotes: 0,
            duplicateNumbers: [],
            mismatchExamples: [],
            orderCorrect: true,
            linkOrder: [],
            sectionOrder: []
        };
        
        if (links.length == 0 || sections.length == 0) {
            return result;
        }
        
        // 1. Проверяем уникальность номеров в ссылках
        var linkNumbers = {};
        for (var i = 0; i < links.length; i++) {
            var num = links[i].noteNum;
            if (linkNumbers[num]) {
                if (result.duplicateNumbers.indexOf(num) == -1) {
                    result.duplicateNumbers.push(num);
                }
            }
            linkNumbers[num] = true;
            result.linkOrder.push(num);
        }
        
        // 2. Проверяем уникальность номеров в примечаниях
        var sectionNumbers = {};
        for (var i = 0; i < sections.length; i++) {
            var num = sections[i].noteNum;
            if (sectionNumbers[num]) {
                if (result.duplicateNumbers.indexOf(num) == -1) {
                    result.duplicateNumbers.push(num);
                }
            }
            sectionNumbers[num] = true;
            result.sectionOrder.push(num);
        }
        
        // 3. Проверяем, все ли ссылки ведут на существующие примечания
        for (var i = 0; i < links.length; i++) {
            var link = links[i];
            var found = false;
            
            for (var j = 0; j < sections.length; j++) {
                if (sections[j].noteNum == link.noteNum) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                result.missingNotes++;
                if (result.mismatchExamples.length < 10) {
                    result.mismatchExamples.push("Ссылка ведет на несуществующее примечание #" + link.noteNum);
                }
            }
        }
        
        // 4. Проверяем, все ли примечания имеют ссылки
        for (var i = 0; i < sections.length; i++) {
            var section = sections[i];
            var hasLink = false;
            
            for (var j = 0; j < links.length; j++) {
                if (links[j].noteNum == section.noteNum) {
                    hasLink = true;
                    break;
                }
            }
            
            if (!hasLink) {
                result.missingNotes++;
                if (result.mismatchExamples.length < 10) {
                    result.mismatchExamples.push("Примечание #" + section.noteNum + " не имеет ссылок в тексте");
                }
            }
        }
        
        // 5. Проверяем порядок: должны быть последовательные номера от 1 до N
        // по порядку следования ссылок в тексте
        result.orderCorrect = true;
        
        // Сначала проверяем, есть ли все номера от 1 до links.length
        var hasAllNumbers = true;
        for (var i = 1; i <= links.length; i++) {
            var foundNum = false;
            for (var j = 0; j < links.length; j++) {
                if (links[j].noteNum == i) {
                    foundNum = true;
                    break;
                }
            }
            if (!foundNum) {
                hasAllNumbers = false;
                result.totalMismatches++;
                if (result.mismatchExamples.length < 10) {
                    result.mismatchExamples.push("Отсутствует номер " + i + " в последовательности");
                }
            }
        }
        
        // Проверяем порядок следования ссылок в тексте
        for (var i = 0; i < links.length; i++) {
            var expectedNum = i + 1;
            if (links[i].noteNum != expectedNum) {
                result.orderCorrect = false;
                result.totalMismatches++;
                if (result.mismatchExamples.length < 10) {
                    result.mismatchExamples.push("Ссылка в позиции " + (i + 1) + " имеет номер " + links[i].noteNum + " (должен быть " + expectedNum + ")");
                }
            }
        }
        
        return result;
    }
    
    // Функция исправления нумерации
    function fixNoteNumbering(links, sections, analysis) {
        var result = {
            fixedLinks: 0,
            fixedSections: 0
        };
        
        // Шаг 1: Создаем карту соответствия старых номеров новым
        var oldToNewMap = {};
        var newToOldMap = {};
        
        for (var i = 0; i < links.length; i++) {
            var oldNum = links[i].noteNum;
            var newNum = i + 1;
            oldToNewMap[oldNum] = newNum;
            newToOldMap[newNum] = oldNum;
            links[i].newNum = newNum;
        }
        
        // Шаг 2: Обновляем ССЫЛКИ в тексте
        for (var i = 0; i < links.length; i++) {
            var link = links[i];
            var newNum = link.newNum;
            var oldHref = link.href;
            
            // Обновляем href - сохраняем полный путь, меняем только якорь
            var newHref = oldHref.replace(/#n_\d+$/, "#n_" + newNum);
            link.element.setAttribute("href", newHref);
            
            // Обновляем текст ссылки [13] → [новый_номер]
            var oldText = link.linkText;
            var newText = oldText.replace(/\[\d+\]/, "[" + newNum + "]");
            link.element.innerHTML = newText;
            
            result.fixedLinks++;
        }
        
        // Шаг 3: Обновляем РАЗДЕЛЫ ПРИМЕЧАНИЙ
        // Находим родительский body с notes
        var notesBody = null;
        if (sections.length > 0) {
            notesBody = sections[0].parentBody;
        }
        
        if (!notesBody) {
            // Если не нашли, ищем вручную
            var allBodies = document.getElementsByTagName("DIV");
            for (var i = 0; i < allBodies.length; i++) {
                var body = allBodies[i];
                if (body.className == "body") {
                    var fbname = body.getAttribute("fbname") || "";
                    if (fbname == "notes") {
                        notesBody = body;
                        break;
                    }
                }
            }
        }
        
        if (!notesBody) {
            MsgBox("Не найден раздел примечаний (notes).");
            return result;
        }
        
        // Шаг 4: Создаем карту примечаний по старым номерам
        var sectionMap = {};
        for (var i = 0; i < sections.length; i++) {
            sectionMap[sections[i].noteNum] = sections[i];
        }
        
        // Шаг 5: Удаляем все существующие примечания из notesBody
        // Но сначала сохраняем их клоны
        var sectionClones = [];
        for (var newNum = 1; newNum <= links.length; newNum++) {
            var oldNum = newToOldMap[newNum];
            if (oldNum && sectionMap[oldNum]) {
                sectionClones.push({
                    newNum: newNum,
                    oldNum: oldNum,
                    element: sectionMap[oldNum].element.cloneNode(true)
                });
            }
        }
        
        // Очищаем notesBody от старых секций
        var child = notesBody.firstChild;
        while (child) {
            var nextChild = child.nextSibling;
            if (child.nodeType == 1 && child.className == "section") {
                child.removeNode(true);
            }
            child = nextChild;
        }
        
        // Шаг 6: Вставляем примечания в правильном порядке
        for (var i = 0; i < sectionClones.length; i++) {
            var clone = sectionClones[i];
            var sectionElement = clone.element;
            var newNum = clone.newNum;
            
            // Обновляем ID секции
            sectionElement.setAttribute("id", "n_" + newNum);
            
            // Обновляем номер в заголовке
            var titleDiv = getFirstChildByClass(sectionElement, "title");
            if (titleDiv) {
                var titleP = getFirstChildByTag(titleDiv, "P");
                if (titleP) {
                    var oldTitle = titleP.innerHTML || "";
                    var newTitle = oldTitle.replace(/\d+/, newNum);
                    titleP.innerHTML = newTitle;
                }
            }
            
            // Вставляем в notesBody
            notesBody.appendChild(sectionElement);
            
            result.fixedSections++;
        }
        
        return result;
    }
}
