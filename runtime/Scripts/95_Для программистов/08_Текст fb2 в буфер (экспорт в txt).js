// Скрипт "Текст fb2 в буфер (экспорт в txt) для редактора FBE
// version 2.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для копирования в буфер обмена текста fb2 документа.
// Фактически производится экспорт документа в txt фомат. 
// Настройки скрипта позволяют опционально:
// - создавать оглавление из имеющихся заголовков, 
// - копировать или пропускать разделы аннотации, хистори, сносок и комментариев,
// - обрамлять заголовки, подзаголовки и размеченные блочные элементы пустыми строками.
// Иллюстрации заменяются на их упоминание в соответствующих местах в скобках: [иллюстрация].
// Скрипт не вносит никаких изменений в fb2 документ.
// Режим работы: обычный или тихий.

// version 2.1, 28.05.2026
//======================================

function Run() {
    var scriptName = "Текст fb2 в буфер (экспорт в txt)";
    var version = "2.1";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима
    
    // Создавать оглавление из заголовков в начале текста
    var createTOC = 1; // 0 - нет, 1 - да
    
    // Копировать раздел аннотации (annotation)
    var processAnnotationSection = 1; // 0 - нет, 1 - да
    
    // Копировать раздел истории (History)
    var processHistorySection = 1; // 0 - нет, 1 - да
    
    // Копировать основной раздел
    var processMainSection = 1; // 0 - нет, 1 - да
    
    // Копировать раздел сносок (примечаний)
    var processNotesSection = 1; // 0 - нет, 1 - да
    
    // Копировать раздел комментариев
    var processCommentsSection = 1; // 0 - нет, 1 - да
    
    // Обрамлять заголовки пустыми строками
    var wrapTitles = 1; // 0 - нет, 1 - да
    
    // Обрамлять подзаголовки пустыми строками
    var wrapSubtitles = 1; // 0 - нет, 1 - да
    
    // Обрамлять другие блочные DIV-элементы пустыми строками (annotation, epigraph, poem, cite)
    var wrapDivElements = 1; // 0 - нет, 1 - да

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    var startTime = new Date();
    var totalParagraphs = 0;
    var totalChars = 0;
    var totalTitles = 0;
    var totalSubtitles = 0;
    var notesSections = 0;
    var commentsSections = 0;
    var resultText = "";
    var tocText = "";
    
    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================
    
    function getCleanText(el) {
        var txt = "";
        var kids = el.childNodes;
        for (var k = 0; k < kids.length; k++) {
            if (kids[k].nodeType == 3) txt += kids[k].nodeValue;
            else if (kids[k].nodeType == 1) {
                if (kids[k].nodeName == "IMG") txt += "[иллюстрация]";
                else if (kids[k].nodeName == "BR") txt += "\n";
                else txt += getCleanText(kids[k]);
            }
        }
        return txt;
    }
    
    function trimStr(str) {
        return str.replace(/^\s+/, "").replace(/\s+$/, "");
    }
    
    function endsWithBlankLine(txt) {
        if (txt.length == 0) return true;
        if (txt.length >= 2 && txt.substring(txt.length - 2) == "\n\n") return true;
        return false;
    }
    
    function appendBlankLine(txt) {
        if (txt.length == 0) return txt;
        while (txt.length > 1 && txt.substring(txt.length - 2) == "\n\n") {
            txt = txt.substring(0, txt.length - 1);
        }
        if (txt.substring(txt.length - 1) != "\n") txt += "\n";
        txt += "\n";
        return txt;
    }
    
    function extractTitleText(titleDiv) {
        var titleText = "";
        var pElements = titleDiv.getElementsByTagName("P");
        for (var i = 0; i < pElements.length; i++) {
            var pText = trimStr(getCleanText(pElements[i]));
            if (pText.length > 0) {
                if (i > 0) titleText += " \u2022 ";
                titleText += pText;
            }
        }
        return titleText;
    }
    
    function extractTitleTextForBody(titleDiv) {
        var titleText = "";
        var pElements = titleDiv.getElementsByTagName("P");
        for (var i = 0; i < pElements.length; i++) {
            var pText = trimStr(getCleanText(pElements[i]));
            if (pText.length > 0) titleText += pText + "\n";
            else titleText += "\n";
        }
        return titleText;
    }
    
    function getAuthorAndTitleFromDesc() {
        var authorName = "";
        var bookTitle = "";
        var desc = document.getElementById("fbw_desc");
        if (!desc) return { author: authorName, title: bookTitle };
        
        var tiAuthor = desc.all["tiAuthor"];
        if (tiAuthor) {
            var authorDivs = tiAuthor.getElementsByTagName("DIV");
            for (var j = 0; j < authorDivs.length; j++) {
                var div = authorDivs[j];
                var firstName = "", middleName = "", lastName = "";
                if (div.all["first"]) firstName = div.all["first"].value;
                if (div.all["middle"]) middleName = div.all["middle"].value;
                if (div.all["last"]) lastName = div.all["last"].value;
                var fullName = "";
                if (firstName != "") fullName += firstName;
                if (middleName != "") { if (fullName != "") fullName += " "; fullName += middleName; }
                if (lastName != "") { if (fullName != "") fullName += " "; fullName += lastName; }
                if (fullName != "") {
                    if (authorName != "") authorName += ", ";
                    authorName += fullName;
                }
            }
        }
        
        var tiTitle = desc.all["tiTitle"];
        if (tiTitle && tiTitle.value != "") bookTitle = trimStr(tiTitle.value);
        return { author: authorName, title: bookTitle };
    }
    
    function getAuthorAndTitleFromBody() {
        var authorName = "", bookTitle = "";
        var fbwBody = document.getElementById("fbw_body");
        if (!fbwBody) return { author: authorName, title: bookTitle };
        var bodyChildren = fbwBody.childNodes;
        var mainBody = null;
        for (var i = 0; i < bodyChildren.length; i++) {
            var child = bodyChildren[i];
            if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "body") {
                if ((child.getAttribute("fbname") || "") == "") { mainBody = child; break; }
            }
        }
        if (!mainBody) return { author: authorName, title: bookTitle };
        var bodyTitle = null;
        var kids = mainBody.childNodes;
        for (var k = 0; k < kids.length; k++) {
            if (kids[k].nodeType == 1 && kids[k].nodeName == "DIV" && kids[k].className == "title") {
                bodyTitle = kids[k]; break;
            }
        }
        if (!bodyTitle) return { author: authorName, title: bookTitle };
        var pElements = bodyTitle.getElementsByTagName("P");
        if (pElements.length == 1) bookTitle = trimStr(getCleanText(pElements[0]));
        else if (pElements.length >= 2) {
            authorName = trimStr(getCleanText(pElements[0]));
            bookTitle = trimStr(getCleanText(pElements[1]));
        }
        return { author: authorName, title: bookTitle };
    }
    
    // Подсчёт заголовков (отдельно от оглавления)
    function countTitles(container, skipFirstTitle) {
        var children = container.childNodes;
        var firstTitleSkipped = false;
        
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 1 && child.nodeName == "DIV") {
                if (child.className == "title") {
                    var parentClass = container.className;
                    if (parentClass == "body" && skipFirstTitle && !firstTitleSkipped) {
                        firstTitleSkipped = true;
                    } else if (parentClass == "section" || parentClass == "body") {
                        totalTitles++;
                    }
                } else if (child.className == "section") {
                    countTitlesInSection(child);
                }
            }
        }
    }
    
    function countTitlesInSection(section) {
        var children = section.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 1 && child.nodeName == "DIV") {
                if (child.className == "title") {
                    totalTitles++;
                } else if (child.className == "section") {
                    countTitlesInSection(child);
                }
            }
        }
    }
    
    // Формирование оглавления
    function buildTOC(container, indent, skipFirstTitle) {
        var children = container.childNodes;
        var firstTitleSkipped = false;
        
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 1 && child.nodeName == "DIV") {
                if (child.className == "title") {
                    var parentClass = container.className;
                    if (parentClass == "body" && skipFirstTitle && !firstTitleSkipped) {
                        firstTitleSkipped = true;
                    } else if (parentClass == "section" || parentClass == "body") {
                        var titleStr = extractTitleText(child);
                        if (titleStr.length > 0) tocText += indent + titleStr + "\n";
                    }
                } else if (child.className == "section") {
                    findTitleInSection(child, indent + "     ");
                }
            }
        }
    }
    
    function findTitleInSection(section, indent) {
        var children = section.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 1 && child.nodeName == "DIV") {
                if (child.className == "title") {
                    var titleStr = extractTitleText(child);
                    if (titleStr.length > 0) tocText += indent + titleStr + "\n";
                } else if (child.className == "section") {
                    findTitleInSection(child, indent + "     ");
                }
            }
        }
    }
    
    function countSections(container) {
        var count = 0;
        var divs = container.getElementsByTagName("DIV");
        for (var i = 0; i < divs.length; i++) {
            if (divs[i].className == "section") count++;
        }
        return count;
    }
    
    function normalizeSpacing(txt) {
        var reMultiNl = new RegExp("\n\n\n+", "g");
        while (txt.indexOf("\n\n\n") != -1) {
            txt = txt.replace(reMultiNl, "\n\n");
        }
        return txt;
    }
    
    function processChildren(container, includeTitles, skipFirstTitle, globalResult) {
        var txt = "";
        var children = container.childNodes;
        var firstTitleSkipped = false;
        
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType != 1) continue;
            
            var tagName = child.nodeName;
            var className = child.className;
            
            if (tagName == "P") {
                if (className == "subtitle") {
                    var subText = trimStr(getCleanText(child));
                    if (subText.length > 0) {
                        totalSubtitles++;
                        if (wrapSubtitles) {
                            if (txt.length > 0 && !endsWithBlankLine(txt)) txt = appendBlankLine(txt);
                            else if (txt.length == 0 && globalResult.length > 0 && !endsWithBlankLine(globalResult)) txt = "\n\n";
                            txt += subText + "\n";
                            txt = appendBlankLine(txt);
                        } else {
                            txt += subText + "\n";
                        }
                    }
                } else if (className == "text-author") {
                    var taText = trimStr(getCleanText(child));
                    if (taText.length > 0) txt += taText + "\n";
                } else {
                    var pText = trimStr(getCleanText(child));
                    txt += pText + "\n";
                    totalParagraphs++;
                }
            } else if (tagName == "DIV") {
                if (className == "title") {
                    if (includeTitles) {
                        if (container.className == "body" && skipFirstTitle && !firstTitleSkipped) {
                            firstTitleSkipped = true;
                        } else {
                            var titleText = extractTitleTextForBody(child);
                            if (titleText.length > 0) {
                                if (wrapTitles) {
                                    if (txt.length > 0 && !endsWithBlankLine(txt)) txt = appendBlankLine(txt);
                                    else if (txt.length == 0 && globalResult.length > 0 && !endsWithBlankLine(globalResult)) txt = "\n\n";
                                    txt += titleText;
                                    txt = appendBlankLine(txt);
                                } else {
                                    txt += titleText;
                                }
                            }
                        }
                    }
                } else if (className == "section") {
                    if (wrapDivElements) {
                        if (txt.length > 0 && !endsWithBlankLine(txt)) txt = appendBlankLine(txt);
                        else if (txt.length == 0 && globalResult.length > 0 && !endsWithBlankLine(globalResult)) txt = "\n\n";
                    }
                    txt += processChildren(child, includeTitles, false, globalResult.length > 0 ? globalResult + txt : txt);
                } else if (className == "stanza") {
                    txt += processChildren(child, true, false, globalResult.length > 0 ? globalResult + txt : txt);
                } else if (className == "epigraph" || className == "annotation" || className == "cite" || className == "poem") {
                    if (wrapDivElements) {
                        if (txt.length > 0 && !endsWithBlankLine(txt)) txt = appendBlankLine(txt);
                        else if (txt.length == 0 && globalResult.length > 0 && !endsWithBlankLine(globalResult)) txt = "\n\n";
                    }
                    var innerText = processChildren(child, true, false, globalResult.length > 0 ? globalResult + txt : txt);
                    if (innerText.length > 0) {
                        txt += innerText;
                        if (wrapDivElements) txt = appendBlankLine(txt);
                    }
                } else if (className == "image") {
                    if (wrapDivElements) {
                        if (txt.length > 0 && !endsWithBlankLine(txt)) txt = appendBlankLine(txt);
                        else if (txt.length == 0 && globalResult.length > 0 && !endsWithBlankLine(globalResult)) txt = "\n\n";
                    }
                    txt += "[иллюстрация]\n";
                    if (wrapDivElements) txt = appendBlankLine(txt);
                } else {
                    txt += processChildren(child, includeTitles, false, globalResult.length > 0 ? globalResult + txt : txt);
                }
            } else if (tagName == "TABLE") {
                if (wrapDivElements) {
                    if (txt.length > 0 && !endsWithBlankLine(txt)) txt = appendBlankLine(txt);
                    else if (txt.length == 0 && globalResult.length > 0 && !endsWithBlankLine(globalResult)) txt = "\n\n";
                }
                txt += "[таблица]\n";
                if (wrapDivElements) txt = appendBlankLine(txt);
            }
        }
        return txt;
    }
    
    function pad(number) {
        var n = number % 100;
        if (n >= 11 && n <= 19) return 2;
        n = number % 10;
        if (n == 1) return 0;
        if (n >= 2 && n <= 4) return 1;
        return 2;
    }
    
    function yesNo(value) {
        if (value == 1) return "\u221A (ДА)";
        return "\u2717 (НЕТ)";
    }
    
    // ==================================================
    // СБОРКА ТЕКСТА
    // ==================================================
    
    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) {
        MsgBox(scriptName + "\nver. " + version + "\n---------------------------------------\n\n\u2717 Ошибка: fbw_body не найден!");
        return;
    }
    
    var bodyChildren = fbwBody.childNodes;
    
    var descAT = getAuthorAndTitleFromDesc();
    var bodyAT = getAuthorAndTitleFromBody();
    var allMatch = (descAT.author == bodyAT.author && descAT.title == bodyAT.title && descAT.author != "" && descAT.title != "");
    
    var displayAuthor = descAT.author || bodyAT.author;
    var displayTitle = descAT.title || bodyAT.title;
    
    if (displayAuthor != "") resultText += displayAuthor + "\n";
    if (displayTitle != "") resultText += displayTitle + "\n";
    if (displayAuthor != "" || displayTitle != "") resultText += "\n";
    
    var skipBodyTitle = allMatch;
    
    // Подсчитываем заголовки (всегда, независимо от createTOC)
    for (var i = 0; i < bodyChildren.length; i++) {
        var child = bodyChildren[i];
        if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "body") {
            if ((child.getAttribute("fbname") || "") == "" && processMainSection) {
                countTitles(child, skipBodyTitle);
            }
        }
    }
    
    // Собираем оглавление (только если createTOC включён)
    if (createTOC) {
        for (var k = 0; k < bodyChildren.length; k++) {
            var child2 = bodyChildren[k];
            if (child2.nodeType == 1 && child2.nodeName == "DIV" && child2.className == "body") {
                if ((child2.getAttribute("fbname") || "") == "" && processMainSection) {
                    buildTOC(child2, "", skipBodyTitle);
                }
            }
        }
        for (var m = 0; m < bodyChildren.length; m++) {
            var bc2 = bodyChildren[m];
            if (bc2.nodeType == 1 && bc2.nodeName == "DIV" && bc2.className == "body") {
                var fn2 = bc2.getAttribute("fbname") || "";
                if (fn2 == "notes" && processNotesSection) notesSections = countSections(bc2);
                else if (fn2 == "comments" && processCommentsSection) commentsSections = countSections(bc2);
            }
        }
    }
    
    // Выводим оглавление
    if (createTOC && tocText.length > 0) {
        resultText += " \u2022 Оглавление:\n";
        resultText += " ~~~~~~~~~~~~~~~~~~~~\n";
        resultText += tocText;
        
        var primechanij = [" примечание", " примечания", " примечаний"];
        var kommentariev = [" комментарий", " комментария", " комментариев"];
        
        if (notesSections > 0) {
            resultText += "\n Примечания\n";
            resultText += " \u2022 " + notesSections + primechanij[pad(notesSections)] + "\n";
        }
        if (commentsSections > 0) {
            resultText += "\n Комментарии\n";
            resultText += " \u2022 " + commentsSections + kommentariev[pad(commentsSections)] + "\n";
        }
        resultText += "\n";
    }
    
    // Обрабатываем annotation, history и body-секции
    for (var j = 0; j < bodyChildren.length; j++) {
        var bodyChild = bodyChildren[j];
        if (bodyChild.nodeType == 1 && bodyChild.nodeName == "DIV") {
            var cName = bodyChild.className;
            
            if (cName == "annotation" && processAnnotationSection) {
                if (resultText.length > 0 && !endsWithBlankLine(resultText)) resultText = appendBlankLine(resultText);
                resultText += processChildren(bodyChild, true, false, resultText);
                resultText = appendBlankLine(resultText);
            } else if (cName == "history" && processHistorySection) {
                if (resultText.length > 0 && !endsWithBlankLine(resultText)) resultText = appendBlankLine(resultText);
                resultText += processChildren(bodyChild, true, false, resultText);
                resultText = appendBlankLine(resultText);
            } else if (cName == "body") {
                var fn3 = bodyChild.getAttribute("fbname") || "";
                var sp = false;
                if (fn3 == "" && processMainSection) sp = true;
                else if (fn3 == "notes" && processNotesSection) sp = true;
                else if (fn3 == "comments" && processCommentsSection) sp = true;
                if (sp) resultText += processChildren(bodyChild, true, skipBodyTitle, resultText);
            }
        }
    }
    
    resultText = normalizeSpacing(resultText);
    resultText = resultText.replace(/\n+$/, "\n");
    totalChars = resultText.length;
    
    if (resultText.length > 0) window.clipboardData.setData("text", resultText);
    
    // ==================================================
    // СТАТИСТИКА
    // ==================================================
    
    var endTime = new Date();
    var elapsed = (endTime - startTime) / 1000;
    var elapsedStr = elapsed.toFixed(3);
    var elapsedFormatted = elapsedStr.replace(".", ",");
    
    if (showStatistics == 1) {
        var msg = scriptName + "\n";
        msg += "ver. " + version + "\n";
        msg += "---------------------------------------\n\n";
        
        if (resultText.length > 0) msg += "\u221A Текст скопирован в буфер обмена\n";
        else msg += "\u2717 Ничего не скопировано!\n";
        
        msg += "---------------------------------------\n";
        msg += "Настройки копирования:\n";
        msg += "  \u2022 Создание оглавления: " + yesNo(createTOC) + "\n";
        msg += "  \u2022 Раздел аннотации: " + yesNo(processAnnotationSection) + "\n";
        msg += "  \u2022 Раздел History: " + yesNo(processHistorySection) + "\n";
        msg += "  \u2022 Основной раздел: " + yesNo(processMainSection) + "\n";
        msg += "  \u2022 Раздел сносок (примечаний): " + yesNo(processNotesSection) + "\n";
        msg += "  \u2022 Раздел комментариев: " + yesNo(processCommentsSection) + "\n";
        msg += "  \u2022 Пустые строки вокруг заголовков: " + yesNo(wrapTitles) + "\n";
        msg += "  \u2022 Пустые строки вокруг подзаголовков: " + yesNo(wrapSubtitles) + "\n";
        msg += "  \u2022 Обрамление DIV: " + yesNo(wrapDivElements) + "\n";
        msg += "---------------------------------------\n\n";
        
        if (displayAuthor != "" || displayTitle != "") {
            if (displayAuthor != "") msg += "Автор: " + displayAuthor + "\n";
            if (displayTitle != "") msg += "Название: " + displayTitle + "\n";
            msg += "\n";
        }
        
        if (createTOC) {
            msg += "\u221A Всего заголовков в оглавлении: " + totalTitles + "\n";
        } else {
            msg += "\u221A Всего заголовков в оглавлении: 0 (отключено)\n";
        }
        msg += "\u221A Всего скопировано заголовков: " + totalTitles + "\n";
        msg += "\u221A Всего скопировано подзаголовков: " + totalSubtitles + "\n";
        msg += "\u221A Всего скопировано абзацев: " + totalParagraphs + "\n";
        msg += "\u221A Всего скопировано символов: " + totalChars + "\n";
        
        if (notesSections > 0) msg += "\u221A Примечаний: " + notesSections + "\n";
        if (commentsSections > 0) msg += "\u221A Комментариев: " + commentsSections + "\n";
        
        msg += "\n---------------------------------------\n";
        msg += "Время выполнения: " + elapsedFormatted + " сек.";
        
        MsgBox(msg);
    } else {
        if (resultText.length == 0) {
            MsgBox(scriptName + "\nver. " + version + "\n---------------------------------------\n\n\u2717 Ошибка: ничего не скопировано! Проверьте настройки скрипта.");
        }
    }
}
