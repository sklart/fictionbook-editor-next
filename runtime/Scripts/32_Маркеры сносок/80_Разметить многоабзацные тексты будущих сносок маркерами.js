// Скрипт "Разметить многоабзацные тексты будущих сносок маркерами" для редактора FBE
// version 1.7
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для разметки многоабзацных текстов будущих сносок в fb2 документах.
// При наличии выделения обрабатывается только текст внутри выделения.
// При отсутствии выделения обрабатывается весь документ.
// Скрипт добавляет маркер (по умолчанию "++") в начала ненумерованных абзацев,
// следующих сразу после нумерованных (возможных маркеров текстов сносок).
// Нумерованными считаются ТОЛЬКО абзацы с явными маркерами текстов сносок (МТС):
// [N], {N}, [~N~], {~N~}, число в SUP, звёздочки * и решётки #.
// Заголовки секций (DIV class=title) прерывают группу и не размечаются.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.7, 29.07.2026
//======================================

function Run() {
    var scriptName = "Разметить многоабзацные тексты будущих сносок маркерами";
    var version = "1.7";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1;

    // Маркер 2-го, 3-го и т.д. абзацев текста сноски
    var UserSign = "++";      // Тут можно задать любой знак

    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0; // 0 - нет, 1 - да

    // Обрабатывать раздел комментариев
    var processCommentsSection = 0; // 0 - нет, 1 - да

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    // Неразрывный пробел из настроек FBE
    var nbspChar = String.fromCharCode(160);
    try {
        nbspChar = window.external.GetNBSP();
    } catch (e) {}

    // Список необычных пробелов, которые должны обрабатываться наравне с обычными
    var unusualSpaces = String.fromCharCode(160) +  // неразрывный пробел
        String.fromCharCode(8194) +  // EN SPACE
        String.fromCharCode(8195) +  // EM SPACE
        String.fromCharCode(8196) +  // THREE-PER-EM SPACE
        String.fromCharCode(8197) +  // FOUR-PER-EM SPACE
        String.fromCharCode(8198) +  // SIX-PER-EM SPACE
        String.fromCharCode(8239) +  // NARROW NO-BREAK SPACE
        String.fromCharCode(8201) +  // THIN SPACE
        String.fromCharCode(8202) +  // HAIR SPACE
        String.fromCharCode(9643) +  // ▫ WHITE SMALL SQUARE (нестандартный пробел FBE)
        nbspChar;

    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================

    // Проверка: является ли символ пробелом (обычным или необычным)
    function isSpace(ch) {
        if (ch == " " || ch == "\t" || ch == "\r" || ch == "\n") return true;
        for (var i = 0; i < unusualSpaces.length; i++) {
            if (ch == unusualSpaces.charAt(i)) return true;
        }
        return false;
    }

    // Проверка: является ли символ цифрой
    function isDigit(ch) {
        return (ch >= "0" && ch <= "9");
    }

    // Проверка: является ли строка пустой (только пробелы и неразрывные пробелы)
    function isEmptyLine(text) {
        for (var i = 0; i < text.length; i++) {
            if (!isSpace(text.charAt(i))) return false;
        }
        return true;
    }

    // ==================================================
    // ФУНКЦИИ ПРОВЕРКИ МАРКЕРОВ
    // ==================================================

    // Проверка: является ли абзац нумерованным маркером сноски
    // Считаем только: [N], {N}, [~N~], {~N~}, число в SUP/SUB, * и #
    function isNumberedMarker(text) {
        if (text.length == 0) return false;
        var i = 0;

        // Пропускаем начальные пробелы
        while (i < text.length && isSpace(text.charAt(i))) i++;
        if (i >= text.length) return false;

        var ch = text.charAt(i);

        // Звёздочка *
        if (ch == "*") return true;

        // Решётка #
        if (ch == "#") return true;

        // Квадратная скобка [N] или [~N~]
        if (ch == "[") {
            i++;
            if (i < text.length && text.charAt(i) == "~") i++; // [~
            var foundDigit = false;
            while (i < text.length && isDigit(text.charAt(i))) { foundDigit = true; i++; }
            if (!foundDigit) return false;
            if (i < text.length && text.charAt(i) == "~") i++; // ~
            if (i < text.length && text.charAt(i) == "]") return true; // ]
            return false;
        }

        // Фигурная скобка {N} или {~N~}
        if (ch == "{") {
            i++;
            if (i < text.length && text.charAt(i) == "~") i++; // {~
            var foundDigit = false;
            while (i < text.length && isDigit(text.charAt(i))) { foundDigit = true; i++; }
            if (!foundDigit) return false;
            if (i < text.length && text.charAt(i) == "~") i++; // ~
            if (i < text.length && text.charAt(i) == "}") return true; // }
            return false;
        }

        // Всё остальное — не маркер сноски
        return false;
    }

    // Проверка: начинается ли абзац уже с маркера UserSign
    function startsWithUserSign(text) {
        if (text.length < UserSign.length) return false;
        var i = 0;

        // Пропускаем начальные пробелы
        while (i < text.length && isSpace(text.charAt(i))) i++;

        // Проверяем посимвольно
        for (var j = 0; j < UserSign.length; j++) {
            if (i + j >= text.length) return false;
            if (text.charAt(i + j) != UserSign.charAt(j)) return false;
        }
        return true;
    }

    // ==================================================
    // ФУНКЦИЯ ПРОВЕРКИ ЗАГОЛОВКОВ
    // ==================================================

    // Проверка: является ли абзац заголовком секции (прерывает группу)
    // Заголовками считаются: P class="title" и P внутри DIV class="title"
    // Подзаголовки (subtitle) и text-author НЕ являются заголовками секций
    function isHeading(pElement) {
        // Проверяем класс самого P
        var pClass = pElement.className || "";
        if (pClass == "title") return true;

        // Проверяем класс родительского DIV
        var parent = pElement.parentNode;
        if (parent && parent.nodeType == 1 && parent.nodeName == "DIV") {
            var parentClass = parent.className || "";
            if (parentClass == "title") return true;
        }

        return false;
    }

    // ==================================================
    // ОСНОВНАЯ ЧАСТЬ
    // ==================================================

    // Получаем fbw_body
    var fbw_body = document.getElementById("fbw_body");
    if (!fbw_body) {
        MsgBox("Ошибка: не найден fbw_body", scriptName + " v" + version);
        return;
    }

    // Определяем, работаем ли с выделением
    var hasSelection = false;
    var selectionRange = null;

    try {
        var sel = document.selection;
        if (sel && sel.type && sel.type == "Text") {
            var tr = sel.createRange();
            if (tr && tr.compareEndPoints("StartToEnd", tr) != 0) {
                var parentEl = tr.parentElement();
                if (parentEl && parentEl.nodeName != "TEXTAREA" && parentEl.nodeName != "INPUT") {
                    // Проверяем, что выделение внутри fbw_body
                    var checkEl = parentEl;
                    var insideFbwBody = false;
                    var depth = 0;
                    while (checkEl && depth < 100) {
                        depth++;
                        if (checkEl === fbw_body) { insideFbwBody = true; break; }
                        checkEl = checkEl.parentNode;
                    }
                    if (insideFbwBody) {
                        hasSelection = true;
                        selectionRange = tr.duplicate();
                    }
                }
            }
        }
    } catch (e) {
        hasSelection = false;
    }

    // Таймер запускаем после всех проверок, перед изменениями
    var startTime = new Date();

    window.external.BeginUndoUnit(document, scriptName);

    var totalChanged = 0;      // Количество размеченных абзацев
    var totalChecked = 0;      // Всего проверено абзацев
    var numberedFound = 0;     // Найдено нумерованных абзацев
    var groupsFound = 0;       // Найдено многоабзацных групп

    // ==================================================
    // СБОР АБЗАЦЕВ ДЛЯ ОБРАБОТКИ
    // ==================================================

    var paragraphsToProcess = [];

    if (hasSelection) {
        // Собираем все P в fbw_body и проверяем пересечение с выделением
        var allPElements = fbw_body.getElementsByTagName("P");
        for (var ap = 0; ap < allPElements.length; ap++) {
            var pEl = allPElements[ap];
            try {
                if (!pEl || !fbw_body.contains(pEl)) continue;
                var pRange = document.body.createTextRange();
                pRange.moveToElementText(pEl);
                var comp1 = selectionRange.compareEndPoints("StartToEnd", pRange);
                var comp2 = selectionRange.compareEndPoints("EndToStart", pRange);
                if (comp1 < 0 && comp2 > 0) {
                    paragraphsToProcess.push(pEl);
                }
            } catch (e2) {}
        }
    } else {
        // Собираем все P из нужных разделов
        var bodyDivs = document.getElementsByTagName("DIV");
        var targetBodies = [];

        for (var b = 0; b < bodyDivs.length; b++) {
            if (bodyDivs[b].className == "body") {
                var fbname = bodyDivs[b].getAttribute("fbname") || "";
                if (fbname == "") {
                    targetBodies.push(bodyDivs[b]);
                } else if (fbname == "notes" && processNotesSection) {
                    targetBodies.push(bodyDivs[b]);
                } else if (fbname == "comments" && processCommentsSection) {
                    targetBodies.push(bodyDivs[b]);
                }
            }
        }

        for (var tb = 0; tb < targetBodies.length; tb++) {
            var allP = targetBodies[tb].getElementsByTagName("P");
            for (var ap = 0; ap < allP.length; ap++) {
                paragraphsToProcess.push(allP[ap]);
            }
        }
    }

    if (paragraphsToProcess.length == 0) {
        window.external.EndUndoUnit(document);
        MsgBox("Не найдено абзацев для обработки.", scriptName + " v" + version);
        return;
    }

    // ==================================================
    // ФАЗА 1: СБОР ДАННЫХ (только чтение)
    // ==================================================

    var paraData = [];

    for (var i = 0; i < paragraphsToProcess.length; i++) {
        var pElement = paragraphsToProcess[i];
        totalChecked++;

        // Получаем текст абзаца
        var text = pElement.innerText || pElement.textContent || "";

        // Определяем свойства абзаца
        var isEmpty = isEmptyLine(text);
        var isNumbered = !isEmpty && isNumberedMarker(text);
        var hasUserSign = !isEmpty && startsWithUserSign(text);
        var heading = isHeading(pElement);

        // Сохраняем данные
        paraData.push({
            element: pElement,
            isEmpty: isEmpty,
            isNumbered: isNumbered,
            hasUserSign: hasUserSign,
            isHeading: heading,
            text: text
        });

        if (isNumbered) numberedFound++;
    }

    // ==================================================
    // ФАЗА 2: ОБРАБОТКА (только запись)
    // ==================================================

    var insideGroup = false;              // Находимся ли внутри группы после нумерованного
    var currentGroupHasMarked = false;    // Был ли в текущей группе размечен хотя бы один абзац

    for (var i = 0; i < paraData.length; i++) {
        var data = paraData[i];

        // Заголовки секций всегда прерывают группу
        if (data.isHeading) {
            if (insideGroup && currentGroupHasMarked) {
                groupsFound++;
            }
            insideGroup = false;
            currentGroupHasMarked = false;
            continue;
        }

        // Нумерованный абзац — начало новой группы
        if (data.isNumbered) {
            // Закрываем предыдущую группу, если в ней были размеченные абзацы
            if (insideGroup && currentGroupHasMarked) {
                groupsFound++;
            }
            insideGroup = true;
            currentGroupHasMarked = false;

        // Пустая строка — не прерываем группу, но и не добавляем маркер
        } else if (data.isEmpty) {
            // Ничего не делаем

        // Ненумерованный непустой абзац внутри группы — добавляем маркер
        } else if (insideGroup && !data.hasUserSign) {
            var pElement = data.element;

            // Ищем первый текстовый узел (с заходом на 1 уровень в теги форматирования)
            var firstTextNode = null;
            var child = pElement.firstChild;
            while (child) {
                if (child.nodeType == 3) { firstTextNode = child; break; }
                if (child.nodeType == 1 && (child.nodeName == "EM" || child.nodeName == "STRONG" || 
                    child.nodeName == "I" || child.nodeName == "B" || child.nodeName == "SPAN" || child.nodeName == "A")) {
                    var innerChild = child.firstChild;
                    while (innerChild) {
                        if (innerChild.nodeType == 3) { firstTextNode = innerChild; break; }
                        innerChild = innerChild.nextSibling;
                    }
                    if (firstTextNode) break;
                }
                child = child.nextSibling;
            }

            // Вставляем маркер в начало текстового узла
            if (firstTextNode) {
                firstTextNode.nodeValue = UserSign + firstTextNode.nodeValue;
                totalChanged++;
                currentGroupHasMarked = true;
            } else {
                // Если текстового узла нет — создаём его
                var newTextNode = document.createTextNode(UserSign);
                if (pElement.firstChild) {
                    pElement.insertBefore(newTextNode, pElement.firstChild);
                } else {
                    pElement.appendChild(newTextNode);
                }
                totalChanged++;
                currentGroupHasMarked = true;
            }
        }
        // Если !insideGroup — обычный текст вне группы, не трогаем
    }

    // Закрываем последнюю группу, если она была
    if (insideGroup && currentGroupHasMarked) {
        groupsFound++;
    }

    window.external.EndUndoUnit(document);

    // ==================================================
    // ТАЙМЕР
    // ==================================================
    var endTime = new Date();
    var executionTime = (endTime - startTime) / 1000;
    var timeStr = executionTime.toFixed(3);
    timeStr = timeStr.replace(".", ",");

    // ==================================================
    // СТАТИСТИКА
    // ==================================================
    if (showStatistics == 1 || totalChanged == 0) {
        var modeStr = hasSelection ? "РЕЖИМ ВЫДЕЛЕНИЯ:" : "РЕЖИМ \"ВЕСЬ ДОКУМЕНТ\":";

        var msg = "";
        msg += scriptName + "\n";
        msg += "ver. " + version + "\n\n";

        msg += modeStr + "\n\n";

        msg += "  \u2022 Всего абзацев проверено: " + totalChecked + "\n";
        msg += "  \u2022 Нумерованных абзацев (с маркерами текстов сносок): " + numberedFound + "\n\n";

        msg += "  \u2713 Размечено абзацев: " + totalChanged + "\n";
        msg += "  \u2022 Многоабзацных сносок: " + groupsFound + "\n";
        msg += "  \u2022 Маркер: " + UserSign + "\n";

        if (totalChanged == 0) {
            msg += "\nМногоабзацных сносок не обнаружено.\n";
        }

        msg += "\nВремя выполнения: " + timeStr + " сек.";

        MsgBox(msg, "FBE скрипт");
    }
}