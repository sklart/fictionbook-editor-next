// Скрипт "Удалить все-все иллюстрации и их бинарники" для редактора FBE
// version 1.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Основано на скриптах: "Удалить все вложения" v1.4 и "Удалить все теги иллюстраций - теги image" v1.6
//  уважаемого тов. Sclex.

// Скрипт предназначен для удаления всех иллюстраций и бинарников в fb2 документе:
// все блочные иллюстрации (DIV с классом image),
// все инлайн иллюстрации (SPAN с классом image),
// все пустые иллюстрации,
// все обложки,
// все бинарные файлы любых иллюстраций, в том числе неиспользуемые (неприлинкованные).

// Можно заменять удаляемые иллюстрации любыми текстовыми маркерами.
// Для блочных и инлайн иллюстраций используются отдельные текстовые маркеры.
// По умолчанию маркеры взамен удаляемых иллюстраций НЕ добавляются!
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.1, 08.02.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Удалить все-все иллюстрации и их бинарники";
    var version = "1.1";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима
    
    // Что удалять:
    var removeInlineImages = 1;    // 1 - удалять инлайн иллюстрации, 0 - нет
    var removeBlockImages = 1;     // 1 - удалять блочные иллюстрации, 0 - нет
    var removeCovers = 1;          // 1 - удалять обложки, 0 - нет
    var removeUnlinkedBinaries = 1; // 1 - удалять неприлинкованные бинарники, 0 - нет
    
    // Создавать пользовательские маркеры взамен удаленных БЛОЧНЫХ иллюстраций
    var processBlockMarkers = 0;        // 0 - нет, 1 - да

    // Создавать пользовательские маркеры взамен удаленных ИНЛАЙН иллюстраций
    var processInlineMarkers = 1;        // 0 - нет, 1 - да
    
    // Маркеры для разных типов иллюстраций:
    var InlinePicMarker = "ZZZ_INLINE_PIC";      // Маркер для инлайн иллюстраций
    var BlockPicMarker = "ZZZ_BLOCK_PIC";        // Маркер для блочных иллюстраций
    
    // Дополнительные настройки:
    var removeEmptyLines = 0;      // 1 - удалять пустые строки вокруг блочных иллюстраций, 0 - нет
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    try {
        // Создаем объект для статистики
        var stats = {
            blockImages: 0,        // Найдено блочных иллюстраций
            inlineImages: 0,       // Найдено инлайн иллюстраций
            blockBinaries: 0,      // Бинарников, используемых в блочных картинках
            inlineBinaries: 0,     // Бинарников, используемых в инлайн картинках
            coverBinaries: 0,      // Бинарников-обложек
            unlinkedBinaries: 0,   // Неприлинкованных бинарников
            totalBinaries: 0,      // Всего бинарных файлов
            deletedBinaries: 0,    // Удалено бинарных файлов
            markersAdded: 0,       // Добавлено маркеров
            emptyLinesRemoved: 0,  // Удалено пустых строк
            blockRemoved: 0,       // Удалено блочных иллюстраций
            inlineRemoved: 0,      // Удалено инлайн иллюстраций
            coversRemoved: 0       // Удалено обложек
        };
        
        // Объекты для хранения ID бинарников
        var binaryIdsToDelete = {}; // ID бинарников, которые нужно удалить
        var coverBinaryIds = {};    // ID бинарников-обложек
        var allBinaryIds = {};      // ID ВСЕХ бинарников (для анализа неприлинкованных)
        
        // Получаем символ неразрывного пробела из настроек FBE
        var nbspChar, nbspEntity;
        try { 
            nbspChar = window.external.GetNBSP(); 
            if (nbspChar.charCodeAt(0) == 160) {
                nbspEntity = "&nbsp;";
            } else {
                nbspEntity = nbspChar;
            }
        } catch(e) { 
            nbspChar = String.fromCharCode(160); 
            nbspEntity = "&nbsp;";
        }
        
        // Регулярное выражение для проверки пустых строк
        var reEmptyLine = new RegExp("^( | |&nbsp;|" + nbspChar + ")*?$", "i");
        
        // Функция проверки, пустая ли строка
        function isLineEmpty(ptr) {
            return reEmptyLine.test(ptr.innerHTML.replace(/<(?!img)[^>]*?>/gi, ""));
        }
        
        // Функция проверки, нужна ли пустая строка для валидности документа
        function emptyLineIsNeededForValidity(elem) {
            if (!elem.previousSibling || 
                (elem.previousSibling.nodeName == "DIV" &&
                 (elem.previousSibling.className == "epigraph" || 
                  elem.previousSibling.className == "title") &&
                 elem.nextSibling && 
                 elem.nextSibling.nodeName == "P" && 
                 isLineEmpty(elem.nextSibling) &&
                 elem.nextSibling.nextSibling == null)) {
                return true;
            } else {
                return false;
            }
        }
        
        // 1. Анализируем документ перед удалением и собираем ID бинарников
        analyzeDocument(stats, binaryIdsToDelete, coverBinaryIds, allBinaryIds);
        
        // 2. Определяем неприлинкованные бинарники
        if (removeUnlinkedBinaries) {
            // Для всех бинарников в документе
            for (var id in allBinaryIds) {
                if (allBinaryIds.hasOwnProperty(id)) {
                    // Если бинарник НЕ используется в иллюстрациях и НЕ является обложкой
                    if (!binaryIdsToDelete[id] && !coverBinaryIds[id]) {
                        stats.unlinkedBinaries++;
                        // Добавляем в список на удаление
                        binaryIdsToDelete[id] = true;
                    }
                }
            }
        }
        
        // Проверяем, есть ли что удалять согласно настройкам
        var hasImagesToRemove = false;
        if (removeBlockImages && stats.blockImages > 0) hasImagesToRemove = true;
        if (removeInlineImages && stats.inlineImages > 0) hasImagesToRemove = true;
        if (removeCovers && stats.coverBinaries > 0) hasImagesToRemove = true;
        if (removeUnlinkedBinaries && stats.unlinkedBinaries > 0) hasImagesToRemove = true;
        
        if (!hasImagesToRemove) {
            var message = scriptName + "\nver. " + version + "\n\n";
            
            if (!removeBlockImages && !removeInlineImages && !removeCovers && !removeUnlinkedBinaries) {
                message += "В настройках отключено удаление блочных, инлайн иллюстраций, обложек и неприлинкованных бинарников.\n" +
                          "Измените настройки скрипта.";
            } else {
                message += "Не найдено объектов для удаления согласно текущим настройкам.";
                if (stats.totalBinaries > 0) {
                    message += "\n\nОбщая информация:\n";
                    message += "• Всего бинарных файлов: " + stats.totalBinaries + "\n";
                    if (stats.blockImages > 0) message += "• Блочных иллюстраций: " + stats.blockImages + "\n";
                    if (stats.inlineImages > 0) message += "• Инлайн иллюстраций: " + stats.inlineImages + "\n";
                    if (stats.coverBinaries > 0) message += "• Бинарников обложек: " + stats.coverBinaries + "\n";
                    if (stats.unlinkedBinaries > 0) message += "• Неприлинкованных бинарников: " + stats.unlinkedBinaries + "\n";
                }
            }
            
            if (showStatistics) {
                MsgBox(message);
            }
            return;
        }
        
        // 3. Показываем статистику анализа (если не тихий режим)
        if (showStatistics) {
            // Считаем сколько бинарников будет удалено
            var willDeleteCount = 0;
            for (var id in binaryIdsToDelete) {
                if (binaryIdsToDelete.hasOwnProperty(id)) {
                    willDeleteCount++;
                }
            }
            
            var analysisMessage = scriptName + "\n" +
                                 "ver. " + version + "\n\n" +
                                 "АНАЛИЗ ДОКУМЕНТА:\n\n";
            
            if (stats.blockImages > 0) {
                analysisMessage += "• Найдено блочных иллюстраций: " + stats.blockImages + "\n";
            }
            
            if (stats.inlineImages > 0) {
                analysisMessage += "• Найдено инлайн иллюстраций: " + stats.inlineImages + "\n";
            }
            
            if (stats.coverBinaries > 0) {
                analysisMessage += "• Найдено бинарников обложек: " + stats.coverBinaries + "\n";
            }
            
            if (stats.unlinkedBinaries > 0) {
                analysisMessage += "• Найдено неприлинкованных бинарников: " + stats.unlinkedBinaries + "\n";
            }
            
            analysisMessage += "\n✓ Всего бинарных файлов в документе: " + stats.totalBinaries + "\n";
            
            // Считаем сколько бинарников будет удалено с учетом всех настроек
            var totalToDelete = willDeleteCount;
            if (removeCovers && !removeUnlinkedBinaries) {
                // Если удаляем обложки, но не неприлинкованные, нужно добавить обложки отдельно
                totalToDelete = 0;
                for (var id in allBinaryIds) {
                    if (allBinaryIds.hasOwnProperty(id)) {
                        if (binaryIdsToDelete[id] || (removeCovers && coverBinaryIds[id])) {
                            totalToDelete++;
                        }
                    }
                }
            }
            
            if (totalToDelete > 0) {
                analysisMessage += "✓ Будет удалено бинарников: " + totalToDelete;
                
                var details = [];
                if (removeBlockImages && stats.blockBinaries > 0) details.push(stats.blockBinaries + " блочных");
                if (removeInlineImages && stats.inlineBinaries > 0) details.push(stats.inlineBinaries + " инлайн");
                if (removeCovers && stats.coverBinaries > 0) details.push(stats.coverBinaries + " обложек");
                if (removeUnlinkedBinaries && stats.unlinkedBinaries > 0) details.push(stats.unlinkedBinaries + " неприлинкованных");
                
                if (details.length > 0) {
                    analysisMessage += " (" + details.join(" + ") + ")";
                }
                analysisMessage += "\n";
            }
            
            analysisMessage += "\nПараметры обработки:\n" +
                               "• Удалять блочные иллюстрации: " + (removeBlockImages ? "ДА" : "НЕТ") + "\n" +
                               "• Удалять инлайн иллюстрации: " + (removeInlineImages ? "ДА" : "НЕТ") + "\n" +
                               "• Удалять обложки: " + (removeCovers ? "ДА" : "НЕТ") + "\n" +
                               "• Удалять неприлинкованные бинарники: " + (removeUnlinkedBinaries ? "ДА" : "НЕТ") + "\n" +
                               "• Добавлять маркеры для блочных: " + (processBlockMarkers ? "ДА" : "НЕТ");
            
            if (processBlockMarkers) {
                analysisMessage += " (" + BlockPicMarker + ")\n";
            } else {
                analysisMessage += "\n";
            }
            
            analysisMessage += "• Добавлять маркеры для инлайн: " + (processInlineMarkers ? "ДА" : "НЕТ");
            
            if (processInlineMarkers) {
                analysisMessage += " (" + InlinePicMarker + ")\n";
            } else {
                analysisMessage += "\n";
            }
            
            if (removeBlockImages) {
                analysisMessage += "• Удалять пустые строки: " + (removeEmptyLines ? "ДА" : "НЕТ") + "\n";
            }
            
            analysisMessage += "\nВНИМАНИЕ: Этот скрипт удаляет ВСЕ иллюстрации и бинарники согласно настройкам!\n" +
                               "Выполнить удаление?";
            
            var userConfirmed = AskYesNo(analysisMessage);
            
            // ТАЙМЕР ЗАПУСКАЕМ ТОЛЬКО ПОСЛЕ ПОСЛЕДНЕГО CONFIRM!
            var startTime = new Date().getTime();
            
            if (!userConfirmed) {
                return; // Пользователь отказался
            }
        } else {
            // В тихом режиме сразу запускаем таймер
            var startTime = new Date().getTime();
        }
        
        // 4. Выполняем удаление с правильной работой Undo
        var firstRemoving = true;
        var undoUnitName = "Полное удаление иллюстраций и бинарников";
        
        // ФАЗА 1: Удаляем иллюстрации согласно настройкам
        if (removeInlineImages && stats.inlineImages > 0) {
            firstRemoving = removeInlineImagesFunc(stats, processInlineMarkers, InlinePicMarker, firstRemoving, undoUnitName);
        }
        
        if (removeBlockImages && stats.blockImages > 0) {
            firstRemoving = removeBlockImagesFunc(stats, processBlockMarkers, BlockPicMarker, removeEmptyLines, isLineEmpty, emptyLineIsNeededForValidity, firstRemoving, undoUnitName);
        }
        
        // ФАЗА 2: Удаляем обложки (если включено)
        if (removeCovers && stats.coverBinaries > 0) {
            firstRemoving = removeCoversFunc(stats, firstRemoving, undoUnitName);
            
            // Добавляем ID обложек в список на удаление
            for (var id in coverBinaryIds) {
                if (coverBinaryIds.hasOwnProperty(id)) {
                    binaryIdsToDelete[id] = true;
                }
            }
        }
        
        // ФАЗА 3: Удаляем ВСЕ соответствующие бинарники
        if (stats.totalBinaries > 0) {
            firstRemoving = removeAllBinaries(binaryIdsToDelete, stats, firstRemoving, undoUnitName, removeUnlinkedBinaries, coverBinaryIds, removeCovers);
        }
        
        // Если что-то удаляли - закрываем транзакцию Undo
        if (!firstRemoving) {
            window.external.EndUndoUnit(document);
        }
        
        // 5. Обновляем списки в FBE
        try {
            if (typeof FillCoverList === "function") {
                FillCoverList();
            }
        } catch (e) {
            // Игнорируем ошибки
        }
        
        // 6. Выводим результаты (если не тихий режим)
        if (showStatistics) {
            var endTime = new Date().getTime();
            var executionTime = (endTime - startTime) / 1000;
            
            var timeStr;
            if (executionTime < 0.001) {
                timeStr = "0,001";
            } else {
                // Форматируем время с запятой
                timeStr = executionTime.toFixed(3);
                timeStr = timeStr.replace(".", ",");
            }
            
            var resultMessage = scriptName + "\n" +
                               "ver. " + version + "\n\n" +
                               "РЕЗУЛЬТАТЫ УДАЛЕНИЯ:\n\n";
            
            var totalImagesRemoved = stats.blockRemoved + stats.inlineRemoved;
            
            if (totalImagesRemoved > 0) {
                resultMessage += "✓ Удалено иллюстраций: " + totalImagesRemoved + "\n";
            }
            
            if (stats.blockRemoved > 0) {
                resultMessage += "  - блочных: " + stats.blockRemoved + "\n";
            }
            
            if (stats.inlineRemoved > 0) {
                resultMessage += "  - инлайн: " + stats.inlineRemoved + "\n";
            }
            
            if (removeCovers && stats.coversRemoved > 0) {
                resultMessage += "✓ Удалено обложек: " + stats.coversRemoved + "\n";
            }
            
            if (stats.deletedBinaries > 0) {
                resultMessage += "✓ Удалено бинарных файлов: " + stats.deletedBinaries + "\n";
                
                var details = [];
                if (stats.blockBinaries > 0) details.push(stats.blockBinaries + " блочных");
                if (stats.inlineBinaries > 0) details.push(stats.inlineBinaries + " инлайн");
                if (removeCovers && stats.coverBinaries > 0) details.push(stats.coverBinaries + " обложек");
                if (removeUnlinkedBinaries && stats.unlinkedBinaries > 0) details.push(stats.unlinkedBinaries + " неприлинкованных");
                
                if (details.length > 0) {
                    resultMessage += "  (" + details.join(" + ") + ")\n";
                }
            }
            
            if ((processBlockMarkers || processInlineMarkers) && stats.markersAdded > 0) {
                resultMessage += "✓ Добавлено маркеров: " + stats.markersAdded + "\n";
            }
            
            if (removeEmptyLines && stats.emptyLinesRemoved > 0) {
                resultMessage += "✓ Удалено пустых строк: " + stats.emptyLinesRemoved + "\n";
            }
            
            if (stats.totalBinaries > 0 && stats.deletedBinaries === stats.totalBinaries) {
                resultMessage += "\n• Все бинарные файлы удалены из документа!\n";
            } else if (stats.deletedBinaries > 0) {
                resultMessage += "\n• Удаление завершено согласно настройкам.\n";
            }
            
            resultMessage += "\n---------------------------------------\n" +
                            "Время обработки: " + timeStr + " сек";
            
            MsgBox(resultMessage);
        }
        
    } catch (error) {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
              "Ошибка выполнения скрипта:\n" + error.message);
    }
}

