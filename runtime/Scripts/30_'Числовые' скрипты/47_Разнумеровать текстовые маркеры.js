/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Разнумеровать текстовые маркеры",
  "version": "1.1",
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
  "summary": "Скрипт предназначен для снятия нумерации с текстовых маркеров в fb2 документах.",
  "description": "Скрипт предназначен для снятия нумерации с текстовых маркеров в fb2 документах.",
  "scope": "Область определяется условиями и обходом DOM в данном скрипте.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Проверьте перечисленные параметры: часть задаётся в исходном JS, часть — через общий диалог настроек.",
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
    "sha256": "a0e806354f78972d930ec558044bdde4e8d838ae5ad40f107ed661d505588f00",
    "reviewStatus": "generated"
  },
  "tags": [
    "'Числовые' скрипты",
    "параметры"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
  },
  "settings": [
    {
      "name": "markerType",
      "value": "zzz_pic",
      "expression": "\"zzz_pic\"",
      "description": "Тип маркера (без подчеркивания и номера)",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "showStatistics",
      "value": 1,
      "expression": "1",
      "description": "Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "processNotesSection",
      "value": 0,
      "expression": "0",
      "description": "0 - нет, 1 - да",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "processCommentsSection",
      "value": 0,
      "expression": "0",
      "description": "0 - нет, 1 - да",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "nbspEntity",
      "value": "&nbsp;",
      "expression": "\"&nbsp;\"",
      "description": "Получаем неразрывный пробел из настроек FBE",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    }
  ]
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Разнумеровать текстовые маркеры" для редактора FBE
// version 1.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для снятия нумерации с текстовых маркеров в fb2 документах.
// Текстовый маркер может быть любым по выбору пользователя.
// По умолчанию используется маркер zzz_pic.
// Маркер должен быть расположен в отдельном абзаце, без другого текста.
// Скрипт находит все маркеры заданного типа с нумерацией и удаляет номер, оставляя чистый маркер.
// Поддерживаются оба формата нумерации: marker_001 и marker001.
// По умолчанию обрабатывается только основной раздел документа, без сносок и комментариев.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.1, 02.07.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Разнумеровать текстовые маркеры";
    var version = "1.1";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    // Тип маркера (без подчеркивания и номера)
    var markerType = "zzz_pic";

    // Настройка: 1 - показывать статистику, 0 - не показывать (тихий режим)
    var showStatistics = 1;

    // Обрабатывать раздел сносок (примечаний)
    var processNotesSection = 0; // 0 - нет, 1 - да

    // Обрабатывать раздел комментариев
    var processCommentsSection = 0; // 0 - нет, 1 - да

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    // Включаем поддержку отмены
    window.external.BeginUndoUnit(document, scriptName);

    // Получаем неразрывный пробел из настроек FBE
    var nbspEntity = "&nbsp;";
    var nbspChar = String.fromCharCode(160);
    try {
        var nbspCharTemp = window.external.GetNBSP();
        if (nbspCharTemp.charCodeAt(0) != 160) {
            nbspEntity = nbspCharTemp;
        }
        nbspChar = nbspCharTemp;
    } catch (e) {
        nbspChar = String.fromCharCode(160);
    }

    // Необычные пробелы для нормализации
    var unusualSpaces = String.fromCharCode(160) +
        String.fromCharCode(8194) +
        String.fromCharCode(8195) +
        String.fromCharCode(8196) +
        String.fromCharCode(8197) +
        String.fromCharCode(8198) +
        String.fromCharCode(8239) +
        String.fromCharCode(8201) +
        String.fromCharCode(8202) +
        nbspChar;

    // ==================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // ==================================================

    // Нормализация пробелов в строке
    function normalizeSpaces(str) {
        var result = "";
        for (var i = 0; i < str.length; i++) {
            var ch = str.charAt(i);
            var isUnusual = false;
            for (var j = 0; j < unusualSpaces.length; j++) {
                if (ch == unusualSpaces.charAt(j)) {
                    isUnusual = true;
                    break;
                }
            }
            if (isUnusual) {
                result += " ";
            } else {
                result += ch;
            }
        }
        return result;
    }

    // Удаление пробелов с обоих концов строки
    function trimStr(str) {
        return str.replace(/^\s+|\s+$/g, '');
    }

    // Проверка, является ли строка маркером с нумерацией (с подчеркиванием или без)
    function isNumberedMarker(str, marker) {
        var pattern = new RegExp("^" + marker + "_?\\d{3,}$");
        return pattern.test(str);
    }

    // Получение всех элементов P в документе
    function getAllParagraphsInBody(bodyDiv) {
        var paragraphs = [];
        var allP = bodyDiv.getElementsByTagName("P");
        for (var i = 0; i < allP.length; i++) {
            paragraphs.push(allP[i]);
        }
        return paragraphs;
    }

    // ==================================================
    // СБОР ДАННЫХ — Фаза 1: ТОЛЬКО чтение
    // ==================================================

    var allParagraphs = [];
    var toUnnumber = [];

    // Находим все body DIV'ы
    var allDivs = document.getElementsByTagName("DIV");
    var bodyDivs = [];
    for (var i = 0; i < allDivs.length; i++) {
        if (allDivs[i].className == "body") {
            bodyDivs.push(allDivs[i]);
        }
    }

    for (var b = 0; b < bodyDivs.length; b++) {
        var bodyDiv = bodyDivs[b];
        var fbname = bodyDiv.getAttribute("fbname") || "";

        // Проверяем, нужно ли обрабатывать этот раздел
        if (fbname == "") {
            // Основной раздел — всегда обрабатываем
        } else if (fbname == "notes" && processNotesSection) {
            // Сноски — обрабатываем если разрешено
        } else if (fbname == "comments" && processCommentsSection) {
            // Комментарии — обрабатываем если разрешено
        } else {
            continue; // Пропускаем этот раздел
        }

        var paragraphs = getAllParagraphsInBody(bodyDiv);
        allParagraphs = allParagraphs.concat(paragraphs);
    }

    // Анализируем каждый абзац
    for (var p = 0; p < allParagraphs.length; p++) {
        var para = allParagraphs[p];
        var text = para.innerText || para.textContent || "";

        // Нормализуем пробелы
        var normalized = normalizeSpaces(text);
        var trimmed = trimStr(normalized);

        if (trimmed.length === 0) continue;

        // Нас интересуют только маркеры с нумерацией (с _ или без)
        if (isNumberedMarker(trimmed, markerType)) {
            toUnnumber.push(para);
        }
    }

    // ==================================================
    // ПРОВЕРКА: маркеры с нумерацией отсутствуют
    // ==================================================
    if (toUnnumber.length === 0) {
        var noMarkersMsg = scriptName + "\n";
        noMarkersMsg += "ver. " + version + "\n\n";
        noMarkersMsg += "Заданные маркеры \"" + markerType + "\" с нумерацией отсутствуют.";

        MsgBox(noMarkersMsg, "FBE скрипт");
        window.external.EndUndoUnit(document);
        return;
    }

    // ==================================================
    // ЗАПРОС ПОДТВЕРЖДЕНИЯ (только в обычном режиме)
    // ==================================================
    if (showStatistics) {
        var confirmMsg = scriptName + "\n";
        confirmMsg += "ver. " + version + "\n\n";
        confirmMsg += "Снять нумерацию с маркеров \"" + markerType + "\"?\n";
        confirmMsg += "Будет обработано: " + toUnnumber.length + " шт.\n";
        confirmMsg += "Пример: " + markerType + "_001 → " + markerType + "\n";
        confirmMsg += "Пример: " + markerType + "001 → " + markerType;

        if (!AskYesNo(confirmMsg)) {
            window.external.EndUndoUnit(document);
            return;
        }
    }

    // Запускаем таймер после подтверждения
    var startTime = new Date();

    // ==================================================
    // Фаза 2: ТОЛЬКО запись (разнумерация)
    // ==================================================

    var totalUnnumbered = toUnnumber.length;

    for (var m = 0; m < totalUnnumbered; m++) {
        var para = toUnnumber[m];

        // Очищаем абзац и вставляем только markerType
        while (para.firstChild) {
            para.removeChild(para.firstChild);
        }
        para.appendChild(document.createTextNode(markerType));
    }

    // Завершаем отмену
    window.external.EndUndoUnit(document);

    // ==================================================
    // СТАТИСТИКА
    // ==================================================

    if (showStatistics) {
        var endTime = new Date();
        var elapsed = (endTime.getTime() - startTime.getTime()) / 1000;
        var elapsedStr = elapsed.toFixed(3).replace(".", ",");

        var msg = scriptName + "\n";
        msg += "ver. " + version + "\n\n";
        msg += "Статистика разнумерации маркеров:\n\n";
        msg += "\u2713 Тип маркера: " + markerType + "\n";
        msg += "\u2713 Разнумеровано: " + totalUnnumbered + " шт.\n\n";
        msg += "Время выполнения: " + elapsedStr + " сек.";

        MsgBox(msg, "FBE скрипт");
    }
}
