// Скрипт "Переместить иллюстрации ниже заголовков" для редактора FBE
// version 5.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для переноса иллюстрации из конца предыдущих секций
// в начало следующих (после заголовка или заголовка и эпиграфа) в fb2 документах.
// Количество переносимых иллюстраций по умолчанию - 1.
// Можно переносить одну, все, что есть в конце секций, или вводить кол-во вручную по запросу скрипта.
// Иллюстрациями в конце секций считаются все иллюстрации, расположенные после текста в конце секции
// (когда после них в секции больше нет никакого текста).
// Пустые строки текстом не считаются.
// В молчаливом режиме скрипт всегда переносит по умолчанию 1 последнюю картинку.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// Настройки скрипта задаются после функции Run в строке 285 и далее.

// version 5.1, 31.12.2026
//======================================

// Безопасная функция для trim (совместимая с IE6)
function trimStr(str) {
    if (typeof str !== 'string') return str;
    return str.replace(/^\s+|\s+$/g, '');
}

// Безопасная проверка наличия подстроки
function containsStr(str, search) {
    if (typeof str !== 'string' || typeof search !== 'string') return false;
    return str.indexOf(search) !== -1;
}

// Функция проверки наличия элемента в массиве (для IE6)
function arrayContains(arr, item) {
    if (!arr || arr.length === undefined) return false;
    for (var i = 0; i < arr.length; i++) {
        if (arr[i] === item) return true;
    }
    return false;
}

// Функция добавления элемента в массив если его там нет
function addToArrayIfNotExists(arr, item) {
    if (!arr) arr = [];
    if (!arrayContains(arr, item)) {
        arr[arr.length] = item;
    }
    return arr;
}

// Безопасное получение длины массива
function safeLength(arr) {
    return (arr && arr.length !== undefined) ? arr.length : 0;
}

// Форматирует заголовок для отображения в статистике
function formatSectionTitle(title, maxLength) {
    if (!title) return "Без заголовка";
    if (!maxLength) maxLength = 30;
    if (title.length <= maxLength) return title;
    return title.substring(0, maxLength - 3) + "...";
}

// Функция для форматирования числа с фиксированной точность (замена toFixed)
function toFixedNumber(num, decimals) {
    if (typeof num !== 'number') return num;
    var factor = Math.pow(10, decimals || 2);
    var rounded = Math.round(num * factor) / factor;
    return rounded.toString();
}

