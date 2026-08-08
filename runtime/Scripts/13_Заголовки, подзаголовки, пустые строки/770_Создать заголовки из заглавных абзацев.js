// Скрипт "Создать заголовки из заглавных абзацев" для редактора FBE
// version 1.4
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для автоматической разметки заголовков разделов в fb2 документах.
// Скрипт находит абзацы, написанные целиком ЗАГЛАВНЫМИ БУКВАМИ + любые символы не-буквы),
// длиной в заданных пределах (по умолчанию до 60 символов).
// Минимальное кол-во БУКВ в заглавном абзаце - по умолчанию - 3.
// По умолчанию обрабатывается только основной раздел документа, без сносок и комментариев.
// По умолчанию исключены из обработки абзацы внутри уже размеченных структурных элементов,
// (аннотации, заголовки, эпиграфы, цитаты, стихи, таблицы, подзаголовки),
// а также абзацы, начинающиеся с тире или дефисов (возможные диалоги).
// Можно исключить из обработки абзацы, заканчивающиеся точкой.
// Также исключаются абзацы, содержащие слова или символы из настраиваемого списка стоп-слов.
// Скрипт преобразует найденные подходящие абзацы в заголовки разделов,
// создавая правильную структуру <DIV class="section"><DIV class="title"><P>...</P></DIV>...
// Подряд идущие заглавные абзацы объединяются в один общий заголовок.
// Между примыкающими исходными и новыми заголовками вставляются пустые строки для валидности fb2 документа.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.4, 15.04.2026
//======================================

