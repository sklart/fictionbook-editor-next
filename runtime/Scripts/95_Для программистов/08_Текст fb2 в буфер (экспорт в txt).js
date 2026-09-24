// Скрипт "Текст fb2 в буфер (экспорт в txt)" для редактора FBE
// version 2.6
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для копирования в буфер обмена текста fb2 документа.
// Фактически производится экспорт документа в txt фомат. 
// Настройки скрипта позволяют опционально:
// - создавать оглавление из имеющихся заголовков, 
// - копировать или пропускать разделы аннотации, хистори, сносок и комментариев,
// - обрамлять заголовки, подзаголовки и размеченные блочные элементы пустыми строками,
// - сохранять результат в txt файл в папку с исходным fb2 документом.
// Иллюстрации заменяются на их упоминание в соответствующих местах в скобках: [иллюстрация].
// Скрипт ищет путь к текущему файлу тремя способами.
// Для версий FBE начиная с 2.8.5 применяется самый быстрый и точный способ - штатно через API FBE.
// Для старых версий FBE (до 2.8.5) применяется самый медленный способ, определяющий путь,
// но не дающий точного названия текущего fb2 файла.
// Поэтому название txt файла создается из названия документа, указанного в Description.
// Настройка отображения имени и пути к txt файлу в статистике - по умолчанию выключено (режим "паранойя").
// Скрипт не вносит никаких изменений в fb2 документ.
// Режим работы: обычный или тихий.

// version 2.6, 03.08.2026
//======================================