// Функция для проверки наличия подстроки (полная замена indexOf)
function stringContains(str, search) {
    if (typeof str !== 'string' || typeof search !== 'string') return false;
    
    // Ручная реализация поиска подстроки для IE6
    var strLen = str.length;
    var searchLen = search.length;
    
    if (searchLen > strLen) return false;
    
    for (var i = 0; i <= strLen - searchLen; i++) {
        var match = true;
        for (var j = 0; j < searchLen; j++) {
            if (str.charAt(i + j) !== search.charAt(j)) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    
    return false;
}

// Функция поиска индекса подстроки (замена indexOf)
function stringIndexOf(str, search, startIndex) {
    if (typeof str !== 'string' || typeof search !== 'string') return -1;
    
    startIndex = startIndex || 0;
    var strLen = str.length;
    var searchLen = search.length;
    
    if (searchLen > strLen || startIndex < 0) return -1;
    
    for (var i = startIndex; i <= strLen - searchLen; i++) {
        var match = true;
        for (var j = 0; j < searchLen; j++) {
            if (str.charAt(i + j) !== search.charAt(j)) {
                match = false;
                break;
            }
        }
        if (match) return i;
    }
    
    return -1;
}

// Функция проверки, является ли текст только пробельными символами (включая &nbsp;)
function isWhitespaceOnly(text) {
    if (!text) return true;
    
    // Заменяем все неразрывные пробелы на обычные
    var normalized = text.replace(/&nbsp;|&#160;|\u00A0|\u2002|\u2003|\u2009|\u200A/g, ' ');
    
    // Удаляем все пробельные символы
    var trimmed = normalized.replace(/\s+/g, '');
    
    return trimmed === '';
}

// Функция проверки, находится ли секция в основном разделе (body) или в сносках (notes)
function isInMainBody(section) {
    var current = section;
    while (current && current.nodeType == 1) {
        var fbname = current.getAttribute('fbname');
        if (fbname === 'body') {
            return true;
        }
        if (fbname === 'notes') {
            return false;
        }
        current = current.parentNode;
    }
    return true; // По умолчанию считаем, что в основном разделе
}

// Функция для поиска родительского элемента с указанным fbname
function findParentWithFbname(element, fbnameValue) {
    var current = element;
    while (current && current.nodeType == 1) {
        var fbname = current.getAttribute('fbname');
        if (fbname === fbnameValue) {
            return current;
        }
        current = current.parentNode;
    }
    return null;
}

// Функция для поиска всех следующих секций В ОСНОВНОМ РАЗДЕЛЕ (тело, не сноски)
function findNextSectionsInMainBody(allSections, startIndex) {
    var result = [];
    for (var i = startIndex + 1; i < safeLength(allSections); i++) {
        var section = allSections[i];
        if (isInMainBody(section)) {
            result.push(section);
        }
    }
    return result;
}

// Функция проверки, есть ли раздел сносок в документе
function hasNotesSectionInDocument() {
    // Ищем div с fbname="notes" в документе
    var allDivs = document.getElementsByTagName('div');
    for (var i = 0; i < safeLength(allDivs); i++) {
        var div = allDivs[i];
        if (div.getAttribute && div.getAttribute('fbname') === 'notes') {
            return true;
        }
    }
    return false;
}

// Функция находит следующую секцию после указанного индекса - ОБНОВЛЕННАЯ ВЕРСИЯ 5.1
function findNextSection(allSections, currentIndex, currentSection) {
    // Проверяем, находимся ли мы в основном разделе (не в сносках)
    var isCurrentInMainBody = isInMainBody(currentSection);
    
    if (!isCurrentInMainBody) {
        // Если текущая секция уже в сносках, не ищем дальше
        return {
            section: null,
            reason: "notes_section",
            message: "Текущая секция находится в разделе сносок"
        };
    }
    
    // Ищем следующие секции ТОЛЬКО в основном разделе
    var nextSectionsInMainBody = [];
    for (var i = currentIndex + 1; i < safeLength(allSections); i++) {
        var section = allSections[i];
        if (isInMainBody(section)) {
            nextSectionsInMainBody.push(section);
        }
    }
    
    if (safeLength(nextSectionsInMainBody) === 0) {
        // Нет больше секций в основном разделе
        // Проверяем, есть ли вообще раздел сносок в документе
        var hasNotes = hasNotesSectionInDocument();
        
        if (hasNotes) {
            return {
                section: null,
                reason: "notes_follows",
                message: "После основного раздела идет раздел сносок"
            };
        } else {
            return {
                section: null,
                reason: "end_of_document",
                message: "Достигнут конец документа (нет раздела сносок)"
            };
        }
    }
    
    // Ищем первую не-изображение секцию в основном разделе
    for (var j = 0; j < safeLength(nextSectionsInMainBody); j++) {
        var nextSection = nextSectionsInMainBody[j];
        
        // Пропускаем секции-иллюстрации (ищем следующую обычную секцию)
        if (!isImageSection(nextSection)) {
            return {
                section: nextSection,
                reason: "valid",
                message: ""
            };
        }
    }
    
    // Если все следующие секции - это секции-иллюстрации
    // Проверяем, что после них еще что-то есть
    var lastMainSectionIndex = -1;
    for (var k = safeLength(allSections) - 1; k >= 0; k--) {
        if (isInMainBody(allSections[k])) {
            lastMainSectionIndex = k;
            break;
        }
    }
    
    if (currentIndex >= lastMainSectionIndex) {
        // Это последняя секция в основном разделе
        var hasNotes = hasNotesSectionInDocument();
        
        if (hasNotes) {
            return {
                section: null,
                reason: "notes_follows",
                message: "После основного раздела идет раздел сносок"
            };
        } else {
            return {
                section: null,
                reason: "end_of_document",
                message: "Достигнут конец документа (нет раздела сносок)"
            };
        }
    }
    
    // Возвращаем первую секцию-иллюстрацию, если ничего другого нет
    return {
        section: nextSectionsInMainBody[0],
        reason: "valid",
        message: ""
    };
}

function Run() {
// ==================================================
// НАСТРОЙКИ СКРИПТА ========== (можно менять перед запуском)
// ==================================================

    var settings = {
        silentMode: 1,             // 0 - обычный режим с диалогами, 1 - молчаливый режим (без диалогов)
        keepOldImages: 0,          // 0 - удалять старые иллюстрации, 1 - оставлять
        moveEmptyImages: 1,        // 1 - переносить пустые иллюстрации, 0 - не переносить
        insertIfImageExists: 1,    // 0 - не вставлять рядом с уже имеющейся иллюстрацией, 1 - вставлять
        insertPosition: 1,         // 0 - перед существующей картинкой, 1 - после существующей картинки
        compactStats: 1,           // 0 - показывать все элементы статистики, 1 - скрывать строки с нулями
        
         // Сколько иллюстраций переносить:
        //    0 - только самую последнюю в секции
        //    1 - все последние иллюстрации в секции (когда после них в секции нет никакого текста)
        //    2 - предлагать указать кол-во
        processMode: 0            // По умолчанию 0 (только последняя иллюстрация)
    };
    
    // Определяем сколько иллюстраций переносить
    var imagesToMoveCount = 1; // По умолчанию 1 (только последняя)
    
    if (settings.processMode === 2 && settings.silentMode !== 1) {
        // Спрашиваем у пользователя количество:
        var message = "FBE скрипт \"Переместить иллюстрации ниже заголовков\" ver. 5.1\n\n";
        message += "СКОЛЬКО ИЛЛЮСТРАЦИЙ ПЕРЕНОСИТЬ?\n\n";
        message += "Укажите количество иллюстраций для переноса из конца секции:\n";
        message += "• 0 - все последние иллюстрации (после которых нет текста)\n";
        message += "• 1 и более - указанное количество с конца секции\n\n";
        message += "Введите число (0-99):";
        
        var defaultValue = "1";
        var result = ""; // Переменная для результата
        
        // Используем InputBox из FBE вместо стандартного prompt
        var inputResult = window.external.InputBox(message, "FBE скрипт", defaultValue);
        
        if (inputResult === null || inputResult === "") {
            // Пользователь нажал отмену или не ввел ничего
            return;
        }
        
        // Получаем результат из InputBox
        result = inputResult;
        
        // Преобразуем в число
        var parsedInput = parseInt(result, 10);
        if (!isNaN(parsedInput) && parsedInput >= 0 && parsedInput <= 99) {
            imagesToMoveCount = parsedInput;
        } else {
            window.external.MsgBox("Неверный ввод! Используется значение по умолчанию (1 - только последняя иллюстрация).");
            imagesToMoveCount = 1; // По умолчанию только последняя
        }
    } else if (settings.processMode === 1) {
        // Режим 1: все последние иллюстрации (после которых нет текста)
        imagesToMoveCount = 0; // 0 означает "все найденные"
    } else {
        // Режим 0 или молчаливый режим: только самая последняя иллюстрация
        imagesToMoveCount = 1;
    }
    
    // В молчаливом режиме отключаем все диалоги
    var askDelete = (settings.silentMode === 1) ? 0 : 1;
    var askEmpty = (settings.silentMode === 1) ? 0 : 1;
    var askExisting = (settings.silentMode === 1) ? 0 : 1;
    
    // Массив для хранения подробной информации о пропущенных секций
    var skippedSectionsDetails = [];
    
    try { 
        var nbspChar = window.external.GetNBSP(); 
        var nbspEntity; 
        if (nbspChar.charCodeAt(0) == 160) nbspEntity = "&nbsp;"; 
        else nbspEntity = nbspChar; 
    } catch(e) { 
        nbspChar = String.fromCharCode(160); 
        nbspEntity = "&nbsp;"; 
    }
    
    // Находим основной body с контентом
    var mainBody = findMainBody();
    
    if (!mainBody) {
        window.external.MsgBox("Ошибка: не найден основной body с контентом!");
        return;
    }
    
    // Анализируем документ перед обработкой
    var analysis = analyzeDocumentBefore(mainBody);
    
    if (analysis.totalImages === 0) {
        window.external.MsgBox("В документе не найдено иллюстраций!");
        return;
    }
    
    // ДИАЛОГИ (пропускаем в молчаливом режиме)
    if (settings.silentMode !== 1) {
        // Показываем информацию о выбранном режиме переноса
        var modeInfo = "";
        if (settings.processMode === 2) {
            if (imagesToMoveCount === 0) {
                modeInfo = "• Режим переноса: ВСЕ последние иллюстрации\n";
            } else if (imagesToMoveCount === 1) {
                modeInfo = "• Режим переноса: ТОЛЬКО последняя иллюстрация\n";
            } else {
                modeInfo = "• Режим переноса: " + imagesToMoveCount + " последних иллюстраций\n";
            }
        } else if (settings.processMode === 1) {
            modeInfo = "• Режим переноса: ВСЕ последние иллюстрации\n";
        } else {
            modeInfo = "• Режим переноса: ТОЛЬКО последняя иллюстрация\n";
        }
        
        // Диалог 1: спрашиваем про удаление старых иллюстраций
        if (askDelete) {
            var deleteResponse = window.confirm(
                "FBE скрипт \"Переместить иллюстрации ниже заголовков\" ver. 5.1\n\n" +
                "АНАЛИЗ ДОКУМЕНТА:\n" +
                "• Секций всего: " + analysis.totalSections + "\n" +
                "• Иллюстраций всего: " + analysis.totalImages + "\n" +
                "• Иллюстраций в концах секций: " + analysis.imagesAtEndOfSections + "\n" +
                "• Секций-иллюстраций: " + analysis.imageSections + "\n" +
                "• Секций с эпиграфами: " + analysis.sectionsWithEpigraph + "\n" +
                "• Секций с иллюстрациями ПОСЛЕ заголовков/эпиграфов: " + analysis.sectionsWithImagesAfterTitle + "\n\n" +
                modeInfo +
                "\nУДАЛЯТЬ старые иллюстрации после перемещения?\n\n" +
                "Нажмите:\n" +
                "• OK - УДАЛЯТЬ старые иллюстрации\n" +
                "• Отмена - ОСТАВЛЯТЬ старые иллюстрации"
            );
            settings.keepOldImages = deleteResponse ? 0 : 1;
        }
        
        // Диалог 2: спрашиваем про пустые иллюстрации
        if (askEmpty) {
            var emptyResponse = window.confirm(
                "FBE скрипт \"Переместить иллюстрации ниже заголовков\" ver. 5.1\n\n" +
                "Переносить ПУСТЫЕ иллюстрации (#undefined)?\n\n" +
                "Нажмите:\n" +
                "• OK - ПЕРЕНОСИТЬ пустые иллюстрации\n" +
                "• Отмена - НЕ переносить пустые иллюстрации"
            );
            settings.moveEmptyImages = emptyResponse ? 1 : 0;
        }
        
        // Диалог 3: спрашиваем про секции с уже существующими иллюстрациями
        if (askExisting && analysis.sectionsWithImagesAfterTitle > 0) {
            var existingResponse = window.confirm(
                "FBE скрипт \"Переместить иллюстрации ниже заголовков\" ver. 5.1\n\n" +
                "ВСТАВЛЯТЬ иллюстрации в секциях, где они уже есть?\n" +
                "(Найдено таких секций: " + analysis.sectionsWithImagesAfterTitle + ")\n\n" +
                "Нажмите:\n" +
                "• OK - ВСТАВЛЯТЬ дополнительные иллюстрации\n" +
                "• Отмена - НЕ вставлять (пропускать такие секции)"
            );
            settings.insertIfImageExists = existingResponse ? 1 : 0;
            
            // Если согласились вставлять, спрашиваем позицию
            if (settings.insertIfImageExists) {
                var positionResponse = window.confirm(
                    "FBE скрипт \"Переместить иллюстрации ниже заголовков\" v5.1\n\n" +
                    "Куда вставлять иллюстрацию относительно существующей?\n\n" +
                    "Нажмите:\n" +
                    "• OK - ПЕРЕД существующей картинкой\n" +
                    "• Отмена - ПОСЛЕ существующей картинки"
                );
                settings.insertPosition = positionResponse ? 0 : 1;
            }
        }
        
        // Показываем сводку перед выполнением
        var summary = "FBE скрипт \"Переместить иллюстрации ниже заголовков\" ver. 5.1\n\n";
        summary += "АНАЛИЗ ДОКУМЕНТА:\n";
        summary += "• Секций всего: " + analysis.totalSections + "\n";
        summary += "• Иллюстраций всего: " + analysis.totalImages + "\n";
        if (analysis.imagesAtEndOfSections > 0 || settings.compactStats === 0) summary += "• Иллюстраций в концах секций: " + analysis.imagesAtEndOfSections + "\n";
        if (analysis.imageSections > 0 || settings.compactStats === 0) summary += "• Секций-иллюстраций: " + analysis.imageSections + "\n";
        if (analysis.sectionsWithEpigraph > 0 || settings.compactStats === 0) summary += "• Секций с эпиграфами: " + (analysis.sectionsWithEpigraph || 0) + "\n";
        if ((analysis.sectionsWithImagesAfterTitle || 0) > 0 || settings.compactStats === 0) summary += "• Секций с иллюстрациями ПОСЛЕ заголовков/эпиграфов: " + (analysis.sectionsWithImagesAfterTitle || 0) + "\n";
        summary += "\n";
        
        summary += "Настройки выполнения:\n";
        summary += modeInfo;
        summary += "• Старые иллюстрации: " + (settings.keepOldImages ? "ОСТАВЛЯТЬ" : "УДАЛЯТЬ") + "\n";
        summary += "• Пустые иллюстрации: " + (settings.moveEmptyImages ? "ПЕРЕНОСИТЬ" : "НЕ переносить") + "\n";
        summary += "• В секциях с существующими иллюстрациями: ";
        if ((analysis.sectionsWithImagesAfterTitle || 0) > 0) {
            summary += (settings.insertIfImageExists ? "ВСТАВЛЯТЬ" : "ПРОПУСКАТЬ") + "\n";
            if (settings.insertIfImageExists) {
                summary += "• Позиция вставки: " + (settings.insertPosition ? "ПОСЛЕ существующей" : "ПЕРЕД существующей") + "\n";
            }
        } else {
            summary += "не обнаружены\n";
        }
        summary += "• Режим работы: " + (settings.silentMode === 1 ? "МОЛЧАЛИВЫЙ" : "ОБЫЧНЫЙ") + "\n";
        summary += "• Статистика: " + (settings.compactStats === 1 ? "КОМПАКТНАЯ" : "ПОЛНАЯ") + "\n";
        summary += "\nНачать выполнение?";
        
        if (!window.confirm(summary)) {
            return;
        }
    }
    
    // Запускаем таймер только после подтверждения (или сразу в молчаливом режиме)
    var startTime = new Date().getTime();
    
    var stats = {
        totalMoved: 0,
        totalDeleted: 0,
        emptyLinesAdded: 0,
        emptyLinesRemoved: 0,
        emptySectionsDeleted: 0,
        totalSections: analysis.totalSections,
        totalImages: analysis.totalImages,
        sectionsWithMovedImages: 0,
        sectionsSkippedExisting: 0,
        emptyImagesMoved: 0,
        emptyImagesSkipped: 0,
        noNextSection: 0,
        nextSectionNoTitle: 0,
        nextSectionHasImage: 0,
        insertedBeforeExisting: 0,
        insertedAfterExisting: 0,
        insertedAfterEpigraph: 0,
        imagesFromEndsMoved: 0,
        imagesFromSectionsMoved: 0,
        sectionsProcessed: 0,
        // Новая статистика для версии 5.1
        skippedNotesSection: 0,
        skippedEndOfDocument: 0,
        skippedNotesFollows: 0,  // Новый тип пропуска
        // Статистика по режиму переноса
        imagesFoundAtEnd: 0,
        imagesSkippedByMode: 0
    };
    
    // Начинаем блок отмены
    window.external.BeginUndoUnit(document, "Переместить иллюстрации ниже заголовков");
    
    try { 
        window.external.SetStatusBarText("Перемещаем иллюстрации ниже заголовков…"); 
    } catch(e) {}
    
    // Получаем все секции в документе
    var allSections = findAllSections(mainBody);
    
    // Обрабатываем каждую секцию
    for (var i = 0; i < safeLength(allSections); i++) {
        stats.sectionsProcessed++;
        
        var currentSection = allSections[i];
        
        // Пропускаем секции в сносках (они обрабатываются отдельно)
        if (!isInMainBody(currentSection)) {
            continue;
        }
        
        // ИСПРАВЛЕНИЕ В ВЕРСИИ 5.1: Правильный поиск последних иллюстраций
        // Ищем иллюстрацию в конце секции (последнюю картинку после которой нет текста)
        var imageAndEmptyLine = findImageAndEmptyLineAtSectionEnd(currentSection);
        stats.imagesFoundAtEnd += imageAndEmptyLine.image ? 1 : 0;
        
        if (imageAndEmptyLine.image) {
            // Для режима 0 (только последняя) - обрабатываем как есть
            if (imagesToMoveCount === 1 || settings.processMode === 0) {
                // Обрабатываем только последнюю иллюстрацию
                var moved = processImageElement(imageAndEmptyLine.image, currentSection, allSections, i, settings, stats, skippedSectionsDetails);
                
                if (moved) {
                    stats.sectionsWithMovedImages++;
                    stats.totalMoved++;
                    
                    // Определяем тип перемещения
                    if (isImageSection(currentSection)) {
                        stats.imagesFromSectionsMoved++;
                    } else {
                        stats.imagesFromEndsMoved++;
                    }
                }
                
                // Удаляем пустую строку после иллюстрации, если она была
                if (imageAndEmptyLine.emptyLine) {
                    var parent = imageAndEmptyLine.emptyLine.parentNode;
                    if (parent && parent.removeChild) {
                        parent.removeChild(imageAndEmptyLine.emptyLine);
                        stats.emptyLinesRemoved++;
                    }
                }
            } 
            else if (imagesToMoveCount === 0 || settings.processMode === 1) {
                // Режим 1 или пользователь ввел 0: обрабатываем все последние иллюстрации
                var allImagesAtEnd = findAllImagesAtSectionEnd(currentSection);
                stats.imagesFoundAtEnd = safeLength(allImagesAtEnd.images);
                
                if (safeLength(allImagesAtEnd.images) > 0) {
                    // Обрабатываем все иллюстрации в правильном порядке
                    var processedCount = 0;
                    for (var j = 0; j < safeLength(allImagesAtEnd.images); j++) {
                        var imageElement = allImagesAtEnd.images[j];
                        
                        // Пропускаем иллюстрации, которые уже были удалены
                        if (!imageElement.parentNode) {
                            continue;
                        }
                        
                        var moved = processImageElement(imageElement, currentSection, allSections, i, settings, stats, skippedSectionsDetails);
                        
                        if (moved) {
                            processedCount++;
                            stats.totalMoved++;
                            
                            // Определяем тип перемещения
                            if (isImageSection(currentSection)) {
                                stats.imagesFromSectionsMoved++;
                            } else {
                                stats.imagesFromEndsMoved++;
                            }
                        }
                    }
                    
                    // Удаляем пустые строки после перемещенных иллюстраций
                    for (var k = 0; k < safeLength(allImagesAtEnd.emptyLines); k++) {
                        var emptyLine = allImagesAtEnd.emptyLines[k];
                        if (emptyLine && emptyLine.parentNode) {
                            var parent = emptyLine.parentNode;
                            if (parent && parent.removeChild) {
                                parent.removeChild(emptyLine);
                                stats.emptyLinesRemoved++;
                            }
                        }
                    }
                    
                    // Обновляем статистику
                    if (processedCount > 0) {
                        stats.sectionsWithMovedImages++;
                    }
                }
            }
            else if (imagesToMoveCount > 1) {
                // Обрабатываем указанное количество с конца
                var allImagesAtEnd = findAllImagesAtSectionEnd(currentSection);
                stats.imagesFoundAtEnd = safeLength(allImagesAtEnd.images);
                
                if (safeLength(allImagesAtEnd.images) > 0) {
                    // Определяем, сколько иллюстраций нужно обработать
                    var imagesToProcess = [];
                    var startIndex = Math.max(0, safeLength(allImagesAtEnd.images) - imagesToMoveCount);
                    
                    // Берем нужное количество с конца, сохраняя порядок
                    for (var imgIdx = startIndex; imgIdx < safeLength(allImagesAtEnd.images); imgIdx++) {
                        imagesToProcess.push(allImagesAtEnd.images[imgIdx]);
                    }
                    
                    // Обновляем статистику пропущенных
                    stats.imagesSkippedByMode += safeLength(allImagesAtEnd.images) - safeLength(imagesToProcess);
                    
                    // Обрабатываем каждую иллюстрацию из списка
                    var processedCount = 0;
                    for (var j = 0; j < safeLength(imagesToProcess); j++) {
                        var imageElement = imagesToProcess[j];
                        
                        // Пропускаем иллюстрации, которые уже были удалены
                        if (!imageElement.parentNode) {
                            continue;
                        }
                        
                        var moved = processImageElement(imageElement, currentSection, allSections, i, settings, stats, skippedSectionsDetails);
                        
                        if (moved) {
                            processedCount++;
                            stats.totalMoved++;
                            
                            // Определяем тип перемещения
                            if (isImageSection(currentSection)) {
                                stats.imagesFromSectionsMoved++;
                            } else {
                                stats.imagesFromEndsMoved++;
                            }
                        }
                    }
                    
                    // Удаляем пустые строки после перемещенных иллюстраций
                    for (var k = 0; k < safeLength(allImagesAtEnd.emptyLines); k++) {
                        var emptyLine = allImagesAtEnd.emptyLines[k];
                        if (emptyLine && emptyLine.parentNode) {
                            var parent = emptyLine.parentNode;
                            if (parent && parent.removeChild) {
                                parent.removeChild(emptyLine);
                                stats.emptyLinesRemoved++;
                            }
                        }
                    }
                    
                    // Обновляем статистику
                    if (processedCount > 0) {
                        stats.sectionsWithMovedImages++;
                    }
                }
            }
        }
    }
    
    // Удаляем оставшиеся пустые секции (только те, откуда переносили картинки)
    var emptySectionsDeleted = removeEmptySectionsAfterMoving(allSections);
    stats.emptySectionsDeleted = emptySectionsDeleted;
    
    // Завершаем блок отмены
    window.external.EndUndoUnit(document);
    
    var endTime = new Date().getTime();
    var executionTime = ((endTime - startTime) / 1000);
    var formattedTime = toFixedNumber(executionTime, 2);
    
    // Показываем статистику (всегда показываем, даже в молчаливом режиме)
    showStatistics(stats, settings, formattedTime, analysis, skippedSectionsDetails, imagesToMoveCount);
}

// НАХОДИМ ОСНОВНОЙ BODY (не notes)
function findMainBody() {
    // Ищем div с id="fbw_body" (корневой элемент FBE)
    var fbwBody = document.getElementById('fbw_body');
    if (!fbwBody) {
        // Ищем по contenteditable='true'
        var allDivs = document.getElementsByTagName('div');
        for (var i = 0; i < safeLength(allDivs); i++) {
            if (allDivs[i].getAttribute('contenteditable') === 'true') {
                fbwBody = allDivs[i];
                break;
            }
        }
    }
    
    if (!fbwBody) return null;
    
    // Ищем body внутри fbw_body (не notes) - ручной поиск
    var allDivsInFbw = fbwBody.getElementsByTagName('div');
    for (var j = 0; j < safeLength(allDivsInFbw); j++) {
        var div = allDivsInFbw[j];
        var className = div.className || '';
        if (containsStr(className, 'body')) {
            var fbname = div.getAttribute('fbname');
            // Берем первый body, который НЕ notes
            if (!fbname || fbname !== 'notes') {
                return div;
            }
        }
    }
    
    return null;
}

// Находит все секции в элементе
function findAllSections(element) {
    var sections = [];
    
    // Рекурсивно ищем все секции
    function findSectionsRecursive(node) {
        if (!node) return;
        
        if (node.nodeType == 1) {
            // Проверяем, является ли элемент секцией
            if (isSectionElement(node)) {
                sections.push(node);
            } else {
                // Ищем секции во вложенных элементах
                var children = node.childNodes;
                for (var i = 0; i < safeLength(children); i++) {
                    findSectionsRecursive(children[i]);
                }
            }
        }
    }
    
    findSectionsRecursive(element);
    return sections;
}

// Анализирует документ перед выполнением
function analyzeDocumentBefore(mainBody) {
    var stats = {
        totalSections: 0,
        totalImages: 0,
        imagesAtEndOfSections: 0,
        sectionsWithImagesAfterTitle: 0,
        imageSections: 0,
        sectionsWithTitle: 0,
        sectionsWithEpigraph: 0
    };
    
    if (!mainBody) return stats;
    
    var allSections = findAllSections(mainBody);
    stats.totalSections = safeLength(allSections);
    
    for (var i = 0; i < safeLength(allSections); i++) {
        var section = allSections[i];
        
        // Пропускаем секции в сносках
        if (!isInMainBody(section)) {
            continue;
        }
        
        // Проверяем, есть ли заголовок
        var hasTitle = findFirstTitleInSection(section) !== null;
        if (hasTitle) {
            stats.sectionsWithTitle++;
        }
        
        // Проверяем, есть ли эпиграф
        var hasEpigraph = findEpigraphInSection(section) !== null;
        if (hasEpigraph) {
            stats.sectionsWithEpigraph++;
        }
        
        // Проверяем, является ли это секцией с только иллюстрацией
        if (isImageSection(section)) {
            stats.imageSections++;
            stats.totalImages++;
        } else {
            // Для обычных секций считаем все иллюстрации
            var allImagesInSection = findAllImagesInElement(section);
            stats.totalImages += safeLength(allImagesInSection);
            
            // Проверяем иллюстрации в конце секции (упрощенная проверка)
            var imageAndEmptyLine = findImageAndEmptyLineAtSectionEnd(section);
            if (imageAndEmptyLine.image) {
                stats.imagesAtEndOfSections++;
            }
            
            // Проверяем, есть ли иллюстрация после заголовка/эпиграфа
            if (hasTitle) {
                var existingImage = findExistingImageAfterTitleOrEpigraph(section);
                if (existingImage) {
                    stats.sectionsWithImagesAfterTitle++;
                }
            }
        }
    }
    
    return stats;
}

// Проверяет, является ли элемент секцией
function isSectionElement(element) {
    if (element.nodeType != 1) return false;
    if (element.tagName.toLowerCase() != 'div') return false;
    if (!element.className) return false;
    
    var className = element.className;
    if (!containsStr(className, 'section')) return false;
    
    return true;
}

// Проверяет, является ли секция содержащей только иллюстрацией
function isImageSection(section) {
    var children = section.childNodes;
    var hasImage = false;
    var hasOtherContent = false;
    
    for (var i = 0; i < safeLength(children); i++) {
        var child = children[i];
        if (child.nodeType == 1) {
            if (isImageElement(child)) {
                hasImage = true;
            } else if (child.className && containsStr(child.className, 'title')) {
                hasOtherContent = true; // Есть заголовок
                break;
            } else if (child.className && containsStr(child.className, 'epigraph')) {
                hasOtherContent = true; // Есть эпиграф
                break;
            } else if (child.tagName.toLowerCase() == 'p') {
                var text = getElementText(child);
                if (text && trimStr(text) !== '' && 
                    trimStr(text) !== '&nbsp;' && 
                    trimStr(text) !== String.fromCharCode(160)) {
                    hasOtherContent = true; // Есть текст
                    break;
                }
            } else {
                hasOtherContent = true; // Другие элементы
                break;
            }
        } else if (child.nodeType == 3) {
            var text = child.nodeValue || '';
            if (text && trimStr(text) !== '') {
                hasOtherContent = true; // Есть текст
                break;
            }
        }
    }
    
    return hasImage && !hasOtherContent;
}

// Находит эпиграф в секции
function findEpigraphInSection(section) {
    var children = section.childNodes;
    for (var i = 0; i < safeLength(children); i++) {
        var child = children[i];
        if (child.nodeType == 1 && child.className && containsStr(child.className, 'epigraph')) {
            return child;
        }
    }
    return null;
}

// Получает текст из элемента
function getElementText(element) {
    if (element.nodeType == 3) {
        return element.nodeValue || '';
    }
    
    if (element.nodeType == 1) {
        var text = '';
        var children = element.childNodes;
        for (var i = 0; i < safeLength(children); i++) {
            text += getElementText(children[i]);
        }
        return text;
    }
    
    return '';
}

// Получает заголовок секции
function getSectionTitle(section) {
    var titleElement = findFirstTitleInSection(section);
    if (titleElement) {
        var titleText = getElementText(titleElement);
        if (titleText && trimStr(titleText) !== '') {
            // Обрезаем длинный текст до 50 символов
            var trimmedTitle = trimStr(titleText);
            if (trimmedTitle.length > 50) {
                trimmedTitle = trimmedTitle.substring(0, 47) + "...";
            }
            return trimmedTitle;
        }
    }
    
    // Если нет заголовка, пытаемся найти любой текст в секции
    var allText = getElementText(section);
    if (allText && trimStr(allText) !== '') {
        var trimmedText = trimStr(allText);
        if (trimmedText.length > 50) {
            trimmedText = trimmedText.substring(0, 47) + "...";
        }
        return trimmedText || "Без заголовка";
    }
    
    return "Без заголовка";
}

// Проверяет, является ли элемент пустым абзацем
function isEmptyParagraph(element) {
    if (element.nodeType != 1) return false;
    if (element.tagName.toLowerCase() != 'p') return false;
    
    var text = getElementText(element);
    if (!text) return true;
    
    return isWhitespaceOnly(text);
}

// Находит последнюю иллюстрацию в конце секции - ИСПРАВЛЕННАЯ ВЕРСИЯ 5.1
function findImageAndEmptyLineAtSectionEnd(section) {
    var result = {
        image: null,
        emptyLine: null
    };
    
    var children = section.childNodes;
    var childCount = safeLength(children);
    
    if (childCount === 0) return result;
    
    // Шаг 1: Найти ВСЕ картинки в секции
    var imageIndices = [];
    for (var i = 0; i < childCount; i++) {
        if (children[i].nodeType == 1 && isImageElement(children[i])) {
            imageIndices.push(i);
        }
    }
    
    if (imageIndices.length === 0) return result;
    
    // Шаг 2: Берем САМУЮ ПОСЛЕДНЮЮ картинку (ближе всех к концу секции)
    var lastImageIndex = imageIndices[imageIndices.length - 1];
    var lastImage = children[lastImageIndex];
    
    // Шаг 3: Проверяем, что после этой картинки нет текста
    var hasTextAfter = false;
    
    for (var j = lastImageIndex + 1; j < childCount; j++) {
        var child = children[j];
        
        if (child.nodeType == 3) {
            // Текстовый узел
            var text = child.nodeValue || '';
            if (!isWhitespaceOnly(text)) {
                hasTextAfter = true;
                break;
            }
        } else if (child.nodeType == 1) {
            if (isImageElement(child)) {
                // Нашли другую картинку - значит lastImage не самая последняя
                hasTextAfter = true;
                break;
            } else if (!isEmptyParagraph(child)) {
                // Любой другой непустой элемент
                hasTextAfter = true;
                break;
            }
        }
    }
    
    if (hasTextAfter) {
        // После картинки есть текст - не переносим
        return result;
    }
    
    // Шаг 4: Проверяем, есть ли пустой абзац сразу после картинки
    if (lastImageIndex + 1 < childCount) {
        var nextChild = children[lastImageIndex + 1];
        if (nextChild.nodeType == 1 && isEmptyParagraph(nextChild)) {
            result.emptyLine = nextChild;
        }
    }
    
    result.image = lastImage;
    return result;
}

// Находит ВСЕ иллюстрации в конце секции (когда после них нет текста) - НОВАЯ ФУНКЦИЯ 5.1
function findAllImagesAtSectionEnd(section) {
    var result = {
        images: [],      // Все картинки в конце секции в правильном порядке
        emptyLines: []   // Пустые строки после картинок
    };
    
    var children = section.childNodes;
    var childCount = safeLength(children);
    
    if (childCount === 0) return result;
    
    // Шаг 1: Найти ВСЕ картинки в секции
    var allImages = [];
    for (var i = 0; i < childCount; i++) {
        if (children[i].nodeType == 1 && isImageElement(children[i])) {
            allImages.push({
                element: children[i],
                index: i
            });
        }
    }
    
    if (allImages.length === 0) return result;
    
    // Шаг 2: Идем с конца и ищем непрерывную последовательность картинок без текста после них
    var lastImageGroupEnd = -1;
    var hasTextAfterLastGroup = false;
    
    // Начинаем с последней картинки
    for (var imgIdx = allImages.length - 1; imgIdx >= 0; imgIdx--) {
        var currentImage = allImages[imgIdx];
        var imageIndex = currentImage.index;
        
        // Проверяем, что после этой картинки нет текста
        var hasTextAfter = false;
        
        for (var j = imageIndex + 1; j < childCount; j++) {
            var child = children[j];
            
            if (child.nodeType == 3) {
                // Текстовый узел
                var text = child.nodeValue || '';
                if (!isWhitespaceOnly(text)) {
                    hasTextAfter = true;
                    break;
                }
            } else if (child.nodeType == 1) {
                if (isImageElement(child)) {
                    // Это другая картинка - продолжаем проверять
                    continue;
                } else if (!isEmptyParagraph(child)) {
                    // Любой другой непустой элемент
                    hasTextAfter = true;
                    break;
                }
            }
        }
        
        if (hasTextAfter) {
            // После этой картинки есть текст - останавливаемся
            hasTextAfterLastGroup = true;
            break;
        } else {
            // После этой картинки нет текста - она в конце
            lastImageGroupEnd = imageIndex;
        }
    }
    
    if (lastImageGroupEnd === -1) return result;
    
    // Шаг 3: Найти начало группы картинок
    var firstImageInGroupIndex = lastImageGroupEnd;
    
    // Идем назад от последней картинки в группе
    for (var i = lastImageGroupEnd - 1; i >= 0; i--) {
        var child = children[i];
        
        if (child.nodeType == 1 && isImageElement(child)) {
            // Это картинка - продолжаем группу
            firstImageInGroupIndex = i;
        } else if (child.nodeType == 3) {
            var text = child.nodeValue || '';
            if (!isWhitespaceOnly(text)) {
                // Нашли текст - останавливаемся
                break;
            }
        } else if (child.nodeType == 1) {
            if (!isEmptyParagraph(child)) {
                // Нашли непустой элемент - останавливаемся
                break;
            }
        }
    }
    
    // Шаг 4: Собираем все картинки из найденной группы (в правильном порядке!)
    for (var i = firstImageInGroupIndex; i < childCount; i++) {
        var child = children[i];
        
        if (child.nodeType == 1 && isImageElement(child)) {
            result.images.push(child);
        } else {
            // Если нашли не картинку, проверяем что это
            if (child.nodeType == 3) {
                var text = child.nodeValue || '';
                if (!isWhitespaceOnly(text)) {
                    // Нашли текст - прерываем сбор картинок
                    break;
                }
            } else if (child.nodeType == 1) {
                if (!isEmptyParagraph(child)) {
                    // Нашли непустой элемент - прерываем сбор картинок
                    break;
                }
            }
        }
    }
    
    // Шаг 5: Собираем пустые строки после группы картинок
    var lastImageIndexInGroup = firstImageInGroupIndex + result.images.length - 1;
    for (var i = lastImageIndexInGroup + 1; i < childCount; i++) {
        var child = children[i];
        
        if (child.nodeType == 1 && isEmptyParagraph(child)) {
            result.emptyLines.push(child);
        } else {
            // Прерываемся при первом не пустом абзаце
            break;
        }
    }
    
    return result;
}

// Получает иллюстрацию из секции
function getImageFromSection(section) {
    var children = section.childNodes;
    for (var i = 0; i < safeLength(children); i++) {
        var child = children[i];
        if (child.nodeType == 1 && isImageElement(child)) {
            return child;
        }
    }
    return null;
}

// Обрабатывает иллюстрацию - ОБНОВЛЕННАЯ ВЕРСИЯ 5.1
function processImageElement(imageElement, sourceSection, allSections, currentIndex, settings, stats, skippedSectionsDetails) {
    // Находим следующую секцию с обновленной логикой
    var nextSectionInfo = findNextSection(allSections, currentIndex, sourceSection);
    var nextSection = nextSectionInfo.section;
    
    if (nextSectionInfo.reason !== "valid") {
        // Сохраняем информацию о пропущенной секции
        var sectionTitle = getSectionTitle(sourceSection);
        var formattedTitle = "Секция \"" + formatSectionTitle(sectionTitle, 30) + "\"";
        
        // Проверяем, нет ли уже такой записи
        var alreadyExists = false;
        for (var j = 0; j < safeLength(skippedSectionsDetails); j++) {
            if (skippedSectionsDetails[j].sectionTitle === sectionTitle && 
                skippedSectionsDetails[j].reason === nextSectionInfo.reason) {
                alreadyExists = true;
                break;
            }
        }
        
        if (!alreadyExists) {
            skippedSectionsDetails.push({
                sectionTitle: sectionTitle,
                formattedTitle: formattedTitle,
                reason: nextSectionInfo.reason,
                message: nextSectionInfo.message
            });
        }
        
        // Обновляем статистику (новая логика для версии 5.1)
        if (nextSectionInfo.reason === "notes_section") {
            stats.skippedNotesSection++;
        } else if (nextSectionInfo.reason === "end_of_document") {
            stats.skippedEndOfDocument++;
        } else if (nextSectionInfo.reason === "notes_follows") {
            stats.skippedNotesFollows++;
        }
        
        return false;
    }
    
    // Проверяем, что следующая секция имеет заголовок
    var nextHasTitle = findFirstTitleInSection(nextSection) !== null;
    if (!nextHasTitle) {
        stats.nextSectionNoTitle++;
        
        // Сохраняем информацию о пропущенной секции
        var sectionTitle = getSectionTitle(sourceSection);
        var formattedTitle = "Секция \"" + formatSectionTitle(sectionTitle, 30) + "\"";
        
        // Проверяем, нет ли уже такой записи
        var alreadyExists = false;
        for (var k = 0; k < safeLength(skippedSectionsDetails); k++) {
            if (skippedSectionsDetails[k].sectionTitle === sectionTitle && 
                skippedSectionsDetails[k].reason === "no_title") {
                alreadyExists = true;
                break;
            }
        }
        
        if (!alreadyExists) {
            skippedSectionsDetails.push({
                sectionTitle: sectionTitle,
                formattedTitle: formattedTitle,
                reason: "no_title",
                message: "Следующая секция без заголовка"
            });
        }
        
        return false;
    }
    
    // Проверяем, есть ли уже иллюстрация после заголовка/эпиграфа следующей секции
    var existingImage = findExistingImageAfterTitleOrEpigraph(nextSection);
    
    // Если есть существующая иллюстрация и не разрешена вставка - пропускаем
    if (existingImage && !settings.insertIfImageExists) {
        stats.nextSectionHasImage++;
        stats.sectionsSkippedExisting++;
        
        // Сохраняем информацию о пропущенной секции
        var sectionTitle = getSectionTitle(sourceSection);
        var formattedTitle = "Секция \"" + formatSectionTitle(sectionTitle, 30) + "\"";
        
        // Проверяем, нет ли уже такой записи
        var alreadyExists = false;
        for (var m = 0; m < safeLength(skippedSectionsDetails); m++) {
            if (skippedSectionsDetails[m].sectionTitle === sectionTitle && 
                skippedSectionsDetails[m].reason === "has_image") {
                alreadyExists = true;
                break;
            }
        }
        
        if (!alreadyExists) {
            skippedSectionsDetails.push({
                sectionTitle: sectionTitle,
                formattedTitle: formattedTitle,
                reason: "has_image",
                message: "Следующая секция уже имеет иллюстрацию"
            });
        }
        
        return false;
    }
    
    // Проверяем, пустая ли иллюстрация
    var isEmpty = isImageEmpty(imageElement);
    
    // Пропускаем пустые, если не нужно их перемещать
    if (isEmpty && !settings.moveEmptyImages) {
        stats.emptyImagesSkipped++;
        return false;
    }
    
    // Перемещаем иллюстрацию
    var moved = moveImageToNextSection(
        imageElement, 
        sourceSection, 
        nextSection, 
        settings, 
        stats,
        existingImage
    );
    
    if (moved) {
        if (isEmpty) {
            stats.emptyImagesMoved++;
        }
        
        return true;
    }
    
    return false;
}

// Находит все иллюстрации в элементе
function findAllImagesInElement(element) {
    var images = [];
    if (!element || element.nodeType != 1) return images;
    
    var children = element.childNodes;
    for (var i = 0; i < safeLength(children); i++) {
        var child = children[i];
        if (child.nodeType == 1) {
            if (isImageElement(child)) {
                images.push(child);
            } else {
                // Рекурсивно ищем во вложенных элементах
                var nestedImages = findAllImagesInElement(child);
                for (var j = 0; j < safeLength(nestedImages); j++) {
                    images.push(nestedImages[j]);
                }
            }
        }
    }
    
    return images;
}

// Проверяет, находится ли секция в разделе сносок
function isInNotesBody(element) {
    var current = element;
    while (current && current.nodeType == 1) {
        if (current.getAttribute('fbname') === 'notes') {
            return true;
        }
        current = current.parentNode;
    }
    return false;
}

// Находит первый заголовок в секции
function findFirstTitleInSection(section) {
    var children = section.childNodes;
    for (var i = 0; i < safeLength(children); i++) {
        var child = children[i];
        if (child.nodeType == 1 && child.className && containsStr(child.className, 'title')) {
            return child;
        }
    }
    return null;
}

// Находит существующую иллюстрацию после заголовка или эпиграфа
function findExistingImageAfterTitleOrEpigraph(section) {
    // Сначала ищем эпиграф
    var epigraph = findEpigraphInSection(section);
    
    if (epigraph) {
        // Если есть эпиграф, ищем иллюстрацию после него
        var nextElement = epigraph.nextSibling;
        while (nextElement) {
            if (nextElement.nodeType == 1) {
                if (isImageElement(nextElement)) {
                    return nextElement;
                }
                break; // Нашли не иллюстрацию
            } else if (nextElement.nodeType == 3) {
                var text = nextElement.nodeValue || '';
                var trimmedText = text.replace(/^\s+|\s+$/g, '');
                if (trimmedText !== '') {
                    break; // Нашли текст
                }
            }
            nextElement = nextElement.nextSibling;
        }
    } else {
        // Если нет эпиграфа, ищем после заголовка
        var title = findFirstTitleInSection(section);
        if (!title) return null;
        
        var nextElement = title.nextSibling;
        while (nextElement) {
            if (nextElement.nodeType == 1) {
                if (isImageElement(nextElement)) {
                    return nextElement;
                }
                break; // Нашли не иллюстрации
            } else if (nextElement.nodeType == 3) {
                var text = nextElement.nodeValue || '';
                var trimmedText = text.replace(/^\s+|\s+$/g, '');
                if (trimmedText !== '') {
                    break; // Нашли текст
                }
            }
            nextElement = nextElement.nextSibling;
        }
    }
    
    return null;
}

// Проверяет, является ли элемент иллюстрацией
function isImageElement(element) {
    if (element.nodeType != 1) return false;
    if (element.tagName.toLowerCase() != 'div') return false;
    if (!element.className) return false;
    
    var className = element.className;
    if (!containsStr(className, 'image')) return false;
    
    return true;
}

// Создает пустой абзац
function createEmptyLine() {
    var emptyLine = document.createElement('p');
    emptyLine.innerHTML = '&nbsp;';
    return emptyLine;
}

// Находит элемент для вставки перед существующей картинкой
function findInsertPositionForExistingImage(section, existingImage, insertPosition) {
    if (insertPosition === 0) {
        // Вставлять ПЕРЕД существующей картинкой
        return existingImage;
    } else {
        // Вставлять ПОСЛЕ существующей картинки
        var nextSibling = existingImage.nextSibling;
        
        // Проверяем, является ли следующий элемент пустым абзацем
        if (nextSibling && nextSibling.nodeType == 1 && isEmptyParagraph(nextSibling)) {
            // Если после картинки уже есть пустой абзац, вставляем после него
            return nextSibling.nextSibling;
        }
        
        // Иначе вставляем просто после картинки
        return nextSibling;
    }
}

// Находит первый текстовый элемент после заголовка/эпиграфа
function findFirstTextElementAfterTitleOrEpigraph(section) {
    // Сначала ищем эпиграф
    var epigraph = findEpigraphInSection(section);
    var startElement = epigraph ? epigraph : findFirstTitleInSection(section);
    
    if (!startElement) return null;
    
    var nextElement = startElement.nextSibling;
    while (nextElement) {
        if (nextElement.nodeType == 1) {
            // Проверяем, является ли это абзацем или другим текстовым блоком
            var className = nextElement.className || '';
            if (containsStr(className, 'p') || containsStr(className, 'empty-line') || 
                containsStr(className, 'subtitle') || containsStr(className, 'cite') ||
                containsStr(className, 'poem') || containsStr(className, 'epigraph') ||
                className === '') { // Пустой класс может быть у простого <p>
                // Проверяем, есть ли текст
                var text = getElementText(nextElement);
                if (text && text.replace(/^\s+|\s+$/g, '') !== '') {
                    return nextElement;
                }
            }
            // Если это не текст, а например изображение - продолжаем поиск
        } else if (nextElement.nodeType == 3) {
            var text = nextElement.nodeValue || '';
            var trimmedText = text.replace(/^\s+|\s+$/g, '');
            if (trimmedText !== '') {
                return nextElement;
            }
        }
        nextElement = nextElement.nextSibling;
    }
    
    return null;
}

// Перемещает иллюстрацию в следующую секцию
function moveImageToNextSection(imageElement, sourceSection, targetSection, settings, stats, existingImage) {
    // СОЗДАЕМ ПОЛНУЮ КОПИЮ картинки перед удалением
    var imageCopy = imageElement.cloneNode(true);
    
    var insertBeforeElement = null;
    
    if (existingImage && settings.insertIfImageExists) {
        // Если есть существующая иллюстрация и разрешена вставка
        insertBeforeElement = findInsertPositionForExistingImage(targetSection, existingImage, settings.insertPosition);
        
        // Статистика по позиции вставки
        if (settings.insertPosition === 0) {
            stats.insertedBeforeExisting++;
            
            // Добавляем пустую строку между двумя картинками, если вставляем ПЕРЕД
            var emptyLine = createEmptyLine();
            
            // Вставляем пустую строку между картинками
            if (existingImage.previousSibling && isImageElement(existingImage.previousSibling)) {
                targetSection.insertBefore(emptyLine, existingImage);
                stats.emptyLinesAdded++;
                insertBeforeElement = emptyLine.nextSibling;
            }
        } else {
            stats.insertedAfterExisting++;
            
            // Всегда добавляем пустую строку между двумя картинками, если вставляем ПОСЛЕ
            var emptyLine = createEmptyLine();
            
            // Вставляем пустую строку между картинками
            if (existingImage.nextSibling) {
                targetSection.insertBefore(emptyLine, existingImage.nextSibling);
            } else {
                targetSection.appendChild(emptyLine);
            }
            stats.emptyLinesAdded++;
            
            insertBeforeElement = emptyLine.nextSibling;
        }
    } else {
        // Стандартная вставка: сразу после заголовка/эпиграфа
        insertBeforeElement = findFirstTextElementAfterTitleOrEpigraph(targetSection);
        
        // Проверяем, есть ли уже картинка на месте вставки
        if (insertBeforeElement && isImageElement(insertBeforeElement)) {
            var emptyLine = createEmptyLine();
            targetSection.insertBefore(emptyLine, insertBeforeElement);
            stats.emptyLinesAdded++;
            insertBeforeElement = emptyLine.nextSibling;
        } else if (!insertBeforeElement) {
            var children = targetSection.childNodes;
            var lastChild = children[safeLength(children) - 1];
            
            if (lastChild && isImageElement(lastChild)) {
                var emptyLine = createEmptyLine();
                targetSection.appendChild(emptyLine);
                stats.emptyLinesAdded++;
                insertBeforeElement = null;
            }
        }
        
        stats.insertedBeforeExisting++;
    }
    
    // Удаляем оригинальную иллюстрацию из исходной секции
    var parent = imageElement.parentNode;
    if (parent && parent.removeChild) {
        parent.removeChild(imageElement);
        
        if (isImageSection(sourceSection) && sourceSection.childNodes.length === 0) {
            var grandParent = sourceSection.parentNode;
            if (grandParent && grandParent.removeChild) {
                grandParent.removeChild(sourceSection);
                stats.totalDeleted++;
            }
        } else if (!settings.keepOldImages) {
            stats.totalDeleted++;
        }
    }
    
    // Вставляем КОПИЮ картинки в целевую секцию
    if (insertBeforeElement && targetSection.insertBefore) {
        targetSection.insertBefore(imageCopy, insertBeforeElement);
        
        // Проверяем, не образовались ли две картинки подряд после вставки
        var nextSibling = imageCopy.nextSibling;
        if (nextSibling && isImageElement(nextSibling)) {
            var emptyLine = createEmptyLine();
            targetSection.insertBefore(emptyLine, nextSibling);
            stats.emptyLinesAdded++;
        }
        
        var prevSibling = imageCopy.previousSibling;
        if (prevSibling && isImageElement(prevSibling)) {
            var emptyLine = createEmptyLine();
            targetSection.insertBefore(emptyLine, imageCopy);
            stats.emptyLinesAdded++;
        }
    } else if (targetSection.appendChild) {
        targetSection.appendChild(imageCopy);
        
        var children = targetSection.childNodes;
        var childCount = safeLength(children);
        if (childCount > 1) {
            var prevSibling = children[childCount - 2];
            if (prevSibling && isImageElement(prevSibling)) {
                var emptyLine = createEmptyLine();
                targetSection.insertBefore(emptyLine, imageCopy);
                stats.emptyLinesAdded++;
            }
        }
    } else {
        return false;
    }
    
    // Если нужно оставить старые иллюстрации, создаем копию на старом месте
    if (settings.keepOldImages && parent && parent.appendChild) {
        var imageCopy2 = imageElement.cloneNode(true);
        parent.appendChild(imageCopy2);
    }
    
    // Проверяем, есть ли эпиграф в целевой секции
    var epigraph = findEpigraphInSection(targetSection);
    if (epigraph) {
        stats.insertedAfterEpigraph++;
    }
    
    return true;
}

// Проверяет, является ли иллюстрация пустой (#undefined)
function isImageEmpty(imageElement) {
    var href = imageElement.getAttribute('href');
    if (href === '#undefined') return true;
    
    var imgs = imageElement.getElementsByTagName('img');
    for (var i = 0; i < safeLength(imgs); i++) {
        var img = imgs[i];
        var src = img.src || '';
        if (stringContains(src, '#undefined')) {
            return true;
        }
    }
    
    return false;
}

// Удаляет пустые секции после перемещения картинок
function removeEmptySectionsAfterMoving(allSections) {
    var deletedCount = 0;
    
    for (var i = 0; i < safeLength(allSections); i++) {
        var section = allSections[i];
        
        // Пропускаем секции в сносках
        if (!isInMainBody(section)) {
            continue;
        }
        
        // Проверяем, является ли секция пустой или содержит только пустые абзацы
        if (isSectionEmptyOrOnlyEmptyParagraphs(section)) {
            var parent = section.parentNode;
            if (parent && parent.removeChild) {
                parent.removeChild(section);
                deletedCount++;
            }
        }
    }
    
    return deletedCount;
}

// Проверяет, пуста ли секция или содержит только пустые абзацы
function isSectionEmptyOrOnlyEmptyParagraphs(section) {
    var children = section.childNodes;
    if (safeLength(children) === 0) return true;
    
    var hasNonEmptyContent = false;
    
    for (var i = 0; i < safeLength(children); i++) {
        var child = children[i];
        
        if (child.nodeType == 1) {
            // Проверяем изображения
            if (isImageElement(child)) {
                hasNonEmptyContent = true;
                break;
            }
            
            // Проверяем заголовки
            if (child.className && containsStr(child.className, 'title')) {
                hasNonEmptyContent = true;
                break;
            }
            
            // Проверяем эпиграфы
            if (child.className && containsStr(child.className, 'epigraph')) {
                hasNonEmptyContent = true;
                break;
            }
            
            // Проверяем абзацы
            if (child.tagName.toLowerCase() == 'p') {
                var text = getElementText(child);
                if (text) {
                    var trimmedText = text.replace(/^\s+|\s+$/g, '');
                    // Если абзац не пустой и не содержит только неразрывные пробелы
                    if (trimmedText !== '' && 
                        trimmedText !== '&nbsp;' && 
                        trimmedText !== String.fromCharCode(160) &&
                        trimmedText !== '\u00A0') {
                        hasNonEmptyContent = true;
                        break;
                    }
                }
            } else {
                // Любой другой элемент считаем непустым
                hasNonEmptyContent = true;
                break;
            }
        } else if (child.nodeType == 3) {
            var text = child.nodeValue || '';
            var trimmedText = text.replace(/^\s+|\s+$/g, '');
            if (trimmedText !== '') {
                hasNonEmptyContent = true;
                break;
            }
        }
    }
    
    return !hasNonEmptyContent;
}

// Показывает статистику выполнения - ОБНОВЛЕННАЯ ВЕРСИЯ 5.1
function showStatistics(stats, settings, executionTime, analysis, skippedSectionsDetails, imagesToMoveCount) {
    var message = "FBE скрипт \"Переместить иллюстрации ниже заголовков\" ver. 5.1\n\n";
    message += "АНАЛИЗ ДОКУМЕНТА (до обработки):\n";
    message += "• Секций всего: " + (analysis.totalSections || 0) + "\n";
    message += "• Иллюстраций всего: " + (analysis.totalImages || 0) + "\n";
    
    // Анализ документа с учетом компактного режима
    if ((analysis.imagesAtEndOfSections || 0) > 0 || settings.compactStats === 0) {
        message += "• Иллюстраций в концах секций: " + (analysis.imagesAtEndOfSections || 0) + "\n";
    }
    if ((analysis.imageSections || 0) > 0 || settings.compactStats === 0) {
        message += "• Секций-иллюстраций: " + (analysis.imageSections || 0) + "\n";
    }
    if ((analysis.sectionsWithEpigraph || 0) > 0 || settings.compactStats === 0) {
        message += "• Секций с эпиграфами: " + (analysis.sectionsWithEpigraph || 0) + "\n";
    }
    if ((analysis.sectionsWithImagesAfterTitle || 0) > 0 || settings.compactStats === 0) {
        message += "• Секций с иллюстрациями ПОСЛЕ заголовков/эпиграфов: " + (analysis.sectionsWithImagesAfterTitle || 0) + "\n";
    }
    
    message += "\nРЕЗУЛЬТАТЫ ВЫПОЛНЕНИЯ:\n\n";
    
    // РЕЖИМ ПЕРЕНОСА
    var modeInfo = "РЕЖИМ ПЕРЕНОСА:\n";
    if (settings.processMode === 0) {
        modeInfo += "• Режим: только последняя иллюстрация\n";
    } else if (settings.processMode === 1) {
        modeInfo += "• Режим: все последние иллюстрации\n";
    } else if (settings.processMode === 2) {
        if (imagesToMoveCount === 0) {
            modeInfo += "• Режим: все последние иллюстрации (введено 0)\n";
        } else if (imagesToMoveCount === 1) {
            modeInfo += "• Режим: только последняя иллюстрация (введено 1)\n";
        } else {
            modeInfo += "• Режим: " + imagesToMoveCount + " последних иллюстраций\n";
        }
    }
    message += modeInfo + "\n";
    
    // НАЙДЕНО В КОНЦАХ СЕКЦИЙ:
    var foundSection = "НАЙДЕНО В КОНЦАХ СЕКЦИЙ:\n";
    var hasFoundItems = false;
    
    if ((stats.imagesFoundAtEnd || 0) > 0 || settings.compactStats === 0) {
        foundSection += "• Иллюстраций найдено: " + (stats.imagesFoundAtEnd || 0) + "\n";
        hasFoundItems = true;
    }
    if ((stats.imagesSkippedByMode || 0) > 0 || settings.compactStats === 0) {
        foundSection += "• Иллюстраций пропущено (по режиму): " + (stats.imagesSkippedByMode || 0) + "\n";
        hasFoundItems = true;
    }
    
    if (hasFoundItems || settings.compactStats === 0) {
        message += foundSection + "\n";
    }
    
    // ПЕРЕМЕЩЕНО:
    var movedSection = "ПЕРЕМЕЩЕНО:\n";
    var hasMovedItems = false;
    
    if ((stats.totalMoved || 0) > 0 || settings.compactStats === 0) {
        movedSection += "• Иллюстраций всего: " + (stats.totalMoved || 0) + "\n";
        hasMovedItems = true;
    }
    if ((stats.imagesFromEndsMoved || 0) > 0 || settings.compactStats === 0) {
        movedSection += "• Из концов секций: " + (stats.imagesFromEndsMoved || 0) + "\n";
        hasMovedItems = true;
    }
    if ((stats.imagesFromSectionsMoved || 0) > 0 || settings.compactStats === 0) {
        movedSection += "• Из секций-иллюстраций: " + (stats.imagesFromSectionsMoved || 0) + "\n";
        hasMovedItems = true;
    }
    if ((stats.emptyImagesMoved || 0) > 0 || settings.compactStats === 0) {
        movedSection += "• Пустых иллюстраций: " + (stats.emptyImagesMoved || 0) + "\n";
        hasMovedItems = true;
    }
    if ((stats.insertedBeforeExisting || 0) > 0 || settings.compactStats === 0) {
        movedSection += "• Перед существующей картинкой: " + (stats.insertedBeforeExisting || 0) + "\n";
        hasMovedItems = true;
    }
    if ((stats.insertedAfterExisting || 0) > 0 || settings.compactStats === 0) {
        movedSection += "• После существующей картинки: " + (stats.insertedAfterExisting || 0) + "\n";
        hasMovedItems = true;
    }
    if ((stats.insertedAfterEpigraph || 0) > 0 || settings.compactStats === 0) {
        movedSection += "• После эпиграфов: " + (stats.insertedAfterEpigraph || 0) + "\n";
        hasMovedItems = true;
    }
    
    if (hasMovedItems) {
        message += movedSection + "\n";
    }
    
    // ДОБАВЛЕНО/УДАЛЕНО:
    var addedDeletedSection = "ДОБАВЛЕНО/УДАЛЕНО:\n";
    var hasAddedDeletedItems = false;
    
    if ((stats.totalDeleted || 0) > 0 || settings.compactStats === 0) {
        addedDeletedSection += "• Старых иллюстраций удалено: " + (stats.totalDeleted || 0) + "\n";
        hasAddedDeletedItems = true;
    }
    if ((stats.emptyLinesAdded || 0) > 0 || settings.compactStats === 0) {
        addedDeletedSection += "• Пустых строк добавлено: " + (stats.emptyLinesAdded || 0) + "\n";
        hasAddedDeletedItems = true;
    }
    if ((stats.emptyLinesRemoved || 0) > 0 || settings.compactStats === 0) {
        addedDeletedSection += "• Пустых строк удалено: " + (stats.emptyLinesRemoved || 0) + "\n";
        hasAddedDeletedItems = true;
    }
    if ((stats.emptySectionsDeleted || 0) > 0 || settings.compactStats === 0) {
        addedDeletedSection += "• Пустых секций удалено: " + (stats.emptySectionsDeleted || 0) + "\n";
        hasAddedDeletedItems = true;
    }
    addedDeletedSection += "• (настройка иллюстраций: " + (settings.keepOldImages ? "ОСТАВЛЯТЬ" : "УДАЛЯТЬ") + ")\n";
    
    if (hasAddedDeletedItems || settings.compactStats === 0) {
        message += addedDeletedSection + "\n";
    }
    
    // ОБРАБОТАНО:
    var processedSection = "ОБРАБОТАНО:\n";
    var hasProcessedItems = false;
    
    if ((stats.sectionsProcessed || 0) > 0 || settings.compactStats === 0) {
        processedSection += "• Секций всего: " + (stats.sectionsProcessed || 0) + "\n";
        hasProcessedItems = true;
    }
    if ((stats.sectionsWithMovedImages || 0) > 0 || settings.compactStats === 0) {
        processedSection += "• Секций с перемещениями: " + (stats.sectionsWithMovedImages || 0) + "\n";
        hasProcessedItems = true;
    }
    if ((stats.sectionsSkippedExisting || 0) > 0 || settings.compactStats === 0) {
        processedSection += "• Секций пропущено (есть иллюстрация): " + (stats.sectionsSkippedExisting || 0) + "\n";
        hasProcessedItems = true;
    }
    // Новая статистика для версии 5.1
    if ((stats.skippedNotesSection || 0) > 0 || settings.compactStats === 0) {
        processedSection += "• Секций пропущено (раздел сносок): " + (stats.skippedNotesSection || 0) + "\n";
        hasProcessedItems = true;
    }
    if ((stats.skippedEndOfDocument || 0) > 0 || settings.compactStats === 0) {
        processedSection += "• Секций пропущено (конец документа): " + (stats.skippedEndOfDocument || 0) + "\n";
        hasProcessedItems = true;
    }
    if ((stats.skippedNotesFollows || 0) > 0 || settings.compactStats === 0) {
        processedSection += "• Секций пропущено (после раздела сносок): " + (stats.skippedNotesFollows || 0) + "\n";
        hasProcessedItems = true;
    }
    processedSection += "• (настройка: " + (settings.insertIfImageExists ? "ВСТАВЛЯТЬ" : "ПРОПУСКАТЬ") + ")\n";
    
    if (hasProcessedItems || settings.compactStats === 0) {
        message += processedSection + "\n";
    }
    
    // ПРОПУЩЕНО ПО ПРИЧИНАМ:
    var skippedDetailsLength = safeLength(skippedSectionsDetails);
    if (skippedDetailsLength > 0 || settings.compactStats === 0) {
        var skippedSection = "ПРОПУЩЕНО ПО ПРИЧИНАМ:\n";
        
        // Группируем по причинам
        var reasons = {};
        for (var i = 0; i < skippedDetailsLength; i++) {
            var detail = skippedSectionsDetails[i];
            if (detail && detail.reason) {
                if (!reasons[detail.reason]) {
                    reasons[detail.reason] = {
                        message: detail.message || "Неизвестная причина",
                        sections: []
                    };
                }
                if (detail.formattedTitle) {
                    reasons[detail.reason].sections.push(detail.formattedTitle);
                }
            }
        }
        
        // Выводим каждую причину
        var hasReasons = false;
        for (var reason in reasons) {
            if (reasons.hasOwnProperty(reason)) {
                var reasonInfo = reasons[reason];
                var reasonSectionsLength = safeLength(reasonInfo.sections);
                
                if (reasonSectionsLength > 0) {
                    // Преобразуем коды причин в читаемые сообщения
                    var readableReason = "";
                    if (reason === "end_of_document") {
                        readableReason = "Достигнут конец документа (нет раздела сносок)";
                    } else if (reason === "notes_follows") {
                        readableReason = "После основного раздела идет раздел сносок";
                    } else if (reason === "notes_section") {
                        readableReason = "Текущая секция находится в разделе сносок";
                    } else if (reason === "no_title") {
                        readableReason = "Следующая секция без заголовка";
                    } else if (reason === "has_image") {
                        readableReason = "Следующая секция уже имеет иллюстрацию";
                    } else {
                        readableReason = reasonInfo.message;
                    }
                    
                    skippedSection += "• " + readableReason + ": " + reasonSectionsLength + "\n";
                    hasReasons = true;
                    
                    // Выводим до 5 секций
                    var maxSectionsToShow = 5;
                    var sectionsToShow = Math.min(maxSectionsToShow, reasonSectionsLength);
                    
                    for (var j = 0; j < sectionsToShow; j++) {
                        if (reasonInfo.sections[j]) {
                            skippedSection += "  - " + reasonInfo.sections[j] + "\n";
                        }
                    }
                    
                    // Если секций больше 5, показываем общее количество
                    if (reasonSectionsLength > maxSectionsToShow) {
                        skippedSection += "  - ... и еще " + (reasonSectionsLength - maxSectionsToShow) + " секций (всего " + reasonSectionsLength + ")\n";
                    }
                }
            }
        }
        
        if (hasReasons) {
            skippedSection += "• (настройка пустых иллюстраций: " + (settings.moveEmptyImages ? "ПЕРЕНОСИТЬ" : "НЕ переносить") + ")\n";
            message += skippedSection + "\n";
        }
    }
    
    message += "Время выполнения: " + executionTime + " сек.";
    
    window.external.MsgBox(message);
}
