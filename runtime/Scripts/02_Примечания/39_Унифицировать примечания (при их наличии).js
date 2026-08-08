// Скрипт "Унифицировать примечания (при их наличии)" для редактора FBE
// version 1.6

// Основано на скрипте "Унификация сносок" (Перенумеровать примечания), версия 3.0
// Автор исходного скрипта - Sclex
// Доработка - DeepSeek, TaKir

// Скрипт предназначен для унификации сносок (примечаний) в fb2 документах
// только при их реальном наличии.
// Данный скрипт НЕ создает раздел сносок (примечаний), если в документе вообще нет
// маркеров сносок и раздела сносок (примечаний).

// Добавлена расширенная статистика сделанных скриптом изменений.
// Добавлена дополнительная проверка корректности оформления примечаний
// из соответствующего скрипта тов. Sclex-а.
// Улучшен и расширен функционал приведения в соответствие
// непоследовательной нумерации примечаний в документе.

// version 1.6, 12.01.2026
//======================================

function Run() {
    var Ts = new Date().getTime();
    var commentRegExp = new RegExp("^c_", "");
    
    //НАСТРОЙКИ - начало
    //здесь шаблоны, которые используются при работе скрипта
    //используйте макрос %N для указания номера примечания
    var strConst1 = "n_%N"; //шаблон ID разделов примечаний
    var strConst2 = "%N"; //шаблон заголовка примечания
    var strConst3 = ""; //шаблон содержания вставленного примечания
    var strConst4 = "[%N]"; //шаблон текста ссылки
    var strConst5 = "Примечания"; //шаблон заголовка боди нотесов
    var strConst6 = "notes"; //значение атрибута name у body примечаний
    var strConst7 = "Note adding";
    
    //функция, которая определяет, является ли элемент el, который
    //заведомо является ссылкой, ссылкой на тот тип примечаний,
    //который обрабатывается данным скриптом
    var isItNote = function(el) {
        return el.className == "note" ? true : false;
    };
    
    var makeNoteFromHref = function(el) {
        el.className = "note";
    };
    
    // выводить ли окно, извещающее о конце работы скрипта?
    // true - выводить. false - не выводить
    var EndWindow = true;
    
    //добавлять ли новую сноску или только провести работы по упорядочению существующих
    //true - добавлять. false - не добавлять
    var addSnoska = false;
    
    //показывать ли форму для ввода текста примечания
    //true - показывать. false - не показывать
    var InputSnoskaText = false;
    
    //перемещать ли фокус видимости на раздел свежесозданного примечания
    var MoveFocusToNote = false;
    
    //true, если это скрипт добавления последней сноски
    var lastSnoskaMode = false;
    
    //режим ускоренной работы
    var forsazh = false;
    //НАСТРОЙКИ - конец
    
    // Переменные для статистики
    var addedClassNote = 0;
    var actuallyRenamedNotes = 0; // СНОСКИ, У КОТОРЫХ ИЗМЕНИЛСЯ НОМЕР
    var changedIds = 0;
    var changedTitles = 0;
    var convertedToNewFormat = false;
    
    // Детальная статистика по преобразованию формата маркеров
    var convertedMarkers = {
        starToBracket: 0,      // * → [N]
        numberToBracket: 0,    // 1 → [N]
        parenToBracket: 0,     // (1) → [N]
        superscriptToBracket: 0, // ¹ → [N]
        otherToBracket: 0      // другие → [N]
    };
    
    // Статистика по унификации ID
    var unifiedIds = 0;
    var oldIdPattern = /^fn\d+$/i; // старый формат fn1, fn2 и т.д.
    
    // Переменные для диагностики
    var diagnosticInfo = {
        brokenSequence: false,     // Нарушена последовательность
        duplicateLinks: 0,         // Дублирующие ссылки
        sectionsWithoutLinks: 0,   // Разделы без ссылок
        linksWithoutSections: 0,   // Ссылки без разделов
        gapsInNumbering: 0,        // Разрывы в нумерации
        sequenceFixed: false,      // Последовательность восстановлена
        duplicatesFixed: false,    // Дубли устранены
        firstLinkNotFirstSection: false, // Первая ссылка не на первый раздел
        // Детальная информация о проблемах
        problemDetails: [],
        firstProblemLink: null,    // Первая проблемная ссылка для перехода
        firstProblemSection: null  // Первый проблемный раздел для перехода
    };
    
    // Функция для извлечения номера из href
    function extractNumberFromHref(href) {
        if (!href) return null;
        
        // Удаляем все до #
        var pos = href.indexOf('#');
        if (pos === -1) return null;
        
        var anchor = href.substring(pos + 1);
        
        // Пытаемся извлечь число из разных форматов
        var match;
        
        // Старый формат: fn1, fn2
        match = anchor.match(/^fn(\d+)$/i);
        if (match) return parseInt(match[1], 10);
        
        // Новый формат: n_1, n_2
        match = anchor.match(/^n_(\d+)$/i);
        if (match) return parseInt(match[1], 10);
        
        // Другие возможные форматы
        match = anchor.match(/(\d+)$/);
        if (match) return parseInt(match[1], 10);
        
        return null;
    }
    
    // Функция для извлечения номера из текста маркера
    function extractNumberFromText(text) {
        if (!text) return null;
        
        // Формат [1], [2]
        var match = text.match(/^\[(\d+)\]$/);
        if (match) return parseInt(match[1], 10);
        
        // Формат (1), (2)
        match = text.match(/^\((\d+)\)$/);
        if (match) return parseInt(match[1], 10);
        
        // Просто цифры: 1, 2
        match = text.match(/^(\d+)$/);
        if (match) return parseInt(match[1], 10);
        
        // Надстрочные цифры - преобразуем
        var superscriptMap = {
            '¹': 1, '²': 2, '³': 3, '⁴': 4, '⁵': 5,
            '⁶': 6, '⁷': 7, '⁸': 8, '⁹': 9, '⁰': 0
        };
        
        if (text.length === 1 && superscriptMap[text] !== undefined) {
            return superscriptMap[text];
        }
        
        // Звездочки - не числа
        if (/^\*+$/.test(text)) return null;
        
        return null;
    }
    
    // Функция для получения локальной части href
    function GetLocalHref(name) {
        var name1 = name;
        if (name1.indexOf("#") < 0) {
            return "1";
        }
        var thg = new RegExp("main\.html\#", "i");
        var srch10 = name1.search(thg);
        if (srch10 == -1) {
            name1 = name1.substring(1, name1.length);
        } else {
            name1 = name1.substring(srch10 + 10, name1.length);
        }
        return name1;
    }
    
    // Функция для определения типа маркера
    function getMarkerType(text) {
        // Проверяем на звездочки: *, **, ***
        if (/^\*+$/.test(text)) {
            return "star";
        }
        // Проверяем на простые цифры: 1, 2, 3
        else if (/^\d+$/.test(text)) {
            return "number";
        }
        // Проверяем на цифры в скобках: (1), (2)
        else if (/^\(\d+\)$/.test(text)) {
            return "paren";
        }
        // Проверяем на надстрочные цифры (Unicode)
        else if (/^[\u00B9\u00B2\u00B3\u2070\u2074-\u2079]+$/.test(text)) {
            return "superscript";
        }
        // Проверяем на новый формат [1], [2]
        else if (/^\[\d+\]$/.test(text)) {
            return "bracket";
        }
        // Все остальное
        else {
            return "other";
        }
    }
    
    // Функция для диагностики последовательности (совместимая с IE6)
    function diagnoseSequence(sectsColl, sectNumById, totalMarkers) {
        var diagnosis = {
            brokenSequence: false,
            duplicateLinks: 0,
            sectionsWithoutLinks: 0,
            linksWithoutSections: 0,
            gapsInNumbering: 0,
            firstLinkNotFirstSection: false,
            expectedNumbers: [],
            actualNumbers: [],
            problemDetails: [],
            firstProblemLink: null,
            firstProblemSection: null
        };
        
        // Собираем информацию о ссылках
        var linkInfo = [];
        var usedSections = {};
        var sectionLinks = {}; // Какие ссылки ссылаются на каждый раздел
        var maxSectionNumber = 0;
        var isFirstLink = true;
        
        for (var i = 0; i < totalMarkers.length; i++) {
            var link = totalMarkers[i];
            var href = link.getAttribute("href") || "";
            
            // Пропускаем внешние ссылки
            if (href.indexOf("http://") != -1 || href.indexOf("https://") != -1 ||
                href.indexOf("ftp://") != -1 || href.indexOf("mailto:") != -1) {
                continue;
            }
            
            var sectionId = GetLocalHref(href);
            if (sectionId === "1" || sectionId === -1) continue;
            
            var sectionNum = sectNumById[sectionId];
            if (sectionNum) {
                linkInfo.push({
                    link: link,
                    sectionNum: sectionNum,
                    sectionId: sectionId,
                    linkIndex: i,
                    linkText: link.innerHTML
                });
                
                // Проверяем первую ссылку
                if (isFirstLink && sectionNum != 1) {
                    diagnosis.firstLinkNotFirstSection = true;
                    diagnosis.problemDetails.push({
                        type: "first_link_not_first",
                        link: link,
                        linkText: link.innerHTML,
                        sectionNum: sectionNum,
                        message: "Первая ссылка ссылается не на первый раздел.\n\nА вот сюда:\nНомер: " + sectionNum + "        id: " + sectionId
                    });
                    if (!diagnosis.firstProblemLink) {
                        diagnosis.firstProblemLink = link;
                    }
                }
                isFirstLink = false;
                
                // Отслеживаем использованные разделы
                if (!usedSections[sectionNum]) {
                    usedSections[sectionNum] = 1;
                    sectionLinks[sectionNum] = [link];
                } else {
                    usedSections[sectionNum]++;
                    if (!sectionLinks[sectionNum]) {
                        sectionLinks[sectionNum] = [];
                    }
                    sectionLinks[sectionNum].push(link);
                }
                
                if (sectionNum > maxSectionNumber) {
                    maxSectionNumber = sectionNum;
                }
                
                diagnosis.actualNumbers.push(sectionNum);
            } else {
                diagnosis.linksWithoutSections++;
                diagnosis.problemDetails.push({
                    type: "link_without_section",
                    link: link,
                    linkText: link.innerHTML,
                    href: href,
                    message: "Ссылка ссылается на несуществующий раздел: " + sectionId
                });
                if (!diagnosis.firstProblemLink) {
                    diagnosis.firstProblemLink = link;
                }
            }
        }
        
        // Проверяем последовательность
        if (linkInfo.length > 0) {
            var prevNum = linkInfo[0].sectionNum;
            diagnosis.expectedNumbers.push(prevNum);
            
            for (var j = 1; j < linkInfo.length; j++) {
                var currentNum = linkInfo[j].sectionNum;
                diagnosis.expectedNumbers.push(currentNum);
                
                // Проверяем, что текущий номер >= предыдущего
                if (currentNum < prevNum) {
                    diagnosis.brokenSequence = true;
                    diagnosis.problemDetails.push({
                        type: "broken_sequence",
                        link: linkInfo[j].link,
                        linkText: linkInfo[j].linkText,
                        currentNum: currentNum,
                        prevNum: prevNum,
                        message: "Нарушена последовательность. После ссылки на раздел " + prevNum + " идет ссылка на раздел " + currentNum
                    });
                    if (!diagnosis.firstProblemLink && !diagnosis.firstLinkNotFirstSection) {
                        diagnosis.firstProblemLink = linkInfo[j].link;
                    }
                }
                prevNum = currentNum;
            }
            
            // Проверяем дублирующие ссылки (совместимо с IE6)
            for (var section in usedSections) {
                if (usedSections.hasOwnProperty(section)) {
                    if (usedSections[section] > 1) {
                        diagnosis.duplicateLinks += (usedSections[section] - 1);
                        
                        // Записываем детали о дублях
                        if (sectionLinks[section] && sectionLinks[section].length > 1) {
                            var duplicateLinks = sectionLinks[section];
                            for (var d = 1; d < duplicateLinks.length; d++) {
                                diagnosis.problemDetails.push({
                                    type: "duplicate_link",
                                    link: duplicateLinks[d],
                                    linkText: duplicateLinks[d].innerHTML,
                                    sectionNum: section,
                                    duplicateCount: duplicateLinks.length,
                                    message: "Вторая сноска " + duplicateLinks[d].innerHTML + ", которая ссылается на тот же раздел."
                                });
                                if (!diagnosis.firstProblemLink && !diagnosis.firstLinkNotFirstSection) {
                                    diagnosis.firstProblemLink = duplicateLinks[d];
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Подсчитываем количество разделов (совместимо с IE6)
        var sectionsCount = 0;
        for (var key in sectsColl) {
            if (sectsColl.hasOwnProperty(key)) {
                sectionsCount++;
            }
        }
        
        // Проверяем разделы без ссылок
        for (var sectNum = 1; sectNum <= sectionsCount; sectNum++) {
            if (!usedSections[sectNum]) {
                diagnosis.sectionsWithoutLinks++;
                diagnosis.problemDetails.push({
                    type: "section_without_link",
                    sectionNum: sectNum,
                    section: sectsColl[sectNum],
                    message: "Раздел " + sectNum + " не имеет ссылок"
                });
                if (!diagnosis.firstProblemSection) {
                    diagnosis.firstProblemSection = sectsColl[sectNum];
                }
            }
        }
        
        // Проверяем разрывы в нумерации (совместимо с IE6)
        if (diagnosis.actualNumbers.length > 1) {
            // Создаем копию массива для сортировки
            var allNumbers = [];
            for (var k = 0; k < diagnosis.actualNumbers.length; k++) {
                allNumbers.push(diagnosis.actualNumbers[k]);
            }
            
            // Простая сортировка пузырьком (совместимо с IE6)
            for (var m = 0; m < allNumbers.length - 1; m++) {
                for (var n = m + 1; n < allNumbers.length; n++) {
                    if (allNumbers[m] > allNumbers[n]) {
                        var temp = allNumbers[m];
                        allNumbers[m] = allNumbers[n];
                        allNumbers[n] = temp;
                    }
                }
            }
            
            // Проверяем разрывы
            var uniqueNumbers = [];
            for (var p = 0; p < allNumbers.length; p++) {
                if (p === 0 || allNumbers[p] !== allNumbers[p-1]) {
                    uniqueNumbers.push(allNumbers[p]);
                }
            }
            
            for (var q = 1; q < uniqueNumbers.length; q++) {
                if (uniqueNumbers[q] - uniqueNumbers[q-1] > 1) {
                    diagnosis.gapsInNumbering++;
                    diagnosis.problemDetails.push({
                        type: "gap_in_numbering",
                        missingStart: uniqueNumbers[q-1] + 1,
                        missingEnd: uniqueNumbers[q] - 1,
                        message: "Пропущены номера с " + (uniqueNumbers[q-1] + 1) + " по " + (uniqueNumbers[q] - 1)
                    });
                }
            }
        }
        
        return diagnosis;
    }
    
    // функция находит номер комментария, соответствующего определенному имени раздела
    // в исходном документе. В name передаем имя раздела, перед ним символ #, если это
    // локальная ссылка. В SectID передаем массив имен разделов.
    function findNum(name) {
        var i = 1;
        var name1 = name;
        var thg2 = new RegExp("#");
        if (name1.search(thg2) == -1) {
            return (-1);
        }
        var thg = new RegExp("main\.html\#", "i");
        srch10 = name1.search(thg);
        if (srch10 == -1) name1 = name1.substring(1);
        else name1 = name1.substring(srch10 + 10);
        while (i <= sectNum && sectIds[i] != name1) i++;
        if (i > sectNum) {
            return (-1);
        }
        return (i);
    }
    
    function PoShablonu(s, n) {
        var ttt3 = new RegExp("\%N");
        return (s.replace(ttt3, n));
    }
    
    function getLocalHref(name) {
        var i = 1;
        var name1 = name;
        var thg2 = new RegExp("#");
        if (name1.search(thg2) == -1) {
            return (-1);
        } //ссылка не может начинаться с 1
        var thg = new RegExp("main\.html\#", "i");
        srch10 = name1.search(thg);
        if (srch10 == -1) {
            name1 = name1.substring(1, name1.length);
        } else {
            name1 = name1.substring(srch10 + 10, name1.length);
        }
        return (name1);
    }
    
    function miniValidate(s) {
        //возвращаемые значения:
        //1 - недопустимый символ в имени тэга
        //2 - неразрешенное имя тэга
        //3 - закрывающий тэг не соответствует открывающему
        //4 - незакрытый тэг (нет правой угловой скобки)
        //5 - для открытого тэга нет соответствующего закрывающего
        var i = 0;
        var InsideTag = false;
        var TagStek = "";
        while (i < s.length) {
            var ch = s.substring(i, i + 1);
            if (InsideTag) {
                var ffg = new RegExp("[/a-z]", "gi");
                if (ch == "<") {
                    return 4;
                }
                if (ch.search(ffg) != -1) {
                    TagName = TagName + ch;
                } else if (ch == ">") {
                    InsideTag = false;
                    if (TagName != "STRONG" && TagName != "EM" && TagName != "P" && TagName != "/P" && TagName != "/STRONG" && TagName != "/EM" && TagName != "SUP" && TagName != "SUB" && TagName != "/SUP" && TagName != "/SUB") {
                        return 2;
                    }
                    if (TagName == "STRONG") {
                        TagStek = TagStek + "1";
                    }
                    if (TagName == "EM") {
                        TagStek = TagStek + "2";
                    }
                    if (TagName == "SUP") {
                        TagStek = TagStek + "3";
                    }
                    if (TagName == "SUB") {
                        TagStek = TagStek + "4";
                    }
                    if (TagName.substr(0, 1) == "/") {
                        var TagCodeFromStek = TagStek.substr(TagStek.length - 1, 1);
                        //        MsgBox("TagCodeFromStek:"+TagCodeFromStek+"\nTagName:"+TagName);
                        if (TagName == "/STRONG" && TagCodeFromStek != "1") {
                            return 3;
                        }
                        if (TagName == "/EM" && TagCodeFromStek != "2") {
                            return 3;
                        }
                        if (TagName == "/SUP" && TagCodeFromStek != "3") {
                            return 3;
                        }
                        if (TagName == "/SUB" && TagCodeFromStek != "4") {
                            return 3;
                        }
                        if (TagName == "/STRONG" || TagName == "/EM" || TagName == "/SUP" || TagName == "/SUB") {
                            TagStek = TagStek.substr(0, TagStek.length - 1);
                        }
                    }
                } else {
                    return 1;
                }
            }
            if (!InsideTag && ch == "<") {
                InsideTag = true;
                var TagName = "";
            }
            i++;
        }
        if (InsideTag) {
            return 4;
        }
        if (TagStek != "") {
            return 5;
        }
        return 0;
    }
    
    function makeGoodSnoska(snoskaTextSrc) {
        snoskaText = "<P>" + snoskaTextSrc + "</P>";
        var ffg1 = new RegExp("<b>", "gi");
        snoskaText = snoskaText.replace(ffg1, "<STRONG>");
        ffg1 = new RegExp("</b>", "gi");
        snoskaText = snoskaText.replace(ffg1, "</STRONG>");
        ffg1 = new RegExp("<i>", "gi");
        snoskaText = snoskaText.replace(ffg1, "<EM>");
        ffg1 = new RegExp("</i>", "gi");
        snoskaText = snoskaText.replace(ffg1, "</EM>");
        ffg1 = new RegExp("<emphasis>", "gi");
        snoskaText = snoskaText.replace(ffg1, "<EM>");
        ffg1 = new RegExp("</emphasis>", "gi");
        snoskaText = snoskaText.replace(ffg1, "</EM>");
        ffg1 = new RegExp("<strong>", "gi");
        snoskaText = snoskaText.replace(ffg1, "<STRONG>");
        ffg1 = new RegExp("</strong>", "gi");
        snoskaText = snoskaText.replace(ffg1, "</STRONG>");
        ffg1 = new RegExp("<sup>", "gi");
        snoskaText = snoskaText.replace(ffg1, "<SUP>");
        ffg1 = new RegExp("</sup>", "gi");
        snoskaText = snoskaText.replace(ffg1, "</SUP>");
        ffg1 = new RegExp("<sub>", "gi");
        snoskaText = snoskaText.replace(ffg1, "<SUB>");
        ffg1 = new RegExp("</sub>", "gi");
        snoskaText = snoskaText.replace(ffg1, "</SUB>");
        ffg1 = new RegExp("<br>", "gi");
        snoskaText = snoskaText.replace(ffg1, "</P>\n<P>");
        return snoskaText;
    }
    
    function getRandomNum(n) {
        var s = "";
        for (var i = 1; i <= n; i++) {
            s = s + Math.floor(Math.random() * 10);
        }
        return s;
    }
    
    // === ПРОВЕРКА НАЛИЧИЯ ПРИМЕЧАНИЙ В ДОКУМЕНТЕ ===
    
    window.external.BeginUndoUnit(document, strConst7);
    
    var body = document.getElementById("fbw_body");
    if (!body) {
        MsgBox("Ошибка. Body не найден!");
        window.external.EndUndoUnit(document);
        return;
    }
    
    // Поиск body с примечаниями
    var bodyNotes = null;
    var ptr = body.firstChild;
    while (ptr) {
        if (ptr.className == "body" && ptr.getAttribute("fbname") == strConst6) {
            bodyNotes = ptr;
            break;
        }
        ptr = ptr.nextSibling;
    }
    
    // Собираем все маркеры сносок
    var totalMarkers = [];
    var hasNoteLinks = false;
    var noteLinksCount = 0;
    var totalMarkersCount = 0;
    
    for (var i = 0; i < document.links.length; i++) {
        var link = document.links[i];
        var href = link.getAttribute("href") || "";
        
        // Проверяем, является ли ссылка сноской
        if (href.indexOf("#") >= 0 || link.className == "note") {
            totalMarkersCount++;
            totalMarkers.push(link);
            
            if (link.className == "note") {
                hasNoteLinks = true;
                noteLinksCount++;
            }
        }
    }
    
    // Проверка: если нет ни body с примечаниями, ни ссылок с классом "note"
    if (!bodyNotes && !hasNoteLinks && totalMarkersCount == 0) {
        var Tf = new Date().getTime();
        var Tmin = Math.floor((Tf - Ts) / 60000);
        var Tsek = Math.ceil(10 * ((Tf - Ts) / 1000 - Tmin * 60)) / 10;
        if (Tmin > 0) {
            var TimeStr = Tmin + " мин. " + Tsek + " с";
        } else {
            var TimeStr = Tsek + " с";
        }
        
        MsgBox("Унифицировать примечания (при их наличии)\n" +
               "ver. 1.6\n" +
               "---------------------------------------\n" +
               "В документе не обнаружено примечаний.\n" +
               "Отсутствуют:\n" +
               "1. Раздел с примечаниями (body с fbname='notes')\n" +
               "2. Ссылки-маркеры сносок\n\n" +
               "Скрипт завершает работу без изменений.\n" +
               "Время работы: " + TimeStr);
        
        window.external.EndUndoUnit(document);
        return;
    }
    
    // Если есть только body с примечаниями, но нет ссылок
    if (bodyNotes && !hasNoteLinks && totalMarkersCount == 0) {
        // Проверяем, есть ли разделы в body с примечаниями
        var hasSections = false;
        var sectCheck = bodyNotes.firstChild;
        while (sectCheck) {
            if (sectCheck.nodeName == "DIV" && sectCheck.className == "section") {
                hasSections = true;
                break;
            }
            sectCheck = sectCheck.nextSibling;
        }
        
        if (!hasSections) {
            var Tf = new Date().getTime();
            var Tmin = Math.floor((Tf - Ts) / 60000);
            var Tsek = Math.ceil(10 * ((Tf - Ts) / 1000 - Tmin * 60)) / 10;
            if (Tmin > 0) {
                var TimeStr = Tmin + " мин. " + Tsek + " с";
            } else {
                var TimeStr = Tsek + " с";
            }
            
            MsgBox("Унифицировать примечания (при их наличии)\n" +
                   "ver. 1.6\n" +
                   "---------------------------------------\n" +
                   "В документе есть пустой раздел для примечания,\n" +
                   "но отсутствуют ссылки-маркеры сносок.\n\n" +
                   "Скрипт завершает работу без изменений.\n" +
                   "Время работы: " + TimeStr);
            
            window.external.EndUndoUnit(document);
            return;
        }
    }
    
    // === ПРОДОЛЖЕНИЕ ОРИГИНАЛЬНОЙ ЛОГИКИ СКРИПТА ===
    
    var whileFlag, hhh, nashliBodyNotes, el, insertN, uic, i3, newSnoskaNum, tmpVar;
    
    var insertCnt = 1;
    
    el = body.firstChild;
    nashliBodyNotes = false;
    whileFlag = true;
    while (el)
        if (el.className == "body" && el.getAttribute("fbname") == strConst6) {
            nashliBodyNotes = true;
            bodyNotes = el;
            break;
        } else el = el.nextSibling;
        
    //вставляем ссылку на наше примечание
    if (addSnoska) document.selection.createRange().pasteHTML('<A href="A">Sclex_Note</A>');
    
    //если нет боди нотесов, создадим его (только если есть примечания)
    if (!nashliBodyNotes && (hasNoteLinks || totalMarkersCount > 0)) {
        el = document.createElement("DIV");
        el.className = "body";
        el.setAttribute("xlmns:l", "http://www.w3.org/1999/xlink");
        el.setAttribute("xlmns:f", "http://www.gribuser.ru/xml/fictionbook/2.0");
        el.setAttribute("fbname", strConst6);
        el.innerHTML = "<DIV class=title><P>" + strConst5 + "</P></DIV>";
        bodyNotes = body.appendChild(el);
    }
    
    //создадим заголовок боди нотесов, если нет его
    if (!forsazh && bodyNotes) {
        var bbb = bodyNotes.firstChild;
        var flag4 = true;
        while (flag4 && !(bbb.nodeName == "DIV" && (bbb.className == "section" || bbb.className == "epigraph" || bbb.className == "title")))
            if (bbb.nextSibling) bbb = bbb.nextSibling;
            else flag4 = false;
        if (flag4) {
            if (bbb.className != "title") {
                el = document.createElement("DIV");
                el.className = "title";
                el.innerHTML = "<P>" + strConst5 + "</P>";
                bbb.parentNode.insertBefore(el, bbb);
            }
        } else {
            if (bbb.className != "title") {
                el = document.createElement("DIV");
                el.className = "title";
                el.innerHTML = "<P>" + strConst5 + "</P>";
                bodyNotes.appendChild(el);
            }
        }
        bbb = undefined;
    }
    
    //прочитаем в массив SectID ID-ы разделов примечаний
    var sectsColl = new Object();
    var sectIds = new Object();
    var sectNumById = new Object();
    var sectNum = 0;
    if (!forsazh && bodyNotes) {
        var ccc = bodyNotes.firstChild;
        while (ccc != null) {
            if (ccc.nodeName == "DIV" && ccc.className == "section") {
                sectNum++;
                sectsColl[sectNum] = ccc;
                sectIds[sectNum] = ccc.id;
                sectNumById[ccc.id] = sectNum;
                
                // Проверяем, является ли ID старым форматом
                if (oldIdPattern.test(ccc.id)) {
                    unifiedIds++;
                }
            }
            ccc = ccc.nextSibling;
        }
    }
    
    // Выполняем диагностику ДО изменений
    var originalDiagnosis = null;
    if (sectNum > 0 && totalMarkers.length > 0) {
        originalDiagnosis = diagnoseSequence(sectsColl, sectNumById, totalMarkers);
        
        // Копируем данные диагностики
        diagnosticInfo.brokenSequence = originalDiagnosis.brokenSequence;
        diagnosticInfo.duplicateLinks = originalDiagnosis.duplicateLinks;
        diagnosticInfo.sectionsWithoutLinks = originalDiagnosis.sectionsWithoutLinks;
        diagnosticInfo.linksWithoutSections = originalDiagnosis.linksWithoutSections;
        diagnosticInfo.gapsInNumbering = originalDiagnosis.gapsInNumbering;
        diagnosticInfo.firstLinkNotFirstSection = originalDiagnosis.firstLinkNotFirstSection;
        diagnosticInfo.problemDetails = originalDiagnosis.problemDetails;
        diagnosticInfo.firstProblemLink = originalDiagnosis.firstProblemLink;
        diagnosticInfo.firstProblemSection = originalDiagnosis.firstProblemSection;
    }
    
    // === ИСПРАВЛЕНИЕ ТАЙМЕРА: ДЕЛАЕМ ПОДСЧЕТ ВРЕМЕНИ ПОСЛЕ ПОСЛЕДНЕГО CONFIRM ===
    
    // Запоминаем текущее время для правильного подсчета
    var timeBeforeConfirm = new Date().getTime();
    
    // Спросим пользователя, хочет ли он перейти к первой проблеме
    var goToProblem = false;
    if (diagnosticInfo.firstProblemLink || diagnosticInfo.firstProblemSection) {
        var problemCount = diagnosticInfo.duplicateLinks + diagnosticInfo.sectionsWithoutLinks + 
                          diagnosticInfo.linksWithoutSections + (diagnosticInfo.brokenSequence ? 1 : 0) +
                          diagnosticInfo.gapsInNumbering + (diagnosticInfo.firstLinkNotFirstSection ? 1 : 0);
        
        if (problemCount > 0) {
            var confirmMsg = "Обнаружено проблем: " + problemCount + "\n\n";
            
            // Добавляем краткое описание первой проблемы
            if (diagnosticInfo.problemDetails.length > 0) {
                var firstProblem = diagnosticInfo.problemDetails[0];
                confirmMsg += "Первая проблема: " + firstProblem.message + "\n\n";
            }
            
            confirmMsg += "Перейти к первой проблеме после выполнения унификации?";
            
            if (confirm(confirmMsg)) {
                goToProblem = true;
            }
        }
    }
    
    // Теперь запускаем таймер для фактического времени выполнения
    var TsActual = new Date().getTime();
    
    //определяем номер нашей сноски
    if (addSnoska) {
        j5 = 0;
        while (true)
            if (j5 < document.links.length)
                if (document.links[j5].innerHTML != "Sclex_Note") j5++;
                else break;
            else break;
        if (j5 == document.links.length) {
            MsgBox("Ошибка. Вставленная временная ссылка сноски не найдена.");
            window.external.EndUndoUnit(document);
            return;
        }
        newSnoskaNum = j5;
        if (lastSnoskaMode) {
            var it7 = newSnoskaNum + 1;
            while (it7 < document.links.length)
                if (isItNote(document.links[it7]) == false) it7++;
                else break;
            if (it7 < document.links.length) {
                document.links[newSnoskaNum].removeNode(true);
                MsgBox("Ошибка. Это получается не последняя сноска в документе!");
                window.external.EndUndoUnit(document);
                return;
            }
        }
        var j6 = newSnoskaNum - 1;
        while (j6 >= 0 && isItNote(document.links[j6]) == false) j6--;
        if (!forsazh && j6 >= 0) {
            var abc = getLocalHref(document.links[j6].href);
            if (abc != -1) insertN = sectNumById[abc];
            if (abc == -1 || insertN == undefined) {
                if (addSnoska) document.links[newSnoskaNum].removeNode(true);
                MsgBox("Не удалось создать примечание.\n\n" + "Чтобы определить, с каким разделом в body примечаний " + "связать сноску, которую пользователь хочет вставить, " + "скрипт смотрит, с каким разделом в body примечаний " + "связана определенная ранее созданная сноска. А именно – " + "скрипт смотрит на ближайшую сноску вверх по документу " + "от той сноски, которую пытается создать и вставить. " + "Но в этот раз оказалось, что эта ближайшая сверху " + "сноска не связана корректным образом с разделом в body " + "примечаний. Поэтому, чтобы вставить новую сноску, " + "исправьте, пожалуйста, сноску, которая идет перед ней.");
                window.external.EndUndoUnit(document);
                return;
            }
            insertN++;
        } else insertN = 1;
        if (insertN == 0) {
            insertN = 1;
        }
        makeNoteFromHref(document.links[newSnoskaNum]);
        if (!forsazh) {
            document.links[newSnoskaNum].innerHTML = PoShablonu(strConst4, insertN)
            document.links[newSnoskaNum].href = "#" + PoShablonu(strConst1, insertN);
        } else {
            insertN = "";
            newSnoskaId = getRandomNum(32);
            document.links[newSnoskaNum].innerHTML = PoShablonu(strConst4, "*");
            document.links[newSnoskaNum].href = "#" + PoShablonu(strConst1, newSnoskaId);
        }
    } else {
        insertN = 1000000;
        insertCnt = 0;
    }
    
    //проверим, нет ли в боди нотесов разделов второго уровня вложенности
    if (!forsazh && bodyNotes) {
        var ptr1 = bodyNotes.firstChild
        var ptr2;
        while (ptr1) {
            if (ptr1.nodeName == "DIV" && ptr1.className == "section") {
                ptr2 = ptr1.firstChild;
                while (ptr2) {
                    if (ptr2.nodeName == "DIV") {
                        if (ptr2.className == "section") {
                            MsgBox("В body примечаний есть разделы второго уровня вложенности. Такие документы не обрабатываются данным скриптом. Работа скрипта завершена.");
                            if (addSnoska) document.links[newSnoskaNum].removeNode(true);
                            window.external.EndUndoUnit(document);
                            return;
                        }
                    }
                    ptr2 = ptr2.nextSibling;
                }
            }
            ptr1 = ptr1.nextSibling;
        }
    }
    
    //введем, если надо, текст примечания
    if (InputSnoskaText) {
        var promptText = "Текст примечания. Используйте <b>...</b> <i>...</i> <br> <strong>...</strong> <emphasis>...</emphasis> <em>...</em>";
        var snoskaTextSrc = prompt(promptText, "&nbsp;");
        if (snoskaTextSrc == null) {
            window.external.EndUndoUnit(document);
            return;
        }
        snoskaText = makeGoodSnoska(snoskaTextSrc);
        var code = miniValidate(snoskaText);
        while (code != 0) {
            if (code == 1) {
                MsgBox("Ошибка. Недопустимый символ в имени тэга.");
            }
            if (code == 2) {
                MsgBox("Ошибка. Неразрешенное имя тэга.");
            }
            if (code == 3) {
                MsgBox("Ошибка. Имя закрывающего тэга не соответствует имени открывающего.");
            }
            if (code == 4) {
                MsgBox("Ошибка. Не закрыт тэг правой угловой скобкой, как уже начинается новый, либо не закрыт последний тэг в строке.");
            }
            if (code == 5) {
                MsgBox("Ошибка. Для открытого тэга нет соответствующего закрывающего.");
            }
            var snoskaTextSrc = prompt(promptText, snoskaTextSrc);
            if (snoskaTextSrc == null) {
                window.external.EndUndoUnit(document);
                return;
            }
            snoskaText = makeGoodSnoska(snoskaTextSrc);
            var code = miniValidate(snoskaText);
        }
    }
    
    //поменяем ID у разделов примечаний
    if (nashliBodyNotes && !lastSnoskaMode) {
        for (var j1 = 1; j1 <= sectNum; j1++) {
            tmpVar = PoShablonu(strConst1, j1 >= insertN ? j1 + insertCnt : j1);
            if (sectsColl[j1].id != tmpVar) {
                sectsColl[j1].id = tmpVar;
                changedIds++;
                
                // Проверяем, был ли старый формат
                if (oldIdPattern.test(sectsColl[j1].getAttribute("data-old-id") || sectsColl[j1].id)) {
                    unifiedIds++;
                }
            }
        }
    }
    
    // Отслеживаем, исправляются ли проблемы с последовательностью
    var hadSequenceProblems = diagnosticInfo.brokenSequence || 
                              diagnosticInfo.duplicateLinks > 0 || 
                              diagnosticInfo.gapsInNumbering > 0 ||
                              diagnosticInfo.firstLinkNotFirstSection;
    
    //анализируем все ссылки документа
    if (!lastSnoskaMode) {
        for (j2 = 0; j2 < document.links.length; j2++) {
            var link = document.links[j2];
            var href = link.getAttribute("href") || "";
            
            // Пропускаем внешние ссылки
            if (href.indexOf("http://") != -1 || href.indexOf("https://") != -1 ||
                href.indexOf("ftp://") != -1 || href.indexOf("mailto:") != -1) {
                continue;
            }
            
            // Пропускаем якорные ссылки без #
            if (href.indexOf("#") == -1 && link.className != "note") {
                continue;
            }
            
            if (addSnoska && j2 == newSnoskaNum) {
                uic = insertN;
            } else {
                uic = sectNumById[getLocalHref(href)];
            }
            
            if (uic != undefined && j2 != newSnoskaNum) {
                // Сохраняем оригинальные данные для сравнения
                var originalText = link.innerHTML;
                var originalHref = href;
                var originalNumberFromText = extractNumberFromText(originalText);
                var originalNumberFromHref = extractNumberFromHref(originalHref);
                
                // Определяем новый номер
                var newNumber = uic;
                if (newNumber >= insertN) newNumber++;
                
                //меняем адрес ссылки
                var newHref = "#" + PoShablonu(strConst1, newNumber);
                if (link.href != newHref && link.href != window.location.href + newHref) {
                    link.href = newHref;
                    
                    // Проверяем, изменился ли НОМЕР (а не только формат)
                    if (originalNumberFromHref !== null && originalNumberFromHref !== newNumber) {
                        actuallyRenamedNotes++;
                        // Если была проблема с последовательностью и номер изменился - считаем, что исправили
                        if (hadSequenceProblems && originalNumberFromHref !== null) {
                            diagnosticInfo.sequenceFixed = true;
                        }
                    }
                }
                
                //меняем текст ссылки
                var newText = PoShablonu(strConst4, newNumber);
                if (link.innerHTML != newText) {
                    // Определяем тип маркера для статистики
                    var markerType = getMarkerType(originalText);
                    
                    // Записываем статистику по преобразованию формата
                    if (markerType != "bracket") {
                        convertedToNewFormat = true;
                        switch (markerType) {
                            case "star":
                                convertedMarkers.starToBracket++;
                                break;
                            case "number":
                                convertedMarkers.numberToBracket++;
                                break;
                            case "paren":
                                convertedMarkers.parenToBracket++;
                                break;
                            case "superscript":
                                convertedMarkers.superscriptToBracket++;
                                break;
                            case "other":
                                convertedMarkers.otherToBracket++;
                                break;
                        }
                    }
                    
                    link.innerHTML = newText;
                    
                    // Проверяем, изменился ли НОМЕР в тексте (а не только формат)
                    if (originalNumberFromText !== null && originalNumberFromText !== newNumber) {
                        actuallyRenamedNotes++;
                        // Если была проблема с последовательностью и номер изменился - считаем, что исправили
                        if (hadSequenceProblems && originalNumberFromText !== null) {
                            diagnosticInfo.sequenceFixed = true;
                        }
                    }
                }
                
                // добавляем class=note
                if (!isItNote(link)) {
                    makeNoteFromHref(link);
                    addedClassNote++;
                    convertedToNewFormat = true;
                }
                
                // Отмечаем, что дубли устранены (если они были)
                if (diagnosticInfo.duplicateLinks > 0) {
                    diagnosticInfo.duplicatesFixed = true;
                }
            }
        }
    }
    
    // поменяем заголовки разделов примечаний
    if (nashliBodyNotes && !lastSnoskaMode) {
        for (i2 = 1; i2 <= sectNum; i2++) {
            if (sectsColl[i2].firstChild != null) {
                if (sectsColl[i2].firstChild.nodeName == "DIV" && sectsColl[i2].firstChild.className == "title") {
                    var newTitle = "<P>" + PoShablonu(strConst2, i2 >= insertN ? i2 + insertCnt : i2) + "</P>";
                    if (sectsColl[i2].firstChild.innerHTML != newTitle) {
                        sectsColl[i2].firstChild.innerHTML = newTitle;
                        changedTitles++;
                    }
                } else {
                    el = document.createElement("DIV");
                    el.className = "title";
                    el.innerHTML = "<P>" + PoShablonu(strConst2, i2 >= insertN ? i2 + insertCnt : i2) + "</P>";
                    sectsColl[i2].insertBefore(el, sectsColl[i2].firstChild);
                    changedTitles++;
                }
            } else {
                el = document.createElement("DIV");
                el.className = "title";
                el.innerHTML = "<P>" + PoShablonu(strConst2, i2 >= insertN ? i2 + insertCnt : i2) + "</P>";
                sectsColl[i2].appendChild(el);
                changedTitles++;
            }
        }
    }
    
    if (!forsazh) {
        var MyTitle = PoShablonu(strConst2, insertN);
        var MyId1 = PoShablonu(strConst1, insertN);
    } else {
        var MyTitle = "&nbsp;";
        var MyId1 = PoShablonu(strConst1, newSnoskaId);
    };
    
    //вставим новый раздел примечания
    if (addSnoska) {
        el = document.createElement("DIV");
        el.id = MyId1;
        el.className = "section";
        if (InputSnoskaText) {
            el.innerHTML = "<DIV class=title><P>" + MyTitle + "</P></DIV>" + snoskaText;
        } else {
            el.innerHTML = "<DIV class=title><P>" + MyTitle + "</P></DIV><P>" + PoShablonu(strConst3, insertN) + "</P>";
        }
        
        if (!lastSnoskaMode) {
            if (sectNum > 0) {
                if (insertN > 1) {
                    el = bodyNotes.insertBefore(el, sectsColl[insertN - 1].nextSibling);
                } else {
                    el = bodyNotes.insertBefore(el, sectsColl[1]);
                }
            } else {
                el = bodyNotes.appendChild(el);
            }
        } else {
            el = bodyNotes.appendChild(el);
        }
        if (strConst3 == "") window.external.inflateBlock(el.lastChild) = true;
    }
    
    if (MoveFocusToNote && addSnoska) {
        var el2 = el.firstChild;
        var whileFlag = true;
        while (whileFlag) {
            if (el2) {
                if (el2.nodeName == "P") {
                    GoTo(el2);
                    whileFlag = false;
                } else {
                    el2 = el2.nextSibling;
                }
            } else {
                whileFlag = false;
            }
        }
        if (el2 == null && el.firstChild != null) GoTo(el.firstChild);
    }
    
    // Переходим к проблеме, если пользователь согласился
    if (goToProblem) {
        if (diagnosticInfo.firstProblemLink) {
            GoTo(diagnosticInfo.firstProblemLink);
        } else if (diagnosticInfo.firstProblemSection) {
            GoTo(diagnosticInfo.firstProblemSection);
        }
    }
    
    // === ФОРМИРОВАНИЕ СТАТИСТИКИ ===
    var Tf = new Date().getTime();
    var Tmin = Math.floor((Tf - TsActual) / 60000);
    var Tsek = Math.ceil(10 * ((Tf - TsActual) / 1000 - Tmin * 60)) / 10;
    if (Tmin > 0) {
        var TimeStr = Tmin + " мин. " + Tsek + " с";
    } else {
        var TimeStr = Tsek + " с";
    }
    
    var msgStr = "Унифицировать примечания (при их наличии)\n" +
                 "ver. 1.6\n" +
                 "---------------------------------------\n";
    
    // Определяем, было ли выполнено перенумерование или унификация
    var totalConvertedMarkers = convertedMarkers.starToBracket + convertedMarkers.numberToBracket + 
                                convertedMarkers.parenToBracket + convertedMarkers.superscriptToBracket + 
                                convertedMarkers.otherToBracket;
    
    var totalChanges = addedClassNote + actuallyRenamedNotes + changedIds + changedTitles + totalConvertedMarkers;
    
    // Формируем диагностическую информацию
    var hasDiagnostics = diagnosticInfo.brokenSequence || 
                         diagnosticInfo.duplicateLinks > 0 || 
                         diagnosticInfo.sectionsWithoutLinks > 0 ||
                         diagnosticInfo.linksWithoutSections > 0 ||
                         diagnosticInfo.gapsInNumbering > 0 ||
                         diagnosticInfo.firstLinkNotFirstSection;
    
    if (totalChanges === 0 && !hasDiagnostics) {
        msgStr += "УНИФИКАЦИЯ И ПЕРЕНУМЕРОВАНИЕ НЕ ТРЕБУЮТСЯ\n\n";
        msgStr += "Все маркеры и разделы сносок уже соответствуют новому формату:\n";
        msgStr += "- Маркеры имеют класс 'note'\n";
        msgStr += "- ID разделов: " + strConst1.replace("%N", "N") + "\n";
        msgStr += "- Текст маркеров: " + strConst4.replace("%N", "N") + "\n";
        msgStr += "- Все сноски перенумерованы последовательно\n";
        msgStr += "- Нет нарушений в структуре примечаний\n\n";
    } else {
        // Выводим диагностическую информацию
        if (hasDiagnostics) {
            msgStr += "ДИАГНОСТИКА СТРУКТУРЫ ПРИМЕЧАНИЙ:\n";
            msgStr += "---------------------------------------\n";
            
            // Выводим детальные описания проблем
            var problemCount = 0;
            for (var p = 0; p < diagnosticInfo.problemDetails.length; p++) {
                var problem = diagnosticInfo.problemDetails[p];
                if (p < 3) { // Показываем только первые 3 проблемы, чтобы не перегружать
                    if (problem.type === "first_link_not_first") {
                        msgStr += "Ошибка в сноске " + problem.linkText + "\n";
                        msgStr += problem.message + "\n";
                    } else {
                        msgStr += (p + 1) + ". " + problem.message + "\n";
                    }
                    problemCount++;
                }
            }
            
            if (diagnosticInfo.problemDetails.length > 3) {
                msgStr += "... и ещё " + (diagnosticInfo.problemDetails.length - 3) + " проблем\n";
            }
            
            // Сводная статистика проблем
            msgStr += "\nСводная статистика проблем:\n";
            if (diagnosticInfo.firstLinkNotFirstSection) {
                msgStr += "• Первая ссылка не на первый раздел\n";
            }
            if (diagnosticInfo.duplicateLinks > 0) {
                msgStr += "• Дублирующих ссылок на разделы: " + diagnosticInfo.duplicateLinks + "\n";
            }
            if (diagnosticInfo.sectionsWithoutLinks > 0) {
                msgStr += "• Разделов без ссылок: " + diagnosticInfo.sectionsWithoutLinks + "\n";
            }
            if (diagnosticInfo.linksWithoutSections > 0) {
                msgStr += "• Ссылок на несуществующие разделы: " + diagnosticInfo.linksWithoutSections + "\n";
            }
            if (diagnosticInfo.gapsInNumbering > 0) {
                msgStr += "• Разрывов в нумерации: " + diagnosticInfo.gapsInNumbering + "\n";
            }
            if (diagnosticInfo.brokenSequence) {
                msgStr += "• Нарушена последовательность маркеров\n";
            }
            
            msgStr += "\n";
        }
        
        // Проверяем, была ли унификация
        if (convertedToNewFormat || addedClassNote > 0 || unifiedIds > 0 || totalConvertedMarkers > 0) {
            msgStr += "ВЫПОЛНЕНА УНИФИКАЦИЯ В НОВЫЙ ФОРМАТ\n\n";
            msgStr += "Статистика изменений:\n";
            msgStr += "---------------------------------------\n";
            
            if (addedClassNote > 0) {
                msgStr += "• Добавлено class=\"note\" к маркерам: " + addedClassNote + "\n";
            }
            
            if (unifiedIds > 0) {
                msgStr += "• Унифицировано ID разделов (fnX → n_X): " + unifiedIds + "\n";
            }
            
            // Детальная статистика по преобразованию формата маркеров
            if (totalConvertedMarkers > 0) {
                msgStr += "\nПреобразовано формата маркеров:\n";
                if (convertedMarkers.starToBracket > 0) {
                    msgStr += "  - из вида * в формат [N]: " + convertedMarkers.starToBracket + "\n";
                }
                if (convertedMarkers.numberToBracket > 0) {
                    msgStr += "  - из вида 1 в формат [N]: " + convertedMarkers.numberToBracket + "\n";
                }
                if (convertedMarkers.parenToBracket > 0) {
                    msgStr += "  - из вида (1) в формат [N]: " + convertedMarkers.parenToBracket + "\n";
                }
                if (convertedMarkers.superscriptToBracket > 0) {
                    msgStr += "  - из вида ¹ в формат [N]: " + convertedMarkers.superscriptToBracket + "\n";
                }
                if (convertedMarkers.otherToBracket > 0) {
                    msgStr += "  - из других форматов в [N]: " + convertedMarkers.otherToBracket + "\n";
                }
            }
            
            // Если было перенумерование в рамках унификации
            if (actuallyRenamedNotes > 0) {
                msgStr += "\n• Перенумеровано маркеров сносок: " + actuallyRenamedNotes + "\n";
            }
            
            if (changedIds > unifiedIds) { // ID которые менялись, но не были унифицированы
                msgStr += "• Изменено ID разделов: " + (changedIds - unifiedIds) + "\n";
            }
            
            if (changedTitles > 0) {
                msgStr += "• Изменено заголовков разделов: " + changedTitles + "\n";
            }
            
            // Информация об исправленных проблемах
            if (hasDiagnostics && (diagnosticInfo.sequenceFixed || diagnosticInfo.duplicatesFixed)) {
                msgStr += "\nИсправленные проблемы:\n";
                if (diagnosticInfo.sequenceFixed) {
                    msgStr += "• Восстановлена последовательность нумерации\n";
                }
                if (diagnosticInfo.duplicatesFixed && diagnosticInfo.duplicateLinks > 0) {
                    msgStr += "• Устранены дублирующие ссылки\n";
                }
            }
            
            msgStr += "\nНовый формат примечаний:\n";
            msgStr += "- Маркеры сносок имеют класс 'note'\n";
            msgStr += "- ID разделов: " + strConst1.replace("%N", "N") + "\n";
            msgStr += "- Текст маркеров: " + strConst4.replace("%N", "N") + "\n";
            msgStr += "- Все сноски пронумерованы последовательно\n\n";
        } else {
            // Только перенумерование без унификации
            msgStr += "ВЫПОЛНЕНО ПЕРЕНУМЕРОВАНИЕ\n\n";
            msgStr += "Перенумеровано маркеров сносок: " + actuallyRenamedNotes + "\n\n";
            
            if (changedIds > 0) {
                msgStr += "• Изменено ID разделов: " + changedIds + "\n";
            }
            
            if (changedTitles > 0) {
                msgStr += "• Изменено заголовков разделов: " + changedTitles + "\n";
            }
            
            msgStr += "\n";
        }
    }
    
    if (addSnoska) {
        msgStr += "Добавлен новый маркер сноски: " + PoShablonu(strConst4, insertN) + "\n\n";
    }
    
    msgStr += "Общее количество маркеров сносок в документе: " + totalMarkersCount + "\n";
    msgStr += "Из них с классом 'note': " + noteLinksCount + "\n";
    msgStr += "Количество разделов сносок (примечаний): " + sectNum + "\n\n";
    
    // Дополнительная информация о структуре (если есть проблемы)
    if (hasDiagnostics) {
        msgStr += "Рекомендации по структуре:\n";
        if (diagnosticInfo.firstLinkNotFirstSection) {
            msgStr += "- Первая ссылка должна ссылаться на первый раздел\n";
        }
        if (diagnosticInfo.duplicateLinks > 0) {
            msgStr += "- Удалите или исправьте дублирующие ссылки\n";
        }
        if (diagnosticInfo.sectionsWithoutLinks > 0) {
            msgStr += "- Удалите или заполните разделы без ссылок\n";
        }
        if (diagnosticInfo.linksWithoutSections > 0) {
            msgStr += "- Удалите или исправьте ссылки на несуществующие разделы\n";
        }
        if (diagnosticInfo.gapsInNumbering > 0) {
            msgStr += "- Проверьте нумерацию на пропуски\n";
        }
        if (diagnosticInfo.brokenSequence) {
            msgStr += "- Упорядочьте маркеры по последовательности\n";
        }
        msgStr += "\n";
    }
    
    msgStr += "Время работы скрипта: " + TimeStr;
    
    if (EndWindow) {
        MsgBox(msgStr);
    }
    
    window.external.EndUndoUnit(document);
}
