// Скрипт "Проверить правильность нумерации заголовков" для редактора FBE
// version 3.3
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для проверки и автоматического исправления 
// последовательности нумерации заголовков в fb2-документах.
// Находит пропуски, дубликаты и другие нарушения в нумерации.
// Поддерживает различные форматы: арабские/римские цифры, русские/английские числительные.
// Автоматически исправляет ошибки, сохраняя исходный стиль оформления.
// Сохраняет сноски в заголовках при исправлении нумерации.
// Находит потерянные заголовки в обычном тексте.
// Сохраняет оригинальную пунктуацию (точки, тире и т.д.).

// Варианты заголовков, обрабатываемые скриптом:
// 1. Маркер + число: Глава 1, Часть 15, Том 7, Книга 3, Раздел 42, Volume 5, Chapter 12
// 2. Маркер + римские: Глава I, Часть XV, Том VII, Книга III, Раздел XLII
// 3. Маркер + русские числительные: Глава первая, Часть пятнадцатый, Том седьмой
// 4. Числительное + маркер: 1 Глава, XV Часть, Первый раздел, Седьмой том
// 5. Только числа: 1, I, Первый, One, First (арабские, римские, русские, английские)
// 6. Составные числительные: Двадцать первый, Девяносто девятая, Twenty-five, Forty-first
// 7. Поддерживает нормальные перезапуски нумерации (1,2,3,1,2,3...)
// 8. Арабские цифры — без ограничений, римские — до 3999, 
//    словесные числительные — до ~1000 (до «тысячи»)
//    Продолжает нумерацию от первого заголовка в группе (Глава 62 → 63 → 64...)

// version 3.3, 05.01.2026
// - Полная переработка словаря русских числительных: использованы готовые формы
//   по родам из проверенного скрипта конвертации чисел
// - Исправлено распознавание всех составных числительных (сто десятая, сто двадцать первая...)
// - Римские и арабские заголовки корректно разделяются по группам
// - Перезапуски нумерации после римских разделов работают правильно
// - Добавлены настройки тихого режима и выбора разделов
//======================================

