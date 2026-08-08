// Скрипт "Показать HTML-карту текущей секции" для редактора FBE
// version 2.3
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для отображения HTML-структуры текущей секции fb2 документа.
// Показывает иерархию тегов, статистику элементов и форматирование.
// Выводит код секции с подсветкой тегов и атрибутов разными цветами.
// Отображается статистика всех блочных и абзацных элементов, ссылок и форматирования.
// Скрипт позволяет просматривать полный или сокращенный текст элементов секции.
// Вывод возможен в отдельном IE-окне или стандартном окне FBE.
// Поддерживает копирование кода секции (требуется ручное выделение).
// Настройки скрипта легко изменяются в начале кода.

// version 2.3, 27.01.2026
//======================================

function Run() {
    // Название и версия для сообщений
    var scriptName = "Показать HTML-карту текущей секции";
    var version = "2.3";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Режим вывода:
    // 1 - показать в IE окне (с подсветкой)
    // 0 - показать в стандартном окне FBE
    var SHOW_IN_IE_WINDOW = 1;
    
    // Режим текста:
    // 1 - сокращенный текст (для быстрого просмотра структуры)
    // 0 - полный текст (для детального анализа)
    var SHOW_SHORT_TEXT = 1;
    
    // Параметры сокращения текста:
    var MAX_TEXT_LENGTH = 40; // Максимальная длина текста для отображения (символов)
    var MAX_LAST_PARAGRAPH_LENGTH = 60; // Макс. длина последнего абзаца в секции
    var MAX_CONSECUTIVE_FULL_PARAGRAPHS = 2; // Макс. количество полных абзацев подряд
    
    // Режим окон IE:
    var UPDATE_EXISTING_WINDOW = 1; // 1 - обновлять существующее окно, 0 - всегда новое
    var MAX_WINDOWS = 5; // Максимальное количество открытых окон
    
    // Настройки отображения в IE окне:
    var IE_WINDOW_WIDTH = 900; // Ширина окна в пикселях
    var IE_WINDOW_HEIGHT = 700; // Высота окна в пикселях
    
    // Цвета подсветки (можно менять):
    var COLOR_TAG = "#0000FF"; // Синий для тегов
    var COLOR_ATTR_NAME = "#FF4500"; // Оранжевый для имен атрибутов
    var COLOR_ATTR_VALUE = "#008000"; // Зеленый для значений атрибутов
    var COLOR_LINK = "#8B008B"; // Темно-пурпурный для ссылок
    var COLOR_TEXT = "#000000"; // Черный для текста
    var COLOR_STRIKE = "#FF0000"; // Красный для strike
    var COLOR_BUTTON = "#4A90E2"; // Голубой для кнопки
    var COLOR_BUTTON_HOVER = "#357AE8"; // Темно-голубой при наведении
    var COLOR_SUCCESS = "#4CAF50"; // Зеленый для успеха
    var COLOR_INSTRUCTION = "#FF0000"; // Красный для инструкции
    var COLOR_WARNING = "#FF4500"; // Оранжевый для предупреждения
    
    // Дополнительные настройки:
    var SHOW_EMPTY_PARAGRAPHS = false; // Не показывать пустые абзацы
    var SHOW_FULL_LINKS = true; // Показывать полные ссылки без сокращения
    var COMPACT_MODE = true; // Компактный режим - теги в одной строке
    
    // ==================================================
    // ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ (не менять)
    // ==================================================
    window._htmlMapWindows = window._htmlMapWindows || [];
    window._htmlMapWindowIndex = window._htmlMapWindowIndex || 0;
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    // Таймер для измерения времени выполнения
    var startTime = new Date().getTime();
    
    try {
        // Получаем текущий выделенный диапазон
        var sel = document.selection;
        if (!sel) {
            showMessage(scriptName, version, "Не удалось получить выделение документа.");
            return;
        }
        
        var range = sel.createRange();
        if (!range) {
            showMessage(scriptName, version, "Не удалось создать диапазон.");
            return;
        }
        
        // Находим родительский элемент (где находится курсор/выделение)
        var parentElement = range.parentElement();
        if (!parentElement) {
            showMessage(scriptName, version, "Не удалось определить текущий элемент.");
            return;
        }
        
        // Ищем ближайшую секцию (DIV class='section')
        var currentSection = findParentSection(parentElement);
        
        if (!currentSection) {
            showMessage(scriptName, version, 
                "Текущий элемент не находится внутри секции (DIV class='section').\n" +
                "Переместите курсор внутрь текста секции и попробуйте снова.");
            return;
        }
        
        // Собираем статистику
        var stats = collectStatistics(currentSection);
        
        // Определяем размер текста в КБ
        var textSizeKB = (stats.textChars / 1024).toFixed(2);
        var textSizeMB = (stats.textChars / (1024 * 1024)).toFixed(3);
        var sizeDisplay = "";
        if (stats.textChars > 1024 * 1024) {
            sizeDisplay = textSizeMB + " МБ";
        } else {
            sizeDisplay = textSizeKB + " КБ";
        }
        
        // Формируем HTML для IE окна
        var htmlContent = generateIEWindowContent(currentSection, scriptName, version, stats, 
            sizeDisplay, SHOW_SHORT_TEXT, MAX_TEXT_LENGTH, MAX_LAST_PARAGRAPH_LENGTH, 
            MAX_CONSECUTIVE_FULL_PARAGRAPHS, SHOW_EMPTY_PARAGRAPHS, SHOW_FULL_LINKS, 
            COMPACT_MODE, COLOR_TAG, COLOR_ATTR_NAME, COLOR_ATTR_VALUE, COLOR_LINK, 
            COLOR_TEXT, COLOR_STRIKE, COLOR_BUTTON, COLOR_BUTTON_HOVER, COLOR_SUCCESS,
            COLOR_INSTRUCTION, COLOR_WARNING);
        
        // Определяем время выполнения
        var endTime = new Date().getTime();
        var execTime = (endTime - startTime) / 1000;
        
        // Добавляем время выполнения в HTML
        htmlContent = htmlContent.replace('<!-- EXEC_TIME -->', formatTime(execTime) + " сек");
        
        // Показываем результат в зависимости от выбранного режима
        if (SHOW_IN_IE_WINDOW) {
            showInIEWindow(htmlContent, IE_WINDOW_WIDTH, IE_WINDOW_HEIGHT, 
                          UPDATE_EXISTING_WINDOW, MAX_WINDOWS);
        } else {
            // В стандартном окне - простой текст
            showInStandardWindow(currentSection, scriptName, version, stats, sizeDisplay, 
                SHOW_SHORT_TEXT, MAX_TEXT_LENGTH, MAX_LAST_PARAGRAPH_LENGTH,
                MAX_CONSECUTIVE_FULL_PARAGRAPHS, SHOW_EMPTY_PARAGRAPHS, 
                SHOW_FULL_LINKS, COMPACT_MODE, execTime);
        }
        
    } catch (e) {
        MsgBox(scriptName + "\nver. " + version + "\n---------------------------------------\n\n" +
               "Ошибка выполнения скрипта:\n" + e.message + "\n\nСтрока: " + (e.lineNumber || "неизвестна"));
    }
}

