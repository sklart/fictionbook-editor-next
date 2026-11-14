// Скрипт "Вставить заданные иллюстрации в началах и концах секций"
// version 2.9
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для расстановки указанных иллюстраций
// в началах и концах всех секций fb2 документа, кроме раздела сносок-примечаний.
// По умолчанию в безымянных секциях (без заголовков) расстановка тоже не производится.
// Для начала секций по умолчанию задано название файла start (можно указать ниже в настройках любое другое)
// Для конца секций по умолчанию задано название файла end (можно указать ниже в настройках любое другое)
// Расширение файлов может быть любым поддерживаемым (jpg или png), скрипт сам найдет нужное.
// Регистр букв в названии данных файлов не имеет значения.
// Соответствующие иллюстрации должны быть заранее прикреплены к fb2 файлу.
// Если после заголовка секции уже есть какая-то картинка, в том числе пустая,
// скрипт вставит стартовую картинку ПЕРЕД этой уже имеющейся и добавит между ними пустую строку.
// Аналогично для концов секций - конечная картинка вставляется в самом конце секции - 
// ПОСЛЕ всех существующих элементов.
// Поддержка отмены действий (Ctrl+Z).

// version 2.9, 04.01.2026
//======================================

function Run() {
    // НАСТРОЙКИ: укажите здесь имена файлов иллюстраций (без расширения!)
    // Пример: "start" для start.png, START.jpg, Start.JPEG и т.д.
    // Пример: "end" для end.png, END.jpg, End.JPEG и т.д.
    var START_IMAGE_NAME = "start";    // Имя файла для иллюстрации в начале секции (без расширения)
    var END_IMAGE_NAME = "end";        // Имя файла для иллюстрации в конце секции (без расширения)
    
    // НАСТРОЙКА: расставлять ли иллюстрации в безымянных секциях (без заголовка)
    var INSERT_IN_NAMELESS_SECTIONS = 0;   // 1 - да, 0 - нет (по умолчанию)
    
    // Сначала проверяем существование иллюстраций в документе
    var imagesExist = checkImagesExist(START_IMAGE_NAME, END_IMAGE_NAME);
    
    if (!imagesExist.startFound && !imagesExist.endFound) {
        MsgBox("Иллюстрации не найдены в документе!\n\n" +
               "Перед запуском скрипта необходимо:\n" +
               "1. Добавить иллюстрации в документ\n" +
               "2. Указать правильные имена файлов в настройках скрипта\n\n" +
               "Указанные имена (без расширения): \n" +
               "• \"" + START_IMAGE_NAME + "\" - для начала секций\n" +
               "• \"" + END_IMAGE_NAME + "\" - для конца секций\n\n" +
               "Скрипт ищет файлы с расширениями: .png, .jpg, .jpeg");
        return;
    }
    
    // Определяем какие иллюстрации можно расставлять
    var canInsertStart = imagesExist.startFound;
    var canInsertEnd = imagesExist.endFound;
    
    // ВЕРНУЛ информационное окно
    if (canInsertStart && canInsertEnd) {
        MsgBox("Скрипт \"Вставить заданные иллюстрации в началах и концах секций\"\n" +
               "ver. 2.9\n\n" +
               "Найдены обе иллюстрации:\n" +
               "• " + imagesExist.startActualName + " - для начала секций\n" +
               "• " + imagesExist.endActualName + " - для конца секций\n\n" +
               "Продолжить расстановку?");
    } else if (canInsertStart) {
        MsgBox("Скрипт \"Вставить заданные иллюстрации в началах и концах секций\"\n" +
               "ver. 2.9\n\n" +
               "Найдена иллюстрация для начала секций: " + imagesExist.startActualName + "\n\n" +
               "Продолжить расстановку?");
    } else if (canInsertEnd) {
        MsgBox("Скрипт \"Вставить заданные иллюстрации в началах и концах секций\"\n" +
               "ver. 2.9\n\n" +
               "Найдена иллюстрация для конца секций: " + imagesExist.endActualName + "\n\n" +
               "Продолжить расстановку?");
    }
    
    var insertStart = false;
    var insertEnd = false;
    
    // Простой выбор через последовательные AskYesNo
    if (canInsertStart && canInsertEnd) {
        // Сначала спрашиваем про расстановку в началах
        var answer1 = window.external.AskYesNo(
            "Расставить иллюстрации в НАЧАЛАХ секций?\n\n" +
            "Да - расставить \"" + imagesExist.startActualName + "\" в началах\n" +
            "Нет - пропустить расстановку в началах"
        );
        
        insertStart = (answer1 == 1); // 1=Да, 0=Нет
        
        // Затем спрашиваем про расстановку в концах
        var answer2 = window.external.AskYesNo(
            "Расставить иллюстрации в КОНЦАХ секций?\n\n" +
            "Да - расставить \"" + imagesExist.endActualName + "\" в концах\n" +
            "Нет - пропустить расстановку в концах"
        );
        
        insertEnd = (answer2 == 1); // 1=Да, 0=Нет
    } else if (canInsertStart) {
        // Только стартовая найдена
        var answer = window.external.AskYesNo(
            "Расставить иллюстрации в началах секций?\n\n" +
            "Да - расставить\n" +
            "Нет - отмена"
        );
        
        if (answer == 1) {
            insertStart = true;
            insertEnd = false;
        } else {
            return; // Отмена
        }
    } else if (canInsertEnd) {
        // Только конечная найдена
        var answer = window.external.AskYesNo(
            "Расставить иллюстрации в концах секций?\n\n" +
            "Да - расставить\n" +
            "Нет - отмена"
        );
        
        if (answer == 1) {
            insertStart = false;
            insertEnd = true;
        } else {
            return; // Отмена
        }
    }
    
    // Если не выбрано ничего
    if (!insertStart && !insertEnd) {
        MsgBox("Не выбрано ни одной опции для расстановку. Скрипт завершен.");
        return;
    }
    
    // Запускаем таймер ПОСЛЕ всех подтверждений
    var startTime = new Date();
    
    var stats = {
        totalInserted: 0,
        startInserted: 0,
        endInserted: 0,
        skippedNotes: 0,
        skippedSections: 0,
        skippedNameless: 0,
        emptyLinesAdded: 0,
        sectionsProcessed: 0,
        insertStart: insertStart,
        insertEnd: insertEnd,
        startImageName: imagesExist.startActualName,
        endImageName: imagesExist.endActualName
    };
    
    window.external.BeginUndoUnit(document, "Вставка заданных иллюстраций");
    
    try {
        window.external.SetStatusBarText("Вставляем иллюстрации...");
    } catch(e) {}
    
    // Находим все секции в основном теле документа
    var sections = [];
    var allDivs = document.getElementsByTagName('div');
    
    for (var i = 0; i < allDivs.length; i++) {
        var div = allDivs[i];
        if (div.className && div.className.toString().indexOf('section') != -1) {
            // Проверяем, что секция НЕ в сносках
            if (!isInNotesBody(div)) {
                sections.push(div);
            } else {
                stats.skippedNotes++;
            }
        }
    }
    
    stats.sectionsProcessed = sections.length;
    
    // Обрабатываем секции
    for (var j = 0; j < sections.length; j++) {
        var section = sections[j];
        var hasTitle = findFirstTitleInSection(section) !== null;
        
        // Пропускаем безымянные секции если настроено так
        if (!hasTitle && INSERT_IN_NAMELESS_SECTIONS == 0) {
            stats.skippedNameless++;
            continue;
        }
        
        // Вставка в начало секции
        if (insertStart && hasTitle) {
            var insertPosition = findInsertPositionAtStart(section);
            if (insertPosition) {
                if (!hasImageAtPosition(insertPosition.nextElement, stats.startImageName)) {
                    if (insertRealImageAtPosition(section, insertPosition, stats.startImageName)) {
                        stats.startInserted++;
                        
                        // Проверяем, нужно ли добавить пустую строку после вставки
                        checkAndAddEmptyLine(section, insertPosition.element, stats);
                    }
                }
            } else {
                stats.skippedSections++;
            }
        }
        
        // Вставка в конец секции
        if (insertEnd) {
            if (!hasImageAtEnd(section, stats.endImageName)) {
                if (insertRealImageAtEnd(section, stats.endImageName)) {
                    stats.endInserted++;
                    
                    // Проверяем, нужно ли добавить пустую строку перед вставкой
                    checkAndAddEmptyLineBefore(section, stats);
                }
            }
        }
    }
    
    window.external.EndUndoUnit(document);
    
    stats.totalInserted = stats.startInserted + stats.endInserted;
    
    // Выводим статистику
    var endTime = new Date();
    var timeDiff = endTime - startTime;
    var timeSec = (timeDiff / 1000).toFixed(2).replace('.', ',') + " сек";
    
    var resultMessage = "Скрипт \"Вставить заданные иллюстрации в началах и концах секций\"\n" +
                       "ver. 2.9\n\n";
    resultMessage += "Время выполнения: " + timeSec + "\n\n";
    resultMessage += "Обработано секций: " + stats.sectionsProcessed + "\n";
    resultMessage += "Всего расставлено иллюстраций: " + stats.totalInserted + "\n\n";
    
    if (insertStart) {
        resultMessage += "Иллюстраций в началах секций (\"" + stats.startImageName + "\"): " + stats.startInserted + "\n";
    } else {
        resultMessage += "Иллюстраций в началах секций: не расставлялись\n";
    }
    
    if (insertEnd) {
        resultMessage += "Иллюстраций в концах секций (\"" + stats.endImageName + "\"): " + stats.endInserted + "\n";
    } else {
        resultMessage += "Иллюстраций в концах секций: не расставлялись\n";
    }
    
    resultMessage += "Пустых строк добавено: " + stats.emptyLinesAdded + "\n";
    resultMessage += "\n";
    resultMessage += "Пропущено секций в сносках: " + stats.skippedNotes + "\n";
    
    if (INSERT_IN_NAMELESS_SECTIONS == 0 && stats.skippedNameless > 0) {
        resultMessage += "Пропущено безымянных секций: " + stats.skippedNameless + "\n";
    }
    
    if (stats.skippedSections > 0) {
        resultMessage += "Пропущено секций (другие причины): " + stats.skippedSections + "\n";
    }
    
    MsgBox(resultMessage);
}

