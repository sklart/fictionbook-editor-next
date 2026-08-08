// Скрипт "HTML код fb2 в буфер (расширенная версия)" для редактора FBE
// version 1.7
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для копирвания HTML кода fb2 документов в буфер обмена.
// Расширенные настройки позволяют задать копирование отдельных областей кода.
// Например - только основной раздел, без разделов аннотации и сносок,
// или все, кроме дескрипшена и служебных полей.
// Или можно копировать отдельно код дескрипшена или код аннотации.
// Скрипт выводит статистику скопированного и показывает текущие настройки.
// По умолчанию отключено копирование дескрипшена и служебных HTML полей (шапка и концовка).
// Скрипт не вносит никаких изменений в fb2 документ.
// Режим работы: обычный или тихий.


// version 1.7, 21.05.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "HTML код fb2 в буфер (расширенная версия)";
    var version = "1.7";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима


    // При включении флагов всех настроек = 1, копируется ПОЛНЫЙ HTML код документа

    
    // Копировать основной раздел
    var processMainSection = 1; // 0 - нет, 1 - да
    
    // Копировать раздел сносок (примечаний)
    var processNotesSection = 1; // 0 - нет, 1 - да
    
    // Копировать раздел комментариев
    var processCommentsSection = 1; // 0 - нет, 1 - да
    
    // Копировать раздел description (служебная информация)
    var processDescriptionSection = 0; // 0 - нет, 1 - да
    
    // Копировать раздел аннотации (annotation)
    var processAnnotationSection = 1; // 0 - нет, 1 - да
    
    // Копировать раздел истории документа (history)
    var processHistorySection = 1; // 0 - нет, 1 - да
    
    // Копировать служебные поля (Шапка HTML и Концовка HTML)
    var processServiceFields = 0; // 0 - нет, 1 - да

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    var startTime = new Date();
    
    // Получаем весь HTML документа
    var fullHTML = document.documentElement.outerHTML;
    
    // Строка для сбора результата
    var resultHTML = "";
    
    // Флаг: был ли открыт fbw_body
    var fbwBodyOpened = false;
    
    // Счётчики для статистики
    var totalParagraphs = 0;
    var totalChars = 0;
    var sectionsCopied = [];
    var sectionsNotFound = [];
    
    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================
    
    // Подсчёт абзацев и символов в HTML-строке
    function countParagraphsAndChars(htmlStr) {
        var pCount = 0;
        var chCount = 0;
        
        var reP = new RegExp("<P[\\s>]", "gi");
        var matches = htmlStr.match(reP);
        if (matches) {
            pCount = matches.length;
        }
        
        chCount = htmlStr.length;
        
        return { paragraphs: pCount, chars: chCount };
    }
    
    // Извлечь HTML элемент по id
    function getElementHTMLById(id) {
        var el = document.getElementById(id);
        if (el) {
            return el.outerHTML;
        }
        return "";
    }
    
    // Получить открывающий тег элемента по id (до первого >)
    function getOpenTagById(id) {
        var el = document.getElementById(id);
        if (!el) return "";
        var html = el.outerHTML;
        var closePos = html.indexOf(">");
        if (closePos != -1) {
            return html.substring(0, closePos + 1);
        }
        return "";
    }
    
    // Найти дочерний DIV внутри fbw_body по классу и fbname
    function findChildDivHTML(parentId, className, fbname) {
        var parent = document.getElementById(parentId);
        if (!parent) return "";
        
        var children = parent.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == className) {
                var childFbname = child.getAttribute("fbname") || "";
                
                if (fbname == "") {
                    if (childFbname == "" || childFbname == null) {
                        return child.outerHTML;
                    }
                } else {
                    if (childFbname == fbname) {
                        return child.outerHTML;
                    }
                }
            }
        }
        return "";
    }
    
    // Вспомогательная функция: ДА/НЕТ для настроек
    function yesNo(value) {
        if (value == 1) return "\u221A (ДА)";
        return "\u2717 (НЕТ)";
    }
    
    // ==================================================
    // СБОРКА РЕЗУЛЬТИРУЮЩЕГО HTML
    // ==================================================
    
    // Проверяем, нужно ли открывать fbw_body
    var needFbwBody = (processAnnotationSection || processHistorySection || 
                       processMainSection || processNotesSection || processCommentsSection);
    
    // 1. Служебные поля — шапка: от <HTML до начала fbw_desc (НЕ включая его)
    if (processServiceFields) {
        var descStartPos = fullHTML.indexOf('<DIV id=fbw_desc');
        if (descStartPos != -1) {
            var headerPart = fullHTML.substring(0, descStartPos);
            resultHTML += headerPart;
            sectionsCopied.push("Служебные поля (шапка HTML)");
        } else {
            // Если fbw_desc не найден, берём до fbw_body
            var bodyStartPos2 = fullHTML.indexOf('<DIV id=fbw_body');
            if (bodyStartPos2 != -1) {
                var headerPart2 = fullHTML.substring(0, bodyStartPos2);
                resultHTML += headerPart2;
                sectionsCopied.push("Служебные поля (шапка HTML)");
            } else {
                sectionsNotFound.push("Служебные поля (шапка HTML)");
            }
        }
    }
    
    // 2. Description (fbw_desc)
    if (processDescriptionSection) {
        var descHTML = getElementHTMLById("fbw_desc");
        if (descHTML != "") {
            resultHTML += descHTML;
            var stats = countParagraphsAndChars(descHTML);
            totalParagraphs += stats.paragraphs;
            totalChars += stats.chars;
            sectionsCopied.push("Раздел Description");
        } else {
            sectionsNotFound.push("Раздел Description");
        }
    }
    
    // 3. Открывающий тег fbw_body (если нужно содержимое)
    if (needFbwBody) {
        var openTag = getOpenTagById("fbw_body");
        if (openTag != "") {
            resultHTML += openTag;
            fbwBodyOpened = true;
        }
    }
    
    // 4. Annotation (дочерний DIV внутри fbw_body)
    if (processAnnotationSection) {
        var annHTML = findChildDivHTML("fbw_body", "annotation", "");
        if (annHTML != "") {
            resultHTML += annHTML;
            var stats2 = countParagraphsAndChars(annHTML);
            totalParagraphs += stats2.paragraphs;
            totalChars += stats2.chars;
            sectionsCopied.push("Раздел аннотации");
        } else {
            sectionsNotFound.push("Раздел аннотации");
        }
    }
    
    // 5. History (дочерний DIV внутри fbw_body)
    if (processHistorySection) {
        var histHTML = findChildDivHTML("fbw_body", "history", "");
        if (histHTML != "") {
            resultHTML += histHTML;
            var stats3 = countParagraphsAndChars(histHTML);
            totalParagraphs += stats3.paragraphs;
            totalChars += stats3.chars;
            sectionsCopied.push("Раздел History");
        } else {
            sectionsNotFound.push("Раздел History");
        }
    }
    
    // 6. Основной раздел (body с fbname="")
    if (processMainSection) {
        var mainHTML = findChildDivHTML("fbw_body", "body", "");
        if (mainHTML != "") {
            resultHTML += mainHTML;
            var stats4 = countParagraphsAndChars(mainHTML);
            totalParagraphs += stats4.paragraphs;
            totalChars += stats4.chars;
            sectionsCopied.push("Основной раздел");
        } else {
            sectionsNotFound.push("Основной раздел");
        }
    }
    
    // 7. Сноски (body с fbname="notes")
    if (processNotesSection) {
        var notesHTML = findChildDivHTML("fbw_body", "body", "notes");
        if (notesHTML != "") {
            resultHTML += notesHTML;
            var stats5 = countParagraphsAndChars(notesHTML);
            totalParagraphs += stats5.paragraphs;
            totalChars += stats5.chars;
            sectionsCopied.push("Раздел сносок (примечаний)");
        } else {
            sectionsNotFound.push("Раздел сносок (примечаний)");
        }
    }
    
    // 8. Комментарии (body с fbname="comments")
    if (processCommentsSection) {
        var commHTML = findChildDivHTML("fbw_body", "body", "comments");
        if (commHTML != "") {
            resultHTML += commHTML;
            var stats6 = countParagraphsAndChars(commHTML);
            totalParagraphs += stats6.paragraphs;
            totalChars += stats6.chars;
            sectionsCopied.push("Раздел комментариев");
        } else {
            sectionsNotFound.push("Раздел комментариев");
        }
    }
    
    // 9. Закрывающий тег fbw_body (если был открыт)
    if (fbwBodyOpened) {
        resultHTML += "</DIV>";
    }
    
    // 10. Концовка (от fbw_updater до конца)
    if (processServiceFields) {
        var updaterPos = fullHTML.indexOf('<DIV id=fbw_updater');
        if (updaterPos != -1) {
            var footerPart = fullHTML.substring(updaterPos);
            resultHTML += footerPart;
            sectionsCopied.push("Служебные поля (концовка HTML)");
        } else {
            sectionsNotFound.push("Служебные поля (концовка HTML)");
        }
    }
    
    // ==================================================
    // ПОСТ-ОБРАБОТКА: добавляем переносы строк перед тегами
    // ==================================================
    
    // Добавляем перенос строки перед каждым <DIV
    var reDiv = new RegExp("<DIV", "gi");
    resultHTML = resultHTML.replace(reDiv, "\n<DIV");
    
    // Добавляем перенос строки перед каждым <P
    var reP = new RegExp("<P[\\s>]", "gi");
    resultHTML = resultHTML.replace(reP, "\n$&");
    
    // Убираем самый первый перенос строки, если он есть
    if (resultHTML.charAt(0) == "\n") {
        resultHTML = resultHTML.substring(1);
    }
    
    // Убираем возможные двойные переносы
    var reDoubleNl = new RegExp("\n\n", "g");
    while (resultHTML.indexOf("\n\n") != -1) {
        resultHTML = resultHTML.replace(reDoubleNl, "\n");
    }
    
    // ==================================================
    // КОПИРОВАНИЕ В БУФЕР
    // ==================================================
    
    if (resultHTML != "") {
        window.clipboardData.setData("text", resultHTML);
    }
    
    // ==================================================
    // СТАТИСТИКА
    // ==================================================
    
    var endTime = new Date();
    var elapsed = (endTime - startTime) / 1000;
    var elapsedStr = elapsed.toFixed(3);
    var elapsedFormatted = elapsedStr.replace(".", ",");
    
    if (showStatistics == 1) {
        var msg = scriptName + "\n";
        msg += "ver. " + version + "\n";
        msg += "---------------------------------------\n\n";
        
        if (resultHTML != "") {
            msg += "\u221A HTML код скопирован в буфер обмена\n";
        } else {
            msg += "\u2717 Ничего не скопировано!\n";
        }
        
        msg += "---------------------------------------\n";
        msg += "Настройки копирования:\n";
        msg += "  \u2022 Служебные HTML поля: " + yesNo(processServiceFields) + "\n";
        msg += "  \u2022 Раздел Description: " + yesNo(processDescriptionSection) + "\n";
        msg += "  \u2022 Раздел аннотации: " + yesNo(processAnnotationSection) + "\n";
        msg += "  \u2022 Раздел History: " + yesNo(processHistorySection) + "\n";
        msg += "  \u2022 Основной раздел: " + yesNo(processMainSection) + "\n";
        msg += "  \u2022 Раздел сносок (примечаний): " + yesNo(processNotesSection) + "\n";
        msg += "  \u2022 Раздел комментариев: " + yesNo(processCommentsSection) + "\n";
        msg += "---------------------------------------\n\n";
        
        if (sectionsCopied.length > 0) {
            msg += "Скопированные разделы:\n";
            for (var i = 0; i < sectionsCopied.length; i++) {
                msg += "  \u2022 " + sectionsCopied[i] + "\n";
            }
        }
        
        if (sectionsNotFound.length > 0) {
            msg += "\nНе найдены разделы:\n";
            for (var j = 0; j < sectionsNotFound.length; j++) {
                msg += "  \u2022 " + sectionsNotFound[j] + "\n";
            }
        }
        
        msg += "\n\u221A Всего скопировано абзацев: " + totalParagraphs + "\n";
        msg += "\u221A Всего скопировано символов: " + totalChars + "\n";
        msg += "\n---------------------------------------\n";
        msg += "Время выполнения: " + elapsedFormatted + " сек.";
        
        MsgBox(msg);
    } else {
        if (resultHTML == "") {
            MsgBox(scriptName + "\nver. " + version + "\n---------------------------------------\n\n\u2717 Ошибка: ничего не скопировано! Проверьте настройки скрипта.");
        }
    }
}
