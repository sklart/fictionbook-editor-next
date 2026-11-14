// Скрипт «Первую букву абзаца -  в верхний регистр» v.1.0
// переработка скрипта ув. Sclex-а «В верхний регистр» под заявленный функционал.
// создано при помощи DeepSeek.
// попросил DeepSeek - stokber (декабрь, 2025)

/* Основные изменения:
Добавлены флаги для отслеживания обработки абзацев:
paragraphProcessed - отмечает, что текущий абзац уже обработан;
paragraphStarted - отмечает начало нового абзаца.
Логика преобразования только первой буквы:
Скрипт ищет первую букву в каждом абзаце с помощью регулярного выражения /[a-zA-Zа-яА-Я]/;
Проверяет, является ли найденная буква строчной;
Преобразует только эту букву в прописную, сохраняя остальной текст неизменным.
Поддержка обеих кодировок:
Регулярное выражение включает как латинские (a-zA-Z), так и русские буквы (а-яА-Я).
Сохранение разметки:
Как и в оригинальном скрипте, теги и HTML-сущности остаются нетронутыми;
Изменяется только текстовое содержимое.
Обработка TEXTAREA:
Добавлена вспомогательная функция для обработки текста в текстовых полях.
Скрипт будет корректно работать с выделением, содержащим несколько абзацев, преобразуя первую букву каждого абзаца внутри выделения. */

