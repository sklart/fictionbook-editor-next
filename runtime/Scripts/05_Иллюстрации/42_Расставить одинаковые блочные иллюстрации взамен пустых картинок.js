// Скрипт "Расставить одинаковые блочные иллюстрации взамен пустых картинок"
// version 1.7
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для вставки заданной иллюстрации на место пустых картинок
// (#undefined) в fb2 документах.
// При наличии выделения, скрипт заменяет картинки-пустышки только в выделенных секциях,
// в противном случае - обрабатывается сразу весь документ.
// В документе предварительно должны быть расставлены пустые картинки
// и прикреплена одна иллюстрация с именем файла, например, 1234567 (без указания расширения).
// Скрипт находит прикреплённый файл по имени, игнорируя префиксы _, unused_, unused__.
// Поддерживаются допустимые типы файлов jpg, jpeg, png.
// Имя файла можно задавать любое - цифрами или буквами с любым регистром.
// По умолчанию обрабатывается только основной раздел документа, без сносок и комментариев.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.7, 24.07.2026

// v. 1.7: Добавлена обработка выделенных секций:
// При наличии выделения, скрипт заменяет картинки-пустышки только в выделенных секциях,
// в противном случае - обрабатывается сразу весь документ.
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Расставить одинаковые блочные иллюстрации взамен пустых картинок";
    var version = "1.7";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Пользовательское имя вставляемой иллюстрации (без расширения):
    var UserImageName = "1234567";
    
    // Примечание: после прикрепления файла к документу, FBE может автоматически
    // добавлять к имени файла префиксы:
    //   "_"         - для ещё не расставленных картинок
    //   "unused_"  или "unused__" - после унификации иллюстраций
    // Скрипт автоматически найдёт файл с любым из этих префиксов.
    // Пример: для имени "1234567" будут найдены файлы:
    //   1234567.png, _1234567.png, unused_1234567.png, unused__1234567.png
    
    // Расширение (формат файла) указывать не требуется.
    // Скрипт понимает и обнаруживает любые допустимые расширения (jpeg, jpg, png)
    // Регистр букв в названии файла может быть любым.
    
    // В окнах сообщений показывать имя файла:
    // 0 - заданное пользователем (например "1234567")
    // 1 - фактическое имя прикреплённого файла (например "_1234567.png" или "unused_1234567.png")
    var showActualFileName = 1; // по умолчанию 1
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима
    
    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0; // 0 - нет, 1 - да
    
    // Обрабатывать раздел комментариев
    var processCommentsSection = 0; // 0 - нет, 1 - да

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    // Проверяем существование иллюстрации в документе
    var imageCheckResult = checkImageExists(UserImageName);
    
    // ==================================================
    // ПРОВЕРКА НАЛИЧИЯ ВЫДЕЛЕНИЯ
    // ==================================================
    
    var selRange = document.selection.createRange();
    var hasSelection = false;
    
    if (selRange && selRange.compareEndPoints("StartToEnd", selRange) != 0) {
        if (selRange.parentElement().nodeName != "TEXTAREA" && selRange.parentElement().nodeName != "INPUT") {
            hasSelection = true;
        }
    }
    
    // ==================================================
    // СБОР СЕКЦИЙ ДЛЯ ОБРАБОТКИ
    // ==================================================
    
    var targetSections = [];
    
    if (hasSelection) {
        // РЕЖИМ: ОБРАБОТКА ВЫДЕЛЕННЫХ СЕКЦИЙ
        
        function getNextNode(el) {
            if (el.firstChild && el.nodeName != "P")
                el = el.firstChild;
            else {
                while (el && !el.nextSibling)
                    el = el.parentNode;
                if (el && el.nextSibling)
                    el = el.nextSibling;
            }
            return el;
        }
        
        function findParentSection(el) {
            var current = el;
            while (current && current.nodeType == 1) {
                if (current.className && current.className.indexOf('section') !== -1) {
                    return current;
                }
                current = current.parentNode;
            }
            return null;
        }
        
        var trStart = document.selection.createRange();
        trStart.collapse(true);
        var blockStartEl = trStart.parentElement();
        
        var trEnd = document.selection.createRange();
        trEnd.collapse(false);
        var blockEndEl = trEnd.parentElement();
        
        var ptr = blockStartEl;
        var collectedSections = [];
        
        while (ptr && ptr != null) {
            var section = findParentSection(ptr);
            if (section) {
                var alreadyExists = false;
                for (var k = 0; k < collectedSections.length; k++) {
                    if (collectedSections[k] === section) {
                        alreadyExists = true;
                        break;
                    }
                }
                if (!alreadyExists) {
                    collectedSections.push(section);
                }
            }
            if (ptr === blockEndEl) break;
            ptr = getNextNode(ptr);
            if (!ptr) break;
        }
        
        targetSections = collectedSections;
    }
    
    // ==================================================
    // ПОДСЧЁТ ПУСТЫХ КАРТИНОК В ОБРАБАТЫВАЕМОЙ ОБЛАСТИ
    // ==================================================
    
    var emptyCount = 0;
    
    if (hasSelection) {
        // Считаем только в выделенных секциях
        for (var s = 0; s < targetSections.length; s++) {
            var section = targetSections[s];
            
            // Проверяем, в каком разделе находится секция
            var sectionBody = findParentBody(section);
            var fbname = sectionBody ? (sectionBody.getAttribute("fbname") || "") : "";
            
            var shouldCount = false;
            if (fbname == "") {
                shouldCount = true;
            } else if (fbname == "notes" && processNotesSection == 1) {
                shouldCount = true;
            } else if (fbname == "comments" && processCommentsSection == 1) {
                shouldCount = true;
            }
            
            if (shouldCount) {
                emptyCount += countEmptyInContainer(section);
            }
        }
    } else {
        // Считаем во всём документе (старая логика)
        var emptyCountResult = countEmptyImagesWithSettings(processNotesSection, processCommentsSection);
        emptyCount = emptyCountResult.total;
    }
    
    // ==================================================
    // ПРОВЕРКИ И ПОДТВЕРЖДЕНИЕ
    // ==================================================
    
    // Проверяем случай: нет ни картинки, ни пустышек
    if (!imageCheckResult.found && emptyCount == 0) {
        var msgNoAll = scriptName + "\n" +
               "ver. " + version + "\n\n" +
               "В документе отсутствует иллюстрация с указанным именем.\n" +
               "Пустых картинок (#undefined) в обрабатываемой области не обнаружено.\n" +
               "Обрабатывать нечего!\n\n" +
               "* Для работы скрипта должны быть расставлены\n" +
               "пустые картинки и одна иллюстрация с именем файла " + UserImageName;
        MsgBox(msgNoAll);
        return;
    }
    
    // Проверяем случай: нет картинки
    if (!imageCheckResult.found) {
        var msgNoImage = scriptName + "\n" +
               "ver. " + version + "\n\n" +
               "В документе отсутствует иллюстрация с указанным именем.\n\n" +
               "Прикрепите к документу файл иллюстрации с именем:\n" +
               UserImageName + " (jpg или png)\n" +
               "и запустите скрипт повторно.";
        MsgBox(msgNoImage);
        return;
    }
    
    // Проверяем случай: нет пустышек
    if (emptyCount == 0) {
        var msgNoEmpty = scriptName + "\n" +
               "ver. " + version + "\n\n" +
               "Пустых картинок (#undefined) в обрабатываемой области не обнаружено.\n\n" +
               "Расставьте в документе пустые картинки в нужных местах\n" +
               "и запустите скрипт повторно.";
        MsgBox(msgNoEmpty);
        return;
    }
    
    var actualImageName = imageCheckResult.actualName;
    var displayImageName = (showActualFileName == 1) ? actualImageName : UserImageName;
    
    // Формируем строку с настройками
    var settingsStr = "";
    settingsStr += "Режим обработки: ";
    if (hasSelection) {
        settingsStr += "ВЫДЕЛЕНИЕ\n";
        settingsStr += "Выделено секций: " + targetSections.length + "\n";
    } else {
        settingsStr += "ВЕСЬ ДОКУМЕНТ\n";
    }
    settingsStr += "\n- Обработка разделов:\n";
    settingsStr += "  • Основной раздел: ДА\n";
    if (processNotesSection == 1) {
        settingsStr += "  • Раздел сносок (примечаний): ДА\n";
    } else {
        settingsStr += "  • Раздел сносок (примечаний): НЕТ\n";
    }
    if (processCommentsSection == 1) {
        settingsStr += "  • Раздел комментариев: ДА\n";
    } else {
        settingsStr += "  • Раздел комментариев: НЕТ\n";
    }
    
    // Если обычный режим - показываем окно подтверждения
    if (showStatistics == 1) {
        var confirmMsg = scriptName + "\n" +
                              "ver. " + version + "\n\n" +
                              "Найдена заданная иллюстрация: " + displayImageName + "\n" +
                              "Найдено пустых картинок (#undefined): " + emptyCount + "\n" +
                              settingsStr + "\n" +
                              "Заменить все пустые картинки (#undefined) на эту иллюстрацию?";
        
        var result = AskYesNo(confirmMsg);
        
        if (result != 1) {
            return;
        }
    }
    
    // Запускаем таймер ПОСЛЕ подтверждения
    var startTime = new Date();
    
    var stats = {
        totalFound: 0,
        replaced: 0,
        skippedNotes: 0,
        skippedComments: 0
    };
    
    window.external.BeginUndoUnit(document, "Замена пустых иллюстраций");
    
    try {
        window.external.SetStatusBarText("Заменяем пустые иллюстрации...");
    } catch(e) {}
    
    // ==================================================
    // СБОР ПУСТЫХ КАРТИНОК ДЛЯ ЗАМЕНЫ
    // ==================================================
    
    var emptyImages = [];
    
    if (hasSelection) {
        // Ищем пустые картинки только в выделенных секциях
        for (var s2 = 0; s2 < targetSections.length; s2++) {
            var section2 = targetSections[s2];
            
            // Проверяем раздел
            var sectionBody2 = findParentBody(section2);
            var fbname2 = sectionBody2 ? (sectionBody2.getAttribute("fbname") || "") : "";
            
            var shouldProcess2 = false;
            if (fbname2 == "") {
                shouldProcess2 = true;
            } else if (fbname2 == "notes" && processNotesSection == 1) {
                shouldProcess2 = true;
            } else if (fbname2 == "comments" && processCommentsSection == 1) {
                shouldProcess2 = true;
            }
            
            if (!shouldProcess2) {
                var skippedInSection = countEmptyInContainer(section2);
                if (fbname2 == "notes") {
                    stats.skippedNotes += skippedInSection;
                } else if (fbname2 == "comments") {
                    stats.skippedComments += skippedInSection;
                }
                continue;
            }
            
            // Ищем все div с классом image внутри этой секции
            var divs = section2.getElementsByTagName("div");
            for (var i = 0; i < divs.length; i++) {
                var div = divs[i];
                var className = div.className ? div.className.toString() : '';
                
                if (className.indexOf('image') != -1) {
                    var href = div.getAttribute('href');
                    if (href) {
                        var hrefLower = href.toLowerCase();
                        if (hrefLower == "#undefined" || hrefLower == "#nobin_undefined") {
                            emptyImages.push(div);
                        }
                    }
                }
            }
        }
    } else {
        // Старая логика: ищем по всем body
        var allBodyDivs = [];
        var allDivs = document.getElementsByTagName("DIV");
        for (var d = 0; d < allDivs.length; d++) {
            var div = allDivs[d];
            if (div.className == "body") {
                allBodyDivs.push(div);
            }
        }
        
        for (var b = 0; b < allBodyDivs.length; b++) {
            var bodyElement = allBodyDivs[b];
            var fbname = bodyElement.getAttribute("fbname") || "";
            
            var shouldProcess = false;
            if (fbname == "") {
                shouldProcess = true;
            } else if (fbname == "notes" && processNotesSection == 1) {
                shouldProcess = true;
            } else if (fbname == "comments" && processCommentsSection == 1) {
                shouldProcess = true;
            }
            
            if (!shouldProcess) {
                var skippedCount = countEmptyInBody(bodyElement);
                if (fbname == "notes") {
                    stats.skippedNotes += skippedCount;
                } else if (fbname == "comments") {
                    stats.skippedComments += skippedCount;
                }
                continue;
            }
            
            var bodyDivs = bodyElement.getElementsByTagName("div");
            for (var i2 = 0; i2 < bodyDivs.length; i2++) {
                var div2 = bodyDivs[i2];
                var className2 = div2.className ? div2.className.toString() : '';
                
                if (className2.indexOf('image') != -1) {
                    var href2 = div2.getAttribute('href');
                    if (href2) {
                        var hrefLower2 = href2.toLowerCase();
                        if (hrefLower2 == "#undefined" || hrefLower2 == "#nobin_undefined") {
                            emptyImages.push(div2);
                        }
                    }
                }
            }
        }
    }
    
    stats.totalFound = emptyImages.length;
    
    // ==================================================
    // ЗАМЕНА ПУСТЫХ КАРТИНОК
    // ==================================================
    
    for (var j = emptyImages.length - 1; j >= 0; j--) {
        var oldDiv = emptyImages[j];
        var newDiv = createImageElement(actualImageName);
        
        try {
            oldDiv.parentNode.replaceChild(newDiv, oldDiv);
            stats.replaced++;
        } catch(e) {
            // Если не удалось заменить, пропускаем
        }
    }
    
    // Обновляем изображения
    updateImages();
    
    window.external.EndUndoUnit(document);
    
    // Вычисляем время выполнения
    var endTime = new Date();
    var timeDiff = endTime - startTime;
    var timeSec = (timeDiff / 1000).toFixed(3).replace('.', ',') + " сек";
    
    // Выводим статистику если нужно
    if (showStatistics == 1) {
        var resultMessage = scriptName + "\n" +
                           "ver. " + version + "\n\n";
        
        resultMessage += "Режим обработки: ";
        if (hasSelection) {
            resultMessage += "ВЫДЕЛЕНИЕ\n";
            resultMessage += "Выделено секций: " + targetSections.length + "\n\n";
        } else {
            resultMessage += "ВЕСЬ ДОКУМЕНТ\n\n";
        }
        
        resultMessage += "✓ Всего найдено пустых картинок: " + stats.totalFound + "\n";
        resultMessage += "✓ Заменено на \"" + displayImageName + "\": " + stats.replaced + "\n";
        
        if (processNotesSection == 0 && stats.skippedNotes > 0) {
            resultMessage += "  • пропущено в сносках: " + stats.skippedNotes + "\n";
        }
        if (processCommentsSection == 0 && stats.skippedComments > 0) {
            resultMessage += "  • пропущено в комментариях: " + stats.skippedComments + "\n";
        }
        
        resultMessage += "\nВремя обработки: " + timeSec;
        
        MsgBox(resultMessage);
    }
}

