// Скрипт: "Статистика иллюстраций" для редактора FBE 
// version 4.3
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для отображения максимально подробной статистики
// по всем иллюстрациям в fb2 документе.
// Никаких изменений в документе скрипт не производит.

// version 4.3, 07.12.2025
// ============================================

// ============================================
// НАСТРОЙКА ВЫСОТЫ ОКНА
// Максимальное количество строк в одном окне:
// 15-17" ноутбук: 25-30 строк
// 19-24" монитор: 35-40 строк
// 27-32" монитор: 45-50 строк
var MAX_LINES_PER_WINDOW = 45; // Измените это значение под ваш экран
// ============================================

function Run() {
    try {
        var scriptName = "Статистика иллюстраций";
        var scriptVersion = "4.3";
        
        var stats = {
            hasCover: false,
            coverFileName: "",
            coverFileSize: 0,
            blockImages: 0,
            emptyBlockImages: 0,
            inlineImages: 0,
            emptyInlineImages: 0,
            totalBinaries: 0,
            usedBinaries: 0,
            unusedBinaries: 0,
            // Форматы FB2
            jpgCount: 0,
            pngCount: 0,
            noExtensionCount: 0,
            // Другие форматы
            bmpCount: 0,
            tifCount: 0,
            gifCount: 0,
            webpCount: 0,
            svgCount: 0,
            epsCount: 0,
            icoCount: 0,
            otherCount: 0,
            // Размеры
            totalOriginalSize: 0,
            totalBase64Size: 0,
            fb2FormatsSize: 0,
            fb2FormatsBase64Size: 0,
            otherFormatsSize: 0,
            otherFormatsBase64Size: 0
        };
        
        // 1. Сначала ищем обложку
        findCoverImage(stats);
        
        // 2. Находим все ссылки на изображения в документе
        var imageLinks = findAllImageLinks();
        stats.blockImages = imageLinks.blockImages;
        stats.inlineImages = imageLinks.inlineImages;
        stats.emptyBlockImages = imageLinks.emptyBlockImages;
        stats.emptyInlineImages = imageLinks.emptyInlineImages;
        
        // 3. Находим ВСЕ бинарные файлы и их ТОЧНЫЕ размеры
        var binaryInfo = getBinariesWithExactSizes(stats);
        stats.totalBinaries = binaryInfo.total;
        stats.totalOriginalSize = binaryInfo.totalOriginalSize;
        stats.totalBase64Size = binaryInfo.totalBase64Size;
        stats.fb2FormatsSize = binaryInfo.fb2FormatsSize;
        stats.fb2FormatsBase64Size = binaryInfo.fb2FormatsBase64Size;
        stats.otherFormatsSize = binaryInfo.otherFormatsSize;
        stats.otherFormatsBase64Size = binaryInfo.otherFormatsBase64Size;
        
        // 4. Проверяем, есть ли вообще иллюстраций в документе
        var totalImages = stats.blockImages + stats.emptyBlockImages + 
                         stats.inlineImages + stats.emptyInlineImages;
        
        // Если нет ни иллюстраций, ни бинарников, ни обложки - показываем простое сообщение
        if (!stats.hasCover && totalImages === 0 && stats.totalBinaries === 0) {
            MsgBox(scriptName + "\nversion: " + scriptVersion + 
                  "\n\nВ документе иллюстраций нет!");
            return;
        }
        
        // 5. Получаем размер обложки отдельно
        if (stats.hasCover && stats.coverFileName) {
            stats.coverFileSize = getCoverFileSizeExact(stats.coverFileName, binaryInfo.binaries);
        }
        
        // 6. Сопоставляем ссылки с бинарниками
        var matchResults = matchLinksToBinaries(imageLinks.usedFiles, binaryInfo.binaries, stats);
        stats.usedBinaries = matchResults.usedBinaries;
        stats.unusedBinaries = matchResults.unusedBinaries;
        
        // 7. Проверяем арифметику
        if (stats.totalBinaries !== (stats.usedBinaries + stats.unusedBinaries)) {
            stats.totalBinaries = stats.usedBinaries + stats.unusedBinaries;
        }
        
        // 8. Формируем текст статистики в массив строк
        var lines = [];
        
       
        // Обложка с ТОЧНЫМ размером
        if (stats.hasCover) {
            if (stats.coverFileName) {
                var coverSizeText = "";
                if (stats.coverFileSize > 0) {
                    coverSizeText = ", " + formatFileSize(stats.coverFileSize);
                }
                lines.push("Обложка - 1 (" + stats.coverFileName + ")" + coverSizeText);
            } else {
                lines.push("Обложка - 1");
            }
        } else {
            lines.push("Обложка - нет");
        }
        lines.push(""); // Пустая строка
        
        lines.push("Блочные иллюстрации - " + stats.blockImages);
        lines.push("Пустые блочные иллюстрации - " + stats.emptyBlockImages);
        lines.push("");
        
        lines.push("Инлайн иллюстрации (внутри абзацев) - " + stats.inlineImages);
        lines.push("Пустые инлайн иллюстрации (внутри абзацев) - " + stats.emptyInlineImages);
        lines.push("");
        
        var totalEmptyImages = stats.emptyBlockImages + stats.emptyInlineImages;
        lines.push("Всего иллюстраций - " + totalImages);
        lines.push("Из них пустых иллюстраций - " + totalEmptyImages);
        lines.push("");
        
        // Информация о бинарниках
        lines.push("ПРИКРЕПЛЕННЫЕ ФАЙЛЫ:");
        lines.push("");
        
        lines.push("Всего бинарных файлов изображений - " + stats.totalBinaries);
        lines.push("Используются в иллюстрациях - " + stats.usedBinaries);
        
        // Добавляем информацию о неподдерживаемых форматах среди ИСПОЛЬЗУЕМЫХ
        var usedOtherFormats = 0;
        var usedOtherFormatTypes = {};
        
        // Перебираем все бинарники и считаем только те, которые ИСПОЛЬЗУЮТСЯ и имеют неподдерживаемые форматы
        for (var fileName in binaryInfo.binaries) {
            if (binaryInfo.binaries.hasOwnProperty(fileName)) {
                var binary = binaryInfo.binaries[fileName];
                if (binary.isUsed) {
                    var fileType = binary.fileType;
                    // Проверяем, является ли неподдерживаемым форматом
                    if (fileType === 'bmp' || fileType === 'tif' || fileType === 'gif' || 
                        fileType === 'webp' || fileType === 'svg' || fileType === 'eps' || 
                        fileType === 'ico' || fileType === 'other_image') {
                        usedOtherFormats++;
                        
                        // Собираем статистику по типам файлов
                        if (fileType === 'bmp') usedOtherFormatTypes['bmp'] = (usedOtherFormatTypes['bmp'] || 0) + 1;
                        else if (fileType === 'tif') usedOtherFormatTypes['tif'] = (usedOtherFormatTypes['tif'] || 0) + 1;
                        else if (fileType === 'gif') usedOtherFormatTypes['gif'] = (usedOtherFormatTypes['gif'] || 0) + 1;
                        else if (fileType === 'webp') usedOtherFormatTypes['webp'] = (usedOtherFormatTypes['webp'] || 0) + 1;
                        else if (fileType === 'svg') usedOtherFormatTypes['svg'] = (usedOtherFormatTypes['svg'] || 0) + 1;
                        else if (fileType === 'eps') usedOtherFormatTypes['eps'] = (usedOtherFormatTypes['eps'] || 0) + 1;
                        else if (fileType === 'ico') usedOtherFormatTypes['ico'] = (usedOtherFormatTypes['ico'] || 0) + 1;
                        else if (fileType === 'other_image') usedOtherFormatTypes['other'] = (usedOtherFormatTypes['other'] || 0) + 1;
                    }
                }
            }
        }
        
        if (usedOtherFormats > 0) {
            // Собираем список форматов, которые действительно есть среди используемых
            var otherFormatsList = [];
            if (usedOtherFormatTypes['bmp']) otherFormatsList.push("bmp");
            if (usedOtherFormatTypes['tif']) otherFormatsList.push("tif");
            if (usedOtherFormatTypes['gif']) otherFormatsList.push("gif");
            if (usedOtherFormatTypes['webp']) otherFormatsList.push("webp");
            if (usedOtherFormatTypes['svg']) otherFormatsList.push("svg");
            if (usedOtherFormatTypes['eps']) otherFormatsList.push("eps");
            if (usedOtherFormatTypes['ico']) otherFormatsList.push("ico");
            if (usedOtherFormatTypes['other']) otherFormatsList.push("др.");
            
            // Формируем текст
            var formatsText = "";
            if (otherFormatsList.length <= 2) {
                formatsText = otherFormatsList.join(", ");
            } else {
                formatsText = otherFormatsList.slice(0, 2).join(", ") + " и др.";
            }
            
            lines.push("Из них в неподдерживаемых форматах (" + formatsText + ") - " + usedOtherFormats);
        }
        
        lines.push("Не используются (не прилинкованы) - " + stats.unusedBinaries);
        lines.push("");
        
        // Статистика по поддерживаемым форматам FB2
        var hasFB2FormatStats = false;
        var fb2FormatLines = [];
        
        if (stats.jpgCount > 0) {
            fb2FormatLines.push("JPG - " + stats.jpgCount);
            hasFB2FormatStats = true;
        }
        
        if (stats.pngCount > 0) {
            fb2FormatLines.push("PNG - " + stats.pngCount);
            hasFB2FormatStats = true;
        }
        
        if (stats.noExtensionCount > 0) {
            var noExtText = "Без расширения (img_0, img_1 и т.д.) - " + stats.noExtensionCount;
            fb2FormatLines.push(noExtText);
            hasFB2FormatStats = true;
        }
        
        if (hasFB2FormatStats) {
            lines.push("ПОДДЕРЖИВАЕМЫЕ ФОРМАТЫ FB2:");
            lines.push("");
            for (var i = 0; i < fb2FormatLines.length; i++) {
                lines.push(fb2FormatLines[i]);
            }
            lines.push("");
        }
        
        // Вес поддерживаемых форматов
        if (stats.fb2FormatsSize > 0) {
            var fb2SizeText = formatFileSize(stats.fb2FormatsSize);
            lines.push("Вес иллюстраций (JPG, PNG) - " + fb2SizeText);
            
            var fb2Base64Text = formatFileSize(stats.fb2FormatsBase64Size);
            lines.push("Размер в FB2 файле (Base64) - " + fb2Base64Text);
        } else if (stats.jpgCount > 0 || stats.pngCount > 0) {
            lines.push("Вес иллюстраций (JPG, PNG) - оценка недоступна");
        }
        
        // Статистика по другим форматам (ВСЕМ, включая неиспользуемые)
        var hasOtherFormatStats = false;
        var otherFormatLines = [];
        
        if (stats.bmpCount > 0) {
            otherFormatLines.push("BMP - " + stats.bmpCount);
            hasOtherFormatStats = true;
        }
        
        if (stats.tifCount > 0) {
            otherFormatLines.push("TIF - " + stats.tifCount);
            hasOtherFormatStats = true;
        }
        
        if (stats.gifCount > 0) {
            otherFormatLines.push("GIF - " + stats.gifCount);
            hasOtherFormatStats = true;
        }
        
        if (stats.webpCount > 0) {
            otherFormatLines.push("WEBP - " + stats.webpCount);
            hasOtherFormatStats = true;
        }
        
        if (stats.svgCount > 0) {
            otherFormatLines.push("SVG - " + stats.svgCount);
            hasOtherFormatStats = true;
        }
        
        if (stats.epsCount > 0) {
            otherFormatLines.push("EPS - " + stats.epsCount);
            hasOtherFormatStats = true;
        }
        
        if (stats.icoCount > 0) {
            otherFormatLines.push("ICO - " + stats.icoCount);
            hasOtherFormatStats = true;
        }
        
        if (stats.otherCount > 0) {
            otherFormatLines.push("Другие форматы - " + stats.otherCount);
            hasOtherFormatStats = true;
        }
        
        if (hasOtherFormatStats) {
            lines.push("");
            lines.push("ДРУГИЕ ФОРМАТЫ:");
            lines.push("(BMP, TIF, GIF, WEBP, SVG, EPS, ICO...)");
            lines.push("");
            for (var i = 0; i < otherFormatLines.length; i++) {
                lines.push(otherFormatLines[i]);
            }
            lines.push("");
            
            if (stats.otherFormatsSize > 0) {
                var otherSizeText = formatFileSize(stats.otherFormatsSize);
                lines.push("Вес иллюстраций (другие форматы) - " + otherSizeText);
                
                var otherBase64Text = formatFileSize(stats.otherFormatsBase64Size);
                lines.push("Размер в FB2 файле (Base64) - " + otherBase64Text);
            }
        }
        
        // Общий вес всех иллюстраций
        if (stats.totalOriginalSize > 0) {
            lines.push("");
            lines.push("----------------------");
            var totalSizeText = formatFileSize(stats.totalOriginalSize);
            lines.push("Общий вес всех иллюстраций - " + totalSizeText);
            
            var totalBase64Text = formatFileSize(stats.totalBase64Size);
            lines.push("Размер в FB2 файле (Base64) - " + totalBase64Text);
        }
        
        // Если есть неприлинкованные бинарники, покажем их
        if (stats.unusedBinaries > 0 && matchResults.unusedList.length > 0) {
            lines.push("");
            lines.push("Неприлинкованные бинарники изображений:");
            var unusedList = matchResults.unusedList;
            var maxShow = 5;
            for (var j = 0; j < Math.min(unusedList.length, maxShow); j++) {
                lines.push(unusedList[j]);
            }
            if (unusedList.length > maxShow) {
                lines.push("... и еще " + (unusedList.length - maxShow) + " файлов");
            }
        }
        
        // Предупреждение о файлах без расширений
        if (stats.noExtensionCount > 0) {
            var fileWord = getFileWord(stats.noExtensionCount);
            lines.push("");
            lines.push("ВНИМАНИЕ: В документе " + stats.noExtensionCount + 
                      " " + fileWord + " без расширений.");
            lines.push("Рекомендуется добавить правильные расширения (.jpg/.png) для совместимости.");
        }
        
        // Предупреждение о неподдерживаемых форматах
        if (hasOtherFormatStats) {
            // Оставляем только общее предупреждение без конкретных цифр
            lines.push("");
            lines.push("ВНИМАНИЕ: В документе найдены форматы, не поддерживаемые FB2.");
            lines.push("Для создания корректного FB2 файла их необходимо конвертировать в JPG/PNG.");
        }
        
        // 9. Разбиваем на страницы и показываем
        showStatisticsInPages(scriptName, scriptVersion, lines);
        
    } catch (error) {
        MsgBox("Ошибка выполнения скрипта: " + error.message);
    }
}

