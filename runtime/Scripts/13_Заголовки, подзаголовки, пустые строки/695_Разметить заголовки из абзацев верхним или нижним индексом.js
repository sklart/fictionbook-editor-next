// Скрипт "Разметить заголовки из абзацев верхним или нижним индексом" для редактора FBE
// version 1.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Разметка заголовков сделана на основе алгоритма скрипта
// "Разметить заголовки разделов согласно оглавлению документа" уважаемого тов. Sclex.

// Скрипт предназначен для автоматической разметки заголовков разделов в fb2 документах.
// Скрипт находит абзацы, целиком выделенные верхним (<P><SUP>...</SUP></P>) 
// или нижним (<P><SUB>...</SUB></P>) индексом, длиной не более заданной (по умолч. 60 символов).
// Можно исключать абзацы внутри блочных элементов (аннотации, эпиграфы, стихи, цитаты, заголовки, подзаголовки),
// а также абзацы, начинающиеся с тире или дефисов (диалоги) или заканчивающиеся точкой.
// Скрипт преобразует найденные подходящие абзацы в заголовки разделов,
// создавая правильную структуру <DIV class="section"><DIV class="title"><P>...</P></DIV>...
// Между примыкающими заголовками вставляются пустые строки для валидности fb2 документа.
// Поддержка отмены действий (Ctrl+Z).

// version 1.2, 18.02.2026
//======================================

