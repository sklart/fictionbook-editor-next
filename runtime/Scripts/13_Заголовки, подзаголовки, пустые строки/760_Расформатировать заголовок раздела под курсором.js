// Скрипт "Расформатировать заголовок раздела под курсором" для редактора FBE
// version 1.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для расформатирования любого текущего заголовка
// секции (раздела) или заголовка body в fb2 документах.
// Скрипт удаляет DIV class="title", оставляя сам текст заголовка
// на месте внутри секции (раздела).
// Работает при установке курсора внутрь заголовка или при его выделении.
// Сохраняется валидная структура секций при расформатировании.
// Эпиграф или аннотация раздела расформатируются вместе с заголовком. 
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// Основано на скрипте: "Снять форматирование блочным элементом.js" v2.7
// уважаемого тов. Sclex

// version 1.1, 08.04.2026
//======================================

function Run() {
    var scriptName = "Расформатировать заголовок раздела под курсором";
    var version = "1.1";
    
    // Настройка: 1 - показывать сообщения, 0 - тихий режим (только ошибки)
    var showStatistics = 0;
    
    var fbw_body = document.getElementById("fbw_body");
    if (!fbw_body) {
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка: не найден fbw_body", "FBE скрипт");
        return;
    }
    
    if (document.selection.type.toLowerCase() != "none" && document.selection.type.toLowerCase() != "text") {
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка: неподдерживаемый тип выделения", "FBE скрипт");
        return;
    }
    
    var randomNum = Math.floor((Math.random()*9)).toString() + 
                    Math.floor((Math.random()*9)).toString() +
                    Math.floor((Math.random()*9)).toString() + 
                    Math.floor((Math.random()*9)).toString() +
                    Math.floor((Math.random()*9)).toString() + 
                    Math.floor((Math.random()*9)).toString();
    var selectionBeginId = "removeTitleBeginId_" + randomNum;
    
    var tr = document.selection.createRange();
    tr.collapse(true);
    
    window.external.BeginUndoUnit(document, "расформатировать заголовок (v" + version + ")");
    
    tr.pasteHTML("<B id=" + selectionBeginId + "></B>");
    var el = document.getElementById(selectionBeginId);
    var selectionIsNone = tr.compareEndPoints("StartToEnd", tr) == 0;
    
    var titleFound = false;
    var insideSection = false;
    var sectionHasTitle = false;
    var targetSection = null;
    var originalEl = el;
    
    if (fbw_body.contains(el)) {
        // Сначала определяем, в какой секции находимся
        var tempEl = el;
        while (tempEl && tempEl.nodeName != "BODY" && tempEl.nodeName != "DIV") {
            tempEl = tempEl.parentNode;
        }
        while (tempEl && tempEl.nodeName == "DIV" && tempEl.className != "section" && tempEl.className != "body") {
            tempEl = tempEl.parentNode;
        }
        if (tempEl && (tempEl.className == "section" || tempEl.className == "body")) {
            insideSection = true;
            targetSection = tempEl;
            
            // Проверяем, есть ли заголовок в этой секции
            var firstChild = targetSection.firstChild;
            if (firstChild && firstChild.nodeName == "DIV" && firstChild.className == "title") {
                sectionHasTitle = true;
            }
        }
        
        // Ищем родительский DIV с class="title" от позиции курсора
        el = originalEl;
        while (!(el.nodeName == "BODY" ||
                (el.nodeName == "DIV" && el.className == "title"))) {
            el = el.parentNode;
        }
        
        if (el.nodeName == "DIV" && el.className == "title") {
            // Проверяем, что это заголовок внутри section или body (не внутри poem)
            var elParent = el.parentNode;
            if (elParent && (elParent.className == "section" || elParent.className == "body")) {
                titleFound = true;
                
                // === ВОССТАНОВЛЕННЫЙ АЛГОРИТМ ИЗ ИСХОДНОГО СКРИПТА ===
                var saveClassName = el.className;
                elParent = el.parentNode;
                var el3 = el;
                
                if ((el.className == "title" || el.className == "epigraph" || el.className == "annotation")
                     && (elParent.className == "section" || elParent.className == "body")) {
                    el3 = el;
                    while (el3 && el3.nodeName == "DIV" && (el3.className == "title" || el3.className == "epigraph" || el3.className == "annotation")) {
                        el3 = el3.nextSibling;
                    }
                } else {
                    el3 = null;
                }
                
                if (el3 && el3.nodeName == "DIV" && el3.className == "section") {
                    var sectionChild = el3.firstChild;
                    var newTargetSection;
                    
                    if (sectionChild &&
                        (sectionChild.nodeName != "DIV" || sectionChild.className == "cite" || sectionChild.className == "poem" || sectionChild.className == "table")) {
                        newTargetSection = el3;
                        var el4 = el3.previousSibling;
                    } else {
                        var newSection = document.createElement("DIV");
                        newSection.className = "section";
                        newSection = el3.insertAdjacentElement("beforeBegin", newSection);
                        newTargetSection = newSection;
                        var el4 = newTargetSection.previousSibling;
                    }
                    
                    var previousNode;
                    var savedPrevousNode = el.previousSibling;
                    var newNode;
                    
                    while (el4 && el4 != savedPrevousNode) {
                        previousNode = el4.previousSibling;
                        el4 = el4.removeNode(true);
                        newNode = newTargetSection.insertAdjacentElement("afterBegin", el4);
                        unformatDivsAndParagraphsInsideElement(newNode);
                        if (newNode.className != "image" && newNode.className != "section") {
                            newNode.removeNode(false);
                        }
                        el4 = previousNode;
                    }
                } else if (el.className == "title") {
                    // Обработка последующих epigraph и annotation
                    var nextEl = el.nextSibling;
                    var nextNode3 = null;
                    if (nextEl) {
                        nextNode3 = nextEl.nextSibling;
                    }
                    while (nextEl && nextEl.className && (nextEl.className == "epigraph" || nextEl.className == "annotation")) {
                        unformatDivsAndParagraphsInsideElement(nextEl);
                        nextEl.removeNode(false);
                        nextEl = nextNode3;
                        if (nextEl) {
                            nextNode3 = nextEl.nextSibling;
                        }
                    }
                    
                    unformatDivsAndParagraphsInsideElement(el);
                }
                
                var saveNextAfterEl = el.nextSibling;
                
                if (el.className != "annotation") {
                    el.removeNode(false);
                } else {
                    if (thereIsParentBodyDiv(el)) {
                        el.removeNode(false);
                    }
                }
                
                if (saveNextAfterEl == null) {
                    InflateIt(elParent.lastChild);
                } else {
                    InflateIt(saveNextAfterEl.previousSibling);
                }
                InflateIt(elParent);
                // === КОНЕЦ АЛГОРИТМА ИЗ ИСХОДНОГО СКРИПТА ===
            }
        }
    }
    
    // Удаляем временный маркер
    var marker = document.getElementById(selectionBeginId);
    if (marker) {
        marker.removeNode(true);
    }
    
    window.external.EndUndoUnit(document);
    
    // Вывод сообщений
    if (!titleFound) {
        var errorMessage = "";
        if (insideSection && sectionHasTitle) {
            errorMessage = scriptName + "\nver. " + version + "\n\nКурсор находится не в заголовке.\nУстановите курсор внутрь заголовка, который надо расформатировать.";
        } else {
            errorMessage = scriptName + "\nver. " + version + "\n\nВ данной секции нет заголовка.\nУстановите курсор или выделите заголовок, который надо расформатировать.";
        }
        MsgBox(errorMessage, "FBE скрипт");
    } else {
        if (showStatistics == 1) {
            MsgBox(scriptName + "\nver. " + version + "\n\nЗаголовок успешно расформатирован.", "FBE скрипт");
        }
    }
    
    // Вспомогательные функции
    function unformatDivsAndParagraphsInsideElement(elem) {
        var divs, ps, i;
        divs = elem.getElementsByTagName("DIV");
        ps = elem.getElementsByTagName("P");
        for (i = 0; i < ps.length; i++) {
            ps[i].removeAttribute("class");
            ps[i].removeAttribute("className");
        }
        for (i = divs.length - 1; i >= 0; i--) {
            if (divs[i].className != "image" && divs[i].className != "section") {
                divs[i].removeNode(false);
            }
        }
        return;
    }
    
    function thereIsParentBodyDiv(ptr) {
        while (ptr && ptr.nodeName && ptr.nodeName != "BODY" &&
            !(ptr.nodeName == "DIV" && ptr.className && ptr.className == "body")) {
            ptr = ptr.parentNode;
        }
        if (ptr && ptr.nodeName == "DIV" && ptr.className && ptr.className == "body") {
            return true;
        } else {
            return false;
        }
    }
}
