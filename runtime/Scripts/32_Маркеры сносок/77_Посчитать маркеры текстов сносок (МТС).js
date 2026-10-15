/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Посчитать маркеры текстов сносок (МТС)",
  "version": "1.1",
  "authors": [
    {
      "name": "TaKir",
      "role": "author"
    },
    {
      "name": "TaKir",
      "role": "contributor",
      "contribution": "idea"
    },
    {
      "name": "DeepSeek",
      "role": "ai_assistance"
    },
    {
      "name": "stokber",
      "role": "contributor",
      "contribution": "declared_credit"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Считает маркеры текстов будущих сносок выбранного вида в начале строк.",
  "description": "Предлагает выбрать тип маркера, преобразует HTML книги в текст и подсчитывает маркеры этого типа в начале строк. Диалог формируется во временном HTML-файле; содержимое книги не меняется.",
  "scope": "Весь текст fbw_body; учитываются маркеры выбранного вида в начале строк.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Откройте книгу и выберите в диалоге вид маркера текста сноски.",
    "Сравните итог с количеством знаков сносок; скрипт не создаёт и не связывает сноски."
  ],
  "warnings": [
    "Для диалога создаётся временный HTML-файл в системной временной папке; его удаление после окна подавляет ошибки.",
    "При наличии служебных символов ☺ или ☻ подсчёт отменяется, а выражение поиска копируется в буфер обмена.",
    "Подсчёт по текстовым шаблонам может включить обычные похожие строки."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "dc6d63ddeb4334ba94914ef12a5bdf479a64f40a8f8fd6db033b98669e89323b",
    "reviewStatus": "generated"
  },
  "tags": [
    "Маркеры сносок",
    "буфер обмена",
    "сноски"
  ],
  "undo": {
    "status": "no_explicit_grouping",
    "description": ""
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Посчитать маркеры текстов сносок (МТС)" для редактора FBE
// version 1.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для подсчета маркеров текстов сносок (МТС) в fb2 документе.
// перед последующей расстановкой сносок.
// Пользователь выбирает вид маркера (надстрочный текст, *, #, [1], {1}, [~1~], {~1~}).
// Скрипт находит маркеры выбранного вида, расположенные в начале строк (тексты сносок)
// и выводит итоговое количество найденных маркеров (МТС).
// Скрип позволяет быстрее найти несовпадение маркеров МЗС и МТС.

// Скрипт сделан на основе алгоритма скрипта
// "Создать таблицу соответствия маркеров будущих сносок" уважаемого тов. stokber.

// version 1.1, 13.07.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Посчитать маркеры текстов сносок (МТС)";
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

    // Создаём временный html-файл для диалога выбора маркера
    var fso = new ActiveXObject("Scripting.FileSystemObject");
    var tempPath = fso.GetSpecialFolder(2) + "\\fbe_mts_dialog_temp.html";
    
    var dialogHTML = '<!DOCTYPE html>\n<html>\n<head>\n <meta http-equiv="Content-Type" content="text/html; charset=windows-1251">\n <meta http-equiv="MSThemeCompatible" content="yes">\n <title>Посчитать маркеры текстов сносок (МТС)</title>\n <style>\n  body, input{font-family:Tahoma; font-size:16px;margin:2px;}\n </style>\n</head>\n<script>\nfunction markerFunc() {\n if (document.getElementById("radio1").checked) return "число надстрочным текстом";\n if (document.getElementById("radio2").checked) return "вида *";\n if (document.getElementById("radio2a").checked) return "вида #";\n if (document.getElementById("radio3").checked) return "вида [1]";\n if (document.getElementById("radio4").checked) return "вида {1}";\n if (document.getElementById("radio5").checked) return "вида [~1~]";\n if (document.getElementById("radio6").checked) return "вида {~1~}";\n }\n</script>\n<body>\n\nВыберите вид маркеров текстов сносок:<br>\n<label for="radio1"><input type="radio" name="markSign" id="radio1" checked>число надстрочным текстом</label><br>\n<label for="radio2"><input type="radio" name="markSign" id="radio2">вида *</label><br>\n<label for="radio2a"><input type="radio" name="markSign" id="radio2a">вида #</label><br>\n<label for="radio3"><input type="radio" name="markSign" id="radio3">вида [1]</label><br>\n<label for="radio4"><input type="radio" name="markSign" id="radio4">вида {1}</label><br>\n<label for="radio5"><input type="radio" name="markSign" id="radio5">вида [~1~]</label><br>\n<label for="radio6"><input type="radio" name="markSign" id="radio6">вида {~1~}</label><br>\n<br>\n<input type="button" value="Посчитать маркеры (МТС)" onclick="window.returnValue=markerFunc(); window.close();" style="width:258px;"><BR>\n<input type="button" value="Отмена" onclick="window.returnValue=null; window.close();" style="width:125px;"><BR>\n</body>\n</html>';
    
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

    // Считаем маркеры в начале строк — это и есть маркеры текстов сносок (МТС)
    var regexpT = new RegExp("^" + s1, "gm");
    var t = (str.match(regexpT) || []).length;

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
    
    if (t == 0) {
        message += "Не найдено маркеров текстов сносок (МТС).\n\n";
    } else {
        message += "✓ Найдено маркеров текстов сносок: " + t + "\n\n";
    }
    
    message += "Время выполнения: " + timeStr + " сек.";

    MsgBox(message);
}