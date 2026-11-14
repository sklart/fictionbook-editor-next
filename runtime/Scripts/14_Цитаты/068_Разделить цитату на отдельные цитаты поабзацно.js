// Скрипт "Разделить цитату на отдельные цитаты поабзацно" для редактора FBE
// version 1.4
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для разделения цитаты под курсором, содержащей несколько абзацев,
// на несколько отдельных цитат, по одному абзацу в каждой цитате, в fb2 документах.
// Отдельная настройка для разделения цитат, в которых есть тэг автор текста (text-author):
// - Авторы текста всегда остаются в самой нижней, последней цитате с сохранением тэга text-author.
// - Авторы текста выделяются в отдельные цитаты с сохранением тэга text-author.
// - Можно вообще не разделять цитаты, где есть тэг text-author.
// Цитаты, содержащие стихи, (тэг poem) не разделяются.
// Цитаты внутри эпиграфов (тэг epigraph) не разделяются.
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.4, 11.03.2026
//======================================

function Run() {

    // Название и версия для сообщений
    var scriptName = "Разделить цитату на отдельные цитаты поабзацно";
    var version = "1.4";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    // Настройка режима отображения:
    // 1 - показывать окна сообщений (обычный режим)
    // 0 - тихий режим, НО сообщения об ошибках показываем ВСЕГДА!
    var showStatistics = 0; // По умолчанию 0 (тихий). Измените на 1 для подробностей.

    // Обработка цитат с автором текста (класс "text-author")
    // 0 - НЕТ, не разделять такие цитаты. Если в цитате найден text-author,
    //     скрипт остановится и сообщит, что разделение таких цитат отключено в настройках.
    // 1 - ДА, разделять, прикрепляя ВСЕХ авторов к ПОСЛЕДНЕЙ созданной цитате.
    // 2 - ДА, разделять, выделяя КАЖДОГО автора в ОТДЕЛЬНУЮ цитату (P class=text-author внутри DIV class=cite).
    var processTextAuthor = 1; // 0, 1 или 2

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    // Функция для формирования единого формата сообщений
    function formatMessage(mainText) {
        return scriptName + "\nver. " + version + "\n\n" + mainText;
    }

    // Функция для безопасного вывода сообщений с учетом режима
    function showMessage(msg, critical) {
        if (critical === undefined) critical = true;
        if (critical || showStatistics == 1) {
            MsgBox(formatMessage(msg));
        }
    }

    // Функция для проверки, находится ли элемент внутри эпиграфа
    function isInsideEpigraph(element) {
        while (element) {
            if (element.nodeName == "DIV" && element.className == "epigraph") {
                return true;
            }
            element = element.parentNode;
        }
        return false;
    }

    // Функция для проверки, содержит ли элемент стихи (poem)
    function containsPoem(element) {
        var poems = element.getElementsByTagName("DIV");
        for (var i = 0; i < poems.length; i++) {
            if (poems[i].className == "poem") {
                return true;
            }
        }
        return false;
    }

    // Функция для проверки, есть ли в цитате авторы текста
    function containsTextAuthor(citeElement) {
        var paragraphs = citeElement.getElementsByTagName("P");
        for (var i = 0; i < paragraphs.length; i++) {
            if (paragraphs[i].className == "text-author") {
                return true;
            }
        }
        return false;
    }

    // Функция для проверки, является ли элемент "пустым"
    function isEmptyElement(node) {
        if (node.nodeType != 1) return true;
        if (!node.innerHTML) return true;
        var text = node.innerText || node.textContent || "";
        var cleanText = text.replace(new RegExp("[\\s" + nbspChar + "]", "g"), "");
        return cleanText.length === 0;
    }

    // === Получаем неразрывный пробел из настроек FBE ===
    var nbspEntity = "&nbsp;";
    var nbspChar = String.fromCharCode(160);
    try {
        var tmpChar = window.external.GetNBSP();
        if (tmpChar.charCodeAt(0) == 160) {
            nbspEntity = "&nbsp;";
            nbspChar = tmpChar;
        } else {
            nbspEntity = tmpChar;
            nbspChar = tmpChar;
        }
    } catch (e) {
        nbspChar = String.fromCharCode(160);
        nbspEntity = "&nbsp;";
    }

    // 1. Получаем текущее выделение и находим родительскую цитату (DIV.cite)
    var sel = document.selection.createRange();
    if (!sel) {
        showMessage("Не удалось определить позицию курсора.", true);
        return;
    }

    var parentElement = sel.parentElement();
    if (!parentElement) {
        showMessage("Не удалось определить элемент под курсором.", true);
        return;
    }

    var citeDiv = parentElement;
    while (citeDiv && !(citeDiv.nodeName == "DIV" && citeDiv.className == "cite")) {
        citeDiv = citeDiv.parentNode;
    }

    if (!citeDiv) {
        showMessage("Курсор не внутри цитаты (DIV class=cite).", true);
        return;
    }

    // 2. Проверка на исключения
    if (isInsideEpigraph(citeDiv)) {
        showMessage("Цитата находится внутри эпиграфа (тег epigraph). Разделение невозможно.", true);
        return;
    }

    if (containsPoem(citeDiv)) {
        showMessage("Цитата содержит стихи (тег poem). Разделение невозможно.", true);
        return;
    }

    // Проверка на наличие авторов, если обработка авторов отключена
    var hasAuthor = containsTextAuthor(citeDiv);
    if (hasAuthor && processTextAuthor == 0) {
        showMessage("Цитата содержит теги автора текста (P class=text-author).\nРазделение таких цитат отключено в настройках скрипта.", true);
        return;
    }

    // 3. Сбор данных (Фаза чтения) с учетом режима processTextAuthor
    var children = []; // Массив групп, каждая группа станет отдельной цитатой
    var currentGroup = [];
    
    if (processTextAuthor == 2) {
        // Режим 2: Каждый элемент - отдельная группа (авторы тоже отдельно)
        for (var i = 0; i < citeDiv.childNodes.length; i++) {
            var node = citeDiv.childNodes[i];
            if (node.nodeType != 1) continue;
            if (isEmptyElement(node)) continue;
            
            // Каждый непустой элемент становится отдельной группой
            children.push([node]);
        }
    } else {
        // Режим 1 (или 0, но мы уже отсекли 0 выше): авторы прикрепляются к последнему не-автору
        var lastNonAuthorIndex = -1;
        
        for (var i = 0; i < citeDiv.childNodes.length; i++) {
            var node = citeDiv.childNodes[i];
            if (node.nodeType != 1) continue;
            if (isEmptyElement(node)) continue;

            var isAuthor = (processTextAuthor == 1 && node.nodeName == "P" && node.className == "text-author");
            
            if (isAuthor) {
                if (currentGroup.length > 0) {
                    currentGroup.push(node);
                } else {
                    currentGroup = [node];
                    lastNonAuthorIndex = -1;
                }
            } else {
                if (currentGroup.length > 0 && lastNonAuthorIndex != -1) {
                    children.push(currentGroup);
                    currentGroup = [];
                    lastNonAuthorIndex = -1;
                }
                currentGroup = [node];
                lastNonAuthorIndex = 0;
            }
        }
        
        if (currentGroup.length > 0) {
            children.push(currentGroup);
        }
    }

    // 4. Проверка на возможность разделения
    if (children.length <= 1) {
        showMessage("В цитате всего один значимый абзац (или группа с автором).\nРазделить цитату невозможно.", true);
        return;
    }

    // 5. Изменение документа
    window.external.BeginUndoUnit(document, scriptName + " v" + version);

    var parentOfCite = citeDiv.parentNode;
    var newCites = [];

    for (var j = children.length - 1; j >= 0; j--) {
        var group = children[j];
        if (group.length == 0) continue;

        var newCite = document.createElement("DIV");
        newCite.className = "cite";

        for (var k = 0; k < group.length; k++) {
            var nodeToMove = group[k];
            nodeToMove.removeNode(true);
            newCite.appendChild(nodeToMove);
        }
        
        newCites.unshift(newCite);
    }

    for (var j = 0; j < newCites.length; j++) {
        parentOfCite.insertBefore(newCites[j], citeDiv);
    }

    citeDiv.removeNode(true);
    window.external.EndUndoUnit(document);

    // 6. Статистика
    if (showStatistics == 1) {
        var message = "✓ Цитата успешно разделена.\n";
        message += "  • Создано отдельных цитат: " + newCites.length + "\n";
        
        if (hasAuthor) {
            if (processTextAuthor == 1) {
                message += "  • Авторы текста прикреплены к последней цитате.\n";
            } else if (processTextAuthor == 2) {
                message += "  • Авторы текста выделены в отдельные цитаты.\n";
            }
        }
        
        showMessage(message, false);
    }
}
