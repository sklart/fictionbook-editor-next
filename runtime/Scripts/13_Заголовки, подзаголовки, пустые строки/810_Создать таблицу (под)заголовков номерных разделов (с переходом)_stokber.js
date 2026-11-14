// Скрипт "Создать таблицу (под)заголовков номерных разделов"
// версия 1.7
// Изменения:
// - ячейки с позицией стали кликабельными (аналогично скрипту для одного регекспа)
// - при клике выделяется соответствующий фрагмент в основном окне редактора
// - добавлен стиль для кликабельных ячеек

function Run() {

    // буквы какого регистра считать римскими числами?
    var registr = 0;
    // 0 - только для верхнего регистра.
    // 1 - для обеих регистров.
    // 2 - только для нижнего регистра.
    var reg; // регексп регистра для римских чисел.

    var name = "Создать таблицу (под)заголовков номерных разделов";
    var vers = "1.7";

    var question = "Нажмите 'ОК', если хотите создать таблицу Заголовков или 'Отмена', если хотите создать таблицу Подзаголовков";
    var razdel = confirm(question);
    var razdels;
    var razdelov;
    var razdela;
    if (razdel === true) {
        razdels = "Заголовки";
        razdelov = "ЗАГОЛОВКОВ";
        razdela = "заголовка";
    }
    if (razdel === false) {
        razdels = "Подзаголовки";
        razdelov = "ПОДЗАГОЛОВКОВ";
        razdela = "подзаголовка";
    }

    var color0 = "darkseagreen"; // цвет заголовка таблицы.
    var color1 = "DarkKhaki"; // цвет заголовка каждого 1-го раздела (главы, части, книги и т .п.)
    var color2 = "Khaki"; // цвет ячейки заголовка очередной главы.
    var color3 = "tomato"; // цвет ячейки перед ячейкой с нарушением порядка нумерации заголовков. 
    var result;
    var perevod = "#"; // ?????????
    var metka1 = "☺"; // метка начала заголовка.
    var metka2 = "☻"; // метка конца заголовка.
    var metka3 = "㋛"; // метка пустой строки.
    var select = 0; // количество символов для выделения заголовка при помощи скрипта "Выделить фрагмент по позиции из буфера обмена".
    var help = "<p><strong>СПРАВКА:</strong><br><small>Красным фоном выделены ячейки с нарушением порядка нумерации заголовков.<br>Цветом хаки выделены ячейки с номерами эквивалентными единице. Такие ячейки могут указывать на ошибки, только если нумерация заголовков сквозная, а не начинается в новых частях документа заново.<br> Для перехода к заголовку кликните по соответствующей ячейке <strong>Позиция</strong>.</small></p>";

    // ... (остальные функции и переменные без изменений) ...

    // ========== ФУНКЦИИ ДЛЯ ВЫДЕЛЕНИЯ ТЕКСТА (аналогично скрипту для регекспа) ==========
    function escapeHtml(str) {
        if (!str) return "";
        return str.replace(/&/g, "&amp;")
                  .replace(/</g, "&lt;")
                  .replace(/>/g, "&gt;");
    }
    // =====================================

	   // числительные мужского рода:
    var numMasc = "(первый|второй|третий|четв[её]ртый|пятый|шестой|седьмой|восьмой|девятый|десятый|одиннадцатый|двенадцатый|тринадцатый|четырнадцатый|пятнадцатый|шестнадцатый|семнадцатый|восемнадцатый|девятнадцатый|двадцатый|двадцать|тридцатый|тридцать|сороковой|сорок|пятидесятый|пятьдесят|шестидесятый|шестьдесят|семидесятый|семьдесят|восьмидесятый|восемьдесят|девяностый|девяносто|сотый|сто|двухсотый|двести|тр[её]хсотый|триста|четыр[её]хсотый|четыреста|пятисотый|пятьсот|шестисотый|шестьсот|семисотый|семьсот|восьмисотый|восемьсот|девятисотый|девятьсот|тысячный| )+";

    // числительные женского рода:
    var numFem = "(первая|вторая|третья|четв[её]ртая|пятая|шестая|седьмая|восьмая|девятая|десятая|одиннадцатая|двенадцатая|тринадцатая|четырнадцатая|пятнадцатая|шестнадцатая|семнадцатая|восемнадцатая|девятнадцатая|двадцатая|двадцать|тридцатая|тридцать|сороковая|сорок|пятидесятая|пятьдесят|шестидесятая|шестьдесят|семидесятая|семьдесят|восьмидесятая|восемьдесят|девяностая|девяносто|сотая|сто|двухсотая|двести|тр[её]хсотая|триста|четыр[её]хсотая|четыреста|пятисотая|пятьсот|шестисотая|шестьсот|семисотая|семьсот|восьмисотая|восемьсот|девятисотая|девятьсот| )+";

    // числительные среднего рода:
    var numNeuter = "(первое|второе|третье|четв[её]ртое|пятое|шестое|седьмое|восьмое|девятое|десятое|одиннадцатое|двенадцатое|тринадцатое|четырнадцатое|пятнадцатое|шестнадцатое|семнадцатое|восемнадцатое|девятнадцатое|двадцатое|двадцать|тридцатое|тридцать|сороковое|сорок|пятидесятое|пятьдесят|шестидесятое|шестьдесят|семидесятое|семьдесят|восьмидесятое|восемьдесят|девяностое|девяносто|сотое|сто|двухсотое|двести|тр[её]хсотое|триста|четыр[её]хсотое|четыреста|пятисотое|пятьсот|шестисотое|шестьсот|семисотое|семьсот|восьмисотое|восемьсот|девятисотое|девятьсот|тысячное| )+";

    // валидные римские числа:
    var numRom = "M{0,3}(CM|CD|D?C{0,3})(XC|XL|L?X{0,3})(IX|IV|V?I{0,3})";
    // var numRom = "[IVXLCDM]+";

    // функция конвертации числительных в арабские числа:
    function convertOrdinalToNumber(numReal) {
        // Удаляем возможные пробелы в начале и конце:
        var str = numReal.replace(/^\s+/, '').replace(/\s+$/, '');

        // Массив регулярных выражений для проверки формата:
        var patterns = [
            "^(перв(ая|ый|ое)|втор(ая|ой|ое)|трет(ья|ий|ье)|четв[её]рт(ая|ый|ое)|пят(ая|ый|ое)|шест(ая|ой|ое)|седьм(ая|ой|ое)|восьм(ая|ой|ое)|девят(ая|ый|ое))$",
            "^(десят(ая|ый|ое)|одиннадцат(ая|ый|ое)|двенадцат(ая|ый|ое)|тринадцат(ая|ый|ое)|четырнадцат(ая|ый|ое)|пятнадцат(ая|ый|ое)|шестнадцат(ая|ый|ое)|семнадцат(ая|ый|ое)|восемнадцат(ая|ый|ое)|девятнадцат(ая|ый|ое)|двадцат(ая|ый|ое)|тридцат(ая|ый|ое)|сороков(ая|ый|ое)|пятидесят(ая|ый|ое)|шестидесят(ая|ый|ое)|семидесят(ая|ый|ое)|восьмидесят(ая|ый|ое)|девяност(ая|ый|ое))$",
            "^(сто|двести|триста|четыреста|пятьсот|шестьсот|семьсот|восемьсот|девятьсот)[ ](двадцать|тридцать|сорок|пятьдесят|шестьдесят|семьдесят|восемьдесят|девяносто)[ ](перв(ая|ый|ое)|втор(ая|ой|ое)|трет(ья|ий|ье)|четв[её]рт(ая|ый|ое)|пят(ая|ый|ое)|шест(ая|ой|ое)|седьм(ая|ой|ое)|восьм(ая|ой|ое)|девят(ая|ый|ое))$",
            "^(сто|двести|триста|четыреста|пятьсот|шестьсот|семьсот|восемьсот|девятьсот)[ ](десят(ая|ый|ое)|одиннадцат(ая|ый|ое)|двенадцат(ая|ый|ое)|тринадцат(ая|ый|ое)|четырнадцат(ая|ый|ое)|пятнадцат(ая|ый|ое)|шестнадцат(ая|ый|ое)|семнадцат(ая|ый|ое)|восемнадцат(ая|ый|ое)|девятнадцат(ая|ый|ое)|двадцат(ая|ый|ое)|тридцат(ая|ый|ое)|сороков(ая|ый|ое)|пятидесят(ая|ый|ое)|шестидесят(ая|ый|ое)|семидесят(ая|ый|ое)|восьмидесят(ая|ый|ое)|девяност(ая|ый|ое))$",
            "^(сто|двести|триста|четыреста|пятьсот|шестьсот|семьсот|восемьсот|девятьсот)[ ](перв(ая|ый|ое)|втор(ая|ой|ое)|трет(ья|ий|ье)|четв[её]рт(ая|ый|ое)|пят(ая|ый|ое)|шест(ая|ой|ое)|седьм(ая|ой|ое)|восьм(ая|ой|ое)|девят(ая|ый|ое))$",
            "^(двадцать|тридцать|сорок|пятьдесят|шестьдесят|семьдесят|восемьдесят|девяносто)[ ](перв(ая|ый|ое)|втор(ая|ой|ое)|трет(ья|ий|ье)|четв[её]рт(ая|ый|ое)|пят(ая|ый|ое)|шест(ая|ой|ое)|седьм(ая|ой|ое)|восьм(ая|ой|ое)|девят(ая|ый|ое))$",
            "^(сот(ая|ый|ое)|двухсот(ая|ый|ое)|тр[её]хсот(ая|ый|ое)|четыр[её]хсот(ая|ый|ое)|пятисот(ая|ый|ое)|шестисот(ая|ый|ое)|семисот(ая|ый|ое)|восьмисот(ая|ый|ое)|девятисот(ая|ый|ое))$"

        ];

        // Проверяем, соответствует ли текст одному из шаблонов:
        var isValid = false;
        for (var i = 0; i < patterns.length; i++) {
            var regex = new RegExp(patterns[i]);
            if (regex.test(str)) {
                isValid = true;
                break;
            }
        }

        if (!isValid) {
            return null; // или можно выбросить исключение.
        }

        // Таблица замен для преобразования:
        var replacements = [{
                pattern: "пятидесят(ая|ый|ое)",
                replacement: "50"
            },
            {
                pattern: "шестидесят(ая|ый|ое)",
                replacement: "60"
            },
            {
                pattern: "семидесят(ая|ый|ое)",
                replacement: "70"
            },
            {
                pattern: "восьмидесят(ая|ый|ое)",
                replacement: "80"
            },
            {
                pattern: "перв(ая|ый|ое)",
                replacement: "1"
            },
            {
                pattern: "втор(ая|ой|ое)",
                replacement: "2"
            },
            {
                pattern: "трет(ья|ий|ье)",
                replacement: "3"
            },
            {
                pattern: "четв[её]рт(ая|ый|ое)",
                replacement: "4"
            },
            {
                pattern: "пят(ая|ый|ое)",
                replacement: "5"
            },
            {
                pattern: "шест(ая|ой|ое)",
                replacement: "6"
            },
            {
                pattern: "восьм(ая|ой|ое)",
                replacement: "8"
            },
            {
                pattern: "седьм(ая|ой|ое)",
                replacement: "7"
            },

            {
                pattern: "девят(ая|ый|ое)",
                replacement: "9"
            },
            {
                pattern: "десят(ая|ый|ое)",
                replacement: "10"
            },
            {
                pattern: "одиннадцат(ая|ый|ое)",
                replacement: "11"
            },
            {
                pattern: "двенадцат(ая|ый|ое)",
                replacement: "12"
            },
            {
                pattern: "тринадцат(ая|ый|ое)",
                replacement: "13"
            },
            {
                pattern: "четырнадцат(ая|ый|ое)",
                replacement: "14"
            },
            {
                pattern: "пятнадцат(ая|ый|ое)",
                replacement: "15"
            },
            {
                pattern: "шестнадцат(ая|ый|ое)",
                replacement: "16"
            },
            {
                pattern: "восемнадцат(ая|ый|ое)",
                replacement: "18"
            },
            {
                pattern: "семнадцат(ая|ый|ое)",
                replacement: "17"
            },

            {
                pattern: "девятнадцат(ая|ый|ое)",
                replacement: "19"
            },
            {
                pattern: "двадцат(ая|ый|ое)",
                replacement: "20"
            },
            {
                pattern: "тридцат(ая|ый|ое)",
                replacement: "30"
            },
            {
                pattern: "сороков(ая|ый|ое)",
                replacement: "40"
            },

            {
                pattern: "девяност(ая|ый|ое)",
                replacement: "90"
            },

            {
                pattern: "двухсот(ая|ый|ое)",
                replacement: "200"
            },
            {
                pattern: "тр[её]хсот(ая|ый|ое)",
                replacement: "300"
            },
            {
                pattern: "четыр[её]хсот(ая|ый|ое)",
                replacement: "400"
            },
            {
                pattern: "пятисот(ая|ый|ое)",
                replacement: "500"
            },
            {
                pattern: "шестисот(ая|ый|ое)",
                replacement: "600"
            },
            {
                pattern: "восьмисот(ая|ый|ое)",
                replacement: "800"
            },
            {
                pattern: "семисот(ая|ый|ое)",
                replacement: "700"
            },


            {
                pattern: "девятисот(ая|ый|ое)",
                replacement: "900"
            },
            {
                pattern: "сот(ая|ый|ое)",
                replacement: "100"
            },
            {
                pattern: "двадцать[ ]",
                replacement: "2"
            },
            {
                pattern: "тридцать[ ]",
                replacement: "3"
            },
            {
                pattern: "сорок[ ]",
                replacement: "4"
            },
            {
                pattern: "пятьдесят[ ]",
                replacement: "5"
            },
            {
                pattern: "шестьдесят[ ]",
                replacement: "6"
            },
            {
                pattern: "восемьдесят[ ]",
                replacement: "8"
            },
            {
                pattern: "семьдесят[ ]",
                replacement: "7"
            },

            {
                pattern: "девяносто[ ]",
                replacement: "9"
            },
            {
                pattern: "^сто[ ]([1-9])$",
                replacement: "10$1"
            },
            {
                pattern: "^двести[ ]([1-9])$",
                replacement: "20$1"
            },
            {
                pattern: "^триста[ ]([1-9])$",
                replacement: "30$1"
            },
            {
                pattern: "^четыреста[ ]([1-9])$",
                replacement: "40$1"
            },
            {
                pattern: "^пятьсот[ ]([1-9])$",
                replacement: "50$1"
            },
            {
                pattern: "^шестьсот[ ]([1-9])$",
                replacement: "60$1"
            },
            {
                pattern: "^восемьсот[ ]([1-9])$",
                replacement: "80$1"
            },
            {
                pattern: "^семьсот[ ]([1-9])$",
                replacement: "70$1"
            },

            {
                pattern: "^девятьсот[ ]([1-9])$",
                replacement: "90$1"
            },
            {
                pattern: "сто[ ]",
                replacement: "1"
            },
            {
                pattern: "двести[ ]",
                replacement: "2"
            },
            {
                pattern: "триста[ ]",
                replacement: "3"
            },
            {
                pattern: "четыреста[ ]",
                replacement: "4"
            },
            {
                pattern: "пятьсот[ ]",
                replacement: "5"
            },
            {
                pattern: "шестьсот[ ]",
                replacement: "6"
            },
            {
                pattern: "восемьсот[ ]",
                replacement: "8"
            },
            {
                pattern: "семьсот[ ]",
                replacement: "7"
            },

            {
                pattern: "девятьсот[ ]",
                replacement: "9"
            }
        ];

        // Применяем замены:
        var result = str;
        for (var j = 0; j < replacements.length; j++) {
            var rep = replacements[j];
            var regex = new RegExp(rep.pattern);

            // Проверяем, содержит ли замена группы захвата:
            if (rep.replacement.indexOf('$1') !== -1) {
                var match = result.match(regex);
                if (match) {
                    result = rep.replacement.replace('$1', match[1]);
                }
            } else {
                result = result.replace(regex, rep.replacement);
            }
        }

        // Удаляем оставшиеся пробелы и возвращаем результат:
        return result.replace(/\s+/g, '');
    }

    // функция конвертации римских чисел в арабские:
    function romanToArabic(numReal) {

        // Таблица соответствия римских цифр значениям:
        var romanMap = {
            'I': 1,
            'V': 5,
            'X': 10,
            'L': 50,
            'C': 100,
            'D': 500,
            'M': 1000
        };

        var result = 0;
        var prevValue = 0;
        var currentValue = 0;

        // Обрабатываем строку с конца к началу:
        for (var i = numReal.length - 1; i >= 0; i--) {
            currentValue = romanMap[numReal.charAt(i)];

            // Если значение не найдено (на всякий случай):
            if (typeof currentValue === 'undefined') {
                return 0;
            }

            // Если текущее значение меньше предыдущего, вычитаем
            // Если больше или равно - прибавляем:
            if (currentValue < prevValue) {
                result -= currentValue;
            } else {
                result += currentValue;
            }
            prevValue = currentValue;
        }
        return result;
    }

    // --------------------------------------
    // блок поиска ошибок нумерации разделов вида "1.1.1."
    function checkParagraphNumbering(str) {
    // Разбиваем строку на строки:
    var lines = str.split('\n');
    var result = [];
    var prevNumbers = null;
    var firstNumberedLineFound = false; // Флаг: найдена ли первая пронумерованная строка.

    for (var i = 0; i < lines.length; i++) {
        var line = lines[i];
        // Извлекаем номер из строки с помощью регулярного выражения:
        var match = line.match(/^(\d+(\.\d+)+\.?)/);

        if (!match) {
            // Если номер не найден, просто добавляем строку в результат
            result.push(line);
            continue;
        }

        var numberStr = match[1];
        var numbers = splitAndConvertToNumbers(numberStr);
        var hasError = false;

        // Правило 5: первая пронумерованная строка должна быть "1.1.":
        if (!firstNumberedLineFound) {
            firstNumberedLineFound = true;
            if (numberStr !== '1.1.') {
                hasError = true;
            }
        } else {
            // Проверяем все 4 правила последовательно — если хотя бы одно подходит, ошибки нет:
            hasError = !checkRule1(prevNumbers, numbers) &&
                       !checkRule2(prevNumbers, numbers) &&
               !checkRule3(prevNumbers, numbers) &&
               !checkRule4(prevNumbers, numbers);
        }

        // Добавляем вопросительный знак, если есть ошибка:
        if (hasError) {
            line = line.replace(numberStr, numberStr + '?');
        }

        result.push(line);
        prevNumbers = numbers;
    }

    return result.join('\n');
}

// Вспомогательная функция для разбиения строки на числа:
function splitAndConvertToNumbers(numberStr) {
    var parts = numberStr.split('.');
    var numbers = [];

    for (var j = 0; j < parts.length; j++) {
        if (parts[j] !== '') {  // Пропускаем пустые строки:
            numbers.push(parseInt(parts[j], 10));
        }
    }

    return numbers;
}

// Правило 1: одинаковое количество уровней, последний номер увеличен на 1:
function checkRule1(prev, curr) {
    if (prev.length !== curr.length) return false;

    for (var k = 0; k < prev.length - 1; k++) {
        if (prev[k] !== curr[k]) return false;
    }
    return curr[prev.length - 1] === prev[prev.length - 1] + 1;
}

// Правило 2: количество уровней в текущей строке больше на 1, все предыдущие уровни совпадают, новый уровень равен 1:
function checkRule2(prev, curr) {
    if (curr.length !== prev.length + 1) return false;

    for (var l = 0; l < prev.length; l++) {
        if (prev[l] !== curr[l]) return false;
    }
    return curr[curr.length - 1] === 1;
}

// Правило 3: количество уровней меньше, все оставшиеся уровни совпадают, последний уровень увеличен на 1:
function checkRule3(prev, curr) {
    if (curr.length >= prev.length) return false;

    var minLen = Math.min(prev.length, curr.length);
    for (var m = 0; m < minLen - 1; m++) {
        if (prev[m] !== curr[m]) return false;
    }
    return curr[minLen - 1] === prev[minLen - 1] + 1;
}

// Правило 4: текущая строка имеет 2 уровня, первый уровень увеличен на 1, второй уровень равен 1:
function checkRule4(prev, curr) {
    if (curr.length !== 2) return false;

    return curr[0] === prev[0] + 1 && curr[1] === 1;
}
    // конец блока поиска ошибок нумерации разделов вида "1.1.1."
    // ================================

    function addLineCounts(text) {
        // Разбиваем текст на строки по символу перевода строки:
        var lines = text.split('\n');
        var result = []; // Массив для итоговых строк.
        var count = 0; // Счетчик непустых строк в текущей группе.

        for (var i = 0; i < lines.length; i++) {
            var line = lines[i];
            // Проверяем, не пустая ли строка (учитываем пробелы):
            if (line.replace(/\s+/g, '').length > 0) {
                // Непустая строка — увеличиваем счетчик:
                count++;
                result.push(line);
            } else {
                // Пустая строка:
                if (count > 0) {
                    // Если до этого были непустые строки — добавляем счетчик:
                    result.push("&nbsp;итого: " + count.toString());
                    count = 0; // Обнуляем счетчик.
                }
                // Добавляем саму пустую строку:
                result.push(line);
            }
        }
        // Если в конце текста не было пустой строки, но были непустые — добавляем счетчик:
        if (count > 0) {
            result.push(count.toString());
        }

        // Собираем строки обратно в текст:
        return result.join('\n');
    }

    var sel = document.getElementById("fbw_body");
    if (!sel) {
        alert("Ошибка: элемент fbw_body не найден");
        return;
    }

	// -------------------------------------------
    //создаём переменную с html-текстом открытого документа:
    var fromHTML = sel.innerHTML;

	
	
    // заменяем невесть откуда попавшие в документ символы выбранные нами в качестве меток заголовков:
    fromHTML = fromHTML.replace(new RegExp(metka1 + "|" + metka2 + "|" + metka3, "g"), "=");

    // избавляемся от разделов примечаний и комментариев:
    fromHTML = fromHTML.replace(new RegExp("<DIV class=body[^>]*? fbname=\\\"(notes|comments)\\\"[\\s\\S]+", "gmi"), "");

	fromHTML = fromHTML.replace(new RegExp("</?(STRONG|EM|SUB|SUP|STRIKE)>", "ig"), ""); // и др.!!!!!!!!!!!!!!
	fromHTML = fromHTML.replace(new RegExp("<A class[^>]+>|</A>", "ig"), "");
	
	// картинка в тексте перед новой секцией:
	fromHTML = fromHTML.replace(new RegExp("<SPAN onresizestart=[^>]+?><IMG\\b[^>]*?></SPAN></P>(</DIV>)*\\r?\\n?<DIV class=section>\\r?\\n?(?=<DIV class=title>)", "gi"), "░░"); //
	fromHTML = fromHTML.replace(new RegExp("<SPAN onresizestart=[^>]+?><IMG\\b[^>]*?></SPAN></P>(</DIV>)*\\r?\\n?<DIV class=section>", "gi"), "░░░"); // если сл. секция с заголовком - оставить 2 символа, если без - 3 символа.
	
    // Преобразование HTML-текста картинок:
	// fromHTML = fromHTML.replace(new RegExp("<SPAN onresizestart=[^>]+?><IMG\\b.*?></SPAN></P></DIV>", "gi"), "@@");
    fromHTML = fromHTML.replace(new RegExp("<SPAN onresizestart=[^>]+?><IMG\\b.*?></SPAN></P></DIV>", "gi"), "░░░");
	// fromHTML = fromHTML.replace(new RegExp("<SPAN onresizestart=[^>]+?><IMG\\b.*?></SPAN></P>", "gi"), "@@@");
    fromHTML = fromHTML.replace(new RegExp("<SPAN onresizestart=[^>]+?><IMG\\b.*?>", "gi"), "░░░");
    fromHTML = fromHTML.replace(new RegExp("<IMG\\b.*?>", "gi"), "~~~#");
	
	fromHTML = fromHTML.replace(new RegExp("<SPAN class=code>(.+?)</SPAN>", "ig"), "$1"); 

    // проставить временную метку пустой строки !!!!!!!!!!!!!
    // заменить метку на более уникальную:
    fromHTML = fromHTML.replace(new RegExp("<P>&nbsp;</P>", "gim"), "<P>" + metka3 + "</P>");

    // добавление меток переводов строк:
    fromHTML = fromHTML.replace(new RegExp("</P>", "gim"), "#$&");

    if (razdel === true) {
        // простановка меток заголовков:
        fromHTML = fromHTML.replace(new RegExp("<DIV class=title>([\\s\\S]*?)</DIV>", "gim"), metka1 + "$1" + metka2);
    }
    if (razdel === false) {
        // простановка меток подзаголовков:
        fromHTML = fromHTML.replace(new RegExp("<P class=subtitle>(.*?)</P>", "gi"), metka1 + "$1" + metka2);
    }
    // удаление переводов строк:
    fromHTML = fromHTML.replace(new RegExp("\r?\n", "gim"), "");

    // Создаем временный элемент для получения текста:
    var tempDiv = document.createElement("div");
    tempDiv.innerHTML = fromHTML;
    var text = tempDiv.innerText;

    // удаление лишних меток: 
    text = text.replace(new RegExp("#" + metka1, "g"), metka1);
    text = text.replace(new RegExp("#" + metka2, "g"), metka2);
    text = text.replace(new RegExp(metka2 + metka1, "g"), metka1);
    text = text.replace(new RegExp("(" + metka1 + "[^" + metka1 + "#" + metka2 + "]+?)#", "g"), "$1" + metka1);
    text = text.replace(new RegExp("(" + metka1 + "[^" + metka1 + "#" + metka2 + "]+?)#", "g"), "$1" + metka1);
    text = text.replace(new RegExp("(" + metka1 + "[^" + metka1 + "#" + metka2 + "]+?)#", "g"), "$1" + metka1);
    // манипуляции с меткой пустой строки: !!!!!!!!!!!!!!
    text = text.replace(new RegExp(metka2 + metka3 + metka1, "g"), metka2 + metka1);
    text = text.replace(new RegExp(metka3, "g"), "");

    // поправка для возможных начальных пробелов в заголовках:
    text = text.replace(new RegExp(metka1 + ("[ ]+"), "g"), "$1" + metka1); // ??????????????

	text = text.replace(/(#~~)~(?=#~~~)/gm, "$1");
	text = text.replace(/(░*)░░░(?=#)/gm, "$1░░");
	
	// ==========================

    var matches = [];
    var match;

    var str = "";

    // вида "Том 1":
    var vid = "Том ([0-9]+?)([\\[\\{. -].+?)?";
    var funk = "zifru";
    vids();
    // вида "Том VII":
    var vid = "Том (" + numRom + ")([\\[\\{. ].+?)?";
    var funk = "rom";
    vids();
    // вида "Том первый":
    var vid = "Том (" + numMasc + ")([\\[\\{., ].+?)?";
    var funk = "masc";
    vids();
    // вида "Первый том":
    var vid = "(" + numMasc + ") том([. ].+?)?";
    var funk = "masc";
    vids();

    // вида "Книга 1":
    var vid = "Книга ([0-9]+?)([\\[\\{. -].+?)?";
    var funk = "zifru";
    vids();
    // вида "Книга VII":
    var vid = "Книга (" + numRom + ")([\\[\\{. ].+?)?";
    var funk = "rom";
    vids();
    // вида "Книга первая":
    var vid = "Книга (" + numFem + ")([\\[\\{., ].+?)?";
    var funk = "fem";
    vids();
    // вида "Первая книга":
    var vid = "(" + numFem + ") книга(\\[\\{[. ].+?)?";
    var funk = "fem";
    vids();

    // вида "Часть 1":
    var vid = "Часть ([0-9]+?)([\\[\\{. -].+?)?";
    var funk = "zifru";
    vids();
    // вида "Часть VII":
    var vid = "Часть (" + numRom + ")([\\[\\{. ].+?)?";
    var funk = "rom";
    vids();
    // вида "Часть первая":
    var vid = "Часть (" + numFem + ")([\\[\\{., ].+?)?";
    var funk = "fem";
    vids();
    // вида "Первая часть":
    var vid = "(" + numFem + ") часть([\\[\\{. ].+?)?";
    var funk = "fem";
    vids();

    // вида "1.1.1":

    var vid = "((?:[0-9]+)(?:[.][0-9]+)+[.]?)(.*?)?";
    var funk = "zifru2";
    vids();

    // вида "1":
    var vid = "([0-9]+)(([\\[\\{ ]|[.](?!\\d)).*?)?";
    var funk = "zifru";
    vids();

    // вида "VII":
    var vid = "(" + numRom + ")([\\[\\{. ].+?)?";
    var funk = "rom";
    vids();

    // вида "Глава 1":
    var vid = "Глава ([0-9]+)([\\[\\{., -].+?)?";
    var funk = "zifru";
    vids();
    // вида "Глава VII":
    var vid = "Глава (" + numRom + ")([\\[\\{. ].+?)?";
    var funk = "rom";
    vids();
    // вида "Глава первая":
    var vid = "Глава (" + numFem + ")([\\[\\{., ].+?)?";
    var funk = "fem";
    vids();
    // вида "Первая глава":
    var vid = "(" + numFem + ") глава([\\[\\{. ].+?)?";
    var funk = "fem";
    vids();

    // вида "Лекция 1":
    var vid = "Лекция ([0-9]+)([\\[\\{., :-].+?)?";
    var funk = "zifru";
    vids();
    // вида "Лекция VII":
    var vid = "Лекция (" + numRom + ")([\\[\\{. :].+?)?";
    var funk = "rom";
    vids();
    // вида "Лекция первая":
    var vid = "Лекция (" + numFem + ")([\\[\\{., :].+?)?";
    var funk = "fem";
    vids();
    // вида "Первая лекция":
    var vid = "(" + numFem + ") лекция([\\[\\{. :].+?)?";
    var funk = "fem";
    vids();

    // вида "Раздел 1":
    var vid = "Раздел ([0-9]+?)([\\[\\{. -].+?)?";
    var funk = "zifru";
    vids();
    // вида "Раздел VII":
    var vid = "Раздел (" + numRom + ")([\\[\\{. ].+?)?";
    var funk = "rom";
    vids();
    // вида "Раздел первый":
    var vid = "Раздел (" + numMasc + ")([\\[\\{., ].+?)?";
    var funk = "masc";
    vids();
    // вида "Первый раздел":
    var vid = "(" + numMasc + ") раздел([\\[\\{. ].+?)?";
    var funk = "masc";
    vids();

    // вида "§ 1":
    var vid = "§ ([0-9]+)([\\[\\{., ].+?)?";
    var funk = "zifru";
    vids();

    // вида "Урок 1":
    var vid = "Урок ([0-9]+)([\\[\\{., :-].+?)?";
    var funk = "zifru";
    vids();
    // вида "Урок VII":
    var vid = "Урок (" + numRom + ")([\\[\\{. :].+?)?";
    var funk = "rom";
    vids();
    // вида "Урок первый":
    var vid = "Урок (" + numMasc + ")([\\[\\{., :].+?)?";
    var funk = "masc";
    vids();
    // вида "Первый урок":
    var vid = "(" + numMasc + ") урок([\\[\\{. :].+?)?";
    var funk = "masc";
    vids();

    // вида "Приложение 1":
    var vid = "Приложение ([0-9]+)([\\[\\{., :-].+?)?";
    var funk = "zifru";
    vids();
    // вида "Приложение VII":
    var vid = "Приложение (" + numRom + ")([\\[\\{. :].+?)?";
    var funk = "rom";
    vids();

    // вида "Занятие 1":
    var vid = "Занятие ([0-9]+)([\\[\\{., :-].+?)?";
    var funk = "zifru";
    vids();
    // вида "Занятие VII":
    var vid = "Занятие (" + numRom + ")([\\[\\{. :].+?)?";
    var funk = "rom";
    vids();
    // вида "Занятие первое":
    var vid = "Занятие (" + numNeuter + ")([\\[\\{., :].+?)?";
    var funk = "neuter";
    vids();
    // вида "Первое занятие":
    var vid = "(" + numNeuter + ") занятие([\\[\\{. :].+?)?";
    var funk = "neuter";
    vids();

    // вида "Задание 1":
    var vid = "Задание ([0-9]+)([\\[\\{., :-].+?)?";
    var funk = "zifru";
    vids();
    // вида "Задание VII":
    var vid = "Задание (" + numRom + ")([\\[\\{. :].+?)?";
    var funk = "rom";
    vids();
    // вида "Задание первое":
    var vid = "Задание (" + numNeuter + ")([\\[\\{., :].+?)?";
    var funk = "neuter";
    vids();
    // вида "Первое задание":
    var vid = "(" + numNeuter + ") задание([\\[\\{. :].+?)?";
    var funk = "neuter";
    vids();

    // вида "Упражнение 1":
    var vid = "Упражнение ([0-9]+)([\\[\\{., :-].+?)?";
    var funk = "zifru";
    vids();
    // вида "Упражнение VII":
    var vid = "Упражнение (" + numRom + ")([\\[\\{. :].+?)?";
    var funk = "rom";
    vids();
    // вида "Упражнение первое":
    var vid = "Упражнение (" + numNeuter + ")([\\[\\{., :].+?)?";
    var funk = "neuter";
    vids();
    // вида "Первое упражнение":
    var vid = "(" + numNeuter + ") упражнение([\\[\\{. :].+?)?";
    var funk = "neuter";
    vids();

    // вида "Рис. 1":
    var vid = "Рис[.] ?([0-9]+)([\\[\\{., ].+?)?";
    var funk = "zifru";
    vids();
    // вида "Таблица 1":
    var vid = "Табл(?:[.]|ица) (?:№ ?)?([0-9]+)([\\[\\{., ].+?)?";
    var funk = "zifru";
    vids();

    // вида "Акт 1":
    var vid = "Акт ([0-9]+?)([\\[\\{. -].+?)?";
    var funk = "zifru";
    vids();
    // вида "Акт VII":
    var vid = "Акт (" + numRom + ")([\\[\\{. ].+?)?";
    var funk = "rom";
    vids();
    // вида "Акт первый":
    var vid = "Акт (" + numMasc + ")([\\[\\{., ].+?)?";
    var funk = "masc";
    vids();
    // вида "Первый акт":
    var vid = "(" + numMasc + ") акт([\\[\\{. ].+?)?";
    var funk = "masc";
    vids();

    // вида "Действие 1":
    var vid = "Действие ([0-9]+)([\\[\\{., -].+?)?";
    var funk = "zifru";
    vids();
    // вида "Действие VII":
    var vid = "Действие (" + numRom + ")([\\[\\{. ].+?)?";
    var funk = "rom";
    vids();
    // вида "Действие первое":
    var vid = "Действие (" + numNeuter + ")([\\[\\{., ].+?)?";
    var funk = "neuter";
    vids();
    // вида "Первое действие":
    var vid = "(" + numNeuter + ") действие([\\[\\{. ].+?)?";
    var funk = "neuter";
    vids();

    // вида "Явление 1":
    var vid = "Явление ([0-9]+)([\\[\\{., -].+?)?";
    var funk = "zifru";
    vids();
    // вида "Явление VII":
    var vid = "Явление (" + numRom + ")([\\[\\{. ].+?)?";
    var funk = "rom";
    vids();
    // вида "Явление первое":
    var vid = "Явление (" + numNeuter + ")([\\[\\{., ].+?)?";
    var funk = "neuter";
    vids();
    // вида "Первое явление":
    var vid = "(" + numNeuter + ") явление([\\[\\{. ].+?)?";
    var funk = "neuter";
    vids();

    // вида "Задача 1":
    var vid = "Задача ([0-9]+)([\\[\\{., -].+?)?";
    var funk = "zifru";
    vids();
    // вида "Задача VII":
    var vid = "Задача (" + numRom + ")([\\[\\{. ].+?)?";
    var funk = "rom";
    vids();
    // вида "Задача первая":
    var vid = "Задача (" + numFem + ")([\\[\\{., ].+?)?";
    var funk = "fem";
    vids();
    // вида "Первая задача":
    var vid = "(" + numFem + ") задача([\\[\\{. ].+?)?";
    var funk = "fem";
    vids();

    // вида "Сцена 1":
    var vid = "Сцена ([0-9]+)([\\[\\{., -].+?)?";
    var funk = "zifru";
    vids();
    // вида "Сцена VII":
    var vid = "Сцена (" + numRom + ")([\\[\\{. ].+?)?";
    var funk = "rom";
    vids();
    // вида "Сцена первая":
    var vid = "Сцена (" + numFem + ")([\\[\\{., ].+?)?";
    var funk = "fem";
    vids();
    // вида "Первая сцена":
    var vid = "(" + numFem + ") сцена([\\[\\{. ].+?)?";
    var funk = "fem";
    vids();

    // вида "Картина 1":
    var vid = "Картина ([0-9]+)([\\[\\{., -].+?)?";
    var funk = "zifru";
    vids();
    // вида "Картина VII":
    var vid = "Картина (" + numRom + ")([\\[\\{. ].+?)?";
    var funk = "rom";
    vids();
    // вида "Картина первая":
    var vid = "Картина (" + numFem + ")([\\[\\{., ].+?)?";
    var funk = "fem";
    vids();
    // вида "Первая картина":
    var vid = "(" + numFem + ") картина([\\[\\{. ].+?)?";
    var funk = "fem";
    vids();

    // вида "Эпизод 1":
    var vid = "Эпизод ([0-9]+?)([\\[\\{. -].+?)?";
    var funk = "zifru";
    vids();
    // вида "Эпизод VII":
    var vid = "Эпизод (" + numRom + ")([\\[\\{. ].+?)?";
    var funk = "rom";
    vids();
    // вида "Эпизод первый":
    var vid = "Эпизод (" + numMasc + ")([\\[\\{., ].+?)?";
    var funk = "masc";
    vids();
    // вида "Первый эпизод":
    var vid = "(" + numMasc + ") эпизод([\\[\\{. ].+?)?";
    var funk = "masc";
    vids();

    function vids() {
        var regexp = new RegExp(metka1 + vid + "(?=[" + metka2 + metka1 + "])", "gi");

        // Ищем все совпадения с позициями:
        while ((match = regexp.exec(text)) !== null) {
            // Удаляем символ metka из найденного совпадения:
            var cleanedMatch = match[0].replace(new RegExp("(" + metka1 + "|" + metka2 + ")", "gi"), "");

            // Номер как есть отдельно в начало строки (игнорируем регистр):
            numReal = cleanedMatch.replace(new RegExp("^" + vid + "$", "gi"), "$1");

            // Увеличиваем позицию на 1 и добавляем количество символов для выделения по позиции:
            var adjustedIndex = (match.index + 1 + 1) + ":" + select; // ????????????????????

            // str += numReal+"\t"+cleanedMatch+"\t"+adjustedIndex+"\n";

            // Сохраняем тройки (номер числом, совпадение, позиция)
            // для римских чисел:
            if (registr === 0) {
                reg = /[^a-z]/;
            }
            if (registr === 1) {
                reg = /[a-z]/i;
            }
            if (registr === 2) {
                reg = /[^A-Z]/;
            }
            if (funk === "rom" && numReal != 0 && reg.test(numReal)) {
                // в верхний регистр:
                numReal = numReal.toUpperCase();
                str += romanToArabic(numReal) + "\t" + cleanedMatch + "\t" + adjustedIndex + "\n";
            }

            if (funk === "fem" || funk === "masc" || funk === "neuter") {
                // в нижний регистр:
                numReal = numReal.toLowerCase();
                str += convertOrdinalToNumber(numReal) + "\t" + cleanedMatch + "\t" + adjustedIndex + "\n";
            }

            if (funk === "zifru") {
                str += numReal + "\t" + cleanedMatch + "\t" + adjustedIndex + "\n";
                // 
            }

            if (funk === "zifru2") {

                // добавить точку в конце номера если её там не оказалось:
                numReal = numReal.replace(new RegExp("[^.]$", "g"), "$0.");

                str += numReal + "\t" + cleanedMatch + "\t" + adjustedIndex + "\n";
                // 
            }
        }

        str = "\n" + str;
        str += "\n";
        str = str.replace(new RegExp("\\n\\n+", "gim"), "\n\n");
    }

    str = str.replace(new RegExp("^([\\s\\S]+)\\n+$", "im"), "$1");
    
    // MsgBox("1:\n"+str);
    
    // --------------------------------------------
// проверка нумерации всех заголовков, кроме заголовков вида "1.1.1.":
function checkNumbering(str) {
    var lines = str.split('\n');
    var resultLines = [];
    var expectedNumber = 1; // Ожидаемое число для первой строки.

    for (var i = 0; i < lines.length; i++) {
        var line = lines[i];
        var match = line.match(/^(\d+(?=\t))/);

        if (match) {
            var currentNumber = parseInt(match[1], 10);
            var isCorrect = false;

            // Правило: если число — единица, строка всегда корректна:
            if (currentNumber === 1) {
                isCorrect = true;
            } else {
                // Для всех остальных чисел проверяем порядок:
                if (i === 0) {
                
                    // Первая строка: должна быть 1 (уже проверено выше, но на всякий случай):
                    isCorrect = (currentNumber === 1);
                } else {
                
                    // Остальные строки: число должно быть на 1 больше предыдущего ожидаемого:
                    isCorrect = (currentNumber === expectedNumber);
                }
            }

            var newLine;
            if (isCorrect) {
                newLine = line;
            } else {
                // Добавляем "?" к числу, если оно неверное:
                newLine = line.replace(/^\d+(?=\t)/, currentNumber + '?');
            }

            resultLines.push(newLine);

            // Обновляем ожидаемое число для следующей строки:
            // следующее должно быть на 1 больше *текущего* числа (даже если оно было помечено ?):
            expectedNumber = currentNumber + 1;
        } else {
            // Если строка не начинается с числа, оставляем её без изменений:
            resultLines.push(line);
        }
    }

    // Собираем строки обратно в одну строку с переводами строк:
    return resultLines.join('\n');
}

    str = checkNumbering(str);
    
    // ====================

    // подсчет всех найденных номерных заголовков:
    var count = (str.match(/\n+/gm) || []).length;
    count--;

    if (count === 0) {
        alert("Номерных " + razdelov + " не найдено!");
        return;
    }

    str = addLineCounts(str);

    // MsgBox("3:\n"+str);
    // clipboardData.setData("Text",str); // поместить данные в буфер обмена.

    var result = checkParagraphNumbering(str);
    str = result;

    // MsgBox("4:\n"+str);

    // ... (всё до формирования таблицы остаётся без изменений) ...

    // Формируем HTML-таблицу:
    // Добавляем стиль для кликабельных ячеек
    var styleBlock = "<style>" +
        ".pos-cell { cursor: pointer; background-color: #eef; }" +
        ".pos-cell:hover { background-color: #ccf; }" +
        "</style>";
    var tableHTML = styleBlock + "<table>";
    tableHTML += "<tr bgcolor=\"" + color0 + "\"><th>Номер</th><th>" + razdels + " (всего " + count + ")</th><th>Позиция</th></tr>";

    var lines = str.split('\n');
    for (var i = 0; i < lines.length; i++) {
        var line = lines[i];

        if (/&nbsp;итого: \d+/.test(line)) {
            // Пустая строка - создаем строку таблицы без ячеек:
            tableHTML += '<tr><td colspan="3"><pre>' + line + '</pre></td></tr>';
            continue;
        }

        var cells = line.split('\t');

        // Обработка строки с табуляциями:
        tableHTML += '<tr>';
        for (var j = 0; j < 3; j++) {
            if (j === 2 && cells[j] !== undefined) {
                // Третья ячейка — позиция:длина. Делаем её кликабельной.
                var posLength = cells[j];
                // Экранируем текст на случай спецсимволов (хотя там только цифры и двоеточие)
                var posLengthEsc = escapeHtml(posLength);
                // Генерируем onclick-код (как в скрипте для одного регекспа)
                var onclickCode = 
                    "var txt = this.innerText || this.textContent; var parts = txt.split(':'); if(parts.length==2) { " +
                    "var p = parseInt(parts[0], 10); var l = parseInt(parts[1], 10); " +
                    // "alert('Позиция: ' + p + '\\nДлина: ' + l); " +
                    "if(window.opener) { window.opener.focus(); " +
                    "try { " +
                    "var fbwBody = window.opener.document.getElementById('fbw_body'); " +
                    "if(fbwBody) { " +
                    "var tr = window.opener.document.body.createTextRange(); " +
                    "tr.moveToElementText(fbwBody); " +
                    "tr.collapse(true); " +
                    "tr.select(); " +
                    "window.opener.scrollTo(0,0); " +
                    "var myRange = window.opener.document.selection.createRange(); " +
                    "myRange.moveStart('character', p); " +
                    "myRange.moveEnd('character', l); " +
                    "myRange.select(); " +
                    "myRange.scrollIntoView(); " +
					"var rect = myRange.getBoundingClientRect ? myRange.getBoundingClientRect() : null; " +
                    
					// "if(rect && (window.opener.document.documentElement.clientHeight - rect.bottom) < 20) { " +
                   // "window.opener.scrollBy(0, 50); " +
				   //поправил на:
				   "if(rect && (window.opener.document.documentElement.clientHeight - rect.bottom) < clientHeight / 2) { " +
				   "var correction = (rect.bottom - document.documentElement.clientHeight / 2);" +
                   "window.opener.scrollBy(0, correction); " +
				   
                   "} " +
                    "} else { alert('Не найден элемент fbw_body'); } " +
                    "} catch(e) { alert('Ошибка выделения: ' + e.message); } " +
                    "} else { alert('Нет доступа к окну редактора'); } " +
                    "} else { alert('Ошибка формата: ' + txt); }";
                tableHTML += '<td class="pos-cell" onclick="' + onclickCode.replace(/"/g, '&quot;') + '">' + posLengthEsc + '</td>';
            } else {
                tableHTML += '<td>';
                if (cells[j] !== undefined) {
                    tableHTML += escapeHtml(cells[j]);
                }
                tableHTML += '</td>';
            }
        }
        tableHTML += '</tr>';
    }
    tableHTML += '</table>';

    // косметика (замена цветов строк) - без изменений
    
	
	// tableHTML = tableHTML.replace(new RegExp("</table><td>(?=[\\d.]+\\?)", "gi"), "<tr bgcolor=\"" + color3 + "\"><td>");
	// исправил на:
	tableHTML = tableHTML.replace(new RegExp("<tr><td>(?=[\\d.]+\\?)", "gi"), "<tr bgcolor=\"" + color3 + "\"><td>");
	
	
    tableHTML = tableHTML.replace(new RegExp("<tr><td>(?=1<)", "gi"), "<tr bgcolor=\"" + color1 + "\"><td>");
    tableHTML = tableHTML.replace(new RegExp("<tr><td>(?=[\\d.]+<)", "gi"), "<tr bgcolor=\"" + color2 + "\"><td>");

    // убрать первый (служебный) столбец (если он был, но у нас уже нет)
    tableHTML = tableHTML.replace(new RegExp("(<tr.+?>)<td>.+?</td>", "gi"), "$1");
    tableHTML = tableHTML.replace(new RegExp("<th>Номер</th>", "gi"), "");

    tableHTML = tableHTML + "\n\n<sub>Скрипт «" + name + "» v." + vers + "</sub>";
    tableHTML = help + tableHTML;

    var okno = 0; // или 1 - для showModelessDialog

    function MyMsgWindow(tableHTML) {
        if (okno == 0) {
            var MsgWindow = window.open("HTML/Таблица совпадений.html", null, "height=680,width=400,status=no,toolbar=no,menubar=no,location=no,scrollbars=yes,resizable=yes");
        }
        if (okno == 1) {
            var MsgWindow = window.showModelessDialog("HTML/Таблица совпадений.html", null, "height=720,width=400,status=no,toolbar=no,menubar=no,location=no,scrollbars=yes,resizable=yes");
        }
        // Выводим таблицу в документ:
        MsgWindow.document.body.innerHTML = tableHTML;
    }
    MyMsgWindow(tableHTML);
}
