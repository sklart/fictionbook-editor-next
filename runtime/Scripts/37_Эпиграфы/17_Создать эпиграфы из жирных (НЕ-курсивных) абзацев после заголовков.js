// Скрипт "Создать эпиграфы из жирных (НЕ-курсивных) абзацев после заголовков" для редактора FBE
// version 1.4
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для создания эпиграфов в fb2 документах
// из полностью жирных абзацев (не содержащих курсив), следующих сразу после заголовков.
// По умолчанию скрипт работает с основным разделом книги (не затрагивает разделы примечаний и комментариев).
// Обрабатываются только обычные абзацы (P без класса), не вложенные в другие DIV'ы (структурные элементы).
// Абзацы с совмещением жирности и курсива (STRONG+EM или EM+STRONG) исключаются из обработки.
// Настрока ограничения по количеству абзацев и их длине.
// Опциональное исключение из обработки абзацев с точкой в конце абзаца.
// Опциональное расформатирование созданных эпиграфов от исходной жирности.
// Опциональное исключение абзацев, похожих на диалоги.
// Опциональное автоматическое создание авторской строки ("text-author") для последнего абзаца эпиграфа.
//
// АЛГОРИТМ СОЗДАНИЯ СТРОКИ "АВТОР ТЕКСТА" (при authorParagraphMode=2):
// 1. Проверяется разница длины с предыдущим абзацем
// 2. Если последний абзац короче предыдущего на minLengthDiffPercent% или более → АВТОР
// 3. Если НЕ короче: проверяются кавычки в тексте
// 4. Если есть кавычки → АВТОР
// 5. Если нет кавычек: проверяется похожесть на ФИО
// 6. Если похоже на ФИО → АВТОР
// 7. Если длиннее, нет кавычек и не ФИО → НЕ АВТОР
//
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.4, 02.05.2026
//======================================

