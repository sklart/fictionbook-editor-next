/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Расширенная нумерация абзацев",
  "version": "1.9",
  "authors": [
    {
      "name": "Haliz",
      "role": "contributor",
      "contribution": "idea"
    },
    {
      "name": "DeepSeek",
      "role": "ai_assistance"
    },
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "partial_attribution",
  "summary": "Добавляет выбранный префикс, номер и суффикс в начало абзацев.",
  "description": "Окно задаёт префикс, включение номера, начальное число, шаг, суффикс и пропуск пустых абзацев. Скрипт добавляет получившийся текст в начало выбранных абзацев; при отсутствии выделения предупреждает и обрабатывает все абзацы книги. Существующие номера не распознаёт и не заменяет.",
  "scope": "Полные абзацы, пересекающие выделенный диапазон в тексте книги.",
  "modifiesDocument": true,
  "requiresSelection": true,
  "usage": [
    "Выделите нужные абзацы, если нумеровать следует только часть книги.",
    "В окне задайте префикс, номер, шаг, суффикс, область и пропуск пустых строк; подтвердите запуск.",
    "Если выделения нет, скрипт предупредит и обработает всю книгу. Проверьте результат и число обработанных абзацев."
  ],
  "warnings": [
    "Команда добавляет текст в начало абзаца и не проверяет существующую нумерацию; повторный запуск добавит ещё один префикс.",
    "Без выделения команда после предупреждения обрабатывает все абзацы книги.",
    "Префикс и суффикс вставляются как HTML; символы разметки в этих полях могут изменить структуру абзаца.",
    "Нулевые начальный номер и шаг заменяются единицей. Если все части вставки пусты, счётчик всё равно может учитывать абзац как обработанный."
  ],
  "dependencies": [
    {
      "path": "HTML/Расширенная нумерация абзацев - параметры.htm",
      "sha256": "46c16b1536e6ecf7be0f8debafb81ac5d6edaf9eb512d761d46919801e21ecae",
      "optional": false
    }
  ],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "2fbf2bd9de97c26c5422fb75adb429a74cb7cc515ac21af868796ef188a1f78d",
    "reviewStatus": "generated"
  },
  "tags": [
    "'Числовые' скрипты",
    "параметры"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": ""
  },
  "settings": [
    {
      "name": "prefix",
      "title": "Префикс",
      "value": "",
      "expression": "settings.prefix",
      "description": "Текст перед номером; по умолчанию пусто.",
      "source": "dialog"
    },
    {
      "name": "useNumbering",
      "title": "Добавлять номер",
      "value": true,
      "expression": "settings.useNumbering",
      "description": "При выключении вставляются только префикс и суффикс.",
      "source": "dialog"
    },
    {
      "name": "startNumber",
      "title": "Начальный номер",
      "value": 1,
      "expression": "settings.startNumber",
      "description": "Первый номер; значение 0 код заменяет на 1.",
      "source": "dialog"
    },
    {
      "name": "increment",
      "title": "Шаг",
      "value": 1,
      "expression": "settings.increment",
      "description": "Приращение номера для следующего обработанного абзаца; 0 код заменяет на 1.",
      "source": "dialog"
    },
    {
      "name": "suffix",
      "title": "Суффикс",
      "value": ". ",
      "expression": "settings.suffix",
      "description": "Текст после номера; по умолчанию точка и пробел.",
      "source": "dialog"
    },
    {
      "name": "onlySelected",
      "title": "Только выделенные абзацы",
      "value": true,
      "expression": "settings.onlySelected",
      "description": "При отсутствии выделения скрипт переходит к обработке всей книги.",
      "source": "dialog"
    },
    {
      "name": "skipEmpty",
      "title": "Пропускать пустые абзацы",
      "value": true,
      "expression": "settings.skipEmpty",
      "description": "Не добавлять префикс к пустым и состоящим только из пробелов абзацам.",
      "source": "dialog"
    }
  ]
}
</FBE-SCRIPT-CARD>*/
// Скрипт «Расширенная нумерация абзацев» для редактора FBE
// version 1.9
// Автор идеи - Haliz
// Реализация - DeepSeek

// Скрипт предназначен для управляемой (настраиваемой) нумерации выделенных абзацев в fb2 документах.
// Поддержка отмены действий (Ctrl+Z).

// version 1.9, 04.11.2025
// ============================================

var scriptName = "Расширенная нумерация абзацев";
var scriptVersion = "1.9";