// ==================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ==================================================

// Анализ документа перед удалением и сбор ID бинарников
function analyzeDocument(stats, binaryIdsToDelete, coverBinaryIds, allBinaryIds) {
    // Считаем бинарники
    var binobj = document.all.binobj;
    if (binobj) {
        var bins = binobj.getElementsByTagName("DIV");
        stats.totalBinaries = bins.length;
        
        // Запоминаем ВСЕ бинарники
        for (var i = 0; i < bins.length; i++) {
            var bin = bins[i];
            var id = "";
            try {
                if (bin.all && bin.all.id) {
                    id = bin.all.id.value || "";
                }
            } catch(e) {
                // Игнорируем ошибки
            }
            
            if (id && id !== "") {
                allBinaryIds[id] = true;
            }
        }
        
        // Ищем все иллюстрации и собираем ID используемых бинарников
        var fbwBody = document.getElementById("fbw_body");
        if (fbwBody) {
            // Ищем DIV с классом image (блочные)
            var divs = fbwBody.getElementsByTagName("DIV");
            for (var i = 0; i < divs.length; i++) {
                var div = divs[i];
                if (div.className && div.className.toLowerCase() == "image") {
                    stats.blockImages++;
                    
                    // Определяем ID бинарника
                    var href = div.getAttribute("href") || "";
                    if (href && href !== "#undefined") {
                        var fileName = href.replace(/^#/, '');
                        if (fileName && fileName !== "undefined") {
                            stats.blockBinaries++;
                            binaryIdsToDelete[fileName] = true;
                        }
                    }
                }
            }
            
            // Ищем SPAN с классом image (инлайн)
            var spans = fbwBody.getElementsByTagName("SPAN");
            for (var i = 0; i < spans.length; i++) {
                var span = spans[i];
                if (span.className && span.className.toLowerCase() == "image") {
                    stats.inlineImages++;
                    
                    // Определяем ID бинарника
                    var href = span.getAttribute("href") || "";
                    if (href && href !== "#undefined") {
                        var fileName = href.replace(/^#/, '');
                        if (fileName && fileName !== "undefined") {
                            stats.inlineBinaries++;
                            binaryIdsToDelete[fileName] = true;
                        }
                    }
                }
            }
        }
        
        // Определяем бинарники-обложки
        try {
            var tiCover = document.getElementById('tiCover');
            if (tiCover) {
                var selects = tiCover.getElementsByTagName('select');
                for (var i = 0; i < selects.length; i++) {
                    var select = selects[i];
                    if (select.id === 'href' || select.name === 'href') {
                        var value = select.value || '';
                        if (value && value !== '') {
                            var fileName = value.replace(/^#/, '');
                            if (fileName && fileName !== 'undefined') {
                                stats.coverBinaries++;
                                coverBinaryIds[fileName] = true;
                            }
                        }
                    }
                }
            }
            
            var stiCover = document.getElementById('stiCover');
            if (stiCover) {
                var selects = stiCover.getElementsByTagName('select');
                for (var i = 0; i < selects.length; i++) {
                    var select = selects[i];
                    if (select.id === 'href' || select.name === 'href') {
                        var value = select.value || '';
                        if (value && value !== '') {
                            var fileName = value.replace(/^#/, '');
                            if (fileName && fileName !== 'undefined') {
                                stats.coverBinaries++;
                                coverBinaryIds[fileName] = true;
                            }
                        }
                    }
                }
            }
        } catch (e) {
            // Игнорируем ошибки
        }
    }
}

// Удаление инлайн иллюстраций
function removeInlineImagesFunc(stats, processInlineMarkers, markerText, firstRemoving, undoUnitName) {
    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) return firstRemoving;
    
    // Ищем все SPAN с классом image
    var spans = fbwBody.getElementsByTagName("SPAN");
    
    // Обрабатываем в обратном порядке (для стабильности DOM)
    for (var i = spans.length - 1; i >= 0; i--) {
        var span = spans[i];
        if (span.className && span.className.toLowerCase() == "image") {
            // Создаем UndoUnit при первом удалении (как в скрипте Sclex)
            if (firstRemoving) {
                window.external.BeginUndoUnit(document, undoUnitName);
                firstRemoving = false;
            }
            
            if (processInlineMarkers) {
                // Заменяем на маркер
                var parent = span.parentNode;
                if (parent) {
                    var markerNode = document.createTextNode(markerText);
                    parent.replaceChild(markerNode, span);
                    stats.markersAdded++;
                    stats.inlineRemoved++;
                }
            } else {
                // Просто удаляем
                span.removeNode(true);
                stats.inlineRemoved++;
            }
        }
    }
    
    return firstRemoving;
}

// Удаление блочных иллюстраций
function removeBlockImagesFunc(stats, processBlockMarkers, markerText, removeEmptyLines, isLineEmpty, emptyLineIsNeededForValidity, firstRemoving, undoUnitName) {
    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) return firstRemoving;
    
    // Ищем все DIV с классом image
    var divs = fbwBody.getElementsByTagName("DIV");
    var emptyLines = [];
    
    // Сначала собираем информацию о пустых строк
    if (removeEmptyLines) {
        for (var i = divs.length - 1; i >= 0; i--) {
            var div = divs[i];
            if (div.className && div.className.toLowerCase() == "image") {
                // Проверяем пустые строки вокруг иллюстрации
                if (div.nextSibling && 
                    div.nextSibling.nodeName == "P" && 
                    isLineEmpty(div.nextSibling)) {
                    if (!emptyLineIsNeededForValidity(div)) {
                        if (emptyLines.length == 0 || emptyLines[emptyLines.length - 1] !== div.nextSibling) {
                            emptyLines.push(div.nextSibling);
                        }
                    }
                }
                
                if (div.previousSibling && 
                    div.previousSibling.nodeName == "P" && 
                    isLineEmpty(div.previousSibling)) {
                    if (emptyLines.length == 0 || emptyLines[emptyLines.length - 1] != div.previousSibling) {
                        emptyLines.push(div.previousSibling);
                    }
                }
            }
        }
    }
    
    // Теперь удаляем блочные иллюстрации
    for (var i = divs.length - 1; i >= 0; i--) {
        var div = divs[i];
        if (div.className && div.className.toLowerCase() == "image") {
            // Создаем UndoUnit при первом удалении (как в скрипте Sclex)
            if (firstRemoving) {
                window.external.BeginUndoUnit(document, undoUnitName);
                firstRemoving = false;
            }
            
            if (processBlockMarkers) {
                // Заменяем на маркер (создаем новый P с маркером)
                var parent = div.parentNode;
                if (parent) {
                    var markerParagraph = document.createElement("P");
                    markerParagraph.innerHTML = markerText;
                    parent.replaceChild(markerParagraph, div);
                    stats.markersAdded++;
                    stats.blockRemoved++;
                }
            } else {
                // Просто удаляем
                div.removeNode(true);
                stats.blockRemoved++;
            }
        }
    }
    
    // Удаляем пустые строки (если включено)
    if (removeEmptyLines && emptyLines.length > 0) {
        for (var i = 0; i < emptyLines.length; i++) {
            try {
                emptyLines[i].removeNode(true);
                stats.emptyLinesRemoved++;
            } catch (e) {
                // Игнорируем ошибки
            }
        }
    }
    
    return firstRemoving;
}

// Удаление обложек
function removeCoversFunc(stats, firstRemoving, undoUnitName) {
    try {
        var tiCover = document.getElementById('tiCover');
        if (tiCover) {
            var selects = tiCover.getElementsByTagName('select');
            for (var i = 0; i < selects.length; i++) {
                var select = selects[i];
                if (select.id === 'href' || select.name === 'href') {
                    var value = select.value || '';
                    if (value && value !== '') {
                        // Создаем UndoUnit при первом удалении
                        if (firstRemoving) {
                            window.external.BeginUndoUnit(document, undoUnitName);
                            firstRemoving = false;
                        }
                        
                        // Очищаем выбор обложки
                        select.selectedIndex = -1;
                        stats.coversRemoved++;
                    }
                }
            }
        }
        
        var stiCover = document.getElementById('stiCover');
        if (stiCover) {
            var selects = stiCover.getElementsByTagName('select');
            for (var i = 0; i < selects.length; i++) {
                var select = selects[i];
                if (select.id === 'href' || select.name === 'href') {
                    var value = select.value || '';
                    if (value && value !== '') {
                        // Создаем UndoUnit при первом удалении (если еще не создали)
                        if (firstRemoving) {
                            window.external.BeginUndoUnit(document, undoUnitName);
                            firstRemoving = false;
                        }
                        
                        // Очищаем выбор обложки
                        select.selectedIndex = -1;
                        stats.coversRemoved++;
                    }
                }
            }
        }
    } catch (e) {
        // Игнорируем ошибки
    }
    
    return firstRemoving;
}

// Удаление ВСЕХ бинарников (включая неприлинкованные и обложки)
function removeAllBinaries(binaryIdsToDelete, stats, firstRemoving, undoUnitName, removeUnlinkedBinaries, coverBinaryIds, removeCovers) {
    var binobj = document.all.binobj;
    if (!binobj) return firstRemoving;
    
    // Проверяем, есть ли бинарники
    if (stats.totalBinaries === 0) {
        return firstRemoving;
    }
    
    // Создаем UndoUnit при первом удалении
    if (firstRemoving) {
        window.external.BeginUndoUnit(document, undoUnitName);
        firstRemoving = false;
    }
    
    // Метод удаления бинарников (как в скрипте "Удалить все вложения", но с фильтрацией)
    var ptr = binobj.firstChild;
    var GoMore = true;
    var HtmlStr = "";
    var deletedCount = 0;
    
    while (GoMore) {
        if (ptr) {
            if (ptr.nodeName != "DIV") { 
                // Сохраняем не-DIV элементы (спейсеры и т.д.)
                HtmlStr += ptr.outerHTML; 
                ptr = ptr.nextSibling; 
            } else {
                // Это DIV бинарника
                var id = "";
                try {
                    if (ptr.all && ptr.all.id) {
                        id = ptr.all.id.value || "";
                    }
                } catch(e) {
                    // Игнорируем ошибки
                }
                
                // Проверяем, нужно ли удалять этот бинарник
                var shouldDelete = false;
                
                if (!id || id === "") {
                    // Бинарник без ID - всегда удаляем (скорее всего, это мусор)
                    shouldDelete = true;
                } else if (binaryIdsToDelete[id]) {
                    // В списке на удаление
                    shouldDelete = true;
                } else if (removeCovers && coverBinaryIds[id]) {
                    // Обложка и включено удаление обложек
                    shouldDelete = true;
                } else if (removeUnlinkedBinaries) {
                    // Включено удаление неприлинкованных бинарников
                    // Если бинарник не используется и не обложка - удаляем
                    shouldDelete = true;
                }
                
                if (shouldDelete) {
                    deletedCount++;
                } else {
                    // Сохраняем бинарник
                    HtmlStr += ptr.outerHTML;
                }
                
                ptr = ptr.nextSibling;
            }
        } else {
            GoMore = false;
        }
    }
    
    // Заменяем содержимое binobj
    binobj.innerHTML = HtmlStr;
    stats.deletedBinaries = deletedCount;
    
    // ОБЯЗАТЕЛЬНО вызываем PutSpacers для обновления внутренних структур FBE
    try {
        if (typeof PutSpacers === "function") {
            PutSpacers(binobj);
        }
    } catch (e) {
        // Игнорируем ошибки
    }
    
    // Обновляем изображения
    try {
        var imgs = document.getElementsByTagName("IMG");
        for (var i = imgs.length - 1; i >= 0; i--) {
            var MyImg = imgs[i];
            var pic_id = MyImg.src; 
            MyImg.src = ""; 
            MyImg.src = pic_id;
        }
    } catch (e) {
        // Игнорируем ошибки
    }
    
    return firstRemoving;
}