function Run() {

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1;

    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0;

    // Обрабатывать раздел комментариев
    var processCommentsSection = 0;

    // Размечать эпиграфом абзацы, похожие на диалоги (тире или дефис в начале абзаца)
    // 0 - нет, 1 - да
    var formatDialogs = 0;

    // Максимальное количество подходящих подряд идущих абзацев, размечаемых эпиграфом после заголовков
    // 0 - 1 абзац, 1 - 2 абзаца, 2 - 3 абзаца, 3 - 4 абзаца.
    var MaxParagraphsQty = 1;

    // Максимальная длина подходящего абзаца в символах (без учета тегов, при превышении не трогаем)
    var maxParagraphsLength = 200;

    // Создавать эпиграфы из абзацев с точкой в конце
    // 0 - нет (пропускаем абзацы с точкой), 1 - да (создаём независимо от точки)
    var requireDotAtEnd = 1;

    // Расформатировать созданные эпиграфы от жирности
    // 0 - оставить как есть, 1 - расформатировать от жирности
    var ReformatEpigraphs = 1;

    // Настройка создания "авторского" абзаца (text-author) для эпиграфа:
    // 0 - Никогда не создавать
    // 1 - Создавать, если последний абзац короче предыдущего
    // 2 - Создавать, если последний абзац содержит кавычки или ФИО
    // 3 - Всегда создавать
    var authorParagraphMode = 2;

    // Минимальный процент разницы длины для создания авторского абзаца (0-100)
    // Если последний абзац короче предыдущего на этот процент или более,
    // то он может стать авторским (при authorParagraphMode=1 или 2)
    var minLengthDiffPercent = 10;

    // Максимальная длина абзаца для проверки ФИО (символов)
    var maxNameLength = 100;

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    var scriptName = "Создать эпиграфы из жирных (НЕ-курсивных) абзацев после заголовков";
    var version = "1.4";

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

    // Список необычных пробелов для нормализации
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

    function trimStr(str) {
        if (!str) return str;
        return str.replace(/^\s+|\s+$/g, '');
    }

    function normalizeSpaces(str) {
        if (!str) return str;
        var result = str;
        for (var i = 0; i < unusualSpaces.length; i++) {
            var ch = unusualSpaces.charAt(i);
            var re = new RegExp(ch, "g");
            result = result.replace(re, " ");
        }
        return trimStr(result);
    }

    function getPlainText(element) {
        if (!element) return "";
        var txt = element.innerText || element.textContent || "";
        return normalizeSpaces(txt);
    }

    function endsWithDot(text) {
        if (!text) return false;
        var t = trimStr(text);
        if (t.length === 0) return false;
        var lastChar = t.charAt(t.length - 1);
        return (lastChar === "." || lastChar === "!" || lastChar === "?");
    }

    function isDialogStart(text) {
        if (!text) return false;
        var t = trimStr(text);
        if (t.length === 0) return false;
        var firstChar = t.charAt(0);
        var code = firstChar.charCodeAt(0);
        if (code === 45) return true;
        if (code === 8211) return true;
        if (code === 8212) return true;
        if (code === 8213) return true;
        if (code === 8208) return true;
        if (code === 8209) return true;
        if (code === 8210) return true;
        return false;
    }

    function isFIO(text) {
        if (!text || text.length > maxNameLength) return false;

        var normalized = normalizeSpaces(text);
        if (normalized.length === 0) return false;

        var cleanText = normalized.replace(/\[\d+\]/g, '');
        cleanText = trimStr(cleanText);
        if (cleanText.length === 0) return false;

        var hasLetterDot = false;
        var capitalCount = 0;
        var dotCount = 0;

        for (var i = 0; i < cleanText.length; i++) {
            var char = cleanText.charAt(i);
            var code = char.charCodeAt(0);

            var isUpper = (code >= 1040 && code <= 1071) || (code >= 65 && code <= 90);

            if (isUpper) {
                capitalCount++;
                if (i < cleanText.length - 1 && cleanText.charAt(i + 1) === '.') {
                    hasLetterDot = true;
                }
            }

            if (char === '.') dotCount++;
        }

        var words = cleanText.split(/\s+/);

        if (words.length > 4) return false;
        if (capitalCount === 0) return false;

        var variant1 = hasLetterDot;
        var variant2 = dotCount >= 2;
        var variant3 = capitalCount >= words.length * 0.7;

        return variant1 || variant2 || variant3;
    }

    function hasQuotes(text) {
        if (!text) return false;

        var quotePairs = [
            { open: '\u00AB', close: '\u00BB' },
            { open: '"', close: '"' },
            { open: '\u201E', close: '\u00AB' }
        ];

        for (var i = 0; i < quotePairs.length; i++) {
            var openChar = quotePairs[i].open;
            var closeChar = quotePairs[i].close;
            var openCount = 0;
            var closeCount = 0;

            for (var j = 0; j < text.length; j++) {
                var currentChar = text.charAt(j);
                if (currentChar === openChar) openCount++;
                if (currentChar === closeChar) closeCount++;
            }

            if (openChar === closeChar) {
                if (openCount >= 2) return true;
            } else {
                if (openCount >= 1 && closeCount >= 1) return true;
            }
        }

        return false;
    }

    // Проверка: полностью ли элемент внутри STRONG (без EM внутри)
    function isFullyStrong(element) {
        if (!element) return false;
        var html = element.innerHTML;
        if (!html) return false;
        html = trimStr(html);

        var startPattern = /^<STRONG(\s[^>]*)?>/i;
        var endPattern = /<\/STRONG>$/i;
        if (!startPattern.test(html) || !endPattern.test(html)) {
            return false;
        }

        var innerHtmlAfter = html.replace(startPattern, "");
        var innerHtml = innerHtmlAfter.replace(endPattern, "");
        if (/<EM(\s[^>]*)?>/i.test(innerHtml) || /<\/EM>/i.test(innerHtml)) {
            return false;
        }

        return true;
    }

    function isSubtitle(element) {
        if (!element || element.nodeType !== 1) return false;
        if (element.nodeName !== "P") return false;
        return (element.className || "") === "subtitle";
    }

    function stripStrong(element) {
        if (!element) return;
        var html = element.innerHTML;
        if (!html) return;
        html = html.replace(/<STRONG(\s[^>]*)?>/gi, "");
        html = html.replace(/<\/STRONG>/gi, "");
        try {
            element.innerHTML = html;
        } catch (e) {}
    }

    function removeAllFormatting(elem) {
        if (!elem) return;
        var html = elem.innerHTML;
        if (!html) return;
        html = html.replace(/<STRONG(\s[^>]*)?>/gi, "");
        html = html.replace(/<\/STRONG>/gi, "");
        html = html.replace(/<EM(\s[^>]*)?>/gi, "");
        html = html.replace(/<\/EM>/gi, "");
        try {
            elem.innerHTML = html;
        } catch (e) {}
    }

    // ==================================================
    // ФУНКЦИЯ ПОЛУЧЕНИЯ РАЗДЕЛОВ ДЛЯ ОБРАБОТКИ
    // ==================================================

    function getBodiesToProcess() {
        var bodies = [];
        var allDivs = document.getElementsByTagName("DIV");
        for (var i = 0; i < allDivs.length; i++) {
            if (allDivs[i].className === "body") {
                var fbname = allDivs[i].getAttribute("fbname") || "";
                if (fbname === "") {
                    bodies.push({ element: allDivs[i], type: "main" });
                } else if (fbname === "notes" && processNotesSection === 1) {
                    bodies.push({ element: allDivs[i], type: "notes" });
                } else if (fbname === "comments" && processCommentsSection === 1) {
                    bodies.push({ element: allDivs[i], type: "comments" });
                }
            }
        }
        return bodies;
    }

    // ==================================================
    // ФУНКЦИЯ ПОИСКА ПОДХОДЯЩИХ АБЗАЦЕВ ПОСЛЕ ЗАГОЛОВКОВ
    // ==================================================

    function findEpigraphCandidates(bodyElement) {
        var candidates = [];

        var allSections = bodyElement.getElementsByTagName("DIV");
        for (var i = 0; i < allSections.length; i++) {
            var div = allSections[i];
            if (div.className !== "section") continue;

            var titleDiv = null;
            var children = div.childNodes;
            for (var j = 0; j < children.length; j++) {
                var child = children[j];
                if (child.nodeType === 1 && child.nodeName === "DIV" && child.className === "title") {
                    titleDiv = child;
                    break;
                }
            }
            if (!titleDiv) continue;

            var nextSibling = titleDiv.nextSibling;
            while (nextSibling && nextSibling.nodeType === 3) {
                var txt = nextSibling.nodeValue || "";
                if (trimStr(txt) !== "") break;
                nextSibling = nextSibling.nextSibling;
            }
            if (!nextSibling) continue;
            if (nextSibling.nodeType !== 1) continue;
            if (nextSibling.nodeName !== "P") continue;
            if (isSubtitle(nextSibling)) continue;

            var firstP = nextSibling;

            if (!isFullyStrong(firstP)) continue;

            var firstPlainText = getPlainText(firstP);
            if (firstPlainText.length > maxParagraphsLength) continue;

            if (formatDialogs === 0 && isDialogStart(firstPlainText)) continue;

            if (requireDotAtEnd === 0 && endsWithDot(firstPlainText)) continue;

            var maxCount = MaxParagraphsQty + 1;
            var chain = [];
            chain.push(firstP);

            var currentP = firstP;
            for (var k = 1; k < maxCount; k++) {
                var ns = currentP.nextSibling;
                while (ns && ns.nodeType === 3) {
                    var t = ns.nodeValue || "";
                    if (trimStr(t) !== "") break;
                    ns = ns.nextSibling;
                }
                if (!ns || ns.nodeType !== 1 || ns.nodeName !== "P") break;
                if (isSubtitle(ns)) break;

                if (!isFullyStrong(ns)) break;

                var nsPlainText = getPlainText(ns);
                if (nsPlainText.length > maxParagraphsLength) break;
                if (formatDialogs === 0 && isDialogStart(nsPlainText)) break;
                if (requireDotAtEnd === 0 && endsWithDot(nsPlainText)) break;

                chain.push(ns);
                currentP = ns;
            }

            candidates.push({
                paragraphs: chain,
                sectionDiv: div
            });
        }

        return candidates;
    }

    // ==================================================
    // ФУНКЦИЯ ФОРМИРОВАНИЯ СТРОКИ НАСТРОЕК (общая)
    // ==================================================

    function getSettingsString() {
        var authorModeText = "";
        switch (authorParagraphMode) {
            case 0: authorModeText = "Никогда"; break;
            case 1: authorModeText = "Если короче предыдущего (" + minLengthDiffPercent + "%+)"; break;
            case 2: authorModeText = "Если короче (" + minLengthDiffPercent + "%+), кавычки или ФИО"; break;
            case 3: authorModeText = "Всегда"; break;
            default: authorModeText = "По умолчанию (2)"; break;
        }

        var settings = "";
        settings += "  • Абзацев после заголовка: " + (MaxParagraphsQty + 1) + "\n";
        settings += "  • Макс. длина абзаца: " + maxParagraphsLength + " символов\n";
        settings += "  • С точкой в конце: " + (requireDotAtEnd === 1 ? "ДА" : "НЕТ") + "\n";
        settings += "  • Диалоги: " + (formatDialogs === 1 ? "ДА" : "НЕТ") + "\n";
        settings += "  • Расформатировать исходные абзацы: " + (ReformatEpigraphs === 1 ? "ДА" : "НЕТ") + "\n";
        settings += "  • Автор текста: " + authorModeText + "\n";
        settings += "  • Обработка раздела сносок (примечаний): " + (processNotesSection === 1 ? "ДА" : "НЕТ") + "\n";
        settings += "  • Обработка раздела комментариев: " + (processCommentsSection === 1 ? "ДА" : "НЕТ");

        return settings;
    }

    // ==================================================
    // ПРЕДВАРИТЕЛЬНЫЙ АНАЛИЗ
    // ==================================================

    var bodiesAll = getBodiesToProcess();
    var allCandidates = [];
    for (var b = 0; b < bodiesAll.length; b++) {
        var cands = findEpigraphCandidates(bodiesAll[b].element);
        for (var c = 0; c < cands.length; c++) {
            cands[c].bodyType = bodiesAll[b].type;
            allCandidates.push(cands[c]);
        }
    }

    var totalCandidates = allCandidates.length;
    var totalParagraphs = 0;
    var byBody = { main: 0, notes: 0, comments: 0 };
    var totalParagraphsByBody = { main: 0, notes: 0, comments: 0 };

    for (var d = 0; d < allCandidates.length; d++) {
        var cand = allCandidates[d];
        var pCount = cand.paragraphs.length;
        totalParagraphs += pCount;
        byBody[cand.bodyType] = (byBody[cand.bodyType] || 0) + pCount;
        totalParagraphsByBody[cand.bodyType] = (totalParagraphsByBody[cand.bodyType] || 0) + pCount;
    }

    // ==================================================
    // ЕСЛИ НИЧЕГО НЕ НАЙДЕНО — СООБЩАЕМ И ВЫХОДИМ
    // ==================================================

    if (totalCandidates === 0) {
        var noResultMsg = scriptName + "\n" +
            "ver. " + version + "\n" +
            "---------------------------------------\n" +
            "Ничего не найдено для обработки.\n\n" +
            "Текущие настройки:\n" +
            getSettingsString();

        MsgBox(noResultMsg);
        return;
    }

    // ==================================================
    // АНАЛИЗ И СТАТИСТИКА (основной режим)
    // ==================================================

    if (showStatistics === 1) {
        var analysisMsg = scriptName + "\n" +
            "ver. " + version + "\n" +
            "---------------------------------------\n" +
            "✓ Всего кандидатов в эпиграфы: " + totalCandidates + "\n" +
            "  • Всего абзацев в них: " + totalParagraphs + "\n\n" +
            "По разделам:\n" +
            "  • В основном разделе: " + (totalParagraphsByBody.main || 0) + " абз. (" + (byBody.main || 0) + " групп)\n" +
            "  • В сносках: " + (totalParagraphsByBody.notes || 0) + " абз. (" + (byBody.notes || 0) + " групп)\n" +
            "  • В комментариях: " + (totalParagraphsByBody.comments || 0) + " абз. (" + (byBody.comments || 0) + " групп)\n\n" +
            "Текущие настройки:\n" +
            getSettingsString() + "\n\n" +
            "Будет создано эпиграфов: " + totalCandidates + "\n" +
            "Будет расформатировано абзацев: " + (ReformatEpigraphs === 1 ? totalParagraphs : 0) + "\n\n" +
            "Продолжить?";

        if (!AskYesNo(analysisMsg)) return;
    }

    // ==================================================
    // ОСНОВНАЯ ОБРАБОТКА
    // ==================================================

    var startTime = new Date().getTime();

    window.external.BeginUndoUnit(document, scriptName);

    var bodiesForProcess = getBodiesToProcess();
    var totalCreatedEpigraphs = 0;
    var totalParagraphsInEpigraphs = 0;
    var totalReformatted = 0;
    var totalAuthorLines = 0;
    var createdByBody = { main: 0, notes: 0, comments: 0 };

    for (var bi = 0; bi < bodiesForProcess.length; bi++) {
        var bodyInfo = bodiesForProcess[bi];
        var candidates = findEpigraphCandidates(bodyInfo.element);

        for (var ci = candidates.length - 1; ci >= 0; ci--) {
            var candidate = candidates[ci];
            var paragraphs = candidate.paragraphs;

            var epDiv = document.createElement("DIV");
            epDiv.className = "epigraph";

            var authorIndex = -1;
            if (paragraphs.length > 1) {
                var lastP = paragraphs[paragraphs.length - 1];
                var lastText = getPlainText(lastP);
                var prevText = getPlainText(paragraphs[paragraphs.length - 2]);

                if (lastText.length > 0 && prevText.length > 0) {
                    var lengthDiffPercent = ((prevText.length - lastText.length) / prevText.length) * 100;
                    var isShorterByPercent = lengthDiffPercent >= minLengthDiffPercent;

                    var makeAuthor = false;
                    switch (authorParagraphMode) {
                        case 0:
                            makeAuthor = false;
                            break;
                        case 1:
                            makeAuthor = isShorterByPercent;
                            break;
                        case 2:
                            if (isShorterByPercent) {
                                makeAuthor = true;
                            } else {
                                if (hasQuotes(lastText)) {
                                    makeAuthor = true;
                                } else {
                                    makeAuthor = isFIO(lastText);
                                }
                            }
                            break;
                        case 3:
                            makeAuthor = true;
                            break;
                        default:
                            if (isShorterByPercent) {
                                makeAuthor = true;
                            } else {
                                if (hasQuotes(lastText)) {
                                    makeAuthor = true;
                                } else {
                                    makeAuthor = isFIO(lastText);
                                }
                            }
                            break;
                    }

                    if (makeAuthor) {
                        authorIndex = paragraphs.length - 1;
                    }
                }
            } else if (paragraphs.length === 1 && authorParagraphMode === 3) {
                authorIndex = 0;
            }

            for (var pi = 0; pi < paragraphs.length; pi++) {
                var p = paragraphs[pi];
                var newP = document.createElement("P");

                if (pi === authorIndex) {
                    newP.className = "text-author";
                    newP.innerHTML = p.innerHTML;
                    removeAllFormatting(newP);
                    totalAuthorLines++;
                } else {
                    newP.innerHTML = p.innerHTML;
                    if (ReformatEpigraphs === 1) {
                        stripStrong(newP);
                        totalReformatted++;
                    }
                }

                epDiv.appendChild(newP);
            }

            var firstParagraph = paragraphs[0];
            firstParagraph.parentNode.insertBefore(epDiv, firstParagraph);

            for (var ri = paragraphs.length - 1; ri >= 0; ri--) {
                paragraphs[ri].parentNode.removeChild(paragraphs[ri]);
            }

            totalCreatedEpigraphs++;
            totalParagraphsInEpigraphs += paragraphs.length;
            createdByBody[bodyInfo.type] = (createdByBody[bodyInfo.type] || 0) + 1;

            var sectionDiv = candidate.sectionDiv;
            var secTitles = sectionDiv.getElementsByTagName("DIV");
            for (var ti = 0; ti < secTitles.length; ti++) {
                if (secTitles[ti].className === "title" && secTitles[ti].parentNode === sectionDiv) {
                    if (!secTitles[ti].firstChild || trimStr(secTitles[ti].innerHTML) === "") {
                        try {
                            secTitles[ti].parentNode.removeChild(secTitles[ti]);
                        } catch (e2) {}
                    }
                    break;
                }
            }
        }
    }

    window.external.EndUndoUnit(document);

    var endTime = new Date().getTime();
    var executionTime = (endTime - startTime) / 1000;

    // ==================================================
    // ИТОГОВАЯ СТАТИСТИКА
    // ==================================================

    if (showStatistics === 1) {
        var resultMsg = scriptName + "\n" +
            "ver. " + version + "\n" +
            "---------------------------------------\n" +
            "✓ Операция завершена\n" +
            "  • Создано эпиграфов: " + totalCreatedEpigraphs + "\n" +
            "  • Всего абзацев в эпиграфах: " + totalParagraphsInEpigraphs + "\n" +
            "  • Создано авторских строк: " + totalAuthorLines + "\n\n" +
            "  • По разделам:\n" +
            "    - Основной: " + (createdByBody.main || 0) + " эпиграфов\n" +
            "    - Сноски: " + (createdByBody.notes || 0) + " эпиграфов\n" +
            "    - Комментарии: " + (createdByBody.comments || 0) + " эпиграфов\n\n" +
            "✓ Расформатировано от жирности: " + totalReformatted + " абз.\n\n" +
            "✓ Время обработки: " + executionTime.toFixed(3).replace(".", ",") + " сек.";

        MsgBox(resultMsg);
    }
}