// ==================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ==================================================

// Поиск родительского body для элемента
function findParentBody(element) {
    var current = element;
    while (current && current.nodeType == 1) {
        if (current.className && current.className.indexOf('body') !== -1) {
            return current;
        }
        current = current.parentNode;
    }
    return null;
}

// Подсчитывает количество пустых картинок в указанном контейнере (секции или body)
function countEmptyInContainer(container) {
    var count = 0;
    
    var divs = container.getElementsByTagName("div");
    for (var i = 0; i < divs.length; i++) {
        var div = divs[i];
        var className = div.className ? div.className.toString() : '';
        
        if (className.indexOf('image') != -1) {
            var href = div.getAttribute('href');
            if (href) {
                var hrefLower = href.toLowerCase();
                if (hrefLower == "#undefined" || hrefLower == "#nobin_undefined") {
                    count++;
                }
            }
        }
    }
    
    return count;
}

// Подсчитывает количество пустых картинок с учётом настроек разделов (для режима "весь документ")
function countEmptyImagesWithSettings(processNotes, processComments) {
    var result = {
        total: 0,
        main: 0,
        notes: 0,
        comments: 0
    };
    
    var allBodyDivs = [];
    var allDivs = document.getElementsByTagName("DIV");
    for (var d = 0; d < allDivs.length; d++) {
        var div = allDivs[d];
        if (div.className == "body") {
            allBodyDivs.push(div);
        }
    }
    
    for (var b = 0; b < allBodyDivs.length; b++) {
        var bodyElement = allBodyDivs[b];
        var fbname = bodyElement.getAttribute("fbname") || "";
        
        var count = countEmptyInBody(bodyElement);
        
        if (fbname == "") {
            result.main = count;
            result.total += count;
        } else if (fbname == "notes" && processNotes == 1) {
            result.notes = count;
            result.total += count;
        } else if (fbname == "comments" && processComments == 1) {
            result.comments = count;
            result.total += count;
        }
    }
    
    return result;
}

