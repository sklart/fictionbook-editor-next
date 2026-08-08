// Скрипт "Удалить все инлайн иллюстрации и их бинарники" для редактора FBE
// version 1.4
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Основано на скриптах: "Удалить все вложения" v1.4 и "Удалить все теги иллюстраций - теги image" v1.6
//  уважаемого тов. Sclex.

// Скрипт предназначен для удаления только инлайн иллюстраций (SPAN с классом image)
// и их бинарных файлов в fb2 документе.
// Блочные иллюстрации (DIV с классом image) НЕ удаляются и НЕ затрагиваются!
// Обложки и их бинарники НЕ удаляются.
// Можно заменять удаленные инлайн иллюстрации любыми текстовыми маркерами.
// По умолчанию такие маркеры взамен удаляемых инлайн иллюстраций - ZZZ_INLINE_PIC - добавляются!
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.4, 08.02.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Удалить все инлайн иллюстрации и их бинарники";
    var version = "1.4";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима
    
    // Создавать пользовательские маркеры взамен удаленных инлайн иллюстраций
    var processMarkers = 1;        // 0 - нет, 1 - да
       
    // Пользовательский маркер удаленных инлайн иллюстраций:
    var InlinePicMarker = "ZZZ_INLINE_PIC";      // Тут можно задать любой маркер
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    try {
        // Создаем объект для статистики
        var stats = {
            inlineImages: 0,       // Найдено инлайн иллюстраций
            blockImages: 0,        // Найдено блочных иллюстраций (для информации)
            markersAdded: 0,       // Добавлено маркеров
            inlineBinaries: 0,     // Бинарников, используемых в инлайн картинках
            blockBinaries: 0,      // Бинарников, используемых в блочных картинках
            coverBinaries: 0,      // Бинарников-обложек
            totalBinaries: 0,      // Всего бинарных файлов
            deletedBinaries: 0,    // Удалено бинарных файлов
            inlineRemoved: 0       // Удалено инлайн иллюстраций
        };
        
        // Объекты для хранения ID бинарников
        var binaryIdsToDelete = {}; // ID бинарников, которые нужно удалить
        var coverBinaryIds = {};    // ID бинарников-обложек (не удалять)
        var blockBinaryIds = {};    // ID бинарников блочных иллюстраций (не удалять и не портить!)
        
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
        
        // 1. Анализируем документ перед удалением и собираем ID бинарников
        analyzeDocument(stats, binaryIdsToDelete, coverBinaryIds, blockBinaryIds);
        
        // Если нет инлайн иллюстраций для обработки
        if (stats.inlineImages === 0) {
            if (showStatistics) {
                MsgBox(scriptName + "\nver. " + version + "\n\n" +
                      "Не найдено инлайн-иллюстраций.");
            }
            return;
        }
        
        // 2. Показываем статистику анализа (если не тихий режим)
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
                                 "АНАЛИЗ ДОКУМЕНТА:\n\n" +
                                 "• Найдено инлайн иллюстраций: " + stats.inlineImages + "\n";
            
            if (stats.blockImages > 0) {
                analysisMessage += "• Найдено блочных иллюстраций: " + stats.blockImages + " (не удаляются и не затрагиваются!)\n";
            }
            
            analysisMessage += "\n✓ Будет удалено бинарников: " + willDeleteCount + "\n";
            
            if (stats.coverBinaries > 0) {
                analysisMessage += "✓ Бинарников обложек: " + stats.coverBinaries + " (не удаляются)\n";
            }
            
            if (stats.blockBinaries > 0) {
                analysisMessage += "✓ Бинарников блочных иллюстраций: " + stats.blockBinaries + " (не удаляются и не затрагиваются!)\n";
            }
            
            analysisMessage += "\n• Всего бинарных файлов в документе: " + stats.totalBinaries + "\n" +
                               "\nПараметры обработки:\n" +
                               "• Добавлять маркеры: " + (processMarkers ? "ДА" : "НЕТ");
            
            if (processMarkers) {
                analysisMessage += " (" + InlinePicMarker + ")\n";
            } else {
                analysisMessage += "\n";
            }
            
            analysisMessage += "\nВыполнить удаление только инлайн иллюстраций и их бинарников?";
            
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
        
        // 3. Выполняем удаление с правильной работой Undo
        var firstRemoving = true;
        var undoUnitName = "Удаление инлайн иллюстраций";
        
        // ФАЗА 1: Удаляем инлайн иллюстрации (с маркерами)
        if (stats.inlineImages > 0) {
            firstRemoving = removeInlineImagesFunc(stats, processMarkers, InlinePicMarker, firstRemoving, undoUnitName);
        }
        
        // ФАЗА 2: Удаляем соответствующие бинарники (ПРАВИЛЬНЫМ методом)
        if (stats.inlineBinaries > 0) {
            firstRemoving = removeCorrespondingBinaries(binaryIdsToDelete, coverBinaryIds, blockBinaryIds, stats, firstRemoving, undoUnitName);
        }
        
        // Если что-то удаляли - закрываем транзакцию Undo
        if (!firstRemoving) {
            window.external.EndUndoUnit(document);
        }
        
        // 4. Обновляем списки в FBE
        try {
            if (typeof FillCoverList === "function") {
                FillCoverList();
            }
        } catch (e) {
            // Игнорируем ошибки
        }
        
        // 5. Выводим результаты (если не тихий режим)
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
                               "РЕЗУЛЬТАТЫ УДАЛЕНИЯ:\n\n" +
                               "✓ Удалено инлайн иллюстраций: " + stats.inlineRemoved + "\n" +
                               "✓ Удалено бинарных файлов: " + stats.deletedBinaries + "\n";
            
            if (processMarkers && stats.markersAdded > 0) {
                resultMessage += "✓ Добавлено маркеров: " + stats.markersAdded + "\n";
            }
            
            if (stats.blockImages > 0) {
                resultMessage += "\n• Блочных иллюстраций не тронуто: " + stats.blockImages + "\n";
            }
            
            if (stats.coverBinaries > 0) {
                resultMessage += "• Обложки сохранены: " + stats.coverBinaries + " шт.\n";
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
function analyzeDocument(stats, binaryIdsToDelete, coverBinaryIds, blockBinaryIds) {
    // Считаем бинарники
    var binobj = document.all.binobj;
    if (binobj) {
        var bins = binobj.getElementsByTagName("DIV");
        stats.totalBinaries = bins.length;
        
        // Ищем все иллюстрации и собираем ID используемых бинарников
        var fbwBody = document.getElementById("fbw_body");
        if (fbwBody) {
            // Ищем SPAN с классом image (инлайн) и собираем их ID
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
            
            // Ищем DIV с классом image (блочные) - собираем их ID чтобы НЕ удалять и НЕ портить!
            var divs = fbwBody.getElementsByTagName("DIV");
            for (var i = 0; i < divs.length; i++) {
                var div = divs[i];
                if (div.className && div.className.toLowerCase() == "image") {
                    stats.blockImages++;
                    
                    // Определяем ID бинарника блочной иллюстрации
                    var href = div.getAttribute("href") || "";
                    if (href && href !== "#undefined") {
                        var fileName = href.replace(/^#/, '');
                        if (fileName && fileName !== "undefined") {
                            stats.blockBinaries++;
                            blockBinaryIds[fileName] = true;
                            // Удаляем из списка на удаление, если был добавлен
                            delete binaryIdsToDelete[fileName];
                        }
                    }
                }
            }
        }
        
        // Определяем бинарники-обложки (их не удаляем)
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
                                // Удаляем из списка на удаление, если был добавлен
                                delete binaryIdsToDelete[fileName];
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
                                // Удаляем из списка на удаление, если был добавлен
                                delete binaryIdsToDelete[fileName];
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

// Удаление инлайн иллюстраций (с возможностью добавления маркеров)
function removeInlineImagesFunc(stats, processMarkers, markerText, firstRemoving, undoUnitName) {
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
            
            if (processMarkers) {
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

// Удаление бинарников по сохраненным ID (ИСПРАВЛЕННЫЙ метод!)
function removeCorrespondingBinaries(binaryIdsToDelete, coverBinaryIds, blockBinaryIds, stats, firstRemoving, undoUnitName) {
    var binobj = document.all.binobj;
    if (!binobj) return firstRemoving;
    
    // Проверяем, есть ли что удалять
    var hasBinariesToDelete = false;
    for (var id in binaryIdsToDelete) {
        if (binaryIdsToDelete.hasOwnProperty(id)) {
            hasBinariesToDelete = true;
            break;
        }
    }
    
    if (!hasBinariesToDelete) {
        return firstRemoving;
    }
    
    // Создаем UndoUnit при первом удалении
    if (firstRemoving) {
        window.external.BeginUndoUnit(document, undoUnitName);
        firstRemoving = false;
    }
    
    // ПРАВИЛЬНЫЙ метод удаления бинарников (как в скрипте "Удалить все вложения")
    var bins = binobj.getElementsByTagName("DIV");
    var binsToDelete = [];
    
    // Собираем DIV бинарников для удаления
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
        
        if (id) {
            // Проверяем, нужно ли удалять этот бинарник
            if (binaryIdsToDelete[id] && !coverBinaryIds[id] && !blockBinaryIds[id]) {
                binsToDelete.push(bin);
            }
        }
    }
    
    // Удаляем в обратном порядке (стабильность DOM)
    stats.deletedBinaries = 0;
    for (var i = binsToDelete.length - 1; i >= 0; i--) {
        try {
            // Используем правильное удаление через removeChild
            var parent = binsToDelete[i].parentNode;
            if (parent) {
                parent.removeChild(binsToDelete[i]);
                stats.deletedBinaries++;
            }
        } catch (e) {
            // Игнорируем ошибки
        }
    }
    
    // ОБЯЗАТЕЛЬНО вызываем PutSpacers для обновления внутренних структур FBE
    try {
        if (typeof PutSpacers === "function") {
            PutSpacers(binobj);
        }
    } catch (e) {
        // Игнорируем ошибки
    }
    
    // Обновляем изображения (важно для перезагрузки оставшихся)
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
