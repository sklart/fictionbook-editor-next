/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Показать все-все таблицы в HTML-виде",
  "version": "1.3",
  "authors": [
    {
      "name": "stokber",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Собирает все таблицы книги в отдельный HTML-просмотр.",
  "description": "Извлекает таблицы из визуальной разметки и выводит их в IE-окне либо сохраняет временный HTML для браузера. Книгу не заменяет, но в браузерном режиме пишет файл и запускает внешнее приложение.",
  "scope": "Текущая книга или определённые кодом её разделы.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Проверьте перечисленные параметры: часть задаётся в исходном JS, часть — через общий диалог настроек.",
    "Сохраняйте структуру Scripts/HTML и связанные файлы; один JS может быть недостаточен для работы.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже."
  ],
  "warnings": [
    "Файловые операции не отменяются обычным Ctrl+Z в книге.",
    "При выборе внешнего просмотра скрипт запускает локальную HTML-таблицу через оболочку Windows.",
    "Записи в DOM могут относиться к временным маркерам, интерфейсу или отчёту; отсутствие постоянных правок книги — результат статического разбора, не runtime-гарантия."
  ],
  "dependencies": [
    {
      "path": "HTML/Показать таблицы в HTML-виде.html",
      "sha256": "6574f2d3824d82289f857b2f570d8ee7150fbbf750043838acb427937c923c27",
      "optional": true
    }
  ],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "ef31b2cc67645bf8b9298280f6d79cc16de0c65d0a2877fdbca34b7d51ab25bb",
    "reviewStatus": "generated"
  },
  "tags": [
    "Таблицы",
    "параметры"
  ],
  "undo": {
    "status": "may_be_delegated",
    "description": "Прямых Undo-вызовов нет; действия могут выполняться связанным HTML/JS. Гарантия отмены не установлена."
  },
  "settings": [
    {
      "name": "display",
      "value": 0,
      "expression": "0",
      "description": "0 - показывать в IE, 1 - в браузере по умолчанию.",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    }
  ]
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Показать все-все таблицы в HTML-виде v.1.3"
// автор - stokber, (сентябрь, 2025).

function Run() {

                    ///НАСТРОЙКИ
    var display = 0; // 0 - показывать в IE, 1 - в браузере по умолчанию.
 
    // путь для показа в браузере по умолчанию:
    if (display == 1) {
       // путь для временного html-файла создаваемого для вашего браузера по умолчанию (HTML/temp.html):
       var filePatch = document.location.href.replace("file:///", "").replace(/%20/g," ").replace(/main\.html/, "HTML/temp.html");
       // или указать любой другой путь, например:
       // var filePatch = "C:\\temp.html"; // Следует иметь ввиду, что скрипт не создаёт новых директорий. Все папки, как и диски указанные в этой переменной, должны реально существовать.
	}
    // ================================

    var name = "Показать все-все таблицы в HTML-виде";
    var ver = "1.3";
    var fbwBody = document.getElementById("fbw_body");
    var str = fbwBody.innerHTML;

    // удаляем все бесполезные для нашей задачи,
    // но глючащие id-ы и fbstyle:
    str = str.replace(/(<(?:P|DIV)) id=["]?[\w]+["]?( class)/gi, "$1$2");
    str = str.replace(/(<[^>]+?) fbstyle=[\"]?\w+[\"]?([^>]*?>)/gi, "$1$2");
    // оставляем только таблицы:
    var str = str.replace(new RegExp("^(?!<(DIV class=(table|tr)|P class=t[hd])[^>]*>).*?(\\\r\\\n|$)", "gm"), "");
    // определяем количество таблиц для инфостроки:
    var countTable = (str.match(/<DIV class=table>/g) || []).length;
    // добавляем строки Табл.:
    var str = str.replace(new RegExp("<DIV class=table[^>]*>", "g"), "<P>Табл.</P>\r\n$&");
    // неразр. пр.:
    str = str.replace(/&nbsp;|[□▫◦]/g, " ");
    //убираем префиксы fb:
    str = str.replace(/fb((?:valign|align|colspan|rowspan)=)(?=[^>]*?>)/ig, "$1"); // !!!
    // Преобразуем:
    //ячейки:
    str = str.replace(/<P class=(td[^>]*?)>([\s\S]*?)<\/P>/ig, "<$1>$2</td>");
    // ячейки-заголовоки:
    str = str.replace(/<P class=(th[^>]*?)>(.*?)<\/P>/ig, "<$1>$2</th>");
    // строки:
    str = str.replace(/<DIV class=(tr[^>]*)>([\s\S]+?)<\/DIV>/igm, "<$1>$2</tr>");
    //таблицу:
    str = str.replace(/<DIV class=table>([\s\S]+?)<\/DIV>/igm, "<table>$1</table>");
    // инфострока Табл.:
    for (var i = 1; i <= countTable; i++) {
        str = str.replace(new RegExp("<P>Табл.</P>", ""), "<P>Табл. " + i + " из " + countTable + "</P>");
    }
    // интервал между таблицами:
    str = str.replace(/<\/table>/ig, "$&<BR><BR><BR><BR>");

    if (!str) {
        alert("В документе таблиц не найдено!\n\nСкрипт '" + name + "' v." + ver + ".");
        return;
    }

    function MyMsgWindow(str) {
        var MsgWindow = window.open("HTML/Показать таблицы в HTML-виде.html", null, "status=no,toolbar=no,menubar=no,location=no,scrollbars=yes,resizable=yes");
        MsgWindow.document.body.innerHTML = str;
    }

    // --------------------------------------------------------
    function WriteFile() {

        var shell = new ActiveXObject("WScript.Shell");
        var pref = '<html><head><meta http-equiv="Content-Type" content="text/html; charset=windows-1251"><meta http-equiv="MSThemeCompatible" content="yes"><title> :: Таблицы в виде HTML :: </title><style type="text/css">body{ color: #000000; font-family: Arial Unicode MS, Candara, Verdana, Arial, Tahoma, Arial, sans-serif; font-size: 10px; text-align:justify;background-color:#FFFFFF; }table {margin-left: 25px;margin-right: auto;margin-top: 25px;margin-bottom: auto;}table, th, td, tr {border: 1px solid black;border-collapse: collapse;padding-left: 15px;padding-right: 15px;padding-bottom: 5px;padding-top: 5px;}</style></head><body>'
        var suf = "</body></html>";
        str = pref + str + suf;
        var fso = new ActiveXObject("Scripting.FileSystemObject");
        var fh = fso.CreateTextFile(filePatch, true);
        fh.WriteLine(str);
        fh.Close();
        shell.Run("\""+filePatch+"\"");
        // ========================================
    }
    // для показа в IE:
    if (display == 0) {
        MyMsgWindow(str);
    }
    // для показа в браузере по умолчанию:
    if (display == 1) {
        WriteFile();
    }

}