function Run() {

var scriptName = "Создать заголовки из заглавных абзацев";
var version = "1.4";

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

    // Минимальное количество БУКВ в абзаце
var minLetters = 3;

    // Максимальная длина абзаца в символах (без учёта тегов)
var maxLength = 60;

    // Обрабатывать заглавные абзацы, похожие на диалоги (начинающиеся с тире/дефиса)
var processDialogs = 0;     // 0 - нет, 1 - да (по умолчанию НЕ обрабатываем)

    // Обрабатывать заглавные абзацы, заканчивающиеся точкой
var processWithDot = 1;     // 0 - нет, 1 - да (по умолчанию обрабатываем)

    // Обрабатывать заглавные абзацы, содержащие слова и символы из перечня стоп-слов
var processStopWords = 0;     // 0 - нет, 1 - да (по умолчанию НЕ обрабатываем)

    // Перечень стоп-слов (через вертикальную черту | )
    // Если processStopWords = 0, абзацы с этими словами/символами НЕ будут обработаны
    // Можно добавлять свои слова или символы, например: УДК|ББК|ISBN|©|#|®|™
var stopWordsList = "УДК|ББК|ISBN|COPYRIGHT|©|®";

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

    // Функция проверки, является ли символ буквой (любого алфавита)
function isLetter(ch) {
    if (!ch || ch.length == 0) return false;
    var code = ch.charCodeAt(0);
    // A-Z (65-90), a-z (97-122), А-Я (1040-1071), а-я (1072-1103), Ё (1025), ё (1105)
    if ((code >= 65 && code <= 90) || (code >= 97 && code <= 122)) return true;
    if ((code >= 1040 && code <= 1071) || (code >= 1072 && code <= 1103)) return true;
    if (code == 1025 || code == 1105) return true;
    return false;
}

    // Функция проверки, является ли символ строчной буквой
function isLowerCaseLetter(ch) {
    if (!ch || ch.length == 0) return false;
    var code = ch.charCodeAt(0);
    // a-z (97-122), а-я (1072-1103), ё (1105)
    if (code >= 97 && code <= 122) return true;
    if (code >= 1072 && code <= 1103) return true;
    if (code == 1105) return true;
    return false;
}

    // Функция подсчета количества букв в строке
function countLetters(str) {
    if (!str || str.length == 0) return 0;
    var count = 0;
    for (var i = 0; i < str.length; i++) {
        if (isLetter(str.charAt(i))) {
            count++;
        }
    }
    return count;
}

    // Функция проверки, является ли абзац написанным ЗАГЛАВНЫМИ БУКВАМИ
function isAllCapsParagraph(ptr) {
    if (!ptr || isLineEmpty(ptr)) return false;
    
    // Получаем текст без тегов
    var text = ptr.innerText;
    if (!text || text.length == 0) return false;
    
    var hasLetter = false;
    
    // Проверяем каждый символ
    for (var i = 0; i < text.length; i++) {
        var ch = text.charAt(i);
        
        if (isLetter(ch)) {
            hasLetter = true;
            // Если нашли строчную букву - сразу возвращаем false
            if (isLowerCaseLetter(ch)) {
                return false;
            }
        }
    }
    
    // Возвращаем true только если была хотя бы одна буква
    return hasLetter;
}

    // Функция приведения строки к верхнему регистру (с поддержкой кириллицы)
function toUpperCaseStr(str) {
    if (!str || str.length == 0) return "";
    var result = "";
    for (var i = 0; i < str.length; i++) {
        var ch = str.charAt(i);
        var code = ch.charCodeAt(0);
        // Русские строчные в заглавные
        if (code >= 1072 && code <= 1103) {
            result += String.fromCharCode(code - 32);
        } else if (code == 1105) { // ё -> Ё
            result += String.fromCharCode(1025);
        } else if (code >= 97 && code <= 122) { // латинские строчные в заглавные
            result += String.fromCharCode(code - 32);
        } else {
            result += ch;
        }
    }
    return result;
}

    // Функция проверки наличия стоп-слов в абзаце
function containsStopWord(ptr) {
    if (!ptr || !ptr.innerText) return false;
    
    var text = ptr.innerText;
    var upperText = toUpperCaseStr(text);
    
    // Разбиваем список стоп-слов на массив
    var stopWords = stopWordsList.split("|");
    
    // Проверяем каждое стоп-слово
    for (var i = 0; i < stopWords.length; i++) {
        var word = stopWords[i];
        if (word.length == 0) continue;
        
        // Приводим стоп-слово к верхнему регистру
        var upperWord = toUpperCaseStr(word);
        
        // Проверяем наличие в тексте
        if (upperText.indexOf(upperWord) >= 0) {
            return true;
        }
    }
    
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

    // Функция получения длины текста абзаца (без учёта тегов)
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
    // ВСЕГДА исключаем такие абзацы из обработки
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
    totalAllCaps: 0,             // всего абзацев заглавными буквами
    totalFound: 0,               // подходящих под все критерии
    mainBody: 0,
    notesSection: 0,
    commentsSection: 0,
    otherSection: 0,
    excludedByMinLetters: 0,
    excludedByMaxLength: 0,
    excludedByDot: 0,
    excludedByDialog: 0,
    excludedByBlock: 0,
    excludedByStopWords: 0,
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
        
            // Проверка на заглавные буквы
        if (!isAllCapsParagraph(p)) continue;
        
        stats.totalAllCaps++;
        
            // Проверка на нахождение внутри блочных элементов (ВСЕГДА исключаем)
        if (isInsideBlockElement(p)) {
            stats.excludedByBlock++;
            continue;
        }
        
            // Проверка на стоп-слова (если отключена обработка)
        if (!processStopWords && containsStopWord(p)) {
            stats.excludedByStopWords++;
            continue;
        }
        
            // Проверка количества букв (минимум)
        var letterCount = countLetters(p.innerText);
        if (letterCount < minLetters) {
            stats.excludedByMinLetters++;
            continue;
        }
        
            // Проверка длины (максимум символов)
        var len = getTextLength(p);
        if (len > maxLength) {
            stats.excludedByMaxLength++;
            continue;
        }
        
            // Проверка на диалоги (если отключено)
        if (!processDialogs && isDialogStart(p)) {
            stats.excludedByDialog++;
            continue;
        }
        
            // Проверка окончания точкой (если отключена обработка)
        if (!processWithDot && endsWithDot(p)) {
            stats.excludedByDot++;
            continue;
        }
        
            // Все проверки пройдены - добавляем в список потенциальных заголовков
        potentialTitles.push({
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
    // ФАЗА 2: ДИАЛОГ С ПОЛЬЗОВАТЕЛЕМ И ВЫВОД РЕЗУЛЬТАТОВ АНАЛИЗА
    // ==================================================

var userConfirmed = true;     // По умолчанию - да (для режимов 0 и 2)

    // Формируем базовое сообщение с настройками
function getSettingsText() {
    var settings = "";
    settings += "НАСТРОЙКИ СКРИПТА:\n";
    settings += "• Режим отображения: " + getDisplayModeText(showStatistics) + "\n";
    settings += "• Обрабатывать раздел сносок: " + (processNotesSection ? "да" : "нет") + "\n";
    settings += "• Обрабатывать раздел комментариев: " + (processCommentsSection ? "да" : "нет") + "\n";
    settings += "• Мин. количество букв: " + minLetters + "\n";
    settings += "• Макс. длина абзаца: " + maxLength + " символов\n";
    settings += "• Обрабатывать диалоги (с тире или дефисом): " + (processDialogs ? "да" : "нет") + "\n";
    settings += "• Обрабатывать абзацы, заканчивающиеся точкой: " + (processWithDot ? "да" : "нет") + "\n";
    settings += "• Обрабатывать абзацы со стоп-словами: " + (processStopWords ? "да" : "нет") + "\n";
    if (!processStopWords) {
        settings += "  (список стоп-слов: " + stopWordsList + ")\n";
    }
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
    
    if (stats.totalAllCaps > 0) {
        notFoundMsg += "Найдено абзацев заглавными буквами: " + stats.totalAllCaps + "\n";
        notFoundMsg += "Исключено по причинам:\n";
        if (stats.excludedByBlock > 0) notFoundMsg += "  • внутри блочных элементов: " + stats.excludedByBlock + "\n";
        if (!processStopWords && stats.excludedByStopWords > 0) notFoundMsg += "  • содержат стоп-слова: " + stats.excludedByStopWords + "\n";
        if (stats.excludedByMinLetters > 0) notFoundMsg += "  • недостаточно букв (< " + minLetters + "): " + stats.excludedByMinLetters + "\n";
        if (stats.excludedByMaxLength > 0) notFoundMsg += "  • слишком длинные (> " + maxLength + "): " + stats.excludedByMaxLength + "\n";
        if (!processWithDot && stats.excludedByDot > 0) notFoundMsg += "  • заканчиваются точкой: " + stats.excludedByDot + "\n";
        if (!processDialogs && stats.excludedByDialog > 0) notFoundMsg += "  • похожи на диалоги: " + stats.excludedByDialog + "\n";
        notFoundMsg += "\n";
    }
    
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
    msg += "✓ Всего абзацев заглавными буквами: " + stats.totalAllCaps + "\n";
    msg += "✓ Найдено потенциальных заголовков: " + stats.totalFound + "\n";
    msg += "  • в основном разделе: " + stats.mainBody + "\n";
    msg += "  • в сносках-примечаниях: " + stats.notesSection + (processNotesSection ? "" : " (исключены настройками)") + "\n";
    msg += "  • в комментариях: " + stats.commentsSection + (processCommentsSection ? "" : " (исключены настройками)") + "\n\n";
    
    if (stats.totalAllCaps > 0) {
        msg += "ИСКЛЮЧЕНО ПО ПРИЧИНАМ:\n";
        if (stats.excludedByBlock > 0) msg += "  • внутри блочных элементов: " + stats.excludedByBlock + "\n";
        if (!processStopWords && stats.excludedByStopWords > 0) msg += "  • содержат стоп-слова: " + stats.excludedByStopWords + "\n";
        if (stats.excludedByMinLetters > 0) msg += "  • недостаточно букв (< " + minLetters + "): " + stats.excludedByMinLetters + "\n";
        if (stats.excludedByMaxLength > 0) msg += "  • слишком длинные (> " + maxLength + "): " + stats.excludedByMaxLength + "\n";
        if (!processWithDot) {
            msg += "  • заканчиваются точкой: " + stats.excludedByDot + "\n";
        }
        if (!processDialogs) {
            msg += "  • похожи на диалоги: " + stats.excludedByDialog + "\n";
        }
        msg += "\n";
    }
    
    msg += getSettingsText() + "\n";
    
    msg += "Преобразовать найденные абзацы в заголовки разделов?";
    
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
        resultMsg += "  (из " + potentialTitles.length + " подходящих абзацев)\n";
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
