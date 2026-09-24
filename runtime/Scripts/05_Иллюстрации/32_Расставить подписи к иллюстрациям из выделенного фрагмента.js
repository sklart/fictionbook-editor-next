// Скрипт "Расставить подписи к иллюстрациям из выделенного фрагмента" для редактора FBE 
// version 4.3
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для расстановки подписей к иллюстрациям в fb2 документах
// из выделенного фрагмента текста, содержащего все подписи сразу ко всем иллюстрациям.
// Абзацы из выделенного фрагмента переносятся к имеющимся в документе иллюстрациями.
// В скрипте реализован самый простой случай - кол-во иллюстраций = кол-ву абзацев подписей к ним.
// Перед запуском данного скрипта рекомендуется произвести унификацию иллюстраций
// и удаление неиспользуемых вложений  соответствующими скриптами из папки скриптов 05_Иллюстрации.
// Также не помешает произвести удаление дублей прикрепленных бинарников картинок
// скриптом 17_Исключение копий вложенных файлов.js из папки скриптов 05_Иллюстрации.
// ============================================

// Для успешного расставления подписей к иллюстрациям,
// количество расставленных в тексте картинок
// и кол-во исходных абзацев будущих подписей к ним должны совпадать.

// Исходные подписи должны быть каждая - одним абзацем.
// Пустых строк между исходными абзацами подписей не должно быть.
// Допустимые маркеры для обозначения вторых, третьих и тд.
// абзацев подписей задаются в окне настроек (по умолчанию ++)

// Исходное форматирование абзацев будущих подписей (болд, курсив)
// при переносе их к картинкам - сохраняется.

// Возможная нумерация абзацев будущих подписей (в формате 1. или 1))
// может автоматически удаляться по запросу скрипта.

// Исходные тексты подписей также могут автоматически удаляться
// по запросу скрипта.

// version 4.3, 11.08.2026
// ============================================


