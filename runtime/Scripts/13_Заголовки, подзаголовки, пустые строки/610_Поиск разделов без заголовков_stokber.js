// Скрипт «Поиск разделов без заголовков» v.1.0
// DeepSeek
// идея и финальная правка — stokber (2025, декабрь)

function Run() {

var name = 'Поиск разделов без заголовков';
var version = '1.0';

function findSectionWithoutTitleSimple() {
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
        
        // Ищем первую секцию без заголовка ниже курсора
        var foundSection = null;
        
        for (var j = 0; j < allSections.length; j++) {
            var section = allSections[j];
            
            // Проверяем, находится ли секция ниже курсора
            var sectionRange = document.body.createTextRange();
            sectionRange.moveToElementText(section);
            sectionRange.collapse(true);
            
            // Если секция начинается после курсора
            if (cursorPos.compareEndPoints('StartToStart', sectionRange) < 0) {
                // Проверяем структуру секции
                var firstChild = null;
                
                // Ищем первый не-текстовый дочерний элемент
                for (var k = 0; k < section.childNodes.length; k++) {
                    if (section.childNodes[k].nodeType === 1) {
                        firstChild = section.childNodes[k];
                        break;
                    }
                }
                
                // Если первый элемент - не DIV с классом title
                if (firstChild && 
                    !(firstChild.tagName && 
                      firstChild.tagName.toUpperCase() === 'DIV' && 
                      (firstChild.className || '').indexOf('title') !== -1)) {
                    
                    // Ищем первый параграф в секции
                    var paragraphs = section.getElementsByTagName('P');
                    if (paragraphs.length > 0) {
                        var firstParagraph = paragraphs[0];
                        
                        // Выделяем параграф
                        var selectRange = document.body.createTextRange();
                        selectRange.moveToElementText(firstParagraph);
                         // selectRange.select();
                    // костыль для корректировки выделения:
                    selectRange.moveStart('character', 1);
                    // selectRange.select();
                    selectRange.moveStart('character', -1);
                        selectRange.select();
                        selectRange.scrollIntoView();
                        
                        // сдвигаем выделение к центру экрана:
                    scrollIfItNeeds();
                        
                        // window.status = 'Найдена секция без заголовка. Текст выделен.';
                        var textMsg = 'Найдена секция без заголовка.';
                        window.external.SetStatusBarText(textMsg);
                        foundSection = section;
                        break;
                    }
                }
            }
        }
        
        if (!foundSection) {
            window.status = '';
            alert('Секций без заголовков ниже курсора не найдено.\n\nСкрипт «'+name+'» v.'+version);
        }
        
    } catch (e) {
        alert('Ошибка: ' + e.message);
    }
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

    findSectionWithoutTitleSimple();
    
}
