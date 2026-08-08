// Скрипт "Преобразовать инлайн иллюстрации в блочные" для редактора FBE
// version 2.0
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для преобразования внутриабзацных (инлайн) иллюстраций (SPAN class=image)
// в обычные - отдельностоящие блочные иллюстрации (DIV class=image) в fb2 документах.
// По умолчанию обрабатывается только основной раздел документа, без сносок и комментариев.
// Два варианта работы: сразу со всем документом или с выделенным фрагментом.
// Можно преобразовать даже одиночную выделенную иллюстрацию, не затрагивая другие.
// Блочные DIV-иллюстрации и любые бинарники НЕ затрагиваются.
// Инлайн картинки внутри уже размеченных структурных элементов
// (history, annotation, epigraph, title, poem, cite, table) игнорируются, не обрабатываются.
// Можно настраивать добавление любых пользовательских маркеров
// в места исходного расположения иллюстраций и добавление пустых строк между иллюстрацией и абзацем.
// По умолчанию добавляется только маркер для иллюстраций, расположенных в "середине" абзаца.
// Для иллюстраций в "пустых" абзацах (без текста) пользовательские маркеры никогда не добавляются.
// Автоматическое добавление пустой строки после преобразования иллюстрации (для валидности),
// если она - единственный элемент в секции, кроме заголовка.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// Для удобства и контроля рекомендуется дополнительно использовать скрипт "Статистика иллюстраций".

