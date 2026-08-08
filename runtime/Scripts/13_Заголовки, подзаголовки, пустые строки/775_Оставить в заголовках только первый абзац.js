// Скрипт "Оставить в заголовках только первый абзац" для редактора FBE
// version 1.9
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для обработки заголовков в fb2 документах, содержащих более одного абзаца.
// Из таких заголовков вырезаются все абзацы кроме первого и переносятся ниже
// в виде подзаголовков или обычных абзацев (в зависимости от настроек).
// Скрипт ищет подходящие заголовки вниз от положения курсора или в выделенном фрагменте.
// Обрабатывается только основной раздел документа, без разделов сносок и комментариев.
// Заголовок основного body и первый заголовок в документе (если нет заголовка body) не обрабатываются.
// Вырезанные из заголовков абзацы могут вставляться после заголовка (согласно настройкам)
// в виде подзаголовков или в виде обычных абзацев.
// При наличии после исходного заголовка иллюстрации, вырезанный текст
// может вставляться (согласно настройкам) до или после иллюстрации.
// При вставке обычных абзацев рядом с примыкающими иллюстрациями могут опционально добавляться пустые строки.
// При наличии после исходного заголовка эпиграфа, вырезанный текст вставляется всегда после эпиграфа.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.9, 20.04.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Оставить в заголовках только первый абзац";
    var version = "1.9";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1;
    
    // Обрабатывать заголовки любого уровня (кроме заголовка основного боди и самого первого в документе) или только вложенные
    // 0 - обрабатывать любые, 1 - только вложенные
    var titleLevel = 1;
    
    // Создавать из вырезанного текста подзаголовки или обычные абзацы
    // 0 - создавать обычные абзацы, 1 - создавать подзаголовки
    var createSubtitles = 1;
    
    // Вставлять вырезанный текст до или после иллюстрации (если она расположена сразу после заголовка или после эпиграфа)
    // 0 - вставлять до иллюстрации, 1 - вставлять после иллюстрации
    var insertBeforeImage = 1;
    
    // Добавлять пустые строки при вставке обычных абзацев рядом с иллюстрациями
    // 0 - не добавлять, 1 - добавлять
    var addEmptyLines = 1;
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    // Функция для показа сообщений (без дублирования названия и версии)
    function showMessage(msg, isError) {
        if (!showStatistics && !isError) return;
        MsgBox(msg, "FBE скрипт");
    }
    
    // Функция для показа сообщения с заголовком
    function showMessageWithHeader(msg, isError) {
        if (!showStatistics && !isError) return;
        MsgBox(scriptName + "\nver. " + version + "\n\n" + msg, "FBE скрипт");
    }
    
    // Получаем неразрывный пробел из настроек FBE
    var nbspChar;
    var nbspEntity;
    try {
        nbspChar = window.external.GetNBSP();
        if (nbspChar.charCodeAt(0) == 160)
            nbspEntity = "&nbsp;";
        else
            nbspEntity = nbspChar;
    }
    catch(e) {
        nbspChar = String.fromCharCode(160);
        nbspEntity = "&nbsp;";
    }
    
    // ==================================================
    // ФУНКЦИЯ: Очистка HTML элемента от мусора
    // ==================================================
    function cleanElementHTML(element) {
        if (!element) return;
        
        var html = element.innerHTML;
        
        // Замена &nbsp; на nbspChar
        var re211 = new RegExp("&nbsp;", "g");
        html = html.replace(re211, nbspChar);
        
        // Чистка пустых строк от пробелов и внутренних тегов (кроме SPAN с картинками)
        var re212ex = new RegExp("<SPAN [^>]{0,}?class=image", "g");
        if (!re212ex.test(html)) {
            var re212 = new RegExp("^(\\s|" + nbspChar + "|<[^>]{1,}>){1,}$", "g");
            if (re212.test(html)) {
                html = "";
            }
        }
        
        element.innerHTML = html;
    }
    
    // ==================================================
    // ФУНКЦИЯ: Получение родительской секции
    // ==================================================
    function GetCP(cp) {
        if (!cp) return null;
        
        if (cp.tagName == "P") cp = cp.parentElement;
        
        if (cp.tagName == "DIV" && cp.className == "title") cp = cp.parentElement;
        
        if (cp.tagName != "DIV") return null;
        
        return cp;
    }
    
    // ==================================================
    // ФУНКЦИЯ: Пропуск элементов с указанными классами
    // ==================================================
    function SkipOver(np, n1, n2, n3) {
        while (np) {
            if (!(np.tagName == "P" && !np.firstChild && !window.external.inflateBlock(np)) &&
                (!n1 || (np.tagName != n1 && np.className != n1)) &&
                (!n2 || (np.tagName != n2 && np.className != n2)) &&
                (!n3 || (np.tagName != n3 && np.className != n3))) {
                break;
            }
            np = np.nextSibling;
        }
        return np;
    }
    
    // ==================================================
    // ФУНКЦИЯ: Вставка перед элементом
    // ==================================================
    function InsBefore(parent, ref, item) {
        if (ref) {
            ref.insertAdjacentElement("beforeBegin", item);
        } else {
            parent.appendChild(item);
        }
    }
    
    // ==================================================
    // ФУНКЦИЯ: Определение типа раздела
    // ==================================================
    function getSectionType(element) {
        var parent = element.parentNode;
        while (parent) {
            if (parent.nodeName == "DIV" && parent.className == "body") {
                var fbname = parent.getAttribute("fbname") || "";
                if (fbname == "") return "main";
                return "other";
            }
            parent = parent.parentNode;
        }
        return "main";
    }
    
    // ==================================================
    // ФУНКЦИЯ: Является ли заголовок заголовком body
    // ==================================================
    function isBodyTitle(titleElement) {
        if (!titleElement) return false;
        var parent = titleElement.parentNode;
        if (parent && parent.nodeName == "DIV" && parent.className == "body") {
            return true;
        }
        return false;
    }
    
    // ==================================================
    // ФУНКЦИЯ: Проверка, есть ли внутри секции вложенные секции с заголовками
    // ==================================================
    function hasNestedSectionsWithTitles(sectionElement) {
        if (!sectionElement) return false;
        
        var children = sectionElement.childNodes;
        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            if (child.nodeType == 1 && child.nodeName == "DIV" && child.className == "section") {
                // Проверяем, есть ли в этой вложенной секции заголовок
                var subChildren = child.childNodes;
                for (var j = 0; j < subChildren.length; j++) {
                    var subChild = subChildren[j];
                    if (subChild.nodeType == 1 && subChild.nodeName == "DIV" && subChild.className == "title") {
                        return true;
                    }
                }
                // Рекурсивно проверяем глубже
                if (hasNestedSectionsWithTitles(child)) {
                    return true;
                }
            }
        }
        return false;
    }
    
    // ==================================================
    // ФУНКЦИЯ: Является ли заголовок вложенным (низшего уровня)
    // ==================================================
    function isNestedTitle(titleElement) {
        var parent = titleElement.parentNode;
        
        // Заголовок должен быть внутри секции
        if (!parent || parent.nodeName != "DIV" || parent.className != "section") {
            return false;
        }
        
        // Проверяем, есть ли внутри этой секции другие секции с заголовками
        // Если есть - это родительский заголовок, его пропускаем
        return !hasNestedSectionsWithTitles(parent);
    }
    
    // ==================================================
    // ФУНКЦИЯ: Поиск первого заголовка в основном разделе
    // ==================================================
    function findFirstTitleInMainBody() {
        var bodyDivs = document.getElementsByTagName("DIV");
        for (var i = 0; i < bodyDivs.length; i++) {
            var div = bodyDivs[i];
            if (div.className == "body" && (div.getAttribute("fbname") || "") == "") {
                var children = div.childNodes;
                for (var j = 0; j < children.length; j++) {
                    var child = children[j];
                    if (child.nodeType == 1) {
                        if (child.nodeName == "DIV" && child.className == "title") {
                            return child;
                        }
                        // Пропускаем image перед title
                        if (child.nodeName == "DIV" && child.className == "image") {
                            continue;
                        }
                        // Если первый элемент не title и не image, то заголовка body нет
                        break;
                    }
                }
                // Проверяем вложенные секции
                var sections = div.getElementsByTagName("DIV");
                for (var k = 0; k < sections.length; k++) {
                    var sec = sections[k];
                    if (sec.className == "section") {
                        var secChildren = sec.childNodes;
                        for (var l = 0; l < secChildren.length; l++) {
                            var secChild = secChildren[l];
                            if (secChild.nodeType == 1 && secChild.nodeName == "DIV" && secChild.className == "title") {
                                return secChild;
                            }
                        }
                    }
                }
                break;
            }
        }
        return null;
    }
    
    // ==================================================
    // ФУНКЦИЯ: Получение следующего узла в порядке обхода
    // ==================================================
    function getNextNode(el) {
        if (el.firstChild) {
            return el.firstChild;
        }
        while (el) {
            if (el.nextSibling) {
                return el.nextSibling;
            }
            el = el.parentNode;
        }
        return null;
    }
    
    // ==================================================
    // ФУНКЦИЯ: Поиск всех заголовков (кроме исключенных) в заданной области
    // ==================================================
    function findAllTitles(startElement, endElement) {
        var titles = [];
        var bodyTitle = null;
        var firstTitle = findFirstTitleInMainBody();
        
        // Находим заголовок body
        var bodyDivs = document.getElementsByTagName("DIV");
        for (var i = 0; i < bodyDivs.length; i++) {
            var div = bodyDivs[i];
            if (div.className == "body" && (div.getAttribute("fbname") || "") == "") {
                var children = div.childNodes;
                for (var j = 0; j < children.length; j++) {
                    var child = children[j];
                    if (child.nodeType == 1) {
                        if (child.nodeName == "DIV" && child.className == "title") {
                            bodyTitle = child;
                            break;
                        }
                        if (child.nodeName == "DIV" && child.className == "image") {
                            continue;
                        }
                        break;
                    }
                }
                break;
            }
        }
        
        // Обходим элементы от startElement до endElement
        var current = startElement;
        
        while (current && current != endElement) {
            if (current.nodeType == 1) {
                if (current.nodeName == "DIV" && current.className == "title") {
                    // Проверяем, в основном ли разделе
                    if (getSectionType(current) == "main") {
                        // Исключаем заголовок body
                        if (current === bodyTitle) {
                            // пропускаем
                        }
                        // Исключаем первый заголовок, если нет заголовка body
                        else if (!bodyTitle && current === firstTitle) {
                            // пропускаем
                        }
                        else {
                            // Проверяем уровень вложенности
                            if (titleLevel == 1) {
                                // Только вложенные заголовки (низшего уровня)
                                if (isNestedTitle(current)) {
                                    titles.push(current);
                                }
                            } else {
                                titles.push(current);
                            }
                        }
                    }
                }
            }
            current = getNextNode(current);
        }
        
        return titles;
    }
    
    // ==================================================
    // ФУНКЦИЯ: Подсчет абзацев в заголовке
    // ==================================================
    function countParagraphsInTitle(titleElement) {
        var count = 0;
        var children = titleElement.childNodes;
        for (var i = 0; i < children.length; i++) {
            if (children[i].nodeName == "P") {
                count++;
            }
        }
        return count;
    }
    
    // ==================================================
    // ФУНКЦИЯ: Нормализация пробелов в HTML
    // ==================================================
    function normalizeSpaces(html, side) {
        if (html.length == 0) return html;
        
        if (side == 'left') {
            var started = false;
            var result = "";
            var inTag = false;
            
            for (var i = 0; i < html.length; i++) {
                var ch = html.charAt(i);
                
                if (ch == '<') {
                    inTag = true;
                    result += ch;
                    continue;
                }
                if (ch == '>') {
                    inTag = false;
                    result += ch;
                    continue;
                }
                if (!inTag) {
                    if (!started) {
                        if (ch != ' ' && ch != nbspChar) {
                            started = true;
                            result += ch;
                        }
                    } else {
                        result += ch;
                    }
                } else {
                    result += ch;
                }
            }
            return result;
        } else {
            var lastNonSpacePos = -1;
            var inTag = false;
            
            for (var i = 0; i < html.length; i++) {
                var ch = html.charAt(i);
                
                if (ch == '<') {
                    inTag = true;
                    continue;
                }
                if (ch == '>') {
                    inTag = false;
                    continue;
                }
                if (!inTag && ch != ' ' && ch != nbspChar) {
                    lastNonSpacePos = i;
                }
            }
            
            if (lastNonSpacePos >= 0) {
                return html.substring(0, lastNonSpacePos + 1);
            }
            return "";
        }
    }
    
    // ==================================================
    // ФУНКЦИЯ: Поиск места для вставки после заголовка
    // ==================================================
    function findInsertionPoint(titleElement) {
        var parent = titleElement.parentNode;
        if (!parent) return { ref: null, hasImage: false, imageElement: null, hasEpigraph: false, epigraphElement: null };
        
        var next = titleElement.nextSibling;
        var hasImage = false;
        var imageElement = null;
        var hasEpigraph = false;
        var epigraphElement = null;
        
        // Пропускаем пустые текстовые узлы
        while (next && next.nodeType != 1) {
            next = next.nextSibling;
        }
        
        // Проверяем, есть ли эпиграф сразу после заголовка
        if (next && next.nodeName == "DIV" && next.className == "epigraph") {
            hasEpigraph = true;
            epigraphElement = next;
            // После эпиграфа ищем дальше
            next = next.nextSibling;
            while (next && next.nodeType != 1) {
                next = next.nextSibling;
            }
        }
        
        // Пропускаем пустые абзацы (в которых только nbspChar или пробелы)
        while (next && next.nodeName == "P") {
            var html = next.innerHTML;
            var reEmpty = new RegExp("^(\\s|" + nbspChar + "|&nbsp;)*$", "g");
            if (reEmpty.test(html)) {
                next = next.nextSibling;
                while (next && next.nodeType != 1) {
                    next = next.nextSibling;
                }
            } else {
                break;
            }
        }
        
        // Теперь next указывает на первый значимый элемент после эпиграфа (или после заголовка)
        // Проверяем, есть ли там иллюстрация
        if (next && next.nodeName == "DIV" && next.className == "image") {
            hasImage = true;
            imageElement = next;
        }
        
        // Определяем, куда вставлять
        // Базовая точка отсчёта:
        // - если есть эпиграф, то базовый ref = после эпиграфа
        // - если эпиграфа нет, то базовый ref = после заголовка
        var baseRef = null;
        if (hasEpigraph) {
            baseRef = epigraphElement.nextSibling;
        } else {
            baseRef = titleElement.nextSibling;
        }
        
        var ref = baseRef;
        
        // Если есть иллюстрация, корректируем ref согласно настройке
        if (hasImage) {
            if (insertBeforeImage == 0) {
                // Вставляем перед иллюстрацией
                ref = imageElement;
            } else {
                // Вставляем после иллюстрации
                ref = imageElement.nextSibling;
            }
        }
        
        return {
            ref: ref,
            hasImage: hasImage,
            imageElement: imageElement,
            hasEpigraph: hasEpigraph,
            epigraphElement: epigraphElement
        };
    }
    
    // ==================================================
    // ФУНКЦИЯ: Обработка одного заголовка
    // ==================================================
    function processTitle(titleElement) {
        var paragraphs = [];
        var children = titleElement.childNodes;
        
        // Собираем все абзацы
        for (var i = 0; i < children.length; i++) {
            if (children[i].nodeName == "P") {
                paragraphs.push(children[i]);
            }
        }
        
        if (paragraphs.length <= 1) return 0;
        
        // Оставляем только первый абзац, остальные вырезаем
        var firstParagraph = paragraphs[0];
        var extractedParagraphs = [];
        
        for (var i = 1; i < paragraphs.length; i++) {
            var p = paragraphs[i];
            var clonedP = p.cloneNode(true);
            extractedParagraphs.push(clonedP);
            p.parentNode.removeChild(p);
        }
        
        // Нормализуем пробелы в оставшемся первом абзаце
        if (firstParagraph) {
            var html = firstParagraph.innerHTML;
            html = normalizeSpaces(html, 'right');
            if (html.length == 0) {
                html = nbspChar;
            }
            firstParagraph.innerHTML = html;
        }
        
        // Находим точку вставки
        var parent = titleElement.parentNode;
        var insertInfo = findInsertionPoint(titleElement);
        
        // Создаем элементы для вставки
        var itemsToInsert = [];
        
        for (var i = 0; i < extractedParagraphs.length; i++) {
            var newP = document.createElement("P");
            if (createSubtitles == 1) {
                newP.className = "subtitle";
            }
            newP.innerHTML = extractedParagraphs[i].innerHTML;
            // Очищаем созданный элемент
            cleanElementHTML(newP);
            itemsToInsert.push(newP);
        }
        
        // Вставляем элементы в обратном порядке, чтобы сохранить последовательность
        var currentRef = insertInfo.ref;
        for (var i = itemsToInsert.length - 1; i >= 0; i--) {
            InsBefore(parent, currentRef, itemsToInsert[i]);
            currentRef = itemsToInsert[i];
        }
        
        // Добавляем пустые строки, если нужно (только для обычных абзацев рядом с иллюстрациями)
        if (createSubtitles == 0 && addEmptyLines == 1 && insertInfo.hasImage) {
            if (insertBeforeImage == 0) {
                // Вставляли перед иллюстрацией - добавляем пустую строку между абзацами и иллюстрацией
                var lastInserted = itemsToInsert[0]; // последний вставленный (ближайший к иллюстрации)
                if (lastInserted) {
                    var emptyP = document.createElement("P");
                    emptyP.innerHTML = nbspChar;
                    window.external.inflateBlock(emptyP) = true;
                    // Очищаем созданный элемент
                    cleanElementHTML(emptyP);
                    InsBefore(parent, insertInfo.imageElement, emptyP);
                }
            } else {
                // Вставляли после иллюстрации - добавляем пустую строку после иллюстрации перед абзацами
                var firstInserted = itemsToInsert[itemsToInsert.length - 1]; // первый вставленный
                if (firstInserted) {
                    var emptyP = document.createElement("P");
                    emptyP.innerHTML = nbspChar;
                    window.external.inflateBlock(emptyP) = true;
                    // Очищаем созданный элемент
                    cleanElementHTML(emptyP);
                    InsBefore(parent, firstInserted, emptyP);
                }
            }
        }
        
        return extractedParagraphs.length;
    }
    
    // ==================================================
    // ОСНОВНОЙ КОД
    // ==================================================
    
    var startTime = new Date();
    
    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) {
        showMessageWithHeader("Ошибка! fbw_body не найдено.", true);
        return;
    }
    
    // Генерируем случайные ID для маркеров
    var randomNum = "";
    for (var i = 0; i < 6; i++) {
        randomNum += Math.floor(Math.random() * 10);
    }
    var selectionBeginId = "titleScriptBeginId" + randomNum;
    var selectionEndId = "titleScriptEndId" + randomNum;
    
    var range;
    var searchDescription = "";
    var hasSelection = false;
    var originalRangeStart = null;
    var originalRangeEnd = null;
    
    // Определяем область поиска
    if (document.selection.type == "Text") {
        // Есть текстовое выделение
        range = document.selection.createRange();
        hasSelection = true;
        searchDescription = "В ВЫДЕЛЕННОМ ФРАГМЕНТЕ";
    } else {
        // Нет выделения - работаем от курсора до конца документа
        var sel = document.selection.createRange();
        if (sel) {
            // Сохраняем позицию курсора
            sel.pasteHTML("<B id=" + selectionBeginId + "></B>");
        } else {
            // Если нет курсора, вставляем в начало body
            var tmpRange = document.body.createTextRange();
            tmpRange.moveToElementText(fbwBody);
            tmpRange.collapse(true);
            tmpRange.pasteHTML("<B id=" + selectionBeginId + "></B>");
        }
        
        // Маркер конца - конец fbw_body
        var fbwBodyRange = document.body.createTextRange();
        fbwBodyRange.moveToElementText(fbwBody);
        fbwBodyRange.collapse(false);
        fbwBodyRange.pasteHTML("<B id=" + selectionEndId + "></B>");
        
        // Устанавливаем range от начала до конца
        range = document.body.createTextRange();
        range.moveToElementText(document.getElementById(selectionBeginId));
        var endRange = document.body.createTextRange();
        endRange.moveToElementText(document.getElementById(selectionEndId));
        range.setEndPoint("EndToEnd", endRange);
        
        hasSelection = false;
        searchDescription = "ОТ МЕСТОПОЛОЖЕНИЯ КУРСОРА ДО КОНЦА ДОКУМЕНТА";
    }
    
    // Сохраняем копии range для повторной вставки маркеров
    var savedRangeStart = range.duplicate();
    savedRangeStart.collapse(true);
    var savedRangeEnd = range.duplicate();
    savedRangeEnd.collapse(false);
    
    // Вставляем маркеры начала и конца (если ещё не вставлены для режима без выделения)
    if (hasSelection) {
        var range1 = range.duplicate();
        range1.collapse(true);
        range1.pasteHTML("<B id=" + selectionBeginId + "></B>");
        
        var range2 = range.duplicate();
        range2.collapse(false);
        range2.pasteHTML("<B id=" + selectionEndId + "></B>");
    }
    
    // Получаем элементы-маркеры
    var beginMarker = document.getElementById(selectionBeginId);
    var endMarker = document.getElementById(selectionEndId);
    
    if (!beginMarker || !endMarker) {
        showMessageWithHeader("Ошибка: не удалось создать маркеры области поиска.", true);
        return;
    }
    
    // Находим все заголовки между маркерами
    var allTitles = findAllTitles(beginMarker, endMarker);
    
    // Анализируем заголовки
    var titlesWith2 = 0;
    var titlesWith3Plus = 0;
    var totalParagraphsToExtract = 0;
    
    for (var i = 0; i < allTitles.length; i++) {
        var pCount = countParagraphsInTitle(allTitles[i]);
        if (pCount == 2) {
            titlesWith2++;
            totalParagraphsToExtract += 1;
        } else if (pCount >= 3) {
            titlesWith3Plus++;
            totalParagraphsToExtract += (pCount - 1);
        }
    }
    
    var totalTitlesToProcess = titlesWith2 + titlesWith3Plus;
    
    // Удаляем маркеры перед показом сообщений
    try {
        beginMarker.removeNode(true);
        endMarker.removeNode(true);
    } catch(e) {}
    
    // Если нет заголовков для обработки
    if (totalTitlesToProcess == 0) {
        var msg = "";
        if (hasSelection) {
            msg = "В выделенном фрагменте подходящих заголовков не найдено.";
        } else {
            msg = "До конца документа подходящих заголовков не найдено.";
        }
        showMessageWithHeader(msg, true);
        return;
    }
    
    // Формируем сообщение анализа
    var analysisMsg = "РЕЗУЛЬТАТ АНАЛИЗА:\n";
    
    if (hasSelection) {
        analysisMsg += "✓ В выделенном фрагменте найдено заголовков: " + allTitles.length + "\n";
    } else {
        analysisMsg += "✓ От курсора до конца документа найдено заголовков: " + allTitles.length + "\n";
    }
    
    analysisMsg += "✓ Заголовков с 2 абзацами: " + titlesWith2 + "\n";
    analysisMsg += "✓ Заголовков с 3 и более абзацами: " + titlesWith3Plus + "\n\n";
    
    analysisMsg += "БУДЕТ ОБРАБОТАНО СОГЛАСНО НАСТРОЕК:\n";
    analysisMsg += "✓ Заголовков с 2 абзацами: " + titlesWith2 + "\n";
    analysisMsg += "✓ Заголовков с 3 и более абзацами: " + titlesWith3Plus + "\n\n";
    
    analysisMsg += "НАСТРОЙКИ СКРИПТА:\n";
    analysisMsg += "• Режим отображения: " + (showStatistics == 1 ? "АНАЛИЗ И СТАТИСТИКА" : "ТИХИЙ РЕЖИМ") + "\n";
    analysisMsg += "• Область поиска: " + searchDescription + "\n";
    analysisMsg += "• Обработка заголовков: " + (titleLevel == 1 ? "ТОЛЬКО ВЛОЖЕННЫЕ" : "ЛЮБОГО УРОВНЯ") + "\n";
    analysisMsg += "• Поведение: " + (createSubtitles == 1 ? "СОЗДАВАТЬ ПОДЗАГОЛОВКИ" : "СОЗДАВАТЬ ОБЫЧНЫЕ АБЗАЦЫ") + "\n";
    analysisMsg += "• Обработка иллюстраций после заголовка: " + (insertBeforeImage == 0 ? "ВСТАВЛЯТЬ ТЕКСТ ПЕРЕД" : "ВСТАВЛЯТЬ ТЕКСТ ПОСЛЕ") + "\n";
    
    if (createSubtitles == 0) {
        analysisMsg += "• Добавлять пустые строки к абзацам при соседстве с иллюстрациями: " + (addEmptyLines == 1 ? "ДА" : "НЕТ") + "\n";
    }
    
    analysisMsg += "\nПреобразовать найденные многоабзацные заголовки?";
    
    // Показываем анализ и запрашиваем подтверждение
    if (showStatistics == 1) {
        if (!AskYesNo(scriptName + "\nver. " + version + "\n\n" + analysisMsg)) {
            return;
        }
    }
    
    // Заново вставляем маркеры в ТУ ЖЕ область (используем сохраненные range)
    savedRangeStart.pasteHTML("<B id=" + selectionBeginId + "></B>");
    savedRangeEnd.pasteHTML("<B id=" + selectionEndId + "></B>");
    
    beginMarker = document.getElementById(selectionBeginId);
    endMarker = document.getElementById(selectionEndId);
    
    // Заново находим заголовки
    allTitles = findAllTitles(beginMarker, endMarker);
    
    // Запускаем таймер после подтверждения
    startTime = new Date();
    
    // Начинаем отмену действий
    window.external.BeginUndoUnit(document, scriptName + " ver." + version);
    
    var processedCount = 0;
    var extractedCount = 0;
    
    try {
        // Обрабатываем заголовки
        for (var i = 0; i < allTitles.length; i++) {
            var extracted = processTitle(allTitles[i]);
            if (extracted > 0) {
                processedCount++;
                extractedCount += extracted;
            }
        }
        
    } catch(e) {
        window.external.EndUndoUnit(document);
        // Удаляем маркеры
        try {
            document.getElementById(selectionBeginId).removeNode(true);
            document.getElementById(selectionEndId).removeNode(true);
        } catch(e2) {}
        showMessageWithHeader("Ошибка при обработке: " + e.message, true);
        return;
    }
    
    window.external.EndUndoUnit(document);
    
    // Удаляем маркеры
    try {
        document.getElementById(selectionBeginId).removeNode(true);
        document.getElementById(selectionEndId).removeNode(true);
    } catch(e) {}
    
    var endTime = new Date();
    var timeDiff = (endTime - startTime) / 1000;
    var timeStr = timeDiff.toFixed(3).replace(".", ",");
    
    // Выводим статистику
    if (showStatistics == 1) {
        var resultMsg = "";
        
        if (hasSelection) {
            resultMsg += "✓ В выделенном фрагменте обработано заголовков: " + processedCount + "\n";
        } else {
            resultMsg += "✓ От курсора до конца документа обработано заголовков: " + processedCount + "\n";
        }
        resultMsg += "✓ Вынесено абзацев: " + extractedCount + "\n\n";
        resultMsg += "Время выполнения: " + timeStr + " сек.";
        
        showMessageWithHeader(resultMsg, false);
    }
    
    // Обновляем статус-бар
    try {
        if (processedCount > 0) {
            window.external.SetStatusBarText(scriptName + " ver. " + version + ": обработано " + processedCount + " заголовков, вынесено " + extractedCount + " абзацев. Время: " + timeStr + " сек.");
        } else {
            window.external.SetStatusBarText(scriptName + " ver. " + version + ": нет заголовков для обработки. Время: " + timeStr + " сек.");
        }
    }
    catch(e) {}
}
