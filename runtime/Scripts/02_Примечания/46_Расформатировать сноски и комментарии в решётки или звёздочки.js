// Скрипт "Расформатировать сноски и комментарии в решётки или звёздочки" для редактора FBE
// version 2.0
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для расформатирования имеющихся сносок (примечаний) и комментариев в fb2 файлах.
// Расформатирование сносок (примечаний) и комментариев настраивается по отдельности.
// Маркеры знаков сносок МЗС (ссылки) а также маркеры знаков комментариев (МЗК) 
// заменяются на знаки звёздочек или решёток (или на альтернативные символы).
// Разделы сносок и комментариев удаляются, а тексты из них маркируются в началах абзацев
// знаками звёздочек или решёток (или альтернативными символами).
// Знаки для разделов сносок и комментариев всегда используются НЕ ОДИНАКОВЫЕ.
// В многоабзацных сносках и комментариях второй и все последующие абзацы маркируются в начале абзаца
// знаком, заданным в настройке NotesCommentsSecondSign (по умолчанию — две тильды ~~).
// Скрипт предварительно проверяет, если в исходном документе уже есть звёздочки или решётки,
// то используются альтернативные символы.
// По умолчанию, подзаголовки из звёздочек или решёток в расчет не берутся,
// но их наличие показывается в статистике.
// Все символы для замены и вставки пользователь может задать самостоятельно.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 2.0, 06.06.2026
//======================================

