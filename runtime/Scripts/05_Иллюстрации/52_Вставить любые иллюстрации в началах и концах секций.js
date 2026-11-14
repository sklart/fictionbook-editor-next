// Скрипт "Вставить любые иллюстрации в началах и концах секций"
// version 1.3
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для расстановки реальных (прилинкованных) иллюстраций
// в началах и концах секций fb2 документа.
// Картинки определяются автоматически из НЕПРИЛИНКОВАННЫХ бинарных объектов.
// Если в имени файла есть "start" (в любом регистре) — картинка ПРИНУДИТЕЛЬНО для начал секций.
// Если есть "end" — ПРИНУДИТЕЛЬНО для концов секций.
// Если меток нет — назначение определяется настройками.
// Несколько картинок одного типа расставляются циклически в алфавитном порядке имён.
// Поддержка отмены действий (Ctrl+Z).

// version 1.3, 15.08.2026
//======================================


function Run() {
    var dialogWidth = "550px";
    var dialogHeight = "470px";
    
    var scriptVersion = "1.3";
    
    var settings = window.showModalDialog(
        "HTML/Вставить любые иллюстрации в началах и концах секций - параметры.htm", 
        scriptVersion,
        "dialogHeight: " + dialogHeight + "; dialogWidth: " + dialogWidth + "; " +
        "center: Yes; help: No; resizable: Yes; status: No;"
    );
    
    if (!settings) return;
    
    var insertStart = settings.insertStart;
    var insertEnd = settings.insertEnd;
    var skipNameless = (settings.skipNameless !== undefined) ? settings.skipNameless : true;
    var insertBeforeAfter = (settings.insertBeforeAfter !== undefined) ? settings.insertBeforeAfter : true;
    var insertEmptyLine = (settings.insertEmptyLine !== undefined) ? settings.insertEmptyLine : true;
    var modeSection = (settings.modeSection !== undefined) ? settings.modeSection : 'all';
    
    // ==================================================
    // СБОР ПРИЛИНКОВАННЫХ КАРТИНОК ИЗ ДОКУМЕНТА
    // ==================================================
    
    var linkedImages = {};
    
    function getLocalHref(name) {
        if (!name || name.indexOf("#") != 0) return null;
        return name.substring(1).toLowerCase();
    }
    
    var fbwBody = document.getElementById("fbw_body");
    if (fbwBody) {
        var allImageDivs = fbwBody.getElementsByTagName("DIV");
        for (var ld = 0; ld < allImageDivs.length; ld++) {
            var div = allImageDivs[ld];
            if (div.className && div.className.indexOf("image") !== -1) {
                var href = div.getAttribute("href");
                if (href) {
                    var localHref = getLocalHref(href);
                    if (localHref) {
                        linkedImages[localHref] = true;
                    }
                }
            }
        }
        
        var allImageSpans = fbwBody.getElementsByTagName("SPAN");
        for (var ls = 0; ls < allImageSpans.length; ls++) {
            var span = allImageSpans[ls];
            if (span.className && span.className.indexOf("image") !== -1) {
                var hrefSpan = span.getAttribute("href");
                if (hrefSpan) {
                    var localHrefSpan = getLocalHref(hrefSpan);
                    if (localHrefSpan) {
                        linkedImages[localHrefSpan] = true;
                    }
                }
            }
        }
    }
    
    // ==================================================
    // СБОР НЕПРИЛИНКОВАННЫХ КАРТИНОК ИЗ BINOBJ
    // ==================================================
    
    var startImages = [];
    var endImages = [];
    var unmarkedImages = [];
    
    var binObjects = document.all.binobj;
    if (!binObjects) {
        window.external.MsgBox("Не найден блок бинарных объектов!");
        return;
    }
    
    var binaryDivs = binObjects.getElementsByTagName("DIV");
    
    for (var i = 0; i < binaryDivs.length; i++) {
        var binDiv = binaryDivs[i];
        var inputs = binDiv.getElementsByTagName("INPUT");
        
        var imageId = "";
        var imageType = "";
        
        for (var j = 0; j < inputs.length; j++) {
            if (inputs[j].id == "id") {
                imageId = inputs[j].value;
            }
            if (inputs[j].id == "type") {
                imageType = inputs[j].value;
            }
        }
        
        if (!imageId || imageId == "") continue;
        
        if (imageType && imageType.toLowerCase().indexOf("image") == -1) continue;
        
        var ext = "";
        var dotIndex = imageId.lastIndexOf('.');
        if (dotIndex != -1) {
            ext = imageId.substring(dotIndex + 1).toLowerCase();
        }
        if (ext != "jpg" && ext != "jpeg" && ext != "png") continue;
        
        var imageIdLower = imageId.toLowerCase();
        if (linkedImages[imageIdLower]) {
            continue;
        }
        
        if (imageIdLower.indexOf("start") != -1) {
            startImages.push(imageId);
        } else if (imageIdLower.indexOf("end") != -1) {
            endImages.push(imageId);
        } else {
            unmarkedImages.push(imageId);
        }
    }
    
    startImages.sort();
    endImages.sort();
    unmarkedImages.sort();
    
    // ==================================================
    // ОПРЕДЕЛЯЕМ НАПРАВЛЕНИЯ И ЦИКЛЫ
    // ==================================================
    
    var startCycle = [];
    var endCycle = [];
    
    startCycle = startCycle.concat(startImages);
    endCycle = endCycle.concat(endImages);
    
    if (unmarkedImages.length > 0) {
        if (startImages.length == 0 && endImages.length == 0) {
            if (insertStart) {
                startCycle = startCycle.concat(unmarkedImages);
            }
            if (insertEnd) {
                endCycle = endCycle.concat(unmarkedImages);
            }
        } else {
            if (insertStart && startImages.length == 0) {
                startCycle = startCycle.concat(unmarkedImages);
            }
            if (insertEnd && endImages.length == 0) {
                endCycle = endCycle.concat(unmarkedImages);
            }
        }
    }
    
    if (startCycle.length == 0 && endCycle.length == 0) {
        if (!insertStart && !insertEnd) {
            window.external.MsgBox("Не выбрано ни одного направления для расстановки!");
        } else {
            window.external.MsgBox("Не найдено подходящих неприлинкованных картинок!");
        }
        return;
    }
    
    // ==================================================
    // СБОР СЕКЦИЙ
    // ==================================================
    
    var sections = [];
    var allDivs = document.getElementsByTagName('div');
    
    for (var d = 0; d < allDivs.length; d++) {
        if (allDivs[d].className && allDivs[d].className.indexOf('section') !== -1) {
            sections.push(allDivs[d]);
        }
    }
    
    // ==================================================
    // ФИЛЬТРАЦИЯ СЕКЦИЙ
    // ==================================================
    
    function isTopLevelSection(section) {
        var parent = section.parentNode;
        return parent && parent.nodeName == "DIV" && parent.className == "body";
    }
    
    function isBottomLevelSection(section) {
        return !hasDirectNestedSections(section);
    }
    
    var filteredSections = [];
    
    for (var si = 0; si < sections.length; si++) {
        var sec = sections[si];
        
        if (modeSection == 'all') {
            filteredSections.push(sec);
        } else if (modeSection == 'top' && isTopLevelSection(sec)) {
            filteredSections.push(sec);
        } else if (modeSection == 'bottom' && isBottomLevelSection(sec)) {
            filteredSections.push(sec);
        }
    }
    
    // ==================================================
    // СТАТИСТИКА
    // ==================================================
    
    var stats = {
        startInserted: 0,
        endInserted: 0,
        skippedNameless: 0,
        skippedHasImage: 0,
        skippedParent: 0,
        skippedNotes: 0,
        startImagesCount: startCycle.length,
        endImagesCount: endCycle.length
    };
    
    window.external.BeginUndoUnit(document, "Расстановка иллюстраций");
    
    try {
        window.external.SetStatusBarText("Расставляем иллюстрации...");
    } catch(e) {}
    
    // ==================================================
    // ОБРАБОТКА СЕКЦИЙ
    // ==================================================
    
    var startIndex = 0;
    var endIndex = 0;
    
    for (var s = 0; s < filteredSections.length; s++) {
        var section = filteredSections[s];
        
        if (isInNotesBody(section)) {
            stats.skippedNotes++;
            continue;
        }
        
        var hasTitle = findFirstTitleInSection(section) !== null;
        var isParent = hasDirectNestedSections(section);
        
        if (!hasTitle && skipNameless) {
            stats.skippedNameless++;
            continue;
        }
        
        // ========== ВСТАВКА В НАЧАЛО ==========
        if (startCycle.length > 0 && hasTitle) {
            var imageName = startCycle[startIndex % startCycle.length];
            var insertResult = insertImageAtStart(section, imageName, insertBeforeAfter, insertEmptyLine);
            if (insertResult == 1) {
                stats.startInserted++;
                startIndex++;
            }
        }
        
        // ========== ВСТАВКА В КОНЕЦ ==========
        if (endCycle.length > 0) {
            if (isParent) {
                stats.skippedParent++;
            } else {
                if (hasTitle || !skipNameless) {
                    var imageNameEnd = endCycle[endIndex % endCycle.length];
                    var insertResultEnd = insertImageAtEnd(section, imageNameEnd, insertBeforeAfter, insertEmptyLine);
                    if (insertResultEnd == 1) {
                        stats.endInserted++;
                        endIndex++;
                    }
                }
            }
        }
        
        checkAndAddEmptyLineFBE(section, stats);
    }
    
    window.external.EndUndoUnit(document);
    
    showStatistics(stats, settings, startCycle.length, endCycle.length);
}