// Функция для показа статистики в нескольких окнах
function showStatisticsInPages(scriptName, scriptVersion, lines) {
    // Добавляем заголовок в начало каждой страницы
    var header = scriptName + "\nversion: " + scriptVersion + "\n\n";
    
    // Определяем количество строк на страницу (за вычетом заголовка и подвала)
    var linesPerPage = MAX_LINES_PER_WINDOW - 3; // 3 строки на заголовок и кнопки
    
    // Создаем страницы
    var pages = [];
    var currentPage = [];
    var lineCount = 0;
    
    for (var i = 0; i < lines.length; i++) {
        // Если текущая страница заполнена, сохраняем ее
        if (lineCount >= linesPerPage) {
            pages.push(currentPage);
            currentPage = [];
            lineCount = 0;
        }
        
        currentPage.push(lines[i]);
        lineCount++;
    }
    
    // Добавляем последнюю страницу, если она не пустая
    if (currentPage.length > 0) {
        pages.push(currentPage);
    }
    
    // Если всего одна страница, показываем без навигации
    if (pages.length === 1) {
        var pageText = header;
        var pageLines = pages[0];
        
        // Добавляем строки страницы
        for (var j = 0; j < pageLines.length; j++) {
            pageText += pageLines[j] + "\n";
        }
        
        MsgBox(pageText, "FBE скрипт");
        return;
    }
    
    // Показываем страницы по очереди с навигацией
    for (var pageNum = 0; pageNum < pages.length; pageNum++) {
        var pageText = header;
        var pageLines = pages[pageNum];
        
        // Добавляем строки страницы
        for (var j = 0; j < pageLines.length; j++) {
            pageText += pageLines[j] + "\n";
        }
        
        // Добавляем подвал с номером страницы
        pageText += "\n";
        pageText += "◊  Страница " + (pageNum + 1) + " из " + pages.length;
        if (pageNum < pages.length - 1) {
            pageText += "  ◊  Показать следующую страницу?";
        } else {
            pageText += " (последняя)  ◊  Закрыть окно?";
        }
        
        // Показываем страницу
        var result = MsgBox(pageText, "FBE скрипт");
        
        // Если пользователь нажал "Нет" на промежуточной странице, прерываем показ
        if (pageNum < pages.length - 1 && result === 7) { // 7 = IDNO (кнопка "Нет")
            break;
        }
    }
}

