// Скрипт "Вставить ссылки на пустые картинки в началах и концах секций"
// version 4.9 
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для массовой расстановки пустых иллюстраций в началах и концах секций fb2 документа.
// При наличии выделения, скрипт расставляет картинки-пустышки только в выделенных секциях,
// в противном случае - обрабатывается сразу весь документ.
// В отдельных случаях из-за структурных особенностей конкретного fb2 документа
// может быть не проставлена пустая картинка или проставлена лишняя.
// Поэтому иногда требуется проверка и небольшая доработка руками в вашем файле.
// Перед запуском данного скрипта НАСТОЯТЕЛЬНО РЕКОМЕНДУЕТСЯ
// обработать ваш  документ скриптом "Почистить структуру.js" из папки "Структура разделов".
// Если где-то в началах и концах секций уже есть расставленные пустые картинки,
// скрипт не будет повторно расставлять пустые картинки в эти же места.
// Пустые картинки, уже вставленные между обычных абзацев,
// не помешают расстановке в началах и концах секций.
// Обычные картинки, уже вставленные в началах или концах секций,
// также не помешают расстановке пустых картинок в началах и концах секций.
// Можно задать шаг чередования (через 1, через 2 секции...) для вставки в началах и концах секций.
// Также можно включить опцию пропуска секций, где уже есть обычные (непустые) картинки в началах/концах секций.
// Поддержка отмены действий (Ctrl+Z).

// ver. 4.9: 
// Добавлена обработка выделенных секций:
// При наличии выделения, скрипт расставляет картинки-пустышки только в выделенных секциях,
// в противном случае - обрабатывается сразу весь документ.
// Исправлена вставка пустышек при наличии эпиграфов - картинка вставляется после (последнего) эпиграфа в секции.
// Добавлен шаг чередования для вставки в началах/концах секций.
// Добавлена опция пропуска секций, где уже есть обычные (непустые) картинки в началах/концах секций.
// Добавлен выбор режима обработки: все секции / только верхний уровень / только нижний уровень
// Добавлена опция пропуска безымянных секций. 

// version 4.9, 15.08.2026
// ============================================

