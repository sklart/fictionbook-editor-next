// Скрипт "Диагностика заголовков" для редактора FBE
// version 1.1
// Идея - TaKir
// Реализация - DeepSeek

// version 1.1, 30.12.2025
// Исправлена ошибка с undefined элементом при выводе групп
//======================================

function Run() {
    var scriptName = "Диагностика заголовков";
    var version = "1.1";

    // Получаем неразрывный пробел из настроек FBE
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

    // Маркеры заголовков
    var markers = ['глава', 'часть', 'книга', 'том', 'раздел', 'chapter', 'part', 'book', 'volume', 'stave'];

    // РУССКИЕ ЧИСЛИТЕЛЬНЫЕ
    var russianNumerals = {};
    var baseNumbers = {
        'ноль': 0, 'нулевой': 0, 'нулевая': 0, 'нулевое': 0,
        'один': 1, 'одна': 1, 'одно': 1, 'первый': 1, 'первая': 1, 'первое': 1,
        'два': 2, 'две': 2, 'два': 2, 'второй': 2, 'вторая': 2, 'второе': 2,
        'три': 3, 'третий': 3, 'третья': 3, 'третье': 3,
        'четыре': 4, 'четвертый': 4, 'четвертая': 4, 'четвертое': 4,
        'пять': 5, 'пятый': 5, 'пятая': 5, 'пятое': 5,
        'шесть': 6, 'шестой': 6, 'шестая': 6, 'шестое': 6,
        'семь': 7, 'седьмой': 7, 'седьмая': 7, 'седьмое': 7,
        'восемь': 8, 'восьмой': 8, 'восьмая': 8, 'восьмое': 8,
        'девять': 9, 'девятый': 9, 'девятая': 9, 'девятое': 9,
        'десять': 10, 'десятый': 10, 'десятая': 10, 'десятое': 10
    };
    for (var key in baseNumbers) {
        russianNumerals[key] = baseNumbers[key];
    }

    // РИМСКИЕ ЦИФРЫ
    var romanNumerals = {
        'I': 1, 'II': 2, 'III': 3, 'IV': 4, 'V': 5, 'VI': 6, 'VII': 7, 'VIII': 8, 'IX': 9,
        'X': 10, 'XI': 11, 'XII': 12, 'XIII': 13, 'XIV': 14, 'XV': 15, 'XVI': 16, 'XVII': 17,
        'XVIII': 18, 'XIX': 19, 'XX': 20, 'XL': 40, 'L': 50, 'LX': 60, 'LXX': 70, 'LXXX': 80,
        'XC': 90, 'C': 100, 'CC': 200, 'CCC': 300, 'CD': 400, 'D': 500, 'M': 1000
    };

    function getTextFromElement(element) {
        if (!element) return "";
        var text = "";
        function getText(node) {
            if (node.nodeType == 3) {
                text += node.nodeValue || "";
            } else if (node.nodeType == 1) {
                for (var i = 0; i < node.childNodes.length; i++) {
                    getText(node.childNodes[i]);
                }
            }
        }
        getText(element);
        return text;
    }

    function normalizeSpaces(text, nbspEntity) {
        if (!text) return "";
        var result = "";
        for (var i = 0; i < text.length; i++) {
            if (i + nbspEntity.length <= text.length) {
                var substr = text.substring(i, i + nbspEntity.length);
                if (substr == nbspEntity) {
                    result += ' ';
                    i += nbspEntity.length - 1;
                    continue;
                }
            }
            if (i + 6 <= text.length) {
                var substr = text.substring(i, i + 6);
                if (substr == '&nbsp;') {
                    result += ' ';
                    i += 5;
                    continue;
                }
            }
            if (text.charCodeAt(i) == 160) {
                result += ' ';
                continue;
            }
            result += text.charAt(i);
        }
        result = result.replace(/\s+/g, ' ');
        result = result.replace(/^\s+|\s+$/g, '');
        return result;
    }

    function splitIntoWords(text) {
        var words = [];
        var current = "";
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            if (ch == ' ' || ch == '\t' || ch == '\n') {
                if (current) {
                    words.push(current);
                    current = "";
                }
            } else {
                current += ch;
            }
        }
        if (current) words.push(current);
        return words;
    }

    function isArabicNumber(word) {
        if (!word) return false;
        var cleanWord = word.replace(/[.,:;!?\-]/g, '');
        if (!cleanWord) return false;
        for (var i = 0; i < cleanWord.length; i++) {
            if (cleanWord.charAt(i) < '0' || cleanWord.charAt(i) > '9') return false;
        }
        return cleanWord.length > 0;
    }

    function isRomanNumber(word) {
        if (!word) return false;
        var cleanWord = word.replace(/[.,:;!?\-]/g, '');
        if (!cleanWord) return false;
        var romanChars = "IVXLCDM";
        for (var i = 0; i < cleanWord.length; i++) {
            var found = false;
            for (var j = 0; j < romanChars.length; j++) {
                if (cleanWord.charAt(i).toUpperCase() == romanChars.charAt(j)) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
        return cleanWord.length > 0;
    }

    function getNumberFromWord(word) {
        if (!word) return null;
        var cleanWord = word.replace(/[.,:;!?\-]/g, '');
        var cleanWordLower = cleanWord.toLowerCase();
        if (isArabicNumber(cleanWord)) return parseInt(cleanWord, 10);
        if (isRomanNumber(cleanWord)) {
            var upperWord = cleanWord.toUpperCase();
            if (romanNumerals[upperWord] !== undefined) return romanNumerals[upperWord];
        }
        if (russianNumerals[cleanWordLower] !== undefined) return russianNumerals[cleanWordLower];
        return null;
    }

    function getNumeralFormat(word) {
        if (!word) return 'unknown';
        var cleanWord = word.replace(/[.,:;!?\-]/g, '');
        if (isArabicNumber(cleanWord)) return 'arabic';
        if (isRomanNumber(cleanWord)) return 'roman';
        var cleanWordLower = cleanWord.toLowerCase();
        if (russianNumerals[cleanWordLower] !== undefined) return 'russian_word';
        return 'unknown';
    }

    function analyzeTitleQuick(text) {
        var normalizedText = normalizeSpaces(text, nbspEntity);
        var words = splitIntoWords(normalizedText);
        if (words.length < 1) return null;

        var firstWord = words[0].toLowerCase();
        var secondWord = words.length > 1 ? words[1] : "";

        // Паттерн: Маркер + числительное
        for (var i = 0; i < markers.length; i++) {
            if (firstWord == markers[i]) {
                if (words.length > 1) {
                    var number = getNumberFromWord(secondWord);
                    if (number !== null) {
                        return {
                            type: 'marker_numeral',
                            marker: firstWord,
                            number: number,
                            numeralForm: secondWord,
                            numeralFormat: getNumeralFormat(secondWord)
                        };
                    }
                }
                return {
                    type: 'marker_only',
                    marker: firstWord,
                    number: null
                };
            }
        }

        // Числительное + маркер
        var firstNumber = getNumberFromWord(words[0]);
        if (firstNumber !== null && words.length > 1) {
            for (var j = 0; j < markers.length; j++) {
                if (words[1].toLowerCase() == markers[j]) {
                    return {
                        type: 'numeral_marker',
                        marker: words[1].toLowerCase(),
                        number: firstNumber,
                        numeralForm: words[0],
                        numeralFormat: getNumeralFormat(words[0])
                    };
                }
            }
        }

        // Только число
        if (firstNumber !== null) {
            return {
                type: 'number_only',
                marker: '',
                number: firstNumber,
                numeralForm: words[0],
                numeralFormat: getNumeralFormat(words[0])
            };
        }

        return null;
    }

    // ====== ОСНОВНАЯ ЧАСТЬ ======
    var report = "---------------------------\n" +
                 scriptName + "\n" +
                 "ver. " + version + "\n" +
                 "---------------------------\n\n";

    var allDivs = document.getElementsByTagName('div');
    var foundTitles = [];

    for (var i = 0; i < allDivs.length; i++) {
        var div = allDivs[i];
        
        var inNotes = false;
        var parent = div.parentNode;
        while (parent) {
            if (parent.nodeName == 'DIV' && parent.className == 'body') {
                var fbname = parent.getAttribute('fbname');
                if (fbname == 'notes' || fbname == 'comments') {
                    inNotes = true;
                    break;
                }
            }
            parent = parent.parentNode;
        }
        if (inNotes) continue;

        if (div.className && typeof div.className == 'string') {
            var classes = div.className.split(' ');
            var isTitle = false;
            for (var j = 0; j < classes.length; j++) {
                if (classes[j] == 'title') {
                    isTitle = true;
                    break;
                }
            }

            if (isTitle) {
                var pElements = div.getElementsByTagName('p');
                if (pElements.length > 0) {
                    var firstP = pElements[0];
                    var text = getTextFromElement(firstP);
                    var analysis = analyzeTitleQuick(text);

                    foundTitles.push({
                        text: text.length > 60 ? text.substring(0, 57) + "..." : text,
                        fullText: text,
                        analysis: analysis
                    });
                }
            }
        }
    }

    report += "Всего найдено заголовков: " + foundTitles.length + "\n\n";

    // Собираем статистику по типам
    var groups = {};
    for (var i = 0; i < foundTitles.length; i++) {
        var t = foundTitles[i];
        if (t && t.analysis && t.analysis.type) {
            var key = t.analysis.type;
            if (t.analysis.marker) {
                key += '_' + t.analysis.marker;
            }
            if (t.analysis.numeralFormat) {
                key += '_' + t.analysis.numeralFormat;
            }
            if (!groups[key]) {
                groups[key] = [];
            }
            groups[key].push(t);
        }
    }

    report += "=== ГРУППЫ ЗАГОЛОВКОВ ===\n\n";
    var groupNum = 1;
    for (var key in groups) {
        var group = groups[key];
        if (!group) continue;
        
        report += "Группа " + groupNum + ": " + key + " (" + group.length + " шт.)\n";
        
        var showCount = Math.min(group.length, 8);
        if (group.length <= 8) {
            for (var j = 0; j < group.length; j++) {
                var item = group[j];
                if (!item) continue;
                report += "  [" + (j + 1) + "] " + (item.text || "???") + "\n";
                if (item.analysis) {
                    report += "      тип=" + item.analysis.type + 
                             ", маркер=" + (item.analysis.marker || "нет") + 
                             ", номер=" + (item.analysis.number || "нет") +
                             ", форма=" + (item.analysis.numeralForm || "нет") +
                             ", формат=" + (item.analysis.numeralFormat || "нет") + "\n";
                }
            }
        } else {
            for (var j = 0; j < 5; j++) {
                var item = group[j];
                if (!item) continue;
                report += "  [" + (j + 1) + "] " + (item.text || "???") + "\n";
                if (item.analysis) {
                    report += "      тип=" + item.analysis.type + 
                             ", маркер=" + (item.analysis.marker || "нет") + 
                             ", номер=" + (item.analysis.number || "нет") +
                             ", форма=" + (item.analysis.numeralForm || "нет") +
                             ", формат=" + (item.analysis.numeralFormat || "нет") + "\n";
                }
            }
            report += "  ...\n";
            for (var j = group.length - 3; j < group.length; j++) {
                var item = group[j];
                if (!item) continue;
                report += "  [" + (j + 1) + "] " + (item.text || "???") + "\n";
                if (item.analysis) {
                    report += "      тип=" + item.analysis.type + 
                             ", маркер=" + (item.analysis.marker || "нет") + 
                             ", номер=" + (item.analysis.number || "нет") +
                             ", форма=" + (item.analysis.numeralForm || "нет") +
                             ", формат=" + (item.analysis.numeralFormat || "нет") + "\n";
                }
            }
        }
        report += "\n";
        groupNum++;
    }

    // Заголовки без анализа
    report += "=== ЗАГОЛОВКИ БЕЗ НУМЕРАЦИИ ===\n\n";
    var noNumCount = 0;
    for (var i = 0; i < foundTitles.length; i++) {
        var t = foundTitles[i];
        if (!t || !t.analysis || !t.analysis.type) {
            noNumCount++;
            if (noNumCount <= 10 && t) {
                report += "  • " + (t.text || "???") + "\n";
            }
        }
    }
    if (noNumCount > 10) {
        report += "  ... и еще " + (noNumCount - 10) + "\n";
    }
    report += "\nВсего без нумерации: " + noNumCount + "\n";

    MsgBox(report, "FBE скрипт");
}