function Run() {
    try {
        var scriptName = "Расставить подписи к иллюстрациям из выделенного фрагмента";
        var scriptVersion = "4.3";
        
        // Получаем неразрывный пробел
        var nbspChar, nbspEntity;
        try {
            nbspChar = window.external.GetNBSP();
            nbspEntity = (nbspChar.charCodeAt(0) == 160) ? "&nbsp;" : nbspChar;
        } catch(e) {
            nbspChar = String.fromCharCode(160);
            nbspEntity = "&nbsp;";
        }
        
        // Проверяем выделение
        if (!document.selection || document.selection.type.toLowerCase() !== "text") {
            MsgBox(scriptName + "\nver. " + scriptVersion + "\n---------------------------\n\nВы ничего не выделили.\n\nПеред запуском данного скрипта, пожалуйста, выделите абзацы с подписями.",
                   "FBE скрипт");
            return;
        }
        
        var myRange = document.selection.createRange();
        if (!myRange.text || myRange.text.replace(/^\s+|\s+$/g, '').replace(/\s+/g, '') === '') {
            MsgBox(scriptName + "\nver. " + scriptVersion + "\n---------------------------\n\nВыделение пустое или содержит только пробелы!",
                   "FBE скрипт");
            return;
        }
        
        // Сохраняем диапазон выделения для возможного удаления
        var originalSelectionRange = myRange.duplicate();
        
        // Получаем HTML выделения
        var selectedHTML = myRange.htmlText;
        
        // Находим все блочные картинки
        var allBlockImages = findAllBlockImagesSimple();
        var totalImages = allBlockImages.length;
        
        // Извлекаем абзацы из HTML
        var rawParagraphs = extractHTMLParagraphs(selectedHTML);
        var totalRawParagraphs = rawParagraphs.length;
        
        // Предварительная группировка со стандартным маркером ++ для подсчёта в диалоге
        var preGrouped = groupCaptions(rawParagraphs, "++");
        var preGroupedCount = preGrouped.length;
        
        // Проверяем, есть ли нумерация в подписях
        var hasNumbering = checkForNumberingSimple(rawParagraphs);
        
        // ==================================================
        // НАСТРОЙКИ СКРИПТА - ПОЛУЧАЕМ ЧЕРЕЗ ДИАЛОГ
        // ==================================================
        
        // Показываем диалог настроек с предварительным подсчётом
        var settings = showSettingsDialog(totalImages, totalRawParagraphs, preGroupedCount, scriptName, scriptVersion);
        if (!settings) {
            return; // Отмена
        }
        
        var skipImages = settings.skipImages;
        var formatStyle = settings.formatStyle;
        var addEmptyBefore = settings.addEmptyBefore;
        var addEmptyAfter = settings.addEmptyAfter;
        var forceInsert = settings.forceInsert;
        var captionMarker = settings.captionMarker;
        
        // Окончательная группировка с выбранным маркером
        var captions = groupCaptions(rawParagraphs, captionMarker);
        var totalCaptions = captions.length;
        
        // Проверка: подписей больше чем картинок - ошибка
        if (totalCaptions > totalImages) {
            MsgBox(scriptName + "\nver. " + scriptVersion + "\n---------------------------\n\nОшибка!\n\nПодписей больше, чем картинок.\nНайдено картинок: " + totalImages + "\nНайдено подписей (с маркером \"" + captionMarker + "\"): " + totalCaptions + "\n\nЛишние подписи удалять нельзя — проверьте выделенный фрагмент.",
                   "FBE скрипт");
            return;
        }
        
        // Проверяем skipImages
        if (skipImages < 0) skipImages = 0;
        if (skipImages >= totalImages) {
            MsgBox(scriptName + "\nver. " + scriptVersion + "\n---------------------------\n\nОшибка!\n\nПропуск (" + skipImages + ") превышает количество картинок (" + totalImages + ").\nНечего вставлять.",
                   "FBE скрипт");
            return;
        }
        
        var availableImages = totalImages - skipImages;
        
        // Проверяем количество с учётом пропуска
        if (totalCaptions > availableImages) {
            MsgBox(scriptName + "\nver. " + scriptVersion + "\n---------------------------\n\nОшибка!\n\nПосле пропуска " + skipImages + " картинок остаётся " + availableImages + ".\nПодписей: " + totalCaptions + " — больше, чем доступных картинок.\n\nУменьшите пропуск или проверьте выделенный фрагмент.",
                   "FBE скрипт");
            return;
        }
        
        if (totalCaptions < availableImages && !forceInsert) {
            MsgBox(scriptName + "\nver. " + scriptVersion + "\n---------------------------\n\nВставка отменена.\n\nКартинок (после пропуска): " + availableImages + "\nПодписей: " + totalCaptions + "\n\nКоличество не совпадает, а принудительная вставка не подтверждена.",
                   "FBE скрипт");
            return;
        }
        
        // Подтверждение расстановки
        var confirmMsg = "Расставить " + totalCaptions + " подписей к " + totalCaptions + " картинкам (начиная с " + (skipImages + 1) + "-й)?";
        if (captionMarker) {
            confirmMsg += "\n\nМаркер продолжения: \"" + captionMarker + "\"";
        }
        if (!confirm(confirmMsg)) {
            return;
        }
        
        // Запрос на удаление нумерации (если она есть)
        var removeNumbering = false;
        if (hasNumbering) {
            removeNumbering = confirm("В подписях обнаружена нумерация (1., 2., 1), 2) и т.д.).\n\n" +
                                   "Удалить нумерацию из расставляемых подписей?\n\n" +
                                   "Рекомендуется: ДА, так как нумерация обычно нужна только в исходном списке.");
        }
        
        // Запрос на удаление исходного фрагмента
        var deleteOriginal = false;
        if (totalCaptions > 0) {
            deleteOriginal = confirm("После расстановки подписей удалить исходный выделенный фрагмент с текстами подписей?\n\n" +
                                    "Рекомендуется: ДА, чтобы избежать дублирования текста в документе.");
        }
        
        // Начинаем транзакцию
        window.external.BeginUndoUnit(document, scriptName);
        
        // Таймер запускаем после confirm'ов
        var startTime = new Date();
        
        // Расставляем подписи
        var successCount = 0;
        var numberingRemovedCount = 0;
        
        // Работаем только с картинками после пропуска
        for (var i = 0; i < totalCaptions; i++) {
            var imageIndex = skipImages + i;
            if (imageIndex >= allBlockImages.length) break;
            
            var caption = captions[i];
            
            // Применяем удаление нумерации к каждому абзацу подписи
            if (removeNumbering) {
                for (var k = 0; k < caption.parts.length; k++) {
                    var originalPart = caption.parts[k];
                    caption.parts[k] = removeNumberingWithDotInTag(caption.parts[k]);
                    if (caption.parts[k] !== originalPart) {
                        numberingRemovedCount++;
                    }
                }
            }
            
            if (insertCaptionWithFormattingFixed(
                allBlockImages[imageIndex].element, 
                caption.parts,
                formatStyle,
                addEmptyBefore,
                addEmptyAfter
            )) {
                successCount++;
            }
        }
        
        // Удаляем исходный фрагмент, если запрошено
        var deleteResult = "";
        if (deleteOriginal && successCount > 0) {
            try {
                originalSelectionRange.select();
                originalSelectionRange.text = "";
                deleteResult = "Исходный фрагмент удален.";
            } catch(e) {
                deleteResult = "Не удалось удалить исходный фрагмент.";
            }
        } else if (deleteOriginal) {
            deleteResult = "Исходный фрагмент НЕ удален (подписи не были расставлены).";
        } else {
            deleteResult = "Исходный фрагмент сохранен.";
        }
        
        // Добавляем информацию об удалении нумерации
        var numberingResult = "";
        if (removeNumbering && numberingRemovedCount > 0) {
            numberingResult = "\nНумерация удалена из абзацев: " + numberingRemovedCount;
        }
        
        // Название формата
        var formatName = "";
        switch (formatStyle) {
            case 0: formatName = "обычный текст"; break;
            case 1: formatName = "как есть"; break;
            case 2: formatName = "курсив"; break;
            case 3: formatName = "жирный"; break;
            case 4: formatName = "жирный + курсив"; break;
            default: formatName = "курсив"; break;
        }
        
        // Время выполнения
        var endTime = new Date();
        var elapsed = (endTime - startTime) / 1000;
        var timeStr = elapsed.toFixed(3).replace('.', ',') + " сек.";
        
        // Завершаем транзакцию
        window.external.EndUndoUnit(document);
        
        // Форматированное сообщение о результате
        var resultMessage = scriptName + "\n" +
                          "ver. " + scriptVersion + "\n" +
                          "---------------------------\n\n" +
                          "Успешно расставлено подписей: " + successCount + "\n" +
                          "Форматирование подписей: " + formatName + "\n" +
                          "Пустые строки перед подписями: " + (addEmptyBefore ? "ДА" : "НЕТ") + "\n" +
                          "Пустые строки после подписей: " + (addEmptyAfter ? "ДА" : "НЕТ");
        
        if (captionMarker) {
            resultMessage += "\nМаркер продолжения: \"" + captionMarker + "\"";
        }
        
        resultMessage += "\n" + deleteResult;
        
        if (numberingResult) {
            resultMessage += numberingResult;
        }
        
        resultMessage += "\n\nВремя выполнения: " + timeStr;
        
        // Результат
        MsgBox(resultMessage, "FBE скрипт");
        
    } catch (error) {
        MsgBox(scriptName + "\nver. " + scriptVersion + "\n---------------------------\n\nОшибка: " + error.message,
               "FBE скрипт");
    }
}