function Run() {
    // Название и версия скрипта
    var scriptName = "Проверить правильность нумерации заголовков";
    var scriptVersion = "3.3";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима
    
    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0; // 0 - нет, 1 - да
    
    // Обрабатывать раздел комментариев
    var processCommentsSection = 0; // 0 - нет, 1 - да

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

    // Маркеры заголовков (в нижнем регистре для сравнения)
    var markers = ['глава', 'часть', 'книга', 'том', 'раздел', 'chapter', 'part', 'book', 'volume', 'stave'];
    
    // Иерархия маркеров (от старших к младшим)
    var markerHierarchy = {
        'том': 1,
        'книга': 1,
        'book': 1,
        'volume': 1,
        'часть': 2,
        'part': 2,
        'раздел': 3,
        'глава': 4,
        'chapter': 4,
        'stave': 4
    };

    // ==================================================
    // СЛОВАРИ ДЛЯ ЧИСЛИТЕЛЬНЫХ (из проверенного скрипта)
    // ==================================================
    
    // Единицы (женский род)
    var unitsFemale = {
        '1': 'первая', '2': 'вторая', '3': 'третья',
        '4': 'четвёртая', '5': 'пятая', '6': 'шестая',
        '7': 'седьмая', '8': 'восьмая', '9': 'девятая'
    };
    
    // Единицы (мужской род)
    var unitsMale = {
        '1': 'первый', '2': 'второй', '3': 'третий',
        '4': 'четвёртый', '5': 'пятый', '6': 'шестой',
        '7': 'седьмой', '8': 'восьмой', '9': 'девятый'
    };
    
    // Десятки (для составных чисел, БЕЗ РОДА)
    var tensAny = {
        '20': 'двадцать', '30': 'тридцать', '40': 'сорок',
        '50': 'пятьдесят', '60': 'шестьдесят', '70': 'семьдесят',
        '80': 'восемьдесят', '90': 'девяносто'
    };
    
    // Круглые десятки (женский род)
    var tensFemale = {
        '20': 'двадцатая', '30': 'тридцатая', '40': 'сороковая',
        '50': 'пятидесятая', '60': 'шестидесятая', '70': 'семидесятая',
        '80': 'восьмидесятая', '90': 'девяностая'
    };
    
    // Круглые десятки (мужской род)
    var tensMale = {
        '20': 'двадцатый', '30': 'тридцатый', '40': 'сороковой',
        '50': 'пятидесятый', '60': 'шестидесятый', '70': 'семидесятый',
        '80': 'восьмидесятый', '90': 'девяностый'
    };
    
    // Числа 10-19 (женский род)
    var teensFemale = {
        '10': 'десятая', '11': 'одиннадцатая', '12': 'двенадцатая',
        '13': 'тринадцатая', '14': 'четырнадцатая', '15': 'пятнадцатая',
        '16': 'шестнадцатая', '17': 'семнадцатая', '18': 'восемнадцатая',
        '19': 'девятнадцатая'
    };
    
    // Числа 10-19 (мужской род)
    var teensMale = {
        '10': 'десятый', '11': 'одиннадцатый', '12': 'двенадцатый',
        '13': 'тринадцатый', '14': 'четырнадцатый', '15': 'пятнадцатый',
        '16': 'шестнадцатый', '17': 'семнадцатый', '18': 'восемнадцатый',
        '19': 'девятнадцатый'
    };
    
    // Сотни (круглые, женский род)
    var hundredsFemale = {
        '100': 'сотая', '200': 'двухсотая', '300': 'трёхсотая',
        '400': 'четырёхсотая', '500': 'пятисотая', '600': 'шестисотая',
        '700': 'семисотая', '800': 'восьмисотая', '900': 'девятисотая'
    };
    
    // Сотни (круглые, мужской род)
    var hundredsMale = {
        '100': 'сотый', '200': 'двухсотый', '300': 'трёхсотый',
        '400': 'четырёхсотый', '500': 'пятисотый', '600': 'шестисотый',
        '700': 'семисотый', '800': 'восьмисотый', '900': 'девятисотый'
    };
    
    // Сотни (для составных чисел, БЕЗ РОДА)
    var hundredsAny = {
        '100': 'сто', '200': 'двести', '300': 'триста',
        '400': 'четыреста', '500': 'пятьсот', '600': 'шестьсот',
        '700': 'семьсот', '800': 'восемьсот', '900': 'девятьсот'
    };

    // ==================================================
    // ПОСТРОЕНИЕ СЛОВАРЯ РУССКИХ ЧИСЛИТЕЛЬНЫХ
    // ==================================================
    
    var russianNumerals = {};
    
    // Функция для добавления всех вариантов написания (с ё и е)
    function addWord(word, value) {
        if (!word || typeof word != 'string') return;
        russianNumerals[word.toLowerCase()] = value;
        // Варианты с буквой ё
        if (word.indexOf('ё') !== -1) {
            russianNumerals[word.toLowerCase().replace(/ё/g, 'е')] = value;
        }
    }
    
    // 1. Базовые единицы и числа 10-19
    var baseDicts = [unitsFemale, unitsMale, teensFemale, teensMale, tensFemale, tensMale, 
                     hundredsFemale, hundredsMale, tensAny, hundredsAny];
    for (var d = 0; d < baseDicts.length; d++) {
        var dict = baseDicts[d];
        if (!dict) continue;
        for (var key in dict) {
            if (dict[key]) {
                addWord(dict[key], parseInt(key, 10));
            }
        }
    }
    
    // 2. Числа 1-9 (количественные)
    var cardinalUnits = {
        '1': ['один', 'одна', 'одно'],
        '2': ['два', 'две'],
        '3': ['три'],
        '4': ['четыре'],
        '5': ['пять'],
        '6': ['шесть'],
        '7': ['семь'],
        '8': ['восемь'],
        '9': ['девять']
    };
    for (var key2 in cardinalUnits) {
        var words2 = cardinalUnits[key2];
        if (words2) {
            for (var w = 0; w < words2.length; w++) {
                addWord(words2[w], parseInt(key2, 10));
            }
        }
    }
    addWord('десять', 10);
    addWord('одиннадцать', 11);
    addWord('двенадцать', 12);
    addWord('тринадцать', 13);
    addWord('четырнадцать', 14);
    addWord('пятнадцать', 15);
    addWord('шестнадцать', 16);
    addWord('семнадцать', 17);
    addWord('восемнадцать', 18);
    addWord('девятнадцать', 19);
    addWord('сорок', 40);
    addWord('девяносто', 90);
    addWord('сто', 100);
    addWord('двести', 200);
    addWord('триста', 300);
    addWord('четыреста', 400);
    addWord('пятьсот', 500);
    addWord('шестьсот', 600);
    addWord('семьсот', 700);
    addWord('восемьсот', 800);
    addWord('девятьсот', 900);
    addWord('тысяча', 1000);
    addWord('тысячный', 1000);
    addWord('тысячная', 1000);
    addWord('тысячное', 1000);
    addWord('ноль', 0);
    addWord('нулевой', 0);
    addWord('нулевая', 0);
    addWord('нулевое', 0);
    
    // 3. Составные десятки 21-99
    var tensKeys = ['20', '30', '40', '50', '60', '70', '80', '90'];
    var unitKeys = ['1', '2', '3', '4', '5', '6', '7', '8', '9'];
    
    for (var t = 0; t < tensKeys.length; t++) {
        var tenKey = tensKeys[t];
        var tenValue = parseInt(tenKey, 10);
        var tenWord = tensAny[tenKey];
        if (!tenWord) continue;
        
        for (var u = 0; u < unitKeys.length; u++) {
            var unitKey = unitKeys[u];
            var unitValue = parseInt(unitKey, 10);
            var totalValue = tenValue + unitValue;
            
            // Порядковые
            if (unitsFemale[unitKey]) {
                addWord(tenWord + ' ' + unitsFemale[unitKey], totalValue);
            }
            if (unitsMale[unitKey]) {
                addWord(tenWord + ' ' + unitsMale[unitKey], totalValue);
            }
        }
    }
    
    // 4. Составные сотни 101-999
    var hundredKeys = ['100', '200', '300', '400', '500', '600', '700', '800', '900'];
    for (var h = 0; h < hundredKeys.length; h++) {
        var hundredKey = hundredKeys[h];
        var hundredValue = parseInt(hundredKey, 10);
        var hundredWord = hundredsAny[hundredKey];
        if (!hundredWord) continue;
        
        // Сотни + единицы (101-109)
        for (var u = 0; u < unitKeys.length; u++) {
            var unitKey = unitKeys[u];
            var unitValue = parseInt(unitKey, 10);
            var totalValue = hundredValue + unitValue;
            
            if (unitsFemale[unitKey]) {
                addWord(hundredWord + ' ' + unitsFemale[unitKey], totalValue);
            }
            if (unitsMale[unitKey]) {
                addWord(hundredWord + ' ' + unitsMale[unitKey], totalValue);
            }
        }
        
        // Сотни + 10-19
        for (var teenKey in teensFemale) {
            if (teensFemale[teenKey]) {
                var teenValue = parseInt(teenKey, 10);
                var totalTeen = hundredValue + teenValue;
                addWord(hundredWord + ' ' + teensFemale[teenKey], totalTeen);
            }
        }
        for (var teenKey2 in teensMale) {
            if (teensMale[teenKey2]) {
                var teenValue2 = parseInt(teenKey2, 10);
                var totalTeen2 = hundredValue + teenValue2;
                addWord(hundredWord + ' ' + teensMale[teenKey2], totalTeen2);
            }
        }
        
        // Сотни + круглые десятки (110, 120, ...)
        for (var t2 = 0; t2 < tensKeys.length; t2++) {
            var tenKey2 = tensKeys[t2];
            var tenValue2 = parseInt(tenKey2, 10);
            var totalHundredTen = hundredValue + tenValue2;
            
            if (tensFemale[tenKey2]) {
                addWord(hundredWord + ' ' + tensFemale[tenKey2], totalHundredTen);
            }
            if (tensMale[tenKey2]) {
                addWord(hundredWord + ' ' + tensMale[tenKey2], totalHundredTen);
            }
            
            // Сотни + десятки + единицы (121-199)
            var tenWord2 = tensAny[tenKey2];
            if (!tenWord2) continue;
            
            for (var u2 = 0; u2 < unitKeys.length; u2++) {
                var unitKey2 = unitKeys[u2];
                var unitValue2 = parseInt(unitKey2, 10);
                var totalComposite = hundredValue + tenValue2 + unitValue2;
                
                if (unitsFemale[unitKey2]) {
                    addWord(hundredWord + ' ' + tenWord2 + ' ' + unitsFemale[unitKey2], totalComposite);
                }
                if (unitsMale[unitKey2]) {
                    addWord(hundredWord + ' ' + tenWord2 + ' ' + unitsMale[unitKey2], totalComposite);
                }
            }
        }
    }
    
    // АНГЛИЙСКИЕ ЧИСЛИТЕЛЬНЫЕ
    var englishNumerals = {
        'zero': 0, 'zeroth': 0,
        'one': 1, 'first': 1, 'once': 1,
        'two': 2, 'second': 2, 'twice': 2,
        'three': 3, 'third': 3, 'thrice': 3,
        'four': 4, 'fourth': 4,
        'five': 5, 'fifth': 5,
        'six': 6, 'sixth': 6,
        'seven': 7, 'seventh': 7,
        'eight': 8, 'eighth': 8,
        'nine': 9, 'ninth': 9,
        'ten': 10, 'tenth': 10,
        'eleven': 11, 'eleventh': 11,
        'twelve': 12, 'twelfth': 12,
        'thirteen': 13, 'thirteenth': 13,
        'fourteen': 14, 'fourteenth': 14,
        'fifteen': 15, 'fifteenth': 15,
        'sixteen': 16, 'sixteenth': 16,
        'seventeen': 17, 'seventeenth': 17,
        'eighteen': 18, 'eighteenth': 18,
        'nineteen': 19, 'nineteenth': 19,
        'twenty': 20, 'twentieth': 20,
        'thirty': 30, 'thirtieth': 30,
        'forty': 40, 'fortieth': 40,
        'fifty': 50, 'fiftieth': 50,
        'sixty': 60, 'sixtieth': 60,
        'seventy': 70, 'seventieth': 70,
        'eighty': 80, 'eightieth': 80,
        'ninety': 90, 'ninetieth': 90,
        'hundred': 100, 'hundredth': 100,
        'thousand': 1000, 'thousandth': 1000
    };
    
    var engTens = ['twenty', 'thirty', 'forty', 'fifty', 'sixty', 'seventy', 'eighty', 'ninety'];
    var engUnits = ['one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine'];
    var engOrdinals = ['first', 'second', 'third', 'fourth', 'fifth', 'sixth', 'seventh', 'eighth', 'ninth'];
    
    for (var t = 0; t < engTens.length; t++) {
        var engTen = engTens[t];
        var engTenValue = englishNumerals[engTen];
        
        for (var u = 0; u < engUnits.length; u++) {
            var cardWord = engTen + '-' + engUnits[u];
            englishNumerals[cardWord] = engTenValue + (u + 1);
            var ordWord = engTen + '-' + engOrdinals[u];
            englishNumerals[ordWord] = engTenValue + (u + 1);
            var cardWordNoDash = engTen + ' ' + engUnits[u];
            englishNumerals[cardWordNoDash] = engTenValue + (u + 1);
            var ordWordNoDash = engTen + ' ' + engOrdinals[u];
            englishNumerals[ordWordNoDash] = engTenValue + (u + 1);
        }
    }
    
    for (var u = 1; u <= 9; u++) {
        var hundredWord = engUnits[u-1] + ' hundred';
        englishNumerals[hundredWord] = u * 100;
        var hundredthWord = engUnits[u-1] + ' hundredth';
        englishNumerals[hundredthWord] = u * 100;
    }
    
    // РИМСКИЕ ЦИФРЫ
    var romanNumerals = {
        'I': 1, 'II': 2, 'III': 3, 'IV': 4, 'V': 5, 'VI': 6, 'VII': 7, 'VIII': 8, 'IX': 9,
        'X': 10, 'XI': 11, 'XII': 12, 'XIII': 13, 'XIV': 14, 'XV': 15, 'XVI': 16, 'XVII': 17, 
        'XVIII': 18, 'XIX': 19, 'XX': 20, 'XXI': 21, 'XXII': 22, 'XXIII': 23, 'XXIV': 24,
        'XXV': 25, 'XXVI': 26, 'XXVII': 27, 'XXVIII': 28, 'XXIX': 29, 'XXX': 30, 'XXXI': 31,
        'XXXII': 32, 'XXXIII': 33, 'XXXIV': 34, 'XXXV': 35, 'XXXVI': 36, 'XXXVII': 37,
        'XXXVIII': 38, 'XXXIX': 39, 'XL': 40, 'XLI': 41, 'XLII': 42, 'XLIII': 43, 'XLIV': 44,
        'XLV': 45, 'XLVI': 46, 'XLVII': 47, 'XLVIII': 48, 'XLIX': 49, 'L': 50, 'LI': 51,
        'LII': 52, 'LIII': 53, 'LIV': 54, 'LV': 55, 'LVI': 56, 'LVII': 57, 'LVIII': 58,
        'LIX': 59, 'LX': 60, 'LXI': 61, 'LXII': 62, 'LXIII': 63, 'LXIV': 64, 'LXV': 65,
        'LXVI': 66, 'LXVII': 67, 'LXVIII': 68, 'LXIX': 69, 'LXX': 70, 'LXXI': 71, 'LXXII': 72,
        'LXXIII': 73, 'LXXIV': 74, 'LXXV': 75, 'LXXVI': 76, 'LXXVII': 77, 'LXXVIII': 78,
        'LXXIX': 79, 'LXXX': 80, 'LXXXI': 81, 'LXXXII': 82, 'LXXXIII': 83, 'LXXXIV': 84,
        'LXXXV': 85, 'LXXXVI': 86, 'LXXXVII': 87, 'LXXXVIII': 88, 'LXXXIX': 89, 'XC': 90,
        'XCI': 91, 'XCII': 92, 'XCIII': 93, 'XCIV': 94, 'XCV': 95, 'XCVI': 96, 'XCVII': 97,
        'XCVIII': 98, 'XCIX': 99, 'C': 100, 'CI': 101, 'CII': 102, 'CIII': 103, 'CIV': 104,
        'CV': 105, 'CVI': 106, 'CVII': 107, 'CVIII': 108, 'CIX': 109, 'CX': 110, 'CXX': 120,
        'CXXX': 130, 'CXL': 140, 'CL': 150, 'CLX': 160, 'CLXX': 170, 'CLXXX': 180, 'CXC': 190,
        'CC': 200, 'CCC': 300, 'CD': 400, 'D': 500, 'DC': 600, 'DCC': 700, 'DCCC': 800,
        'CM': 900, 'M': 1000
    };

    // Статистика
    var stats = {
        totalTitles: 0,
        titlesInNotes: 0,
        numberedTitles: 0,
        errorsFound: 0,
        processed: 0,
        fixed: 0,
        lostTitlesFound: 0
    };

    var allTitles = [];
    var errors = [];
    var correctionMap = [];
    var lostTitles = [];

    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================

    // Функция для получения текста из элемента
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

    // Функция нормализации пробелов
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

    // Функция разбивки на слова
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

    // Функция проверки, является ли слово числом (арабские цифры)
    function isArabicNumber(word) {
        if (!word) return false;
        
        var cleanWord = word.replace(/[.,:;!?\-]/g, '');
        if (!cleanWord) return false;
        
        for (var i = 0; i < cleanWord.length; i++) {
            var ch = cleanWord.charAt(i);
            if (ch < '0' || ch > '9') {
                return false;
            }
        }
        return cleanWord.length > 0;
    }

    // Функция проверки, является ли слово римским числом
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

    // Функция получения числового значения из слова
    function getNumberFromWord(word) {
        if (!word) return null;
        
        var cleanWord = word.replace(/[.,:;!?\-]/g, '');
        var cleanWordLower = cleanWord.toLowerCase();
        
        if (isArabicNumber(cleanWord)) {
            return parseInt(cleanWord, 10);
        }
        
        if (isRomanNumber(cleanWord)) {
            var upperWord = cleanWord.toUpperCase();
            if (romanNumerals[upperWord] !== undefined) {
                return romanNumerals[upperWord];
            }
            return calculateRomanValue(cleanWord);
        }
        
        if (cleanWordLower.indexOf(' ') !== -1) {
            for (var numeral in russianNumerals) {
                if (numeral.indexOf(' ') !== -1 && cleanWordLower == numeral) {
                    return russianNumerals[numeral];
                }
            }
        }
        
        if (russianNumerals[cleanWordLower] !== undefined) {
            return russianNumerals[cleanWordLower];
        }
        
        if (englishNumerals[cleanWordLower] !== undefined) {
            return englishNumerals[cleanWordLower];
        }
        
        return null;
    }

    // Функция для вычисления значения римской цифры
    function calculateRomanValue(roman) {
        if (!roman) return null;
        
        roman = roman.toUpperCase();
        var values = {
            'I': 1, 'V': 5, 'X': 10, 'L': 50, 'C': 100, 'D': 500, 'M': 1000
        };
        
        var total = 0;
        var prevValue = 0;
        
        for (var i = roman.length - 1; i >= 0; i--) {
            var currentChar = roman.charAt(i);
            var currentValue = values[currentChar];
            
            if (!currentValue) return null;
            
            if (currentValue < prevValue) {
                total -= currentValue;
            } else {
                total += currentValue;
            }
            
            prevValue = currentValue;
        }
        
        return total;
    }

    // Функция определения формата числительного
    function getNumeralFormat(word) {
        if (!word) return 'unknown';
        
        var cleanWord = word.replace(/[.,:;!?\-]/g, '');
        
        if (isArabicNumber(cleanWord)) {
            return 'arabic';
        }
        
        if (isRomanNumber(cleanWord)) {
            return 'roman';
        }
        
        var cleanWordLower = cleanWord.toLowerCase();
        
        if (russianNumerals[cleanWordLower] !== undefined) {
            return 'russian_word';
        }
        
        if (englishNumerals[cleanWordLower] !== undefined) {
            return 'english_word';
        }
        
        return 'unknown';
    }

    // Функция проверки, начинается ли текст с диалога (тире в начале)
    function startsWithDialogue(text) {
        if (!text) return false;
        var trimmed = text.replace(/^\s+/, '');
        return trimmed.charAt(0) == '-' || trimmed.charAt(0) == '—';
    }

    // Функция анализа заголовка и извлечения номера с пунктуацией
    function analyzeTitle(text) {
        var normalizedText = normalizeSpaces(text, nbspEntity);
        var words = splitIntoWords(normalizedText);
        
        if (words.length < 1) return null;
        
        if (startsWithDialogue(normalizedText)) {
            return null;
        }
        
        var firstWord = words[0].toLowerCase();
        var secondWord = words.length > 1 ? words[1] : "";
        var thirdWord = words.length > 2 ? words[2] : "";
        var fourthWord = words.length > 3 ? words[3] : "";
        
        // Паттерн 1: Маркер + числительное
        for (var i = 0; i < markers.length; i++) {
            if (firstWord == markers[i]) {
                if (words.length > 1) {
                    // 3-словные СНАЧАЛА
                    if (words.length > 3) {
                        var combinedWord3 = secondWord + ' ' + thirdWord + ' ' + fourthWord;
                        var combinedNumber3 = getNumberFromWord(combinedWord3);
                        if (combinedNumber3 !== null) {
                            var punctuationAfter = "";
                            var remainingText = "";
                            for (var w = 3; w < words.length; w++) {
                                remainingText += " " + words[w];
                            }
                            for (var pos = 0; pos < remainingText.length; pos++) {
                                var ch = remainingText.charAt(pos);
                                if (ch == '.' || ch == ',' || ch == ':' || ch == ';' || ch == '-' || ch == '—') {
                                    punctuationAfter = ch;
                                    if (pos + 1 < remainingText.length && remainingText.charAt(pos + 1) == ' ') {
                                        punctuationAfter += ' ';
                                    }
                                    break;
                                }
                            }
                            
                            return {
                                type: 'marker_numeral',
                                marker: firstWord,
                                number: combinedNumber3,
                                numeralForm: combinedWord3,
                                numeralFormat: getNumeralFormat(combinedWord3),
                                originalText: normalizedText,
                                words: words,
                                punctuationAfter: punctuationAfter
                            };
                        }
                    }
                    
                    // 2-словные
                    if (words.length > 2) {
                        var combinedWord2 = secondWord + ' ' + thirdWord;
                        var combinedNumber2 = getNumberFromWord(combinedWord2);
                        if (combinedNumber2 !== null) {
                            var punctuationAfter = "";
                            var remainingText = "";
                            for (var w = 2; w < words.length; w++) {
                                remainingText += " " + words[w];
                            }
                            for (var pos = 0; pos < remainingText.length; pos++) {
                                var ch = remainingText.charAt(pos);
                                if (ch == '.' || ch == ',' || ch == ':' || ch == ';' || ch == '-' || ch == '—') {
                                    punctuationAfter = ch;
                                    if (pos + 1 < remainingText.length && remainingText.charAt(pos + 1) == ' ') {
                                        punctuationAfter += ' ';
                                    }
                                    break;
                                }
                            }
                            
                            return {
                                type: 'marker_numeral',
                                marker: firstWord,
                                number: combinedNumber2,
                                numeralForm: combinedWord2,
                                numeralFormat: getNumeralFormat(combinedWord2),
                                originalText: normalizedText,
                                words: words,
                                punctuationAfter: punctuationAfter
                            };
                        }
                    }
                    
                    // Однокомпонентное
                    var number = getNumberFromWord(secondWord);
                    if (number !== null) {
                        var punctuationAfter = "";
                        var remainingText = "";
                        for (var w = 1; w < words.length; w++) {
                            remainingText += " " + words[w];
                        }
                        for (var pos = 0; pos < remainingText.length; pos++) {
                            var ch = remainingText.charAt(pos);
                            if (ch == '.' || ch == ',' || ch == ':' || ch == ';' || ch == '-' || ch == '—') {
                                punctuationAfter = ch;
                                if (pos + 1 < remainingText.length && remainingText.charAt(pos + 1) == ' ') {
                                    punctuationAfter += ' ';
                                }
                                break;
                            }
                        }
                        
                        return {
                            type: 'marker_numeral',
                            marker: firstWord,
                            number: number,
                            numeralForm: secondWord,
                            numeralFormat: getNumeralFormat(secondWord),
                            originalText: normalizedText,
                            words: words,
                            punctuationAfter: punctuationAfter
                        };
                    }
                }
                break;
            }
        }
        
        // Паттерн 2: Числительное в начале
        var firstWordNumber = getNumberFromWord(words[0]);
        var combinedFirstWords = words[0];
        
        // 3 слова сначала
        if (words.length > 2) {
            var combined3 = words[0] + ' ' + words[1] + ' ' + words[2];
            var num3 = getNumberFromWord(combined3);
            if (num3 !== null) {
                combinedFirstWords = combined3;
                firstWordNumber = num3;
            }
        }
        
        // 2 слова
        if (firstWordNumber === null && words.length > 1) {
            combinedFirstWords = words[0] + ' ' + words[1];
            firstWordNumber = getNumberFromWord(combinedFirstWords);
        }
        
        if (firstWordNumber !== null) {
            var wordIndex = 1;
            if (combinedFirstWords === words[0]) {
                wordIndex = 1;
            } else if (combinedFirstWords === words[0] + ' ' + words[1]) {
                wordIndex = 2;
            } else {
                wordIndex = 3;
            }
            
            if (wordIndex < words.length) {
                var markerWord = words[wordIndex].toLowerCase();
                for (i = 0; i < markers.length; i++) {
                    if (markerWord == markers[i]) {
                        var punctuationAfter = "";
                        var remainingText = "";
                        for (var w = wordIndex; w < words.length; w++) {
                            remainingText += " " + words[w];
                        }
                        for (var pos = 0; pos < remainingText.length; pos++) {
                            var ch = remainingText.charAt(pos);
                            if (ch == '.' || ch == ',' || ch == ':' || ch == ';' || ch == '-' || ch == '—') {
                                punctuationAfter = ch;
                                if (pos + 1 < remainingText.length && remainingText.charAt(pos + 1) == ' ') {
                                    punctuationAfter += ' ';
                                }
                                break;
                            }
                        }
                        
                        return {
                            type: 'numeral_marker',
                            marker: markerWord,
                            number: firstWordNumber,
                            numeralForm: combinedFirstWords,
                            numeralFormat: getNumeralFormat(combinedFirstWords),
                            originalText: normalizedText,
                            words: words,
                            punctuationAfter: punctuationAfter
                        };
                    }
                }
            }
            
            var numeralFormat = getNumeralFormat(combinedFirstWords);
            
            if (numeralFormat == 'arabic' || numeralFormat == 'roman') {
                if (words.length <= 3) {
                    var punctuationAfter = "";
                    var remainingText = "";
                    for (var w = 0; w < words.length; w++) {
                        remainingText += " " + words[w];
                    }
                    for (var pos = 0; pos < remainingText.length; pos++) {
                        var ch = remainingText.charAt(pos);
                        if (ch == '.' || ch == ',' || ch == ':' || ch == ';' || ch == '-' || ch == '—') {
                            punctuationAfter = ch;
                            if (pos + 1 < remainingText.length && remainingText.charAt(pos + 1) == ' ') {
                                punctuationAfter += ' ';
                            }
                            break;
                        }
                    }
                    
                    return {
                        type: 'number_only',
                        number: firstWordNumber,
                        numeralForm: combinedFirstWords,
                        numeralFormat: numeralFormat,
                        originalText: normalizedText,
                        words: words,
                        punctuationAfter: punctuationAfter
                    };
                }
            }
        }
        
        return null;
    }

    // Функция formatNumber
    function formatNumber(number, originalForm, numeralFormat) {
        if (!originalForm) return number.toString();
        
        if (numeralFormat == 'arabic') {
            return number.toString();
        }
        
        if (numeralFormat == 'roman') {
            for (var roman in romanNumerals) {
                if (romanNumerals[roman] == number) {
                    return roman;
                }
            }
            return generateRomanNumber(number);
        }
        
        var originalLower = originalForm.toLowerCase();
        
        if (numeralFormat == 'russian_word') {
            var candidates = [];
            
            for (var numeral in russianNumerals) {
                if (russianNumerals[numeral] == number) {
                    candidates.push(numeral);
                }
            }
            
            // Точное совпадение
            for (var c = 0; c < candidates.length; c++) {
                if (candidates[c] == originalLower) {
                    if (originalForm.charAt(0).toUpperCase() == originalForm.charAt(0)) {
                        return candidates[c].charAt(0).toUpperCase() + candidates[c].slice(1);
                    }
                    return candidates[c];
                }
            }
            
            // По окончанию
            var isOrdinal = false;
            if (originalLower.length > 2) {
                var ending2 = originalLower.slice(-2);
                if (ending2 == 'ый' || ending2 == 'ая' || ending2 == 'ое' || ending2 == 'ие' ||
                    ending2 == 'ой' || ending2 == 'ья' || ending2 == 'ье') {
                    isOrdinal = true;
                }
            }
            
            if (isOrdinal) {
                var targetEnding = originalLower.slice(-2);
                for (var c2 = 0; c2 < candidates.length; c2++) {
                    var cand = candidates[c2];
                    if (cand.length > 2 && cand.slice(-2) == targetEnding) {
                        if (originalForm.charAt(0).toUpperCase() == originalForm.charAt(0)) {
                            return cand.charAt(0).toUpperCase() + cand.slice(1);
                        }
                        return cand;
                    }
                }
                for (var c3 = 0; c3 < candidates.length; c3++) {
                    var cand3 = candidates[c3];
                    if (cand3.length > 2) {
                        var candEnding3 = cand3.slice(-2);
                        if (candEnding3 == 'ый' || candEnding3 == 'ая' || candEnding3 == 'ое' || candEnding3 == 'ие' ||
                            candEnding3 == 'ой' || candEnding3 == 'ья' || candEnding3 == 'ье') {
                            if (originalForm.charAt(0).toUpperCase() == originalForm.charAt(0)) {
                                return cand3.charAt(0).toUpperCase() + cand3.slice(1);
                            }
                            return cand3;
                        }
                    }
                }
            } else {
                for (var c4 = 0; c4 < candidates.length; c4++) {
                    var cand4 = candidates[c4];
                    if (cand4.length > 2) {
                        var candEnding4 = cand4.slice(-2);
                        if (!(candEnding4 == 'ый' || candEnding4 == 'ая' || candEnding4 == 'ое' || candEnding4 == 'ие' ||
                              candEnding4 == 'ой' || candEnding4 == 'ья' || candEnding4 == 'ье')) {
                            if (originalForm.charAt(0).toUpperCase() == originalForm.charAt(0)) {
                                return cand4.charAt(0).toUpperCase() + cand4.slice(1);
                            }
                            return cand4;
                        }
                    }
                }
            }
            
            if (candidates.length > 0) {
                if (originalForm.charAt(0).toUpperCase() == originalForm.charAt(0)) {
                    return candidates[0].charAt(0).toUpperCase() + candidates[0].slice(1);
                }
                return candidates[0];
            }
        }
        
        if (numeralFormat == 'english_word') {
            var engCandidates = [];
            for (var engNumeral in englishNumerals) {
                if (englishNumerals[engNumeral] == number) {
                    engCandidates.push(engNumeral);
                }
            }
            
            for (var ec = 0; ec < engCandidates.length; ec++) {
                if (engCandidates[ec].toLowerCase() == originalLower) {
                    if (originalForm.charAt(0).toUpperCase() == originalForm.charAt(0)) {
                        return engCandidates[ec].charAt(0).toUpperCase() + engCandidates[ec].slice(1);
                    }
                    return engCandidates[ec];
                }
            }
            
            if (engCandidates.length > 0) {
                if (originalForm.charAt(0).toUpperCase() == originalForm.charAt(0)) {
                    return engCandidates[0].charAt(0).toUpperCase() + engCandidates[0].slice(1);
                }
                return engCandidates[0];
            }
        }
        
        return number.toString();
    }

    // Функция генерации римского числа
    function generateRomanNumber(num) {
        if (num < 1 || num > 3999) return num.toString();
        
        var roman = "";
        var values = [
            {value: 1000, symbol: "M"}, {value: 900, symbol: "CM"},
            {value: 500, symbol: "D"}, {value: 400, symbol: "CD"},
            {value: 100, symbol: "C"}, {value: 90, symbol: "XC"},
            {value: 50, symbol: "L"}, {value: 40, symbol: "XL"},
            {value: 10, symbol: "X"}, {value: 9, symbol: "IX"},
            {value: 5, symbol: "V"}, {value: 4, symbol: "IV"},
            {value: 1, symbol: "I"}
        ];
        
        for (var i = 0; i < values.length; i++) {
            while (num >= values[i].value) {
                roman += values[i].symbol;
                num -= values[i].value;
            }
        }
        return roman;
    }

    // Функция сортировки массива по индексу
    function sortByIndex(arr) {
        var n = arr.length;
        for (var i = 0; i < n - 1; i++) {
            for (var j = 0; j < n - i - 1; j++) {
                if (arr[j].index > arr[j + 1].index) {
                    var temp = arr[j];
                    arr[j] = arr[j + 1];
                    arr[j + 1] = temp;
                }
            }
        }
        return arr;
    }

    // Склонение слова "заголовок"
    function declineTitleWord(count) {
        if (count % 10 == 1 && count % 100 != 11) return "заголовок";
        if (count % 10 >= 2 && count % 10 <= 4 && (count % 100 < 10 || count % 100 >= 20)) return "заголовка";
        return "заголовков";
    }

    // Уровень иерархии маркера
    function getMarkerLevel(marker) {
        if (!marker) return 99;
        var lower = marker.toLowerCase();
        if (markerHierarchy[lower] !== undefined) return markerHierarchy[lower];
        return 99;
    }

    // Проверка на старшие маркеры
    function hasParentMarkersInDocument(group, allMarkersInDoc) {
        if (!group || group.length === 0) return false;
        if (!allMarkersInDoc || allMarkersInDoc.length === 0) return false;
        
        var groupMarker = '';
        if (group[0] && group[0].analysis) groupMarker = group[0].analysis.marker || '';
        if (!groupMarker) return false;
        
        var groupLevel = getMarkerLevel(groupMarker);
        for (var i = 0; i < allMarkersInDoc.length; i++) {
            if (getMarkerLevel(allMarkersInDoc[i]) < groupLevel) return true;
        }
        return false;
    }

    // Другой формат между заголовками
    function hasOtherFormatBetween(allTitlesSorted, idx1, idx2, currentFormat) {
        if (!allTitlesSorted || idx1 < 0 || idx2 <= idx1 || !currentFormat) return false;
        for (var i = idx1 + 1; i < idx2; i++) {
            var t = allTitlesSorted[i];
            if (t && t.analysis && t.analysis.numeralFormat && t.analysis.numeralFormat != currentFormat) return true;
        }
        return false;
    }

    // Проверка последовательности
    function checkSequence(group, allMarkersInDoc, allTitlesSorted) {
        var sequenceErrors = [];
        var sequenceCorrections = [];
        
        if (group.length === 0) return {errors: [], corrections: []};
        if (!group[0] || !group[0].analysis) return {errors: [], corrections: []};
        
        var startNumber = group[0].analysis.number;
        var expectedNumber = startNumber;
        var lastCorrectNumber = startNumber;
        var restartDetected = false;
        var currentFormat = group[0].analysis.numeralFormat || '';
        
        if (startNumber != 1 && !hasParentMarkersInDocument(group, allMarkersInDoc)) {
            sequenceErrors.push({title: group[0], expected: 1, actual: startNumber, position: 1});
            sequenceCorrections.push({title: group[0], correctNumber: 1});
            expectedNumber = 1;
            lastCorrectNumber = 1;
        }
        
        for (var i = 0; i < group.length; i++) {
            var title = group[i];
            if (!title || !title.analysis) continue;
            
            var actualNumber = title.analysis.number;
            
            if (i === 0 && startNumber != 1 && !hasParentMarkersInDocument(group, allMarkersInDoc)) {
                lastCorrectNumber = 1;
                expectedNumber = 2;
                continue;
            }
            
            if (i > 0 && allTitlesSorted && currentFormat) {
                var prevTitle = group[i-1];
                if (prevTitle && prevTitle.index !== undefined && title.index !== undefined) {
                    if (hasOtherFormatBetween(allTitlesSorted, prevTitle.index, title.index, currentFormat)) {
                        expectedNumber = actualNumber;
                        lastCorrectNumber = actualNumber;
                        restartDetected = true;
                    }
                }
            }
            
            if (i > 0 && actualNumber === startNumber) {
                var prevNumber = group[i-1].analysis.number;
                if (prevNumber !== actualNumber) {
                    expectedNumber = startNumber;
                    lastCorrectNumber = expectedNumber;
                    restartDetected = true;
                }
            }
            
            if (actualNumber !== expectedNumber) {
                if (!(restartDetected && actualNumber === startNumber)) {
                    sequenceErrors.push({title: title, expected: expectedNumber, actual: actualNumber, position: i + 1});
                    sequenceCorrections.push({title: title, correctNumber: expectedNumber});
                    lastCorrectNumber = expectedNumber;
                    expectedNumber = lastCorrectNumber + 1;
                } else {
                    lastCorrectNumber = actualNumber;
                    expectedNumber = lastCorrectNumber + 1;
                    restartDetected = false;
                }
            } else {
                lastCorrectNumber = actualNumber;
                expectedNumber = lastCorrectNumber + 1;
                restartDetected = false;
            }
        }
        
        return {errors: sequenceErrors, corrections: sequenceCorrections};
    }

    // Доминирующий паттерн
    function determineDominantPattern(titles) {
        if (!titles || titles.length === 0) return null;
        
        var patternCounts = {};
        var maxCount = 0;
        var dominantPattern = null;
        
        var sampleSize = Math.min(titles.length, 5);
        for (var i = 0; i < sampleSize; i++) {
            var title = titles[i];
            if (title && title.analysis) {
                var patternKey = title.analysis.type;
                if (title.analysis.type == 'marker_numeral' || title.analysis.type == 'numeral_marker') {
                    patternKey += '_' + title.analysis.marker;
                }
                if (title.analysis.numeralFormat) patternKey += '_' + title.analysis.numeralFormat;
                
                if (!patternCounts[patternKey]) patternCounts[patternKey] = 0;
                patternCounts[patternKey]++;
                
                if (patternCounts[patternKey] > maxCount) {
                    maxCount = patternCounts[patternKey];
                    dominantPattern = {
                        type: title.analysis.type,
                        marker: title.analysis.marker || '',
                        numeralFormat: title.analysis.numeralFormat || '',
                        sample: title.analysis
                    };
                }
            }
        }
        return dominantPattern;
    }

    // Поиск потерянных заголовков
    function findLostTitles(dominantPattern) {
        var lostTitles = [];
        if (!dominantPattern) return lostTitles;
        
        var allParagraphs = document.getElementsByTagName('p');
        for (var i = 0; i < allParagraphs.length; i++) {
            var p = allParagraphs[i];
            
            var parent = p.parentNode;
            var isTitle = false;
            while (parent) {
                if (parent.nodeName == 'DIV' && parent.className && typeof parent.className == 'string') {
                    var classes = parent.className.split(' ');
                    for (var c = 0; c < classes.length; c++) {
                        if (classes[c] == 'title') { isTitle = true; break; }
                    }
                    if (isTitle) break;
                }
                parent = parent.parentNode;
            }
            if (isTitle) continue;
            
            var text = getTextFromElement(p);
            var normalizedText = normalizeSpaces(text, nbspEntity);
            if (startsWithDialogue(normalizedText)) continue;
            
            var analysis = analyzeTitle(normalizedText);
            if (analysis !== null) {
                if (analysis.type == 'number_only' && analysis.numeralFormat != 'arabic' && analysis.numeralFormat != 'roman') continue;
                
                var matchesPattern = false;
                if (dominantPattern.type == 'marker_numeral') {
                    if (analysis.type == 'marker_numeral' && analysis.marker == dominantPattern.marker) {
                        if (!dominantPattern.numeralFormat || analysis.numeralFormat == dominantPattern.numeralFormat) matchesPattern = true;
                    }
                } else if (dominantPattern.type == 'numeral_marker') {
                    if (analysis.type == 'numeral_marker' && analysis.marker == dominantPattern.marker) {
                        if (!dominantPattern.numeralFormat || analysis.numeralFormat == dominantPattern.numeralFormat) matchesPattern = true;
                    }
                } else if (dominantPattern.type == 'number_only') {
                    if (analysis.type == 'number_only' && (!dominantPattern.numeralFormat || analysis.numeralFormat == dominantPattern.numeralFormat)) matchesPattern = true;
                }
                
                if (matchesPattern) {
                    var trimmedText = normalizedText.replace(/^\s+|\s+$/g, '');
                    var analysisText = analysis.originalText.replace(/^\s+|\s+$/g, '');
                    if (trimmedText.indexOf(analysisText) === 0) {
                        var remainingText = trimmedText.substring(analysisText.length).replace(/^\s+/, '');
                        var remainingWords = splitIntoWords(remainingText);
                        if (remainingWords.length > 10 || remainingText.length > 50) continue;
                        
                        lostTitles.push({element: p, text: normalizedText, analysis: analysis, position: 'beginning', context: getContextAround(p)});
                        stats.lostTitlesFound++;
                    }
                }
            }
        }
        return lostTitles;
    }

    // Контекст
    function getContextAround(element) {
        var context = {before: "", current: getTextFromElement(element), after: ""};
        if (context.current.length > 60) context.current = context.current.substring(0, 57) + "...";
        
        var prevElement = element.previousSibling;
        while (prevElement && prevElement.nodeName != 'P') prevElement = prevElement.previousSibling;
        if (prevElement) {
            context.before = getTextFromElement(prevElement);
            if (context.before.length > 50) context.before = "..." + context.before.substring(context.before.length - 50);
        }
        
        var nextElement = element.nextSibling;
        while (nextElement && nextElement.nodeName != 'P') nextElement = nextElement.nextSibling;
        if (nextElement) {
            context.after = getTextFromElement(nextElement);
            if (context.after.length > 50) context.after = context.after.substring(0, 50) + "...";
        }
        return context;
    }

    // Отчет о потерянных
    function showLostTitlesReport(lostTitles) {
        if (lostTitles.length === 0) return "";
        var report = "\n\n\u26A0 ОБНАРУЖЕНЫ ПОТЕРЯННЫЕ ЗАГОЛОВКИ:\n----------------------------------------\n\n";
        var maxToShow = Math.min(lostTitles.length, 5);
        for (var i = 0; i < maxToShow; i++) {
            var lost = lostTitles[i];
            report += (i + 1) + ". " + (lost.position == 'beginning' ? "В начале абзаца: " : "В конце абзаца: ");
            var titleText = lost.analysis.originalText;
            if (titleText.length > 40) titleText = titleText.substring(0, 37) + "...";
            report += "'" + titleText + "'\n\n";
            if (lost.context.before) report += "Контекст (перед): " + lost.context.before + "\n";
            report += "\u2192 " + lost.context.current + "\n";
            if (lost.context.after) report += "Контекст (после): " + lost.context.after + "\n";
            report += "\n----------------------------------------\n\n";
        }
        if (lostTitles.length > 5) report += "... и еще " + (lostTitles.length - 5) + " потерянных заголовков\n\n";
        report += "\u2717 Пожалуйста, разметьте эти заголовки вручную\n   (добавьте DIV с классом 'title' вокруг этих абзацев)\n   и затем повторно запустите скрипт.\n";
        return report;
    }

    // Исправление заголовка
    function fixTitle(titleInfo, correctNumber) {
        var pElement = titleInfo.pElement;
        var analysis = titleInfo.analysis;
        if (!pElement || !analysis || !analysis.words) return false;
        
        var savedNotes = [];
        var childNodes = pElement.childNodes;
        for (var i = 0; i < childNodes.length; i++) {
            var child = childNodes[i];
            if (child.nodeType == 1 && child.nodeName == 'A' && child.className == 'note') {
                savedNotes.push({innerHTML: child.innerHTML, href: child.getAttribute('href') || ''});
            }
        }
        
        var newNumeral = formatNumber(correctNumber, analysis.numeralForm, analysis.numeralFormat);
        var newText = "";
        var words = analysis.words;
        
        if (analysis.type == 'marker_numeral') {
            newText = words[0] + ' ' + newNumeral;
            if (analysis.punctuationAfter) newText += analysis.punctuationAfter;
            var numeralWordCount = analysis.numeralForm.indexOf(' ') !== -1 ? analysis.numeralForm.split(' ').length : 1;
            for (var i = 1 + numeralWordCount; i < words.length; i++) newText += ' ' + words[i];
        } else if (analysis.type == 'numeral_marker') {
            newText = newNumeral + ' ' + analysis.marker;
            if (analysis.punctuationAfter) newText += analysis.punctuationAfter;
            var numeralWordCount = analysis.numeralForm.indexOf(' ') !== -1 ? analysis.numeralForm.split(' ').length : 1;
            for (var i = numeralWordCount + 1; i < words.length; i++) newText += ' ' + words[i];
        } else if (analysis.type == 'number_only') {
            newText = newNumeral;
            if (analysis.punctuationAfter) newText += analysis.punctuationAfter;
            var numeralWordCount = analysis.numeralForm.indexOf(' ') !== -1 ? analysis.numeralForm.split(' ').length : 1;
            for (var i = numeralWordCount; i < words.length; i++) newText += ' ' + words[i];
        }
        
        while (pElement.firstChild) pElement.removeChild(pElement.firstChild);
        pElement.appendChild(document.createTextNode(newText));
        
        for (var i = 0; i < savedNotes.length; i++) {
            var note = savedNotes[i];
            var noteElement = document.createElement('A');
            noteElement.className = 'note';
            noteElement.setAttribute('href', note.href);
            noteElement.innerHTML = note.innerHTML;
            pElement.appendChild(document.createTextNode(' '));
            pElement.appendChild(noteElement);
        }
        return true;
    }

    // Поиск в абзацах
    function searchInParagraphs(missingNumbers, currentPattern) {
        var foundInParagraphs = [];
        if (!currentPattern) return foundInParagraphs;
        
        var allParagraphs = document.getElementsByTagName('p');
        var targetFormat = currentPattern.numeralFormat || 'arabic';
        
        for (var i = 0; i < allParagraphs.length; i++) {
            var p = allParagraphs[i];
            var parent = p.parentNode;
            var isTitle = false;
            while (parent) {
                if (parent.nodeName == 'DIV' && parent.className && typeof parent.className == 'string') {
                    var classes = parent.className.split(' ');
                    for (var c = 0; c < classes.length; c++) {
                        if (classes[c] == 'title') { isTitle = true; break; }
                    }
                    if (isTitle) break;
                }
                parent = parent.parentNode;
            }
            if (isTitle) continue;
            
            var text = getTextFromElement(p);
            var normalizedText = normalizeSpaces(text, nbspEntity);
            if (startsWithDialogue(normalizedText)) continue;
            
            for (var j = 0; j < missingNumbers.length; j++) {
                var missingNum = missingNumbers[j];
                var searchPatterns = [];
                
                if (targetFormat == 'arabic') {
                    searchPatterns.push(missingNum.toString());
                } else if (targetFormat == 'roman') {
                    for (var roman in romanNumerals) { if (romanNumerals[roman] == missingNum) searchPatterns.push(roman); }
                } else if (targetFormat == 'russian_word') {
                    for (var numeral in russianNumerals) { if (russianNumerals[numeral] == missingNum) searchPatterns.push(numeral); }
                } else if (targetFormat == 'english_word') {
                    for (var numeral in englishNumerals) { if (englishNumerals[numeral] == missingNum) searchPatterns.push(numeral); }
                }
                if (currentPattern.marker && targetFormat == 'arabic') searchPatterns.push(currentPattern.marker + ' ' + missingNum);
                
                for (var k = 0; k < searchPatterns.length; k++) {
                    var pattern = searchPatterns[k];
                    var lowerText = normalizedText.toLowerCase();
                    var lowerPattern = pattern.toLowerCase();
                    var patternIndex = lowerText.indexOf(lowerPattern);
                    
                    if (patternIndex >= 0 && patternIndex <= 5) {
                        if (targetFormat == 'arabic') {
                            var before = lowerText.substring(Math.max(0, patternIndex - 4), patternIndex);
                            var after = lowerText.substring(patternIndex + lowerPattern.length, Math.min(lowerText.length, patternIndex + lowerPattern.length + 4));
                            if (missingNum > 1900 || before.indexOf('год') >= 0 || after.indexOf('год') >= 0) continue;
                        }
                        foundInParagraphs.push({number: missingNum, text: normalizedText, element: p});
                        break;
                    }
                }
            }
        }
        return foundInParagraphs;
    }

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    var startTime = new Date();
    var allDivs = document.getElementsByTagName('div');
    var globalIndex = 0;
    
    for (var i = 0; i < allDivs.length; i++) {
        var div = allDivs[i];
        var inNotesSection = false;
        var inCommentsSection = false;
        var parent = div.parentNode;
        while (parent) {
            if (parent.nodeName == 'DIV' && parent.className == 'body') {
                var fbname = parent.getAttribute('fbname');
                if (fbname == 'notes') inNotesSection = true;
                if (fbname == 'comments') inCommentsSection = true;
                break;
            }
            parent = parent.parentNode;
        }
        
        // Пропускаем сноски/комментарии если не задано их обрабатывать
        if (inNotesSection && !processNotesSection) {
            if (div.className && typeof div.className == 'string') {
                var classes = div.className.split(' ');
                for (var j = 0; j < classes.length; j++) {
                    if (classes[j] == 'title') { stats.titlesInNotes++; break; }
                }
            }
            continue;
        }
        if (inCommentsSection && !processCommentsSection) {
            if (div.className && typeof div.className == 'string') {
                var classes = div.className.split(' ');
                for (var j = 0; j < classes.length; j++) {
                    if (classes[j] == 'title') { stats.titlesInNotes++; break; }
                }
            }
            continue;
        }
        
        if (div.className && typeof div.className == 'string') {
            var classes = div.className.split(' ');
            var isTitle = false;
            for (var j = 0; j < classes.length; j++) {
                if (classes[j] == 'title') { isTitle = true; break; }
            }
            if (isTitle) {
                stats.totalTitles++;
                var pElements = div.getElementsByTagName('p');
                if (pElements.length > 0) {
                    var firstP = pElements[0];
                    var text = getTextFromElement(firstP);
                    if (!startsWithDialogue(text)) {
                        var analysis = analyzeTitle(text);
                        if (analysis !== null) {
                            stats.numberedTitles++;
                            allTitles.push({element: div, pElement: firstP, text: text, analysis: analysis, index: globalIndex});
                        }
                    }
                }
                globalIndex++;
            }
        }
    }

    var dominantPattern = determineDominantPattern(allTitles);
    var allMarkersInDoc = [];
    for (i = 0; i < allTitles.length; i++) {
        var title = allTitles[i];
        if (title && title.analysis && title.analysis.marker) {
            var mrk = title.analysis.marker.toLowerCase();
            var alreadyExists = false;
            for (var m = 0; m < allMarkersInDoc.length; m++) {
                if (allMarkersInDoc[m] == mrk) { alreadyExists = true; break; }
            }
            if (!alreadyExists) allMarkersInDoc.push(mrk);
        }
    }

    var allTitlesSorted = [];
    for (i = 0; i < allTitles.length; i++) allTitlesSorted.push(allTitles[i]);
    for (i = 0; i < allTitlesSorted.length - 1; i++) {
        for (var j = 0; j < allTitlesSorted.length - i - 1; j++) {
            if (allTitlesSorted[j].index > allTitlesSorted[j + 1].index) {
                var temp = allTitlesSorted[j];
                allTitlesSorted[j] = allTitlesSorted[j + 1];
                allTitlesSorted[j + 1] = temp;
            }
        }
    }

    if (allTitles.length > 0) {
        var patternGroups = {};
        for (i = 0; i < allTitles.length; i++) {
            var title = allTitles[i];
            if (!title || !title.analysis) continue;
            var patternKey = title.analysis.type;
            if (title.analysis.type == 'marker_numeral' || title.analysis.type == 'numeral_marker') patternKey += '_' + title.analysis.marker;
            if (title.analysis.numeralFormat) patternKey += '_' + title.analysis.numeralFormat;
            if (!patternGroups[patternKey]) patternGroups[patternKey] = [];
            patternGroups[patternKey].push(title);
        }
        
        for (var patternKey in patternGroups) {
            var group = patternGroups[patternKey];
            if (!group || group.length === 0) continue;
            group = sortByIndex(group);
            var sequenceCheck = checkSequence(group, allMarkersInDoc, allTitlesSorted);
            for (i = 0; i < sequenceCheck.errors.length; i++) { errors.push(sequenceCheck.errors[i]); stats.errorsFound++; }
            for (i = 0; i < sequenceCheck.corrections.length; i++) correctionMap.push(sequenceCheck.corrections[i]);
        }
    }

    if (stats.errorsFound > 0) lostTitles = findLostTitles(dominantPattern);

    var totalTime = new Date() - startTime;
    var timeSeconds = (totalTime / 1000).toFixed(2).replace('.', ',');

    // Формирование сообщения
    var message = "---------------------------\n" + scriptName + "\nver. " + scriptVersion + "\n---------------------------\n\n" +
                 "Статистика проверки:\n\nВсего заголовков: " + (stats.totalTitles + stats.titlesInNotes) + "\n" +
                 "  • в основном разделе: " + stats.totalTitles + "\n" +
                 "  • в сносках-примечаниях: " + stats.titlesInNotes + " (все исключены)\n" +
                 "  • с нумерацией: " + stats.numberedTitles + "\n";
    
    if (stats.errorsFound == 0) {
        message += "\n✓ Ошибок не обнаружено\nНумерация заголовков правильная!\n\n";
        message += "\nВремя проверки: " + timeSeconds + " сек\n---------------------------";
        
        if (showStatistics == 1) MsgBox(message, "FBE скрипт");
        return;
    }
    
    if (lostTitles.length > 0) {
        message += showLostTitlesReport(lostTitles);
        message += "\n✗ Автоматическое исправление невозможно!\n   Разметьте потерянные заголовки вручную и повторно запустите скрипт.\n\nВремя проверки: " + timeSeconds + " сек\n---------------------------";
        MsgBox(message, "FBE скрипт");
        return;
    }
    
    message += "\n✗ Найдено ошибок в нумерации: " + stats.errorsFound + "\n\n";
    if (errors.length > 0) {
        message += "Список ошибок:\n";
        for (i = 0; i < Math.min(errors.length, 10); i++) {
            var error = errors[i];
            if (error && error.title && error.title.analysis) {
                message += (i + 1) + ". Позиция " + error.position + ": Ожидалось " + error.expected + ", найдено " + error.actual + " (" + error.title.analysis.originalText.substring(0, 30);
                if (error.title.analysis.originalText.length > 30) message += "...";
                message += ")\n";
            }
        }
        if (errors.length > 10) message += "... и еще " + (errors.length - 10) + " ошибок\n";
        message += "\n";
    }
    
    if (allTitles.length > 0 && errors.length > 0) {
        var missingNumbers = [];
        for (i = 0; i < errors.length; i++) {
            if (errors[i] && errors[i].expected) missingNumbers.push(errors[i].expected);
        }
        
        var currentPattern = dominantPattern || (allTitles[0] && allTitles[0].analysis ? allTitles[0].analysis : null);
        var foundInParagraphs = searchInParagraphs(missingNumbers, currentPattern);
        
        if (foundInParagraphs.length > 0) {
            message += "Найдено в обычных абзацах:\n";
            for (i = 0; i < Math.min(foundInParagraphs.length, 5); i++) {
                var found = foundInParagraphs[i];
                message += "• " + found.text.substring(0, 50);
                if (found.text.length > 50) message += "...";
                message += "\n";
            }
            if (foundInParagraphs.length > 5) message += "... и еще " + (foundInParagraphs.length - 5) + " совпадений\n";
            message += "\n";
        }
    }
    
    var declineWord = declineTitleWord(correctionMap.length);
    message += "Исправить нумерацию автоматически?\nБудет исправлено: " + correctionMap.length + " " + declineWord;
    message += "\nВремя проверки: " + timeSeconds + " сек\n---------------------------";

    if (!confirm(message)) {
        if (showStatistics == 1) MsgBox(scriptName + "\nver. " + scriptVersion + "\n\nИсправление отменено пользователем.", "FBE скрипт");
        return;
    }

    // Таймер запускаем после confirm
    var fixStartTime = new Date();
    
    window.external.BeginUndoUnit(document, scriptName + " - исправление нумерации");
    
    for (i = 0; i < correctionMap.length; i++) {
        var correction = correctionMap[i];
        if (correction && correction.title && fixTitle(correction.title, correction.correctNumber)) stats.fixed++;
    }
    
    window.external.EndUndoUnit(document);
    
    var fixTime = new Date() - fixStartTime;
    var fixTimeSeconds = (fixTime / 1000).toFixed(2).replace('.', ',');

    var resultMessage = "---------------------------\n" + scriptName + "\nver. " + scriptVersion + "\n---------------------------\n\n" +
                       "Результат исправления:\n\n✓ Исправлено заголовков: " + stats.fixed + " из " + correctionMap.length + "\n" +
                       "✗ Ошибок при исправлении: " + (correctionMap.length - stats.fixed) + "\n\n" +
                       "Время исправления: " + fixTimeSeconds + " сек\n" +
                       "Общее время работы: " + ((totalTime + fixTime) / 1000).toFixed(2).replace('.', ',') + " сек\n---------------------------";

    if (showStatistics == 1) MsgBox(resultMessage, "FBE скрипт");
}