function getBinariesWithExactSizes(stats) {
    var result = {
        total: 0,
        totalOriginalSize: 0,
        totalBase64Size: 0,
        fb2FormatsSize: 0,
        fb2FormatsBase64Size: 0,
        otherFormatsSize: 0,
        otherFormatsBase64Size: 0,
        binaries: {}
    };
    
    try {
        var binobj = document.all.binobj;
        if (binobj) {
            var objects = binobj.getElementsByTagName("DIV");
            
            for (var i = 0; i < objects.length; i++) {
                var bin = objects[i];
                var id = bin.all.id.value || "";
                
                if (id) {
                    // Определяем тип файла
                    var fileType = getFileType(id);
                    var isImage = false;
                    
                    // Проверяем, является ли файлом изображения
                    if (fileType !== 'other') {
                        isImage = true;
                        
                        // Обновляем статистику форматов
                        if (fileType === 'jpg') {
                            stats.jpgCount++;
                        } else if (fileType === 'png') {
                            stats.pngCount++;
                        } else if (fileType === 'bmp') {
                            stats.bmpCount++;
                        } else if (fileType === 'tif') {
                            stats.tifCount++;
                        } else if (fileType === 'gif') {
                            stats.gifCount++;
                        } else if (fileType === 'webp') {
                            stats.webpCount++;
                        } else if (fileType === 'svg') {
                            stats.svgCount++;
                        } else if (fileType === 'eps') {
                            stats.epsCount++;
                        } else if (fileType === 'ico') {
                            stats.icoCount++;
                        } else if (fileType === 'no_extension') {
                            stats.noExtensionCount++;
                        } else if (fileType === 'other_image') {
                            stats.otherCount++;
                        }
                    }
                    
                    if (isImage) {
                        var originalSize = 0;
                        var base64Size = 0;
                        
                        // Получаем точный размер
                        try {
                            var len = window.external.GetBinarySize(bin.base64data);
                            originalSize = len;
                            
                            // Рассчитываем размер в Base64
                            var b64 = Math.floor(len * 4 / 3);
                            base64Size = b64 + Math.floor(b64 / 72) * 2 + 60;
                        } catch (e) {
                            // Если не получается, используем fallback
                            originalSize = estimateSizeByFileName(id);
                            base64Size = Math.floor(originalSize * 4 / 3) + 100;
                        }
                        
                        result.binaries[id] = {
                            fileName: id,
                            isUsed: false,
                            originalSize: originalSize,
                            base64Size: base64Size,
                            isCover: isCoverFileName(id),
                            fileType: fileType
                        };
                        
                        result.totalOriginalSize += originalSize;
                        result.totalBase64Size += base64Size;
                        
                        // Разделяем по типам форматов
                        if (fileType === 'jpg' || fileType === 'png' || fileType === 'no_extension') {
                            result.fb2FormatsSize += originalSize;
                            result.fb2FormatsBase64Size += base64Size;
                        } else if (fileType !== 'other') {
                            result.otherFormatsSize += originalSize;
                            result.otherFormatsBase64Size += base64Size;
                        }
                    }
                }
            }
        }
        
        // Подсчитываем общее количество
        result.total = 0;
        for (var key in result.binaries) {
            if (result.binaries.hasOwnProperty(key)) {
                result.total++;
            }
        }
        
    } catch (e) {
        // Игнорируем ошибки
    }
    
    return result;
}