// ==================================================
// ДИАЛОГ НАСТРОЕК
// ==================================================
function showSettingsDialog(totalImages, totalRawParagraphs, preGroupedCount, scriptName, scriptVersion) {
    var result = null;
    
    try {
        var fso = new ActiveXObject("Scripting.FileSystemObject");
        var tempPath = fso.GetSpecialFolder(2) + "\\fbe_captions_dialog_temp.html";
        
        var showWarning = (preGroupedCount < totalImages);
        
        var dialogHTML = '<!DOCTYPE html>\n<html>\n<head>\n' +
            '<meta http-equiv="Content-Type" content="text/html; charset=windows-1251">\n' +
            '<meta http-equiv="MSThemeCompatible" content="yes">\n' +
            '<title>' + scriptName + '</title>\n' +
            '<style>\n' +
            'body{font-family:Tahoma;font-size:14px;margin:10px;background:#f0f0f0;}\n' +
            '.title{font-weight:bold;font-size:15px;margin-bottom:2px;}\n' +
            '.ver{font-size:12px;margin-bottom:12px;color:#555;}\n' +
            'fieldset{border:1px solid #999;padding:8px 10px;margin-bottom:8px;background:#fff;}\n' +
            'legend{font-weight:bold;color:#333;}\n' +
            'label{cursor:pointer;}\n' +
            '.info{background:#fff;border:1px solid #999;padding:8px 10px;margin-bottom:8px;}\n' +
            '.warning{color:#856404;}\n' +
            '.buttons{text-align:center;margin-top:12px;}\n' +
            'input[type="button"]{width:130px;height:28px;font-family:Tahoma;font-size:14px;margin:0 5px;}\n' +
            'input[type="text"]{font-family:Tahoma;font-size:14px;}\n' +
            '.small{font-size:11px;color:#888;}\n' +
            '.hint{font-size:11px;color:#666;margin-top:4px;}\n' +
            '</style>\n' +
            '</head>\n' +
            '<script>\n' +
            'function getValues() {\n' +
            '  var skipVal = parseInt(document.getElementById("skipInput").value);\n' +
            '  if (isNaN(skipVal) || skipVal < 0) skipVal = 0;\n' +
            '  var formatVal = 2;\n' +
            '  var radios = document.getElementsByName("format");\n' +
            '  for (var i = 0; i < radios.length; i++) {\n' +
            '    if (radios[i].checked) { formatVal = parseInt(radios[i].value); break; }\n' +
            '  }\n' +
            '  var forceVal = 1;\n';
        
        if (showWarning) {
            dialogHTML += '  forceVal = document.getElementById("forceInsert").checked ? 1 : 0;\n';
        }
        
        dialogHTML += '  var markerVal = document.getElementById("markerInput").value;\n' +
            '  if (markerVal.length > 5) markerVal = markerVal.substring(0, 5);\n' +
            '  var res = skipVal + "|" + formatVal + "|" + ' +
            '(document.getElementById("emptyBefore").checked ? 1 : 0) + "|" + ' +
            '(document.getElementById("emptyAfter").checked ? 1 : 0) + "|" + forceVal + "|" + markerVal;\n' +
            '  window.returnValue = res;\n' +
            '  window.close();\n' +
            '}\n' +
            '</script>\n' +
            '<body>\n' +
            '<div class="title">' + scriptName + '</div>\n' +
            '<div class="ver">ver. ' + scriptVersion + '</div>\n' +
            '<fieldset>\n' +
            '<legend>Начальная позиция</legend>\n' +
            'Пропустить картинок от начала: <input type="text" id="skipInput" value="0" maxlength="4" size="4">\n' +
            '</fieldset>\n' +
            '<fieldset>\n' +
            '<legend>Многоабзацные подписи</legend>\n' +
            'Маркер 2-го, 3-го и т.д. абзацев подписи: <input type="text" id="markerInput" value="++" maxlength="5" size="6">\n' +
            '<div class="hint">Задайте маркер для обозначения продолжения подписи (например ++ или ~~)</div>\n' +
            '</fieldset>\n' +
            '<fieldset>\n' +
            '<legend>Форматирование подписей</legend>\n' +
            '<label><input type="radio" name="format" value="0"> Обычный текст (снять форматирование)</label><br>\n' +
            '<label><input type="radio" name="format" value="1"> Оставить как есть</label><br>\n' +
            '<label><input type="radio" name="format" value="2" checked> Курсив</label><br>\n' +
            '<label><input type="radio" name="format" value="3"> Жирный</label><br>\n' +
            '<label><input type="radio" name="format" value="4"> Жирный + курсив</label>\n' +
            '</fieldset>\n' +
            '<fieldset>\n' +
            '<legend>Пустые строки</legend>\n' +
            '<label><input type="checkbox" id="emptyBefore"> Пустая строка МЕЖДУ картинкой и подписью</label><br>\n' +
            '<label><input type="checkbox" id="emptyAfter" checked> Пустая строка ПОСЛЕ подписи</label>\n' +
            '</fieldset>\n' +
            '<div class="info">\n' +
            'Найдено картинок: <b>' + totalImages + '</b><br>\n' +
            'Найдено подписей: <b>' + preGroupedCount + '</b>';
        
        if (showWarning) {
            dialogHTML += '<br><br><span class="warning">Подписей меньше, чем картинок (' + preGroupedCount + ' &lt; ' + totalImages + ')</span>\n' +
                '<br><label><input type="checkbox" id="forceInsert" checked> Вставить подписи к первым ' + preGroupedCount + ' картинкам</label>\n' +
                '<br><span class="small">(с учётом пропуска, если он задан)</span>';
        }
        
        dialogHTML += '</div>\n' +
            '<div class="buttons">\n' +
            '<input type="button" value="OK" onclick="getValues();">\n' +
            '<input type="button" value="Отмена" onclick="window.returnValue=null; window.close();">\n' +
            '</div>\n' +
            '</body>\n</html>';
        
        var fh = fso.CreateTextFile(tempPath, true);
        fh.WriteLine(dialogHTML);
        fh.Close();
        
        // Показываем диалог
        var rawResult = window.showModalDialog(tempPath, null,
            "dialogHeight: 620px; dialogWidth: 500px; " +
            "center: Yes; help: No; resizable: No; status: No;");
        
        // Удаляем временный файл
        try { fso.DeleteFile(tempPath); } catch(e) {}
        
        if (!rawResult) return null;
        
        // Разбираем результат
        var parts = rawResult.split("|");
        result = {
            skipImages: parseInt(parts[0]) || 0,
            formatStyle: parseInt(parts[1]),
            addEmptyBefore: parseInt(parts[2]) || 0,
            addEmptyAfter: parseInt(parts[3]) || 1,
            forceInsert: parseInt(parts[4]) || 0,
            captionMarker: parts[5] || ""
        };
        
        // Если formatStyle не распознан — по умолчанию курсив (2)
        if (isNaN(result.formatStyle) || result.formatStyle < 0 || result.formatStyle > 4) {
            result.formatStyle = 2;
        }
        
        // Ограничиваем маркер 5 символами
        if (result.captionMarker.length > 5) {
            result.captionMarker = result.captionMarker.substring(0, 5);
        }
        
    } catch(e) {
        MsgBox(scriptName + "\nver. " + scriptVersion + "\n---------------------------\n\nНе удалось открыть окно настроек.\nБудут использованы значения по умолчанию.",
               "FBE скрипт");
        result = {
            skipImages: 0,
            formatStyle: 2,
            addEmptyBefore: 0,
            addEmptyAfter: 1,
            forceInsert: (totalImages >= preGroupedCount) ? 1 : 0,
            captionMarker: "++"
        };
    }
    
    return result;
}

