/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Создать таблицу расположения имеющихся маркеров сносок",
  "version": "1.4",
  "authors": [
    {
      "name": "stokber",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Создаёт таблицу расположения имеющихся маркеров сносок.",
  "description": "Анализирует имеющиеся маркеры/готовые примечания и формирует таблицу соответствий в отдельном HTML-отчёте. Пользователь использует её для поиска пропусков и несоответствий; сама таблица не является новой таблицей FB2 в тексте.",
  "scope": "Область определяется условиями и обходом DOM в данном скрипте.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Проверьте перечисленные параметры: часть задаётся в исходном JS, часть — через общий диалог настроек.",
    "Сохраняйте структуру Scripts/HTML и связанные файлы; один JS может быть недостаточен для работы.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже."
  ],
  "warnings": [
    "Текущее содержимое буфера обмена может быть заменено или очищено.",
    "Файловые операции не отменяются обычным Ctrl+Z в книге.",
    "Есть запуск внешнего приложения или открытие сетевого адреса; проверьте сформированную команду и передаваемые данные.",
    "Записи в DOM могут относиться к временным маркерам, интерфейсу или отчёту; отсутствие постоянных правок книги — результат статического разбора, не runtime-гарантия."
  ],
  "dependencies": [
    {
      "path": "HTML/Создать таблицу маркеров-сносок.htm",
      "sha256": "c2981048f7f71a730521b5ab09960699d4f4b225a655db49fe1175c636af0dc2",
      "optional": true
    }
  ],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "603f1ed40bd21d5cc26038d921fe7d8902f602f40d008a2f920ce3e99d32acb4",
    "reviewStatus": "generated"
  },
  "tags": [
    "Маркеры сносок",
    "буфер обмена",
    "сноски",
    "параметры"
  ],
  "undo": {
    "status": "may_be_delegated",
    "description": "Прямых Undo-вызовов нет; действия могут выполняться связанным HTML/JS. Гарантия отмены не установлена."
  },
  "settings": [
    {
      "name": "display",
      "value": 1,
      "expression": "1",
      "description": "0 - показывать в IE, 1 - в браузере по умолчанию.",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "colour1",
      "value": "#f1e7a2",
      "expression": "\"#f1e7a2\"",
      "description": "цвет первой полосы «зебры».",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "colour5",
      "value": "orange",
      "expression": "\"orange\"",
      "description": "цвет строки маркера знака сноски №1.",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "colour6",
      "value": "white",
      "expression": "\"white\"",
      "description": "цвет новой страницы",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "lenZ",
      "value": 120,
      "expression": "120",
      "description": "кол. символов для отображения в ячейке для маркера знака сноски.",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "lenT",
      "value": 120,
      "expression": "120",
      "description": "кол. символов для отображения в ячейке для маркера текста сноски.",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "dialogWidth",
      "value": "330px",
      "expression": "\"330px\"",
      "description": "окно выбора вида маркеров сносок:",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "isIE6",
      "value": false,
      "expression": "false",
      "description": "проверка версии IE:",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "ast_",
      "value": "$1$2@$4$5",
      "expression": "\"$1$2@$4$5\"",
      "description": "заменяем звездочки в пустых строках другими символами (@).",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "grid_",
      "value": "$1$2@$4$5",
      "expression": "\"$1$2@$4$5\"",
      "description": "заменяем решётки в пустых строках другими символами (@).",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    }
  ]
}
</FBE-SCRIPT-CARD>*/
// Создать таблицу расположения имеющихся маркеров сносок. v. 1.4.
// Автор: stokber