// ==================================================
// ФУНКЦИЯ ВСТАВКИ В НАЧАЛО
// ==================================================

function insertImageAtStart(section, imageName, insertBeforeAfter, insertEmptyLine) {
    try {
        var imageDiv = createImageElement(imageName);
        var firstTitle = findFirstTitleInSection(section);
        
        if (!firstTitle) return 0;
        
        var insertPoint = firstTitle;
        var nextEl = getNextSibling(firstTitle);
        while (nextEl && isDivClass(nextEl, "epigraph")) {
            insertPoint = nextEl;
            nextEl = getNextSibling(nextEl);
        }
        
        var afterPoint = getNextSibling(insertPoint);
        
        if (afterPoint) {
            if (isImageElement(afterPoint)) {
                if (isEmptyImageElement(afterPoint)) {
                    // Вставляем перед пустой картинкой
                    section.insertBefore(imageDiv, afterPoint);
                    return 1;
                } else if (isNormalImageElement(afterPoint) && insertBeforeAfter) {
                    // Вставляем перед обычной картинкой
                    // Сначала вставляем новую картинку перед старой
                    section.insertBefore(imageDiv, afterPoint);
                    // Потом вставляем пустую строку МЕЖДУ новой и старой
                    if (insertEmptyLine) {
                        insertEmptyLineBetween(section, imageDiv, afterPoint);
                    }
                    return 1;
                } else {
                    return 0;
                }
            } else {
                if (afterPoint) {
                    section.insertBefore(imageDiv, afterPoint);
                } else {
                    section.appendChild(imageDiv);
                }
                return 1;
            }
        } else {
            section.appendChild(imageDiv);
            return 1;
        }
    } catch(e) {
        return 0;
    }
}

