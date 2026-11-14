// Скрипт "Унифицировать сокращения в примечаниях и комментариях" для редактора FBE
// version 3.0
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для приведения к однотипному варианту оформления
// в разделах примечаний и комментариев стандартных случаев типа:
// (примеч. автора), (Прим. автора), (прим. ред.), (примеч. перев).
// а также указания языков (рус), (англ), (нем.)... с конце обычных абзацев в соответствующих разделах.
// Скрипт оформляет все это однотипно - согласно выбору в настройках.
// Скрипт НЕ дописывает текст, а только меняет регистр в отдельных случаях,
// делает одинаковым форматирование тэгами курсива или жирности
// и расставляет отсутствующие точки - внутри скобок - для сокращений и в концах абзацев.
// Скрипт показывает анализ примечаний и комментариев в начале обработки
// и финальную подробную статистику с точным указанием всех изменений.
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).
// ---------------------------------------------------------------
// Примеры унификации:
// (Англ) ---> (англ.) - изменен регистр и добавлена точка внутри скобок.
// (примеч перев) ---> (Примеч. перев.) - изменен регистр и добавлены 2 точки к сокращениям внутри скобок.
// Дополнительно текст в скобках оформлен курсивом.
// ---------------------------------------------------------------
// В общем, задача скрипта - сделать разделы примечаний и комментариев красиво и единообразно оформленными.

// В скрипте используется список принятых сокращений для основных языков (176+ вариантов сокращений)
// и список вариантов написаний случаев (с точкой или без) всяческих
// прим ред, примеч. переводчика, примечания редакции и все такое (80+ вариантов написаний)
// Оба списка можно пополнять если попадается что-то другое типичное.

// version 3.0, 22.01.2026
//======================================

