// Скрипт "Произвести вложенность однотипных разделов" для редактора FBE
// version 5.3
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для вложения однотипных равноуровневых разделов с единообразными заголовками
// типа Часть, Глава, Раздел, или заголовки, состоящие из арабских или римских цифр (чисел).
// внутрь других однотипных разделов в fb2 документах.
// Обрабатывается только основной раздел документа (сноски и комментарии исключены из обработки).
// Производится обработка только разделов первого уровня (без вложенности).
// Уже вложенные разделы скрипт не обрабатывает.
// Вложенность производится на глубину только 1 раздела.
// Скрипт анализирует заголовки разделов, определяет будущие родительские и вложенные разделы
// по началу или содержанию типичных ключевых слов или цифр.
// На основе анализа fb2 документа, скрипт автоматически определяет
// наиболее подходящие для обработки разделы с типовыми заголовками.
// Если скрипт неверно определил однотипные разделы для создания вложенности, можно указать их вручную.
// Разделы с нетипичными заголовками в конце (перед следующим "родителем")
// по умолчанию не вкладываются (есть отдельная настройка).
// Пустые "подозрительные" секции показываются в предупреждении перед выполнением вложенности.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 5.3, 22.08.2026
//======================================

function Run() {
    var scriptName = "Произвести вложенность однотипных разделов";
    var version = "5.3";
    
    var showStatistics = 1;
    
    var settingsFile = "HTML\\Вложенность однотипных разделов - параметры.htm";
    
    var nbspChar;
    try {
        nbspChar = window.external.GetNBSP();
    } catch(e) {
        nbspChar = String.fromCharCode(160);
    }
    
    var re0 = new RegExp("^( |\u00A0|&nbsp;|" + nbspChar + ")*?$", "");
    var re2 = new RegExp("<(?!img)[^>]*?>", "gi");
    
    // ==================================================
    // ФУНКЦИЯ: getSectionTitleText
    // НАЗНАЧЕНИЕ: Получает текст заголовка секции
    // ==================================================
    function getSectionTitleText(sectionElement) {
        var children = sectionElement.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "title") {
                var titleText = "";
                function collectText(node) {
                    if (node.nodeType == 3) {
                        titleText += node.nodeValue;
                    } else if (node.nodeType == 1) {
                        var kids = node.childNodes;
                        for (var k = 0; k < kids.length; k++) {
                            collectText(kids[k]);
                        }
                    }
                }
                collectText(child);
                titleText = titleText.replace(/\s+/g, ' ');
                titleText = titleText.replace(/^\s+|\s+$/g, '');
                return titleText;
            }
        }
        return "";
    }
    
    // ==================================================
    // ФУНКЦИЯ: startsWith
    // ==================================================
    function startsWith(str, prefix) {
        if (!str || !prefix) return false;
        if (str.length < prefix.length) return false;
        return str.substring(0, prefix.length) == prefix;
    }
    
    // ==================================================
    // ФУНКЦИЯ: getArabicLevel
    // НАЗНАЧЕНИЕ: Определяет уровень арабского числа
    // ПАРАМЕТРЫ: str - проверяемая строка
    // ВОЗВРАЩАЕТ: 1 - один уровень (1), 2 - два уровня (1.1), 3 - три уровня (1.1.2)
    //             0 - если строка не является арабским числом
    // ПРИМЕЧАНИЕ: Убирает точку в конце, считает количество точек-разделителей
    // ==================================================
    function getArabicLevel(str) {
        if (!str || str.length == 0) return 0;
        
        // Убираем точку в конце (если есть)
        var cleanStr = str;
        if (cleanStr.charAt(cleanStr.length - 1) == '.') {
            cleanStr = cleanStr.substring(0, cleanStr.length - 1);
        }
        
        if (cleanStr.length == 0) return 0;
        
        // Проверяем, что строка состоит только из цифр и точек
        for (var i = 0; i < cleanStr.length; i++) {
            var ch = cleanStr.charAt(i);
            if (ch != '.' && (ch < '0' || ch > '9')) {
                return 0;
            }
        }
        
        // Считаем количество точек-разделителей
        var separatorCount = 0;
        for (var j = 0; j < cleanStr.length; j++) {
            if (cleanStr.charAt(j) == '.') {
                separatorCount++;
            }
        }
        
        // Проверяем, что нет пустых частей между точками
        var parts = cleanStr.split('.');
        for (var k = 0; k < parts.length; k++) {
            if (parts[k].length == 0) {
                return 0;
            }
        }
        
        // Уровень = количество разделителей + 1
        return separatorCount + 1;
    }
    
    // ==================================================
    // ФУНКЦИЯ: isArabicNumber
    // НАЗНАЧЕНИЕ: Проверяет, является ли строка арабским числом
    // ==================================================
    function isArabicNumber(str) {
        return getArabicLevel(str) > 0;
    }
    
    // ==================================================
    // ФУНКЦИЯ: isRomanNumber
    // ==================================================
    function isRomanNumber(str) {
        if (!str || str.length == 0) return false;
        var cleanStr = str.replace(/\.$/, '');
        if (cleanStr.length == 0) return false;
        var romanChars = "IVXLCDMivxlcdm";
        for (var i = 0; i < cleanStr.length; i++) {
            var ch = cleanStr.charAt(i);
            if (romanChars.indexOf(ch) < 0) return false;
        }
        return true;
    }
    
    // ==================================================
    // ФУНКЦИЯ: getFirstWord
    // ==================================================
    function getFirstWord(titleText) {
        if (!titleText || titleText.length == 0) return "";
        var spaceIndex = titleText.indexOf(" ");
        if (spaceIndex < 0) return titleText;
        return titleText.substring(0, spaceIndex);
    }
    
    // ==================================================
    // ФУНКЦИЯ: toLowerCaseStr
    // ==================================================
    function toLowerCaseStr(str) {
        if (!str) return "";
        return str.toLowerCase();
    }
    
    // ==================================================
    // ФУНКЦИЯ: matchesTitleType
    // ==================================================
    function matchesTitleType(titleText, typePrefix) {
        if (!titleText || !typePrefix) return false;
        
        if (typePrefix == 'Арабские - 1 уровень') {
            return getArabicLevel(titleText) == 1;
        }
        if (typePrefix == 'Арабские - 2 уровня') {
            return getArabicLevel(titleText) == 2;
        }
        if (typePrefix == 'Арабские - 3 уровня') {
            return getArabicLevel(titleText) == 3;
        }
        if (typePrefix == 'Римские') {
            return isRomanNumber(titleText);
        }
        if (typePrefix.substring(0, 7) == 'CUSTOM:') {
            var customPrefix = typePrefix.substring(7).replace(/^\s+|\s+$/g, '');
            var lowerTitle = toLowerCaseStr(titleText);
            var lowerCustom = toLowerCaseStr(customPrefix);
            return startsWith(lowerTitle, lowerCustom) || lowerTitle.indexOf(lowerCustom) >= 0;
        }
        
        var lowerTitle = toLowerCaseStr(titleText);
        var lowerPrefix = toLowerCaseStr(typePrefix);
        
        return startsWith(lowerTitle, lowerPrefix) || lowerTitle.indexOf(lowerPrefix) >= 0;
    }
    
    // ==================================================
    // ФУНКЦИЯ: isEmptyLineElement
    // ==================================================
    function isEmptyLineElement(element) {
        if (element.nodeType != 1) return false;
        if (element.nodeName != "P") return false;
        
        var html = element.innerHTML || "";
        
        if (re0.test(html.replace(re2, ""))) {
            return true;
        }
        
        return false;
    }
    
    // ==================================================
    // ФУНКЦИЯ: removeEmptyLinesInsideParent
    // ==================================================
    function removeEmptyLinesInsideParent(parentSection) {
        var children = parentSection.childNodes;
        var toRemove = [];
        
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            
            if (child.nodeType == 1 && child.nodeName == "P") {
                if (isEmptyLineElement(child)) {
                    toRemove.push(child);
                }
            } else if (child.nodeType == 3) {
                var nodeText = child.nodeValue || "";
                if (re0.test(nodeText)) {
                    toRemove.push(child);
                }
            }
        }
        
        for (var r = 0; r < toRemove.length; r++) {
            toRemove[r].removeNode(false);
        }
    }
    
    // ==================================================
    // ФУНКЦИЯ: collectAllSections
    // ==================================================
    function collectAllSections(container, resultArray) {
        var children = container.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "section") {
                resultArray.push(child);
                collectAllSections(child, resultArray);
            }
        }
    }
    
    // ==================================================
    // ФУНКЦИЯ: hasNestedSections
    // ==================================================
    function hasNestedSections(sectionElement) {
        var children = sectionElement.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "section") {
                return true;
            }
        }
        return false;
    }
    
    // ==================================================
    // ФУНКЦИЯ: isSectionEmpty
    // ==================================================
    function isSectionEmpty(sectionElement) {
        var children = sectionElement.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            
            if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "title") {
                continue;
            }
            
            if (child.nodeType == 1 && child.nodeName == "P") {
                if (isEmptyLineElement(child)) {
                    continue;
                }
            }
            
            if (child.nodeType == 3) {
                var nodeText = child.nodeValue || "";
                if (re0.test(nodeText)) {
                    continue;
                }
            }
            
            return false;
        }
        
        return true;
    }
    
    // ==================================================
    // ФУНКЦИЯ: analyzeTitles
    // ==================================================
    function analyzeTitles(sections) {
        var typicalWords = [
            "книга", "том", "часть", "глава", "раздел", "приложение", "параграф", "задание",
            "book", "volume", "part", "chapter", "section", "appendix", "paragraph", "exercise"
        ];
        var wordFrequencies = {};
        var firstWordFrequencies = {};
        var arabic1Count = 0;
        var arabic2Count = 0;
        var arabic3Count = 0;
        var romanCount = 0;
        
        for (var i = 0; i < sections.length; i++) {
            var titleText = getSectionTitleText(sections[i]);
            if (titleText === "") continue;
            
            var lowerTitle = toLowerCaseStr(titleText);
            
            var arabicLevel = getArabicLevel(titleText);
            if (arabicLevel == 1) {
                arabic1Count++;
                continue;
            }
            if (arabicLevel == 2) {
                arabic2Count++;
                continue;
            }
            if (arabicLevel == 3) {
                arabic3Count++;
                continue;
            }
            if (isRomanNumber(titleText)) {
                romanCount++;
                continue;
            }
            
            var foundTypical = false;
            for (var w = 0; w < typicalWords.length; w++) {
                var word = typicalWords[w];
                var lowerWord = toLowerCaseStr(word);
                
                if (startsWith(lowerTitle, lowerWord) || lowerTitle.indexOf(lowerWord) >= 0) {
                    if (typeof wordFrequencies[word] == 'undefined') {
                        wordFrequencies[word] = 0;
                    }
                    wordFrequencies[word]++;
                    foundTypical = true;
                    break;
                }
            }
            
            if (!foundTypical) {
                var firstWord = getFirstWord(titleText);
                if (firstWord !== "") {
                    if (typeof firstWordFrequencies[firstWord] == 'undefined') {
                        firstWordFrequencies[firstWord] = 0;
                    }
                    firstWordFrequencies[firstWord]++;
                }
            }
        }
        
        var allCandidates = [];
        
        for (var key in wordFrequencies) {
            if (typeof wordFrequencies[key] == 'number' && wordFrequencies[key] > 0) {
                allCandidates.push({word: key, count: wordFrequencies[key]});
            }
        }
        
        for (var key2 in firstWordFrequencies) {
            if (typeof firstWordFrequencies[key2] == 'number' && firstWordFrequencies[key2] > 0) {
                allCandidates.push({word: key2, count: firstWordFrequencies[key2]});
            }
        }
        
        if (arabic1Count > 0) {
            allCandidates.push({word: "Арабские - 1 уровень", count: arabic1Count});
        }
        if (arabic2Count > 0) {
            allCandidates.push({word: "Арабские - 2 уровня", count: arabic2Count});
        }
        if (arabic3Count > 0) {
            allCandidates.push({word: "Арабские - 3 уровня", count: arabic3Count});
        }
        if (romanCount > 0) {
            allCandidates.push({word: "Римские", count: romanCount});
        }
        
        for (var s1 = 0; s1 < allCandidates.length - 1; s1++) {
            for (var s2 = 0; s2 < allCandidates.length - s1 - 1; s2++) {
                if (allCandidates[s2].count < allCandidates[s2 + 1].count) {
                    var temp = allCandidates[s2];
                    allCandidates[s2] = allCandidates[s2 + 1];
                    allCandidates[s2 + 1] = temp;
                }
            }
        }
        
        var parentCandidate = "";
        var childCandidate = "";
        
        if (allCandidates.length >= 2) {
            childCandidate = allCandidates[0].word;
            parentCandidate = allCandidates[1].word;
        } else if (allCandidates.length == 1) {
            childCandidate = allCandidates[0].word;
            parentCandidate = allCandidates[0].word;
        }
        
        return {
            parent: parentCandidate,
            child: childCandidate,
            counts: {
                "Книга": (wordFrequencies["книга"] || 0),
                "Том": (wordFrequencies["том"] || 0),
                "Часть": (wordFrequencies["часть"] || 0),
                "Глава": (wordFrequencies["глава"] || 0),
                "Раздел": (wordFrequencies["раздел"] || 0),
                "Приложение": (wordFrequencies["приложение"] || 0),
                "Параграф": (wordFrequencies["параграф"] || 0),
                "Задание": (wordFrequencies["задание"] || 0),
                "Book": (wordFrequencies["book"] || 0),
                "Volume": (wordFrequencies["volume"] || 0),
                "Part": (wordFrequencies["part"] || 0),
                "Chapter": (wordFrequencies["chapter"] || 0),
                "Section": (wordFrequencies["section"] || 0),
                "Appendix": (wordFrequencies["appendix"] || 0),
                "Paragraph": (wordFrequencies["paragraph"] || 0),
                "Exercise": (wordFrequencies["exercise"] || 0),
                "Арабские - 1 уровень": arabic1Count,
                "Арабские - 2 уровня": arabic2Count,
                "Арабские - 3 уровня": arabic3Count,
                "Римские": romanCount,
                "Событие": (firstWordFrequencies["Событие"] || 0)
            }
        };
    }
    
    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) {
        if (showStatistics) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Не найден контейнер fbw_body.");
        }
        return;
    }
    
    // Находим основной body (с пустым fbname="")
    var mainBody = null;
    var bodyElements = document.getElementsByTagName("DIV");
    for (var b = 0; b < bodyElements.length; b++) {
        if (bodyElements[b].className == "body") {
            var fbname = bodyElements[b].getAttribute("fbname") || "";
            if (fbname == "") {
                mainBody = bodyElements[b];
                break;
            }
        }
    }
    
    if (!mainBody) {
        if (showStatistics) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Не найден основной раздел документа.");
        }
        return;
    }
    
    // Собираем все секции из основного body
    var allSections = [];
    collectAllSections(mainBody, allSections);
    
    if (allSections.length == 0) {
        if (showStatistics) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Не найдено секций в документе.");
        }
        return;
    }
    
    // Анализируем заголовки для определения типов
    var analysis = analyzeTitles(allSections);
    
    // Открываем диалог настроек
    var dialogData = null;
    try {
        var dialogParams = {version: version, analysis: analysis};
        dialogData = window.showModalDialog(settingsFile, dialogParams, "dialogWidth:550px;dialogHeight:955px;center:yes;resizable:no;");
    } catch(e) {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "Не удалось открыть окно настроек.");
        return;
    }
    
    if (!dialogData) {
        return;
    }
    
    var parentPrefix = dialogData.parentPrefix;
    var childPrefix = dialogData.childPrefix;
    var includeBeforeNext = dialogData.includeBeforeNext;
    var removeEmptySections = dialogData.removeEmptySections;
    
    if (typeof removeEmptySections == 'undefined') {
        removeEmptySections = true; // По умолчанию - убираем
    }
    
    if (!parentPrefix || parentPrefix === '') {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "Не указан тип родительских разделов.");
        return;
    }
    
    if (!childPrefix || childPrefix === '') {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "Не указан тип вложенных разделов.");
        return;
    }
    
    if (parentPrefix.substring(0, 7) == 'CUSTOM:' && parentPrefix.substring(7).replace(/^\s+|\s+$/g, '') === '') {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "Укажите текст для своего варианта родительских разделов.");
        return;
    }
    
    if (childPrefix.substring(0, 7) == 'CUSTOM:' && childPrefix.substring(7).replace(/^\s+|\s+$/g, '') === '') {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "Укажите текст для своего варианта вложенных разделов.");
        return;
    }
    
    // Получаем секции верхнего уровня (прямые потомки mainBody)
    var topSections = [];
    var mainChildren = mainBody.childNodes;
    for (var c = 0; c < mainChildren.length; c++) {
        var child = mainChildren[c];
        if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "section") {
            topSections.push(child);
        }
    }
    
    if (topSections.length == 0) {
        if (showStatistics) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Не найдено секций верхнего уровня для обработки.");
        }
        return;
    }
    
    // Определяем индексы родительских секций (по типу заголовка)
    var parentIndices = [];
    for (var p = 0; p < topSections.length; p++) {
        var titleText = getSectionTitleText(topSections[p]);
        if (matchesTitleType(titleText, parentPrefix)) {
            parentIndices.push(p);
        }
    }
    
    // Если не нашли по типу - ищем секции с вложенными секциями
    if (parentIndices.length == 0) {
        for (var p2 = 0; p2 < topSections.length; p2++) {
            if (hasNestedSections(topSections[p2])) {
                parentIndices.push(p2);
            }
        }
    }
    
    if (parentIndices.length == 0) {
        if (showStatistics) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Не найдено разделов для обработки.");
        }
        return;
    }
    
    var lastParentIndex = parentIndices[parentIndices.length - 1];
    
    // ПРЕДВАРИТЕЛЬНЫЙ АНАЛИЗ - поиск подозрительных пустых секций
    var criticalWarnings = [];
    
    for (var pi = 0; pi < parentIndices.length; pi++) {
        var parentIndex = parentIndices[pi];
        
        var endIndex;
        if (pi + 1 < parentIndices.length) {
            endIndex = parentIndices[pi + 1];
        } else {
            endIndex = topSections.length;
        }
        
        for (var s = parentIndex + 1; s < endIndex; s++) {
            var section = topSections[s];
            var sectionTitle = getSectionTitleText(section);
            
            var isTypicalChild = matchesTitleType(sectionTitle, childPrefix);
            var isLastBeforeNextParent = (s == endIndex - 1);
            
            if (!isTypicalChild && !isLastBeforeNextParent) {
                if (isSectionEmpty(section)) {
                    criticalWarnings.push("Секция \"" + sectionTitle + "\" пустая и нетипичная");
                }
            }
        }
    }
    
    if (criticalWarnings.length > 0) {
        var critMsg = scriptName + "\nver. " + version + "\n\n";
        critMsg += "ВНИМАНИЕ! Обнаружены подозрительные секции:\n\n";
        for (var cw = 0; cw < criticalWarnings.length && cw < 10; cw++) {
            critMsg += "  \u2022 " + criticalWarnings[cw] + "\n";
        }
        if (criticalWarnings.length > 10) {
            critMsg += "  \u2022 ... и ещё " + (criticalWarnings.length - 10) + "\n";
        }
        critMsg += "\nПродолжить выполнение несмотря на подозрительные секции?";
        
        var userConfirmed = AskYesNo(critMsg);
        if (!userConfirmed) {
            return;
        }
    }
    
    window.external.BeginUndoUnit(document, scriptName);
    
    try { window.external.SetStatusBarText("Вложение разделов..."); } catch(e) {}
    
    var startTime = new Date();
    
    var parentCount = 0;
    var nestedCount = 0;
    var wrappedCount = 0;
    var removedEmptySectionsCount = 0;
    
    // ==================================================
    // ФУНКЦИЯ: wrapOrphanContent
    // ==================================================
    function wrapOrphanContent(parentSection) {
        var children = parentSection.childNodes;
        var orphanElements = [];
        var foundTitle = false;
        var foundSection = false;
        
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            
            if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "title") {
                foundTitle = true;
                continue;
            }
            
            if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "section") {
                foundSection = true;
                break;
            }
            
            if (foundTitle && !foundSection) {
                if (child.nodeType == 1) {
                    orphanElements.push(child);
                } else if (child.nodeType == 3) {
                    var text = child.nodeValue || "";
                    if (!re0.test(text)) {
                        orphanElements.push(child);
                    }
                }
            }
        }
        
        if (orphanElements.length == 0) {
            return 0;
        }
        
        var newSection = document.createElement("DIV");
        newSection.className = "section";
        
        for (var m = 0; m < orphanElements.length; m++) {
            newSection.appendChild(orphanElements[m]);
        }
        
        var firstSection = null;
        var childrenAfter = parentSection.childNodes;
        for (var j = 0; j < childrenAfter.length; j++) {
            var childAfter = childrenAfter[j];
            if (childAfter.nodeType == 1 && childAfter.nodeName == "DIV" && childAfter.className == "section") {
                firstSection = childAfter;
                break;
            }
        }
        
        if (firstSection) {
            parentSection.insertBefore(newSection, firstSection);
        } else {
            parentSection.appendChild(newSection);
        }
        
        return 1;
    }
    
    // ==================================================
    // ФУНКЦИЯ: removeEmptySectionsAtStart
    // ==================================================
    function removeEmptySectionsAtStart(parentSection) {
        var removed = 0;
        var children = parentSection.childNodes;
        var foundTitle = false;
        var sectionsToRemove = [];
        
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            
            if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "title") {
                foundTitle = true;
                continue;
            }
            
            if (!foundTitle) {
                continue;
            }
            
            if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "section") {
                var hasTitle = false;
                var sectionChildren = child.childNodes;
                var hasContent = false;
                
                for (var j = 0; j < sectionChildren.length; j++) {
                    var sectionChild = sectionChildren[j];
                    
                    if (sectionChild.nodeType == 1 && sectionChild.nodeName == "DIV" && sectionChild.className == "title") {
                        hasTitle = true;
                        break;
                    }
                    
                    if (sectionChild.nodeType == 1 && sectionChild.nodeName == "P") {
                        if (!isEmptyLineElement(sectionChild)) {
                            hasContent = true;
                            break;
                        }
                    } else if (sectionChild.nodeType == 1) {
                        hasContent = true;
                        break;
                    } else if (sectionChild.nodeType == 3) {
                        var text = sectionChild.nodeValue || "";
                        if (!re0.test(text)) {
                            hasContent = true;
                            break;
                        }
                    }
                }
                
                if (!hasTitle && !hasContent) {
                    sectionsToRemove.push(child);
                    continue;
                }
                
                if (hasTitle || hasContent) {
                    break;
                }
            }
        }
        
        for (var r = 0; r < sectionsToRemove.length; r++) {
            sectionsToRemove[r].removeNode(false);
            removed++;
        }
        
        return removed;
    }
    
    // ОСНОВНОЙ ЦИКЛ ОБРАБОТКИ РОДИТЕЛЬСКИХ СЕКЦИЙ
    for (var pi = 0; pi < parentIndices.length; pi++) {
        var parentIndex = parentIndices[pi];
        var parentSection = topSections[parentIndex];
        
        var endIndex;
        if (pi + 1 < parentIndices.length) {
            endIndex = parentIndices[pi + 1];
        } else {
            endIndex = topSections.length;
        }
        
        var firstNestIndex = -1;
        var lastNestIndex = -1;
        var hasTypicalInRange = false;
        
        for (var s = parentIndex + 1; s < endIndex; s++) {
            var section = topSections[s];
            var sectionTitle = getSectionTitleText(section);
            
            var isTypicalChild = matchesTitleType(sectionTitle, childPrefix);
            var isLastBeforeNextParent = (s == endIndex - 1);
            
            var shouldNest = false;
            
            if (isTypicalChild) {
                shouldNest = true;
                hasTypicalInRange = true;
            } else {
                if (!isLastBeforeNextParent) {
                    shouldNest = true;
                } else if (isLastBeforeNextParent && includeBeforeNext && parentIndex != lastParentIndex) {
                    shouldNest = true;
                }
            }
            
            if (shouldNest) {
                if (firstNestIndex == -1) firstNestIndex = s;
                lastNestIndex = s;
            }
        }
        
        if (firstNestIndex != -1 && hasTypicalInRange) {
            parentCount++;
            wrappedCount += wrapOrphanContent(parentSection);
            
            var firstElement = parentSection.nextSibling;
            var lastElement = topSections[lastNestIndex];
            
            var elementsToMove = [];
            var currentElement = firstElement;
            
            while (currentElement) {
                elementsToMove.push(currentElement);
                
                if (currentElement === lastElement) {
                    break;
                }
                
                currentElement = currentElement.nextSibling;
            }
            
            for (var m = 0; m < elementsToMove.length; m++) {
                parentSection.appendChild(elementsToMove[m]);
                
                if (elementsToMove[m].nodeType == 1 && elementsToMove[m].nodeName == "DIV" && elementsToMove[m].className == "section") {
                    nestedCount++;
                }
            }
            
            removeEmptyLinesInsideParent(parentSection);
            
            if (removeEmptySections) {
                removedEmptySectionsCount += removeEmptySectionsAtStart(parentSection);
            }
        }
    }
    
    // ==================================================
    // ЧИСТКА ПУСТЫХ СТРОК (алгоритм из скрипта Sclex, только чистка)
    // ==================================================
    if (removeEmptySections) {
        var EmptyCleared = 0;
        var EmptyClearedSectionBegin = 0;
        var while_flag;
        
        function isLineEmpty(ptr) {
            return re0.test(ptr.innerHTML.replace(/<(?!img)[^>]*?>/gi, ""));
        }
        
        function Recursive(ptr) {
            function removeEmptiesAtBegin(elemName) {
                var go_more = true;
                var a4 = savedFirstEmpty;
                var SaveNextA4;
                if (elemName == "section" && a4 && a4.previousSibling && a4.previousSibling.className == "image") return;
                while (a4 && go_more) {
                    SaveNextA4 = a4.nextSibling;
                    if (a4.nodeName == "P" &&
                        isLineEmpty(a4) && a4.parentNode != null) {
                        a4.outerHTML = "";
                        EmptyCleared++;
                        EmptyClearedSectionBegin++;
                    } else {
                        go_more = false;
                    }
                    a4 = SaveNextA4;
                }
            }
            
            var savedPtr = ptr;
            if (ptr == null) return;
            var a5 = ptr.parentNode;
            var flag_of_begin = true;
            var firstEmptyMemorized = false;
            var image_flag = false;
            var image_flag_2 = false;
            var savedFirstEmpty = null;
            
            while (ptr != null) {
                var SaveNextPtr = ptr.nextSibling;
                
                if (ptr.nodeName == "DIV" &&
                    (ptr.className == "section" || ptr.className == "body" || ptr.className == "poem" ||
                     ptr.className == "stanza" || ptr.className == "cite" || ptr.className == "epigraph")) {
                    Recursive(ptr.firstChild);
                }
                
                if (ptr.nodeName == "DIV" && ptr.className == "image") {
                    if (flag_of_begin && !image_flag) {
                        flag_of_begin = false;
                        image_flag = true;
                        image_flag_2 = true;
                    } else if (image_flag) image_flag = false;
                }
                
                if (ptr.nodeName == "P" && ptr.parentNode.className == "section") {
                    if (!isLineEmpty(ptr)) {
                        flag_of_begin = false;
                        image_flag = false;
                    } else if (firstEmptyMemorized == false && (flag_of_begin || image_flag)) {
                        firstEmptyMemorized = true;
                        savedFirstEmpty = ptr;
                        flag_of_begin = false;
                    }
                }
                
                if (!firstEmptyMemorized && flag_of_begin && ptr.nodeName == "P" &&
                    (ptr.parentNode.className == "epigraph" || ptr.parentNode.className == "poem" ||
                     ptr.parentNode.className == "cite")) {
                    if (isLineEmpty(ptr)) {
                        firstEmptyMemorized = true;
                        savedFirstEmpty = ptr;
                    }
                    else flag_of_begin = false;
                }
                
                if (ptr.nodeName == "DIV" &&
                    (ptr.className == "table" || ptr.className == "cite" || ptr.className == "poem"))
                    flag_of_begin = false;
                
                if ((ptr.nodeName == "DIV" &&
                     (ptr.className == "poem" || ptr.className == "cite")) ||
                    (ptr.nodeName == "P" && isLineEmpty(ptr))) {
                    var a1 = ptr.previousSibling;
                    var flag = true;
                    while (a1 != null && flag) {
                        var SavePrev = a1.previousSibling;
                        if (a1.nodeName == "P" && isLineEmpty(a1)) {
                            a1.outerHTML = "";
                            EmptyCleared++;
                        } else flag = false;
                        a1 = SavePrev;
                    }
                    var flag = true;
                    var a2 = ptr.nextSibling;
                    while (a2 != null && flag) {
                        var SaveNext = a2.nextSibling;
                        if (a2.nodeName == "P" && isLineEmpty(a2)) {
                            SaveNextPtr = a2.nextSibling;
                            a2.outerHTML = "";
                            EmptyCleared++;
                        } else flag = false;
                        a2 = SaveNext;
                    }
                    if (ptr.parentNode.nodeName == "DIV" && ptr.parentNode.className == "stanza") {
                        ptr.outerHTML = "";
                        EmptyCleared++;
                    }
                }
                
                ptr = SaveNextPtr;
            }
            
            if (savedPtr.parentNode && savedPtr.parentNode.nodeName == "DIV" && savedPtr.parentNode.className == "section") {
                var a3 = a5.lastChild;
                if (firstEmptyMemorized && savedFirstEmpty.nextSibling)
                    if (image_flag_2 ?
                        savedFirstEmpty.nextSibling.className != "image" :
                        !(savedFirstEmpty.nextSibling.className == "image" && savedFirstEmpty.nextSibling.nextSibling &&
                          savedFirstEmpty.nextSibling.nextSibling.className == "image"
                         ) && !(savedFirstEmpty.nextSibling.className == "image" && !savedFirstEmpty.nextSibling.nextSibling)
                       )
                        removeEmptiesAtBegin("section");
            }
        }
        
        Recursive(mainBody.firstChild);
        removedEmptySectionsCount += EmptyCleared;
    }
    
    var endTime = new Date();
    var timeDiff = (endTime - startTime) / 1000;
    var timeStr = timeDiff.toFixed(3);
    timeStr = timeStr.replace(".", ",");
    
    // Функция формирования отчета
    function buildReportMsg() {
        var msg = scriptName + "\n";
        msg += "ver. " + version + "\n";
        msg += "---------------------------------------\n\n";
        
        if (parentCount > 0) {
            msg += "\u221A Создано родительских разделов: " + parentCount + "\n";
        }
        
        if (nestedCount > 0) {
            msg += "\u221A Вложено разделов: " + nestedCount + "\n";
        } else {
            msg += "\u221A Не найдено разделов для вложения.\n";
        }
        
        if (wrappedCount > 0) {
            msg += "\u221A Обрамлено в безымянные секции: " + wrappedCount + "\n";
        }
        
        if (removedEmptySectionsCount > 0) {
            msg += "\u221A Удалено пустых строк и секций: " + removedEmptySectionsCount + "\n";
        }
        
        if (criticalWarnings.length > 0) {
            msg += "\n---------------------------------------\n";
            msg += "ПОДОЗРИТЕЛЬНЫЕ СЕКЦИИ (пользователь подтвердил):\n";
            for (var cw = 0; cw < criticalWarnings.length && cw < 10; cw++) {
                msg += "  \u2022 " + criticalWarnings[cw] + "\n";
            }
        }
        
        msg += "\n---------------------------------------\n";
        msg += "Настройки обработки:\n";
        msg += "  \u2022 Родительские: \"" + parentPrefix + "\"\n";
        msg += "  \u2022 Вложенные: \"" + childPrefix + "\"\n";
        msg += "  \u2022 Вкладывать перед следующим родителем: " + (includeBeforeNext ? "\u221A (ДА)" : "\u2717 (НЕТ)") + "\n";
        msg += "  \u2022 Убирать пустые секции в начале: " + (removeEmptySections ? "\u221A (ДА)" : "\u2717 (НЕТ)") + "\n";
        
        msg += "\n---------------------------------------\n";
        msg += "Время выполнения: " + timeStr + " сек.";
        
        return msg;
    }
    
    if (showStatistics) {
        MsgBox(buildReportMsg());
    } else {
        if (parentCount == 0 || criticalWarnings.length > 0) {
            MsgBox(buildReportMsg());
        }
    }
    
    try { window.external.SetStatusBarText("ОК"); } catch(e) {}
    window.external.EndUndoUnit(document);
}