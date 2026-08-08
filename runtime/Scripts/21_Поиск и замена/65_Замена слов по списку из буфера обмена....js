    // Скрипт: Замена слов по списку из буфера обмена
    // Версия: 2.9 (2026, май)
    // автор - stokber
    // использованы фрагменты кода из скриптов ув. Sclex-а, а также помощь DeepSeek.

function Run() {

    var name = "Замена слов по списку из буфера обмена ";
    var ver = "v.2.9";

    // ========== НАСТРАИВАЕМЫЕ ПАРАМЕТРЫ ==========
    var checkBoundaryLetters = true;   // если true, не заменять слова, примыкающие к буквам в соседних узлах.
    var matchNonBreakingSpaceAsSpace = true; // если true, при поиске обычный пробел = (обычный или неразрывный); при замене всегда обычный пробел.
    // =============================================

    // Определяем символ неразрывного пробела (из настроек FBE или по умолчанию):
    var nbspEntity = "&nbsp;";
    var nbspChar = String.fromCharCode(160);
    try {
        var nbspFromFBE = window.external.GetNBSP();
        if (nbspFromFBE.charCodeAt(0) != 160)
            nbspEntity = nbspFromFBE;
        nbspChar = nbspFromFBE;
    } catch (e) {
        nbspChar = String.fromCharCode(160);
    }

    // --------------------------------------------------------------------
    // Получение и подготовка списка замен из буфера обмена:
    var help = "\n\nСправка\n\nСкрипт производит замену любых слов в выделенном тексте (или во всём документе, если выделения нет). Перед запуском скопируйте в буфер обмена список замен. Список представляет собой любое количество строк с парой разделённых символом решетки (#) слов или словосочетаний с пробелами — тех, которые меняем и то, на которые меняем. Перед, после и между ними (словами) может находиться любое количество пробелов. Пустые строки между таких строк замен игнорируются. В списке не должно быть никаких других символов, кроме символов разделителя (#), пробелов и букв русского и английского алфавитов. В качестве списка замен может выступать любой скопированный текст из любой программы, включая FBE.\n\nПример строки замен:\n\nашипка#ошибка\nапичатка#опечатка\nмалако#молоко\n";

    var str = window.clipboardData.getData('Text');
    if (str == null) {
        MsgBox("Буфер обмена пуст или в нем нет текстовых данных!" + help);
        return;
    }
    else {
        str = str.replace(/( |□|▫|◦| )+/g, " ");
        str = str.replace(/^ +| +$/gm, "");
        str = str.replace(/ +(?=#)/g, "");
        str = str.replace(/# +/g, "#");
        str = str.replace(/\r\n(\r\n)+/gm, "\r\n");
        str = str.replace(/^\r\n|\r\n$/gm, "");

        if (str == false) {
            MsgBox("Буфер обмена пуст или в нем нет текстовых данных!" + help);
            return;
        }

        var regexp = new RegExp("#", "");
        if (regexp.test(str) == false) {
            MsgBox("В вашем списке из буфера обмена по меньшей мере в одной из строк отсутствует разделитель в виде символа решетки (#). Проверьте свой список." + help);
            return;
        }

        var regexp = new RegExp("[^a-zа-яё\\\s#]", "i");
        if (regexp.test(str)) {
            MsgBox("По меньшей мере один символ в вашем списке из буфера обмена не является буквой. Проверьте свой список." + help);
            return;
        }

        var regexp = new RegExp("#.*?#", "");
        if (regexp.test(str)) {
            MsgBox("В вашем списке из буфера обмена по меньшей мере в одной из строк более одного разделителя в виде символа решетки (#). Проверьте свой список." + help);
            return;
        }

        var regexp = new RegExp("^#|#$", "");
        if (regexp.test(str)) {
            MsgBox("В вашем списке из буфера обмена по меньшей мере в одной из строк разделитель в виде символа решетки (#) находится не между слов. Проверьте свой список." + help);
            return;
        }

        var strLength = str.split(/\r\n|\r|\n/).length;
        MsgBox("В буфере обмена обнаружен следующий список строк замен (" + strLength + "):\n\n" + str);

        str = str.replace(/^([a-zа-яё #]+)#([a-zа-яё ]+)$/igm, "\"$1\": \"$2\",");
        str = str.replace(/\r\n/gm, "");
        str = str.replace(/,$/g, "");
        str = str.replace(/^.+$/gm, "var words = {$&};");
    }
    eval(str);

    // Сохраняем оригинальные ключи:
    var originalKeys = [];
    for (var k in words) {
        originalKeys.push(k);
    }

    // --------------------------------------------------------------------
    // var ObrabotkaType = 2;
    var MyTagName = "B";
    var Ts = new Date().getTime();
    var count_01 = 0;
    var neSlovo = new RegExp("[^a-zA-Zа-яёА-ЯЁ #]+");
    var s;

    // --------------------------------------------------------------------
    // Поиск предыдущего текстового узла (ограничен текущим абзацем):
    function getPrevTextNode(node, container) {
        var limit = node.parentNode;
        while (limit && limit.nodeName != "P") limit = limit.parentNode;
        if (!limit) limit = container;
        var cur = node;
        while (cur && cur != limit) {
            if (cur.previousSibling) {
                cur = cur.previousSibling;
                while (cur.lastChild) cur = cur.lastChild;
                if (cur.nodeType == 3) return cur;
                if (cur.nodeType == 1) {
                    var deep = cur;
                    while (deep.lastChild) deep = deep.lastChild;
                    if (deep.nodeType == 3) return deep;
                }
            } else {
                cur = cur.parentNode;
                if (cur == limit) return null;
            }
        }
        return null;
    }

    // Поиск следующего текстового узла:
    function getNextTextNode(node, container) {
        var limit = node.parentNode;
        while (limit && limit.nodeName != "P") limit = limit.parentNode;
        if (!limit) limit = container;
        var cur = node;
        while (cur && cur != limit) {
            if (cur.nextSibling) {
                cur = cur.nextSibling;
                while (cur.firstChild) cur = cur.firstChild;
                if (cur.nodeType == 3) return cur;
                if (cur.nodeType == 1) {
                    var deep = cur;
                    while (deep.firstChild) deep = deep.firstChild;
                    if (deep.nodeType == 3) return deep;
                }
            } else {
                cur = cur.parentNode;
                if (cur == limit) return null;
            }
        }
        return null;
    }

     // Функция замены слов (упрощённая: при замене всегда ставим обычные пробелы):
    function replacWord(node, prevTextNode, nextTextNode) {
        for (var key in words) {
            var correct = words[key];
            if (neSlovo.test(key)) continue;
            var searchKey = key;
            if (matchNonBreakingSpaceAsSpace) {
                searchKey = key.replace(/ /g, "[ " + nbspChar + "]");
            }
            var re01 = new RegExp("(^|[^a-zA-Z0-9а-яёА-ЯЁ])(" + searchKey + ")(?=[^a-zA-Z0-9а-яёА-ЯЁ]|$)", "g");
            function replacer(match, p1, p2, offset, full) {
                var wordStartPos = offset + p1.length;
                var wordEndPos = wordStartPos + p2.length;
                var isAtStart = (wordStartPos == 0);
                var isAtEnd = (wordEndPos == full.length);
                if (checkBoundaryLetters) {
                    if (isAtStart && prevTextNode && prevTextNode.nodeValue.length > 0) {
                        var lastCharPrev = prevTextNode.nodeValue.charAt(prevTextNode.nodeValue.length - 1);
                        if (/[a-zA-Zа-яёА-ЯЁ]/.test(lastCharPrev)) return match;
                    }
                    if (isAtEnd && nextTextNode && nextTextNode.nodeValue.length > 0) {
                        var firstCharNext = nextTextNode.nodeValue.charAt(0);
                        if (/[a-zA-Zа-яёА-ЯЁ]/.test(firstCharNext)) return match;
                    }
                }
                count_01++;
                return p1 + correct;
            }
            s = s.replace(re01, replacer);
        }
    }

    // поиск незаменённых слов:
    function findRemainingMatches(text, keys, nbspChar, matchNonBreakingSpaceAsSpace) {
        var found = [];
        for (var i = 0; i < keys.length; i++) {
            var key = keys[i];
            if (!key || key.length == 0) continue;
            var searchKey = key;
            if (matchNonBreakingSpaceAsSpace) {
                searchKey = key.replace(/ /g, "[ " + nbspChar + "]");
            }
            var re = new RegExp("(^|[^a-zA-Z0-9а-яёА-ЯЁ])(" + searchKey + ")(?=[^a-zA-Z0-9а-яёА-ЯЁ]|$)", "g");
            var match;
            while ((match = re.exec(text)) !== null) {
                var word = match[2];
                if (word && word.length > 0) found.push(word);
            }
        }
        var unique = [];
        for (var i = 0; i < found.length; i++) {
            var duplicate = false;
            for (var j = 0; j < unique.length; j++) {
                if (unique[j] == found[i]) { duplicate = true; break; }
            }
            if (!duplicate) unique.push(found[i]);
        }
        return unique;
    }

    // --------------------------------------------------------------------
    var rng = document.selection.createRange();
    var hasSelection = (rng.compareEndPoints("StartToEnd", rng) != 0);
    // Переменные для хранения информации о замене и для сбора текста после замены:
    var wholeDocumentFlag = false;
    var textareaElement = null;
    var selectionTextNodes = [];

    if (hasSelection) {
        var tr = document.selection.createRange();
        if (!tr) {
            MsgBox("Нет выделения.\n\nПеред запуском скрипта нужно выделить текст, который будет обработан.");
            return;
        }
        window.external.BeginUndoUnit(document, "замену слов по списку");

        if (tr.parentElement().nodeName == "TEXTAREA") {
            textareaElement = tr.parentElement();
            var tr1 = document.body.createTextRange();
            tr1.moveToElementText(textareaElement);
            tr1.setEndPoint("EndToStart", tr);
            var tr2 = document.body.createTextRange();
            tr2.moveToElementText(textareaElement);
            tr2.setEndPoint("StartToEnd", tr);
            s = tr.text;
            // if (ObrabotkaType == 2) 
            replacWord(null, null, null);
            textareaElement.value = tr1.text + s + tr2.text;
            wholeDocumentFlag = false;
        } else if (tr.parentElement().nodeName != "INPUT") {
            var body = document.getElementById("fbw_body");
            if (!body) body = document.body;
            var ttr1 = document.selection.createRange();
            var coll = tr.getClientRects();
            var el = body.document ? body.document.elementFromPoint(coll[0].left, coll[0].top) : document.elementFromPoint(coll[0].left, coll[0].top);
            var cursorPos = null;
            if (tr.compareEndPoints("StartToEnd", tr) == 0) {
                var el2 = document.getElementById("CursorPosition");
                if (el2) el2.removeAttribute("id");
                ttr1.pasteHTML("<" + MyTagName + " id=CursorPosition></" + MyTagName + ">");
                cursorPos = document.getElementById("CursorPosition");
                ttr1.expand("word");
            }
            tr = ttr1.duplicate();
            tr.collapse();
            tr.pasteHTML("<" + MyTagName + " id=BlockStart></" + MyTagName + ">");
            tr = ttr1.duplicate();
            tr.collapse(false);
            tr.pasteHTML("<" + MyTagName + " id=BlockEnd></" + MyTagName + ">");

            var BlockStartNode = document.getElementById("BlockStart");
            var BlockEndNode = document.getElementById("BlockEnd");
            if (!BlockStartNode || !BlockEndNode) {
                MsgBox("Ошибка установки маркеров выделения.");
                window.external.EndUndoUnit(document);
                return;
            }

            var container = BlockStartNode.parentNode;
            while (container) {
                if (container.contains(BlockEndNode)) break;
                container = container.parentNode;
            }
            if (!container) container = body;

            var InsideP = false;
            var InsideSelection = false;
            var ptr = container;

            while (ptr) {
                if (ptr == container.parentNode) break;
                if (ptr.nodeType == 1 && ptr.nodeName == "P") InsideP = true;
                if (ptr.nodeType == 1 && ptr.nodeName == MyTagName && ptr.getAttribute("id") == "BlockStart") InsideSelection = true;
                if (ptr.nodeType == 1 && ptr.nodeName == MyTagName && ptr.getAttribute("id") == "BlockEnd") InsideSelection = false;
                if (ptr.nodeType == 3 && InsideP && InsideSelection) {
                    selectionTextNodes.push(ptr);
                    s = ptr.nodeValue;
                    // if (ObrabotkaType == 2) {
                        var prevNode = getPrevTextNode(ptr, container);
                        var nextNode = getNextTextNode(ptr, container);
                        replacWord(ptr, prevNode, nextNode);
                    // }
                    ptr.nodeValue = s;
                }
                if (ptr.firstChild) {
                    ptr = ptr.firstChild;
                } else if (ptr.nextSibling) {
                    ptr = ptr.nextSibling;
                } else {
                    while (ptr && !ptr.nextSibling) {
                        ptr = ptr.parentNode;
                        if (ptr == container) { ptr = null; break; }
                    }
                    if (ptr) ptr = ptr.nextSibling;
                }
            }

            var newRange = document.body.createTextRange();
            if (!cursorPos) {
                newRange.moveToElementText(BlockStartNode);
                var endRange = document.body.createTextRange();
                endRange.moveToElementText(BlockEndNode);
                newRange.setEndPoint("StartToStart", endRange);
                newRange.select();
            } else {
                newRange.moveToElementText(cursorPos);
                newRange.select();
            }
            if (BlockStartNode && BlockStartNode.parentNode) BlockStartNode.parentNode.removeChild(BlockStartNode);
            if (BlockEndNode && BlockEndNode.parentNode) BlockEndNode.parentNode.removeChild(BlockEndNode);
            if (cursorPos && cursorPos.parentNode) cursorPos.parentNode.removeChild(cursorPos);
            wholeDocumentFlag = false;
        }
        window.external.EndUndoUnit(document);
    } else {
        window.external.BeginUndoUnit(document, "замену слов по списку");
        var body = document.getElementById("fbw_body");
        if (!body) body = document.body;
        function traverse(node, insideP, container) {
            if (!node) return;
            if (node.nodeType == 1 && (node.nodeName == "SCRIPT" || node.nodeName == "STYLE")) return;
            if (node.nodeType == 3 && insideP) {
                s = node.nodeValue;
                // if (ObrabotkaType == 2) {
                    var prevNode = getPrevTextNode(node, container);
                    var nextNode = getNextTextNode(node, container);
                    replacWord(node, prevNode, nextNode);
                // }
                node.nodeValue = s;
            } else if (node.nodeType == 1) {
                var newInsideP = insideP || (node.nodeName == "P");
                var newContainer = (node.nodeName == "P") ? node : container;
                var children = node.childNodes;
                for (var i = 0; i < children.length; i++) {
                    traverse(children[i], newInsideP, newContainer);
                }
            }
        }
        traverse(body, false, body);
        window.external.EndUndoUnit(document);
        wholeDocumentFlag = true;
    }

    // --------------------------------------------------------------------
    // Подсчёт времени
    var Tf = new Date().getTime();
    var T2 = Tf - Ts;
    var Tmin = Math.floor(T2 / 60000);
    var TsecD = (T2 % 60000) / 1000;
    var Tsec = Math.floor(TsecD);
    var tempus = "";
    if (Tmin == 0)
        tempus = (TsecD + "").replace(/(.{1,5}).*/g, "$1").replace(".", ",") + " сек";
    else {
        tempus = Tmin + " мин";
        if (Tsec != 0) tempus += " " + Tsec + " с";
    }

    // --------------------------------------------------------------------
    // Проверка оставшихся совпадений
    var remainingMatches = [];
    if (originalKeys.length > 0) {
        var fullText = "";
        if (textareaElement) {
            fullText = textareaElement.value;
        } else if (!wholeDocumentFlag && selectionTextNodes.length > 0) {
            // Сбор текста из узлов выделения с добавлением пробелов на границах абзацев при необходимости
            for (var i = 0; i < selectionTextNodes.length; i++) {
                var nodeVal = selectionTextNodes[i].nodeValue;
                if (nodeVal && typeof nodeVal == "string") {
                    fullText += nodeVal;
                    if (i < selectionTextNodes.length - 1) {
                        var nextNode = selectionTextNodes[i+1];
                        var nextVal = nextNode.nodeValue;
                        if (nextVal && nextVal.length > 0) {
                            var pCurrent = selectionTextNodes[i].parentNode;
                            while (pCurrent && pCurrent.nodeName != "P") pCurrent = pCurrent.parentNode;
                            var pNext = nextNode.parentNode;
                            while (pNext && pNext.nodeName != "P") pNext = pNext.parentNode;
                            if (pCurrent != pNext) {
                                var lastChar = nodeVal.charAt(nodeVal.length - 1);
                                var firstChar = nextVal.charAt(0);
                                if (/[a-zA-Zа-яёА-ЯЁ]/.test(lastChar) && /[a-zA-Zа-яёА-ЯЁ]/.test(firstChar)) {
                                    fullText += ' ';
                                }
                            }
                        }
                    }
                }
            }
        } else if (wholeDocumentFlag) {
            var bodyText = document.getElementById("fbw_body");
            if (!bodyText) bodyText = document.body;
            fullText = bodyText.innerText || bodyText.textContent || "";
        }
        if (fullText && typeof fullText == "string" && fullText.length > 0) {
            remainingMatches = findRemainingMatches(fullText, originalKeys, nbspChar, matchNonBreakingSpaceAsSpace);
        }
    }

    // Фильтрация пустых/пробельных строк
    var filteredMatches = [];
    for (var i = 0; i < remainingMatches.length; i++) {
        var trimmed = remainingMatches[i].replace(/^\s+|\s+$/g, '');
        if (trimmed.length > 0) filteredMatches.push(trimmed);
    }
    remainingMatches = filteredMatches;

    // Формируем основное сообщение о результатах
    var mainMsg = "Заменено совпадений: " + count_01 + "\nВремя выполнения: " + tempus + "\n\nСкрипт '" + name + "' " + ver;

    // Если есть незаменённые слова, добавляем вопрос и показываем confirm
    if (remainingMatches.length > 0) {
        var regexPattern = "";
        for (var i = 0; i < remainingMatches.length; i++) {
            var word = remainingMatches[i].replace(/[.*+?^${}()|[\]\\]/g, '\\$&'); // здесь можно было обойтись регекспом по-проще, но оставил на будущее.
            if (i > 0) regexPattern += "|";
            regexPattern += word;
        }
        var finalRegex = "(?<![a-zA-Zа-яёА-ЯЁ])(" + regexPattern + ")(?![a-zA-Zа-яёА-ЯЁ])";
        
        // заменит ещё одby вид неразр. пробела:
        finalRegex = finalRegex.replace(/ /g, " ");
        
        var confirmMsg = mainMsg + "\n\nНайдены незаменённые слова (возможно, из-за разного начертания):\n" + remainingMatches.join(", ") +
                         "\n\nСкопировать регулярное выражение для ручного поиска в буфер обмена?";
        if (confirm(confirmMsg)) {
            try {
                window.clipboardData.setData('Text', finalRegex);
                alert("Регулярное выражение скопировано в буфер обмена.\nВставьте его в окно расширенного поиска FBE (Cntrl+F, галочка 'Регулярное выражение').");
            } catch(e) {
                alert("Не удалось скопировать регексп в буфер обмена.\n\n" + finalRegex);
            }
        }
    } else {
        alert(mainMsg);
    }
}