function Run() {

                            /// НАСТРОЙКИ
    var display = 1; // 0 - показывать в IE, 1 - в браузере по умолчанию.
 
    // путь для показа в браузере по умолчанию:
    if (display == 1) {
       // путь для временного html-файла создаваемого для вашего браузера по умолчанию (HTML/temp.html):
       var filePatch = document.location.href.replace("file:///", "").replace(/%20/g," ").replace(/main\.html/, "HTML/temp.html");
       // или указать любой другой путь, например:
       // var filePatch = "C:\\temp.html"; // Следует иметь ввиду, что скрипт не создаёт новых директорий. Все папки, как и диски указанные в этой переменной, должны реально существовать.
   }

    var name = "Создать таблицу расположения имеющихся маркеров сносок";
    var version = "1.4";
    var avtor = "stokber";
    var sel = document.getElementById("fbw_body");
    var str = sel.innerHTML;
    var s1 = "☺"; //символ начала маркера сноски.
    var s2 = "☻"; //символ конца маркера сноски.
    var nZ;
    var nT;
    var report = "";
    var colour;
	

	var colour0a = "#bdb76b"; // цвет фона Заголовков таблицы.
    // var colour0a = "DarkKhaki"; // цвет фона Заголовков таблицы.
	// var colour0d = "silver"; // цвет пустых ячеек.
    var colour0d = "#c0c0c0"; // цвет пустых ячеек.
    // var colour1 = "Khaki"; // цвет первой полосы «зебры».
	var colour1 = "#f1e7a2"; // цвет первой полосы «зебры».
    // var colour2 = "LightCyan"; // цвет второй полосы «зебры».
    // var colour5 = "#ffa500"; // цвет строки маркера знака сноски №1.
	var colour5 = "orange"; // цвет строки маркера знака сноски №1.
    // var colour6 = "#ffffff"; // цвет новой страницы #FFFFFF
	var colour6 = "white"; // цвет новой страницы
    var lenZ = 120; // кол. символов для отображения в ячейке для маркера знака сноски.
    var lenT = 120; // кол. символов для отображения в ячейке для маркера текста сноски.

    // окно выбора вида маркеров сносок:
    var dialogWidth = "330px";
    var dialogHeight;
    // проверка версии IE: 
    var isIE6 = false;
    if (navigator.appName === 'Microsoft Internet Explorer') {
        var userAgent = navigator.userAgent;
        if (userAgent.indexOf('MSIE 6.0') !== -1) {
            isIE6 = true;
        }
    }
    if (isIE6) {
        // alert('Используется Internet Explorer 6');
        dialogHeight = "310px";
    } else {
        // alert('Используется другая версия Internet Explorer или другой браузер');
        dialogHeight = "280px";
    }

    var fbwBody = document.getElementById("fbw_body");
    var coll = new Object();
    coll["fbwBody"] = fbwBody;
    coll["mainDocument"] = document;
    coll["window"] = window;
    // coll["versionNum"]=listMarker_versionNum;
    var markSign = window.showModalDialog("HTML/Создать таблицу маркеров-сносок.htm", coll,
        "dialogHeight: " + dialogHeight + "; dialogWidth: " + dialogWidth + "; " +
        "center: Yes; help: No; resizable: Yes; status: No;");
    if (!markSign) return;

    // Проверяем случайное наличие в документе символов ☺ и ☻, которые 
    // будут использоваться скриптом как временные метки s1 и  s2 маркеров сносок 
    // в  переменной str: 
    var regexp = new RegExp("(" + s1 + "|" + s2 + ")+", "g")
    /*    var metk = (str.match(regexp) || []).length;
      // var Metk = (str.match(/☺|☻/g) || []).length;
    if (metk > 0) { */
    if (regexp.test(str)) {
        window.clipboardData.setData("text", "(" + s1 + "|" + s2 + ")+");
        alert("В вашем документе имеются символ(ы) " + s1 + " и(ли) " + s2 + ", которые используются скриптом как временные метки. Для корректной работы рекомендуем на время работы скрипта заменить их на другие символы.\nНайти в документе их можно перейдя в режим Кода и вставив регулярное выражение из буфера обмена в строку поиска окна Поиск (Ctrl+F)\n\"" + name + "\" v." + version);
        return;
    }

    if (markSign == "надстрочным текстом") {
        str = str.replace(/<\/?(STRONG|EM|SUB|STRIKE)>/ig, "");
        str = str.replace(/<SPAN class=code>(.+?)<\/SPAN>/ig, "$1"); // 

        str = str.replace(/<A class=note [^<]+?><SUP>(\d+)<\/SUP><\/A>/ig, "$1"); // убить FineReader-ские сноски. !!!!! 
        // str = str.replace(/(<A href=[^>]+>)<SUP>(\{\d+\})<\/SUP>(<\/A>)/ig, "$1$2$3"); 

        str = str.replace(/&nbsp;/g, " "); // 
        str = str.replace(/[ □▫◦]/g, " "); // 
        str = str.replace(/<SUP>\s*(\d+)(\s*)<\/SUP>/ig, s1 + "$1" + s2 + "$2"); //подменяем теги SUP на метки.
        str = str.replace(/<\/?[^<>]+>/g, ""); // удаляем все теги.
        str = str.replace(/[¹²³⁴⁵⁶⁷⁸⁹⁰]+/g, s1 + "$&" + s2); // учитываем надстрочные символы цифр.
    } else if (markSign == "вида *") {
        str = str.replace(/<\/?(STRONG|EM|SUP|SUB|STRIKE)>/ig, "");
        str = str.replace(/<SPAN class=code>(.+?)<\/SPAN>/ig, "$1"); // 
        str = str.replace(/&nbsp;(&nbsp;| |\*)/g, " $1"); // 
        str = str.replace(/(&nbsp;| |\*)&nbsp;/g, "$1 "); // 
        str = str.replace(/[ □▫◦]/g, " "); // 
        for (; countAst != 0;) { // заменяем звездочки в пустых строках другими символами (@).
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
        str = str.replace(/<SPAN class=code>(.+?)<\/SPAN>/ig, "$1"); // 
        str = str.replace(/&nbsp;(&nbsp;| |#)/g, " $1"); // 
        str = str.replace(/(&nbsp;| |#)&nbsp;/g, "$1 "); // 
        str = str.replace(/[ □▫◦]/g, " "); // 
        for (; countGrid != 0;) { // заменяем решётки в пустых строках другими символами (@).
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
        str = str.replace(/<SPAN class=code>(.+?)<\/SPAN>/ig, "$1"); // 
        str = str.replace(/(<A class=note [^<]+?>)\[(\d+)\](<\/A>)/ig, "$1($2)$3"); // 
        str = str.replace(/\[\d+\]/ig, s1 + "$&" + s2);
        str = str.replace(/(<A class=note [^<]+?>)\((\d+)\)(<\/A>)/ig, "$1[$2]$3"); // 
        str = str.replace(/☻☺/mg, "☻]☺"); // чтобы были видны два маркера знака сносок подряд без пробелов между. 
        // alert(str);
    } else if (markSign == "вида {1}") {
        str = str.replace(/<\/?(STRONG|EM|SUP|SUB|STRIKE)>/ig, "");
        str = str.replace(/<SPAN class=code>(.+?)<\/SPAN>/ig, "$1"); // 
        str = str.replace(/(<A href=[^<]+?>)\{(\d+)\}(<\/A>)/ig, "$1($2)$3"); // 
        str = str.replace(/\{\d+\}/ig, s1 + "$&" + s2);
        str = str.replace(/(<A href=[^<]+?>)\((\d+)\)(<\/A>)/ig, "$1{$2}$3"); // 
        str = str.replace(/☻☺/mg, "☻}☺"); // чтобы были видны два маркера знака сносок подряд без пробелов между. 
    } else if (markSign == "вида [~1~]") {
        str = str.replace(/\[~\d+~\]/ig, s1 + "$&" + s2);
    }

    if (markSign == "вида {~1~}") {
        str = str.replace(/\{~\d+~\}/ig, s1 + "$&" + s2);
    }

    Report();

    function Report() {
        str = str.replace(/<\/?[^<>]+>/g, ""); // удаляем все теги
        str = str.replace(/&lt;/g, "<"); // 
        str = str.replace(/&gt;/g, ">"); // 
        str = str.replace(/&amp;/g, "&"); // 
        // str = str.replace(/&nbsp;/g, " "); // 
        str = str.replace(/[ □▫◦]/g, " ");
        str = str.replace(/^\s+/gm, ""); // удаляем начальные пробелы

        var regexp = new RegExp(s1, "g")
        var colMarker = (str.match(regexp) || []).length;
        if (colMarker == 0) {
            MsgBox("Не найдено ни одного маркера сносок " + markSign + ".\nТаблица создана не будет.\n\n=================\n\nСкрипт '" + name + "' v." + version);
            return;
        }
        var regexp = new RegExp("^" + s1, "gm")
        // var t = (str.match(/^☺/gm) || []).length;
        var t = (str.match(regexp) || []).length;
        var z = (colMarker - t);
        // if (t == 0 || z ==0) {
        if (t == 0 && z == 0) {
            MsgBox("Вы выбрали маркеры " + markSign + ".\n\nНайдено:\nмаркеров знаков сносок: " + z + " \nмаркеров текстов сносок : " + t + "\nТаблица создана не будет.\n\n=================\n\nСкрипт '" + name + "' v." + version);
            return;
        }

        if (z == 0) {
            MsgBox("Вы выбрали маркеры " + markSign + ".\n\nВ документе не оказалось маркеров знаков сносок такого вида.\nБудет создана таблица только маркеров текстов сносок.");
        }
        if (t == 0) {
            MsgBox("Вы выбрали маркеры " + markSign + ".\n\nВ документе не оказалось маркеров текстов сносок такого вида.\nБудет создана таблица только маркеров знаков сносок.");
        }

        var regexp = new RegExp("([^" + s2 + "\\n\\r]{1," + lenZ + "})(" + s1 + ".+?" + s2 + ")|^(" + s1 + ".+?" + s2 + ")([^" + s1 + "\\n\\r]{1," + lenT + "})", "gm"); // регулярное выражение для маркера сноски.

        var result;
        // var report = ""; // список маркеров  сносок.

        while (result = regexp.exec(str)) {
            report += (result[0]) + "\r\n"; // для маркера сноски.
        }

        report = report.replace(/^☺.+?$/mg, "<tr><td></td><td>$&</td></tr>");
        report = report.replace(/.+?☻$/mg, "<tr><td align=\"right\">$&</td><td></td></tr>");

        if (markSign == "надстрочным текстом") {
            report = report.replace(/☺/mg, "<sup>");
            report = report.replace(/☻/mg, "</sup>");
        }

        report = report.replace(/[☺☻]/mg, "");

        report = report.replace(/(<tr>.+?\n)<tr>/gm, "$1<tr BGCOLOR=\"" + colour1 + "\">");
        report = report.replace(/<tr>/gm, "<tr BGCOLOR=\"" + colour1 + "\">");

        /*     window.clipboardData.setData("text",report); // данные в буфер обмена для Excel и т. п. программ.
           alert(report); */

        // ------------------------------------------------------------------
        // задаём фон для фрагментов с маркерами №1:
        if (markSign == "надстрочным текстом") {
            report = report.replace(/^<tr[^>]*?>(<td[^>]*?>.+?<sup>1<\/sup><\/td><td><\/td><\/tr>)$/gm, "<tr BGCOLOR=\"" + colour5 + "\">$1");
            report = report.replace(/^<tr[^>]*?>(<td><\/td><td><sup>1<\/sup>.+?<\/td><\/tr>)$/gm, "<tr BGCOLOR=\"" + colour5 + "\">$1");
        }
        if (markSign == "вида *") {
            report = report.replace(/^<tr[^>]*?>(<td[^>]*?>.+?[^*]\*<\/td><td><\/td><\/tr>)$/gm, "<tr BGCOLOR=\"" + colour5 + "\">$1");
            report = report.replace(/^<tr[^>]*?>(<td><\/td><td>\*[^*].+?<\/td><\/tr>)$/gm, "<tr BGCOLOR=\"" + colour5 + "\">$1");
        }
		if (markSign == "вида #") {
            report = report.replace(/(<td align=\"right\")>(.*?[^#]#<\/td>)/gm, "$1 BGCOLOR=\"" + colour5 + "\">$2");
            report = report.replace(/(<td)>(#[^#].+?<\/td><\/tr>)/gm, "$1 BGCOLOR=\"" + colour5 + "\">$2");
        }
        if (markSign == "вида [1]") {
            report = report.replace(/^<tr[^>]*?>(<td[^>]*?>.+?\[1\]<\/td><td><\/td><\/tr>)$/gm, "<tr BGCOLOR=\"" + colour5 + "\">$1");
            report = report.replace(/^<tr[^>]*?>(<td><\/td><td>\[1\].+?<\/td><\/tr>)$/gm, "<tr BGCOLOR=\"" + colour5 + "\">$1");
        }
        if (markSign == "вида {1}") {

            report = report.replace(/^<tr[^>]*?>(<td><\/td><td>\{1\}.+?<\/td><\/tr>)$/gm, "<tr BGCOLOR=\"" + colour5 + "\">$1");
            report = report.replace(/^<tr[^>]*?>(<td[^>]*?>.+?\{1\}<\/td><td><\/td><\/tr>)$/gm, "<tr BGCOLOR=\"" + colour5 + "\">$1");
        }
        if (markSign == "вида [~1~]") {
            report = report.replace(/^<tr[^>]*?>(<td[^>]*?>.+?\[~1~\]<\/td><td><\/td><\/tr>)$/gm, "<tr BGCOLOR=\"" + colour5 + "\">$1");
            report = report.replace(/^<tr[^>]*?>(<td><\/td><td>\[~1~\].+?<\/td><\/tr>)$/gm, "<tr BGCOLOR=\"" + colour5 + "\">$1");
        }
        if (markSign == "вида {~1~}") {
            report = report.replace(/^<tr[^>]*?>(<td[^>]*?>.+?\{~1~\}<\/td><td><\/td><\/tr>)$/gm, "<tr BGCOLOR=\"" + colour5 + "\">$1");
            report = report.replace(/^<tr[^>]*?>(<td><\/td><td>\{~1~\}.+?<\/td><\/tr>)$/gm, "<tr BGCOLOR=\"" + colour5 + "\">$1");
        }

        // закрасить пустые ячейки:
        report = report.replace(/<td><\/td>/mg, "<td BGCOLOR=\"" + colour0d + "\"><\/td>");

        // делим книгу на страницы:
        report = report.replace(new RegExp("([^>]</td></tr>\\r\\n)(<tr BGCOLOR=\"" + colour5 + "\"><td align=\"right\">)", "gm"), "$1<tr BGCOLOR=\"" + colour6 + "\"><td><sub>Новая страница с маркерами сносок:</sub></td><td></td></tr>\r\n$2");
        report = report.replace(new RegExp("([^>]</td></tr>\\r\\n)(<tr BGCOLOR=\"" + colour1 + "\"><td align=\"right\">)", "gm"), "$1<tr BGCOLOR=\"" + colour6 + "\"><td>Новая страница с маркерами сносок:</td><td></td></tr>\r\n$2");

        var reportDo = "<h2 align=\"center\">Таблица расположения имеющихся маркеров сносок</h2><table width=\"100%\" border=\"1\" cellpadding=\"5\" cellspacing=\"5\"><tr BGCOLOR=\"" + colour0a + "\"><th height=\"40\">Сноска (" + z + ")</th><th>Текст сноски (" + t + ")</th></tr><tr><td><sub>Первая страница с маркерами сносок:</sub></td><td></td></tr>\r\n";
        var reportPosle = "</table><p align=\"center\">Скрипт \"" + name + "\" v." + version + " (" + avtor + ")</p>";
        report = reportDo + "" + report + "" + reportPosle;
       
	   /* MsgBox(report);
        window.clipboardData.setData("text",report); // данные в буфер обмена для Excel и т. п. программ. */
	   

		
   // для показа в IE:
    if (display == 0) {
        MyMsgWindow(report);
    }
    // для показа в браузере по умолчанию:
    if (display == 1) {
        WriteFile();
    }
    }

    function MyMsgWindow(report) {
        var MsgWindow = window.open("HTML/Создать таблицу маркеров-сносок.htm", null, "status=no,toolbar=no,menubar=no,location=no,scrollbars=yes,resizable=yes");
        MsgWindow.document.body.innerHTML = report;
      }

        function WriteFile() {
        var shell = new ActiveXObject("WScript.Shell");
        var fso = new ActiveXObject("Scripting.FileSystemObject");
        var fh = fso.CreateTextFile(filePatch, true);
        fh.WriteLine(report);
        fh.Close();
        shell.Run("\""+filePatch+"\"");
    }
        // ========================================

}