// Подсчитывает количество пустых картинок в указанном body
function countEmptyInBody(bodyElement) {
    return countEmptyInContainer(bodyElement);
}

// Удаляет расширение из имени файла
function removeExtension(filename) {
    var lastDotIndex = filename.lastIndexOf('.');
    if (lastDotIndex != -1) {
        return filename.substring(0, lastDotIndex);
    }
    return filename;
}

// Проверяет существование иллюстрации в документе (регистронезависимо, ищет .png/.jpg/.jpeg)
function checkImageExists(imageName) {
    var result = {
        found: false,
        actualName: ""
    };
    
    var baseName = removeExtension(imageName).toLowerCase();
    var supportedExtensions = ['.png', '.jpg', '.jpeg'];
    
    var possiblePrefixes = ["", "_", "unused_", "unused__"];
    
    var binObjects = document.all.binobj;
    if (binObjects) {
        var binaryDivs = binObjects.getElementsByTagName("DIV");
        
        for (var i = 0; i < binaryDivs.length; i++) {
            var div = binaryDivs[i];
            var inputs = div.getElementsByTagName("INPUT");
            
            for (var j = 0; j < inputs.length; j++) {
                if (inputs[j].id == "id") {
                    var imageId = inputs[j].value;
                    var imageIdLower = imageId.toLowerCase();
                    var imageIdWithoutExt = removeExtension(imageIdLower);
                    
                    for (var p = 0; p < possiblePrefixes.length; p++) {
                        if (imageIdWithoutExt === possiblePrefixes[p] + baseName) {
                            for (var extIdx = 0; extIdx < supportedExtensions.length; extIdx++) {
                                if (imageIdLower.indexOf(supportedExtensions[extIdx], imageIdWithoutExt.length) != -1) {
                                    result.found = true;
                                    result.actualName = imageId;
                                    return result;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (!result.found) {
        var allDivs = document.getElementsByTagName('div');
        for (var i2 = 0; i2 < allDivs.length; i2++) {
            var div2 = allDivs[i2];
            var className2 = div2.className ? div2.className.toString() : '';
            if (className2.indexOf('image') != -1) {
                var href = div2.getAttribute('href');
                if (href && href.charAt(0) === '#') {
                    var imgName = href.substring(1);
                    var imgNameLower = imgName.toLowerCase();
                    var imgNameWithoutExt = removeExtension(imgNameLower);
                    
                    for (var p2 = 0; p2 < possiblePrefixes.length; p2++) {
                        if (imgNameWithoutExt === possiblePrefixes[p2] + baseName) {
                            for (var extIdx2 = 0; extIdx2 < supportedExtensions.length; extIdx2++) {
                                if (imgNameLower.indexOf(supportedExtensions[extIdx2], imgNameWithoutExt.length) != -1) {
                                    result.found = true;
                                    result.actualName = imgName;
                                    return result;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    return result;
}

// Создает элемент изображения с указанным именем файла
function createImageElement(imageName) {
    var imageDiv = document.createElement('div');
    imageDiv.className = 'image';
    imageDiv.setAttribute('onresizestart', 'return false');
    imageDiv.setAttribute('contenteditable', 'false');
    imageDiv.setAttribute('href', '#' + imageName);
    
    var img = document.createElement('img');
    img.src = 'fbw-internal:#' + imageName;
    imageDiv.appendChild(img);
    
    return imageDiv;
}

// Обновляет изображения в документе
function updateImages() {
    var imgs = document.getElementsByTagName("IMG");
    for (var i = imgs.length - 1; i >= 0; i--) {
        var MyImg = imgs[i];
        var pic_id = MyImg.src;
        MyImg.src = "";
        MyImg.src = pic_id;
    }
}

// Функция для вывода сообщений
function MsgBox(str) {
    window.external.MsgBox(str);
}

// Функция для запроса подтверждения
function AskYesNo(str) {
    return window.external.AskYesNo(str);
}