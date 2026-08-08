// Скрипт "Разметить подзаголовки из абзацев верхним или нижним индексом" для редактора FBE
// version 1.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для автоматической разметки подзаголовков в fb2 документах.
// Скрипт находит абзацы, целиком выделенные верхним (<P><SUP>...</SUP></P>) 
// или нижним (<P><SUB>...</SUB></P>) индексом, длиной не более заданной (по умолч. 60 символов).
// Можно исключить из обработки абзацы внутри уже размеченных элементов
// (аннотации, эпиграфы, заголовки, подзаголовки, стихи, цитаты, таблицы),
// а также абзацы, начинающиеся с тире или дефисов (диалоги) или заканчивающиеся точкой.
// Скрипт преобразует найденные подходящие абзацы в подзаголовки <P class="subtitle">,
// опционально удаляя внешние теги индексов.
// Режим работы: обычный, тихий или только статистика.
// Поддержка отмены действий (Ctrl+Z).

// version 1.2, 18.02.2026
//======================================

function Run() {

var scriptName = "Разметить подзаголовки из абзацев верхним или нижним индексом";
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

    // Максимальная длина абзаца в символах (при превышении не считаем подзаголовком)
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

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    // Получаем тело документа
var fbwBody = document.getElementById("fbw_body");
if (!fbwBody) {
    MsgBox("Ошибка: не найден элемент fbw_body", "FBE скрипт\n" + scriptName + "\nver. " + version);
    return;
}

    // Массив для сбора потенциальных подзаголовков
var potentialSubtitles = [];

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
    excludedByNotes: 0,
    excludedByComments: 0,
    converted: 0
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
        
            // Все проверки пройдены - добавляем в список потенциальных подзаголовков
        potentialSubtitles.push({
            element: p,
            bodyType: bodyType,
            indexType: indexType     // запоминаем, какой это был индекс (для удаления тегов)
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

    // Формируем базовое сообщение с настройками (пригодится для всех случаев)
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
    
        // Запускаем таймер для статистики (чтобы было корректное время 0.000 сек)
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
    msg += "✓ Найдено потенциальных подзаголовков: " + stats.totalFound + "\n";
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
    
    msg += "Преобразовать найденные индексные абзацы в подзаголовки?";
    
    userConfirmed = AskYesNo(msg);
}

    // ==================================================
    // ФАЗА 3: ПРЕОБРАЗОВАНИЕ (только если пользователь согласен)
    // ==================================================

    // Запускаем таймер ПОСЛЕ confirm
var startTime = new Date().getTime();

if (userConfirmed && potentialSubtitles.length > 0) {
        // Начинаем транзакцию для возможности отмены Ctrl+Z
    window.external.BeginUndoUnit(document, scriptName + " ver. " + version);
    
        // Обрабатываем найденные элементы (в прямом порядке, т.к. мы не удаляем, а изменяем)
    for (var i = 0; i < potentialSubtitles.length; i++) {
        var item = potentialSubtitles[i];
        var p = item.element;
        
            // Меняем класс на subtitle
        p.className = "subtitle";
        
            // Удаляем внешние теги индекса, если нужно
        if (item.indexType == "sup" && removeSupTags) {
            removeOuterSupTags(p);
        } else if (item.indexType == "sub" && removeSubTags) {
            removeOuterSubTags(p);
        }
        
        stats.converted++;
    }
    
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
        resultMsg += "✓ Размечено подзаголовков: " + stats.converted + "\n\n";
    } else if (potentialSubtitles.length > 0 && !userConfirmed) {
        resultMsg += "✓ Преобразование отменено пользователем\n\n";
    }
    
    resultMsg += "Время обработки: " + timeStr + " сек";
    
    MsgBox(resultMsg, "FBE скрипт");
}

}
