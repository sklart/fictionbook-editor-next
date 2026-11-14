// Скрипт "Создать заголовки из абзацев, содержащих указание времени" для редактора FBE
// version 4.3
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для разметки заголовками в fb2 документах
// обнаруженных коротких абзацев в стиле:
// Прошло три года
// Семь месяцев спустя
// Двумя неделями ранее
// 5 лет назад
// Минувшей ночью
// 29 декабря, понедельник
// 21:00

// По умолчанию скрипт размечает заголовки только в основном разделе,
// не затрагивая разделы сносок-примечаний и комментариев.

// Исключены из обработки:
// Заголовки, подзаголовки, аннотации, эпиграфы, цитаты, стихи, строфы, авторы текста.
// Диалоги, списки, абзацы с личными и притяжательными местоимениями,
// абзацы, заканчивающиеся на !?:;
// Абзацы начинающиеся с кавычек или заканчивающиеся кавычками.

// РАЗДЕЛЬНЫЕ НАСТРОЙКИ:
// 1. Для кандидатов СРАЗУ ПОСЛЕ заголовков/подзаголовков/эпиграфов:
//    - Макс. длина: maxLengthAfterTitle (по умолчанию 60)
//    - Макс. кол-во предложений в абзаце: maxSentencesAfterTitle (по умолчанию 4)
//    - Размечать с точкой в конце: allowDotAfterTitle (по умолчанию 1 = ДА)
//    - Размечать чистые даты: markCleanDatesAfterTitle (по умолчанию 1 = ДА)
//    - Размечать чистое время: markCleanTimeAfterTitle (по умолчанию 1 = ДА)
//    - Размечать чистые даты с точкой: allowDotForDatesAfterTitle (по умолчанию 1 = ДА)
//    - Размечать чистое время с точкой: allowDotForTimeAfterTitle (по умолчанию 1 = ДА)
//
// 2. Для кандидатов В ОБЫЧНОМ ТЕКСТЕ:
//    - Макс. длина: maxLengthInText (по умолчанию 45)
//    - Макс. кол-во предложений в абзаце: maxSentencesInText (по умолчани 2)
//    - Размечать с точкой в конце: allowDotInText (по умолчанию 0 = НЕТ)
//    - Размечать чистые даты: markCleanDatesInText (по умолчанию 1 = ДА)
//    - Размечать чистое время: markCleanTimeInText (по умолчанию 1 = ДА)
//    - Размечать чистые даты с точкой: allowDotForDatesInText (по умолчанию 1 = ДА)
//    - Размечать чистое время с точкой: allowDotForTimeInText (по умолчанию 1 = ДА)

// !! ВОЗМОЖНЫ ЛОЖНЫЕ СРАБАТЫВАНИЯ, ПОСЛЕ ОБРАБОТКИ ДОКУМЕНТА
// ТРЕБУЕТСЯ ПРОВЕРКА СОЗДАННЫХ ЗАГОЛОВКОВ!!

// ПРИ СОЗДАНИИ ЗАГОЛОВКОВ:
// - Кандидат становится заголовком новой секции
// - Секция разделяется на две в месте кандидата
// - Все элементы после кандидата перемещаются в новую секцию
// - Кандидат помещается в <div class="title"> новой секции

// После создания заголовков НАСТОЯТЕЛЬНО РЕКОМЕНДУЕТСЯ запустить скрипт "Почистить структуру",
// который устранит возможные ошибки валидности документа из-за автоматического создания заголовков.

// version 4.3, 11.01.2026
//======================================

