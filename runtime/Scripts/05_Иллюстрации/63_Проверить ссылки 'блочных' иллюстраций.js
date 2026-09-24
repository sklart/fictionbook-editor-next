// Скрипт "Проверить ссылки 'блочных' иллюстраций" для редактора FBE
// version 1.3
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для проверки всех блочных иллюстраций (<DIV class="image">) в fb2 документе
// Выявляет проблемные ссылки на картинки.
//
// Категории иллюстраций:
//   Обычные      - ссылка корректна, бинарник найден, href и src совпадают.
//   Пустые       - заготовки-пустышки без ссылки (href и src = #undefined).
//                  Это штатная ситуация, ошибкой не считается.
//   Битые        - ссылка есть, но картинка не может быть показана:
//                    • без бинарника       - файла с таким именем нет в документе;
//                    • без расширения       - в имени отсутствует .jpg/.png и т.п.;
//                    • неподдерживаемое     - расширение не из списка FB2 (bmp/tif/gif);
//                    • потерянная ссылка    - href пуст, но src на что-то указывает.
//   Ошибочные    - href и src ссылаются на разные файлы (несовпадение имён).
//
// Отдельно выводится разбивка по разделам: основной, примечания, комментарии.
// Если проблем нет - сообщается "Иллюстрации в порядке".

// Скрипт не вносит никаких изменений в fb2 документ.