// ==================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ (совместимые с IE6)
// ==================================================

// Функция для поиска элемента в массиве (замена indexOf)
function arrayIndexOf(arr, item) {
    for (var i = 0; i < arr.length; i++) {
        if (arr[i] === item) {
            return i;
        }
    }
    return -1;
}

// Функция для поиска подстроки в строке (замена indexOf для строк)
function stringIndexOf(str, searchStr) {
    // Используем встроенный indexOf для строк (он есть в IE6)
    if (typeof str.indexOf === 'function') {
        return str.indexOf(searchStr);
    }
    // Запасной вариант
    for (var i = 0; i <= str.length - searchStr.length; i++) {
        if (str.substring(i, i + searchStr.length) === searchStr) {
            return i;
        }
    }
    return -1;
}

// Функция для создания разделителя
function createSeparator(length) {
    var separator = "";
    for (var i = 0; i < length; i++) {
        separator += "=";
    }
    return separator;
}

// Функция для форматирования времени
function formatTime(seconds) {
    var str = seconds.toString();
    var dotIndex = stringIndexOf(str, ".");
    if (dotIndex == -1) {
        return str + ".000";
    }
    
    var decimalPart = str.substring(dotIndex + 1);
    if (decimalPart.length > 3) {
        decimalPart = decimalPart.substring(0, 3);
    } else {
        while (decimalPart.length < 3) {
            decimalPart += "0";
        }
    }
    
    return str.substring(0, dotIndex) + "." + decimalPart;
}

// Функция для показа сообщения
function showMessage(scriptName, version, message) {
    var fullMessage = scriptName + "\n" +
                      "ver. " + version + "\n" +
                      "---------------------------------------\n\n" +
                      message;
    MsgBox(fullMessage);
}

// Функция для показа в IE окне
function showInIEWindow(content, width, height, updateExisting, maxWindows) {
    var win = null;
    var windowId = "htmlMapWindow";
    
    // Очищаем закрытые окна из массива
    cleanClosedWindows();
    
    if (updateExisting && window._htmlMapWindows.length > 0) {
        // Используем последнее открытое окно
        win = window._htmlMapWindows[window._htmlMapWindows.length - 1];
        
        // Проверяем, не закрыто ли окно
        try {
            if (win.closed) {
                // Удаляем из массива и создаем новое
                window._htmlMapWindows.pop();
                win = null;
            }
        } catch (e) {
            // Окно закрыто
            window._htmlMapWindows.pop();
            win = null;
        }
    }
    
    if (!win) {
        // Создаем новое окно
        if (window._htmlMapWindows.length >= maxWindows) {
            // Закрываем самое старое окно
            try {
                var oldWin = window._htmlMapWindows.shift();
                if (oldWin && !oldWin.closed) {
                    oldWin.close();
                }
            } catch (e) {
                // Игнорируем ошибки
            }
        }
        
        var features = "width=" + width + ",height=" + height + 
                      ",resizable=yes,scrollbars=yes,status=no,location=no,toolbar=no,menubar=no";
        
        win = window.open("", windowId + "_" + (++window._htmlMapWindowIndex), features);
        
        if (!win) {
            MsgBox("Не удалось открыть окно. Возможно, блокировщик всплывающих окон.\nИспользуется стандартное окно.");
            return;
        }
        
        // Сохраняем ссылку на окно
        window._htmlMapWindows.push(win);
    }
    
    // Обновляем содержимое окна
    try {
        win.document.open();
        win.document.write(content);
        win.document.close();
        win.focus();
    } catch (e) {
        MsgBox("Ошибка при обновлении окна: " + e.message);
    }
}

// Очистка массива закрытых окон
function cleanClosedWindows() {
    var activeWindows = [];
    
    for (var i = 0; i < window._htmlMapWindows.length; i++) {
        var win = window._htmlMapWindows[i];
        try {
            if (!win.closed) {
                activeWindows.push(win);
            }
        } catch (e) {
            // Окно закрыто
        }
    }
    
    window._htmlMapWindows = activeWindows;
}

