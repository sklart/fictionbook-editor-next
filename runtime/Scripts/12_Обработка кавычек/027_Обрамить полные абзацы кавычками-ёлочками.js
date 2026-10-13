/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Обрамить полные абзацы кавычками-ёлочками",
  "version": "1.0",
  "authors": [
    {
      "name": "TaKir",
      "role": "author"
    },
    {
      "name": "TaKir",
      "role": "contributor",
      "contribution": "idea"
    },
    {
      "name": "DeepSeek",
      "role": "ai_assistance"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Скрипт предназначен для обрамления кавычками-ёлочками выделенных полных абзацев текста в fb2 документах.",
  "description": "Скрипт предназначен для обрамления кавычками-ёлочками выделенных полных абзацев текста в fb2 документах.",
  "scope": "Выделенный диапазон либо полные абзацы, которые он пересекает; см. описание варианта.",
  "modifiesDocument": true,
  "requiresSelection": true,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Подготовьте выделение, описанное в scope и условиях запуска; некоторые команды работают с целыми абзацами.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже.",
    "Просмотрите результат до сохранения. Для структурной правки проверьте книгу и связанные ссылки."
  ],
  "warnings": [
    "Скрипт содержит операции изменения документа. Перед массовой обработкой сохраните отдельную копию книги."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "a2f32ec880ea07937057312e8305e01c6cbbb3cdaeb8dbbd442527ecc9e0bbf9",
    "reviewStatus": "generated"
  },
  "tags": [
    "Обработка кавычек"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Обрамить полные абзацы кавычками-ёлочками" для редактора FBE
// version 1.0
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для обрамления кавычками-ёлочками выделенных полных абзацев текста в fb2 документах.
// Выделение абзаца(ев) может быть полным или частичным.
// При выделении нескольких абзацев, открывающая кавычка ставится в начале первого,
// а закрывающая - в конце последнего абзаца.
// Скрипт расставляет кавычки без оглядки на типы размеченных элементов - заголовки, цитаты, стихи, эпиграфы,
// и на то, что начало и конец выделения могут быть в разных секциях.
// По умолчанию скрипт работает в тихом режиме.
// Поддержка отмены действий (Ctrl+Z).

// version 1.0, 07.07.2026
//======================================

function Run() {
    var scriptName = "Обрамить полные абзацы кавычками-ёлочками";
    var version = "1.0";

    var nbspChar = String.fromCharCode(160);
    try {
        var tmpNbsp = window.external.GetNBSP();
        if (tmpNbsp.charCodeAt(0) != 160) {
            nbspChar = tmpNbsp;
        }
    } catch(e) {}

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

    if (document.selection.type == "Control") {
        return;
    }

    window.external.BeginUndoUnit(document, "Обрамить абзацы кавычками-ёлочками");

    // ==================================================
    // Находим первый и последний P в выделении
    // ==================================================
    
    // Получаем диапазон выделения
    var rangeStart = tr.duplicate();
    rangeStart.collapse(true);
    var rangeEnd = tr.duplicate();
    rangeEnd.collapse(false);

    // Находим элемент в начале выделения и поднимаемся до P
    var startP = rangeStart.parentElement();
    while (startP && startP.nodeName != "P" && startP != fbw_body) {
        startP = startP.parentNode;
    }

    // Находим элемент в конце выделения и поднимаемся до P
    var endP = rangeEnd.parentElement();
    while (endP && endP.nodeName != "P" && endP != fbw_body) {
        endP = endP.parentNode;
    }

    if (!startP || !endP || startP.nodeName != "P" || endP.nodeName != "P") {
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка: не удалось найти абзацы.");
        window.external.EndUndoUnit(document);
        return;
    }

    // ==================================================
    // Находим первый текстовый узел в startP
    // ==================================================
    var startTextNode = null;
    var child = startP.firstChild;
    while (child) {
        if (child.nodeType == 3) {
            startTextNode = child;
            break;
        }
        if (child.nodeType == 1) {
            var deep = child.firstChild;
            while (deep) {
                if (deep.nodeType == 3) {
                    startTextNode = deep;
                    break;
                }
                if (deep.firstChild) {
                    deep = deep.firstChild;
                } else if (deep.nextSibling) {
                    deep = deep.nextSibling;
                } else {
                    while (deep && deep.parentNode && deep.parentNode != child && !deep.nextSibling) {
                        deep = deep.parentNode;
                    }
                    if (!deep || deep.parentNode == child) break;
                    deep = deep.nextSibling;
                }
            }
        }
        if (startTextNode) break;
        child = child.nextSibling;
    }

    // ==================================================
    // Находим последний текстовый узел в endP
    // ==================================================
    var endTextNode = null;
    child = endP.lastChild;
    while (child) {
        if (child.nodeType == 3) {
            endTextNode = child;
            break;
        }
        if (child.nodeType == 1) {
            var deep = child.lastChild;
            while (deep) {
                if (deep.nodeType == 3) {
                    endTextNode = deep;
                    break;
                }
                if (deep.lastChild) {
                    deep = deep.lastChild;
                } else if (deep.previousSibling) {
                    deep = deep.previousSibling;
                } else {
                    while (deep && deep.parentNode && deep.parentNode != child && !deep.previousSibling) {
                        deep = deep.parentNode;
                    }
                    if (!deep || deep.parentNode == child) break;
                    deep = deep.previousSibling;
                }
            }
        }
        if (endTextNode) break;
        child = child.previousSibling;
    }

    if (!startTextNode || !endTextNode) {
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка: не найдены текстовые узлы.");
        window.external.EndUndoUnit(document);
        return;
    }

    // ==================================================
    // Вставка кавычек
    // ==================================================
    var s = startTextNode.nodeValue;
    startTextNode.nodeValue = "\u00AB" + s;

    if (startTextNode == endTextNode) {
        s = startTextNode.nodeValue;
        endTextNode.nodeValue = s + "\u00BB";
    } else {
        s = endTextNode.nodeValue;
        endTextNode.nodeValue = s + "\u00BB";
    }

    window.external.EndUndoUnit(document);
}
