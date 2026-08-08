// Скрипт "Разметить подзаголовки из обычных абзацев без препинаний в конце" для редактора FBE
// version 1.5
// Идея - TaKir
// Реализация - DeepSeek, TaKir
  
// Скрипт предназначен для автоматической разметки подзаголовков в fb2 документах.
// Находит обычные короткие абзацы без тэгов жирности или курсива
// и не заканчивающиеся знаками препинания, длиной не более заданной.
// По умолчанию длина подходящего абзаца - 30 символов.
// Позволяет исключать абзацы внутри уже размеченных элементов (стихи, цитаты, эпиграфы),
// а также абзацы, начинающиеся с дефисов или тире (диалоги). 
// Учитывается наличие сносок в тексте.
// Скрипт преобразует найденные подходящие абзацы в <P class="subtitle">.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.5, 13.05.2025
//======================================

function Run() {

var scriptName = "Разметить подзаголовки из обычных абзацев без препинаний в конце";
var version = "1.5";

// ==================================================
// НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
// ==================================================

// Настройка режима отображения:
// 0 - не показывать ничего (только ошибки и сообщение об отсутствии результатов)
// 1 - показывать анализ и статистику
// 2 - показывать только статистику в конце
var showStatistics = 1;

// Обрабатывать раздел сносок (примечаний)
var processNotesSection = 0; // 0 - нет, 1 - да

// Обрабатывать раздел комментариев
var processCommentsSection = 0; // 0 - нет, 1 - да

// Обрабатывать уже размеченные "структурные" элементы
// (poem, stanza, epigraph, cite, annotation, table, title, subtitle)
var processBlockElements = 0; // 0 - нет, 1 - да

// Максимальная длина абзаца в символах (при превышении не считаем подзаголовком)
var maxLength = 30;

// Разрешённые знаки в конце абзаца (только квадратная скобка для сносок)
var allowedEndChars = "]";

// Размечать ли абзацы, похожие на диалоги (начинающиеся с тире/дефиса)
var processDialogs = 0; // 0 - нет, 1 - да

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

// Функция проверки, есть ли в абзаце теги STRONG или EM
function hasFormattingTags(elem) {
    if (!elem) return false;
    
    var html = elem.innerHTML;
    if (html.indexOf("<STRONG") != -1) return true;
    if (html.indexOf("</STRONG>") != -1) return true;
    if (html.indexOf("<EM") != -1) return true;
    if (html.indexOf("</EM>") != -1) return true;
    
    return false;
}

// Функция проверки, является ли абзац уже подзаголовком
function isAlreadySubtitle(elem) {
    if (!elem) return false;
    return (elem.className == "subtitle");
}

// Функция проверки, начинается ли абзац с тире/дефиса
function isDialogStart(ptr) {
    if (!ptr || !ptr.innerText) return false;
    
    var text = ptr.innerText;
    if (text.length == 0) return false;
    
    var firstChar = text.charAt(0);
    if (firstChar == "-" || firstChar == "–" || firstChar == "—") 
        return true;
    
    return false;
}

// Функция проверки длины абзаца
function getTextLength(ptr) {
    if (!ptr || !ptr.innerText) return 0;
    return ptr.innerText.length;
}

// Функция проверки окончания абзаца (не должно быть знаков препинания)
function endsWithoutPunctuation(ptr) {
    if (!ptr || !ptr.innerText) return false;
    
    var text = ptr.innerText;
    if (text.length == 0) return false;
    
    var lastChar = text.charAt(text.length - 1);
    
    // Список знаков препинания, которые не допускаются в конце
    var punctuationMarks = ".!?,:;-()[]{}«»\"'…";
    
    for (var i = 0; i < punctuationMarks.length; i++) {
        if (lastChar == punctuationMarks.charAt(i)) {
            // Если это квадратная скобка - проверяем, разрешена ли она
            if (lastChar == "]") {
                for (var j = 0; j < allowedEndChars.length; j++) {
                    if (lastChar == allowedEndChars.charAt(j)) 
                        return true;
                }
                return false;
            }
            return false; // Это знак препинания - не подходит
        }
    }
    
    // Если это не знак препинания (буква, цифра и т.д.) - подходит
    return true;
}

// Функция проверки, находится ли абзац внутри блочного элемента
function isInsideBlockElement(ptr) {
    if (!ptr) return false;
    
    var parent = ptr.parentNode;
    while (parent && parent.nodeName != "BODY") {
        if (parent.nodeName == "DIV") {
            var className = parent.className || "";
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

// ==================================================
// НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
// ==================================================

var fbwBody = document.getElementById("fbw_body");
if (!fbwBody) {
    MsgBox("Ошибка: не найден элемент fbw_body", "FBE скрипт\n" + scriptName + "\nver. " + version);
    return;
}

var potentialSubtitles = [];

var stats = {
    totalChecked: 0,
    totalFound: 0,
    mainBody: 0,
    notesSection: 0,
    commentsSection: 0,
    otherSection: 0,
    excludedByFormatting: 0,
    excludedByLength: 0,
    excludedByPunctuation: 0,
    excludedByDialog: 0,
    excludedByBlock: 0,
    excludedBySubtitle: 0,
    excludedByNotes: 0,
    excludedByComments: 0,
    converted: 0
};

// ==================================================
// ФАЗА 1: СБОР ДАННЫХ
// ==================================================

var bodies = fbwBody.getElementsByTagName("DIV");
for (var b = 0; b < bodies.length; b++) {
    var body = bodies[b];
    if (body.className != "body") continue;
    
    var bodyType = getBodyType(body);
    
    if (bodyType == "notes" && !processNotesSection) {
        continue;
    }
    if (bodyType == "comments" && !processCommentsSection) {
        continue;
    }
    
    var paragraphs = body.getElementsByTagName("P");
    
    for (var i = 0; i < paragraphs.length; i++) {
        var p = paragraphs[i];
        
        if (isLineEmpty(p)) continue;
        
        stats.totalChecked++;
        
        // Проверка: абзац не должен быть уже подзаголовком
        if (isAlreadySubtitle(p)) {
            stats.excludedBySubtitle++;
            continue;
        }
        
        // Проверка: в абзаце не должно быть STRONG и EM
        if (hasFormattingTags(p)) {
            stats.excludedByFormatting++;
            continue;
        }
        
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
        
        // Проверка окончания (не должно быть знаков препинания)
        if (!endsWithoutPunctuation(p)) {
            stats.excludedByPunctuation++;
            continue;
        }
        
        // Проверка на нахождение внутри блочных элементов
        if (!processBlockElements && isInsideBlockElement(p)) {
            stats.excludedByBlock++;
            continue;
        }
        
        potentialSubtitles.push({
            element: p,
            bodyType: bodyType
        });
        stats.totalFound++;
        
        if (bodyType == "main") stats.mainBody++;
        else if (bodyType == "notes") stats.notesSection++;
        else if (bodyType == "comments") stats.commentsSection++;
        else stats.otherSection++;
    }
}

// ==================================================
// ФАЗА 2: ДИАЛОГ С ПОЛЬЗОВАТЕЛЕМ И ВЫВОД РЕЗУЛЬТАТОВ
// ==================================================

// Запускаем таймер
var startTime = new Date().getTime();

// Функция для формирования сообщения с настройками
function getSettingsMessage() {
    var msg = "";
    msg += "НАСТРОЙКИ СКРИПТА:\n";
    msg += "• Отображение: ";
    if (showStatistics == 1) msg += "показывать анализ и статистику";
    else if (showStatistics == 2) msg += "только статистика";
    else msg += "выключен (только ошибки)";
    msg += "\n";
    msg += "• Обработка сносок: " + (processNotesSection ? "да" : "нет") + "\n";
    msg += "• Обработка комментариев: " + (processCommentsSection ? "да" : "нет") + "\n";
    msg += "• Обработка блочных элементов: " + (processBlockElements ? "да" : "нет") + "\n";
    msg += "• Макс. длина: " + maxLength + " символов\n";
    msg += "• Разрешённые знаки в конце: " + allowedEndChars + "\n";
    msg += "• Обработка диалогов: " + (processDialogs ? "да" : "нет") + "\n";
    return msg;
}

// Функция для формирования сообщения "ничего не найдено"
function getNotFoundMessage() {
    var endTime = new Date().getTime();
    var timeDiff = (endTime - startTime) / 1000;
    var timeStr = timeDiff.toFixed(3).replace(".", ",");
    
    var msg = "";
    msg += "---------------------------\n";
    msg += scriptName + "\n";
    msg += "ver. " + version + "\n";
    msg += "---------------------------\n\n";
    msg += "Подходящих для обработки абзацев не найдено.\n\n";
    msg += getSettingsMessage() + "\n";
    msg += "Время обработки: " + timeStr + " сек";
    return msg;
}

var userConfirmed = true;

// РЕЖИМ 1: показываем анализ и запрашиваем подтверждение
if (showStatistics == 1) {
    if (stats.totalFound == 0) {
        // Ничего не найдено - показываем сообщение с таймером
        MsgBox(getNotFoundMessage(), "FBE скрипт");
    } else {
        // Найдены кандидаты - показываем анализ и запрашиваем подтверждение
        var msg = "";
        msg += "---------------------------\n";
        msg += scriptName + "\n";
        msg += "ver. " + version + "\n";
        msg += "---------------------------\n\n";
        
        msg += "РЕЗУЛЬТАТ АНАЛИЗА:\n";
        msg += "✓ Всего проверено абзацев: " + stats.totalChecked + "\n";
        msg += "✓ Найдено потенциальных подзаголовков: " + stats.totalFound + "\n";
        msg += "  • в основном разделе: " + stats.mainBody + "\n";
        msg += "  • в сносках-примечаниях: " + stats.notesSection + (processNotesSection ? "" : " (исключены настройками)") + "\n";
        msg += "  • в комментариях: " + stats.commentsSection + (processCommentsSection ? "" : " (исключены настройками)") + "\n\n";
        
        if (stats.totalChecked > 0) {
            msg += "ИСКЛЮЧЕНО ПО ПРИЧИНАМ:\n";
            if (stats.excludedBySubtitle > 0) msg += "  • уже являются подзаголовками: " + stats.excludedBySubtitle + "\n";
            if (stats.excludedByFormatting > 0) msg += "  • содержат STRONG/EM: " + stats.excludedByFormatting + "\n";
            if (stats.excludedByLength > 0) msg += "  • слишком длинные (> " + maxLength + "): " + stats.excludedByLength + "\n";
            if (stats.excludedByPunctuation > 0) msg += "  • заканчиваются знаком препинания: " + stats.excludedByPunctuation + "\n";
            if (stats.excludedByDialog > 0) msg += "  • похожи на диалоги: " + stats.excludedByDialog + "\n";
            if (stats.excludedByBlock > 0) msg += "  • внутри блочных элементов: " + stats.excludedByBlock + "\n";
            msg += "\n";
        }
        
        msg += getSettingsMessage() + "\n\n";
        msg += "Преобразовать найденные абзацы в подзаголовки?";
        
        userConfirmed = AskYesNo(msg);
    }
}

// ==================================================
// ФАЗА 3: ПРЕОБРАЗОВАНИЕ
// ==================================================

if (userConfirmed && potentialSubtitles.length > 0) {
    window.external.BeginUndoUnit(document, scriptName + " ver. " + version);
    
    for (var i = 0; i < potentialSubtitles.length; i++) {
        var p = potentialSubtitles[i].element;
        
        // Меняем класс
        p.className = "subtitle";
        
        // Добавляем и сразу удаляем неразрывный пробел в конце, чтобы редактор заметил изменение
        var oldHtml = p.innerHTML;
        p.innerHTML = oldHtml + nbspChar;
        p.innerHTML = oldHtml;
        
        stats.converted++;
    }
    
    window.external.EndUndoUnit(document);
}

// ==================================================
// ФАЗА 4: ВЫВОД РЕЗУЛЬТАТА ПРЕОБРАЗОВАНИЯ
// ==================================================

var endTime = new Date().getTime();
var timeDiff = (endTime - startTime) / 1000;
var timeStr = timeDiff.toFixed(3).replace(".", ",");

// РЕЖИМ 1 и 2: показываем результат преобразования (если были кандидаты)
if ((showStatistics == 1 || showStatistics == 2) && potentialSubtitles.length > 0 && userConfirmed) {
    var resultMsg = "";
    resultMsg += "---------------------------\n";
    resultMsg += scriptName + "\n";
    resultMsg += "ver. " + version + "\n";
    resultMsg += "---------------------------\n\n";
    
    if (stats.converted > 0) {
        resultMsg += "✓ Создано подзаголовков: " + stats.converted + "\n\n";
    }
    
    resultMsg += "Время обработки: " + timeStr + " сек";
    
    MsgBox(resultMsg, "FBE скрипт");
}

// РЕЖИМ 2: если ничего не найдено - показываем сообщение
if (showStatistics == 2 && stats.totalFound == 0) {
    MsgBox(getNotFoundMessage(), "FBE скрипт");
}

// РЕЖИМ 0 (тихий): если ничего не найдено - показываем сообщение
if (showStatistics == 0 && stats.totalFound == 0) {
    MsgBox(getNotFoundMessage(), "FBE скрипт");
}

// Если преобразование отменено пользователем - показываем сообщение в режимах 1 и 2
if ((showStatistics == 1 || showStatistics == 2) && potentialSubtitles.length > 0 && !userConfirmed) {
    var cancelMsg = "";
    cancelMsg += "---------------------------\n";
    cancelMsg += scriptName + "\n";
    cancelMsg += "ver. " + version + "\n";
    cancelMsg += "---------------------------\n\n";
    cancelMsg += "✓ Преобразование отменено пользователем\n\n";
    cancelMsg += "Время обработки: " + timeStr + " сек";
    
    MsgBox(cancelMsg, "FBE скрипт");
}

}
