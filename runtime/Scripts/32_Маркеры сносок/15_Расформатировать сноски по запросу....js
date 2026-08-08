// Скрипт «Расформатировать сноски по запросу».
// v.2.0
// автор - stokber+DeepSpeek (июль, 2026)
function Run() {
    var name = "«Расформатировать сноски по запросу...»";
    var version = "2.0";

    // Выберите скобки по умолчанию:
    // var brasket = "[ ]";
    // var brasket = "{ }";
    var brasket = "[~ ~]";
    // var brasket = "{~ ~}";

    var show = "0"; // не показывать сообщение о некорректном отображении сносок.
    // var show = "1"; // показывать сообщение.

    try {
        var nbspChar = window.external.GetNBSP();
        var nbspEntity;
        if (nbspChar.charCodeAt(0) == 160) nbspEntity = "&nbsp;";
        else nbspEntity = nbspChar;
    } catch (e) {
        var nbspChar = String.fromCharCode(160);
        var nbspEntity = "&nbsp;";
    }

    var sel = document.getElementById("fbw_body");
    var fromHTML = sel.innerHTML; // код документа
	
    // поправляем копию html:
    // испр. баг с путем к файлу:
    fromHTML = fromHTML.replace(/(<A (?:class=note )?href=\")file:[^>]+?main[.]html/gi, "$1");
    // удал. все теги:
    // fromHTML =  fromHTML.replace(/<(?!\/?a(?=\s|\/|>|$))[^>]*>/gi, "");
    // кроме sup:
    fromHTML = fromHTML.replace(/<(?!\/?(a|sup)(?=\s|\/|>|$))[^>]*>/gi, "");

    // пробелы за ссылку:
    fromHTML = fromHTML.replace(new RegExp("(<A [^>]+?>)(<SUP>)?(\\\s|" + nbspEntity + ")+", "gi"), "$3$1$2"); //

    fromHTML = fromHTML.replace(new RegExp("(\\\s|" + nbspEntity + ")+(</SUP>)?(</A>)", "gi"), "$2$3$1");
	
   // сокращаем двойные <sup>-ы (если они есть):
    fromHTML = fromHTML.replace(new RegExp("<SUP><SUP>([^>]*)</SUP></SUP>", "gi"), "<SUP>$1</SUP>");

    // alert("fromHTML:\n"+fromHTML);
    var col = 10; // максимальное количество ссылок в сообщении.

    try {
        var doc = window.document;
        var selection = doc.selection;

        if (!selection) {
            MsgBox("Не удалось получить информацию о выделении.");
            return;
        }

        var range = selection.createRange();
        if (!range) {
            MsgBox("Не удалось создать диапазон для выделения.");
            return;
        }

        var parentElement = range.parentElement();
        if (!parentElement) {
            MsgBox("Курсор не находится внутри HTML ссылки или выделены лишние символы. Пожалуйста, поместите курсор внутрь ссылки.");
            return;
        }

        // Ищем в выделении родительский элемент <a>
        var linkElement = null;
        var currentElement = parentElement;
        while (currentElement) {
            if (currentElement.tagName && currentElement.tagName.toLowerCase() === "a") {
                linkElement = currentElement;
                break;
            }
            currentElement = currentElement.parentElement;
        }

        if (!linkElement) {
            MsgBox("Курсор не находится внутри HTML‑ссылки. Пожалуйста, поместите курсор внутрь ссылки или выделите всю ссылку, если она состоит только из одного символа.");
            return;
        }

        var linkHTML = linkElement.outerHTML;
        // удалить паразитов:
        linkHTML = linkHTML.replace(/(<A (?:class=note )?href=\")file:[^>]+?main[.]html/gi, "$1");
        // все теги:
        // linkHTML =  linkHTML.replace(/<(?!\/?a(?=\s|\/|>|$))[^>]*>/gi, "");
        // кроме sup:
        linkHTML = linkHTML.replace(/<(?!\/?(a|sup)(?=\s|\/|>|$))[^>]*>/gi, "");
        linkHTML = linkHTML.replace(new RegExp("(<A [^>]+?>)(<SUP>)?(\\\s|" + nbspEntity + ")+(<SUP>)?", "gi"), "$1$2$4"); //
        linkHTML = linkHTML.replace(new RegExp("(</SUP>)?(\\\s|" + nbspEntity + ")+(</SUP>)?(</A>)", "gi"), "$1$3$4");
        linkHTML = linkHTML.replace(new RegExp("<SUP><SUP>([^>]*)</SUP></SUP>", "gi"), "<SUP>$1</SUP>");
        // alert("linkHTML:\n"+linkHTML);

        /* if(show==1) {   
                // Проверяем, является ли ссылка некорректной сноской (содержит file:// и /main.html)
                if (linkHTML.indexOf('file://') !== -1 && linkHTML.indexOf('/main.html') !== -1) {
                    MsgBox(
                        "Обнаружено некорректное отоброжение ссылок сноски (содержит file:// и /main.html).\n\n" +
                        "Для исправления выполните вручную:\n" +
                        "1. Выйдите из скрипта.\n" +
                        "2. Переключите программу в режим кода (нажмите Alt+F3).\n" +
                        "3. Вернитесь в обычный режим (нажмите Alt+F2).\n" +
                        "4. Запустите скрипт повторно для проверки ссылки.\n\n" +
                        "После переключения отображение сносок должно нормализоваться."
                    );
                    return;
                }
                } */

    } catch (e) {
        MsgBox("Произошла ошибка при выполнении скрипта: " + e.message);
    }

    // составляем регексп для поиска ссылок выбранного вида:
    // var rxLink = linkHTML.replace(/(\.|\?|\-|\+|\*|\\|\(|\)|\[|\]|\{|\}|\^|\$|\|)/g, "\\$1");
    var rxLink = linkHTML.replace(/(\.|\?|\-|\+|\^|\*|\\|\/|\"|\(|\)|\[|\]|\{|\}|\$|\|)/g, "\\$1"); // 
    var rxLinkBezSk = rxLink;
    rxLink = rxLink.replace(/(<A (?:class=note )?href=[^>]+?)(\d+)([^>\d\"]*?\">)/g, "$1($2)$3");

    // ------------------------------------------------------------------------------------
    // для звёздочек и латиницы:
    rxLink = rxLink.replace(/(>)([\\][*])+(<)/g, "$1([*]+)$3");
    rxLink = rxLink.replace(/(>[\[\{\(\\]+)([\\][*])+(\\[\]\}\)]+<)/g, "$1([*]+)$3");

    rxLink = rxLink.replace(/(>)([ivxlcdm]+)(<)/g, "$1([ivxlcm]+)$3");
    rxLink = rxLink.replace(/(>)([IVXLCDM]+)(<)/g, "$1([IVXLCM]+)$3");
    rxLink = rxLink.replace(/(>[\[\{\(\\]+)([ivxlcdm]+)(\\[\]\}\)]+<)/g, "$1([ivxlcm]+)$3");
    rxLink = rxLink.replace(/(>[\[\{\(\\]+)([IVXLCDM]+)(\\[\]\}\)]+<)/g, "$1([IVXLCM]+)$3");

    // ----------------------------------------------------------------------

    // все остальные маркеры: ????????

    /* 	rxLink = rxLink.replace(/(>)([^\s0-9<>\*]+)(<)/g, "$1([^\\d<>\\*]+)$3");// ???????
    	rxLink = rxLink.replace(/(>[\[\{\(\\]+)([^\s0-9<>\*]+)(\\[\]\}\)]+<)/g, "$1([^\\d<>\\*]+)$3");// ??????? */

    // ============================================

    rxLink = rxLink.replace(/(>[^0-9<>]*)(\d+)([^0-9<>]*<)/g, "$1($2)$3");
    rxLink = rxLink.replace(/\d+/g, "\\d+");

    var res1 = linkHTML.replace(new RegExp(rxLink, "g"), "$1");
    // alert("res1: "+res1);
    var res2 = linkHTML.replace(new RegExp(rxLink, "g"), "$2");
    // alert("res2: "+res2);

    var rxLink2 = new RegExp(rxLink, "g"); // Создаем регулярное выражение с флагом g
    var matches = fromHTML.match(rxLink2) || [];
    var count = matches.length;
    var message = "Найдено всего ссылок такого вида: " + count + "\n\n";

    if (count === 0) {
        alert("Выбранная вами ссылка на сноску не похожа! Скрипт завершает свою работу..");
        return;
    } else {
        // Выводим только совпадения (максимум col)
        for (var i = 0; i < Math.min(count, col); i++) {
            message += matches[i] + "\n";

            // убираем паразитные части ссылок из сообщения:
            if (show == 0) {
                message = message.replace(/(<A (?:class=note )?href=\")file:[^>]+?main[.]html/g, "$1");
            }
        }

        // Если больше col — добавляем уведомление
        if (count > col) {
            message += "\n...ещё " + (count - col) + " совпадений";
        }
    }

    MsgBox(message);

    var res;
    var mark;

    if (res1 == "$1" && res2 == "$2") {
        alert("Выбранная вами ссылка на сноску не похожа! Скрипт завершает свою работу.");
        return;
    } else {

        if (res2 == "$2") {
            res = res1;
            mark = "$1";

            var question = "Кажется, атрибут из тега сноски пуст или нечислового вида, или символы из маркера (если они есть) не поддерживаются скриптом. Для расформатирования ссылки будут использованы символы (в данном случае — вида \'" + res1 + "\')"
            var result = confirm(question);
            // alert(question);
            if (result == false) {
                return
            }
        } else {
            var takge;
            if (res1 == res2) {
                takge = " тоже"
            } else {
                takge = ""
            }
            var question = "Выберите OK если хотите выбрать для расформатирования ссылки символы из маркера (в данном случае — вида \'" + res2 + "\'), или Отмена, если хотите выбрать числа из атрибутов тега сноски (в данном случае —" + takge + " вида \'" + res1 + "\').";
            var result = confirm(question);
            if (result == true) {
                res = res2;
                mark = "$2";
                // alert("Вы выбрали символы из маркера ("+res2+").");
            }
            if (result == false) {
                res = res1;
                mark = "$1";
                // alert("Вы выбрали число из атрибутов тега сноски ("+res1+").");
            }
        }
    }

    var skobki = prompt('Введите строку, содержащую скобки простым текстом, разделенные пробелом, или просто пробел, если решили обойтись без скобок.', brasket);
    if (!skobki) {
        return;
    }

    var ttt1 = new RegExp(" +", "gi");
    var ttt2 = skobki.match(ttt1);
    var ttt3 = skobki.search(ttt1);
    if (ttt3 == -1) {
        MsgBox("Ошибка. Во введенной вами строке отсутствует пробел.");
        return;
    }
    if (ttt2.length > 1) {
        MsgBox("Ошибка. Во введенной вами строке более чем одна группа пробелов.");
        return;
    }
    var arr = skobki.split(" ");
    var otkrSk = arr[0];
    var zakrSk = arr[1];

    // alert("Вы выбрали "+otkrSk+res+zakrSk);
    var result = confirm("Вы выбрали для расформатирования сносок маркеры вида: " + otkrSk + res + zakrSk + "\nПродолжить?");
    if (result == false) {
        return;
    }


    // ---------------------------------------------------------------------------

    /// ЗАМЕНА

    var Ts = new Date().getTime();
    var TimeStr = 0;

    //обрабатывать ли history
    var ObrabotkaHistory = true;
    //обрабатывать ли annotation
    var ObrabotkaAnnotation = true;

    var atrib = linkHTML.replace(/(<A [^>]+>).+/g, "$1");
    atrib = atrib.replace(/<A [^>"]+(\".*?\")>/g, "$1");
    atrib = atrib.replace(new RegExp("\\d+", "g"), "\\d+");
    var reATag = new RegExp('(<A\\b[^>]*?' + atrib + '>)([\\s\\S]*?)(</A>)', 'gi');

    // шаблоны замены на маркеры
    var re00 = new RegExp("(<A (?:class=note )?href=\")file:[^>]+?main[.]html", "gi");
    var re00_ = "$1";
    // переместить возможные пробелы:
    var re01 = new RegExp("(\\\s|" + nbspEntity + ")+(</SUP>)?(</A>)", "gi");
    var re01_ = "$2$3$1";

    var re02 = new RegExp("(<A [^>]+?>)(<SUP>)?(\\\s|" + nbspEntity + ")+", "gi");
    var re02_ = "$3$1$2";

    var re03 = new RegExp("<SUP><SUP>([^>]*)</SUP></SUP>", "g");
    var re03_ = "<SUP>$1</SUP>";

    // удалить внешние sup-ы:
    var re100 = new RegExp("(?:<SUP>)(" + rxLink + ")(?:</SUP>)", "g");
    var re100_ = "$1";

    var re101 = new RegExp(rxLink, "g");
    // var re101_ = otkrSk+res+zakrSk;
    var re101_ = otkrSk + mark + zakrSk;
    var count_101 = 0;

    // var id;
    var s = "";

    // функция, обрабатывающая абзац P
    function HandleP(ptr) {

        s = ptr.innerHTML;
        if (s.search(re00) != -1) {
            s = s.replace(re00, re00_);
        }

        /*  // -------------------------------------
	       // удалить все теги внутри A
    s = s.replace(reATag, function(match, openTag, content, closeTag) {
      var cleaned = content.replace(/<[^>]*>/g, "");
      return openTag + cleaned + closeTag;
    });
	   // ======================= */

        // --------------------------------------
        // удалить все, кроме <SUP>
        s = s.replace(reATag, function(match, openTag, content, closeTag) {
            var placeholderOpen = "\x00SUPOP\x00";
            var placeholderClose = "\x00SUPCL\x00";
            var protectedContent = content
                .replace(/<sup\b[^>]*>/gi, placeholderOpen)
                .replace(/<\/sup\s*>/gi, placeholderClose);
            var cleaned = protectedContent.replace(/<[^>]*>/g, "");
            cleaned = cleaned.split(placeholderOpen).join("<SUP>").split(placeholderClose).join("</SUP>");
            return openTag + cleaned + closeTag;
        });
        // =======================

        // trim
        if (s.search(re01) != -1) {
            s = s.replace(re01, re01_)
        }
        if (s.search(re02) != -1) {
            s = s.replace(re02, re02_)
        }
        if (s.search(re03) != -1) {
            s = s.replace(re03, re03_)
        }


        if (s.search(re100) != -1) {
            s = s.replace(re100, re100_);
        }

        if (s.search(re101) != -1) {
            count_101 += s.match(re101).length;
            s = s.replace(re101, re101_);
        }

        ptr.innerHTML = s;
    }

    window.external.BeginUndoUnit(document, "расформатирование сносок"); // ОТКАТ (UNDO) начало

    var body = document.getElementById("fbw_body");
    var ptr = body;
    var ProcessingEnding = false;
    while (!ProcessingEnding && ptr) {
        SaveNext = ptr;
        if (SaveNext.firstChild != null && SaveNext.nodeName != "P" &&
            !(SaveNext.nodeName == "DIV" &&
                ((SaveNext.className == "history" && !ObrabotkaHistory) ||
                    (SaveNext.className == "annotation" && !ObrabotkaAnnotation)))) {
            SaveNext = SaveNext.firstChild;
        } // либо углубляемся...
        else {
            while (SaveNext.nextSibling == null) {
                SaveNext = SaveNext.parentNode; // ...либо поднимаемся (если уже сходили вглубь)
                // поднявшись до элемента P, не забудем поменять флаг
                if (SaveNext == body) {
                    ProcessingEnding = true;
                }
            }
            SaveNext = SaveNext.nextSibling; //и переходим на соседний элемент
        }
        if (ptr.nodeName == "P") HandleP(ptr);
        ptr = SaveNext;
    }

    window.external.EndUndoUnit(document); // undo конец

    // =============================================

    var Tf = new Date().getTime();
    var Thour = Math.floor((Tf - Ts) / 3600000);
    var Tmin = Math.floor((Tf - Ts) / 60000 - Thour * 60);
    var Tsec = Math.ceil((Tf - Ts) / 1000 - Tmin * 60 - Thour * 3600);
    var Tsec1 = Math.ceil(10 * ((Tf - Ts) / 1000 - Tmin * 60)) / 10;
    var Tsec2 = Math.ceil(100 * ((Tf - Ts) / 1000 - Tmin * 60)) / 100;
    var Tsec3 = Math.ceil(1000 * ((Tf - Ts) / 1000 - Tmin * 60)) / 1000;

    if (Tsec3 < 1 && Tmin < 1) TimeStr = Tsec3 + " сек"
    else {
        if (Tsec2 < 10 && Tmin < 1) TimeStr = Tsec2 + " сек"
        else {
            if (Tsec1 < 30 && Tmin < 1) TimeStr = Tsec1 + " сек"
            else {
                if (Tmin < 1) TimeStr = Tsec + " сек"
                else {
                    if (Tmin >= 1 && Thour < 1) TimeStr = Tmin + " мин " + Tsec + " с"
                    else {
                        if (Thour >= 1) TimeStr = Thour + " ч " + Tmin + " мин " + Tsec + " с"
                    }
                }
            }
        }
    }
    // вывод статистики^
    var st2 = "";
    if (count_101 != 0) {
        st2 += '\nРасформатировано сносок:                             	' + count_101;
    }
    MsgBox('    Скрипт ' + name + ' v.' + version + '       \n\nВремя: ' + TimeStr + '.' + st2);
}