// Удаляет расширение из имени файла
function removeExtension(filename) {
    // Находим последнюю точку в имени
    var lastDotIndex = filename.lastIndexOf('.');
    if (lastDotIndex != -1) {
        return filename.substring(0, lastDotIndex);
    }
    return filename;
}

// Проверяет существование иллюстраций в документе (регистронезависимо, ищет .png/.jpg/.jpeg)
function checkImagesExist(startImageName, endImageName) {
    var result = {
        startFound: false,
        endFound: false,
        startActualName: "",  // Оригинальное имя из документа
        endActualName: ""     // Оригинальное имя из документа
    };
    
    // Убираем расширение если оно есть (на случай если пользователь указал с расширением)
    var startBaseName = removeExtension(startImageName).toLowerCase();
    var endBaseName = removeExtension(endImageName).toLowerCase();
    
    // Массив поддерживаемых расширений
    var supportedExtensions = ['.png', '.jpg', '.jpeg'];
    
    // Ищем бинарные объекты (иллюстрации) как в main.js
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
                    
                    // Проверяем ID изображения (регистронезависимо, без учета расширения)
                    if (imageIdWithoutExt === startBaseName) {
                        // Проверяем расширение
                        for (var extIdx = 0; extIdx < supportedExtensions.length; extIdx++) {
                            if (imageIdLower.indexOf(supportedExtensions[extIdx], imageIdWithoutExt.length) != -1) {
                                result.startFound = true;
                                result.startActualName = imageId; // Сохраняем оригинальное имя
                                break;
                            }
                        }
                    }
                    
                    if (imageIdWithoutExt === endBaseName) {
                        // Проверяем расширение
                        for (var extIdx = 0; extIdx < supportedExtensions.length; extIdx++) {
                            if (imageIdLower.indexOf(supportedExtensions[extIdx], imageIdWithoutExt.length) != -1) {
                                result.endFound = true;
                                result.endActualName = imageId; // Сохраняем оригинальное имя
                                break;
                            }
                        }
                    }
                    
                    // Если нашли обе, можно прекратить поиск
                    if (result.startFound && result.endFound) {
                        return result;
                    }
                }
            }
        }
    }
    
    // Если не нашли в binobj, пробуем другие способы
    if (!result.startFound || !result.endFound) {
        // Ищем элементы image в документе
        var allImages = document.getElementsByTagName('image');
        for (var i = 0; i < allImages.length; i++) {
            var href = allImages[i].getAttribute('href');
            if (href && href.charAt(0) === '#') {
                var imageName = href.substring(1); // Убираем # в начале
                var imageNameLower = imageName.toLowerCase();
                var imageNameWithoutExt = removeExtension(imageNameLower);
                
                if (imageNameWithoutExt === startBaseName && !result.startFound) {
                    // Проверяем расширение
                    for (var extIdx = 0; extIdx < supportedExtensions.length; extIdx++) {
                        if (imageNameLower.indexOf(supportedExtensions[extIdx], imageNameWithoutExt.length) != -1) {
                            result.startFound = true;
                            result.startActualName = imageName; // Сохраняем оригинальное имя
                            break;
                        }
                    }
                }
                
                if (imageNameWithoutExt === endBaseName && !result.endFound) {
                    // Проверяем расширение
                    for (var extIdx = 0; extIdx < supportedExtensions.length; extIdx++) {
                        if (imageNameLower.indexOf(supportedExtensions[extIdx], imageNameWithoutExt.length) != -1) {
                            result.endFound = true;
                            result.endActualName = imageName; // Сохраняем оригинальное имя
                            break;
                        }
                    }
                }
            }
        }
        
        // Ищем div с классом image
        var allDivs = document.getElementsByTagName('div');
        for (var i = 0; i < allDivs.length; i++) {
            var div = allDivs[i];
            var className = div.className ? div.className.toString() : '';
            if (className.indexOf('image') != -1) {
                var href = div.getAttribute('href');
                if (href && href.charAt(0) === '#') {
                    var imageName = href.substring(1); // Убираем # в начале
                    var imageNameLower = imageName.toLowerCase();
                    var imageNameWithoutExt = removeExtension(imageNameLower);
                    
                    if (imageNameWithoutExt === startBaseName && !result.startFound) {
                        // Проверяем расширение
                        for (var extIdx = 0; extIdx < supportedExtensions.length; extIdx++) {
                            if (imageNameLower.indexOf(supportedExtensions[extIdx], imageNameWithoutExt.length) != -1) {
                                result.startFound = true;
                                result.startActualName = imageName; // Сохраняем оригинальное имя
                                break;
                            }
                        }
                    }
                    
                    if (imageNameWithoutExt === endBaseName && !result.endFound) {
                        // Проверяем расширение
                        for (var extIdx = 0; extIdx < supportedExtensions.length; extIdx++) {
                            if (imageNameLower.indexOf(supportedExtensions[extIdx], imageNameWithoutExt.length) != -1) {
                                result.endFound = true;
                                result.endActualName = imageName; // Сохраняем оригинальное имя
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    
    return result;
}

// Проверяет, находится ли элемент в сносках (body с fbname="notes")
function isInNotesBody(element) {
    var current = element;
    while (current && current.nodeType == 1) {
        // В main.js используется проверка на body с fbname="notes"
        if (current.nodeName.toLowerCase() === 'body' || 
            (current.className && current.className.toString().indexOf('body') != -1)) {
            var fbname = current.getAttribute('fbname');
            if (fbname && fbname.toLowerCase() === 'notes') {
                return true; // Нашли body с fbname="notes" - это сноски
            }
            break; // Нашли body, но не notes - выходим
        }
        current = current.parentNode;
    }
    return false;
}

// Находит позицию для вставки в начале секции (учитывая эпиграфы)
function findInsertPositionAtStart(section) {
    var firstTitle = findFirstTitleInSection(section);
    if (!firstTitle) {
        return null; // Секция без заголовка
    }
    
    // Ищем эпиграф после заголовка
    var nextElement = firstTitle.nextSibling;
    while (nextElement) {
        if (nextElement.nodeType == 1) {
            var className = nextElement.className ? nextElement.className.toString() : '';
            if (className.indexOf('epigraph') != -1) {
                // Нашли эпиграф - возвращаем позицию после эпиграфа
                return {
                    element: nextElement,
                    nextElement: getNextNonEmptySibling(nextElement),
                    afterEpigraph: true
                };
            }
            if (className.indexOf('image') == -1 && 
                className.indexOf('empty-line') == -1 &&
                className.indexOf('title') == -1) {
                break; // Нашли другой элемент, не эпиграф
            }
        }
        nextElement = nextElement.nextSibling;
    }
    
    // Нет эпиграфа - вставляем после заголовка
    return {
        element: firstTitle,
        nextElement: getNextNonEmptySibling(firstTitle),
        afterEpigraph: false
    };
}

// Получает следующий непустой sibling
function getNextNonEmptySibling(element) {
    var next = element.nextSibling;
    while (next) {
        if (next.nodeType == 1) {
            return next;
        } else if (next.nodeType == 3) {
            var text = next.nodeValue || "";
            if (text && text.replace(/^\s+|\s+$/g, '') != '') {
                return next;
            }
        }
        next = next.nextSibling;
    }
    return null;
}

// Находит первый заголовок в секции
function findFirstTitleInSection(section) {
    var children = section.childNodes;
    for (var i = 0; i < children.length; i++) {
        var child = children[i];
        if (child.nodeType == 1) {
            var className = child.className ? child.className.toString() : '';
            if (className.indexOf('title') != -1) {
                return child;
            }
        }
    }
    return null;
}

// Проверяет, есть ли уже иллюстрация в указанной позиции (регистронезависимо)
function hasImageAtPosition(nextElement, imageName) {
    if (!nextElement || nextElement.nodeType != 1) {
        return false;
    }
    
    var className = nextElement.className ? nextElement.className.toString() : '';
    if (className.indexOf('image') == -1) {
        return false;
    }
    
    var href = nextElement.getAttribute('href');
    if (!href) return false;
    
    // Приводим к нижнему регистру для сравнения
    var hrefLower = href.toLowerCase();
    var imageNameLower = imageName.toLowerCase();
    
    return hrefLower === "#" + imageNameLower;
}

// Проверяет, есть ли уже иллюстрация в конце секции (регистронезависимо)
function hasImageAtEnd(section, imageName) {
    var lastChild = getLastNonEmptyElement(section);
    if (!lastChild || lastChild.nodeType != 1) {
        return false;
    }
    
    var className = lastChild.className ? lastChild.className.toString() : '';
    if (className.indexOf('image') == -1) {
        return false;
    }
    
    var href = lastChild.getAttribute('href');
    if (!href) return false;
    
    // Приводим к нижнему регистру для сравнения
    var hrefLower = href.toLowerCase();
    var imageNameLower = imageName.toLowerCase();
    
    return hrefLower === "#" + imageNameLower;
}

// Получает последний непустой элемент в секции
function getLastNonEmptyElement(section) {
    var lastChild = section.lastChild;
    while (lastChild) {
        if (lastChild.nodeType == 1) {
            var className = lastChild.className ? lastChild.className.toString() : '';
            // Пропускаем пустые строки
            if (className.indexOf('empty-line') == -1) {
                return lastChild;
            }
        } else if (lastChild.nodeType == 3) {
            var text = lastChild.nodeValue || "";
            if (text && text.replace(/^\s+|\s+$/g, '') != '') {
                return lastChild;
            }
        }
        lastChild = lastChild.previousSibling;
    }
    return null;
}

// Проверяет и добавляет пустую строку после вставки картинки (для вставки в начале)
function checkAndAddEmptyLine(section, referenceElement, stats) {
    try {
        // Находим элемент, после которого вставили картинку
        var insertedImage = null;
        var children = section.childNodes;
        
        // Ищем только что вставленную картинку
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 1 && child.className && child.className.toString().indexOf('image') != -1) {
                // Проверяем, что это наша картинка (после referenceElement)
                if (child.previousSibling === referenceElement || 
                    (referenceElement.nextSibling && referenceElement.nextSibling === child)) {
                    insertedImage = child;
                    break;
                }
            }
        }
        
        if (!insertedImage) return;
        
        // Проверяем следующий элемент после вставленной картинки
        var nextElement = insertedImage.nextSibling;
        if (nextElement && nextElement.nodeType == 1) {
            var className = nextElement.className ? nextElement.className.toString() : '';
            
            // Если следующий элемент - тоже картинка, добавляем пустую строку между ними
            if (className.indexOf('image') != -1) {
                var emptyLine = document.createElement('P');
                emptyLine.innerHTML = '&nbsp;';
                section.insertBefore(emptyLine, nextElement);
                stats.emptyLinesAdded++;
            }
        }
    } catch(e) {
        // Игнорируем ошибки при добавлении пустой строки
    }
}

// Проверяет и добавляет пустую строку перед вставке картинки (для вставки в конце)
function checkAndAddEmptyLineBefore(section, stats) {
    try {
        // Получаем последний элемент (это должна быть только что вставленная картинка)
        var lastChild = section.lastChild;
        if (!lastChild || lastChild.nodeType != 1) return;
        
        var className = lastChild.className ? lastChild.className.toString() : '';
        if (className.indexOf('image') == -1) return;
        
        // Проверяем предпоследний элемент
        var prevElement = lastChild.previousSibling;
        while (prevElement && prevElement.nodeType == 3 && 
               (!prevElement.nodeValue || prevElement.nodeValue.replace(/^\s+|\s+$/g, '') == '')) {
            prevElement = prevElement.previousSibling;
        }
        
        if (prevElement && prevElement.nodeType == 1) {
            var prevClassName = prevElement.className ? prevElement.className.toString() : '';
            
            // Если предпоследний элемент - тоже картинка, добавляем пустую строку между ними
            if (prevClassName.indexOf('image') != -1) {
                var emptyLine = document.createElement('P');
                emptyLine.innerHTML = '&nbsp;';
                section.insertBefore(emptyLine, lastChild);
                stats.emptyLinesAdded++;
            }
        }
    } catch(e) {
        // Игнорируем ошибки при добавлении пустой строки
    }
}

// Вставляет реальную иллюстрацию в указанную позицию
function insertRealImageAtPosition(section, insertPosition, imageName) {
    try {
        var imageDiv = createImageElement(imageName);
        
        if (insertPosition.nextElement) {
            section.insertBefore(imageDiv, insertPosition.nextElement);
        } else {
            // Если нет следующего элемента, вставляем после элемента
            if (insertPosition.element.nextSibling) {
                section.insertBefore(imageDiv, insertPosition.element.nextSibling);
            } else {
                section.appendChild(imageDiv);
            }
        }
        
        return true;
    } catch(e) {
        return false;
    }
}

// Вставляет реальную иллюстрацию в конец секции
function insertRealImageAtEnd(section, imageName) {
    try {
        var imageDiv = createImageElement(imageName);
        section.appendChild(imageDiv);
        return true;
    } catch(e) {
        return false;
    }
}

// Создает элемент изображения с указанным именем файла (без атрибута title)
function createImageElement(imageName) {
    var imageDiv = document.createElement('div');
    imageDiv.className = 'image';
    imageDiv.setAttribute('onresizestart', 'return false');
    imageDiv.setAttribute('contenteditable', 'false');
    
    // Используем оригинальное имя из документа
    imageDiv.setAttribute('href', '#' + imageName);
    // БЕЗ атрибута title!
    
    var img = document.createElement('img');
    img.src = 'fbw-internal:#' + imageName;
    // БЕЗ атрибута alt!
    imageDiv.appendChild(img);
    
    return imageDiv;
}

// Функция для вывода сообщений (аналогично MsgBox из main.js)
function MsgBox(str) {
    window.external.MsgBox(str);
}
