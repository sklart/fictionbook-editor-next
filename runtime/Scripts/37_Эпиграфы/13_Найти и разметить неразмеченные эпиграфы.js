// Скрипт "Найти и разметить неразмеченные эпиграфы" для редактора FBE
// version 1.4
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для автоматической разметки неразмеченных эпиграфов в fb2 документах.
// Скрипт выполняет простой цикл:
// Нашли кандидата в эпиграфы (алгоритм скрипта stokber-а), выполнили алгоритм разметки эпиграфов,
// повторили цикл нужное кол-во раз до конца документа.
// Режим работы: обычный и тихий.
// Поддержка отмены действий (Ctrl+Z).

// Основано на:
// - Скрипте "Поиск неразмеченных эпиграфов" от stokber (оригинальный код без изменений)
// - Скрипте "Создать эпиграф из полных абзацев (расширенная версия)" от Sclex/TaKir (оригинальный код без изменений)

// version 1.4, 11.02.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Найти и разметить неразмеченные эпиграфы";
    var version = "1.4";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1; // Измените на 0 для тихого режима
    
    // Настройка: 1 - спрашивать подтверждение для каждого эпиграфа, 0 - обрабатывать автоматически
    var askForEachEpigraph = 0; // Измените на 1 для ручного подтверждения
    
    // Какие разделы body обрабатывать:
    // "main" - только основной body (fbname="")
    // "notes" - только раздел сносок (fbname="notes")  
    // "comments" - только раздел комментариев (fbname="comments")
    // "all" - все разделы
    var processBodyType = "main"; // По умолчанию только основной body
    
    // ==================================================
    // ФУНКЦИЯ ПОИСКА ЭПИГРАФОВ
