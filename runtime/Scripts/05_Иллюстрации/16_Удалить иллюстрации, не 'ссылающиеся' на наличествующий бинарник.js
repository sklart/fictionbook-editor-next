/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Удалить иллюстрации, не 'ссылающиеся' на наличествующий бинарник",
  "version": "1.0",
  "authors": [
    {
      "name": "stokber",
      "role": "author"
    },
    {
      "name": "Sclex",
      "role": "original_author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Удаляет элементы иллюстраций, адреса которых не проходят проверку по именам вложений.",
  "description": "Собирает id всех бинарных вложений в одно регулярное выражение без экранирования. Удаляет SPAN.image и DIV.image, если их href не совпадает с этим выражением; отдельно проверяет href оставшихся IMG и убирает часть соседних пустых абзацев. Самих бинарных вложений не удаляет. Показывает счётчики удалённых элементов.",
  "scope": "Все SPAN.image, DIV.image и IMG внутри fbw_body, а также пустые абзацы рядом с удалёнными блочными иллюстрациями; выделение не используется.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Сохраните копию книги и запустите скрипт для поиска и удаления иллюстраций, которые он считает не связанными с бинарниками.",
    "Сверьте отчёт и проверьте оставшиеся изображения вручную: сопоставление основано на регулярном выражении, а не на точном сравнении id."
  ],
  "warnings": [
    "Имена вложений включаются в регулярное выражение без экранирования и без проверки точного совпадения; точка и другие специальные символы могут дать ошибочный результат.",
    "Для оставшихся IMG проверяется href, хотя источник изображения обычно хранится в src; самостоятельные IMG могут быть удалены даже при существующем бинарнике.",
    "Проверка пустых строк создаётся до получения пользовательского символа неразрывного пробела, поэтому нестандартный символ может не распознаться.",
    "При отсутствии вложений условие str==\"\" приводит к удалению всех найденных элементов иллюстраций."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "c6b50b4bbccd37286b6e240eecaca381c5257ca3e3e8045419b9e4db436d39f8",
    "reviewStatus": "generated"
  },
  "tags": [
    "Иллюстрации",
    "изображения"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Операция отмены начинается при первом удалении и завершается после прохода. Явных досрочных выходов внутри неё нет; полный откат при ошибке не проверен."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Удалить иллюстрации, не 'ссылающиеся' на наличествующий бинарник"
// Версия 1.0
// Автор stokber (сентябрь, 2025)
// Скрипт написан по мотивам скрипта "Удалить все теги иллюстраций - теги image -  v1.6" за авторством Sclex-а.

function Run() {
    var name = "Удалить иллюстрации, не 'ссылающиеся' на наличествующий бинарник";
    var versionStr = "1.0";
    var rx;
	var str;

    binFile();

    function binFile() {
        var binobj = document.getElementById("binobj"); //  Раздел с информацией о вложенных файлах
        var mBin = binobj.getElementsByTagName("DIV"); //  Массив с отдельными блоками для файлов
        var count_Bin = mBin.length; //  Счетчик для всех вложенных файлов
        var mBinName = []; //  Массив с описанием файлов
        var binName; //  Описание одного из файлов
        for (n = 0; n < count_Bin; n++) {
            binName = mBin[n].all.id.value;
            mBinName[n] = binName;
        }

        str = mBinName.join('|'); // объединить массив в строку через "|".
        rx = new RegExp(str); // в регексп для test.
    }

    // пустые строки:
    var re0 = new RegExp("^( | |&nbsp;|" + nbspChar + ")*?$", "i");

    try {
        var nbspChar = window.external.GetNBSP();
        var nbspEntity;
        if (nbspChar.charCodeAt(0) == 160) nbspEntity = "&nbsp;";
        else nbspEntity = nbspChar;
    } catch (e) {
        var nbspChar = String.fromCharCode(160);
        var nbspEntity = "&nbsp;";
    }

    function isLineEmpty(ptr) {
        return re0.test(ptr.innerHTML.replace(/<(?!img)[^>]*?>/gi, ""));
    }

    function emptyLineIsNeededForValidity(elem) {
        if (!elem.previousSibling || (elem.previousSibling.nodeName == "DIV" &&
                (elem.previousSibling.className == "epigraph" || elem.previousSibling.className == "title")) &&
            elem.nextSibling && elem.nextSibling.nodeName == "P" && isLineEmpty(elem.nextSibling) &&
            elem.nextSibling.nextSibling == null)
            return true;
        else return false;
    }

    var emptyLines = [];
    var fbwBody = document.getElementById("fbw_body");
    var undoUnitName = "удаление иллюстраций";
    var firstRemoving = true;
    var elem, elem2, elems2, i, j;
    var replaceCounter1 = 0;
    var replaceCounter2 = 0;
    var replaceCounter3 = 0;
    elems = fbwBody.getElementsByTagName("SPAN");
    for (var i = elems.length - 1; i >= 0; i--) {
        elem = elems[i];
        if (elem.className && elem.className.toLowerCase() == "image") {

            if (!rx.test(elems[i].href) || str == "") { // ловим пустые ссылки и удаляем.

                if (firstRemoving) {
                    window.external.BeginUndoUnit(document, undoUnitName);
                    firstRemoving = false;
                }
                elem.removeNode(true);
                replaceCounter2 += 1;
            }
        }
    }
    var elems = fbwBody.getElementsByTagName("DIV");
    for (var i = elems.length - 1; i >= 0; i--) {
        elem = elems[i];
        if (elem.className && elem.className.toLowerCase() == "image") {

            if (!rx.test(elems[i].href) || str == "") { // ловим пустые ссылки и удаляем.

                if (firstRemoving) {
                    window.external.BeginUndoUnit(document, undoUnitName);
                    firstRemoving = false;
                }
                if (elem.nextSibling && elem.nextSibling.nodeName == "P" && isLineEmpty(elem.nextSibling))
                    if (!emptyLineIsNeededForValidity(elem))
                        if (emptyLines[emptyLines.length - 1] !== elem.nextSibling) emptyLines.push(elem.nextSibling);
                if (elem.previousSibling && elem.previousSibling.nodeName == "P" && isLineEmpty(elem.previousSibling))
                    if (emptyLines[emptyLines.length - 1] != elem.previousSibling) emptyLines.push(elem.previousSibling);
                elem.removeNode(true);
                replaceCounter1 += 1;
            }
        }
    }
    elems = fbwBody.getElementsByTagName("IMG");
    for (var i = elems.length - 1; i >= 0; i--) {

        if (!rx.test(elems[i].href) || str == "") { // ловим пустые ссылки и удаляем.

            elem = elems[i];
            if (true) {
                if (firstRemoving) {
                    window.external.BeginUndoUnit(document, undoUnitName);
                    firstRemoving = false;
                }
                elem.removeNode(true);
                replaceCounter3 += 1;
            }
        }
    }
    var removedEmptyLinesCnt = emptyLines.length;
    for (i = 0; i < removedEmptyLinesCnt; i++)
        emptyLines[i].removeNode(true);
    var s;
    s = "Скрипт «" + name + "» v" + versionStr + ".\n" +
        "\n" +
        "Удалено «блочных» иллюстраций: " + replaceCounter1 + ".\n" +
        "Удалено внутриабзацных иллюстраций: " + replaceCounter2 + ".\n" +
        (replaceCounter3 == 0 ? "" : "Удалено ссылок непонятных (вероятно, ошибочных) иллюстраций: " + replaceCounter3 + ".\n") +
        "\n" +
        "Всего удалено иллюстраций (тегов image): " + (replaceCounter1 + replaceCounter2 + replaceCounter3) + ".\n" +
        "\n" +
        "Удалено пустых строк (которые были рядом с иллюстрациями): " + removedEmptyLinesCnt + ".";
    if (!firstRemoving) window.external.EndUndoUnit(document);
    if ((replaceCounter1 + replaceCounter2 + replaceCounter3) == 0) {
        MsgBox("Изображений не 'ссылающихся' на наличествующий бинарник не найдено!\n\nСкрипт «" + name + "» v" + versionStr)
    } else {
        MsgBox(s);
    }
}
