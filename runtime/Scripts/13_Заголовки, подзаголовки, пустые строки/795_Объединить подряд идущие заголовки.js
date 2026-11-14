// Скрипт "Объединить подряд идущие заголовки" для редактора FBE
// version 2.3
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для объединения одноуровневых заголовков в fb2 документах,
// когда перед непустой секцией находится пустая секция (без значимого текста).
// Заголовок из непустой секции переносится в заголовок пустой секции,
// пустая секция удаляется, содержимое непустой секции переходит в объединённую.
// Два режима объединения заголовков: отдельными абзацами
// или в продолжение последнего абзаца верхнего заголовка через точку с пробелом.
// При отсутствии выделения скрипт обрабатывает весь документ.
// При выделении проверяются все секции внутри выделения.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 2.3, 26.06.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Объединить подряд идущие заголовки";
    var version = "2.3";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима
    
    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0; // 0 - нет, 1 - да
    
    // Обрабатывать раздел комментариев
    var processCommentsSection = 0; // 0 - нет, 1 - да
    
    // Режим объединения заголовков:
    // 1 - отдельными абзацами (второй заголовок добавляется новым P)
    // 2 - нижний заголовок присоединяется к последнему абзацу верхнего заголовка через точку и пробел
    var mergeMode = 1;
    
    // Список "мусорных" символов для удаления (шестнадцатеричные коды Unicode через запятую)
    // Символы из Области частного использования (Private Use Area):
    // Для добавления новых: укажите код через запятую, например: "F06E,F06F,E000"
    var junkSymbolsHex = "F06E,F04A";
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    var undoMsg = "Объединить подряд идущие заголовки";
    
    try { 
        var nbspChar = window.external.GetNBSP(); 
        var nbspEntity; 
        if (nbspChar.charCodeAt(0) == 160) 
            nbspEntity = "&nbsp;"; 
        else 
            nbspEntity = nbspChar; 
    } catch(e) { 
        var nbspChar = String.fromCharCode(160); 
        var nbspEntity = "&nbsp;";
    }
    
    // Собираем регулярное выражение для мусорных символов из настройки
    function buildJunkRegExp() {
        var hexList = junkSymbolsHex.split(",");
        var charClass = "";
        for (var i = 0; i < hexList.length; i++) {
            var hex = hexList[i].replace(/^\s+|\s+$/g, "");
            if (hex.length > 0) {
                charClass += "\\u" + hex;
            }
        }
        if (charClass.length == 0) {
            // Если список пуст, создаём регэксп, который ничего не находит
            return new RegExp("(?!)", "gi");
        }
        return new RegExp("([" + charClass + "])", "gi");
    }
    
    var rePrivateUse = buildJunkRegExp();
    
    function makeEmptyLineRegExp() {
        return new RegExp("^( |\u00A0|&nbsp;|" + nbspChar + ")*?$", "i");
    }
    
    // Проверка: является ли строка пустой после удаления мусорных символов
    function isLineEmptyAfterCleanup(ptr) {
        var html = ptr.innerHTML;
        html = html.replace(rePrivateUse, "");
        var textOnly = html.replace(/<[^>]*?>/gi, "");
        return makeEmptyLineRegExp().test(textOnly);
    }
    
    // Проверка: является ли секция пустой (нет значимого текста, исключая заголовок)
    function isSectionEmpty(sectionElement) {
        function collectTextOutsideTitle(container, textParts) {
            var children = container.childNodes;
            for (var i = 0; i < children.length; i++) {
                var child = children[i];
                if (child.nodeType == 3) {
                    textParts.push(child.nodeValue);
                } else if (child.nodeType == 1) {
                    if (child.nodeName == "DIV" && child.className == "title") {
                        // Заголовок не учитываем при проверке пустоты секции
                        continue;
                    }
                    if (child.innerText !== undefined) {
                        textParts.push(child.innerText);
                    } else {
                        collectTextOutsideTitle(child, textParts);
                    }
                }
            }
        }
        
        var allText = [];
        collectTextOutsideTitle(sectionElement, allText);
        var combinedText = allText.join("");
        
        // Удаляем мусорные символы
        combinedText = combinedText.replace(rePrivateUse, "");
        // Удаляем пробельные символы
        combinedText = combinedText.replace(/\s/g, "");
        combinedText = combinedText.replace(/\u00A0/g, "");
        if (nbspChar.charCodeAt(0) != 160) {
            combinedText = combinedText.replace(new RegExp(nbspChar, "g"), "");
        }
        combinedText = combinedText.replace(/&nbsp;/gi, "");
        
        return combinedText.length == 0;
    }
    
    // Проверка: находится ли элемент в разделе сносок или комментариев
    function isInSpecialSection(element) {
        while (element && element.nodeName != "BODY") {
            if (element.nodeName == "DIV" && element.className == "body") {
                var fbname = element.getAttribute("fbname") || "";
                if (fbname == "notes" && !processNotesSection) return true;
                if (fbname == "comments" && !processCommentsSection) return true;
                return false;
            }
            element = element.parentNode;
        }
        return false;
    }
    
    // Находим заголовок внутри секции
    function findTitleInSection(sectionElement) {
        var children = sectionElement.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "title") {
                return child;
            }
        }
        return null;
    }
    
    // Удаляет мусорные элементы сразу после заголовка (и только их)
    function cleanupAfterTitle(sectionElement) {
        var titleEl = findTitleInSection(sectionElement);
        if (!titleEl) return;
        
        // Идём по элементам сразу после title и удаляем мусорные,
        // пока не встретим что-то значимое
        var current = titleEl.nextSibling;
        var toRemove = [];
        
        while (current) {
            var isMuseum = false;
            
            if (current.nodeType == 3) {
                // Текстовый узел — проверяем на мусорность
                var cleaned = current.nodeValue.replace(rePrivateUse, "");
                cleaned = cleaned.replace(/\s/g, "");
                cleaned = cleaned.replace(/\u00A0/g, "");
                if (nbspChar.charCodeAt(0) != 160) {
                    cleaned = cleaned.replace(new RegExp(nbspChar, "g"), "");
                }
                cleaned = cleaned.replace(/&nbsp;/gi, "");
                if (cleaned.length == 0) {
                    isMuseum = true;
                }
            } else if (current.nodeType == 1 && current.nodeName == "P") {
                // Очищаем P от мусорных символов
                var s = current.innerHTML;
                if (s.search(rePrivateUse) != -1) {
                    s = s.replace(rePrivateUse, "");
                    current.innerHTML = s;
                }
                // Проверяем, стал ли P пустым после очистки
                if (isLineEmptyAfterCleanup(current)) {
                    isMuseum = true;
                }
            }
            
            if (isMuseum) {
                toRemove.push(current);
                current = current.nextSibling;
            } else {
                // Встретили значимый элемент — прекращаем
                break;
            }
        }
        
        // Удаляем собранные мусорные элементы в обратном порядке
        for (var i = toRemove.length - 1; i >= 0; i--) {
            toRemove[i].removeNode(false);
        }
    }
    
    // Переносим содержимое заголовка sourceTitle в targetTitle согласно mergeMode
    function mergeTitles(targetTitle, sourceTitle, mode) {
        // Собираем все P из sourceTitle
        var sourcePElements = [];
        var sourceChildren = sourceTitle.childNodes;
        for (var i = 0; i < sourceChildren.length; i++) {
            if (sourceChildren[i].nodeType == 1 && sourceChildren[i].nodeName == "P") {
                sourcePElements.push(sourceChildren[i]);
            }
        }
        
        if (mode == 1) {
            // Режим: отдельными абзацами
            for (var j = 0; j < sourcePElements.length; j++) {
                var pElement = sourcePElements[j];
                sourceTitle.removeChild(pElement);
                targetTitle.appendChild(pElement);
            }
        } else if (mode == 2) {
            // Режим: в один абзац через точку и пробел
            if (sourcePElements.length == 0) return;
            
            // Находим последний P в целевом заголовке
            var targetPElements = [];
            var targetChildren = targetTitle.childNodes;
            for (var k = 0; k < targetChildren.length; k++) {
                if (targetChildren[k].nodeType == 1 && targetChildren[k].nodeName == "P") {
                    targetPElements.push(targetChildren[k]);
                }
            }
            
            if (targetPElements.length == 0) {
                // Если в целевом заголовке нет P, просто переносим
                for (var j = 0; j < sourcePElements.length; j++) {
                    var pElement = sourcePElements[j];
                    sourceTitle.removeChild(pElement);
                    targetTitle.appendChild(pElement);
                }
                return;
            }
            
            // Берём последний P целевого заголовка и первый из исходного
            var lastTargetP = targetPElements[targetPElements.length - 1];
            var firstSourceP = sourcePElements[0];
            
            // Добавляем ". " к последнему P целевого заголовка
            var targetHTML = lastTargetP.innerHTML;
            targetHTML = targetHTML + ". " + firstSourceP.innerHTML;
            lastTargetP.innerHTML = targetHTML;
            
            // Удаляем первый P из исходного заголовка
            sourceTitle.removeChild(firstSourceP);
            
            // Остальные P из исходного заголовка переносим как отдельные абзацы
            while (sourcePElements.length > 1) {
                sourcePElements = [];
                var sc = sourceTitle.childNodes;
                for (var m = 0; m < sc.length; m++) {
                    if (sc[m].nodeType == 1 && sc[m].nodeName == "P") {
                        sourcePElements.push(sc[m]);
                    }
                }
                if (sourcePElements.length == 0) break;
                var pElem = sourcePElements[0];
                sourceTitle.removeChild(pElem);
                targetTitle.appendChild(pElem);
                sourcePElements.splice(0, 1);
            }
        }
    }
    
    // Проверка: попадает ли элемент в текущее выделение
    function isElementInSelection(element) {
        try {
            var selRange = document.selection.createRange();
            
            if (selRange.type && selRange.type == "Control") {
                try {
                    var controlRange = selRange;
                    for (var c = 0; c < controlRange.length; c++) {
                        var item = controlRange.item(c);
                        if (item === element || item.contains(element)) {
                            return true;
                        }
                    }
                } catch(e) {}
                return false;
            }
            
            // Text range
            var elemRange = document.body.createTextRange();
            elemRange.moveToElementText(element);
            
            if (elemRange.compareEndPoints("StartToEnd", selRange) > 0) return false;
            if (elemRange.compareEndPoints("EndToStart", selRange) < 0) return false;
            
            return true;
        } catch(e) {
            return false;
        }
    }
    
    // ==================================================
    // ОСНОВНОЙ АЛГОРИТМ
    // ==================================================
    
    window.external.BeginUndoUnit(document, undoMsg);
    
    try { window.external.SetStatusBarText("Поиск секций для объединения..."); } catch(e) {}
    
    var startTime = new Date();
    
    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) {
        if (showStatistics) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Не найден контейнер fbw_body.");
        }
        try { window.external.SetStatusBarText("ОК"); } catch(e) {}
        window.external.EndUndoUnit(document);
        return;
    }
    
    // Определяем, есть ли выделение
    var hasSelection = false;
    if (document.selection && document.selection.type) {
        var selType = document.selection.type.toLowerCase();
        if (selType == "text" || selType == "control") {
            hasSelection = true;
        }
    }
    
    // Список секций для обработки
    var sectionsToProcess = [];
    
    if (hasSelection) {
        // Режим выделения: собираем все секции внутри выделения
        function collectSectionsInSelection(container) {
            var children = container.childNodes;
            for (var i = 0; i < children.length; i++) {
                var child = children[i];
                if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "section") {
                    if (!isInSpecialSection(child) && isElementInSelection(child)) {
                        // Находим предыдущую сестринскую секцию
                        var prevSibling = child.previousSibling;
                        while (prevSibling && prevSibling.nodeType != 1) {
                            prevSibling = prevSibling.previousSibling;
                        }
                        
                        if (prevSibling && prevSibling.nodeName == "DIV" && prevSibling.className == "section") {
                            // Проверяем равноуровневость
                            if (child.parentNode === prevSibling.parentNode) {
                                // Проверяем, что верхняя секция пустая
                                if (isSectionEmpty(prevSibling)) {
                                    var upperTitle = findTitleInSection(prevSibling);
                                    var lowerTitle = findTitleInSection(child);
                                    
                                    if (upperTitle && lowerTitle) {
                                        // Проверяем, не обработана ли уже эта верхняя секция
                                        var alreadyProcessed = false;
                                        for (var s = 0; s < sectionsToProcess.length; s++) {
                                            if (sectionsToProcess[s].upperSection === prevSibling) {
                                                alreadyProcessed = true;
                                                break;
                                            }
                                        }
                                        
                                        if (!alreadyProcessed) {
                                            sectionsToProcess.push({
                                                upperSection: prevSibling,
                                                lowerSection: child,
                                                upperTitle: upperTitle,
                                                lowerTitle: lowerTitle
                                            });
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                // Рекурсивно обрабатываем вложенные элементы
                if (child.nodeType == 1) {
                    collectSectionsInSelection(child);
                }
            }
        }
        
        collectSectionsInSelection(fbwBody);
    } else {
        // Режим без выделения: собираем все секции
        function collectSections(container) {
            var children = container.childNodes;
            for (var i = 0; i < children.length; i++) {
                var child = children[i];
                if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "section") {
                    if (!isInSpecialSection(child)) {
                        // Находим предыдущую сестринскую секцию
                        var prevSibling = child.previousSibling;
                        while (prevSibling && prevSibling.nodeType != 1) {
                            prevSibling = prevSibling.previousSibling;
                        }
                        
                        if (prevSibling && prevSibling.nodeName == "DIV" && prevSibling.className == "section") {
                            // Проверяем равноуровневость
                            if (child.parentNode === prevSibling.parentNode) {
                                // Проверяем, что верхняя секция пустая
                                if (isSectionEmpty(prevSibling)) {
                                    var upperTitle = findTitleInSection(prevSibling);
                                    var lowerTitle = findTitleInSection(child);
                                    
                                    if (upperTitle && lowerTitle) {
                                        // Проверяем, не обработана ли уже эта верхняя секция
                                        var alreadyProcessed = false;
                                        for (var s = 0; s < sectionsToProcess.length; s++) {
                                            if (sectionsToProcess[s].upperSection === prevSibling) {
                                                alreadyProcessed = true;
                                                break;
                                            }
                                        }
                                        
                                        if (!alreadyProcessed) {
                                            sectionsToProcess.push({
                                                upperSection: prevSibling,
                                                lowerSection: child,
                                                upperTitle: upperTitle,
                                                lowerTitle: lowerTitle
                                            });
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                // Рекурсивно обрабатываем вложенные элементы
                if (child.nodeType == 1) {
                    collectSections(child);
                }
            }
        }
        
        collectSections(fbwBody);
    }
    
    // Статистика
    var mergedCount = 0;
    
    // Обрабатываем секции в обратном порядке (для безопасного удаления)
    for (var t = sectionsToProcess.length - 1; t >= 0; t--) {
        var data = sectionsToProcess[t];
        var upperSection = data.upperSection;
        var lowerSection = data.lowerSection;
        var upperTitle = data.upperTitle;
        var lowerTitle = data.lowerTitle;
        
        // Объединяем заголовки
        mergeTitles(upperTitle, lowerTitle, mergeMode);
        
        // Удаляем пустой title в нижней секции
        lowerTitle.removeNode(false);
        
        // Переносим всё содержимое нижней секции в верхнюю
        while (lowerSection.firstChild) {
            var childToMove = lowerSection.firstChild;
            lowerSection.removeChild(childToMove);
            upperSection.appendChild(childToMove);
        }
        
        // Удаляем пустую нижнюю секцию
        lowerSection.removeNode(false);
        
        // Чистим ТОЛЬКО мусорные элементы сразу после заголовка
        cleanupAfterTitle(upperSection);
        
        mergedCount++;
    }
    
    // Вычисляем время выполнения
    var endTime = new Date();
    var timeDiff = (endTime - startTime) / 1000;
    var timeStr = timeDiff.toFixed(3);
    timeStr = timeStr.replace(".", ",");
    
    // Формируем сообщение статистики
    function buildReportMsg() {
        var msg = scriptName + "\n";
        msg += "ver. " + version + "\n";
        msg += "---------------------------------------\n\n";
        
        if (mergedCount > 0) {
            msg += "\u221A Объединено заголовков: " + mergedCount + "\n";
        } else {
            if (hasSelection) {
                msg += "\u221A В выделении не найдено подходящих секций для объединения.\n";
            } else {
                msg += "\u221A Не найдено подходящих секций для объединения.\n";
            }
        }
        
        msg += "\n---------------------------------------\n";
        msg += "Настройки обработки:\n";
        msg += "  \u2022 Обработка раздела сносок: " + (processNotesSection ? "\u221A (ДА)" : "\u2717 (НЕТ)") + "\n";
        msg += "  \u2022 Обработка раздела комментариев: " + (processCommentsSection ? "\u221A (ДА)" : "\u2717 (НЕТ)") + "\n";
        
        var modeText = (mergeMode == 1) ? "Отдельными абзацами" : "В один абзац (через точку и пробел)";
        msg += "  \u2022 Режим объединения: " + modeText + "\n";
        msg += "  \u2022 Мусорные символы (hex): " + junkSymbolsHex + "\n";
        
        msg += "\n---------------------------------------\n";
        msg += "Время выполнения: " + timeStr + " сек.";
        
        return msg;
    }
    
    if (showStatistics) {
        MsgBox(buildReportMsg());
    } else {
        // Тихий режим: показываем сообщение только если ничего не найдено
        if (mergedCount == 0) {
            MsgBox(buildReportMsg());
        }
    }
    
    try { window.external.SetStatusBarText("ОК"); } catch(e) {}
    window.external.EndUndoUnit(document);
}