function Run() {
    var scriptName = "Текст fb2 в буфер (экспорт в txt)";
    var version = "2.6";

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
    
    // Сохранять txt файл в папку с документом
    var saveToFile = 1; // 0 - нет, 1 - да
    
    // Создавать копию файла или перезаписывать
    var overwriteFile = 1; // 0 - перезаписывать, 1 - создавать копию с нумерацией
    
    // Показывать имя и путь к txt файлу в статистике
    var showFilePath = 0; // 0 - нет, 1 - да

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
    var fileSaved = false;
    var saveError = "";
    var savedFilePath = "";
    
    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================
    
    // Извлечение чистого текста из элемента (без тегов)
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
    
    // Убираем начальные и конечные пробелы
    function trimStr(str) {
        return str.replace(/^\s+/, "").replace(/\s+$/, "");
    }
    
    // Проверяет, заканчивается ли строка пустой строкой
    function endsWithBlankLine(txt) {
        if (txt.length == 0) return true;
        if (txt.length >= 2 && txt.substring(txt.length - 2) == "\n\n") return true;
        return false;
    }
    
    // Добавляет пустую строку в конец
    function appendBlankLine(txt) {
        if (txt.length == 0) return txt;
        while (txt.length > 1 && txt.substring(txt.length - 2) == "\n\n") {
            txt = txt.substring(0, txt.length - 1);
        }
        if (txt.substring(txt.length - 1) != "\n") txt += "\n";
        txt += "\n";
        return txt;
    }
    
    // Извлечение текста заголовка для оглавления (все P через буллет)
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
    
    // Извлечение текста заголовка для вывода в тексте (каждый P с новой строки)
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
    
    // Получение автора и названия из description
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
    
    // Получение автора и названия из заголовка body (запасной вариант)
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
    
    // Подсчёт заголовков в body (отдельно от оглавления)
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
    
    // Подсчёт заголовков внутри section (рекурсивно)
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
    
    // Формирование оглавления (рекурсивный обход body)
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
    
    // Поиск заголовков внутри section для оглавления
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
    
    // Подсчёт количества section внутри контейнера
    function countSections(container) {
        var count = 0;
        var divs = container.getElementsByTagName("DIV");
        for (var i = 0; i < divs.length; i++) {
            if (divs[i].className == "section") count++;
        }
        return count;
    }
    
    // Убираем множественные пустые строки (больше 2 подряд)
    function normalizeSpacing(txt) {
        var reMultiNl = new RegExp("\n\n\n+", "g");
        while (txt.indexOf("\n\n\n") != -1) {
            txt = txt.replace(reMultiNl, "\n\n");
        }
        return txt;
    }
    
    // Обработка дочерних элементов внутри section/body (основная функция обхода)
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
    
    // Склонение числительных
    function pad(number) {
        var n = number % 100;
        if (n >= 11 && n <= 19) return 2;
        n = number % 10;
        if (n == 1) return 0;
        if (n >= 2 && n <= 4) return 1;
        return 2;
    }
    
    // ДА/НЕТ для настроек
    function yesNo(value) {
        if (value == 1) return "\u221A (ДА)";
        return "\u2717 (НЕТ)";
    }
    
    // Очистка имени файла от запрещённых символов
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
    
    // Определение пути к текущему fb2 документу (три метода)
    function getDocumentPath() {
        var folderPath = "";
        var fileName = "";
        var fb2Path = "";
        
        // Метод 1: Штатное API FBE (начиная с FBE 2.8.5)
        try {
            fb2Path = window.external.GetDocumentFilePath();
            fileName = window.external.GetDocumentFileName();
            folderPath = window.external.GetDocumentDirectory();
            if (folderPath != "") return { folder: folderPath, file: fileName, full: fb2Path, method: "API" };
        } catch(e) {}
        
        // Метод 2: PowerShell (заголовок окна)
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
        
        // Метод 3: Временный файл + CMD-поиск (запасной)
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
    
    // Сохранение текста в txt файл (с перезаписью или созданием копии)
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
        
        var exportFileName = safeName + "_export.txt";
        var fullPath = folderPath + "\\" + exportFileName;
        
        if (createCopy == 0) {
            // Перезаписываем существующий файл
            try {
                var file = fso.CreateTextFile(fullPath, true, true);
                file.Write(textToSave);
                file.Close();
                return { success: true, path: fullPath, method: "перезаписан" };
            } catch(e) {
                return { success: false, error: "Ошибка записи файла: " + e.message, path: "" };
            }
        } else {
            // Создаём копию с нумерацией
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
            
            var counter = 1;
            var found = false;
            var newFullPath = "";
            
            while (counter <= 999) {
                var candidateName = safeName + "_export(" + counter + ").txt";
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
    // СБОРКА ТЕКСТА
    // ==================================================
    
    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) {
        MsgBox(scriptName + "\nver. " + version + "\n---------------------------------------\n\n\u2717 Ошибка: fbw_body не найден!");
        return;
    }
    
    var bodyChildren = fbwBody.childNodes;
    
    // Получаем автора и название книги
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
                if (sp) {
                    var skipTitle = (fn3 == "") ? skipBodyTitle : false;
                    resultText += processChildren(bodyChild, true, skipTitle, resultText);
                }
            }
        }
    }
    
    // Нормализация пустых строк
    resultText = normalizeSpacing(resultText);
    resultText = resultText.replace(/\n+$/, "\n");
    totalChars = resultText.length;
    
    // Всегда копируем в буфер обмена
    if (resultText.length > 0) window.clipboardData.setData("text", resultText);
    
    // Сохраняем в txt файл (если включено)
    if (saveToFile == 1 && resultText.length > 0) {
        var pathInfo = getDocumentPath();
        
        if (pathInfo.folder != "") {
            var baseFileName = "";
            if (pathInfo.file != "" && pathInfo.method != "CMD") {
                var dotPos = pathInfo.file.lastIndexOf(".fb2");
                if (dotPos == -1) dotPos = pathInfo.file.lastIndexOf(".FB2");
                if (dotPos != -1) {
                    baseFileName = pathInfo.file.substring(0, dotPos);
                } else {
                    baseFileName = pathInfo.file;
                }
            } else {
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
            msg += "\u221A Текст скопирован в буфер обмена\n";
            if (saveToFile == 1 && fileSaved) {
                msg += "\u221A Текст сохранён в txt файл\n";
            }
        } else {
            msg += "\u2717 Ничего не скопировано!\n";
        }
        
        if (saveToFile == 1 && saveError != "") {
            msg += "\u2717 Ошибка сохранения файла: " + saveError + "\n";
        }
        
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
        msg += "  \u2022 Сохранение в txt файл: " + yesNo(saveToFile) + "\n";
        if (saveToFile == 1) {
            msg += "  \u2022 Режим сохранения: " + (overwriteFile == 0 ? "перезаписывать" : "создавать копии") + "\n";
            msg += "  \u2022 Показывать путь к файлу: " + yesNo(showFilePath) + "\n";
        }
        msg += "---------------------------------------\n\n";
        
        if (displayAuthor != "" || displayTitle != "") {
            if (displayAuthor != "") msg += "Автор: " + displayAuthor + "\n";
            if (displayTitle != "") msg += "Название: " + displayTitle + "\n";
            msg += "\n";
        }
        
        msg += "Всего скопировано:\n\n";
        msg += "\u221A Заголовков: " + totalTitles + "\n";
        msg += "\u221A Абзацев (включая заголовки): " + (totalParagraphs + totalTitles) + "\n";
        msg += "\u221A Символов: " + totalChars + "\n";
        
        if (processNotesSection) {
            if (notesSections > 0) {
                msg += "\u221A Примечаний: " + notesSections + "\n";
            } else {
                msg += "\u221A Примечаний: отсутствуют\n";
            }
        } else {
            msg += "\u221A Примечаний: отключены\n";
        }
        
        if (processCommentsSection) {
            if (commentsSections > 0) {
                msg += "\u221A Комментариев: " + commentsSections + "\n";
            } else {
                msg += "\u221A Комментариев: отсутствуют\n";
            }
        } else {
            msg += "\u221A Комментариев: отключены\n";
        }
        
        msg += "\n---------------------------------------\n";
        
        if (saveToFile == 1 && fileSaved && savedFilePath != "" && showFilePath == 1) {
            msg += "Файл сохранён: " + savedFilePath + "\n";
            msg += "\n---------------------------------------\n";
        }
        
        msg += "Время выполнения: " + elapsedFormatted + " сек.";
        
        MsgBox(msg);
    } else {
        if (resultText.length == 0) {
            MsgBox(scriptName + "\nver. " + version + "\n---------------------------------------\n\n\u2717 Ошибка: ничего не скопировано! Проверьте настройки скрипта.");
        }
        
        if (saveToFile == 1 && saveError != "") {
            MsgBox(scriptName + "\nver. " + version + "\n---------------------------------------\n\n\u2717 Ошибка сохранения файла: " + saveError);
        }
    }
}