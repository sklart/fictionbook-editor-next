// Скрипт "Драма (расширенная версия)" для редактора FBE
// version 1.7

// Скрипт предназначен для разметки тэгами жирности имён говорящих в пьесах в fb2 документах.
// При наличии выделения, обрабатывается выделенный фрагмент, в противном случае - обрабатывается весь документ.
// Имена говорящих определяются как и в оригинальном  исходном скрипте:
// от начала абзаца и до разделителя (точка или левая круглая скобка).
// По умолчанию обрабатывается только основной раздел документа, без сносок и комментариев.
// Отдельная настройка для обработки разделов сносок и комментариев.
// Отображается статистика обработанных абзацев и типов разделителей имён.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// Оригинальный скрипт — Sclex  "Драма..."  (он же "Разметка имен говорящих в пьесах") (v.1.2 от 20.03.2010)
// Доработка — DeepSeek, TaKir

// Добавлено по сравнению с оригинальным скриптом:
// 1) Автоматический режим - Работа с выделенным фрагментом или со всем документом.
// 2) Настройка обработки разделов сносок и комментариев.
// 3) Опциональное исключение из обработки диалогов (дефис или тире в начале абзаца).
// 4) Отображение статистики обработки и заданных настроек.
// 5) Переключение обычного и тихого режимов вывода сообщений.
// 6) Выбор типов разделителей имён: точка и скобка, двоеточие и скобка, только двоеточие.

// version 1.7, 20.05.2026
//======================================

