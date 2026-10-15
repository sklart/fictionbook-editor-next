/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Расформатировать от курсива абзацы, следующие сразу после заголовков",
  "version": "1.0",
  "authors": [
    {
      "name": "TaKir",
      "role": "author"
    },
    {
      "name": "DeepSeek",
      "role": "ai_assistance"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Снимает курсивное форматирование (EM/I) с выбранных по типу абзацев после заголовков, по умолчанию из первых двух.",
  "description": "Обходит заголовки DIV.title во включённых частях книги и собирает первые P после каждого из них; по умолчанию также заглядывает во вложенные блочные DIV и проверяет до двух абзацев. Классифицирует их как полностью оформленные курсивом, начинающиеся с неё или содержащие её только внутри. По умолчанию снимает курсивное форматирование с первых двух типов, полностью разворачивая теги EM/I внутри выбранного абзаца. Показывает анализ и просит подтверждение перед изменением; в конце показывает число выбранных абзацев и время. Сноски и комментарии по умолчанию исключены.",
  "scope": "Первые два P после каждого DIV.title в основном разделе, считая вложенные P соседних блочных DIV; число и типы P задаются параметрами в начале JS. Выделение и курсор не используются. Сноски и комментарии исключены по умолчанию.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "При необходимости измените в начале JS число проверяемых абзацев, типы форматирования, разделы книги и режим сообщений.",
    "Запустите команду; при исходном режиме 1 изучите предварительную статистику и подтвердите обработку.",
    "После выполнения проверьте абзацы после заголовков, особенно внутри соседних блочных DIV, и убедитесь, что нужное курсивное форматирование снято полностью."
  ],
  "warnings": [
    "При включённом поиске в блочных DIV берутся их первые P без остановки на вложенном заголовке; фактическая область может выйти за непосредственно следующие абзацы.",
    "Удаляются все теги EM/I внутри каждого выбранного абзаца, а не только начальное выделение.",
    "Режим сообщений 2 показывает предварительную и итоговую статистику без подтверждения; он не является режимом «только в конце»."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "698c72c927e70e0101500e109bd41258f64555e65fd74015358d2dd871250875",
    "reviewStatus": "generated"
  },
  "tags": [
    "Обработка форматирования",
    "параметры"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": ""
  },
  "settings": [
    {
      "name": "showStatistics",
      "value": 1,
      "expression": "1",
      "description": "0 — не показывать сообщения, кроме ошибок; 1 — показать анализ и запросить подтверждение; 2 — показать анализ без подтверждения, затем итог. Режим 2 всё равно показывает сообщение до обработки.",
      "title": "Показывать статистику"
    },
    {
      "name": "processNotesSection",
      "value": 0,
      "expression": "0",
      "description": "1 — включать раздел сносок; 0 — пропускать его (по умолчанию).",
      "title": "Обрабатывать примечания"
    },
    {
      "name": "processCommentsSection",
      "value": 0,
      "expression": "0",
      "description": "1 — включать раздел комментариев; 0 — пропускать его (по умолчанию).",
      "title": "Обрабатывать комментарии"
    },
    {
      "name": "processBlockElements",
      "value": 1,
      "expression": "1",
      "description": "1 — рассматривать первые P внутри блочных DIV после заголовка; 0 — рассматривать только соседние P.",
      "title": "Обрабатывать блочные элементы"
    },
    {
      "name": "processStartingItalic",
      "value": 1,
      "expression": "1",
      "description": "1 — снимать курсив в абзацах, где первый значимый текст выделен этим форматированием, но не весь абзац; 0 — пропускать.",
      "title": "Снимать курсив начала абзаца"
    },
    {
      "name": "processInnerItalic",
      "value": 0,
      "expression": "0",
      "description": "1 — снимать курсив, встречающийся после первого значимого текста; 0 — пропускать (по умолчанию).",
      "title": "Снимать курсив внутри абзаца"
    },
    {
      "name": "processFullItalic",
      "value": 1,
      "expression": "1",
      "description": "1 — снимать курсив с полностью форматированных абзацев; 0 — пропускать.",
      "title": "Обрабатывать курсивные абзацы полностью оформленные"
    },
    {
      "name": "paragraphsToCheck",
      "value": 2,
      "expression": "2",
      "description": "Максимум P после каждого заголовка для проверки; исходное значение 2.",
      "title": "Число абзацев после заголовка"
    }
  ]
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Расформатировать от курсива абзацы, следующие сразу после заголовков" для редактора FBE
// version 1.0
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для удаления тегов курсива (<EM>) в абзацах, 
// которые следуют непосредственно за заголовками (<DIV class="title">) в fb2 документах.
// Скрипт работает сразу со всем документом.
// По умолчанию обрабатывается только основной раздел документа, без сносок и комментариев.
// Скрипт анализирует абзацы и классифицирует их по трём типам:
// - Полностью курсивные — весь абзац внутри тегов курсива.
// - Начинающиеся с курсива — первый значимый текст в абзаце выделен курсивом.
// - С курсивом внутри — курсив встречается не в начале абзаца.
// Настройки скрипта позволяют выбирать, какие типы абзацев обрабатывать.
// Можно задавать произвольное кол-во абзацев после заголовков для удаления курсива.
// При обработке удаляются ТОЛЬКО теги курсива, остальное форматирование 
// (жирность, зачёркнутость, индексы и т.д.) сохраняется в полном объёме.
// Отображается подробная статистика по обработанным абзацам.
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.0, 05.03.2026
// ======================================

function Run() {
    var scriptName = "Расформатировать от курсива абзацы, следующие сразу после заголовков";
    var version = "1.0";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    // Настройка режима отображения:
    // 0 - не показывать ничего (только ошибки)
    // 1 - показывать анализ и статистику
    // 2 - показывать только статистику в конце
    var showStatistics = 1;

    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0;     // 0 - нет, 1 - да

    // Обрабатывать раздел комментариев
    var processCommentsSection = 0;     // 0 - нет, 1 - да

    // Обрабатывать уже размеченные "блочные" элементы
    // (poem, stanza, epigraph, cite, annotation, table, title, subtitle)
    var processBlockElements = 1;     // 0 - нет, 1 - да

    // --------------------------------------------------
    // Дополнительные настройки обработки
    // --------------------------------------------------

    // Расформатировать абзацы, начинающиеся с курсива, но не полностью курсивные
    var processStartingItalic = 1;      // 1 - да, 0 - нет

    // Расформатировать абзацы, НЕ начинающиеся с курсива (курсив внутри)
    var processInnerItalic = 0;         // 1 - да, 0 - нет

    // Расформатировать полностью курсивные абзацы после заголовков
    var processFullItalic = 1;          // 1 - да, 0 - нет

    // --------------------------------------------------
    // НАСТРОЙКА: количество абзацев после заголовка для проверки
    // --------------------------------------------------
    var paragraphsToCheck = 2;         // 1 - первый абзац, 2 - первые два и т.д.

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    // Получаем неразрывный пробел из настроек FBE
    var nbspEntity = "&nbsp;";
    var nbspChar = " ";
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

    // Список необычных пробелов для нормализации
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

    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================

    // Функция для получения типа раздела (основной, сноски, комментарии)
    function getSectionType(element) {
        var body = findParentBody(element);
        if (!body) return "main";
        
        var fbname = body.getAttribute("fbname") || "";
        if (fbname == "notes") return "notes";
        if (fbname == "comments") return "comments";
        return "main";
    }

    // Функция для проверки, нужно ли обрабатывать элемент
    function shouldProcessElement(element) {
        if (!element || element.nodeType != 1) return false;

        // Получаем body элемент и его тип
        var body = findParentBody(element);
        if (!body) return true; // Если не нашли body, обрабатываем

        var fbname = body.getAttribute("fbname") || "";

        // Проверяем разделы
        if (fbname == "notes" && !processNotesSection) return false;
        if (fbname == "comments" && !processCommentsSection) return false;

        return true;
    }

    // Функция для поиска родительского body
    function findParentBody(element) {
        while (element) {
            if (element.nodeName == "DIV" && element.className == "body") {
                return element;
            }
            element = element.parentNode;
        }
        return null;
    }

    // Функция для проверки, является ли элемент блочным
    function isBlockElement(element) {
        if (!element || element.nodeType != 1) return false;
        var blockClasses = ["poem", "stanza", "epigraph", "cite", "annotation", "table", "title", "subtitle"];
        for (var i = 0; i < blockClasses.length; i++) {
            if (element.className == blockClasses[i]) return true;
        }
        return false;
    }

    // Функция для проверки, является ли элемент заголовком
    function isTitleElement(element) {
        if (!element || element.nodeType != 1) return false;
        if (element.nodeName == "DIV" && element.className == "title") return true;
        return false;
    }

    // Функция для проверки, является ли тег тегом курсива
    function isItalicTag(node) {
        if (!node || node.nodeType != 1) return false;
        var tagName = node.nodeName.toUpperCase();
        return (tagName == "EM" || tagName == "I");
    }

    // Функция для проверки, есть ли в узле значимый текст (не только пробелы)
    function hasSignificantText(node) {
        if (!node) return false;
        if (node.nodeType == 3) { // Текстовый узел
            return node.nodeValue.replace(/\s/g, "").length > 0;
        }
        if (node.nodeType == 1) { // Элемент
            for (var i = 0; i < node.childNodes.length; i++) {
                if (hasSignificantText(node.childNodes[i])) {
                    return true;
                }
            }
        }
        return false;
    }

    // Функция для правильного определения типа курсива абзаца
    function getParagraphItalicType(pElement) {
        var hasItalic = false;
        var startsWithItalic = false;
        var isFullItalic = true;
        var scanningForFirstContent = true; // Ищем первый значимый контент

        // Вспомогательная рекурсивная функция для обхода узлов
        function scanNode(node) {
            if (node.nodeType == 3) { // Текстовый узел
                if (hasSignificantText(node)) {
                    // Нашли значимый текст. Если мы всё ещё ищем первый контент,
                    // значит, он не в теге курсива.
                    if (scanningForFirstContent) {
                        startsWithItalic = false;
                        scanningForFirstContent = false;
                    }
                }
            } else if (node.nodeType == 1) { // Элемент
                var isItalic = isItalicTag(node);
                if (isItalic) {
                    hasItalic = true;
                    // Если мы внутри курсива и всё ещё ищем первый контент, проверяем её детей
                    if (scanningForFirstContent) {
                        // Проверяем, есть ли значимый текст непосредственно в этом теге курсива
                        if (hasSignificantText(node)) {
                            startsWithItalic = true;
                            scanningForFirstContent = false;
                        }
                    }
                }

                // Рекурсивно проверяем дочерние узлы
                for (var i = 0; i < node.childNodes.length; i++) {
                    scanNode(node.childNodes[i]);
                }
            }
        }

        // Запускаем сканирование
        for (var i = 0; i < pElement.childNodes.length; i++) {
            scanNode(pElement.childNodes[i]);
        }

        // Если после всего обхода мы так и не нашли первый контент
        if (scanningForFirstContent) {
            startsWithItalic = false;
        }

        // Проверка на "полностью курсивный" (все значимые тексты внутри тегов курсива)
        if (hasItalic) {
            function checkFullItalic(node) {
                if (node.nodeType == 3) {
                    if (hasSignificantText(node)) {
                        // Текстовый узел не внутри курсива? Тогда не Full.
                        var parent = node.parentNode;
                        var foundItalicParent = false;
                        while (parent && parent != pElement) {
                            if (isItalicTag(parent)) {
                                foundItalicParent = true;
                                break;
                            }
                            parent = parent.parentNode;
                        }
                        if (!foundItalicParent) {
                            isFullItalic = false;
                        }
                    }
                } else if (node.nodeType == 1 && !isItalicTag(node)) {
                    for (var j = 0; j < node.childNodes.length; j++) {
                        checkFullItalic(node.childNodes[j]);
                    }
                }
            }
            for (var i = 0; i < pElement.childNodes.length; i++) {
                checkFullItalic(pElement.childNodes[i]);
            }
        } else {
            isFullItalic = false;
        }

        if (!hasItalic) return "none";
        if (isFullItalic) return "full";
        if (startsWithItalic) return "starting";
        return "inner";
    }

    // Функция для удаления тегов курсива из узла
    function removeItalicTags(node) {
        if (!node) return;

        // Проходим в обратном порядке, чтобы не сломать итерацию
        for (var i = node.childNodes.length - 1; i >= 0; i--) {
            var child = node.childNodes[i];

            if (child.nodeType == 1) {
                if (isItalicTag(child)) {
                    // Переносим всех детей тега курсива на его место
                    while (child.firstChild) {
                        var grandChild = child.removeChild(child.firstChild);
                        node.insertBefore(grandChild, child);
                    }
                    // Удаляем пустой тег
                    node.removeChild(child);
                } else {
                    // Рекурсивно обрабатываем дочерние элементы
                    removeItalicTags(child);
                }
            }
        }
    }

    // ==================================================
    // ОСНОВНАЯ ЛОГИКА СКРИПТА (СБОР ДАННЫХ)
    // ==================================================

    // Находим все заголовки
    var allTitles = [];
    var bodies = document.getElementsByTagName("DIV");

    for (var i = 0; i < bodies.length; i++) {
        if (bodies[i].className == "body") {
            if (shouldProcessElement(bodies[i])) {
                // Ищем заголовки внутри body
                var divs = bodies[i].getElementsByTagName("DIV");
                for (var j = 0; j < divs.length; j++) {
                    if (divs[j].className == "title") {
                        allTitles.push(divs[j]);
                    }
                }
            }
        }
    }

    // ==================================================
    // Собираем НЕСКОЛЬКО абзацев после каждого заголовка
    // ==================================================
    var targetParagraphs = [];

    for (var i = 0; i < allTitles.length; i++) {
        var title = allTitles[i];
        var paragraphsFound = 0;
        
        // Ищем абзацы после заголовка
        var next = title.nextSibling;
        while (next && paragraphsFound < paragraphsToCheck) {
            if (next.nodeType == 1) {
                if (next.nodeName == "P") {
                    targetParagraphs.push(next);
                    paragraphsFound++;
                } else if (next.nodeName == "DIV" && processBlockElements) {
                    // Если нашли блочный элемент и включена обработка блочных,
                    // проверяем его первые абзацы
                    var blockParagraphs = next.getElementsByTagName("P");
                    for (var k = 0; k < blockParagraphs.length && paragraphsFound < paragraphsToCheck; k++) {
                        targetParagraphs.push(blockParagraphs[k]);
                        paragraphsFound++;
                    }
                }
            }
            next = next.nextSibling;
        }
    }

    // Анализируем найденные абзацы
    var fullItalicCount = 0;
    var startingItalicCount = 0;
    var innerItalicCount = 0;
    var noneItalicCount = 0;
    
    // Статистика по разделам
    var mainSectionCount = 0;
    var notesSectionCount = 0;
    var commentsSectionCount = 0;
    
    var paragraphsToProcess = [];

    for (var i = 0; i < targetParagraphs.length; i++) {
        var p = targetParagraphs[i];
        var type = getParagraphItalicType(p);
        var sectionType = getSectionType(p);

        // Подсчёт по типам курсива
        switch(type) {
            case "full": fullItalicCount++; break;
            case "starting": startingItalicCount++; break;
            case "inner": innerItalicCount++; break;
            default: noneItalicCount++;
        }
        
        // Подсчёт по разделам
        switch(sectionType) {
            case "main": mainSectionCount++; break;
            case "notes": notesSectionCount++; break;
            case "comments": commentsSectionCount++; break;
        }

        // Отбор для обработки согласно настройкам
        var shouldProcess = false;
        switch(type) {
            case "full": shouldProcess = processFullItalic; break;
            case "starting": shouldProcess = processStartingItalic; break;
            case "inner": shouldProcess = processInnerItalic; break;
        }
        
        if (shouldProcess && shouldProcessElement(p)) {
            paragraphsToProcess.push(p);
        }
    }

    var totalItalic = fullItalicCount + startingItalicCount + innerItalicCount;

    // Формируем сообщение со статистикой
    var statsMessage = "";
    var header = scriptName + "\n" + "ver. " + version;

    if (totalItalic == 0) {
        statsMessage = "Не найдено абзацев с курсивом сразу после заголовков.";

        if (showStatistics >= 1) {
            MsgBox("---------------------------\n" +
                   header + "\n" +
                   "---------------------------\n\n" +
                   statsMessage + "\n\n" +
                   "Работа скрипта завершена.");
        }
        return;
    }

    // Формируем строку статистики по разделам
    var sectionsStats = "";
    if (mainSectionCount > 0) sectionsStats += "  • В основном разделе: " + mainSectionCount + "\n";
    if (notesSectionCount > 0) sectionsStats += "  • В разделе сносок: " + notesSectionCount + "\n";
    if (commentsSectionCount > 0) sectionsStats += "  • В разделе комментариев: " + commentsSectionCount + "\n";

    // Добавляем информацию о настройке количества абзацев
    var paragraphsSettingInfo = "  • Проверяется абзацев после заголовка: " + paragraphsToCheck;

    statsMessage = "Всего абзацев с курсивом сразу после заголовков: " + totalItalic + "\n" +
                   "Из них:\n" +
                   "  • Полностью курсивных: " + fullItalicCount + "\n" +
                   "  • Начинающихся с курсива: " + startingItalicCount + "\n" +
                   "  • Не начинающихся с курсива: " + innerItalicCount + "\n\n" +
                   "Текущие настройки скрипта:\n" +
                   "  • Полностью курсивные: " + (processFullItalic ? "ДА" : "НЕТ") + "\n" +
                   "  • Начинающиеся с курсива: " + (processStartingItalic ? "ДА" : "НЕТ") + "\n" +
                   "  • Курсив внутри: " + (processInnerItalic ? "ДА" : "НЕТ") + "\n" +
                   "  • Обработка раздела сносок: " + (processNotesSection ? "ДА" : "НЕТ") + "\n" +
                   "  • Обработка раздела комментариев: " + (processCommentsSection ? "ДА" : "НЕТ") + "\n" +
                   "  • Обработка блочных элементов (эпиграфы, цитаты, стихи и пр.): " + (processBlockElements ? "ДА" : "НЕТ") + "\n" +
                   paragraphsSettingInfo;

    if (paragraphsToProcess.length == 0) {
        if (showStatistics >= 1) {
            MsgBox("---------------------------\n" +
                   header + "\n" +
                   "---------------------------\n\n" +
                   statsMessage + "\n\n" +
                   "Подходящих абзацев (согласно настройкам) не найдено.\n\n" +
                   "Работа скрипта завершена.");
        }
        return;
    }

    // Добавляем информацию о том, сколько абзацев в каких разделах будет обработано
    var processMainCount = 0;
    var processNotesCount = 0;
    var processCommentsCount = 0;
    
    for (var i = 0; i < paragraphsToProcess.length; i++) {
        var sectionType = getSectionType(paragraphsToProcess[i]);
        switch(sectionType) {
            case "main": processMainCount++; break;
            case "notes": processNotesCount++; break;
            case "comments": processCommentsCount++; break;
        }
    }
    
    var processSectionsStats = "";
    if (processMainCount > 0) processSectionsStats += "  • В основном разделе: " + processMainCount + "\n";
    if (processNotesCount > 0) processSectionsStats += "  • В разделе сносок: " + processNotesCount + "\n";
    if (processCommentsCount > 0) processSectionsStats += "  • В разделе комментариев: " + processCommentsCount + "\n";

    // Запрашиваем подтверждение (только в режиме 1)
    if (showStatistics == 1) {
        var confirmMsg = statsMessage + "\n\n" +
                        "Будет расформатировано абзацев: " + paragraphsToProcess.length + ".\n" +
                        "Из них:\n" + processSectionsStats + "\n" +
                        "Продолжить?";

        if (!AskYesNo("---------------------------\n" + header + "\n---------------------------\n\n" + confirmMsg)) {
            return;
        }
    } else if (showStatistics == 2) {
        // Только статистика, без подтверждения
        if (showStatistics >= 1) {
            MsgBox("---------------------------\n" +
                   header + "\n" +
                   "---------------------------\n\n" +
                   statsMessage + "\n\n" +
                   "Будет расформатировано абзацев: " + paragraphsToProcess.length + ".\n" +
                   "Из них:\n" + processSectionsStats);
        }
    }

    // ==================================================
    // ОСНОВНАЯ ЛОГИКА СКРИПТА (ОБРАБОТКА)
    // ==================================================
    // ТАЙМЕР ЗАПУСКАЕМ ЗДЕСЬ, ПОСЛЕ ВСЕХ ДИАЛОГОВ
    var startTime = new Date();

    // Начинаем операцию для отмены Ctrl+Z
    window.external.BeginUndoUnit(document, scriptName + " " + version);

    // Обрабатываем абзацы (удаляем курсив)
    for (var i = 0; i < paragraphsToProcess.length; i++) {
        removeItalicTags(paragraphsToProcess[i]);
    }

    // Завершаем операцию для отмены
    window.external.EndUndoUnit(document);

    // Вычисляем время выполнения
    var endTime = new Date();
    var timeDiff = (endTime - startTime) / 1000; // в секундах
    var timeStr = timeDiff.toFixed(3).replace(".", ",");

    // Итоговая статистика
    var resultMessage = "✓ Операция завершена\n" +
                       "  • Расформатировано абзацев: " + paragraphsToProcess.length + "\n" +
                       "    - полностью курсивных: " + (processFullItalic ? fullItalicCount : 0) + "\n" +
                       "    - начинающихся с курсива: " + (processStartingItalic ? startingItalicCount : 0) + "\n" +
                       "    - с курсивом внутри: " + (processInnerItalic ? innerItalicCount : 0) + "\n\n" +
                       "  • Из них по разделам:\n" + processSectionsStats + "\n" +
                       "✓ Настройка: проверка " + paragraphsToCheck + " абз. после каждого заголовка\n\n" +
                       "✓ Время обработки: " + timeStr + " сек";

    if (showStatistics >= 1) {
        MsgBox("---------------------------\n" +
               header + "\n" +
               "---------------------------\n\n" +
               resultMessage);
    }
}

// ==================================================
// КОНЕЦ СКРИПТА
// ==================================================