// Определение типа файла
function getFileType(fileName) {
    if (!fileName) return 'other';
    
    var ext = getFileExtension(fileName);
    
    if (!ext) {
        return 'no_extension';
    }
    
    // Форматы FB2
    if (ext === 'jpg' || ext === 'jpeg') {
        return 'jpg';
    } else if (ext === 'png') {
        return 'png';
    }
    
    // Другие графические форматы
    if (ext === 'bmp' || ext === 'dib') {
        return 'bmp';
    } else if (ext === 'tif' || ext === 'tiff') {
        return 'tif';
    } else if (ext === 'gif') {
        return 'gif';
    } else if (ext === 'webp') {
        return 'webp';
    } else if (ext === 'svg') {
        return 'svg';
    } else if (ext === 'eps' || ext === 'ai' || ext === 'ps') {
        return 'eps';
    } else if (ext === 'ico' || ext === 'cur') {
        return 'ico';
    }
    
    // Другие возможные форматы изображений
    if (ext === 'raw' || ext === 'cr2' || ext === 'nef' || ext === 'arw' || 
        ext === 'sr2' || ext === 'orf' || ext === 'rw2' || ext === 'pef' ||
        ext === 'srw' || ext === 'kdc' || ext === 'dcr' || ext === 'mrw' ||
        ext === 'raf' || ext === 'x3f' || ext === 'erf' || ext === 'mef' ||
        ext === 'mos' || ext === 'nrw' || ext === 'dng') {
        return 'other_image';
    }
    
    return 'other';
}