// Группировка абзацев по маркеру
function groupCaptions(rawParagraphs, marker) {
    var captions = [];
    if (!rawParagraphs || rawParagraphs.length === 0) return captions;
    
    // Если маркер пустой — каждый абзац отдельная подпись
    if (!marker) {
        for (var i = 0; i < rawParagraphs.length; i++) {
            captions.push({
                parts: [rawParagraphs[i].html]
            });
        }
        return captions;
    }
    
    var currentCaption = null;
    var markerLen = marker.length;
    
    for (var i = 0; i < rawParagraphs.length; i++) {
        var html = rawParagraphs[i].html;
        var plainText = getPlainTextFromHTML(html);
        
        // Проверяем, начинается ли строка с маркера
        var startsWithMarker = false;
        if (plainText.length >= markerLen) {
            var start = plainText.substring(0, markerLen);
            if (start === marker) {
                startsWithMarker = true;
            }
        }
        
        if (startsWithMarker && currentCaption) {
            // Это продолжение предыдущей подписи — удаляем маркер из текста
            var cleanedHtml = removeMarkerFromHTML(html, marker);
            currentCaption.parts.push(cleanedHtml);
        } else {
            // Это новая подпись
            currentCaption = {
                parts: [html]
            };
            captions.push(currentCaption);
        }
    }
    
    return captions;
}

