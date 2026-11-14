// Скрипт "Объединить разделы сносок и комментариев по порядку" для редактора FBE
// version 1.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для объединения разделов сносок и комментариев в fb2 документах.
// Сноски и комментарии размещаются в порядке их появления в тексте (режим mixed).
// Все секции собираются в одну общую секцию внутри раздела notes.
// Нумерация сносок и комментариев при переносе не меняется.
// Ссылки в основном тексте документа остаются без изменений.
// Режим работы скрипта: обычный или тихий.
// Поддержка отмены всех действий (Ctrl+Z).

// version 1.1, 06.08.2026
//======================================

function Run() {
    var scriptName = "Объединить разделы сносок и комментариев по порядку";
    var version = "1.1";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима

    // Название общего заголовка:
    // 1 - "Примечания и комментарии"
    // 2 - "Сноски и комментарии"
    var titleMode = 1;

    // Или можно задать свой вариант (если не устраивают стандартные):
    // Оставьте пустым "", чтобы использовать titleMode
    var customTitle = ""; // Например: "Примечания и комментарии редактора"

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    var fbw_body = document.getElementById("fbw_body");
    if (!fbw_body) {
        MsgBox("Ошибка: не найден fbw_body", scriptName + " v" + version);
        return;
    }

    // Определяем общий заголовок
    var mergedTitle = "";
    if (customTitle.length > 0) {
        mergedTitle = customTitle;
    } else if (titleMode == 1) {
        mergedTitle = "Примечания и комментарии";
    } else {
        mergedTitle = "Сноски и комментарии";
    }

    // Находим все body разделы
    var mainBody = null;
    var notesBody = null;
    var commentsBody = null;

    var bodyDivs = document.getElementsByTagName("DIV");
    for (var i = 0; i < bodyDivs.length; i++) {
        if (bodyDivs[i].className == "body") {
            var fbname = bodyDivs[i].getAttribute("fbname") || "";
            if (fbname == "") {
                mainBody = bodyDivs[i];
            } else if (fbname == "notes") {
                notesBody = bodyDivs[i];
            } else if (fbname == "comments") {
                commentsBody = bodyDivs[i];
            }
        }
    }

    if (!mainBody) {
        MsgBox("Ошибка: не найден основной раздел документа", scriptName + " v" + version);
        return;
    }

    // Проверяем наличие разделов
    var notesFound = (notesBody != null);
    var commentsFound = (commentsBody != null);

    // Если нет ни одного раздела
    if (!notesFound && !commentsFound) {
        MsgBox("Объединить разделы сносок и комментариев по порядку\nver. " + version + "\n\nРазделы сносок и комментариев отсутствуют.\nОбъединять нечего.", scriptName + " v" + version);
        return;
    }

    // Если нет раздела сносок
    if (!notesFound) {
        MsgBox("Объединить разделы сносок и комментариев по порядку\nver. " + version + "\n\nРаздел сносок (примечаний) отсутствует.\nОбъединять нечего.", scriptName + " v" + version);
        return;
    }

    // Если нет раздела комментариев
    if (!commentsFound) {
        MsgBox("Объединить разделы сносок и комментариев по порядку\nver. " + version + "\n\nРаздел комментариев отсутствует.\nОбъединять нечего.", scriptName + " v" + version);
        return;
    }

    // ==================================================
    // Шаг 1: Сбор порядка ссылок из основного текста
    // ==================================================

    // Получаем все ссылки из mainBody
    var orderedIds = [];
    var allLinks = mainBody.getElementsByTagName("A");

    for (var i = 0; i < allLinks.length; i++) {
        var href = allLinks[i].getAttribute("href") || "";
        if (href.length > 0) {
            // Извлекаем локальную часть href (после #)
            var hashPos = -1;
            for (var j = 0; j < href.length; j++) {
                if (href.charAt(j) == "#") {
                    hashPos = j;
                    break;
                }
            }
            if (hashPos >= 0) {
                var id = href.substring(hashPos + 1);
                // Проверяем, что это сноска (n_) или комментарий (c_)
                if (id.length > 2 && (id.substring(0, 2) == "n_" || id.substring(0, 2) == "c_")) {
                    // Проверяем, что после префикса идут только цифры
                    var numPart = id.substring(2);
                    var allDigits = true;
                    for (var k = 0; k < numPart.length; k++) {
                        if (numPart.charAt(k) < "0" || numPart.charAt(k) > "9") {
                            allDigits = false;
                            break;
                        }
                    }
                    if (allDigits && numPart.length > 0) {
                        // Проверяем, нет ли уже такого id в списке (избегаем дублей)
                        var alreadyInList = false;
                        for (var m = 0; m < orderedIds.length; m++) {
                            if (orderedIds[m] == id) {
                                alreadyInList = true;
                                break;
                            }
                        }
                        if (!alreadyInList) {
                            orderedIds.push(id);
                        }
                    }
                }
            }
        }
    }

    if (orderedIds.length == 0) {
        MsgBox("Объединить разделы сносок и комментариев по порядку\nver. " + version + "\n\nВ основном тексте не найдено ссылок на сноски или комментарии.\nОбъединять нечего.", scriptName + " v" + version);
        return;
    }

    // ==================================================
    // Шаг 2: Сверка — собираем все доступные секции и проверяем
    // ==================================================

    // Собираем все секции из notesBody в массив ключей и объект
    var allNoteIds = [];
    var allNoteSections = {};
    var notesSections = notesBody.getElementsByTagName("DIV");
    for (var i = 0; i < notesSections.length; i++) {
        if (notesSections[i].className == "section") {
            var secId = notesSections[i].getAttribute("id") || "";
            if (secId.length > 0) {
                allNoteIds.push(secId);
                allNoteSections[secId] = notesSections[i];
            }
        }
    }

    // Собираем все секции из commentsBody в массив ключей и объект
    var allCommentIds = [];
    var allCommentSections = {};
    var commentsSections = commentsBody.getElementsByTagName("DIV");
    for (var i = 0; i < commentsSections.length; i++) {
        if (commentsSections[i].className == "section") {
            var secId = commentsSections[i].getAttribute("id") || "";
            if (secId.length > 0) {
                allCommentIds.push(secId);
                allCommentSections[secId] = commentsSections[i];
            }
        }
    }

    // Проверяем, что все id из списка имеют соответствующие секции
    var missingIds = [];
    for (var i = 0; i < orderedIds.length; i++) {
        var id = orderedIds[i];
        var found = false;
        if (id.substring(0, 2) == "n_") {
            if (allNoteSections[id]) found = true;
        } else if (id.substring(0, 2) == "c_") {
            if (allCommentSections[id]) found = true;
        }
        if (!found) {
            missingIds.push(id);
        }
    }

    if (missingIds.length > 0) {
        var missingMsg = "Объединить разделы сносок и комментариев по порядку\nver. " + version + "\n\n";
        missingMsg += "Не найдены секции для следующих ссылок:\n";
        for (var i = 0; i < missingIds.length; i++) {
            missingMsg += "  \u2022 " + missingIds[i] + "\n";
        }
        missingMsg += "\nПроверьте документ и повторите попытку.";
        MsgBox(missingMsg, scriptName + " v" + version);
        return;
    }

    // Проверяем, нет ли «лишних» секций без ссылок (используем массивы ключей вместо for...in)
    var unusedIds = [];

    for (var i = 0; i < allNoteIds.length; i++) {
        var id = allNoteIds[i];
        var found = false;
        for (var j = 0; j < orderedIds.length; j++) {
            if (orderedIds[j] == id) {
                found = true;
                break;
            }
        }
        if (!found) {
            unusedIds.push(id + " (примечание)");
        }
    }

    for (var i = 0; i < allCommentIds.length; i++) {
        var id = allCommentIds[i];
        var found = false;
        for (var j = 0; j < orderedIds.length; j++) {
            if (orderedIds[j] == id) {
                found = true;
                break;
            }
        }
        if (!found) {
            unusedIds.push(id + " (комментарий)");
        }
    }

    if (unusedIds.length > 0) {
        var unusedMsg = "Объединить разделы сносок и комментариев по порядку\nver. " + version + "\n\n";
        unusedMsg += "Найдены секции без ссылок в тексте:\n";
        for (var i = 0; i < unusedIds.length; i++) {
            unusedMsg += "  \u2022 " + unusedIds[i] + "\n";
        }
        unusedMsg += "\nПроверьте документ и повторите попытку.";
        MsgBox(unusedMsg, scriptName + " v" + version);
        return;
    }

    // ==================================================
    // Подтверждение (только в обычном режиме)
    // ==================================================
    if (showStatistics == 1) {
        var noteCount = 0;
        var commentCount = 0;
        for (var i = 0; i < orderedIds.length; i++) {
            if (orderedIds[i].substring(0, 2) == "n_") {
                noteCount++;
            } else {
                commentCount++;
            }
        }

        var confirmMsg = "Объединить разделы сносок и комментариев по порядку\n";
        confirmMsg += "ver. " + version + "\n\n";
        confirmMsg += "Будут объединены в порядке появления в тексте:\n";
        confirmMsg += "  \u2022 Сносок (примечаний): " + noteCount + "\n";
        confirmMsg += "  \u2022 Комментариев: " + commentCount + "\n";
        confirmMsg += "  \u2022 Всего: " + orderedIds.length + "\n\n";
        confirmMsg += "Общий заголовок: \"" + mergedTitle + "\"\n";
        confirmMsg += "Ссылки в основном тексте останутся без изменений.\n\n";
        confirmMsg += "Продолжить?";

        if (!AskYesNo(confirmMsg, scriptName + " v" + version)) {
            return;
        }
    }

    // Таймер включаем после подтверждения
    var startTime = new Date();

    window.external.BeginUndoUnit(document, scriptName);

    // ==================================================
    // Шаг 3: Перестроение секций в notesBody
    // ==================================================

    // Создаём общую секцию-обёртку
    var mergedSection = document.createElement("DIV");
    mergedSection.className = "section";

    // Создаём заголовок
    var titleDiv = document.createElement("DIV");
    titleDiv.className = "title";
    var titleP = document.createElement("P");
    titleP.innerHTML = mergedTitle;
    titleDiv.appendChild(titleP);
    mergedSection.appendChild(titleDiv);

    // Переносим секции в порядке orderedIds
    var movedCount = 0;
    for (var i = 0; i < orderedIds.length; i++) {
        var id = orderedIds[i];
        var section = null;
        if (id.substring(0, 2) == "n_") {
            section = allNoteSections[id];
        } else {
            section = allCommentSections[id];
        }
        if (section) {
            if (section.parentNode) {
                section.parentNode.removeChild(section);
            }
            mergedSection.appendChild(section);
            movedCount++;
        }
    }

    // Удаляем всё содержимое notesBody
    while (notesBody.firstChild) {
        notesBody.removeChild(notesBody.firstChild);
    }

    // Добавляем объединённую секцию в notesBody
    notesBody.appendChild(mergedSection);

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
        msg += "Объединение разделов сносок и комментариев по порядку\n";
        msg += "ver. " + version + "\n\n";

        msg += "\u2713 Разделы объединены\n\n";
        msg += "  \u2022 Порядок: по мере появления в тексте\n";
        msg += "  \u2022 Перенесено секций: " + movedCount + "\n";
        msg += "  \u2022 Общий заголовок: \"" + mergedTitle + "\"\n";
        msg += "\n";
        msg += "Результат:\n";
        msg += "  Все сноски и комментарии теперь в разделе\n";
        msg += "  \"" + (notesBody.getAttribute("fbname") || "notes") + "\" в порядке следования ссылок.\n\n";

        msg += "Время выполнения: " + timeStr + " сек.";

        MsgBox(msg, "FBE скрипт");
    }
}