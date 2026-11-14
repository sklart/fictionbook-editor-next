 // Стихи лесенкой v1.0;
 // создано при помощи DeepSeek.
 // идея и финальная правка — stokber (2025, декабрь)
function Run() {
    var scriptName = "Стихи лесенкой";
    var undoMsg = "Форматирование стихов лесенкой";
    var statusBarMsg = "Форматируем стихи лесенкой...";
    var version = "1.0";
    
    // Удаление пустых строк после оформления:
    // 0 - не удалять пустые строки
    // 1 - удалять пустые строки
    // 2 - запросить подтверждение на удаление пустых строк
    var delEmptyLine = 0; // Измените это значение по необходимости
    
    // Режим определения отступа:
    // 0 - по количеству символов (обычный режим)
    // 1 - по координате X окна документа (экспериментальный режим)
    var mode = 0; // Измените это значение по необходимости
    
    // коффициент отступа для mode = 0;
    // увеличьте коэффициент, если хотите уменьшить отступ, и наоборот, уменьшите, если хотите увеличить отступ.
    var coeff_0 = 1;
    // var coeff_0 = 0.63;

    // коффициент отступа для mode = 1;
    //Переводим пиксели в пробелы (примерно 10px на пробел)
    var coeff_1 = 10;
    // var coeff_1 = 5.1;
    
    // Показывать ли завершающее сообщение?
    var mBox = 0; // 1 - Показывать; 0 - Не показывать.

    
    try {
        window.external.BeginUndoUnit(document, undoMsg);
    } catch(e) {
        MsgBox("Ошибка начала операции отмены");
        return;
    }
    
    try {
        window.external.SetStatusBarText(statusBarMsg);
    } catch(e) {}
    
    // Получаем символ неразрывного пробела
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
    
    // Функция для проверки пустой строки
    function isLineEmpty(element) {
        if (!element || !element.innerHTML) return true;
        var html = element.innerHTML.replace(/<(?!img)[^>]*?>/gi, "");
        return /^( | |&nbsp;)*$/i.test(html);
    }
    
    // Функция для удаления пустых строк
    function removeEmptyLines(paragraphs) {
        var removedCount = 0;
        
        // Проходим по всем параграфам с конца, чтобы не нарушать порядок индексов
        for (var i = paragraphs.length - 1; i >= 0; i--) {
            var p = paragraphs[i];
            if (isLineEmpty(p)) {
                // Проверяем, не является ли элемент единственным в родителе
                if (p.parentNode && (p.nextSibling || p.previousSibling)) {
                    p.parentNode.removeChild(p);
                    removedCount++;
                }
            }
        }
        
        return removedCount;
    }
    
    // Функция для подсчета длины текста без учета тегов
    function getTextLengthWithoutTags(element) {
        if (!element || !element.innerHTML) return 0;
        var text = element.innerHTML.replace(/<[^>]*>/g, "");
        text = text.replace(/&nbsp;/g, " ");
        text = text.replace(/ /g, " ");
        return text.length;
    }
    
    // Функция для получения позиции конца текста в пикселях
    function getTextEndPosition(element) {
        if (!element) return 0;
        
        try {
            // Создаем текстовый диапазон для элемента
            var range = document.body.createTextRange();
            range.moveToElementText(element);
            
            // Получаем ширину текста
            var originalWidth = range.boundingWidth;
            
            // Если не удалось получить ширину, используем приблизительный расчет
            if (!originalWidth || originalWidth <= 0) {
                return getTextLengthWithoutTags(element) * 10; // Примерно 10px на символ
            }
            
            return originalWidth;
        } catch(e) {
            // В случае ошибки используем приблизительный расчет
            return getTextLengthWithoutTags(element) * 10;
        }
    }
    
    // Функция для получения родительского элемента P для любого элемента
    function getParentP(element) {
        var el = element;
        while (el && el.nodeName != "P" && el.nodeName != "BODY" && el.nodeName != "HTML") {
            el = el.parentNode;
        }
        if (el && el.nodeName == "P") {
            return el;
        }
        return null;
    }
    
    // Функция для получения следующего элемента P
    function getNextP(startEl) {
        var el = startEl;
        
        // Если startEl не является P, ищем родительский P
        if (el.nodeName != "P") {
            el = getParentP(el);
            if (!el) return null;
        }
        
        // Ищем следующий элемент после текущего P
        var nextEl = el.nextSibling;
        while (nextEl && nextEl.nodeName != "P") {
            if (nextEl.firstChild) {
                nextEl = nextEl.firstChild;
            } else if (nextEl.nextSibling) {
                nextEl = nextEl.nextSibling;
            } else {
                while (nextEl && !nextEl.nextSibling) {
                    nextEl = nextEl.parentNode;
                }
                if (nextEl) nextEl = nextEl.nextSibling;
            }
        }
        
        return nextEl;
    }
    
    // Функция для добавления неразрывных пробелов после тега <p>
    function addNBSpacesAfterP(pElement, spaces) {
        if (!pElement || spaces <= 0) return;
        
        // Создаем строку с неразрывными пробелами
        var nbspString = "";
        for (var i = 0; i < spaces; i++) {
            nbspString += nbspEntity;
        }
        
        // Создаем текстовый узел или элемент с &nbsp;
        var spaceNode;
        if (nbspEntity == "&nbsp;") {
            // Создаем временный элемент для преобразования &nbsp;
            var tempDiv = document.createElement("div");
            tempDiv.innerHTML = nbspString;
            spaceNode = tempDiv.firstChild;
        } else {
            // Используем символ напрямую
            spaceNode = document.createTextNode(nbspString);
        }
        
        // Ищем первый дочерний элемент или текстовый узел
        if (pElement.firstChild) {
            // Вставляем пробелы перед первым дочерним элементом
            pElement.insertBefore(spaceNode, pElement.firstChild);
        } else {
            // Если элемент пустой, добавляем пробелы как содержимое
            pElement.appendChild(spaceNode);
        }
    }
    
    var tr = document.selection.createRange();
    if (!tr) {
        window.external.EndUndoUnit(document);
        MsgBox("Нет выделения.\n\nПеред запуском скрипта нужно выделить абзацы, которые будут обработаны.");
        return;
    }
    
    if (tr.parentElement().nodeName == "TEXTAREA" || tr.parentElement().nodeName == "INPUT") {
        window.external.EndUndoUnit(document);
        MsgBox("Ошибка. Должно быть выделение в тексте книги, а не в поле ввода.");
        return;
    }
    
    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) {
        window.external.EndUndoUnit(document);
        MsgBox("Ошибка: не найден элемент fbw_body");
        return;
    }
    
    // Определяем начало и конец выделенного блока
    var trStart = document.selection.createRange();
    trStart.collapse(true);
    var startParent = trStart.parentElement();
    
    var trEnd = document.selection.createRange();
    trEnd.collapse(false);
    var endParent = trEnd.parentElement();
    
    // Получаем элементы P для начала и конца выделения
    var blockStartEl = getParentP(startParent);
    var blockEndEl = getParentP(endParent);
    
    if (!blockStartEl || !blockEndEl) {
        window.external.EndUndoUnit(document);
        MsgBox("Выделение должно находиться внутри абзацев <P>.");
        return;
    }
    
    // Находим все элементы P в выделении
    var paragraphs = [];
    var ptr = blockStartEl;
    
    // Собираем все элементы P в выделении
    while (ptr && fbwBody.contains(ptr) && ptr.nodeName == "P") {
        paragraphs.push(ptr);
        if (ptr === blockEndEl) break;
        
        // Ищем следующий элемент P
        var nextPtr = getNextP(ptr);
        if (!nextPtr || !fbwBody.contains(nextPtr)) break;
        ptr = nextPtr;
    }
    
    if (paragraphs.length === 0) {
        window.external.EndUndoUnit(document);
        MsgBox("В выделении не найдены абзацы для обработки.");
        return;
    }
    
    // Обрабатываем абзацы
    var processedCount = 0;
    var currentIndent = 0;
    
    // Массив для хранения обработанных параграфов (со ссылками на элементы)
    var processedParagraphs = [];
    
    for (var i = 0; i < paragraphs.length; i++) {
        var p = paragraphs[i];
        
        // Сохраняем ссылку на параграф
        processedParagraphs.push(p);
        
        // Пропускаем пустые строки и строки из одних пробелов
        if (isLineEmpty(p)) {
            currentIndent = 0; // Сбрасываем отступ для пустых строк
            continue;
        }
        
        // Получаем длину текста без учета возможных пробелов в начале
        var textLength = 0;
        if (mode === 0) {
            // Режим 0: по количеству символов
            textLength = getTextLengthWithoutTags(p) / coeff_0;
        } else if (mode === 1) {
            // Режим 1: по координате X (ширине текста в пикселях
            textLength = Math.round(getTextEndPosition(p) / coeff_1); 
            
        }
        
        // Добавляем отступ в начале абзаца
        if (currentIndent > 0) {
            addNBSpacesAfterP(p, currentIndent);
        }
        
        // Вычисляем отступ для следующей строки
        currentIndent += textLength;
        
        processedCount++;
    }
    
    // Удаление пустых строк после оформления
    var removedEmptyLines = 0;
    var shouldRemove = false;
    
    if (delEmptyLine === 1) {
        shouldRemove = true;
    } else if (delEmptyLine === 2) {
        // Подсчитываем количество пустых строк для запроса подтверждения
        var emptyCount = 0;
        for (var j = 0; j < processedParagraphs.length; j++) {
            if (isLineEmpty(processedParagraphs[j])) {
                emptyCount++;
            }
        }
        
        if (emptyCount > 0) {
            shouldRemove = confirm("Найдено " + emptyCount + " пустых строк.\nУдалить их?");
        }
    }
    
    if (shouldRemove) {
        removedEmptyLines = removeEmptyLines(processedParagraphs);
    }
    
    try {
        var statusMsg = "Обработано строк: " + processedCount;
        if (mode === 0) {
            statusMsg += " (режим: по символам)";
        } else if (mode === 1) {
            statusMsg += " (режим: по координате X)";
        }
        
        if (removedEmptyLines > 0) {
            statusMsg += ". Удалено пустых строк: " + removedEmptyLines;
        }
        window.external.SetStatusBarText(statusMsg);
    } catch(e) {}
    
    window.external.EndUndoUnit(document);
    
    var finalMsg = "Скрипт '" + scriptName + "' завершен.\nОбработано строк: " + processedCount;
    
    if (mode === 0) {
        finalMsg += "\nРежим: по количеству символов";
    } else if (mode === 1) {
        finalMsg += "\nРежим: по координате X окна документа";
    }
    
    if (removedEmptyLines > 0) {
        finalMsg += "\nУдалено пустых строк: " + removedEmptyLines;
    } else if (delEmptyLine === 1) {
        finalMsg += "\nПустые строки не найдены";
    }
    
    finalMsg += "\n\nИспользованы неразрывные пробелы (&nbsp;).";
    
    if(mBox ==1) {MsgBox(finalMsg);}
}
