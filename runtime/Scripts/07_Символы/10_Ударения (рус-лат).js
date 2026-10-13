/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Ударения (рус-лат)",
  "version": "2.0",
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
      "name": "TaKir",
      "role": "original_author"
    },
    {
      "name": "DeepSeek",
      "role": "ai_assistance"
    },
    {
      "name": "Sclex",
      "role": "original_author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Открывает таблицу «Ударения (рус-лат)» для вставки символов в текст.",
  "description": "Немодальное HTML-окно получает документ и окно FBE. Нажатие на ячейку формирует код вставки через диапазон/pasteHTML в исходном окне и группирует его в Undo. Открытие таблицы само по себе текст не меняет; нажатие на символ — меняет.",
  "scope": "Область определяется обходом DOM и условиями запуска; подробности в правилах и функциях.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Проверьте перечисленные параметры: часть задаётся в исходном JS, часть — через общий диалог настроек.",
    "Сохраняйте структуру Scripts/HTML и связанные файлы; один JS может быть недостаточен для работы.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже.",
    "Просмотрите результат до сохранения. Для структурной правки проверьте книгу и связанные ссылки."
  ],
  "warnings": [
    "Скрипт предусматривает изменение книги. Перед массовой обработкой сохраните отдельную копию.",
    "Вставка выполняется HTML-зависимостью через eval в окне FBE; короткий файл запуска не содержит самих DOM-записей."
  ],
  "dependencies": [
    {
      "path": "HTML/Ударения рус-лат.html",
      "sha256": "68244d99be6733ad88ce948614ed3bc866eb8b9ce1e41d07be5b75e12d83d7f6",
      "optional": true
    }
  ],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "3dd9a3313e8e92c7dec2b34a1e56be5c9933625c0b871037b13beba7b70f8069",
    "reviewStatus": "generated"
  },
  "tags": [
    "Символы",
    "параметры"
  ],
  "undo": {
    "status": "may_be_delegated",
    "description": "Прямых Undo-вызовов нет; действия могут выполняться связанным HTML/JS. Гарантия отмены не установлена."
  },
  "settings": [
    {
      "name": "windowPosition",
      "value": 6,
      "expression": "6",
      "description": "по умолчанию - флаг 6",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "dialogWidthPx",
      "value": 440,
      "expression": "440",
      "description": "ширина окна, px",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "dialogHeightPx",
      "value": 225,
      "expression": "225",
      "description": "высота окна, px",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "edgeMargin",
      "value": 5,
      "expression": "5",
      "description": "Отступ окна от края монитора (для флагов 0 и 4), px",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "scrollbarWidth",
      "value": 35,
      "expression": "35",
      "description": "зависит от системной темы и масштаба",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "dividerWidth",
      "value": 35,
      "expression": "35",
      "description": "Зависит от системного масштаба (100%, 125%, 150%, 200%)",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "scrLeft",
      "value": 0,
      "expression": "0",
      "description": "Границы рабочей области монитора",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "scrTop",
      "value": 0,
      "expression": "0",
      "description": "Границы рабочей области монитора",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "posParams",
      "value": "",
      "expression": "\"\"",
      "description": "Строка параметров позиционирования для showModelessDialog",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "offX",
      "value": 0,
      "expression": "0",
      "description": "Индивидуальные сдвиги для текущего флага",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "offY",
      "value": 0,
      "expression": "0",
      "description": "Индивидуальные сдвиги для текущего флага",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    }
  ]
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Ударения (рус-лат)" для редактора FBE
// version 2.0
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для вставки в текст fb2 документа русских и латинских гласных,
// над которыми стоит знак ударения.
// Клик по букве в табличке вставляет эту букву с ударением в текст книги в позицию курсора.
// Работает только с текстом внутри fbw_body (основной текст книги).
// Позиция окна таблички настраивается - 7 вариантов расположения.
// Поддержка отмены действий (Ctrl+Z).

// Основано на скриптах серии "Символы"
//        jurgennt™, май 2008 г.
//        script engene by Sclex v.1.3 с доработкой TaKir + DeepSeek
//        таблица: TaKir, сентябрь 2026

// version 2.0, 20.09.2026
//======================================

