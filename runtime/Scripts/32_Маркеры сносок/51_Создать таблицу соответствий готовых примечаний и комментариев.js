// Скрипт «Создать таблицу соответствий готовых примечаний и комментариев»
// Версия: 1.6 (июнь, 2026)
// автор: stokber + DeepSeek;
// за часть кода показа таблицы в браузере по умолчанию отдельная благодарность Lancer-у.
// Скрипт поможет проверить правильность соответствия фрагментов текста перед ссылкой и текстов готовых примечаний\комментариев.
// Сценарий рассчитан на работу с примечаниями (вида "n_1") и комментариями (вида "c_1"), созданными и унифицированными скриптами, входящими в штатный состав программы FBE, и расположенными в отдельных "body". С другими видами примечаний и комментариев корректная работа не гарантируется.

function Run() {

    var name = "Создать таблицу соответствий готовых примечаний и комментариев";
    var version = "1.6";

    // ========== НАСТРОЙКИ СКРИПТА ==========
    // 1- показывать в window.open.; 2-показывать в браузере по умолчанию; 
    var show = "1"; // показывать в window.open.
    // var show = "2"; // показывать в браузере по умолчанию.

    // Открытие таблицы (только для window.open; show = "1")
    var windowPosition = "left"; // "left" - в левой половине экрана, "right" - правая половина
    // var windowPosition = "right"; // "right" - в правой половине экрана.
    var windowWidthPercent = 0.49; // доля ширины экрана (0.5 = половина)
    var windowHeightPercent = 0.95; // доля высоты экрана (0.5 = половина)

    var fragmentLength = 120; // Длина фрагмента текста перед ссылкой.
    var noteLength = 120; // Длина текста примечания/комментария.

    // Режим отображения: "separate" - отдельно (сначала примечания, потом комментарии):
    //                   "mixed" - в порядке появления в документе:
    var displayMode = "separate";
    // var displayMode = "mixed";

    // Цвета фона для заголовка:
    // var titleColor = "#4CAF50";
    var titleColor = "DarkKhaki";

    // Цвета фона для строк
    // var noteColor = "#E6F0FA";   // Светло-голубой для примечаний.
    var noteColor = "Khaki"; // 
    // var commentColor = "#F0E6FA"; // Светло-фиолетовый для комментариев.
    var commentColor = "Goldenrod"; // 

    // отображение текста примечаний-комментариев:
    var colParagraphsTextNoteComm = "1"; // только одну первую строку.
    // var colParagraphsTextNoteComm = "2"; // все строки, но насколько позволяет noteLength.

    // Режим исправления ошибок FBE:
    // true - автоматически исправлять пути в ссылках;
    // false - показать сообщение с рекомендацией (Alt+F3 > Alt+F2)
    var autoFixPaths = true;
    // var autoFixPaths = false;
    // Конец блока настроек скрипта.
    // =========================

    // блок открытия таблицы (только для window.open; show = "1"):
    // Вычисляем размеры и положение окна результатов
    var screenWidth = window.screen.availWidth || window.screen.width;
    var screenHeight = window.screen.availHeight || window.screen.height;
    var winWidth = Math.floor(screenWidth * windowWidthPercent);
    var winHeight = Math.floor(screenHeight * windowHeightPercent);
    var winLeft = 0;
    var winTop = 0;

    if (windowPosition === "right") {
        winLeft = screenWidth - winWidth;
    } else {
        winLeft = 0;
    }
    // Можно также центрировать по вертикали.
    // конец блока для открытия таблицы (только для window.open; show = "1").
    // ==============================================

    // Получаем элемент, содержащий тело документа FB2:
    var bodyDiv = document.getElementById("fbw_body");
    if (!bodyDiv) {
        MsgBox("Не удалось получить содержимое документа.");
        return;
    }

    // Проверяем наличие проблемных ссылок с путями файлов:
    var hasPathIssues = checkForPathIssues(bodyDiv);

    if (hasPathIssues) {
        if (autoFixPaths) {

            // Автоматически исправляем пути в ссылках:
            window.external.BeginUndoUnit(document, "исправление ссылок");
            fixPathIssues(bodyDiv);
            window.external.EndUndoUnit(document);

            // MsgBox("Обнаружены проблемные ссылки с путями файлов.\nОни были автоматически исправлены.");
        } else {
            // Просто показываем информационное сообщение с рекомендацией:
            MsgBox("Обнаружены проблемные ссылки с путями файлов.\n\nРекомендуется переключиться в режим Кода и обратно для исправления.\n(Alt+F3 > Alt+F2)");
            // Продолжаем выполнение скрипта.
        }
    }

    // Массивы для хранения информации:
    var notes = [];
    var comments = [];

    // Для mixed-режима нужен общий массив с указанием типа:
    var mixedItems = [];

    // Получаем все ссылки:
    var allLinks = bodyDiv.getElementsByTagName("a");
    for (var i = 0; i < allLinks.length; i++) {
        var link = allLinks[i];
        var href = getCleanHref(link.href || "");

        // Проверяем примечания (class содержит "note" и href содержит "#n_"):
        if (link.className && link.className.indexOf("note") != -1 && href.indexOf("#n_") != -1) {
            processLink(link, "note", href, notes, mixedItems);
        }

        // Проверяем комментарии (href содержит "#c_"):
        if (href.indexOf("#c_") != -1) {
            processLink(link, "comment", href, comments, mixedItems);
        }
    }
    var html = "<html>";

    if (displayMode == "separate") {
        // Режим 1: отдельно примечания, потом комментарии:
        if (notes.length > 0) {
            html += "<h2>Примечания (найдено: " + notes.length + ")</h2>";
            html += generateTable(notes, "note");
        }

        if (comments.length > 0) {
            html += "<h2>Комментарии (найдено: " + comments.length + ")</h2>";
            html += generateTable(comments, "comment");
        }

        if (notes.length == 0 && comments.length == 0) {
            alert("Примечания и комментарии не найдены.");
            return;
        }
    } else {
        // Режим 2: смешанный (в порядке появления):
        if (mixedItems.length > 0) {
            html += "<h2>Примечания и комментарии в порядке появления</h2>";
            html += "<p>Примечаний: " + notes.length + ", Комментариев: " + comments.length + "</p>";
            html += generateMixedTable(mixedItems);
        } else {
            alert("Примечания и комментарии не найдены.");
            return;
        }
    }
    html += "<p align=\"center\"><small>Скрипт \"" + name + "\" v." + version + "</small></p>";
    html += "</html>";

    // MsgBox(html);
    // clipboardData.setData("Text",html); // поместить данные в буфер обмена.

    if (show == 1) {
        MyMsgWindow1(html);
    }
    if (show == 2) {
        // Базовый путь к папке HTML: отрезаем 'main.html' из адреса оболочки FBE:
        var basePath = document.location.href.replace("file:///", "").replace(/%20/g, " ").replace(/main\.html/, "HTML/");
        // Задаем строгое постоянное имя файла отчета (без штампов даты и времени):
        var filePatch = basePath + "table_notes_temp.html";
        // Формируем понятный системный путь для вывода в финальное сообщение пользователю:
        var systemPath = basePath.replace(/\//g, "\\");
        MyMsgWindow2(html);
    }

    function MyMsgWindow1(html) {
        var MsgWindow = window.open("HTML/Создать таблицу соответствий готовых примечаний и комментариев.html", null, "height=" + winHeight +
            ",width=" + winWidth +
            ",left=" + winLeft +
            ",top=" + winTop + ",status=no,toolbar=no,menubar=no,location=no,scrollbars=yes,resizable=yes");

        // Выводим таблицу в документ:
        MsgWindow.document.body.innerHTML = html;
    }

    function MyMsgWindow2(html) {
        try {
            // Создаем системные объекты для работы с файлами (FSO) и командной строкой (Shell):
            var shell = new ActiveXObject("WScript.Shell");
            var fso = new ActiveXObject("Scripting.FileSystemObject");

            // Создаем файл. Флаги (true, true) принудительно включают Юникод (UTF-16), защищая от пустых файлов:
            var fh = fso.CreateTextFile(filePatch, true, true);

            // Записываем шапку HTML-документа с кодировкой и кастомным тайтлом вкладки:
            fh.WriteLine("<!DOCTYPE html><html><head><meta http-equiv='Content-Type' content='text/html; charset=utf-8'><title>Таблица готовых примечаний и комментариев</title>");

            // Задаем стили таблицы: шрифт без засечек, увеличенный размер 18px для удобства чтения:
            fh.WriteLine("<style>body { font-family: sans-serif; font-size: 16px; padding: 20px; } table { border-collapse: collapse; width: 100%; font-size: 14px; } th, td { border: 1px solid #ccc; padding: 10px; } th { background-color: " + titleColor + "; }" +

                ".summary-list { margin: 10px 0 20px 0; padding: 10px; background-color: #f0f0f0; border-left: 4px solid #ccc; }" +
                ".summary-list ul { margin: 0; padding-left: 20px; }" +
                ".summary-list li { margin: 5px 0; }" +

                "</style></head><body>");

            var help = "<p ><small>Скрипт открыл таблицу в браузере по умолчанию.</small><br><small>" +
                "Отчет сохранен в файле table_notes_temp.html в папке:\n" + systemPath + ".</small><br><small>" +
                "При необходимости сохраните этот файл отдельно.</small></p>";
            html = '<div class="summary-list">' + help + '</div>' + html;

            // Записываем само содержимое таблицы, переданное из основного скрипта:
            fh.WriteLine(html);

            // Закрываем теги структуры и корректно закрываем файл на запись:
            fh.WriteLine("</body></html>");
            fh.Close();

            // Запускаем файл в браузере по умолчанию (Catsxp) из его постоянного места:
            shell.Run("\"" + filePatch + "\"");

        } catch (e) {
            // Извещение на случай непредвиденного сбоя ОС с копированием данных в буфер:
            window.clipboardData.setData("Text", html);
            alert("Не удалось автоматически запустить браузер.\nОшибка: " + e.description + "\n\nHTML-код таблицы скопирован в буфер обмена.");
        }
    }

    // ================= Вспомогательные функции =================

    // Проверяет наличие ссылок с путями файлов:
    function checkForPathIssues(container) {
        var links = container.getElementsByTagName("a");
        for (var i = 0; i < links.length; i++) {
            var href = links[i].href || "";
            // Ищем признаки пути к файлу (file:/// или буква диска с ://)
            if (href.indexOf("file:///") != -1 || href.match(/[a-zA-Z]:\\/)) {
                return true;
            }
        }
        return false;
    }

    // Исправляет пути в ссылках:
    function fixPathIssues(container) {
        var links = container.getElementsByTagName("a");
        for (var i = 0; i < links.length; i++) {
            var link = links[i];
            var href = link.href || "";

            // Извлекаем якорь из полного пути:
            var anchorMatch = href.match(/#([^#]+)$/);
            if (anchorMatch) {
                // Меняем атрибут href напрямую:
                link.setAttribute("href", "#" + anchorMatch[1]);
            }
        }
    }

    // Очищает href от пути к файлу:
    function getCleanHref(href) {
        if (!href) return "";

        // Если href содержит путь к файлу, извлекаем только якорь:
        if (href.indexOf("file:///") != -1 || href.match(/[a-zA-Z]:\\/)) {
            var anchorMatch = href.match(/#([^#]+)$/);
            if (anchorMatch) {
                return "#" + anchorMatch[1];
            }
        }
        return href;
    }

    function processLink(link, type, href, typeArray, mixedArray) {
        // Номер (текст ссылки):
        var number = "";
        // Для комментариев нужно искать внутри <sup>:
        if (type == "comment") {
            var sup = link.getElementsByTagName("sup")[0];
            if (sup) {
                number = sup.innerText || sup.innerHTML || "";
            } else {
                number = link.innerText || link.innerHTML || "";
            }
        } else {
            number = link.innerText || link.innerHTML || "";
        }

        // Получаем текст перед ссылкой (не более одного параграфа):
        var beforeText = getPrecedingTextInParagraph(link);
        if (beforeText.length > fragmentLength) {
            beforeText = beforeText.substring(beforeText.length - fragmentLength);
        }

        // Извлекаем ID:
        var id = href.substring(href.indexOf("#") + 1);
        var contentDiv = document.getElementById(id);
        var contentText = "";

        if (contentDiv) {
            // contentText = extractContentText(contentDiv);
            if (colParagraphsTextNoteComm == 2) {
                contentText = extractContentTextWithParagraphs(contentDiv);
            } // все абзацы
            if (colParagraphsTextNoteComm == 1) {
                contentText = extractFirstParagraphText(contentDiv);
            } // изменено: только первый абзац
            if (contentText.length > noteLength) {
                contentText = contentText.substring(0, noteLength);
            }
        } else {
            contentText = "[Не найден элемент с id=" + id + "]";
        }

        // Создаем объект с данными:
        var item = {
            type: type,
            beforeText: beforeText,
            number: number,
            contentText: contentText
        };

        // Добавляем в соответствующие массивы:
        typeArray.push(item);

        // Для смешанного режима сохраняем порядок:
        if (displayMode == "mixed") {
            mixedArray.push(item);
        }
    }

    function getPrecedingTextInParagraph(node) {
        // Находим родительский параграф
        var p = node;
        while (p && p.tagName != "P") {
            p = p.parentNode;
        }
        if (!p) return "";

        // Получаем полный текст параграфа (включая все вложенные элементы)
        var fullText = p.innerText || p.textContent || "";

        // Получаем текст ссылки (номер примечания/комментария)
        var linkText = "";
        if (node.tagName == "A") {
            linkText = node.innerText || node.textContent || "";
        }
        // Если ссылка пустая (например, внутри неё только <SUP>), ищем в <SUP>
        if (linkText == "") {
            var sup = node.getElementsByTagName("sup")[0];
            if (sup) {
                linkText = sup.innerText || sup.textContent || "";
            }
        }

        // Ищем позицию текста ссылки в полном тексте
        var pos = fullText.indexOf(linkText);
        if (pos != -1) {
            return fullText.substring(0, pos);
        }
        return "";
    }

    // Функция извлекает текст всех абзацев, сохраняя между ними разделитель "\n\n"
    function extractContentTextWithParagraphs(container) {
        var paragraphs = container.getElementsByTagName("p");
        var texts = [];
        for (var i = 0; i < paragraphs.length; i++) {
            var p = paragraphs[i];
            var parent = p.parentNode;
            var insideTitle = false;
            while (parent && parent != container) {
                if (parent.className && parent.className.indexOf("title") != -1) {
                    insideTitle = true;
                    break;
                }
                parent = parent.parentNode;
            }
            if (!insideTitle) {
                var pText = p.innerText || p.textContent || "";
                if (pText.length > 0) {
                    texts.push(pText);
                }
            }
        }
        // Объединяем абзацы с двойным переводом строки для сохранения структуры
        return texts.join("\n\n");
    }

    // function extractContentText(container) {
    function extractFirstParagraphText(container) {
        var paragraphs = container.getElementsByTagName("p");
        // var texts = [];
        for (var i = 0; i < paragraphs.length; i++) {
            var p = paragraphs[i];
            // Проверяем, не находится ли абзац внутри заголовка (class="title");
            var parent = p.parentNode;
            var insideTitle = false;
            while (parent && parent != container) {
                if (parent.className && parent.className.indexOf("title") != -1) {
                    insideTitle = true;
                    break;
                }
                parent = parent.parentNode;
            }
            if (!insideTitle) {
                var pText = p.innerText || p.textContent || "";
                if (pText.length > 0) {
                    // texts.push(pText);
                    return pText; // возвращаем первый же значимый абзац
                }
            }
        }
        return "";
    }

    function generateTable(items, type) {
        var table = "<table>";
        table += "<tr bgcolor='" + titleColor + "'><th>Фрагмент текста</th><th>№</th><th>Текст " + (type == "note" ? "примечания" : "комментария") + "</th></tr>";

        for (var i = 0; i < items.length; i++) {
            var item = items[i];
            var rowClass = (type == "note") ? "note-row" : "comment-row";
            table += "<tr class='" + rowClass + "'>";

            if (rowClass == "note-row") {
                table += "<td align=\"right\"; bgcolor='" + noteColor + "'>" + escapeHTML(item.beforeText) + "</td>";
                table += "<td align=\"center\" bgcolor='" + titleColor + "'>" + escapeHTML(item.number) + "</td>";

                // Текст примечания/комментария
                var displayText = escapeHTML(item.contentText).replace(/\n/g, "<br>");
                table += "<td bgcolor='" + noteColor + "'>" + displayText + "</td>";
            }

            if (rowClass == "comment-row") {
                table += "<td align=\"right\"; bgcolor='" + commentColor + "'>" + escapeHTML(item.beforeText) + "</td>";
                table += "<td align=\"center\" bgcolor='" + titleColor + "'>" + escapeHTML(item.number) + "</td>";

                var displayText = escapeHTML(item.contentText).replace(/\n/g, "<br>");
                table += "<td bgcolor='" + commentColor + "'>" + displayText + "</td>";
            }
            table += "</tr>";
        }

        table += "</table>";

        return table;

    }

    function generateMixedTable(items) {
        var table = "<table>";
        table += "<tr bgcolor='" + titleColor + "'><th>Тип</th><th>Фрагмент текста</th><th>№</th><th>Текст</th></tr>";

        for (var i = 0; i < items.length; i++) {
            var item = items[i];
            var rowClass = (item.type == "note") ? "note-row" : "comment-row";
            var typeText = (item.type == "note") ? "Прим." : "Комм.";

            table += "<tr class='" + rowClass + "'>";

            if (rowClass == "note-row") {
                table += "<td bgcolor='" + titleColor + "'>" + typeText + "</td>";
                table += "<td align=\"right\"; bgcolor='" + noteColor + "'>" + escapeHTML(item.beforeText) + "</td>";
                table += "<td align=\"center\" bgcolor='" + titleColor + "'>" + escapeHTML(item.number) + "</td>";

                // Текст
                var displayText = escapeHTML(item.contentText).replace(/\n/g, "<br>");
                table += "<td bgcolor='" + noteColor + "'>" + displayText + "</td>";

            }

            if (rowClass == "comment-row") {
                table += "<td bgcolor='" + titleColor + "'>" + typeText + "</td>";
                table += "<td align=\"right\"; bgcolor='" + commentColor + "'>" + escapeHTML(item.beforeText) + "</td>";
                table += "<td align=\"center\" bgcolor='" + titleColor + "'>" + escapeHTML(item.number) + "</td>";

                var displayText = escapeHTML(item.contentText).replace(/\n/g, "<br>");
                table += "<td bgcolor='" + commentColor + "'>" + displayText + "</td>";

            }
            table += "</tr>";
        }

        table += "</table>";
        return table;
    }

    function escapeHTML(str) {
        if (!str) return "";
        return str.replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/"/g, "&quot;")
            .replace(/'/g, "&#39;");
    }
}
