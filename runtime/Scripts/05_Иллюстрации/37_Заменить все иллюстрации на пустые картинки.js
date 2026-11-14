// Скрипт "Заменить все иллюстрации на пустые картинки" для редактора FBE
// version 1.5
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для замены всех иллюстраций в fb2 документах на пустые картинки.
// Соответствующие бинарники при этом удаляются.
//  (В дальнейшем такая замена позволяет другим скриптом расставить новые иллюстрации на место "пустышек").
// Скрипт обрабатывает блочные и инлайн иллюстрации, оставляя обложки без изменений.
// Иллюстрации-обложки, если такие встречаются в самом тексте, заменяются на "пустышки",
// но их бинарники сохраняются.
// Обработка разделов сносок и комментариев (опционально).
// Отдельная настройка для обработки блочных и инлайн иллюстраций.
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.5, 03.02.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Заменить все иллюстрации на пустые картинки";
    var version = "1.5";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима
    
    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0; // 0 - нет, 1 - да
    
    // Обрабатывать раздел комментариев
    var processCommentsSection = 0; // 0 - нет, 1 - да
    
    // Обрабатывать блочные иллюстрации
    var processBlockImages = 1; // 1 - да, 0 - нет
    
    // Обрабатывать инлайн (внутриабзацные) иллюстрации
    var processInlineImages = 1; // 1 - да, 0 - нет
    
    // Удалять прикрепленные бинарные файлы
    var deleteBinaries = 1; // 1 - да, 0 - нет
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    try {
        // Создаем объект для статистики
        var stats = {
            totalImages: 0,                    // Только обычные иллюстрации (НЕ обложки)
            blockImages: 0,                    // Блочные обычные
            inlineImages: 0,                   // Инлайн обычные
            replacedImages: 0,                 // Всего заменено (обычные + обложки в тексте)
            replacedNormalImages: 0,           // Заменено обычных иллюстраций
            replacedCoverInText: 0,            // Будет заменено обложек в тексте
            actuallyReplacedCoverInText: 0,    // Фактически заменено обложек в тексте
            coverInTextFiles: [],              // Файлы обложек, которые будут заменены в тексте
            actuallyReplacedCoverFiles: [],    // Фактически замененные файлы обложек в тексте
            skippedCoverOnly: 0,               // Обложки, которые только как обложки
            totalBinaries: 0,                  // Всего бинарных файлов
            deletedBinaries: 0,                // Удалено бинарных файлов
            keptBinaries: 0,                   // Оставлено бинарников (обложки)
            usedBinaryIds: {},                 // ID бинарников, используемых в тексте
            coverBinaryIds: {},                // ID бинарников-обложек
            coverBinaryNames: [],              // Имена всех обложек
            binaryUsageCount: {},              // Количество использований каждого бинарника
            processedBodies: []                // Какие body были обработаны (для отладки)
        };
        
        // 1. Сначала собираем статистику
        var analysisResult = analyzeDocument(stats, processNotesSection, processCommentsSection);
        
        // Если нет иллюстраций для обработки
        if (stats.totalImages === 0 && stats.replacedCoverInText === 0) {
            if (showStatistics) {
                MsgBox(scriptName + "\nver. " + version + "\n\n" +
                      "Не найдено иллюстраций для замены.");
            }
            return;
        }
        
        // 2. Показываем статистику анализа (если не тихий режим)
        if (showStatistics) {
            // Считаем количество использованных бинарников (обычных, не обложек)
            var usedBinariesCount = 0;
            for (var key in stats.usedBinaryIds) {
                if (stats.usedBinaryIds.hasOwnProperty(key)) {
                    usedBinariesCount++;
                }
            }
            
            var analysisMessage = scriptName + "\n" +
                                 "ver. " + version + "\n\n" +
                                 "АНАЛИЗ ДОКУМЕНТА:\n\n";
            
            // Показываем информацию об обработанных разделах (для отладки)
            if (stats.processedBodies.length > 0) {
                analysisMessage += "Обработаны разделы:\n";
                for (var j = 0; j < stats.processedBodies.length; j++) {
                    analysisMessage += "• " + stats.processedBodies[j] + "\n";
                }
                analysisMessage += "\n";
            }
            
            analysisMessage += "• Обычных иллюстраций: " + stats.totalImages + "\n" +
                                 "  - блочных: " + stats.blockImages + "\n" +
                                 "  - инлайн: " + stats.inlineImages + "\n" +
                                 "• Обложек в тексте: " + stats.replacedCoverInText + "\n";
            
            if (stats.coverInTextFiles.length > 0) {
                analysisMessage += "  файлы: " + stats.coverInTextFiles.join(", ") + "\n";
            }
            
            analysisMessage += "• Бинарных файлов: " + stats.totalBinaries + "\n" +
                              "• Используется бинарников: " + usedBinariesCount + "\n";
            
            // Добавляем информацию об обложках
            if (stats.coverBinaryNames.length > 0) {
                analysisMessage += "• Всего обложек: " + stats.coverBinaryNames.length + 
                                 " (" + stats.coverBinaryNames.join(", ") + ")\n";
            }
            
            if (stats.skippedCoverOnly > 0) {
                analysisMessage += "• Обложек (только как обложки): " + stats.skippedCoverOnly + "\n";
            }
            
            // Рассчитываем общее количество замен
            var totalToReplace = stats.totalImages + stats.replacedCoverInText;
            analysisMessage += "\n• ВСЕГО БУДЕТ ЗАМЕНЕНО: " + totalToReplace + " иллюстраций\n";
            
            analysisMessage += "\nПараметры обработки:\n" +
                              "• Блочные иллюстрации: " + (processBlockImages ? "ДА" : "НЕТ") + "\n" +
                              "• Инлайн иллюстрации: " + (processInlineImages ? "ДА" : "НЕТ") + "\n" +
                              "• Удаление бинарников: " + (deleteBinaries ? "ДА" : "НЕТ") + "\n" +
                              "• Сноски: " + (processNotesSection ? "ДА" : "НЕТ") + "\n" +
                              "• Комментарии: " + (processCommentsSection ? "ДА" : "НЕТ") + "\n\n" +
                              "Выполнить замену иллюстраций на пустышки?";
            
            if (!AskYesNo(analysisMessage)) {
                return; // Пользователь отказался
            }
        }
        
        // ТАЙМЕР ЗАПУСКАЕМ ТОЛЬКО ПОСЛЕ ПОСЛЕДНЕГО CONFIRM!
        var startTime = new Date().getTime();
        
        // 3. Выполняем замену иллюстраций
        window.external.BeginUndoUnit(document, scriptName);
        
        var replaceResult = replaceImages(
            analysisResult.imagesToReplace, 
            stats,
            processBlockImages,
            processInlineImages
        );
        
        // 4. Удаляем бинарные файлы (если включено в настройках)
        if (deleteBinaries) {
            deleteBinaryFiles(stats);
        }
        
        window.external.EndUndoUnit(document);
        
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
                               "РЕЗУЛЬТАТЫ:\n\n" +
                               "✓ ВСЕГО ЗАМЕНЕНО: " + stats.replacedImages + " иллюстраций\n\n" +
                               "Из них:\n" +
                               "• Обычных иллюстраций: " + stats.replacedNormalImages + "\n";
            
            if (stats.replacedNormalImages > 0) {
                if (processBlockImages) {
                    resultMessage += "  - блочных: " + replaceResult.replacedBlock + "\n";
                }
                
                if (processInlineImages) {
                    resultMessage += "  - инлайн: " + replaceResult.replacedInline + "\n";
                }
            }
            
            if (stats.actuallyReplacedCoverInText > 0) {
                resultMessage += "• Обложек в тексте: " + stats.actuallyReplacedCoverInText + "\n";
                if (stats.actuallyReplacedCoverFiles.length > 0) {
                    resultMessage += "  файлы: " + stats.actuallyReplacedCoverFiles.join(", ") + "\n";
                }
            }
            
            if (deleteBinaries) {
                resultMessage += "\n✓ Удалено бинарных файлов: " + stats.deletedBinaries + "\n";
                resultMessage += "✓ Оставлено бинарников (обложки): " + stats.keptBinaries + "\n";
            }
            
            if (stats.skippedCoverOnly > 0) {
                resultMessage += "\n• Обложек не тронуто (только как обложки): " + stats.skippedCoverOnly + "\n";
            }
            
            if (stats.coverBinaryNames.length > 0) {
                resultMessage += "• Всего обложек в документе: " + stats.coverBinaryNames.join(", ") + "\n";
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

// Функция анализа документа
function analyzeDocument(stats, processNotes, processComments) {
    var result = {
        imagesToReplace: [],
        coverBinaryIds: {}
    };
    
    // 1. Сначала находим все бинарники-обложки и их имена
    findCoverBinaries(result.coverBinaryIds, stats.coverBinaryNames);
    
    // 2. Находим все body элементы
    var bodyElements = [];
    var allDivs = document.getElementsByTagName("DIV");
    
    for (var i = 0; i < allDivs.length; i++) {
        var div = allDivs[i];
        if (div.className == "body") {
            var fbname = div.getAttribute("fbname") || "";
            
            // ОСНОВНОЕ ИСПРАВЛЕНИЕ: обрабатываем ВСЕ body, кроме специальных разделов
            // Основной body может быть с fbname="" или с любым другим именем
            
            if (fbname == "notes") {
                // Раздел сносок - обрабатываем только если включено в настройках
                if (processNotes) {
                    bodyElements.push(div);
                    stats.processedBodies.push("сноски (notes)");
                }
            } else if (fbname == "comments") {
                // Раздел комментариев - обрабатываем только если включено в настройках
                if (processComments) {
                    bodyElements.push(div);
                    stats.processedBodies.push("комментарии (comments)");
                }
            } else {
                // ВСЕ ОСТАЛЬНЫЕ body (включая основной и любые другие) - обрабатываем
                bodyElements.push(div);
                if (fbname == "") {
                    stats.processedBodies.push("основной (без имени)");
                } else {
                    stats.processedBodies.push("'" + fbname + "'");
                }
            }
        }
    }
    
    // 3. Ищем все иллюстрации в выбранных разделах
    for (var i = 0; i < bodyElements.length; i++) {
        findImagesInElement(bodyElements[i], result.imagesToReplace, stats, result.coverBinaryIds);
    }
    
    // 4. Анализируем бинарные файлы
    analyzeBinaryFiles(stats);
    
    // 5. Переносим информацию об обложках в stats
    for (var coverId in result.coverBinaryIds) {
        if (result.coverBinaryIds.hasOwnProperty(coverId)) {
            stats.coverBinaryIds[coverId] = true;
        }
    }
    
    return result;
}

// Поиск бинарников-обложек и их имен
function findCoverBinaries(coverBinaryIds, coverBinaryNames) {
    try {
        // Ищем в tiCover (основная обложка)
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
                            coverBinaryIds[fileName] = true;
                            // Добавляем имя в список (без дубликатов)
                            addUniqueToArray(coverBinaryNames, fileName);
                        }
                    }
                }
            }
        }
        
        // Ищем в stiCover (обложка оригинала)
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
                            coverBinaryIds[fileName] = true;
                            // Добавляем имя в список (без дубликатов)
                            addUniqueToArray(coverBinaryNames, fileName);
                        }
                    }
                }
            }
        }
        
    } catch (e) {
        // Игнорируем ошибки
    }
}

