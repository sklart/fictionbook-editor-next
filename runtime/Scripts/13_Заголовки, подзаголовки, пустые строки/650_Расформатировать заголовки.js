// Скрипт "Расформатировать заголовки" для редактора FBE
// version 2.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для расформатирования заголовков в документах fb2.
// Скрипт удаляет структурное оформление заголовков, преобразуя их в обычные параграфы.
// Внутриабзацные тэги форматирования заголовков (жирность, курсив) и сноски сохраняются.
// Вся основная структура секций, сами секции и их вложенность - сохраняются.
// Предусмотрены отдельные настройки для обработки основного раздела, разделов сносок и комментариев.

// ОСНОВНЫЕ ВОЗМОЖНОСТИ СКРИПТА:
// 1. Расформатирование заголовков разделов (body) с настройками:
// - Заголовок основного body раздела (опционально)
// - Заголовки раздела сносок/примечаний (опционально)
// - Заголовки раздела комментариев (опционально)

// 2. Автоматическое расформатирование эпиграфов (опционально):
// 3. Автоматическое расформатирование аннотаций внутри body (опционально):
// (Обрабатываются все аннотации, кроме основной)

// 4. Добавление маркеров для идентификации расформатированных элементов (опционально):
// - Маркеры для эпиграфов: zzz_epigr_1, zzz_epigr_2...
// - Маркеры для авторов эпиграфов: zzz_epigrauthor_1, zzz_epigrauthor_2...
// - Маркеры для аннотаций: zzz_anno_1, zzz_anno_2...
// - Нумерация в каждом элементе начинается с 1
// - Все маркеры можно настроить самостоятельно

// 5. Проверка безопасности:
// - Обнаруживает эпиграфы и аннотации в документе
// - Предупреждает о возможных проблемах с валидностью документа
// - Запрашивает подтверждение перед выполнением

// 6. Подробная статистика:
// - Показывает количество обработанных элементов
// - Отображает время выполнения
// - Указывает использованные настройки
// 
// 7. Поддержка отмены действий скрипта (Ctrl+Z)
// 
// ВАЖНО: После расформатирования заголовков в секциях, содержащих эпиграфы или аннотации,
// если внутри body оставить нерасформатированными эпиграфы или аннотации,
// понадобится ручное исправление структуры для сохранения валидности документа fb2.

// version 2.1, 17.01.2026
//======================================


