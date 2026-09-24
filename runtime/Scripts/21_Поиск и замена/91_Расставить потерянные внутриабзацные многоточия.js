// Скрипт "Расставить потерянные внутриабзацные многоточия" для редактора FBE
// version 1.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для восстановления потерянных ВНУТРИАБЗАЦНЫХ многоточий в fb2 документах.
// Проблема замены многоточий на обычную точку характерна для многих pdf текстов, распознанных Файнридером.
// По очевидным причинам, восстановить многоточия в концах абзацев невозможно.
// При наличии выделения обрабатывается только текст внутри выделения.
// При отсутствии выделения обрабатывается весь документ.
// По умолчанию обрабатываются все разделы документа, кроме разделов сносок и комментариев.
// Скрипт находит ВНУТРИ абзацев случаи, где вместо многоточия стоит точка:
// 1) Заглавная буква короткого союза/предлога + точка + пробел + строчная
//    Пример: "И. что дальше" → "И… что дальше", "На. какой" → "На… какой"
// 2) Строчная буква + точка + пробел + строчная
//    Пример: "голова. болит" → "голова… болит"
// и заменяет точку на символ многоточия (… U+2026).
// Режим работы скрипта: обычный или тихий.
// Поддержка отмены всех действий (Ctrl+Z).

// !!! Во избежание ложных замен, лучше не запускать данный скрипт
// на сложных технических текстах, списках литературы, библиографиях и подобном.

// version 1.2, 04.08.2026
//======================================