function Run() {
    var scriptName = "Расформатировать сноски и комментарии в решётки или звёздочки";
    var version = "2.0";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1;

    // Обрабатывать раздел сносок (примечаний)
    var processNotes = 1; // 0 - нет, 1 - да

    // Обрабатывать раздел комментариев
    var processComments = 1; // 0 - нет, 1 - да

    // Знак-маркер №1 (приоритетный)
    var sign1 = "*"; // при необходимости укажите любой свой подходящий знак

    // Знак-маркер №2
    var sign2 = "#"; // при необходимости укажите любой свой подходящий знак

    // Альтернативный знак №1
    var altSign1 = "♠"; // при необходимости укажите любой свой подходящий знак

    // Альтернативный знак №2
    var altSign2 = "♣"; // при необходимости укажите любой свой подходящий знак
    
    // Маркер для вторых и последующих абзацев сносок или комментариев, ставится в начале таких абзацев
    var NotesCommentsSecondSign = "~~"; // при необходимости укажите любой свой подходящий знак, например "++"

    // Считать "звёздочками" подзаголовки, состоящие только из звёздочек и пробелов
    var countStarSubtitles = 0; // 0 - не считать, 1 - считать

    // Считать "решётками" подзаголовки, состоящие только из решёток и пробелов
    var countHashSubtitles = 0; // 0 - не считать, 1 - считать

    // Принудительный знак-маркер для сносок (если задан — используется вместо автоматического выбора)
    // Оставьте пустым "", чтобы работал автоматический выбор
    var forceNotesSign = "";  // Например: "{zzz}" или "†"

    // Принудительный знак-маркер для комментариев
    // Оставьте пустым "", чтобы работал автоматический выбор
    var forceCommentsSign = "";  // Например: "[К]" или "‡"

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    // Неразрывный пробел из настроек FBE
    var nbspEntity = "&nbsp;";
    try {
        var nbspChar = window.external.GetNBSP();
        if (nbspChar.charCodeAt(0) != 160) {
            nbspEntity = nbspChar;
        }
    } catch (e) {
        var nbspChar = String.fromCharCode(160);
    }

    var fbw_body = document.getElementById("fbw_body");
    if (!fbw_body) {
        MsgBox("Ошибка: не найден fbw_body", scriptName + " v" + version);
        return;
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

    // Если не найдены оба раздела, сообщаем и завершаем работу
    if (!notesFound && !commentsFound) {
        MsgBox("Не найдены раздел сносок (примечаний) и раздел комментариев.", scriptName + " v" + version);
        return;
    }

    // Если какой-то раздел не найден, отключаем его обработку
    if (!notesFound) processNotes = 0;
    if (!commentsFound) processComments = 0;

    // ==================================================
    // Функция для получения локальной части href
    // ==================================================
    function getLocalHref(name) {
        if (!name) return "";
        var hashPos = -1;
        for (var i = 0; i < name.length; i++) {
            if (name.charAt(i) == "#") {
                hashPos = i;
                break;
            }
        }
        if (hashPos == -1) return "";

        var mainHtml = "main.html#";
        var nameLower = name.toLowerCase();
        var searchStr = nameLower;
        var mainIdx = -1;
        for (var i = 0; i <= searchStr.length - mainHtml.length; i++) {
            var match = true;
            for (var j = 0; j < mainHtml.length; j++) {
                if (searchStr.charAt(i + j) != mainHtml.charAt(j)) {
                    match = false;
                    break;
                }
            }
            if (match) {
                mainIdx = i;
                break;
            }
        }
        if (mainIdx == -1) {
            return name.substring(hashPos + 1);
        } else {
            return name.substring(mainIdx + 10);
        }
    }

    // ==================================================
    // Функция проверки: является ли подзаголовок состоящим только из знака и пробелов
    // ==================================================
    function isSubtitleOfOnlySigns(pElement, signChar) {
        if (pElement.className != "subtitle") return false;

        var text = pElement.innerText || pElement.textContent || "";

        var hasSign = false;
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            if (ch == signChar) {
                hasSign = true;
            } else if (ch == " " || ch == "\u00A0" || ch == nbspChar || ch == "\u2002" || ch == "\u2003" || ch == "\u2009") {
                // пробел или неразрывный пробел — ок
            } else {
                return false;
            }
        }
        return hasSign;
    }

    // ==================================================
    // Подсчёт знаков и подзаголовков в body
    // ==================================================
    function countSignsAndSubtitles(bodyElement, signChar) {
        var totalSignCount = 0;
        var subtitleCount = 0;
        var signsInSubtitlesCount = 0;
        var regularSignCount = 0;

        var allPElements = bodyElement.getElementsByTagName("P");
        for (var i = 0; i < allPElements.length; i++) {
            var pEl = allPElements[i];
            if (pEl.className == "subtitle") {
                if (isSubtitleOfOnlySigns(pEl, signChar)) {
                    subtitleCount++;
                    var text = pEl.innerText || pEl.textContent || "";
                    for (var j = 0; j < text.length; j++) {
                        if (text.charAt(j) == signChar) {
                            signsInSubtitlesCount++;
                        }
                    }
                } else {
                    var text = pEl.innerText || pEl.textContent || "";
                    for (var j = 0; j < text.length; j++) {
                        if (text.charAt(j) == signChar) {
                            regularSignCount++;
                            totalSignCount++;
                        }
                    }
                }
            }
        }

        function countSignsInTextNodes(node) {
            if (!node) return;
            if (node.nodeType == 3) {
                var text = node.nodeValue;
                for (var j = 0; j < text.length; j++) {
                    if (text.charAt(j) == signChar) {
                        regularSignCount++;
                        totalSignCount++;
                    }
                }
            } else if (node.nodeType == 1) {
                if (node.nodeName == "P" && node.className == "subtitle" && isSubtitleOfOnlySigns(node, signChar)) {
                    return;
                }
                var child = node.firstChild;
                while (child) {
                    countSignsInTextNodes(child);
                    child = child.nextSibling;
                }
            }
        }
        countSignsInTextNodes(bodyElement);

        totalSignCount = regularSignCount;

        return {
            totalSignCount: totalSignCount,
            subtitleCount: subtitleCount,
            signsInSubtitlesCount: signsInSubtitlesCount,
            regularSignCount: regularSignCount
        };
    }

    // ==================================================
    // Функция проверки наличия знака в тексте
    // ==================================================
    function signExistsInText(text, sign) {
        for (var i = 0; i < text.length; i++) {
            if (text.charAt(i) == sign) {
                return true;
            }
        }
        return false;
    }

    // ==================================================
    // Функция: занят ли знак в ИСХОДНОМ документе (не учитывая уже выбранный для другого раздела)
    // ==================================================
    function isSignOccupiedInDocument(sign) {
        if (sign == "*") return (starCount > 0);
        if (sign == "#") return (hashCount > 0);
        return signExistsInText(mainBody.innerText || mainBody.textContent || "", sign);
    }

    // ==================================================
    // Функция: занят ли знак (с учётом уже выбранного знака для другого раздела)
    // ==================================================
    function isSignOccupied(sign, alreadyUsedSign) {
        if (alreadyUsedSign.length > 0 && sign == alreadyUsedSign) return true;
        return isSignOccupiedInDocument(sign);
    }

    // ==================================================
    // Функция: сообщение о занятом знаке
    // ==================================================
    function getOccupiedSignMessage(signChar) {
        var count = 0;
        var hasSubtitles = false;

        if (signChar == "*") {
            count = starStats.regularSignCount;
            hasSubtitles = (starStats.subtitleCount > 0);
        } else if (signChar == "#") {
            count = hashStats.regularSignCount;
            hasSubtitles = (hashStats.subtitleCount > 0);
        } else {
            count = 0;
            var bodyText = mainBody.innerText || mainBody.textContent || "";
            for (var i = 0; i < bodyText.length; i++) {
                if (bodyText.charAt(i) == signChar) {
                    count++;
                }
            }
            hasSubtitles = false;
        }

        var msg = "Знак " + signChar + " уже используется (" + count + ")";
        if (hasSubtitles) {
            msg += " кроме подзаголовков";
        }
        return msg;
    }

    // ==================================================
    // Определяем знаки
    // ==================================================
    var starStats = { totalSignCount: 0, subtitleCount: 0, signsInSubtitlesCount: 0, regularSignCount: 0 };
    var hashStats = { totalSignCount: 0, subtitleCount: 0, signsInSubtitlesCount: 0, regularSignCount: 0 };

    if (processNotes || processComments) {
        starStats = countSignsAndSubtitles(mainBody, "*");
        hashStats = countSignsAndSubtitles(mainBody, "#");
    }

    var starCount = countStarSubtitles ? starStats.totalSignCount + starStats.signsInSubtitlesCount : starStats.regularSignCount;
    var hashCount = countHashSubtitles ? hashStats.totalSignCount + hashStats.signsInSubtitlesCount : hashStats.regularSignCount;

    // Функция выбора знака с записью занятых знаков ИЗ ДОКУМЕНТА
    function selectSignWithLog(candidates, occupiedList, alreadyUsedSign) {
        for (var i = 0; i < candidates.length; i++) {
            var sign = candidates[i];
            if (!isSignOccupied(sign, alreadyUsedSign)) {
                return sign;
            }
            // Добавляем в список занятых, только если знак был в исходном документе
            if (isSignOccupiedInDocument(sign)) {
                var alreadyInList = false;
                for (var k = 0; k < occupiedList.length; k++) {
                    if (occupiedList[k] == sign) {
                        alreadyInList = true;
                        break;
                    }
                }
                if (!alreadyInList) {
                    occupiedList.push(sign);
                }
            }
        }
        return candidates[candidates.length - 1];
    }

    var useNotesSign = "";
    var useCommentsSign = "";
    var notesOccupiedList = [];
    var commentsOccupiedList = [];

    // Для сносок
    if (processNotes) {
        if (forceNotesSign.length > 0) {
            if (isSignOccupied(forceNotesSign, "")) {
                notesOccupiedList.push(forceNotesSign);
                var warnMsg = getOccupiedSignMessage(forceNotesSign) + ".\n\nВсё равно использовать этот знак для сносок?";
                var answer = AskYesNo(warnMsg, scriptName + " v" + version);
                if (answer) {
                    useNotesSign = forceNotesSign;
                } else {
                    MsgBox("Работа скрипта отменена. Измените настройку forceNotesSign.", scriptName + " v" + version);
                    return;
                }
            } else {
                useNotesSign = forceNotesSign;
            }
        } else {
            var notesCandidates = [sign1, sign2, altSign1, altSign2];
            useNotesSign = selectSignWithLog(notesCandidates, notesOccupiedList, "");
        }
    }

    // Для комментариев — учитываем уже выбранный знак сносок
    if (processComments) {
        var alreadyUsed = (processNotes ? useNotesSign : "");

        if (forceCommentsSign.length > 0) {
            if (isSignOccupied(forceCommentsSign, alreadyUsed)) {
                if (isSignOccupiedInDocument(forceCommentsSign)) {
                    commentsOccupiedList.push(forceCommentsSign);
                }
                var warnMsg2 = getOccupiedSignMessage(forceCommentsSign) + ".\n\nВсё равно использовать этот знак для комментариев?";
                var answer2 = AskYesNo(warnMsg2, scriptName + " v" + version);
                if (answer2) {
                    useCommentsSign = forceCommentsSign;
                } else {
                    MsgBox("Работа скрипта отменена. Измените настройку forceCommentsSign.", scriptName + " v" + version);
                    return;
                }
            } else {
                useCommentsSign = forceCommentsSign;
            }
        } else {
            var commCandidates = [];
            if (alreadyUsed.length > 0 && alreadyUsed == sign1) {
                commCandidates = [sign2, altSign1, altSign2, sign1];
            } else if (alreadyUsed.length > 0 && alreadyUsed == sign2) {
                commCandidates = [sign1, altSign1, altSign2, sign2];
            } else {
                commCandidates = [sign1, sign2, altSign1, altSign2];
            }
            useCommentsSign = selectSignWithLog(commCandidates, commentsOccupiedList, alreadyUsed);
        }
    }

    // ==================================================
    // Регулярки для замены спецсимволов
    // ==================================================
    var ampRegExp = new RegExp("&", "g");
    var ampRegExp_ = "&amp;";
    var ltRegExp = new RegExp("<", "g");
    var ltRegExp_ = "&lt;";
    var gtRegExp = new RegExp(">", "g");
    var gtRegExp_ = "&gt;";

    // ==================================================
    // Функция для получения открывающего тега
    // ==================================================
    function getOpeningTag(myTag) {
        if (myTag.nodeName == "I" || myTag.nodeName == "EM" || myTag.nodeName == "B" ||
            myTag.nodeName == "STRONG" || myTag.nodeName == "SUP" || myTag.nodeName == "SUB" ||
            myTag.nodeName == "STRIKE") return "<" + myTag.nodeName + ">";
        if (myTag.nodeName == "SPAN" && myTag.className == "code") return "<SPAN class=code>";
        if (myTag.nodeName == "A" && myTag.className != "note") return "<A href='" + myTag.href + "'>";
        return "";
    }

    // ==================================================
    // Функция для получения закрывающего тега
    // ==================================================
    function getClosingTag(myTag) {
        if (myTag.nodeName == "I" || myTag.nodeName == "EM" || myTag.nodeName == "B" ||
            myTag.nodeName == "STRONG" || myTag.nodeName == "SUP" || myTag.nodeName == "SUB" ||
            myTag.nodeName == "STRIKE") return "</" + myTag.nodeName + ">";
        if (myTag.nodeName == "SPAN" && myTag.className == "code") return "</SPAN>";
        if (myTag.nodeName == "A" && myTag.className != "note") return "</A>";
        return "";
    }

    // ==================================================
    // Функция сбора текста секции — возвращает массив абзацев
    // ==================================================
    function collectSectionParagraphs(sectionElement) {
        var paragraphs = [];
        var currentPara = "";

        function processNode(node) {
            if (!node) return;
            if (node.nodeType == 3) {
                currentPara += node.nodeValue.replace(ampRegExp, ampRegExp_).replace(ltRegExp, ltRegExp_).replace(gtRegExp, gtRegExp_);
            } else if (node.nodeType == 1) {
                if (node.nodeName == "DIV" && node.className == "title") {
                    return;
                }
                if (node.nodeName == "P") {
                    if (currentPara.length > 0) {
                        paragraphs.push(currentPara);
                        currentPara = "";
                    }
                }

                var openTag = getOpeningTag(node);
                currentPara += openTag;

                var child = node.firstChild;
                while (child) {
                    processNode(child);
                    child = child.nextSibling;
                }

                var closeTag = getClosingTag(node);
                currentPara += closeTag;

                if (node.nodeName == "P") {
                    if (currentPara.length > 0) {
                        paragraphs.push(currentPara);
                        currentPara = "";
                    }
                }
            }
        }

        processNode(sectionElement);

        if (currentPara.length > 0) {
            paragraphs.push(currentPara);
        }

        return paragraphs;
    }

    // ==================================================
    // Функция замены ссылок на знаки и удаления ссылок
    // ==================================================
    function processNoteLinks(bodyElement, fbnameType, sign) {
        var count = 0;
        var linksToRemove = [];

        if (fbnameType == "notes") {
            var allLinks = bodyElement.getElementsByTagName("A");
            for (var i = 0; i < allLinks.length; i++) {
                if (allLinks[i].className == "note") {
                    linksToRemove.push(allLinks[i]);
                }
            }
        } else if (fbnameType == "comments") {
            var commentRegExp = new RegExp("^c_", "");
            var allLinks = bodyElement.getElementsByTagName("A");
            for (var i = 0; i < allLinks.length; i++) {
                var localHref = getLocalHref(allLinks[i].getAttribute("href") || "");
                if (localHref.length > 0) {
                    if (localHref.search(commentRegExp) >= 0) {
                        linksToRemove.push(allLinks[i]);
                    }
                }
            }
        }

        for (var i = linksToRemove.length - 1; i >= 0; i--) {
            var link = linksToRemove[i];
            var textNode = document.createTextNode(sign);
            if (link.parentNode) {
                link.parentNode.replaceChild(textNode, link);
                count++;
            }
        }

        return count;
    }

    // ==================================================
    // Функция переноса секций в конец основного body
    // ==================================================
    function moveSectionsToEnd(sourceBody, targetBody, sign, secondSign, titleText) {
        var sections = [];
        var child = sourceBody.firstChild;
        while (child) {
            if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "section") {
                sections.push(child);
            }
            child = child.nextSibling;
        }

        if (sections.length == 0) return 0;

        var subtitleP = document.createElement("P");
        subtitleP.className = "subtitle";
        subtitleP.innerHTML = titleText + " (" + sections.length + ")";

        var notesSection = document.createElement("DIV");
        notesSection.className = "section";
        notesSection.appendChild(subtitleP);

        for (var i = 0; i < sections.length; i++) {
            var section = sections[i];
            var paragraphs = collectSectionParagraphs(section);

            for (var p = 0; p < paragraphs.length; p++) {
                var newP = document.createElement("P");
                var paraText = paragraphs[p];

                if (p == 0) {
                    newP.innerHTML = sign + paraText;
                } else {
                    newP.innerHTML = secondSign + paraText;
                }

                notesSection.appendChild(newP);
            }
        }

        targetBody.appendChild(notesSection);

        return sections.length;
    }

    // ==================================================
    // ОСНОВНАЯ ОБРАБОТКА
    // ==================================================

    // Таймер включаем после всех проверок и подтверждений, перед изменениями
    var startTime = new Date();

    window.external.BeginUndoUnit(document, scriptName);

    var notesCount = 0;
    var commentsCount = 0;
    var notesLinksCount = 0;
    var commentsLinksCount = 0;

    if (processNotes && notesBody) {
        notesLinksCount = processNoteLinks(mainBody, "notes", useNotesSign);
        notesCount = moveSectionsToEnd(notesBody, mainBody, useNotesSign, NotesCommentsSecondSign, "Расформатированные сноски-примечания");
        notesBody.removeNode(true);
    }

    if (processComments && commentsBody) {
        commentsLinksCount = processNoteLinks(mainBody, "comments", useCommentsSign);
        commentsCount = moveSectionsToEnd(commentsBody, mainBody, useCommentsSign, NotesCommentsSecondSign, "Расформатированные комментарии");
        commentsBody.removeNode(true);
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
    if (showStatistics == 1) {
        var msg = "";
        msg += "Расформатировать сноски и комментарии в решётки или звёздочки\n";
        msg += "ver. " + version + "\n\n";

        // ========== ВЕРХНЯЯ ЧАСТЬ: что выполнено ==========

        if (processNotes) {
            msg += "\u2713 Сноски-примечания:\n";
            msg += "  \u2022 Знак-маркер: " + useNotesSign + "\n";
            msg += "  \n";
            msg += "  \u2022 Ссылок заменено: " + notesLinksCount + "\n";
            msg += "  \u2022 Текстов сносок перенесено: " + notesCount + "\n";

            if (!commentsFound) {
                msg += "  \n";
                msg += "  Раздел комментариев не найден.\n";
            }
        } else if (!notesFound && commentsFound) {
            msg += "\u2713 Комментарии:\n";
            msg += "  \u2022 Знак-маркер: " + useCommentsSign + "\n";
            msg += "  \n";
            msg += "  \u2022 Ссылок заменено: " + commentsLinksCount + "\n";
            msg += "  \u2022 Текстов комментариев перенесено: " + commentsCount + "\n";
            msg += "  \n";
            msg += "  Раздел сносок (примечаний) не найден.\n";
        }

        if (processNotes && processComments) {
            msg += "\n";
            msg += "\u2713 Комментарии:\n";
            msg += "  \u2022 Знак-маркер: " + useCommentsSign + "\n";
            msg += "  \n";
            msg += "  \u2022 Ссылок заменено: " + commentsLinksCount + "\n";
            msg += "  \u2022 Текстов комментариев перенесено: " + commentsCount + "\n";
        }

        // Собираем нижнюю часть (пояснения) — только знаки из исходного документа
        var bottomMsg = "";
        var allOccupied = [];

        for (var n = 0; n < notesOccupiedList.length; n++) {
            var sign = notesOccupiedList[n];
            var dup = false;
            for (var d = 0; d < allOccupied.length; d++) {
                if (allOccupied[d] == sign) { dup = true; break; }
            }
            if (!dup) allOccupied.push(sign);
        }
        for (var c = 0; c < commentsOccupiedList.length; c++) {
            var sign = commentsOccupiedList[c];
            var dup = false;
            for (var d = 0; d < allOccupied.length; d++) {
                if (allOccupied[d] == sign) { dup = true; break; }
            }
            if (!dup) allOccupied.push(sign);
        }

        for (var a = 0; a < allOccupied.length; a++) {
            bottomMsg += "  " + getOccupiedSignMessage(allOccupied[a]) + "\n";
        }

        // Подзаголовки из звёздочек
        if (starStats.subtitleCount > 0 && !countStarSubtitles) {
            if (bottomMsg.length > 0) bottomMsg += "\n";
            bottomMsg += "  Подзаголовков из звёздочек: " + starStats.subtitleCount + " (не учтены в подсчёте)\n";
            if (starStats.signsInSubtitlesCount > 0) {
                bottomMsg += "  Звёздочек в подзаголовках из звёздочек: " + starStats.signsInSubtitlesCount + " (не учтены в подсчёте)\n";
            }
        }

        // Подзаголовки из решёток
        if (hashStats.subtitleCount > 0 && !countHashSubtitles) {
            if (bottomMsg.length > 0 && starStats.subtitleCount == 0) bottomMsg += "\n";
            bottomMsg += "  Подзаголовков из решёток: " + hashStats.subtitleCount + " (не учтены в подсчёте)\n";
            if (hashStats.signsInSubtitlesCount > 0) {
                bottomMsg += "  Решёток в подзаголовках из решёток: " + hashStats.signsInSubtitlesCount + " (не учтены в подсчёте)\n";
            }
        }

        if (bottomMsg.length > 0) {
            msg += "---------------------------\n";
            msg += bottomMsg;
        }

        msg += "\nВремя выполнения: " + timeStr + " сек.";

        MsgBox(msg, "FBE скрипт");
    }
}
