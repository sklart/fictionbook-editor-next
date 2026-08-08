// Скрипт "Посчитать маркеры будущих сносок (МЗС и МТС)" для редактора FBE
// version 1.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для одновременного подсчета маркеров знаков сносок (МЗС)
// и маркеров текстов сносок (МТС) в fb2 документе перед дальнейшей расстановкой сносок.
// Пользователь выбирает вид маркера отдельно для МЗС и для МТС:
// надстрочный текст, *, #, [1], {1}, [~1~], {~1~}.
// Скрипт находит маркеры каждого выбранного вида и выводит оба результата.
// Позволяет быстро найти несовпадение количества маркеров МЗС и МТС.

// Скрипт сделан на основе алгоритма скрипта
// "Создать таблицу соответствия маркеров будущих сносок" уважаемого тов. stokber.

// version 1.1, 13.07.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Посчитать маркеры будущих сносок (МЗС и МТС)";
    var version = "1.1";

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

    // Создаём временный html-файл для диалога выбора маркеров
    var fso = new ActiveXObject("Scripting.FileSystemObject");
    var tempPath = fso.GetSpecialFolder(2) + "\\fbe_mzs_mts_dialog_temp.html";
    
    var dialogHTML = '<!DOCTYPE html>\n<html>\n<head>\n <meta http-equiv="Content-Type" content="text/html; charset=windows-1251">\n <meta http-equiv="MSThemeCompatible" content="yes">\n <title>Посчитать маркеры сносок (МЗС и МТС)</title>\n <style>\n  body, input{font-family:Tahoma; font-size:16px;margin:2px;}\n  fieldset{margin-bottom:10px;}\n  legend{font-weight:bold;}\n </style>\n</head>\n<script>\nfunction getValues() {\n var mzs = "";\n var mts = "";\n if (document.getElementById("mzs1").checked) mzs = "число надстрочным текстом";\n if (document.getElementById("mzs2").checked) mzs = "вида *";\n if (document.getElementById("mzs2a").checked) mzs = "вида #";\n if (document.getElementById("mzs3").checked) mzs = "вида [1]";\n if (document.getElementById("mzs4").checked) mzs = "вида {1}";\n if (document.getElementById("mzs5").checked) mzs = "вида [~1~]";\n if (document.getElementById("mzs6").checked) mzs = "вида {~1~}";\n if (document.getElementById("mts1").checked) mts = "число надстрочным текстом";\n if (document.getElementById("mts2").checked) mts = "вида *";\n if (document.getElementById("mts2a").checked) mts = "вида #";\n if (document.getElementById("mts3").checked) mts = "вида [1]";\n if (document.getElementById("mts4").checked) mts = "вида {1}";\n if (document.getElementById("mts5").checked) mts = "вида [~1~]";\n if (document.getElementById("mts6").checked) mts = "вида {~1~}";\n window.returnValue = mzs + "|" + mts;\n window.close();\n}\n</script>\n<body>\n\n<fieldset>\n<legend>Выберите вид маркера для МЗС:</legend>\n<label for="mzs1"><input type="radio" name="mzs" id="mzs1" checked>число надстрочным текстом</label><br>\n<label for="mzs2"><input type="radio" name="mzs" id="mzs2">вида *</label><br>\n<label for="mzs2a"><input type="radio" name="mzs" id="mzs2a">вида #</label><br>\n<label for="mzs3"><input type="radio" name="mzs" id="mzs3">вида [1]</label><br>\n<label for="mzs4"><input type="radio" name="mzs" id="mzs4">вида {1}</label><br>\n<label for="mzs5"><input type="radio" name="mzs" id="mzs5">вида [~1~]</label><br>\n<label for="mzs6"><input type="radio" name="mzs" id="mzs6">вида {~1~}</label>\n</fieldset>\n\n<fieldset>\n<legend>Выберите вид маркера для МТС:</legend>\n<label for="mts1"><input type="radio" name="mts" id="mts1" checked>число надстрочным текстом</label><br>\n<label for="mts2"><input type="radio" name="mts" id="mts2">вида *</label><br>\n<label for="mts2a"><input type="radio" name="mts" id="mts2a">вида #</label><br>\n<label for="mts3"><input type="radio" name="mts" id="mts3">вида [1]</label><br>\n<label for="mts4"><input type="radio" name="mts" id="mts4">вида {1}</label><br>\n<label for="mts5"><input type="radio" name="mts" id="mts5">вида [~1~]</label><br>\n<label for="mts6"><input type="radio" name="mts" id="mts6">вида {~1~}</label>\n</fieldset>\n\n<input type="button" value="Посчитать маркеры" onclick="getValues();" style="width:258px;"><BR>\n<input type="button" value="Отмена" onclick="window.returnValue=null; window.close();" style="width:125px;"><BR>\n</body>\n</html>';
    
    var fh = fso.CreateTextFile(tempPath, true);
    fh.WriteLine(dialogHTML);
    fh.Close();

    // Показываем диалог
    var result = window.showModalDialog(tempPath, null,
        "dialogHeight: 520px; dialogWidth: 360px; " +
        "center: Yes; help: No; resizable: Yes; status: No;");
    
    // Удаляем временный файл
    try { fso.DeleteFile(tempPath); } catch(e) {}

    if (!result) return;

    // Разбираем результат: markSignMZS|markSignMTS
    var parts = result.split("|");
    var markSignMZS = parts[0];
    var markSignMTS = parts[1];

    // Запускаем таймер после выбора маркеров
    var startTime = new Date();

    // ==================================================
    // ОБРАБОТКА ДЛЯ МЗС
    // ==================================================
    var strMZS = str;
    strMZS = processMarkers(strMZS, markSignMZS, s1, s2);
    
    // Удаляем все теги и подготавливаем текст для подсчёта
    strMZS = strMZS.replace(/<\/?[^<>]+>/g, "");
    strMZS = strMZS.replace(/&lt;/g, "<");
    strMZS = strMZS.replace(/&gt;/g, ">");
    strMZS = strMZS.replace(/&amp;/g, "&");
    strMZS = strMZS.replace(/&nbsp;/g, " ");
    strMZS = strMZS.replace(/[ □▫◦]/g, " ");
    strMZS = strMZS.replace(/^\s+/gm, "");

    var regexpAllMZS = new RegExp(s1, "g");
    var colMarkerMZS = (strMZS.match(regexpAllMZS) || []).length;
    var regexpTMZS = new RegExp("^" + s1, "gm");
    var tMZS = (strMZS.match(regexpTMZS) || []).length;
    var zMZS = colMarkerMZS - tMZS;

    // ==================================================
    // ОБРАБОТКА ДЛЯ МТС
    // ==================================================
    var strMTS = str;
    strMTS = processMarkers(strMTS, markSignMTS, s1, s2);
    
    // Удаляем все теги и подготавливаем текст для подсчёта
    strMTS = strMTS.replace(/<\/?[^<>]+>/g, "");
    strMTS = strMTS.replace(/&lt;/g, "<");
    strMTS = strMTS.replace(/&gt;/g, ">");
    strMTS = strMTS.replace(/&amp;/g, "&");
    strMTS = strMTS.replace(/&nbsp;/g, " ");
    strMTS = strMTS.replace(/[ □▫◦]/g, " ");
    strMTS = strMTS.replace(/^\s+/gm, "");

    var regexpTMTS = new RegExp("^" + s1, "gm");
    var tMTS = (strMTS.match(regexpTMTS) || []).length;

    // Вычисляем время выполнения
    var endTime = new Date();
    var elapsed = (endTime - startTime) / 1000;
    var timeStr = elapsed.toFixed(3).replace(".", ",");

    // Формируем сообщение с результатами
    var message = "";
    message += scriptName + "\n";
    message += "ver. " + version + "\n";
    message += "---------------------------\n\n";
    
    // Результат МЗС
    message += "Маркеры знаков сносок (МЗС):\n";
    message += "Вид маркера: " + markSignMZS + "\n";
    if (zMZS == 0) {
        message += "Не найдено маркеров знаков сносок (МЗС).\n\n";
    } else {
        message += "✓ Найдено: " + zMZS + "\n\n";
    }
    
    // Результат МТС
    message += "Маркеры текстов сносок (МТС):\n";
    message += "Вид маркера: " + markSignMTS + "\n";
    if (tMTS == 0) {
        message += "Не найдено маркеров текстов сносок (МТС).\n\n";
    } else {
        message += "✓ Найдено: " + tMTS + "\n\n";
    }
    
    // Сравнение МЗС и МТС
    if (zMZS == tMTS && zMZS > 0) {
        message += "Ура, кол-во маркеров МЗС и МТС совпало!\n\n";
    } else if (zMZS != tMTS && zMZS > 0 && tMTS > 0) {
        message += "Кол-во маркеров МЗС и МТС не совпадает!\n\n";
    }
    
    message += "Время выполнения: " + timeStr + " сек.";

    MsgBox(message);
}

// Вспомогательная функция обработки маркеров
function processMarkers(str, markSign, s1, s2) {
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
    
    return str;
}