// Удалить маркер из начала HTML
function removeMarkerFromHTML(html, marker) {
    if (!html || !marker) return html;
    
    var markerLen = marker.length;
    
    // Создаём временный DOM
    var tempDiv = document.createElement('div');
    tempDiv.innerHTML = html;
    
    // Собираем текстовые узлы
    var textNodes = [];
    collectTextNodesForMarker(tempDiv, textNodes);
    
    if (textNodes.length === 0) return html;
    
    // Удаляем символы маркера из начала
    var remaining = markerLen;
    for (var j = 0; j < textNodes.length; j++) {
        if (remaining <= 0) break;
        
        var node = textNodes[j];
        var text = node.nodeValue || "";
        
        // Пропускаем пробелы перед маркером
        var trimmedText = text.replace(/^\s+/, '');
        var spacesCount = text.length - trimmedText.length;
        
        if (spacesCount > 0 && trimmedText.length > 0) {
            var textAfterSpaces = text.substring(spacesCount);
            if (textAfterSpaces.substring(0, 1) === marker.substring(0, 1)) {
                var toRemove = Math.min(remaining, textAfterSpaces.length);
                var matched = textAfterSpaces.substring(0, toRemove);
                if (marker.substring(0, matched.length) === matched) {
                    node.nodeValue = text.substring(0, spacesCount) + textAfterSpaces.substring(toRemove);
                    remaining -= toRemove;
                    continue;
                }
            }
        }
        
        if (text.length <= remaining) {
            node.nodeValue = "";
            remaining -= text.length;
        } else {
            node.nodeValue = text.substring(remaining);
            remaining = 0;
        }
    }
    
    return tempDiv.innerHTML;
}

