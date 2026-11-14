// Скрипт "Заменить неразрывные пробелы в выделении" для редактора FBE
// version 2.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для замены неразрывных пробелов на обычные
// в выделенном фрагменте в fb2 документах.
// Поддержка отмены действий (Ctrl+Z).

// version 2.1, 21.12.2025
//======================================

function Run() {
    // Настройки (флаги)
    var showDialogs = 1; // 1 - показывать диалоги и статистику, 0 - только при ошибках
    var removeDoubleSpaces = 1; // 1 - удалять сдвоенные пробелы, 0 - оставлять как есть
    
    var undoMsg = "Заменить неразрывные пробелы в выделении";
    var statusBarMsg = "Заменяем неразрывные пробелы в выделении…";
    
    var totalReplaced = 0;
    var doubleSpacesRemoved = 0;
    
    // Имя тега для маркеров начала и конца выделения
    var markerTagName = "B";
    
    // Получаем символ неразрывного пробела из настроек FBE
    var nbspChar, nbspEntity;
    try { 
        nbspChar = window.external.GetNBSP(); 
        if (nbspChar.charCodeAt(0) == 160) {
            nbspEntity = "&nbsp;";
        } else {
            nbspEntity = nbspChar;
        }
    }
    catch(e) { 
        nbspChar = String.fromCharCode(160); 
        nbspEntity = "&nbsp;";
    }
    
    // Безопасная функция для создания regex из любого символа
    function createSafeRegexForChar(char) {
        if (!char) return '';
        
        // Если это одиночный символ
        if (char.length === 1) {
            var charCode = char.charCodeAt(0);
            // Для безопасных символов просто возвращаем символ
            var safePattern = /[.*+?^${}()|[\]\\]/g;
            if (safePattern.test(char)) {
                return '\\' + char;
            }
            return char;
        }
        
        // Для строк (HTML entities)
        return char.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    }
    
    // Собираем ВСЕ возможные варианты неразрывных пробелов
    var nbspPatterns = [];
    
    // 1. Стандартные варианты
    nbspPatterns.push(String.fromCharCode(160)); // &nbsp; как символ
    nbspPatterns.push("&nbsp;");                 // HTML entity
    nbspPatterns.push(" ");                     // Unicode в числовой форме
    
    // 2. Добавляем текущий символ из настроек FBE (если отличается)
    if (nbspChar && nbspChar != String.fromCharCode(160)) {
        nbspPatterns.push(nbspChar);
    }
    
    // 3. Добавляем entity из настроек (если отличается)
    if (nbspEntity && nbspEntity != "&nbsp;" && nbspEntity != nbspChar) {
        nbspPatterns.push(nbspEntity);
    }
    
    // 4. Другие возможные символы НП из FBE
    var otherNBSChars = [
        "□", // U+25A1 WHITE SQUARE
        "▫", // U+25AB WHITE SMALL SQUARE  
        "◦"  // U+25E6 WHITE BULLET
    ];
    
    for (var i = 0; i < otherNBSChars.length; i++) {
        nbspPatterns.push(otherNBSChars[i]);
    }
    
    // Удаляем дубликаты
    var uniquePatterns = [];
    for (var i = 0; i < nbspPatterns.length; i++) {
        var pattern = nbspPatterns[i];
        var isDuplicate = false;
        
        for (var j = 0; j < uniquePatterns.length; j++) {
            if (uniquePatterns[j] === pattern) {
                isDuplicate = true;
                break;
            }
        }
        
        if (!isDuplicate) {
            uniquePatterns.push(pattern);
        }
    }
    
    // Создаем безопасное регулярное выражение
    var nbspRegexStr = "";
    for (var i = 0; i < uniquePatterns.length; i++) {
        if (i > 0) nbspRegexStr += "|";
        nbspRegexStr += createSafeRegexForChar(uniquePatterns[i]);
    }
    
    var nbspRegex;
    try {
        nbspRegex = new RegExp("(" + nbspRegexStr + ")", "g");
    } catch(e) {
        // Если не удалось создать regex, используем альтернативный метод
        nbspRegex = null;
        if (showDialogs) {
            MsgBox("Внимание: используется альтернативный метод замены.\nСимвол НП: '" + nbspChar + "'", "FBE скрипт");
        }
    }
    
    // Проверяем наличие выделения
    var tr = document.selection.createRange();
    var errMsg = "Нет выделения.\n\nПеред запуском скрипта нужно выделить текст, который будет обработан.";
    
    if (!tr) {
        if (showDialogs) {
            MsgBox(errMsg, "FBE скрипт");
        }
        return;
    }
    
    if (tr.parentElement().nodeName == "TEXTAREA" || tr.parentElement().nodeName == "INPUT") {
        MsgBox("Ошибка: должно быть выделение в тексте книги, а не в поле ввода.", "FBE скрипт");
        return;
    }
    
    // Запрашиваем подтверждение при showDialogs = 1
    if (showDialogs) {
        var confirmMsg = "Заменить все неразрывные пробелы на обычные в выделении?\n\n" +
                         "Будут заменены все варианты неразрывных пробелов, включая настраиваемые в FBE.\n\n" +
                         "OK - выполнить замену\nОтмена - отменить выполнение скрипта";
        
        if (!confirm(confirmMsg)) {
            return;
        }
    }
    
    // Запускаем таймер после подтверждения
    var startTime = new Date();
    
    // Начинаем блок отмены
    window.external.BeginUndoUnit(document, undoMsg);
    
    try { 
        window.external.SetStatusBarText(statusBarMsg); 
    }
    catch(e) {}
    
    var body = document.getElementById("fbw_body");
    var coll = tr.getClientRects();
    var ttr1 = body.document.selection.createRange();
    var el = body.document.elementFromPoint(coll[0].left, coll[0].top);
    var cursorPos = null;
    
    // Если выделение пустое (курсор)
    if (tr.compareEndPoints("StartToEnd", tr) == 0) {
        var el2 = document.getElementById("CursorPosition");
        if (el2) el2.removeAttribute("id");
        ttr1.pasteHTML("<" + markerTagName + " id=CursorPosition></" + markerTagName + ">");
        cursorPos = document.getElementById("CursorPosition");
        ttr1.expand("word");
    }
    
    // Ставим маркеры блока в виде тегов с ID
    tr = ttr1.duplicate();
    tr.collapse();
    tr.pasteHTML("<" + markerTagName + " id=BlockStart></" + markerTagName + ">");
    tr = ttr1.duplicate();
    tr.collapse(false);
    tr.pasteHTML("<" + markerTagName + " id=BlockEnd></" + markerTagName + ">");
    
    // Поднимаемся вверх по дереву, пока не найдем DIV или P
    while (el && el.nodeName != "DIV" && el.nodeName != "P") { 
        el = el.parentNode; 
    }
    
    var InsideP = false; // true, если находимся внутри тега P
    var InsideSelection = false; // true, когда текущая позиция внутри выделенного текста
    var ProcessingEnded = false; // true, когда обработка закончена и пора выходить
    var ptr = el;
    
    var BlockStartNode = null;
    var BlockEndNode = null;
    
    // Обрабатываем DOM
    while (!ProcessingEnded) {
        // Если встретили тег P, меняем флаг
        if (ptr.nodeType == 1 && ptr.nodeName == "P") {
            InsideP = true;
        }
        
        // Если встретили маркер начала блока
        if (ptr.nodeType == 1 && ptr.nodeName == markerTagName && ptr.getAttribute("id") == "BlockStart") {
            InsideSelection = true;
            BlockStartNode = ptr;
        }
        
        // Если встретили маркер конца блока
        if (ptr.nodeType == 1 && ptr.nodeName == markerTagName && ptr.getAttribute("id") == "BlockEnd") {
            InsideSelection = false;
            ProcessingEnded = true;
            BlockEndNode = ptr;
        }
        
        // Если нашли текст и находимся внутри P и внутри выделения
        if (ptr.nodeType == 3 && InsideP && InsideSelection) {
            var originalText = ptr.nodeValue;
            var processedText = originalText;
            
            // Шаг 1: Заменяем неразрывные пробелы на обычные
            if (nbspRegex) {
                // Используем regex если он создан успешно
                processedText = originalText.replace(nbspRegex, " ");
                
                // Считаем количество замен
                if (processedText != originalText) {
                    var matches = originalText.match(nbspRegex);
                    if (matches) {
                        totalReplaced += matches.length;
                    }
                }
            } else {
                // Альтернативный метод без regex
                var newText = originalText;
                var replacedInNode = 0;
                
                for (var i = 0; i < uniquePatterns.length; i++) {
                    var pattern = uniquePatterns[i];
                    var index = -1;
                    
                    while ((index = newText.indexOf(pattern, index + 1)) !== -1) {
                        newText = newText.substring(0, index) + " " + newText.substring(index + pattern.length);
                        replacedInNode++;
                        // Корректируем индекс
                        index = index + 1 - pattern.length;
                    }
                }
                
                if (replacedInNode > 0) {
                    totalReplaced += replacedInNode;
                    processedText = newText;
                }
            }
            
            // Шаг 2: Удаляем сдвоенные пробелы (если включено)
            if (removeDoubleSpaces && processedText != originalText) {
                var beforeLength = processedText.length;
                
                // Удаляем двойные пробелы (2 и более подряд)
                // Сначала заменяем 3+ пробелов на 2 пробела, затем 2 пробела на 1
                while (processedText.indexOf("   ") !== -1) {
                    processedText = processedText.replace(/   /g, "  ");
                }
                processedText = processedText.replace(/  /g, " ");
                
                var afterLength = processedText.length;
                doubleSpacesRemoved += (beforeLength - afterLength);
            }
            
            // Применяем изменения, если текст изменился
            if (processedText != originalText) {
                ptr.nodeValue = processedText;
            }
        }
        
        // Находим следующий узел для обработки
        if (ptr.firstChild != null) {
            ptr = ptr.firstChild; // углубляемся
        } else {
            while (ptr.nextSibling == null) {
                ptr = ptr.parentNode; // поднимаемся
                // Поднявшись до элемента P, меняем флаг
                if (ptr && ptr.nodeType == 1 && ptr.nodeName == "P") {
                    InsideP = false;
                }
            }
            ptr = ptr.nextSibling; // переходим на соседний элемент
        }
    }
    
    // Восстанавливаем выделение
    var tr1 = document.body.createTextRange();
    if (!cursorPos) {
        tr1.moveToElementText(BlockStartNode);
        var tr2 = document.body.createTextRange();
        tr2.moveToElementText(BlockEndNode);
        tr1.setEndPoint("StartToStart", tr2);
        tr1.select();
    } else {
        tr1.moveToElementText(cursorPos);
        tr1.select();
    }
    
    // Удаляем маркеры блока
    if (BlockStartNode && BlockStartNode.parentNode) {
        BlockStartNode.parentNode.removeChild(BlockStartNode);
    }
    if (BlockEndNode && BlockEndNode.parentNode) {
        BlockEndNode.parentNode.removeChild(BlockEndNode);
    }
    if (cursorPos && cursorPos.parentNode) {
        cursorPos.parentNode.removeChild(cursorPos);
    }
    
    var endTime = new Date();
    var executionTime = (endTime - startTime) / 1000;
    
    // Выводим статистику при showDialogs = 1
    if (showDialogs) {
        if (totalReplaced > 0 || doubleSpacesRemoved > 0) {
            var statsMsg = "---------------------------------------\n" +
                           "Заменить неразрывные пробелы в выделении\n" +
                           "ver. 2.1\n" +
                           "---------------------------------------\n\n" +
                           "Заменено неразрывных пробелов: " + totalReplaced;
            
            if (removeDoubleSpaces && doubleSpacesRemoved > 0) {
                statsMsg += "\nУдалено лишних пробелов: " + doubleSpacesRemoved;
            }
            
            statsMsg += "\n\nВремя выполнения: " + executionTime.toFixed(2) + " с";
            
            MsgBox(statsMsg, "FBE скрипт");
        } else {
            MsgBox("В выделенном фрагменте не найдено неразрывных пробелов для замены.", "FBE скрипт");
        }
    }
    
    // Обновляем статусную строку
    if (totalReplaced > 0 || doubleSpacesRemoved > 0) {
        var statusMsg = "Заменено неразрывных пробелов: " + totalReplaced;
        if (removeDoubleSpaces && doubleSpacesRemoved > 0) {
            statusMsg += ", удалено лишних пробелов: " + doubleSpacesRemoved;
        }
        
        try { 
            window.external.SetStatusBarText(statusMsg + "."); 
        }
        catch(e) {}
    } else {
        try { 
            window.external.SetStatusBarText("Неразрывные пробелы в выделении не найдены."); 
        }
        catch(e) {}
    }
    
    // Завершаем блок отмены
    window.external.EndUndoUnit(document);
}
