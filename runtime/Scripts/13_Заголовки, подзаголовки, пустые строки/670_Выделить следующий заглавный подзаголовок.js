// Скрипт "Выделить следующий заглавный подзаголовок" для редактора FBE
// version 1.0
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для поиска и выделения ЗАГЛАВНЫХ подзаголовков
// от местоположения курсора и дальше по тексту документа.
// Есть настройка % заглавности (30% будет означать - от 30 до 100%).

// Основано на серии скриптов Перейти на... (Выделить следующий подзаголовок).

// version 1.0, 21.01.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Выделить следующий заглавный подзаголовок";
    var version = "1.0";
    
    // ========== НАСТРОЙКИ СКРИПТА ==========
    // Можно менять значение (процент заглавных букв для поиска)
    var percentThreshold = 100; // 100% по умолчанию (полностью заглавные)
    
    // ========== КОНСТАНТЫ И ПЕРЕМЕННЫЕ ==========
    var blockElementClass = "subtitle"; // класс элемента подзаголовка
    var undoMsg = "переход на следующий заглавный подзаголовок";
    var statusBarMsg = "Переходим на следующий заглавный подзаголовок…";
    
    // Сообщения для MsgBox
    var errorNoSelectionMsg = "Нет выделения.\n\nПеред запуском скрипта нужно выделить текст в книге.";
    var errorInputFieldMsg = "Ошибка. Должно быть выделение в тексте книги, а не в поле ввода.";
    var errorNoBodyMsg = "Ошибка: Не найден элемент fbw_body!";
    var notFoundMsg = "До конца документа заглавных подзаголовков не найдено.";
    var notFoundAnyMsg = "Во всем документе не найдено ни одного заглавного подзаголовка (>=%P% заглавных букв).";
    
    // Регулярное выражение для проверки что это реальный подзаголовок (есть текст)
    var subtitleRegExp = new RegExp("[A-Za-zА-яЁё|0-9](.*)");
    
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
    
    // Функция из скрипта для подзаголовков (проверка что это подзаголовок с текстом)
    function checkP(elem1) {
        if (elem1.className == "subtitle" && subtitleRegExp.test(elem1.innerHTML.replace(/<[^>]*?>/gi, ""))) return true;
        return false;
    }
    
    // Функция для проверки наличия класса среди родителей (оставлена для совместимости)
    function hasAmongParents(elem2, nameOfClass) {
        while (elem2 && elem2.nodeName != "BODY") {
            if (elem2.nodeName == "DIV" && elem2.className == nameOfClass) return true;
            elem2 = elem2.parentNode;
        }
        return false;
    }
    
    // Функция из скрипта для подзаголовков (немного модифицирована)
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
    
    // Функция из скрипта для подзаголовков
    function getNextP(el) {
        var savedEl = el;
        while (el && (el.nodeName != "P" || el == savedEl))
            el = getNextNode(el);
        return el;
    }
    
    // Функция из скрипта для подзаголовков для прокрутки
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
    
    // Ищем следующий абзац
    ptr = getNextP(ptr);
    
    // Флаг, указывающий был ли найден заглавный подзаголовок
    var foundCapitalSubtitle = false;
    var foundElement = null;
    
    // Ищем следующий заглавный подзаголовок
    while (ptr && fbwBody.contains(ptr)) {
        if (checkP(ptr)) {
            // Проверяем, является ли подзаголовок заглавным
            var text = getCleanText(ptr);
            var percent = calculateUpperCasePercent(text);
            
            if (percent >= percentThreshold) {
                foundCapitalSubtitle = true;
                foundElement = ptr;
                break;
            }
        }
        ptr = getNextP(ptr);
    }
    
    // Если не нашли до конца документа
    if (!foundCapitalSubtitle) {
        // Формируем сообщение для AskYesNo
        var restartMsg = notFoundMsg + "\n\nНачать поиск заново (с начала документа)?";
        
        // Используем AskYesNo (в FBE он сам добавляет правильный заголовок)
        var userResponse = AskYesNo(restartMsg);
        
        if (userResponse) {
            // Начинаем поиск с начала документа
            var allElements = [];
            findAllSubtitleElements(fbwBody, allElements);
            
            // Ищем первый заглавный подзаголовок
            for (var i = 0; i < allElements.length; i++) {
                var elem = allElements[i];
                var text = getCleanText(elem);
                var percent = calculateUpperCasePercent(text);
                
                if (percent >= percentThreshold) {
                    foundCapitalSubtitle = true;
                    foundElement = elem;
                    break;
                }
            }
            
            if (!foundCapitalSubtitle) {
                // Заменяем %P% на фактическое значение процента
                var finalMsg = notFoundAnyMsg.replace("%P%", percentThreshold);
                
                try { window.external.SetStatusBarText("ОК"); } catch(e) {} 
                window.external.EndUndoUnit(document);
                
                // Выводим сообщение об отсутствии подзаголовков
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
    
    // Выделяем найденный подзаголовок
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
    
    // Рекурсивная функция для поиска ВСЕХ элементов subtitle в DOM
    function findAllSubtitleElements(parent, results) {
        if (!parent || !parent.childNodes) return;
        
        for (var i = 0; i < parent.childNodes.length; i++) {
            var node = parent.childNodes[i];
            
            if (node.nodeType == 1) { // Элемент
                // Проверяем подзаголовки (p class="subtitle")
                if (node.nodeName == "P" && node.className == "subtitle") {
                    // Дополнительная проверка что это реальный подзаголовок с текстом
                    if (subtitleRegExp.test(node.innerHTML.replace(/<[^>]*?>/gi, ""))) {
                        results.push(node);
                    }
                }
                
                // Рекурсивно ищем вложенные элементы
                findAllSubtitleElements(node, results);
            }
        }
    }
}