function Run() {

var scriptName = "Разметить заголовки из абзацев верхним или нижним индексом";
var version = "1.2";

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
var processBlockElements = 0;     // 0 - нет, 1 - да

    // Максимальная длина абзаца в символах (при превышении не считаем заголовком)
var maxLength = 60;

    // Обрабатывать абзацы, целиком оформленные верхним индексом (<SUP>)
var processSup = 1;     // 0 - нет, 1 - да

    // Обрабатывать абзацы, целиком оформленные нижним индексом (<SUB>)
var processSub = 1;     // 0 - нет, 1 - да

    // Размечать ли абзацы, похожие на диалоги (начинающиеся с тире/дефиса)
var processDialogs = 0;     // 0 - нет, 1 - да

    // Исключать ли абзацы, заканчивающиеся точкой
var excludeWithDot = 0;     // 0 - нет (обрабатываем любые), 1 - да (исключаем)

    // Удалять ли тэги верхнего индекса (<SUP>) после преобразования
var removeSupTags = 1;     // 0 - нет, 1 - да (по умолчанию удаляем)

    // Удалять ли тэги нижнего индекса (<SUB>) после преобразования
var removeSubTags = 1;     // 0 - нет, 1 - да (по умолчанию удаляем)

    // Вставлять пустые строки между примыкающими заголовками
    // (всегда должно быть да, иначе нарушается структура FB2)
var insertEmptyLines = 1;     // 0 - нет, 1 - да

    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================

    // Получаем неразрывный пробел из настроек FBE
try { 
    var nbspChar = window.external.GetNBSP(); 
    var nbspEntity; 
    if (nbspChar.charCodeAt(0) == 160) 
        nbspEntity = "&nbsp;"; 
    else 
        nbspEntity = nbspChar;
} catch(e) { 
    var nbspChar = String.fromCharCode(160); 
    var nbspEntity = "&nbsp;";
}

    // Нижеследующая команда задает список необычных пробелов,
    // которые должны обрабатываться наравне с обычными пробелами:
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

var unusualSpacesRegExp = new RegExp("[" + unusualSpaces + "]", "g");

    // Регулярка для проверки пустой строки
var emptyLineRegExp = new RegExp("^( | |&nbsp;|" + nbspChar + ")*?$", "i");

    // Функция проверки, является ли абзац пустым
function isLineEmpty(ptr) {
    if (!ptr || !ptr.innerHTML) return true;
    return emptyLineRegExp.test(ptr.innerHTML.replace(/<[^>]*?>/gi, ""));
}

    // Функция проверки, является ли абзац целиком верхним индексом
function isEntirelySup(elem) {
    if (!elem || isLineEmpty(elem)) return false;
    
    var myHtml = elem.innerHTML;
    var reSup = /<SUP>(((?!<\/?SUP>).)*?)<\/SUP>/ig;
    var reTags = /<\/?[^>]*>/ig;
    var reSpaces = new RegExp("^[\\s" + nbspChar + "&nbsp;]*$", "i");
    
        // Удаляем все вхождения <SUP>...</SUP>
    var searchResult = reSup.test(myHtml);
    while (searchResult) {
        myHtml = myHtml.replace(reSup, "");
        searchResult = reSup.test(myHtml);
    }
    
        // Удаляем все оставшиеся теги
    myHtml = myHtml.replace(reTags, "");
    
        // Если после удаления тегов остался только текст с пробелами - значит абзац был целиком в <SUP>
    if (myHtml == "" || reSpaces.test(myHtml)) 
        return true;
    
    return false;
}

    // Функция проверки, является ли абзац целиком нижним индексом
function isEntirelySub(elem) {
    if (!elem || isLineEmpty(elem)) return false;
    
    var myHtml = elem.innerHTML;
    var reSub = /<SUB>(((?!<\/?SUB>).)*?)<\/SUB>/ig;
    var reTags = /<\/?[^>]*>/ig;
    var reSpaces = new RegExp("^[\\s" + nbspChar + "&nbsp;]*$", "i");
    
        // Удаляем все вхождения <SUB>...</SUB>
    var searchResult = reSub.test(myHtml);
    while (searchResult) {
        myHtml = myHtml.replace(reSub, "");
        searchResult = reSub.test(myHtml);
    }
    
        // Удаляем все оставшиеся теги
    myHtml = myHtml.replace(reTags, "");
    
        // Если после удаления тегов остался только текст с пробелами - значит абзац был целиком в <SUB>
    if (myHtml == "" || reSpaces.test(myHtml)) 
        return true;
    
    return false;
}

    // Функция проверки, начинается ли абзац с тире/дефиса (диалог)
function isDialogStart(ptr) {
    if (!ptr || !ptr.innerText) return false;
    
    var text = ptr.innerText;
    if (text.length == 0) return false;
    
        // Проверяем первый символ на наличие тире или дефиса
    var firstChar = text.charAt(0);
        // Дефис, короткое тире, длинное тире
    if (firstChar == "-" || firstChar == "–" || firstChar == "—") 
        return true;
    
    return false;
}

    // Функция проверки длины абзаца (без учёта тегов)
function getTextLength(ptr) {
    if (!ptr || !ptr.innerText) return 0;
    return ptr.innerText.length;
}

    // Функция проверки окончания абзаца точкой
function endsWithDot(ptr) {
    if (!ptr || !ptr.innerText) return false;
    
    var text = ptr.innerText;
    if (text.length == 0) return false;
    
    var lastChar = text.charAt(text.length - 1);
    
    return (lastChar == ".");
}

    // Функция проверки, находится ли абзац внутри блочного элемента
function isInsideBlockElement(ptr) {
    if (!ptr) return false;
    
    var parent = ptr.parentNode;
    while (parent && parent.nodeName != "BODY") {
        if (parent.nodeName == "DIV") {
            var className = parent.className || "";
                // Список блочных элементов: poem, stanza, epigraph, cite, annotation, table, title, subtitle
            if (className == "poem" || className == "stanza" || className == "epigraph" || 
                className == "cite" || className == "annotation" || className == "table" || 
                className == "title" || className == "subtitle") {
                return true;
            }
        }
        parent = parent.parentNode;
    }
    return false;
}

    // Функция проверки, является ли body основным или служебным
function getBodyType(bodyElement) {
    var fbname = bodyElement.getAttribute("fbname") || "";
    if (fbname == "") return "main";
    if (fbname == "notes") return "notes";
    if (fbname == "comments") return "comments";
    return "other";
}

    // Функция удаления внешних тегов SUP из абзаца (с сохранением внутреннего форматирования)
function removeOuterSupTags(ptr) {
    if (!ptr) return;
    
    var html = ptr.innerHTML;
        // Удаляем только внешние открывающие и закрывающие теги SUP
    html = html.replace(/^<SUP>|<\/SUP>$/gi, "");
    ptr.innerHTML = html;
}

    // Функция удаления внешних тегов SUB из абзаца (с сохранением внутреннего форматирования)
function removeOuterSubTags(ptr) {
    if (!ptr) return;
    
    var html = ptr.innerHTML;
        // Удаляем только внешние открывающие и закрывающие теги SUB
    html = html.replace(/^<SUB>|<\/SUB>$/gi, "");
    ptr.innerHTML = html;
}

    // Функция для получения текстового описания режима отображения
function getDisplayModeText(mode) {
    if (mode == 0) return "тихий (только ошибки)";
    if (mode == 1) return "анализ и статистика";
    if (mode == 2) return "только статистика";
    return "неизвестный";
}

    // Функция генерации случайного числа для маркеров
function getRandomNum(n) {
    var s = "";
    for (var i = 1; i <= n; i++) {
        s += Math.floor(Math.random() * 10);
    }
    return s;
}

    // Функция проверки, является ли элемент заголовком (title)
function isTitleElement(elem) {
    if (!elem || elem.nodeName != "DIV") return false;
    return (elem.className == "title");
}

    // Функция проверки, является ли элемент секцией (section)
function isSectionElement(elem) {
    if (!elem || elem.nodeName != "DIV") return false;
    return (elem.className == "section");
}

    // Функция для создания пустого абзаца
function createEmptyParagraph() {
    var p = document.createElement("P");
    p.innerHTML = nbspChar;  // неразрывный пробел как содержимое
    return p;
}

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    // Получаем тело документа
var fbwBody = document.getElementById("fbw_body");
if (!fbwBody) {
    MsgBox("Ошибка: не найден элемент fbw_body", "FBE скрипт\n" + scriptName + "\nver. " + version);
    return;
}

    // Массив для сбора потенциальных заголовков
var potentialTitles = [];

    // Статистика
var stats = {
    totalSup: 0,                // всего целиком верхним индексом
    totalSub: 0,                // всего целиком нижним индексом
    totalFound: 0,               // подходящих под все критерии
    mainBody: 0,
    notesSection: 0,
    commentsSection: 0,
    otherSection: 0,
    excludedByLength: 0,
    excludedByDot: 0,
    excludedByDialog: 0,
    excludedByBlock: 0,
    converted: 0,
    emptyLinesInserted: 0
};

    // ==================================================
    // ФАЗА 1: СБОР ДАННЫХ (только чтение)
    // ==================================================

    // Получаем все DIV'ы с классом body
var bodies = fbwBody.getElementsByTagName("DIV");
for (var b = 0; b < bodies.length; b++) {
    var body = bodies[b];
    if (body.className != "body") continue;
    
    var bodyType = getBodyType(body);
    
        // Проверяем, нужно ли обрабатывать этот тип body
    if (bodyType == "notes" && !processNotesSection) {
        continue;
    }
    if (bodyType == "comments" && !processCommentsSection) {
        continue;
    }
    
        // Получаем все абзацы внутри этого body
    var paragraphs = body.getElementsByTagName("P");
    
    for (var i = 0; i < paragraphs.length; i++) {
        var p = paragraphs[i];
        
            // Проверка: абзац не должен быть пустым
        if (isLineEmpty(p)) continue;
        
        var isSup = false;
        var isSub = false;
        var indexType = "";     // "sup" или "sub" для статистики
        
            // Проверка на верхний индекс (если включено)
        if (processSup && isEntirelySup(p)) {
            isSup = true;
            indexType = "sup";
            stats.totalSup++;
        }
        
            // Проверка на нижний индекс (если включено и ещё не определились)
        if (!isSup && processSub && isEntirelySub(p)) {
            isSub = true;
            indexType = "sub";
            stats.totalSub++;
        }
        
            // Если ни один индекс не подошёл - пропускаем
        if (!isSup && !isSub) continue;
        
            // Проверка на диалоги (если отключено)
        if (!processDialogs && isDialogStart(p)) {
            stats.excludedByDialog++;
            continue;
        }
        
            // Проверка длины
        var len = getTextLength(p);
        if (len > maxLength) {
            stats.excludedByLength++;
            continue;
        }
        
            // Проверка окончания точкой (если исключение включено)
        if (excludeWithDot && endsWithDot(p)) {
            stats.excludedByDot++;
            continue;
        }
        
            // Проверка на нахождение внутри блочных элементов (если отключено)
        if (!processBlockElements && isInsideBlockElement(p)) {
            stats.excludedByBlock++;
            continue;
        }
        
            // Все проверки пройдены - добавляем в список потенциальных заголовков
        potentialTitles.push({
            element: p,
            bodyType: bodyType,
            indexType: indexType,     // запоминаем, какой это был индекс (для удаления тегов)
            originalHTML: p.innerHTML   // сохраняем исходный HTML
        });
        stats.totalFound++;
        
            // Считаем по разделам
        if (bodyType == "main") stats.mainBody++;
        else if (bodyType == "notes") stats.notesSection++;
        else if (bodyType == "comments") stats.commentsSection++;
        else stats.otherSection++;
    }
}

    // ==================================================
    // ФАЗА 2: ДИАЛОГ С ПОЛЬЗОВАТЕЛЕМ И ВЫВОД РЕЗУЛЬТАТОВ АНАЛИЗА
    // ==================================================

var userConfirmed = true;     // По умолчанию - да (для режимов 0 и 2)

    // Формируем базовое сообщение с настройками
function getSettingsText() {
    var settings = "";
    settings += "НАСТРОЙКИ СКРИПТА:\n";
    settings += "• Режим отображения: " + getDisplayModeText(showStatistics) + "\n";
    settings += "• Обработка раздела сносок: " + (processNotesSection ? "да" : "нет") + "\n";
    settings += "• Обработка раздела комментариев: " + (processCommentsSection ? "да" : "нет") + "\n";
    settings += "• Обработка блочных элементов: " + (processBlockElements ? "да" : "нет") + "\n";
    settings += "• Макс. длина абзаца: " + maxLength + " символов\n";
    settings += "• Обработка абзацев в верхнем индексе (<sup>): " + (processSup ? "да" : "нет") + "\n";
    settings += "• Обработка абзацев в нижнем индексе (<sub>): " + (processSub ? "да" : "нет") + "\n";
    settings += "• Обработка диалогов (с тире или дефисом): " + (processDialogs ? "да" : "нет") + "\n";
    settings += "• Исключать абзацы, заканчивающиеся точкой: " + (excludeWithDot ? "да" : "нет") + "\n";
    settings += "• Удалять внешний <sup> после преобразования: " + (removeSupTags ? "да" : "нет") + "\n";
    settings += "• Удалять внешний <sub> после преобразования: " + (removeSubTags ? "да" : "нет") + "\n";
    settings += "• Вставлять пустые строки между заголовками: " + (insertEmptyLines ? "да" : "нет") + "\n";
    return settings;
}

    // Если ничего не найдено - показываем сообщение в ЛЮБОМ режиме
if (stats.totalFound == 0) {
    var notFoundMsg = "";
    notFoundMsg += "---------------------------\n";
    notFoundMsg += scriptName + "\n";
    notFoundMsg += "ver. " + version + "\n";
    notFoundMsg += "---------------------------\n\n";
    
    notFoundMsg += "Подходящих для обработки абзацев не найдено.\n\n";
    
    notFoundMsg += getSettingsText();
    
    MsgBox(notFoundMsg, "FBE скрипт");
    
        // Запускаем таймер для статистики
    var startTime = new Date().getTime();
    var endTime = new Date().getTime();
    var timeDiff = (endTime - startTime) / 1000;
    var timeStr = timeDiff.toFixed(3).replace(".", ",");
    
        // В режимах 1 и 2 показываем ещё и время
    if (showStatistics == 1 || showStatistics == 2) {
        var timeMsg = "";
        timeMsg += "---------------------------\n";
        timeMsg += scriptName + "\n";
        timeMsg += "ver. " + version + "\n";
        timeMsg += "---------------------------\n\n";
        timeMsg += "Время обработки: " + timeStr + " сек";
        MsgBox(timeMsg, "FBE скрипт");
    }
    
    return;     // Завершаем работу скрипта
}

    // Если что-то найдено и режим 1 - показываем анализ и спрашиваем подтверждение
if (showStatistics == 1 && stats.totalFound > 0) {
    var msg = "";
    
    msg += "---------------------------\n";
    msg += scriptName + "\n";
    msg += "ver. " + version + "\n";
    msg += "---------------------------\n\n";
    
    msg += "РЕЗУЛЬТАТ АНАЛИЗА:\n";
    msg += "✓ Всего абзацев верхним индексом (<sup>): " + stats.totalSup + "\n";
    msg += "✓ Всего абзацев нижним индексом (<sub>): " + stats.totalSub + "\n";
    msg += "✓ Найдено потенциальных заголовков: " + stats.totalFound + "\n";
    msg += "  • в основном разделе: " + stats.mainBody + "\n";
    msg += "  • в сносках-примечаниях: " + stats.notesSection + (processNotesSection ? "" : " (исключены настройками)") + "\n";
    msg += "  • в комментариях: " + stats.commentsSection + (processCommentsSection ? "" : " (исключены настройками)") + "\n\n";
    
    if (stats.totalSup + stats.totalSub > 0) {
        msg += "ИСКЛЮЧЕНО ПО ПРИЧИНАМ:\n";
        msg += "  • слишком длинные (> " + maxLength + "): " + stats.excludedByLength + "\n";
        if (excludeWithDot) {
            msg += "  • заканчиваются точкой: " + stats.excludedByDot + "\n";
        }
        msg += "  • похожи на диалоги: " + stats.excludedByDialog + "\n";
        msg += "  • внутри блочных элементов: " + stats.excludedByBlock + "\n\n";
    }
    
    msg += getSettingsText() + "\n";
    
    msg += "Преобразовать найденные индексные абзацы в заголовки разделов?";
    
    userConfirmed = AskYesNo(msg);
}

    // ==================================================
    // ФАЗА 3: ПРЕОБРАЗОВАНИЕ (только если пользователь согласен)
    // ==================================================

    // Запускаем таймер ПОСЛЕ confirm
var startTime = new Date().getTime();

if (userConfirmed && potentialTitles.length > 0) {
    
        // Начинаем транзакцию для возможности отмены Ctrl+Z
    window.external.BeginUndoUnit(document, scriptName + " ver. " + version);
    
        // Генерируем случайное число для маркеров
    var randomNum = getRandomNum(6);
    var newTitleCnt = 0;
    
        // Сначала удаляем внешние теги индексов, если нужно
    for (var i = 0; i < potentialTitles.length; i++) {
        var item = potentialTitles[i];
        var p = item.element;
        
        if (item.indexType == "sup" && removeSupTags) {
            removeOuterSupTags(p);
        } else if (item.indexType == "sub" && removeSubTags) {
            removeOuterSubTags(p);
        }
    }
    
        // ВАЖНО: Определяем группы примыкающих заголовков (которые идут подряд в документе)
    var candidates = [];
    for (var i = 0; i < potentialTitles.length; i++) {
        candidates.push({
            element: potentialTitles[i].element,
            index: i
        });
    }
    
        // Группируем примыкающие заголовки
    var groups = [];
    var currentGroup = [];
    
    for (var i = 0; i < candidates.length; i++) {
        var currentElem = candidates[i].element;
        
        if (i == 0) {
            currentGroup.push(candidates[i]);
        } else {
            var prevElem = candidates[i-1].element;
            
                // Проверяем, являются ли элементы соседями в DOM
            var isAdjacent = false;
            var next = prevElem.nextSibling;
            
                // Пропускаем пустые текстовые узлы и другие не-P элементы
            while (next && next.nodeType != 1) {
                next = next.nextSibling;
            }
            
            if (next == currentElem) {
                isAdjacent = true;
            }
            
            if (isAdjacent) {
                currentGroup.push(candidates[i]);
            } else {
                if (currentGroup.length > 0) {
                    groups.push(currentGroup);
                }
                currentGroup = [candidates[i]];
            }
        }
    }
    
    if (currentGroup.length > 0) {
        groups.push(currentGroup);
    }
    
        // Теперь для каждой группы вставляем маркеры
    for (var g = 0; g < groups.length; g++) {
        var group = groups[g];
        
        if (group.length == 0) continue;
        
        newTitleCnt++;
        
            // Берем первый элемент группы для вставки begin маркера
        var firstElem = group[0].element;
        
            // Вставляем begin маркер перед первым элементом группы
        var beginMarker = document.createElement("P");
        beginMarker.innerHTML = "Sclex_SplittingIntoSections_" + randomNum + "_" + newTitleCnt + "_begin";
        firstElem.parentNode.insertBefore(beginMarker, firstElem);
        
            // Берем последний элемент группы для вставки end маркера
        var lastElem = group[group.length - 1].element;
        
            // Вставляем end маркер после последнего элемента группы
        var endMarker = document.createElement("P");
        endMarker.innerHTML = "Sclex_SplittingIntoSections_" + randomNum + "_" + newTitleCnt + "_end";
        
        var nextAfterLast = lastElem.nextSibling;
        if (nextAfterLast) {
            lastElem.parentNode.insertBefore(endMarker, nextAfterLast);
        } else {
            lastElem.parentNode.appendChild(endMarker);
        }
    }
    
        // ==================================================
        // ВАЖНО: Проверяем примыкание к существующим заголовкам
        // ==================================================
    
    if (insertEmptyLines) {
            // Проходим по всем группам и проверяем, что перед началом группы
            // и после конца группы нет других заголовков
        
        for (var g = 0; g < groups.length; g++) {
            var group = groups[g];
            
                // Получаем маркеры для этой группы (они уже вставлены)
                // Нам нужно проверить, что перед begin маркером нет заголовка,
                // и после end маркера нет заголовка
            
                // Ищем begin маркер (он перед первым элементом группы)
            var firstElem = group[0].element;
            var beginMarker = firstElem.previousSibling;
            while (beginMarker && beginMarker.nodeType != 1) {
                beginMarker = beginMarker.previousSibling;
            }
            
                // Проверяем, что перед begin маркером нет заголовка
            if (beginMarker) {
                var prev = beginMarker.previousSibling;
                while (prev && prev.nodeType != 1) {
                    prev = prev.previousSibling;
                }
                
                if (prev) {
                        // Проверяем, является ли предыдущий элемент заголовком
                        // Заголовок может быть либо <DIV class="title">, либо закрывающим тегом секции
                    var isPrevTitle = false;
                    
                    if (prev.nodeName == "DIV" && prev.className == "title") {
                        isPrevTitle = true;
                    } else if (prev.nodeName == "DIV" && prev.className == "section") {
                            // Проверяем, есть ли в этой секции заголовок
                        var titles = prev.getElementsByTagName("DIV");
                        for (var t = 0; t < titles.length; t++) {
                            if (titles[t].className == "title") {
                                isPrevTitle = true;
                                break;
                            }
                        }
                    }
                    
                    if (isPrevTitle) {
                            // Вставляем пустую строку между заголовками
                        var emptyP = createEmptyParagraph();
                        beginMarker.parentNode.insertBefore(emptyP, beginMarker);
                        stats.emptyLinesInserted++;
                    }
                }
            }
            
                // Аналогично проверяем после end маркера
            var lastElem = group[group.length - 1].element;
            var endMarker = lastElem.nextSibling;
            while (endMarker && endMarker.nodeType != 1) {
                endMarker = endMarker.nextSibling;
            }
            
            if (endMarker) {
                var next = endMarker.nextSibling;
                while (next && next.nodeType != 1) {
                    next = next.nextSibling;
                }
                
                if (next) {
                    var isNextTitle = false;
                    
                    if (next.nodeName == "DIV" && next.className == "title") {
                        isNextTitle = true;
                    } else if (next.nodeName == "DIV" && next.className == "section") {
                        var titles = next.getElementsByTagName("DIV");
                        for (var t = 0; t < titles.length; t++) {
                            if (titles[t].className == "title") {
                                isNextTitle = true;
                                break;
                            }
                        }
                    }
                    
                    if (isNextTitle) {
                            // Вставляем пустую строку после end маркера
                        var emptyP = createEmptyParagraph();
                        if (next.previousSibling) {
                            next.parentNode.insertBefore(emptyP, next);
                        } else {
                            next.parentNode.appendChild(emptyP);
                        }
                        stats.emptyLinesInserted++;
                    }
                }
            }
        }
    }
    
        // Теперь проходим по всем body и делаем глобальную замену через outerHTML
    for (var b = 0; b < bodies.length; b++) {
        var body = bodies[b];
        if (body.className != "body") continue;
        
        var bodyType = getBodyType(body);
        
            // Проверяем, нужно ли обрабатывать этот тип body
        if (bodyType == "notes" && !processNotesSection) {
            continue;
        }
        if (bodyType == "comments" && !processCommentsSection) {
            continue;
        }
        
            // Создаём регулярные выражения для замены
        var regExpBegin = new RegExp("<P(\\s[^>]*?)?>Sclex_SplittingIntoSections_" + randomNum + "_" + "(\\d+)_begin</P>", "g");
        var regExpEnd = new RegExp("<P(\\s[^>]*?)?>Sclex_SplittingIntoSections_" + randomNum + "_" + "(\\d+)_end</P>", "g");
        
            // Заменяем маркеры на структуру секций
            // begin маркер заменяем на закрытие предыдущей секции и открытие новой с заголовком
        var replacementBegin = "</DIV><DIV class=section><DIV class=title>";
            // end маркер заменяем на закрытие заголовка (оставляем открытой секцию для содержимого)
        var replacementEnd = "</DIV>";
        
            // Выполняем замену в HTML всего body
        var bodyHTML = body.outerHTML;
        bodyHTML = bodyHTML.replace(regExpBegin, replacementBegin);
        bodyHTML = bodyHTML.replace(regExpEnd, replacementEnd);
        
            // Дополнительная очистка: убираем пустые секции, которые могли образоваться
        var regExpEmptySection = new RegExp("</DIV><DIV class=section><DIV class=title></DIV><DIV class=section>", "g");
        bodyHTML = bodyHTML.replace(regExpEmptySection, "</DIV><DIV class=section>");
        
        body.outerHTML = bodyHTML;
    }
    
        // Обновляем статистику
    stats.converted = groups.length;    // количество созданных заголовков (групп)
    
        // Завершаем транзакцию
    window.external.EndUndoUnit(document);
}

    // ==================================================
    // ФАЗА 4: ВЫВОД РЕЗУЛЬТАТА (для режимов 1 и 2)
    // ==================================================

    // Вычисляем время выполнения
var endTime = new Date().getTime();
var timeDiff = (endTime - startTime) / 1000;     // в секундах
var timeStr = timeDiff.toFixed(3).replace(".", ",");

    // В режимах 1 и 2 показываем итоговую статистику
if ((showStatistics == 1 || showStatistics == 2) && stats.totalFound > 0) {
    var resultMsg = "";
    resultMsg += "---------------------------\n";
    resultMsg += scriptName + "\n";
    resultMsg += "ver. " + version + "\n";
    resultMsg += "---------------------------\n\n";
    
    if (stats.converted > 0) {
        resultMsg += "✓ Размечено заголовков разделов: " + stats.converted + "\n";
        resultMsg += "  (из " + potentialTitles.length + " индексных абзацев)\n";
        if (stats.emptyLinesInserted > 0) {
            resultMsg += "✓ Вставлено пустых строк-разделителей: " + stats.emptyLinesInserted + "\n";
        }
        resultMsg += "\n";
    } else if (potentialTitles.length > 0 && !userConfirmed) {
        resultMsg += "✓ Преобразование отменено пользователем\n\n";
    }
    
    resultMsg += "Время обработки: " + timeStr + " сек";
    
    MsgBox(resultMsg, "FBE скрипт");
}

}