function Run() {

    // имя тега, который будет использован для маркеров начала и конца выделения
    var MyTagName = "B";

    var tr;
    var errMsg = "Нет выделения.\n\nПеред запуском скрипта нужно выделить текст, который будет обработан.";
    tr = document.selection.createRange();
    if (!tr) {
        MsgBox(errMsg);
        return;
    }

    window.external.BeginUndoUnit(document, "смену регистра первой буквы");

    if (tr.parentElement().nodeName == "TEXTAREA") {
        // код для обработки выделения в INPUT'е
        var tr1 = document.body.createTextRange();
        tr1.moveToElementText(tr.parentElement());
        tr1.setEndPoint("EndToStart", tr);
        var tr2 = document.body.createTextRange();
        tr2.moveToElementText(tr.parentElement());
        tr2.setEndPoint("StartToEnd", tr);
        var s = tr.text;
        
        // Обработка для TEXTAREA: преобразуем первую букву каждого абзаца
        s = capitalizeFirstLetterOfParagraphs(s);
        
        tr.parentElement().value = tr1.text + s + tr2.text;
    }
    else if (tr.parentElement().nodeName != "INPUT") {
        var body = document.getElementById("fbw_body");
        var coll = tr.getClientRects();
        var ttr1 = body.document.selection.createRange();
        var el = body.document.elementFromPoint(coll[0].left, coll[0].top);
        var cursorPos = null;
        
        if (tr.compareEndPoints("StartToEnd", tr) == 0) {
            var el2 = document.getElementById("CursorPosition");
            if (el2) el2.removeAttribute("id");
            ttr1.pasteHTML("<" + MyTagName + " id=CursorPosition></" + MyTagName + ">");
            cursorPos = document.getElementById("CursorPosition");
            ttr1.expand("word");
        }
        
        // поставим маркеры блока в виде пустых ссылок
        tr = ttr1.duplicate();
        tr.collapse();
        tr.pasteHTML("<" + MyTagName + " id=BlockStart></" + MyTagName + ">");
        tr = ttr1.duplicate();
        tr.collapse(false);
        tr.pasteHTML("<" + MyTagName + " id=BlockEnd></" + MyTagName + ">");
        
        // поднимаемся вверх по дереву, пока не найдем DIV или P,
        // в который входит начало выделения
        while (el && el.nodeName != "DIV" && el.nodeName != "P") {
            el = el.parentNode;
        }
        
        var InsideP = false; // true, если находимся внутри тега P
        var InsideSelection = false; // true, когда текущая позиция внутри выделенного текста
        var ProcessingEnded = false; // true, когда обработка закончена и пора выходить
        var ptr = el;
        var paragraphProcessed = false; // флаг, что текущий абзац уже обработан
        var paragraphStarted = false; // флаг, что мы вошли в новый абзац
        
        while (!ProcessingEnded) {
            // nodeType=1 для элемента (тега) и nodeType=3 для текста
            
            // если встретили тег P
            if (ptr.nodeType == 1 && ptr.nodeName == "P") {
                InsideP = true;
                paragraphProcessed = false; // сброс флага для нового абзаца
                paragraphStarted = true; // начался новый абзац
            }
            
            // если встретили маркер начала блока
            if (ptr.nodeType == 1 && ptr.nodeName == MyTagName && ptr.getAttribute("id") == "BlockStart") {
                InsideSelection = true;
                var BlockStartNode = ptr;
            }
            
            // аналогично для маркера конца выделения
            if (ptr.nodeType == 1 && ptr.nodeName == MyTagName && ptr.getAttribute("id") == "BlockEnd") {
                InsideSelection = false;
                ProcessingEnded = true;
                var BlockEndNode = ptr;
            }
            
            // если нашли текст и находимся внутри P и внутри выделения
            if (ptr.nodeType == 3 && InsideP && InsideSelection && !paragraphProcessed) {
                var s = ptr.nodeValue;
                if (s && s.length > 0) {
                    // Ищем первую букву в тексте (латиница или кириллица)
                    var match = s.match(/[a-zA-Zа-яА-Я]/);
                    if (match && match.index !== undefined) {
                        var firstLetterIndex = match.index;
                        var firstLetter = s.charAt(firstLetterIndex);
                        
                        // Проверяем, является ли первая буква строчной
                        if (firstLetter === firstLetter.toLowerCase() && firstLetter !== firstLetter.toUpperCase()) {

                            // Преобразуем только первую букву в прописную
                            var newText = s.substring(0, firstLetterIndex) +
                                        firstLetter.toUpperCase() +
                                        s.substring(firstLetterIndex + 1);
                            ptr.nodeValue = newText;
                        }
                    }
                    paragraphProcessed = true; // текущий абзац обработан
                }
            }
            
            // если вышли из тега P
            if (ptr.nodeType == 1 && paragraphStarted && ptr.nodeName != "P") {
                paragraphStarted = false;
            }
            
            // теперь надо найти следующий по дереву узел для обработки
            if (ptr.firstChild != null) {
                ptr = ptr.firstChild; // либо углубляемся...
            } else {
                while (ptr.nextSibling == null) {
                    ptr = ptr.parentNode; // ...либо поднимаемся (если уже сходили вглубь)
                    // поднявшись до элемента P, не забудем поменять флаг
                    if (ptr && ptr.nodeType == 1 && ptr.nodeName == "P") {
                        InsideP = false;
                        paragraphProcessed = false;
                        paragraphStarted = false;
                    }
                }
                ptr = ptr.nextSibling; // и переходим на соседний элемент
            }
        }
        
        // удаляем маркеры блока
        var tr1 = document.body.createTextRange();
        if (!cursorPos) {
            tr1.moveToElementText(BlockStartNode);
            var tr2 = document.body.createTextRange();
            tr2.moveToElementText(BlockEndNode);
            tr1.setEndPoint("StartToStart", tr2);
            tr1.select();
        } else {
            tr1.moveToElementText(cursorPos);
            tr1.select();
        }
        BlockStartNode.parentNode.removeChild(BlockStartNode);
        BlockEndNode.parentNode.removeChild(BlockEndNode);
    }
    
    window.external.EndUndoUnit(document);
}

// Вспомогательная функция для обработки текста в TEXTAREA
function capitalizeFirstLetterOfParagraphs(text) {
    if (!text) return text;
    
    // Разбиваем текст на абзацы
    var paragraphs = text.split(/\r?\n/);
    var result = [];
    
    for (var i = 0; i < paragraphs.length; i++) {
        var paragraph = paragraphs[i];
        if (paragraph.length > 0) {
            // Ищем первую букву в абзаце (латиница или кириллица)
            var match = paragraph.match(/[a-zA-Zа-яА-Я]/);
            if (match && match.index !== undefined) {
                var firstLetterIndex = match.index;
                var firstLetter = paragraph.charAt(firstLetterIndex);
                
                // Проверяем, является ли первая буква строчной
                if (firstLetter === firstLetter.toLowerCase() && firstLetter !== firstLetter.toUpperCase()) {
                    // Преобразуем только первую букву в прописную
                    paragraph = paragraph.substring(0, firstLetterIndex) +
                               firstLetter.toUpperCase() +
                               paragraph.substring(firstLetterIndex + 1);
                }
            }
        }
        result.push(paragraph);
    }
    
    return result.join('\n');
}
