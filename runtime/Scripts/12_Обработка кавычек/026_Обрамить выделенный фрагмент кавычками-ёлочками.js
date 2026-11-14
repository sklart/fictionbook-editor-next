// Скрипт "Обрамить выделенный фрагмент кавычками-ёлочками" для редактора FBE
// version 2.3
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для обрамления кавычками-ёлочками выделенного фрагмента текста в fb2 документах.
// Скрипт учитывает пробелы до и после слов и ставит открывающую кавычку после пробела,
// а закрывающую кавычку после слова и до пробела.
// Знаки препинания, находящиеся на конце выделения, оказываются внутри кавычек.
// При полном выделении нескольких абзацев, открывающая кавычка ставится в начале первого,
// а закрывающая - в конце последнего абзаца.
// Скрипт расставляет кавычки без оглядки на типы размеченных элементов - заголовки, цитаты, стихи, эпиграфы,
// и на то, что начало и конец выделения могут быть в разных секциях.
// Неполное выделение слов допустимо, кавычка ставится в конце слова.
// По умолчанию скрипт работает в тихом режиме.
// Поддержка отмены действий (Ctrl+Z).

// version 2.3, 07.07.2026
//======================================

function Run() {
    var scriptName = "Обрамить выделенный фрагмент кавычками-ёлочками";
    var version = "2.3";

    var nbspChar = String.fromCharCode(160);
    try {
        var tmpNbsp = window.external.GetNBSP();
        if (tmpNbsp.charCodeAt(0) != 160) {
            nbspChar = tmpNbsp;
        }
    } catch(e) {}

    var MyTagName = "B";

    var fbw_body = document.getElementById("fbw_body");
    if (!fbw_body) {
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка: fbw_body не найден.");
        return;
    }

    if (document.selection.type != "Text" && document.selection.type != "Control") {
        MsgBox(scriptName + "\nver. " + version + "\n\nНет выделения.");
        return;
    }

    var tr = document.selection.createRange();
    if (!tr || tr.text === "") {
        return;
    }

    var onlySpaces = true;
    for (var i = 0; i < tr.text.length; i++) {
        var ch = tr.text.charAt(i);
        if (ch != " " && ch != "\t" && ch != "\n" && ch != "\r" && ch != nbspChar) {
            onlySpaces = false;
            break;
        }
    }
    if (onlySpaces) {
        return;
    }

    if (document.selection.type == "Control") {
        return;
    }

    window.external.BeginUndoUnit(document, "Обрамить кавычками-ёлочками");

    var selectedText = tr.text;

    function isSpace(ch) {
        if (ch == " " || ch == nbspChar || ch == "\t" || ch == "\n" || ch == "\r") return true;
        return false;
    }

    function isPunct(ch) {
        if (ch == "." || ch == "," || ch == "!" || ch == "?" || ch == ":" || ch == ";") return true;
        if (ch == "-" || ch == String.fromCharCode(8211) || ch == String.fromCharCode(8212)) return true;
        if (ch == "\u2026") return true;
        return false;
    }

    function isSpaceOrPunct(ch) {
        return isSpace(ch) || isPunct(ch);
    }

    // ==================================================
    // ШАГ 1: Вставляем маркеры
    // ==================================================
    var range1 = tr.duplicate();
    range1.collapse(true);
    range1.pasteHTML("<" + MyTagName + " id=QsStart></" + MyTagName + ">");

    var range2 = tr.duplicate();
    range2.collapse(false);
    range2.pasteHTML("<" + MyTagName + " id=QsEnd></" + MyTagName + ">");

    var startMarker = document.getElementById("QsStart");
    var endMarker = document.getElementById("QsEnd");

    if (!startMarker || !endMarker) {
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка: маркеры не вставлены.");
        window.external.EndUndoUnit(document);
        return;
    }

    // ==================================================
    // ШАГ 2: Находим текстовый узел ПОСЛЕ startMarker
    // ==================================================
    var startTextNode = null;
    var startOffset = 0;
    
    var node = startMarker.nextSibling;
    while (node && !startTextNode) {
        if (node.nodeType == 3) {
            startTextNode = node;
            startOffset = 0;
        } else if (node.nodeType == 1 && node != endMarker) {
            var inner = node.firstChild;
            while (inner) {
                if (inner.nodeType == 3) {
                    startTextNode = inner;
                    startOffset = 0;
                    break;
                }
                if (inner.firstChild) {
                    inner = inner.firstChild;
                } else if (inner.nextSibling) {
                    inner = inner.nextSibling;
                } else {
                    while (inner && inner.parentNode && inner.parentNode != node && !inner.nextSibling) {
                        inner = inner.parentNode;
                    }
                    if (!inner || inner.parentNode == node) break;
                    inner = inner.nextSibling;
                }
            }
        }
        if (!startTextNode) {
            if (node.nextSibling) node = node.nextSibling;
            else break;
        }
    }

    // ==================================================
    // ШАГ 3: Находим текстовый узел ПЕРЕД endMarker
    // ==================================================
    var endTextNode = null;
    var endOffset = 0;
    
    node = endMarker.previousSibling;
    
    if (endMarker.parentNode && endMarker.parentNode.nodeName == "DIV") {
        var prevP = endMarker.previousSibling;
        while (prevP && prevP.nodeName != "P") {
            prevP = prevP.previousSibling;
        }
        if (prevP && prevP.nodeName == "P") {
            var inner = prevP.lastChild;
            while (inner) {
                if (inner.nodeType == 3) {
                    endTextNode = inner;
                    endOffset = inner.nodeValue.length;
                    break;
                }
                if (inner.lastChild) {
                    inner = inner.lastChild;
                } else if (inner.previousSibling) {
                    inner = inner.previousSibling;
                } else {
                    while (inner && inner.parentNode && inner.parentNode != prevP && !inner.previousSibling) {
                        inner = inner.parentNode;
                    }
                    if (!inner || inner.parentNode == prevP) break;
                    inner = inner.previousSibling;
                }
            }
        }
    }
    
    if (!endTextNode) {
        while (node && !endTextNode) {
            if (node.nodeType == 3) {
                endTextNode = node;
                endOffset = node.nodeValue.length;
            } else if (node.nodeType == 1 && node != startMarker) {
                var inner = node.lastChild;
                while (inner) {
                    if (inner.nodeType == 3) {
                        endTextNode = inner;
                        endOffset = inner.nodeValue.length;
                        break;
                    }
                    if (inner.lastChild) {
                        inner = inner.lastChild;
                    } else if (inner.previousSibling) {
                        inner = inner.previousSibling;
                    } else {
                        while (inner && inner.parentNode && inner.parentNode != node && !inner.previousSibling) {
                            inner = inner.parentNode;
                        }
                        if (!inner || inner.parentNode == node) break;
                        inner = inner.previousSibling;
                    }
                }
            }
            if (!endTextNode) {
                if (node.previousSibling) node = node.previousSibling;
                else break;
            }
        }
    }

    if (!startTextNode || !endTextNode) {
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка: не найдены текстовые узлы.");
        if (startMarker.parentNode) startMarker.parentNode.removeChild(startMarker);
        if (endMarker.parentNode) endMarker.parentNode.removeChild(endMarker);
        window.external.EndUndoUnit(document);
        return;
    }

    // ==================================================
    // ШАГ 4: Проверяем соседей МАРКЕРОВ
    // ==================================================
    var textBefore = "";
    var prevOfMarker = startMarker.previousSibling;
    if (prevOfMarker && prevOfMarker.nodeType == 3) {
        textBefore = prevOfMarker.nodeValue;
    }

    var textAfter = "";
    var nextOfMarker = endMarker.nextSibling;
    if (nextOfMarker && nextOfMarker.nodeType == 3) {
        textAfter = nextOfMarker.nodeValue;
    }

    // ==================================================
    // ШАГ 5: Корректировка НАЧАЛЬНОЙ границы
    // ==================================================
    var s = startTextNode.nodeValue;

    if (textBefore.length > 0 && s.length > 0) {
        var lastChBefore = textBefore.charAt(textBefore.length - 1);
        var firstChCurrent = s.charAt(0);
        if (!isSpaceOrPunct(lastChBefore) && !isSpaceOrPunct(firstChCurrent)) {
            var fullText = textBefore + s;
            var wordStart = textBefore.length;
            while (wordStart > 0 && !isSpaceOrPunct(fullText.charAt(wordStart - 1))) {
                wordStart--;
            }
            if (wordStart < textBefore.length) {
                startTextNode = prevOfMarker;
                startOffset = wordStart;
            } else {
                startOffset = wordStart - textBefore.length;
            }
        }
    }

    s = startTextNode.nodeValue;
    while (startOffset < s.length && isSpace(s.charAt(startOffset))) {
        startOffset++;
    }

    // ==================================================
    // ШАГ 6: Корректировка КОНЕЧНОЙ границы
    // ==================================================
    s = endTextNode.nodeValue;

    if (textAfter.length > 0 && s.length > 0) {
        var lastChCurrent = s.charAt(s.length - 1);
        var firstChAfter = textAfter.charAt(0);
        if (!isSpaceOrPunct(lastChCurrent) && !isSpaceOrPunct(firstChAfter)) {
            var fullText2 = s + textAfter;
            var wordEnd = s.length;
            while (wordEnd < fullText2.length && !isSpaceOrPunct(fullText2.charAt(wordEnd))) {
                wordEnd++;
            }
            if (wordEnd > s.length) {
                endTextNode = nextOfMarker;
                endOffset = wordEnd - s.length;
            } else {
                endOffset = wordEnd;
            }
        }
    }

    s = endTextNode.nodeValue;
    while (endOffset > 0 && isSpace(s.charAt(endOffset - 1))) {
        endOffset--;
    }

    // ==================================================
    // ШАГ 7: Проверяем P
    // ==================================================
    var ptr = startTextNode;
    var insideP = false;
    while (ptr && ptr != fbw_body) {
        if (ptr.nodeType == 1 && ptr.nodeName == "P") {
            insideP = true;
            break;
        }
        ptr = ptr.parentNode;
    }
    if (!insideP) {
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка: выделение не внутри абзаца.");
        if (startMarker.parentNode) startMarker.parentNode.removeChild(startMarker);
        if (endMarker.parentNode) endMarker.parentNode.removeChild(endMarker);
        window.external.EndUndoUnit(document);
        return;
    }

    // ==================================================
    // ШАГ 8: Вставка кавычек
    // ==================================================
    s = startTextNode.nodeValue;
    startTextNode.nodeValue = s.substring(0, startOffset) + "\u00AB" + s.substring(startOffset);

    if (startTextNode == endTextNode) {
        if (endOffset >= startOffset) {
            endOffset += 1;
        }
    }

    s = endTextNode.nodeValue;
    endTextNode.nodeValue = s.substring(0, endOffset) + "\u00BB" + s.substring(endOffset);

    // ==================================================
    // Удаляем маркеры
    // ==================================================
    if (startMarker.parentNode) startMarker.parentNode.removeChild(startMarker);
    if (endMarker.parentNode) endMarker.parentNode.removeChild(endMarker);

    window.external.EndUndoUnit(document);
}