// ==================================================
// ФУНКЦИЯ ВСТАВКИ В КОНЕЦ
// ==================================================

function insertImageAtEnd(section, imageName, insertBeforeAfter, insertEmptyLine) {
    try {
        var imageDiv = createImageElement(imageName);
        
        var lastChild = section.lastChild;
        while (lastChild && lastChild.nodeType == 3) {
            lastChild = lastChild.previousSibling;
        }
        
        if (lastChild) {
            if (isImageElement(lastChild)) {
                if (isEmptyImageElement(lastChild)) {
                    // Вставляем после пустой картинки
                    if (lastChild.nextSibling) {
                        section.insertBefore(imageDiv, lastChild.nextSibling);
                    } else {
                        section.appendChild(imageDiv);
                    }
                    return 1;
                } else if (isNormalImageElement(lastChild) && insertBeforeAfter) {
                    // Вставляем после обычной картинки
                    // Сначала вставляем пустую строку между старой и новой
                    if (insertEmptyLine) {
                        insertEmptyLineBetween(section, lastChild, imageDiv);
                    }
                    // Потом вставляем новую картинку
                    section.appendChild(imageDiv);
                    return 1;
                } else {
                    return 0;
                }
            } else {
                section.appendChild(imageDiv);
                return 1;
            }
        } else {
            section.appendChild(imageDiv);
            return 1;
        }
    } catch(e) {
        return 0;
    }
}

// ==================================================
// ФУНКЦИЯ ВСТАВКИ ПУСТОЙ СТРОКИ МЕЖДУ ДВУМЯ ЭЛЕМЕНТАМИ
// ==================================================

function insertEmptyLineBetween(parent, firstElement, secondElement) {
    try {
        var p = document.createElement("P");
        // Вставляем пустую строку между firstElement и secondElement
        if (secondElement) {
            parent.insertBefore(p, secondElement);
        } else {
            parent.appendChild(p);
        }
        window.external.inflateBlock(p) = true;
        return p;
    } catch(e) {
        return null;
    }
}

