// Скрипт "Переместить курсивные или жирные абзацы в заголовки" для редактора FBE
// version 1.3
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для переноса курсивных и/или жирных абзацев
// внутрь ближайшего верхнего заголовка в fb2 документах.
// Скрипт ищет "сразу" после каждого заголовка подходящие абзацы (пропуская пустые строки и картинки)
// и перемещает их в конец заголовка, отдельным последним абзацем (-ами).
// Т.е. основное условие - курсивный и/или жирный абзац должны идти после заголовка до обычного неформатированного текста.
// Если после заголовка первым идет обычный некурсивный или нежирный абзац,
//  то такая секция не обрабатывается, пропускается.
// Поддерживается перенос только курсива, только жирного, только их сочетания или всех вариантов сразу.
// Настраивается количество переносимых подряд абзацев (один, несколько или все).
// Защищены от переноса абзацы внутри блочных элементов: эпиграфов, цитат, стихов, таблиц и других DIV элементов,
// а также элементы subtitle и text-author.
// Если после переноса в секции не останется обычного текста, скрипт пропускает такой заголовок.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.3, 23.06.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Переместить курсивные или жирные абзацы в заголовки";
    var version = "1.3";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима
    
    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0; // 0 - нет, 1 - да
    
    // Обрабатывать раздел комментариев
    var processCommentsSection = 0; // 0 - нет, 1 - да
    
    // Обработка вариантов искомых абзацев:
    // 0 - Переносить все варианты: жирные и курсивные абзацы а также их сочетание (жирность+курсив)
    // 1 - Переносить только курсивные абзацы
    // 2 - Переносить только жирные абзацы
    // 3 - Переносить только одновременно и жирные и курсивные абзацы (сочетание)
    var processParagraphs = 1;
    
    // Переносить искомые абзацы, идущие после блочных иллюстраций и пустых строк (заголовок-иллюстрация-искомый абзац)
    var processImages = 1; // 0 - нет, 1 - да
    
    // Обработка искомых абзацев, если их отделяют от заголовка только пустые строки
    var processEmptyLines = 1; // 0 - нет, 1 - да
    
    // Сколько подряд идущих подходящих абзацев переносить:
    // 0 - переносить все подряд идущие подходящие абзацы
    // 1 - переносить только один (ближайший после заголовка)
    // 2 - переносить не более 2 подряд идущих
    // 3 - переносить не более 3 подряд идущих
    // и т.д.
    var MaxNumber = 1;
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    var undoMsg = "переместить курсивные или жирные абзацы в заголовки";
    
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
    
    var re5 = new RegExp("<EM>(((?!</?EM>).)*)</EM>", "ig");
    var re6 = new RegExp("</?[^>]*>", "ig");
    var reStrong = new RegExp("<STRONG>(((?!</?STRONG>).)*)</STRONG>", "ig");
    var reStrongEm = new RegExp("<STRONG><EM>(((?!</?EM>).)*)</EM></STRONG>", "ig");
    var reEmStrong = new RegExp("<EM><STRONG>(((?!</?STRONG>).)*)</STRONG></EM>", "ig");
    var re2 = new RegExp(" |&nbsp;|" + nbspChar, "g");
    
    var emptyLineRegExp = new RegExp("^( |\u00A0|&nbsp;|" + nbspChar + ")*?$", "i");
    
    function isLineEmpty(ptr) {
        return emptyLineRegExp.test(ptr.innerHTML.replace(/<[^>]*?>/gi, ""));
    }
    
    // Проверка: является ли абзац целиком курсивным
    function isEntirelyItalic(elem1) {
        if (isLineEmpty(elem1)) return false;
        var myHtml = elem1.innerHTML;
        var searchResult = re5.test(myHtml);
        while (searchResult) {
            myHtml = myHtml.replace(re5, "");
            searchResult = re5.test(myHtml);
        }
        myHtml = myHtml.replace(re6, "");
        if (myHtml == "" || myHtml.replace(re2, "") == "") return true;
        return false;
    }
    
    // Проверка: является ли абзац целиком жирным
    function isEntirelyStrong(elem1) {
        if (isLineEmpty(elem1)) return false;
        var myHtml = elem1.innerHTML;
        var searchResult = reStrong.test(myHtml);
        while (searchResult) {
            myHtml = myHtml.replace(reStrong, "");
            searchResult = reStrong.test(myHtml);
        }
        myHtml = myHtml.replace(re6, "");
        if (myHtml == "" || myHtml.replace(re2, "") == "") return true;
        return false;
    }
    
    // Проверка: является ли абзац целиком жирно-курсивным (STRONG+EM)
    function isEntirelyStrongEm(elem1) {
        if (isLineEmpty(elem1)) return false;
        var myHtml = elem1.innerHTML;
        var searchResult = reStrongEm.test(myHtml);
        while (searchResult) {
            myHtml = myHtml.replace(reStrongEm, "");
            searchResult = reStrongEm.test(myHtml);
        }
        myHtml = myHtml.replace(re6, "");
        if (myHtml == "" || myHtml.replace(re2, "") == "") return true;
        return false;
    }
    
    // Проверка: является ли абзац целиком курсивно-жирным (EM+STRONG)
    function isEntirelyEmStrong(elem1) {
        if (isLineEmpty(elem1)) return false;
        var myHtml = elem1.innerHTML;
        var searchResult = reEmStrong.test(myHtml);
        while (searchResult) {
            myHtml = myHtml.replace(reEmStrong, "");
            searchResult = reEmStrong.test(myHtml);
        }
        myHtml = myHtml.replace(re6, "");
        if (myHtml == "" || myHtml.replace(re2, "") == "") return true;
        return false;
    }
    
    // Проверка: подходит ли абзац под настройку processParagraphs
    function isMatchingParagraph(elem1) {
        if (isLineEmpty(elem1)) return false;
        
        var isItalic = isEntirelyItalic(elem1);
        var isStrong = isEntirelyStrong(elem1);
        var isStrongEm = isEntirelyStrongEm(elem1);
        var isEmStrong = isEntirelyEmStrong(elem1);
        var isCombined = isStrongEm || isEmStrong;
        
        if (processParagraphs == 0) {
            return isItalic || isStrong || isCombined;
        } else if (processParagraphs == 1) {
            return isItalic && !isCombined;
        } else if (processParagraphs == 2) {
            return isStrong && !isCombined;
        } else if (processParagraphs == 3) {
            return isCombined;
        }
        return false;
    }
    
    // Проверка: находится ли абзац внутри DIV-контейнера, в котором его нельзя трогать
    // или является subtitle/text-author
    function isInProtectedContainer(elem1) {
        // Проверяем сам элемент на subtitle или text-author
        if (elem1.nodeName == "P") {
            var elemClass = elem1.className || "";
            if (elemClass == "subtitle" || elemClass == "text-author") {
                return true;
            }
        }
        
        // Проверяем всех родителей на защищенные DIV-контейнеры
        var parent = elem1.parentNode;
        while (parent && parent.nodeName != "BODY") {
            if (parent.nodeName == "DIV") {
                var pClass = parent.className || "";
                if (pClass == "annotation" || pClass == "epigraph" || pClass == "cite" || 
                    pClass == "poem" || pClass == "stanza" || pClass == "table" || 
                    pClass == "image") {
                    return true;
                }
            }
            parent = parent.parentNode;
        }
        return false;
    }
    
    // Проверка: является ли элемент иллюстрацией (DIV class="image")
    function isImageElement(el) {
        return el && el.nodeName == "DIV" && el.className == "image";
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
    
    // Проверка: останется ли в секции хотя бы один непустой обычный абзац после удаления указанных
    function sectionHasRemainingContent(sectionElement, paragraphsToRemove) {
        // Рекурсивно проверяем все P внутри секции
        function checkAllP(container) {
            var children = container.childNodes;
            for (var i = 0; i < children.length; i++) {
                var child = children[i];
                if (child.nodeType == 1) {
                    if (child.nodeName == "P") {
                        // Проверяем, не входит ли этот P в список на удаление
                        var isToRemove = false;
                        for (var j = 0; j < paragraphsToRemove.length; j++) {
                            if (child == paragraphsToRemove[j]) {
                                isToRemove = true;
                                break;
                            }
                        }
                        // Если не удаляется и не пустой - значит есть остаток
                        if (!isToRemove && !isLineEmpty(child)) {
                            return true;
                        }
                    }
                    // Рекурсивно проверяем дочерние элементы
                    if (checkAllP(child)) {
                        return true;
                    }
                }
            }
            return false;
        }
        
        return checkAllP(sectionElement);
    }
    
    function getNextNode(el) {
        if (el.firstChild && el.nodeName != "P")
            el = el.firstChild;
        else {
            while (el && !el.nextSibling)
                el = el.parentNode;
            if (el && el.nextSibling) el = el.nextSibling;
        }
        return el;
    }
    
    function getNextP(el) {
        var savedEl = el;
        while (el && (el.nodeName != "P" || el == savedEl))
            el = getNextNode(el);
        return el;
    }
    
    // ==================================================
    // ОСНОВНОЙ АЛГОРИТМ
    // ==================================================
    
    window.external.BeginUndoUnit(document, undoMsg);
    
    try { window.external.SetStatusBarText("Поиск и перемещение абзацев..."); } catch(e) {}
    
    var startTime = new Date();
    
    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) {
        if (showStatistics) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Не найден контейнер fbw_body");
        }
        try { window.external.SetStatusBarText("ОК"); } catch(e) {}
        window.external.EndUndoUnit(document);
        return;
    }
    
    // Собираем все заголовки (DIV class="title")
    var allTitles = [];
    
    function collectTitles(container) {
        var children = container.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 1) {
                if (child.nodeName == "DIV" && child.className == "title") {
                    var parent = child.parentNode;
                    if (parent && parent.nodeName == "DIV" && 
                        (parent.className == "section" || parent.className == "body")) {
                        if (!isInSpecialSection(child)) {
                            allTitles.push(child);
                        }
                    }
                }
                collectTitles(child);
            }
        }
    }
    
    collectTitles(fbwBody);
    
    // Статистика
    var titlesChanged = 0;
    var italicMoved = 0;
    var strongMoved = 0;
    var combinedMoved = 0;
    
    // Обрабатываем заголовки в обратном порядке (для безопасного перемещения)
    for (var t = allTitles.length - 1; t >= 0; t--) {
        var titleElement = allTitles[t];
        
        // Находим секцию, содержащую этот заголовок
        var sectionElement = titleElement.parentNode;
        
        // Ищем ближайший абзац после заголовка
        var searchEl = titleElement;
        while (searchEl && searchEl.nextSibling == null) {
            searchEl = searchEl.parentNode;
        }
        if (!searchEl || !searchEl.nextSibling) continue;
        
        var ptr = searchEl.nextSibling;
        
        // Пропускаем текстовые узлы
        while (ptr && ptr.nodeType != 1) {
            ptr = getNextNode(ptr);
            if (!ptr) break;
        }
        
        if (!ptr) continue;
        
        // Собираем подряд идущие подходящие абзацы
        var matchedParagraphs = [];
        var currentPtr = ptr;
        var stopCollecting = false;
        
        while (currentPtr && fbwBody.contains(currentPtr) && !stopCollecting) {
            // Если наткнулись на следующий заголовок - прекращаем
            if (currentPtr.nodeName == "DIV" && currentPtr.className == "title") {
                break;
            }
            
            // Если нашли P
            if (currentPtr.nodeName == "P") {
                if (isLineEmpty(currentPtr)) {
                    if (matchedParagraphs.length > 0) {
                        // Уже нашли подходящие абзацы, пустая строка их прерывает
                        break;
                    }
                    if (processEmptyLines) {
                        currentPtr = getNextP(currentPtr);
                        continue;
                    } else {
                        break;
                    }
                } else {
                    // Непустой P
                    if (isMatchingParagraph(currentPtr) && !isInProtectedContainer(currentPtr)) {
                        matchedParagraphs.push(currentPtr);
                        
                        // Проверяем, не достигли ли лимита
                        if (MaxNumber > 0 && matchedParagraphs.length >= MaxNumber) {
                            break;
                        }
                        
                        currentPtr = getNextP(currentPtr);
                        continue;
                    } else {
                        // Не подходит - прекращаем
                        break;
                    }
                }
            }
            
            // Если нашли иллюстрацию
            if (isImageElement(currentPtr)) {
                if (matchedParagraphs.length > 0) {
                    // Уже нашли подходящие абзацы, иллюстрация их прерывает
                    break;
                }
                if (processImages) {
                    currentPtr = getNextP(currentPtr);
                    continue;
                } else {
                    break;
                }
            }
            
            // Любой другой не-P элемент
            if (matchedParagraphs.length > 0) {
                break;
            }
            
            // Если ещё не нашли подходящих абзацев, а встретили другой элемент - прекращаем
            break;
        }
        
        // Если нашли подходящие абзацы
        if (matchedParagraphs.length > 0) {
            // Проверяем, останется ли в секции хотя бы один непустой обычный абзац после удаления
            if (!sectionHasRemainingContent(sectionElement, matchedParagraphs)) {
                // Не останется - пропускаем этот заголовок
                continue;
            }
            
            // Перемещаем абзацы в заголовок (в прямом порядке, они уже в нужной последовательности)
            for (var m = 0; m < matchedParagraphs.length; m++) {
                var targetP = matchedParagraphs[m];
                
                // Определяем тип абзаца для статистики
                var isItalic = isEntirelyItalic(targetP);
                var isStrong = isEntirelyStrong(targetP);
                var isStrongEm = isEntirelyStrongEm(targetP);
                var isEmStrong = isEntirelyEmStrong(targetP);
                var isCombined = isStrongEm || isEmStrong;
                
                // Удаляем абзац с текущего места
                targetP.parentNode.removeChild(targetP);
                
                // Вставляем в конец заголовка
                titleElement.appendChild(targetP);
                
                titlesChanged++;
                if (isCombined) {
                    combinedMoved++;
                } else if (isItalic) {
                    italicMoved++;
                } else if (isStrong) {
                    strongMoved++;
                }
            }
        }
    }
    
    // Вычисляем время выполнения
    var endTime = new Date();
    var timeDiff = (endTime - startTime) / 1000;
    
    // Формируем сообщение статистики
    function buildReportMsg() {
        var msg = scriptName + "\n";
        msg += "ver. " + version + "\n";
        msg += "---------------------------------------\n\n";
        
        if (titlesChanged > 0) {
            msg += "\u221A Заголовков изменено всего: " + titlesChanged + "\n";
            msg += "\u221A Перенесено курсивных абзацев: " + italicMoved + "\n";
            msg += "\u221A Перенесено жирных абзацев: " + strongMoved + "\n";
            if (combinedMoved > 0) {
                msg += "\u221A Перенесено жирно-курсивных абзацев: " + combinedMoved + "\n";
            }
        } else {
            msg += "\u221A Не найдено подходящих абзацев для переноса\n";
        }
        
        msg += "\n---------------------------------------\n";
        msg += "Настройки обработки:\n";
        msg += "  \u2022 Обработка раздела сносок: " + (processNotesSection ? "\u221A (ДА)" : "\u2717 (НЕТ)") + "\n";
        msg += "  \u2022 Обработка раздела комментариев: " + (processCommentsSection ? "\u221A (ДА)" : "\u2717 (НЕТ)") + "\n";
        
        var modeText = "";
        if (processParagraphs == 0) modeText = "Все варианты (курсив, жирный, сочетание)";
        else if (processParagraphs == 1) modeText = "Только курсивные";
        else if (processParagraphs == 2) modeText = "Только жирные";
        else if (processParagraphs == 3) modeText = "Только жирно-курсивные";
        msg += "  \u2022 Режим переноса: " + modeText + "\n";
        msg += "  \u2022 Абзацы после иллюстраций: " + (processImages ? "\u221A (ДА)" : "\u2717 (НЕТ)") + "\n";
        msg += "  \u2022 Абзацы после пустых строк: " + (processEmptyLines ? "\u221A (ДА)" : "\u2717 (НЕТ)") + "\n";
        
        var maxConsText = (MaxNumber == 0) ? "Все подряд" : "Не более " + MaxNumber.toString();
        msg += "  \u2022 Максимум подряд искомых абзацев: " + maxConsText + "\n";
        
        msg += "\n---------------------------------------\n";
        msg += "Время выполнения: " + timeDiff.toFixed(3).replace(".", ",") + " сек.";
        
        return msg;
    }
    
    if (showStatistics) {
        MsgBox(buildReportMsg());
    } else {
        // Тихий режим: показываем сообщение только если ничего не найдено
        if (titlesChanged == 0) {
            MsgBox(buildReportMsg());
        }
    }
    
    try { window.external.SetStatusBarText("ОК"); } catch(e) {}
    window.external.EndUndoUnit(document);
}