// Функция для генерации контента для IE окна
function generateIEWindowContent(section, scriptName, version, stats, sizeDisplay, 
                                 showShortText, maxTextLength, maxLastParagraphLength,
                                 maxFullParagraphs, showEmptyParagraphs, showFullLinks, 
                                 compactMode, colorTag, colorAttrName, colorAttrValue, 
                                 colorLink, colorText, colorStrike, colorButton, 
                                 colorButtonHover, colorSuccess, colorInstruction, 
                                 colorWarning) {
    
    var settingsText = showShortText ? "да" : "нет";
    
    var html = '<!DOCTYPE html><html><head><meta charset="UTF-8">';
    html += '<title>' + scriptName + '</title>';
    html += '<style>';
    html += 'body { font-family: "Courier New", monospace; font-size: 12px; margin: 0; padding: 10px; background: #f5f5f5; }';
    html += '.header { background: #2c3e50; color: white; padding: 10px; border-radius: 5px; margin-bottom: 10px; position: relative; }';
    html += '.stats { background: white; padding: 10px; border-radius: 5px; margin-bottom: 10px; border: 1px solid #ddd; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }';
    html += '.code-container { background: white; padding: 15px; border-radius: 5px; border: 1px solid #ddd; overflow: auto; font-family: "Tahoma", monospace; font-size: 15px; line-height: 1.4; white-space: pre; position: relative; }';
    html += '.tag { color: ' + colorTag + '; font-weight: bold; }';
    html += '.attr-name { color: ' + colorAttrName + '; }';
    html += '.attr-value { color: ' + colorAttrValue + '; }';
    html += '.link { color: ' + colorLink + '; text-decoration: none; }';
    html += '.link:hover { text-decoration: underline; }';
    html += '.text { color: ' + colorText + '; }';
    html += '.strike { color: ' + colorStrike + '; }';
    html += '.copy-button { position: absolute; top: 10px; right: 10px; background: ' + colorButton + '; color: white; border: none; padding: 5px 10px; border-radius: 3px; cursor: pointer; font-size: 11px; font-family: Arial, sans-serif; }';
    html += '.copy-button:hover { background: ' + colorButtonHover + '; }';
    html += '.copy-button.success { background: ' + colorSuccess + '; }';
    html += 'h1 { margin: 0 0 5px 0; font-size: 16px; font-weight: normal; }';
    html += 'h2 { margin: 0 0 10px 0; font-size: 13px; color: #2c3e50; border-bottom: 1px solid #eee; padding-bottom: 5px; }';
    html += '.stats-table { width: 100%; border-collapse: collapse; margin: 5px 0; }';
    html += '.stats-table td { padding: 3px 5px; border-bottom: 1px solid #eee; }';
    html += '.stats-table tr:last-child td { border-bottom: none; }';
    html += '.footer { margin-top: 10px; padding: 5px; text-align: center; font-size: 11px; color: #777; border-top: 1px solid #ddd; }';
    html += '.colors-info { font-size: 10px; color: #666; margin-top: 5px; }';
    html += '.color-sample { display: inline-block; width: 12px; height: 12px; margin: 0 2px 0 5px; border: 1px solid #ccc; }';
    html += '.instruction { font-size: 14px; font-weight: bold; color: ' + colorInstruction + '; text-align: center; margin: 10px 0; padding: 10px; background: #FFF3CD; border: 2px solid ' + colorInstruction + '; border-radius: 5px; }';
    html += '.warning { font-size: 12px; color: ' + colorWarning + '; font-weight: bold; text-align: center; margin: 5px 0; padding: 5px; background: #FFF3CD; border: 1px solid ' + colorWarning + '; border-radius: 3px; }';
    html += '</style>';
    
    // ПРОСТАЯ функция копирования - работает только в некоторых случаях
    html += '<script type="text/javascript">';
    html += 'function tryToCopy() {';
    html += '  try {';
    html += '    // Пытаемся использовать старый способ IE6';
    html += '    var codeContainer = document.getElementById("htmlCodeContent");';
    html += '    if (codeContainer && window.clipboardData) {';
    html += '      var text = codeContainer.innerText || codeContainer.textContent;';
    html += '      window.clipboardData.setData("Text", text);';
    html += '      alert("✓ Код скопирован в буфер обмена!\\n\\nТеперь вы можете вставить его куда нужно (Ctrl+V).");';
    html += '      return true;';
    html += '    }';
    html += '  } catch (e) {';
    html += '    // Игнорируем ошибку';
    html += '  }';
    html += '  alert("⚠ Автоматическое копирование не работает в вашем браузере.\\n\\n' + 
            'ИСПОЛЬЗУЙТЕ РУЧНОЙ СПОСОБ:\\n' +
            '1. Трижды щелкните мышью в ПЕРВОЙ строке кода\\n' +
            '2. Нажмите Ctrl+C\\n' +
            '3. Вставьте код куда нужно (Ctrl+V)\\n\\n' +
            'Или просто выделите код мышью и нажмите Ctrl+C.");';
    html += '  return false;';
    html += '}';
    html += '</script>';
    html += '</head><body>';
    
    // Заголовок
    html += '<div class="header">';
    html += '<h1>' + scriptName + ' (ver. ' + version + ')</h1>';
    html += '</div>';
    
    // Статистика
    html += '<div class="stats">';
    html += '<h2>Статистика секции:</h2>';
    html += '<table class="stats-table">';
    
    html += '<tr><td><strong>Заголовков (title):</strong></td><td>' + stats.titles + '</td></tr>';
    html += '<tr><td><strong>Абзацев (P):</strong></td><td>' + stats.paragraphs + '</td></tr>';
    html += '<tr><td><strong>Строк текста:</strong></td><td>' + stats.textLines + '</td></tr>';
    html += '<tr><td><strong>Пустых строк:</strong></td><td>' + stats.emptyLines + '</td></tr>';
    
    if (stats.images > 0) html += '<tr><td><strong>Изображений (image):</strong></td><td>' + stats.images + '</td></tr>';
    if (stats.epigraphs > 0) html += '<tr><td><strong>Эпиграфов (epigraph):</strong></td><td>' + stats.epigraphs + '</td></tr>';
    if (stats.annotations > 0) html += '<tr><td><strong>Аннотаций (annotation):</strong></td><td>' + stats.annotations + '</td></tr>';
    if (stats.cites > 0) html += '<tr><td><strong>Цитат (cite):</strong></td><td>' + stats.cites + '</td></tr>';
    if (stats.poems > 0) html += '<tr><td><strong>Стихов (poem):</strong></td><td>' + stats.poems + '</td></tr>';
    if (stats.stanzas > 0) html += '<tr><td><strong>Строф (stanza):</strong></td><td>' + stats.stanzas + '</td></tr>';
    if (stats.textAuthors > 0) html += '<tr><td><strong>Авторов текста (text-author):</strong></td><td>' + stats.textAuthors + '</td></tr>';
    if (stats.subtitles > 0) html += '<tr><td><strong>Подзаголовков (subtitle):</strong></td><td>' + stats.subtitles + '</td></tr>';
    if (stats.tables > 0) html += '<tr><td><strong>Таблиц:</strong></td><td>' + stats.tables + '</td></tr>';
    
    html += '<tr><td><strong>Тегов STRONG:</strong></td><td>' + stats.strongTags + '</td></tr>';
    html += '<tr><td><strong>Тегов EM:</strong></td><td>' + stats.emTags + '</td></tr>';
    html += '<tr><td><strong>Тегов STRIKE:</strong></td><td>' + stats.strikeTags + '</td></tr>';
    html += '<tr><td><strong>Ссылок (A):</strong></td><td>' + stats.aTags + ' (' + stats.noteLinks + ' примечаний, ' + stats.commentLinks + ' комментариев)</td></tr>';
    if (stats.supTags > 0) html += '<tr><td><strong>Надстрочных (SUP):</strong></td><td>' + stats.supTags + '</td></tr>';
    if (stats.subTags > 0) html += '<tr><td><strong>Подстрочных (SUB):</strong></td><td>' + stats.subTags + '</td></tr>';
    
    // Измененная строка с объемом текста (добавлены строки)
    html += '<tr><td><strong>Объем текста:</strong></td><td>' + sizeDisplay + ' (' + stats.textChars + ' символов, ' + stats.words + ' слов, ' + stats.textLines + ' строк)</td></tr>';
    html += '<tr><td><strong>Настройки:</strong></td><td>Сокращение: ' + settingsText + ' (' + maxTextLength + ' симв., макс. ' + maxFullParagraphs + ' абз. подряд, последний: ' + maxLastParagraphLength + ' симв.)</td></tr>';
    
    html += '</table>';
    
    // Информация о цветах
    html += '<div class="colors-info">';
    html += '<span class="color-sample" style="background:' + colorTag + '"></span>Теги';
    html += '<span class="color-sample" style="background:' + colorAttrName + '"></span>Атрибуты';
    html += '<span class="color-sample" style="background:' + colorAttrValue + '"></span>Значения';
    html += '<span class="color-sample" style="background:' + colorLink + '"></span>Ссылки';
    html += '<span class="color-sample" style="background:' + colorStrike + '"></span>Strike';
    html += '<span class="color-sample" style="background:' + colorText + '"></span>Текст';
    html += '</div>';
    
    html += '</div>';
    
    // ЯРКАЯ ИНСТРУКЦИЯ для копирования
    html += '<div class="instruction">';
    html += '📋 ДЛЯ КОПИРОВАНИЯ КОДА: трижды щелкните мышью в первой строке кода, затем Ctrl+C!';
    html += '</div>';
    
    // Предупреждение о кнопке
    html += '<div class="warning">';
    html += '⚠ Кнопка "Попробовать скопировать" работает не во всех браузерах. Лучше используйте тройной щелчок!';
    html += '</div>';
    
    // HTML код секции с кнопкой копирования
    html += '<div class="code-container">';
    html += '<button id="copyButton" class="copy-button" onclick="tryToCopy()" title="Попытаться скопировать код в буфер обмена (работает не во всех браузерах)">Попробовать скопировать</button>';
    
    // Получаем все абзацы для определения последнего
    var allParagraphs = getAllParagraphs(section);
    var lastParagraphIndex = allParagraphs.length - 1;
    
    var highlightedHTML = generateHighlightedHTML(section, 0, maxTextLength, maxLastParagraphLength, 
                                   showShortText, maxFullParagraphs, showEmptyParagraphs, 
                                   showFullLinks, compactMode, lastParagraphIndex, allParagraphs);
    
    html += '<div id="htmlCodeContent">' + highlightedHTML + '</div>';
    html += '</div>';
    
    // Футер с инструкцией
    html += '<div class="footer">';
    html += '<div style="color: ' + colorInstruction + '; font-weight: bold; margin-bottom: 5px;">';
    html += '👆 Три щелчка в первой строке кода → Ctrl+C → Ctrl+V в нужное место';
    html += '</div>';
    html += 'Время выполнения: <!-- EXEC_TIME -->';
    html += '</div>';
    
    html += '</body></html>';
    
    return html;
}