// version 2.0, 16.04.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Преобразовать инлайн иллюстрации в блочные";
    var version = "2.0";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // 0 - тихий режим, 1 - показывать статистику
    
    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0; // 0 - нет, 1 - да
    
    // Обрабатывать раздел комментариев
    var processCommentsSection = 0; // 0 - нет, 1 - да
    
    // Преобразовывать иллюстрации в пустых абзацах (без текста)
    var processEmptyPic = 1; // 0 - нет, 1 - да
    
    // Преобразовывать иллюстрации, расположенные в началах абзацев
    var processStartPic = 1; // 0 - нет, 1 - да
    
    // Преобразовывать иллюстрации, расположенные в концах абзацев
    var processEndPic = 1; // 0 - нет, 1 - да
    
    // Преобразовывать иллюстрации, расположенные в "середине" абзацев
    var processCenterPic = 1; // 0 - нет, 1 - да
    
    // Вставлять пользовательские маркеры в началах абзацев (на исходное место иллюстраций)
    var processMarkersStart = 0; // 0 - нет, 1 - да
    
    // Вставлять пользовательские маркеры в концах абзацев (на исходное место иллюстраций)
    var processMarkersEnd = 0; // 0 - нет, 1 - да
    
    
    // Вставлять пользовательские маркеры в "серединах" абзацев (на исходное место иллюстраций)
    
    // Если иллюстрация расположена в "середине" абзаца - абзац корректно разрывается в этом месте
    //и маркер вставляется в конец первой части абзаца
    // Если в абзаце несколько иллюстраций, принцип остается такой же
    var processMarkersCenter = 1; // 0 - нет, 1 - да
    
    
    // Пользовательский маркер для начала абзаца
    var InlinePicMarkerStart = "ZZZ_INLINE_PIC_START_";
    
    // Пользовательский маркер для конца абзаца
    var InlinePicMarkerEnd = "_ZZZ_INLINE_PIC_END";
    
    // Пользовательский маркер для середины абзаца
    var InlinePicMarkerCenter = "_ZZZ_INLINE_PIC_CENTER";
    
    // Добавлять пустые строки между иллюстрацией и примыкающими абзацами
    var AddEmptyLines = 1; // 0 - нет, 1 - да
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    // Объект для статистики
    var stats = {
        totalInline: 0,
        convertedEmpty: 0,
        convertedStart: 0,
        convertedEnd: 0,
        convertedCenter: 0,
        markersStartAdded: 0,
        markersEndAdded: 0,
        markersCenterAdded: 0,
        emptyLinesAdded: 0,
        skippedTotal: 0,
        skippedHistory: 0,
        skippedAnnotation: 0,
        skippedEpigraph: 0,
        skippedTitle: 0,
        skippedPoem: 0,
        skippedCite: 0,
        skippedTable: 0
    };
    
    // Получаем неразрывный пробел из настроек FBE
    var nbspChar;
    var nbspEntity;
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
    
    // Список необычных пробелов
    var unusualSpaces = String.fromCharCode(160) +
        String.fromCharCode(8194) +
        String.fromCharCode(8195) +
        String.fromCharCode(8196) +
        String.fromCharCode(8197) +
        String.fromCharCode(8198) +
        String.fromCharCode(8239) +
        String.fromCharCode(8201) +
        String.fromCharCode(8202) +
        nbspChar;
    
    // Список защищённых DIV-контейнеров
    var protectedDivs = ["history", "annotation", "epigraph", "title", "poem", "cite", "table"];
    
    // Список важных атрибутов, которые НЕ нужно удалять
    var keepAttrs = {
        "class": true,
        "href": true,
        "id": true,
        "src": true,
        "alt": true,
        "title": true
    };
    
    // ==================================================
    // ФУНКЦИЯ ОЧИСТКИ СЛУЖЕБНЫХ АТРИБУТОВ
    // ==================================================
    
    function cleanAttributes(element) {
        if (!element || element.nodeType != 1) return;
        
        // Получаем список атрибутов
        var attrs = [];
        if (element.attributes) {
            for (var i = 0; i < element.attributes.length; i++) {
                attrs.push(element.attributes[i].name);
            }
        }
        
        // Удаляем все атрибуты, которых нет в keepAttrs
        for (var i = 0; i < attrs.length; i++) {
            var attrName = attrs[i];
            if (!keepAttrs[attrName]) {
                try {
                    element.removeAttribute(attrName);
                } catch(e) {}
            }
        }
        
        // Рекурсивно очищаем дочерние элементы
        var children = element.childNodes;
        for (var i = 0; i < children.length; i++) {
            cleanAttributes(children[i]);
        }
    }
    
    // ==================================================
    // ФУНКЦИИ ПРОВЕРКИ
    // ==================================================
    
    function isSpaceChar(ch) {
        if (ch == " " || ch == "\t" || ch == "\n" || ch == "\r") return true;
        if (ch == nbspChar) return true;
        for (var i = 0; i < unusualSpaces.length; i++) {
            if (ch == unusualSpaces.charAt(i)) return true;
        }
        return false;
    }
    
    function isEmptyText(str) {
        if (!str) return true;
        for (var i = 0; i < str.length; i++) {
            if (!isSpaceChar(str.charAt(i))) return false;
        }
        return true;
    }
    
    function isInsideProtectedDiv(element) {
        var parent = element.parentNode;
        while (parent && parent.nodeName != "BODY") {
            if (parent.nodeName == "DIV") {
                var className = parent.className || "";
                for (var i = 0; i < protectedDivs.length; i++) {
                    if (className == protectedDivs[i]) {
                        return true;
                    }
                }
            }
            parent = parent.parentNode;
        }
        return false;
    }
    
    function getProtectedDivType(element) {
        var parent = element.parentNode;
        while (parent && parent.nodeName != "BODY") {
            if (parent.nodeName == "DIV") {
                var className = parent.className || "";
                for (var i = 0; i < protectedDivs.length; i++) {
                    if (className == protectedDivs[i]) {
                        return className;
                    }
                }
            }
            parent = parent.parentNode;
        }
        return "";
    }
    
    function isEmptyLineP(element) {
        if (!element || element.nodeName != "P") return false;
        var html = element.innerHTML || "";
        var trimmed = html.replace(/^\s+|\s+$/g, '');
        return trimmed == nbspEntity || trimmed == nbspChar;
    }
    
    function hasEmptyLineBefore(element) {
        var prev = element.previousSibling;
        while (prev && prev.nodeType == 3 && isEmptyText(prev.nodeValue)) {
            prev = prev.previousSibling;
        }
        return prev && isEmptyLineP(prev);
    }
    
    function hasEmptyLineAfter(element) {
        var next = element.nextSibling;
        while (next && next.nodeType == 3 && isEmptyText(next.nodeValue)) {
            next = next.nextSibling;
        }
        return next && isEmptyLineP(next);
    }
    
    function addEmptyLineBefore(element, parent) {
        if (AddEmptyLines && !hasEmptyLineBefore(element)) {
            var emptyP = document.createElement("P");
            emptyP.innerHTML = nbspEntity;
            parent.insertBefore(emptyP, element);
            stats.emptyLinesAdded++;
            return emptyP;
        }
        return null;
    }
    
    function addEmptyLineAfter(element, parent) {
        if (AddEmptyLines && !hasEmptyLineAfter(element)) {
            var emptyP = document.createElement("P");
            emptyP.innerHTML = nbspEntity;
            var next = element.nextSibling;
            if (next) {
                parent.insertBefore(emptyP, next);
            } else {
                parent.appendChild(emptyP);
            }
            stats.emptyLinesAdded++;
            return emptyP;
        }
        return null;
    }
    
    function isOnlyParagraphInSection(p) {
        var parent = p.parentNode;
        if (!parent || parent.nodeName != "DIV" || parent.className != "section") return false;
        
        var children = parent.childNodes;
        var pCount = 0;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 1 && child.nodeName == "P") {
                pCount++;
                if (pCount > 1) return false;
            }
        }
        return pCount == 1;
    }
    
    function shouldProcessSection(div) {
        var fbname = div.getAttribute("fbname") || "";
        if (fbname == "") return true;
        if (fbname == "notes" && processNotesSection) return true;
        if (fbname == "comments" && processCommentsSection) return true;
        return false;
    }
    
    function isInProcessedSection(element) {
        var parent = element.parentNode;
        while (parent && parent.nodeName != "BODY") {
            if (parent.nodeName == "DIV" && parent.className == "body") {
                return shouldProcessSection(parent);
            }
            parent = parent.parentNode;
        }
        return true;
    }
    
    function trimStart(str) {
        if (!str) return str;
        var i = 0;
        while (i < str.length && isSpaceChar(str.charAt(i))) {
            i++;
        }
        return str.substring(i);
    }
    
    function trimEnd(str) {
        if (!str) return str;
        var i = str.length - 1;
        while (i >= 0 && isSpaceChar(str.charAt(i))) {
            i--;
        }
        return str.substring(0, i + 1);
    }
    
    function cleanTextNodeEdges(textNode, trimLeft, trimRight) {
        if (!textNode || textNode.nodeType != 3) return;
        var text = textNode.nodeValue;
        if (!text) return;
        
        var newText = text;
        if (trimLeft) {
            newText = trimStart(newText);
        }
        if (trimRight) {
            newText = trimEnd(newText);
        }
        
        if (newText != text) {
            textNode.nodeValue = newText;
        }
    }
    
    function cleanParagraphStart(p) {
        if (!p) return;
        var children = p.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 3) {
                cleanTextNodeEdges(child, true, false);
                if (child.nodeValue && child.nodeValue.length > 0) break;
            } else if (child.nodeType == 1) {
                break;
            }
        }
    }
    
    function cleanParagraphEnd(p) {
        if (!p) return;
        var children = p.childNodes;
        for (var i = children.length - 1; i >= 0; i--) {
            var child = children[i];
            if (child.nodeType == 3) {
                cleanTextNodeEdges(child, false, true);
                if (child.nodeValue && child.nodeValue.length > 0) break;
            } else if (child.nodeType == 1) {
                break;
            }
        }
    }
    
    function getImagePositionInParagraph(span, p) {
        var hasTextBefore = false;
        var hasTextAfter = false;
        
        var range = document.body.createTextRange();
        range.moveToElementText(p);
        var spanRange = document.body.createTextRange();
        spanRange.moveToElementText(span);
        
        var compareStart = range.compareEndPoints("StartToStart", spanRange);
        
        if (compareStart < 0) {
            var beforeRange = range.duplicate();
            beforeRange.setEndPoint("EndToStart", spanRange);
            var beforeText = beforeRange.text || "";
            if (!isEmptyText(beforeText)) {
                hasTextBefore = true;
            }
        }
        
        var compareEnd = range.compareEndPoints("EndToEnd", spanRange);
        
        if (compareEnd > 0) {
            var afterRange = range.duplicate();
            afterRange.setEndPoint("StartToEnd", spanRange);
            var afterText = afterRange.text || "";
            if (!isEmptyText(afterText)) {
                hasTextAfter = true;
            }
        }
        
        if (!hasTextBefore && !hasTextAfter) return "empty";
        if (!hasTextBefore && hasTextAfter) return "start";
        if (hasTextBefore && !hasTextAfter) return "end";
        return "center";
    }
    
    function convertSpanToDiv(span) {
        var div = document.createElement("DIV");
        div.className = "image";
        div.setAttribute("onresizestart", "return false");
        div.setAttribute("contentEditable", "false");
        
        var href = span.getAttribute("href") || "";
        div.setAttribute("href", href);
        
        var children = span.childNodes;
        for (var i = 0; i < children.length; i++) {
            div.appendChild(children[i].cloneNode(true));
        }
        
        return div;
    }
    
    function addMarkerToParagraph(p, markerText, atEnd) {
        if (!p || !markerText) return false;
        
        var markerNode = document.createTextNode(markerText);
        
        if (atEnd) {
            p.appendChild(markerNode);
        } else {
            var firstChild = p.firstChild;
            if (firstChild) {
                p.insertBefore(markerNode, firstChild);
            } else {
                p.appendChild(markerNode);
            }
        }
        return true;
    }
    
    // ПЕРЕПИСАННАЯ ФУНКЦИЯ РАЗРЫВА АБЗАЦА (с очисткой атрибутов после клонирования)
    function splitParagraphAroundSpan(span) {
        var p = span;
        while (p && p.nodeName != "P") {
            p = p.parentNode;
        }
        if (!p || p.nodeName != "P") return null;
        
        var parent = p.parentNode;
        if (!parent) return null;
        
        var spanContainer = span.parentNode;
        
        // Собираем все узлы ПОСЛЕ span в пределах его контейнера
        var nodesAfter = [];
        var foundSpan = false;
        var containerChildren = spanContainer.childNodes;
        
        for (var i = 0; i < containerChildren.length; i++) {
            var child = containerChildren[i];
            if (child == span) {
                foundSpan = true;
                continue;
            }
            if (foundSpan) {
                nodesAfter.push(child);
            }
        }
        
        // Если spanContainer не P, собираем также узлы после spanContainer
        var nodesAfterContainer = [];
        if (spanContainer != p) {
            var foundContainer = false;
            var pChildren = p.childNodes;
            for (var i = 0; i < pChildren.length; i++) {
                var child = pChildren[i];
                if (child == spanContainer) {
                    foundContainer = true;
                    continue;
                }
                if (foundContainer) {
                    nodesAfterContainer.push(child);
                }
            }
        }
        
        // Создаём новый абзац для части после span
        var newP = null;
        var hasContentAfter = false;
        
        // Проверяем, есть ли непустой контент после span
        for (var i = 0; i < nodesAfter.length; i++) {
            var node = nodesAfter[i];
            if (node.nodeType == 3) {
                if (!isEmptyText(node.nodeValue)) {
                    hasContentAfter = true;
                    break;
                }
            } else if (node.nodeType == 1) {
                hasContentAfter = true;
                break;
            }
        }
        
        if (!hasContentAfter) {
            for (var i = 0; i < nodesAfterContainer.length; i++) {
                var node = nodesAfterContainer[i];
                if (node.nodeType == 3) {
                    if (!isEmptyText(node.nodeValue)) {
                        hasContentAfter = true;
                        break;
                    }
                } else if (node.nodeType == 1) {
                    hasContentAfter = true;
                    break;
                }
            }
        }
        
        if (hasContentAfter) {
            newP = document.createElement("P");
            
            // Восстанавливаем структуру для второй части
            if (spanContainer != p) {
                // Клонируем spanContainer
                var clonedContainer = spanContainer.cloneNode(false); // false - без детей
                
                // Добавляем узлы после span (клонируем)
                for (var i = 0; i < nodesAfter.length; i++) {
                    clonedContainer.appendChild(nodesAfter[i].cloneNode(true));
                }
                
                newP.appendChild(clonedContainer);
                
                // Добавляем узлы после контейнера (клонируем)
                for (var i = 0; i < nodesAfterContainer.length; i++) {
                    newP.appendChild(nodesAfterContainer[i].cloneNode(true));
                }
            } else {
                // Просто добавляем узлы после span (клонируем)
                for (var i = 0; i < nodesAfter.length; i++) {
                    newP.appendChild(nodesAfter[i].cloneNode(true));
                }
            }
            
            // Очищаем служебные атрибуты в новом абзаце
            cleanAttributes(newP);
        }
        
        // Удаляем узлы после span из исходного абзаца
        for (var i = nodesAfter.length - 1; i >= 0; i--) {
            nodesAfter[i].removeNode(true);
        }
        
        if (spanContainer != p) {
            // Удаляем узлы после контейнера
            for (var i = nodesAfterContainer.length - 1; i >= 0; i--) {
                nodesAfterContainer[i].removeNode(true);
            }
        }
        
        // Очищаем конец первого абзаца
        cleanParagraphEnd(p);
        
        if (newP) {
            cleanParagraphStart(newP);
        }
        
        return { firstP: p, secondP: newP, parent: parent };
    }
    
    function processInlineImage(span) {
        if (!span || span.nodeName != "SPAN") return false;
        if (span.className != "image") return false;
        if (isInsideProtectedDiv(span)) {
            var divType = getProtectedDivType(span);
            stats.skippedTotal++;
            if (divType == "history") stats.skippedHistory++;
            else if (divType == "annotation") stats.skippedAnnotation++;
            else if (divType == "epigraph") stats.skippedEpigraph++;
            else if (divType == "title") stats.skippedTitle++;
            else if (divType == "poem") stats.skippedPoem++;
            else if (divType == "cite") stats.skippedCite++;
            else if (divType == "table") stats.skippedTable++;
            return false;
        }
        if (!isInProcessedSection(span)) return false;
        
        var p = span;
        while (p && p.nodeName != "P") {
            p = p.parentNode;
        }
        if (!p || p.nodeName != "P") return false;
        
        var position = getImagePositionInParagraph(span, p);
        
        if (position == "empty" && !processEmptyPic) return false;
        if (position == "start" && !processStartPic) return false;
        if (position == "end" && !processEndPic) return false;
        if (position == "center" && !processCenterPic) return false;
        
        var parent = p.parentNode;
        var isOnlyP = isOnlyParagraphInSection(p);
        
        var div = convertSpanToDiv(span);
        
        if (position == "center") {
            var splitResult = splitParagraphAroundSpan(span);
            var firstP = splitResult.firstP;
            var secondP = splitResult.secondP;
            
            // Удаляем SPAN
            span.removeNode(true);
            
            var nextSib = firstP.nextSibling;
            if (nextSib) {
                parent.insertBefore(div, nextSib);
            } else {
                parent.appendChild(div);
            }
            
            if (secondP) {
                var afterDiv = div.nextSibling;
                if (afterDiv) {
                    parent.insertBefore(secondP, afterDiv);
                } else {
                    parent.appendChild(secondP);
                }
            }
            
            stats.convertedCenter++;
            
            if (processMarkersCenter) {
                addMarkerToParagraph(firstP, InlinePicMarkerCenter, true);
                stats.markersCenterAdded++;
            }
            
            addEmptyLineBefore(div, parent);
            addEmptyLineAfter(div, parent);
            
            return true;
        }
        
        span.removeNode(true);
        
        if (position == "empty") {
            parent.replaceChild(div, p);
            stats.convertedEmpty++;
            
            if (isOnlyP) {
                var emptyP = document.createElement("P");
                emptyP.innerHTML = nbspEntity;
                var next = div.nextSibling;
                if (next) {
                    parent.insertBefore(emptyP, next);
                } else {
                    parent.appendChild(emptyP);
                }
                stats.emptyLinesAdded++;
            } else {
                addEmptyLineBefore(div, parent);
                addEmptyLineAfter(div, parent);
            }
            
        } else if (position == "start") {
            parent.insertBefore(div, p);
            stats.convertedStart++;
            
            cleanParagraphStart(p);
            
            if (processMarkersStart) {
                addMarkerToParagraph(p, InlinePicMarkerStart, false);
                stats.markersStartAdded++;
            }
            
            addEmptyLineBefore(div, parent);
            addEmptyLineAfter(div, parent);
            
        } else if (position == "end") {
            var next = p.nextSibling;
            if (next) {
                parent.insertBefore(div, next);
            } else {
                parent.appendChild(div);
            }
            stats.convertedEnd++;
            
            cleanParagraphEnd(p);
            
            if (processMarkersEnd) {
                addMarkerToParagraph(p, InlinePicMarkerEnd, true);
                stats.markersEndAdded++;
            }
            
            addEmptyLineBefore(div, parent);
            addEmptyLineAfter(div, parent);
        }
        
        return true;
    }
    
    function getAllInlineImages() {
        var fbwBody = document.getElementById("fbw_body");
        if (!fbwBody) return [];
        
        var spans = fbwBody.getElementsByTagName("SPAN");
        var images = [];
        for (var i = 0; i < spans.length; i++) {
            if (spans[i].className == "image") {
                images.push(spans[i]);
            }
        }
        return images;
    }
    
    function getInlineImagesInSelection(selectionRange) {
        var images = [];
        var fbwBody = document.getElementById("fbw_body");
        if (!fbwBody) return images;
        
        var allSpans = fbwBody.getElementsByTagName("SPAN");
        
        for (var i = 0; i < allSpans.length; i++) {
            var span = allSpans[i];
            if (span.className != "image") continue;
            
            try {
                var spanRange = document.body.createTextRange();
                spanRange.moveToElementText(span);
                
                var compareStart = selectionRange.compareEndPoints("StartToStart", spanRange);
                var compareEnd = selectionRange.compareEndPoints("EndToEnd", spanRange);
                
                if (compareStart <= 0 && compareEnd >= 0) {
                    images.push(span);
                }
            } catch(e) {}
        }
        
        return images;
    }
    
    // ==================================================
    // ОСНОВНАЯ ЛОГИКА
    // ==================================================
    
    var sel = document.selection;
    if (!sel) {
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка. Не удалось получить выделение.", "FBE скрипт");
        return;
    }
    
    var testRange = sel.createRange();
    try {
        var parentEl = testRange.parentElement();
        if (parentEl && (parentEl.nodeName == "TEXTAREA" || parentEl.nodeName == "INPUT")) {
            MsgBox(scriptName + "\nver. " + version + "\n\nОшибка. Должно быть выделение в тексте книги, а не в поле ввода.", "FBE скрипт");
            return;
        }
    } catch(e) {}
    
    var imagesToProcess = [];
    var hasSelection = false;
    var isControlSelection = false;
    var scopeDescription = "во всем документе";
    
    if (sel.type == "Control") {
        var controlRange = sel.createRange();
        if (controlRange && controlRange.length > 0) {
            var controlElement = controlRange.item(0);
            if (controlElement && controlElement.nodeName == "SPAN" && controlElement.className == "image") {
                imagesToProcess = [controlElement];
                isControlSelection = true;
                hasSelection = true;
                scopeDescription = "в выделенном фрагменте";
            }
        }
    }
    
    if (!isControlSelection) {
        var textRange = sel.createRange();
        if (textRange && textRange.text && textRange.text.length > 0) {
            hasSelection = true;
            scopeDescription = "в выделенном фрагменте";
            imagesToProcess = getInlineImagesInSelection(textRange);
        }
    }
    
    if (!hasSelection) {
        imagesToProcess = getAllInlineImages();
        scopeDescription = "во всем документе";
    }
    
    stats.totalInline = imagesToProcess.length;
    
    if (stats.totalInline == 0) {
        if (hasSelection) {
            MsgBox(scriptName + "\nver. " + version + "\n\nВ выделенном фрагменте нет инлайн иллюстраций.", "FBE скрипт");
        } else {
            MsgBox(scriptName + "\nver. " + version + "\n\nВ документе нет инлайн иллюстраций.", "FBE скрипт");
        }
        return;
    }
    
    var hasAccessibleImage = false;
    for (var i = 0; i < imagesToProcess.length; i++) {
        if (!isInsideProtectedDiv(imagesToProcess[i]) && isInProcessedSection(imagesToProcess[i])) {
            hasAccessibleImage = true;
            break;
        }
    }
    
    if (!hasAccessibleImage) {
        var msgPrefix = (hasSelection) ? "Все инлайн иллюстрации в выделенном фрагменте" : "Все инлайн иллюстрации в документе";
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               msgPrefix + " находятся внутри размеченных div-элементов.\n" +
               "Обработка инлайн иллюстраций внутри размеченных div-элементов\n" +
               "(history, annotation, epigraph, title, poem, cite, table)\n" +
               "не производится.", "FBE скрипт");
        return;
    }
    
    if (showStatistics) {
        var confirmMsg = scriptName + "\nver. " + version + "\n\n";
        confirmMsg += "Найдено инлайн иллюстраций (" + scopeDescription + "): " + stats.totalInline + "\n\n";
        confirmMsg += "Выполнить преобразование?";
        
        if (!AskYesNo(confirmMsg)) {
            return;
        }
    }
    
    var startTime = new Date().getTime();
    
    window.external.BeginUndoUnit(document, scriptName + " ver. " + version);
    
    try {
        for (var i = imagesToProcess.length - 1; i >= 0; i--) {
            processInlineImage(imagesToProcess[i]);
        }
        
        window.external.EndUndoUnit(document);
        
        var endTime = new Date().getTime();
        var timeDiff = (endTime - startTime) / 1000;
        var timeStr = timeDiff.toFixed(3).replace(".", ",");
        
        if (showStatistics) {
            var totalLabel = (hasSelection) ? "• Всего в выделенном фрагменте: " : "• Всего в документе: ";
            
            var msg = scriptName + "\nver. " + version + "\n\n";
            msg += "Внутриабзацных (инлайн) иллюстраций\n\n";
            msg += totalLabel + stats.totalInline + "\n\n";
            msg += "Преобразовано:\n";
            msg += "  ✓ В пустых абзацах: " + stats.convertedEmpty + "\n";
            msg += "  ✓ В начале абзацев: " + stats.convertedStart + "\n";
            msg += "  ✓ В конце абзацев: " + stats.convertedEnd + "\n";
            msg += "  ✓ В середине абзацев: " + stats.convertedCenter + "\n";
            
            var hasMarkers = (processMarkersStart && stats.markersStartAdded > 0) ||
                             (processMarkersEnd && stats.markersEndAdded > 0) ||
                             (processMarkersCenter && stats.markersCenterAdded > 0);
            
            if (hasMarkers) {
                msg += "\nДобавлено пользовательских маркеров:\n";
                if (processMarkersStart && stats.markersStartAdded > 0) {
                    msg += "  • В начале (" + InlinePicMarkerStart + "): " + stats.markersStartAdded + "\n";
                }
                if (processMarkersEnd && stats.markersEndAdded > 0) {
                    msg += "  • В конце (" + InlinePicMarkerEnd + "): " + stats.markersEndAdded + "\n";
                }
                if (processMarkersCenter && stats.markersCenterAdded > 0) {
                    msg += "  • В середине (" + InlinePicMarkerCenter + "): " + stats.markersCenterAdded + "\n";
                }
            }
            
            if (stats.emptyLinesAdded > 0) {
                msg += "\n• Добавлено пустых строк: " + stats.emptyLinesAdded + "\n";
            }
            
            if (stats.skippedTotal > 0) {
                msg += "\nПропущено иллюстраций внутри div элементов: " + stats.skippedTotal + "\n";
                if (stats.skippedHistory > 0) msg += "  • В history: " + stats.skippedHistory + "\n";
                if (stats.skippedAnnotation > 0) msg += "  • В annotation: " + stats.skippedAnnotation + "\n";
                if (stats.skippedEpigraph > 0) msg += "  • В epigraph: " + stats.skippedEpigraph + "\n";
                if (stats.skippedTitle > 0) msg += "  • В title: " + stats.skippedTitle + "\n";
                if (stats.skippedPoem > 0) msg += "  • В poem: " + stats.skippedPoem + "\n";
                if (stats.skippedCite > 0) msg += "  • В cite: " + stats.skippedCite + "\n";
                if (stats.skippedTable > 0) msg += "  • В table: " + stats.skippedTable + "\n";
            }
            
            msg += "\n---------------------------------------\n";
            msg += "Настройки обработки:\n\n";
            msg += "• Иллюстрации в пустых абзацах: " + (processEmptyPic ? "ДА" : "НЕТ") + "\n";
            msg += "• Иллюстрации в начале абзацев: " + (processStartPic ? "ДА" : "НЕТ") + "\n";
            msg += "• Иллюстрации в конце абзацев: " + (processEndPic ? "ДА" : "НЕТ") + "\n";
            msg += "• Иллюстрации в середине абзацев: " + (processCenterPic ? "ДА" : "НЕТ") + "\n\n";
            msg += "• Вставка маркеров в начале абзацев: " + (processMarkersStart ? "ДА" : "НЕТ") + "\n";
            msg += "• Вставка маркеров в конце абзацев: " + (processMarkersEnd ? "ДА" : "НЕТ") + "\n";
            msg += "• Вставка маркеров в середине абзацев: " + (processMarkersCenter ? "ДА" : "НЕТ") + "\n\n";
            msg += "• Добавление пустых строк: " + (AddEmptyLines ? "ДА" : "НЕТ") + "\n\n";
            msg += "• Обработка раздела сносок (примечаний): " + (processNotesSection ? "ДА" : "НЕТ") + "\n";
            msg += "• Обработка раздела комментариев: " + (processCommentsSection ? "ДА" : "НЕТ") + "\n";
            
            msg += "\nВремя выполнения: " + timeStr + " сек";
            
            MsgBox(msg, "FBE скрипт");
        }
        
    } catch (e) {
        window.external.EndUndoUnit(document);
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка выполнения:\n" + e.message, "FBE скрипт");
    }
}
