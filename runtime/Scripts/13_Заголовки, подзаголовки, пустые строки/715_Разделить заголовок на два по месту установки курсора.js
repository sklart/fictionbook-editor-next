// Скрипт "Разделить заголовок на два по месту установки курсора" для редактора FBE
// version 3.8
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для разделения одного заголовка на два (по позиции курсора) в fb2 документах.
// Для валидности документа скрипт создает абзац с пустой строкой сразу после исходного заголовка.
// Разделяются только обычные заголовки секций. Заголовок боди не разделяется.
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 3.8, 10.03.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Разделить заголовок на два по месту установки курсора";
    var version = "3.8";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1;
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    var startTime = new Date();
    
    // Функция для показа сообщений
    function showMessage(msg, isError) {
        if (!showStatistics && !isError) return;
        MsgBox(scriptName + "\nver. " + version + "\n\n" + msg, "FBE скрипт");
    }
    
    // Получаем неразрывный пробел
    var nbspChar = String.fromCharCode(160);
    try {
        nbspChar = window.external.GetNBSP();
    } catch(e) {}
    
    // Проверяем наличие курсора
    var sel = document.selection.createRange();
    if (!sel) {
        showMessage("Установите курсор в желаемое место разделения заголовка!", true);
        return;
    }
    
    // Проверка на выделение текста
    if (sel.compareEndPoints("StartToEnd", sel) != 0) {
        showMessage("Установите курсор в желаемое место разделения заголовка!", true);
        return;
    }
    
    // Находим элемент, в котором стоит курсор
    var cursorElement = sel.parentElement();
    if (!cursorElement) {
        showMessage("Установите курсор в желаемое место разделения заголовка!", true);
        return;
    }
    
    // Поднимаемся до заголовка
    var titleElement = cursorElement;
    while (titleElement && !(titleElement.nodeName == "DIV" && titleElement.className == "title")) {
        titleElement = titleElement.parentNode;
    }
    
    // Если курсор не в заголовке - сообщение
    if (!titleElement) {
        showMessage("Установите курсор в желаемое место разделения заголовка!", true);
        return;
    }
    
    // Проверяем, не заголовок ли это body
    if (titleElement.parentNode && 
        titleElement.parentNode.nodeName == "DIV" && 
        titleElement.parentNode.className == "body") {
        showMessage("Нельзя разделять заголовок body!", true);
        return;
    }
    
    // Находим секцию
    var sectionElement = titleElement.parentNode;
    while (sectionElement && !(sectionElement.nodeName == "DIV" && sectionElement.className == "section")) {
        sectionElement = sectionElement.parentNode;
    }
    
    if (!sectionElement) {
        showMessage("Установите курсор в желаемое место разделения заголовка!", true);
        return;
    }
    
    // Собираем все абзацы заголовка
    var paragraphs = [];
    for (var i = 0; i < titleElement.childNodes.length; i++) {
        if (titleElement.childNodes[i].nodeName == "P") {
            paragraphs.push(titleElement.childNodes[i]);
        }
    }
    
    if (paragraphs.length == 0) {
        showMessage("Установите курсор в желаемое место разделения заголовка!", true);
        return;
    }
    
    // Находим абзац с курсором
    var currentParagraph = null;
    var paragraphIndex = -1;
    
    for (var i = 0; i < paragraphs.length; i++) {
        if (sel.parentElement() == paragraphs[i] || paragraphs[i].contains(sel.parentElement())) {
            currentParagraph = paragraphs[i];
            paragraphIndex = i;
            break;
        }
    }
    
    // Если курсор не в абзаце (между абзацами или в конце)
    if (!currentParagraph) {
        showMessage("Установите курсор в желаемое место разделения заголовка!", true);
        return;
    }
    
    // Получаем текст до курсора
    var range = document.body.createTextRange();
    range.moveToElementText(currentParagraph);
    range.setEndPoint("EndToStart", sel);
    var textBeforeCursor = range.text;
    
    // Получаем весь текст абзаца
    var fullText = getFullText(currentParagraph);
    
    // Проверка на короткий заголовок
    if (fullText.length <= 1) {
        showMessage("Нечего разделять!\n\n(Заголовок слишком короткий)", true);
        return;
    }
    
    // Проверка на курсор в начале
    if (paragraphIndex == 0 && textBeforeCursor.length == 0) {
        showMessage("Установите курсор в желаемое место разделения заголовка!", true);
        return;
    }
    
    // Проверка на курсор в конце
    var endRange = sel.duplicate();
    endRange.collapse(true);
    range.moveToElementText(currentParagraph);
    range.setEndPoint("EndToEnd", endRange);
    
    if (range.text.length == 0) {
        showMessage("Установите курсор в желаемое место разделения заголовка!", true);
        return;
    }
    
    // Получаем HTML абзаца
    var fullHtml = currentParagraph.innerHTML;
    
    // Находим позицию курсора в HTML
    var htmlPos = findHtmlPosition(fullHtml, textBeforeCursor);
    
    if (htmlPos == -1) {
        showMessage("Установите курсор в желаемое место разделения заголовка!", true);
        return;
    }
    
    // Режем HTML
    var leftHtml = fullHtml.substring(0, htmlPos);
    var rightHtml = fullHtml.substring(htmlPos);
    
    // Нормализация пробелов
    leftHtml = normalizeSpaces(leftHtml, 'right');
    rightHtml = normalizeSpaces(rightHtml, 'left');
    
    // Начинаем отмену действий
    window.external.BeginUndoUnit(document, scriptName + " ver." + version);
    
    try {
        // Собираем содержимое секции после заголовка
        var sectionContent = [];
        var afterTitle = false;
        for (var i = 0; i < sectionElement.childNodes.length; i++) {
            var child = sectionElement.childNodes[i];
            if (afterTitle && child.nodeType == 1) {
                sectionContent.push(child);
            }
            if (child == titleElement) {
                afterTitle = true;
            }
        }
        
        // Обновляем текущий абзац
        currentParagraph.innerHTML = leftHtml;
        
        // Создаём новую секцию
        var newSection = document.createElement("DIV");
        newSection.className = "section";
        
        // Создаём новый заголовок
        var newTitle = document.createElement("DIV");
        newTitle.className = "title";
        
        // Добавляем правую часть
        if (rightHtml.length > 0) {
            var newParagraph = document.createElement("P");
            newParagraph.innerHTML = rightHtml;
            newTitle.appendChild(newParagraph);
        }
        
        // Переносим остальные абзацы
        for (var i = paragraphIndex + 1; i < paragraphs.length; i++) {
            var p = paragraphs[i].cloneNode(true);
            newTitle.appendChild(p);
            paragraphs[i].parentNode.removeChild(paragraphs[i]);
        }
        
        newSection.appendChild(newTitle);
        
        // Переносим содержимое секции
        for (var i = 0; i < sectionContent.length; i++) {
            var content = sectionContent[i];
            var clonedContent = content.cloneNode(true);
            newSection.appendChild(clonedContent);
            content.parentNode.removeChild(content);
        }
        
        // Добавляем пустой абзац
        var emptyParagraph = document.createElement("P");
        emptyParagraph.innerHTML = nbspChar;
        window.external.inflateBlock(emptyParagraph) = true;
        sectionElement.appendChild(emptyParagraph);
        
        // Вставляем новую секцию
        sectionElement.parentNode.insertBefore(newSection, sectionElement.nextSibling);
        
        // Перемещаем курсор
        if (newTitle.firstChild) {
            var r = document.body.createTextRange();
            r.moveToElementText(newTitle.firstChild);
            r.collapse(true);
            r.select();
        }
        
        // Статистика
        if (showStatistics) {
            var endTime = new Date();
            var timeDiff = (endTime - startTime) / 1000;
            var timeStr = timeDiff.toFixed(3).replace(".", ",");
            
            showMessage("✓ Заголовок успешно разделён!\n\nВремя выполнения: " + timeStr + " сек", false);
        }
        
    } catch(e) {
        showMessage("Ошибка: " + e.message, true);
    }
    
    window.external.EndUndoUnit(document);
    
    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
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
    
    function getFullText(element) {
        var text = "";
        for (var i = 0; i < element.childNodes.length; i++) {
            var node = element.childNodes[i];
            if (node.nodeType == 3) {
                text += node.nodeValue;
            } else if (node.nodeType == 1) {
                text += getFullText(node);
            }
        }
        return text;
    }
    
    function findHtmlPosition(html, targetText) {
        var textFound = "";
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
            if (!inTag) {
                textFound += ch;
                if (textFound == targetText) {
                    return i + 1;
                }
            }
        }
        return -1;
    }
}

// Для совместимости
function GetTitle() { return "Разделить заголовок на два по месту установки курсора"; }
function GetClassName() { return "Разделить заголовок"; }
function ProcessCmd() { Run(); }