// Функция для показа в стандартном окне
function showInStandardWindow(section, scriptName, version, stats, sizeDisplay,
                              showShortText, maxTextLength, maxLastParagraphLength,
                              maxFullParagraphs, showEmptyParagraphs, showFullLinks, 
                              compactMode, execTime) {
    
    var result = scriptName + "\n" +
                "ver. " + version + "\n" +
                "---------------------------------------\n\n" +
                "Статистика секции:\n";
    
    result += "Заголовков (title): " + stats.titles + "\n";
    result += "Абзацев (P): " + stats.paragraphs + "\n";
    result += "Строк текста: " + stats.textLines + "\n";
    result += "Пустых строк: " + stats.emptyLines + "\n";
    
    if (stats.images > 0) result += "Изображений (image): " + stats.images + "\n";
    if (stats.epigraphs > 0) result += "Эпиграфов (epigraph): " + stats.epigraphs + "\n";
    if (stats.annotations > 0) result += "Аннотаций (annotation): " + stats.annotations + "\n";
    if (stats.cites > 0) result += "Цитат (cite): " + stats.cites + "\n";
    if (stats.poems > 0) result += "Стихов (poem): " + stats.poems + "\n";
    if (stats.stanzas > 0) result += "Строф (stanza): " + stats.stanzas + "\n";
    if (stats.textAuthors > 0) result += "Авторов текста (text-author): " + stats.textAuthors + "\n";
    if (stats.subtitles > 0) result += "Подзаголовков (subtitle): " + stats.subtitles + "\n";
    if (stats.tables > 0) result += "Таблиц: " + stats.tables + "\n";
    
    result += "Тегов STRONG: " + stats.strongTags + "\n";
    result += "Тегов EM: " + stats.emTags + "\n";
    result += "Тегов STRIKE: " + stats.strikeTags + "\n";
    result += "Ссылок (A): " + stats.aTags + " (" + stats.noteLinks + " примечаний, " + stats.commentLinks + " комментариев)\n";
    if (stats.supTags > 0) result += "Надстрочных (SUP): " + stats.supTags + "\n";
    if (stats.subTags > 0) result += "Подстрочных (SUB): " + stats.subTags + "\n";
    
    // Измененная строка с объемом текста (добавлены строки)
    result += "Объем текста: " + sizeDisplay + " (" + stats.textChars + " символов, " + stats.words + " слов, " + stats.textLines + " строк)\n\n";
    result += createSeparator(30) + "\n\n";
    
    // Получаем все абзацы для определения последнего
    var allParagraphs = getAllParagraphs(section);
    var lastParagraphIndex = allParagraphs.length - 1;
    
    // HTML код
    var htmlCode = generatePlainHTML(section, 0, maxTextLength, maxLastParagraphLength,
                                    showShortText, maxFullParagraphs, showEmptyParagraphs, 
                                    showFullLinks, compactMode, lastParagraphIndex, allParagraphs);
    result += htmlCode;
    
    result += "\n" + createSeparator(30) + "\n";
    result += "Время выполнения: " + formatTime(execTime) + " сек";
    
    MsgBox(result);
}

