// Скрипт "Перейти на следующий маркер знака сноски (МЗС)" для редактора FBE
// version 3.5
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для пошагового поиска и выделения возможных маркеров знаков сносок (МЗС)
// внутри или в конце абзацев fb2 документа.
// Ищет маркеры: надстрочные цифры, *, #, [1], {1}, [~1~], {~1~}, прилипшие цифры.
// Пропускает маркеры в начале абзацев и абзацы целиком из * или #.
// Скрипт находит ближайший к курсору вниз по тексту маркер, выделяет его и останавливается.
// Повторный запуск скрипта продолжает поиск от текущего положения курсора.

// version 3.5, 16.08.2026
//======================================

function Run() {
    var scriptName = "Перейти на следующий маркер знака сноски (МЗС)";
    var version = "3.5";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    // Маркеры верхним индексом (SUP с цифрами):
    // 0 - не ищем, 1 - ищем, по умолчанию 1 - ищем
    var searchSupDigits = 1;

    // Маркеры звёздочки (*):
    // 0 - не ищем, 1 - ищем, по умолчанию 1 - ищем
    var searchStar = 1;

    // Маркеры решётки (#):
    // 0 - не ищем, 1 - ищем, по умолчанию 1 - ищем
    var searchHash = 1;

    // Маркеры в квадратных скобках [1]:
    // 0 - не ищем, 1 - ищем, по умолчанию 1 - ищем
    var searchSquareBracket = 1;

    // Маркеры в фигурных скобках {1}:
    // 0 - не ищем, 1 - ищем, по умолчанию 1 - ищем
    var searchCurlyBracket = 1;

    // Маркеры [~1~]:
    // 0 - не ищем, 1 - ищем, по умолчанию 1 - ищем
    var searchSquareTilde = 1;

    // Маркеры {~1~}:
    // 0 - не ищем, 1 - ищем, по умолчанию 1 - ищем
    var searchCurlyTilde = 1;

    // Маркеры "прилипшая к тексту цифра":
    // 0 - не ищем, 1 - ищем, по умолчанию 1 - ищем
    var searchGluedDigit = 1;

    // Пользовательский маркер № 1:
    // 0 - не ищем, 1 - ищем, по умолчанию 0 - не ищем
    var searchUserMarker1 = 0;
    var UserMarker_1 = "zzzz";      // Тут можно задать любой текстовый маркер

    // Пользовательский маркер № 2:
    // 0 - не ищем, 1 - ищем, по умолчанию 0 - не ищем
    var searchUserMarker2 = 0;
    var UserMarker_2 = "gggg";      // Тут можно задать любой текстовый маркер

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    var fbwBody = document.getElementById("fbw_body");
    var sel = document.selection;
    
    if (sel.type != "None" && sel.type != "Text") {
        MsgBox(scriptName + "\nver. " + version + "\n---------------------------\n\nНе обрабатываемый тип выделения: " + sel.type);
        return;
    }
    
    function scrollIfItNeeds() {
        var selection = document.selection;
        if (selection) {
            var range = selection.createRange();
            var rect = range.getBoundingClientRect();
            var correction = (rect.bottom - document.documentElement.clientHeight / 2);
            window.scrollBy(0, correction);
        }
    }

    // Функция экранирования спецсимволов regexp
    function escapeRegExp(str) {
        return str.replace(/([.*+?^=!:${}()|\[\]\/\\])/g, "\\$1");
    }

    // Формируем список регэкспов из настроек
    var regExps = [];
    if (searchSquareBracket == 1) regExps.push(/\[\d+\]/g);
    if (searchCurlyBracket == 1) regExps.push(/\{\d+\}/g);
    if (searchSquareTilde == 1) regExps.push(/\[~\d+~\]/g);
    if (searchCurlyTilde == 1) regExps.push(/\{~\d+~\}/g);
    if (searchStar == 1) regExps.push(/\*/g);
    if (searchHash == 1) regExps.push(/#/g);
    if (searchUserMarker1 == 1 && UserMarker_1.length > 0) {
        regExps.push(new RegExp(escapeRegExp(UserMarker_1), "g"));
    }
    if (searchUserMarker2 == 1 && UserMarker_2.length > 0) {
        regExps.push(new RegExp(escapeRegExp(UserMarker_2), "g"));
    }
    var regExpCnt = regExps.length;
    
    // Регэксп для поиска цифр (для "прилипшей цифры")
    var gluedDigitFindRe = /[1-9]\d{0,3}/g;
    
    // Символы, которые могут быть перед прилипшей цифрой
    var gluedDigitBeforeChars = "A-Za-zА-яЁё%…,;.»\"!?:%“™”©";
    
    var tr, tr2, el, el2, ptr;
    var s, s_len, s1_len, s_html, s1_html_len;
    var foundPos, foundLen, foundMatch = false;
    var currentP = null;
    
    var removeTagsRE = new RegExp("<(?!IMG\\b).*?(>|$)", "ig");
    var removeTagsRE_ = "";
    
    // Функция рекурсивного поиска SUP элементов внутри абзаца
    function findSupElements(pElement, supArray) {
        var children = pElement.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 1) {
                if (child.nodeName == "SUP") {
                    supArray.push(child);
                }
                // Рекурсивно ищем внутри вложенных элементов
                if (child.childNodes && child.childNodes.length > 0) {
                    findSupElements(child, supArray);
                }
            }
        }
    }
    
    // Функция проверки: содержит ли SUP только цифры
    function isSupDigits(supElement) {
        var text = supElement.innerText || supElement.textContent || "";
        if (text.length == 0) return false;
        return /^\d+$/.test(text);
    }
    
    // Функция поиска ближайшего SUP с цифрами
    function findSupMatch(el, startPos) {
        var supArray = [];
        findSupElements(el, supArray);
        
        var minSupPos = -1;
        var minSupElement = null;
        var minSupLen = 0;
        
        for (var i = 0; i < supArray.length; i++) {
            var supEl = supArray[i];
            if (!isSupDigits(supEl)) continue;
            
            // Создаем range до SUP элемента
            var supRange = document.body.createTextRange();
            supRange.moveToElementText(supEl);
            
            var elStartRange = document.body.createTextRange();
            elStartRange.moveToElementText(el);
            elStartRange.setEndPoint("EndToStart", supRange);
            
            var supPos = elStartRange.text.length;
            var supLen = (supEl.innerText || supEl.textContent || "").length;
            
            // Пропускаем если SUP до текущей позиции
            if (supPos + supLen <= startPos) continue;
            
            // Пропускаем если SUP в начале абзаца (до него только пробелы)
            var beforeText = elStartRange.text;
            if (/^[\s\u00A0]*$/.test(beforeText)) continue;
            
            // Выбираем ближайший
            if (minSupPos == -1 || supPos < minSupPos) {
                minSupPos = supPos;
                minSupElement = supEl;
                minSupLen = supLen;
            }
        }
        
        if (minSupPos != -1) {
            return {pos: minSupPos, len: minSupLen, element: minSupElement};
        }
        return null;
    }
    
    // Функция поиска прилипшей цифры
    function findGluedDigitMatch(s, startPos) {
        gluedDigitFindRe.lastIndex = startPos;
        var rslt = gluedDigitFindRe.exec(s);
        
        var minGluedPos = -1;
        var minGluedLen = 0;
        
        while (rslt) {
            var digitPos = rslt.index;
            var digitLen = rslt[0].length;
            
            // Проверяем символ перед цифрой
            var charBefore = "";
            if (digitPos > 0) {
                charBefore = s.charAt(digitPos - 1);
            }
            
            // Проверяем: символ перед цифрой должен быть буквой или знаком из списка
            var beforeOk = false;
            if (charBefore != "") {
                var beforeCheckRe = new RegExp("[" + gluedDigitBeforeChars + "]");
                if (beforeCheckRe.test(charBefore)) {
                    beforeOk = true;
                }
            }
            
            if (!beforeOk) {
                gluedDigitFindRe.lastIndex = digitPos + digitLen;
                rslt = gluedDigitFindRe.exec(s);
                continue;
            }
            
            // Проверяем символы перед цифрой: не должно быть точки/запятой с цифрой перед
            if (digitPos >= 2) {
                var twoBefore = s.charAt(digitPos - 2);
                if ((charBefore == "." || charBefore == ",") && /\d/.test(twoBefore)) {
                    // Это десятичная дробь, пропускаем
                    gluedDigitFindRe.lastIndex = digitPos + digitLen;
                    rslt = gluedDigitFindRe.exec(s);
                    continue;
                }
            }
            
            // Проверяем символ после цифры: не должно быть точки/запятой с цифрой
            var charAfter = "";
            if (digitPos + digitLen < s.length) {
                charAfter = s.charAt(digitPos + digitLen);
            }
            
            if ((charAfter == "." || charAfter == ",") && digitPos + digitLen + 1 < s.length) {
                var afterNext = s.charAt(digitPos + digitLen + 1);
                if (/\d/.test(afterNext)) {
                    // Это десятичная дробь, пропускаем
                    gluedDigitFindRe.lastIndex = digitPos + digitLen;
                    rslt = gluedDigitFindRe.exec(s);
                    continue;
                }
            }
            
            // Проверяем: не в начале ли абзаца (до цифры только пробелы)
            var beforeDigit = s.substring(0, digitPos);
            if (/^[\s\u00A0]*$/.test(beforeDigit)) {
                gluedDigitFindRe.lastIndex = digitPos + digitLen;
                rslt = gluedDigitFindRe.exec(s);
                continue;
            }
            
            // Подходит!
            if (minGluedPos == -1 || digitPos < minGluedPos) {
                minGluedPos = digitPos;
                minGluedLen = digitLen;
            }
            
            gluedDigitFindRe.lastIndex = digitPos + digitLen;
            rslt = gluedDigitFindRe.exec(s);
        }
        
        if (minGluedPos != -1) {
            return {pos: minGluedPos, len: minGluedLen};
        }
        return null;
    }
    
    function searchNext() {
        el = ptr;
        
        s = el.innerHTML.replace(removeTagsRE, removeTagsRE_);
        s = s.replace(/&nbsp;/g, "\u00A0");
        s = s.replace(/&amp;/g, "&");
        s = s.replace(/&lt;/g, "<");
        s = s.replace(/&gt;/g, ">");
        s_len = s.length;
        
        tr.moveToElementText(el);
        tr.move("character", s1_len);
        
        tr2 = tr.duplicate();
        tr2.moveToElementText(el);
        tr2.setEndPoint("EndToEnd", tr);
        s1_len = tr2.text.length;
        
        while (el && el != fbwBody) {
            if (el.nodeName == "P" && (s1_len < s_len || s_len == 0)) {
                
                // Проверяем абзац: не целиком ли из * и #
                var pFullText = el.innerText || el.textContent || "";
                var isOnlySH = /^[\*#\s\u00A0]+$/.test(pFullText);
                
                var minPos = -1;
                var minLen = 0;
                var minRe = -1;
                
                // Поиск текстовых маркеров
                for (var i = 0; i < regExpCnt; i++) {
                    regExps[i].lastIndex = s1_len;
                    var rslt = regExps[i].exec(s);
                    
                    if (rslt) {
                        var pos = rslt.index;
                        var len = rslt[0].length;
                        var markerChar = rslt[0].charAt(0);
                        
                        // Пропускаем * и # если абзац целиком из них
                        if ((markerChar == "*" || markerChar == "#") && isOnlySH) continue;
                        
                        // Проверяем: не в начале ли абзаца
                        var before = s.substring(0, pos);
                        if (/^[\s\u00A0]*$/.test(before)) continue;
                        
                        // Подходит!
                        if (minPos == -1 || pos < minPos) {
                            minPos = pos;
                            minLen = len;
                            minRe = i;
                        }
                    }
                }
                
                // Поиск прилипшей цифры
                if (searchGluedDigit == 1) {
                    var gluedMatch = findGluedDigitMatch(s, s1_len);
                    
                    if (gluedMatch && (minPos == -1 || gluedMatch.pos < minPos)) {
                        minPos = gluedMatch.pos;
                        minLen = gluedMatch.len;
                        minRe = -3; // маркер "прилипшая цифра"
                    }
                }
                
                // Поиск SUP с цифрами
                if (searchSupDigits == 1) {
                    var supMatch = findSupMatch(el, s1_len);
                    
                    // Сравниваем и выбираем ближайший
                    if (supMatch && (minPos == -1 || supMatch.pos < minPos)) {
                        minPos = supMatch.pos;
                        minLen = supMatch.len;
                        minRe = -2; // маркер SUP
                    }
                }
                
                if (minPos != -1) {
                    ptr = el;
                    foundPos = minPos;
                    foundLen = minLen;
                    s1_len = minPos + minLen;
                    foundMatch = true;
                    return false;
                }
            }
            
            // Переход к следующему P
            if (el.firstChild && el.nodeName != "P") {
                el = el.firstChild;
            } else {
                while (el && el.nextSibling == null) el = el.parentNode;
                if (el) el = el.nextSibling;
            }
            
            while (el && el != fbwBody && el.nodeName != "P") {
                if (el.firstChild && el.nodeName != "P") {
                    el = el.firstChild;
                } else {
                    while (el && el != fbwBody && el.nextSibling == null) el = el.parentNode;
                    if (el && el != fbwBody) el = el.nextSibling;
                }
            }
            
            if (el && el.nodeName == "P") {
                s = el.innerHTML.replace(removeTagsRE, removeTagsRE_);
                s = s.replace(/&nbsp;/g, "\u00A0");
                s = s.replace(/&amp;/g, "&");
                s = s.replace(/&lt;/g, "<");
                s = s.replace(/&gt;/g, ">");
                s1_len = 0;
                s_len = s.length;
            }
        }
        
        return true; // ничего не найдено
    }
    
    // Начальная позиция
    tr = sel.createRange();
    tr.collapse(false);
    el = tr.parentElement();
    el2 = el;
    
    while (el2 && el2.nodeName != "BODY" && el2.nodeName != "P") {
        el2 = el2.parentNode;
    }
    ptr = el2;
    
    if (el2 && el2.nodeName == "P") {
        tr2 = document.body.createTextRange();
        tr2.moveToElementText(el2);
        tr2.setEndPoint("EndToEnd", tr);
        s1_len = tr2.text.length;
    } else {
        ptr = fbwBody;
        s1_len = 0;
    }
    
    // Единый цикл поиска с защитой от зацикливания
    var searchResult = false;
    var prevPtr = null;
    var prevS1Len = -1;
    
    while (true) {
        searchResult = searchNext();
        
        if (searchResult) {
            // searchNext вернул true - ничего не найдено
            break;
        }
        
        if (foundMatch) {
            // Нашли маркер - выходим
            break;
        }
        
        // Защита от зацикливания
        if (ptr == prevPtr && s1_len == prevS1Len) {
            // Позиция не изменилась - ничего не найдено
            break;
        }
        
        prevPtr = ptr;
        prevS1Len = s1_len;
    }
    
    if (foundMatch) {
        tr = document.body.createTextRange();
        tr.moveToElementText(ptr);
        tr.move("character", foundPos);
        tr2 = tr.duplicate();
        tr2.move("character", foundLen);
        tr.setEndPoint("EndToStart", tr2);
        tr.select();
        scrollIfItNeeds();
        return "Found";
    }
    
    MsgBox(scriptName + "\nver. " + version + "\n---------------------------\n\nМаркеры знаков сносок (МЗС) не найдены.");
    return "NotFound";
}