function Run() {
    var dialogWidth = "570px";
    var dialogHeight = "420px";
    
    var scriptVersion = "4.9";
    
    var settings = window.showModalDialog(
        "HTML/Вставить ссылки на пустые картинки - параметры.htm", 
        scriptVersion,
        "dialogHeight: " + dialogHeight + "; dialogWidth: " + dialogWidth + "; " +
        "center: Yes; help: No; resizable: Yes; status: No;"
    );
    
    if (!settings) return;

    var insertEnd = settings.insertEnd;
    var insertStart = settings.insertStart;
    
    if (!insertEnd && !insertStart) {
        window.external.MsgBox("Не выбрано ни одной опции для вставки!");
        return;
    }

    var stepStart = (settings.stepStart !== undefined) ? settings.stepStart : -1;
    var stepEnd = (settings.stepEnd !== undefined) ? settings.stepEnd : -1;
    var skipStartWithImage = (settings.skipStartWithImage !== undefined) ? settings.skipStartWithImage : false;
    var skipEndWithImage = (settings.skipEndWithImage !== undefined) ? settings.skipEndWithImage : false;
    var modeStart = (settings.modeStart !== undefined) ? settings.modeStart : 'all';
    var modeEnd = (settings.modeEnd !== undefined) ? settings.modeEnd : 'all';
    var skipNameless = (settings.skipNameless !== undefined) ? settings.skipNameless : true;

    var stats = {
        totalInserted: 0,
        startInserted: 0, 
        endInserted: 0,
        emptyLinesAdded: 0,
        skippedNotes: 0,
        skippedParentSections: 0,
        skippedNamelessSections: 0,
        skippedStartHasEmpty: 0,
        skippedEndHasEmpty: 0,
        skippedStartHasImage: 0,
        skippedEndHasImage: 0,
        insertStart: insertStart,
        insertEnd: insertEnd,
        stepStart: stepStart,
        stepEnd: stepEnd,
        skipStartWithImage: skipStartWithImage,
        skipEndWithImage: skipEndWithImage,
        modeStart: modeStart,
        modeEnd: modeEnd,
        skipNameless: skipNameless
    };
    
    window.external.BeginUndoUnit(document, "вставка ссылок на пустые картинки");
    
    try { 
        window.external.SetStatusBarText("Вставляем ссылки на пустые картинки…"); 
    } catch(e) {}
    
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
    
    var sections = [];
    
    if (hasSelection) {
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
        
        sections = collectedSections;
        
    } else {
        var allDivs = document.getElementsByTagName('div');
        
        for (var i = 0; i < allDivs.length; i++) {
            if (allDivs[i].className && allDivs[i].className.indexOf('section') !== -1) {
                sections.push(allDivs[i]);
            }
        }
    }
    
    // ==================================================
    // ФИЛЬТРАЦИЯ СЕКЦИЙ ПО УРОВНЮ
    // ==================================================
    
    function isTopLevelSection(section) {
        var parent = section.parentNode;
        return parent && parent.nodeName == "DIV" && parent.className == "body";
    }
    
    function isBottomLevelSection(section) {
        return !hasDirectNestedSections(section);
    }
    
    var sectionsStart = [];
    var sectionsEnd = [];
    
    for (var si = 0; si < sections.length; si++) {
        var sec = sections[si];
        
        if (modeStart == 'all') {
            sectionsStart.push(sec);
        } else if (modeStart == 'top' && isTopLevelSection(sec)) {
            sectionsStart.push(sec);
        } else if (modeStart == 'bottom' && isBottomLevelSection(sec)) {
            sectionsStart.push(sec);
        }
        
        if (modeEnd == 'all') {
            sectionsEnd.push(sec);
        } else if (modeEnd == 'top' && isTopLevelSection(sec)) {
            sectionsEnd.push(sec);
        } else if (modeEnd == 'bottom' && isBottomLevelSection(sec)) {
            sectionsEnd.push(sec);
        }
    }
    
    // ==================================================
    // ОБРАБОТКА ВСТАВКИ В НАЧАЛАХ
    // ==================================================
    
    var stepCounterStart = 0;
    
    if (insertStart) {
        for (var js = 0; js < sectionsStart.length; js++) {
            var sectionStart = sectionsStart[js];
            
            if (isInNotesBody(sectionStart)) {
                stats.skippedNotes++;
                continue;
            }
            
            var hasTitleStart = findFirstTitleInSection(sectionStart) !== null;
            
            if (!hasTitleStart) {
                if (skipNameless) {
                    // Безымянные пропускаем полностью (не участвуют в чередовании)
                    stats.skippedNamelessSections++;
                    continue;
                }
                // Если не пропускаем безымянные — для начал они всё равно не подходят
                stats.skippedNamelessSections++;
                continue;
            }
            
            var hasNormalImg = hasNormalImageAtStart(sectionStart);
            var hasEmptyImg = hasEmptyImageAtStart(sectionStart);
            
            if (hasNormalImg) {
                if (skipStartWithImage) {
                    stats.skippedStartHasImage++;
                }
                checkAndAddEmptyLineFBE(sectionStart, stats);
                continue;
            }
            
            if (hasEmptyImg) {
                stats.skippedStartHasEmpty++;
                checkAndAddEmptyLineFBE(sectionStart, stats);
                continue;
            }
            
            if (stepStart < 0) {
                stats.startInserted += insertFBEImageAtStart(sectionStart);
            } else {
                if (stepCounterStart == 0) {
                    stats.startInserted += insertFBEImageAtStart(sectionStart);
                    stepCounterStart = 1;
                } else {
                    stepCounterStart++;
                    if (stepCounterStart > stepStart) {
                        stepCounterStart = 0;
                    }
                }
            }
            
            checkAndAddEmptyLineFBE(sectionStart, stats);
        }
    }
    
    // ==================================================
    // ОБРАБОТКА ВСТАВКИ В КОНЦАХ
    // ==================================================
    
    var stepCounterEnd = 0;
    
    if (insertEnd) {
        for (var je = 0; je < sectionsEnd.length; je++) {
            var sectionEnd = sectionsEnd[je];
            
            if (isInNotesBody(sectionEnd)) {
                stats.skippedNotes++;
                continue;
            }
            
            var isParentSectionEnd = hasDirectNestedSections(sectionEnd);
            var hasTitleEnd = findFirstTitleInSection(sectionEnd) !== null;
            
            if (isParentSectionEnd) {
                stats.skippedParentSections++;
                continue;
            }
            
            if (!hasTitleEnd) {
                if (skipNameless) {
                    stats.skippedNamelessSections++;
                    continue;
                }
                if (!canInsertImageInNamelessSection(sectionEnd)) {
                    continue;
                }
            }
            
            var hasNormalImgEnd = hasNormalImageAtEnd(sectionEnd);
            var hasEmptyImgEnd = hasEmptyImageAtEnd(sectionEnd);
            
            if (hasNormalImgEnd) {
                if (skipEndWithImage) {
                    stats.skippedEndHasImage++;
                }
                checkAndAddEmptyLineFBE(sectionEnd, stats);
                continue;
            }
            
            if (hasEmptyImgEnd) {
                stats.skippedEndHasEmpty++;
                checkAndAddEmptyLineFBE(sectionEnd, stats);
                continue;
            }
            
            if (stepEnd < 0) {
                var imageDiv = createFBEImage();
                sectionEnd.appendChild(imageDiv);
                stats.endInserted += 1;
            } else {
                if (stepCounterEnd == 0) {
                    var imageDiv2 = createFBEImage();
                    sectionEnd.appendChild(imageDiv2);
                    stats.endInserted += 1;
                    stepCounterEnd = 1;
                } else {
                    stepCounterEnd++;
                    if (stepCounterEnd > stepEnd) {
                        stepCounterEnd = 0;
                    }
                }
            }
            
            checkAndAddEmptyLineFBE(sectionEnd, stats);
        }
    }
    
    window.external.EndUndoUnit(document);
    
    showStatistics(stats, hasSelection);
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

function canInsertImageInNamelessSection(section) {
    var lastElement = getLastNonEmptyElement(section);
    if (lastElement && lastElement.className && lastElement.className.indexOf('subtitle') !== -1) {
        return false;
    }
    return true;
}

function getLastNonEmptyElement(section) {
    var lastChild = section.lastChild;
    while (lastChild) {
        if (lastChild.nodeType == 1) {
            return lastChild;
        } else if (lastChild.nodeType == 3) {
            if (lastChild.textContent && lastChild.textContent.replace(/^\s+|\s+$/g, '') !== '') {
                return lastChild;
            }
        }
        lastChild = lastChild.previousSibling;
    }
    return null;
}

function isInNotesBody(section) {
    var bodyParent = findNotesBodyParent(section);
    return bodyParent !== null;
}

function findNotesBodyParent(element) {
    var current = element;
    while (current && current.nodeType == 1) {
        if (current.nodeName.toLowerCase() === 'body' && current.getAttribute('fbname') === 'notes') {
            return current;
        }
        if (current.className && current.className.indexOf('body') !== -1) {
            if (current.getAttribute('fbname') === 'notes' || current.getAttribute('name') === 'notes') {
                return current;
            }
        }
        current = current.parentNode;
    }
    return null;
}

function insertFBEImageAtStart(section) {
    var inserted = 0;
    
    try {
        var imageDiv = createFBEImage();
        var firstTitle = findFirstTitleInSection(section);
        
        if (firstTitle) {
            var insertAfter = firstTitle;
            
            var nextEl = getNextSibling(firstTitle);
            while (nextEl && isDivClass(nextEl, "epigraph")) {
                insertAfter = nextEl;
                nextEl = getNextSibling(nextEl);
            }
            
            if (insertAfter.nextSibling) {
                section.insertBefore(imageDiv, insertAfter.nextSibling);
            } else {
                section.appendChild(imageDiv);
            }
            inserted = 1;
        }
    } catch(e) {}
    
    return inserted;
}

function hasEmptyImageAtStart(section) {
    var firstTitle = findFirstTitleInSection(section);
    if (!firstTitle) return false;
    
    var checkAfter = firstTitle;
    var nextEl = getNextSibling(firstTitle);
    while (nextEl && isDivClass(nextEl, "epigraph")) {
        checkAfter = nextEl;
        nextEl = getNextSibling(nextEl);
    }
    
    var nextSibling = getNextSibling(checkAfter);
    while (nextSibling && nextSibling.nodeType == 3 && 
           (!nextSibling.textContent || nextSibling.textContent.replace(/^\s+|\s+$/g, '') === '')) {
        nextSibling = nextSibling.nextSibling;
    }
    return nextSibling && isEmptyImageElement(nextSibling);
}

function hasNormalImageAtStart(section) {
    var firstTitle = findFirstTitleInSection(section);
    if (!firstTitle) return false;
    
    var checkAfter = firstTitle;
    var nextEl = getNextSibling(firstTitle);
    while (nextEl && isDivClass(nextEl, "epigraph")) {
        checkAfter = nextEl;
        nextEl = getNextSibling(nextEl);
    }
    
    var nextSibling = getNextSibling(checkAfter);
    while (nextSibling && nextSibling.nodeType == 3 && 
           (!nextSibling.textContent || nextSibling.textContent.replace(/^\s+|\s+$/g, '') === '')) {
        nextSibling = nextSibling.nextSibling;
    }
    return nextSibling && isNormalImageElement(nextSibling);
}

function hasEmptyImageAtEnd(section) {
    var lastChild = section.lastChild;
    while (lastChild && lastChild.nodeType == 3 && 
           (!lastChild.textContent || lastChild.textContent.replace(/^\s+|\s+$/g, '') === '')) {
        lastChild = lastChild.previousSibling;
    }
    return lastChild && isEmptyImageElement(lastChild);
}

function hasNormalImageAtEnd(section) {
    var lastChild = section.lastChild;
    while (lastChild && lastChild.nodeType == 3 && 
           (!lastChild.textContent || lastChild.textContent.replace(/^\s+|\s+$/g, '') === '')) {
        lastChild = lastChild.previousSibling;
    }
    return lastChild && isNormalImageElement(lastChild);
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

function isEmptyImageElement(element) {
    if (element.nodeType != 1) return false;
    if (!element.className || element.className.indexOf('image') === -1) return false;
    if (element.getAttribute('href') !== '#undefined') return false;
    
    var imgs = element.getElementsByTagName('img');
    return imgs.length > 0 && imgs[0].src.indexOf('#undefined') !== -1;
}

function isNormalImageElement(element) {
    if (element.nodeType != 1) return false;
    if (!element.className || element.className.indexOf('image') === -1) return false;
    
    var href = element.getAttribute('href');
    if (!href || href === '' || href === '#undefined') return false;
    
    var imgs = element.getElementsByTagName('img');
    if (imgs.length > 0) {
        if (imgs[0].src.indexOf('#undefined') === -1) {
            return true;
        }
    }
    return false;
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

function createFBEImage() {
    var imageDiv = document.createElement('div');
    imageDiv.className = 'image';
    imageDiv.setAttribute('onresizestart', 'return false');
    imageDiv.setAttribute('contenteditable', 'false');
    imageDiv.setAttribute('href', '#undefined');
    
    var img = document.createElement('img');
    img.src = 'fbw-internal:#undefined';
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
        stats.emptyLinesAdded++;
    }
}

function showStatistics(stats, hasSelection) {
    stats.totalInserted = stats.startInserted + stats.endInserted;
    
    var message = "Скрипт «Вставить ссылки на пустые картинки в началах и концах секций» v4.9\n\n";
    
    message += "Режим обработки: ";
    if (hasSelection) {
        message += "ВЫДЕЛЕНИЕ\n\n";
    } else {
        message += "ВЕСЬ ДОКУМЕНТ\n\n";
    }
    
    message += "Результаты выполнения:\n";
    message += "Всего расставлено пустых картинок: " + stats.totalInserted + "\n";
    
    if (stats.insertStart) {
        message += "• В начале секций: " + stats.startInserted + "\n";
        if (stats.modeStart == 'all') {
            message += "  Уровень: все секции\n";
        } else if (stats.modeStart == 'top') {
            message += "  Уровень: только верхний\n";
        } else if (stats.modeStart == 'bottom') {
            message += "  Уровень: только нижний\n";
        }
        if (stats.stepStart < 0) {
            message += "  Чередование в началах: вставка везде\n";
        } else {
            message += "  Чередование в началах: через " + stats.stepStart + "\n";
        }
        if (stats.skippedStartHasEmpty > 0) {
            message += "  Пропущено из-за наличия пустых картинок: " + stats.skippedStartHasEmpty + "\n";
        }
        if (stats.skippedStartHasImage > 0) {
            message += "  Пропущено из-за наличия обычных картинок: " + stats.skippedStartHasImage + "\n";
        }
    } else {
        message += "• В начале секций: " + stats.startInserted + " (не расставлялись)\n";
    }
    
    if (stats.insertEnd) {
        message += "• В конце секций: " + stats.endInserted + "\n";
        if (stats.modeEnd == 'all') {
            message += "  Уровень: все секции\n";
        } else if (stats.modeEnd == 'top') {
            message += "  Уровень: только верхний\n";
        } else if (stats.modeEnd == 'bottom') {
            message += "  Уровень: только нижний\n";
        }
        if (stats.stepEnd < 0) {
            message += "  Чередование в концах: вставка везде\n";
        } else {
            message += "  Чередование в концах: через " + stats.stepEnd + "\n";
        }
        if (stats.skippedEndHasEmpty > 0) {
            message += "  Пропущено из-за наличия пустых картинок: " + stats.skippedEndHasEmpty + "\n";
        }
        if (stats.skippedEndHasImage > 0) {
            message += "  Пропущено из-за наличия обычных картинок: " + stats.skippedEndHasImage + "\n";
        }
    } else {
        message += "• В конце секций: " + stats.endInserted + " (не расставлялись)\n";
    }
    
    if (stats.skipNameless) {
        message += "• Пропущено безымянных секций: " + stats.skippedNamelessSections + "\n";
    }
    
    message += "• Добавлено пустых строк: " + stats.emptyLinesAdded + "\n";
    message += "• Пропущено секций в сносках: " + stats.skippedNotes + "\n";
    
    if (stats.insertEnd) {
        message += "• Пропущено родительских секций: " + stats.skippedParentSections;
    } else {
        message += "• Пропущено родительских секций: " + stats.skippedParentSections + " (не применялось)";
    }
    
    window.external.MsgBox(message);
}