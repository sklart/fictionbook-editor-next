// Скрипт "Диагностика структуры примечаний и комментариев" для редактора FBE
// version 3.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir
// За некоторые решения в скрипте спасибо уважаемым тов. stokber и Lancer.

// Скрипт предназначен для проверки структуры сносок (примечаний) и комментариев в fb2 документах
// до и после унификации примечаний.
// Проверяет соответствие маркеров (ссылок в тексте) и разделов примечаний/комментариев.
// Выявляет проблемы: несоответствие номеров, отсутствие маркеров, отсутствие разделов,
// дубликаты, нарушения последовательности, пропуски в нумерации.
// Показ подробной статистики осуществляется в IE или в браузере по умолчанию.
// При отсутствии проблем, показ расширенной статистики выключен (настраивается).
// Скрипт не вносит никаких изменений в fb2 документ.

// version 3.2, 17.06.2026
//======================================

function Run() {
    // Засекаем время начала выполнения
    var Ts = new Date().getTime();
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Способ показа расширенной HTML-таблицы:
    // "1" - показывать в окне IE (window.open)
    // "2" - показывать в браузере по умолчанию (через временный файл)
    var show = "1";
    
    // Режим отображения в HTML-окне:
    // "separate" - отдельно: сначала все примечания, потом все комментарии
    // "mixed"    - вперемешку в порядке появления маркеров в тексте документа
    var displayMode = "separate";
    
    // Показывать расширенную HTML-таблицу, если проблем не обнаружено:
    // 0 - не показывать (только MsgBox с сообщением "проблем нет")
    // 1 - показывать всегда
    var showTableIfNoProblems = 0;
    
    // ========== НАСТРОЙКИ ОКНА РЕЗУЛЬТАТОВ (только для show = "1") ==========
    // Положение окна: "left" - у левого края экрана, "right" - у правого края
    var windowPosition = "left";
    // Ширина окна в долях от ширины экрана (0.30 = 30%)
    var windowWidthPercent = 0.30;
    // Высота окна в долях от высоты экрана (0.95 = 95%)
    var windowHeightPercent = 0.95;
    
    // Вычисляем размеры и положение окна результатов (только для show = "1")
    var screenWidth = window.screen.availWidth || window.screen.width;
    var screenHeight = window.screen.availHeight || window.screen.height;
    var winWidth = Math.floor(screenWidth * windowWidthPercent);
    var winHeight = Math.floor(screenHeight * windowHeightPercent);
    var winLeft = 0;
    var winTop = 0;
    
    if (windowPosition === "right") {
        winLeft = screenWidth - winWidth;
    } else {
        winLeft = 0;
    }
    
    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================
    
    // Извлекает локальную часть href (всё после #), отбрасывая путь к файлу
    function GetLocalHref(name) {
        var name1 = name;
        if (name1.indexOf("#") < 0) return "1";
        var thg = new RegExp("main\\.html\\#", "i");
        var srch10 = name1.search(thg);
        if (srch10 == -1) {
            name1 = name1.substring(1, name1.length);
        } else {
            name1 = name1.substring(srch10 + 10, name1.length);
        }
        return name1;
    }
    
    // Извлекает число из href (например, из "n_5" -> 5, из "c_3" -> 3)
    function extractNumberFromHref(href) {
        if (!href) return null;
        var pos = href.indexOf('#');
        if (pos === -1) return null;
        var anchor = href.substring(pos + 1);
        var match = anchor.match(/^fn(\d+)$/i);
        if (match) return parseInt(match[1], 10);
        match = anchor.match(/^n_(\d+)$/i);
        if (match) return parseInt(match[1], 10);
        match = anchor.match(/^c_(\d+)$/i);
        if (match) return parseInt(match[1], 10);
        match = anchor.match(/(\d+)$/);
        if (match) return parseInt(match[1], 10);
        return null;
    }
    
    // Извлекает число из текста маркера: [1], (2), {3}, просто 4, верхний индекс
    function extractNumberFromText(text) {
        if (!text) return null;
        var match = text.match(/^\[(\d+)\]$/);
        if (match) return parseInt(match[1], 10);
        match = text.match(/^\((\d+)\)$/);
        if (match) return parseInt(match[1], 10);
        match = text.match(/^\{(\d+)\}$/);
        if (match) return parseInt(match[1], 10);
        match = text.match(/^(\d+)$/);
        if (match) return parseInt(match[1], 10);
        // Карта верхних индексов ¹²³⁰⁴⁵⁶⁷⁸⁹
        var superscriptMap = {
            '¹': 1, '²': 2, '³': 3, '⁴': 4, '⁵': 5,
            '⁶': 6, '⁷': 7, '⁸': 8, '⁹': 9, '⁰': 0
        };
        if (text.length === 1 && superscriptMap[text] !== undefined) {
            return superscriptMap[text];
        }
        if (/^\*+$/.test(text)) return null;
        return null;
    }
    
    // Определяет тип маркера по его тексту
    function getMarkerType(text) {
        if (/^\*+$/.test(text)) return "star";           // звёздочки
        else if (/^\d+$/.test(text)) return "number";    // просто цифра
        else if (/^\(\d+\)$/.test(text)) return "paren"; // в круглых скобках
        else if (/^[\u00B9\u00B2\u00B3\u2070\u2074-\u2079]+$/.test(text)) return "superscript"; // верхний индекс
        else if (/^\[\d+\]$/.test(text)) return "bracket"; // в квадратных скобках [N]
        else if (/^\{\d+\}$/.test(text)) return "brace";   // в фигурных скобках {N}
        else return "other";
    }
    
    // Возвращает краткое обозначение типа маркера для отображения в таблице
    function getMarkerTypeDisplay(markerType) {
        switch(markerType) {
            case "bracket": return "[]";
            case "paren": return "()";
            case "brace": return "{}";
            case "number": return "N";
            case "superscript": return "\u00B9\u00B2\u00B3";
            case "star": return "*";
            case "other": return "?";
            default: return markerType;
        }
    }
    
    // Экранирует HTML-спецсимволы для безопасного вывода
    function escapeHTML(str) {
        if (!str) return "";
        return str.replace(/&/g, "&amp;")
                  .replace(/</g, "&lt;")
                  .replace(/>/g, "&gt;")
                  .replace(/"/g, "&quot;")
                  .replace(/'/g, "&#39;");
    }
    
    // Удаляет HTML-теги из строки, оставляя только чистый текст
    function stripHTML(str) {
        if (!str) return "";
        var result = "";
        var insideTag = false;
        for (var i = 0; i < str.length; i++) {
            var ch = str.charAt(i);
            if (ch == '<') {
                insideTag = true;
            } else if (ch == '>') {
                insideTag = false;
            } else if (!insideTag) {
                result += ch;
            }
        }
        return result;
    }
    
    // Сортирует массив проблем по порядку (от меньшего номера к большему)
    // Используется пузырьковая сортировка для совместимости с IE6
    function sortProblems(problems) {
        for (var i = 0; i < problems.length - 1; i++) {
            for (var j = i + 1; j < problems.length; j++) {
                var orderI = getProblemOrder(problems[i]);
                var orderJ = getProblemOrder(problems[j]);
                if (orderI > orderJ) {
                    var temp = problems[i];
                    problems[i] = problems[j];
                    problems[j] = temp;
                }
            }
        }
    }
    
    // Возвращает числовой приоритет для сортировки проблемы
    function getProblemOrder(problem) {
        if (problem.type == "missing_both") {
            return problem.missingNum * 1000;
        }
        if (problem.type == "section_without_link") {
            return problem.sectionIdNum * 1000 + 1;
        }
        if (problem.type == "missing_marker") {
            return problem.missingNum * 1000 + 2;
        }
        if (problem.type == "missing_section") {
            return problem.missingNum * 1000 + 3;
        }
        if (problem.type == "first_link_not_first") {
            return (problem.linkIndex || 0) + 4;
        }
        if (problem.type == "link_without_section") {
            return (problem.linkIndex || 0) + 5;
        }
        if (problem.linkIndex !== undefined && problem.linkIndex !== null) {
            return problem.linkIndex + 10;
        }
        if (problem.sectionNum !== undefined && problem.sectionNum !== null) {
            return problem.sectionNum * 1000 + 100;
        }
        return 999999;
    }
    
    // ==================================================
    // ОСНОВНАЯ ЧАСТЬ — СБОР ДАННЫХ
    // ==================================================
    
    // Получаем основной контейнер документа
    var body = document.getElementById("fbw_body");
    if (!body) {
        MsgBox("Диагностика структуры примечаний и комментариев\nver. 3.2\n\nОшибка. Body не найден!");
        return;
    }
    
    // 1. Ищем разделы примечаний (fbname="notes") и комментариев (fbname="comments")
    var bodyNotes = null;
    var bodyComments = null;
    
    var bodyDivs = document.getElementsByTagName("DIV");
    for (var i = 0; i < bodyDivs.length; i++) {
        if (bodyDivs[i].className == "body") {
            var fbname = bodyDivs[i].getAttribute("fbname") || "";
            if (fbname == "notes") {
                bodyNotes = bodyDivs[i];
            } else if (fbname == "comments") {
                bodyComments = bodyDivs[i];
            }
        }
    }
    
    // 2. Собираем разделы (секции) внутри примечаний
    var notesSectsColl = {};
    var notesSectIds = {};
    var notesSectNumById = {};
    var notesSectNum = 0;
    var notesIdToNumber = {};
    var notesMaxIdNum = 0;
    
    if (bodyNotes) {
        var ccc = bodyNotes.firstChild;
        while (ccc != null) {
            if (ccc.nodeName == "DIV" && ccc.className == "section") {
                notesSectNum++;
                notesSectsColl[notesSectNum] = ccc;
                notesSectIds[notesSectNum] = ccc.id;
                notesSectNumById[ccc.id] = notesSectNum;
                var idNum = extractNumberFromHref("#" + ccc.id);
                if (idNum !== null) {
                    notesIdToNumber[ccc.id] = idNum;
                    if (idNum > notesMaxIdNum) notesMaxIdNum = idNum;
                }
            }
            ccc = ccc.nextSibling;
        }
    }
    
    // 3. Собираем разделы (секции) внутри комментариев
    var commentsSectsColl = {};
    var commentsSectIds = {};
    var commentsSectNumById = {};
    var commentsSectNum = 0;
    var commentsIdToNumber = {};
    var commentsMaxIdNum = 0;
    
    if (bodyComments) {
        var ddd = bodyComments.firstChild;
        while (ddd != null) {
            if (ddd.nodeName == "DIV" && ddd.className == "section") {
                commentsSectNum++;
                commentsSectsColl[commentsSectNum] = ddd;
                commentsSectIds[commentsSectNum] = ddd.id;
                commentsSectNumById[ddd.id] = commentsSectNum;
                var idNum = extractNumberFromHref("#" + ddd.id);
                if (idNum !== null) {
                    commentsIdToNumber[ddd.id] = idNum;
                    if (idNum > commentsMaxIdNum) commentsMaxIdNum = idNum;
                }
            }
            ddd = ddd.nextSibling;
        }
    }
    
    // 4. Собираем все маркеры (ссылки) в тексте документа
    var notesMarkers = [];
    var commentsMarkers = [];
    
    var allLinks = document.links;
    for (var i = 0; i < allLinks.length; i++) {
        var link = allLinks[i];
        var href = link.getAttribute("href") || "";
        var className = link.className || "";
        
        if (className == "note") {
            notesMarkers.push({
                link: link,
                href: href,
                index: i,
                type: "note"
            });
        } else if (href.indexOf("#") >= 0) {
            var localHref = GetLocalHref(href);
            if (localHref.length > 0) {
                var commentRegExp = new RegExp("^c_", "");
                if (localHref.search(commentRegExp) >= 0) {
                    commentsMarkers.push({
                        link: link,
                        href: href,
                        index: i,
                        type: "comment"
                    });
                }
            }
        }
    }
    
    // ==================================================
    // ФУНКЦИЯ АНАЛИЗА МАРКЕРОВ И РАЗДЕЛОВ
    // ==================================================
    
    function analyzeMarkers(markers, sectsColl, sectNumById, sectIds, sectionCount, typeName, idToNumber, maxIdNum) {
        var diagnosis = {
            typeName: typeName,
            totalMarkers: markers.length,
            sectionCount: sectionCount,
            maxIdNum: maxIdNum,
            markersByType: {
                star: 0,
                number: 0,
                paren: 0,
                superscript: 0,
                bracket: 0,
                brace: 0,
                other: 0
            },
            linkSequence: [],
            problems: [],
            sectionLinksMap: {},
            usedSections: {},
            markersByNum: {},
            existingSectionIds: {},
            sectionIdToNum: {},
            maxMarkerNum: 0
        };
        
        for (var sn = 1; sn <= sectionCount; sn++) {
            if (sectIds[sn]) {
                diagnosis.existingSectionIds[sectIds[sn]] = sn;
                if (idToNumber[sectIds[sn]] !== undefined) {
                    diagnosis.sectionIdToNum[sectIds[sn]] = idToNumber[sectIds[sn]];
                } else {
                    diagnosis.sectionIdToNum[sectIds[sn]] = sn;
                }
            }
        }
        
        for (var i = 0; i < markers.length; i++) {
            var marker = markers[i];
            var link = marker.link;
            var href = marker.href;
            var linkText = link.innerHTML || "";
            var linkTextStripped = stripHTML(linkText);
            var markerType = getMarkerType(linkTextStripped);
            
            diagnosis.markersByType[markerType]++;
            
            var sectionId = GetLocalHref(href);
            var sectionNum = sectNumById[sectionId] || null;
            
            var numFromText = extractNumberFromText(linkTextStripped);
            var numFromHref = extractNumberFromHref(href);
            
            if (numFromText !== null && numFromText > diagnosis.maxMarkerNum) {
                diagnosis.maxMarkerNum = numFromText;
            }
            
            var sectionIdNum = null;
            if (sectionId && idToNumber[sectionId] !== undefined) {
                sectionIdNum = idToNumber[sectionId];
            } else if (sectionId) {
                sectionIdNum = extractNumberFromHref("#" + sectionId);
            }
            
            var linkInfo = {
                index: marker.index,
                link: link,
                text: linkText,
                textStripped: linkTextStripped,
                href: href,
                markerType: markerType,
                sectionId: sectionId,
                sectionNum: sectionNum,
                sectionIdNum: sectionIdNum,
                numFromText: numFromText,
                numFromHref: numFromHref,
                className: link.className || "",
                isNote: (link.className == "note"),
                sectionExists: (sectionNum !== null)
            };
            
            diagnosis.linkSequence.push(linkInfo);
            
            if (sectionNum !== null) {
                if (!diagnosis.sectionLinksMap[sectionNum]) {
                    diagnosis.sectionLinksMap[sectionNum] = [];
                }
                diagnosis.sectionLinksMap[sectionNum].push({
                    link: link,
                    text: linkTextStripped,
                    index: marker.index
                });
                diagnosis.usedSections[sectionNum] = 1;
            }
            
            if (numFromText !== null) {
                if (!diagnosis.markersByNum[numFromText]) {
                    diagnosis.markersByNum[numFromText] = [];
                }
                diagnosis.markersByNum[numFromText].push(linkInfo);
            }
        }
        
        var maxNum = diagnosis.maxMarkerNum;
        if (maxIdNum > maxNum) maxNum = maxIdNum;
        diagnosis.maxNum = maxNum;
        
        var numToSectionId = {};
        for (var id in diagnosis.sectionIdToNum) {
            if (diagnosis.sectionIdToNum.hasOwnProperty(id)) {
                var n = diagnosis.sectionIdToNum[id];
                numToSectionId[n] = id;
            }
        }
        diagnosis.numToSectionId = numToSectionId;
        
        // Проверка полностью отсутствующих
        for (var num = 1; num <= maxNum; num++) {
            var hasMarker = (diagnosis.markersByNum[num] && diagnosis.markersByNum[num].length > 0);
            var hasSection = (numToSectionId[num] && numToSectionId[num].length > 0);
            
            if (!hasMarker && !hasSection) {
                var itemName = (typeName == "note") ? "примечания" : "комментария";
                diagnosis.problems.push({
                    type: "missing_both",
                    message: "Нет " + itemName + " № " + num + " (нет ни маркера ни раздела). Возможно, оно потеряно. Или нужно выполнить перенумерацию " + itemName + ".",
                    missingNum: num
                });
            }
        }
        
        var prevSectionNum = null;
        var addedDuplicateProblems = {};
        
        for (var i = 0; i < diagnosis.linkSequence.length; i++) {
            var linkInfo = diagnosis.linkSequence[i];
            
            if (!linkInfo.sectionExists && linkInfo.sectionId && linkInfo.sectionId !== "1" && linkInfo.sectionId !== -1) {
                diagnosis.problems.push({
                    type: "link_without_section",
                    message: "Ссылка [" + linkInfo.textStripped + "] ссылается на несуществующий раздел с ID: " + linkInfo.sectionId,
                    linkIndex: i,
                    link: linkInfo.link
                });
            }
            
            if (linkInfo.sectionNum !== null) {
                if (!addedDuplicateProblems[linkInfo.sectionNum]) {
                    var dupCount = 0;
                    for (var d = 0; d < diagnosis.linkSequence.length; d++) {
                        if (diagnosis.linkSequence[d].sectionNum == linkInfo.sectionNum) {
                            dupCount++;
                        }
                    }
                    if (dupCount > 1) {
                        addedDuplicateProblems[linkInfo.sectionNum] = true;
                        diagnosis.problems.push({
                            type: "duplicate_link",
                            message: "Дублирующая ссылка на раздел " + linkInfo.sectionNum + " (ID: " + linkInfo.sectionId + ")",
                            sectionNum: linkInfo.sectionNum,
                            linkIndex: i,
                            link: linkInfo.link
                        });
                    }
                }
            }
            
            if (linkInfo.sectionNum !== null && prevSectionNum !== null) {
                if (linkInfo.sectionNum < prevSectionNum) {
                    diagnosis.problems.push({
                        type: "broken_sequence",
                        message: "Нарушена последовательность: после ссылки на раздел " + prevSectionNum + " идет ссылка на раздел " + linkInfo.sectionNum,
                        prevNum: prevSectionNum,
                        currentNum: linkInfo.sectionNum,
                        linkIndex: i,
                        link: linkInfo.link
                    });
                }
            }
            
            if (linkInfo.sectionNum !== null) {
                prevSectionNum = linkInfo.sectionNum;
            }
            
            if (i === 0 && linkInfo.sectionNum !== null && linkInfo.sectionNum !== 1) {
                diagnosis.problems.push({
                    type: "first_link_not_first",
                    message: "ПЕРВАЯ ссылка в тексте ссылается на раздел " + linkInfo.sectionNum + " (а должна на 1)",
                    sectionNum: linkInfo.sectionNum,
                    linkIndex: i,
                    link: linkInfo.link
                });
            }
            
            if (typeName == "note" && linkInfo.className !== "note") {
                diagnosis.problems.push({
                    type: "missing_note_class",
                    message: "Маркер [" + linkInfo.textStripped + "] не имеет class='note'",
                    linkIndex: i,
                    link: linkInfo.link
                });
            }
        }
        
        for (var sectNum = 1; sectNum <= sectionCount; sectNum++) {
            if (!diagnosis.usedSections[sectNum]) {
                var sectionIdVal = sectIds[sectNum];
                var idNumForMsg = idToNumber[sectionIdVal] || sectNum;
                diagnosis.problems.push({
                    type: "section_without_link",
                    message: "Раздел " + idNumForMsg + " (ID: " + sectionIdVal + ") не имеет ссылок",
                    sectionNum: sectNum,
                    sectionIdNum: idNumForMsg,
                    section: sectsColl[sectNum]
                });
            }
        }
        
        // Проверка пропусков в нумерации маркеров
        for (var num = 1; num <= diagnosis.maxMarkerNum; num++) {
            if (!diagnosis.markersByNum[num] || diagnosis.markersByNum[num].length == 0) {
                var isMissingBoth = false;
                for (var p = 0; p < diagnosis.problems.length; p++) {
                    if (diagnosis.problems[p].type == "missing_both" && diagnosis.problems[p].missingNum == num) {
                        isMissingBoth = true;
                        break;
                    }
                }
                if (!isMissingBoth) {
                    var itemName = (typeName == "note") ? "примечания" : "комментария";
                    diagnosis.problems.push({
                        type: "missing_marker",
                        message: "Пропущен маркер № " + num + " " + itemName,
                        missingNum: num
                    });
                }
            }
        }
        
        // Проверка пропусков в нумерации разделов по ID
        var allIdNums = [];
        for (var sn = 1; sn <= sectionCount; sn++) {
            var sid = sectIds[sn];
            var idNum = idToNumber[sid] || sn;
            allIdNums.push(idNum);
        }
        
        for (var m = 0; m < allIdNums.length - 1; m++) {
            for (var n = m + 1; n < allIdNums.length; n++) {
                if (allIdNums[m] > allIdNums[n]) {
                    var temp = allIdNums[m];
                    allIdNums[m] = allIdNums[n];
                    allIdNums[n] = temp;
                }
            }
        }
        
        for (var q = 1; q < allIdNums.length; q++) {
            if (allIdNums[q] - allIdNums[q-1] > 1) {
                var itemName = (typeName == "note") ? "примечания" : "комментария";
                for (var missing = allIdNums[q-1] + 1; missing < allIdNums[q]; missing++) {
                    var isMissingBoth2 = false;
                    for (var p2 = 0; p2 < diagnosis.problems.length; p2++) {
                        if (diagnosis.problems[p2].type == "missing_both" && diagnosis.problems[p2].missingNum == missing) {
                            isMissingBoth2 = true;
                            break;
                        }
                    }
                    if (!isMissingBoth2) {
                        diagnosis.problems.push({
                            type: "missing_section",
                            message: "Пропущен раздел " + itemName + " № " + missing,
                            missingNum: missing
                        });
                    }
                }
            }
        }
        
        sortProblems(diagnosis.problems);
        
        return diagnosis;
    }
    
    // 5. Запускаем анализ
    var notesDiagnosis = analyzeMarkers(notesMarkers, notesSectsColl, notesSectNumById, notesSectIds, notesSectNum, "note", notesIdToNumber, notesMaxIdNum);
    var commentsDiagnosis = analyzeMarkers(commentsMarkers, commentsSectsColl, commentsSectNumById, commentsSectIds, commentsSectNum, "comment", commentsIdToNumber, commentsMaxIdNum);
    
    // 6. Формирование отчёта для MsgBox
    var msgStr = "Диагностика структуры примечаний и комментариев\n";
    msgStr += "ver. 3.2\n";
    msgStr += "=======================================\n\n";
    
    var hasNotes = notesMarkers.length > 0 || notesSectNum > 0;
    var hasComments = commentsMarkers.length > 0 || commentsSectNum > 0;
    var hasAny = hasNotes || hasComments;
    
    if (!hasAny) {
        msgStr += "Примечания и комментарии в документе отсутствуют.\n\n";
    } else {
        msgStr += "СВОДКА ПО ПРОБЛЕМАМ:\n";
        msgStr += "---------------------------------------\n";
        
        if (hasNotes) {
            if (notesDiagnosis.problems.length == 0) {
                msgStr += "\u2713 Примечания: проблем не обнаружено\n";
            } else {
                msgStr += "\u2717 Примечания: обнаружено проблем " + notesDiagnosis.problems.length + "\n";
                for (var p = 0; p < notesDiagnosis.problems.length; p++) {
                    msgStr += "  - " + notesDiagnosis.problems[p].message + "\n";
                }
            }
        } else {
            msgStr += "\u2717 Примечания: не обнаружены\n";
        }
        
        if (hasComments) {
            if (commentsDiagnosis.problems.length == 0) {
                msgStr += "\u2713 Комментарии: проблем не обнаружено\n";
            } else {
                msgStr += "\u2717 Комментарии: обнаружено проблем " + commentsDiagnosis.problems.length + "\n";
                for (var p = 0; p < commentsDiagnosis.problems.length; p++) {
                    msgStr += "  - " + commentsDiagnosis.problems[p].message + "\n";
                }
            }
        } else {
            msgStr += "\u2717 Комментарии: не обнаружены\n";
        }
        
        msgStr += "\n";
        
        msgStr += "ВСЕГО НАЙДЕНО:\n";
        msgStr += "---------------------------------------\n";
        
        if (hasNotes) {
            var notesLastNumStr = "";
            if (notesDiagnosis.maxNum > 0) {
                notesLastNumStr = " (последний номер " + notesDiagnosis.maxNum + ")";
            }
            msgStr += "\u2022 Примечаний (class notes): маркеров " + notesMarkers.length + notesLastNumStr + "\n";
            msgStr += "  Маркеров в тексте: " + notesMarkers.length + "\n";
            msgStr += "  ID номеров примечаний: " + notesSectNum;
            if (notesDiagnosis.maxIdNum > 0) {
                msgStr += " (последний номер " + notesDiagnosis.maxIdNum + ")";
            }
            msgStr += "\n";
            msgStr += "  Максимальный номер маркера: " + notesDiagnosis.maxMarkerNum + "\n";
            msgStr += "  Максимальный ID номер раздела примечания: " + notesDiagnosis.maxIdNum + "\n";
            msgStr += "  Раздел 'notes' существует: " + (bodyNotes ? "ДА" : "НЕТ") + "\n";
        } else {
            msgStr += "\u2022 Примечаний (class notes): не обнаружено\n";
        }
        
        msgStr += "\n";
        
        if (hasComments) {
            var commentsLastNumStr = "";
            if (commentsDiagnosis.maxNum > 0) {
                commentsLastNumStr = " (последний номер " + commentsDiagnosis.maxNum + ")";
            }
            msgStr += "\u2022 Комментариев (class comments): маркеров " + commentsMarkers.length + commentsLastNumStr + "\n";
            msgStr += "  Маркеров в тексте: " + commentsMarkers.length + "\n";
            msgStr += "  ID номеров комментариев: " + commentsSectNum;
            if (commentsDiagnosis.maxIdNum > 0) {
                msgStr += " (последний номер " + commentsDiagnosis.maxIdNum + ")";
            }
            msgStr += "\n";
            msgStr += "  Максимальный номер маркера: " + commentsDiagnosis.maxMarkerNum + "\n";
            msgStr += "  Максимальный ID номер раздела комментария: " + commentsDiagnosis.maxIdNum + "\n";
            msgStr += "  Раздел 'comments' существует: " + (bodyComments ? "ДА" : "НЕТ") + "\n";
        } else {
            msgStr += "\u2022 Комментариев (class comments): не обнаружено\n";
        }
    }
    
    var Tf = new Date().getTime();
    var Tsek = Math.round(100 * ((Tf - Ts) / 1000)) / 100;
    msgStr += "\nВремя диагностики: " + Tsek.toFixed(2) + " сек";
    
    MsgBox(msgStr);
    
    // Определяем, нужно ли показывать расширенную HTML-таблицу
    var notesHaveProblems = (notesDiagnosis.problems.length > 0);
    var commentsHaveProblems = (commentsDiagnosis.problems.length > 0);
    var showTable = showTableIfNoProblems == 1 || notesHaveProblems || commentsHaveProblems;
    
    if (hasAny && showTable) {
        setTimeout(function() {
            ShowExtendedStatistics(notesDiagnosis, commentsDiagnosis, bodyNotes, bodyComments);
        }, 100);
    }
    
    // ==================================================
    // ФУНКЦИЯ ПОКАЗА РАСШИРЕННОЙ СТАТИСТИКИ В HTML
    // ==================================================
    
    function ShowExtendedStatistics(notesDiag, commentsDiag, notesBody, commentsBody) {
        var hasNotes = notesDiag.totalMarkers > 0 || notesDiag.sectionCount > 0;
        var hasComments = commentsDiag.totalMarkers > 0 || commentsDiag.sectionCount > 0;
        
        var html = "";
        html += '<style>\n';
        html += 'body { font-family: Tahoma, sans-serif; font-size: 12px; color: #000000; }\n';
        html += 'table { border-collapse: collapse; }\n';
        html += 'th, td { font-family: Tahoma, sans-serif; font-size: 12px; color: #000000; }\n';
        html += '.small-info { font-size: 14px; font-weight: bold; margin: 4px 0; }\n';
        html += '.summary-line { font-size: 13px; margin: 6px 0; }\n';
        html += '</style>\n';
        
        // === Общая информация о наличии разделов ===
        html += '<div style="background-color: #f0f8ff; border: 2px solid #336699; padding: 10px; margin: 10px 0;">\n';
        
        if (hasNotes && hasComments) {
            html += '<p style="font-weight: bold; margin: 0;">В документе найдены разделы примечаний (class notes) и комментариев (class comments).</p>\n';
        } else if (hasNotes && !hasComments) {
            html += '<p style="font-weight: bold; margin: 0;">В документе найден только раздел примечаний (class notes).</p>\n';
            html += '<p style="margin: 5px 0 0 0;">Раздел комментариев (class comments) отсутствует.</p>\n';
        } else if (!hasNotes && hasComments) {
            html += '<p style="font-weight: bold; margin: 0;">В документе найден только раздел комментариев (class comments).</p>\n';
            html += '<p style="margin: 5px 0 0 0;">Раздел примечаний (class notes) отсутствует.</p>\n';
        }
        
        html += '</div>\n';
        
        if (displayMode == "separate") {
            // Режим: отдельно примечания, потом комментарии
            if (hasNotes) {
                html += "<h2>Примечания (class notes)</h2>";
                html += '<p class="summary-line"><b>Найдено маркеров:</b> ' + notesDiag.totalMarkers;
                if (notesDiag.maxNum > 0) {
                    html += " (последний номер " + notesDiag.maxNum + ")";
                }
                html += '</p>';
                html += '<p class="small-info">Максимальный номер маркера: ' + notesDiag.maxMarkerNum + '</p>';
                html += '<p class="summary-line"><b>Найдено ID номеров примечаний:</b> ' + notesDiag.sectionCount;
                if (notesDiag.maxIdNum > 0) {
                    html += " (последний номер " + notesDiag.maxIdNum + ")";
                }
                html += '</p>';
                html += '<p class="small-info">Максимальный ID номер раздела примечания: ' + notesDiag.maxIdNum + '</p>';
                html += generateDiagnosisHTML(notesDiag, "примечаний");
                
                if (hasComments) {
                    html += "<hr>";
                }
            }
            
            if (hasComments) {
                html += "<h2>Комментарии (class comments)</h2>";
                html += '<p class="summary-line"><b>Найдено маркеров:</b> ' + commentsDiag.totalMarkers;
                if (commentsDiag.maxNum > 0) {
                    html += " (последний номер " + commentsDiag.maxNum + ")";
                }
                html += '</p>';
                html += '<p class="small-info">Максимальный номер маркера: ' + commentsDiag.maxMarkerNum + '</p>';
                html += '<p class="summary-line"><b>Найдено ID номеров комментариев:</b> ' + commentsDiag.sectionCount;
                if (commentsDiag.maxIdNum > 0) {
                    html += " (последний номер " + commentsDiag.maxIdNum + ")";
                }
                html += '</p>';
                html += '<p class="small-info">Максимальный ID номер раздела комментария: ' + commentsDiag.maxIdNum + '</p>';
                html += generateDiagnosisHTML(commentsDiag, "комментариев");
            }
        } else {
            // Режим: смешанный (в порядке появления в документе)
            html += "<h2>Примечания и комментарии в порядке появления в документе</h2>";
            html += '<p class="summary-line"><b>Примечаний:</b> ' + notesDiag.totalMarkers;
            if (notesDiag.maxNum > 0) html += " (последний номер " + notesDiag.maxNum + ")";
            html += ', <b>Комментариев:</b> ' + commentsDiag.totalMarkers;
            if (commentsDiag.maxNum > 0) html += " (последний номер " + commentsDiag.maxNum + ")";
            html += '</p>';
            
            // Общая статистика по маркерам
            if (hasNotes) {
                html += '<p class="small-info">Максимальный номер маркера примечаний: ' + notesDiag.maxMarkerNum + '</p>';
                html += '<p class="small-info">Максимальный ID номер раздела примечания: ' + notesDiag.maxIdNum + '</p>';
            }
            if (hasComments) {
                html += '<p class="small-info">Максимальный номер маркера комментариев: ' + commentsDiag.maxMarkerNum + '</p>';
                html += '<p class="small-info">Максимальный ID номер раздела комментария: ' + commentsDiag.maxIdNum + '</p>';
            }
            
            // Объединённая таблица в порядке появления
            html += '<h3>Последовательность ссылок в порядке появления</h3>';
            html += generateMixedDiagnosisHTML(notesDiag, commentsDiag);
            
            // Если какого-то раздела нет — сообщаем
            if (!hasNotes) {
                html += "<p>Примечания не обнаружены.</p>";
            }
            if (!hasComments) {
                html += "<p>Комментарии не обнаружены.</p>";
            }
        }
        
        html += "<p align=\"center\"><small>Диагностика структуры примечаний и комментариев - версия 3.2<br>Скрипт для редактора FBE (Fiction Book Editor)</small></p>";
        
        // Выбираем способ показа
        if (show == "1") {
            MyMsgWindow1(html);
        } else {
            // Базовый путь к папке HTML
            var basePath = document.location.href.replace("file:///", "").replace(/%20/g, " ").replace(/main\.html/, "HTML/");
            var filePatch = basePath + "diagnostic_notes_temp.html";
            var systemPath = basePath.replace(/\//g, "\\");
            MyMsgWindow2(html, filePatch, systemPath);
        }
    }
    
    // ==================================================
    // MyMsgWindow1 — окно IE через window.open
    // ==================================================
    
    function MyMsgWindow1(html) {
        var MsgWindow = window.open("HTML/Диагностика структуры примечаний и комментариев.html", null,
            "height=" + winHeight +
            ",width=" + winWidth +
            ",left=" + winLeft +
            ",top=" + winTop + ",status=no,toolbar=no,menubar=no,location=no,scrollbars=yes,resizable=yes");
        
        if (MsgWindow) {
            MsgWindow.document.body.innerHTML = html;
        } else {
            MsgBox("Не удалось открыть окно для расширенной статистики.\nВозможно, ваш браузер блокирует всплывающие окна.");
        }
    }
    
    // ==================================================
    // MyMsgWindow2 — браузер по умолчанию через временный файл
    // ==================================================
    
    function MyMsgWindow2(html, filePatch, systemPath) {
        try {
            var shell = new ActiveXObject("WScript.Shell");
            var fso = new ActiveXObject("Scripting.FileSystemObject");
            
            // Создаем файл с поддержкой Unicode (UTF-16)
            var fh = fso.CreateTextFile(filePatch, true, true);
            
            // Записываем шапку HTML
            fh.WriteLine("<!DOCTYPE html><html><head><meta http-equiv='Content-Type' content='text/html; charset=utf-8'><title>Диагностика структуры примечаний и комментариев</title>");
            
            // Стили
            fh.WriteLine("<style>body { font-family: Tahoma, sans-serif; font-size: 12px; padding: 20px; color: #000000; } table { border-collapse: collapse; width: 100%; font-size: 12px; } th, td { border: 1px solid #ccc; padding: 6px; font-size: 12px; color: #000000; } th { background-color: #f2f2f2; }" +
                ".small-info { font-size: 14px; font-weight: bold; margin: 4px 0; }" +
                ".summary-line { font-size: 13px; margin: 6px 0; }" +
                "</style></head><body>");
            
            // Добавляем информационную плашку
            var help = "<p><small>Скрипт открыл таблицу в браузере по умолчанию.</small><br><small>" +
                "Отчет сохранен в файле diagnostic_notes_temp.html в папке:\n" + systemPath + ".</small><br><small>" +
                "При необходимости сохраните этот файл отдельно.</small></p>";
            html = '<div style="margin: 10px 0 20px 0; padding: 10px; background-color: #f0f0f0; border-left: 4px solid #ccc;">' + help + '</div>' + html;
            
            // Записываем содержимое
            fh.WriteLine(html);
            
            // Закрываем теги и файл
            fh.WriteLine("</body></html>");
            fh.Close();
            
            // Запускаем в браузере по умолчанию
            shell.Run("\"" + filePatch + "\"");
            
        } catch (e) {
            window.clipboardData.setData("Text", html);
            MsgBox("Не удалось автоматически запустить браузер.\nОшибка: " + e.description + "\n\nHTML-код таблицы скопирован в буфер обмена.");
        }
    }
    
    // ==================================================
    // ФУНКЦИИ ГЕНЕРАЦИИ HTML-ТАБЛИЦ
    // ==================================================
    
    function generateDiagnosisHTML(diagnosis, sectionTypeName) {
        var html = "";
        
        if (diagnosis.problems.length == 0) {
            html += '<div style="background-color: #f0fff0; border: 2px solid #006600; padding: 10px; margin: 10px 0;">\n';
            html += '<p style="color: #006600; font-weight: bold; margin: 0;">\u2713 ПРОБЛЕМ НЕ ОБНАРУЖЕНО!</p>\n';
            html += '<p style="margin: 5px 0 0 0;">Структура ' + sectionTypeName + ' корректна.</p>\n';
            html += '</div>\n';
        } else {
            html += '<div style="background-color: #fff0f0; border: 2px solid #cc0000; padding: 10px; margin: 10px 0;">\n';
            html += '<p style="color: #cc0000; font-weight: bold; margin: 0;">\u2717 ОБНАРУЖЕННЫЕ ПРОБЛЕМЫ (' + diagnosis.problems.length + ')</p>\n';
            
            for (var i = 0; i < diagnosis.problems.length; i++) {
                var problem = diagnosis.problems[i];
                var problemColor = "#cc3333";
                if (problem.type == "duplicate_link" || problem.type == "broken_sequence") problemColor = "#cc6600";
                if (problem.type == "missing_both") problemColor = "#cc0000";
                
                html += '<div style="background-color: #fff0f0; border-left: 3px solid ' + problemColor + '; padding: 5px; margin: 3px 0;">\n';
                html += '<b>' + (i + 1) + '.</b> ' + escapeHTML(problem.message) + '\n';
                html += '</div>\n';
            }
            html += '</div>\n';
        }
        
        html += '<table border="1" cellpadding="4" cellspacing="0" style="border-collapse: collapse; width: 100%; margin: 10px 0;">\n';
        html += '<tr style="background-color: #f2f2f2;"><td style="width: 50%;"><b>Всего маркеров:</b></td><td>' + diagnosis.totalMarkers + '</td></tr>\n';
        html += '<tr><td><b>Типы маркеров:</b></td><td>\n';
        html += '[] (квадратные скобки): ' + diagnosis.markersByType.bracket + '<br>\n';
        html += 'N (просто цифра): ' + diagnosis.markersByType.number + '<br>\n';
        html += '() (круглые скобки): ' + diagnosis.markersByType.paren + '<br>\n';
        html += '{} (фигурные скобки): ' + diagnosis.markersByType.brace + '<br>\n';
        html += '* (звездочки): ' + diagnosis.markersByType.star + '<br>\n';
        html += 'Верхний индекс: ' + diagnosis.markersByType.superscript + '<br>\n';
        html += 'Другие форматы: ' + diagnosis.markersByType.other + '\n';
        html += '</td></tr>\n';
        html += '</table>\n';
        
        if (diagnosis.maxNum > 0) {
            html += '<h3>Последовательность ссылок</h3>\n';
            html += '<table border="1" cellpadding="4" cellspacing="0" style="border-collapse: collapse; width: 100%;">\n';
            html += '<tr style="background-color: #f2f2f2;"><th>№</th><th>Маркер</th><th>Тип маркера</th><th>Раздел</th><th>ID раздела</th><th>Число</th><th>Соответствие</th></tr>\n';
            
            for (var num = 1; num <= diagnosis.maxNum; num++) {
                var markerInfos = diagnosis.markersByNum[num];
                var sectionId = diagnosis.numToSectionId[num] || "";
                var sectionExists = (sectionId.length > 0);
                
                var hasMarker = (markerInfos && markerInfos.length > 0);
                
                var isProblem = false;
                
                if (!hasMarker && !sectionExists) {
                    isProblem = true;
                } else if (!hasMarker && sectionExists) {
                    isProblem = true;
                } else if (hasMarker && !sectionExists) {
                    isProblem = true;
                } else if (hasMarker && sectionExists) {
                    var mi0 = markerInfos[0];
                    if (!mi0.sectionExists) {
                        isProblem = true;
                    }
                }
                
                var rowStyle = isProblem ? "background-color: #fff0f0;" : "";
                
                html += '<tr style="' + rowStyle + '">';
                html += '<td>' + num + '</td>';
                
                if (!hasMarker && !sectionExists) {
                    html += '<td>-</td>';
                    html += '<td>-</td>';
                    html += '<td>-</td>';
                    html += '<td>-</td>';
                    html += '<td>-</td>';
                    html += '<td style="color: #cc0000; font-weight: bold;">НЕТ</td>';
                } else if (!hasMarker && sectionExists) {
                    html += '<td style="color: #cc0000;">нет маркера</td>';
                    html += '<td>-</td>';
                    html += '<td>' + num + '</td>';
                    html += '<td>' + sectionId + '</td>';
                    html += '<td>-</td>';
                    html += '<td style="color: #cc0000; font-weight: bold;">НЕТ</td>';
                } else if (hasMarker && !sectionExists) {
                    var mi0 = markerInfos[0];
                    var numFromTextDisplay = "нет";
                    if (mi0.numFromText !== null && mi0.numFromText !== undefined) {
                        numFromTextDisplay = mi0.numFromText.toString();
                    }
                    
                    html += '<td style="max-width: 80px; overflow: hidden; white-space: nowrap;">' + escapeHTML(mi0.textStripped) + '</td>';
                    html += '<td>' + getMarkerTypeDisplay(mi0.markerType) + '</td>';
                    html += '<td style="color: #cc0000;">отсутствует</td>';
                    html += '<td style="color: #cc0000;">' + (mi0.sectionId || "нет") + '</td>';
                    html += '<td style="max-width: 60px; overflow: hidden; white-space: nowrap;">' + numFromTextDisplay + '</td>';
                    html += '<td style="color: #cc0000; font-weight: bold;">НЕТ</td>';
                } else {
                    var mi0 = markerInfos[0];
                    
                    var corresponds = "";
                    var correspondColor = "";
                    
                    if (!mi0.sectionExists) {
                        corresponds = "НЕТ";
                        correspondColor = " style=\"color: #cc0000; font-weight: bold;\"";
                    } else {
                        if (mi0.numFromText !== null && mi0.sectionIdNum !== null) {
                            if (mi0.numFromText == mi0.sectionIdNum) {
                                corresponds = "ДА";
                            } else {
                                corresponds = "НЕТ";
                                correspondColor = " style=\"color: #cc0000; font-weight: bold;\"";
                            }
                        } else if (mi0.numFromText !== null && mi0.sectionNum !== null) {
                            if (mi0.numFromText == mi0.sectionNum) {
                                corresponds = "ДА";
                            } else {
                                corresponds = "НЕТ";
                                correspondColor = " style=\"color: #cc0000; font-weight: bold;\"";
                            }
                        } else {
                            corresponds = "?";
                        }
                    }
                    
                    var sectionNumDisplay = "";
                    if (mi0.sectionIdNum !== null && mi0.sectionIdNum !== undefined) {
                        sectionNumDisplay = mi0.sectionIdNum.toString();
                    } else if (mi0.sectionNum !== null && mi0.sectionNum !== undefined) {
                        sectionNumDisplay = mi0.sectionNum.toString();
                    } else {
                        sectionNumDisplay = "?";
                    }
                    
                    var numFromTextDisplay = "";
                    if (mi0.numFromText !== null && mi0.numFromText !== undefined) {
                        numFromTextDisplay = mi0.numFromText.toString();
                    } else {
                        numFromTextDisplay = "нет";
                    }
                    
                    html += '<td>' + escapeHTML(mi0.textStripped) + '</td>';
                    html += '<td>' + getMarkerTypeDisplay(mi0.markerType) + '</td>';
                    html += '<td>' + sectionNumDisplay + '</td>';
                    html += '<td>' + (mi0.sectionId || sectionId || "нет") + '</td>';
                    html += '<td>' + numFromTextDisplay + '</td>';
                    html += '<td' + correspondColor + '>' + corresponds + '</td>';
                    
                    for (var d = 1; d < markerInfos.length; d++) {
                        var miD = markerInfos[d];
                        var corrD = "?";
                        var corrColorD = "";
                        
                        if (!miD.sectionExists) {
                            corrD = "НЕТ";
                            corrColorD = " style=\"color: #cc0000; font-weight: bold;\"";
                        } else if (miD.numFromText !== null && miD.sectionIdNum !== null && miD.numFromText == miD.sectionIdNum) {
                            corrD = "ДА";
                        } else {
                            corrD = "НЕТ";
                            corrColorD = " style=\"color: #cc0000; font-weight: bold;\"";
                        }
                        
                        var secNumDD = "?";
                        if (miD.sectionIdNum !== null && miD.sectionIdNum !== undefined) {
                            secNumDD = miD.sectionIdNum.toString();
                        } else if (miD.sectionNum !== null && miD.sectionNum !== undefined) {
                            secNumDD = miD.sectionNum.toString();
                        }
                        
                        var numTxtDD = "нет";
                        if (miD.numFromText !== null && miD.numFromText !== undefined) {
                            numTxtDD = miD.numFromText.toString();
                        }
                        
                        html += '<tr style="' + rowStyle + '">';
                        html += '<td></td>';
                        html += '<td>' + escapeHTML(miD.textStripped) + '</td>';
                        html += '<td>' + getMarkerTypeDisplay(miD.markerType) + '</td>';
                        html += '<td>' + secNumDD + '</td>';
                        html += '<td>' + (miD.sectionId || sectionId || "нет") + '</td>';
                        html += '<td>' + numTxtDD + '</td>';
                        html += '<td' + corrColorD + '>' + corrD + '</td>';
                        html += '</tr>\n';
                    }
                }
                html += '</tr>\n';
            }
            html += '</table>\n';
        }
        
        return html;
    }
    
    // Генерирует HTML для смешанного режима (примечания + комментарии вперемешку)
    function generateMixedDiagnosisHTML(notesDiag, commentsDiag) {
        var html = "";
        
        var mixedSequence = [];
        
        for (var i = 0; i < notesDiag.linkSequence.length; i++) {
            var item = notesDiag.linkSequence[i];
            item.linkType = "Примечание";
            mixedSequence.push(item);
        }
        
        for (var i = 0; i < commentsDiag.linkSequence.length; i++) {
            var item = commentsDiag.linkSequence[i];
            item.linkType = "Комментарий";
            mixedSequence.push(item);
        }
        
        for (var m = 0; m < mixedSequence.length - 1; m++) {
            for (var n = m + 1; n < mixedSequence.length; n++) {
                if (mixedSequence[m].index > mixedSequence[n].index) {
                    var temp = mixedSequence[m];
                    mixedSequence[m] = mixedSequence[n];
                    mixedSequence[n] = temp;
                }
            }
        }
        
        if (mixedSequence.length > 0) {
            html += '<table border="1" cellpadding="4" cellspacing="0" style="border-collapse: collapse; width: 100%;">\n';
            html += '<tr style="background-color: #f2f2f2;"><th>Тип</th><th>Маркер</th><th>Тип маркера</th><th>Раздел</th><th>ID раздела</th><th>Число</th><th>Соответствие</th></tr>\n';
            
            for (var i = 0; i < mixedSequence.length; i++) {
                var info = mixedSequence[i];
                
                var isProblemRow = !info.sectionExists;
                var rowStyle = isProblemRow ? "background-color: #fff0f0;" : "";
                
                var corresponds = "?";
                var correspondColor = "";
                
                if (!info.sectionExists) {
                    corresponds = "НЕТ";
                    correspondColor = " style=\"color: #cc0000; font-weight: bold;\"";
                } else if (info.numFromText !== null && info.sectionIdNum !== null) {
                    if (info.numFromText == info.sectionIdNum) {
                        corresponds = "ДА";
                    } else {
                        corresponds = "НЕТ";
                        correspondColor = " style=\"color: #cc0000; font-weight: bold;\"";
                    }
                }
                
                var sectionNumDisplay = "?";
                if (info.sectionIdNum !== null && info.sectionIdNum !== undefined) {
                    sectionNumDisplay = info.sectionIdNum.toString();
                } else if (info.sectionNum !== null && info.sectionNum !== undefined) {
                    sectionNumDisplay = info.sectionNum.toString();
                }
                
                var numFromTextDisplay = "нет";
                if (info.numFromText !== null && info.numFromText !== undefined) {
                    numFromTextDisplay = info.numFromText.toString();
                }
                
                var rowColor = (info.linkType == "Примечание") ? "#DEB887" : "#87CEFA";
                
                html += '<tr style="' + rowStyle + '">';
                html += '<td style="background-color: ' + rowColor + ';"><b>' + info.linkType + '</b></td>';
                html += '<td>' + escapeHTML(info.textStripped) + '</td>';
                html += '<td>' + getMarkerTypeDisplay(info.markerType) + '</td>';
                html += '<td>' + sectionNumDisplay + '</td>';
                html += '<td>' + (info.sectionId || "нет") + '</td>';
                html += '<td>' + numFromTextDisplay + '</td>';
                html += '<td' + correspondColor + '>' + corresponds + '</td>';
                html += '</tr>\n';
            }
            html += '</table>\n';
        }
        
        var allProblems = [];
        for (var i = 0; i < notesDiag.problems.length; i++) {
            var p = notesDiag.problems[i];
            p.sourceType = "Примечание";
            allProblems.push(p);
        }
        for (var i = 0; i < commentsDiag.problems.length; i++) {
            var p = commentsDiag.problems[i];
            p.sourceType = "Комментарий";
            allProblems.push(p);
        }
        
        sortProblems(allProblems);
        
        if (allProblems.length > 0) {
            html += '<h3>Все проблемы (' + allProblems.length + ')</h3>\n';
            for (var i = 0; i < allProblems.length; i++) {
                var problem = allProblems[i];
                var problemColor = "#cc3333";
                if (problem.type == "duplicate_link" || problem.type == "broken_sequence") problemColor = "#cc6600";
                if (problem.type == "missing_both") problemColor = "#cc0000";
                
                html += '<div style="background-color: #fff0f0; border-left: 3px solid ' + problemColor + '; padding: 5px; margin: 3px 0;">\n';
                html += '<b>' + (i + 1) + '. [' + problem.sourceType + ']</b> ' + escapeHTML(problem.message) + '\n';
                html += '</div>\n';
            }
        } else {
            html += '<div style="background-color: #f0fff0; border: 2px solid #006600; padding: 10px; margin: 10px 0;">\n';
            html += '<p style="color: #006600; font-weight: bold;">\u2713 ПРОБЛЕМ НЕ ОБНАРУЖЕНО!</p>\n';
            html += '</div>\n';
        }
        
        return html;
    }
}
