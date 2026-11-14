// Скрипт "Вставить пустой раздел-секцию перед текущим разделом" для редактора FBE
// version 1.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для вставки пустого раздела-секции перед тем разделом,
// в котором установлен курсор в fb2 документах.
// Если курсор находится во вложенном разделе-секции - вставляется пустой раздел
// того же уровня вложенности перед текущим, внутри родительского раздела-секции.
// Если курсор находится в корневом разделе-секции (первого уровня) - вставляется
// пустой раздел-секция того же уровня перед текущим.
// Пустой раздел-секция содержит один абзац с пустой строкой - для валидности.
// По умолчанию обрабатывается только основной раздел документа.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.2, 09.04.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Вставить пустой раздел-секцию перед текущим разделом";
    var version = "1.2";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима
    
    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 1; // 0 - нет, 1 - да
    
    // Обрабатывать раздел комментариев
    var processCommentsSection = 1; // 0 - нет, 1 - да
    
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
        
        // Получаем текущую позицию курсора
        var range = document.selection.createRange();
        if (!range) {
            if (showStatistics == 1) {
                MsgBox(scriptName + "\nver. " + version + "\n\nНе удалось определить текущую позицию курсора.");
            }
            return;
        }
        
        var parentElement = range.parentElement();
        if (!parentElement) {
            if (showStatistics == 1) {
                MsgBox(scriptName + "\nver. " + version + "\n\nНе удалось определить текущий элемент.");
            }
            return;
        }
        
        // Ищем текущую секцию (ищем DIV с классом "section")
        var currentSection = findCurrentSection(parentElement);
        
        // Проверяем, нашли ли мы секцию
        if (!currentSection) {
            if (showStatistics == 1) {
                MsgBox(scriptName + "\nver. " + version + "\n\nКурсор не находится внутри секции.\n\nУстановите курсор в секцию, перед которой нужно вставить пустую.");
            }
            return;
        }
        
        // Проверяем, можно ли обрабатывать этот раздел body
        if (!canProcessBodySection(currentSection)) {
            if (showStatistics == 1) {
                MsgBox(scriptName + "\nver. " + version + "\n\nСекция находится в разделе, который не обрабатывается согласно настройкам.\n\nИзмените настройки скрипта для обработки разделов сносок или комментариев.");
            }
            return;
        }
        
        // Определяем родительский контейнер для вставки
        var parentContainer = getParentContainer(currentSection);
        var nestingLevel = getNestingLevel(currentSection);
        
        // Начинаем блок отмены действий (только после всех проверок и confirm-ов)
        // Таймер включаем ПОСЛЕ confirm-ов, чтобы считать только время выполнения операции
        var startTime = null;
        if (showStatistics == 1) {
            // Запрашиваем подтверждение перед началом операции
            var confirmMessage = "Курсор находится ";
            if (nestingLevel == 1) {
                confirmMessage += "в корневой секции (первого уровня).";
            } else {
                confirmMessage += "во вложенной секции (уровень " + nestingLevel + ").";
            }
            confirmMessage += "\n\nВставить пустую секцию перед текущей?";
            
            if (!AskYesNo(scriptName + "\nver. " + version + "\n\n" + confirmMessage)) {
                return; // Пользователь отказался
            }
            
            // Запускаем таймер только после подтверждения
            startTime = new Date().getTime();
        } else {
            // В тихом режиме запускаем таймер сразу
            startTime = new Date().getTime();
        }
        
        // Начинаем запись в систему отмены
        window.external.BeginUndoUnit(document, scriptName);
        
        // Создаем новую пустую секцию
        var newSection = document.createElement("DIV");
        newSection.className = "section";
        
        // Создаем пустой абзац внутри секции
        var emptyParagraph = document.createElement("P");
        newSection.appendChild(emptyParagraph);
        
        // Наполняем абзац неразрывным пробелом
        emptyParagraph.innerHTML = nbspEntity;
        
        // Вставляем новую секцию перед текущей внутри родительского контейнера
        parentContainer.insertBefore(newSection, currentSection);
        
        // Перемещаем курсор в начало новой секции
        goToSection(newSection);
        
        // Завершаем запись в систему отмены
        window.external.EndUndoUnit(document);
        
        // Выводим сообщение об успехе (если включена статистика)
        if (showStatistics == 1) {
            var endTime = new Date().getTime();
            var executionTime = ((endTime - startTime) / 1000).toFixed(3);
            
            var successMessage = "✓ Пустая секция успешно вставлена!";
            if (nestingLevel == 1) {
                successMessage += "\n  • Уровень вложенности: корневая секция";
            } else {
                successMessage += "\n  • Уровень вложенности: " + nestingLevel;
            }
            successMessage += "\n\nВремя выполнения: " + executionTime.replace(".", ",") + " сек";
            
            MsgBox(scriptName + "\nver. " + version + "\n\n" + successMessage);
        }
        
    } catch(e) {
        // В случае ошибки завершаем отмену и показываем сообщение
        try {
            window.external.EndUndoUnit(document);
        } catch(e2) {
            // Игнорируем
        }
        
        if (showStatistics == 1) {
            MsgBox(scriptName + "\nver. " + version + "\n\nПроизошла ошибка: " + e.message);
        }
    }
    
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
    
    // Функция проверки возможности обработки раздела body
    function canProcessBodySection(section) {
        // Ищем родительский body
        var parent = section;
        while (parent && parent.nodeName != "BODY") {
            if (parent.nodeName == "DIV" && parent.className == "body") {
                var fbname = parent.getAttribute("fbname") || "";
                
                if (fbname == "notes" && processNotesSection == 0) {
                    return false; // Раздел сносок не обрабатывается
                }
                if (fbname == "comments" && processCommentsSection == 0) {
                    return false; // Раздел комментариев не обрабатывается
                }
                return true; // Основной body или разрешенный раздел
            }
            parent = parent.parentElement;
        }
        
        return true; // Если body не найден, разрешаем обработку
    }
    
    // Функция перехода к секции и прокрутки
    function goToSection(section) {
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
