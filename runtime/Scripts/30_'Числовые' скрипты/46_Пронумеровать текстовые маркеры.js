/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Пронумеровать текстовые маркеры",
  "version": "1.2",
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
  "summary": "Нумерует отдельные абзацы с маркером zzz_pic.",
  "description": "Ищет отдельные абзацы, чей нормализованный текст равен zzz_pic, и заменяет их на zzz_pic_001, zzz_pic_002 и далее. Если в области найдётся уже нумерованный маркер, отменяет всю операцию. По умолчанию разделы примечаний и комментариев пропускаются.",
  "scope": "Абзацы P в основных body без имени fbname; notes и comments по умолчанию исключены. Положение курсора и выделение не учитываются.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Проверьте, что нужные маркеры занимают отдельные абзацы и имеют выбранный тип zzz_pic.",
    "При необходимости измените параметры обработки примечаний и комментариев в исходнике.",
    "Запустите команду, подтвердите действие в сообщении и проверьте результат по всей книге."
  ],
  "warnings": [
    "Если найден хотя бы один уже нумерованный маркер вида zzz_pic_001, новые номера не присваиваются.",
    "Содержимое совпавшего абзаца заменяется новым текстовым узлом; прежнее оформление внутри маркера теряется."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "d3bcb2ff9f84cdfea2bbba762651182cffb9756d3a82a121b4d969766dca35ae",
    "reviewStatus": "generated"
  },
  "tags": [
    "'Числовые' скрипты",
    "параметры"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Штатные выходы, включая отказ от подтверждения, закрывают группу отмены. При ошибке во время замены абзацев полный откат не проверен."
  },
  "settings": [
    {
      "name": "markerType",
      "title": "Тип маркера",
      "value": "zzz_pic",
      "expression": "\"zzz_pic\"",
      "description": "Текст маркера без номера; по умолчанию zzz_pic.",
      "source": "script"
    },
    {
      "name": "showStatistics",
      "title": "Подтверждение и статистика",
      "value": 1,
      "expression": "1",
      "description": "1 — спрашивать подтверждение и показывать итог; 0 — тихий режим.",
      "source": "script"
    },
    {
      "name": "processNotesSection",
      "title": "Обрабатывать примечания",
      "value": 0,
      "expression": "0",
      "description": "1 — включить body примечаний; 0 — пропустить.",
      "source": "script"
    },
    {
      "name": "processCommentsSection",
      "title": "Обрабатывать комментарии",
      "value": 0,
      "expression": "0",
      "description": "1 — включить body комментариев; 0 — пропустить.",
      "source": "script"
    }
  ]
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Пронумеровать текстовые маркеры" для редактора FBE
// version 1.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для автоматической сквозной нумерации текстовых маркеров в fb2 документах.
// Текстовый маркер может быть любым по выбору пользователя.
// По умолчанию используется маркер  zzz_pic.
// Маркер должен быть расположен в отдельном абзаце, без другого текста.
// Скрипт находит все маркеры заданного типа и добавляет к ним порядковый номер.
// Формат нумерации: marker_001, marker_002... (минимум 3 цифры, при >999 маркеров — 4 цифры).
// Маркеры с уже существующей нумерацией не обрабатываются — скрипт сообщит о них и отменит операцию.
// По умолчанию обрабатывается только основной раздел документа, без сносок и комментариев.
// Режим работы: обычный или тихий.
// Поддержка отмены действий (Ctrl+Z).

// version 1.2, 02.07.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Пронумеровать текстовые маркеры";
    var version = "1.2";

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

    // Проверка, является ли строка маркером с нумерацией
    function isNumberedMarker(str, marker) {
        var pattern = new RegExp("^" + marker + "_\\d{3,}$");
        return pattern.test(str);
    }

    // Проверка, является ли строка маркером без нумерации
    function isPlainMarker(str, marker) {
        return str === marker;
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
    var alreadyNumbered = 0;
    var toNumber = [];

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

        // Проверяем, это маркер с номером или без
        if (isNumberedMarker(trimmed, markerType)) {
            alreadyNumbered++;
        } else if (isPlainMarker(trimmed, markerType)) {
            toNumber.push(para);
        }
    }

    // ==================================================
    // ПРОВЕРКА: маркеры отсутствуют
    // ==================================================
    if (toNumber.length === 0 && alreadyNumbered === 0) {
        var noMarkersMsg = scriptName + "\n";
        noMarkersMsg += "ver. " + version + "\n\n";
        noMarkersMsg += "Заданные маркеры \"" + markerType + "\" отсутствуют.";

        MsgBox(noMarkersMsg, "FBE скрипт");
        window.external.EndUndoUnit(document);
        return;
    }

    // ==================================================
    // ПРОВЕРКА: если есть уже нумерованные — сообщаем и выходим
    // ==================================================
    if (alreadyNumbered > 0) {
        var alreadyMsg = scriptName + "\n";
        alreadyMsg += "ver. " + version + "\n\n";
        alreadyMsg += "В документе найдены уже нумерованные маркеры \"" + markerType + "\": " + alreadyNumbered + " шт.\n\n";
        alreadyMsg += "Операция отменена.";

        MsgBox(alreadyMsg, "FBE скрипт");
        window.external.EndUndoUnit(document);
        return;
    }

    // ==================================================
    // ЗАПРОС ПОДТВЕРЖДЕНИЯ (только в обычном режиме)
    // ==================================================
    if (showStatistics) {
        var confirmMsg = scriptName + "\n";
        confirmMsg += "ver. " + version + "\n\n";
        confirmMsg += "Пронумеровать маркеры \"" + markerType + "\"?\n";
        confirmMsg += "Маркеры должны быть в отдельных абзацах.";

        if (!AskYesNo(confirmMsg)) {
            window.external.EndUndoUnit(document);
            return;
        }
    }

    // Запускаем таймер после подтверждения
    var startTime = new Date();

    // ==================================================
    // Фаза 2: ТОЛЬКО запись (нумерация)
    // ==================================================

    // Определяем количество цифр
    var totalMarkers = toNumber.length;
    var digitCount = 3;
    if (totalMarkers > 999) {
        digitCount = 4;
    }

    for (var m = 0; m < totalMarkers; m++) {
        var para = toNumber[m];
        var num = m + 1;
        var numStr = String(num);
        while (numStr.length < digitCount) {
            numStr = "0" + numStr;
        }
        var newText = markerType + "_" + numStr;

        // Очищаем абзац и вставляем новый текст
        while (para.firstChild) {
            para.removeChild(para.firstChild);
        }
        para.appendChild(document.createTextNode(newText));
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
        msg += "Статистика нумерации маркеров:\n\n";
        msg += "\u2713 Тип маркера: " + markerType + "\n";
        msg += "\u2713 Пронумеровано: " + totalMarkers + " шт.\n";
        msg += "  \u2022 Формат номера: " + digitCount + " цифр (" + markerType + "_" + numStr.substring(0, digitCount) + "...)\n\n";
        msg += "Время выполнения: " + elapsedStr + " сек.";

        MsgBox(msg, "FBE скрипт");
    }
}
