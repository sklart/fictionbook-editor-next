// Скрипт "Оглавление fb2 в буфер" для редактора FBE
// version 1.7
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для копирования в буфер обмена оглавления текущего fb2 документа.
// Оглавление строится на основе заголовков внутри разделов (section).
// Для разделов сносок (примечаний) и комментариев передается только общее кол-во элементов в них.
// Опционально может быть передано в буфер:
// автор и название книги, разделы аннотации (annotation) и истории (history).
// Также опционально можно сохранить оглавление в txt файл в папку с документом.
// Скрипт ищет путь к текущему файлу тремя способами.
// Для версий FBE начиная с 2.8.5 применяется самый быстрый и точный способ через API FBE.
// Для старых версий FBE (до 2.8.5) применяется самый медленный способ, определяющий путь,
// но не дающий точного названия текущего fb2 файла.
// Поэтому название txt файла оглавления создается из названия документа, указанного в Description.
// Настройка отображения имени и пути к txt файлу в статистике - по умолчанию выключено (режим "паранойя").
// Скрипт не вносит никаких изменений в fb2 документ.
// Режим работы: обычный или тихий.

// version 1.7, 03.08.2026
//======================================

function Run() {
    var scriptName = "Оглавление fb2 в буфер";
    var version = "1.7";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима
    
    // Копировать название книги (автор и заголовок)
    var copyBookTitle = 1; // 0 - нет, 1 - да
    
    // Копировать раздел аннотации (annotation)
    var processAnnotationSection = 0; // 0 - нет, 1 - да
    
    // Копировать раздел истории (History)
    var processHistorySection = 0; // 0 - нет, 1 - да
    
    // Сохранять txt файл оглавления в папку с документом
    var saveToFile = 1; // 0 - нет, 1 - да
    
    // Создавать копию файла оглавления или перезаписывать
    var overwriteFile = 1; // 0 - перезаписывать, 1 - создавать копию с нумерацией
    
    // Показывать имя и путь к txt файлу в статистике
    var showFilePath = 0; // 0 - нет, 1 - да

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    var startTime = new Date();
    var totalTitles = 0;
    var notesSections = 0;
    var commentsSections = 0;
    var tocText = "";
    var fileSaved = false;
    var saveError = "";
    var savedFilePath = "";
    
    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================
    
    function trimStr(str) {
        return str.replace(/^\s+/, "").replace(/\s+$/, "");
    }
    
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
    
    function processAnnotationOrHistory(container) {
        var txt = "";
        var children = container.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 1 && child.nodeName == "DIV") {
                var className = child.className;
                if (className == "title") {
                    var titleStr = extractTitleText(child);
                    if (titleStr.length > 0) txt += titleStr + "\n";
                } else if (className == "section") {
                    txt += processAnnotationOrHistory(child);
                } else {
                    var innerText = processAnnotationOrHistory(child);
                    if (innerText.length > 0) txt += innerText;
                }
            } else if (child.nodeType == 1 && child.nodeName == "P") {
                var pText = trimStr(getCleanText(child));
                if (pText.length > 0) txt += pText + "\n";
            }
        }
        return txt;
    }
    
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
    
    function countSections(container) {
        var count = 0;
        var divs = container.getElementsByTagName("DIV");
        for (var i = 0; i < divs.length; i++) {
            if (divs[i].className == "section") count++;
        }
        return count;
    }
    
    function pad(number) {
        var n = number % 100;
        if (n >= 11 && n <= 19) return 2;
        n = number % 10;
        if (n == 1) return 0;
        if (n >= 2 && n <= 4) return 1;
        return 2;
    }
    
    function sanitizeFileName(name) {
        var result = "";
        var forbidden = "\\/:*?\"<>|";
        for (var i = 0; i < name.length; i++) {
            var ch = name.charAt(i);
            var isForbidden = false;
            for (var j = 0; j < forbidden.length; j++) {
                if (ch == forbidden.charAt(j)) { isForbidden = true; break; }
            }
            if (!isForbidden) result += ch;
        }
        return trimStr(result);
    }
    
    function getDocumentPath() {
        var folderPath = "";
        var fileName = "";
        var fb2Path = "";
        
        // Метод 1: Штатное API FBE
        try {
            fb2Path = window.external.GetDocumentFilePath();
            fileName = window.external.GetDocumentFileName();
            folderPath = window.external.GetDocumentDirectory();
            if (folderPath != "") return { folder: folderPath, file: fileName, full: fb2Path, method: "API" };
        } catch(e) {}
        
        // Метод 2: PowerShell
        try {
            var shell = new ActiveXObject("WScript.Shell");
            var fso = new ActiveXObject("Scripting.FileSystemObject");
            var tempFile = "C:\\__fbe_path_result.txt";
            
            var psCmd = "powershell -ExecutionPolicy Bypass -command \"(Get-Process -Name 'fbe' -ErrorAction SilentlyContinue).MainWindowTitle\" > \"" + tempFile + "\" 2>&1";
            shell.Run(psCmd, 0, true);
            
            if (fso.FileExists(tempFile)) {
                var file = fso.OpenTextFile(tempFile, 1, false, -1);
                var title = "";
                if (!file.AtEndOfStream) {
                    title = file.ReadLine();
                    title = title.replace(/^\s+|\s+$/g, "");
                }
                file.Close();
                try { fso.DeleteFile(tempFile); } catch(e) {}
                
                if (title.length > 0) {
                    var pathMatch = title.match(/[A-Za-z]:\\.+?\.fb2/i);
                    if (pathMatch) {
                        fb2Path = pathMatch[0];
                        var lastSlash = fb2Path.lastIndexOf("\\");
                        if (lastSlash != -1) {
                            folderPath = fb2Path.substring(0, lastSlash);
                            fileName = fb2Path.substring(lastSlash + 1);
                            return { folder: folderPath, file: fileName, full: fb2Path, method: "PowerShell" };
                        }
                    }
                }
            }
        } catch(e) {}
        
        // Метод 3: Временный файл + CMD
        try {
            var shell2 = new ActiveXObject("WScript.Shell");
            var fso2 = new ActiveXObject("Scripting.FileSystemObject");
            var tempFile2 = "C:\\__fbe_path_result.txt";
            
            var binObjects = document.all.binobj.getElementsByTagName("DIV");
            var binData = "";
            if (binObjects.length > 0) {
                binData = binObjects[0].base64data;
            } else {
                binData = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPj/HwADBwIAMCbHYQAAAABJRU5ErkJggg==";
            }
            
            var testId = "__fbe_path_test_" + new Date().getTime();
            window.external.SaveBinary(testId, binData, 0);
            
            var drivesToCheck = [];
            try {
                var allDrives = fso2.Drives;
                for (var dc = new Enumerator(allDrives); !dc.atEnd(); dc.moveNext()) {
                    var drv = dc.item();
                    if (drv.IsReady) drivesToCheck.push(drv.DriveLetter + ":");
                }
            } catch(e) {
                drivesToCheck = ["C:"];
            }
            
            var foundPath = "";
            for (var d = 0; d < drivesToCheck.length; d++) {
                var disk = drivesToCheck[d];
                var cmd = "cmd /u /c dir /s /b \"" + disk + "\\" + testId + "*\" > \"" + tempFile2 + "\" 2>nul";
                shell2.Run(cmd, 0, true);
                
                if (fso2.FileExists(tempFile2)) {
                    var resultFile = fso2.OpenTextFile(tempFile2, 1, false, -1);
                    var line = "";
                    if (!resultFile.AtEndOfStream) {
                        line = resultFile.ReadLine();
                        line = line.replace(/^\s+|\s+$/g, "");
                        if (line.length > 0 && line.indexOf(testId) != -1) foundPath = line;
                    }
                    resultFile.Close();
                    try { fso2.DeleteFile(tempFile2); } catch(e) {}
                    if (foundPath != "") break;
                }
            }
            
            if (foundPath != "") {
                try { fso2.DeleteFile(foundPath); } catch(e) {}
                var lastSlash2 = foundPath.lastIndexOf("\\");
                if (lastSlash2 != -1) {
                    folderPath = foundPath.substring(0, lastSlash2);
                    fileName = "(неизвестно)";
                    return { folder: folderPath, file: fileName, full: foundPath, method: "CMD" };
                }
            }
        } catch(e) {}
        
        return { folder: "", file: "", full: "", method: "не найден" };
    }
    
    function saveTextToFile(folderPath, baseFileName, textToSave, createCopy) {
        var fso;
        try {
            fso = new ActiveXObject("Scripting.FileSystemObject");
        } catch(e) {
            return { success: false, error: "Не удалось создать объект FileSystemObject: " + e.message, path: "" };
        }
        
        if (!fso.FolderExists(folderPath)) {
            return { success: false, error: "Папка не существует: " + folderPath, path: "" };
        }
        
        var safeName = sanitizeFileName(baseFileName);
        if (safeName == "") safeName = "документ";
        
        var tocFileName = safeName + "_Оглавление.txt";
        var fullPath = folderPath + "\\" + tocFileName;
        
        if (createCopy == 0) {
            // ПЕРЕЗАПИСЫВАЕМ (всегда пишем в один и тот же файл)
            try {
                var file = fso.CreateTextFile(fullPath, true, true);
                file.Write(textToSave);
                file.Close();
                return { success: true, path: fullPath, method: "перезаписан" };
            } catch(e) {
                return { success: false, error: "Ошибка записи файла: " + e.message, path: "" };
            }
        } else {
            // СОЗДАЁМ КОПИЮ с нумерацией
            // Если базовый файл (без номера) НЕ существует — пишем в него
            if (!fso.FileExists(fullPath)) {
                try {
                    var fileNew = fso.CreateTextFile(fullPath, true, true);
                    fileNew.Write(textToSave);
                    fileNew.Close();
                    return { success: true, path: fullPath, method: "сохранён (новый)" };
                } catch(e) {
                    return { success: false, error: "Ошибка записи файла: " + e.message, path: "" };
                }
            }
            
            // Базовый файл уже есть — ищем свободный номер
            var counter = 1;
            var found = false;
            var newFullPath = "";
            
            while (counter <= 999) {
                var candidateName = safeName + "_Оглавление(" + counter + ").txt";
                var candidatePath = folderPath + "\\" + candidateName;
                if (!fso.FileExists(candidatePath)) {
                    newFullPath = candidatePath;
                    found = true;
                    break;
                }
                counter++;
            }
            
            if (!found) {
                return { success: false, error: "Не удалось создать копию: превышен лимит (999)", path: "" };
            }
            
            try {
                var fileCopy = fso.CreateTextFile(newFullPath, true, true);
                fileCopy.Write(textToSave);
                fileCopy.Close();
                return { success: true, path: newFullPath, method: "сохранён (копия)" };
            } catch(e) {
                return { success: false, error: "Ошибка записи файла: " + e.message, path: "" };
            }
        }
    }
    
    // ==================================================
    // СБОРКА ОГЛАВЛЕНИЯ
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
    
    var skipBodyTitle = allMatch;
    
    // Подсчитываем заголовки в основном разделе
    for (var i = 0; i < bodyChildren.length; i++) {
        var child = bodyChildren[i];
        if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "body") {
            var fbname = child.getAttribute("fbname") || "";
            if (fbname == "") {
                countTitles(child, skipBodyTitle);
            } else if (fbname == "notes") {
                notesSections = countSections(child);
            } else if (fbname == "comments") {
                commentsSections = countSections(child);
            }
        }
    }
    
    // Собираем оглавление из основного раздела
    for (var j = 0; j < bodyChildren.length; j++) {
        var child2 = bodyChildren[j];
        if (child2.nodeType == 1 && child2.nodeName == "DIV" && child2.className == "body") {
            if ((child2.getAttribute("fbname") || "") == "") {
                buildTOC(child2, "", skipBodyTitle);
            }
        }
    }
    
    // Формируем итоговый текст для буфера
    var resultText = "";
    
    // Название книги (если включено)
    if (copyBookTitle) {
        if (displayAuthor != "") resultText += displayAuthor + "\n";
        if (displayTitle != "") resultText += displayTitle + "\n";
        if (displayAuthor != "" || displayTitle != "") resultText += "\n";
    }
    
    // Оглавление
    if (tocText.length > 0) {
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
    
    // Аннотация
    if (processAnnotationSection) {
        for (var k = 0; k < bodyChildren.length; k++) {
            var bc = bodyChildren[k];
            if (bc.nodeType == 1 && bc.nodeName == "DIV" && bc.className == "annotation") {
                var annText = processAnnotationOrHistory(bc);
                if (annText.length > 0) {
                    resultText += annText + "\n";
                }
            }
        }
    }
    
    // History
    if (processHistorySection) {
        for (var m = 0; m < bodyChildren.length; m++) {
            var bc2 = bodyChildren[m];
            if (bc2.nodeType == 1 && bc2.nodeName == "DIV" && bc2.className == "history") {
                var histText = processAnnotationOrHistory(bc2);
                if (histText.length > 0) {
                    resultText += histText + "\n";
                }
            }
        }
    }
    
    resultText = resultText.replace(/\n+$/, "\n");
    
    // Всегда копируем в буфер
    if (resultText.length > 0) window.clipboardData.setData("text", resultText);
    
    // Сохраняем в файл (если включено)
    if (saveToFile == 1 && resultText.length > 0) {
        var pathInfo = getDocumentPath();
        
        if (pathInfo.folder != "") {
            var baseFileName = "";
            if (pathInfo.file != "" && pathInfo.method != "CMD") {
                // Убираем расширение .fb2
                var dotPos = pathInfo.file.lastIndexOf(".fb2");
                if (dotPos == -1) dotPos = pathInfo.file.lastIndexOf(".FB2");
                if (dotPos != -1) {
                    baseFileName = pathInfo.file.substring(0, dotPos);
                } else {
                    baseFileName = pathInfo.file;
                }
            } else {
                // Если имя файла неизвестно, пробуем взять название книги
                baseFileName = displayTitle;
                if (baseFileName == "") baseFileName = "документ";
            }
            
            var saveResult = saveTextToFile(pathInfo.folder, baseFileName, resultText, overwriteFile);
            fileSaved = saveResult.success;
            if (saveResult.success) {
                savedFilePath = saveResult.path;
            } else {
                saveError = saveResult.error;
            }
        } else {
            saveError = "Не удалось определить путь к документу.\nВозможно, это новый (ещё не сохранённый) документ.";
        }
    }
    
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
        
        if (resultText.length > 0) {
            msg += "\u221A Оглавление скопировано в буфер обмена\n";
            if (saveToFile == 1 && fileSaved) {
                msg += "\u221A Оглавление сохранено в txt файл\n";
            }
        } else {
            if (totalTitles == 0) {
                msg += "Заголовков секций в документе нет.\n";
            }
            msg += "\u2717 Ничего не скопировано!\n";
        }
        
        if (saveToFile == 1 && saveError != "") {
            msg += "\u2717 Ошибка сохранения файла: " + saveError + "\n";
        }
        
        msg += "---------------------------------------\n";
        msg += "Настройки копирования:\n";
        msg += "  \u2022 Название книги: " + (copyBookTitle == 1 ? "\u221A (ДА)" : "\u2717 (НЕТ)") + "\n";
        msg += "  \u2022 Раздел аннотации: " + (processAnnotationSection == 1 ? "\u221A (ДА)" : "\u2717 (НЕТ)") + "\n";
        msg += "  \u2022 Раздел History: " + (processHistorySection == 1 ? "\u221A (ДА)" : "\u2717 (НЕТ)") + "\n";
        msg += "  \u2022 Сохранение в txt файл: " + (saveToFile == 1 ? "\u221A (ДА)" : "\u2717 (НЕТ)") + "\n";
        if (saveToFile == 1) {
            msg += "  \u2022 Режим сохранения: " + (overwriteFile == 0 ? "перезаписывать" : "создавать копии") + "\n";
            msg += "  \u2022 Показывать путь к файлу: " + (showFilePath == 1 ? "\u221A (ДА)" : "\u2717 (НЕТ)") + "\n";
        }
        msg += "---------------------------------------\n\n";
        
        if (displayAuthor != "" || displayTitle != "") {
            if (displayAuthor != "") msg += "Автор: " + displayAuthor + "\n";
            if (displayTitle != "") msg += "Название: " + displayTitle + "\n";
            msg += "\n";
        }
        
        msg += "\u221A Всего заголовков в оглавлении: " + totalTitles + "\n";
        
        if (notesSections > 0) msg += "\u221A Примечаний: " + notesSections + "\n";
        if (commentsSections > 0) msg += "\u221A Комментариев: " + commentsSections + "\n";
        
        // Путь к сохранённому файлу (только если включено в настройках)
        if (saveToFile == 1 && fileSaved && savedFilePath != "" && showFilePath == 1) {
            msg += "\n---------------------------------------\n";
            msg += "Файл сохранён: " + savedFilePath + "\n";
        }
        
        msg += "\n---------------------------------------\n";
        msg += "Время выполнения: " + elapsedFormatted + " сек.";
        
        MsgBox(msg);
    } else {
        // Тихий режим
        if (resultText.length == 0) {
            var errMsg = scriptName + "\nver. " + version + "\n---------------------------------------\n\n";
            if (totalTitles == 0) {
                errMsg += "Заголовков секций в документе нет.\n";
            }
            errMsg += "\u2717 Ничего не скопировано! Проверьте настройки скрипта.";
            MsgBox(errMsg);
        }
        
        if (saveToFile == 1 && saveError != "") {
            MsgBox(scriptName + "\nver. " + version + "\n---------------------------------------\n\n\u2717 Ошибка сохранения файла: " + saveError);
        }
    }
}