// Получить все абзацы в секции
function getAllParagraphs(element) {
    var paragraphs = [];
    
    function collectParagraphs(node) {
        if (node.nodeType == 1) {
            if (node.nodeName.toUpperCase() == "P") {
                paragraphs.push(node);
            }
            
            for (var i = 0; i < node.childNodes.length; i++) {
                collectParagraphs(node.childNodes[i]);
            }
        }
    }
    
    collectParagraphs(element);
    return paragraphs;
}

// Функция для поиска родительской секции
function findParentSection(element) {
    var current = element;
    
    while (current) {
        if (current.nodeType == 1) {
            if (current.nodeName.toUpperCase() == "DIV") {
                var className = current.className || "";
                if (className == "section") {
                    return current;
                }
            }
        }
        current = current.parentNode;
    }
    
    return null;
}

// Функция для генерации HTML с подсветкой (для IE окна)
function generateHighlightedHTML(element, depth, maxTextLength, maxLastParagraphLength, 
                                 showShortText, maxFullParagraphs, showEmptyParagraphs, 
                                 showFullLinks, compactMode, lastParagraphIndex, allParagraphs) {
    var result = "";
    var indent = "";
    
    // Создаем отступ
    for (var i = 0; i < depth; i++) {
        indent += "  ";
    }
    
    if (element.nodeType == 1) {
        var tagName = element.nodeName.toUpperCase();
        var className = element.className || "";
        
        // Проверяем, нужно ли пропускать пустой абзац
        if (tagName == "P" && !showEmptyParagraphs) {
            if (isEmptyParagraph(element)) {
                return "";
            }
        }
        
        // Определяем цвет для тега
        var tagColorClass = "tag";
        if (tagName == "STRIKE") {
            tagColorClass = "strike";
        }
        
        // Проверяем, можно ли использовать компактный режим
        var canBeCompact = false;
        if (compactMode && depth > 0) {
            canBeCompact = canElementBeCompact(element);
        }
        
        // Находим индекс этого абзаца в массиве
        var paragraphIndex = findParagraphIndex(allParagraphs, element);
        var isLastParagraph = (paragraphIndex == lastParagraphIndex);
        
        if (canBeCompact) {
            // Компактный режим
            result += indent + '<span class="' + tagColorClass + '">&lt;' + tagName.toLowerCase() + '</span>';
            
            var attrs = getCleanAttributesForDisplay(element, showFullLinks);
            if (attrs) {
                result += attrs;
            }
            
            result += '<span class="' + tagColorClass + '">&gt;</span>';
            
            // Содержимое
            var currentMaxLength = isLastParagraph ? maxLastParagraphLength : maxTextLength;
            var content = generateCompactContent(element, currentMaxLength, showShortText, 
                                                maxFullParagraphs, showFullLinks, false, 
                                                isLastParagraph);
            result += content;
            
            result += '<span class="' + tagColorClass + '">&lt;/' + tagName.toLowerCase() + '&gt;</span>\n';
        } else {
            // Обычный режим
            result += indent + '<span class="' + tagColorClass + '">&lt;' + tagName.toLowerCase() + '</span>';
            
            var attrs = getCleanAttributesForDisplay(element, showFullLinks);
            if (attrs) {
                result += attrs;
            }
            
            result += '<span class="' + tagColorClass + '">&gt;</span>\n';
            
            // Дочерние элементы
            for (var i = 0; i < element.childNodes.length; i++) {
                var child = element.childNodes[i];
                var childHTML = generateHighlightedHTML(child, depth + 1, maxTextLength, 
                                                       maxLastParagraphLength, showShortText, 
                                                       maxFullParagraphs, showEmptyParagraphs, 
                                                       showFullLinks, compactMode, 
                                                       lastParagraphIndex, allParagraphs);
                if (childHTML) {
                    result += childHTML;
                }
            }
            
            result += indent + '<span class="' + tagColorClass + '">&lt;/' + tagName.toLowerCase() + '&gt;</span>\n';
        }
        
    } else if (element.nodeType == 3) {
        var text = element.nodeValue;
        text = text.replace(/^\s+|\s+$/g, '');
        
        if (text) {
            if (text == "&nbsp;" || text.charCodeAt(0) == 160) {
                result += indent + '<span class="text">&nbsp;</span>\n';
            } else {
                // Определяем максимальную длину для этого текста
                var currentMaxLength = maxTextLength;
                
                if (showShortText && text.length > currentMaxLength) {
                    text = text.substring(0, currentMaxLength) + "...";
                }
                
                // Экранируем HTML
                text = text.replace(/&/g, '&amp;')
                           .replace(/</g, '&lt;')
                           .replace(/>/g, '&gt;')
                           .replace(/"/g, '&quot;');
                
                result += indent + '<span class="text">' + text + '</span>\n';
            }
        }
    }
    
    return result;
}

// Найти индекс абзаца в массиве
function findParagraphIndex(paragraphs, paragraph) {
    for (var i = 0; i < paragraphs.length; i++) {
        if (paragraphs[i] === paragraph) {
            return i;
        }
    }
    return -1;
}

// Функция для генерации простого HTML (для стандартного окна)
function generatePlainHTML(element, depth, maxTextLength, maxLastParagraphLength,
                          showShortText, maxFullParagraphs, showEmptyParagraphs, 
                          showFullLinks, compactMode, lastParagraphIndex, allParagraphs) {
    var result = "";
    var indent = "";
    
    // Создаем отступ
    for (var i = 0; i < depth; i++) {
        indent += "  ";
    }
    
    if (element.nodeType == 1) {
        var tagName = element.nodeName.toUpperCase();
        var className = element.className || "";
        
        // Проверяем, нужно ли пропускать пустой абзац
        if (tagName == "P" && !showEmptyParagraphs) {
            if (isEmptyParagraph(element)) {
                return "";
            }
        }
        
        // Проверяем, можно ли использовать компактный режим
        var canBeCompact = false;
        if (compactMode && depth > 0) {
            canBeCompact = canElementBeCompact(element);
        }
        
        // Находим индекс этого абзаца в массиве
        var paragraphIndex = findParagraphIndex(allParagraphs, element);
        var isLastParagraph = (paragraphIndex == lastParagraphIndex);
        
        if (canBeCompact) {
            // Компактный режим
            result += indent + "<" + tagName.toLowerCase();
            
            var attrs = getCleanAttributesForDisplay(element, showFullLinks, true);
            if (attrs) {
                result += attrs;
            }
            
            result += ">";
            
            var currentMaxLength = isLastParagraph ? maxLastParagraphLength : maxTextLength;
            var content = generateCompactContent(element, currentMaxLength, showShortText, 
                                                maxFullParagraphs, showFullLinks, true, 
                                                isLastParagraph);
            result += content;
            
            result += "</" + tagName.toLowerCase() + ">\n";
        } else {
            // Обычный режим
            result += indent + "<" + tagName.toLowerCase();
            
            var attrs = getCleanAttributesForDisplay(element, showFullLinks, true);
            if (attrs) {
                result += attrs;
            }
            
            result += ">\n";
            
            // Дочерние элементы
            for (var i = 0; i < element.childNodes.length; i++) {
                var child = element.childNodes[i];
                var childHTML = generatePlainHTML(child, depth + 1, maxTextLength, 
                                                 maxLastParagraphLength, showShortText,
                                                 maxFullParagraphs, showEmptyParagraphs, 
                                                 showFullLinks, compactMode, 
                                                 lastParagraphIndex, allParagraphs);
                if (childHTML) {
                    result += childHTML;
                }
            }
            
            result += indent + "</" + tagName.toLowerCase() + ">\n";
        }
        
    } else if (element.nodeType == 3) {
        var text = element.nodeValue;
        text = text.replace(/^\s+|\s+$/g, '');
        
        if (text) {
            if (text == "&nbsp;" || text.charCodeAt(0) == 160) {
                result += indent + "&nbsp;\n";
            } else {
                if (showShortText && text.length > maxTextLength) {
                    text = text.substring(0, maxTextLength) + "...";
                }
                
                // Экранируем HTML
                text = text.replace(/&/g, '&amp;')
                           .replace(/</g, '&lt;')
                           .replace(/>/g, '&gt;')
                           .replace(/"/g, '&quot;');
                
                result += indent + text + "\n";
            }
        }
    }
    
    return result;
}

// Получение чистых атрибутов для отображения
function getCleanAttributesForDisplay(element, showFullLinks, plainMode) {
    var result = "";
    var attrs = [];
    
    if (!element.attributes) {
        return "";
    }
    
    var tagName = element.nodeName.toUpperCase();
    var className = element.className || "";
    
    for (var i = 0; i < element.attributes.length; i++) {
        var attr = element.attributes[i];
        var attrName = attr.name.toLowerCase();
        var attrValue = attr.value;
        
        // Фильтруем системные атрибуты IE
        if (isSystemAttribute(attrName)) {
            continue;
        }
        
        // Пропускаем пустые атрибуты
        if (!attrValue || attrValue == "" || attrValue == "null") {
            continue;
        }
        
        // Проверяем, нужно ли показывать этот атрибут
        if (shouldShowAttribute(tagName, className, attrName, attrValue)) {
            if (plainMode) {
                // Простой режим
                attrs.push(attrName + '="' + escapeHtml(attrValue) + '"');
            } else {
                // Режим с подсветкой
                var attrClass = "attr-name";
                if (tagName == "A" && attrName == "href") {
                    attrClass = "link";
                } else if (attrName == "class") {
                    attrClass = "attr-value";
                }
                
                var displayValue = attrValue;
                if (tagName == "A" && attrName == "href" && !showFullLinks && displayValue.length > 50) {
                    displayValue = displayValue.substring(0, 50) + "...";
                }
                
                attrs.push(' <span class="' + attrClass + '">' + attrName + '</span>="<span class="attr-value">' + 
                          escapeHtml(displayValue) + '</span>"');
            }
        }
    }
    
    if (attrs.length > 0) {
        result = attrs.join("");
    }
    
    return result;
}

// Проверка, является ли атрибут системным
function isSystemAttribute(attrName) {
    var systemAttrs = [
        "implementation", "spellcheck", "role", "tabindex", "lang", "disabled",
        "x-ms-aria-flowfrom", "x-ms-acceleratorkey", "hidefocus", "contenteditable",
        "language", "dir", "accesskey", "nofocusrect", "nowrap", "clear", "cite",
        "datetime", "rev", "hreflang", "coords", "type", "charset", "shape", "urn",
        "methods", "rel", "target", "onclick", "ondblclick", "onmouseover",
        "onmouseout", "onmousedown", "onmouseup", "onkeydown", "onkeyup", "onkeypress",
        "onfocus", "onblur", "aria-"
    ];
    
    for (var i = 0; i < systemAttrs.length; i++) {
        if (attrName === systemAttrs[i] || stringIndexOf(attrName, systemAttrs[i]) === 0) {
            return true;
        }
    }
    
    return false;
}

// Проверка, нужно ли показывать атрибут
function shouldShowAttribute(tagName, className, attrName, attrValue) {
    // Всегда показываем class если он не пустой
    if (attrName == "class" && attrValue && attrValue != "") {
        return true;
    }
    
    // Для разных тегов показываем разные атрибуты
    switch (tagName) {
        case "A":
            return (attrName == "href" || attrName == "name");
        case "IMG":
            return (attrName == "src" || attrName == "alt" || attrName == "width" || attrName == "height");
        case "DIV":
            if (className == "image") {
                return (attrName == "href" || attrName == "contenteditable" || attrName == "onresizestart");
            }
            if (className == "table") {
                return (attrName == "border" || attrName == "cellpadding" || attrName == "cellspacing");
            }
            return false;
        case "TABLE":
            return (attrName == "border" || attrName == "cellpadding" || attrName == "cellspacing" || attrName == "width");
        case "TD":
        case "TH":
            return (attrName == "colspan" || attrName == "rowspan" || attrName == "width");
        default:
            return false;
    }
}

// Экранирование HTML
function escapeHtml(text) {
    return text.replace(/&/g, "&amp;")
               .replace(/</g, "&lt;")
               .replace(/>/g, "&gt;")
               .replace(/"/g, "&quot;");
}

// Проверка, пустой ли абзац
function isEmptyParagraph(element) {
    for (var i = 0; i < element.childNodes.length; i++) {
        var child = element.childNodes[i];
        if (child.nodeType == 3) {
            var text = child.nodeValue;
            text = text.replace(/^\s+|\s+$/g, '');
            if (text && text != "&nbsp;" && text != String.fromCharCode(160)) {
                return false;
            }
        } else if (child.nodeType == 1) {
            return false;
        }
    }
    return true;
}

// Проверка, можно ли элемент вывести компактно
function canElementBeCompact(element) {
    var tagName = element.nodeName.toUpperCase();
    var compactableTags = ["P", "STRONG", "EM", "STRIKE", "SUP", "SUB", "A", "SPAN", "CODE"];
    
    for (var i = 0; i < compactableTags.length; i++) {
        if (tagName == compactableTags[i]) {
            return true;
        }
    }
    
    return false;
}

// Генерация компактного содержимого
function generateCompactContent(element, maxTextLength, showShortText, maxFullParagraphs, 
                               showFullLinks, plainMode, isLastParagraph) {
    var result = "";
    var consecutiveFull = 0;
    
    for (var i = 0; i < element.childNodes.length; i++) {
        var child = element.childNodes[i];
        
        if (child.nodeType == 3) {
            var text = child.nodeValue;
            text = text.replace(/^\s+|\s+$/g, '');
            
            if (text) {
                if (text == "&nbsp;" || text.charCodeAt(0) == 160) {
                    result += "&nbsp;";
                } else {
                    var displayText = text;
                    
                    if (showShortText) {
                        if (displayText.length > maxTextLength) {
                            if (isLastParagraph) {
                                // Для последнего абзаца показываем начало и конец
                                var startLength = Math.floor(maxTextLength * 0.6);
                                var endLength = Math.floor(maxTextLength * 0.4);
                                var start = displayText.substring(0, startLength);
                                var end = displayText.substring(displayText.length - endLength);
                                displayText = start + "..." + end;
                            } else {
                                displayText = displayText.substring(0, maxTextLength) + "...";
                            }
                            consecutiveFull = 0;
                        } else {
                            consecutiveFull++;
                            if (consecutiveFull > maxFullParagraphs && !isLastParagraph) {
                                displayText = displayText.substring(0, Math.min(maxTextLength, displayText.length)) + "...";
                                consecutiveFull = 0;
                            }
                        }
                    }
                    
                    if (!plainMode) {
                        displayText = escapeHtml(displayText);
                        result += '<span class="text">' + displayText + '</span>';
                    } else {
                        displayText = escapeHtml(displayText);
                        result += displayText;
                    }
                }
            }
        } else if (child.nodeType == 1) {
            var childTagName = child.nodeName.toUpperCase();
            
            // Определяем цвет для вложенного тега
            var childTagColorClass = "tag";
            if (childTagName == "STRIKE") {
                childTagColorClass = "strike";
            }
            
            if (plainMode) {
                result += "<" + childTagName.toLowerCase();
                var attrs = getCleanAttributesForDisplay(child, showFullLinks, true);
                if (attrs) {
                    result += attrs;
                }
                result += ">";
                
                var isChildLastParagraph = false; // Вложенные элементы не считаются последним абзацем секции
                var childContent = generateCompactContent(child, maxTextLength, showShortText, 
                                                         maxFullParagraphs, showFullLinks, true, 
                                                         isChildLastParagraph);
                result += childContent;
                
                result += "</" + childTagName.toLowerCase() + ">";
            } else {
                result += '<span class="' + childTagColorClass + '">&lt;' + childTagName.toLowerCase() + '</span>';
                var attrs = getCleanAttributesForDisplay(child, showFullLinks, false);
                if (attrs) {
                    result += attrs;
                }
                result += '<span class="' + childTagColorClass + '">&gt;</span>';
                
                var isChildLastParagraph = false;
                var childContent = generateCompactContent(child, maxTextLength, showShortText,
                                                         maxFullParagraphs, showFullLinks, false, 
                                                         isChildLastParagraph);
                result += childContent;
                
                result += '<span class="' + childTagColorClass + '">&lt;/' + childTagName.toLowerCase() + '&gt;</span>';
            }
        }
    }
    
    return result;
}

// Функция для сбора статистики
function collectStatistics(rootElement) {
    var stats = {
        titles: 0,
        subtitles: 0,
        paragraphs: 0,
        textLines: 0,
        emptyLines: 0,
        images: 0,
        epigraphs: 0,
        annotations: 0,
        cites: 0,
        poems: 0,
        stanzas: 0,
        textAuthors: 0,
        tables: 0,
        strongTags: 0,
        emTags: 0,
        strikeTags: 0,
        aTags: 0,
        supTags: 0,
        subTags: 0,
        noteLinks: 0,
        commentLinks: 0,
        textChars: 0,
        words: 0
    };
    
    function analyzeNode(node) {
        if (node.nodeType == 1) {
            var tagName = node.nodeName.toUpperCase();
            var className = node.className || "";
            
            if (tagName == "DIV") {
                if (className == "title") stats.titles++;
                else if (className == "image") stats.images++;
                else if (className == "epigraph") stats.epigraphs++;
                else if (className == "annotation") stats.annotations++;
                else if (className == "cite") stats.cites++;
                else if (className == "poem") stats.poems++;
                else if (className == "stanza") stats.stanzas++;
                else if (className == "table") stats.tables++;
                
            } else if (tagName == "P") {
                stats.paragraphs++;
                if (className == "subtitle") stats.subtitles++;
                else if (className == "text-author") stats.textAuthors++;
                
                if (!isEmptyParagraph(node)) {
                    stats.textLines++;
                } else {
                    stats.emptyLines++;
                }
                
            } else if (tagName == "EMPTY-LINE") {
                stats.emptyLines++;
                
            } else if (tagName == "STRONG") {
                stats.strongTags++;
            } else if (tagName == "EM") {
                stats.emTags++;
            } else if (tagName == "STRIKE") {
                stats.strikeTags++;
            } else if (tagName == "A") {
                stats.aTags++;
                if (className == "note") {
                    stats.noteLinks++;
                } else {
                    var href = node.getAttribute("href") || "";
                    if (stringIndexOf(href, "#c_") != -1) {
                        stats.commentLinks++;
                    }
                }
            } else if (tagName == "SUP") {
                stats.supTags++;
            } else if (tagName == "SUB") {
                stats.subTags++;
            }
            
            for (var i = 0; i < node.childNodes.length; i++) {
                analyzeNode(node.childNodes[i]);
            }
            
        } else if (node.nodeType == 3) {
            var text = node.nodeValue;
            text = text.replace(/^\s+|\s+$/g, '');
            
            if (text) {
                stats.textChars += text.length;
                
                var words = text.split(/\s+/);
                for (var w = 0; w < words.length; w++) {
                    if (words[w].length > 0) {
                        stats.words++;
                    }
                }
            }
        }
    }
    
    analyzeNode(rootElement);
    return stats;
}

// ==================================================
// КОНЕЦ СКРИПТА
// ==================================================
