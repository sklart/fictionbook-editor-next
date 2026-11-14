// Скрипт "Сделать заглавной каждую первую букву слова в заголовках (латиница)" для редактора FBE
// version 1.6
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для преобразования регистра слов, состоящих только из латиницы
// в размеченных заголовках (DIV class="title") fb2 документов.
// Каждое слово на латинице преобразуется: первая буква становится ЗАГЛАВНОЙ, остальные - строчными,
// с учетом устоявшихся исключений ("короткие слова" - союзы, предлоги, артикли).
// Аббревиатуры, римские цифры, кириллица и прочая НЕ-латиница не изменяются (остается как есть).
// По умолчанию обрабатывается только основной раздел документа, без сносок и комментариев.
// Настройка для обработки разделов сносок и комментариев.
// Если нет выделения - обрабатываются все заголовки в документе.
// Если есть выделение - обрабатываются только заголовки внутри выделения.
// Настройка для обработки заголовка основного body.
// Отдельные настройки для CAPS-заголовков и Normal-заголовков.
// Словари исключений (короткие слова, аббревиатуры, римские цифры) - только для латиницы.
// Поддерживаются языки: английский, немецкий, французский, испанский, итальянский, португальский,
// датский, норвежский, шведский, финский, исландский, польский, чешский, словацкий,
// венгерский, румынский, латышский, литовский и другие на латинице с диакритикой.
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.6, 04.04.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Сделать заглавной каждую первую букву слова в заголовках (латиница)";
    var version = "1.6";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима
    
    // Обрабатывать заголовки в разделе сносок (примечаний)
    var processNotesSection = 0; // 0 - нет, 1 - да
    
    // Обрабатывать заголовки в разделе комментариев
    var processCommentsSection = 0; // 0 - нет, 1 - да
    
    // Обрабатывать заголовок основного body
    var processFirstTitle = 0; // 0 - нет, 1 - да
    
    // Обрабатывать заголовки, где латинские слова написаны ЗАГЛАВНЫМИ (CAPS)
    var processCapitalTitles = 1; // 0 - нет, 1 - да
    
    // Обрабатывать заголовки, где латинские слова написаны в обычном регистре (Normal)
    var processNormalTitles = 1; // 0 - нет, 1 - да
    
    // Использовать словари исключений (некоторые слова не делаем с ЗАГЛАВНОЙ буквы)
    var UseExceptionsDictionary = 1; // 0 - нет, 1 - да
    
    // Обработка аббревиатур из списка (целиком все слово ЗАГЛАВНЫМИ буквами)
    var keepAbbreviations = 1; // 0 - преобразовывать, 1 - оставить как есть
    
    // Обработка римских цифр
    var keepRomanNumerals = 1; // 0 - преобразовывать, 1 - оставить как есть
    
    // Обработка коротких слов из списка (делать с маленькой буквы, если не в начале)
    var keepShortWords = 1; // 0 - делать с заглавной, 1 - делать с маленькой (кроме начала)
    
    // Имя тега для маркеров (при работе с выделением)
    var MyTagName = "B";
    
    // ========== РАСШИРЕННОЕ РЕГУЛЯРНОЕ ВЫРАЖЕНИЕ ДЛЯ ЛАТИНИЦЫ ==========
    // Поддерживает базовую латиницу + все буквы с диакритикой из Latin-1 Supplement,
    // Latin Extended-A и Latin Extended-B (диапазон \u00C0-\u024F)
    var latinLettersPattern = "a-zA-Z\u00C0-\u024F";
    
    // Диапазоны заглавных букв для определения регистра (из того же набора)
    var uppercasePattern = "A-Z\u00C0-\u00D6\u00D8-\u00DE\u0100\u0102\u0104\u0106\u0108\u010A\u010C\u010E\u0110\u0112\u0114\u0116\u0118\u011A\u011C\u011E\u0120\u0122\u0124\u0126\u0128\u012A\u012C\u012E\u0130\u0132\u0134\u0136\u0139\u013B\u013D\u013F\u0141\u0143\u0145\u0147\u014A\u014C\u014E\u0150\u0152\u0154\u0156\u0158\u015A\u015C\u015E\u0160\u0162\u0164\u0166\u0168\u016A\u016C\u016E\u0170\u0172\u0174\u0176\u0178\u0179\u017B\u017D\u01F1\u01F4\u01F7\u01FA\u01FC\u01FE\u0200\u0202\u0204\u0206\u0208\u020A\u020C\u020E\u0210\u0212\u0214\u0216\u0218\u021A\u021C\u021E\u0220\u0222\u0224\u0226\u0228\u022A\u022C\u022E\u0230\u0232\u023B\u023D\u023F\u0240\u0241\u0243\u0244\u0245\u0246\u0247\u0248\u0249\u024A\u024B\u024C\u024D\u024E";
    
    // Диапазоны строчных букв для определения регистра
    var lowercasePattern = "a-z\u00DF-\u00F6\u00F8-\u00FF\u0101\u0103\u0105\u0107\u0109\u010B\u010D\u010F\u0111\u0113\u0115\u0117\u0119\u011B\u011D\u011F\u0121\u0123\u0125\u0127\u0129\u012B\u012D\u012F\u0131\u0133\u0135\u0137\u0138\u013A\u013C\u013E\u0140\u0142\u0144\u0146\u0148\u0149\u014B\u014D\u014F\u0151\u0153\u0155\u0157\u0159\u015B\u015D\u015F\u0161\u0163\u0165\u0167\u0169\u016B\u016D\u016F\u0171\u0173\u0175\u0177\u017A\u017C\u017E\u017F\u01F2\u01F3\u01F5\u01F8\u01F9\u01FA\u01FB\u01FC\u01FD\u01FE\u01FF\u0201\u0203\u0205\u0207\u0209\u020B\u020D\u020F\u0211\u0213\u0215\u0217\u0219\u021B\u021D\u021F\u0221\u0223\u0225\u0227\u0229\u022B\u022D\u022F\u0231\u0233\u0234\u0235\u0236\u0237\u0238\u0239\u023C\u023E\u0242\u024F";
    
    // ========== СПИСКИ ДЛЯ ПРОВЕРКИ (ТОЛЬКО ЛАТИНИЦА) ==========
    
    var englishShortWords = "a|an|and|at|but|by|for|from|in|nor|of|on|or|the|to|with|without";
    var germanShortWords = "aber|an|auf|aus|bei|beim|bis|das|dass|dem|den|denn|der|die|durch|ein|eine|einem|einen|einer|eines|für|gegen|hinter|im|in|mit|nach|neben|ob|oder|ohne|seit|sondern|über|um|und|unter|von|vor|weil|wenn|zu|zum|zur|zwischen";
    
    var shortWordsArray = (englishShortWords + "|" + germanShortWords).split("|");
    
    var romanNumeralsArray = [
        "I","II","III","IV","V","VI","VII","VIII","IX","X",
        "XI","XII","XIII","XIV","XV","XVI","XVII","XVIII","XIX","XX",
        "XXI","XXII","XXIII","XXIV","XXV","XXVI","XXVII","XXVIII","XXIX","XXX",
        "XXXI","XXXII","XXXIII","XXXIV","XXXV","XXXVI","XXXVII","XXXVIII","XXXIX","XL",
        "XLI","XLII","XLIII","XLIV","XLV","XLVI","XLVII","XLVIII","XLIX","L",
        "LI","LII","LIII","LIV","LV","LVI","LVII","LVIII","LIX","LX",
        "LXI","LXII","LXIII","LXIV","LXV","LXVI","LXVII","LXVIII","LXIX","LXX",
        "LXXI","LXXII","LXXIII","LXXIV","LXXV","LXXVI","LXXVII","LXXVIII","LXXIX","LXXX",
        "LXXXI","LXXXII","LXXXIII","LXXXIV","LXXXV","LXXXVI","LXXXVII","LXXXVIII","LXXXIX","XC",
        "XCI","XCII","XCIII","XCIV","XCV","XCVI","XCVII","XCVIII","XCIX","C"
    ];
    
    var latinAbbreviations = [
        "BBC","BMW","CEO","CFO","CERN","CNN","COO","CTO","DOI","ESA","FIFA","GE","GM","GMC","HBO","HDMI","HP","IBM","ISBN","ISSN","KPI",
        "L","LAN","LCD","LED","LG","M","MTV","NASA","NATO","NBC","NEC","OLED","PC","R&D","S","SAP","TV","UAV","UN","UNESCO","USB","UEFA","VPN","VW","WAN","WHO","WI-FI","WTO","XL","XS","XXL","XXXL"
    ];
    
    var abbreviationsArray = [];
    for (var i = 0; i < latinAbbreviations.length; i++) {
        abbreviationsArray.push(latinAbbreviations[i]);
    }
    
    var nbspChar = String.fromCharCode(160);
    try {
        nbspChar = window.external.GetNBSP();
    } catch(e) {
        nbspChar = String.fromCharCode(160);
    }
    
    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================
    
    function isLetter(ch) {
        if (!ch || ch.length == 0) return false;
        var lettersRE = new RegExp("[" + latinLettersPattern + "]", "i");
        return lettersRE.test(ch);
    }
    
    function isUpperCase(ch) {
        if (!isLetter(ch)) return false;
        var upperRE = new RegExp("[" + uppercasePattern + "]");
        return upperRE.test(ch);
    }
    
    function isLowerCase(ch) {
        if (!isLetter(ch)) return false;
        var lowerRE = new RegExp("[" + lowercasePattern + "]");
        return lowerRE.test(ch);
    }
    
    function isWhitespace(ch) {
        return (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == nbspChar);
    }
    
    function isDash(ch) {
        return (ch == '-' || ch == '—' || ch == '–');
    }
    
    function isOpeningQuote(ch) {
        return (ch == '"' || ch == '«' || ch == '„' || ch == '“');
    }
    
    function cleanWordForCheck(word) {
        var start = 0;
        var end = word.length - 1;
        
        while (start <= end && !isLetter(word.charAt(start))) {
            start++;
        }
        
        while (end >= start && !isLetter(word.charAt(end))) {
            end--;
        }
        
        if (start > end) return "";
        return word.substring(start, end + 1);
    }
    
    function isRomanNumeral(word) {
        if (!word) return false;
        var cleanWord = cleanWordForCheck(word);
        if (!cleanWord) return false;
        
        var upperWord = cleanWord.toUpperCase();
        for (var i = 0; i < romanNumeralsArray.length; i++) {
            if (romanNumeralsArray[i] === upperWord) {
                return true;
            }
        }
        return false;
    }
    
    function isAbbreviation(word) {
        if (!word) return false;
        var cleanWord = cleanWordForCheck(word);
        if (!cleanWord) return false;
        
        var upperWord = cleanWord.toUpperCase();
        for (var i = 0; i < abbreviationsArray.length; i++) {
            if (abbreviationsArray[i] === upperWord) {
                return true;
            }
        }
        return false;
    }
    
    function isShortWord(word) {
        if (!word) return false;
        var cleanWord = cleanWordForCheck(word);
        if (!cleanWord) return false;
        
        var lowerWord = cleanWord.toLowerCase();
        for (var i = 0; i < shortWordsArray.length; i++) {
            if (shortWordsArray[i] === lowerWord) {
                return true;
            }
        }
        return false;
    }
    
    function isSentenceStart(text, offset) {
        if (offset <= 0) return true;
        
        var pos = offset - 1;
        
        while (pos >= 0 && isWhitespace(text.charAt(pos))) {
            pos--;
        }
        
        if (pos < 0) return true;
        
        var ch = text.charAt(pos);
        
        if (isDash(ch)) return true;
        if (isOpeningQuote(ch)) return true;
        if (ch == '.' || ch == '!' || ch == '?' || ch == '…') return true;
        
        return false;
    }
    
    function isAllLatin(word) {
        if (!word) return false;
        for (var i = 0; i < word.length; i++) {
            var ch = word.charAt(i);
            if (!isLetter(ch)) {
                return false;
            }
        }
        return true;
    }
    
    function isAllUppercaseLatin(word) {
        if (!word) return false;
        var hasLatinLetter = false;
        for (var i = 0; i < word.length; i++) {
            var ch = word.charAt(i);
            if (isLetter(ch)) {
                hasLatinLetter = true;
                if (isLowerCase(ch)) {
                    return false;
                }
            }
        }
        return hasLatinLetter;
    }
    
    // Функция обработки слова (без апострофа, это делаем отдельно)
    function processWord(full_match, offset_of_match, string_we_search_in) {
        if (lastSymbolLetter && offset_of_match == 0) {
            lastSymbolLetter = false;
            return full_match.toLowerCase();
        } else {
            lastSymbolLetter = false;
            
            // Проверяем исключения
            var wordIsRoman = false;
            var wordIsAbbrev = false;
            var wordIsShort = false;
            
            if (UseExceptionsDictionary == 1) {
                wordIsRoman = isRomanNumeral(full_match);
                wordIsAbbrev = isAbbreviation(full_match);
                wordIsShort = isShortWord(full_match);
            }
            
            if ((wordIsRoman && keepRomanNumerals == 1) || (wordIsAbbrev && keepAbbreviations == 1)) {
                return full_match;
            }
            
            if (wordIsShort && keepShortWords == 1) {
                if (isSentenceStart(string_we_search_in, offset_of_match)) {
                    return full_match.substr(0, 1).toUpperCase() + full_match.substr(1).toLowerCase();
                } else {
                    return full_match.toLowerCase();
                }
            }
            
            // Обычное слово: первая буква заглавная, остальные строчные
            return full_match.substr(0, 1).toUpperCase() + full_match.substr(1).toLowerCase();
        }
    }
    
    // Функция для исправления апострофов + S в тексте
    function fixApostropheS(text) {
        if (!text) return text;
        
        var pattern = new RegExp("([" + latinLettersPattern + "])'S([\\s\\" + nbspChar + "!\\?\\.,;:…\\)\\]”»]|$)", "gi");
        
        var result = text.replace(pattern, function(match, letter, after) {
            return letter + "'s" + after;
        });
        
        return result;
    }
    
    // Рекурсивная функция для исправления апострофов в элементе
    function fixApostropheSInElement(element) {
        var changed = false;
        
        if (element.nodeType == 3) {
            var oldText = element.nodeValue;
            var newText = fixApostropheS(oldText);
            if (oldText != newText) {
                element.nodeValue = newText;
                changed = true;
            }
        } else if (element.nodeType == 1) {
            for (var i = 0; i < element.childNodes.length; i++) {
                if (fixApostropheSInElement(element.childNodes[i])) {
                    changed = true;
                }
            }
        }
        
        return changed;
    }
    
    var lastSymbolLetter = false;
    
    // ==================================================
    // ФУНКЦИИ ДЛЯ РАБОТЫ С ЗАГОЛОВКАМИ
    // ==================================================
    
    function getCleanTextArray(element) {
        var paragraphs = [];
        var currentText = "";
        
        function collectText(node) {
            if (node.nodeType == 3) {
                currentText += node.nodeValue;
            } else if (node.nodeType == 1) {
                if (node.nodeName == "P") {
                    if (currentText) {
                        paragraphs.push(currentText);
                        currentText = "";
                    }
                }
                for (var i = 0; i < node.childNodes.length; i++) {
                    collectText(node.childNodes[i]);
                }
            }
        }
        
        collectText(element);
        
        if (currentText) {
            paragraphs.push(currentText);
        }
        
        if (paragraphs.length === 0) {
            paragraphs.push(getCleanTextSimple(element));
        }
        
        return paragraphs;
    }
    
    function getCleanTextSimple(element) {
        var text = "";
        if (element.nodeType == 3) {
            text = element.nodeValue;
        } else if (element.nodeType == 1) {
            for (var i = 0; i < element.childNodes.length; i++) {
                text += getCleanTextSimple(element.childNodes[i]);
            }
        }
        return text;
    }
    
    function hasLatinLetters(text) {
        var latinRE = new RegExp("[" + latinLettersPattern + "]", "i");
        return latinRE.test(text);
    }
    
    function getTitleLatinType(element) {
        var text = getCleanTextSimple(element);
        if (!hasLatinLetters(text)) {
            return "none";
        }
        
        var words = [];
        var currentWord = "";
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            if (isLetter(ch)) {
                currentWord += ch;
            } else {
                if (currentWord.length > 0) {
                    words.push(currentWord);
                    currentWord = "";
                }
            }
        }
        if (currentWord.length > 0) {
            words.push(currentWord);
        }
        
        var hasLatinWord = false;
        var allCaps = true;
        
        for (var i = 0; i < words.length; i++) {
            var word = words[i];
            if (isAllLatin(word)) {
                hasLatinWord = true;
                if (!isAllUppercaseLatin(word)) {
                    allCaps = false;
                }
            }
        }
        
        if (!hasLatinWord) return "none";
        if (allCaps) return "caps";
        return "normal";
    }
    
    function replaceTextInElementRecursive(element) {
        var changed = false;
        
        if (element.nodeType == 3) {
            var oldText = element.nodeValue;
            var latinRE = new RegExp("[" + latinLettersPattern + "]", "i");
            if (!latinRE.test(oldText)) {
                return false;
            }
            
            var newText = "";
            var i = 0;
            lastSymbolLetter = false;
            
            while (i < oldText.length) {
                var ch = oldText.charAt(i);
                if (isLetter(ch)) {
                    var start = i;
                    while (i < oldText.length && isLetter(oldText.charAt(i))) {
                        i++;
                    }
                    var word = oldText.substring(start, i);
                    var processedWord = processWord(word, start, oldText);
                    newText += processedWord;
                    if (processedWord.length > 0) {
                        var lastChar = processedWord.charAt(processedWord.length - 1);
                        if (isLetter(lastChar)) {
                            lastSymbolLetter = true;
                        } else {
                            lastSymbolLetter = false;
                        }
                    }
                } else {
                    newText += ch;
                    i++;
                }
            }
            
            if (oldText != newText) {
                element.nodeValue = newText;
                changed = true;
            }
        } else if (element.nodeType == 1) {
            for (var i = 0; i < element.childNodes.length; i++) {
                if (replaceTextInElementRecursive(element.childNodes[i])) {
                    changed = true;
                }
            }
        }
        
        return changed;
    }
    
    // ==================================================
    // ФУНКЦИИ ПОИСКА ЗАГОЛОВКОВ
    // ==================================================
    
    function getBodyTypeFromElement(element) {
        var parentBody = element;
        while (parentBody && (parentBody.nodeName != "DIV" || parentBody.className != "body")) {
            parentBody = parentBody.parentNode;
        }
        if (parentBody) {
            var fbname = parentBody.getAttribute("fbname") || "";
            if (fbname == "") return "main";
            if (fbname == "notes") return "notes";
            if (fbname == "comments") return "comments";
            return "other";
        }
        return "main";
    }
    
    var mainBodyTitleElement = null;
    
    function findBodyTitles(parent, results) {
        if (!parent || !parent.childNodes) return;
        
        for (var i = 0; i < parent.childNodes.length; i++) {
            var node = parent.childNodes[i];
            
            if (node.nodeType == 1 && node.nodeName == "DIV" && node.className == "body") {
                var fbname = node.getAttribute("fbname") || "";
                var bodyType = (fbname == "" ? "main" : (fbname == "notes" ? "notes" : (fbname == "comments" ? "comments" : "other")));
                
                var bodyChildren = node.childNodes;
                for (var j = 0; j < bodyChildren.length; j++) {
                    var child = bodyChildren[j];
                    if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "title") {
                        var titleInfo = {
                            element: child,
                            type: "title",
                            isBodyTitle: true,
                            bodyType: bodyType,
                            fbname: fbname
                        };
                        results.push(titleInfo);
                        
                        if (bodyType == "main" && mainBodyTitleElement == null) {
                            mainBodyTitleElement = titleInfo;
                        }
                        break;
                    }
                }
                
                findFilteredTitles(node, results);
            }
        }
    }
    
    function findFilteredTitles(parent, results) {
        if (!parent || !parent.childNodes) return;
        
        for (var i = 0; i < parent.childNodes.length; i++) {
            var node = parent.childNodes[i];
            
            if (node.nodeType == 1) {
                if (node.nodeName == "DIV" && node.className == "title") {
                    var parentNode = node.parentNode;
                    if (parentNode && parentNode.nodeName == "DIV" && parentNode.className == "body") {
                        continue;
                    }
                }
                
                if (node.nodeName == "DIV" && node.className == "title") {
                    var bodyType = getBodyTypeFromElement(node);
                    results.push({
                        element: node,
                        type: "title",
                        isBodyTitle: false,
                        bodyType: bodyType,
                        fbname: (bodyType == "main" ? "" : bodyType)
                    });
                }
                
                findFilteredTitles(node, results);
            }
        }
    }
    
    function isElementInSelection(element, selectionRange) {
        if (!element) return false;
        
        var elementRange = document.body.createTextRange();
        try {
            elementRange.moveToElementText(element);
        } catch(e) {
            return false;
        }
        
        var startToStart = elementRange.compareEndPoints("StartToStart", selectionRange);
        var endToEnd = elementRange.compareEndPoints("EndToEnd", selectionRange);
        var startToEnd = elementRange.compareEndPoints("StartToEnd", selectionRange);
        var endToStart = elementRange.compareEndPoints("EndToStart", selectionRange);
        
        if (startToStart >= 0 && endToEnd <= 0) {
            return true;
        }
        
        if ((startToStart <= 0 && endToStart >= 0) || (startToEnd <= 0 && endToEnd >= 0)) {
            return true;
        }
        
        if (startToStart <= 0 && endToEnd >= 0) {
            return true;
        }
        
        return false;
    }
    
    function getTitlesInSelection(selectionRange, allTitles) {
        var titlesInSelection = [];
        for (var i = 0; i < allTitles.length; i++) {
            if (isElementInSelection(allTitles[i].element, selectionRange)) {
                titlesInSelection.push(allTitles[i]);
            }
        }
        return titlesInSelection;
    }
    
    // ==================================================
    // ОСНОВНАЯ ЛОГИКА
    // ==================================================
    
    var body = document.getElementById("fbw_body");
    if (!body) {
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка: Не найден элемент body!");
        return;
    }
    
    mainBodyTitleElement = null;
    
    var allTitleElements = [];
    
    findBodyTitles(body, allTitleElements);
    
    var otherTitles = [];
    findFilteredTitles(body, otherTitles);
    for (var i = 0; i < otherTitles.length; i++) {
        allTitleElements.push(otherTitles[i]);
    }
    
    var uniqueTitles = [];
    var seenElements = {};
    for (var i = 0; i < allTitleElements.length; i++) {
        var item = allTitleElements[i];
        var elementId = item.element.uniqueID || item.element.sourceIndex || i;
        if (!seenElements[elementId]) {
            seenElements[elementId] = true;
            uniqueTitles.push(item);
        }
    }
    allTitleElements = uniqueTitles;
    
    var statsMain = { total: 0, caps: 0, normal: 0, capsChanged: 0, normalChanged: 0 };
    var statsMainBody = { total: 0, caps: 0, normal: 0, capsChanged: 0, normalChanged: 0 };
    var statsNotes = { total: 0, caps: 0, normal: 0, capsChanged: 0, normalChanged: 0 };
    var statsComments = { total: 0, caps: 0, normal: 0, capsChanged: 0, normalChanged: 0 };
    var statsOther = { total: 0, caps: 0, normal: 0, capsChanged: 0, normalChanged: 0 };
    
    for (var i = 0; i < allTitleElements.length; i++) {
        var item = allTitleElements[i];
        var bodyType = item.bodyType;
        var titleType = getTitleLatinType(item.element);
        
        if (item.isBodyTitle && item.bodyType == "main") {
            statsMainBody.total++;
            if (titleType == "caps") statsMainBody.caps++;
            if (titleType == "normal") statsMainBody.normal++;
        } else if (bodyType == "main") {
            statsMain.total++;
            if (titleType == "caps") statsMain.caps++;
            if (titleType == "normal") statsMain.normal++;
        } else if (bodyType == "notes") {
            statsNotes.total++;
            if (titleType == "caps") statsNotes.caps++;
            if (titleType == "normal") statsNotes.normal++;
        } else if (bodyType == "comments") {
            statsComments.total++;
            if (titleType == "caps") statsComments.caps++;
            if (titleType == "normal") statsComments.normal++;
        } else {
            statsOther.total++;
            if (titleType == "caps") statsOther.caps++;
            if (titleType == "normal") statsOther.normal++;
        }
    }
    
    var hasSelection = false;
    var selectionRange = null;
    var titlesToProcess = [];
    
    var sel = document.selection;
    if (sel) {
        try {
            var range = sel.createRange();
            if (range && range.compareEndPoints("StartToEnd", range) != 0) {
                hasSelection = true;
                selectionRange = range;
            }
        } catch(e) {}
    }
    
    if (hasSelection) {
        titlesToProcess = getTitlesInSelection(selectionRange, allTitleElements);
    } else {
        if (processFirstTitle == 1 && mainBodyTitleElement) {
            titlesToProcess.push(mainBodyTitleElement);
        }
        
        for (var i = 0; i < allTitleElements.length; i++) {
            var item = allTitleElements[i];
            var shouldInclude = true;
            
            if (item.isBodyTitle && item.bodyType == "main") {
                continue;
            }
            
            if (item.bodyType == "notes" && processNotesSection == 0) {
                shouldInclude = false;
            }
            if (item.bodyType == "comments" && processCommentsSection == 0) {
                shouldInclude = false;
            }
            
            if (shouldInclude) {
                titlesToProcess.push(item);
            }
        }
    }
    
    var finalTitlesToProcess = [];
    var statsProcess = { caps: 0, normal: 0 };
    var statsProcessMainBody = { caps: 0, normal: 0 };
    
    for (var i = 0; i < titlesToProcess.length; i++) {
        var item = titlesToProcess[i];
        var titleType = getTitleLatinType(item.element);
        var isMainBody = (item.isBodyTitle && item.bodyType == "main");
        
        if (titleType == "caps" && processCapitalTitles == 1) {
            finalTitlesToProcess.push(item);
            if (isMainBody) {
                statsProcessMainBody.caps++;
            } else {
                statsProcess.caps++;
            }
        } else if (titleType == "normal" && processNormalTitles == 1) {
            finalTitlesToProcess.push(item);
            if (isMainBody) {
                statsProcessMainBody.normal++;
            } else {
                statsProcess.normal++;
            }
        }
    }
    
    if (finalTitlesToProcess.length === 0) {
        if (showStatistics == 1) {
            var msg = scriptName + "\nver. " + version + "\n\n";
            if (hasSelection) {
                msg += "В выделении не найдено заголовков с латинскими словами для обработки.";
            } else {
                msg += "Заголовков с латинскими словами для обработки не найдено.";
            }
            MsgBox(msg);
        }
        return;
    }
    
    if (showStatistics == 1) {
        var confirmMsg = scriptName + "\nver. " + version + "\n\n";
        
        if (hasSelection) {
            confirmMsg += "ВЫДЕЛЕНИЕ ОБНАРУЖЕНО\n";
            confirmMsg += "=========================\n";
            confirmMsg += "Заголовков в выделении: " + titlesToProcess.length + "\n";
            confirmMsg += "Из них с латинскими словами (CAPS): " + (statsProcess.caps + statsProcessMainBody.caps) + "\n";
            confirmMsg += "Из них с латинскими словами (Normal): " + (statsProcess.normal + statsProcessMainBody.normal) + "\n";
        } else {
            confirmMsg += "АНАЛИЗ ДОКУМЕНТА\n";
            confirmMsg += "=========================\n";
            confirmMsg += "Всего заголовков в документе: " + allTitleElements.length + "\n";
            confirmMsg += "Заголовков в основном разделе: " + (statsMain.total + statsMainBody.total) + "\n";
            if (statsMainBody.total > 0) {
                confirmMsg += "  • Из них основной заголовок body: " + statsMainBody.total + "\n";
            }
            confirmMsg += "Заголовков в разделе сносок (примечаний): " + statsNotes.total + "\n";
            confirmMsg += "Заголовков в разделе комментариев: " + statsComments.total + "\n";
        }
        
        confirmMsg += "=========================\n\n";
        confirmMsg += "БУДУТ ОБРАБОТАНЫ (согласно настройкам):\n";
        if (statsProcessMainBody.caps > 0 || statsProcessMainBody.normal > 0) {
            confirmMsg += "• Основной заголовок body: ";
            if (statsProcessMainBody.caps > 0) confirmMsg += "CAPS: " + statsProcessMainBody.caps + " ";
            if (statsProcessMainBody.normal > 0) confirmMsg += "Normal: " + statsProcessMainBody.normal;
            confirmMsg += "\n";
        }
        if (statsProcess.caps > 0) {
            confirmMsg += "• Остальные CAPS-заголовков: " + statsProcess.caps + "\n";
        }
        if (statsProcess.normal > 0) {
            confirmMsg += "• Остальные Normal-заголовков: " + statsProcess.normal + "\n";
        }
        confirmMsg += "=========================\n\n";
        confirmMsg += "Преобразование: каждое латинское слово → первая буква ЗАГЛАВНАЯ, остальные строчные\n";
        confirmMsg += "Кириллица не изменяется.\n\n";
        confirmMsg += "Продолжить?";
        
        var confirm = AskYesNo(confirmMsg);
        if (!confirm) {
            MsgBox(scriptName + "\nver. " + version + "\n\nОбработка отменена пользователем.");
            return;
        }
    }
    
    var Ts = new Date().getTime();
    window.external.BeginUndoUnit(document, scriptName);
    
    var changedCapsMain = 0, changedNormalMain = 0;
    var changedCapsMainBody = 0, changedNormalMainBody = 0;
    var changedCapsNotes = 0, changedNormalNotes = 0;
    var changedCapsComments = 0, changedNormalComments = 0;
    
    for (var i = 0; i < finalTitlesToProcess.length; i++) {
        var item = finalTitlesToProcess[i];
        var titleType = getTitleLatinType(item.element);
        var bodyType = item.bodyType;
        var isMainBody = (item.isBodyTitle && item.bodyType == "main");
        
        var changed = replaceTextInElementRecursive(item.element);
        
        if (changed) {
            if (isMainBody) {
                if (titleType == "caps") changedCapsMainBody++;
                if (titleType == "normal") changedNormalMainBody++;
            } else if (bodyType == "main") {
                if (titleType == "caps") changedCapsMain++;
                if (titleType == "normal") changedNormalMain++;
            } else if (bodyType == "notes") {
                if (titleType == "caps") changedCapsNotes++;
                if (titleType == "normal") changedNormalNotes++;
            } else if (bodyType == "comments") {
                if (titleType == "caps") changedCapsComments++;
                if (titleType == "normal") changedNormalComments++;
            }
        }
    }
    
    for (var i = 0; i < finalTitlesToProcess.length; i++) {
        var item = finalTitlesToProcess[i];
        fixApostropheSInElement(item.element);
    }
    
    window.external.EndUndoUnit(document);
    
    if (showStatistics == 1) {
        var Tf = new Date().getTime();
        var timeSec = Math.round((Tf - Ts) / 10) / 100;
        
        var resultMsg = scriptName + "\nver. " + version + "\n\n";
        resultMsg += "РЕЗУЛЬТАТЫ ОБРАБОТКИ:\n";
        resultMsg += "=========================\n";
        resultMsg += "Всего заголовков в документе: " + allTitleElements.length + "\n";
        resultMsg += "Заголовков в основном разделе: " + (statsMain.total + statsMainBody.total) + "\n";
        if (statsMainBody.total > 0) {
            resultMsg += "  • Из них основной заголовок body: " + statsMainBody.total + "\n";
        }
        resultMsg += "Заголовков в разделе сносок (примечаний): " + statsNotes.total + "\n";
        resultMsg += "Заголовков в разделе комментариев: " + statsComments.total + "\n";
        resultMsg += "=========================\n\n";
        
        if (statsProcessMainBody.caps > 0 || statsProcessMainBody.normal > 0) {
            resultMsg += "• Основной заголовок body:\n";
            if (statsProcessMainBody.caps > 0) {
                resultMsg += "    CAPS изменено: " + changedCapsMainBody + " из " + statsProcessMainBody.caps + "\n";
            }
            if (statsProcessMainBody.normal > 0) {
                resultMsg += "    Normal изменено: " + changedNormalMainBody + " из " + statsProcessMainBody.normal + "\n";
            }
        }
        
        if (statsProcess.caps > 0) {
            resultMsg += "\n• Остальные CAPS-заголовков изменено: " + (changedCapsMain + changedCapsNotes + changedCapsComments) + " из " + statsProcess.caps + "\n";
            if (changedCapsMain > 0 || changedCapsNotes > 0 || changedCapsComments > 0) {
                resultMsg += "  Из них:\n";
                if (changedCapsMain > 0) resultMsg += "    • В основном разделе: " + changedCapsMain + "\n";
                if (changedCapsNotes > 0) resultMsg += "    • В разделе сносок: " + changedCapsNotes + "\n";
                if (changedCapsComments > 0) resultMsg += "    • В разделе комментариев: " + changedCapsComments + "\n";
            }
        }
        
        if (statsProcess.normal > 0) {
            resultMsg += "\n• Остальные Normal-заголовков изменено: " + (changedNormalMain + changedNormalNotes + changedNormalComments) + " из " + statsProcess.normal + "\n";
            if (changedNormalMain > 0 || changedNormalNotes > 0 || changedNormalComments > 0) {
                resultMsg += "  Из них:\n";
                if (changedNormalMain > 0) resultMsg += "    • В основном разделе: " + changedNormalMain + "\n";
                if (changedNormalNotes > 0) resultMsg += "    • В разделе сносок: " + changedNormalNotes + "\n";
                if (changedNormalComments > 0) resultMsg += "    • В разделе комментариев: " + changedNormalComments + "\n";
            }
        }
        
        resultMsg += "==============================\n\n";
        resultMsg += "Время выполнения: " + timeSec + " сек";
        
        MsgBox(resultMsg);
    }
}
