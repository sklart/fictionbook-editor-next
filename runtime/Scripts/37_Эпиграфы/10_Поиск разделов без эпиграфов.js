// Скрипт «Поиск разделов без эпиграфов» v.1.1
// DeepSeek
// идея и финальная правка — stokber (2025, декабрь)

function Run() {

var name = 'Поиск разделов без эпиграфов';
var version = '1.1';

 function findSectionWithoutEpigraphExact() {
    try {
        // Получаем текущую позицию курсора
        var selection = document.selection;
        if (!selection) {
            alert('Не удалось получить выделение.');
            return;
        }
        
        var range = selection.createRange();
        var cursorPos = range.duplicate();
        cursorPos.collapse(true);
        
        // ПОЛУЧАЕМ ЭЛЕМЕНТ, В КОТОРОМ НАХОДИТСЯ КУРСОР
        var currentElement = range.parentElement();
        
        // ПРОВЕРЯЕМ, НАХОДИТСЯ ЛИ КУРСОР В notes ИЛИ comments
        if (isCursorInNotesOrComments(currentElement)) {
            // Курсор находится в notes или comments - завершаем работу скрипта
            window.external.SetStatusBarText('Курсор находится в примечаниях или комментариях. Поиск отменен.');
            alert('Курсор находится в примечаниях или комментариях. Поиск отменен.');
            return;
        }
        
        // Находим все секции
        var allSections = [];
        var allDivs = document.getElementsByTagName('DIV');
        
        for (var i = 0; i < allDivs.length; i++) {
            var div = allDivs[i];
            var className = div.className || '';
            if (className.indexOf('section') !== -1) {
                allSections.push(div);
            }
        }
        
        // Ищем первую секцию без эпиграфа ниже курсора
        for (var j = 0; j < allSections.length; j++) {
            var section = allSections[j];
            
            // Проверяем, находится ли секция ниже курсора
            var sectionRange = document.body.createTextRange();
            sectionRange.moveToElementText(section);
            sectionRange.collapse(true);
            
            if (cursorPos.compareEndPoints('StartToStart', sectionRange) < 0) {
                var hasEpigraph = false;
                var foundValidElement = false;
                var elementToHighlight = null;
                
                // Проходим по всем элементам секции по порядку
                for (var k = 0; k < section.childNodes.length; k++) {
                    var child = section.childNodes[k];
                    
                    if (child.nodeType === 1) { // ELEMENT_NODE
                        var childTagName = child.tagName ? child.tagName.toUpperCase() : '';
                        var childClassName = child.className || '';
                        
                        // Проверяем порядок элементов
                        if (childTagName === 'DIV') {
                            if (childClassName.indexOf('title') !== -1) {
                                // Это заголовок, пропускаем
                                continue;
                            } else if (childClassName.indexOf('epigraph') !== -1) {
                                // Нашли эпиграф
                                hasEpigraph = true;
                                
                                // Проверяем, что после эпиграфа есть хотя бы один параграф
                                var hasContentAfterEpigraph = false;
                                for (var m = k + 1; m < section.childNodes.length; m++) {
                                    var nextChild = section.childNodes[m];
                                    if (nextChild.nodeType === 1 && 
                                        nextChild.tagName && 
                                        nextChild.tagName.toUpperCase() === 'P') {
                                        hasContentAfterEpigraph = true;
                                        elementToHighlight = nextChild;
                                        break;
                                    }
                                }
                                
                                // Если после эпиграфа нет контента, считаем что эпиграф есть
                                if (!hasContentAfterEpigraph) {
                                    hasEpigraph = true;
                                }
                                break;
                            }
                        } else if (childTagName === 'P') {
                            // Нашли параграф, который не внутри DIV
                            // Проверяем, был ли перед ним эпиграф
                            var hasEpigraphBefore = false;
                            for (var n = 0; n < k; n++) {
                                var prevChild = section.childNodes[n];
                                if (prevChild.nodeType === 1 && 
                                    prevChild.tagName && 
                                    prevChild.tagName.toUpperCase() === 'DIV' &&
                                    (prevChild.className || '').indexOf('epigraph') !== -1) {
                                    hasEpigraphBefore = true;
                                    break;
                                }
                            }
                            
                            if (!hasEpigraphBefore) {
                                // Это параграф без эпиграфа перед ним
                                foundValidElement = true;
                                elementToHighlight = child;
                                break;
                            }
                        }
                    }
                }
                
                // Если не нашли эпиграф и нашли элемент для выделения
                if (!hasEpigraph && elementToHighlight) {
                    // ПРОВЕРЯЕМ, НАХОДИТСЯ ЛИ СЕКЦИЯ В notes ИЛИ comments
                    if (isElementInNotesOrComments(section)) {
                        // Пропускаем эту секцию - она находится в notes или comments
                        continue;
                    }
                    
                    // Выделяем элемент
                    var selectRange = document.body.createTextRange();
                    selectRange.moveToElementText(elementToHighlight);
                    // костыль для корректировки выделения:
                    selectRange.moveStart('character', 1);
                    selectRange.moveStart('character', -1);
                    selectRange.select();
                    // selectRange.scrollIntoView();
                    
                    // сдвигаем выделение к центру экрана:
                    scrollIfItNeeds();
                    
                    // Выводим сообщение в статус-бар
                    var textMsg = 'Найдена секция без эпиграфа. Текст выделен.';
                    window.external.SetStatusBarText(textMsg);

                    return; // Завершаем после нахождения
                }
            }
        }
        
        // Если ничего не найдено
        window.external.SetStatusBarText('');
        alert('Секций без эпиграфа ниже курсора не найдено.\n\nСкрипт «'+name+'» v.'+version);
        
    } catch (e) {
        alert('Ошибка: ' + e.message);
    }
}

// ФУНКЦИЯ ДЛЯ ПРОВЕРКИ, НАХОДИТСЯ ЛИ КУРСОР В notes ИЛИ comments
function isCursorInNotesOrComments(element) {
    if (!element) return false;
    
    // Поднимаемся вверх по дереву DOM
    var current = element;
    while (current && current !== document.body) {
        // Проверяем класс элемента
        var className = current.className || '';
        var fbname = current.getAttribute ? current.getAttribute('fbname') : '';
        
        // Проверяем, является ли элемент DIV с нужными атрибутами
        if (current.tagName && current.tagName.toUpperCase() === 'DIV') {
            // Проверяем комбинацию class=body и fbname="notes" или "comments"
            if (className.indexOf('body') !== -1) {
                if (fbname === 'notes' || fbname === 'comments') {
                    return true; // Нашли notes или comments
                }
            }
            
            // Также проверяем другие возможные комбинации
            if (className.indexOf('notes') !== -1 || className.indexOf('comments') !== -1) {
                return true;
            }
        }
        
        // Переходим к родительскому элементу
        current = current.parentNode;
    }
    
    return false;
}

// ФУНКЦИЯ ДЛЯ ПРОВЕРКИ, НАХОДИТСЯ ЛИ ЭЛЕМЕНТ В notes ИЛИ comments
function isElementInNotesOrComments(element) {
    if (!element) return false;
    
    // Поднимаемся вверх по дереву DOM от секции
    var current = element;
    while (current && current !== document.body) {
        // Проверяем класс элемента
        var className = current.className || '';
        var fbname = current.getAttribute ? current.getAttribute('fbname') : '';
        
        // Проверяем, является ли элемент DIV с нужными атрибутами
        if (current.tagName && current.tagName.toUpperCase() === 'DIV') {
            // Проверяем комбинацию class=body и fbname="notes" или "comments"
            if (className.indexOf('body') !== -1) {
                if (fbname === 'notes' || fbname === 'comments') {
                    return true; // Нашли notes или comments
                }
            }
            
            // Также проверяем другие возможные комбинации
            if (className.indexOf('notes') !== -1 || className.indexOf('comments') !== -1) {
                return true;
            }
        }
        
        // Переходим к родительскому элементу
        current = current.parentNode;
    }
    
    return false;
}

function scrollIfItNeeds() {
    var selection = document.selection;
    if(selection) {
        var range = selection.createRange();
        var rect = range.getBoundingClientRect();
        // var correction = (rect.bottom - document.documentElement.clientHeight/2); // центр
        var correction = (rect.bottom - document.documentElement.clientHeight / 2); // верх
        // var popravka = (rect.bottom - document.documentElement.clientHeight/8* 6); // низ
        window.scrollBy(0, correction);
    }
}

// ЗАПУСКАЕМ ОСНОВНУЮ ФУНКЦИЮ
findSectionWithoutEpigraphExact();
}
