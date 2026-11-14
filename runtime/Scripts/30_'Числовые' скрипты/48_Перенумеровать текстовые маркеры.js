// Скрипт "Перенумеровать текстовые маркеры" для редактора FBE
// version 1.0
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для приведения нумерации текстовых маркеров к единому сквозному виду в fb2 документах.
// Текстовый маркер может быть любым по выбору пользователя.
// По умолчанию используется маркер zzz_pic.
// Маркер должен быть расположен в отдельном абзаце, без другого текста.
// Скрипт находит все маркеры заданного типа (с нумерацией или без) и присваивает им сквозную нумерацию.
// Устраняет пропуски и приводит нумерацию к формату: marker_001, marker_002... (минимум 3 цифры, при >999 — 4 цифры).
// По умолчанию обрабатывается только основной раздел документа, без сносок и комментариев.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.0, 02.07.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Перенумеровать текстовые маркеры";
    var version = "1.0";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    // Тип маркера (без подчеркивания и номера)
    var markerType = "zzz_pic";

    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1;

    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0; // 0 - нет, 1 - да

    // Обрабатывать раздел комментариев
    var processCommentsSection = 0; // 0 - нет, 1 - да

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    // Включаем поддержку отмены
    window.external.BeginUndoUnit(document, scriptName);

    // Получаем неразрывный пробел из настроек FBE
    var nbspEntity = "&nbsp;";
    var nbspChar = String.fromCharCode(160);
    try {
        var nbspCharTemp = window.external.GetNBSP();
        if (nbspCharTemp.charCodeAt(0) != 160) {
            nbspEntity = nbspCharTemp;
        }
        nbspChar = nbspCharTemp;
    } catch (e) {
        nbspChar = String.fromCharCode(160);
    }

    // Необычные пробелы для нормализации
    var unusualSpaces = String.fromCharCode(160) +
        String.fromCharCode(8194) +
        String.fromCharCode(8195) +
        String.fromCharCode(8196) +
        String.fromCharCode(8197) +
        String.fromCharCode(8198) +
        String.fromCharCode(8239) +
        String.fromCharCode(8201) +
        String.fromCharCode(8202) +
        nbspChar;

    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================

    // Нормализация пробелов в строке
    function normalizeSpaces(str) {
        var result = "";
        for (var i = 0; i < str.length; i++) {
            var ch = str.charAt(i);
            var isUnusual = false;
            for (var j = 0; j < unusualSpaces.length; j++) {
                if (ch == unusualSpaces.charAt(j)) {
                    isUnusual = true;
                    break;
                }
            }
            if (isUnusual) {
                result += " ";
            } else {
                result += ch;
            }
        }
        return result;
    }

    // Удаление пробелов с обоих концов строки
    function trimStr(str) {
        return str.replace(/^\s+|\s+$/g, '');
    }

    // Проверка, является ли строка маркером (с нумерацией или без)
    function isMarker(str, marker) {
        // Точное совпадение без номера
        if (str === marker) return true;
        // С нумерацией (с _ или без)
        var pattern = new RegExp("^" + marker + "_?\\d{3,}$");
        return pattern.test(str);
    }

    // Получение всех элементов P в документе
    function getAllParagraphsInBody(bodyDiv) {
        var paragraphs = [];
        var allP = bodyDiv.getElementsByTagName("P");
        for (var i = 0; i < allP.length; i++) {
            paragraphs.push(allP[i]);
        }
        return paragraphs;
    }

    // ==================================================
    // СБОР ДАННЫХ — Фаза 1: ТОЛЬКО чтение
    // ==================================================

    var allParagraphs = [];
    var allMarkers = []; // массив объектов {paragraph: P, text: "..."}

    // Находим все body DIV'ы
    var allDivs = document.getElementsByTagName("DIV");
    var bodyDivs = [];
    for (var i = 0; i < allDivs.length; i++) {
        if (allDivs[i].className == "body") {
            bodyDivs.push(allDivs[i]);
        }
    }

    for (var b = 0; b < bodyDivs.length; b++) {
        var bodyDiv = bodyDivs[b];
        var fbname = bodyDiv.getAttribute("fbname") || "";

        // Проверяем, нужно ли обрабатывать этот раздел
        if (fbname == "") {
            // Основной раздел — всегда обрабатываем
        } else if (fbname == "notes" && processNotesSection) {
            // Сноски — обрабатываем если разрешено
        } else if (fbname == "comments" && processCommentsSection) {
            // Комментарии — обрабатываем если разрешено
        } else {
            continue; // Пропускаем этот раздел
        }

        var paragraphs = getAllParagraphsInBody(bodyDiv);

        // Проходим по абзацам В ПОРЯДКЕ следования (от начала к концу)
        for (var p = 0; p < paragraphs.length; p++) {
            var para = paragraphs[p];
            var text = para.innerText || para.textContent || "";

            // Нормализуем пробелы
            var normalized = normalizeSpaces(text);
            var trimmed = trimStr(normalized);

            if (trimmed.length === 0) continue;

            // Проверяем, является ли маркером (любым)
            if (isMarker(trimmed, markerType)) {
                allMarkers.push({
                    paragraph: para,
                    text: trimmed
                });
            }
        }
    }

    // ==================================================
    // ПРОВЕРКА: маркеры отсутствуют
    // ==================================================
    if (allMarkers.length === 0) {
        var noMarkersMsg = scriptName + "\n";
        noMarkersMsg += "ver. " + version + "\n\n";
        noMarkersMsg += "Заданные маркеры \"" + markerType + "\" отсутствуют.";

        MsgBox(noMarkersMsg, "FBE скрипт");
        window.external.EndUndoUnit(document);
        return;
    }

    // ==================================================
    // ЗАПРОС ПОДТВЕРЖДЕНИЯ (только в обычном режиме)
    // ==================================================
    if (showStatistics) {
        var confirmMsg = scriptName + "\n";
        confirmMsg += "ver. " + version + "\n\n";
        confirmMsg += "Перенумеровать маркеры \"" + markerType + "\"?\n";
        confirmMsg += "Будет обработано: " + allMarkers.length + " шт.\n";
        confirmMsg += "Все маркеры получат сквозную нумерацию:\n";
        confirmMsg += markerType + " → " + markerType + "_001\n";
        confirmMsg += markerType + "_005 → " + markerType + "_001";

        if (!AskYesNo(confirmMsg)) {
            window.external.EndUndoUnit(document);
            return;
        }
    }

    // Запускаем таймер после подтверждения
    var startTime = new Date();

    // ==================================================
    // Фаза 2: ТОЛЬКО запись (перенумерация)
    // ==================================================

    var totalMarkers = allMarkers.length;
    var digitCount = 3;
    if (totalMarkers > 999) {
        digitCount = 4;
    }

    // Обрабатываем в порядке от начала к концу документа
    for (var m = 0; m < totalMarkers; m++) {
        var para = allMarkers[m].paragraph;
        var num = m + 1;
        var numStr = String(num);
        while (numStr.length < digitCount) {
            numStr = "0" + numStr;
        }
        var newText = markerType + "_" + numStr;

        // Очищаем абзац и вставляем новый текст
        while (para.firstChild) {
            para.removeChild(para.firstChild);
        }
        para.appendChild(document.createTextNode(newText));
    }

    // Завершаем отмену
    window.external.EndUndoUnit(document);

    // ==================================================
    // СТАТИСТИКА
    // ==================================================

    if (showStatistics) {
        var endTime = new Date();
        var elapsed = (endTime.getTime() - startTime.getTime()) / 1000;
        var elapsedStr = elapsed.toFixed(3).replace(".", ",");

        var msg = scriptName + "\n";
        msg += "ver. " + version + "\n\n";
        msg += "Статистика перенумерации маркеров:\n\n";
        msg += "\u2713 Тип маркера: " + markerType + "\n";
        msg += "\u2713 Перенумеровано: " + totalMarkers + " шт.\n";
        msg += "  \u2022 Формат номера: " + digitCount + " цифр (" + markerType + "_" + numStr.substring(0, digitCount) + "...)\n\n";
        msg += "Время выполнения: " + elapsedStr + " сек.";

        MsgBox(msg, "FBE скрипт");
    }
}
