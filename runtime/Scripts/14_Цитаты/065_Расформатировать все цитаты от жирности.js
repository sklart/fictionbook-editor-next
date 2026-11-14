// Скрипт "Расформатировать все цитаты от жирности" для редактора FBE
// version 1.6
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для расформатирования от тэгов жирности (STRONG)
// сразу всех оформленных цитат (cite) в fb2 документах.
// По умолчанию обрабатывается сразу весь документ, включая разделы сносок и комментариев.
// Скрипт анализирует абзацы внутри цитат и классифицирует их по трём типам:
// - Полностью жирные — весь абзац внутри тегов жирности.
// - Начинающиеся с жирности — первый значимый текст в абзаце выделен жирностью.
// - С жирностью внутри — жирность встречается не в начале абзаца.
// Настройки скрипта позволяют выбирать, какие типы абзацев обрабатывать.
// При обработке удаляются ТОЛЬКО теги жирности, остальное форматирование 
// (курсив, зачёркнутость, индексы и т.д.) сохраняется в полном объёме.
// По умолчанию расформатируются только полностью жирные абзацы в цитатах.
// Отображается подробная статистика по обработанным абзацам.
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.6, 04.03.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Расформатировать все цитаты от жирности";
    var version = "1.6";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка режима отображения:
    // 0 - не показывать ничего (только ошибки)
    // 1 - показывать анализ и статистику
    // 2 - показывать только статистику в конце
    var showStatistics = 1;
    
    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 1;     // 0 - нет, 1 - да
    
    // Обрабатывать раздел комментариев
    var processCommentsSection = 1;     // 0 - нет, 1 - да
    
    // Расформатировать в цитатах ПОЛНОСТЬЮ жирные абзацы (весь текст внутри STRONG)
    var processFullBold = 1;     // 0 - нет, 1 - да
    
    // Расформатировать в цитатах абзацы, НАЧИНАЮЩИЕСЯ с жирности, но не полностью жирные
    var processStartBold = 0;     // 0 - нет, 1 - да
    
    // Расформатировать в цитатах абзацы, СОДЕРЖАЩИЕ жирность ВНУТРИ, но не подходящие под первые два типа
    var processPartialBold = 0;     // 0 - нет, 1 - да
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    // Получаем неразрывный пробел из настроек FBE
    var nbspChar = " ";
    try {
        nbspChar = window.external.GetNBSP();
    } catch(e) {
        nbspChar = String.fromCharCode(160);
    }
    
    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================
    
    // Функция рекурсивного поиска элементов с классом "cite"
    function findCiteElements(root, resultArray) {
        if (!root || !resultArray) return;
        
        if (root.nodeType == 1) {
            var className = root.className || "";
            className = className.toLowerCase ? className.toLowerCase() : className;
            
            if (className == "cite") {
                resultArray.push(root);
            }
            
            if (root.childNodes) {
                for (var i = 0; i < root.childNodes.length; i++) {
                    findCiteElements(root.childNodes[i], resultArray);
                }
            }
        }
    }
    
    // Функция для получения всех абзацев (P) внутри цитаты
    function getParagraphsInCite(citeElement) {
        var paragraphs = [];
        
        function findParagraphs(node) {
            if (!node) return;
            
            if (node.nodeType == 1) {
                var nodeName = node.nodeName || "";
                nodeName = nodeName.toUpperCase ? nodeName.toUpperCase() : nodeName;
                
                if (nodeName == "P") {
                    paragraphs.push(node);
                }
                
                if (node.childNodes) {
                    for (var i = 0; i < node.childNodes.length; i++) {
                        findParagraphs(node.childNodes[i]);
                    }
                }
            }
        }
        
        findParagraphs(citeElement);
        return paragraphs;
    }
    
    // Функция для проверки, находится ли текст внутри STRONG (рекурсивно)
    function isTextInsideStrong(node) {
        if (!node) return false;
        
        // Поднимаемся по родителям в поисках STRONG
        var current = node;
        while (current) {
            if (current.nodeType == 1) {
                var nodeName = current.nodeName || "";
                nodeName = nodeName.toUpperCase ? nodeName.toUpperCase() : nodeName;
                if (nodeName == "STRONG") {
                    return true;
                }
            }
            current = current.parentNode;
        }
        return false;
    }
    
    // Функция проверки, является ли абзац полностью жирным (весь текст внутри STRONG)
    function isParagraphFullBold(pElement) {
        if (!pElement) return false;
        
        // Собираем все текстовые узлы в абзаце
        var textNodes = [];
        
        function collectTextNodes(node) {
            if (!node) return;
            
            if (node.nodeType == 3) { // TEXT_NODE
                // Проверяем, что это не пустой текст
                if (node.nodeValue && node.nodeValue.replace(/\s/g, "").length > 0) {
                    textNodes.push(node);
                }
            } else if (node.nodeType == 1) {
                // Рекурсивно обходим детей
                if (node.childNodes) {
                    for (var i = 0; i < node.childNodes.length; i++) {
                        collectTextNodes(node.childNodes[i]);
                    }
                }
            }
        }
        
        collectTextNodes(pElement);
        
        // Если нет текста - считаем как не жирный
        if (textNodes.length == 0) return false;
        
        // Проверяем каждый текстовый узел
        for (var i = 0; i < textNodes.length; i++) {
            if (!isTextInsideStrong(textNodes[i])) {
                return false; // Нашелся текст не внутри STRONG
            }
        }
        
        return true; // Весь текст внутри STRONG
    }
    
    // Функция проверки, начинается ли абзац с жирности
    function isParagraphStartBold(pElement) {
        if (!pElement || !pElement.firstChild) return false;
        
        function findFirstTextNode(node) {
            if (!node) return null;
            
            if (node.nodeType == 3) { // TEXT_NODE
                if (node.nodeValue && node.nodeValue.replace(/\s/g, "").length > 0) {
                    return node;
                }
            } else if (node.nodeType == 1) {
                // Рекурсивно ищем первый текстовый узел
                if (node.childNodes) {
                    for (var i = 0; i < node.childNodes.length; i++) {
                        var found = findFirstTextNode(node.childNodes[i]);
                        if (found) return found;
                    }
                }
            }
            return null;
        }
        
        var firstTextNode = findFirstTextNode(pElement);
        if (!firstTextNode) return false;
        
        return isTextInsideStrong(firstTextNode);
    }
    
    // Функция проверки наличия жирности в абзаце
    function hasStrongInParagraph(pElement) {
        if (!pElement || !pElement.childNodes) return false;
        
        for (var i = 0; i < pElement.childNodes.length; i++) {
            var child = pElement.childNodes[i];
            
            if (child.nodeType == 1) {
                var nodeName = child.nodeName || "";
                nodeName = nodeName.toUpperCase ? nodeName.toUpperCase() : nodeName;
                
                if (nodeName == "STRONG") {
                    return true;
                }
                if (hasStrongInParagraph(child)) {
                    return true;
                }
            }
        }
        return false;
    }
    
    // Функция удаления всех тегов STRONG внутри элемента
    function removeStrongTags(element) {
        if (!element || !element.childNodes) return;
        
        for (var i = element.childNodes.length - 1; i >= 0; i--) {
            var child = element.childNodes[i];
            
            if (child.nodeType == 1) {
                var nodeName = child.nodeName || "";
                nodeName = nodeName.toUpperCase ? nodeName.toUpperCase() : nodeName;
                
                if (nodeName == "STRONG") {
                    while (child.firstChild) {
                        element.insertBefore(child.firstChild, child);
                    }
                    element.removeChild(child);
                } else {
                    removeStrongTags(child);
                }
            }
        }
    }
    
    // Функции проверки типов body
    function isMainBody(bodyElement) {
        var fbname = bodyElement.getAttribute("fbname") || "";
        return (fbname == "");
    }
    
    function isNotesBody(bodyElement) {
        var fbname = bodyElement.getAttribute("fbname") || "";
        return (fbname == "notes");
    }
    
    function isCommentsBody(bodyElement) {
        var fbname = bodyElement.getAttribute("fbname") || "";
        return (fbname == "comments");
    }
    
    // Функция для определения типа body в читаемом виде
    function getBodyTypeName(bodyElement) {
        var fbname = bodyElement.getAttribute("fbname") || "";
        if (fbname == "") return "основном разделе";
        if (fbname == "notes") return "разделе сносок (примечаний)";
        if (fbname == "comments") return "разделе комментариев";
        return "разделе: " + fbname;
    }
    
    // Функция для заголовка в статистике (с заглавной)
    function getBodyTypeNameCapital(bodyElement) {
        var fbname = bodyElement.getAttribute("fbname") || "";
        if (fbname == "") return "основном разделе";
        if (fbname == "notes") return "разделе сносок (примечаний)";
        if (fbname == "comments") return "разделе комментариев";
        return "Разделе: " + fbname;
    }
    
    // Функция форматирования времени
    function formatTime(seconds) {
        var str = seconds.toString();
        if (str.indexOf(".") == -1) {
            return str + ",000";
        } else {
            var parts = str.split(".");
            while (parts[1].length < 3) {
                parts[1] += "0";
            }
            if (parts[1].length > 3) {
                parts[1] = parts[1].substring(0, 3);
            }
            return parts[0] + "," + parts[1];
        }
    }
    
    // Функция для форматирования настроек в читаемый вид
    function getSettingsString() {
        var s = "Текущие настройки скрипта:\n";
        s += "  • Полностью жирные абзацы: " + (processFullBold ? "ДА" : "НЕТ") + "\n";
        s += "  • Начинающиеся с жирности абзацы: " + (processStartBold ? "ДА" : "НЕТ") + "\n";
        s += "  • Жирность внутри абзаца: " + (processPartialBold ? "ДА" : "НЕТ") + "\n";
        s += "\n";
        s += "  • Обработка раздела сносок (примечаний): " + (processNotesSection ? "ДА" : "НЕТ") + "\n";
        s += "  • Обработка раздела комментариев: " + (processCommentsSection ? "ДА" : "НЕТ");
        return s;
    }
    
    // Функция для формирования заголовка сообщения
    function getMsgHeader() {
        return scriptName + "\nver. " + version;
    }
    
    // ==================================================
    // ОСНОВНАЯ ЛОГИКА СКРИПТА
    // ==================================================
    
    // Массивы для сбора информации
    var allCiteElements = [];           // Все найденные цитаты
    var bodiesToProcess = [];            // Body-элементы для обработки
    
    // Собираем все DIV с классом "body"
    var allDivs = document.getElementsByTagName("DIV");
    for (var i = 0; i < allDivs.length; i++) {
        var div = allDivs[i];
        if (div.className == "body") {
            var fbname = div.getAttribute("fbname") || "";
            
            if (isMainBody(div)) {
                bodiesToProcess.push(div);
            } else if (processNotesSection && isNotesBody(div)) {
                bodiesToProcess.push(div);
            } else if (processCommentsSection && isCommentsBody(div)) {
                bodiesToProcess.push(div);
            }
        }
    }
    
    // Если не нашли ни одного body для обработки
    if (bodiesToProcess.length == 0) {
        if (showStatistics > 0) {
            MsgBox(getMsgHeader() + "\n\n" +
                   "✗ Не найдено разделов для обработки!");
        }
        return;
    }
    
    // Собираем все цитаты из выбранных body
    for (var b = 0; b < bodiesToProcess.length; b++) {
        findCiteElements(bodiesToProcess[b], allCiteElements);
    }
    
    // Если цитат нет вообще
    if (allCiteElements.length == 0) {
        if (showStatistics > 0) {
            MsgBox(getMsgHeader() + "\n\n" +
                   "✗ В документе нет ни одной цитаты (DIV class=cite)");
        }
        return;
    }
    
    // Собираем статистику по разделам и абзацам
    var statsByBody = [];
    var totalParagraphsWithBold = 0;
    var paragraphsFullCount = 0;
    var paragraphsStartCount = 0;
    var paragraphsPartialCount = 0;
    var paragraphsEligibleCount = 0; // Подходят под настройки
    var citesEligibleCount = 0;       // Цитаты, в которых есть абзацы подходящие под настройки
    
    // Инициализируем статистику для каждого body
    for (var b = 0; b < bodiesToProcess.length; b++) {
        statsByBody.push({
            body: bodiesToProcess[b],
            typeName: getBodyTypeName(bodiesToProcess[b]),
            typeNameCapital: getBodyTypeNameCapital(bodiesToProcess[b]),
            citesCount: 0,
            citesWithBoldCount: 0,
            paragraphsWithBoldCount: 0,
            citesEligibleCount: 0,
            paragraphsEligibleCount: 0
        });
    }
    
    // Проходим по всем цитатам и собираем статистику
    for (var i = 0; i < allCiteElements.length; i++) {
        var cite = allCiteElements[i];
        
        // Определяем, к какому body относится цитата
        var parentBody = null;
        var current = cite.parentNode;
        while (current) {
            if (current.nodeType == 1 && current.className == "body") {
                parentBody = current;
                break;
            }
            current = current.parentNode;
        }
        
        // Находим индекс body в statsByBody
        var bodyIndex = -1;
        for (var b = 0; b < statsByBody.length; b++) {
            if (statsByBody[b].body == parentBody) {
                bodyIndex = b;
                break;
            }
        }
        
        if (bodyIndex != -1) {
            statsByBody[bodyIndex].citesCount++;
        }
        
        var paragraphs = getParagraphsInCite(cite);
        var hasBoldInCite = false;
        var hasEligibleInCite = false;
        
        for (var j = 0; j < paragraphs.length; j++) {
            var p = paragraphs[j];
            
            if (hasStrongInParagraph(p)) {
                hasBoldInCite = true;
                totalParagraphsWithBold++;
                
                if (bodyIndex != -1) {
                    statsByBody[bodyIndex].paragraphsWithBoldCount++;
                }
                
                var isFull = isParagraphFullBold(p);
                var isStart = isParagraphStartBold(p);
                var isEligible = false;
                
                if (isFull) {
                    paragraphsFullCount++;
                    if (processFullBold) {
                        isEligible = true;
                        paragraphsEligibleCount++;
                    }
                } else if (isStart) {
                    paragraphsStartCount++;
                    if (processStartBold) {
                        isEligible = true;
                        paragraphsEligibleCount++;
                    }
                } else {
                    paragraphsPartialCount++;
                    if (processPartialBold) {
                        isEligible = true;
                        paragraphsEligibleCount++;
                    }
                }
                
                if (isEligible) {
                    hasEligibleInCite = true;
                    if (bodyIndex != -1) {
                        statsByBody[bodyIndex].paragraphsEligibleCount++;
                    }
                }
            }
        }
        
        if (hasBoldInCite && bodyIndex != -1) {
            statsByBody[bodyIndex].citesWithBoldCount++;
        }
        
        if (hasEligibleInCite) {
            citesEligibleCount++;
            if (bodyIndex != -1) {
                statsByBody[bodyIndex].citesEligibleCount++;
            }
        }
    }
    
    // Определяем, сколько цитат имеют жирность (для статистики)
    var citeElementsWithBold = [];
    for (var i = 0; i < allCiteElements.length; i++) {
        var cite = allCiteElements[i];
        var paragraphs = getParagraphsInCite(cite);
        var hasBold = false;
        
        for (var j = 0; j < paragraphs.length; j++) {
            if (hasStrongInParagraph(paragraphs[j])) {
                hasBold = true;
                break;
            }
        }
        
        if (hasBold) {
            citeElementsWithBold.push(cite);
        }
    }
    
    // Если нет цитат с жирностью
    if (citeElementsWithBold.length == 0) {
        if (showStatistics > 0) {
            var msg = getMsgHeader() + "\n\n" +
                      "Анализ документа:\n" +
                      "✓ Всего цитат: " + allCiteElements.length + "\n" +
                      "  • цитат с жирностью: 0\n\n" +
                      "Расформатирование не требуется.";
            MsgBox(msg);
        }
        return;
    }
    
    // Если нет абзацев, подходящих под настройки
    if (paragraphsEligibleCount == 0) {
        if (showStatistics > 0) {
            var msg = getMsgHeader() + "\n\n" +
                      "Анализ документа:\n" +
                      "✓ Всего цитат: " + allCiteElements.length + "\n" +
                      "  • цитат с жирностью: " + citeElementsWithBold.length + "\n" +
                      "  • абзацев с жирностью: " + totalParagraphsWithBold + "\n";
            
            // Добавляем статистику по разделам
            for (var b = 0; b < statsByBody.length; b++) {
                if (statsByBody[b].paragraphsWithBoldCount > 0) {
                    msg += "    В " + statsByBody[b].typeNameCapital + ": " + statsByBody[b].paragraphsWithBoldCount + "\n";
                }
            }
            
            msg += "\n  • из них по типам:\n" +
                   "    - полностью жирных: " + paragraphsFullCount + "\n" +
                   "    - начинаются с жирности: " + paragraphsStartCount + "\n" +
                   "    - частично жирных: " + paragraphsPartialCount + "\n\n" +
                   "  ---------------------------\n\n" +
                   getSettingsString() + "\n\n" +
                   "✗ Нет абзацев, подходящих под текущие настройки!\n" +
                   "Расформатирование не требуется.";
            MsgBox(msg);
        }
        return;
    }
    
    // Если нужно показать анализ (режим 1) - ТАЙМЕР ЗАПУСКАЕМ ПОСЛЕ ЭТОГО!
    if (showStatistics == 1) {
        var analysisMsg = getMsgHeader() + "\n\n" +
                          "Анализ документа:\n" +
                          "✓ Всего цитат: " + allCiteElements.length + "\n" +
                          "  • цитат с жирностью: " + citeElementsWithBold.length + "\n" +
                          "  • абзацев с жирностью: " + totalParagraphsWithBold + "\n";
        
        // Добавляем статистику по разделам
        for (var b = 0; b < statsByBody.length; b++) {
            if (statsByBody[b].paragraphsWithBoldCount > 0) {
                analysisMsg += "    В " + statsByBody[b].typeNameCapital + ": " + statsByBody[b].paragraphsWithBoldCount + "\n";
            }
        }
        
        analysisMsg += "\n  • из них по типам:\n" +
                       "    - полностью жирных: " + paragraphsFullCount + "\n" +
                       "    - начинаются с жирности: " + paragraphsStartCount + "\n" +
                       "    - частично жирных: " + paragraphsPartialCount + "\n\n" +
                       "  ---------------------------\n\n" +
                       getSettingsString() + "\n\n" +
                       "  • Цитат, подходящих для обработки: " + citesEligibleCount + "\n" +
                       "  • Абзацев, подходящих для обработки: " + paragraphsEligibleCount + "\n\n" +
                       "Будет выполнено расформатирование цитат от жирности.\n" +
                       "Продолжить?";
        
        if (!AskYesNo(analysisMsg)) {
            return;
        }
    }
    
    // ==================================================
    // ТАЙМЕР СТАРТУЕТ ЗДЕСЬ - ПОСЛЕ ВСЕХ ПОДТВЕРЖДЕНИЙ
    // ==================================================
    var startTime = new Date();
    
    // Начинаем группировку действий для отмены (Ctrl+Z)
    window.external.BeginUndoUnit(document, scriptName + " " + version);
    
    try {
        // ==================================================
        // ОБРАБОТКА (удаление STRONG из цитат С УЧЕТОМ НАСТРОЕК)
        // ==================================================
        
        var processedCiteCount = 0;
        var processedParagraphCount = 0;
        var processedByBody = [];
        var statsFull = 0;
        var statsStart = 0;
        var statsPartial = 0;
        
        // Инициализируем статистику обработки по разделам
        for (var b = 0; b < statsByBody.length; b++) {
            processedByBody.push({
                typeNameCapital: statsByBody[b].typeNameCapital,
                paragraphsProcessed: 0
            });
        }
        
        // Проходим по всем цитатам
        for (var i = 0; i < allCiteElements.length; i++) {
            var cite = allCiteElements[i];
            
            // Определяем, к какому body относится цитата
            var parentBody = null;
            var current = cite.parentNode;
            while (current) {
                if (current.nodeType == 1 && current.className == "body") {
                    parentBody = current;
                    break;
                }
                current = current.parentNode;
            }
            
            // Находим индекс body в processedByBody
            var bodyIndex = -1;
            for (var b = 0; b < statsByBody.length; b++) {
                if (statsByBody[b].body == parentBody) {
                    bodyIndex = b;
                    break;
                }
            }
            
            var paragraphs = getParagraphsInCite(cite);
            var citeChanged = false;
            
            // Анализируем каждый абзац в цитате
            for (var j = 0; j < paragraphs.length; j++) {
                var p = paragraphs[j];
                
                // Определяем тип жирности в абзаце
                var isFull = isParagraphFullBold(p);
                var isStart = isParagraphStartBold(p);
                var hasAny = hasStrongInParagraph(p);
                
                if (!hasAny) continue; // Пропускаем абзацы без жирности
                
                // Проверяем, нужно ли обрабатывать этот абзац по настройкам
                var shouldProcessThisParagraph = false;
                
                if (isFull && processFullBold) {
                    shouldProcessThisParagraph = true;
                    statsFull++;
                } else if (isStart && processStartBold && !isFull) {
                    shouldProcessThisParagraph = true;
                    statsStart++;
                } else if (hasAny && processPartialBold && !isFull && !isStart) {
                    shouldProcessThisParagraph = true;
                    statsPartial++;
                }
                
                // Если нужно обработать - удаляем STRONG из этого абзаца
                if (shouldProcessThisParagraph) {
                    removeStrongTags(p);
                    processedParagraphCount++;
                    citeChanged = true;
                    
                    if (bodyIndex != -1) {
                        processedByBody[bodyIndex].paragraphsProcessed++;
                    }
                }
            }
            
            // Если в цитате были изменения - увеличиваем счетчик цитат
            if (citeChanged) {
                processedCiteCount++;
            }
        }
        
        // ==================================================
        // ВЫВОД СТАТИСТИКИ
        // ==================================================
        
        // Вычисляем время выполнения
        var endTime = new Date();
        var timeDiff = (endTime - startTime) / 1000;
        var timeStr = formatTime(timeDiff);
        
        // Формируем сообщение в зависимости от режима
        if (showStatistics > 0) {
            var msg = getMsgHeader() + "\n";
            
            if (showStatistics == 1) {
                msg += "\n✓ Операция завершена\n";
            }
            
            msg += "---------------------------\n" +
                   "✓ Расформатировано цитат от жирности: " + processedCiteCount + "\n" +
                   "  • Обработано абзацев: " + processedParagraphCount + "\n";
            
            // Добавляем статистику по разделам
            var hasBodyStats = false;
            for (var b = 0; b < processedByBody.length; b++) {
                if (processedByBody[b].paragraphsProcessed > 0) {
                    if (!hasBodyStats) {
                        msg += "\n  Из них:\n";
                        hasBodyStats = true;
                    }
                    msg += "    В " + processedByBody[b].typeNameCapital + ": " + processedByBody[b].paragraphsProcessed + "\n";
                }
            }
            
            // Детализация по типам жирности
            if (statsFull > 0 || statsStart > 0 || statsPartial > 0) {
                msg += "\n  Детализация по типам жирности абзацев:\n";
                if (statsFull > 0) msg += "    • полностью жирных: " + statsFull + "\n";
                if (statsStart > 0) msg += "    • начинаются с жирности: " + statsStart + "\n";
                if (statsPartial > 0) msg += "    • частично жирных: " + statsPartial + "\n";
            }
            
            msg += "\n  ---------------------------\n\n";
            msg += getSettingsString() + "\n\n";
            msg += "Время обработки: " + timeStr + " сек";
            
            MsgBox(msg);
        }
        
    } catch(e) {
        MsgBox(getMsgHeader() + "\n\n" +
               "✗ Произошла ошибка:\n" + e.message + "\n\n" +
               "Строка: " + (e.lineNumber || "?"));
    }
    
    // Завершаем группировку действий для отмены
    window.external.EndUndoUnit(document);
}