// version 1.3, 21.09.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Проверить ссылки 'блочных' иллюстраций";
    var version = "1.3";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Обрабатывать раздел сносок (примечаний) - 0 нет, 1 да
    var processNotesSection = 1;
    
    // Обрабатывать раздел комментариев - 0 нет, 1 да
    var processCommentsSection = 1;
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    // Получаем символ неразрывного пробела из настроек FBE
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
    
    var startTime = new Date();
    
    // ==================================================
    // СБОР СПИСКА БИНАРНИКОВ
    // ==================================================
    
    var binaryList = {};
    var totalBinaries = 0;
    
    try {
        var binobj = document.all.binobj;
        if (binobj) {
            var objects = binobj.getElementsByTagName("DIV");
            for (var i = 0; i < objects.length; i++) {
                var bin = objects[i];
                var id = bin.all.id.value || "";
                if (id) {
                    binaryList[id] = true;
                    totalBinaries++;
                }
            }
        }
    } catch(e) {}
    
    // ==================================================
    // ОПРЕДЕЛЕНИЕ РАЗДЕЛА ЭЛЕМЕНТА
    // ==================================================
    
    function getSectionType(element) {
        var parent = element.parentNode;
        while (parent) {
            if (parent.nodeName == "DIV" && parent.className == "body") {
                var fbname = parent.getAttribute("fbname") || "";
                if (fbname == "") return "main";
                if (fbname == "notes") return "notes";
                if (fbname == "comments") return "comments";
                return "other";
            }
            parent = parent.parentNode;
        }
        return "main";
    }
    
    // ==================================================
    // ИЗВЛЕЧЕНИЕ ИМЕНИ ФАЙЛА ИЗ СТРОКИ
    // ==================================================
    
    function extractHrefName(href) {
        if (!href) return "";
        if (href.charAt(0) == "#") return href.substring(1);
        return href;
    }
    
    function extractSrcName(src) {
        if (!src) return "";
        var prefix1 = "fbw-internal:#";
        var prefix2 = "fbw-internal:";
        if (src.length >= prefix1.length && src.substring(0, prefix1.length) == prefix1) {
            return src.substring(prefix1.length);
        }
        if (src.length >= prefix2.length && src.substring(0, prefix2.length) == prefix2) {
            return src.substring(prefix2.length);
        }
        return src;
    }
    
    // ==================================================
    // ПРОВЕРКА РАСШИРЕНИЯ ИМЕНИ ФАЙЛА
    // ==================================================
    
    function getExtension(fileName) {
        if (!fileName) return "";
        var lastDot = fileName.lastIndexOf(".");
        if (lastDot == -1) return "";
        return fileName.substring(lastDot + 1).toLowerCase();
    }
    
    function isSupportedExtension(ext) {
        return (ext == "jpg" || ext == "jpeg" || ext == "png");
    }
    
    // ==================================================
    // ПОИСК ВСЕХ БЛОЧНЫХ ИЛЛЮСТРАЦИЙ И ИХ КЛАССИФИКАЦИЯ
    // ==================================================
    
    function makeSectionStats() {
        return {
            normal: 0,
            empty: 0,
            brokenNoHref: 0,
            brokenNoBinary: 0,
            brokenNoExt: 0,
            brokenBadExt: 0,
            wrong: 0,
            listNoHref: [],
            listNoBinary: [],
            listNoExt: [],
            listBadExt: [],
            listWrong: []
        };
    }
    
    var statsMain = makeSectionStats();
    var statsNotes = makeSectionStats();
    var statsComments = makeSectionStats();
    
    var totalNormal = 0;
    var totalEmpty = 0;
    var totalNoHref = 0;
    var totalNoBinary = 0;
    var totalNoExt = 0;
    var totalBadExt = 0;
    var totalWrong = 0;
    var totalBlockImages = 0;
    
    var allListNoHref = [];
    var allListNoBinary = [];
    var allListNoExt = [];
    var allListBadExt = [];
    var allListWrong = [];
    
    try {
        var allDivs = document.getElementsByTagName("DIV");
        
        for (var i = 0; i < allDivs.length; i++) {
            var div = allDivs[i];
            
            if (div.className != "image") continue;
            
            var sectionType = getSectionType(div);
            if (sectionType == "other") continue;
            if (sectionType == "notes" && !processNotesSection) continue;
            if (sectionType == "comments" && !processCommentsSection) continue;
            
            var targetStats;
            if (sectionType == "main") targetStats = statsMain;
            else if (sectionType == "notes") targetStats = statsNotes;
            else if (sectionType == "comments") targetStats = statsComments;
            else continue;
            
            totalBlockImages++;
            
            var href = div.getAttribute("href") || "";
            var img = null;
            var children = div.childNodes;
            for (var j = 0; j < children.length; j++) {
                if (children[j].nodeName == "IMG") {
                    img = children[j];
                    break;
                }
            }
            var src = img ? (img.getAttribute("src") || "") : "";
            
            var hrefHasUndef = (href.indexOf("undefined") != -1);
            var srcHasUndef = (src.indexOf("undefined") != -1);
            
            if (hrefHasUndef && srcHasUndef) {
                targetStats.empty++;
                totalEmpty++;
                continue;
            }
            
            var hrefName = extractHrefName(href);
            var srcName = extractSrcName(src);
            
            // 1. Потерянная ссылка
            if (!hrefName || hrefName == "") {
                var display = "[нет href] --> " + (srcName || "?");
                targetStats.brokenNoHref++;
                totalNoHref++;
                targetStats.listNoHref.push(display);
                allListNoHref.push(display);
                continue;
            }
            
            var ext = getExtension(hrefName);
            
            // 2. Без расширения
            if (ext == "") {
                targetStats.brokenNoExt++;
                totalNoExt++;
                targetStats.listNoExt.push(hrefName);
                allListNoExt.push(hrefName);
                continue;
            }
            
            var binaryExists = (binaryList[hrefName] == true);
            
            // 3. Неподдерживаемое расширение
            if (!isSupportedExtension(ext)) {
                if (binaryExists) {
                    targetStats.brokenBadExt++;
                    totalBadExt++;
                    targetStats.listBadExt.push(hrefName);
                    allListBadExt.push(hrefName);
                } else {
                    targetStats.brokenNoBinary++;
                    totalNoBinary++;
                    targetStats.listNoBinary.push(hrefName);
                    allListNoBinary.push(hrefName);
                }
                continue;
            }
            
            // 4. Поддерживаемое расширение, но бинарника нет
            if (!binaryExists) {
                targetStats.brokenNoBinary++;
                totalNoBinary++;
                targetStats.listNoBinary.push(hrefName);
                allListNoBinary.push(hrefName);
                continue;
            }
            
            // 5. Ошибочная
            if (srcName != hrefName) {
                var display2 = hrefName + " --> " + (srcName || "?");
                targetStats.wrong++;
                totalWrong++;
                targetStats.listWrong.push(display2);
                allListWrong.push(display2);
                continue;
            }
            
            // 6. Обычная
            targetStats.normal++;
            totalNormal++;
        }
    } catch(e) {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "Ошибка при анализе документа: " + e.message);
        return;
    }
    
    var totalBroken = totalNoHref + totalNoBinary + totalNoExt + totalBadExt;
    var hasErrors = (totalBroken > 0 || totalWrong > 0);
    
    var endTime = new Date();
    var timeDiff = (endTime - startTime) / 1000;
    var timeStr = timeDiff.toFixed(3).replace(".", ",");
    
    // ==================================================
    // ФОРМИРОВАНИЕ СООБЩЕНИЯ
    // ==================================================
    
    var msg = "";
    
    // Случай: блочных иллюстраций нет вообще
    if (totalBlockImages == 0) {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "Блочных иллюстраций в документе нет.\n\n" +
               "Время выполнения: " + timeStr + " сек.");
        try { window.external.SetStatusBarText("ОК"); } catch(e) {}
        return;
    }
    
    // Случай: бинарников нет вообще, но есть битые
    if (totalBinaries == 0 && totalBroken > 0) {
        msg += "Обычных блочных иллюстраций: " + totalNormal + "\n";
        msg += "Пустых блочных иллюстраций: " + totalEmpty + "\n";
        msg += "\nБитых блочных иллюстраций всего: " + totalBroken + "\n";
        msg += "Бинарные файлы иллюстраций в документе отсутствуют.\n";
        msg += "\nВремя выполнения: " + timeStr + " сек.";
        MsgBox(scriptName + "\nver. " + version + "\n\n" + msg);
        try { window.external.SetStatusBarText("ОК"); } catch(e) {}
        return;
    }
    
    // Обычный случай
    msg += "Обычных блочных иллюстраций: " + totalNormal + "\n";
    msg += "Пустых блочных иллюстраций: " + totalEmpty + "\n";
    
    // Блок битых/ошибочных - только если есть хоть одна ошибка
    if (hasErrors) {
        msg += "\nБитых блочных иллюстраций всего: " + totalBroken + "\n";
        
        if (totalNoHref > 0) {
            msg += "  – потерянная ссылка (" + totalNoHref + "): " + 
                   allListNoHref.join(", ") + "\n";
        }
        if (totalNoBinary > 0) {
            msg += "  – без бинарника (" + totalNoBinary + "): " + 
                   allListNoBinary.join(", ") + "\n";
        }
        if (totalNoExt > 0) {
            msg += "  – без расширения (" + totalNoExt + "): " + 
                   allListNoExt.join(", ") + "\n";
        }
        if (totalBadExt > 0) {
            msg += "  – неподдерживаемое расширение (" + totalBadExt + "): " + 
                   allListBadExt.join(", ") + "\n";
        }
        
        msg += "\nОшибочных блочных иллюстраций: " + totalWrong + "\n";
        if (totalWrong > 0) {
            msg += "  – " + allListWrong.join(", ") + "\n";
        }
    }
    
    // Разбивка по разделам - всегда
    var hasSectionsInfo = false;
    var sectionsMsg = "";
    
    function sectionTotal(s) {
        return s.normal + s.empty + s.brokenNoHref + s.brokenNoBinary + 
               s.brokenNoExt + s.brokenBadExt + s.wrong;
    }
    
    function sectionBroken(s) {
        return s.brokenNoHref + s.brokenNoBinary + s.brokenNoExt + s.brokenBadExt;
    }
    
    if (sectionTotal(statsMain) > 0) {
        sectionsMsg += "  • Основной раздел: обычных " + statsMain.normal + 
                       ", пустых " + statsMain.empty;
        if (hasErrors) {
            sectionsMsg += ", битых " + sectionBroken(statsMain) + 
                           ", ошибочных " + statsMain.wrong;
        }
        sectionsMsg += "\n";
        hasSectionsInfo = true;
    }
    
    if (processNotesSection && sectionTotal(statsNotes) > 0) {
        sectionsMsg += "  • Примечания: обычных " + statsNotes.normal + 
                       ", пустых " + statsNotes.empty;
        if (hasErrors) {
            sectionsMsg += ", битых " + sectionBroken(statsNotes) + 
                           ", ошибочных " + statsNotes.wrong;
        }
        sectionsMsg += "\n";
        hasSectionsInfo = true;
    }
    
    if (processCommentsSection && sectionTotal(statsComments) > 0) {
        sectionsMsg += "  • Комментарии: обычных " + statsComments.normal + 
                       ", пустых " + statsComments.empty;
        if (hasErrors) {
            sectionsMsg += ", битых " + sectionBroken(statsComments) + 
                           ", ошибочных " + statsComments.wrong;
        }
        sectionsMsg += "\n";
        hasSectionsInfo = true;
    }
    
    if (hasSectionsInfo) {
        msg += "\nПо разделам:\n" + sectionsMsg;
    }
    
    // Финальное заключение - только если ошибок нет
    if (!hasErrors) {
        msg += "\nИллюстрации в порядке.\n";
    }
    
    msg += "\nВремя выполнения: " + timeStr + " сек.";
    
    MsgBox(scriptName + "\nver. " + version + "\n\n" + msg);
    
    try { window.external.SetStatusBarText("ОК"); } catch(e) {}
    
    return;
}