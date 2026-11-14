// Скрипт "Переместить текущую секцию вниз с перенумерацией сносок" для редактора FBE
// version 1.8
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для перемещения текущей секции fb2 документа вниз (ниже по тексту).
// Скрипт может перемещать как родительские секции, так и вложенные - внутри своих родительских.
// Родительские секции перемещаются целиком - включая все вложенные секции.
// По умолчанию обрабатывается только основной раздел документа.
// Для удобства выделения для перемещения всей родительской секции
// можно просто дважды быстро щелкнуть по ее заголовку слева на панели структуры документа.
// После перемещения секции скрипт может автоматически исправить нумерацию сносок (примечаний).
// При перемещении секций может выполняться перенумерация сносок - по запросу или автоматически.
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.8, 28.01.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Переместить текущую секцию вниз с перенумерацией сносок";
    var version = "1.8";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Показывать сообщения (1 - Да, 0 - Нет (тихий режим))
    var showMessages = 1; // По умолчанию: 1 - показывать сообщения
    
    // Обрабатывать раздел сносок (примечаний) (0 - Нет, 1 - Да)
    var processNotes = 1; // По умолчанию: 1 - обрабатывать
    
    // Обрабатывать раздел комментариев (0 - Нет, 1 - Да)
    var processComments = 0; // По умолчанию: 0 - не обрабатывать
    
    // Исправлять нумерацию сносок после перемещения:
    // 0 - не исправлять, 1 - автоматически, 2 - спрашивать
    var fixNotesAfterMove = 2; // По умолчанию: 2 - спрашивать
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    try {
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
        
        // Начинаем блок отмены действий
        window.external.BeginUndoUnit(document, scriptName);
        
        // Получаем текущую позицию курсора
        var range = document.selection.createRange();
        if (!range) {
            if (showMessages) {
                MsgBox(scriptName + "\nver. " + version + "\n\nНе удалось определить текущую позицию курсора.");
            }
            window.external.EndUndoUnit(document);
            return;
        }
        
        var parentElement = range.parentElement();
        if (!parentElement) {
            if (showMessages) {
                MsgBox(scriptName + "\nver. " + version + "\n\nНе удалось определить текущий элемент.");
            }
            window.external.EndUndoUnit(document);
            return;
        }
        
        // Ищем текущую секцию (ищем DIV с классом "section")
        var currentSection = findCurrentSection(parentElement);
        
        // Проверяем, нашли ли мы секцию
        if (!currentSection) {
            if (showMessages) {
                MsgBox(scriptName + "\nver. " + version + "\n\nКурсор не находится внутри секции.\n\nУстановите курсор в секцию, которую хотите переместить вниз.");
            }
            window.external.EndUndoUnit(document);
            return;
        }
        
        // Проверяем, можно ли обрабатывать этот раздел body
        if (!canProcessBodySection(currentSection)) {
            if (showMessages) {
                MsgBox(scriptName + "\nver. " + version + "\n\nСекция находится в разделе, который не обрабатывается согласно настройкам.\n\nИзмените настройки скрипта для обработки разделов сносок или комментариев.");
            }
            window.external.EndUndoUnit(document);
            return;
        }
        
        // Определяем характеристики секции
        var parentContainer = getParentContainer(currentSection);
        var nestingLevel = getNestingLevel(currentSection);
        var isRootSection = (nestingLevel == 1);
        var isParentSection = isParent(currentSection);
        
        // Находим следующую секцию того же уровня для ТЕКУЩЕЙ секции
        var nextSection = findNextSection(currentSection, parentContainer);
        
        // Диалоги с пользователем (только в обычном режиме)
        var moveOperation = null; // null - отмена, "nested" - вложенная, "parent" - родительская, "root" - корневая
        var targetSection = currentSection; // Какую секцию будем перемещать
        var targetParent = parentContainer; // Родительский контейнер для перемещения
        var targetNextSection = nextSection; // Следующая секция для перемещения
        
        // ЗАПОМИНАЕМ ПОРЯДОК СЕКЦИЙ ДО ПЕРЕМЕЩЕНИЯ (для анализа сносок)
        var originalSectionsOrder = [];
        if (isRootSection && targetParent.className == "body") {
            // Для корневых секций запоминаем порядок в body
            for (var i = 0; i < targetParent.childNodes.length; i++) {
                var child = targetParent.childNodes[i];
                if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "section") {
                    originalSectionsOrder.push(child);
                }
            }
        }
        
        if (showMessages) {
            if (isRootSection) {
                // Корневая секция (первого уровня)
                if (!targetNextSection) {
                    var message = "Секция ";
                    if (isParentSection) message += "(родительская) ";
                    message += "уже последняя и не может быть перемещена вниз.";
                    MsgBox(scriptName + "\nver. " + version + "\n\n" + message);
                    window.external.EndUndoUnit(document);
                    return;
                }
                
                var confirmMessage = "Вы находитесь в секции первого уровня";
                if (isParentSection) confirmMessage += " (родительской, содержит вложенные секции)";
                confirmMessage += ".\n\nХотите перенести эту секцию вниз?";
                
                if (AskYesNo(scriptName + "\nver. " + version + "\n\n" + confirmMessage)) {
                    moveOperation = "root";
                }
            } else {
                // Вложенная секция (уровень 2 и выше)
                if (!targetNextSection) {
                    // Вложенная секция последняя, проверяем родительскую
                    var parentNextSection = findNextSection(parentContainer, getParentContainer(parentContainer));
                    
                    if (!parentNextSection) {
                        MsgBox(scriptName + "\nver. " + version + "\n\nВложенная секция последняя. Родительская секция также последняя.\n\nСекции не могут быть перемещены вниз.");
                        window.external.EndUndoUnit(document);
                        return;
                    }
                    
                    // Предлагаем только родительскую
                    var parentConfirm = "Вложенная секция последняя в родительской.\n\nХотите переместить ВСЮ РОДИТЕЛЬСКУЮ секцию?\n(вместе со всеми вложенными секциями)";
                    
                    if (AskYesNo(scriptName + "\nver. " + version + "\n\n" + parentConfirm)) {
                        moveOperation = "parent";
                        targetSection = parentContainer;
                        targetParent = getParentContainer(parentContainer);
                        targetNextSection = parentNextSection;
                    }
                } else {
                    // Есть куда перемещать вложенную секцию - даем выбор
                    var choiceMessage = "Вы находитесь во вложенной секции (уровень " + nestingLevel + ")";
                    if (isParentSection) choiceMessage += ", которая содержит вложенные секции";
                    choiceMessage += ".\n\nЧто вы хотите сделать?\n\nНажмите 'Да' - чтобы переместить только ВЛОЖЕННУЮ секцию\nНажмите 'Нет' - чтобы перейти к выбору перемещения РОДИТЕЛЬСКОЙ секции";
                    
                    if (AskYesNo(scriptName + "\nver. " + version + "\n\n" + choiceMessage)) {
                        moveOperation = "nested";
                    } else {
                        // Проверяем, можно ли переместить родительскую
                        var parentNextSection = findNextSection(parentContainer, getParentContainer(parentContainer));
                        
                        if (!parentNextSection) {
                            MsgBox(scriptName + "\nver. " + version + "\n\nРодительская секция уже последняя и не может быть перемещена вниз.");
                            window.external.EndUndoUnit(document);
                            return;
                        }
                        
                        // Подтверждение перемещения родительской секции
                        var parentConfirm = "Хотите переместить ВСЮ РОДИТЕЛЬСКУЮ секцию?\n(вместе со всеми вложенными секциями)";
                        
                        if (AskYesNo(scriptName + "\nver. " + version + "\n\n" + parentConfirm)) {
                            moveOperation = "parent";
                            targetSection = parentContainer;
                            targetParent = getParentContainer(parentContainer);
                            targetNextSection = parentNextSection;
                        }
                    }
                }
            }
            
            // Если операция отменена пользователем
            if (moveOperation === null) {
                window.external.EndUndoUnit(document);
                return;
            }
        } else {
            // Тихий режим - просто проверяем возможность перемещения
            if (!targetNextSection) {
                if (nestingLevel > 1) {
                    // Во вложенной секции - проверяем родительскую
                    var parentNextSection = findNextSection(parentContainer, getParentContainer(parentContainer));
                    if (!parentNextSection) {
                        window.external.EndUndoUnit(document);
                        return; // Ничего не делаем в тихом режиме
                    }
                    // Перемещаем родительскую
                    moveOperation = "parent";
                    targetSection = parentContainer;
                    targetParent = getParentContainer(parentContainer);
                    targetNextSection = parentNextSection;
                } else {
                    window.external.EndUndoUnit(document);
                    return; // Ничего не делаем в тихом режиме
                }
            } else {
                moveOperation = isRootSection ? "root" : "nested";
            }
        }
        
        // ТАЙМЕР ВКЛЮЧАЕМ ТОЛЬКО ПОСЛЕ ВСЕХ ДИАЛОГОВ ПЕРЕДВИЖЕНИЯ!
        var moveStartTime = new Date().getTime();
        
        // Перемещаем секцию
        var moved = moveSectionDown(targetSection, targetNextSection, targetParent);
        
        if (!moved) {
            if (showMessages) {
                MsgBox(scriptName + "\nver. " + version + "\n\nНе удалось переместить секцию.");
            }
            window.external.EndUndoUnit(document);
            return;
        }
        
        // Прокручиваем к новой позиции секции
        scrollToSection(targetSection);
        
        var moveEndTime = new Date().getTime();
        var moveTime = ((moveEndTime - moveStartTime) / 1000).toFixed(3);
        
        // ==================================================
        // ИСПРАВЛЕНИЕ НУМЕРАЦИИ СНОСОК (ПРИМЕЧАНИЙ) - ЕСЛИ НУЖНО
        // ==================================================
        
        var fixNotesResult = null;
        var shouldFixNotes = false;
        var notesAnalysis = null;
        
        // Проверяем, есть ли вообще сноски в документе
        var hasNotes = checkIfDocumentHasNotes();
        
        if (hasNotes && fixNotesAfterMove > 0 && originalSectionsOrder.length > 0) {
            // Анализируем, нуждаются ли сноски в исправлении после перемещения
            notesAnalysis = analyzeNotesAfterSectionMove(targetSection, originalSectionsOrder);
            
            if (notesAnalysis.requiresFix) {
                // Проверяем, нужно ли исправлять нумерацию
                if (fixNotesAfterMove == 1) {
                    // Автоматически исправлять
                    shouldFixNotes = true;
                } else if (fixNotesAfterMove == 2 && showMessages) {
                    // Спрашивать пользователя
                    var fixConfirm = scriptName + "\nver. " + version + 
                        "\n\nСекция успешно перемещена за " + moveTime + " сек.\n\n" +
                        "АНАЛИЗ СНОСОК:\n" +
                        "• Всего сносок в документе: " + notesAnalysis.totalNotes + "\n" +
                        "• Сносок в перемещаемой секции: " + notesAnalysis.notesInMovedSection + "\n" +
                        "• Сносок в секциях после перемещаемой (в исходном порядке): " + notesAnalysis.notesAfterMovedSection + "\n" +
                        "• Всего сносок, требующих исправления: " + notesAnalysis.notesNeedFix + "\n\n" +
                        "Хотите исправить их нумерацию?";
                    
                    shouldFixNotes = AskYesNo(fixConfirm);
                }
                
                if (shouldFixNotes) {
                    var fixStartTime = new Date().getTime();
                    fixNotesResult = fixNoteNumberingIntelligently(notesAnalysis);
                    var fixEndTime = new Date().getTime();
                    var fixTime = ((fixEndTime - fixStartTime) / 1000).toFixed(3);
                }
            } else if (showMessages && fixNotesAfterMove == 2) {
                // Сообщаем, что исправление не требуется
                var noFixNeeded = scriptName + "\nver. " + version + 
                    "\n\nСекция успешно перемещена за " + moveTime + " сек.\n\n" +
                    "✓ Нумерация сносок не нарушена.\n" +
                    "Исправление не требуется.";
                MsgBox(noFixNeeded);
                window.external.EndUndoUnit(document);
                return;
            }
        }
        
        // Выводим сообщение об успехе (если включены сообщения)
        if (showMessages) {
            var message = "✓ ";
            
            if (moveOperation == "nested") {
                message += "Вложенная секция перенесена успешно!";
            } else if (moveOperation == "parent") {
                message += "Родительская секция перенесена успешно!";
            } else {
                if (isParent(targetSection)) {
                    message += "Секция (родительская, содержит вложенные) перенесена успешно!";
                } else {
                    message += "Секция перенесена успешно!";
                }
            }
            
            message += "\n\nВремя перемещения: " + moveTime + " сек";
            
            if (shouldFixNotes && fixNotesResult) {
                message += "\n\n✓ НУМЕРАЦИЯ СНОСОК ИСПРАВЛЕНА";
                message += "\n• Обновлено ссылок: " + fixNotesResult.fixedLinks;
                message += "\n• Переупорядочено примечаний: " + fixNotesResult.fixedSections;
                message += "\n• Время исправления нумерации: " + fixTime + " сек";
            } else if (hasNotes && fixNotesAfterMove == 0) {
                message += "\n\nПримечание: Исправление нумерации сносок отключено в настройках.";
            } else if (hasNotes && notesAnalysis && notesAnalysis.requiresFix && !shouldFixNotes) {
                message += "\n\nПримечание: Нумерация сносок не была исправлена.";
            }
            
            MsgBox(scriptName + "\nver. " + version + "\n\n" + message);
        }
        
    } catch(e) {
        if (showMessages) {
            MsgBox(scriptName + "\nver. " + version + "\n\nПроизошла ошибка: " + e.message);
        }
    }
    
    // Завершаем блок отмены действий
    window.external.EndUndoUnit(document);
    
    // ==================================================
    // ОСНОВНЫЕ ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ (ПЕРЕМЕЩЕНИЕ)
    // ==================================================
    
    // Функция поиска текущей секции
    function findCurrentSection(element) {
        var current = element;
        
        // Ищем вверх по иерархии DIV с классом "section"
        while (current && current.nodeName != "BODY") {
            if (current.nodeName == "DIV" && current.className == "section") {
                return current;
            }
            current = current.parentElement;
        }
        
        return null;
    }
    
    // Функция получения родительского контейнеров
    function getParentContainer(section) {
        var parent = section.parentNode;
        
        // Ищем ближайший DIV контейнер (body или другая секция)
        while (parent && parent.nodeName != "BODY") {
            if (parent.nodeName == "DIV" && 
                (parent.className == "body" || parent.className == "section")) {
                return parent;
            }
            parent = parent.parentNode;
        }
        
        // Если не нашли, возвращаем непосредственного родителя
        return section.parentNode;
    }
    
    // Функция определения уровня вложенности
    function getNestingLevel(section) {
        var level = 0;
        var current = section;
        
        while (current && current.nodeName != "BODY") {
            if (current.nodeName == "DIV" && current.className == "section") {
                level++;
            }
            current = current.parentElement;
        }
        
        return level;
    }
    
    // Функция проверки, является ли секция родительской (содержит вложенные секции)
    function isParent(section) {
        if (!section || section.nodeName != "DIV" || section.className != "section") {
            return false;
        }
        
        // Проверяем все дочерние элементы
        for (var i = 0; i < section.childNodes.length; i++) {
            var child = section.childNodes[i];
            if (child.nodeType == 1 && // ELEMENT_NODE
                child.nodeName == "DIV" && 
                child.className == "section") {
                return true; // Нашли вложенную секцию
            }
        }
        
        return false;
    }
    
    // Функция поиска следующей секции того же уровня
    function findNextSection(section, parentContainer) {
        var foundCurrent = false;
        
        // Перебираем все дочерние элементы родительского контейнера
        for (var i = 0; i < parentContainer.childNodes.length; i++) {
            var child = parentContainer.childNodes[i];
            
            if (child.nodeType == 1) { // ELEMENT_NODE
                if (child === section) {
                    foundCurrent = true;
                } else if (foundCurrent && child.nodeName == "DIV" && child.className == "section") {
                    return child; // Нашли следующую секцию
                }
            }
        }
        
        return null; // Следующая секция не найдена
    }
    
    // Функция проверки возможности обработки раздела body
    function canProcessBodySection(section) {
        // Ищем родительский body
        var parent = section;
        while (parent && parent.nodeName != "BODY") {
            if (parent.nodeName == "DIV" && parent.className == "body") {
                var fbname = parent.getAttribute("fbname") || "";
                
                if (fbname == "notes" && processNotes == 0) {
                    return false; // Раздел сносок не обрабатывается
                }
                if (fbname == "comments" && processComments == 0) {
                    return false; // Раздел комментариев не обрабатывается
                }
                return true; // Основной body или разрешенный раздел
            }
            parent = parent.parentElement;
        }
        
        return true; // Если body не найден, разрешаем обработку
    }
    
    // Функция перемещения секции вниз
    function moveSectionDown(section, nextSection, parentContainer) {
        try {
            // Вставляем следующую секцию перед текущей
            parentContainer.insertBefore(nextSection, section);
            return true;
        } catch(e) {
            return false;
        }
    }
    
    // Функция прокрутки к секции
    function scrollToSection(section) {
        try {
            // Создаем диапазон и выделяем начало секции
            var range = document.body.createTextRange();
            range.moveToElementText(section);
            range.collapse(true);
            range.select();
            
            // Прокручиваем к выделению
            range.scrollIntoView();
        } catch(e) {
            // Игнорируем ошибки прокрутки
        }
    }
    
    // ==================================================
    // ФУНКЦИИ ДЛЯ АНАЛИЗА И ИСПРАВЛЕНИЯ СНОСОК
    // ==================================================
    
    // Функция проверки, есть ли в документе сноски
    function checkIfDocumentHasNotes() {
        // Проверяем наличие ссылок на примечания
        var allLinks = document.getElementsByTagName("A");
        for (var i = 0; i < allLinks.length; i++) {
            var href = allLinks[i].getAttribute("href") || "";
            if (href.match(/#n_\d+$/)) {
                return true;
            }
        }
        
        return false;
    }
    
    // Основная функция анализа сносок после перемещения секции
    function analyzeNotesAfterSectionMove(movedSection, originalSectionsOrder) {
        var analysis = {
            requiresFix: false,
            totalNotes: 0,
            notesInMovedSection: 0,
            notesAfterMovedSection: 0,
            notesNeedFix: 0,
            sectionsWithNotes: [], // Массив объектов {section: ссылка, notes: [], isMoved: bool, originalIndex: number}
            movedSection: movedSection,
            originalOrder: originalSectionsOrder
        };
        
        try {
            // 1. Находим все сноски в документе и определяем их секции
            var allNoteLinks = [];
            var allLinks = document.getElementsByTagName("A");
            
            for (var i = 0; i < allLinks.length; i++) {
                var link = allLinks[i];
                var href = link.getAttribute("href") || "";
                var noteMatch = href.match(/#n_(\d+)$/);
                
                if (noteMatch) {
                    var noteNum = parseInt(noteMatch[1], 10);
                    var linkText = link.innerHTML || "";
                    
                    // Находим родительскую секцию для этой ссылки
                    var parentSection = findParentSectionForElement(link, originalSectionsOrder);
                    
                    allNoteLinks.push({
                        element: link,
                        href: href,
                        noteNum: noteNum,
                        linkText: linkText,
                        parentSection: parentSection
                    });
                }
            }
            
            analysis.totalNotes = allNoteLinks.length;
            if (analysis.totalNotes == 0) {
                return analysis;
            }
            
            // 2. Группируем сноски по секциям (в оригинальном порядке)
            for (var i = 0; i < originalSectionsOrder.length; i++) {
                var section = originalSectionsOrder[i];
                var sectionNotes = [];
                
                for (var j = 0; j < allNoteLinks.length; j++) {
                    if (allNoteLinks[j].parentSection === section) {
                        sectionNotes.push(allNoteLinks[j]);
                    }
                }
                
                if (sectionNotes.length > 0) {
                    analysis.sectionsWithNotes.push({
                        section: section,
                        notes: sectionNotes,
                        isMovedSection: (section === movedSection),
                        originalIndex: i
                    });
                }
            }
            
            // 3. Определяем, какие сноски нужно исправлять
            // Находим индекс перемещаемой секции в оригинальном порядке
            var movedSectionIndex = -1;
            for (var i = 0; i < analysis.sectionsWithNotes.length; i++) {
                if (analysis.sectionsWithNotes[i].isMovedSection) {
                    movedSectionIndex = analysis.sectionsWithNotes[i].originalIndex;
                    analysis.notesInMovedSection = analysis.sectionsWithNotes[i].notes.length;
                    analysis.notesNeedFix += analysis.sectionsWithNotes[i].notes.length;
                    break;
                }
            }
            
            // Добавляем сноски из секций, которые были ПОСЛЕ перемещаемой в оригинальном порядке
            if (movedSectionIndex >= 0) {
                for (var i = 0; i < analysis.sectionsWithNotes.length; i++) {
                    var sectionData = analysis.sectionsWithNotes[i];
                    if (!sectionData.isMovedSection && sectionData.originalIndex > movedSectionIndex) {
                        analysis.notesAfterMovedSection += sectionData.notes.length;
                        analysis.notesNeedFix += sectionData.notes.length;
                    }
                }
            }
            
            analysis.requiresFix = (analysis.notesNeedFix > 0);
            
        } catch(e) {
            // Игнорируем ошибки анализа
        }
        
        return analysis;
    }
    
    // Функция поиска родительской секции для элемента
    function findParentSectionForElement(element, sectionsList) {
        var parent = element.parentElement;
        
        while (parent && parent.nodeName != "BODY") {
            // Проверяем, является ли это одной из корневых секций
            for (var i = 0; i < sectionsList.length; i++) {
                if (parent === sectionsList[i]) {
                    return parent;
                }
            }
            parent = parent.parentElement;
        }
        
        return null;
    }
    
    // Умная функция исправления нумерации сносок
    function fixNoteNumberingIntelligently(analysis) {
        var result = {
            fixedLinks: 0,
            fixedSections: 0
        };
        
        try {
            if (!analysis.requiresFix || analysis.notesNeedFix == 0) {
                return result;
            }
            
            // 1. Находим все разделы примечаний
            var notesBody = null;
            var allNoteSections = [];
            var allBodies = document.getElementsByTagName("DIV");
            
            for (var i = 0; i < allBodies.length; i++) {
                var body = allBodies[i];
                if (body.className == "body") {
                    var fbname = body.getAttribute("fbname") || "";
                    if (fbname == "notes") {
                        notesBody = body;
                        // Находим все секции примечаний
                        var noteDivs = body.getElementsByTagName("DIV");
                        for (var j = 0; j < noteDivs.length; j++) {
                            if (noteDivs[j].className == "section") {
                                var id = noteDivs[j].getAttribute("id") || "";
                                var match = id.match(/^n_(\d+)$/);
                                if (match) {
                                    var noteNum = parseInt(match[1], 10);
                                    allNoteSections.push({
                                        element: noteDivs[j],
                                        id: id,
                                        noteNum: noteNum
                                    });
                                }
                            }
                        }
                        break;
                    }
                }
            }
            
            if (!notesBody || allNoteSections.length == 0) {
                return result;
            }
            
            // 2. Создаем карту примечаний по номерам
            var noteMap = {};
            for (var i = 0; i < allNoteSections.length; i++) {
                noteMap[allNoteSections[i].noteNum] = allNoteSections[i];
            }
            
            // 3. Определяем новый порядок нумерации
            // Проходим по секциям в ИСХОДНОМ порядке и назначаем новые номера
            var oldToNewMap = {};
            var currentNewNumber = 1;
            
            // Сначала идем по секциям ДО перемещенной (включая ее)
            var foundMovedSection = false;
            
            for (var i = 0; i < analysis.sectionsWithNotes.length; i++) {
                var sectionData = analysis.sectionsWithNotes[i];
                
                if (sectionData.originalIndex < analysis.movedSectionIndex) {
                    // Секции ДО перемещенной - сохраняем оригинальные номера их сносок
                    for (var j = 0; j < sectionData.notes.length; j++) {
                        var oldNum = sectionData.notes[j].noteNum;
                        oldToNewMap[oldNum] = currentNewNumber;
                        currentNewNumber++;
                    }
                }
            }
            
            // Теперь секции, которые были ПОСЛЕ перемещенной в оригинальном порядке
            for (var i = 0; i < analysis.sectionsWithNotes.length; i++) {
                var sectionData = analysis.sectionsWithNotes[i];
                
                if (sectionData.originalIndex > analysis.movedSectionIndex) {
                    for (var j = 0; j < sectionData.notes.length; j++) {
                        var oldNum = sectionData.notes[j].noteNum;
                        oldToNewMap[oldNum] = currentNewNumber;
                        currentNewNumber++;
                    }
                }
            }
            
            // Наконец, сама перемещенная секция
            for (var i = 0; i < analysis.sectionsWithNotes.length; i++) {
                var sectionData = analysis.sectionsWithNotes[i];
                
                if (sectionData.isMovedSection) {
                    for (var j = 0; j < sectionData.notes.length; j++) {
                        var oldNum = sectionData.notes[j].noteNum;
                        oldToNewMap[oldNum] = currentNewNumber;
                        currentNewNumber++;
                    }
                }
            }
            
            // 4. Обновляем ссылки в тексте
            for (var i = 0; i < analysis.sectionsWithNotes.length; i++) {
                var sectionData = analysis.sectionsWithNotes[i];
                
                for (var j = 0; j < sectionData.notes.length; j++) {
                    var noteLink = sectionData.notes[j];
                    var oldNum = noteLink.noteNum;
                    var newNum = oldToNewMap[oldNum];
                    
                    if (oldNum != newNum) {
                        // Обновляем href
                        var oldHref = noteLink.href;
                        var newHref = oldHref.replace(/#n_\d+$/, "#n_" + newNum);
                        noteLink.element.setAttribute("href", newHref);
                        
                        // Обновляем текст ссылки
                        var oldText = noteLink.linkText;
                        var newText = oldText.replace(/\[\d+\]/, "[" + newNum + "]");
                        noteLink.element.innerHTML = newText;
                        
                        result.fixedLinks++;
                    }
                }
            }
            
            // 5. Обновляем разделы примечаний
            // Очищаем notesBody
            var child = notesBody.firstChild;
            while (child) {
                var nextChild = child.nextSibling;
                if (child.nodeType == 1 && child.className == "section") {
                    child.removeNode(true);
                }
                child = nextChild;
            }
            
            // Вставляем примечания в правильном порядке (по новым номерам)
            for (var newNum = 1; newNum <= analysis.totalNotes; newNum++) {
                // Находим старый номер для этого нового номера
                var oldNum = -1;
                for (var key in oldToNewMap) {
                    if (oldToNewMap[key] == newNum) {
                        oldNum = parseInt(key, 10);
                        break;
                    }
                }
                
                if (oldNum > 0 && noteMap[oldNum]) {
                    var section = noteMap[oldNum];
                    var sectionElement = section.element.cloneNode(true);
                    
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
            }
            
        } catch(e) {
            // Игнорируем ошибки
        }
        
        return result;
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
}