// Собрать текстовые узлы для удаления маркера
function collectTextNodesForMarker(element, resultArray) {
    if (!element) return;
    
    for (var i = 0; i < element.childNodes.length; i++) {
        var child = element.childNodes[i];
        
        if (child.nodeType === 3) {
            var text = child.nodeValue || "";
            resultArray.push(child);
        } else if (child.nodeType === 1) {
            collectTextNodesForMarker(child, resultArray);
        }
    }
}

// Проверить наличие нумерации в подписях
function checkForNumberingSimple(captions) {
    if (!captions || captions.length === 0) return false;
    
    var numberingPatterns = [
        /^\s*[0-9]{1,3}\.\s/,
        /^\s*[0-9]{1,3}\)\s/,
        /^\s*[0-9]{1,3}\s/,
        /^\s*[0-9]{1,3}\.\s*$/,
        /^\s*[0-9]{1,3}\)\s*$/
    ];
    
    for (var i = 0; i < captions.length; i++) {
        var caption = captions[i];
        if (!caption || !caption.html) continue;
        
        var textContent = getPlainTextFromHTML(caption.html);
        
        for (var p = 0; p < numberingPatterns.length; p++) {
            if (numberingPatterns[p].test(textContent)) {
                return true;
            }
        }
    }
    
    return false;
}

// Получить plain text из HTML
function getPlainTextFromHTML(html) {
    if (!html) return "";
    
    var text = html.replace(/<[^>]*>/g, ' ');
    text = text.replace(/&nbsp;/g, ' ');
    text = text.replace(/&amp;/g, '&');
    text = text.replace(/&lt;/g, '<');
    text = text.replace(/&gt;/g, '>');
    text = text.replace(/&quot;/g, '"');
    text = text.replace(/\s+/g, ' ');
    text = text.replace(/^\s+|\s+$/g, '');
    
    return text;
}