function Run() {
    // Название и версия для сообщений
    var scriptName = "Расформатировать заголовки";
    var version = "2.1";
    
    // НАСТРОЙКИ СКРИПТА (можно менять)
    
    // Расформатировать заголовок основного body
    var processFirstTitle = 0;     // 0 - нет, 1 - да.
    
    // Расформатировать заголовки в body сносок (примечаний)
    var processNotesBody = 0;      // 0 - нет, 1 - да.
    
    // Расформатировать заголовки в body комментариев
    var processCommentsBody = 0;   // 0 - нет, 1 - да.
    
    // Автоматически расформатировать эпиграфы, включая авторов текста (text-author)
    var processEpigraphs = 1;      // 0 - нет, 1 - да
    
    // Автоматически расформатировать аннотации (кроме основной)
    var processAnnotations = 1;    // 0 - нет, 1 - да
    
    // Создавать маркеры расформатированных абзацев аннотаций, эпиграфов, авторов текста
    var processMarkers = 1;        // 0 - нет, 1 - да
    
    // МАРКЕРЫ для идентификации расформатированных элементов
    // Можно менять на любые другие обозначения
    
    // Маркеры расформатированных абзацев эпиграфов:
    var epigraphMarker = "zzz_epigr_";      // Будет дополняться номером: zzz_epigr_1, zzz_epigr_2...
    
    // Маркеры расформатированных абзацев авторов текста для эпиграфов:
    var epigraphAuthorMarker = "zzz_epigrauthor_"; // Будет дополняться номером: zzz_epigrauthor_1...
    
    // Маркеры расформатированных абзацев аннотаций (кроме первой):
    var annotationMarker = "zzz_anno_";     // Будет дополняться номером: zzz_anno_1, zzz_anno_2...
    
    // Получаем основной контейнер документа
    var fbw_body = document.getElementById("fbw_body");
    if (!fbw_body) {
        MsgBox("Ошибка: не найден основной контейнер документа (fbw_body).");
        return;
    }
    
    // ФУНКЦИЯ: Подсчет эпиграфов и аннотаций
    function countEpigraphsAndAnnotations(bodyElement, bodyType) {
        var epigraphs = 0;
        var annotations = 0;
        var epigraphsArray = []; // Массив для хранения самих элементов
        var annotationsArray = []; // Массив для хранения самих элементов
        
        // Ищем все DIV элементы в body
        var allDivs = bodyElement.getElementsByTagName("DIV");
        
        for (var i = 0; i < allDivs.length; i++) {
            var div = allDivs[i];
            var className = div.className || "";
            
            if (className == "epigraph") {
                epigraphs++;
                epigraphsArray.push({
                    element: div,
                    bodyType: bodyType
                });
            }
            else if (className == "annotation") {
                // Проверяем, не является ли это основной аннотацией (первой в документе)
                var isMainAnnotation = false;
                if (bodyType == "main") {
                    // Проверяем, находится ли аннотация прямо в body (не в section)
                    var parent = div.parentNode;
                    if (parent && parent.className == "body") {
                        isMainAnnotation = true;
                    }
                }
                
                if (!isMainAnnotation) {
                    annotations++;
                    annotationsArray.push({
                        element: div,
                        bodyType: bodyType
                    });
                }
            }
        }
        
        return {
            epigraphs: epigraphs,
            annotations: annotations,
            epigraphsArray: epigraphsArray,
            annotationsArray: annotationsArray,
            bodyType: bodyType
        };
    }
    
    // ФУНКЦИЯ: Расформатирование эпиграфа с маркерами (нумерация с 1 в каждом эпиграфе)
    function unformatEpigraph(epigraphElement) {
        var parent = epigraphElement.parentNode;
        var nextSibling = epigraphElement.nextSibling;
        
        // Создаем DocumentFragment для сохранения порядка элементов
        var fragment = document.createDocumentFragment();
        var paragraphs = epigraphElement.getElementsByTagName("P");
        
        // Счетчики для этого конкретного эпиграфа
        var epigrCounter = 0;
        var authorCounter = 0;
        
        // Копируем все параграфы в правильном порядке
        for (var i = 0; i < paragraphs.length; i++) {
            var paragraph = paragraphs[i];
            var clonedParagraph = paragraph.cloneNode(true);
            var isTextAuthor = (clonedParagraph.className == "text-author");
            
            // Определяем маркер (если включено создание маркеров)
            var marker = "";
            if (processMarkers == 1) {
                if (isTextAuthor) {
                    authorCounter++;
                    marker = epigraphAuthorMarker + authorCounter + " ";
                } else {
                    epigrCounter++;
                    marker = epigraphMarker + epigrCounter + " ";
                }
                
                // Добавляем маркер в начало параграфа
                var markerText = document.createTextNode(marker);
                
                // Если параграф пустой, создаем текстовый узел
                if (!clonedParagraph.firstChild) {
                    clonedParagraph.appendChild(markerText);
                } else {
                    // Вставляем маркер перед первым дочерним элементом
                    clonedParagraph.insertBefore(markerText, clonedParagraph.firstChild);
                }
            }
            
            // Если это text-author, снимаем класс
            if (isTextAuthor) {
                clonedParagraph.removeAttribute("className");
                clonedParagraph.removeAttribute("class");
            }
            
            // Активируем параграф для редактирования
            window.external.inflateBlock(clonedParagraph) = true;
            
            fragment.appendChild(clonedParagraph);
        }
        
        // Вставляем фрагмент перед эпиграфом
        if (nextSibling) {
            parent.insertBefore(fragment, nextSibling);
        } else {
            parent.appendChild(fragment);
        }
        
        // Удаляем эпиграф
        epigraphElement.removeNode(true);
        
        return {
            epigrParagraphs: epigrCounter,
            authorParagraphs: authorCounter
        };
    }
    
    // ФУНКЦИЯ: Расформатирование аннотации с маркерами (нумерация с 1 в каждой аннотации)
    function unformatAnnotation(annotationElement) {
        var parent = annotationElement.parentNode;
        var nextSibling = annotationElement.nextSibling;
        
        // Создаем DocumentFragment для сохранения порядка элементов
        var fragment = document.createDocumentFragment();
        var paragraphs = annotationElement.getElementsByTagName("P");
        
        // Счетчик для этой конкретной аннотации
        var annoCounter = 0;
        
        // Копируем все параграфы в правильном порядке
        for (var i = 0; i < paragraphs.length; i++) {
            var paragraph = paragraphs[i];
            var clonedParagraph = paragraph.cloneNode(true);
            
            // Добавляем маркер (если включено создание маркеров)
            if (processMarkers == 1) {
                annoCounter++;
                var marker = annotationMarker + annoCounter + " ";
                var markerText = document.createTextNode(marker);
                
                // Если параграф пустой, создаем текстовый узел
                if (!clonedParagraph.firstChild) {
                    clonedParagraph.appendChild(markerText);
                } else {
                    // Вставляем маркер перед первым дочерним элементом
                    clonedParagraph.insertBefore(markerText, clonedParagraph.firstChild);
                }
            }
            
            // Активируем параграф для редактирования
            window.external.inflateBlock(clonedParagraph) = true;
            
            fragment.appendChild(clonedParagraph);
        }
        
        // Вставляем фрагмент перед аннотацией
        if (nextSibling) {
            parent.insertBefore(fragment, nextSibling);
        } else {
            parent.appendChild(fragment);
        }
        
        // Удаляет аннотацию
        annotationElement.removeNode(true);
        
        return annoCounter;
    }
    
    // 1. Находим все body элементы и анализируем структуру
    var allBodies = [];
    var bodyStats = [];
    var totalEpigraphs = 0;
    var totalAnnotations = 0;
    var hasProblematicElements = false;
    
    var ptr = fbw_body.firstChild;
    var bodyIndex = 0;
    
    while (ptr) {
        if (ptr.nodeName == "DIV" && ptr.className == "body") {
            var fbname = ptr.getAttribute("fbname") || "";
            var isFirstBody = (bodyIndex == 0);
            var bodyType = "";
            
            // Определяем тип body
            if (isFirstBody) {
                bodyType = "main";
            } else if (fbname == "notes") {
                bodyType = "notes";
            } else if (fbname == "comments") {
                bodyType = "comments";
            } else {
                bodyType = "other";
            }
            
            // Проверяем, нужно ли анализировать этот body
            var shouldAnalyze = true;
            if (bodyType == "notes" && processNotesBody == 0) {
                shouldAnalyze = false;
            }
            if (bodyType == "comments" && processCommentsBody == 0) {
                shouldAnalyze = false;
            }
            
            if (shouldAnalyze) {
                allBodies.push({
                    element: ptr,
                    type: bodyType,
                    isFirstBody: isFirstBody,
                    fbname: fbname
                });
                
                // Анализируем структуру body
                var stats = countEpigraphsAndAnnotations(ptr, bodyType);
                bodyStats.push(stats);
                
                totalEpigraphs += stats.epigraphs;
                totalAnnotations += stats.annotations;
                
                if (stats.epigraphs > 0 || stats.annotations > 0) {
                    hasProblematicElements = true;
                }
            }
            bodyIndex++;
        }
        ptr = ptr.nextSibling;
    }
    
    // 2. Формируем предупреждающее сообщение
    var warningMessage = "";
    var willProcessEpigraphs = false;
    var willProcessAnnotations = false;
    
    if (hasProblematicElements) {
        warningMessage = "Внимание!\nВ документе обнаружены:\n\n";
        
        if (totalAnnotations > 0) {
            warningMessage += "Аннотации (кроме основной): " + totalAnnotations + "\n";
            warningMessage += "Из них:\n";
            
            var mainAnn = 0, notesAnn = 0, commentsAnn = 0, otherAnn = 0;
            for (var i = 0; i < bodyStats.length; i++) {
                var stats = bodyStats[i];
                if (stats.bodyType == "main") mainAnn = stats.annotations;
                else if (stats.bodyType == "notes") notesAnn = stats.annotations;
                else if (stats.bodyType == "comments") commentsAnn = stats.annotations;
                else otherAnn += stats.annotations;
            }
            
            if (mainAnn > 0) warningMessage += "• В основном разделе - " + mainAnn + "\n";
            if (notesAnn > 0) warningMessage += "• В разделе примечаний - " + notesAnn + "\n";
            if (commentsAnn > 0) warningMessage += "• В разделе комментариев - " + commentsAnn + "\n";
            if (otherAnn > 0) warningMessage += "• В других разделах - " + otherAnn + "\n";
            
            warningMessage += "\n";
            
            // Проверяем настройку для аннотаций
            if (processAnnotations == 1) {
                willProcessAnnotations = true;
            }
        }
        
        if (totalEpigraphs > 0) {
            warningMessage += "Эпиграфы: " + totalEpigraphs + "\n";
            warningMessage += "Из них:\n";
            
            var mainEpi = 0, notesEpi = 0, commentsEpi = 0, otherEpi = 0;
            for (var i = 0; i < bodyStats.length; i++) {
                var stats = bodyStats[i];
                if (stats.bodyType == "main") mainEpi = stats.epigraphs;
                else if (stats.bodyType == "notes") notesEpi = stats.epigraphs;
                else if (stats.bodyType == "comments") commentsEpi = stats.epigraphs;
                else otherEpi += stats.epigraphs;
            }
            
            if (mainEpi > 0) warningMessage += "• В основном разделе - " + mainEpi + "\n";
            if (notesEpi > 0) warningMessage += "• В разделе примечаний - " + notesEpi + "\n";
            if (commentsEpi > 0) warningMessage += "• В разделе комментариев - " + commentsEpi + "\n";
            if (otherEpi > 0) warningMessage += "• В других разделах - " + otherEpi + "\n";
            
            warningMessage += "\n";
            
            // Проверяем настройку для эпиграфов
            if (processEpigraphs == 1) {
                willProcessEpigraphs = true;
            }
        }
        
        warningMessage += "==============================\n";
        warningMessage += "В результате расформатирования заголовков в отдельных секциях, содержащих эти элементы, документ может стать невалидным!\n";
        warningMessage += "Потребуется ручное исправление структуры в данных секциях!\n";
        warningMessage += "==============================\n\n";
        
        // Добавляем информацию о том, что будет расформатировано согласно настройкам
        var autoProcessMessage = "";
        if (willProcessEpigraphs || willProcessAnnotations) {
            autoProcessMessage = "Согласно настройкам, кроме заголовков, будут расформатированы найденные:\n";
            
            if (willProcessEpigraphs && totalEpigraphs > 0) {
                autoProcessMessage += "Эпиграфы - " + totalEpigraphs + "\n";
            }
            if (willProcessAnnotations && totalAnnotations > 0) {
                autoProcessMessage += "Аннотации (кроме основной) - " + totalAnnotations + "\n";
            }
            
            autoProcessMessage += "\n";
            autoProcessMessage += "==============================\n\n";
            
            // Добавляем информацию о маркерах (если включено)
            if (processMarkers == 1) {
                autoProcessMessage += "Будут добавлены маркеры (нумерация в каждом элементе начинается с 1):\n";
                if (willProcessEpigraphs && totalEpigraphs > 0) {
                    autoProcessMessage += "• Для эпиграфов: " + epigraphMarker + "1, " + epigraphMarker + "2...\n";
                    autoProcessMessage += "• Для авторов эпиграфов: " + epigraphAuthorMarker + "1, " + epigraphAuthorMarker + "2...\n";
                }
                if (willProcessAnnotations && totalAnnotations > 0) {
                    autoProcessMessage += "• Для аннотаций: " + annotationMarker + "1, " + annotationMarker + "2...\n";
                }
                autoProcessMessage += "\n";
                autoProcessMessage += "==============================\n\n";
            }
        }
        
        if (autoProcessMessage) {
            warningMessage += autoProcessMessage;
        }
        
        warningMessage += "ОК - продолжить, Отмена - прервать скрипт.";
        
        // Используем стандартный confirm для IE6
        if (!confirm(warningMessage)) {
            return;
        }
    }
    
    // Таймер включаем только после последнего confirm
    var startTime = new Date().getTime();
    
    // Счетчики для статистики
    var firstTitleCount = 0;
    var mainBodyCount = 0;
    var notesCount = 0;
    var commentsCount = 0;
    var epigraphsProcessed = 0;
    var annotationsProcessed = 0;
    var totalEpigrMarkers = 0;
    var totalAuthorMarkers = 0;
    var totalAnnoMarkers = 0;
    
    // Начинаем блок отмены действий
    window.external.BeginUndoUnit(document, scriptName + " (v" + version + ")");
    
    // 3. Обрабатываем эпиграфы и аннотации (если нужно)
    if (willProcessEpigraphs || willProcessAnnotations) {
        // Для сохранения порядка обрабатываем в прямом порядке
        // Но сначала собираем все элементы, чтобы не мешать обходу DOM
        var allEpigraphsToProcess = [];
        var allAnnotationsToProcess = [];
        
        for (var statsIndex = 0; statsIndex < bodyStats.length; statsIndex++) {
            var stats = bodyStats[statsIndex];
            
            if (willProcessEpigraphs) {
                for (var epiIndex = 0; epiIndex < stats.epigraphsArray.length; epiIndex++) {
                    allEpigraphsToProcess.push(stats.epigraphsArray[epiIndex].element);
                }
            }
            
            if (willProcessAnnotations) {
                for (var annIndex = 0; annIndex < stats.annotationsArray.length; annIndex++) {
                    allAnnotationsToProcess.push(stats.annotationsArray[annIndex].element);
                }
            }
        }
        
        // Обрабатываем эпиграфы (в обратном порядке для стабильности DOM)
        for (var i = allEpigraphsToProcess.length - 1; i >= 0; i--) {
            var result = unformatEpigraph(allEpigraphsToProcess[i]);
            epigraphsProcessed++;
            if (processMarkers == 1) {
                totalEpigrMarkers += result.epigrParagraphs;
                totalAuthorMarkers += result.authorParagraphs;
            }
        }
        
        // Обрабатываем аннотации (в обратном порядке для стабильности DOM)
        for (var i = allAnnotationsToProcess.length - 1; i >= 0; i--) {
            var result = unformatAnnotation(allAnnotationsToProcess[i]);
            annotationsProcessed++;
            if (processMarkers == 1) {
                totalAnnoMarkers += result;
            }
        }
    }
    
    // 4. Обрабатываем каждый body (заголовки)
    for (var i = 0; i < allBodies.length; i++) {
        var currentBody = allBodies[i];
        var bodyElement = currentBody.element;
        var bodyType = currentBody.type;
        var isFirstBody = currentBody.isFirstBody;
        
        // 4.1. Обрабатываем заголовок самого body (если есть)
        var bodyTitle = null;
        var bodyChildren = bodyElement.childNodes;
        
        // Ищем первый DIV с class="title" в body
        for (var bcIndex = 0; bcIndex < bodyChildren.length; bcIndex++) {
            var child = bodyChildren[bcIndex];
            if (child.nodeType == 1 && // ELEMENT_NODE
                child.nodeName == "DIV" && 
                child.className == "title") {
                bodyTitle = child;
                break;
            }
        }
        
        // Обрабатываем заголовок body, если он есть
        if (bodyTitle) {
            // Проверяем специальные настройки
            var shouldProcessThisTitle = true;
            
            if (bodyType == "main" && processFirstTitle == 0) {
                shouldProcessThisTitle = false;
            }
            
            if (shouldProcessThisTitle) {
                // Создаем новую section для содержимого заголовка
                var newSection = document.createElement("DIV");
                newSection.className = "section";
                
                // Создаем параграф для содержимого заголовка
                var newParagraph = document.createElement("P");
                
                // Копируем все содержимое из заголовка body
                var titleChildren = bodyTitle.childNodes;
                for (var tcIndex = 0; tcIndex < titleChildren.length; tcIndex++) {
                    var clonedChild = titleChildren[tcIndex].cloneNode(true);
                    newParagraph.appendChild(clonedChild);
                }
                
                // Если параграф пуст, добавляем неразрывный пробел
                if (!newParagraph.firstChild || newParagraph.innerHTML == "") {
                    var nbspText = document.createTextNode(String.fromCharCode(160));
                    newParagraph.appendChild(nbspText);
                }
                
                // Активируем параграф для редактирования
                window.external.inflateBlock(newParagraph) = true;
                
                // Добавляем параграф в новую section
                newSection.appendChild(newParagraph);
                
                // Вставляем новую section перед заголовком body
                bodyTitle.parentNode.insertBefore(newSection, bodyTitle);
                
                // Удаляем старый заголовок body
                bodyTitle.removeNode(true);
                
                // Увеличиваем счетчик
                if (bodyType == "main") {
                    firstTitleCount++;
                } else if (bodyType == "notes") {
                    notesCount++;
                } else if (bodyType == "comments") {
                    commentsCount++;
                }
            }
        }
        
        // 4.2. Обрабатываем заголовки внутри sections этого body
        var sections = bodyElement.getElementsByTagName("DIV");
        var sectionsArray = [];
        
        // Собираем все section в массив
        for (var j = 0; j < sections.length; j++) {
            if (sections[j].className == "section") {
                sectionsArray.push(sections[j]);
            }
        }
        
        // Обрабатываем section в обратном порядке (для стабильности DOM)
        for (var sectionIndex = sectionsArray.length - 1; sectionIndex >= 0; sectionIndex--) {
            var currentSection = sectionsArray[sectionIndex];
            
            // Ищем заголовок в этой section
            var titleDiv = null;
            var children = currentSection.childNodes;
            
            for (var childIndex = 0; childIndex < children.length; childIndex++) {
                var child = children[childIndex];
                if (child.nodeType == 1 && // ELEMENT_NODE
                    child.nodeName == "DIV" && 
                    child.className == "title") {
                    titleDiv = child;
                    break;
                }
            }
            
            // Если нашли заголовок, обрабатываем его
            if (titleDiv) {
                // Проверяем, не является ли это заголовком body, который мы уже обработали
                if (titleDiv === bodyTitle) {
                    continue;
                }
                
                // Создаем новый параграф для содержимого заголовка
                var newParagraph = document.createElement("P");
                
                // Копируем все содержимое заголовка
                var titleChildren = titleDiv.childNodes;
                for (var tcIndex = 0; tcIndex < titleChildren.length; tcIndex++) {
                    var clonedChild = titleChildren[tcIndex].cloneNode(true);
                    newParagraph.appendChild(clonedChild);
                }
                
                // Если параграф пуст, добавляем неразрывный пробел
                if (!newParagraph.firstChild || newParagraph.innerHTML == "") {
                    var nbspText = document.createTextNode(String.fromCharCode(160));
                    newParagraph.appendChild(nbspText);
                }
                
                // Активируем параграф для редактирования
                window.external.inflateBlock(newParagraph) = true;
                
                // Вставляем новый параграф перед titleDiv
                titleDiv.parentNode.insertBefore(newParagraph, titleDiv);
                
                // Удаляет старый заголовок
                titleDiv.removeNode(true);
                
                // Увеличиваем счетчик
                if (bodyType == "main") {
                    mainBodyCount++;
                } else if (bodyType == "notes") {
                    notesCount++;
                } else if (bodyType == "comments") {
                    commentsCount++;
                } else {
                    mainBodyCount++; // Для других body считаем как основной раздел
                }
            }
        }
    }
    
    // Завершаем блок отмены действий
    window.external.EndUndoUnit(document);
    
    // Вычисляем время выполнения (только после последнего confirm)
    var endTime = new Date().getTime();
    var executionTime = (endTime - startTime) / 1000;
    var timeStr = executionTime.toFixed(2).replace('.', ',') + " сек";
    
    // Выводим результат
    var totalProcessed = firstTitleCount + mainBodyCount + notesCount + commentsCount;
    var message = "";
    
    message += scriptName + "\nver. " + version + "\n\n";
    
    if (totalProcessed > 0) {
        message += "Расформатировано заголовков: " + totalProcessed + "\n";
        message += "Из них:\n";
        
        if (firstTitleCount > 0) {
            message += "• Заголовков основного раздела - " + firstTitleCount + "\n";
        }
        if (mainBodyCount > 0) {
            message += "• В основном разделе - " + mainBodyCount + "\n";
        }
        if (notesCount > 0) {
            message += "• В разделе сносок - " + notesCount + "\n";
        }
        if (commentsCount > 0) {
            message += "• В разделе комментариев - " + commentsCount + "\n";
        }
        
        message += "\n";
        
        // Добавляем информацию о расформатированных эпиграфах и аннотациях
        if (epigraphsProcessed > 0 || annotationsProcessed > 0) {
            message += "==============================\n";
            message += "Дополнительно расформатировано:\n";
            if (epigraphsProcessed > 0) {
                message += "• Эпиграфов: " + epigraphsProcessed + "\n";
            }
            if (annotationsProcessed > 0) {
                message += "• Аннотаций (кроме основной): " + annotationsProcessed + "\n";
            }
            message += "\n";
            
            // Добавляем информацию о маркерах (если они создавались)
            if (processMarkers == 1 && (totalEpigrMarkers > 0 || totalAuthorMarkers > 0 || totalAnnoMarkers > 0)) {
                message += "==============================\n";
                message += "Добавлены маркеры (в каждом элементе нумерация с 1):\n";
                if (totalEpigrMarkers > 0) {
                    message += "• Для эпиграфов: " + epigraphMarker + "1... (" + totalEpigrMarkers + " шт)\n";
                }
                if (totalAuthorMarkers > 0) {
                    message += "• Для авторов эпиграфов: " + epigraphAuthorMarker + "1... (" + totalAuthorMarkers + " шт)\n";
                }
                if (totalAnnoMarkers > 0) {
                    message += "• Для аннотаций: " + annotationMarker + "1... (" + totalAnnoMarkers + " шт)\n";
                }
                message += "\n";
            }
        }
        
        // Добавляем информацию о проблемных элементах, если они не были расформатированы
        if (hasProblematicElements && (epigraphsProcessed == 0 && annotationsProcessed == 0)) {
            message += "==============================\n";
            message += "Обнаружены проблемные элементы:\n";
            
            if (totalAnnotations > 0) {
                message += "• Аннотации: " + totalAnnotations + "\n";
            }
            if (totalEpigraphs > 0) {
                message += "• Эпиграфы: " + totalEpigraphs + "\n";
            }
            
            message += "\n";
        }
        
        message += "==============================\n";
        message += "Настройки расформатирования:\n";
        message += "• Заголовок основного раздела: " + (processFirstTitle ? "ДА" : "НЕТ") + "\n";
        message += "• Заголовки раздела сносок: " + (processNotesBody ? "ДА" : "НЕТ") + "\n";
        message += "• Заголовки раздела комментариев: " + (processCommentsBody ? "ДА" : "НЕТ") + "\n";
        message += "• Автоматически расформатировать эпиграфы: " + (processEpigraphs ? "ДА" : "НЕТ") + "\n";
        message += "• Автоматически расформатировать аннотации: " + (processAnnotations ? "ДА" : "НЕТ") + "\n";
        message += "• Автоматически добавлять маркеры: " + (processMarkers ? "ДА" : "НЕТ") + "\n\n";
        
        // Добавляем информацию о маркерах (если они создавались)
        if (processMarkers == 1) {
            message += "==============================\n";
            message += "Расставлены маркеры (нумерация в каждом элементе с 1):\n";
            message += "• Эпиграфы: " + epigraphMarker + "1, " + epigraphMarker + "2...\n";
            message += "• Авторы эпиграфов: " + epigraphAuthorMarker + "1, " + epigraphAuthorMarker + "2...\n";
            message += "• Аннотации: " + annotationMarker + "1, " + annotationMarker + "2...\n\n";
        }
        
        message += "==============================\n";
        message += "Время выполнения: " + timeStr;
    } else {
        // Если заголовков не найдено - только одна строка
        message += "Заголовков в документе не обнаружено!";
    }
    
    MsgBox(message);
}

function MsgBox(text) {
    window.external.MsgBox(text);
}
