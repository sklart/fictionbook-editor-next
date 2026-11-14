// Скрипт "Удалить все блочные иллюстрации и их бинарники" для редактора FBE
// version 2.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Основано на скриптах: "Удалить все вложения" v1.4 и "Удалить все теги иллюстраций - теги image" v1.6
// уважаемого тов. Sclex

// Скрипт предназначен для удаления только блочных иллюстраций (DIV с классом image) 
// и их бинарных файлов в fb2 документе.
// Инлайн иллюстрации (SPAN с классом image) НЕ удаляются и НЕ затрагиваются!
// Обложки и их бинарники НЕ удаляются.
// Можно заменять удаленные блочные иллюстрации любыми текстовыми маркерами.
// По умолчанию маркеры взамен удаляемых иллюстраций НЕ добавляются!
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 2.2, 08.02.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Удалить все блочные иллюстрации и их бинарники";
    var version = "2.2";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима
    
    // Создавать пользовательские маркеры взамен удаленных блочных иллюстраций
    var processMarkers = 1;        // 0 - нет, 1 - да (по умолчанию ВЫКЛЮЧЕНО!)
    
    // Пользовательский маркер удаленных блочных иллюстраций:
    var BlockPicMarker = "ZZZ_BLOCK_PIC";      // Тут можно задать любой маркер
    
    // Удалять пустые строки, которые остаются после удаления блочных иллюстраций
    var removeEmptyLines = 0; // 1 - да, 0 - нет
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    try {
        // Создаем объект для статистики
        var stats = {
            blockImages: 0,        // Найдено блочных иллюстраций
            inlineImages: 0,       // Найдено инлайн иллюстраций (для информации)
            markersAdded: 0,       // Добавлено маркеров
            blockBinaries: 0,      // Бинарников, используемых в блочных картинках
            inlineBinaries: 0,     // Бинарников, используемых в инлайн картинках (для информации)
            coverBinaries: 0,      // Бинарников-обложек
            totalBinaries: 0,      // Всего бинарных файлов
            deletedBinaries: 0,    // Удалено бинарных файлов
            emptyLinesRemoved: 0,  // Удалено пустых строк
            blockRemoved: 0        // Удалено блочных иллюстраций
        };
        
        // Объекты для хранения ID бинарников
        var binaryIdsToDelete = {}; // ID бинарников, которые нужно удалить
        var coverBinaryIds = {};    // ID бинарников-обложек (не удалять)
        var inlineBinaryIds = {};   // ID бинарников инлайн иллюстраций (не удалять и не портить!)
        
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
        analyzeDocument(stats, binaryIdsToDelete, coverBinaryIds, inlineBinaryIds);
        
        // Если нет блочных иллюстраций для обработки
        if (stats.blockImages === 0) {
            if (showStatistics) {
                MsgBox(scriptName + "\nver. " + version + "\n\n" +
                      "Не найдено блочных иллюстраций для удаления.");
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
                                 "• Найдено блочных иллюстраций: " + stats.blockImages + "\n";
            
            if (stats.inlineImages > 0) {
                analysisMessage += "• Найдено инлайн иллюстраций: " + stats.inlineImages + " (не удаляются и не затрагиваются!)\n";
            }
            
            analysisMessage += "\n✓ Будет удалено бинарников: " + willDeleteCount + "\n";
            
            if (stats.coverBinaries > 0) {
                analysisMessage += "✓ Бинарников обложек: " + stats.coverBinaries + " (не удаляются)\n";
            }
            
            if (stats.inlineBinaries > 0) {
                analysisMessage += "✓ Бинарников инлайн иллюстраций: " + stats.inlineBinaries + " (не удаляются и не затрагиваются!)\n";
            }
            
            analysisMessage += "\n• Всего бинарных файлов в документе: " + stats.totalBinaries + "\n" +
                               "\nПараметры обработки:\n" +
                               "• Добавлять маркеры: " + (processMarkers ? "ДА" : "НЕТ");
            
            if (processMarkers) {
                analysisMessage += " (" + BlockPicMarker + ")\n";
            } else {
                analysisMessage += "\n";
            }
            
            analysisMessage += "• Удалять пустые строки: " + (removeEmptyLines ? "ДА" : "НЕТ") + "\n" +
                               "\nВыполнить удаление блочных иллюстраций и их бинарников?";
            
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
        var undoUnitName = "Удаление блочных иллюстраций";
        
        // ФАЗА 1: Удаляем блочные иллюстрации (с маркерами и обработкой пустых строк)
        if (stats.blockImages > 0) {
            firstRemoving = removeBlockImagesFunc(stats, processMarkers, BlockPicMarker, removeEmptyLines, isLineEmpty, emptyLineIsNeededForValidity, firstRemoving, undoUnitName);
        }
        
        // ФАЗА 2: Удаляем соответствующие бинарники (по сохраненным ID)
        if (stats.blockBinaries > 0) {
            firstRemoving = removeCorrespondingBinaries(binaryIdsToDelete, coverBinaryIds, inlineBinaryIds, stats, firstRemoving, undoUnitName);
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
                               "✓ Удалено блочных иллюстраций: " + stats.blockRemoved + "\n" +
                               "✓ Удалено бинарных файлов: " + stats.deletedBinaries + "\n";
            
            if (processMarkers && stats.markersAdded > 0) {
                resultMessage += "✓ Добавлено маркеров: " + stats.markersAdded + "\n";
            }
            
            if (removeEmptyLines && stats.emptyLinesRemoved > 0) {
                resultMessage += "✓ Удалено пустых строк: " + stats.emptyLinesRemoved + "\n";
            }
            
            if (stats.inlineImages > 0) {
                resultMessage += "\n• Инлайн иллюстраций не тронуто: " + stats.inlineImages + "\n";
            }
            
            if (stats.coverBinaries > 0) {
                resultMessage += "• Обложек сохранено: " + stats.coverBinaries + "\n";
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
function analyzeDocument(stats, binaryIdsToDelete, coverBinaryIds, inlineBinaryIds) {
    // Считаем бинарники
    var binobj = document.all.binobj;
    if (binobj) {
        var bins = binobj.getElementsByTagName("DIV");
        stats.totalBinaries = bins.length;
        
        // Ищем все иллюстрации и собираем ID используемых бинарников
        var fbwBody = document.getElementById("fbw_body");
        if (fbwBody) {
            // Ищем DIV с классом image (блочные) и собираем их ID
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
            
            // Ищем SPAN с классом image (инлайн) - собираем их ID чтобы НЕ удалять и НЕ портить!
            var spans = fbwBody.getElementsByTagName("SPAN");
            for (var i = 0; i < spans.length; i++) {
                var span = spans[i];
                if (span.className && span.className.toLowerCase() == "image") {
                    stats.inlineImages++;
                    
                    // Определяем ID бинарника инлайн иллюстрации
                    var href = span.getAttribute("href") || "";
                    if (href && href !== "#undefined") {
                        var fileName = href.replace(/^#/, '');
                        if (fileName && fileName !== "undefined") {
                            stats.inlineBinaries++;
                            inlineBinaryIds[fileName] = true;
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

// Удаление блочных иллюстраций (с возможностью добавления маркеров и удаления пустых строк)
function removeBlockImagesFunc(stats, processMarkers, markerText, removeEmptyLines, isLineEmpty, emptyLineIsNeededForValidity, firstRemoving, undoUnitName) {
    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) return firstRemoving;
    
    // Ищем все DIV с классом image
    var divs = fbwBody.getElementsByTagName("DIV");
    var emptyLines = [];
    
    // Сначала собираем информацию о пустых строках
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
            
            if (processMarkers) {
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

// Удаление бинарников по сохраненным ID (исправленный метод!)
function removeCorrespondingBinaries(binaryIdsToDelete, coverBinaryIds, inlineBinaryIds, stats, firstRemoving, undoUnitName) {
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
    
    // Правильный метод удаления бинарников
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
            if (binaryIdsToDelete[id] && !coverBinaryIds[id] && !inlineBinaryIds[id]) {
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