// Удалить нумерацию с учетом точки внутри тега
function removeNumberingWithDotInTag(html) {
    if (!html) return html;
    
    var originalHTML = html;
    
    try {
        var simplePatterns = [
            /^(<p[^>]*>\s*)([0-9]{1,3}\.\s+)/i,
            /^(<p[^>]*>\s*)([0-9]{1,3}\)\s+)/i,
            /^(<p[^>]*>\s*)([0-9]{1,3}\s+)/i
        ];
        
        for (var i = 0; i < simplePatterns.length; i++) {
            if (simplePatterns[i].test(html)) {
                var newHtml = html.replace(simplePatterns[i], '$1');
                if (getPlainTextFromHTML(newHtml).replace(/\s/g, '') !== '') {
                    return newHtml;
                }
            }
        }
        
        var tempDiv = document.createElement('div');
        tempDiv.innerHTML = html;
        
        var textNodes = [];
        collectTextNodes(tempDiv, textNodes);
        
        if (textNodes.length === 0) {
            return html;
        }
        
        if (textNodes.length >= 2) {
            var firstText = textNodes[0].nodeValue || "";
            var secondText = textNodes[1].nodeValue || "";
            
            var numberMatch = firstText.match(/^\s*([0-9]{1,3})\s*$/);
            var dotMatch = secondText.match(/^\s*(\.|\))\s*/);
            
            if (numberMatch && dotMatch) {
                textNodes[0].nodeValue = "";
                textNodes[1].nodeValue = secondText.substring(dotMatch[0].length);
                
                var newHtml = tempDiv.innerHTML;
                
                if (getPlainTextFromHTML(newHtml).replace(/\s/g, '') !== '') {
                    return newHtml;
                }
            }
        }
        
        var allText = getPlainTextFromHTML(html);
        var numberingMatch = allText.match(/^\s*([0-9]{1,3})(\.|\)|\s+)/);
        
        if (numberingMatch) {
            var numberingLength = numberingMatch[0].length;
            var remaining = numberingLength;
            
            for (var j = 0; j < textNodes.length; j++) {
                if (remaining <= 0) break;
                
                var node = textNodes[j];
                var text = node.nodeValue || "";
                
                if (text.length <= remaining) {
                    node.nodeValue = "";
                    remaining -= text.length;
                } else {
                    node.nodeValue = text.substring(remaining);
                    remaining = 0;
                }
            }
            
            var newHtml = tempDiv.innerHTML;
            newHtml = newHtml.replace(/<[^>]+>\s*<\/[^>]+>/g, '');
            
            if (getPlainTextFromHTML(newHtml).replace(/\s/g, '') !== '') {
                return newHtml;
            }
        }
        
        return originalHTML;
        
    } catch(e) {
        return originalHTML;
    }
}

// Собрать все текстовые узлы
function collectTextNodes(element, resultArray) {
    if (!element) return;
    
    for (var i = 0; i < element.childNodes.length; i++) {
        var child = element.childNodes[i];
        
        if (child.nodeType === 3) {
            var text = child.nodeValue || "";
            if (text.replace(/\s/g, '') !== '') {
                resultArray.push(child);
            }
        } else if (child.nodeType === 1) {
            collectTextNodes(child, resultArray);
        }
    }
}

// Извлечь абзацы из HTML
function extractHTMLParagraphs(html) {
    var paragraphs = [];
    if (!html) return paragraphs;
    
    try {
        var tempDiv = document.createElement('div');
        tempDiv.style.position = 'absolute';
        tempDiv.style.left = '-9999px';
        tempDiv.innerHTML = html;
        document.body.appendChild(tempDiv);
        
        var pTags = tempDiv.getElementsByTagName('P');
        
        for (var i = 0; i < pTags.length; i++) {
            var htmlContent = pTags[i].innerHTML;
            paragraphs.push({
                html: htmlContent
            });
        }
        
        if (paragraphs.length === 0) {
            paragraphs.push({
                html: html
            });
        }
        
        document.body.removeChild(tempDiv);
        
    } catch(e) {}
    
    return paragraphs;
}

