// Скрипт "Переместить текущую секцию вверх" для редактора FBE
// version 1.0
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для перемещения текущей секции fb2 документа вверх (выше по тексту).
// Скрипт может перемещать как родительские секции, так и вложенные - внутри своих родительских.
// Родительские секции перемещаются целиком - включая все вложенные секции.
// По умолчанию обрабатывается только основной раздел документа.
// Для удобства выделения для перемещения всей родительской секции
// можно просто дважды быстро щелкнуть по ее заголовку слева на панели структуры документа.
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// При перемещении секций ПЕРЕНУМЕРАЦИЯ СНОСОК НЕ ПРОИЗВОДИТСЯ.
// Перенумерацию сносок надо выполнять отдельным скриптом!

// version 1.0, 28.01.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Переместить текущую секцию вверх";
    var version = "1.0";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Показывать сообщения (1 - Да, 0 - Нет (тихий режим))
    var showMessages = 1; // По умолчанию: 1 - показывать сообщения
    
    // Обрабатывать раздел сносок (примечаний) (0 - Нет, 1 - Да)
    var processNotes = 0; // По умолчанию: 0 - не обрабатывать
    
    // Обрабатывать раздел комментариев (0 - Нет, 1 - Да)
    var processComments = 0; // По умолчанию: 0 - не обрабатывать
    
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
                MsgBox(scriptName + "\nver. " + version + "\n\nКурсор не находится внутри секции.\n\nУстановите курсор в секцию, которую хотите переместить вверх.");
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
        
        // Находим предыдущую секцию того же уровня для ТЕКУЩЕЙ секции
        var previousSection = findPreviousSection(currentSection, parentContainer);
        
        // Диалоги с пользователем (только в обычном режиме)
        var moveOperation = null; // null - отмена, "nested" - вложенная, "parent" - родительская, "root" - корневая
        var targetSection = currentSection; // Какую секцию будем перемещать
        var targetParent = parentContainer; // Родительский контейнер для перемещения
        var targetPreviousSection = previousSection; // Предыдущая секция для перемещения
        
        if (showMessages) {
            if (isRootSection) {
                // Корневая секция (первого уровня)
                if (!targetPreviousSection) {
                    var message = "Секция ";
                    if (isParentSection) message += "(родительская) ";
                    message += "уже первая и не может быть перемещена выше.";
                    MsgBox(scriptName + "\nver. " + version + "\n\n" + message);
                    window.external.EndUndoUnit(document);
                    return;
                }
                
                var confirmMessage = "Вы находитесь в секции первого уровня";
                if (isParentSection) confirmMessage += " (родительской, содержит вложенные секции)";
                confirmMessage += ".\n\nХотите перенести эту секцию выше?";
                
                if (AskYesNo(scriptName + "\nver. " + version + "\n\n" + confirmMessage)) {
                    moveOperation = "root";
                }
            } else {
                // Вложенная секция (уровень 2 и выше)
                if (!targetPreviousSection) {
                    // Вложенная секция первая, проверяем родительскую
                    var parentPreviousSection = findPreviousSection(parentContainer, getParentContainer(parentContainer));
                    
                    if (!parentPreviousSection) {
                        MsgBox(scriptName + "\nver. " + version + "\n\nВложенная секция первая. Родительская секция также первая.\n\nСекции не могут быть перемещены выше.");
                        window.external.EndUndoUnit(document);
                        return;
                    }
                    
                    // Предлагаем только родительскую
                    var parentConfirm = "Вложенная секция первая в родительской.\n\nХотите переместить ВСЮ РОДИТЕЛЬСКУЮ секцию?\n(вместе со всеми вложенными секциями)";
                    
                    if (AskYesNo(scriptName + "\nver. " + version + "\n\n" + parentConfirm)) {
                        moveOperation = "parent";
                        targetSection = parentContainer;
                        targetParent = getParentContainer(parentContainer);
                        targetPreviousSection = parentPreviousSection;
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
                        var parentPreviousSection = findPreviousSection(parentContainer, getParentContainer(parentContainer));
                        
                        if (!parentPreviousSection) {
                            MsgBox(scriptName + "\nver. " + version + "\n\nРодительская секция уже первая и не может быть перемещена выше.");
                            window.external.EndUndoUnit(document);
                            return;
                        }
                        
                        // Подтверждение перемещения родительской секции
                        var parentConfirm = "Хотите переместить ВСЮ РОДИТЕЛЬСКУЮ секцию?\n(вместе со всеми вложенными секциями)";
                        
                        if (AskYesNo(scriptName + "\nver. " + version + "\n\n" + parentConfirm)) {
                            moveOperation = "parent";
                            targetSection = parentContainer;
                            targetParent = getParentContainer(parentContainer);
                            targetPreviousSection = parentPreviousSection;
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
            if (!targetPreviousSection) {
                if (nestingLevel > 1) {
                    // Во вложенной секции - проверяем родительскую
                    var parentPreviousSection = findPreviousSection(parentContainer, getParentContainer(parentContainer));
                    if (!parentPreviousSection) {
                        window.external.EndUndoUnit(document);
                        return; // Ничего не делаем в тихом режиме
                    }
                    // Перемещаем родительскую
                    moveOperation = "parent";
                    targetSection = parentContainer;
                    targetParent = getParentContainer(parentContainer);
                    targetPreviousSection = parentPreviousSection;
                } else {
                    window.external.EndUndoUnit(document);
                    return; // Ничего не делаем в тихом режиме
                }
            } else {
                moveOperation = isRootSection ? "root" : "nested";
            }
        }
        
        // Таймер включаем ТОЛЬКО после всех диалогов!
        var startTime = new Date().getTime();
        
        // Перемещаем секцию
        var moved = moveSectionUp(targetSection, targetPreviousSection, targetParent);
        
        if (moved) {
            // Прокручиваем к новой позиции секции
            scrollToSection(targetSection);
            
            // Выводим сообщение об успехе (если включены сообщения)
            if (showMessages) {
                var endTime = new Date().getTime();
                var executionTime = ((endTime - startTime) / 1000).toFixed(3);
                
                var message = "✓ ";
                
                if (moveOperation == "nested") {
                    message += "Вложенная секция перенесена успешно!";
                } else if (moveOperation == "parent") {
                    message += "Родительская секция перенесена успешно!";
                } else {
                    // Корневая секция
                    if (isParent(targetSection)) {
                        message += "Секция (родительская, содержит вложенные) перенесена успешно!";
                    } else {
                        message += "Секция перенесена успешно!";
                    }
                }
                
                message += "\n\nВремя выполнения: " + executionTime + " сек";
                
                MsgBox(scriptName + "\nver. " + version + "\n\n" + message);
            }
        } else {
            if (showMessages) {
                MsgBox(scriptName + "\nver. " + version + "\n\nНе удалось переместить секцию.");
            }
        }
        
    } catch(e) {
        if (showMessages) {
            MsgBox(scriptName + "\nver. " + version + "\n\nПроизошла ошибка: " + e.message);
        }
    }
    
    // Завершаем блок отмены действий
    window.external.EndUndoUnit(document);
    
    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
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
    
    // Функция получения родительского контейнера
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
    
    // Функция поиска ПРЕДЫДУЩЕЙ секции того же уровня (НОВОЕ для перемещения вверх)
    function findPreviousSection(section, parentContainer) {
        var foundSection = false;
        var previousSection = null;
        
        // Перебираем все дочерние элементы родительского контейнера
        for (var i = 0; i < parentContainer.childNodes.length; i++) {
            var child = parentContainer.childNodes[i];
            
            if (child.nodeType == 1) { // ELEMENT_NODE
                if (child === section) {
                    // Нашли текущую секцию - возвращаем предыдущую
                    return previousSection;
                } else if (child.nodeName == "DIV" && child.className == "section") {
                    // Нашли секцию, которая может быть предыдущей
                    previousSection = child;
                }
            }
        }
        
        return null; // Предыдущая секция не найдена (текущая секция первая)
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
    
    // Функция перемещения секции ВВЕРХ (ИЗМЕНЕНО для перемещения вверх)
    function moveSectionUp(section, previousSection, parentContainer) {
        try {
            // Если предыдущей секции нет, нельзя переместить вверх
            if (!previousSection) {
                return false;
            }
            
            // Вставляем текущую секцию перед предыдущей
            parentContainer.insertBefore(section, previousSection);
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
}
