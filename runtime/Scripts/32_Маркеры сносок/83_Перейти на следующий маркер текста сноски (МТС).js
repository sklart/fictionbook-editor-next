// Скрипт "Перейти на следующий маркер текста сноски (МТС)" для редактора FBE
// version 1.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для пошагового поиска и выделения возможных маркеров текстов сносок (МТС)
// в началах абзацев fb2 документа.
// Ищет маркеры: надстрочные цифры, *, #, [1], {1}, [~1~], {~1~}, прилипшие цифры.
// Пропускает абзацы целиком из * или #.
// Также исключаются из поиска заголовки, подзаголовки, стихи, эпиграфы.
// Скрипт находит ближайший к курсору вниз по тексту маркер, выделяет его и останавливается.
// Повторный запуск скрипта продолжает поиск от текущего положения курсора.

// version 1.1, 16.08.2026
//======================================

function Run() {
    var scriptName = "Перейти на следующий маркер текста сноски (МТС)";
    var version = "1.1";

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

    // Маркеры "прилипшая к тексту цифра" (1Текст):
    // 0 - не ищем, 1 - ищем, по умолчанию 1 - ищем
    var searchGluedDigit = 1;

    // Маркеры "цифра с пробелом" (1 Текст):
    // 0 - не ищем, 1 - ищем, по умолчанию 0 - не ищем (могут быть ложные срабатывания)
    var searchDigitWithSpace = 1;

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
    
    var tr, tr2, el, el2, ptr;
    var s, s_len, s1_len, s_html, s1_html_len;
    var foundPos, foundLen, foundMatch = false;
    var currentP = null;
    
    var removeTagsRE = new RegExp("<(?!IMG\\b).*?(>|$)", "ig");
    var removeTagsRE_ = "";
    
    // Функция проверки: не находится ли абзац внутри исключаемых тегов
    function isInExcludedTags(pElement) {
        var parent = pElement.parentNode;
        while (parent && parent != fbwBody) {
            if (parent.nodeName == "DIV") {
                var className = parent.className || "";
                if (className == "title" || className == "subtitle" || 
                    className == "stanza" || className == "poem" || 
                    className == "epigraph") {
                    return true;
                }
            }
            parent = parent.parentNode;
        }
        return false;
    }
    
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
    
    // Функция поиска SUP с цифрами в начале абзаца
    function findSupAtStart(el, startPos) {
        var supArray = [];
        findSupElements(el, supArray);
        
        for (var i = 0; i < supArray.length; i++) {
            var supEl = supArray[i];
            if (!isSupDigits(supEl)) continue;
            
            // Проверяем, что SUP в начале абзаца
            var supRange = document.body.createTextRange();
            supRange.moveToElementText(supEl);
            
            var elStartRange = document.body.createTextRange();
            elStartRange.moveToElementText(el);
            elStartRange.setEndPoint("EndToStart", supRange);
            
            var supPos = elStartRange.text.length;
            var supLen = (supEl.innerText || supEl.textContent || "").length;
            
            // Пропускаем если SUP до текущей позиции
            if (supPos + supLen <= startPos) continue;
            
            // До SUP только пробелы?
            var beforeText = elStartRange.text;
            if (!/^[\s\u00A0]*$/.test(beforeText)) continue;
            
            // После SUP: пробел + текст, или сразу текст
            var supEndRange = document.body.createTextRange();
            supEndRange.moveToElementText(supEl);
            supEndRange.collapse(false);
            
            var afterRange = document.body.createTextRange();
            afterRange.moveToElementText(el);
            afterRange.setEndPoint("StartToEnd", supEndRange);
            
            var afterText = afterRange.text;
            
            // После SUP не должно быть точки, скобки, двоеточия
            if (/^[.\:\)]/.test(afterText)) continue;
            
            return {pos: supPos, len: supLen};
        }
        
        return null;
    }
    
    // Функция поиска текстовых маркеров в начале абзаца
    function findTextMarkerAtStart(s, markerRe, startPos) {
        markerRe.lastIndex = 0;
        var rslt = markerRe.exec(s);
        
        if (!rslt) return null;
        
        var pos = rslt.index;
        var len = rslt[0].length;
        var markerChar = rslt[0].charAt(0);
        
        // Пропускаем если маркер до текущей позиции
        if (pos + len <= startPos) return null;
        
        // Маркер должен быть в начале (до него только пробелы)
        var before = s.substring(0, pos);
        if (!/^[\s\u00A0]*$/.test(before)) return null;
        
        // Для * и # проверяем: абзац не целиком из них
        if (markerChar == "*" || markerChar == "#") {
            var pFullText = s.replace(/[\s\u00A0]/g, "");
            if (/^[\*#]+$/.test(pFullText)) return null;
        }
        
        // После маркера не должно быть точки, скобки, двоеточия
        var afterMarker = s.charAt(pos + len);
        if (afterMarker == "." || afterMarker == ":" || afterMarker == ")") {
            return null;
        }
        
        return {pos: pos, len: len};
    }
    
    // Функция поиска прилипшей цифры в начале абзаца
    function findGluedDigitAtStart(s, startPos) {
        gluedDigitFindRe.lastIndex = 0;
        var rslt = gluedDigitFindRe.exec(s);
        
        while (rslt) {
            var digitPos = rslt.index;
            var digitLen = rslt[0].length;
            
            // Пропускаем если цифра до текущей позиции
            if (digitPos + digitLen <= startPos) {
                gluedDigitFindRe.lastIndex = digitPos + digitLen;
                rslt = gluedDigitFindRe.exec(s);
                continue;
            }
            
            // Цифра должна быть в начале (до неё только пробелы)
            var before = s.substring(0, digitPos);
            if (!/^[\s\u00A0]*$/.test(before)) {
                gluedDigitFindRe.lastIndex = digitPos + digitLen;
                rslt = gluedDigitFindRe.exec(s);
                continue;
            }
            
            // После цифры не должно быть точки, скобки, двоеточия
            var charAfter = s.charAt(digitPos + digitLen);
            if (charAfter == "." || charAfter == ":" || charAfter == ")") {
                gluedDigitFindRe.lastIndex = digitPos + digitLen;
                rslt = gluedDigitFindRe.exec(s);
                continue;
            }
            
            // Проверяем, что после цифры идёт текст (буква или знак)
            var afterDigit = s.substring(digitPos + digitLen);
            if (!/^[A-Za-zА-яЁё0-9—…,;.»"!?:%“™”©]/.test(afterDigit)) {
                gluedDigitFindRe.lastIndex = digitPos + digitLen;
                rslt = gluedDigitFindRe.exec(s);
                continue;
            }
            
            return {pos: digitPos, len: digitLen};
        }
        
        return null;
    }
    
    // Функция поиска цифры с пробелом в начале абзаца
    function findDigitWithSpaceAtStart(s, startPos) {
        var digitSpaceRe = /^[\s\u00A0]*([1-9]\d{0,3})[\s\u00A0]+[A-Za-zА-яЁё0-9—…,;.»"!?:%“™”©]/;
        var rslt = digitSpaceRe.exec(s);
        
        if (!rslt) return null;
        
        var digitPos = rslt.index + rslt[0].indexOf(rslt[1]);
        var digitLen = rslt[1].length;
        
        // Пропускаем если цифра до текущей позиции
        if (digitPos + digitLen <= startPos) return null;
        
        // После цифры и пробела(ов) должен быть текст
        var afterDigitSpace = s.substring(digitPos + digitLen);
        if (!/^[\s\u00A0]+[A-Za-zА-яЁё0-9—…,;.»"!?:%“™”©]/.test(afterDigitSpace)) {
            return null;
        }
        
        // Проверяем, что после цифры нет точки, скобки, двоеточия
        var charAfter = s.charAt(digitPos + digitLen);
        if (charAfter == "." || charAfter == ":" || charAfter == ")") {
            return null;
        }
        
        return {pos: digitPos, len: digitLen};
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
                
                // Проверяем: не в исключаемых тегах
                if (!isInExcludedTags(el)) {
                    
                    var minPos = -1;
                    var minLen = 0;
                    var minRe = -1;
                    
                    // Проверяем абзац: не целиком ли из * и #
                    var pFullText = el.innerText || el.textContent || "";
                    var isOnlySH = /^[\*#\s\u00A0]+$/.test(pFullText);
                    
                    // Поиск текстовых маркеров
                    for (var i = 0; i < regExpCnt; i++) {
                        var textMarker = findTextMarkerAtStart(s, regExps[i], s1_len);
                        
                        if (textMarker) {
                            var pos = textMarker.pos;
                            var len = textMarker.len;
                            var markerChar = s.charAt(pos);
                            
                            // Пропускаем * и # если абзац целиком из них
                            if ((markerChar == "*" || markerChar == "#") && isOnlySH) continue;
                            
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
                        var gluedMatch = findGluedDigitAtStart(s, s1_len);
                        
                        if (gluedMatch && (minPos == -1 || gluedMatch.pos < minPos)) {
                            minPos = gluedMatch.pos;
                            minLen = gluedMatch.len;
                            minRe = -3; // маркер "прилипшая цифра"
                        }
                    }
                    
                    // Поиск цифры с пробелом
                    if (searchDigitWithSpace == 1) {
                        var digitSpaceMatch = findDigitWithSpaceAtStart(s, s1_len);
                        
                        if (digitSpaceMatch && (minPos == -1 || digitSpaceMatch.pos < minPos)) {
                            minPos = digitSpaceMatch.pos;
                            minLen = digitSpaceMatch.len;
                            minRe = -4; // маркер "цифра с пробелом"
                        }
                    }
                    
                    // Поиск SUP с цифрами
                    if (searchSupDigits == 1) {
                        var supMatch = findSupAtStart(el, s1_len);
                        
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
    
    MsgBox(scriptName + "\nver. " + version + "\n---------------------------\n\nМаркеры текстов сносок (МТС) не найдены.");
    return "NotFound";
}