function Run() {
    var scriptName = "Ударения (рус-лат)";
    var version = "2.0";

    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================

    // --------------------------------------------------
    // 1. ГДЕ ОТКРЫВАТЬ ОКНО ТАБЛИЧКИ
    // --------------------------------------------------
    // Выберите один из 7 вариантов расположения окна:
    //
    //   0 - у левой границы монитора
    //   1 - по центру экрана
    //   2 - слева от текстового поля (отступ задаётся вручную)
    //   3 - справа от текстового поля (с учётом полосы прокрутки)
    //   4 - у правой границы монитора
    //   5 - слева от текстового поля, окно прижато к НИЗУ текста
    //   6 - слева от текстового поля, окно прижато к ВЕРХУ текста
    //
    // Флаги 5 и 6 не зависят от ширины панели структуры документа,
    // поэтому при её изменении окно остаётся на месте.
    //
    var windowPosition = 6;   // по умолчанию - флаг 6

    // --------------------------------------------------
    // 2. РАЗМЕР ОКНА ТАБЛИЧКИ
    // --------------------------------------------------
    // Если в табличке много пустого места снизу - уменьшите высоту.
    // Если не помещаются все буквы - увеличьте ширину.
    //
    var dialogWidthPx  = 440;   // ширина окна, px
    var dialogHeightPx = 225;   // высота окна, px

    // --------------------------------------------------
    // 3. ТЕХНИЧЕСКИЕ ПАРАМЕТРЫ (обычно менять не нужно)
    // --------------------------------------------------

    // Отступ окна от края монитора (для флагов 0 и 4), px
    var edgeMargin = 5;

    // Ширина вертикальной полосы прокрутки (для флага 3), px
    // зависит от системной темы и масштаба
    var scrollbarWidth = 35;

    // Толщина разделителя между панелью структуры и текстовым полем
    // (для флагов 5 и 6), px
    // Зависит от системного масштаба (100%, 125%, 150%, 200%)
    var dividerWidth = 35;

    // --------------------------------------------------
    // 4. ТОНКАЯ НАСТРОЙКА ПОЛОЖЕНИЯ ОКНА (можно менять)
    // --------------------------------------------------
    // Если окно при выбранном флаге встало не совсем туда, куда хочется,
    // его можно немного подвинуть. Для каждого флага (0..6) есть свой
    // сдвиг по горизонтали (X) и по вертикали (Y).
    //
    //   offsetX:  "+" - сдвинуть вправо,  "-" - сдвинуть влево
    //   offsetY:  "+" - сдвинуть вниз,    "-" - сдвинуть вверх
    //
    // Индекс массива = номер флага. Например, posOffsetX[6] - сдвиг
    // по горизонтали для флага 6.
    //
    // Флаг 1 (центр экрана) сдвиги не использует - там штатная центровка.
    //
    // Пример: если при флаге 6 окно стоит слишком высоко, поставьте
    // posOffsetY[6] = 20 - окно опустится на 20 пикселей.
    //
    // Массивы:       флаг 0   1    2     3    4    5     6
    var posOffsetX = [   0,   0,  -300,   0,   0,   0,    0 ];
    var posOffsetY = [   0,   0,     0,   0,   0,  -90,  140 ];

    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================

    var dialogWidth  = dialogWidthPx  + "px";
    var dialogHeight = dialogHeightPx + "px";

    // Позиция текстового поля редактора (IE-контрола) на экране
    var edLeft   = window.screenLeft;
    var edTop    = window.screenTop;
    var edWidth  = document.body.clientWidth;

    // Высота видимой области текстового поля.
    // ВАЖНО: document.body.clientHeight здесь НЕ подходит - на реальном
    // документе он равен высоте всего текста (сотни тысяч пикселей).
    // document.documentElement.clientHeight - это именно высота видимой
    // области, она меняется при изменении размера окна редактора.
    var edHeight = document.documentElement.clientHeight;

    // Границы рабочей области монитора
    var scrLeft   = 0;
    var scrTop    = 0;
    var scrRight  = screen.availWidth  - dialogWidthPx;
    var scrBottom = screen.availHeight - dialogHeightPx;

    // Итоговые координаты окна
    var posLeft = 0;
    var posTop  = edTop;

    // Строка параметров позиционирования для showModelessDialog
    var posParams = "";

    // Индивидуальные сдвиги для текущего флага
    var offX = 0;
    var offY = 0;
    if (windowPosition >= 0 && windowPosition <= 6) {
        offX = posOffsetX[windowPosition];
        offY = posOffsetY[windowPosition];
    }

    if (windowPosition == 0) {
        // У левой границы монитора
        posLeft = edgeMargin + offX;
        posTop  = edTop + offY;
        if (posLeft < scrLeft)   posLeft = scrLeft + edgeMargin;
        if (posTop  < scrTop)    posTop  = scrTop  + edgeMargin;
        if (posTop  > scrBottom) posTop  = scrBottom;
        posParams = "dialogLeft: " + posLeft + "px; " +
                    "dialogTop: "  + posTop  + "px; ";
    }
    else if (windowPosition == 1) {
        // По центру экрана - штатный флаг center (без сдвигов)
        posParams = "center: Yes; ";
    }
    else if (windowPosition == 2) {
        // Слева от текстового поля редактора
        posLeft = edLeft - dialogWidthPx + offX;
        posTop  = edTop  + offY;
        if (posLeft < scrLeft)   posLeft = scrLeft + edgeMargin;
        if (posTop  < scrTop)    posTop  = scrTop  + edgeMargin;
        if (posTop  > scrBottom) posTop  = scrBottom;
        posParams = "dialogLeft: " + posLeft + "px; " +
                    "dialogTop: "  + posTop  + "px; ";
    }
    else if (windowPosition == 3) {
        // Справа от текстового поля (с учётом полосы прокрутки)
        posLeft = edLeft + edWidth + scrollbarWidth + offX;
        posTop  = edTop  + offY;
        if (posLeft > scrRight)  posLeft = scrRight - edgeMargin;
        if (posTop  < scrTop)    posTop  = scrTop   + edgeMargin;
        if (posTop  > scrBottom) posTop  = scrBottom;
        posParams = "dialogLeft: " + posLeft + "px; " +
                    "dialogTop: "  + posTop  + "px; ";
    }
    else if (windowPosition == 4) {
        // У правой границы монитора
        posLeft = scrRight - edgeMargin + offX;
        posTop  = edTop + offY;
        if (posLeft < scrLeft)   posLeft = scrLeft + edgeMargin;
        if (posLeft > scrRight)  posLeft = scrRight - edgeMargin;
        if (posTop  < scrTop)    posTop  = scrTop  + edgeMargin;
        if (posTop  > scrBottom) posTop  = scrBottom;
        posParams = "dialogLeft: " + posLeft + "px; " +
                    "dialogTop: "  + posTop  + "px; ";
    }
    else if (windowPosition == 5) {
        // Слева от текстового поля: правый край окна у разделителя,
        // всё окно левее текстового поля, низ окна = низ видимой области
        posLeft = edLeft - dialogWidthPx - dividerWidth + offX;

        // Привязка по вертикали: низ окна = низ видимой области текста.
        // Если видимая область ниже высоты окна - привязка к верху.
        if (edHeight >= dialogHeightPx) {
            posTop = edTop + edHeight - dialogHeightPx + offY;
        } else {
            posTop = edTop + offY;
        }

        if (posTop  < scrTop)    posTop  = scrTop  + edgeMargin;
        if (posTop  > scrBottom) posTop  = scrBottom;
        posParams = "dialogLeft: " + posLeft + "px; " +
                    "dialogTop: "  + posTop  + "px; ";
    }
    else if (windowPosition == 6) {
        // Слева от текстового поля: правый край окна у разделителя,
        // всё окно левее текстового поля, верх окна = верх видимой области
        posLeft = edLeft - dialogWidthPx - dividerWidth + offX;
        posTop  = edTop + offY;

        if (posTop  < scrTop)    posTop  = scrTop  + edgeMargin;
        if (posTop  > scrBottom) posTop  = scrBottom;
        posParams = "dialogLeft: " + posLeft + "px; " +
                    "dialogTop: "  + posTop  + "px; ";
    }
    else {
        // На всякий случай - если флаг задан неверно, ставим по центру
        posParams = "center: Yes; ";
    }

    var params = new Object();
    params["fbw_body"] = document.getElementById("fbw_body");
    params["document"] = document;
    params["window"]   = window;

    var rslt = window.showModelessDialog(
        "HTML/Ударения рус-лат.html",
        params,
        "dialogHeight: " + dialogHeight + "; " +
        "dialogWidth: "  + dialogWidth  + "; " +
        posParams +
        "help: No; resizable: Yes; status: No; scroll: No;"
    );
}