function Run() {
    var scriptName = "Драма (расширенная версия)";
    var version = "1.7";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима

    // Обрабатывать ли раздел History
    var ObrabotkaHistory = 0; // 0 - нет, 1 - да

    // Обрабатывать ли первую аннотацию (перед основным боди)
    var ObrabotkaAnnotation = 0; // 0 - нет, 1 - да

    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0; // 0 - нет, 1 - да

    // Обрабатывать раздел комментариев
    var processCommentsSection = 0; // 0 - нет, 1 - да

    // Обрабатывать текст в диалогах (тире или дефис с пробелами в начале абзаца)
    var processDialogs = 0; // 0 - нет, 1 - да

    // ==================================================
    // НАСТРОЙКИ ПРИМЕНЯЕМЫХ ТИПОВ РАЗДЕЛИТЕЛЕЙ
    // Можно включать одновременно только первые два или первый и третий варианты.
    // Двоеточие+скобка и только двоеточие — ВЗАИМОИСКЛЮЧАЮЩИЕ, не включайте их одновременно!
    // ==================================================

    // 1) Точка и левая круглая скобка: 0 - нет, 1 - да
    var processDotsLeftRoundBrackets = 1;

    // 2) Двоеточие и левая круглая скобка: 0 - нет, 1 - да (НЕ включать одновременно с processColons)
    var processColonsLeftRoundBrackets = 0;

    // 3) Только двоеточие (без скобок): 0 - нет, 1 - да (НЕ включать одновременно с processColonsLeftRoundBrackets)
    var processColons = 0;

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    try {
        var nbspChar = window.external.GetNBSP();
        var nbspEntity;
        if (nbspChar.charCodeAt(0) == 160) nbspEntity = "&nbsp;";
        else nbspEntity = nbspChar;
    } catch (e) {
        var nbspChar = String.fromCharCode(160);
        var nbspEntity = "&nbsp;";
    }

    // Если оба флага двоеточия включены — приоритет у processColons
    if (processColonsLeftRoundBrackets && processColons) {
        processColonsLeftRoundBrackets = 0;
    }

    var deleteSpacing = true;
    var s, m1, count = 0,
        i, ch, myTagName, wholeTag, whileFlag, accumStr, prevCh, closing, chCnt;
    var level, flag1, s2, s3, lev, tagCnt, tagsOnEndRegExp, tagsOnEndRegExp_, accumTags;
    var bigLetterRegExp = new RegExp("[А-ЯЁA-Z(]", "");
    var strongRegExp = new RegExp("<STRONG>|</STRONG>", "gi");
    var strongRegExp_ = "";
    var coll = new Object();
    var limit = 10;

    // Счётчики для статистики
    var countTotal = 0;    // Всего обработано
    var countDot = 0;      // С разделителем-точкой
    var countColon = 0;    // С разделителем-двоеточием
    var countBracket = 0;  // С разделителем-скобкой

    // Режим работы: 1 - выделенный фрагмент, 0 - весь документ
    var isSelectionMode = 0;

    // Проверка: находится ли узел внутри раздела, который не нужно обрабатывать
    function isInExcludedSection(node) {
        var current = node;
        while (current) {
            if (current.nodeName == "DIV") {
                var fbname = current.getAttribute("fbname") || "";
                if ((fbname == "notes" && !processNotesSection) ||
                    (fbname == "comments" && !processCommentsSection)) {
                    return true; // Узел внутри исключённого раздела
                }
            }
            current = current.parentNode;
        }
        return false;
    }

    // Проверка: начинается ли текст абзаца с тире/дефиса + пробел (диалог)
    function isDialogStart(innerHTML) {
        // Пропускаем возможные открывающие теги в начале (например <EM>)
        var idx = 0;
        while (idx < innerHTML.length && innerHTML.charAt(idx) == "<") {
            // Пропускаем тег
            while (idx < innerHTML.length && innerHTML.charAt(idx) != ">") idx++;
            if (idx < innerHTML.length) idx++; // пропускаем ">"
        }
        if (idx >= innerHTML.length) return false;
        // Проверяем первый значащий символ
        var firstCh = innerHTML.charAt(idx);
        if (firstCh == String.fromCharCode(8212) || firstCh == String.fromCharCode(8211) || firstCh == "-") {
            // Тире (U+2014), дефис (U+2013) или обычный дефис
            // Проверяем, что дальше пробел (обычный или неразрывный)
            var nextIdx = idx + 1;
            if (nextIdx < innerHTML.length) {
                var nextCh = innerHTML.charAt(nextIdx);
                if (nextCh == " " || nextCh == nbspChar) return true;
            }
        }
        return false;
    }

    // функция, обрабатывающая абзац P
    function HandleP(ptr) {
        if (ptr.parentNode.className != "section" || ptr.className == "subtitle") return;
        s = ptr.innerHTML;

        // Проверка на диалог
        if (!processDialogs && isDialogStart(s)) return;

        i = 0;
        whileFlag = true;
        accumStr = "";
        prevCh = "";
        chCnt = 0;
        level = 0;
        flag1 = false;
        tagCnt = 0;
        accumTags = "";
        while (i < s.length && whileFlag) {
            ch = s.substring(i, i + 1);
            if (ch == "<") {
                // эта ветка выполняется, если наткнулись на тэг
                closing = false;
                i++;
                myTagName = "";
                wholeTag = "<";
                // прочитаем тэг до закрывающей угловой скобки или до пробела
                ch = s.substring(i, i + 1);
                if (ch == "/") {
                    closing = true;
                    wholeTag += ch;
                    i++;
                    ch = s.substring(i, i + 1);
                }
                while (ch != ">" && ch != " ") {
                    myTagName += ch;
                    i++;
                    ch = s.substring(i, i + 1);
                }
                wholeTag += myTagName;
                // прочитаем остаток тэга
                while (ch != ">") {
                    wholeTag += ch;
                    i++;
                    ch = s.substring(i, i + 1);
                }
                wholeTag += ">";
                i++;
                accumTags += wholeTag;
                if (closing) level--;
                else {
                    level++;
                    coll["tagName_" + level] = myTagName;
                    coll["wholeTag_" + level] = wholeTag;
                }
                if (myTagName != "STRONG" && !closing) tagCnt++;
            } else {
                // эта ветка выполняется, если встреченный символ - не открывающая угловая скобка
                // то есть когда нужно обработать простой символ текста
                if (ch == "!" || ch == "?" || ch == "…") return;
                if (deleteSpacing && ch.search(bigLetterRegExp) >= 0 && (prevCh == " " || prevCh == nbspChar)) accumStr += prevCh;
                accumStr += accumTags;
                accumTags = "";

                // Проверка разделителей
                var separatorFound = false;
                var separatorType = ""; // "dot", "colon", "bracket"

                // Определяем, активна ли скобка как разделитель
                var bracketActive = (processDotsLeftRoundBrackets || processColonsLeftRoundBrackets);

                // Проверяем скобку как самостоятельный разделитель
                if (ch == "(" && chCnt > 0 && bracketActive) {
                    if (chCnt > limit) return;
                    separatorFound = true;
                    separatorType = "bracket";
                }

                // Проверяем точку как разделитель (если ещё не сработало)
                if (!separatorFound && prevCh == "." && chCnt > 0 && processDotsLeftRoundBrackets) {
                    if (chCnt > limit) return;
                    separatorFound = true;
                    separatorType = "dot";
                }

                // Проверяем двоеточие как разделитель (если ещё не сработало)
                if (!separatorFound && prevCh == ":" && chCnt > 0) {
                    if (processColonsLeftRoundBrackets || processColons) {
                        if (chCnt > limit) return;
                        separatorFound = true;
                        separatorType = "colon";
                    }
                }

                if (separatorFound) {
                    s2 = "";
                    s3 = "";
                    tagsOnEndRegExp = new RegExp("(<[^>]*?>){" + tagCnt.toString() + "}$", "i");
                    tagsOnEndRegExp_ = "";
                    s2 = accumStr.replace(tagsOnEndRegExp, tagsOnEndRegExp_);
                    for (lev = level - tagCnt; lev > 0; lev--)
                        s2 += "</" + coll["tagName_" + lev] + ">";
                    for (lev = level; lev > 0; lev--)
                        s3 = coll["wholeTag_" + lev] + s3;
                    s2 = s2.replace(strongRegExp, strongRegExp_);
                    s2 = "<STRONG>" + s2 + "</STRONG>";
                    ptr.innerHTML = s2 + s3 + s.substr(i);
                    // Подсчитываем тип разделителя
                    countTotal++;
                    if (separatorType == "dot") countDot++;
                    if (separatorType == "colon") countColon++;
                    if (separatorType == "bracket") countBracket++;
                    return;
                }

                if (ch != " " && ch != nbspChar) {
                    accumStr += ch;
                    chCnt++;
                } else if (!deleteSpacing) accumStr += ch;
                prevCh = ch;
                i++;
                if (chCnt > limit + 1) return;
                tagCnt = 0;
            }
        }
    }

    // Сбор всех P внутри выделенного диапазона
    function collectSelectedParagraphs() {
        var paragraphs = [];
        var sel = document.selection;
        if (!sel) return paragraphs;

        // Проверяем тип выделения
        if (sel.type && sel.type == "Control") {
            var controlRange = sel.createRange();
            if (controlRange && controlRange.length > 0) {
                var element = controlRange.item(0);
                if (element) {
                    collectParagraphsFromNode(element, paragraphs);
                }
            }
        } else {
            // Text Range
            var range = document.selection.createRange();
            if (!range) return paragraphs;

            var parentElement;
            try {
                parentElement = range.parentElement();
            } catch (e) {
                return paragraphs;
            }
            if (!parentElement) return paragraphs;

            // Находим ближайший общий DIV-контейнер (section или body)
            var container = parentElement;
            while (container && container.nodeName != "BODY" &&
                !(container.nodeName == "DIV" && (container.className == "section" || container.className == "body"))) {
                container = container.parentNode;
            }
            if (!container) container = document.getElementById("fbw_body");
            if (!container) return paragraphs;

            // Собираем ВСЕ P внутри контейнера
            var allP = container.getElementsByTagName("P");
            for (var k = 0; k < allP.length; k++) {
                // Проверяем, не в исключённом ли разделе
                if (isInExcludedSection(allP[k])) continue;
                // Проверяем, пересекается ли данный P с выделением
                if (isNodeInRange(allP[k], range)) {
                    paragraphs.push(allP[k]);
                }
            }
        }
        return paragraphs;
    }

    // Рекурсивный сбор P из узла (для Control Range)
    function collectParagraphsFromNode(node, paragraphs) {
        if (node.nodeName == "P") {
            // Проверяем, не в исключённом ли разделе
            if (!isInExcludedSection(node)) {
                paragraphs.push(node);
            }
            return;
        }
        var children = node.childNodes;
        for (var c = 0; c < children.length; c++) {
            collectParagraphsFromNode(children[c], paragraphs);
        }
    }

    // Проверка, находится ли узел в диапазоне выделения
    function isNodeInRange(node, range) {
        try {
            // Создаём диапазон для проверки пересечения
            var nodeRange = document.body.createTextRange();
            nodeRange.moveToElementText(node);

            // compareEndPoints: 0 - равны, 1 - первый после второго, -1 - первый до второго
            // StartToEnd: начало range <= конец nodeRange
            // EndToStart: конец range >= начало nodeRange
            if (range.compareEndPoints("StartToEnd", nodeRange) <= 0 &&
                range.compareEndPoints("EndToStart", nodeRange) >= 0) {
                return true;
            }
        } catch (e) {}
        return false;
    }

    // Обход для обработки массива абзацев (режим выделения)
    function processParagraphsArray(paragraphs) {
        for (var p = 0; p < paragraphs.length; p++) {
            HandleP(paragraphs[p]);
        }
    }

    // Обход всего документа (оригинальный алгоритм)
    function obhod() {
        var body = document.getElementById("fbw_body");
        var ptr = body;
        var ProcessingEnding = false;
        while (!ProcessingEnding && ptr) {
            SaveNext = ptr;

            // Проверка на разделы сносок и комментариев (по fbname)
            if (SaveNext.nodeName == "DIV") {
                var fbname = SaveNext.getAttribute("fbname") || "";
                if ((fbname == "notes" && !processNotesSection) ||
                    (fbname == "comments" && !processCommentsSection)) {
                    // Пропускаем весь этот DIV — переходим к следующему sibling
                    if (SaveNext.nextSibling) {
                        SaveNext = SaveNext.nextSibling;
                        ptr = SaveNext;
                        continue;
                    } else {
                        while (SaveNext.nextSibling == null && SaveNext != body) {
                            SaveNext = SaveNext.parentNode;
                        }
                        if (SaveNext == body) { ProcessingEnding = true; break; }
                        SaveNext = SaveNext.nextSibling;
                        ptr = SaveNext;
                        continue;
                    }
                }
            }

            if (SaveNext.firstChild != null && SaveNext.nodeName != "P" &&
                !(SaveNext.nodeName == "DIV" &&
                    ((SaveNext.className == "history" && !ObrabotkaHistory) ||
                        (SaveNext.className == "annotation" && !ObrabotkaAnnotation)))) {
                SaveNext = SaveNext.firstChild; // либо углубляемся...
            } else {
                while (SaveNext.nextSibling == null) {
                    SaveNext = SaveNext.parentNode; // ...либо поднимаемся (если уже сходили вглубь)
                    // поднявшись до элемента P, не забудем поменять флаг
                    if (SaveNext == body) { ProcessingEnding = true; }
                }
                SaveNext = SaveNext.nextSibling; // и переходим на соседний элемент
            }
            if (ptr.nodeName == "P") HandleP(ptr);
            ptr = SaveNext;
        }
    }

    // Запрашиваем параметры
    limit = prompt("Сколько символов может быть до разделителя?", limit);
    if (!limit) return;
    try {
        limit = eval(limit);
    } catch (e) { return; }

    deleteSpacing = AskYesNo("Удалять разрядку?");

    // Проверяем, есть ли выделение
    var selectedParagraphs = [];
    isSelectionMode = 0;
    try {
        var sel = document.selection;
        if (sel && sel.type && (sel.type == "Text" || sel.type == "Control")) {
            if (sel.type == "Control") {
                selectedParagraphs = collectSelectedParagraphs();
            } else {
                var range = document.selection.createRange();
                if (range && range.text && range.text.length > 0) {
                    selectedParagraphs = collectSelectedParagraphs();
                }
            }
            if (selectedParagraphs.length > 0) {
                isSelectionMode = 1;
            }
        }
    } catch (e) {}

    // Запускаем таймер ПОСЛЕ диалогов
    var startTime = new Date();

    window.external.BeginUndoUnit(document, "Разметка имен говорящих в пьесах");
    if (isSelectionMode) {
        processParagraphsArray(selectedParagraphs);
    } else {
        obhod();
    }
    window.external.EndUndoUnit(document);

    // Вычисляем время выполнения
    var endTime = new Date();
    var elapsed = (endTime - startTime) / 1000;
    var elapsedStr = elapsed.toFixed(3).replace(".", ",");

    // Формируем строку с типами разделителей
    var separatorsStr = "";
    if (processDotsLeftRoundBrackets && processColonsLeftRoundBrackets) {
        separatorsStr = "Точка+скобка, Двоеточие+скобка";
    } else if (processDotsLeftRoundBrackets && processColons) {
        separatorsStr = "Точка+скобка, Только двоеточие";
    } else if (processDotsLeftRoundBrackets) {
        separatorsStr = "Точка и левая скобка";
    } else if (processColonsLeftRoundBrackets) {
        separatorsStr = "Двоеточие и левая скобка";
    } else if (processColons) {
        separatorsStr = "Только двоеточие";
    } else {
        separatorsStr = "не заданы";
    }

    // Формируем блок настроек (нужен и для обычного, и для тихого режима)
    var settingsBlock = "";
    settingsBlock += "Заданные настройки:\n";
    settingsBlock += "  • Обработка History: " + (ObrabotkaHistory ? "да" : "нет") + "\n";
    settingsBlock += "  • Обработка первой аннотации: " + (ObrabotkaAnnotation ? "да" : "нет") + "\n";
    settingsBlock += "  • Обработка раздела сносок (примечаний): " + (processNotesSection ? "да" : "нет") + "\n";
    settingsBlock += "  • Обработка раздела комментариев: " + (processCommentsSection ? "да" : "нет") + "\n";
    settingsBlock += "  • Обработка диалогов (тире/дефис): " + (processDialogs ? "да" : "нет") + "\n";
    settingsBlock += "  • Разделители имен: " + separatorsStr + "\n";
    settingsBlock += "  • Макс. кол-во символов в именах: " + limit + "\n";
    settingsBlock += "  • Удаление разрядки: " + (deleteSpacing ? "да" : "нет") + "\n\n";

    // Вывод статистики
    if (showStatistics == 1) {
        var msg = "---------------------------\n";
        msg += scriptName + "\n";
        msg += "ver. " + version + "\n";
        msg += "---------------------------\n\n";

        msg += "Режим: " + (isSelectionMode ? "ВЫДЕЛЕННЫЙ ФРАГМЕНТ" : "ВЕСЬ ДОКУМЕНТ") + "\n\n";

        if (countTotal == 0) {
            msg += "Подходящих для обработки абзацев не найдено.\n\n";
        } else {
            msg += "✓ Обработано абзацев всего: " + countTotal + "\n";
            msg += "  • С разделителем-точкой: " + countDot + "\n";
            msg += "  • С разделителем-двоеточием: " + countColon + "\n";
            msg += "  • С разделителем-скобкой: " + countBracket + "\n\n";
        }

        msg += settingsBlock;
        msg += "Время выполнения: " + elapsedStr + " сек.";
        MsgBox(msg);
    } else {
        // Тихий режим
        if (countTotal == 0) {
            var msg = "---------------------------\n";
            msg += scriptName + "\n";
            msg += "ver. " + version + "\n";
            msg += "---------------------------\n\n";
            msg += "Режим: " + (isSelectionMode ? "ВЫДЕЛЕННЫЙ ФРАГМЕНТ" : "ВЕСЬ ДОКУМЕНТ") + "\n\n";
            msg += "Подходящих для обработки абзацев не найдено.\n\n";
            msg += settingsBlock;
            msg += "Время выполнения: " + elapsedStr + " сек.";
            MsgBox(msg);
        }
        // Если всё ОК — молча завершаем
    }
}
