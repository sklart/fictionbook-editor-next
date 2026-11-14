// Скрипт "Создать подзаголовки из абзацев, содержащих указание времени" для редактора FBE
// version 4.7
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для разметки подзаголовками в fb2 документах
// обнаруженных коротких абзацев в стиле:
// Прошло три года
// Семь месяцев спустя
// Двумя неделями ранее
// 5 лет назад
// Минувшей ночью
// 29 декабря, понедельник
// 21:00

// По умолчанию скрипт размечает подзаголовки только в основном разделе,
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
//    - Макс. кол-во предложений в абзаце: maxSentencesInText (по умолчанию 2)
//    - Размечать с точкой в конце: allowDotInText (по умолчанию 0 = НЕТ)
//    - Размечать чистые даты: markCleanDatesInText (по умолчанию 1 = ДА)
//    - Размечать чистое время: markCleanTimeInText (по умолчанию 1 = ДА)
//    - Размечать чистые даты с точкой: allowDotForDatesInText (по умолчанию 1 = ДА)
//    - Размечать чистое время с точкой: allowDotForTimeInText (по умолчанию 1 = ДА)

// !! ВОЗМОЖНЫ ЛОЖНЫЕ СРАБАТЫВАНИЯ, ПОСЛЕ ОБРАБОТКИ ДОКУМЕНТА
// ТРЕБУЕТСЯ ПРОВЕРКА РАЗМЕЧЕННЫХ ПОДЗАГОЛОВКОВ!!

// version 4.7, 11.01.2026
//======================================