// Добавление уникального элемента в массив
function addUniqueToArray(array, element) {
    for (var i = 0; i < array.length; i++) {
        if (array[i] === element) {
            return; // Уже есть
        }
    }
    array.push(element);
}

// Поиск изображений в элементе (рекурсивно)
function findImagesInElement(element, imagesToReplace, stats, coverBinaryIds) {
    if (!element || !element.childNodes) return;
    
    var children = element.childNodes;
    for (var i = 0; i < children.length; i++) {
        var child = children[i];
        
        if (child.nodeType == 1) { // ELEMENT_NODE
            var tagName = child.tagName ? child.tagName.toUpperCase() : '';
            var className = child.className || '';
            
            // Проверяем, является ли элемент иллюстрацией
            if (className == "image" && (tagName == "DIV" || tagName == "SPAN")) {
                var href = child.getAttribute("href") || "";
                var imgElement = null;
                
                // Находим вложенный IMG
                if (child.firstChild && child.firstChild.tagName == "IMG") {
                    imgElement = child.firstChild;
                }
                
                if (href && imgElement) {
                    var fileName = href.replace(/^#/, '');
                    
                    // Проверяем, не является ли это уже "пустышкой"
                    if (fileName !== "undefined" && href !== "#undefined") {
                        var isBlock = (tagName == "DIV");
                        
                        // Проверяем, является ли этот бинарник обложкой
                        var isCoverBinary = false;
                        for (var coverId in coverBinaryIds) {
                            if (coverBinaryIds.hasOwnProperty(coverId)) {
                                if (fileName === coverId) {
                                    isCoverBinary = true;
                                    break;
                                }
                            }
                        }
                        
                        if (isCoverBinary) {
                            // Этот бинарник используется как обложка
                            stats.replacedCoverInText++;
                            
                            // Добавляем файл в список (без дубликатов)
                            addUniqueToArray(stats.coverInTextFiles, fileName);
                            
                            // Добавляем в список для замены (НО бинарник не удаляем!)
                            imagesToReplace.push({
                                element: child,
                                imgElement: imgElement,
                                isBlock: isBlock,
                                originalHref: href,
                                originalSrc: imgElement.src,
                                binaryId: fileName,
                                isCoverBinary: true  // Помечаем как обложку
                            });
                            
                        } else {
                            // Обычная картинка (не обложка)
                            if (isBlock) {
                                stats.blockImages++;
                            } else {
                                stats.inlineImages++;
                            }
                            stats.totalImages++; // Только обычные иллюстрации!
                            
                            // Запоминаем ID бинарника
                            if (fileName && fileName !== "undefined") {
                                stats.usedBinaryIds[fileName] = true;
                                stats.binaryUsageCount[fileName] = (stats.binaryUsageCount[fileName] || 0) + 1;
                            }
                            
                            // Добавляем в список для замены
                            imagesToReplace.push({
                                element: child,
                                imgElement: imgElement,
                                isBlock: isBlock,
                                originalHref: href,
                                originalSrc: imgElement.src,
                                binaryId: fileName,
                                isCoverBinary: false  // Не обложка
                            });
                        }
                    }
                }
            } else {
                // Рекурсивный поиск в дочерних элементах
                findImagesInElement(child, imagesToReplace, stats, coverBinaryIds);
            }
        }
    }
}

// Анализ бинарных файлов
function analyzeBinaryFiles(stats) {
    try {
        var binobj = document.all.binobj;
        if (binobj) {
            var objects = binobj.getElementsByTagName("DIV");
            stats.totalBinaries = objects.length;
            
            // Считаем бинарники, которые используются только как обложки
            for (var i = 0; i < objects.length; i++) {
                var bin = objects[i];
                var id = bin.all.id.value || "";
                
                if (id) {
                    // Проверяем, является ли этот бинарник обложкой
                    var isCover = false;
                    for (var coverId in stats.coverBinaryIds) {
                        if (stats.coverBinaryIds.hasOwnProperty(coverId)) {
                            if (id === coverId) {
                                isCover = true;
                                break;
                            }
                        }
                    }
                    
                    // Если это обложка
                    if (isCover) {
                        // Проверяем, используется ли она также в тексте
                        var usedInText = false;
                        for (var usedId in stats.usedBinaryIds) {
                            if (stats.usedBinaryIds.hasOwnProperty(usedId)) {
                                if (id === usedId) {
                                    usedInText = true;
                                    break;
                                }
                            }
                        }
                        
                        // Если НЕ используется в тексте - это обложка, которая только как обложка
                        if (!usedInText) {
                            stats.skippedCoverOnly++;
                        }
                    }
                }
            }
        }
    } catch (e) {
        // Игнорируем ошибки
    }
}

// Замена изображений на "пустышки"
function replaceImages(imagesToReplace, stats, processBlockImages, processInlineImages) {
    var result = {
        replacedBlock: 0,
        replacedInline: 0,
        replacedCoverInText: 0
    };
    
    for (var i = 0; i < imagesToReplace.length; i++) {
        var imageInfo = imagesToReplace[i];
        
        // Проверяем, нужно ли обрабатывать этот тип изображений
        if ((imageInfo.isBlock && !processBlockImages) || 
            (!imageInfo.isBlock && !processInlineImages)) {
            continue;
        }
        
        try {
            // Заменяем href и src на #undefined
            imageInfo.element.setAttribute("href", "#undefined");
            imageInfo.imgElement.src = "fbw-internal:#undefined";
            
            stats.replacedImages++; // Всего заменено (обычные + обложки)
            
            if (imageInfo.isCoverBinary) {
                // Если это обложка в тексте
                stats.actuallyReplacedCoverInText++;
                result.replacedCoverInText++;
                
                // Добавляем файл в список фактически замененных
                addUniqueToArray(stats.actuallyReplacedCoverFiles, imageInfo.binaryId);
                
            } else {
                // Если это обычная иллюстрация
                stats.replacedNormalImages++;
                
                if (imageInfo.isBlock) {
                    result.replacedBlock++;
                } else {
                    result.replacedInline++;
                }
            }
            
        } catch (e) {
            // Игнорируем ошибки при замене отдельных изображений
        }
    }
    
    return result;
}

// Удаление бинарных файлов
function deleteBinaryFiles(stats) {
    try {
        var binobj = document.all.binobj;
        if (binobj) {
            var objects = binobj.getElementsByTagName("DIV");
            var binsToDelete = [];
            
            // Собираем бинарники для удаления
            for (var i = 0; i < objects.length; i++) {
                var bin = objects[i];
                var id = bin.all.id.value || "";
                
                if (id) {
                    // Проверяем, используется ли этот бинарник в тексте (обычные иллюстрации)
                    var isUsedInText = false;
                    for (var usedId in stats.usedBinaryIds) {
                        if (stats.usedBinaryIds.hasOwnProperty(usedId)) {
                            if (id === usedId) {
                                isUsedInText = true;
                                break;
                            }
                        }
                    }
                    
                    // Проверяем, является ли этот бинарник обложкой
                    var isCover = false;
                    for (var coverId in stats.coverBinaryIds) {
                        if (stats.coverBinaryIds.hasOwnProperty(coverId)) {
                            if (id === coverId) {
                                isCover = true;
                                break;
                            }
                        }
                    }
                    
                    // Удаляем только если:
                    // 1. Используется в тексте (был заменен на undefined)
                    // 2. НЕ является обложкой
                    if (isUsedInText && !isCover) {
                        binsToDelete.push(bin);
                    } else if (isCover) {
                        // Оставляем бинарники-обложки (даже если они использовались в тексте)
                        stats.keptBinaries++;
                    }
                }
            }
            
            // Удаляем в обратном порядке (для стабильности DOM)
            for (var i = binsToDelete.length - 1; i >= 0; i--) {
                try {
                    // Используем функцию Remove из main.js
                    if (typeof Remove === "function") {
                        Remove(binsToDelete[i]);
                        stats.deletedBinaries++;
                    }
                } catch (e) {
                    // Игнорируем ошибки при удалении отдельных бинарников
                }
            }
        }
    } catch (e) {
        // Игнорируем ошибки
    }
}