function Run() {
    // === НАСТРОЙКИ ===
    var showFullStats = 1; // 1 - полная статистика, 0 - сокращенная (по умолчанию)
    
    // === НАСТРОЙКИ ДЛЯ КАНДИДАТОВ СРАЗУ ПОСЛЕ ЗАГОЛОВКОВ/ПОДЗАГОЛОВКОВ ===
    var maxLengthAfterTitle = 60; // максимальная длина искомого абзаца в символах
    var maxSentencesAfterTitle = 4; // максимальное количество предложений в абзаце
    var allowDotAfterTitle = 1; // 1 - размечать абзацы с точкой в конце, 0 - не размечать
    var markCleanDatesAfterTitle = 1; // 1 - размечать чистые даты, 0 - не размечать
    var markCleanTimeAfterTitle = 1; // 1 - размечать чистое время, 0 - не размечать
    var allowDotForDatesAfterTitle = 1; // 1 - размечать чистые даты с точкой, 0 - не размечать
    var allowDotForTimeAfterTitle = 1; // 1 - размечать чистое время с точкой, 0 - не размечать
    var maxDateLengthAfterTitle = 35; // максимальная длина абзаца для дат
    var maxTimeLengthAfterTitle = 30; // максимальная длина абзаца для времени
    
    // === НАСТРОЙКИ ДЛЯ КАНДИДАТОВ В ОБЫЧНОМ ТЕКСТЕ ===
    var maxLengthInText = 45; // максимальная длина искомого абзаца в символах
    var maxSentencesInText = 2; // максимальное количество предложений в абзаце
    var allowDotInText = 0; // 1 - размечать абзацы с точкой в конце, 0 - не размечать
    var markCleanDatesInText = 1; // 1 - размечать чистые даты, 0 - не размечать
    var markCleanTimeInText = 1; // 1 - размечать чистое время, 0 - не разметь
    var allowDotForDatesInText = 1; // 1 - размечать чистые даты с точкой, 0 - не размечать
    var allowDotForTimeInText = 1; // 1 - размечать чистое время с точкой, 0 - не размечать
    var maxDateLengthInText = 35; // максимальная длина абзаца для дат
    var maxTimeLengthInText = 30; // максимальная длина абзаца для времени
    
    // === ГЛАВНЫЕ ПРИЗНАКИ ===
    
    // 1. ЕДИНИЦЫ ВРЕМЕНИ (обязательно)
    var timeUnits = [
        "минут", "минута", "минуту", "минуты", "минутой", "минутою", "минутами",
        "час", "часа", "часов", "часу", "часом", "часами", "часы",
        "год", "года", "годов", "году", "годом", "лет", "годами", "годы",
        "месяц", "месяца", "месяцев", "месяцу", "месяцем", "месяце", "месяцами", "месяцы",
        "неделя", "недели", "неделю", "неделей", "неделе", "неделею", "недель", "неделях", "неделям", "неделями",
        "день", "дня", "дней", "дню", "днем", "днём", "дне", "днями", "дни",
        "ночь", "ночи", "ночей", "ночью", "ночам", "ночами",
        "сутки", "суток", "суткам", "сутками",
        "утро", "утром", "утра", "утре", "утрам", "утру",
        "вечер", "вечером", "вечера", "вечеров", "вечерами", "вечерам",
        "полдень", "полудня", "пополудни",
        "полночь", "полночи", "полуночи", "полночью", "полуночью"
    ];

    
    // МЕСЯЦЫ во всех падежах и сокращениях
    var months = [
        "январь", "января", "январю", "январем", "январе", "январями", "январях", "январям",
        "февраль", "февраля", "февралю", "февралем", "феврале", "февралями", "февралях", "февралям",
        "март", "марта", "марту", "мартом", "марте", "мартами", "мартах", "мартам",
        "апрель", "апреля", "апрелю", "апрелем", "апреле", "апрелями", "апрелях", "апрелям",
        "май", "мая", "маю", "маем", "мае", "маями", "маях", "маям",
        "июнь", "июня", "июню", "июнем", "июне", "июнями", "июнях", "июням",
        "июль", "июля", "июлю", "июлем", "июле", "июлями", "июлях", "июлям",
        "август", "августа", "августу", "августом", "августе", "августами", "августах", "августам",
        "сентябрь", "сентября", "сентябрю", "сентябрем", "сентябре", "сентябрями", "сентябрях", "сентябрям",
        "октябрь", "октября", "октябрю", "октябрем", "октябре", "октябрями", "октябрях", "октябрям",
        "ноябрь", "ноября", "ноябрю", "ноябрем", "ноябре", "ноябрями", "ноябрях", "ноябрям",
        "декабрь", "декабря", "декабрю", "декабрем", "декабре", "декабрями", "декабрях", "декабрям",
        // Сокращения
        "янв", "фев", "февр", "мар", "апр", "май", "июн", "июл", "авг", "сен", "сент", "окт", "ноя", "нояб", "дек",
        "янв.", "фев.", "февр.", "мар.", "апр.", "май.", "июн.", "июл.", "авг.", "сен.", "сент.", "окт.", "ноя.", "нояб.", "дек."
    ];
    
    // Дни недели
    var weekDays = [
        "понедельник", "вторник", "среда", "четверг", "пятница", "суббота", "воскресенье",
        "среду", "пятницу", "субботу",
        "понедельником", "вторником", "средой", "четвергом", "пятницей", "субботой", "воскресеньем",
        "пн", "вт", "ср", "чт", "пт", "сб", "вс",
        "пн.", "вт.", "ср.", "чт.", "пт.", "сб.", "вс."
    ];
    
    // Времена года (тоже единицы времени)
    var seasons = [
        "зим", "зима", "зиму", "зиы", "зимой", "зимою", "зиме",
        "весен", "вёсен", "весна", "весну", "весны", "весной", "весною", "весне",
        "лет", "лето", "лета", "лету", "летом", "лете",
        "осень", "осени", "осенью", "осеней"
    ];
    
    // 2. ХАРАКТЕРИСТИКИ ВРЕМЕНИ (обязательно в паре с единицей)
    var timeCharacteristics = [
        "спустя", "назад", "ранее", "позже", "позднее",
        "после", "до", "через", "примерно",
        "прошел", "прошла", "прошло", "прошли",
        "минул", "минула", "минуло", "минули",
        "миновал", "миновала", "миновало", "миновали",
        "промчалось", "промчались", "промчался", "промчалась"
    ];
    
    // === ВТОРОСТЕПЕННЫЕ ПРИЗНАКИ ===
    
    // Прилагательные времени (усиливают)
    var timeAdjectives = [
        "минувш", "минувший", "минувшая", "минувшее", "минувшего", "минувшей", "минувшему", "минувшим", "минувшем",
        "минувшие", "минувших", "минувшими",
        "прошл", "прошлый", "прошлая", "прошлое", "прошлого", "прошлой", "прошлому", "прошлым", "прошлом",
        "прошлые", "прошлых", "прошлыми",
        "прошедш", "прошедший", "прошедшая", "прошедшее", "прошедшего", "прошедшей", "прошедшему", "прошедшим", "прошедшем",
        "прошедшие", "прошедших", "прошедшими",
        "следущ", "следующий", "следующая", "следующее", "следующего", "следующей", "следующему", "следующим", "следующем",
        "следующие", "следующих", "следующими",
        "предыдущ", "предыдущий", "предыдущая", "предыдущее", "предыдущего", "предыдущей", "предыдущему", "предыдущим", "предыдущем",
        "предыдущие", "предыдущих", "предыдущими",
        "нов", "новый", "новая", "новое", "нового", "новой", "новому", "новым", "новом",
        "будущ", "будущий", "будущая", "будущее", "будущего", "будущей", "будущему", "будущим", "будущем"
    ];
    
    // Дополнительные слова времени (усиливают)
    var additionalTimeWords = [
        "вчера", "потом", "сейчас", "сегодня", "завтра"
    ];
    
    // Предлоги (усиливают)
    var prepositions = [
        "на", "в", "за", "к", "по", "до", "при",
        "перед", "после", "около", "близ", "вокруг", "после"
    ];
    
    // === ФИЛЬТРЫ ИСКЛЮЧЕНИЙ ===
    
    // Союзы (с заглавной буквы и строчные)
    var conjunctions = [
        "И", "А", "Но", "Да", "Или", "Либо", "То", "Чтобы", "Если",
        "Хотя", "Как", "Пока", "Когда", "потому", "Так", "Зато",
        "Что", "Чем", "Чтоб", "Несмотря", "Ведь",
        "и", "а", "но", "да", "или", "либо", "то", "чтобы", "если",
        "хотя", "как", "пока", "когда", "потому", "так", "зато",
        "что", "чем", "чтоб", "несмотря", "ведь"
    ];
    
    // Местоимения (с заглавной буквы и строчные) - УПРОЩЕННЫЙ СПИСОК
    var pronouns = [
        "я", "ты", "вы", "он", "она", "оно", "мы", "они",
        "меня", "тебя", "его", "её", "нас", "вас", "их",
        "мне", "тебе", "ему", "ей", "нам", "вам", "им",
        "мной", "тобой", "ним", "ней", "нами", "вами", "ими", "них", "ними",
        "мой", "твой", "его", "её", "наш", "ваш", "их",
        "моя", "твоя", "его", "её", "наша", "ваша", "их",
        "моё", "твоё", "его", "её", "наше", "ваше", "их",
        "мои", "твои", "его", "её", "наши", "ваши", "их"
    ];
    
    // Частицы (с заглавной буквы и строчные)
    var particles = [
        "Не", "Ни", "Же", "Ли", "Бы", "Б", "Ведь", "Вот", "Мол",
        "Дескать", "Де", "То", "Ибо", "Пусть", "Пускай", "Давай",
        "Давайте", "Ну", "Что", "Как", "Чтобы",
        "не", "ни", "же", "ли", "бы", "б", "ведь", "вот", "мол",
        "деска", "де", "то", "ибо", "пусть", "пускай", "давай",
        "давайте", "ну", "что", "как", "чтобы"
    ];
    
    // Запрещенные знаки в конце абзаца
    var forbiddenEndChars = ["!", "?", ":", ";"];
    
    // Символы начала диалога
    var dialogueStartChars = ["—", "-", "–", "‑", "−", "‒"];
    
    // Символы кавычек
    var quoteChars = ["\"", "'", "«", "»", "„", "«", "»", "‚", "‘", "’"];
    
    // Паттерны списков (регулярные выражения)
    var listPatterns = [
        /^\d+[\)\.]/,
        /^[а-я]\)/i,
        /^[a-z]\)/i,
        /^[ivx]+[\)\.]/i,
        /^[а-я]\./i,
        /^[a-z]\./i
    ];
    
    // === СЛУЖЕБНЫЕ ФУНКЦИИ ===
    
    // Получение неразрывного пробела из настроек FBE
    var nbspChar, nbspEntity;
    try { 
        nbspChar = window.external.GetNBSP(); 
        if (nbspChar.charCodeAt(0) == 160) nbspEntity = "&nbsp;";
        else nbspEntity = nbspChar;
    }
    catch(e) { 
        nbspChar = String.fromCharCode(160);
        nbspEntity = "&nbsp;";
    }
    
    // Список необычных пробелов
    var unusualSpaces = String.fromCharCode(160) +  // неразрывный пробел
        String.fromCharCode(8194) +  // EN SPACE // (EN ПРОБЕЛ) // U+2002
        String.fromCharCode(8195) +  // EM SPACE // (EM ПРОБЕЛ) // U+2003
        String.fromCharCode(8196) +  // THREE-PER-EM SPACE // (ТРИ В EM ПРОБЕЛ) // U+2004
        String.fromCharCode(8197) +  // FOUR-PER-EM SPACE // (ЧЕТЫРЕ В EM ПРОБЕЛ) // U+2005
        String.fromCharCode(8198) +  // SIX-PER-EM SPACE // (ШЕСТЬ В EM ПРОБЕЛ) // U+2006
        String.fromCharCode(8239) +  // NARROW NO-BREAK SPACE // (УЗКИЙ НЕРАЗРЫВНЫЙ ПРОБЕЛ) // U+202F
        String.fromCharCode(8201) +  // THIN SPACE // (ТОНКИЙ ПРОБЕЛ) // U+2009
        String.fromCharCode(8202) +  // HAIR SPACE // (САМЫЙ ТОНКИЙ ПРОБЕЛ) // U+200A
        nbspChar;
    
    // Функция нормализации пробелов
    function normalizeSpaces(text) {
        if (!text || text.length === 0) return "";
        
        var result = "";
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            var found = false;
            for (var j = 0; j < unusualSpaces.length; j++) {
                if (ch === unusualSpaces.charAt(j)) {
                    result += " ";
                    found = true;
                    break;
                }
            }
            if (!found) result += ch;
        }
        
        var start = 0;
        var end = result.length - 1;
        
        while (start < result.length && result.charAt(start) === " ") start++;
        while (end >= 0 && result.charAt(end) === " ") end--;
        
        if (start > end) return "";
        return result.substring(start, end + 1);
    }
    
    // Функция получения чистого текста
    function getPlainText(element) {
        if (!element) return "";
        
        var text = "";
        var nodes = element.childNodes;
        
        for (var i = 0; i < nodes.length; i++) {
            var node = nodes[i];
            if (node.nodeType === 3) {
                text += node.nodeValue;
            }
            else if (node.nodeType === 1) {
                text += getPlainText(node);
            }
        }
        
        return text;
    }
    
    // УСИЛЕННАЯ ФУНКЦИЯ ПРОВЕРКИ ЭЛЕМЕНТОВ
    // Пропускает ЛЮБЫЕ элементы внутри защищенных контейнеров
    function isSkipElement(element) {
        if (!element || !element.tagName) return false;
        
        // Если сам элемент является защищенным - пропускаем
        if (element.className) {
            var className = element.className.toString().toLowerCase();
            if (className === "title" || className === "subtitle" || 
                className === "annotation" || className === "epigraph" ||
                className === "cite" || className === "poem" ||
                className === "stanza" || className === "text-author") {
                return true;
            }
        }
        
        // Проверяем родительские элементы на всех уровнях
        var parent = element.parentNode;
        var maxLevels = 20;
        
        while (parent && maxLevels-- > 0) {
            if (parent.tagName && parent.tagName.toUpperCase() === "DIV") {
                if (parent.className) {
                    var parentClassName = parent.className.toString().toLowerCase();
                    
                    // Если родитель - защищенный контейнер, пропускаем элемент
                    if (parentClassName === "title" || 
                        parentClassName === "subtitle" || 
                        parentClassName === "annotation" ||
                        parentClassName === "epigraph" ||
                        parentClassName === "cite" ||
                        parentClassName === "poem" ||
                        parentClassName === "stanza" ||
                        parentClassName === "text-author") {
                        return true;
                    }
                }
                
                // Пропускаем разделы сносок и комментариев
                if (parent.getAttribute("fbname")) {
                    var fbname = parent.getAttribute("fbname").toLowerCase();
                    if (fbname === "notes" || fbname === "comments") {
                        return true;
                    }
                }
                
                // Пропускаем разделы с заголовками "примечания", "комментарии"
                if (parent.className && parent.className.toString().toLowerCase() === "body") {
                    var titleDiv = null;
                    var children = parent.childNodes;
                    for (var i = 0; i < children.length; i++) {
                        if (children[i].nodeType === 1 && 
                            children[i].tagName && 
                            children[i].tagName.toUpperCase() === "DIV" &&
                            children[i].className && 
                            children[i].className.toString().toLowerCase() === "title") {
                            titleDiv = children[i];
                            break;
                        }
                    }
                    
                    if (titleDiv) {
                        var titleText = getPlainText(titleDiv).toLowerCase();
                        if (titleText.indexOf("примечания") !== -1 || 
                            titleText.indexOf("комментарии") !== -1 ||
                            titleText.indexOf("сноск") !== -1) {
                            return true;
                        }
                    }
                }
            }
            parent = parent.parentNode;
        }
        
        return false;
    }
    
    // Функция проверки форматирования
    function checkFormatting(element) {
        var isItalic = false;
        var isBold = false;
        
        if (element.tagName.toUpperCase() === "EM" || element.tagName.toUpperCase() === "I") {
            isItalic = true;
        }
        if (element.tagName.toUpperCase() === "STRONG" || element.tagName.toUpperCase() === "B") {
            isBold = true;
        }
        
        var emElements = element.getElementsByTagName("EM");
        var iElements = element.getElementsByTagName("I");
        var strongElements = element.getElementsByTagName("STRONG");
        var bElements = element.getElementsByTagName("B");
        
        if (emElements.length > 0 || iElements.length > 0) isItalic = true;
        if (strongElements.length > 0 || bElements.length > 0) isBold = true;
        
        return { italic: isItalic, bold: isBold };
    }
    
    // Функция проверки позиции абзаца
    function checkPosition(paragraph) {
        var prevElement = paragraph.previousSibling;
        
        while (prevElement) {
            if (prevElement.nodeType === 1) {
                var plainText = getPlainText(prevElement);
                var normalized = normalizeSpaces(plainText);
                if (normalized.length > 0) {
                    break;
                }
            }
            prevElement = prevElement.previousSibling;
        }
        
        if (!prevElement) return "in_text";
        
        if (prevElement.className) {
            var className = prevElement.className.toString().toLowerCase();
            
            var parentDiv = prevElement.parentNode;
            while (parentDiv && parentDiv.tagName && parentDiv.tagName.toUpperCase() !== "DIV") {
                parentDiv = parentDiv.parentNode;
            }
            
            if (parentDiv && parentDiv.className) {
                var parentClass = parentDiv.className.toString().toLowerCase();
                
                if (parentClass === "title") return "after_title";
                if (parentClass === "subtitle") return "after_subtitle";
                if (parentClass === "epigraph") return "after_epigraph";
            }
            
            if (className === "title") return "after_title";
            if (className === "subtitle") return "after_subtitle";
            if (className === "epigraph") return "after_epigraph";
        }
        
        return "in_text";
    }
    
    // Функция проверки, является ли абзац последним перед концом секции
    function isLastBeforeSectionEnd(paragraph) {
        var parent = paragraph.parentNode;
        while (parent && parent.tagName && parent.tagName.toUpperCase() !== "DIV") {
            parent = parent.parentNode;
        }
        
        if (!parent || !parent.className || parent.className.toString().toLowerCase() !== "section") {
            return false;
        }
        
        var nextElement = paragraph.nextSibling;
        while (nextElement) {
            if (nextElement.nodeType === 1 && nextElement.tagName && nextElement.tagName.toUpperCase() === "P") {
                return false;
            }
            
            if (nextElement.nodeType === 1 && nextElement.tagName && nextElement.tagName.toUpperCase() === "DIV") {
                break;
            }
            
            nextElement = nextElement.nextSibling;
        }
        
        return true;
    }
    
    // Функция проверки, начинается ли абзац с диалога
    function startsWithDialogue(text) {
        if (!text || text.length === 0) return false;
        
        var firstChar = text.charAt(0);
        for (var i = 0; i < dialogueStartChars.length; i++) {
            if (firstChar === dialogueStartChars[i]) {
                return true;
            }
        }
        
        return false;
    }
    
    // Функция проверки запрещенных символов в конце
    function hasForbiddenEndChar(text) {
        if (!text || text.length === 0) return false;
        
        var lastChar = text.charAt(text.length - 1);
        for (var i = 0; i < forbiddenEndChars.length; i++) {
            if (lastChar === forbiddenEndChars[i]) {
                return true;
            }
        }
        
        return false;
    }
    
    // Функция проверки, является ли абзац списком
    function isList(text) {
        if (!text || text.length === 0) return false;
        
        var trimmed = normalizeSpaces(text);
        if (trimmed.length === 0) return false;
        
        for (var i = 0; i < listPatterns.length; i++) {
            if (listPatterns[i].test(trimmed)) {
                return true;
            }
        }
        
        return false;
    }
    
    // Функция проверки, начинается ли абзац с кавычек
    function startsWithQuote(text) {
        if (!text || text.length === 0) return false;
        
        var firstChar = text.charAt(0);
        for (var i = 0; i < quoteChars.length; i++) {
            if (firstChar === quoteChars[i]) {
                return true;
            }
        }
        
        return false;
    }
    
    // Функция проверки количества предложений
    function countSentences(text) {
        if (!text || text.length === 0) return 0;
        
        var count = 0;
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            if (ch === '.' || ch === '!' || ch === '?' || ch === '…') {
                count++;
            }
        }
        
        return count;
    }
    
    // Функция проверки наличия точки в конце
    function hasDotAtEnd(text) {
        if (!text || text.length === 0) return false;
        
        var lastChar = text.charAt(text.length - 1);
        return lastChar === '.';
    }
    
    // Функция извлечения слов из текста
    function extractWords(text) {
        if (!text || text.length === 0) return [];
        
        var words = [];
        var currentWord = "";
        var inWord = false;
        
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            
            var isWordChar = 
                (ch >= 'А' && ch <= 'Я') || 
                (ch >= 'а' && ch <= 'я') || 
                (ch >= 'A' && ch <= 'Z') || 
                (ch >= 'a' && ch <= 'z') ||
                (ch >= '0' && ch <= '9') ||
                ch === '-' || ch === '—' || ch === 'ё' || ch === 'Ё';
            
            if (isWordChar) {
                if (!inWord) {
                    inWord = true;
                }
                currentWord += ch;
            } else {
                if (inWord && currentWord.length > 0) {
                    words.push(currentWord.toLowerCase()); // сразу в нижнем регистре
                    currentWord = "";
                    inWord = false;
                }
            }
        }
        
        if (inWord && currentWord.length > 0) {
            words.push(currentWord.toLowerCase());
        }
        
        return words;
    }
    
    // Функция проверки, содержит ли текст местоимения - СТРОГАЯ ВЕРСИЯ
    function containsPronouns(text) {
        if (!text || text.length === 0) return false;
        
        var words = extractWords(text);
        
        for (var i = 0; i < words.length; i++) {
            var word = words[i];
            
            // Проверяем все местоимения
            for (var j = 0; j < pronouns.length; j++) {
                if (word === pronouns[j].toLowerCase()) {
                    return true;
                }
            }
            
            // Также проверяем союзы как местоимения (они часто в начале предложений)
            for (var k = 0; k < conjunctions.length; k++) {
                if (word === conjunctions[k].toLowerCase()) {
                    // Но не все союзы - исключаем только если это не начало времени
                    if (i > 0) { // если союз не первый в предложении
                        return true;
                    }
                }
            }
        }
        
        return false;
    }
    
    // Функция проверки, начинается ли абзац с запрещенного слова
    function startsWithForbiddenWord(text) {
        if (!text || text.length === 0) return false;
        
        var trimmed = normalizeSpaces(text);
        if (trimmed.length === 0) return false;
        
        var words = extractWords(trimmed);
        
        if (words.length === 0) return false;
        
        var firstWord = words[0];
        
        for (var i = 0; i < conjunctions.length; i++) {
            if (firstWord === conjunctions[i].toLowerCase()) {
                return true;
            }
        }
        
        for (var j = 0; j < pronouns.length; j++) {
            if (firstWord === pronouns[j].toLowerCase()) {
                return true;
            }
        }
        
        for (var k = 0; k < particles.length; k++) {
            if (firstWord === particles[k].toLowerCase()) {
                return true;
            }
        }
        
        return false;
    }
    
    // Функция проверки отсутствия точки в конце
    function hasNoDotAtEnd(text) {
        if (!text || text.length === 0) return false;
        
        var lastChar = text.charAt(text.length - 1);
        return lastChar !== '.';
    }
    
    // Функция поиска целых слов
    function containsWholeWord(text, wordList) {
        if (!text || text.length === 0 || !wordList || wordList.length === 0) {
            return false;
        }
        
        var words = extractWords(text);
        
        for (var i = 0; i < words.length; i++) {
            var currentWord = words[i];
            
            for (var j = 0; j < wordList.length; j++) {
                var searchWord = wordList[j].toLowerCase();
                
                // Прямое совпадение
                if (currentWord === searchWord) {
                    return true;
                }
                
                // Для слов с усеченных окончаниях
                if (searchWord.length < currentWord.length) {
                    if (currentWord.indexOf(searchWord) === 0) {
                        // Проверяем что следующая буква не продолжает слово
                        var nextChar = currentWord.charAt(searchWord.length);
                        var isLetter = 
                            (nextChar >= 'а' && nextChar <= 'я') || 
                            (nextChar >= 'a' && nextChar <= 'z');
                        if (!isLetter) {
                            return true;
                        }
                    }
                }
            }
        }
        
        return false;
    }
    
    // Функция поиска числительных
    function containsNumeral(text) {
        if (!text || text.length === 0) return false;
        
        var words = extractWords(text);
        
        var numerals = [
            "один", "одна", "одно", "одни", "перв",
            "два", "две", "втор", "двух",
            "три", "трех", "треть",
            "четыре", "четырех", "четверт",
            "пять", "пяти", "пят",
            "шесть", "шести", "шест",
            "семь", "семи", "седьм",
            "восемь", "восьми", "восьм",
            "девять", "девяти", "девят",
            "десять", "десяти", "десят",
            "одиннадцать", "двенадцать", "тринадцать", "четырнадцать", "пятнадцать",
            "шестнадцать", "семнадцать", "восемнадцать", "девятнадцать", "двадцать",
            "тридцать", "сорок", "пятьдесят", "шестьдесят", "семьдесят", "восемьдесят", "девяносто",
            "сто", "двести", "триста", "четыреста", "пятьсот", "шестьсот", "семьсот", "восемьсот", "девятьсот",
            "тысяча", "миллион", "миллиард",
            "пол", "половина", "полтора", "двое", "трое", "четверо", "пятеро", "шестеро", "семеро", "восьмеро", "девятеро", "десятеро"
        ];
        
        for (var i = 0; i < words.length; i++) {
            var currentWord = words[i];
            
            // Проверяем числительные
            for (var j = 0; j < numerals.length; j++) {
                var numeral = numerals[j].toLowerCase();
                if (currentWord.indexOf(numeral) === 0) {
                    return true;
                }
            }
            
            // Проверяем цифры
            var hasDigit = false;
            for (var k = 0; k < currentWord.length; k++) {
                if (currentWord.charAt(k) >= "0" && currentWord.charAt(k) <= "9") {
                    hasDigit = true;
                    break;
                }
            }
            if (hasDigit) return true;
        }
        
        return false;
    }
    
    // Функция проверки, является ли текст "чистой датой"
    function isCleanDate(text) {
        if (!text || text.length === 0) return false;
        
        // Проверяем наличие дня недели
        var hasWeekDay = containsWholeWord(text, weekDays);
        
        // Проверяем наличие месяца
        var hasMonth = containsWholeWord(text, months);
        
        // Проверяем наличие цифр
        var hasDigits = false;
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            if (ch >= '0' && ch <= '9') {
                hasDigits = true;
                break;
            }
        }
        
        // Проверяем форматы дат
        var datePattern1 = /\d{1,2}\s*[\.\-]\s*\d{1,2}\s*[\.\-]\s*\d{2,4}/; // 25.12.1972 или 25-12-1972
        var datePattern2 = /\d{1,2}\s*[\.\-]\s*\d{1,2}/; // 25.12 или 25-12
        var datePattern3 = /\d{1,2}\s*[гг]?\.?$/; // 13-го или 13-го.
        
        var isDatePattern = datePattern1.test(text) || datePattern2.test(text) || datePattern3.test(text);
        
        // Датой считаем если:
        // 1. Есть месяц И (цифры ИЛИ день недели)
        // 2. ИЛИ есть паттерн даты
        if ((hasMonth && (hasDigits || hasWeekDay)) || isDatePattern) {
            return true;
        }
        
        return false;
    }
    
    // Функция проверки, является ли текст "чистым временем"
    function isCleanTime(text) {
        if (!text || text.length === 0) return false;
        
        // Проверяем наличие дня недели
        var hasWeekDay = containsWholeWord(text, weekDays);
        
        // Проверяем форматы времени
        var timePattern1 = /\d{1,2}\s*[:]\s*\d{2}/; // 21:00, 05:45
        var timePattern2 = /\d{1,2}\s*[\-]\s*\d{2}/; // 20-15, 23-59
        var timePattern3 = /\d{1,2}\s*[ч]\s*\d{2}/; // 21 ч 00
        
        var isTimePattern = timePattern1.test(text) || timePattern2.test(text) || timePattern3.test(text);
        
        // Временем считаем если:
        // 1. Есть паттерн времени
        // 2. Может быть с днем недели
        if (isTimePattern) {
            return true;
        }
        
        return false;
    }
    
    // ГЛАВНАЯ ФУНКЦИЯ ПРОВЕРКИ КЛЮЧЕВЫХ СЛОВ - ИСПРАВЛЕННАЯ
    function containsKeywords(text) {
        if (!text || text.length === 0) return false;
        
        // ШАГ 0: Проверяем местоимения СРАЗУ (самый важный фильтр!)
        if (containsPronouns(text)) {
            return false; // НЕМЕДЛЕННО отбрасываем если есть местоимения
        }
        
        // ШАГ 1: Проверяем наличие ЕДИНИЦЫ ВРЕМЕНИ (обязательно)
        var hasTimeUnit = containsWholeWord(text, timeUnits);
        
        if (!hasTimeUnit) {
            hasTimeUnit = containsWholeWord(text, seasons);
        }
        
        if (!hasTimeUnit) {
            hasTimeUnit = containsWholeWord(text, months);
        }
        
        if (!hasTimeUnit) {
            return false;
        }
        
        // ШАГ 2: Проверяем наличие ХАРАКТЕРИСТИКИ ВРЕМЕНИ
        var hasTimeCharacteristic = containsWholeWord(text, timeCharacteristics);
        
        // ШАГ 3: Проверяем числительные
        var hasNumeral = containsNumeral(text);
        
        // ШАГ 4: Проверяем другие признаки
        var hasTimeAdjective = containsWholeWord(text, timeAdjectives);
        var hasAdditionalWord = containsWholeWord(text, additionalTimeWords);
        var hasPreposition = containsWholeWord(text, prepositions);
        var hasNoDot = hasNoDotAtEnd(text);
        
        // ПРАВИЛО 1: Единица + Характеристика + Числительное = 100% кандидат
        if (hasTimeCharacteristic && hasNumeral) {
            return true; // "7 недель спустя", "Три года прошло"
        }
        
        // ПРАВИЛО 2: Единица + Характеристика (без числительного)
        if (hasTimeCharacteristic) {
            return true; // "Прошлой ночью", "Спустя время"
        }
        
        // ПРАВИЛО 3: Только единица времени (без характеристики)
        // Нужны дополнительные усиливающие признаки
        
        var secondaryCount = 0;
        if (hasTimeAdjective) secondaryCount++;
        if (hasNumeral) secondaryCount++;
        if (hasAdditionalWord) secondaryCount++;
        if (hasPreposition) secondaryCount++;
        if (hasNoDot) secondaryCount++;
        
        // Нужно минимум 2 признака
        return secondaryCount >= 2;
    }
    
    // ФУНКЦИЯ: Найти родительскую секцию для элемента
    function findParentSection(element) {
        if (!element || !element.tagName) return null;
        
        var current = element;
        var maxLevels = 20;
        
        while (current && maxLevels-- > 0) {
            if (current.tagName && current.tagName.toUpperCase() === "DIV") {
                if (current.className) {
                    var className = current.className.toString().toLowerCase();
                    if (className === "section") {
                        return current;
                    }
                }
            }
            current = current.parentNode;
        }
        
        return null;
    }
    
    // ФУНКЦИЯ: Создать заголовок из абзаца
    function createTitleFromParagraph(paragraph, nbspEntity) {
        if (!paragraph || !paragraph.tagName || paragraph.tagName.toUpperCase() !== "P") {
            return null;
        }
        
        // Создаем заголовок
        var titleDiv = document.createElement("DIV");
        titleDiv.className = "title";
        
        // Создаем абзац внутри заголовка с текстом из оригинала
        var titleParagraph = document.createElement("P");
        titleParagraph.innerHTML = paragraph.innerHTML;
        titleDiv.appendChild(titleParagraph);
        
        return titleDiv;
    }
    
    // ФУНКЦИЯ: Разделить секцию в месте кандидата
    function splitSectionAtCandidate(candidateParagraph, nbspEntity) {
        if (!candidateParagraph) return false;
        
        // Находим родительскую секцию
        var parentSection = findParentSection(candidateParagraph);
        if (!parentSection) return false;
        
        // Находим родителя секции (обычно body или другая секция)
        var sectionParent = parentSection.parentNode;
        if (!sectionParent) return false;
        
        // Создаем новую секцию
        var newSection = document.createElement("DIV");
        newSection.className = "section";
        
        // Создаем заголовок из кандидата
        var titleDiv = createTitleFromParagraph(candidateParagraph, nbspEntity);
        if (!titleDiv) return false;
        
        // Добавляем заголовок в новую секцию
        newSection.appendChild(titleDiv);
        
        // Находим ВСЕ элементы после кандидата в оригинальной секции
        var elementsToMove = [];
        var currentElement = candidateParagraph.nextSibling;
        
        while (currentElement) {
            // Проверяем, не вышли ли мы за пределы секции
            var isInSameSection = false;
            var checkParent = currentElement.parentNode;
            while (checkParent) {
                if (checkParent === parentSection) {
                    isInSameSection = true;
                    break;
                }
                checkParent = checkParent.parentNode;
            }
            
            if (!isInSameSection) {
                break; // Вышли за пределы секции
            }
            
            // Добавляем элемент в список для перемещения
            elementsToMove.push(currentElement);
            currentElement = currentElement.nextSibling;
        }
        
        // Перемещаем элементы в новую секцию (в обратном порядке для сохранения последовательности)
        for (var i = elementsToMove.length - 1; i >= 0; i--) {
            var element = elementsToMove[i];
            // Удаляем из старой секции
            if (element.parentNode) {
                element.parentNode.removeChild(element);
            }
            // Добавляем в новую секцию ПОСЛЕ заголовка
            if (newSection.firstChild && newSection.firstChild.nextSibling) {
                newSection.insertBefore(element, newSection.firstChild.nextSibling);
            } else {
                newSection.appendChild(element);
            }
        }
        
        // Удаляем кандидата из оригинальной секции
        if (candidateParagraph.parentNode) {
            candidateParagraph.parentNode.removeChild(candidateParagraph);
        }
        
        // Проверяем, не осталась ли оригинальная секция пустой
        var hasContentInOriginal = false;
        var originalChildren = parentSection.childNodes;
        for (var j = 0; j < originalChildren.length; j++) {
            var child = originalChildren[j];
            if (child.nodeType === 1) {
                // Пропускаем заголовки
                if (child.tagName && child.tagName.toUpperCase() === "DIV" && 
                    child.className && child.className.toString().toLowerCase() === "title") {
                    continue;
                }
                
                // Проверяем, есть ли контент
                if (child.tagName && child.tagName.toUpperCase() === "P") {
                    var text = getPlainText(child);
                    if (normalizeSpaces(text).length > 0) {
                        hasContentInOriginal = true;
                        break;
                    }
                } else {
                    // Любой другой элемент считаем контентом
                    hasContentInOriginal = true;
                    break;
                }
            }
        }
        
        // Если оригинальная секция пустая - добавляем пустой абзац
        if (!hasContentInOriginal) {
            var emptyPara = document.createElement("P");
            emptyPara.innerHTML = nbspEntity;
            parentSection.appendChild(emptyPara);
        }
        
        // Вставляем новую секцию ПОСЛЕ оригинальной
        if (parentSection.nextSibling) {
            sectionParent.insertBefore(newSection, parentSection.nextSibling);
        } else {
            sectionParent.appendChild(newSection);
        }
        
        return true;
    }
    
    // Функция для проверки, находится ли элемент1 перед элементом2 в DOM
    function isElementBefore(element1, element2) {
        if (element1 === element2) return false;
        
        // Обходим DOM от корня документа
        var allElements = document.getElementsByTagName("*");
        
        var index1 = -1;
        var index2 = -1;
        
        for (var i = 0; i < allElements.length; i++) {
            if (allElements[i] === element1) {
                index1 = i;
            }
            if (allElements[i] === element2) {
                index2 = i;
            }
            if (index1 !== -1 && index2 !== -1) {
                break;
            }
        }
        
        return index1 < index2;
    }
    
    // === ОСНОВНАЯ ЛОГИКА ===
    
    var stats = {
        totalFound: 0,
        afterTitle: 0,
        afterSubtitle: 0,
        afterEpigraph: 0,
        italicInText: 0,
        boldInText: 0,
        plainInText: 0,
        cleanDates: 0,
        cleanTime: 0,
        processed: 0,
        filteredByPronouns: 0,
        filteredByLength: 0,
        filteredByDot: 0,
        filteredBySentences: 0,
        filteredCleanDates: 0,
        filteredCleanTime: 0,
        skippedInProtectedContainers: 0,
        sectionsSplit: 0,
        emptySectionsFixed: 0
    };
    
    var candidates = [];
    var allParagraphs = document.getElementsByTagName("P");
    
    for (var i = 0; i < allParagraphs.length; i++) {
        var paragraph = allParagraphs[i];
        
        // ВАЖНО: сначала проверяем, не находится ли элемент в защищенном контейнере
        if (isSkipElement(paragraph)) {
            // Сразу пропускаем, не проверяя дальше
            continue;
        }
        
        var plainText = getPlainText(paragraph);
        var normalizedText = normalizeSpaces(plainText);
        
        if (normalizedText.length === 0) {
            continue;
        }
        
        if (isLastBeforeSectionEnd(paragraph)) continue;
        if (startsWithDialogue(normalizedText)) continue;
        if (hasForbiddenEndChar(normalizedText)) continue;
        if (isList(normalizedText)) continue;
        if (startsWithQuote(normalizedText)) continue;
        if (startsWithForbiddenWord(normalizedText)) continue;
        
        // Считаем сколько отфильтровано местоимениями
        if (containsPronouns(normalizedText)) {
            stats.filteredByPronouns++;
            continue;
        }
        
        // Проверяем, является ли текст "чистой датой" или "чистым временем"
        var isCleanDateText = isCleanDate(normalizedText);
        var isCleanTimeText = isCleanTime(normalizedText);
        
        // Проверяем ключевые слова (для обычных временных выражений)
        var hasKeywords = containsKeywords(normalizedText);
        
        // Если не чистая дата/время и нет ключевых слов - пропускаем
        if (!isCleanDateText && !isCleanTimeText && !hasKeywords) {
            continue;
        }
        
        // Определяем позицию абзаца
        var position = checkPosition(paragraph);
        var hasDot = hasDotAtEnd(normalizedText);
        
        // Применяем раздельные настройки в зависимости от позиции
        var maxLength = 0;
        var maxSentences = 0;
        var allowDot = 0;
        var markCleanDates = 0;
        var markCleanTime = 0;
        var allowDotForDates = 0;
        var allowDotForTime = 0;
        var maxDateLength = 0;
        var maxTimeLength = 0;
        
        if (position === "after_title" || position === "after_subtitle" || position === "after_epigraph") {
            maxLength = maxLengthAfterTitle;
            maxSentences = maxSentencesAfterTitle;
            allowDot = allowDotAfterTitle;
            markCleanDates = markCleanDatesAfterTitle;
            markCleanTime = markCleanTimeAfterTitle;
            allowDotForDates = allowDotForDatesAfterTitle;
            allowDotForTime = allowDotForTimeAfterTitle;
            maxDateLength = maxDateLengthAfterTitle;
            maxTimeLength = maxTimeLengthAfterTitle;
        } else {
            maxLength = maxLengthInText;
            maxSentences = maxSentencesInText;
            allowDot = allowDotInText;
            markCleanDates = markCleanDatesInText;
            markCleanTime = markCleanTimeInText;
            allowDotForDates = allowDotForDatesInText;
            allowDotForTime = allowDotForTimeInText;
            maxDateLength = maxDateLengthInText;
            maxTimeLength = maxTimeLengthInText;
        }
        
        // Для чистых дат/времени применяем специальные ограничения длины
        var applicableMaxLength = maxLength;
        var applicableAllowDot = allowDot;
        
        if (isCleanDateText) {
            if (markCleanDates === 0) continue; // пропускаем если не разрешено
            applicableMaxLength = maxDateLength;
            applicableAllowDot = allowDotForDates; // используем специальную настройку для дат
        } else if (isCleanTimeText) {
            if (markCleanTime === 0) continue; // пропускаем если не разрешено
            applicableMaxLength = maxTimeLength;
            applicableAllowDot = allowDotForTime; // используем специальную настройку для времени
        }
        
        // Проверяем длину абзаца
        if (normalizedText.length > applicableMaxLength) {
            stats.filteredByLength++;
            if (isCleanDateText) stats.filteredCleanDates++;
            if (isCleanTimeText) stats.filteredCleanTime++;
            continue;
        }
        
        // Проверяем количество предложений (для обычных временных выражений)
        if (!isCleanDateText && !isCleanTimeText) {
            var sentenceCount = countSentences(normalizedText);
            if (sentenceCount > maxSentences) {
                stats.filteredBySentences++;
                continue;
            }
        }
        
        // Проверяем наличие точки в конце (если запрещено)
        if (hasDot && applicableAllowDot === 0) {
            stats.filteredByDot++;
            if (isCleanDateText) stats.filteredCleanDates++;
            if (isCleanTimeText) stats.filteredCleanTime++;
            continue;
        }
        
        var formatting = checkFormatting(paragraph);
        
        // Теперь считаем чистые даты/время только для прошедших кандидатов
        if (isCleanDateText) stats.cleanDates++;
        if (isCleanTimeText) stats.cleanTime++;
        
        candidates.push({
            element: paragraph,
            text: normalizedText,
            position: position,
            hasDot: hasDot,
            italic: formatting.italic,
            bold: formatting.bold,
            isCleanDate: isCleanDateText,
            isCleanTime: isCleanTimeText
        });
        
        stats.totalFound++;
        
        if (position === "after_title") stats.afterTitle++;
        else if (position === "after_subtitle") stats.afterSubtitle++;
        else if (position === "after_epigraph") stats.afterEpigraph++;
        else {
            if (formatting.italic) stats.italicInText++;
            else if (formatting.bold) stats.boldInText++;
            else stats.plainInText++;
        }
    }
    
    if (stats.totalFound === 0) {
        var noResultsMsg = "Создать заголовки из абзацев, содержащих указание времени\n";
        noResultsMsg += "ver. 4.3\n\n";
        noResultsMsg += "Потенциальных заголовков не найдено.\n";
        
        if (showFullStats === 1) {
            noResultsMsg += "\nФильтрация:\n";
            var totalFiltered = stats.filteredByPronouns + stats.filteredByLength + stats.filteredByDot + 
                               stats.filteredBySentences + stats.filteredCleanDates + stats.filteredCleanTime;
            noResultsMsg += "Всего отфильтровано: " + totalFiltered + "\n";
            noResultsMsg += "Из них отброшено:\n";
            if (stats.filteredByPronouns > 0) noResultsMsg += "Местоимениями: " + stats.filteredByPronouns + "\n";
            if (stats.filteredByLength > 0) noResultsMsg += "По длине: " + stats.filteredByLength + "\n";
            if (stats.filteredByDot > 0) noResultsMsg += "По точке в конце: " + stats.filteredByDot + "\n";
            if (stats.filteredBySentences > 0) noResultsMsg += "По количеству предложений: " + stats.filteredBySentences + "\n";
            if (stats.filteredCleanDates > 0) noResultsMsg += "Чистые даты (не прошли фильтры): " + stats.filteredCleanDates + "\n";
            if (stats.filteredCleanTime > 0) noResultsMsg += "Чистое время (не прошли фильтры): " + stats.filteredCleanTime + "\n";
        }
        
        MsgBox(noResultsMsg);
        return;
    }
    
    var message = "Создать заголовки из абзацев, содержащих указание времени\n";
    message += "ver. 4.3\n\n";
    
    if (showFullStats === 1) {
        message += "НАСТРОЙКИ:\n";
        message += "После заголовков - макс. " + maxLengthAfterTitle + " симв., " + maxSentencesAfterTitle + " предл., точка: " + (allowDotAfterTitle === 1 ? "ДА" : "НЕТ") + "\n";
        message += "              даты до " + maxDateLengthAfterTitle + " симв., точка: " + (allowDotForDatesAfterTitle === 1 ? "ДА" : "НЕТ") + "\n";
        message += "              время до " + maxTimeLengthAfterTitle + " симв., точка: " + (allowDotForTimeAfterTitle === 1 ? "ДА" : "НЕТ") + "\n";
        message += "В обычном тексте - макс. " + maxLengthInText + " симв., " + maxSentencesInText + " предл., точка: " + (allowDotInText === 1 ? "ДА" : "НЕТ") + "\n";
        message += "              даты до " + maxDateLengthInText + " симв., точка: " + (allowDotForDatesInText === 1 ? "ДА" : "НЕТ") + "\n";
        message += "              время до " + maxTimeLengthInText + " симв., точка: " + (allowDotForTimeInText === 1 ? "ДА" : "НЕТ") + "\n\n";
    }
    
    message += "РЕЗУЛЬТАТЫ ПОИСКА:\n";
    message += "Найдено потенциальных заголовков - " + stats.totalFound + "\n\n";
    
    if (showFullStats === 1) {
        message += "ДЕТАЛИЗАЦИЯ найденных:\n";
        
        var detailSum = 0;
        if (stats.afterTitle > 0) {
            message += "Сразу после заголовков - " + stats.afterTitle + "\n";
            detailSum += stats.afterTitle;
        }
        if (stats.afterSubtitle > 0) {
            message += "Сразу после подзаголовков - " + stats.afterSubtitle + "\n";
            detailSum += stats.afterSubtitle;
        }
        if (stats.afterEpigraph > 0) {
            message += "Сразу после эпиграфов - " + stats.afterEpigraph + "\n";
            detailSum += stats.afterEpigraph;
        }
        if (stats.italicInText > 0) {
            message += "Курсивных среди обычного текста - " + stats.italicInText + "\n";
            detailSum += stats.italicInText;
        }
        if (stats.boldInText > 0) {
            message += "Жирных среди обычного текста - " + stats.boldInText + "\n";
            detailSum += stats.boldInText;
        }
        if (stats.plainInText > 0) {
            message += "Без форматирования среди обычного текста - " + stats.plainInText + "\n";
            detailSum += stats.plainInText;
        }
        
        // Проверяем сумму детализации
        if (detailSum !== stats.totalFound) {
            message += "ОШИБКА СУММИРОВАНИЯ! Детализация: " + detailSum + ", но найдено: " + stats.totalFound + "\n";
        }
        
        if (stats.cleanDates > 0) message += "(Из них чистые даты - " + stats.cleanDates + ")\n";
        if (stats.cleanTime > 0) message += "(Из них чистое время - " + stats.cleanTime + ")\n\n";
        
        // Фильтрация
        var totalFiltered = stats.filteredByPronouns + stats.filteredByLength + stats.filteredByDot + 
                           stats.filteredBySentences + stats.filteredCleanDates + stats.filteredCleanTime;
        
        message += "ФИЛЬТРАЦИЯ (отброшено): " + totalFiltered + "\n";
        message += "Из них отброшено:\n";
        
        var filterSum = 0;
        if (stats.filteredByPronouns > 0) {
            message += "Местоимениями: " + stats.filteredByPronouns + "\n";
            filterSum += stats.filteredByPronouns;
        }
        if (stats.filteredByLength > 0) {
            message += "По длине: " + stats.filteredByLength + "\n";
            filterSum += stats.filteredByLength;
        }
        if (stats.filteredByDot > 0) {
            message += "По точке в конце: " + stats.filteredByDot + "\n";
            filterSum += stats.filteredByDot;
        }
        if (stats.filteredBySentences > 0) {
            message += "По количеству предложений: " + stats.filteredBySentences + "\n";
            filterSum += stats.filteredBySentences;
        }
        if (stats.filteredCleanDates > 0) {
            message += "Чистые даты (не прошли фильтры): " + stats.filteredCleanDates + "\n";
            filterSum += stats.filteredCleanDates;
        }
        if (stats.filteredCleanTime > 0) {
            message += "Чистое время (не прошли фильтры): " + stats.filteredCleanTime + "\n";
            filterSum += stats.filteredCleanTime;
        }
        
        // Проверяем сумму фильтрации
        if (filterSum !== totalFiltered) {
            message += "ОШИБКА СУММИРОВАНИЯ! Фильтрация: " + filterSum + ", но всего: " + totalFiltered + "\n";
        }
        
        message += "\n";
    }
    
    message += "Итого:\n";
    message += "Всего будет создано заголовков - " + stats.totalFound + "\n";
    message += "Будет разделено секций - " + stats.totalFound + "\n\n";
    message += "Расставить заголовки?";
    
    var response = window.external.AskYesNo(message);
    
    if (response !== 1) {
        MsgBox("Отменено пользователем.");
        return;
    }
    
    var startTime = new Date().getTime();
    window.external.BeginUndoUnit(document, "Создать заголовки из абзацев со временем");
    
    // Сортируем кандидатов по позиции в DOM (от конца к началу)
    // Это важно, чтобы при разделении секций индексы не сбивались
    var sortedCandidates = [];
    for (var j = 0; j < candidates.length; j++) {
        sortedCandidates.push(candidates[j]);
    }
    
    // Сортировка пузырьком от конца к началу
    for (var i = 0; i < sortedCandidates.length - 1; i++) {
        for (var j = i + 1; j < sortedCandidates.length; j++) {
            if (!isElementBefore(sortedCandidates[i].element, sortedCandidates[j].element)) {
                var temp = sortedCandidates[i];
                sortedCandidates[i] = sortedCandidates[j];
                sortedCandidates[j] = temp;
            }
        }
    }
    
    // СЛОЖНЫЙ СЛУЧАЙ: создание заголовков с разделением секций
    // Обрабатываем с конца к началу (это важно!)
    for (var k = 0; k < sortedCandidates.length; k++) {
        var candidate = sortedCandidates[k];
        
        // Дополнительная проверка: не обрабатываем элементы в защищенных контейнерах
        if (isSkipElement(candidate.element)) {
            stats.skippedInProtectedContainers++;
            continue;
        }
        
        // Разделяем секцию в месте кандидата
        if (splitSectionAtCandidate(candidate.element, nbspEntity)) {
            stats.processed++;
            stats.sectionsSplit++;
        }
    }
    
    window.external.EndUndoUnit(document);
    
    var endTime = new Date().getTime();
    var elapsed = ((endTime - startTime) / 1000).toFixed(2);
    
    var finalMessage = "Создать заголовки из абзацев, содержащих указание времени\n";
    finalMessage += "ver. 4.3\n\n";
    finalMessage += "ИТОГИ ОБРАБОТКИ:\n";
    finalMessage += "Найдено кандидатов: " + stats.totalFound + "\n";
    finalMessage += "Создано заголовков: " + stats.processed + "\n";
    finalMessage += "Разделено секций: " + stats.sectionsSplit + "\n";
    
    finalMessage += "\n";
    
    if (showFullStats === 1) {
        if (stats.afterTitle > 0) finalMessage += "После заголовков: " + stats.afterTitle + "\n";
        if (stats.afterSubtitle > 0) finalMessage += "После подзаголовков: " + stats.afterSubtitle + "\n";
        if (stats.afterEpigraph > 0) finalMessage += "После эпиграфов: " + stats.afterEpigraph + "\n";
        if (stats.italicInText > 0) finalMessage += "Курсивные: " + stats.italicInText + "\n";
        if (stats.boldInText > 0) finalMessage += "Жирные: " + stats.boldInText + "\n";
        if (stats.plainInText > 0) finalMessage += "Без форматирования: " + stats.plainInText + "\n";
        if (stats.cleanDates > 0) finalMessage += "Чистые даты: " + stats.cleanDates + "\n";
        if (stats.cleanTime > 0) finalMessage += "Чистое время: " + stats.cleanTime + "\n";
        
        if (stats.skippedInProtectedContainers > 0) {
            finalMessage += "Пропущено в защищенных контейнерах: " + stats.skippedInProtectedContainers + "\n";
        }
        
        finalMessage += "\nФильтрация:\n";
        var totalFiltered = stats.filteredByPronouns + stats.filteredByLength + stats.filteredByDot + 
                           stats.filteredBySentences + stats.filteredCleanDates + stats.filteredCleanTime;
        finalMessage += "Всего отфильтровано: " + totalFiltered + "\n";
        if (stats.filteredByPronouns > 0) finalMessage += "Местоимениями: " + stats.filteredByPronouns + "\n";
        if (stats.filteredByLength > 0) finalMessage += "По длине: " + stats.filteredByLength + "\n";
        if (stats.filteredByDot > 0) finalMessage += "По точке в конце: " + stats.filteredByDot + "\n";
        if (stats.filteredBySentences > 0) finalMessage += "По количеству предложений: " + stats.filteredBySentences + "\n";
        if (stats.filteredCleanDates > 0) finalMessage += "Чистые даты (не прошли фильтры): " + stats.filteredCleanDates + "\n";
        if (stats.filteredCleanTime > 0) finalMessage += "Чистое время (не прошли фильтры): " + stats.filteredCleanTime + "\n";
        
        finalMessage += "\nВремя выполнения: " + elapsed + " сек\n\n";
    } else {
        finalMessage += "Время выполнения: " + elapsed + " сек\n\n";
    }
    
    // Добавляем предупреждение о необходимости чистки структуры только при создании заголовков
    if (stats.processed > 0) {
        finalMessage += "================================\n\n";
        finalMessage += "После создания заголовков\n";
        finalMessage += "НАСТОЯТЕЛЬНО РЕКОМЕНДУЕТСЯ\n";
        finalMessage += "запустить скрипт 'Почистить структуру',\n";
        finalMessage += "который устранит возможные ошибки\n";
        finalMessage += "с валидностью документа\n";
        finalMessage += "из-за автоматического создания заголовков.\n";
    }
    
    MsgBox(finalMessage);
}