function Run() {
    // === НАСТРОЙКИ ===
    var showFullStats = 1; // 1 - полная статистика, 0 - сокращенная (по умолчанию)
    
    // === НОВЫЕ НАСТРОЙКИ РАЗДЕЛОВ ===
    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0; // 0 - нет, 1 - да
    
    // Обрабатывать раздел комментариев
    var processCommentsSection = 0; // 0 - нет, 1 - да
    
    // === НАСТРОЙКИ ДЛЯ КАНДИДАТОВ СРАЗУ ПОСЛЕ ЗАГОЛОВКОВ/ПОДЗАГОЛОВКОВ ===
    var maxLengthAfterTitle = 60; // максимальная длина искомого абзаца в символах
    var maxSentencesAfterTitle = 4; // максимальное количество предложений в абзаце
    var allowDotAfterTitle = 1; // 1 - размечать абзацы с точкой в конце, 0 - не размечать
    var markCleanDatesAfterTitle = 1; // 1 - размечать чистые даты, 0 - не размечать
    var markCleanTimeAfterTitle = 1; // 1 - размечать чистое время, 0 - не разметь
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
    
    // === ТЕХНИЧЕСКИЕ ТЕРМИНЫ (исключения) ===
    var technicalTerms = [
        "версия", "version", "ver", "выпуск", "release", "ред", "изд", "издание",
        "часть", "part", "глава", "chapter", "том", "volume",
        "стр", "страница", "page", "лист", "sheet",
        "рис", "рисунок", "иллюстрация", "image", "картинка",
        "табл", "таблица", "table", "график", "chart"
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
        String.fromCharCode(8194) +  // EM SPACE
        String.fromCharCode(8195) +  // EN SPACE
        String.fromCharCode(8196) +  // EM SPACE
        String.fromCharCode(8197) +  // EN SPACE
        String.fromCharCode(8198) +  // EM SPACE
        String.fromCharCode(8239) +  // NARROW NO-BREAK SPACE
        String.fromCharCode(8201) +  // THIN SPACE
        String.fromCharCode(8202) +  // HAIR SPACE
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
    
    // УСИЛЕННАЯ ФУНКЦИЯ ПРОВЕРКИ ЭЛЕМЕНТОВ С УЧЕТОМ НОВЫХ НАСТРОЕК
    // Пропускает ЛЮБЫЕ элементы внутри защищенных контейнеров и в необрабатываемых разделах
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
                    
                    // Пропускаем разделы annotation и history всегда (основные)
                    if (parentClassName === "annotation" || parentClassName === "history") {
                        return true;
                    }
                }
                
                // Проверяем атрибут fbname для разделов сносок и комментариев
                if (parent.getAttribute("fbname")) {
                    var fbname = parent.getAttribute("fbname").toLowerCase();
                    
                    // Раздел сносок (примечаний)
                    if (fbname === "notes") {
                        if (processNotesSection === 0) {
                            return true; // пропускаем если не разрешено обрабатывать
                        }
                    }
                    // Раздел комментариев
                    else if (fbname === "comments") {
                        if (processCommentsSection === 0) {
                            return true; // пропускаем если не разрешено обрабатывать
                        }
                    }
                }
                
                // Пропускаем разделы с заголовками "примечания", "комментарии" (старый формат)
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
                            // Проверяем, разрешено ли обрабатывать этот раздел
                            if (titleText.indexOf("примечания") !== -1 && processNotesSection === 0) {
                                return true;
                            }
                            if (titleText.indexOf("комментарии") !== -1 && processCommentsSection === 0) {
                                return true;
                            }
                            if (titleText.indexOf("сноск") !== -1 && processNotesSection === 0) {
                                return true;
                            }
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
    
    // УЛУЧШЕННАЯ ФУНКЦИЯ извлечения слов из текста
    function extractWords(text) {
        if (!text || text.length === 0) return [];
        
        var words = [];
        var currentWord = "";
        
        for (var i = 0; i < text.length; i++) {
            var ch = text.charAt(i);
            var chCode = text.charCodeAt(i);
            
            // Буквы (русские и английские), цифры, дефисы, тире, ё
            var isWordChar = 
                (chCode >= 1040 && chCode <= 1103) ||  // русские буквы
                ch === 'ё' || ch === 'Ё' ||
                (chCode >= 65 && chCode <= 90) ||     // A-Z
                (chCode >= 97 && chCode <= 122) ||    // a-z
                (chCode >= 48 && chCode <= 57) ||     // 0-9
                ch === '-' || ch === '—';
            
            if (isWordChar) {
                currentWord += ch;
            } else {
                if (currentWord.length > 0) {
                    words.push(currentWord.toLowerCase());
                    currentWord = "";
                }
            }
        }
        
        if (currentWord.length > 0) {
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
                        var charCode = nextChar.charCodeAt(0);
                        var isLetter = 
                            (charCode >= 1072 && charCode <= 1103) ||  // русские буквы
                            (charCode >= 97 && charCode <= 122);       // английские буквы
                        if (!isLetter) {
                            return true;
                        }
                    }
                }
            }
        }
        
        return false;
    }
    
    // НОВАЯ ФУНКЦИЯ: проверка на технические термины
    function containsTechnicalTerms(text) {
        if (!text || text.length === 0) return false;
        
        var textLower = text.toLowerCase();
        
        for (var i = 0; i < technicalTerms.length; i++) {
            var term = technicalTerms[i].toLowerCase();
            // Используем indexOf для строки (разрешено в IE6)
            if (textLower.indexOf(term) !== -1) {
                return true;
            }
        }
        
        return false;
    }
    
    // НОВАЯ ФУНКЦИЯ: проверка, является ли число месяцом (1-12)
    function isMonthNumber(num) {
        if (!num || num.length === 0) return false;
        
        // Пытаемся преобразовать в число
        var n = parseInt(num, 10);
        if (isNaN(n)) return false;
        
        return n >= 1 && n <= 12;
    }
    
    // НОВАЯ ФУНКЦИЯ: проверка, является ли число днем месяца (1-31)
    function isDayNumber(num) {
        if (!num || num.length === 0) return false;
        
        var n = parseInt(num, 10);
        if (isNaN(n)) return false;
        
        return n >= 1 && n <= 31;
    }
    
    // УЛУЧШЕННАЯ ФУНКЦИЯ поиска числительных
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
    
    // УЛУЧШЕННАЯ ФУНКЦИЯ проверки, является ли текст "чистой датой"
    function isCleanDate(text) {
        if (!text || text.length === 0) return false;
        
        // Сначала проверяем наличие технических терминов
        if (containsTechnicalTerms(text)) {
            return false; // Если есть "версия" и т.д. - это не дата!
        }
        
        // Проверяем наличие дня недели
        var hasWeekDay = containsWholeWord(text, weekDays);
        
        // Проверяем наличие месяца в тексте
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
        
        // УЛУЧШЕННЫЕ ПАТТЕРНЫ ДАТ:
        
        // 1. Полные даты с годом: 25.12.1972, 25-12-1972
        var datePattern1 = /\b(\d{1,2})\s*[\.\-]\s*(\d{1,2})\s*[\.\-]\s*(\d{2,4})\b/;
        
        // 2. Даты с месяцем в тексте и числом: 25 декабря, декабрь 25
        var datePattern2 = /\b(\d{1,2})\s*(-го|-го\.|\.)?\s+[а-я]+|[а-я]+\s+(\d{1,2})\b/i;
        
        // 3. Даты с годом и месяцем в тексте: 1847 год, декабрь, 29
        var datePattern3 = /\b(\d{4})\s*(год)?\s*[,\s]\s*[а-я]+\s*,\s*(\d{1,2})\b/i;
        
        // 4. Даты с годом, месяцем и числом: 1847, декабрь, 29
        var datePattern4 = /\b(\d{4})\s*,\s*[а-я]+\s*,\s*(\d{1,2})\b/i;
        
        // 5. Новый паттерн: год месяц число (разные разделители)
        var datePattern5 = /\b(\d{4})\s*(год)?[\s,]*([а-я]+)[\s,]*(\d{1,2})\b/i;
        
        // 6. Новый паттерн: месяц число год
        var datePattern6 = /\b([а-я]+)\s+(\d{1,2})\s*(года?)?\s*(\d{4})?\b/i;
        
        // 7. Просто число.число - проверяем что это может быть дата
        var datePattern7 = /\b(\d{1,2})\s*[\.\-]\s*(\d{1,2})\b/;
        
        // Проверяем паттерны
        var match;
        
        // Паттерн 1: дд.мм.гггг
        match = datePattern1.exec(text);
        if (match) {
            var day = match[1];
            var monthNum = match[2];
            // Проверяем что это реальная дата: день 1-31, месяц 1-12
            if (isDayNumber(day) && isMonthNumber(monthNum)) {
                return true;
            }
        }
        
        // Паттерн 2: число + месяц в тексте
        if (datePattern2.test(text) && hasMonth) {
            return true;
        }
        
        // Паттерн 3: год (возможно с "год"), месяц, число
        match = datePattern3.exec(text);
        if (match) {
            var year = match[1];
            var day = match[3];
            if (year && day && hasMonth) {
                return true;
            }
        }
        
        // Паттерн 4: год, месяц, число (без слова "год")
        match = datePattern4.exec(text);
        if (match) {
            var year = match[1];
            var day = match[3];
            if (year && day && hasMonth) {
                return true;
            }
        }
        
        // Паттерн 5: год (возможно с "год") месяц число (гибкие разделители)
        match = datePattern5.exec(text);
        if (match) {
            var year = match[1];
            var day = match[4];
            if (year && day && hasMonth) {
                return true;
            }
        }
        
        // Паттерн 6: месяц число (возможно год)
        match = datePattern6.exec(text);
        if (match) {
            var day = match[2];
            if (day && hasMonth) {
                return true;
            }
        }
        
        // Паттерн 7: просто число.число - ТОЛЬКО если это может быть дата
        match = datePattern7.exec(text);
        if (match) {
            var firstNum = match[1];
            var secondNum = match[2];
            
            // Это дата только если:
            // 1. Первое число - день (1-31) И второе число - месяц (1-12)
            // 2. ИЛИ есть контекст (месяц в тексте или день недели)
            if (isDayNumber(firstNum) && isMonthNumber(secondNum)) {
                return true; // Например: 25.12, 1.1, 31.10
            }
            
            // Если второе число 0 (1.0, 2.0) - это НЕ дата!
            var secondNumInt = parseInt(secondNum, 10);
            if (secondNumInt === 0) {
                return false; // 1.0, 2.0 и т.д. - НЕ даты!
            }
            
            // Если есть контекст даты
            if (hasMonth || hasWeekDay) {
                return true;
            }
        }
        
        // Датой считаем если:
        // 1. Есть месяц в тексте И есть цифры (любые)
        if (hasMonth && hasDigits) {
            return true;
        }
        
        // 2. Есть день недели И есть цифры
        if (hasWeekDay && hasDigits) {
            return true;
        }
        
        // 3. Есть год (4 цифры) И месяц в тексте
        var yearPattern = /\b\d{4}\b/;
        if (yearPattern.test(text) && hasMonth) {
            return true;
        }
        
        return false;
    }
    
    // Функция проверки, является ли текст "чистым временем"
    function isCleanTime(text) {
        if (!text || text.length === 0) return false;
        
        // Проверяем наличие технических терминов
        if (containsTechnicalTerms(text)) {
            return false;
        }
        
        // Проверяем наличие дня недели
        var hasWeekDay = containsWholeWord(text, weekDays);
        
        // Проверяем форматы времени
        var timePattern1 = /\b\d{1,2}\s*[:]\s*\d{2}\b/; // 21:00, 05:45
        var timePattern2 = /\b\d{1,2}\s*[\-]\s*\d{2}\b/; // 20-15, 23-59
        var timePattern3 = /\b\d{1,2}\s*[ч]\s*\d{2}\b/; // 21 ч 00
        
        var isTimePattern = timePattern1.test(text) || timePattern2.test(text) || timePattern3.test(text);
        
        // Временем считаем если:
        // 1. Есть паттерн времени
        // 2. Может быть с днем недели
        if (isTimePattern) {
            return true;
        }
        
        return false;
    }
    
    // УЛУЧШЕННАЯ ГЛАВНАЯ ФУНКЦИЯ ПРОВЕРКИ КЛЮЧЕВЫХ СЛОВ
    function containsKeywords(text) {
        if (!text || text.length === 0) return false;
        
        // ШАГ 0: Проверяем технические термины СРАЗУ
        if (containsTechnicalTerms(text)) {
            return false; // НЕМЕДЛЕННО отбрасываем если есть "версия", "ver" и т.д.
        }
        
        // ШАГ 1: Проверяем местоимения СРАЗУ (самый важный фильтр!)
        if (containsPronouns(text)) {
            return false; // НЕМЕДЛЕННО отбрасываем если есть местоимения
        }
        
        // ШАГ 2: Проверяем наличие ЕДИНИЦЫ ВРЕМЕНИ (обязательно)
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
        
        // ШАГ 3: Проверяем наличие ХАРАКТЕРИСТИКИ ВРЕМЕНИ
        var hasTimeCharacteristic = containsWholeWord(text, timeCharacteristics);
        
        // ШАГ 4: Проверяем числительные
        var hasNumeral = containsNumeral(text);
        
        // ШАГ 5: Проверяем другие признаки
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
        filteredByTechnicalTerms: 0,
        skippedInProtectedContainers: 0,
        skippedNotesSection: 0,
        skippedCommentsSection: 0,
        skippedAnnotation: 0,
        skippedHistory: 0
    };
    
    var candidates = [];
    var allParagraphs = document.getElementsByTagName("P");
    
    for (var i = 0; i < allParagraphs.length; i++) {
        var paragraph = allParagraphs[i];
        
        // ВАЖНО: сначала проверяем, не находится ли элемент в защищенном контейнере
        if (isSkipElement(paragraph)) {
            // Статистика по пропущенным элементам
            var parent = paragraph.parentNode;
            while (parent && parent.tagName && parent.tagName.toUpperCase() !== "DIV") {
                parent = parent.parentNode;
            }
            
            if (parent && parent.className) {
                var parentClassName = parent.className.toString().toLowerCase();
                if (parentClassName === "annotation") stats.skippedAnnotation++;
                else if (parentClassName === "history") stats.skippedHistory++;
                else if (parent.getAttribute("fbname")) {
                    var fbname = parent.getAttribute("fbname").toLowerCase();
                    if (fbname === "notes") stats.skippedNotesSection++;
                    else if (fbname === "comments") stats.skippedCommentsSection++;
                }
            }
            
            stats.skippedInProtectedContainers++;
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
        
        // Считаем сколько отфильтровано техническими терминами
        if (containsTechnicalTerms(normalizedText)) {
            stats.filteredByTechnicalTerms++;
            continue;
        }
        
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
        var noResultsMsg = "Создать подзаголовки из абзацев, содержащих указание времени\n";
        noResultsMsg += "ver. 4.7\n\n";
        noResultsMsg += "Потенциальных подзаголовков не найдено.\n";
        
        if (showFullStats === 1) {
            noResultsMsg += "\nНастройки обработки разделов:\n";
            noResultsMsg += "• Основной раздел: ВКЛЮЧЕН\n";
            noResultsMsg += "• Раздел сносок (notes): " + (processNotesSection === 1 ? "ВКЛЮЧЕН" : "ВЫКЛЮЧЕН") + "\n";
            noResultsMsg += "• Раздел комментариев (comments): " + (processCommentsSection === 1 ? "ВКЛЮЧЕН" : "ВЫКЛЮЧЕН") + "\n\n";
            
            noResultsMsg += "Фильтрация:\n";
            var totalFiltered = stats.filteredByPronouns + stats.filteredByLength + stats.filteredByDot + 
                               stats.filteredBySentences + stats.filteredCleanDates + stats.filteredCleanTime +
                               stats.filteredByTechnicalTerms;
            noResultsMsg += "Всего отфильтровано: " + totalFiltered + "\n";
            noResultsMsg += "Из них отброшено:\n";
            if (stats.filteredByTechnicalTerms > 0) noResultsMsg += "Техническими терминами: " + stats.filteredByTechnicalTerms + "\n";
            if (stats.filteredByPronouns > 0) noResultsMsg += "Местоимениями: " + stats.filteredByPronouns + "\n";
            if (stats.filteredByLength > 0) noResultsMsg += "По длине: " + stats.filteredByLength + "\n";
            if (stats.filteredByDot > 0) noResultsMsg += "По точке в конце: " + stats.filteredByDot + "\n";
            if (stats.filteredBySentences > 0) noResultsMsg += "По количеству предложений: " + stats.filteredBySentences + "\n";
            if (stats.filteredCleanDates > 0) noResultsMsg += "Чистые даты (не прошли фильтры): " + stats.filteredCleanDates + "\n";
            if (stats.filteredCleanTime > 0) noResultsMsg += "Чистое время (не прошли фильтры): " + stats.filteredCleanTime + "\n";
            
            noResultsMsg += "\nПропущено элементов в специальных разделах:\n";
            if (stats.skippedAnnotation > 0) noResultsMsg += "• В аннотации: " + stats.skippedAnnotation + "\n";
            if (stats.skippedHistory > 0) noResultsMsg += "• В истории: " + stats.skippedHistory + "\n";
            if (stats.skippedNotesSection > 0) noResultsMsg += "• В разделе сносок (notes): " + stats.skippedNotesSection + "\n";
            if (stats.skippedCommentsSection > 0) noResultsMsg += "• В разделе комментариев (comments): " + stats.skippedCommentsSection + "\n";
            if (stats.skippedInProtectedContainers > 0) noResultsMsg += "• Всего пропущено в защищенных контейнерах: " + stats.skippedInProtectedContainers + "\n";
        }
        
        MsgBox(noResultsMsg);
        return;
    }
    
    var message = "Создать подзаголовки из абзацев, содержащих указание времени\n";
    message += "ver. 4.7\n\n";
    
    if (showFullStats === 1) {
        message += "НАСТРОЙКИ РАЗДЕЛОВ:\n";
        message += "• Основной раздел: ВКЛЮЧЕН\n";
        message += "• Раздел сносок (notes): " + (processNotesSection === 1 ? "ВКЛЮЧЕН" : "ВЫКЛЮЧЕН") + "\n";
        message += "• Раздел комментариев (comments): " + (processCommentsSection === 1 ? "ВКЛЮЧЕН" : "ВЫКЛЮЧЕН") + "\n\n";
        
        message += "НАСТРОЙКИ ОБРАБОТКИ:\n";
        message += "После заголовков - макс. " + maxLengthAfterTitle + " симв., " + maxSentencesAfterTitle + " предл., точка: " + (allowDotAfterTitle === 1 ? "ДА" : "НЕТ") + "\n";
        message += "              даты до " + maxDateLengthAfterTitle + " симв., точка: " + (allowDotForDatesAfterTitle === 1 ? "ДА" : "НЕТ") + "\n";
        message += "              время до " + maxTimeLengthAfterTitle + " симв., точка: " + (allowDotForTimeAfterTitle === 1 ? "ДА" : "НЕТ") + "\n";
        message += "В обычном тексте - макс. " + maxLengthInText + " симв., " + maxSentencesInText + " предл., точка: " + (allowDotInText === 1 ? "ДА" : "НЕТ") + "\n";
        message += "              даты до " + maxDateLengthInText + " симв., точка: " + (allowDotForDatesInText === 1 ? "ДА" : "НЕТ") + "\n";
        message += "              время до " + maxTimeLengthInText + " симв., точка: " + (allowDotForTimeInText === 1 ? "ДА" : "НЕТ") + "\n\n";
    }
    
    message += "РЕЗУЛЬТАТЫ ПОИСКА:\n";
    message += "Найдено потенциальных подзаголовков - " + stats.totalFound + "\n\n";
    
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
                           stats.filteredBySentences + stats.filteredCleanDates + stats.filteredCleanTime +
                           stats.filteredByTechnicalTerms;
        
        message += "ФИЛЬТРАЦИЯ (отброшено): " + totalFiltered + "\n";
        message += "Из них отброшено:\n";
        
        var filterSum = 0;
        if (stats.filteredByTechnicalTerms > 0) {
            message += "Техническими терминами: " + stats.filteredByTechnicalTerms + "\n";
            filterSum += stats.filteredByTechnicalTerms;
        }
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
        
        message += "Пропущено элементов в специальных разделах:\n";
        if (stats.skippedAnnotation > 0) message += "• В аннотации: " + stats.skippedAnnotation + "\n";
        if (stats.skippedHistory > 0) message += "• В истории: " + stats.skippedHistory + "\n";
        if (stats.skippedNotesSection > 0) message += "• В разделе сносок (notes): " + stats.skippedNotesSection + "\n";
        if (stats.skippedCommentsSection > 0) message += "• В разделе комментариев (comments): " + stats.skippedCommentsSection + "\n";
        if (stats.skippedInProtectedContainers > 0) message += "• Всего пропущено в защищенных контейнерах: " + stats.skippedInProtectedContainers + "\n";
        
        message += "\n";
    }
    
    message += "Итого:\n";
    message += "Всего будет расставлено подзаголовков - " + stats.totalFound + "\n\n";
    message += "Расставить подзаголовки?";
    
    var response = window.external.AskYesNo(message);
    
    if (response !== 1) {
        MsgBox("Отменено пользователем.");
        return;
    }
    
    var startTime = new Date().getTime();
    window.external.BeginUndoUnit(document, "Создать подзаголовки из абзацев со временем");
    
    // Просто меняем класс на "subtitle" для всех найденных кандидатов
    // Обрабатываем с конца к началу для безопасности
    for (var i = candidates.length - 1; i >= 0; i--) {
        var candidate = candidates[i];
        if (!isSkipElement(candidate.element)) {
            candidate.element.className = "subtitle";
            stats.processed++;
        } else {
            stats.skippedInProtectedContainers++;
        }
    }
    
    window.external.EndUndoUnit(document);
    
    var endTime = new Date().getTime();
    var elapsed = ((endTime - startTime) / 1000).toFixed(2);
    
    var finalMessage = "Создать подзаголовки из абзацев, содержащих указание времени\n";
    finalMessage += "ver. 4.7\n\n";
    finalMessage += "ИТОГИ ОБРАБОТКИ:\n";
    finalMessage += "Найдено кандидатов: " + stats.totalFound + "\n";
    finalMessage += "Преобразовано в подзаголовки: " + stats.processed + "\n";
    
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
                           stats.filteredBySentences + stats.filteredCleanDates + stats.filteredCleanTime +
                           stats.filteredByTechnicalTerms;
        finalMessage += "Всего отфильтровано: " + totalFiltered + "\n";
        if (stats.filteredByTechnicalTerms > 0) finalMessage += "Техническими терминами: " + stats.filteredByTechnicalTerms + "\n";
        if (stats.filteredByPronouns > 0) finalMessage += "Местоимениями: " + stats.filteredByPronouns + "\n";
        if (stats.filteredByLength > 0) finalMessage += "По длине: " + stats.filteredByLength + "\n";
        if (stats.filteredByDot > 0) finalMessage += "По точке в конце: " + stats.filteredByDot + "\n";
        if (stats.filteredBySentences > 0) finalMessage += "По количеству предложений: " + stats.filteredBySentences + "\n";
        if (stats.filteredCleanDates > 0) finalMessage += "Чистые даты (не прошли фильтры): " + stats.filteredCleanDates + "\n";
        if (stats.filteredCleanTime > 0) finalMessage += "Чистое время (не прошли фильтры): " + stats.filteredCleanTime + "\n";
        
        finalMessage += "\nПропущено элементов в специальных разделах:\n";
        if (stats.skippedAnnotation > 0) finalMessage += "• В аннотации: " + stats.skippedAnnotation + "\n";
        if (stats.skippedHistory > 0) finalMessage += "• В истории: " + stats.skippedHistory + "\n";
        if (stats.skippedNotesSection > 0) finalMessage += "• В разделе сносок (notes): " + stats.skippedNotesSection + "\n";
        if (stats.skippedCommentsSection > 0) finalMessage += "• В разделе комментариев (comments): " + stats.skippedCommentsSection + "\n";
        
        finalMessage += "\nВремя выполнения: " + elapsed + " сек\n\n";
    } else {
        finalMessage += "Время выполнения: " + elapsed + " сек\n\n";
    }
    
    MsgBox(finalMessage);
}