// Найти все блочные картинки
function findAllBlockImagesSimple() {
    var images = [];
    
    try {
        var allElements = document.getElementsByTagName('*');
        
        for (var i = 0; i < allElements.length; i++) {
            var element = allElements[i];
            var className = element.className || '';
            
            if (typeof className === 'string' && className.indexOf('image') !== -1) {
                var tagName = element.tagName ? element.tagName.toUpperCase() : '';
                
                if (tagName === 'DIV') {
                    var href = element.getAttribute('href') || element.getAttribute('l:href') || '';
                    
                    var isEmpty = (href === '#undefined' || href.indexOf('undefined') !== -1 || href === '#');
                    if (!isEmpty && href && href !== '#') {
                        images.push({
                            element: element,
                            href: href
                        });
                    }
                }
            }
        }
        
    } catch (e) {}
    
    return images;
}

// Вставить подпись (поддерживает многоабзацные)
function insertCaptionWithFormattingFixed(imageElement, captionParts, formatStyle, addEmptyBefore, addEmptyAfter) {
    try {
        var parent = imageElement.parentNode;
        if (!parent) return false;
        
        var insertPoint = imageElement.nextSibling;
        
        // Пустая строка перед подписью
        if (addEmptyBefore) {
            if (!isEmptyParagraph(insertPoint)) {
                var emptyLineBefore = document.createElement('p');
                parent.insertBefore(emptyLineBefore, insertPoint);
                window.external.inflateBlock(emptyLineBefore) = true;
            }
        }
        
        // Вставляем все части подписи
        var lastInserted = null;
        for (var j = 0; j < captionParts.length; j++) {
            var formattedPart = applyFormatting(captionParts[j], formatStyle);
            var part = cleanHTMLPart(formattedPart);
            if (part && part.replace(/^\s+|\s+$/g, '').replace(/\s+/g, '') !== '') {
                var p = document.createElement('p');
                p.innerHTML = part;
                parent.insertBefore(p, insertPoint);
                lastInserted = p;
            }
        }
        
        // Пустая строка после подписи
        if (addEmptyAfter && lastInserted) {
            var afterLast = lastInserted.nextSibling;
            if (!isEmptyParagraph(afterLast)) {
                var emptyLine = document.createElement('p');
                parent.insertBefore(emptyLine, afterLast);
                window.external.inflateBlock(emptyLine) = true;
            }
        } else if (addEmptyAfter) {
            if (!isEmptyParagraph(insertPoint)) {
                var emptyLine2 = document.createElement('p');
                parent.insertBefore(emptyLine2, insertPoint);
                window.external.inflateBlock(emptyLine2) = true;
            }
        }
        
        return true;
        
    } catch (e) {
        return false;
    }
}

// Применить форматирование к HTML подписи
function applyFormatting(html, formatStyle) {
    if (!html) return html;
    
    switch (formatStyle) {
        case 0:
            // Обычный текст — вырезаем все теги, оставляем plain text
            return getPlainTextFromHTML(html);
        case 1:
            // Оставить как есть
            return html;
        case 2:
            // Курсив
            return '<EM>' + html + '</EM>';
        case 3:
            // Жирный
            return '<STRONG>' + html + '</STRONG>';
        case 4:
            // Жирный + курсив
            return '<EM><STRONG>' + html + '</STRONG></EM>';
        default:
            return html;
    }
}

// Проверить, является ли элемент пустым абзацем
function isEmptyParagraph(element) {
    if (!element) return false;
    if (element.nodeType !== 1) return false;
    if (element.tagName && element.tagName.toUpperCase() !== 'P') return false;
    
    var html = element.innerHTML || '';
    html = html.replace(/^\s+|\s+$/g, '');
    html = html.replace(/&nbsp;/g, '');
    html = html.replace(/\s+/g, '');
    
    return html === '' || html === '<br>' || html === '<br/>';
}

// Очистить часть HTML (убрать лишние пробелы, сохранить теги)
function cleanHTMLPart(html) {
    if (!html) return '';
    
    var trimmed = html;
    trimmed = trimmed.replace(/^(\s*)([^<])/, '$2');
    trimmed = trimmed.replace(/([^>])(\s*)$/, '$1');
    
    return trimmed;
}