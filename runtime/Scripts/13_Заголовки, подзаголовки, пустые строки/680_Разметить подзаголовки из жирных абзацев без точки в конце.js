// Скрипт "Разметить подзаголовки из жирных абзацев без точки в конце" для редактора FBE
// version 1.7
// Идея - TaKir
// Реализация - DeepSeek, TaKir
  
// Скрипт предназначен для автоматической разметки подзаголовков в fb2 документах.
// Находит абзацы, целиком выделенные жирным (<P><STRONG>...</STRONG></P>),
// не заканчивающиеся точкой, длиной не более заданной (по умолч. 60 символов).
// Позволяет исключать абзацы внутри блочных элементов (стихи, цитаты, эпиграфы),
// а также абзацы, начинающиеся с тире (диалоги). Учитывает наличие сносок в тексте.
// Скрипт преобразует найденные подходящие абзацы в <P class="subtitle">,
// опционально удаляя внешние теги жирности.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.7, 17.02.2025
//======================================

function Run() {

var scriptName = "Разметить подзаголовки из жирных абзацев без точки в конце";
var version = "1.7";

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

    // Разрешённые знаки препинания в конце абзаца (кроме точки)
    // Абзац будет считаться подходящим, если заканчивается на один из этих символов
    // Квадратная скобка добавлена для сносок вида [7]
var allowedEndChars = "!?…:;»\"]";

    // Размечать ли жирные абзацы, похожие на диалоги (начинающиеся с тире/дефиса)
var processDialogs = 0;     // 0 - нет, 1 - да

    // Удалять ли тэги жирности после преобразования (только внешний STRONG)
var removeStrong = 1;     // 0 - нет, 1 - да (по умолчанию удаляем)

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

    // Функция проверки, является ли абзац целиком жирным
function isEntirelyBold(elem) {
    if (!elem || isLineEmpty(elem)) return false;
    
    var myHtml = elem.innerHTML;
    var reStrong = /<STRONG>(((?!<\/?STRONG>).)*?)<\/STRONG>/ig;
    var reTags = /<\/?[^>]*>/ig;
    var reSpaces = new RegExp("^[\\s" + nbspChar + "&nbsp;]*$", "i");
    
        // Удаляем все вхождения <STRONG>...</STRONG>
    var searchResult = reStrong.test(myHtml);
    while (searchResult) {
        myHtml = myHtml.replace(reStrong, "");
        searchResult = reStrong.test(myHtml);
    }
    
        // Удаляем все оставшиеся теги
    myHtml = myHtml.replace(reTags, "");
    
        // Если после удаления жирных тегов остался только текст с пробелами - значит абзац был целиком жирным
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

    // Функция проверки окончания абзаца
function endsWithoutDot(ptr) {
    if (!ptr || !ptr.innerText) return false;
    
    var text = ptr.innerText;
    if (text.length == 0) return false;
    
    var lastChar = text.charAt(text.length - 1);
    
        // Если последний символ - точка, то возвращаем false (не подходит)
    if (lastChar == ".") return false;
    
        // Проверяем, является ли последний символ разрешённым
    for (var i = 0; i < allowedEndChars.length; i++) {
        if (lastChar == allowedEndChars.charAt(i)) 
            return true;
    }
    
        // Проверяем, является ли последний символ знаком препинания (кроме разрешённых)
    var punctuationMarks = ".,:;?!-(){}«»'…";
    for (var i = 0; i < punctuationMarks.length; i++) {
        if (lastChar == punctuationMarks.charAt(i)) {
            return false;     // Это другой знак препинания - не подходит
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

    // Функция удаления внешних тегов STRONG из абзаца (с сохранением внутреннего форматирования)
function removeOuterStrongTags(ptr) {
    if (!ptr) return;
    
    var html = ptr.innerHTML;
        // Удаляем только внешние открывающие и закрывающие теги STRONG
        // Сноски внутри остаются нетронутыми
    html = html.replace(/^<STRONG>|<\/STRONG>$/gi, "");
    ptr.innerHTML = html;
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
    totalBold: 0,               // всего целиком жирных абзацев
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
        
            // Проверка: абзац должен быть целиком жирным
        if (!isEntirelyBold(p)) continue;
        
            // Нашли целиком жирный абзац
        stats.totalBold++;
        
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
        
            // Проверка окончания
        if (!endsWithoutDot(p)) {
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
            bodyType: bodyType
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
    // ФАЗА 2: ДИАЛОГ С ПОЛЬЗОВАТЕЛЕМ (если режим 1)
    // ==================================================

var userConfirmed = true;     // По умолчанию - да (для режимов 0 и 2)

if (showStatistics == 1) {
        // Формируем сообщение со статистикой
    var msg = "";
    
    if (stats.totalFound == 0) {
        msg += "---------------------------\n";
        msg += scriptName + "\n";
        msg += "ver. " + version + "\n";
        msg += "---------------------------\n\n";
        msg += "Подходящих для обработки абзацев не найдено.\n\n";
        msg += "НАСТРОЙКИ СКРИПТА:\n";
        msg += "• Отображение: " + (showStatistics == 1 ? "показывать анализ и статистику" : (showStatistics == 2 ? "только статистика" : "выключен")) + "\n";
        msg += "• Обработка сносок: " + (processNotesSection ? "да" : "нет") + "\n";
        msg += "• Обработка комментариев: " + (processCommentsSection ? "да" : "нет") + "\n";
        msg += "• Обработка блочных элементов: " + (processBlockElements ? "да" : "нет") + "\n";
        msg += "• Макс. длина: " + maxLength + " символов\n";
        msg += "• Разрешённые знаки в конце: " + allowedEndChars + "\n";
        msg += "• Обработка диалогов: " + (processDialogs ? "да" : "нет") + "\n";
        msg += "• Удалять внешний <strong>: " + (removeStrong ? "да" : "нет") + "\n\n";
        
        MsgBox(msg, "FBE скрипт");
    } else {
        msg += "---------------------------\n";
        msg += scriptName + "\n";
        msg += "ver. " + version + "\n";
        msg += "---------------------------\n\n";
        
        msg += "РЕЗУЛЬТАТ АНАЛИЗА:\n";
        msg += "✓ Всего целиком жирных абзацев: " + stats.totalBold + "\n";
        msg += "✓ Найдено потенциальных подзаголовков: " + stats.totalFound + "\n";
        msg += "  • в основном разделе: " + stats.mainBody + "\n";
        msg += "  • в сносках-примечаниях: " + stats.notesSection + (processNotesSection ? "" : " (исключены настройками)") + "\n";
        msg += "  • в комментариях: " + stats.commentsSection + (processCommentsSection ? "" : " (исключены настройками)") + "\n\n";
        
        if (stats.totalBold > 0) {
            msg += "ИСКЛЮЧЕНО ПО ПРИЧИНАМ:\n";
            msg += "  • слишком длинные (> " + maxLength + "): " + stats.excludedByLength + "\n";
            msg += "  • заканчиваются точкой или др. знаком: " + stats.excludedByDot + "\n";
            msg += "  • похожи на диалоги: " + stats.excludedByDialog + "\n";
            msg += "  • внутри блочных элементов: " + stats.excludedByBlock + "\n\n";
        }
        
        msg += "НАСТРОЙКИ СКРИПТА:\n";
        msg += "• Отображение: " + (showStatistics == 1 ? "показывать анализ и статистику" : (showStatistics == 2 ? "только статистика" : "выключен")) + "\n";
        msg += "• Обработка сносок: " + (processNotesSection ? "да" : "нет") + "\n";
        msg += "• Обработка комментариев: " + (processCommentsSection ? "да" : "нет") + "\n";
        msg += "• Обработка блочных элементов: " + (processBlockElements ? "да" : "нет") + "\n";
        msg += "• Макс. длина: " + maxLength + " символов\n";
        msg += "• Разрешённые знаки в конце: " + allowedEndChars + "\n";
        msg += "• Обработка диалогов: " + (processDialogs ? "да" : "нет") + "\n";
        msg += "• Удалять внешний <strong>: " + (removeStrong ? "да" : "нет") + "\n\n";
        
        msg += "Преобразовать найденные жирные абзацы в подзаголовки?";
        
        userConfirmed = AskYesNo(msg);
    }
}

    // ==================================================
    // ФАЗА 3: ПРЕОБРАЗОВАНИЕ (только если пользователь согласен)
    // ==================================================

    // Запускаем таймер ПОСЛЕ confirm
var startTime = new Date().getTime();

if (userConfirmed && potentialSubtitles.length > 0) {
        // Начинаем транзакцию для возможности отмены Ctrl+Z
    window.external.BeginUndoUnit(document, scriptName + " ver. " + version);
    
        // Обрабатываем найденные элементы
    for (var i = 0; i < potentialSubtitles.length; i++) {
        var item = potentialSubtitles[i];
        var p = item.element;
        
            // Меняем класс на subtitle
        p.className = "subtitle";
        
            // Удаляем внешние теги strong, если нужно
        if (removeStrong) {
            removeOuterStrongTags(p);
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