function Run() {
    var dialogWidth = "765px";
    var dialogHeight = "815px";
    
    // Показываем диалог настроек
    var settings = window.showModalDialog(
        "HTML/Расширенная нумерация абзацев - параметры.htm", 
        window,
        "dialogHeight: " + dialogHeight + "; dialogWidth: " + dialogWidth + "; " +
        "center: Yes; help: No; resizable: Yes; status: No;"
    );
    
    // Если пользователь нажал Отмена
    if (!settings) {
        return;
    }

    // Основной код нумерации
    var prefix = settings.prefix || "";
    var useNumbering = settings.useNumbering !== false;
    var startNumber = parseInt(settings.startNumber) || 1;
    var increment = parseInt(settings.increment) || 1;
    var suffix = settings.suffix || "";
    var onlySelected = settings.onlySelected !== false;
    var skipEmpty = settings.skipEmpty !== false;
    
    var cnt = startNumber;
    var nbspChar = String.fromCharCode(160);
    
    function isLineEmpty(ptr) {
        var emptyLineRegExp = new RegExp("^( | |&nbsp;|"+nbspChar+")*?$","i");
        return emptyLineRegExp.test(ptr.innerHTML.replace(/<(?!img)[^>]*?>/gi,""));
    }
    
    function processP(p) {
        if (skipEmpty && isLineEmpty(p)) {
            return false;
        }
        
        // Формируем текст для вставки
        var textToInsert = "";
        
        // Добавляем префикс, если он указан
        if (prefix) {
            textToInsert += prefix;
        }
        
        // Добавляем номер, если нумерация включена
        if (useNumbering) {
            textToInsert += cnt;
        }
        
        // Добавляем суффикс, если он указан
        if (suffix) {
            textToInsert += suffix;
        }
        
        // Вставляем только если есть что вставлять
        if (textToInsert) {
            p.insertAdjacentHTML("afterbegin", textToInsert);
        }
        
        if (useNumbering) {
            cnt += increment;
        }
        return true;
    }
    
    // Проверяем выделение
    var tr = document.selection.createRange();
    if (onlySelected && (!tr || tr.compareEndPoints("StartToEnd", tr) == 0)) {
        alert("Нет выделения. Будут обработаны все абзацы в документе.");
        onlySelected = false;
    }
    
    window.external.BeginUndoUnit(document, "добавление нумерации абзацев");
    
    try { 
        window.external.SetStatusBarText("Добавляем нумерацию в абзацы…"); 
    } catch(e) {}
    
    var body = document.getElementById("fbw_body");
    var processedCount = 0;
    
    if (onlySelected) {
        // Используем подход из рабочего скрипта с маркерами
        var MyTagName = "B";
        var coll = tr.getClientRects();
        var ttr1 = document.selection.createRange();
        var el = document.elementFromPoint(coll[0].left, coll[0].top);
        var cursorPos = null;
        
        if (tr.compareEndPoints("StartToEnd", tr) == 0) {
            var el2 = document.getElementById("CursorPosition");
            if (el2) el2.removeAttribute("id");
            ttr1.pasteHTML("<" + MyTagName + " id=CursorPosition></" + MyTagName + ">");
            cursorPos = document.getElementById("CursorPosition");
            ttr1.expand("word");
        }
        
        var rndm = Math.round(Math.random() * 100000).toString();
        var startId = "BlockStart" + rndm;
        var endId = "BlockEnd" + rndm;
        
        tr = ttr1.duplicate();
        tr.collapse();
        tr.pasteHTML("<" + MyTagName + " id=" + startId + "></" + MyTagName + ">");
        tr = ttr1.duplicate();
        tr.collapse(false);
        tr.pasteHTML("<" + MyTagName + " id=" + endId + "></" + MyTagName + ">");
        
        while (el && el.nodeName != "DIV" && el.nodeName != "P") { 
            el = el.parentNode; 
        }
        
        var insideSelection = false;
        var processingEnded = false;
        var ptr = el;
        
        while (!processingEnded) {
            if (ptr.nodeType == 1 && ptr.nodeName == MyTagName && ptr.getAttribute("id") == startId) {
                insideSelection = true;
                var BlockStartNode = ptr;
                var pp = ptr;
                while (pp && pp.nodeName.toUpperCase() != "BODY" && pp.nodeName.toUpperCase() != "P") pp = pp.parentNode;
                if (pp.nodeName.toUpperCase() == "P") {
                    if (processP(pp)) {
                        processedCount++;
                    }
                }
            }
            
            if (ptr.nodeType == 1 && ptr.nodeName == MyTagName && ptr.getAttribute("id") == endId) {
                insideSelection = false;
                processingEnded = true;
                var BlockEndNode = ptr;
            }
            
            if (insideSelection && ptr.nodeName == "P") {
                if (processP(ptr)) {
                    processedCount++;
                }
            }
            
            var nextPtr;
            if (ptr.firstChild != null) {
                nextPtr = ptr.firstChild;
            } else {
                nextPtr = ptr;
                while (nextPtr && nextPtr != body && !nextPtr.nextSibling) {
                    nextPtr = nextPtr.parentNode;
                }
                if (nextPtr && nextPtr != body) nextPtr = nextPtr.nextSibling;
                else nextPtr = null;
            }
            if (!nextPtr) break;
            ptr = nextPtr;
        }
        
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
    } else {
        // Обработка всех абзацев
        var paragraphs = body.getElementsByTagName("P");
        for (var i = 0; i < paragraphs.length; i++) {
            if (processP(paragraphs[i])) {
                processedCount++;
            }
        }
    }
    
    window.external.EndUndoUnit(document);
    
    if (processedCount > 0) {
        alert("Обработано абзацев: " + processedCount);
    } else {
        alert("Не найдено абзацев для обработки.");
    }
}