// Скрипт "Поиск неразмеченных эпиграфов" от stokber (оригинальный код без изменений)
    // ==================================================
    
    function searchNextEpigraph() {
        var name = "Поиск неразмеченных эпиграфов";
        var version = "1.12";
        var nonStop = 0;
        
        var blockElementClass = "title";
        var undoMsg = "переход на следующий неразмеченный эпиграф";
        var statusBarMsg = "Переходим на следующий неразмеченный эпиграф…";
        var notFoundMsg = "До конца документа скрипт не видит неразмеченных эпиграфов.";
        
        try {
            var nbspChar = window.external.GetNBSP();
            var nbspEntity;
            if(nbspChar.charCodeAt(0) == 160) nbspEntity = "&nbsp;";
            else nbspEntity = nbspChar;
        } catch(e) {
            var nbspChar = String.fromCharCode(160);
            var nbspEntity = "&nbsp;";
        }
        
        var re2 = new RegExp(" |&nbsp;|" + nbspChar, "g");

        function checkP(elem1) {
            if(hasAmongParents(elem1, "title")) return true;
            return false;
        }

        function hasAmongParents(elem2, nameOfClass) {
            while(elem2 && elem2.nodeName != "BODY") {
                if(elem2.nodeName == "DIV" && elem2.className == nameOfClass) return true;
                elem2 = elem2.parentNode;
            }
            return false;
        }

        function getParentWithClass(elem3, nameOfClass) {
            while(elem3 && elem3.nodeName != "BODY") {
                if(elem3.nodeName == "DIV" && elem3.className == nameOfClass) return elem3;
                elem3 = elem3.parentNode;
            }
            return null;
        }
        
        var emptyLineRegExp = new RegExp("^( | |&nbsp;|" + nbspChar + ")*?$", "i");

        function isLineEmpty(ptr) {
            return emptyLineRegExp.test(ptr.innerHTML.replace(/<[^>]*?>/gi, ""));
        }

        function getNextNode(el) {
            if(el.firstChild && el.nodeName != "P") el = el.firstChild;
            else {
                while(el && !el.nextSibling) el = el.parentNode;
                if(el && el.nextSibling) el = el.nextSibling;
            }
            return el;
        }

        function getNextP(el) {
            var savedEl = el;
            while(el && (el.nodeName != "P" || el == savedEl)) el = getNextNode(el);
            return el;
        }

        function scrollIfItNeeds() {
            var selection = document.selection;
            if(selection) {
                var range = selection.createRange();
                var rect = range.getBoundingClientRect();
                var correction = (rect.bottom - document.documentElement.clientHeight / 2);
                window.scrollBy(0, correction);
            }
        }

        function selEpi() {
            if(!tr1) {
                return { found: false, text: "" };
            }
            
            var colStr = 12;
            var rxTitle = new RegExp("^(<DIV class=stanza>)?<DIV class=title>", "im");
            var rxDiv = new RegExp("^<DIV class=(epigraph|annotation|cite|poem)>", "im");
            var rxP = new RegExp("^<DIV class=section>", "im");
            var rxCopyright = new RegExp("^©|®", "");
            var rxNote = new RegExp("<DIV id=([nc]|comment|FbAutId|bookmark)_?\\d+ class=section><DIV class=title>", "im");
            
            tr1.moveEnd("character", 2);
            tr1.moveEnd("character", -1);
            tr1.collapse(false);
            tr1.select();
            var rng = document.selection.createRange();
            if(!rng) return { found: false, text: "" };
            var pe = rng.parentElement();
            var p = "";
            while(pe.parentElement && pe.tagName != "DIV" && pe.className != "section") pe = pe.parentElement;
            
            p = pe.outerHTML;
            
            // если добрались до разделов с примечаниями или комментариями — завершаем работу:
            if((rxNote.test(p)) == true) {
                return { found: false, text: "", stop: true };
            }
            
            rxDiv.lastIndex = 0;
            rxTitle.lastIndex = 0;
            rxP.lastIndex = 0;
            if((rxTitle.test(p)) == true && (rxP.test(p)) == false) {
                tr1.moveEnd("character", -1);
                tr1.collapse;
                tr1.select();
                return { found: false, text: "" };
            } else if((rxDiv.test(p)) == true && (rxP.test(p)) == false) {
                return { found: false, text: "" };
            } else if(rxCopyright.test(p) == true) {
                return { found: false, text: "" };
            } else {
                p = p.replace(new RegExp("^<DIV class=section>(?!<DIV class=title>)", "im"), "$&</DIV>\r\n");
                p = p.replace(new RegExp("^(.|[\\r\\n])+?<\/DIV>", "im"), "");
                p = p.replace(new RegExp("^<DIV onresizestart=(.|[\\r\\n])+", "im"), "");
                p = p.replace(new RegExp("^<DIV class=(.|[\\r\\n])+", "im"), "");
                p = p.replace(new RegExp("^<P class=subtitle>(.|[\\r\\n])+", "im"), "");
                p = p.replace(new RegExp("^((?:.*\\r\\n){"+colStr+"})(?:(?:.|[\\r\\n])+)", "gm"), "$1");
                p = p.replace(new RegExp("</DIV>", "g"), "");
                p = p.replace(new RegExp("<sup>", "g"), "☺");
                p = p.replace(new RegExp("</sup>", "g"), "☻");
                p = p.replace(new RegExp("<P>&nbsp;</P>", "mgi"), "<P>∇∇∇.</P>");
                p = p.replace(new RegExp("&nbsp;", "g"), " ");
                p = p.replace(new RegExp("<[^>]+>", "g"), "");
                p = p.replace(new RegExp("&lt;", "g"), "<");
                p = p.replace(new RegExp("&gt;", "g"), ">");
                p = p.replace(new RegExp("&amp;", "g"), "&");
                p = p.replace(new RegExp("&shy;", "g"), " ");
                p = p.replace(new RegExp("([.?…!])([ ]?(\\*+|\\[\\d+\\]|\\{\\d+\\}|☺\\d+☻)[ ]?)$", "gm"), "$2$1");
                p = p.replace(new RegExp("[☺☻]", "g"), "");
                
                var rxEndStr = new RegExp("^.{1,80}[\\da-zа-яё]([!?]?[\\)\\}\\]»”“*])?$", "im");
                
                p = p.replace(new RegExp("^(.{1,80}\\r\\n).{81,}\\r\\n[\\s\\S]*", "im"), "$1");
                p = p.replace(new RegExp("^((.+\\r\\n)+.{1,80}[\\da-zа-яё]([!?]?[\\\\}\\]»”“*])?\\r\\n)[\\s\\S]*", "im"), "$1");
                
                if((rxEndStr.test(p)) == true) {
                    p = p.replace(new RegExp("^∇∇∇\\.$", "mgi"), "");
                    
                    var n = (p.match(/\r\n\r?\n?/g) || []).length;
                    p = p.replace(new RegExp("\\r\\n", "mgi"), " ");
                    var colSumb = p.length - 1;
                    
                    if(n > 3 && colSumb > n * 80) {
                        return { found: false, text: "" };
                    } else {
                        tr1.moveEnd("character", colSumb);
                        tr1.select();
                        return { found: true, text: p, range: tr1.duplicate() };
                    }
                } else {
                    return { found: false, text: "" };
                }
            }
        }
        
        var s;
        var tr, el, prv, pm, saveNext, saveFirstEmpty, nextPtr;
        var errMsg = "Нет выделения.\n\nПеред запуском скрипта нужно выделить абзацы, которые будут обработаны.";
        
        tr = document.selection.createRange();
        if(tr.parentElement().nodeName == "TEXTAREA" || tr.parentElement().nodeName == "INPUT") {
            return { found: false, text: "", error: "Ошибка. Должно быть выделение в тексте книги, а не в поле ввода." };
        }
        
        try {
            window.external.SetStatusBarText(statusBarMsg);
        } catch(e) {}
        
        var fbwBody = document.getElementById("fbw_body");
        var tr3 = document.selection.createRange();
        tr3.collapse(false);
        var ptr = tr3.parentElement();
        var blockElementAtStart = getParentWithClass(ptr, blockElementClass);
        ptr = getNextP(ptr);
        
        while(ptr && fbwBody.contains(ptr)) {
            if(checkP(ptr) && getParentWithClass(ptr, blockElementClass) != blockElementAtStart) break;
            ptr = getNextP(ptr);
        }
        
        if(ptr && fbwBody.contains(ptr)) {
            var tr1 = document.body.createTextRange();
            var parentElem = getParentWithClass(ptr, blockElementClass);
            if(parentElem) {
                tr1.moveToElementText(parentElem);
                if(tr1.moveStart("character", 1) == 1) tr1.moveStart("character", -1);
                tr1.moveEnd("character", -1);
                tr1.select();
                scrollIfItNeeds();
            }
        } else {
            return { found: false, text: "", end: true };
        }
        
        try {
            window.external.SetStatusBarText("Эпиграф?");
        } catch(e) {}
        
        return selEpi();
    }
    
    // ==================================================
    // ФУНКЦИЯ РАЗМЕТКИ ЭПИГРАФА