function Run() {
var scriptName = "Унифицировать сокращения в примечаниях и комментариях";
var version = "3.0";

// НАСТРОЙКИ СКРИПТА
// ==================================================


// 1. Режим статистики: 1 - показывать, 0 - тихий режим
var showStatistics = 1; // По умолчанию 1

// 2. Какие разделы обрабатывать:
//    0 - только раздел сносок (примечаний)
//    1 - разделы примечаний и комментариев (если есть)
//    2 - только раздел комментариев
var processMode = 1; // По умолчанию 1

// 3. Форматирование сокращений в скобках:
//    0 - жирным (STRONG)
//    1 - курсивом (EM)
var formatStyle = 1; // По умолчанию 1 (курсив)

// 4. Регистр для языков в скобках:
//    0 - оставить без изменения
//    1 - нормализовать (все буквы маленькие)
var normalizeLanguageCase = 1; // По умолчанию 1

// 5. Регистр для примечаний в скобках:
//    0 - оставить без изменения
//    1 - нормализовать (первая буква заглавная)
var normalizeNoteCase = 1; // По умолчанию 1

// 6. Точки в концах абзацев:
//    0 - не расставлять
//    1 - расставлять с форматированием
var addParagraphDots = 1; // По умолчанию 1

// 7. Точки внутри круглых скобок:
//    0 - не расставлять
//    1 - расставлять с форматированием
var addBracketDots = 1; // По умолчанию 1

// 8. Максимальное количество номеров примечаний или комментариев
// для вывода в статистике
var maxNumbersToShow = 50; // По умолчанию 50

// ==================================================
// СПИСКИ ДЛЯ ОБРАБОТКИ
// ==================================================

// Языки (полные списки)

// Европейские языки (60 вариантов)
var europeanLanguages = ["алб","алб.","албан","албан.","анг","анг.","англ","англ.","босн","босн.","болг","болг.","вен","вен.","венг","венг.","греч","греч.","дат","дат.","исп","исп.","итал","итал.","ит","ит.","исл","исл.","лат","лат.","макед","макед.","нем","нем.","нид","нид.","норв","норв.","польск","польск.","пол","пол.","порт","порт.","рум","рум.","серб","серб.","слов","слов.","словац","словац.","словен","словен.","тур","тур.","фин","фин.","фр","фр.","франц","франц.","хорв","хорв.","чеш","чеш.","швед","швед."];

// Языки СССР (68 вариантов)
var ussrLanguages = ["аз","аз.","азер","азер.","азерб","азерб.","арм","арм.","армян","армян.","бел","бел.","белор","белор.","белорус","белорус.","груз","груз.","грузин","грузин.","каз","каз.","казах","казах.","кирг","кирг.","киргиз","киргиз.","лат","лат.","латыш","латыш.","лит","лит.","литов","литов.","молд","молд.","молдав","молдав.","рус","рус.","руск","руск.","тадж","тадж.","таджик","таджик.","турк","турк.","туркм","туркм.","туркмен","туркмен.","узб","узб.","узбек","узбек.","укр","укр.","эст","эст.","эстон","эстон"];

// Азиатские языки (32 варианта)
var asianLanguages = ["вьет","вьет.","вьетн","вьетн.","ивр","ивр.","иврит","иврит.","идиш","идиш.","индон","индон.","индонез","индонез.","кит","кит.","кор","кор.","малай","малай.","перс","перс.","персид","персид.","тай","тай.","тайск","тайск.","хинди","хинди.","яп","яп."];

// Арабские языки (16 вариантов)
var arabicLanguages = ["ар","ар.","араб","араб.","арабск","арабск.","афг","афг.","афганск","афганск.","урду","урду","фарси"];

// Объединенный список языков (176 вариантов)
var languages = europeanLanguages.concat(ussrLanguages, asianLanguages, arabicLanguages);

// Полный список вариантов примечаний (99 вариантов)
var notes = [
    // Автор (25 вариантов)
    "прим авт", "прим авт.", "прим. авт", "прим. авт.", 
    "прим автора", "прим. автора", "прим авторов", "прим. авторов",
    "примеч авт", "примеч авт.", "примеч. авт", "примеч. авт.",
    "примеч автора", "примеч. автора", "примеч авторов", "примеч. авторов",
    "примечание авт", "примечание авт.", "примечание автора", "примечания авт",
    "примечания авт.", "примечания автора", "примечание авторов", "примечания авторов",
    
    // Переводчик (27 вариантов)
    "прим пер", "прим пер.", "прим. пер", "прим. пер.",
    "прим перев", "прим перев.", "прим. перев", "прим. перев.",
    "прим переводчика", "прим. переводчика", "прим переводчиков", "прим. переводчиков",
    "примеч пер", "примеч пер.", "примеч. пер", "примеч. пер.",
    "примеч перев", "примеч перев.", "примеч. перев", "примеч. перев.",
    "примеч переводчика", "примеч. переводчика", "примеч переводчиков", "примеч. переводчиков",
    "примечание пер", "примечание пер.", "примечание перев", "примечания перев",
    "примечание переводчика", "примечание переводчиков", "примечания переводчика", "примечания переводчиков",
    
    // Редактор (27 вариантов)
    "прим ред", "прим ред.", "прим. ред", "прим. ред.",
    "прим редактора", "прим. редактора", "прим редакторов", "прим. редакторов",
    "примеч ред", "примеч ред.", "примеч. ред", "примеч. ред.",
    "примеч редактора", "примеч. редактора", "прим редакции", "прим. редакции",
    "примечание ред", "примечание ред.", "примечания ред", "примечания ред.",
    "примечание редактора", "примечания редактора", "примечания редакторов", "примечание редакторов",
    "примечание редакции", "примечания редакции",
    
    // Составитель (8 вариантов)
    "прим сост", "прим сост.", "прим. сост", "прим. сост.",
    "примеч сост", "примеч сост.", "примеч. сост", "примеч. сост."
];

// ==================================================
// НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
// ==================================================

// Получаем неразрывный пробел
var nbspChar, nbspEntity;
try { 
    nbspChar = window.external.GetNBSP(); 
    nbspEntity = (nbspChar.charCodeAt(0) == 160) ? "&nbsp;" : nbspChar; 
}
catch(e) { 
    nbspChar = String.fromCharCode(160); 
    nbspEntity = "&nbsp;";
}

// Вспомогательные функции (без рекурсии!)
function getElementText(element) {
    if (!element) return "";
    if (typeof element.innerText !== "undefined") return element.innerText;
    var text = "";
    var nodes = [element];
    var i = 0;
    while (i < nodes.length) {
        var node = nodes[i];
        i++;
        if (node.nodeType == 3) {
            text += node.nodeValue;
        } else if (node.nodeType == 1) {
            for (var j = 0; j < node.childNodes.length; j++) {
                nodes.push(node.childNodes[j]);
            }
        }
    }
    return text;
}

function endsWithPunctuation(str) {
    if (str.length == 0) return false;
    var punctuation = ".?!,;:…";
    var lastChar = str.charAt(str.length - 1);
    for (var i = 0; i < punctuation.length; i++) {
        if (lastChar == punctuation.charAt(i)) return true;
    }
    return false;
}

function addFormattedDot(element) {
    if (!element) return;
    element.innerHTML = element.innerHTML + ".";
}

function getSectionNumber(section) {
    if (!section) return "";
    var titles = section.getElementsByTagName("DIV");
    for (var i = 0; i < titles.length; i++) {
        if (titles[i].className == "title") {
            var paragraphs = titles[i].getElementsByTagName("P");
            for (var j = 0; j < paragraphs.length; j++) {
                var text = getElementText(paragraphs[j]);
                if (text && text.length > 0) return text;
            }
        }
    }
    return "";
}

function extractTextFromElement(element) {
    var text = "";
    var nodes = [element];
    var i = 0;
    while (i < nodes.length) {
        var node = nodes[i];
        i++;
        if (node.nodeType == 3) {
            text += node.nodeValue;
        } else if (node.nodeType == 1) {
            for (var j = 0; j < node.childNodes.length; j++) {
                nodes.push(node.childNodes[j]);
            }
        }
    }
    return text;
}

function normalizeText(text, isLanguage) {
    var result = text;
    var dotsAdded = 0;
    var caseChanged = false;
    var changed = false;
    
    var words = text.split(" ");
    var newWords = [];
    
    for (var i = 0; i < words.length; i++) {
        var word = words[i];
        var originalWord = word;
        
        if (addBracketDots == 1) {
            if (isLanguage) {
                for (var j = 0; j < languages.length; j++) {
                    var lang = languages[j];
                    var langWithoutDot = lang.replace(/\.$/, '');
                    
                    if (word == langWithoutDot) {
                        word = langWithoutDot + ".";
                        dotsAdded++;
                        changed = true;
                        break;
                    }
                }
            } else {
                var abbreviations = ["прим", "примеч", "пер", "ред", "авт", "перев"];
                for (var j = 0; j < abbreviations.length; j++) {
                    if (word == abbreviations[j]) {
                        word = word + ".";
                        dotsAdded++;
                        changed = true;
                        break;
                    }
                }
            }
        }
        
        newWords.push(word);
    }
    
    result = newWords.join(" ");
    
    if (isLanguage && normalizeLanguageCase == 1) {
        var lowerResult = result.toLowerCase();
        if (lowerResult != result) {
            result = lowerResult;
            caseChanged = true;
            changed = true;
        }
    } else if (!isLanguage && normalizeNoteCase == 1) {
        if (result.length > 0) {
            var firstChar = result.charAt(0).toUpperCase();
            var rest = result.substring(1);
            if (firstChar != result.charAt(0)) {
                result = firstChar + rest;
                caseChanged = true;
                changed = true;
            }
        }
    }
    
    return {
        text: result,
        dotsAdded: dotsAdded,
        caseChanged: caseChanged,
        changed: changed
    };
}

// ==================================================
// ПОИСК РАЗДЕЛОВ
// ==================================================

var bodyDivs = document.getElementsByTagName("DIV");
var notesSection = null;
var commentsSection = null;

for (var i = 0; i < bodyDivs.length; i++) {
    var div = bodyDivs[i];
    if (div.className == "body" && div.getAttribute("fbname")) {
        var fbname = div.getAttribute("fbname");
        if (fbname == "notes") notesSection = div;
        else if (fbname == "comments") commentsSection = div;
    }
}

if (processMode == 0 && !notesSection) {
    MsgBox(scriptName + "\nver. " + version + "\n---------------------------------------\n\nНе найден раздел сносок (примечаний).");
    return;
}
if (processMode == 2 && !commentsSection) {
    MsgBox(scriptName + "\nver. " + version + "\n---------------------------------------\n\nНе найден раздел комментариев.");
    return;
}
if (processMode == 1 && !notesSection && !commentsSection) {
    MsgBox(scriptName + "\nver. " + version + "\n---------------------------------------\n\nНе найден раздел сносок (примечаний).\nНе найден раздел комментариев.");
    return;
}

// ==================================================
// АНАЛИЗ
// ==================================================

var analysisResults = { notes: { total: 0, sections: [] }, comments: { total: 0, sections: [] } };

function analyzeSection(section, sectionType) {
    if (!section) return;
    var sections = section.getElementsByTagName("DIV");
    for (var i = 0; i < sections.length; i++) {
        var subSection = sections[i];
        if (subSection.className == "section") {
            var text = getElementText(subSection);
            var hasMatches = false;
            
            for (var j = 0; j < languages.length; j++) {
                var lang = languages[j];
                if (text.indexOf("(" + lang + ")") != -1 || text.indexOf("(" + lang + " ") != -1) {
                    hasMatches = true;
                    break;
                }
            }
            
            if (!hasMatches) {
                for (var j = 0; j < notes.length; j++) {
                    var note = notes[j];
                    if (text.indexOf("(" + note + ")") != -1 || text.indexOf("(" + note + " ") != -1) {
                        hasMatches = true;
                        break;
                    }
                }
            }
            
            if (hasMatches) {
                analysisResults[sectionType].total++;
                var num = getSectionNumber(subSection);
                if (num) analysisResults[sectionType].sections.push(num);
            }
        }
    }
}

if (processMode == 0 || processMode == 1) analyzeSection(notesSection, "notes");
if (processMode == 2 || processMode == 1) analyzeSection(commentsSection, "comments");

var totalFound = analysisResults.notes.total + analysisResults.comments.total;
if (totalFound == 0) {
    MsgBox(scriptName + "\nver. " + version + "\n---------------------------------------\n\nПодходящих мест для обработки не найдено.");
    return;
}

// Показываем анализ и спрашиваем подтверждение
var analysisMessage = scriptName + "\nver. " + version + "\n---------------------------------------\n\nРезультаты анализа:\n\n";

if (notesSection && (processMode == 0 || processMode == 1)) {
    analysisMessage += "В разделе ПРИМЕЧАНИЙ:\n";
    analysisMessage += "Подходящих мест для обработки: " + analysisResults.notes.total + "\n";
    if (analysisResults.notes.sections.length > 0) {
        analysisMessage += "Номера примечаний: ";
        for (var i = 0; i < analysisResults.notes.sections.length && i < maxNumbersToShow; i++) {
            if (i > 0) analysisMessage += ", ";
            analysisMessage += analysisResults.notes.sections[i];
        }
        if (analysisResults.notes.sections.length > maxNumbersToShow) {
            analysisMessage += ", ... (еще " + (analysisResults.notes.sections.length - maxNumbersToShow) + ")";
        }
        analysisMessage += "\n";
    }
    analysisMessage += "\n";
}

if (notesSection && (processMode == 0 || processMode == 1) && commentsSection && (processMode == 2 || processMode == 1)) {
    analysisMessage += "==========================\n\n";
}

if (commentsSection && (processMode == 2 || processMode == 1)) {
    analysisMessage += "В разделе КОММЕНТАРИЕВ:\n";
    analysisMessage += "Подходящих мест для обработки: " + analysisResults.comments.total + "\n";
    if (analysisResults.comments.sections.length > 0) {
        analysisMessage += "Номера комментариев: ";
        for (var i = 0; i < analysisResults.comments.sections.length && i < maxNumbersToShow; i++) {
            if (i > 0) analysisMessage += ", ";
            analysisMessage += analysisResults.comments.sections[i];
        }
        if (analysisResults.comments.sections.length > maxNumbersToShow) {
            analysisMessage += ", ... (еще " + (analysisResults.comments.sections.length - maxNumbersToShow) + ")";
        }
        analysisMessage += "\n";
    }
    analysisMessage += "\n";
}

analysisMessage += "==========================\n\nНастройки:\n";
analysisMessage += "• Обрабатываем: " + (processMode == 0 ? "только примечания" : processMode == 1 ? "примечания и комментарии" : "только комментарии") + "\n";
analysisMessage += "• Форматирование: " + (formatStyle == 0 ? "жирным" : "курсивом") + "\n";
analysisMessage += "• Регистр языков: " + (normalizeLanguageCase == 0 ? "без изменения" : "нормализовать") + "\n";
analysisMessage += "• Регистр примечаний: " + (normalizeNoteCase == 0 ? "без изменения" : "нормализовать") + "\n";
analysisMessage += "• Точки в концах абзацев: " + (addParagraphDots == 0 ? "не расставлять" : "расставлять") + "\n";
analysisMessage += "• Точки внутри скобок: " + (addBracketDots == 0 ? "не расставлять" : "расставлять") + "\n\n";

var response = window.external.AskYesNo(analysisMessage + "Выполнить обработку?");
if (!response) {
    MsgBox(scriptName + "\nver. " + version + "\n---------------------------------------\n\nОбработка отменена пользователем.");
    return;
}

// ==================================================
// ОБРАБОТКА (БЕЗ РЕКУРСИИ!)
// ==================================================

// ТАЙМЕР СТАРТУЕТ ЗДЕСЬ - ПОСЛЕ ПОДТВЕРЖДЕНИЯ!
var startTime = new Date().getTime();
window.external.BeginUndoUnit(document, scriptName);

var stats = {
    notes: { dotsAdded: 0, caseChanged: 0, formatted: 0, paragraphDots: 0, sections: { dots: [], "case": [], format: [], paragraph: [] } },
    comments: { dotsAdded: 0, caseChanged: 0, formatted: 0, paragraphDots: 0, sections: { dots: [], "case": [], format: [], paragraph: [] } }
};

function processSection(section, sectionType) {
    if (!section) return;
    
    var sections = section.getElementsByTagName("DIV");
    for (var i = 0; i < sections.length; i++) {
        var subSection = sections[i];
        if (subSection.className == "section") {
            var sectionNumber = getSectionNumber(subSection);
            var sectionStats = { dots: false, "case": false, format: false, paragraph: false };
            var sectionBracketCount = 0;
            
            // Собираем ВСЕ элементы для обработки (без рекурсии)
            var allElements = [];
            var elementsToProcess = [subSection];
            var elemIdx = 0;
            
            while (elemIdx < elementsToProcess.length) {
                var currentElement = elementsToProcess[elemIdx];
                elemIdx++;
                
                if (!currentElement || currentElement.nodeType != 1) continue;
                
                var className = currentElement.className || "";
                var nodeName = currentElement.nodeName;
                
                // Пропускаем специальные элементы
                if (!(className == "poem" || className == "cite" || 
                      className == "epigraph" || className == "annotation" ||
                      className == "stanza" || className == "history" ||
                      nodeName == "A")) {
                    allElements.push(currentElement);
                }
                
                // Добавляем детей в очередь обработки
                var children = currentElement.childNodes;
                for (var j = 0; j < children.length; j++) {
                    elementsToProcess.push(children[j]);
                }
            }
            
            // Обрабатываем все элементы
            for (var elemIdx = 0; elemIdx < allElements.length; elemIdx++) {
                var element = allElements[elemIdx];
                var html = element.innerHTML;
                var changed = false;
                
                // Ищем все скобки в этом элементе
                var bracketStart = 0;
                while ((bracketStart = html.indexOf("(", bracketStart)) != -1) {
                    var bracketEnd = html.indexOf(")", bracketStart);
                    if (bracketEnd == -1) {
                        bracketStart++;
                        continue;
                    }
                    
                    var bracketContent = html.substring(bracketStart + 1, bracketEnd);
                    var extractedText = bracketContent;
                    
                    // Извлекаем текст из тегов если нужно
                    var hasTags = bracketContent.indexOf("<") != -1 && bracketContent.indexOf(">") != -1;
                    if (hasTags) {
                        var tempDiv = document.createElement("DIV");
                        tempDiv.innerHTML = bracketContent;
                        extractedText = extractTextFromElement(tempDiv);
                    }
                    
                    extractedText = extractedText.replace(/^\s+|\s+$/g, '');
                    
                    // Проверяем языки
                    var isLanguage = false;
                    var shouldProcess = false;
                    
                    for (var j = 0; j < languages.length; j++) {
                        var lang = languages[j];
                        var langWithoutDot = lang.replace(/\.$/, '');
                        
                        if (extractedText == lang || extractedText == langWithoutDot) {
                            isLanguage = true;
                            shouldProcess = true;
                            break;
                        }
                        
                        if (extractedText.indexOf(lang + " ") == 0 || 
                            extractedText.indexOf(langWithoutDot + " ") == 0) {
                            isLanguage = true;
                            shouldProcess = true;
                            break;
                        }
                    }
                    
                    // Проверяем примечания
                    if (!isLanguage) {
                        for (var j = 0; j < notes.length; j++) {
                            var note = notes[j];
                            var noteWithoutDot = note.replace(/\.$/, '');
                            
                            if (extractedText == note || extractedText == noteWithoutDot) {
                                shouldProcess = true;
                                break;
                            }
                            
                            if (extractedText.indexOf(note + " ") == 0 || 
                                extractedText.indexOf(noteWithoutDot + " ") == 0) {
                                shouldProcess = true;
                                break;
                            }
                        }
                    }
                    
                    if (shouldProcess) {
                        sectionBracketCount++;
                        
                        var normalized = normalizeText(extractedText, isLanguage);
                        
                        if (normalized.changed || formatStyle == 0 || formatStyle == 1) {
                            if (normalized.dotsAdded > 0) {
                                stats[sectionType].dotsAdded += normalized.dotsAdded;
                                sectionStats.dots = true;
                            }
                            if (normalized.caseChanged) {
                                stats[sectionType].caseChanged++;
                                sectionStats["case"] = true;
                            }
                            
                            var newContent;
                            if (formatStyle == 0) {
                                newContent = "<STRONG>(" + normalized.text + ")</STRONG>";
                            } else {
                                newContent = "<EM>(" + normalized.text + ")</EM>";
                            }
                            
                            html = html.substring(0, bracketStart) + newContent + 
                                   html.substring(bracketEnd + 1);
                            changed = true;
                            bracketStart += newContent.length;
                        } else {
                            bracketStart = bracketEnd + 1;
                        }
                    } else {
                        bracketStart = bracketEnd + 1;
                    }
                }
                
                if (changed) {
                    element.innerHTML = html;
                }
            }
            
            // Добавляем статистику по скобкам
            if (sectionBracketCount > 0) {
                stats[sectionType].formatted += sectionBracketCount;
                sectionStats.format = true;
            }
            
            // Обработка точек в концах абзацев
            if (addParagraphDots == 1) {
                var paragraphs = subSection.getElementsByTagName("P");
                for (var j = 0; j < paragraphs.length; j++) {
                    var paragraph = paragraphs[j];
                    
                    var skip = false;
                    var parent = paragraph.parentNode;
                    while (parent && parent != subSection) {
                        if (parent.nodeType == 1) {
                            var className = parent.className;
                            if (className == "poem" || className == "cite" || 
                                className == "epigraph" || className == "annotation" ||
                                className == "stanza" || className == "history") {
                                skip = true;
                                break;
                            }
                        }
                        parent = parent.parentNode;
                    }
                    
                    if (!skip) {
                        var parentElement = paragraph.parentNode;
                        if (!(parentElement && parentElement.className == "title") && 
                            paragraph.className != "subtitle") {
                            
                            var text = getElementText(paragraph);
                            if (text.length > 0 && !endsWithPunctuation(text.replace(/\s+$/, ""))) {
                                addFormattedDot(paragraph);
                                stats[sectionType].paragraphDots++;
                                sectionStats.paragraph = true;
                            }
                        }
                    }
                }
            }
            
            // Сохраняем статистику
            if (sectionNumber) {
                if (sectionStats.dots) stats[sectionType].sections.dots.push(sectionNumber);
                if (sectionStats["case"]) stats[sectionType].sections["case"].push(sectionNumber);
                if (sectionStats.format) stats[sectionType].sections.format.push(sectionNumber);
                if (sectionStats.paragraph) stats[sectionType].sections.paragraph.push(sectionNumber);
            }
        }
    }
}

if (processMode == 0 || processMode == 1) processSection(notesSection, "notes");
if (processMode == 2 || processMode == 1) processSection(commentsSection, "comments");

window.external.EndUndoUnit(document);

// ==================================================
// СТАТИСТИКА
// ==================================================

var endTime = new Date().getTime();
var elapsed = (endTime - startTime) / 1000;

var finalMessage = scriptName + "\nver. " + version + "\n---------------------------------------\n\n";

if (notesSection && (processMode == 0 || processMode == 1)) {
    finalMessage += "Раздел ПРИМЕЧАНИЙ обработан.\n";
}
if (commentsSection && (processMode == 2 || processMode == 1)) {
    finalMessage += "Раздел КОММЕНТАРИЕВ обработан.\n";
}
finalMessage += "\nСтатистика изменений:\n\n";

function formatNumberList(numbers) {
    if (numbers.length == 0) return "нет";
    var result = "";
    for (var i = 0; i < numbers.length && i < maxNumbersToShow; i++) {
        if (i > 0) result += ", ";
        result += numbers[i];
    }
    if (numbers.length > maxNumbersToShow) {
        result += ", ... (еще " + (numbers.length - maxNumbersToShow) + ")";
    }
    return result;
}

// Удаляем дубликаты из списков
function removeDuplicates(arr) {
    var result = [];
    var seen = {};
    for (var i = 0; i < arr.length; i++) {
        if (!seen[arr[i]]) {
            seen[arr[i]] = true;
            result.push(arr[i]);
        }
    }
    return result;
}

if (notesSection && (processMode == 0 || processMode == 1)) {
    stats.notes.sections.dots = removeDuplicates(stats.notes.sections.dots);
    stats.notes.sections["case"] = removeDuplicates(stats.notes.sections["case"]);
    stats.notes.sections.format = removeDuplicates(stats.notes.sections.format);
    stats.notes.sections.paragraph = removeDuplicates(stats.notes.sections.paragraph);
    
    finalMessage += "ПРИМЕЧАНИЯ:\n";
    finalMessage += "• Расставлены точки в скобках: " + stats.notes.dotsAdded + "\n";
    if (stats.notes.dotsAdded > 0) finalMessage += "  Номера: " + formatNumberList(stats.notes.sections.dots) + "\n";
    finalMessage += "• Изменен регистр: " + stats.notes.caseChanged + "\n";
    if (stats.notes.caseChanged > 0) finalMessage += "  Номера: " + formatNumberList(stats.notes.sections["case"]) + "\n";
    finalMessage += "• Скобочных сокращений обработано: " + stats.notes.formatted + " (оформлены " + (formatStyle == 0 ? "жирным" : "курсивом") + ")\n";
    if (stats.notes.formatted > 0) finalMessage += "  Номера: " + formatNumberList(stats.notes.sections.format) + "\n";
    if (addParagraphDots == 1) {
        finalMessage += "• Точки в концах абзацев: " + stats.notes.paragraphDots + "\n";
        if (stats.notes.paragraphDots > 0) finalMessage += "  Номера: " + formatNumberList(stats.notes.sections.paragraph) + "\n";
    }
    finalMessage += "\n";
}

if (notesSection && (processMode == 0 || processMode == 1) && commentsSection && (processMode == 2 || processMode == 1)) {
    finalMessage += "==========================\n\n";
}

if (commentsSection && (processMode == 2 || processMode == 1)) {
    stats.comments.sections.dots = removeDuplicates(stats.comments.sections.dots);
    stats.comments.sections["case"] = removeDuplicates(stats.comments.sections["case"]);
    stats.comments.sections.format = removeDuplicates(stats.comments.sections.format);
    stats.comments.sections.paragraph = removeDuplicates(stats.comments.sections.paragraph);
    
    finalMessage += "КОММЕНТАРИИ:\n";
    finalMessage += "• Расставлены точки в скобках: " + stats.comments.dotsAdded + "\n";
    if (stats.comments.dotsAdded > 0) finalMessage += "  Номера: " + formatNumberList(stats.comments.sections.dots) + "\n";
    finalMessage += "• Изменен регистр: " + stats.comments.caseChanged + "\n";
    if (stats.comments.caseChanged > 0) finalMessage += "  Номера: " + formatNumberList(stats.comments.sections["case"]) + "\n";
    finalMessage += "• Скобочных сокращений обработано: " + stats.comments.formatted + " (оформлены " + (formatStyle == 0 ? "жирным" : "курсивом") + ")\n";
    if (stats.comments.formatted > 0) finalMessage += "  Номера: " + formatNumberList(stats.comments.sections.format) + "\n";
    if (addParagraphDots == 1) {
        finalMessage += "• Точки в концах абзацев: " + stats.comments.paragraphDots + "\n";
        if (stats.comments.paragraphDots > 0) finalMessage += "  Номера: " + formatNumberList(stats.comments.sections.paragraph) + "\n";
    }
    finalMessage += "\n";
}

finalMessage += "==========================\n\nНастройки использованы:\n";
finalMessage += "• Форматирование: " + (formatStyle == 0 ? "жирным (STRONG)" : "курсивом (EM)") + "\n";
finalMessage += "• Регистр языков: " + (normalizeLanguageCase == 0 ? "без изменения" : "нормализован") + "\n";
finalMessage += "• Регистр примечаний: " + (normalizeNoteCase == 0 ? "без изменения" : "нормализован") + "\n";
finalMessage += "• Точки в концах абзацев: " + (addParagraphDots == 0 ? "не расставлены" : "расставлены") + "\n";
finalMessage += "• Точки внутри скобок: " + (addBracketDots == 0 ? "не расставлены" : "расставлены") + "\n\n";

finalMessage += "Время выполнения: " + elapsed.toFixed(2) + " сек.";

if (showStatistics == 1) {
    MsgBox(finalMessage);
} else {
    MsgBox(scriptName + "\nver. " + version + "\n---------------------------------------\n\nОбработка завершена успешно.\n\nВремя выполнения: " + elapsed.toFixed(2) + " сек.");
    }
}