function Run() {
    var scriptName = "Расставить потерянные внутриабзацные многоточия";
    var version = "1.2";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1;

    // Обрабатывать аннотацию ко всему документу
    var processAnnotation = 1; // 0 - нет, 1 - да

    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0; // 0 - нет, 1 - да

    // Обрабатывать раздел комментариев
    var processCommentsSection = 0; // 0 - нет, 1 - да

    // Короткие союзы и предлоги (1-3 буквы) - по алфавиту (только для случаев точки после ЗАГЛАВНЫХ союзов-предлогов):
    var shortWords = [
        "а", "без", "в", "вне", "во", "вон", "вот", "да", "для", "до",
        "же", "за", "и", "ибо", "из", "из-за", "изо", "или",
        "к", "как", "ко", "ли", "мол",
        "на", "над", "не", "ни", "но",
        "о", "об", "обо", "от", "ото",
        "по", "под", "при", "про", "раз",
        "с", "со", "то", "у", "что"
    ];

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    // Получаем неразрывный пробел из настроек FBE
    var nbspChar;
    var nbspEntity;
    try {
        nbspChar = window.external.GetNBSP();
        if (nbspChar.charCodeAt(0) == 160)
            nbspEntity = "&nbsp;";
        else
            nbspEntity = nbspChar;
    }
    catch(e) {
        nbspChar = String.fromCharCode(160);
        nbspEntity = "&nbsp;";
    }

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
        nbspChar;

    // Русские строчные буквы
    var lowerLetters = "абвгдеёжзийклмнопрстуфхцчшщъыьэюя";
    // Русские заглавные буквы
    var upperLetters = "АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ";

    // Символ многоточия
    var ellipsis = "\u2026"; // …

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

    // Проверка: является ли символ строчной русской буквой
    function isLowerRus(ch) {
        return lowerLetters.indexOf(ch) != -1;
    }

    // Проверка: является ли символ заглавной русской буквой
    function isUpperRus(ch) {
        return upperLetters.indexOf(ch) != -1;
    }

    // Проверка: совпадает ли строка (с учётом регистра первой буквы)
    // с коротким словом из словаря, начинающимся с заглавной
    function matchShortWord(text, startPos) {
        for (var w = 0; w < shortWords.length; w++) {
            var word = shortWords[w];
            var wordLen = word.length;
            if (startPos + wordLen > text.length) continue;
            var firstChar = text.charAt(startPos);
            if (firstChar != word.charAt(0).toUpperCase()) continue;
            var match = true;
            for (var j = 1; j < wordLen; j++) {
                if (text.charAt(startPos + j) != word.charAt(j)) {
                    match = false;
                    break;
                }
            }
            if (match) return wordLen;
        }
        return 0;
    }

    // Рекурсивный сбор текстовых узлов в правильном порядке (по ходу текста)
    function collectTextNodesInOrder(element, resultArray) {
        var children = element.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 3) {
                // Текстовый узел
                resultArray.push(child);
            } else if (child.nodeType == 1) {
                // Пропускаем сноски целиком
                if (child.nodeName == "A") {
                    var cn = child.className || "";
                    var hr = child.getAttribute("href") || "";
                    if (cn == "note" || (hr.length > 0 && hr.charAt(0) == "#")) {
                        continue;
                    }
                }
                // Заходим внутрь элемента
                collectTextNodesInOrder(child, resultArray);
            }
        }
    }

    // ==================================================
    // ОПРЕДЕЛЕНИЕ РЕЖИМА (ВЫДЕЛЕНИЕ / ВЕСЬ ДОКУМЕНТ)
    // ==================================================

    var fbw_body = document.getElementById("fbw_body");
    if (!fbw_body) {
        MsgBox("Ошибка: не найден fbw_body", "FBE скрипт");
        return;
    }

    var hasSelection = false;
    var selectionRange = null;

    try {
        var sel = document.selection;
        if (sel && sel.type && sel.type == "Text") {
            var tr = sel.createRange();
            if (tr && tr.compareEndPoints("StartToEnd", tr) != 0) {
                var parentEl = tr.parentElement();
                if (parentEl && parentEl.nodeName != "TEXTAREA" && parentEl.nodeName != "INPUT") {
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

    // ==================================================
    // СБОР АБЗАЦЕВ ДЛЯ ОБРАБОТКИ
    // ==================================================

    var paragraphsToProcess = [];

    if (hasSelection) {
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
        var bodyDivs = document.getElementsByTagName("DIV");
        var targetBodies = [];

        // Аннотация (если включена)
        if (processAnnotation) {
            var fbwBodyChildren = fbw_body.childNodes;
            for (var ac = 0; ac < fbwBodyChildren.length; ac++) {
                var child = fbwBodyChildren[ac];
                if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "annotation") {
                    var allP = child.getElementsByTagName("P");
                    for (var ap = 0; ap < allP.length; ap++) {
                        paragraphsToProcess.push(allP[ap]);
                    }
                    break;
                }
            }
        }

        // Основные разделы
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
        MsgBox("Не найдено абзацев для обработки.", "FBE скрипт");
        return;
    }

    // ==================================================
    // ТАЙМЕР ЗАПУСКАЕМ ПОСЛЕ ПРОВЕРОК, ПЕРЕД ИЗМЕНЕНИЯМИ
    // ==================================================
    var startTime = new Date();

    window.external.BeginUndoUnit(document, scriptName);

    // ==================================================
    // ФАЗА 1: СБОР ДАННЫХ (только чтение)
    // ==================================================

    var replacementData = [];

    for (var i = 0; i < paragraphsToProcess.length; i++) {
        var pElement = paragraphsToProcess[i];

        // Собираем текстовые узлы в правильном порядке (рекурсивно, по ходу текста)
        var textNodes = [];
        collectTextNodesInOrder(pElement, textNodes);

        if (textNodes.length == 0) continue;

        // Собираем полный текст абзаца и карту привязки к текстовым узлам
        var fullText = "";
        var nodeMap = [];

        for (var tn = 0; tn < textNodes.length; tn++) {
            var text = textNodes[tn].nodeValue;
            nodeMap.push({
                node: textNodes[tn],
                start: fullText.length,
                length: text.length
            });
            fullText += text;
        }

        if (fullText.length < 3) continue;

        // Ищем совпадения в полном тексте
        var replacements = [];

        for (var pos = 0; pos < fullText.length - 2; pos++) {
            var ch = fullText.charAt(pos);

            // Правило 1: заглавная буква + короткое слово + точка + пробел + строчная
            if (isUpperRus(ch)) {
                var wordLen = matchShortWord(fullText, pos);
                if (wordLen > 0) {
                    var dotPos = pos + wordLen;
                    if (dotPos < fullText.length && fullText.charAt(dotPos) == ".") {
                        var spacePos = dotPos + 1;
                        if (spacePos < fullText.length && isSpace(fullText.charAt(spacePos))) {
                            var afterSpacePos = spacePos + 1;
                            if (afterSpacePos < fullText.length && isLowerRus(fullText.charAt(afterSpacePos))) {
                                replacements.push({ offset: dotPos, wordLen: wordLen });
                            }
                        }
                    }
                }
            }

            // Правило 2: строчная + точка + пробел + строчная
            if (isLowerRus(ch)) {
                if (pos + 1 < fullText.length && fullText.charAt(pos + 1) == ".") {
                    var spacePos2 = pos + 2;
                    if (spacePos2 < fullText.length && isSpace(fullText.charAt(spacePos2))) {
                        var afterSpacePos2 = spacePos2 + 1;
                        if (afterSpacePos2 < fullText.length && isLowerRus(fullText.charAt(afterSpacePos2))) {
                            var isAlreadyCovered = false;
                            for (var r = 0; r < replacements.length; r++) {
                                if (replacements[r].offset == pos + 1) {
                                    isAlreadyCovered = true;
                                    break;
                                }
                            }
                            if (!isAlreadyCovered) {
                                replacements.push({ offset: pos + 1, wordLen: 0 });
                            }
                        }
                    }
                }
            }
        }

        if (replacements.length > 0) {
            replacementData.push({
                element: pElement,
                textNodes: nodeMap,
                fullText: fullText,
                replacements: replacements
            });
        }
    }

    // ==================================================
    // ФАЗА 2: ПРИМЕНЕНИЕ ЗАМЕН (только запись)
    // ==================================================

    var totalReplacements = 0;
    var affectedParagraphs = 0;

    for (var d = 0; d < replacementData.length; d++) {
        var data = replacementData[d];
        var replacements = data.replacements;
        var nodeMap = data.textNodes;

        if (replacements.length == 0) continue;

        // Сортируем замены по offset (по возрастанию)
        for (var si = 0; si < replacements.length - 1; si++) {
            for (var sj = si + 1; sj < replacements.length; sj++) {
                if (replacements[si].offset > replacements[sj].offset) {
                    var temp = replacements[si];
                    replacements[si] = replacements[sj];
                    replacements[sj] = temp;
                }
            }
        }

        var paraReplacements = 0;

        for (var r = 0; r < replacements.length; r++) {
            var offset = replacements[r].offset;

            for (var tn = 0; tn < nodeMap.length; tn++) {
                var map = nodeMap[tn];
                if (offset >= map.start && offset < map.start + map.length) {
                    var localPos = offset - map.start;
                    var textNode = map.node;
                    var nodeValue = textNode.nodeValue;

                    if (localPos < nodeValue.length && nodeValue.charAt(localPos) == ".") {
                        textNode.nodeValue = nodeValue.substring(0, localPos) + ellipsis + nodeValue.substring(localPos + 1);
                        paraReplacements++;
                        totalReplacements++;
                    }
                    break;
                }
            }
        }

        if (paraReplacements > 0) {
            affectedParagraphs++;
        }
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
    if (showStatistics == 1 || totalReplacements == 0) {
        var modeStr = hasSelection ? "Режим: ВЫДЕЛЕНИЕ" : "Режим: ВЕСЬ ДОКУМЕНТ";

        var sectionsInfo = "";
        sectionsInfo = "\n  \u2022 Обработка аннотации: " + (processAnnotation ? "включена" : "выключена");
        sectionsInfo += "\n  \u2022 Обработка сносок: " + (processNotesSection ? "включена" : "выключена");
        sectionsInfo += "\n  \u2022 Обработка комментариев: " + (processCommentsSection ? "включена" : "выключена");

        var msg = "";
        msg += scriptName + "\n";
        msg += "ver. " + version + "\n\n";

        msg += modeStr + "\n";
        msg += sectionsInfo + "\n\n";

        msg += "  \u2022 Всего проверено абзацев: " + paragraphsToProcess.length + "\n";
        msg += "  \u2022 Абзацев с заменами: " + affectedParagraphs + "\n";
        msg += "  \u2713 Всего замен точки на многоточие: " + totalReplacements + "\n";

        if (totalReplacements == 0) {
            msg += "\nПотерянных многоточий не обнаружено.\n";
        }

        msg += "\nВремя выполнения: " + timeStr + " сек.";

        MsgBox(msg, "FBE скрипт");
    }
}