// Скрипт "Создать эпиграф из полных абзацев (расширенная версия)" от Sclex/TaKir (оригинальный код без изменений)
    // ==================================================
    
    function createEpigraph(cp, check) {
        // Настройки из оригинала
        var showStatistics = 0;
        var authorParagraphMode = 2;
        var minLengthDiffPercent = 10;
        var reformatMode = 1;
        var maxNameLength = 100;

        function GoToEndOfElement(elem) {
            if(!elem) return;
            var b=elem.getBoundingClientRect();
            if (b.bottom-b.top<=window.external.getViewHeight())
                window.scrollBy(0,(b.top+b.bottom-window.external.getViewHeight())/2);
            else
                window.scrollBy(0,b.top);
            var r=document.selection.createRange();
            if (!r || !("compareEndPoints" in r)) return;
            r.moveToElementText(elem);
            r.collapse(false);
            r.select();
        }

        function trimStr(str) {
            if (!str) return str;
            return str.replace(/^\s+|\s+$/g, '');
        }

        function normalizeText(text) {
            if (!text) return text;
            return trimStr(text.replace(/\u00A0/g, ' '));
        }

        function isFIO(text) {
            if (!text || text.length > maxNameLength) return false;
            var normalized = normalizeText(text);
            if (normalized.length === 0) return false;
            var cleanText = normalized.replace(/\[\d+\]/g, '');
            cleanText = trimStr(cleanText);
            if (cleanText.length === 0) return false;
            
            var hasLetterDot = false;
            var capitalCount = 0;
            var dotCount = 0;
            
            for(var i = 0; i < cleanText.length; i++) {
                var char = cleanText.charAt(i);
                var code = char.charCodeAt(0);
                var isUpper = (code >= 1040 && code <= 1071) || (code >= 65 && code <= 90);
                
                if (isUpper) {
                    capitalCount++;
                    if (i < cleanText.length - 1 && cleanText.charAt(i + 1) === '.') {
                        hasLetterDot = true;
                    }
                }
                if (char === '.') dotCount++;
            }
            
            var words = cleanText.split(/\s+/);
            if (words.length > 4) return false;
            if (capitalCount === 0) return false;
            
            var variant1 = hasLetterDot;
            var variant2 = dotCount >= 2;
            var variant3 = capitalCount >= words.length * 0.7;
            
            return variant1 || variant2 || variant3;
        }

        function hasQuotes(text) {
            if (!text) return false;
            var quotePairs = [
                { open: '«', close: '»' },
                { open: '"', close: '"' },
                { open: '„', close: '«' }
            ];
            
            for(var i = 0; i < quotePairs.length; i++) {
                var openChar = quotePairs[i].open;
                var closeChar = quotePairs[i].close;
                var openCount = 0;
                var closeCount = 0;
                
                for(var j = 0; j < text.length; j++) {
                    var currentChar = text.charAt(j);
                    if (currentChar === openChar) openCount++;
                    if (currentChar === closeChar) closeCount++;
                }
                
                if (openChar === closeChar) {
                    if (openCount >= 2) return true;
                } else {
                    if (openCount >= 1 && closeCount >= 1) return true;
                }
            }
            return false;
        }

        function isFullyFormatted(elem) {
            if (!elem) return false;
            var html = elem.innerHTML || '';
            if (!html) return false;
            var htmlWithoutNotes = html.replace(/<a\s+[^>]*class\s*=\s*["']?note["']?[^>]*>.*?<\/a>/gi, '');
            var pattern = /^\s*<(strong|em)(\s+[^>]*)?>(.*)<\/\1>\s*$/i;
            return pattern.test(htmlWithoutNotes);
        }

        function removeOuterFormatting(elem) {
            if (!elem) return;
            var originalHTML = elem.innerHTML;
            if (!originalHTML) return;
            
            var noteCounter = 0;
            var notesMap = {};
            var htmlWithMarkers = originalHTML.replace(/<a\s+[^>]*class\s*=\s*["']?note["']?[^>]*>.*?<\/a>/gi, function(match) {
                var marker = '<!--NOTE_' + (noteCounter++) + '-->';
                notesMap[marker] = match;
                return marker;
            });
            
            var pattern = /^\s*<(strong|em)(\s+[^>]*)?>(.*)<\/\1>\s*$/i;
            var match = htmlWithMarkers.match(pattern);
            
            if (match) {
                var newHTML = match[3];
                for(var marker in notesMap) {
                    newHTML = newHTML.replace(marker, notesMap[marker]);
                }
                try {
                    elem.innerHTML = newHTML;
                } catch(e) {}
            }
        }

        function removeAllFormatting(elem) {
            if (!elem) return;
            var originalHTML = elem.innerHTML;
            if (!originalHTML) return;
            var newHTML = originalHTML;
            for(var pass = 0; pass < 3; pass++) {
                newHTML = newHTML.replace(/<(em|strong)(\s+[^>]*)?>([^<]*)<\/\1>/gi, '$3');
                newHTML = newHTML.replace(/<(em|strong)(\s+[^>]*)?>([^<]*(?:<(?!\/?\1)[^>]*>[^<]*)*)<\/\1>/gi, '$3');
            }
            try {
                elem.innerHTML = newHTML;
            } catch(e) {}
        }

        function reformatElement(elem, mode) {
            if (!elem || mode === 0) return;
            if (mode === 1) {
                if (isFullyFormatted(elem)) {
                    removeOuterFormatting(elem);
                }
            } else if (mode === 2) {
                removeAllFormatting(elem);
            }
        }

        function forceReformatAuthorElement(elem) {
            removeAllFormatting(elem);
        }

        function GetCP(cp) {
            if(!cp) return null;
            if(cp.tagName == "P") cp = cp.parentElement;
            if(cp.tagName == "DIV" && cp.className == "title") cp = cp.parentElement;
            if(cp.tagName != "DIV") return null;
            return cp;
        }

        function InsBefore(parent, ref, item) {
            if(ref) ref.insertAdjacentElement("beforeBegin", item);
            else parent.insertAdjacentElement("beforeEnd", item);
        }

        function InflateIt(elem) {
            if(!elem || elem.nodeType != 1) return;
            if(elem.tagName == "P"){ window.external.inflateBlock(elem) = true; return; }
            elem = elem.firstChild;
            while(elem){ InflateIt(elem); elem = elem.nextSibling; }
        }

        function SkipOver(np, n1, n2, n3) {
            while (np) {
                if(!(np.tagName == "P" && !np.firstChild && !window.external.inflateBlock(np)) &&
                    (!n1 || (np.tagName != n1 && np.className != n1)) &&
                    (!n2 || (np.tagName != n2 && np.className != n2)) &&
                    (!n3 || (np.tagName != n3 && np.className != n3)))
                    break;
                np = np.nextSibling;
            }
            return np;
        }

        var tr = document.selection.createRange();
        function findParentParagraph(element) {
            while (element && element.nodeName != "P" && element.nodeName != "DIV") {
                element = element.parentElement;
            }
            return element;
        }
        
        var rngStart = tr.duplicate();
        rngStart.collapse(true);
        var startEl = findParentParagraph(rngStart.parentElement());
        
        var rngEnd = tr.duplicate();
        rngEnd.collapse(false);
        var endEl = findParentParagraph(rngEnd.parentElement());
        
        var rng2 = document.body.createTextRange();
        rng2.moveToElementText(startEl);
        tr.setEndPoint("StartToStart", rng2);
        rng2 = document.body.createTextRange();
        rng2.moveToElementText(endEl);
        tr.setEndPoint("EndToEnd", rng2);
        
        var cp = tr.parentElement();
        
        cp = GetCP(cp);
        if(!cp) return;
        
        if(cp.className != "body" && cp.className != "section" && cp.className != "poem") return;

        var pp = cp.firstChild;
        if(cp.className == "body")
            pp = SkipOver(pp, "title", "image", "epigraph");
        else
            pp = SkipOver(pp, "title", "epigraph", null);

        if(check) return true;

        if (document.selection.type && document.selection.type == "Control") {
            return { error: "Вы используете не тот тип выделения, с которым работает вставка эпиграфа." };
        }

        var rng = tr.duplicate();
        var txt = "";
        var pps;

        if(rng && rng.text != "") {
            var dpps = document.createElement("DIV");
            dpps.innerHTML = rng.htmlText;
            pps = dpps.getElementsByTagName("P");
            if(pps.length == 0) {
                dpps.innerHTML = "<P>" + rng.htmlText + "</P>";
                pps = dpps.getElementsByTagName("P");
                if(pps.length == 0) {
                    txt = rng.text;
                }
            }
        }

        window.external.BeginUndoUnit(document, "создание эпиграфа из полных абзацев (расширенная версия)");
        var ep = document.createElement("DIV");
        ep.className = "epigraph";
        
        var processedParagraphs = 0;
        var authorParagraphs = 0;
        var reformattedParagraphs = 0;
        
        if(txt != "") {
            var pwt = document.createElement("P");
            pwt.innerHTML = txt;
            processedParagraphs = 1;
            ep.appendChild(pwt);
        } else if(pps && pps.length > 0) {
            processedParagraphs = pps.length;
            
            for(var i = 0; i < pps.length; ++i) {
                var pwt = document.createElement("P");
                pwt.innerHTML = pps[i].innerHTML;
                
                if (reformatMode > 0) {
                    var shouldReformat = false;
                    if (reformatMode === 2) {
                        shouldReformat = true;
                    } else if (reformatMode === 1) {
                        shouldReformat = isFullyFormatted(pwt);
                    }
                    
                    if (shouldReformat) {
                        try {
                            reformatElement(pwt, reformatMode);
                            reformattedParagraphs++;
                        } catch(e) {}
                    }
                }
                
                var makeAuthor = false;
                
                if (i === pps.length - 1 && pps.length > 1) {
                    var currentText = pps[i].innerText || pps[i].textContent || '';
                    var prevText = pps[i-1].innerText || pps[i-1].textContent || '';
                    
                    var current = normalizeText(currentText);
                    var prev = normalizeText(prevText);
                    
                    if (current.length > 0 && prev.length > 0) {
                        var lengthDiffPercent = 0;
                        if (prev.length > 0) {
                            lengthDiffPercent = ((prev.length - current.length) / prev.length) * 100;
                        }
                        
                        var isShorterByPercent = lengthDiffPercent >= minLengthDiffPercent;
                        
                        switch(authorParagraphMode) {
                            case 0:
                                makeAuthor = false;
                                break;
                            case 1:
                                makeAuthor = isShorterByPercent;
                                break;
                            case 2:
                                if (isShorterByPercent) {
                                    makeAuthor = true;
                                } else {
                                    var hasQuotesInText = hasQuotes(current);
                                    if (hasQuotesInText) {
                                        makeAuthor = true;
                                    } else {
                                        var isFIOtext = isFIO(current);
                                        makeAuthor = isFIOtext;
                                    }
                                }
                                break;
                            case 3:
                                makeAuthor = true;
                                break;
                            default:
                                if (isShorterByPercent) {
                                    makeAuthor = true;
                                } else {
                                    var hasQuotesInText = hasQuotes(current);
                                    if (hasQuotesInText) {
                                        makeAuthor = true;
                                    } else {
                                        var isFIOtext = isFIO(current);
                                        makeAuthor = isFIOtext;
                                    }
                                }
                                break;
                        }
                    }
                    
                    if (makeAuthor) {
                        pwt.className = "text-author";
                        authorParagraphs++;
                        try {
                            forceReformatAuthorElement(pwt);
                        } catch(e) {}
                    }
                }
                
                ep.appendChild(pwt);
            }
        } else {
            ep.appendChild(document.createElement("P"));
        }

        var el = startEl;
        var psArr = [];
        psArr.push(el);
        while (el != endEl) {
            if (el.firstChild)
                el = el.firstChild;
            else {
                while (el.nextSibling == null)
                    el = el.parentNode;
                el = el.nextSibling;
            }
            if (el.nodeName == "P") psArr.push(el);
        }

        InsBefore(cp, pp, ep);
        InflateIt(ep);

        var el2, parentOfEl2;
        while (psArr.length > 0) {
            el2 = psArr.pop();
            parentOfEl2 = el2.parentNode;
            if (el2 && (el2.nextSibling || el2.parentNode.className != "section")) {
                el2.removeNode(true);
            } else {
                el2.innerHTML = "";
                InflateIt(el2);
            }
            if (parentOfEl2.nodeName == "DIV" && 
                (parentOfEl2.className == "title" || parentOfEl2.className == "epigraph" || parentOfEl2.className == "cite") &&
                !parentOfEl2.firstChild)
                parentOfEl2.removeNode(true);
        }
        
        window.external.EndUndoUnit(document);
        GoToEndOfElement(ep);
        
        return { success: true, processed: processedParagraphs };
    }
    
    // ==================================================
    // ОСНОВНОЙ ЦИКЛ ОБРАБОТКИ
    // ==================================================
    
    function executeScript() {
        var startTime = new Date().getTime();
        var totalFound = 0;
        var totalProcessed = 0;
        var totalSkipped = 0;
        var foundEpigraphs = [];
        
        window.external.BeginUndoUnit(document, scriptName + " v." + version);
        
        try {
            // Поиск всех эпиграфов
            var keepSearching = true;
            while(keepSearching) {
                var result = searchNextEpigraph();
                
                if(result.end) {
                    keepSearching = false;
                } else if(result.found) {
                    totalFound++;
                    foundEpigraphs.push({
                        range: result.range,
                        text: result.text
                    });
                    
                    // Если нужно спрашивать подтверждение
                    if(askForEachEpigraph) {
                        var shortText = result.text;
                        if(shortText.length > 150) shortText = shortText.substring(0, 150) + "...";
                        
                        var confirmMsg = "Найден потенциальный эпиграф (" + totalFound + "):\n\n" +
                                        shortText + "\n\n" +
                                        "Создать эпиграф из этого текста?";
                        
                        if(!AskYesNo(confirmMsg)) {
                            totalSkipped++;
                            continue;
                        }
                    }
                    
                    // Создаем эпиграф
                    try {
                        var createResult = createEpigraph();
                        if(createResult && createResult.success) {
                            totalProcessed++;
                            if(showStatistics) {
                                window.external.SetStatusBarText("Обработано эпиграфов: " + totalProcessed);
                            }
                        }
                    } catch(e) {
                        // Пропускаем ошибки
                    }
                }
                
                // Небольшая задержка для стабильности
                try {
                    var waitTime = new Date().getTime();
                    while(new Date().getTime() - waitTime < 10) {}
                } catch(e) {}
            }
            
            window.external.EndUndoUnit(document);
            
            var endTime = new Date().getTime();
            var executionTime = (endTime - startTime) / 1000;
            
            if(showStatistics) {
                var statsMessage = scriptName + "\n" +
                                  "ver. " + version + "\n" +
                                  "---------------------------------------\n" +
                                  "✓ Всего найдено потенциальных эпиграфов: " + totalFound + "\n" +
                                  "✓ Успешно обработано: " + totalProcessed + "\n" +
                                  "✓ Пропущено: " + totalSkipped + "\n" +
                                  "---------------------------------------\n" +
                                  "Настройки обработки:\n" +
                                  "• Разделы: " + 
                                  (processBodyType == "main" ? "только основной body" :
                                   processBodyType == "notes" ? "только сноски" :
                                   processBodyType == "comments" ? "только комментарии" : "все разделы") + "\n" +
                                  "• Режим подтверждения: " + (askForEachEpigraph ? "спрашивать для каждого" : "автоматически") + "\n" +
                                  "---------------------------------------\n" +
                                  "Время выполнения: " + executionTime.toFixed(3) + " сек.";
                
                MsgBox(statsMessage);
            }
            
        } catch(e) {
            window.external.EndUndoUnit(document);
            if(showStatistics) {
                MsgBox("Произошла ошибка при выполнении скрипта:\n" + e.message);
            }
        }
    }
    
    // Запускаем скрипт
    executeScript();
}
