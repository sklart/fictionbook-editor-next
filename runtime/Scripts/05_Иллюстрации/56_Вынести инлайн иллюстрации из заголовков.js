// Скрипт "Вынести инлайн иллюстрации из заголовков" для редактора FBE
// version 1.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для выноса внутриабзацных (инлайн) иллюстраций (SPAN class=image)
// из заголовков разделов (DIV class=title) и вставки их после заголовка
// уже как блочных иллюстраций (DIV class=image) в fb2 документах.
// Обрабатываются ВСЕ разделы документа: основной, сноски (notes) и комментарии (comments).
// Если в заголовке несколько иллюстраций - все переносятся после заголовка
// в порядке их обнаружения, разделяясь пустыми строками.
// Настройка: переносить реальные иллюстрации или заменять на пустышки (href="#undefined").
// Настройка: если заголовок после выноса остался пустым (без текста) -
// добавлять пользовательский маркер или оставить пустым.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.2, 05.09.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Вынести инлайн иллюстрации из заголовков";
    var version = "1.2";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // 0 - тихий режим, 1 - показывать статистику

    // Переносить реальные иллюстрации или заменять их на пустые картинки
    // 0 - переносить реальные, 1 - заменять на пустые картинки (href="#undefined")
    var replaceWithEmpty = 0; // 0 - реальные, 1 - пустышки

    // Если заголовок после выноса остался пустым (без текста):
    // 0 - оставить заголовок пустым, 1 - добавить пользовательский маркер
    var addMarkerToEmptyTitle = 1; // 0 - оставить пустым, 1 - добавить маркер

    // Пользовательский маркер для пустого заголовка
    var emptyTitleMarker = "ZZZ_empty_title";

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    // Объект для статистики
    var stats = {
        titlesProcessed: 0,
        imagesMoved: 0,
        emptyTitlesMarked: 0,
        titlesSkippedNoImages: 0
    };

    // Получаем неразрывный пробел из настроек FBE
    var nbspChar;
    var nbspEntity;
    try {
        nbspChar = window.external.GetNBSP();
        if (nbspChar.charCodeAt(0) == 160) {
            nbspEntity = "&nbsp;";
        } else {
            nbspEntity = nbspChar;
        }
    } catch(e) {
        nbspChar = String.fromCharCode(160);
        nbspEntity = "&nbsp;";
    }

    // Список необычных пробелов
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
    // ФУНКЦИИ ПРОВЕРКИ
    // ==================================================

    function isSpaceChar(ch) {
        if (ch == " " || ch == "\t" || ch == "\n" || ch == "\r") return true;
        if (ch == nbspChar) return true;
        for (var i = 0; i < unusualSpaces.length; i++) {
            if (ch == unusualSpaces.charAt(i)) return true;
        }
        return false;
    }

    function isEmptyText(str) {
        if (!str) return true;
        for (var i = 0; i < str.length; i++) {
            if (!isSpaceChar(str.charAt(i))) return false;
        }
        return true;
    }

    // Проверка, есть ли текстовое содержимое в элементе
    function hasTextContent(element) {
        var children = element.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 3) {
                // Текстовый узел
                if (!isEmptyText(child.nodeValue)) {
                    return true;
                }
            } else if (child.nodeType == 1) {
                // Элемент, рекурсивно проверяем
                if (child.nodeName != "SPAN" || child.className != "image") {
                    // Не считаем инлайн иллюстрации текстом
                    if (hasTextContent(child)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // Проверка, является ли P пустой строкой
    function isEmptyLineP(p) {
        if (!p || p.nodeName != "P") return false;
        var html = p.innerHTML || "";
        var trimmed = html.replace(/^\s+|\s+$/g, '');
        return trimmed == nbspEntity || trimmed == nbspChar || trimmed == "";
    }

    // Удаление пустых строк из заголовка
    function removeEmptyLinesFromTitle(titleElement) {
        var children = titleElement.childNodes;
        for (var i = children.length - 1; i >= 0; i--) {
            var child = children[i];
            if (child.nodeType == 1 && child.nodeName == "P" && isEmptyLineP(child)) {
                child.removeNode(true);
            }
        }
    }

    // Создание блочной иллюстрации DIV из инлайн SPAN
    function createDivFromSpan(span) {
        var div = document.createElement("DIV");
        div.className = "image";
        div.setAttribute("onresizestart", "return false");
        div.setAttribute("contentEditable", "false");

        if (replaceWithEmpty) {
            div.setAttribute("href", "#undefined");
        } else {
            var href = span.getAttribute("href") || "";
            div.setAttribute("href", href);
        }

        // Копируем содержимое SPAN в DIV
        var children = span.childNodes;
        for (var i = 0; i < children.length; i++) {
            div.appendChild(children[i].cloneNode(true));
        }

        return div;
    }

    // Создание пустой строки
    function createEmptyLine() {
        var emptyP = document.createElement("P");
        emptyP.innerHTML = nbspEntity;
        window.external.inflateBlock(emptyP) = true;
        return emptyP;
    }

    // Добавление маркера в пустой заголовок
    function addMarkerToTitle(titleElement) {
        // Сначала удаляем все пустые строки
        removeEmptyLinesFromTitle(titleElement);

        // Потом добавляем маркер
        var p = document.createElement("P");
        var markerText = document.createTextNode(emptyTitleMarker);
        p.appendChild(markerText);
        titleElement.appendChild(p);
        stats.emptyTitlesMarked++;
    }

    // Обработка одного заголовка
    function processTitle(titleElement) {
        // Находим все инлайн иллюстрации внутри заголовка
        var spans = titleElement.getElementsByTagName("SPAN");
        var imageSpans = [];
        for (var i = 0; i < spans.length; i++) {
            if (spans[i].className == "image") {
                imageSpans.push(spans[i]);
            }
        }

        if (imageSpans.length == 0) {
            return false; // Нет иллюстраций в заголовке
        }

        // Создаем блочные иллюстрации заранее
        var blockImages = [];
        for (var i = 0; i < imageSpans.length; i++) {
            blockImages.push(createDivFromSpan(imageSpans[i]));
        }

        // Удаляем инлайн SPAN'ы из заголовка
        for (var i = imageSpans.length - 1; i >= 0; i--) {
            imageSpans[i].removeNode(true);
        }

        // Проверяем, остался ли текст в заголовке
        if (!hasTextContent(titleElement)) {
            if (addMarkerToEmptyTitle) {
                addMarkerToTitle(titleElement);
            }
        }

        // Вставляем блочные иллюстрации после заголовка
        var parent = titleElement.parentNode;
        var insertAfter = titleElement;

        for (var i = 0; i < blockImages.length; i++) {
            // Если не первая иллюстрация - вставляем пустую строку перед ней
            if (i > 0) {
                var emptyLine = createEmptyLine();
                var nextSibling = insertAfter.nextSibling;
                if (nextSibling) {
                    parent.insertBefore(emptyLine, nextSibling);
                } else {
                    parent.appendChild(emptyLine);
                }
                insertAfter = emptyLine;
            }

            // Вставляем иллюстрацию
            var nextSibling = insertAfter.nextSibling;
            if (nextSibling) {
                parent.insertBefore(blockImages[i], nextSibling);
            } else {
                parent.appendChild(blockImages[i]);
            }
            insertAfter = blockImages[i];
        }

        stats.titlesProcessed++;
        stats.imagesMoved += imageSpans.length;

        return true;
    }

    // ==================================================
    // ОСНОВНАЯ ЛОГИКА
    // ==================================================

    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) {
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка. Не удалось найти раздел fbw_body.", "FBE скрипт");
        return;
    }

    // Находим все заголовки
    var allDivs = fbwBody.getElementsByTagName("DIV");
    var titleDivs = [];
    for (var i = 0; i < allDivs.length; i++) {
        if (allDivs[i].className == "title") {
            titleDivs.push(allDivs[i]);
        }
    }

    if (titleDivs.length == 0) {
        MsgBox(scriptName + "\nver. " + version + "\n\nВ документе не найдено заголовков.", "FBE скрипт");
        return;
    }

    // Подсчитываем заголовки с иллюстрациями
    var titlesWithImages = 0;
    var totalImages = 0;
    for (var i = 0; i < titleDivs.length; i++) {
        var spans = titleDivs[i].getElementsByTagName("SPAN");
        var count = 0;
        for (var j = 0; j < spans.length; j++) {
            if (spans[j].className == "image") {
                count++;
            }
        }
        if (count > 0) {
            titlesWithImages++;
            totalImages += count;
        }
    }

    if (titlesWithImages == 0) {
        MsgBox(scriptName + "\nver. " + version + "\n\nВ заголовках документа не найдено инлайн иллюстраций.", "FBE скрипт");
        return;
    }

    if (showStatistics) {
        var confirmMsg = scriptName + "\nver. " + version + "\n\n";
        confirmMsg += "Найдено заголовков с иллюстрациями: " + titlesWithImages + "\n";
        confirmMsg += "Всего инлайн иллюстраций в заголовках: " + totalImages + "\n\n";
        confirmMsg += "Выполнить перенос иллюстраций?";

        if (!AskYesNo(confirmMsg)) {
            return;
        }
    }

    var startTime = new Date().getTime();

    window.external.BeginUndoUnit(document, scriptName + " ver. " + version);

    try {
        // Обрабатываем заголовки в обратном порядке
        for (var i = titleDivs.length - 1; i >= 0; i--) {
            processTitle(titleDivs[i]);
        }

        window.external.EndUndoUnit(document);

        var endTime = new Date().getTime();
        var timeDiff = (endTime - startTime) / 1000;
        var timeStr = timeDiff.toFixed(3).replace(".", ",");

        if (showStatistics) {
            var msg = scriptName + "\nver. " + version + "\n\n";
            msg += "Результаты обработки:\n\n";
            msg += "✓ Обработано заголовков: " + stats.titlesProcessed + "\n";
            msg += "✓ Вынесено иллюстраций: " + stats.imagesMoved + "\n";
            if (stats.emptyTitlesMarked > 0) {
                msg += "✓ Добавлено маркеров в пустые заголовки: " + stats.emptyTitlesMarked + "\n";
            }
            msg += "\n• Режим: " + (replaceWithEmpty ? "замена на пустышки" : "перенос реальных иллюстраций") + "\n";
            if (stats.emptyTitlesMarked > 0) {
                msg += "• Маркер пустого заголовка: " + emptyTitleMarker + "\n";
            }
            msg += "\nВремя выполнения: " + timeStr + " сек";

            MsgBox(msg, "FBE скрипт");
        }

    } catch (e) {
        window.external.EndUndoUnit(document);
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка выполнения:\n" + e.message, "FBE скрипт");
    }
}