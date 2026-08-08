// Скрипт "Добавить пустые строки после каждых N абзацев" для редактора FBE
// version 2.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для добавления пустых строк после каждых N абзацев в fb2 документах.
// Количество строк, после которых вставляются пустые строки, задается по запросу скрипта.
// Отдельные настройки для игнорирования уже размеченных структурных элементов (стихи, цитаты, аннотации...)

// version 2.1, 24.12.2025
//======================================

function Run() {
    // НАСТРОЙКИ СКРИПТА ==========================================
    var defaultN = 4;        // По умолчанию вставлять пустую строку после каждых 4 абзацев
    var askUser = 1;         // 1 - показывать окно запроса числа N, 0 - использовать default N
    
    // НАСТРОЙКИ ОБРАБОТКИ ЭЛЕМЕНТОВ (1 - обрабатывать, 0 - не обрабатывать)
    var processAnnotation = 0;    // Аннотация общая
    var processHistory = 0;       // История
    var processNotes = 0;         // Сноски
    var processComments = 0;      // Комментарии
    // -------------------------------
    var processTitles = 0;        // Заголовки
    var processSectionAnnotation = 0;  // Аннотации секций
    var processEpigraphs = 0;     // Эпиграфы
    var processCites = 0;         // Цитаты
    var processPoems = 1;         // Стихи (poem, stanza) - по умолчанию ДА
    var processTextAuthors = 0;   // Авторы текста
    var processSubtitles = 0;     // Подзаголовки
    var processParagraphs = 1;    // Обычный текст (p) - по умолчанию ДА
    // ===========================================================
    
    var versionNumber = "2.1";
    
    // Сохраняем текущее выделение перед началом работы
    var savedSelection = null;
    try {
        if (document.selection) {
            savedSelection = document.selection.createRange();
        }
    } catch(e) {
        // Игнорируем ошибку
    }
    
    // Запрос у пользователя числа N
    var N = defaultN;
    if (askUser == 1) {
        var input = window.external.InputBox(
            "Добавить пустые строки после каждых N абзацев\n" +
            "ver. " + versionNumber + "\n" +
            "----------------------------------------\n" +
            "Введите число N (после каждых скольких абзацев \n" +
            "вставлять пустую строку):",
            defaultN.toString(),
            "Ввод числа N"
        );
        
        // Проверяем введенное значение
        if (input === null || input === "") {
            MsgBox("Операция отменена пользователем.", "FBE скрипт");
            return;
        }
        
        // Преобразуем в число
        N = parseInt(input, 10);
        if (isNaN(N) || N < 1) {
            MsgBox(
                "Ошибка: необходимо ввести целое число больше 0.\n" +
                "Будет использовано значение по умолчанию: " + defaultN,
                "FBE скрипт"
            );
            N = defaultN;
        }
    }
    
    // Определяем область обработки
    var selection = document.selection;
    var range = null;
    var processFromCursor = 0;
    var processSelection = 0;
    var processAll = 0;
    var startNode = null;
    var endNode = null;
    
    if (selection) {
        range = selection.createRange();
        if (range && range.text && range.text !== "") {
            // Есть выделение
            processSelection = 1;
            
            // Получаем начальный и конечный узлы выделения
            try {
                // Копируем range для работы с границами
                var rangeCopy = range.duplicate();
                rangeCopy.collapse(true);
                startNode = rangeCopy.parentElement();
                
                var rangeEnd = range.duplicate();
                rangeEnd.collapse(false);
                endNode = rangeEnd.parentElement();
            } catch(e) {
                // Если не получилось получить границы
            }
            
            // Проверяем, не выделен ли весь документ (Ctrl+A)
            try {
                var allText = document.body.innerText || document.body.textContent;
                var selectedText = range.text;
                if (selectedText.length > allText.length * 0.95) {
                    // Похоже на выделение всего документа
                    processAll = 1;
                    processSelection = 0;
                    startNode = null;
                    endNode = null;
                }
            } catch(e) {
                // Если не получилось сравнить - оставляем как выделение
            }
        } else {
            // Нет выделения, но есть курсор
            processFromCursor = 1;
            
            // Получаем узел курсора
            if (range) {
                try {
                    range.collapse(true);
                    startNode = range.parentElement();
                } catch(e) {
                    // Игнорируем ошибку
                }
            }
        }
    } else {
        // Нет ни выделения, ни курсора - начинаем с начала основного текста
        processAll = 1;
    }
    
    var modeText = "";
    if (processAll == 1) {
        modeText = "весь основной текст";
    } else if (processSelection == 1) {
        modeText = "выделение";
    } else {
        modeText = "от курсора";
    }
    
    // Сообщение с настройками обработки
    var settingsInfo = "Настройки обработки элементов:\n";
    settingsInfo += "Аннотация общая: " + (processAnnotation ? "ДА" : "НЕТ") + "\n";
    settingsInfo += "История: " + (processHistory ? "ДА" : "НЕТ") + "\n";
    settingsInfo += "Сноски: " + (processNotes ? "ДА" : "НЕТ") + "\n";
    settingsInfo += "Комментарии: " + (processComments ? "ДА" : "НЕТ") + "\n";
    settingsInfo += "Заголовки: " + (processTitles ? "ДА" : "НЕТ") + "\n";
    settingsInfo += "Аннотации секций: " + (processSectionAnnotation ? "ДА" : "НЕТ") + "\n";
    settingsInfo += "Эпиграфы: " + (processEpigraphs ? "ДА" : "НЕТ") + "\n";
    settingsInfo += "Цитаты: " + (processCites ? "ДА" : "НЕТ") + "\n";
    settingsInfo += "Стихи: " + (processPoems ? "ДА" : "НЕТ") + "\n";
    settingsInfo += "Авторы текста: " + (processTextAuthors ? "ДА" : "НЕТ") + "\n";
    settingsInfo += "Подзаголовки: " + (processSubtitles ? "ДА" : "НЕТ") + "\n";
    settingsInfo += "Обычный текст: " + (processParagraphs ? "ДА" : "НЕТ") + "\n";
    
    var confirmMsg = 
        "FBE скрипт\n" +
        "---------------------------\n" +
        "Добавить пустые строки после каждых N абзацев\n" +
        "ver. " + versionNumber + "\n" +
        "----------------------------------------\n" +
        "Начинаю обработку...\n" +
        "Вставляю пустую строку после каждых " + N + " абзацев.\n\n" +
        "Режим: " + modeText + ".\n\n" +
        settingsInfo;
    
    // Запрашиваем подтверждение
    if (!confirm(confirmMsg)) {
        MsgBox("Операция отменена пользователем.", "FBE скрипт");
        return;
    }
    
    // Теперь запускаем таймер
    var startTime = new Date().getTime();
    
    // Находим основной fbw_body
    var fbw_body = document.getElementById("fbw_body");
    if (!fbw_body) {
        MsgBox("Ошибка: не найден элемент fbw_body!", "FBE скрипт");
        return;
    }
    
    // Начинаем блок отмены действий
    window.external.BeginUndoUnit(document, "Добавить пустые строки после каждых " + N + " абзацев");
    
    // Основные переменные
    var totalAdded = 0;
    
    // Функция для создания пустой строки как в FBE
    function createEmptyLine() {
        var p = document.createElement("P");
        p.innerHTML = "&nbsp;";
        return p;
    }
    
    // Функция проверки, является ли node1 предком node2
    function isAncestor(node1, node2) {
        var parent = node2;
        while (parent) {
            if (parent === node1) {
                return true;
            }
            parent = parent.parentNode;
        }
        return false;
    }
    
    // Улучшенная функция для проверки, можно ли вставлять пустую строку после этого узла
    function canInsertEmptyLineAfter(node) {
        if (!node) return false;
        
        // Если узел сам является text-author или подзаголовком, НЕ вставляем после него
        if (node.nodeName === "P") {
            if (node.className === "text-author" || node.className === "subtitle") {
                return false;
            }
        }
        
        // Проверяем следующий элемент в том же родителе
        var nextNode = node.nextSibling;
        
        // Если есть следующий элемент
        if (nextNode) {
            // Если следующий элемент - DIV (любой), НЕ вставляем
            if (nextNode.nodeName === "DIV") {
                return false;
            }
            
            // Если следующий элемент - text-author, НЕ вставляем
            if (nextNode.nodeName === "P" && nextNode.className === "text-author") {
                return false;
            }
            
            // Если следующий элемент - подзаголовок, НЕ вставляем
            if (nextNode.nodeName === "P" && nextNode.className === "subtitle") {
                return false;
            }
        }
        // Если нет следующего элемента (node - последний в своем родителе)
        else {
            // Получаем родителя узла
            var parent = node.parentNode;
            
            // Если родитель - DIV (например, stanza, poem, section и т.д.)
            // И мы последний элемент в этом DIV, то после нас будет закрывающий тег DIV
            // В этом случае НЕ вставляем ПС
            if (parent && parent.nodeName === "DIV") {
                // Проверяем, что родитель НЕ является обычным контейнером для текста
                // который может содержать только P элементы
                var parentClass = parent.className;
                
                // Если это stanza или poem - точно не вставляем
                if (parentClass === "stanza" || parentClass === "poem") {
                    return false;
                }
                
                // Если это любой другой DIV, тоже не вставляем
                return false;
            }
            
            // Проверяем следующего sibling у родителя
            if (parent) {
                var parentNext = parent.nextSibling;
                if (parentNext) {
                    // Если следующий элемент родителя - DIV, НЕ вставляем
                    if (parentNext.nodeName === "DIV") {
                        return false;
                    }
                    
                    // Если следующий элемент родителя - P с text-author, НЕ вставляем
                    if (parentNext.nodeName === "P" && parentNext.className === "text-author") {
                        return false;
                    }
                }
            }
        }
        
        return true;
    }
    
    // Функция для вставки пустой строки после указанного узла
    function insertEmptyLineAfter(node) {
        if (!node) return null;
        
        // Проверяем, можно ли вставлять
        if (!canInsertEmptyLineAfter(node)) {
            return null;
        }
        
        var emptyLine = createEmptyLine();
        
        // Вставляем после текущего узла
        try {
            if (node.nextSibling) {
                node.parentNode.insertBefore(emptyLine, node.nextSibling);
            } else {
                node.parentNode.appendChild(emptyLine);
            }
            return emptyLine;
        } catch(e) {
            return null;
        }
    }
    
    // Функция для проверки, является ли узел элементом, сбрасывающим счетчик
    function isResetElement(node) {
        if (!node) return false;
        
        // ЛЮБОЙ DIV сбрасывает счетчик
        if (node.nodeName === "DIV") {
            return true;
        }
        
        // Проверяем пустую строку
        if (node.nodeName === "P") {
            if (node.className === "empty-line") {
                return true;
            }
            
            // Проверяем, не пустой ли это абзац
            var html = node.innerHTML || "";
            var cleanText = html.replace(/<[^>]*>/g, "");
            cleanText = cleanText.replace(/ |&nbsp;|\s/g, " ");
            if (cleanText.replace(/ /g, "").length === 0) {
                return true;
            }
            
            // Подзаголовки и авторы текста сбрасывают счетчик
            if (node.className === "subtitle" || node.className === "text-author") {
                return true;
            }
        }
        
        return false;
    }
    
    // Функция для проверки, является ли узел абзацем, который нужно считать
    function isParagraphToCount(node) {
        if (!node) return false;
        
        // Проверяем, что это тег <p>
        if (node.nodeName !== "P") return false;
        
        // Проверяем, что он не является подзаголовком, пустой строкой или text-author
        if (node.className === "subtitle" || 
            node.className === "empty-line" || 
            node.className === "text-author") {
            return false;
        }
        
        // Проверяем, не пустой ли абзац
        var html = node.innerHTML || "";
        var cleanText = html.replace(/<[^>]*>/g, "");
        cleanText = cleanText.replace(/ |&nbsp;|\s/g, " ");
        if (cleanText.replace(/ /g, "").length === 0) {
            return false;
        }
        
        // Проверяем родительский элемент
        var parent = node.parentNode;
        if (parent && parent.nodeName === "DIV") {
            var parentClass = parent.className;
            
            // Проверяем настройки обработки
            if (parentClass === "annotation" && !processSectionAnnotation) return false;
            if (parentClass === "title" && !processTitles) return false;
            if (parentClass === "epigraph" && !processEpigraphs) return false;
            if (parentClass === "cite" && !processCites) return false;
            if ((parentClass === "poem" || parentClass === "stanza") && !processPoems) return false;
            if (parentClass === "text-author" && !processTextAuthors) return false;
        }
        
        // Проверяем общую настройку для обычного текста
        if (!processParagraphs) {
            // Если обычный текст не обрабатываем, проверяем родителя
            if (!parent || parent.nodeName !== "DIV" || 
                (parent.className !== "poem" && parent.className !== "stanza")) {
                return false;
            }
        }
        
        return true;
    }
    
    // Функция проверки, нужно ли обрабатывать данный body элемент
    function shouldProcessBody(bodyElement) {
        if (!bodyElement || bodyElement.nodeName !== "DIV" || bodyElement.className !== "body") {
            return false;
        }
        
        var fbname = bodyElement.getAttribute("fbname");
        
        // Проверяем настройки
        if (fbname === "notes" && !processNotes) return false;
        if (fbname === "comments" && !processComments) return false;
        if (bodyElement.className === "annotation" && !processAnnotation) return false;
        if (bodyElement.className === "history" && !processHistory) return false;
        
        return true;
    }
    
    // Функция для нахождения корневого body элемента для узла
    function findBodyForNode(node) {
        var current = node;
        while (current && current !== fbw_body) {
            if (current.nodeName === "DIV" && current.className === "body") {
                return current;
            }
            current = current.parentNode;
        }
        return null;
    }
    
    // Функция для поиска следующего обрабатываемого узла от начальной точки
    function findFirstProcessableNode(fromNode, bodyElement) {
        if (!fromNode) return null;
        
        var currentNode = fromNode;
        
        // Пропускаем необрабатываемые элементы, пока не найдем подходящий
        while (currentNode) {
            // Проверяем, не вышли ли за пределы body
            if (!isAncestor(bodyElement, currentNode) && currentNode !== bodyElement) {
                return null;
            }
            
            // Если это абзац, который можно обрабатывать
            if (isParagraphToCount(currentNode)) {
                return currentNode;
            }
            
            // Если это DIV, который нужно обрабатывать
            if (currentNode.nodeName === "DIV") {
                var className = currentNode.className;
                
                // Проверяем, нужно ли обрабатывать этот DIV
                var shouldProcessDiv = false;
                
                if (className === "section") {
                    shouldProcessDiv = true;
                } else if ((className === "poem" || className === "stanza") && processPoems) {
                    shouldProcessDiv = true;
                } else if (className === "cite" && processCites) {
                    shouldProcessDiv = true;
                } else if (className === "epigraph" && processEpigraphs) {
                    shouldProcessDiv = true;
                } else if (className === "title" && processTitles) {
                    shouldProcessDiv = true;
                } else if (className === "annotation" && processSectionAnnotation) {
                    shouldProcessDiv = true;
                }
                
                if (shouldProcessDiv) {
                    // Ищем первый обрабатываемый элемент внутри этого DIV
                    var firstChild = currentNode.firstChild;
                    if (firstChild) {
                        var found = findFirstProcessableNode(firstChild, bodyElement);
                        if (found) {
                            return found;
                        }
                    }
                }
            }
            
            // Переходим к следующему элементу
            currentNode = currentNode.nextSibling;
        }
        
        return null;
    }
    
    // Улучшенная функция обработки узла и его потомков
    function processNode(node, counters, stopNode) {
        if (!node) return;
        
        var currentNode = node;
        while (currentNode && currentNode !== stopNode) {
            // Если это элемент, сбрасывающий счетчик - сбрасываем
            if (isResetElement(currentNode)) {
                counters.paragraphCount = 0;
            }
            
            // Если это абзац, который нужно считать
            if (isParagraphToCount(currentNode)) {
                counters.paragraphCount++;
                
                // Если достигли N абзацев
                if (counters.paragraphCount === N) {
                    // ВАЖНО: Проверяем, можно ли вставить ПС после этого абзаца
                    if (canInsertEmptyLineAfter(currentNode)) {
                        var emptyLine = insertEmptyLineAfter(currentNode);
                        if (emptyLine) {
                            totalAdded++;
                        }
                    }
                    // Сбрасываем счетчик в ЛЮБОМ случае
                    counters.paragraphCount = 0;
                }
            }
            
            // Рекурсивно обрабатываем дочерние элементы
            if (currentNode.nodeName === "DIV") {
                var className = currentNode.className;
                
                // Проверяем, нужно ли обрабатывать этот DIV
                var shouldProcessDiv = false;
                
                if (className === "section") {
                    shouldProcessDiv = true;
                } else if ((className === "poem" || className === "stanza") && processPoems) {
                    shouldProcessDiv = true;
                } else if (className === "cite" && processCites) {
                    shouldProcessDiv = true;
                } else if (className === "epigraph" && processEpigraphs) {
                    shouldProcessDiv = true;
                } else if (className === "title" && processTitles) {
                    shouldProcessDiv = true;
                } else if (className === "annotation" && processSectionAnnotation) {
                    shouldProcessDiv = true;
                }
                
                if (shouldProcessDiv && currentNode.firstChild) {
                    // Сохраняем текущий счетчик
                    var childCounters = {
                        paragraphCount: counters.paragraphCount
                    };
                    processNode(currentNode.firstChild, childCounters, stopNode);
                }
            }
            
            currentNode = currentNode.nextSibling;
        }
    }
    
    // Основная логика обработки
    if (processAll == 1) {
        // Обрабатываем весь основной текст согласно настройкам
        var bodyElement = fbw_body.firstChild;
        
        while (bodyElement) {
            if (shouldProcessBody(bodyElement)) {
                // Обрабатываем этот body
                var section = bodyElement.firstChild;
                while (section) {
                    if (section.nodeName === "DIV" && section.className === "section") {
                        var counters = {
                            paragraphCount: 0
                        };
                        processNode(section.firstChild, counters, null);
                    }
                    section = section.nextSibling;
                }
            }
            bodyElement = bodyElement.nextSibling;
        }
    } else if (processSelection == 1) {
        // Обрабатываем только выделение
        if (!startNode || !endNode) {
            MsgBox("Не удалось определить границы выделения.", "FBE скрипт");
            window.external.EndUndoUnit(document);
            return;
        }
        
        // Находим общий родительский body
        var startBody = findBodyForNode(startNode);
        var endBody = findBodyForNode(endNode);
        
        if (!startBody || !endBody || startBody !== endBody) {
            MsgBox("Выделение должно находиться в пределах одного раздела body.", "FBE скрипт");
            window.external.EndUndoUnit(document);
            return;
        }
        
        // Проверяем, нужно ли обрабатывать этот body
        if (!shouldProcessBody(startBody)) {
            MsgBox("Выделение находится в разделе, который не обрабатывается согласно настройкам.", "FBE скрипт");
            window.external.EndUndoUnit(document);
            return;
        }
        
        // НАХОДИМ ПЕРВЫЙ ОБРАБАТЫВАЕМЫЙ УЗЕЛ ОТ НАЧАЛА ВЫДЕЛЕНИЯ
        var firstProcessableNode = findFirstProcessableNode(startNode, startBody);
        
        if (!firstProcessableNode) {
            // Если не нашли обрабатываемый узел от начала выделения, ищем от следующего элемента
            var nextNode = startNode.nextSibling;
            while (nextNode) {
                firstProcessableNode = findFirstProcessableNode(nextNode, startBody);
                if (firstProcessableNode) break;
                nextNode = nextNode.nextSibling;
            }
        }
        
        if (!firstProcessableNode) {
            MsgBox("В выделенной области не найдено элементов для обработки.", "FBE скрипт");
            window.external.EndUndoUnit(document);
            return;
        }
        
        // Обрабатываем от найденного узла до конца выделения
        var counters = {
            paragraphCount: 0
        };
        
        var currentNode = firstProcessableNode;
        var stopNode = endNode.nextSibling;
        
        while (currentNode && currentNode !== stopNode) {
            if (isResetElement(currentNode)) {
                counters.paragraphCount = 0;
            }
            
            if (isParagraphToCount(currentNode)) {
                counters.paragraphCount++;
                
                if (counters.paragraphCount === N) {
                    if (canInsertEmptyLineAfter(currentNode)) {
                        var emptyLine = insertEmptyLineAfter(currentNode);
                        if (emptyLine) {
                            totalAdded++;
                        }
                    }
                    counters.paragraphCount = 0;
                }
            }
            
            if (currentNode.nodeName === "DIV") {
                var className = currentNode.className;
                
                var shouldProcessDiv = false;
                
                if (className === "section") {
                    shouldProcessDiv = true;
                } else if ((className === "poem" || className === "stanza") && processPoems) {
                    shouldProcessDiv = true;
                } else if (className === "cite" && processCites) {
                    shouldProcessDiv = true;
                } else if (className === "epigraph" && processEpigraphs) {
                    shouldProcessDiv = true;
                } else if (className === "title" && processTitles) {
                    shouldProcessDiv = true;
                }
                
                if (shouldProcessDiv && currentNode.firstChild) {
                    var childCounters = {
                        paragraphCount: counters.paragraphCount
                    };
                    processNode(currentNode.firstChild, childCounters, stopNode);
                }
            }
            
            currentNode = currentNode.nextSibling;
        }
    } else if (processFromCursor == 1) {
        // Обрабатываем от курсора до конца всего основного текста
        
        if (!startNode) {
            MsgBox("Не удалось определить позицию курсора.", "FBE скрипт");
            window.external.EndUndoUnit(document);
            return;
        }
        
        // Находим body элемент, в котором находится курсор
        var startBody = findBodyForNode(startNode);
        if (!startBody) {
            MsgBox("Курсор находится вне основного текста книги.", "FBE скрипт");
            window.external.EndUndoUnit(document);
            return;
        }
        
        // Проверяем, нужно ли обрабатывать этот body
        if (!shouldProcessBody(startBody)) {
            MsgBox("Курсор находится в разделе, который не обрабатывается согласно настройкам.", "FBE скрипт");
            window.external.EndUndoUnit(document);
            return;
        }
        
        // НАХОДИМ ПЕРВЫЙ ОБРАБАТЫВАЕМЫЙ УЗЕЛ ОТ КУРСОРА
        var firstProcessableNode = findFirstProcessableNode(startNode, startBody);
        
        if (!firstProcessableNode) {
            // Если не нашли обрабатываемый узел от курсора, ищем от следующего элемента
            var nextNode = startNode.nextSibling;
            while (nextNode) {
                firstProcessableNode = findFirstProcessableNode(nextNode, startBody);
                if (firstProcessableNode) break;
                nextNode = nextNode.nextSibling;
            }
        }
        
        if (!firstProcessableNode) {
            MsgBox("От позиции курсора не найдено элементов для обработки.", "FBE скрипт");
            window.external.EndUndoUnit(document);
            return;
        }
        
        // НАЧИНАЕМ ОБРАБОТКУ ОТ НАЙДЕННОГО УЗЛА
        
        // Находим первый body в основном тексте для начала обхода
        var firstMainBody = null;
        var bodyElement = fbw_body.firstChild;
        
        while (bodyElement) {
            if (shouldProcessBody(bodyElement)) {
                firstMainBody = bodyElement;
                break;
            }
            bodyElement = bodyElement.nextSibling;
        }
        
        if (!firstMainBody) {
            MsgBox("Не найден основной текст для обработки.", "FBE скрипт");
            window.external.EndUndoUnit(document);
            return;
        }
        
        // Флаг, что мы дошли до найденного узла и начинаем обработку
        var foundStartNode = false;
        var counters = {
            paragraphCount: 0
        };
        
        // Функция для обхода всех узлов от начала основного текста
        function processFromBeginning(currentBody) {
            var body = currentBody;
            
            while (body) {
                if (!shouldProcessBody(body)) {
                    body = body.nextSibling;
                    continue;
                }
                
                // Обрабатываем все section в этом body
                var section = body.firstChild;
                while (section) {
                    if (section.nodeName === "DIV" && section.className === "section") {
                        
                        // Проверяем, содержит ли эта секция наш стартовый узел
                        var containsStartNode = isAncestor(section, firstProcessableNode) || section === firstProcessableNode;
                        
                        // Если мы еще не нашли стартовый узел и эта секция не содержит его, пропускаем
                        if (!foundStartNode && !containsStartNode) {
                            section = section.nextSibling;
                            continue;
                        }
                        
                        // Если секция содержит стартовый узел, отмечаем что нашли
                        if (containsStartNode) {
                            foundStartNode = true;
                        }
                        
                        // Обрабатываем секцию
                        var sectionCounters = {
                            paragraphCount: counters.paragraphCount
                        };
                        
                        // Если это секция со стартовым узлом, начинаем с него
                        if (containsStartNode) {
                            // Начинаем с firstProcessableNode
                            var currentNode = firstProcessableNode;
                            var inSection = false;
                            
                            // Проверяем, что мы внутри этой секции
                            var checkNode = firstProcessableNode;
                            while (checkNode && checkNode !== body) {
                                if (checkNode === section) {
                                    inSection = true;
                                    break;
                                }
                                checkNode = checkNode.parentNode;
                            }
                            
                            if (inSection) {
                                // Обрабатываем от стартового узла до конца секции
                                while (currentNode) {
                                    // Проверяем, не вышли ли за пределы секции
                                    if (!isAncestor(section, currentNode) && currentNode !== section) {
                                        break;
                                    }
                                    
                                    if (isResetElement(currentNode)) {
                                        sectionCounters.paragraphCount = 0;
                                    }
                                    
                                    if (isParagraphToCount(currentNode)) {
                                        sectionCounters.paragraphCount++;
                                        
                                        if (sectionCounters.paragraphCount === N) {
                                            if (canInsertEmptyLineAfter(currentNode)) {
                                                var emptyLine = insertEmptyLineAfter(currentNode);
                                                if (emptyLine) {
                                                    totalAdded++;
                                                }
                                            }
                                            sectionCounters.paragraphCount = 0;
                                        }
                                    }
                                    
                                    if (currentNode.nodeName === "DIV") {
                                        var className = currentNode.className;
                                        
                                        var shouldProcessDiv = false;
                                        
                                        if (className === "section") {
                                            shouldProcessDiv = true;
                                        } else if ((className === "poem" || className === "stanza") && processPoems) {
                                            shouldProcessDiv = true;
                                        } else if (className === "cite" && processCites) {
                                            shouldProcessDiv = true;
                                        } else if (className === "epigraph" && processEpigraphs) {
                                            shouldProcessDiv = true;
                                        } else if (className === "title" && processTitles) {
                                            shouldProcessDiv = true;
                                        }
                                        
                                        if (shouldProcessDiv && currentNode.firstChild) {
                                            var childCounters = {
                                                paragraphCount: sectionCounters.paragraphCount
                                            };
                                            processNode(currentNode.firstChild, childCounters, null);
                                        }
                                    }
                                    
                                    currentNode = currentNode.nextSibling;
                                }
                            }
                        } else {
                            // Обычная обработка секции (после стартового узла)
                            processNode(section.firstChild, sectionCounters, null);
                        }
                        
                        // Сохраняем счетчик для следующей секции
                        counters.paragraphCount = sectionCounters.paragraphCount;
                    }
                    
                    section = section.nextSibling;
                }
                
                body = body.nextSibling;
            }
        }
        
        // Запускаем обработку от начала основного текста
        processFromBeginning(firstMainBody);
    }
    
    // Завершаем блок отмены действий
    window.external.EndUndoUnit(document);
    
    // Восстанавливаем выделение
    if (savedSelection) {
        try {
            savedSelection.select();
        } catch(e) {
            // Игнорируем ошибку восстановления выделения
        }
    }
    
    // Вычисляем время выполнения
    var endTime = new Date().getTime();
    var totalSeconds = (endTime - startTime) / 1000;
    var minutes = Math.floor(totalSeconds / 60);
    var seconds = Math.ceil((totalSeconds - minutes * 60) * 10) / 10;
    
    var timeStr;
    if (minutes > 0) {
        timeStr = minutes + " мин. " + seconds + " с";
    } else {
        timeStr = seconds + " с";
    }
    
    // Выводим результаты
    var message = 
        "FBE скрипт\n" +
        "---------------------------\n" +
        "Добавить пустые строки после каждых N абзацев\n" +
        "ver. " + versionNumber + "\n" +
        "----------------------------------------\n" +
        "ОБРАБОТКА ЗАВЕРШЕНА\n" +
        "----------------------------------------\n" +
        "Параметр N: " + N + "\n" +
        "Режим обработки: " + modeText + "\n" +
        "Добавлено пустых строк: " + totalAdded + "\n" +
        "Время работы: " + timeStr + "\n" +
        "----------------------------------------\n" +
        "По умолчанию НЕ обрабатываются:\n" +
        "Аннотация, история, заголовки, эпиграфы,\n" +
        "подзаголовки, цитаты, сноски и комментарии.\n\n" +
        "Заголовки, аннотации, эпиграфы, подзаголовки, цитаты,\n" +
        "стихи, иллюстрации СБРАСЫВАЮТ СЧЕТЧИК АБЗАЦЕВ.\n\n" +
        "Не вставляются пустые строки рядом с:\n" +
        "- аннотациями, заголовками, подзаголовками,\n" +
        "- эпиграфами, цитатами, стихами,\n" +
        "- авторами текста, секциями, иллюстрациями,\n" +
        "- любыми другими DIV элементами\n" +
        "- перед закрывающими DIV тегами";
    
    MsgBox(message, "FBE скрипт");
    
    // Выводим в статусную строку
    try {
        window.external.SetStatusBarText(
            "Добавлено пустых строк: " + totalAdded + 
            " (после каждых " + N + " абзацев). " +
            "Время: " + timeStr + ". Версия: " + versionNumber
        );
    } catch (e) {
        // Игнорируем ошибку, если статусная строка недоступна
    }
}
