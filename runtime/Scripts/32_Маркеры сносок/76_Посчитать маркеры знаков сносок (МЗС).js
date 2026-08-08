// Скрипт "Посчитать маркеры знаков сносок (МЗС)" для редактора FBE
// version 1.6
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для подсчета маркеров знаков сносок (МЗС) в fb2 документе.
// перед последующей расстановкой сносок.
// Пользователь выбирает вид маркера (надстрочный текст, *, #, [1], {1}, [~1~], {~1~}).
// Скрипт находит все маркеры выбранного вида, исключая маркеры текстов сносок (в начале строк)
// и выводит итоговое количество найденных маркеров (МЗС).
// Скрип позволяет быстрее найти несовпадение маркеров МЗС и МТС.

// Скрипт сделан на основе алгоритма скрипта
// "Создать таблицу соответствия маркеров будущих сносок" уважаемого тов. stokber.

// version 1.6, 13.07.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Посчитать маркеры знаков сносок (МЗС)";
    var version = "1.6";

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    var sel = document.getElementById("fbw_body");
    var str = sel.innerHTML;
    var s1 = "☺"; // метка начала маркера сноски
    var s2 = "☻"; // метка конца маркера сноски

    // Проверяем случайное наличие в документе символов ☺ и ☻
    var regexp = new RegExp("(" + s1 + "|" + s2 + ")+", "g");
    if (regexp.test(str)) {
        window.clipboardData.setData("text", "(" + s1 + "|" + s2 + ")+");
        MsgBox(scriptName + "\nver. " + version + "\n---------------------------\n\nВ вашем документе имеются символ(ы) " + s1 + " и(ли) " + s2 + ", которые используются скриптом как временные метки. Для корректной работы рекомендуем на время работы скрипта заменить их на другие символы.\nНайти в документе их можно перейдя в режим Кода и вставив регулярное выражение из буфера обмена в строку поиска окна Поиск (Ctrl+F)\n\nВремя выполнения: 0,000 сек.");
        return;
    }

    // Создаём временный html-файл для диалога выбора маркера
    var fso = new ActiveXObject("Scripting.FileSystemObject");
    var tempPath = fso.GetSpecialFolder(2) + "\\fbe_mzs_dialog_temp.html";
    
    var dialogHTML = '<!DOCTYPE html>\n<html>\n<head>\n <meta http-equiv="Content-Type" content="text/html; charset=windows-1251">\n <meta http-equiv="MSThemeCompatible" content="yes">\n <title>Посчитать маркеры знаков сносок (МЗС)</title>\n <style>\n  body, input{font-family:Tahoma; font-size:16px;margin:2px;}\n </style>\n</head>\n<script>\nfunction markerFunc() {\n if (document.getElementById("radio1").checked) return "число надстрочным текстом";\n if (document.getElementById("radio2").checked) return "вида *";\n if (document.getElementById("radio2a").checked) return "вида #";\n if (document.getElementById("radio3").checked) return "вида [1]";\n if (document.getElementById("radio4").checked) return "вида {1}";\n if (document.getElementById("radio5").checked) return "вида [~1~]";\n if (document.getElementById("radio6").checked) return "вида {~1~}";\n }\n</script>\n<body>\n\nВыберите вид маркеров знаков сносок:<br>\n<label for="radio1"><input type="radio" name="markSign" id="radio1" checked>число надстрочным текстом</label><br>\n<label for="radio2"><input type="radio" name="markSign" id="radio2">вида *</label><br>\n<label for="radio2a"><input type="radio" name="markSign" id="radio2a">вида #</label><br>\n<label for="radio3"><input type="radio" name="markSign" id="radio3">вида [1]</label><br>\n<label for="radio4"><input type="radio" name="markSign" id="radio4">вида {1}</label><br>\n<label for="radio5"><input type="radio" name="markSign" id="radio5">вида [~1~]</label><br>\n<label for="radio6"><input type="radio" name="markSign" id="radio6">вида {~1~}</label><br>\n<br>\n<input type="button" value="Посчитать маркеры (МЗС)" onclick="window.returnValue=markerFunc(); window.close();" style="width:258px;"><BR>\n<input type="button" value="Отмена" onclick="window.returnValue=null; window.close();" style="width:125px;"><BR>\n</body>\n</html>';
    
    var fh = fso.CreateTextFile(tempPath, true);
    fh.WriteLine(dialogHTML);
    fh.Close();

    // Показываем диалог
    var markSign = window.showModalDialog(tempPath, null,
        "dialogHeight: 310px; dialogWidth: 330px; " +
        "center: Yes; help: No; resizable: Yes; status: No;");
    
    // Удаляем временный файл
    try { fso.DeleteFile(tempPath); } catch(e) {}

    if (!markSign) return;

    // Запускаем таймер после выбора маркера
    var startTime = new Date();

    // Обработка в зависимости от выбранного типа маркера
    if (markSign == "число надстрочным текстом") {
        str = str.replace(/<\/?(STRONG|EM|SUB|STRIKE)>/ig, "");
        str = str.replace(/<SPAN class=code>(.+?)<\/SPAN>/ig, "$1");
        str = str.replace(/<A class=note [^<]+?><SUP>(\d+)<\/SUP><\/A>/ig, "$1");
        str = str.replace(/&nbsp;/g, " ");
        str = str.replace(/[ □▫◦]/g, " ");
        str = str.replace(/<SUP>\s*(\d+)(\s*)<\/SUP>/ig, s1 + "$1" + s2 + "$2");
        str = str.replace(/<\/?[^<>]+>/g, "");
        str = str.replace(/[¹²³⁴⁵⁶⁷⁸⁹⁰]+/g, s1 + "$&" + s2);

    } else if (markSign == "вида *") {
        str = str.replace(/<\/?(STRONG|EM|SUP|SUB|STRIKE)>/ig, "");
        str = str.replace(/<SPAN class=code>(.+?)<\/SPAN>/ig, "$1");
        str = str.replace(/&nbsp;(&nbsp;| |\*)/g, " $1");
        str = str.replace(/(&nbsp;| |\*)&nbsp;/g, "$1 ");
        str = str.replace(/[ □▫◦]/g, " ");
        for (; countAst != 0;) {
            var ast = new RegExp("^((?:<\\/?[^>]+>)*)([ @]*)([*])([ *]*)((?:<\\/?[^>]+>)*)$", "gm");
            var ast_ = "$1$2@$4$5";
            var countAst = 0;
            if (str.search(ast) != -1) {
                str = str.replace(ast, ast_);
                countAst++
            }
        }
        str = str.replace(/[*]+/ig, s1 + "$&" + s2);

    } else if (markSign == "вида #") {
        str = str.replace(/<\/?(STRONG|EM|SUP|SUB|STRIKE)>/ig, "");
        str = str.replace(/<SPAN class=code>(.+?)<\/SPAN>/ig, "$1");
        str = str.replace(/&nbsp;(&nbsp;| |#)/g, " $1");
        str = str.replace(/(&nbsp;| |#)&nbsp;/g, "$1 ");
        str = str.replace(/[ □▫◦]/g, " ");
        for (; countGrid != 0;) {
            var grid = new RegExp("^((?:<\\/?[^>]+>)*)([ @]*)([#])([ #]*)((?:<\\/?[^>]+>)*)$", "gm");
            var grid_ = "$1$2@$4$5";
            var countGrid = 0;
            if (str.search(grid) != -1) {
                str = str.replace(grid, grid_);
                countGrid++
            }
        }
        str = str.replace(/[#]+/ig, s1 + "$&" + s2);

    } else if (markSign == "вида [1]") {
        str = str.replace(/<\/?(STRONG|EM|SUP|SUB|STRIKE)>/ig, "");
        str = str.replace(/<SPAN class=code>(.+?)<\/SPAN>/ig, "$1");
        str = str.replace(/(<A class=note [^<]+?>)\[(\d+)\](<\/A>)/ig, "$1($2)$3");
        str = str.replace(/\[\d+\]/ig, s1 + "$&" + s2);
        str = str.replace(/(<A class=note [^<]+?>)\((\d+)\)(<\/A>)/ig, "$1[$2]$3");

    } else if (markSign == "вида {1}") {
        str = str.replace(/<\/?(STRONG|EM|SUP|SUB|STRIKE)>/ig, "");
        str = str.replace(/<SPAN class=code>(.+?)<\/SPAN>/ig, "$1");
        str = str.replace(/(<A href=[^<]+?>)\{(\d+)\}(<\/A>)/ig, "$1($2)$3");
        str = str.replace(/\{\d+\}/ig, s1 + "$&" + s2);
        str = str.replace(/(<A href=[^<]+?>)\((\d+)\)(<\/A>)/ig, "$1{$2}$3");

    } else if (markSign == "вида [~1~]") {
        str = str.replace(/\[~\d+~\]/ig, s1 + "$&" + s2);

    } else if (markSign == "вида {~1~}") {
        str = str.replace(/\{~\d+~\}/ig, s1 + "$&" + s2);
    }

    // Удаляем все теги и подготавливаем текст для подсчёта
    str = str.replace(/<\/?[^<>]+>/g, "");
    str = str.replace(/&lt;/g, "<");
    str = str.replace(/&gt;/g, ">");
    str = str.replace(/&amp;/g, "&");
    str = str.replace(/&nbsp;/g, " ");
    str = str.replace(/[ □▫◦]/g, " ");
    str = str.replace(/^\s+/gm, "");

    // Считаем общее количество маркеров
    var regexpAll = new RegExp(s1, "g");
    var colMarker = (str.match(regexpAll) || []).length;
    
    if (colMarker == 0) {
        var endTime = new Date();
        var elapsed = (endTime - startTime) / 1000;
        var timeStr = elapsed.toFixed(3).replace(".", ",");
        MsgBox(scriptName + "\nver. " + version + "\n---------------------------\n\nВид маркера: " + markSign + "\n\nНе найдено маркеров знаков сносок (МЗС).\n\nВремя выполнения: " + timeStr + " сек.");
        return;
    }

    // Считаем маркеры в начале строк (маркеры текстов сносок)
    var regexpT = new RegExp("^" + s1, "gm");
    var t = (str.match(regexpT) || []).length;
    
    // Маркеры знаков сносок = общие минус маркеры в начале строк
    var z = colMarker - t;

    // Вычисляем время выполнения
    var endTime = new Date();
    var elapsed = (endTime - startTime) / 1000;
    var timeStr = elapsed.toFixed(3).replace(".", ",");

    // Формируем сообщение с результатами
    var message = "";
    message += scriptName + "\n";
    message += "ver. " + version + "\n";
    message += "---------------------------\n\n";
    message += "Вид маркера: " + markSign + "\n\n";
    message += "✓ Найдено маркеров знаков сносок: " + z + "\n\n";
    message += "Время выполнения: " + timeStr + " сек.";

    MsgBox(message);
}