// Получение расширения файла (всегда в нижнем регистре)
function getFileExtension(fileName) {
    if (!fileName) return '';
    
    var lastDot = fileName.lastIndexOf('.');
    if (lastDot === -1) return '';
    
    return fileName.substring(lastDot + 1).toLowerCase();
}

// Функция для правильного согласования числа с существительным "файл"
function getFileWord(count) {
    if (count % 10 === 1 && count % 100 !== 11) {
        return "файл";
    } else if (count % 10 >= 2 && count % 10 <= 4 && 
               (count % 100 < 10 || count % 100 >= 20)) {
        return "файла";
    } else {
        return "файлов";
    }
}

// Функция для проверки, является ли имя файла обложкой
function isCoverFileName(fileName) {
    if (!fileName) return false;
    var lowerName = fileName.toLowerCase();
    
    return (lowerName.indexOf('cover') !== -1 || 
            lowerName === 'img_0' || 
            lowerName === '_img_0' ||
            lowerName === 'i_img_0' ||
            lowerName.indexOf('cover.') === 0 ||
            lowerName.indexOf('cover_') === 0 ||
            lowerName.indexOf('обложка') === 0 ||
            lowerName.indexOf('_cover') === 0);
}

function getCoverFileSizeExact(coverFileName, binaries) {
    if (binaries[coverFileName]) {
        return binaries[coverFileName].originalSize;
    }
    
    var variants = getFileNameVariants(coverFileName);
    for (var i = 0; i < variants.length; i++) {
        if (binaries[variants[i]]) {
            return binaries[variants[i]].originalSize;
        }
    }
    
    return 0;
}

