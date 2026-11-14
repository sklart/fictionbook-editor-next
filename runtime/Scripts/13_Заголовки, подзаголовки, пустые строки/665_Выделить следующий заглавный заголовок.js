// Скрипт "Выделить следующий заглавный заголовок" для редактора FBE
// version 1.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для поиска и выделения ЗАГЛАВНЫХ заголовков
// от местоположения курсора и дальше по тексту документа.
// Есть настройка % заглавности (30% будет означать - от 30 до 100%).

// Основано на серии скриптов Перейти на... (Выделить следующий заголовок).

// version 1.2, 20.01.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Выделить следующий заглавный заголовок";
    var version = "1.2";
    
    // ========== НАСТРОЙКИ СКРИПТА ==========
    // Можно менять значение (процент заглавных букв для поиска)
    var percentThreshold = 100; // 100% по умолчанию (полностью заглавные)
    
    // ========== КОНСТАНТЫ И ПЕРЕМЕННЫЕ ==========
    var blockElementClass = "title"; // класс блочного элемента, который нужно искать
    var undoMsg = "переход на следующий заглавный заголовок";
    var statusBarMsg = "Переходим на следующий заглавный заголовок…";
    
    // Сообщения для MsgBox
    var errorNoSelectionMsg = "Нет выделения.\n\nПеред запуском скрипта нужно выделить текст в книге.";
    var errorInputFieldMsg = "Ошибка. Должно быть выделение в тексте книги, а не в поле ввода.";
    var errorNoBodyMsg = "Ошибка: Не найден элемент fbw_body!";
    var notFoundMsg = "До конца документа заглавных заголовков не найдено.";
    var notFoundAnyMsg = "Во всем документе не найдено ни одного заглавного заголовка (>=%P% заглавных букв).";
    
    // Неразрывный пробел из настроек FBE
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
    
    // ========== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ==========
    
    // Функция для вывода сообщения об ошибке с правильным форматированием
    function showError(message) {
        MsgBox("FBE скрипт\n---------------------------\n\"" + scriptName + "\"\nver. " + version + "\n\n" + message);
    }
    
    // Функция для проверки, является ли символ пробельным
    function isWhitespace(ch) {
        return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch.charCodeAt(0) == 160;
    }
    
    // Функция для проверки, является ли символ буквой (кириллица или латиница)
    function isLetter(ch) {
        var code = ch.charCodeAt(0);
        return (code >= 1040 && code <= 1103) || // Кириллица
               (code == 1025 || code == 1105) || // Ёё
               (code >= 65 && code <= 90) ||     // Латинские A-Z
               (code >= 97 && code <= 122);      // Латинские a-z
    }
    
    // Функция для проверки, является ли символ заглавной буквой
    function isUpperCase(ch) {
        if (!isLetter(ch)) return false;
        var code = ch.charCodeAt(0);
        return (code >= 1040 && code <= 1071) || // Кириллица заглавные
               (code == 1025) ||                 // Ё заглавное
               (code >= 65 && code <= 90);       // Латинские заглавные
    }
    
    // Функция для расчета процента заглавных букв в тексте
    function calculateUpperCasePercent(text) {
        if (!text) return 0;
        
        var totalLetters = 0;
        var upperLetters = 0;
        
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            if (isLetter(ch)) {
                totalLetters++;
                if (isUpperCase(ch)) {
                    upperLetters++;
                }
            }
        }
        
        if (totalLetters == 0) return 0;
        return Math.round((upperLetters / totalLetters) * 100);
    }
    
    // Функция для получения чистого текста из элемента (без тегов)
    function getCleanText(element) {
        var text = "";
        if (element.nodeType == 3) { // Текстовый узел
            text = element.nodeValue;
        } else if (element.nodeType == 1) { // Элемент
            for (var i = 0; i < element.childNodes.length; i++) {
                text += getCleanText(element.childNodes[i]);
            }
        }
        return text;
    }
    
    // Функция из первого скрипта
    function checkP(elem1) {
        if (hasAmongParents(elem1, "title")) return true;
        return false;
    }
    
    // Функция из первого скрипта
    function hasAmongParents(elem2, nameOfClass) {
        while (elem2 && elem2.nodeName != "BODY") {
            if (elem2.nodeName == "DIV" && elem2.className == nameOfClass) return true;
            elem2 = elem2.parentNode;
        }
        return false;
    }
    
    // Функция из первого скрипта
    function getParentWithClass(elem3, nameOfClass) {
        while (elem3 && elem3.nodeName != "BODY") {
            if (elem3.nodeName == "DIV" && elem3.className == nameOfClass) return elem3;
            elem3 = elem3.parentNode;
        }
        return null;
    }
    
    // Функция из первого скрипта (немного модифицирована)
    function getNextNode(el) {
        if (el.firstChild && el.nodeName != "P")
            el = el.firstChild;
        else {
            while (el && !el.nextSibling)
                el = el.parentNode;
            if (el && el.nextSibling) el = el.nextSibling; 
        }
        return el;
    }
    
    // Функция из первого скрипта
    function getNextP(el) {
        var savedEl = el;
        while (el && (el.nodeName != "P" || el == savedEl))
            el = getNextNode(el);
        return el;
    }
    
    // Функция из первого скрипта для прокрутки
    function scrollIfItNeeds() { 
        var selection = document.selection;
        if (selection) {
            var range = selection.createRange();
            var rect = range.getBoundingClientRect();
            var correction = (rect.bottom - document.documentElement.clientHeight / 2);
            window.scrollBy(0, correction);
        }
    }
    
    // ========== ОСНОВНАЯ ЛОГИКА ==========
    
    var s;
    var tr, el, prv, pm, saveNext, saveFirstEmpty, nextPtr;
    
    // Проверяем выделение
    tr = document.selection.createRange();
    if (!tr) {
        showError(errorNoSelectionMsg);
        return;
    }
    
    // Проверяем, что выделение не в поле ввода
    if (tr.parentElement().nodeName == "TEXTAREA" || tr.parentElement().nodeName == "INPUT") {
        showError(errorInputFieldMsg);
        return;
    }
    
    // Начинаем блок отмены действий
    window.external.BeginUndoUnit(document, undoMsg);
    
    // Устанавливаем статус
    try { 
        window.external.SetStatusBarText(statusBarMsg); 
    } catch(e) {}
    
    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) {
        window.external.EndUndoUnit(document);
        showError(errorNoBodyMsg);
        return;
    }
    
    // Определяем текущую позицию
    var tr3 = document.selection.createRange();
    tr3.collapse(false);
    var ptr = tr3.parentElement();
    var blockElementAtStart = getParentWithClass(ptr, blockElementClass);
    
    // Ищем следующий абзац
    ptr = getNextP(ptr);
    
    // Флаг, указывающий был ли найден заглавный заголовок
    var foundCapitalHeader = false;
    var foundElement = null;
    
    // Ищем следующий заглавный заголовок
    while (ptr && fbwBody.contains(ptr)) {
        if (checkP(ptr) && getParentWithClass(ptr, blockElementClass) != blockElementAtStart) {
            var parentElem = getParentWithClass(ptr, blockElementClass);
            if (parentElem) {
                // Проверяем, является ли заголовок заглавным
                var text = getCleanText(parentElem);
                var percent = calculateUpperCasePercent(text);
                
                if (percent >= percentThreshold) {
                    foundCapitalHeader = true;
                    foundElement = parentElem;
                    break;
                }
            }
        }
        ptr = getNextP(ptr);
    }
    
    // Если не нашли до конца документа
    if (!foundCapitalHeader) {
        // Формируем сообщение для AskYesNo
        var restartMsg = notFoundMsg + "\n\nНачать поиск заново (с начала документа)?";
        
        // Используем AskYesNo (в FBE он сам добавляет правильный заголовок)
        var userResponse = AskYesNo(restartMsg);
        
        if (userResponse) {
            // Начинаем поиск с начала документа
            var allElements = [];
            findAllTitleElements(fbwBody, allElements);
            
            // Ищем первый заглавный заголовок
            for (var i = 0; i < allElements.length; i++) {
                var elem = allElements[i];
                var text = getCleanText(elem);
                var percent = calculateUpperCasePercent(text);
                
                if (percent >= percentThreshold) {
                    foundCapitalHeader = true;
                    foundElement = elem;
                    break;
                }
            }
            
            if (!foundCapitalHeader) {
                // Заменяем %P% на фактическое значение процента
                var finalMsg = notFoundAnyMsg.replace("%P%", percentThreshold);
                
                try { window.external.SetStatusBarText("ОК"); } catch(e) {} 
                window.external.EndUndoUnit(document);
                
                // Выводим сообщение об отсутствии заголовков
                showError(finalMsg);
                return;
            }
        } else {
            // Пользователь отказался от поиска с начала
            try { window.external.SetStatusBarText("ОК"); } catch(e) {} 
            window.external.EndUndoUnit(document);
            return; // Просто выходим без сообщений
        }
    }
    
    // Выделяем найденный заголовок
    if (foundElement) {
        var tr1 = document.body.createTextRange();
        tr1.moveToElementText(foundElement);
        
        // Корректируем выделение
        if (tr1.moveStart("character", 1) == 1)
            tr1.moveStart("character", -1);
        tr1.moveEnd("character", -1);
        tr1.select();
        
        // Прокручиваем к выделению
        scrollIfItNeeds();
    }
    
    // Завершаем блок отмены действий
    try { window.external.SetStatusBarText("ОК"); } catch(e) {} 
    window.external.EndUndoUnit(document);
    
    // ========== ДОПОЛНИТЕЛЬНЫЕ ФУНКЦИИ ==========
    
    // Рекурсивная функция для поиска ВСЕХ элементов title в DOM
    function findAllTitleElements(parent, results) {
        if (!parent || !parent.childNodes) return;
        
        for (var i = 0; i < parent.childNodes.length; i++) {
            var node = parent.childNodes[i];
            
            if (node.nodeType == 1) { // Элемент
                // Проверяем заголовки (div class="title")
                if (node.nodeName == "DIV" && node.className == "title") {
                    results.push(node);
                }
                
                // Рекурсивно ищем вложенные элементы
                findAllTitleElements(node, results);
            }
        }
    }
}