// ==================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ==================================================

function getNextSibling(element) {
    if (!element) return null;
    var next = element.nextSibling;
    while (next && next.nodeType == 3) {
        next = next.nextSibling;
    }
    return next;
}

function isDivClass(element, className) {
    return element && element.nodeName == "DIV" && element.className == className;
}

function isImageElement(element) {
    return element && element.nodeType == 1 && 
           element.className && element.className.indexOf('image') !== -1;
}

function isEmptyImageElement(element) {
    if (!isImageElement(element)) return false;
    var href = element.getAttribute('href');
    return href === '#undefined';
}

function isNormalImageElement(element) {
    if (!isImageElement(element)) return false;
    var href = element.getAttribute('href');
    return href && href !== '' && href !== '#undefined';
}

function isInNotesBody(section) {
    var current = section;
    while (current && current.nodeType == 1) {
        if (current.nodeName.toLowerCase() === 'body' && current.getAttribute('fbname') === 'notes') {
            return true;
        }
        if (current.className && current.className.indexOf('body') !== -1) {
            if (current.getAttribute('fbname') === 'notes' || current.getAttribute('name') === 'notes') {
                return true;
            }
        }
        current = current.parentNode;
    }
    return false;
}

function findFirstTitleInSection(section) {
    var children = section.childNodes;
    for (var i = 0; i < children.length; i++) {
        var child = children[i];
        if (child.nodeType == 1 && child.className && child.className.indexOf('title') !== -1) {
            return child;
        }
    }
    return null;
}

function hasDirectNestedSections(section) {
    var children = section.childNodes;
    for (var i = 0; i < children.length; i++) {
        var child = children[i];
        if (child.nodeType == 1 && child.className && child.className.indexOf('section') !== -1) {
            return true;
        }
    }
    return false;
}

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

function checkAndAddEmptyLineFBE(section, stats) {
    var children = section.childNodes;
    var hasContent = false;
    var imageCount = 0;
    
    for (var i = 0; i < children.length; i++) {
        var child = children[i];
        if (child.nodeType == 1) {
            var className = child.className || '';
            if (!(className.indexOf('image') !== -1 && child.getAttribute('href') === '#undefined') &&
                className.indexOf('title') === -1) {
                hasContent = true;
            } else if (className.indexOf('image') !== -1 && child.getAttribute('href') === '#undefined') {
                imageCount++;
            }
        } else if (child.nodeType == 3) {
            var text = child.textContent || '';
            if (text && text.replace(/^\s+|\s+$/g, '') !== '') {
                hasContent = true;
            }
        }
    }
    
    if (!hasContent && imageCount > 0) {
        var emptyLine = document.createElement('p');
        emptyLine.innerHTML = '&nbsp;';
        section.appendChild(emptyLine);
    }
}

// ==================================================
// СТАТИСТИКА
// ==================================================

function showStatistics(stats, settings, startCount, endCount) {
    var total = stats.startInserted + stats.endInserted;
    
    var message = "Вставить любые иллюстрации в началах и концах секций v1.3\n\n";
    
    message += "Настройки:\n";
    
    if (startCount > 0) {
        message += "• Начала: ДА (картинок: " + startCount + ")\n";
    } else {
        message += "• Начала: НЕТ\n";
    }
    
    if (endCount > 0) {
        message += "• Концы: ДА (картинок: " + endCount + ")\n";
    } else {
        message += "• Концы: НЕТ\n";
    }
    
    message += "\nРезультаты:\n";
    message += "• Вставлено в началах: " + stats.startInserted + "\n";
    message += "• Вставлено в концах: " + stats.endInserted + "\n";
    message += "• Всего вставлено: " + total + "\n";
    
    if (stats.skippedNameless > 0) {
        message += "• Пропущено безымянных секций: " + stats.skippedNameless + "\n";
    }
    if (stats.skippedHasImage > 0) {
        message += "• Пропущено секций с обычными картинками: " + stats.skippedHasImage + "\n";
    }
    if (stats.skippedParent > 0) {
        message += "• Пропущено родительских секций: " + stats.skippedParent + "\n";
    }
    if (stats.skippedNotes > 0) {
        message += "• Пропущено секций в сносках: " + stats.skippedNotes + "\n";
    }
    
    window.external.MsgBox(message);
}