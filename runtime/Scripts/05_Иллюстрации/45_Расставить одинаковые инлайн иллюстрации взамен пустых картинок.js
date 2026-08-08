// Скрипт "Расставить одинаковые инлайн иллюстрации взамен пустых картинок"
// version 1.0
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для вставки заданной иллюстрации на место пустых инлайн-картинок
// (#undefined) в fb2 документах.
// В документе предварительно должны быть расставлены пустые инлайн-картинки
// и прикреплена одна иллюстрация с именем файла, например, 1234567 (без указания расширения).
// Скрипт находит прикреплённый файл по имени, игнорируя префиксы _, unused_, unused__.
// Поддерживаются допустимые типы файлов jpg, jpeg, png.
// Имя файла можно задавать любое - цифрами или буквами с любым регистром.
// По умолчанию обрабатывается только основной раздел документа, без сносок и комментариев.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.0, 02.05.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Расставить одинаковые инлайн иллюстрации взамен пустых картинок";
    var version = "1.0";

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
    
    // Подсчитываем количество пустых инлайн-картинок с учётом настроек разделов
    var emptyCountResult = countEmptyImagesWithSettings(processNotesSection, processCommentsSection);
    var emptyCount = emptyCountResult.total;
    
    // Проверяем случай: нет ни картинки, ни пустышек
    if (!imageCheckResult.found && emptyCount == 0) {
        MsgBox(scriptName + "\n" +
               "ver. " + version + "\n\n" +
               "В документе отсутствует иллюстрация с указанным именем.\n" +
               "Пустых инлайн-картинок (#undefined) в документе не обнаружено.\n" +
               "Обрабатывать нечего!\n\n" +
               "* Для работы скрипта должны быть расставлены\n" +
               "пустые инлайн-картинки и одна иллюстрация с именем файла " + UserImageName);
        return;
    }
    
    // Проверяем случай: нет картинки
    if (!imageCheckResult.found) {
        MsgBox(scriptName + "\n" +
               "ver. " + version + "\n\n" +
               "В документе отсутствует иллюстрация с указанным именем.\n\n" +
               "Прикрепите к документу файл иллюстрации с именем:\n" +
               UserImageName + " (jpg или png)\n" +
               "и запустите скрипт повторно.");
        return;
    }
    
    // Проверяем случай: нет пустышек
    if (emptyCount == 0) {
        MsgBox(scriptName + "\n" +
               "ver. " + version + "\n\n" +
               "Пустых инлайн-картинок (#undefined) в документе не обнаружено.\n\n" +
               "Расставьте в документе пустые инлайн-картинки в нужных местах\n" +
               "и запустите скрипт повторно.");
        return;
    }
    
    var actualImageName = imageCheckResult.actualName;
    var displayImageName = (showActualFileName == 1) ? actualImageName : UserImageName;
    
    // Формируем строку с настройками разделов
    var settingsStr = "\n- Обработка разделов:\n";
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
        var result = AskYesNo(scriptName + "\n" +
                              "ver. " + version + "\n\n" +
                              "Найдена заданная иллюстрация: " + displayImageName + "\n" +
                              "Найдено пустых инлайн-картинок (#undefined): " + emptyCount + "\n" +
                              settingsStr + "\n" +
                              "Заменить все пустые инлайн-картинки (#undefined) на эту иллюстрацию?");
        
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
    
    window.external.BeginUndoUnit(document, "Замена пустых инлайн-иллюстраций");
    
    try {
        window.external.SetStatusBarText("Заменяем пустые инлайн-иллюстрации...");
    } catch(e) {}
    
    // Находим все пустые инлайн-картинки
    var emptyImages = [];
    
    // Находим все DIV с классом body
    var allBodyDivs = [];
    var allDivs = document.getElementsByTagName("DIV");
    for (var d = 0; d < allDivs.length; d++) {
        var div = allDivs[d];
        if (div.className == "body") {
            allBodyDivs.push(div);
        }
    }
    
    // Обрабатываем каждый найденный body
    for (var b = 0; b < allBodyDivs.length; b++) {
        var bodyElement = allBodyDivs[b];
        var fbname = bodyElement.getAttribute("fbname") || "";
        
        // Проверяем, нужно ли обрабатывать этот раздел
        var shouldProcess = false;
        if (fbname == "") {
            shouldProcess = true;
        } else if (fbname == "notes" && processNotesSection == 1) {
            shouldProcess = true;
        } else if (fbname == "comments" && processCommentsSection == 1) {
            shouldProcess = true;
        }
        
        if (!shouldProcess) {
            // Считаем количество пропущенных картинок в этом разделе
            var skippedCount = countEmptyInBody(bodyElement);
            if (fbname == "notes") {
                stats.skippedNotes += skippedCount;
            } else if (fbname == "comments") {
                stats.skippedComments += skippedCount;
            }
            continue;
        }
        
        // Ищем все SPAN с классом image внутри этого body
        var spans = bodyElement.getElementsByTagName("SPAN");
        for (var i = 0; i < spans.length; i++) {
            var span = spans[i];
            var className = span.className ? span.className.toString() : '';
            
            if (className.indexOf('image') != -1) {
                var href = span.getAttribute('href');
                if (href) {
                    var hrefLower = href.toLowerCase();
                    // Проверяем оба варианта пустых картинок: #undefined и #nobin_undefined
                    if (hrefLower == "#undefined" || hrefLower == "#nobin_undefined") {
                        emptyImages.push(span);
                    }
                }
            }
        }
    }
    
    stats.totalFound = emptyImages.length;
    
    // Заменяем найденные пустые инлайн-картинки (в обратном порядке)
    for (var j = emptyImages.length - 1; j >= 0; j--) {
        var oldSpan = emptyImages[j];
        var newSpan = createInlineImageElement(actualImageName);
        
        try {
            oldSpan.parentNode.replaceChild(newSpan, oldSpan);
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
        resultMessage += "✓ Всего найдено пустых инлайн-картинок: " + stats.totalFound + "\n";
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

// Подсчитывает количество пустых инлайн-картинок с учётом настроек разделов
function countEmptyImagesWithSettings(processNotes, processComments) {
    var result = {
        total: 0,
        main: 0,
        notes: 0,
        comments: 0
    };
    
    // Находим все DIV с классом body
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

// Подсчитывает количество пустых инлайн-картинок в указанном body
function countEmptyInBody(bodyElement) {
    var count = 0;
    
    var spans = bodyElement.getElementsByTagName("SPAN");
    for (var i = 0; i < spans.length; i++) {
        var span = spans[i];
        var className = span.className ? span.className.toString() : '';
        
        if (className.indexOf('image') != -1) {
            var href = span.getAttribute('href');
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
    
    // Возможные префиксы для имени файла
    var possiblePrefixes = ["", "_", "unused_", "unused__"];
    
    // Ищем бинарные объекты (иллюстрации)
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
                    
                    // Проверяем все возможные варианты с префиксами
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
    
    // Если не нашли в binobj, ищем в документе
    if (!result.found) {
        var allDivs = document.getElementsByTagName('div');
        for (var i = 0; i < allDivs.length; i++) {
            var div = allDivs[i];
            var className = div.className ? div.className.toString() : '';
            if (className.indexOf('image') != -1) {
                var href = div.getAttribute('href');
                if (href && href.charAt(0) === '#') {
                    var imgName = href.substring(1);
                    var imgNameLower = imgName.toLowerCase();
                    var imgNameWithoutExt = removeExtension(imgNameLower);
                    
                    // Проверяем все возможные варианты с префиксами
                    for (var p = 0; p < possiblePrefixes.length; p++) {
                        if (imgNameWithoutExt === possiblePrefixes[p] + baseName) {
                            for (var extIdx = 0; extIdx < supportedExtensions.length; extIdx++) {
                                if (imgNameLower.indexOf(supportedExtensions[extIdx], imgNameWithoutExt.length) != -1) {
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

// Создает элемент инлайн-изображения с указанным именем файла
function createInlineImageElement(imageName) {
    var imageSpan = document.createElement('SPAN');
    imageSpan.className = 'image';
    imageSpan.setAttribute('onresizestart', 'return false');
    imageSpan.setAttribute('contenteditable', 'false');
    imageSpan.setAttribute('href', '#' + imageName);
    
    var img = document.createElement('IMG');
    img.src = 'fbw-internal:#' + imageName;
    imageSpan.appendChild(img);
    
    return imageSpan;
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