function estimateSizeByFileName(fileName) {
    var lowerName = fileName.toLowerCase();
    
    if (isCoverFileName(lowerName)) {
        return 800 * 1024;
    }
    
    if (lowerName.indexOf('large') !== -1 || lowerName.indexOf('big') !== -1) {
        return 300 * 1024;
    }
    
    if (lowerName.indexOf('small') !== -1 || lowerName.indexOf('thumb') !== -1) {
        return 50 * 1024;
    }
    
    return 100 * 1024;
}

function findAllImageLinks() {
    var result = {
        blockImages: 0,
        inlineImages: 0,
        emptyBlockImages: 0,
        emptyInlineImages: 0,
        usedFiles: {}
    };
    
    try {
        var allElements = document.getElementsByTagName('*');
        
        for (var i = 0; i < allElements.length; i++) {
            var element = allElements[i];
            var className = element.className || '';
            
            if (typeof className === 'string' && className.indexOf('image') !== -1) {
                var href = element.getAttribute('href') || element.getAttribute('l:href') || '';
                var tagName = element.tagName ? element.tagName.toUpperCase() : '';
                var isBlock = (tagName === 'DIV');
                var isEmpty = (href === '#undefined' || href.indexOf('undefined') !== -1);
                
                if (isBlock) {
                    if (isEmpty) {
                        result.emptyBlockImages++;
                    } else {
                        result.blockImages++;
                    }
                } else {
                    if (isEmpty) {
                        result.emptyInlineImages++;
                    } else {
                        result.inlineImages++;
                    }
                }
                
                if (!isEmpty && href && href !== '#') {
                    var fileName = href.replace(/^#/, '');
                    if (fileName !== 'undefined') {
                        result.usedFiles[fileName] = true;
                    }
                }
            }
        }
        
    } catch (e) {
        // Игнорируем ошибки
    }
    
    return result;
}

function matchLinksToBinaries(usedFiles, binaries, stats) {
    var usedBinaries = 0;
    var unusedBinaries = 0;
    var unusedList = [];
    
    // Инициализация
    for (var fileName in binaries) {
        if (binaries.hasOwnProperty(fileName)) {
            binaries[fileName].isUsed = false;
        }
    }
    
    // Сопоставление использованных файлов
    for (var usedFileName in usedFiles) {
        if (usedFiles.hasOwnProperty(usedFileName)) {
            var found = false;
            
            // Прямое совпадение
            if (binaries[usedFileName] && !binaries[usedFileName].isUsed) {
                binaries[usedFileName].isUsed = true;
                usedBinaries++;
                found = true;
            }
            
            // Варианты с префиксами/суффиксами
            if (!found) {
                var variants = getFileNameVariants(usedFileName);
                for (var v = 0; v < variants.length; v++) {
                    if (binaries[variants[v]] && !binaries[variants[v]].isUsed) {
                        binaries[variants[v]].isUsed = true;
                        usedBinaries++;
                        found = true;
                        break;
                    }
                }
            }
            
            // Расширенный поиск для файлов без расширений
            if (!found && usedFileName.indexOf('.') === -1) {
                var possibleExtensions = ['', '.jpg', '.jpeg', '.png', '.bmp', '.tif', '.tiff', '.gif', '.webp', '.svg', '.eps', '.ico'];
                for (var extIndex = 0; extIndex < possibleExtensions.length; extIndex++) {
                    var testName = usedFileName + possibleExtensions[extIndex];
                    if (binaries[testName] && !binaries[testName].isUsed) {
                        binaries[testName].isUsed = true;
                        usedBinaries++;
                        found = true;
                        break;
                    }
                }
            }
        }
    }
    
    // Обработка обложки
    if (stats.hasCover && stats.coverFileName) {
        var coverFound = false;
        
        if (binaries[stats.coverFileName] && !binaries[stats.coverFileName].isUsed) {
            binaries[stats.coverFileName].isUsed = true;
            if (!usedFiles[stats.coverFileName]) {
                usedBinaries++;
            }
            coverFound = true;
        }
        
        if (!coverFound) {
            var variants = getFileNameVariants(stats.coverFileName);
            for (var v = 0; v < variants.length; v++) {
                if (binaries[variants[v]] && !binaries[variants[v]].isUsed) {
                    binaries[variants[v]].isUsed = true;
                    if (!usedFiles[variants[v]]) {
                        usedBinaries++;
                    }
                    coverFound = true;
                    break;
                }
            }
        }
        
        // Расширенный поиск для обложки
        if (!coverFound && stats.coverFileName.indexOf('.') === -1) {
            var possibleExtensions = ['', '.jpg', '.jpeg', '.png'];
            for (var extIndex = 0; extIndex < possibleExtensions.length; extIndex++) {
                var testName = stats.coverFileName + possibleExtensions[extIndex];
                if (binaries[testName] && !binaries[testName].isUsed) {
                    binaries[testName].isUsed = true;
                    if (!usedFiles[testName]) {
                        usedBinaries++;
                    }
                    coverFound = true;
                    break;
                }
            }
        }
    }
    
    // Подсчет неиспользуемых
    for (var fileName in binaries) {
        if (binaries.hasOwnProperty(fileName)) {
            if (!binaries[fileName].isUsed) {
                unusedBinaries++;
                unusedList.push("  • " + fileName);
            }
        }
    }
    
    // Сортировка (простая пузырьковая для совместимости)
    for (var i = 0; i < unusedList.length - 1; i++) {
        for (var j = i + 1; j < unusedList.length; j++) {
            if (unusedList[i] > unusedList[j]) {
                var temp = unusedList[i];
                unusedList[i] = unusedList[j];
                unusedList[j] = temp;
            }
        }
    }
    
    return {
        usedBinaries: usedBinaries,
        unusedBinaries: unusedBinaries,
        unusedList: unusedList
    };
}

function getFileNameVariants(fileName) {
    var variants = [];
    variants.push(fileName);
    
    if (fileName.charAt(0) !== '_') {
        variants.push('_' + fileName);
    }
    
    if (fileName.charAt(0) === '_') {
        variants.push(fileName.substring(1));
    }
    
    if (fileName.indexOf('i_') !== 0 && fileName.indexOf('_i_') !== 0) {
        variants.push('i_' + fileName.replace(/^_/, ''));
    }
    
    if (fileName.indexOf('i_') === 0) {
        variants.push(fileName.substring(2));
        variants.push('_' + fileName.substring(2));
    }
    
    return variants;
}

function formatFileSize(bytes) {
    if (bytes <= 0) return "0 байт";
    
    if (bytes < 1024 * 1024) {
        var kb = bytes / 1024;
        return Math.round(kb) + " Кб";
    } else {
        var mb = bytes / (1024 * 1024);
        return Math.round(mb * 100) / 100 + " Мб";
    }
}

function findCoverImage(stats) {
    try {
        var tiCover = document.getElementById('tiCover');
        
        if (tiCover) {
            var selects = tiCover.getElementsByTagName('select');
            
            for (var i = 0; i < selects.length; i++) {
                var select = selects[i];
                if (select.id === 'href') {
                    var value = select.value || '';
                    
                    if (value && value !== '') {
                        stats.hasCover = true;
                        
                        if (value.indexOf('#') === 0) {
                            stats.coverFileName = value.substring(1);
                        } else {
                            stats.coverFileName = value;
                        }
                        return;
                    }
                }
            }
        }
        
        var stiCover = document.getElementById('stiCover');
        
        if (stiCover) {
            var selects = stiCover.getElementsByTagName('select');
            
            for (var i = 0; i < selects.length; i++) {
                var select = selects[i];
                if (select.id === 'href') {
                    var value = select.value || '';
                    
                    if (value && value !== '') {
                        stats.hasCover = true;
                        
                        if (value.indexOf('#') === 0) {
                            stats.coverFileName = value.substring(1);
                        } else {
                            stats.coverFileName = value;
                        }
                        return;
                    }
                }
            }
        }
        
        var legends = document.getElementsByTagName('legend');
        
        for (var i = 0; i < legends.length; i++) {
            var legend = legends[i];
            var legendText = legend.textContent || legend.innerText || '';
            
            if (legendText.indexOf('Обложка') !== -1 || 
                legendText.toLowerCase().indexOf('cover') !== -1) {
                
                var parent = legend.parentNode;
                if (parent) {
                    var selects = parent.getElementsByTagName('select');
                    
                    for (var j = 0; j < selects.length; j++) {
                        var select = selects[j];
                        if (select.id === 'href' || select.name === 'href') {
                            var value = select.value || '';
                            
                            if (value && value !== '') {
                                stats.hasCover = true;
                                
                                if (value.indexOf('#') === 0) {
                                    stats.coverFileName = value.substring(1);
                                } else {
                                    stats.coverFileName = value;
                                }
                                return;
                            }
                        }
                    }
                }
            }
        }
        
        var coverElements = document.getElementsByTagName('*');
        
        for (var i = 0; i < coverElements.length; i++) {
            var element = coverElements[i];
            var className = element.className || '';
            
            if (typeof className === 'string' && 
                (className.indexOf('cover') !== -1 || className.indexOf('coverpage') !== -1)) {
                
                var selects = element.getElementsByTagName('select');
                for (var j = 0; j < selects.length; j++) {
                    var select = selects[j];
                    if (select.value && select.value !== '') {
                        stats.hasCover = true;
                        var value = select.value;
                        
                        if (value.indexOf('#') === 0) {
                            stats.coverFileName = value.substring(1);
                        } else {
                            stats.coverFileName = value;
                        }
                        return;
                    }
                }
            }
        }
        
    } catch (e) {
        // Игнорируем ошибки
    }
}
