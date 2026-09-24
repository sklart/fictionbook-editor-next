// Скрипт "Объединить разделы сносок и комментариев" для редактора FBE
// version 1.7
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для объединения разделов сносок и комментариев в fb2 документах.
// Раздел комментариев переносится в конец раздела сносок с сохранением заголовков.
// Сноски и комментарии размещаются в отдельных секциях внутри общего раздела notes.
// Нумерация комментариев при переносе в раздел notes не меняется.
// Ссылки в основном тексте документа остаются без изменений.
// Стандартный заголовок сносок-примечаний опционально переименовывается
// (по умолчанию "Примечания" → "Сноски").
// Нестандартные заголовки сносок-примечания и комментариев
// (например "Примечания автора" или "Комментарии редакции") по умолчанию не изменяются.
// Режим работы скрипта: обычный или тихий.
// Поддержка отмены всех действий (Ctrl+Z).

// version 1.7, 05.08.2026
//======================================

function Run() {
    var scriptName = "Объединить разделы сносок и комментариев";
    var version = "1.7";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима

    // Переименование заголовка сносок:
    // 0 - никогда не переименовывать
    // 1 - переименовывать только стандартный заголовок "Примечания"
    // 2 - всегда переименовывать (даже нестандартный заголовок)
    var renameNotesTitle = 1;

    // Новое название для заголовка сносок (если переименование включено)
    var newNotesTitle = "Сноски"; // Можно изменить на любое другое

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    var fbw_body = document.getElementById("fbw_body");
    if (!fbw_body) {
        MsgBox("Ошибка: не найден fbw_body", scriptName + " v" + version);
        return;
    }

    // Находим все body разделы
    var notesBody = null;
    var commentsBody = null;

    var bodyDivs = document.getElementsByTagName("DIV");
    for (var i = 0; i < bodyDivs.length; i++) {
        if (bodyDivs[i].className == "body") {
            var fbname = bodyDivs[i].getAttribute("fbname") || "";
            if (fbname == "notes") {
                notesBody = bodyDivs[i];
            } else if (fbname == "comments") {
                commentsBody = bodyDivs[i];
            }
        }
    }

    // ==================================================
    // Функция: проверить, есть ли в элементе секции комментариев (id="c_N")
    // ==================================================
    function hasCommentSectionsInside(containerElement) {
        var allDivs = containerElement.getElementsByTagName("DIV");
        for (var i = 0; i < allDivs.length; i++) {
            if (allDivs[i].className == "section") {
                var secId = allDivs[i].getAttribute("id") || "";
                if (secId.length > 0) {
                    if (secId.charAt(0) == "c" && secId.charAt(1) == "_") {
                        var numStr = secId.substring(2);
                        var isNumber = true;
                        for (var k = 0; k < numStr.length; k++) {
                            var ch = numStr.charAt(k);
                            if (ch < "0" || ch > "9") {
                                isNumber = false;
                                break;
                            }
                        }
                        if (isNumber && numStr.length > 0) {
                            var num = parseInt(numStr, 10);
                            if (num >= 1 && num <= 5000) {
                                return true;
                            }
                        }
                    }
                }
            }
        }
        return false;
    }

    // ==================================================
    // Функция: найти заголовок сносок (первый title среди прямых потомков или внутри первой section)
    // ==================================================
    function findNotesTitleElement(bodyElement) {
        // Сначала ищем title среди прямых потомков body
        var child = bodyElement.firstChild;
        while (child) {
            if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "title") {
                return child;
            }
            child = child.nextSibling;
        }
        // Если не нашли — ищем внутри первой section
        child = bodyElement.firstChild;
        while (child) {
            if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "section") {
                var innerChild = child.firstChild;
                while (innerChild) {
                    if (innerChild.nodeType == 1 && innerChild.nodeName == "DIV" && innerChild.className == "title") {
                        return innerChild;
                    }
                    innerChild = innerChild.nextSibling;
                }
                break; // Проверили только первую section
            }
            child = child.nextSibling;
        }
        return null;
    }

    // ==================================================
    // Функция: получить текст заголовка
    // ==================================================
    function getTitleText(titleElement) {
        var text = titleElement.innerText || titleElement.textContent || "";
        return text.replace(/^\s+|\s+$/g, '');
    }

    // Проверяем наличие разделов
    var notesFound = (notesBody != null);
    var commentsFound = (commentsBody != null);

    // Если нет ни одного раздела
    if (!notesFound && !commentsFound) {
        MsgBox("Объединить разделы сносок и комментариев\nver. " + version + "\n\nРазделы сносок и комментариев отсутствуют.\nОбъединять нечего.", scriptName + " v" + version);
        return;
    }

    // Если нет раздела сносок
    if (!notesFound) {
        MsgBox("Объединить разделы сносок и комментариев\nver. " + version + "\n\nРаздел сносок (примечаний) отсутствует.\nОбъединять нечего.", scriptName + " v" + version);
        return;
    }

    // Если нет раздела комментариев — проверяем, не объединены ли они уже
    if (!commentsFound) {
        // Проверяем наличие секций с id="c_N" внутри notesBody
        if (hasCommentSectionsInside(notesBody)) {
            MsgBox("Объединить разделы сносок и комментариев\nver. " + version + "\n\nРазделы сносок и комментариев уже объединены.\nПовторное объединение не требуется.", scriptName + " v" + version);
        } else {
            MsgBox("Объединить разделы сносок и комментариев\nver. " + version + "\n\nРаздел комментариев отсутствует.\nОбъединять нечего.", scriptName + " v" + version);
        }
        return;
    }

    // Если оба раздела есть — выводим предупреждение (только в обычном режиме)
    if (showStatistics == 1) {
        var confirmMsg = "Объединить разделы сносок и комментариев\n";
        confirmMsg += "ver. " + version + "\n\n";
        confirmMsg += "Будут объединены разделы:\n";
        confirmMsg += "  \u2022 Сноски (примечания)\n";
        confirmMsg += "  \u2022 Комментарии\n\n";
        confirmMsg += "Раздел комментариев будет перенесён в конец раздела сносок.\n\n";
        confirmMsg += "Ссылки в основном тексте останутся без изменений.\n\n";

        // Информация о переименовании
        if (renameNotesTitle == 0) {
            confirmMsg += "Заголовок сносок не будет переименован.\n";
        } else if (renameNotesTitle == 1) {
            confirmMsg += "Заголовок сносок будет переименован в \"" + newNotesTitle + "\",\n";
            confirmMsg += "если он стандартный (\"Примечания\").\n";
        } else if (renameNotesTitle == 2) {
            confirmMsg += "Заголовок сносок будет переименован в \"" + newNotesTitle + "\".\n";
        }

        confirmMsg += "\nПродолжить?";

        if (!AskYesNo(confirmMsg, scriptName + " v" + version)) {
            return;
        }
    }

    // Таймер включаем после подтверждения
    var startTime = new Date();

    window.external.BeginUndoUnit(document, scriptName);

    // Определяем, нужно ли переименовывать заголовок
    var shouldRename = false;
    if (renameNotesTitle == 2) {
        shouldRename = true;
    } else if (renameNotesTitle == 1) {
        var notesTitleEl = findNotesTitleElement(notesBody);
        if (notesTitleEl) {
            var currentTitle = getTitleText(notesTitleEl);
            if (currentTitle == "Примечания") {
                shouldRename = true;
            }
        }
    }

    var titleRenamed = false;

    // Шаг 1: Оборачиваем содержимое notesBody в секцию
    var notesWrapper = document.createElement("DIV");
    notesWrapper.className = "section";

    // Переносим ВСЕ дочерние элементы из notesBody в notesWrapper
    var notesChildren = [];
    var notesChild = notesBody.firstChild;
    while (notesChild) {
        var nextNotesChild = notesChild.nextSibling;
        notesChildren.push(notesChild);
        notesChild = nextNotesChild;
    }

    for (var i = 0; i < notesChildren.length; i++) {
        // Проверяем, не заголовок ли это сносок и нужно ли его переименовать
        if (shouldRename && !titleRenamed) {
            if (notesChildren[i].nodeType == 1 && notesChildren[i].nodeName == "DIV" && notesChildren[i].className == "title") {
                // Ищем первый <P> внутри title
                var firstP = null;
                var pChild = notesChildren[i].firstChild;
                while (pChild) {
                    if (pChild.nodeType == 1 && pChild.nodeName == "P") {
                        firstP = pChild;
                        break;
                    }
                    pChild = pChild.nextSibling;
                }
                if (firstP) {
                    // Меняем текст первого <P> на новое название
                    firstP.innerHTML = newNotesTitle;
                    titleRenamed = true;
                }
            }
        }
        notesWrapper.appendChild(notesChildren[i]);
    }

    // Вставляем обёртку обратно в notesBody
    notesBody.appendChild(notesWrapper);

    // Шаг 2: Создаём секцию-обёртку для комментариев
    var commentsWrapper = document.createElement("DIV");
    commentsWrapper.className = "section";

    // Собираем все дочерние элементы из commentsBody
    var commentsChildren = [];
    var commentsChild = commentsBody.firstChild;
    while (commentsChild) {
        var nextCommentsChild = commentsChild.nextSibling;
        commentsChildren.push(commentsChild);
        commentsChild = nextCommentsChild;
    }

    var movedSections = 0;
    var commentsTitleFound = false;

    // Переносим элементы в секцию-обёртку комментариев
    for (var i = 0; i < commentsChildren.length; i++) {
        commentsWrapper.appendChild(commentsChildren[i]);
        if (commentsChildren[i].nodeType == 1 && commentsChildren[i].nodeName == "DIV") {
            if (commentsChildren[i].className == "section") {
                movedSections++;
            } else if (commentsChildren[i].className == "title") {
                commentsTitleFound = true;
            }
        }
    }

    // Добавляем секцию с комментариями в конец notesBody
    notesBody.appendChild(commentsWrapper);

    // Удаляем опустевший commentsBody
    commentsBody.removeNode(true);

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
    if (showStatistics == 1) {
        var msg = "";
        msg += "Объединение разделов сносок и комментариев\n";
        msg += "ver. " + version + "\n\n";

        msg += "\u2713 Разделы объединены\n\n";
        if (titleRenamed) {
            msg += "  \u2022 Заголовок сносок переименован в \"" + newNotesTitle + "\"\n";
        } else {
            msg += "  \u2022 Заголовок сносок сохранён без изменений\n";
        }
        msg += "  \u2022 Создана секция для комментариев\n";
        msg += "  \u2022 Перенесено секций комментариев: " + movedSections + "\n";
        if (commentsTitleFound) {
            msg += "  \u2022 Заголовок комментариев сохранён\n";
        }
        msg += "\n";
        msg += "Результат:\n";
        msg += "  Раздел \"" + (notesBody.getAttribute("fbname") || "notes") + "\" теперь содержит\n";
        msg += "  сноски и комментарии в одном разделе.\n\n";

        msg += "Время выполнения: " + timeStr + " сек.";

        MsgBox(msg, "FBE скрипт");
    }
}