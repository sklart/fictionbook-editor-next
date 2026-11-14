// Скрипт "Показать миниатюры иллюстраций из документа" для редактора FBE
// version 6.4
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для показа миниатюр иллюстраций текущего fb2 документа.
// Также отображается сводка по всем иллюстрациям и бинарным файлам.
// При выделении в документе картинки и запуске скрипта, фокус на соответствующей миниатюре в окне
// сохраняется и обозначен синей рамкой.
// Для перехода в документ на позицию выделенной в окне миниатюры нажмите соответствующую кнопку в меню окна.
// Миниатюры отображаются в html окне, отображение можно настраивать.
// Для работы скрипт создает папку temp_thumbs в основной папке программы FBE
// и временно помещает в нее все иллюстрации из документа и дополнительные служебный файлы.
// При закрытии html окна папка temp_thumbs автоматически очищается от всех временных файлов.
// При клике на миниатюру открывается её увеличенное изображение (лайтбокс).
// В "режиме редактирования" можно помечать иллюстрации на удаление или менять их порядок прямо в окне просмотра.
// Для вступления в силу сделанных изменений, необходимо закрыть окно просмотра (редактирования) миниатюр.
// Скрипт выдаст запрос на подтверждение изменений.
// Поддержка отмены действий (Ctrl+Z).

// version 6.4, 10.07.2026
//======================================

function Run() {
    try {
        var scriptName = "Показать миниатюры иллюстраций из документа";
        var scriptVersion = "6.4";
        
        // ==================================================
        // НАСТРОЙКИ СКРИПТА
        // ==================================================
        
        var defaultThumbSize = "medium";  // "small", "medium", "large"
        var defaultSortOrder = "mixed";   // "mixed" - по порядку, "blockfirst" - блочные → инлайн
        
        // ==================================================
        // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
        // ==================================================
        
        // === Определяем путь к папке редактора ===
        var editorPath = '';
        var editorPathDecoded = '';
        try {
            var href = document.location.href;
            editorPath = href.replace(/main\.html.*$/, '');
            editorPath = editorPath.replace(/^file:\/\/\//, '');
            editorPathDecoded = decodeURIComponent(editorPath);
            editorPathDecoded = editorPathDecoded.replace(/\//g, '\\');
        } catch (e) {}
        
        if (!editorPathDecoded) {
            MsgBox(scriptName + "\nver. " + scriptVersion + "\n\nНе удалось определить путь к папке редактора.");
            return;
        }
        
        // Пути к файлам для обмена данными с окном миниатюр
        var tempDir = editorPathDecoded + 'temp_thumbs\\';
        var deletedFilePath = tempDir + '_deleted.txt';
        var swapsFilePath = tempDir + '_swaps.txt';
        var fso = new ActiveXObject("Scripting.FileSystemObject");
        
        // === Очистка временной папки от предыдущих запусков ===
        try {
            if (fso.FolderExists(tempDir)) {
                var folder = fso.GetFolder(tempDir);
                var fileEnum = new Enumerator(folder.Files);
                var filesToDelete = [];
                for (; !fileEnum.atEnd(); fileEnum.moveNext()) {
                    var file = fileEnum.item();
                    // Не удаляем _thumbs.html — он нам нужен
                    if (file.Name !== '_thumbs.html') {
                        filesToDelete.push(file);
                    }
                }
                // Удаляем в обратном порядке для стабильности
                for (var i = filesToDelete.length - 1; i >= 0; i--) {
                    try { filesToDelete[i].Delete(true); } catch(e3) {}
                }
            } else {
                fso.CreateFolder(tempDir);
            }
        } catch (e) {
            // Если не удалось очистить — пробуем удалить и создать заново
            try {
                if (fso.FolderExists(tempDir)) {
                    fso.DeleteFolder(tempDir, true);
                }
                fso.CreateFolder(tempDir);
            } catch (e2) {
                MsgBox(scriptName + "\nver. " + scriptVersion + "\n\nНе удалось очистить временную папку.");
                return;
            }
        }
        
        // === Находим обложку документа ===
        var coverFileName = '';
        try {
            var tiCover = document.getElementById('tiCover');
            if (tiCover) {
                var selects = tiCover.getElementsByTagName('select');
                for (var i = 0; i < selects.length; i++) {
                    if (selects[i].id === 'href') {
                        var val = selects[i].value || '';
                        if (val && val !== '') {
                            coverFileName = val.replace(/^#/, '');
                            break;
                        }
                    }
                }
            }
            if (!coverFileName) {
                var stiCover = document.getElementById('stiCover');
                if (stiCover) {
                    var selects2 = stiCover.getElementsByTagName('select');
                    for (var i = 0; i < selects2.length; i++) {
                        if (selects2[i].id === 'href') {
                            var val = selects2[i].value || '';
                            if (val && val !== '') {
                                coverFileName = val.replace(/^#/, '');
                                break;
                            }
                        }
                    }
                }
            }
        } catch (e) {}
        
        // === Собираем данные о бинарниках (mime, размер, габариты) ===
        var binaryData = {};
        try {
            var binobj = document.all.binobj;
            if (binobj) {
                var binDivs = binobj.getElementsByTagName("DIV");
                for (var i = 0; i < binDivs.length; i++) {
                    var id = binDivs[i].all.id.value || "";
                    if (id) {
                        var mime = binDivs[i].all.type.value || "";
                        var sizeStr = binDivs[i].all.size.value || "0";
                        var dimsStr = "";
                        if (binDivs[i].all.dims) {
                            dimsStr = binDivs[i].all.dims.value || "";
                        }
                        binaryData[id] = {
                            mime: mime,
                            size: parseInt(sizeStr, 10) || 0,
                            dims: dimsStr
                        };
                    }
                }
            }
        } catch (e) {}
        
        // === Собираем все иллюстрации из fbw_body (каждое вхождение уникально) ===
        var allImages = [];
        var usedFileNames = {};
        var emptyCounter = 0;
        var fbwBody2 = document.getElementById("fbw_body");
        
        if (fbwBody2) {
            // Обходим все элементы в порядке DOM для сохранения правильного порядка
            var allElements = fbwBody2.getElementsByTagName('*');
            for (var i = 0; i < allElements.length; i++) {
                var el = allElements[i];
                var tagName = el.tagName ? el.tagName.toUpperCase() : '';
                var className = el.className || '';
                
                if (typeof className === 'string' && className.toLowerCase() == 'image') {
                    var hrefAttr = el.getAttribute("href") || "";
                    var isEmpty = (hrefAttr === '#undefined' || hrefAttr.indexOf('undefined') !== -1);
                    var isBlock = (tagName === 'DIV');
                    var fileName = '';
                    var displayName = '';
                    
                    if (!isEmpty && hrefAttr && hrefAttr !== '#') {
                        // Обычная картинка с ссылкой на бинарник
                        fileName = hrefAttr.replace(/^#/, '');
                        displayName = fileName;
                        usedFileNames[fileName] = true;
                    } else {
                        // Пустышка — даём уникальное имя для отображения
                        emptyCounter++;
                        displayName = '_empty_' + emptyCounter;
                        fileName = '';
                    }
                    
                    var binInfo = null;
                    if (!isEmpty && fileName && binaryData[fileName]) {
                        binInfo = binaryData[fileName];
                    }
                    
                    // Уникальный ID для каждого вхождения (нужен для точечного удаления)
                    var uid = allImages.length;
                    
                    allImages.push({
                        uid: uid,
                        fileName: fileName,
                        displayName: displayName,
                        isBlock: isBlock,
                        isEmpty: isEmpty,
                        binInfo: binInfo,
                        isCover: false,
                        isUnlinked: false,
                        domElement: el  // Сохраняем ссылку на DOM-элемент для быстрого перехода
                    });
                }
            }
        }
        
        if (allImages.length === 0 && !coverFileName) {
            MsgBox(scriptName + "\nver. " + scriptVersion + "\n\nВ документе иллюстраций нет!");
            return;
        }
        
        // === Сохраняем массив DOM-элементов для перехода из окна миниатюр ===
        window._thumbsElements = [];
        for (var i = 0; i < allImages.length; i++) {
            window._thumbsElements.push(allImages[i].domElement);
        }
        
        // === Неприлинкованные бинарники (не используются ни в одной иллюстрации) ===
        var unlinkedFiles = [];
        for (var key in binaryData) {
            if (binaryData.hasOwnProperty(key)) {
                if (!usedFileNames[key] && key !== coverFileName) {
                    unlinkedFiles.push({
                        uid: -1,
                        fileName: key,
                        displayName: key,
                        isBlock: false,
                        isEmpty: false,
                        binInfo: binaryData[key],
                        isCover: false,
                        isUnlinked: true,
                        domElement: null
                    });
                }
            }
        }
        
        // === Обложка (если не используется в иллюстрациях) ===
        var coverImage = null;
        if (coverFileName && !usedFileNames[coverFileName]) {
            coverImage = {
                uid: -1,
                fileName: coverFileName,
                displayName: coverFileName,
                isBlock: false,
                isEmpty: false,
                binInfo: binaryData[coverFileName] || null,
                isCover: true,
                isUnlinked: false,
                domElement: null
            };
        }
        
        // === Сохраняем все бинарники во временную папку ===
        var savedCount = 0;
        try {
            var binobj2 = document.all.binobj;
            if (binobj2) {
                var binDivs = binobj2.getElementsByTagName("DIV");
                for (var i = 0; i < binDivs.length; i++) {
                    var id = binDivs[i].all.id.value || "";
                    if (id) {
                        var destPath = tempDir + id;
                        try {
                            if (window.external.SaveBinary(destPath, binDivs[i].base64data, 0)) {
                                savedCount++;
                            }
                        } catch(e2) {}
                    }
                }
            }
        } catch (e) {}
        
        // === Статистика ===
        var blockCount = 0, emptyBlockCount = 0, inlineCount = 0, emptyInlineCount = 0;
        for (var i = 0; i < allImages.length; i++) {
            if (allImages[i].isBlock) {
                if (allImages[i].isEmpty) emptyBlockCount++; else blockCount++;
            } else {
                if (allImages[i].isEmpty) emptyInlineCount++; else inlineCount++;
            }
        }
        
        var totalBinaries = 0, jpgCount = 0, pngCount = 0, otherFormatCount = 0;
        var totalOriginalSize = 0, fb2FormatsSize = 0, usedCount = 0;
        
        for (var key in binaryData) {
            if (binaryData.hasOwnProperty(key)) {
                totalBinaries++;
                var bd = binaryData[key];
                var ml = bd.mime.toLowerCase();
                var kl = key.toLowerCase();
                
                if (ml.indexOf('jpeg') !== -1 || ml.indexOf('jpg') !== -1 || kl.indexOf('.jpg') !== -1) {
                    jpgCount++;
                } else if (ml.indexOf('png') !== -1 || kl.indexOf('.png') !== -1) {
                    pngCount++;
                } else {
                    otherFormatCount++;
                }
                
                totalOriginalSize += bd.size;
                if (ml.indexOf('jpeg') !== -1 || ml.indexOf('jpg') !== -1 || ml.indexOf('png') !== -1 || kl.indexOf('.jpg') !== -1 || kl.indexOf('.png') !== -1) {
                    fb2FormatsSize += bd.size;
                }
                if (usedFileNames[key] || key === coverFileName) {
                    usedCount++;
                }
            }
        }
        
        var unusedCount = totalBinaries - usedCount;
        var totalBase64Size = Math.floor(totalOriginalSize * 4 / 3);
        var fb2FormatsBase64Size = Math.floor(fb2FormatsSize * 4 / 3);
        var totalImages = allImages.length;
        var totalEmpty = emptyBlockCount + emptyInlineCount;
        
        // === Формируем массив данных для передачи в JavaScript окна ===
        var jsDataArray = '[\n';
        for (var i = 0; i < allImages.length; i++) {
            jsDataArray += thumbToJson(allImages[i]);
            if (i < allImages.length - 1 || unlinkedFiles.length > 0 || coverImage) {
                jsDataArray += ',';
            }
            jsDataArray += '\n';
        }
        for (var i = 0; i < unlinkedFiles.length; i++) {
            jsDataArray += thumbToJson(unlinkedFiles[i]);
            if (i < unlinkedFiles.length - 1 || coverImage) {
                jsDataArray += ',';
            }
            jsDataArray += '\n';
        }
        if (coverImage) {
            jsDataArray += thumbToJson(coverImage);
            jsDataArray += '\n';
        }
        jsDataArray += ']';
        
        // === Определяем выделенную в документе картинку ===
        var selectedUid = -1;
        try {
            var sel = document.selection;
            // Блочные картинки выделяются как Control Range
            if (sel && sel.type == "Control") {
                var controlRange = sel.createRange();
                if (controlRange && controlRange.length > 0) {
                    var currentElement = controlRange.item(0);
                    if (currentElement) {
                        // Ищем этот элемент в массиве иллюстраций
                        for (var i = 0; i < allImages.length; i++) {
                            if (allImages[i].domElement === currentElement) {
                                selectedUid = i;
                                break;
                            }
                        }
                    }
                }
            }
        } catch (e) {}
        
        // ==================================================
        // ФОРМИРОВАНИЕ HTML-СТРАНИЦЫ С МИНИАТЮРАМИ
        // ==================================================
        
        var html = '<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">\n';
        html += '<html xmlns="http://www.w3.org/1999/xhtml">\n';
        html += '<head>\n';
        html += '<meta http-equiv="Content-Type" content="text/html; charset=utf-8">\n';
        html += '<title>Миниатюры иллюстраций</title>\n';
        
        // === Стили ===
        html += '<style>\n';
        html += 'body{font-family:Tahoma,sans-serif;font-size:12px;margin:10px;padding-bottom:140px}\n';
        html += '.summary{background:#f5f5f5;border:1px solid #ccc;padding:10px;margin-bottom:15px}\n';
        html += '.summary table{width:100%;border-collapse:collapse}\n';
        html += '.summary td{vertical-align:top;padding:0 15px}\n';
        html += '.summary td.left{border-right:1px solid #ccc}\n';
        html += '.summary pre{margin:0;font-family:Tahoma,sans-serif;font-size:12px}\n';
        html += '.script-title{font-weight:bold;font-size:14px}\n';
        // Панель управления (фиксированная внизу)
        html += '.controls{position:fixed;bottom:0;left:0;right:0;z-index:100;padding:8px;background:#eef;border-top:2px solid #ccd}\n';
        html += '.controls label{margin-right:15px;cursor:pointer}\n';
        html += '.controls .checks{margin-top:5px}\n';
        html += '.controls button{margin-left:6px;padding:3px 8px;cursor:pointer}\n';
        // Кнопка помощи
        html += '.help-btn{background:#ff0;color:#000;font-weight:bold;font-size:14px;padding:2px 7px;border:1px solid #cc0;cursor:pointer;margin-left:6px}\n';
        html += '.help-panel{display:none;background:#fffde0;border:1px solid #e0c000;padding:8px;margin-top:8px;position:relative}\n';
        html += '.help-panel .help-close{position:absolute;top:4px;right:8px;cursor:pointer;font-weight:bold;color:#c00}\n';
        html += '.help-panel .help-text{font-size:11px;line-height:1.6;margin:0;white-space:normal}\n';
        html += '.help-warning{color:#c00;font-weight:bold}\n';
        // Счётчики
        html += '.delete-counter{background:#fdd;padding:5px 10px;margin-top:8px;border:1px solid #faa}\n';
        html += '.swap-counter{background:#dfd;padding:5px 10px;margin-top:4px;border:1px solid #afa}\n';
        html += '.edit-mode-label{font-weight:bold;color:#c00}\n';
        // Миниатюры
        html += '.thumb-item{text-align:center;vertical-align:top;display:inline-block;margin:5px;border:1px solid #eee;padding:3px;background:#fafafa;position:relative;cursor:default}\n';
        html += '.thumb-item.marked{opacity:.3;border-color:#f00;background:#fee}\n';
        html += '.thumb-item.selected{border-color:#00f;border-width:2px;background:#eef}\n';
        html += '.thumb-item.dragging{opacity:.4}\n';
        html += '.thumb-img{border:1px solid #ddd;display:block;margin:0 auto;cursor:pointer}\n';
        html += '.thumb-caption{font-size:10px;margin-top:2px;word-wrap:break-word;cursor:pointer}\n';
        html += '.thumb-caption:hover{color:#00f;text-decoration:underline}\n';
        html += '.thumb-info{font-size:10px;color:#666;cursor:pointer}\n';
        html += '.thumb-info:hover{color:#00f}\n';
        // Разделители
        html += '.separator{width:100%;border-top:2px dashed #999;margin:15px 0;padding-top:5px;font-weight:bold;color:#555;clear:both}\n';
        html += '.empty-placeholder{background:#000;color:#fff;font-weight:bold}\n';
        // Кнопки редактирования (видны только в режиме редактирования)
        html += '.edit-btn{position:absolute;width:20px;height:20px;border:none;font-size:14px;line-height:18px;cursor:pointer;text-align:center;padding:0;display:none}\n';
        html += '.edit-mode .edit-btn{display:block}\n';
        html += '.delete-btn{top:-8px;right:-8px;background:#f00;color:#fff}\n';
        // Лайтбокс
        html += '.overlay{display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:#000;z-index:9999;text-align:center}\n';
        html += '.overlay-close{position:absolute;top:10px;right:20px;color:#fff;font-size:30px;cursor:pointer;z-index:10000;font-weight:bold}\n';
        html += '.overlay-img-wrap{position:absolute;top:50%;left:50%;margin-right:-50%;transform:translate(-50%,-50%);max-width:95%;max-height:95%;overflow:auto}\n';
        html += '.overlay-img{max-width:100%;max-height:100%;border:2px solid #fff;display:block}\n';
        // Стрелки лайтбокса
        html += '.overlay-arrow{position:absolute;top:50%;transform:translateY(-50%);color:rgba(255,255,255,0.7);font-size:50px;cursor:pointer;z-index:10001;text-shadow:0 0 8px rgba(0,0,0,0.8);user-select:none;padding:20px}\n';
        html += '.overlay-arrow:hover{color:#fff}\n';
        html += '.overlay-arrow-left{left:5px}\n';
        html += '.overlay-arrow-right{right:5px}\n';
        // Drag-and-drop
        html += '.drag-clone{position:absolute;z-index:9998;opacity:.6;border:2px dashed #00f;background:#fff}\n';
        html += '.drop-indicator{display:inline-block;width:4px;background:#f00;margin:0 3px;vertical-align:top}\n';
        html += '</style>\n';
        html += '</head>\n';
        html += '<body>\n';
        
        // === Сводка (два столбца) ===
        html += '<div class="summary"><table><tr>';
        html += '<td class="left"><pre><span class="script-title">' + scriptName + '</span>\nversion: ' + scriptVersion + '\n\n';
        html += 'Всего иллюстраций - ' + totalImages + '\nИз них пустых - ' + totalEmpty + '\n';
        if (coverFileName) {
            html += 'Обложка - 1 (' + coverFileName + ')';
            if (coverImage && coverImage.binInfo) {
                html += ', ' + formatFileSize(coverImage.binInfo.size);
            }
            html += '\n';
        }
        html += '\nБлочные иллюстрации - ' + blockCount + '\nПустые блочные - ' + emptyBlockCount + '\n\n';
        html += 'Инлайн иллюстрации - ' + inlineCount + '\nПустые инлайн - ' + emptyInlineCount + '\n';
        html += '</pre></td>';
        html += '<td><pre>ПРИКРЕПЛЕННЫЕ ФАЙЛЫ:\n\n';
        html += 'Всего бинарных файлов - ' + totalBinaries + '\nИспользуются - ' + usedCount + '\nНе используются - ' + unusedCount + '\n\n';
        html += 'ПОДДЕРЖИВАЕМЫЕ ФОРМАТЫ:\n\n';
        if (jpgCount > 0) html += 'JPG - ' + jpgCount + '\n';
        if (pngCount > 0) html += 'PNG - ' + pngCount + '\n';
        if (otherFormatCount > 0) html += 'Другие - ' + otherFormatCount + '\n';
        html += '\nВес (JPG, PNG) - ' + formatFileSize(fb2FormatsSize) + '\nBase64 - ' + formatFileSize(fb2FormatsBase64Size) + '\n\n';
        html += '----------------------\nОбщий вес - ' + formatFileSize(totalOriginalSize) + '\nBase64 - ' + formatFileSize(totalBase64Size) + '\n';
        html += '</pre></td></tr></table></div>\n';
        
        // === Контейнер с миниатюрами ===
        html += '<div id="thumbsContainer" class="" onclick="clearSelection()">\n';
        html += generateThumbsHTML(allImages, unlinkedFiles, coverImage, defaultThumbSize, defaultSortOrder, blockCount, inlineCount);
        html += '</div>\n';
        
        // === Клон для drag-and-drop ===
        html += '<div id="dragClone" class="drag-clone" style="display:none"></div>\n';
        
        // === Лайтбокс со стрелками ===
        html += '<div id="overlay" class="overlay" onclick="closeOverlay()">\n';
        html += '<span class="overlay-close" onclick="closeOverlay()">×</span>\n';
        html += '<span class="overlay-arrow overlay-arrow-left" onclick="overlayPrev();event.cancelBubble=true;">◄</span>\n';
        html += '<span class="overlay-arrow overlay-arrow-right" onclick="overlayNext();event.cancelBubble=true;">►</span>\n';
        html += '<div class="overlay-img-wrap"><img id="overlayImg" class="overlay-img" src="" onclick="event.cancelBubble=true"></div>\n';
        html += '</div>\n';
        
        // === Панель управления (фиксированная внизу) ===
        html += '<div class="controls">';
        // Выбор размера
        html += '<b>Размер:</b> ';
        html += '<label><input type="radio" name="thumbSize" value="small" onclick="changeSize(\'small\')"'; if (defaultThumbSize == 'small') html += ' checked'; html += '> Мелко</label> ';
        html += '<label><input type="radio" name="thumbSize" value="medium" onclick="changeSize(\'medium\')"'; if (defaultThumbSize == 'medium') html += ' checked'; html += '> Средне</label> ';
        html += '<label><input type="radio" name="thumbSize" value="large" onclick="changeSize(\'large\')"'; if (defaultThumbSize == 'large') html += ' checked'; html += '> Крупно</label> &nbsp;&nbsp;&nbsp; ';
        // Выбор порядка
        html += '<b>Порядок:</b> ';
        html += '<label><input type="radio" name="sortOrder" value="mixed" onclick="changeOrder(\'mixed\')"'; if (defaultSortOrder == 'mixed') html += ' checked'; html += '> По порядку</label> ';
        html += '<label><input type="radio" name="sortOrder" value="blockfirst" onclick="changeOrder(\'blockfirst\')"'; if (defaultSortOrder == 'blockfirst') html += ' checked'; html += '> Блочные → Инлайн</label> ';
        // Кнопка помощи
        html += '<button class="help-btn" onclick="toggleHelp()" title="Помощь">?</button>';
        // Чекбоксы
        html += '<div class="checks">';
        html += '<label><input type="checkbox" id="showDims" onclick="refreshThumbs()" checked> Размеры в px</label> ';
        html += '<label><input type="checkbox" id="showSize" onclick="refreshThumbs()"> Вес</label> ';
        html += '<label class="edit-mode-label"><input type="checkbox" id="editMode" onclick="onEditModeChange()"> Режим редактирования</label>';
        html += '</div>';
        
        // Панель помощи (скрыта по умолчанию)
        html += '<div id="helpPanel" class="help-panel"><span class="help-close" onclick="toggleHelp()">×</span><div id="helpText" class="help-text"></div></div>';
        
        // Кнопки действий
        html += '<div style="margin-top:8px"><button onclick="goToSelected()">Перейти в документ</button> ';
        html += '<button onclick="resetMoves()">Сбросить перемещения</button> ';
        html += '<button onclick="resetDeletes()">Сбросить удаления</button> ';
        html += '<button onclick="resetAll()">Сбросить всё</button></div>';
        
        // Счётчики
        html += '<div id="swapCounter" class="swap-counter" style="display:none">Перестановок: <span id="swapCount">0</span> <span id="swapNames"></span></div>';
        html += '<div id="deleteCounter" class="delete-counter" style="display:none">Помечено на удаление: <span id="deleteCount">0</span> <span id="deleteNames"></span></div>';
        html += '</div>\n';
        
        // ==================================================
        // JAVASCRIPT ДЛЯ ОКНА МИНИАТЮР
        // ==================================================
        
        html += '<script>\n';
        
        // Основные переменные
        html += 'var allImagesData = ' + jsDataArray + ';\n';
        html += 'var mainCount = ' + allImages.length + ';\n';         // Количество основных иллюстраций
        html += 'var unlinkedCount = ' + unlinkedFiles.length + ';\n'; // Количество неприлинкованных
        html += 'var hasCover = ' + (coverImage ? '1' : '0') + ';\n';  // Есть ли обложка
        html += 'var currentSize = "' + defaultThumbSize + '";\n';     // Текущий размер миниатюр
        html += 'var currentOrder = "' + defaultSortOrder + '";\n';    // Текущий режим сортировки
        html += 'var deletedUIDs = {};\n';                               // Объект: UID → true/false (помечен на удаление)
        html += 'var currentOrderArray = [];\n';                         // Текущий порядок UID'ов
        html += 'for (var i = 0; i < mainCount; i++) currentOrderArray.push(i);\n';
        html += 'var moveList = [];\n';                                  // Список перемещений [{uid, name, from, to}]
        html += 'var itemsPerRow = 8;\n';                                // Количество миниатюр в строке (адаптивно)
        html += 'var tempDirJs = "' + escapeJsString(tempDir) + '";\n'; // Путь к временной папке
        // Переменные лайтбокса
        html += 'var overlayIndex = -1;\n';                              // Индекс текущей картинки в лайтбоксе
        html += 'var overlayZone = "main";\n';                           // Зона: "main" или "extra"
        // Переменные выделения
        html += 'var selectedUid = ' + selectedUid + ';\n';             // UID выделенной в документе картинки
        html += 'var highlightedUid = selectedUid;\n';                   // UID выделенной в окне миниатюр
        // Переменные drag-and-drop
        html += 'var dragInfo = null;\n';                                // Информация о текущем перетаскивании
        html += '\n';
        
        // === ИНСТРУКЦИЯ ===
        // Обновляет текст инструкции в зависимости от режима
        html += 'function updateHelpText() {\n';
        html += '  var em = document.getElementById("editMode").checked;\n';
        html += '  var ht = document.getElementById("helpText");\n';
        html += '  if (em) {\n';
        html += '    ht.innerHTML = "<b>Инструкция (режим редактирования):</b><br><br>" +\n';
        html += '      "• Выделение миниатюры — кликом по её названию.<br>" +\n';
        html += '      "• Клавиши ← → ↑ ↓ Home End — для перемещения выделенной миниатюры.<br>" +\n';
        html += '      "• Или перетащите миниатюру мышью (drag-and-drop).<br>" +\n';
        html += '      "• Крестик в углу — пометить на удаление.<br><br>" +\n';
        html += '      "<b>Как работает перемещение:</b><br>" +\n';
        html += '      "При перемещении миниатюр сами картинки остаются на своих местах в тексте документа. Меняются только ссылки (href) — какая картинка отображается в каком месте.<br><br>" +\n';
        html += '      "<b>Как работает удаление:</b><br>" +\n';
        html += '      "При применении изменений сначала выполняются все перемещения, и только потом удаляются отмеченные ссылки. Сами бинарные файлы не удаляются из документа — они становятся неприлинкованными.<br><br>" +\n';
        html += '      "<span class=\\\'help-warning\\\'>ВАЖНО: Для применения изменений закройте окно с миниатюрами. Скрипт запросит подтверждение на внесение изменений в fb2 документ.</span>";\n';
        html += '  } else {\n';
        html += '    ht.innerHTML = "<b>Инструкция (режим просмотра):</b><br><br>" +\n';
        html += '      "• Выделение миниатюры — кликом по её названию.<br>" +\n';
        html += '      "• Клавиши ← → ↑ ↓ Home End — для пролистывания миниатюр.<br>" +\n';
        html += '      "• При выделении в документе картинки и запуске скрипта, фокус на соответствующей миниатюре в окне сохраняется и обозначен синей рамкой.<br>" +\n';
        html += '      "• Для перехода в документ на позицию выделенной в окне миниатюры нажмите соответствующую кнопку в меню окна.<br>" +\n';        
        html += '      "• Клик по картинке — увеличенный просмотр (лайтбокс).<br>" +\n';
        html += '      "  В лайтбоксе: ← → — листание, Esc — закрыть, стрелки ◄ ► по бокам — листание мышью.";\n';
        html += '  }\n';
        html += '}\n';
        
        // Показать/скрыть панель помощи
        html += 'function toggleHelp() {\n';
        html += '  var hp = document.getElementById("helpPanel");\n';
        html += '  if (hp.style.display == "block") { hp.style.display = "none"; }\n';
        html += '  else { updateHelpText(); hp.style.display = "block"; }\n';
        html += '}\n';
        
        // Обработчик переключения режима редактирования
        html += 'function onEditModeChange() { updateHelpText(); refreshThumbs(); }\n';
        html += 'updateHelpText();\n';
        html += '\n';
        
        // === ЕДИНЫЙ ОБРАБОТЧИК КЛАВИШ ===
        // Если открыт лайтбокс — передаём управление ему, иначе — стрелкам миниатюр
        html += 'function globalKeyHandler() {\n';
        html += '  if (document.getElementById("overlay").style.display == "block") { overlayKeyHandler(); return; }\n';
        html += '  arrowKeyHandler();\n';
        html += '}\n';
        html += 'document.onkeydown = globalKeyHandler;\n';
        html += '\n';
        
        // Подсветка выделенной картинки при загрузке
        html += 'if (highlightedUid >= 0) {\n';
        html += '  window.setTimeout(function() {\n';
        html += '    var el = document.getElementById("thumb_" + highlightedUid);\n';
        html += '    if (el) { el.className += " selected"; centerOnEl(el); }\n';
        html += '  }, 500);\n';
        html += '}\n';
        html += '\n';
        
        // === ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ===
        
        // Получить индекс UID в основном массиве (currentOrderArray)
        html += 'function getMainIndex(uid) {\n';
        html += '  for (var i = 0; i < mainCount; i++) {\n';
        html += '    if (allImagesData[currentOrderArray[i]].uid == uid) return i;\n';
        html += '  }\n';
        html += '  return -1;\n';
        html += '}\n';
        
        // Получить индекс UID в extra-зоне (неприлинкованные + обложка)
        html += 'function getExtraIndex(uid) {\n';
        html += '  for (var i = mainCount; i < allImagesData.length; i++) {\n';
        html += '    if (allImagesData[i].uid == uid) return i - mainCount;\n';
        html += '  }\n';
        html += '  return -1;\n';
        html += '}\n';
        html += '\n';
        
        // === УДАЛЕНИЕ ===
        // Пометить/снять пометку на удаление (работает только в режиме редактирования)
        html += 'function markForDelete(uid) {\n';
        html += '  var em = document.getElementById("editMode").checked;\n';
        html += '  if (!em) return;\n';
        html += '  if (deletedUIDs[uid]) { deletedUIDs[uid] = false; }\n';
        html += '  else { deletedUIDs[uid] = true; }\n';
        html += '  saveChanges();\n';
        html += '  updateCounters();\n';
        html += '  refreshThumbs();\n';
        html += '}\n';
        
        // === ПЕРЕМЕЩЕНИЕ ===
        // Переместить миниатюру с позиции fromPos на позицию toPos
        html += 'function moveThumb(fromPos, toPos) {\n';
        html += '  if (fromPos < 0 || fromPos >= mainCount || toPos < 0 || toPos > mainCount || fromPos == toPos) return;\n';
        html += '  var movedUid = currentOrderArray[fromPos];\n';
        html += '  var dn = allImagesData[movedUid].displayName || allImagesData[movedUid].fn;\n';
        html += '  moveList.push({uid: movedUid, name: dn, from: fromPos, to: toPos});\n';
        html += '  var na = [];\n';
        html += '  for (var i = 0; i < currentOrderArray.length; i++) {\n';
        html += '    if (i == fromPos) continue;\n';
        html += '    if (na.length == toPos) na.push(movedUid);\n';
        html += '    na.push(currentOrderArray[i]);\n';
        html += '  }\n';
        html += '  if (toPos == currentOrderArray.length) na.push(movedUid);\n';
        html += '  currentOrderArray = na;\n';
        html += '  saveChanges();\n';
        html += '  updateCounters();\n';
        html += '  refreshThumbs();\n';
        html += '}\n';
        
        // === СБРОС ===
        html += 'function resetMoves() {\n';
        html += '  moveList = [];\n';
        html += '  currentOrderArray = [];\n';
        html += '  for (var i = 0; i < mainCount; i++) currentOrderArray.push(i);\n';
        html += '  saveChanges();\n';
        html += '  updateCounters();\n';
        html += '  refreshThumbs();\n';
        html += '}\n';
        html += 'function resetDeletes() { deletedUIDs = {}; saveChanges(); updateCounters(); refreshThumbs(); }\n';
        html += 'function resetAll() { resetMoves(); resetDeletes(); }\n';
        
        // === СОХРАНЕНИЕ ИЗМЕНЕНИЙ В ФАЙЛЫ ===
        html += 'function saveChanges() {\n';
        html += '  var dl = [];\n';
        html += '  for (var k in deletedUIDs) { if (deletedUIDs[k]) dl.push(k); }\n';
        html += '  if (dl.length > 0) {\n';
        html += '    try { var fso = new ActiveXObject("Scripting.FileSystemObject");\n';
        html += '      var f = fso.CreateTextFile(tempDirJs + "_deleted.txt", true, false);\n';
        html += '      f.Write(dl.join("\\n")); f.Close(); } catch(e) {}\n';
        html += '  } else {\n';
        html += '    try { var fso = new ActiveXObject("Scripting.FileSystemObject");\n';
        html += '      if (fso.FileExists(tempDirJs + "_deleted.txt")) fso.DeleteFile(tempDirJs + "_deleted.txt", true); } catch(e) {}\n';
        html += '  }\n';
        html += '  var ol = [];\n';
        html += '  for (var i = 0; i < mainCount; i++) { ol.push(currentOrderArray[i]); }\n';
        html += '  if (ol.length > 0) {\n';
        html += '    try { var fso2 = new ActiveXObject("Scripting.FileSystemObject");\n';
        html += '      var f2 = fso2.CreateTextFile(tempDirJs + "_swaps.txt", true, false);\n';
        html += '      f2.Write(ol.join("\\n")); f2.Close(); } catch(e) {}\n';
        html += '  } else {\n';
        html += '    try { var fso2 = new ActiveXObject("Scripting.FileSystemObject");\n';
        html += '      if (fso2.FileExists(tempDirJs + "_swaps.txt")) fso2.DeleteFile(tempDirJs + "_swaps.txt", true); } catch(e) {}\n';
        html += '  }\n';
        html += '}\n';
        
        // === ОБНОВЛЕНИЕ СЧЁТЧИКОВ ===
        html += 'function updateCounters() {\n';
        html += '  var dc = 0;\n';
        html += '  var dn = [];\n';
        html += '  for (var k in deletedUIDs) {\n';
        html += '    if (deletedUIDs[k]) {\n';
        html += '      dc++;\n';
        html += '      dn.push(allImagesData[parseInt(k)].displayName || allImagesData[parseInt(k)].fn);\n';
        html += '    }\n';
        html += '  }\n';
        html += '  document.getElementById("deleteCount").innerHTML = dc;\n';
        html += '  document.getElementById("deleteNames").innerHTML = dn.length > 0 ? "(" + dn.join(", ") + ")" : "";\n';
        html += '  document.getElementById("deleteCounter").style.display = dc > 0 ? "block" : "none";\n';
        html += '  document.getElementById("swapCount").innerHTML = moveList.length;\n';
        html += '  var sn = [];\n';
        html += '  for (var i = 0; i < Math.min(moveList.length, 20); i++) {\n';
        html += '    sn.push(moveList[i].name + ": поз." + (moveList[i].from + 1) + "→" + (moveList[i].to + 1));\n';
        html += '  }\n';
        html += '  if (moveList.length > 20) sn.push("... и ещё " + (moveList.length - 20));\n';
        html += '  document.getElementById("swapNames").innerHTML = sn.length > 0 ? "(" + sn.join(", ") + ")" : "";\n';
        html += '  document.getElementById("swapCounter").style.display = moveList.length > 0 ? "block" : "none";\n';
        html += '}\n';
        html += '\n';
        
        // === ПРОКРУТКА И ЦЕНТРИРОВАНИЕ ===
        html += 'function centerOnEl(el) {\n';
        html += '  if (!el) return;\n';
        html += '  var rect = getElementRect(el);\n';
        html += '  if (!rect) return;\n';
        html += '  var winH = document.documentElement.clientHeight || document.body.clientHeight;\n';
        html += '  var tY = rect.top - Math.floor((winH - rect.height) / 2);\n';
        html += '  if (tY < 0) tY = 0;\n';
        html += '  window.scrollTo(0, tY);\n';
        html += '}\n';
        html += 'function centerOnSelected() {\n';
        html += '  if (highlightedUid >= 0) {\n';
        html += '    var el = document.getElementById("thumb_" + highlightedUid);\n';
        html += '    if (el) centerOnEl(el);\n';
        html += '  }\n';
        html += '}\n';
        html += '\n';
        
        // === ВЫДЕЛЕНИЕ ===
        // Выделить миниатюру кликом по названию
        html += 'function selectThumb(uid) {\n';
        html += '  if (highlightedUid === uid) { highlightedUid = -1; }\n';
        html += '  else { highlightedUid = uid; }\n';
        html += '  refreshThumbs();\n';
        html += '  event.cancelBubble = true;\n';
        html += '}\n';
        // Снять выделение кликом по пустому месту
        html += 'function clearSelection() {\n';
        html += '  if (highlightedUid >= 0) { highlightedUid = -1; refreshThumbs(); }\n';
        html += '}\n';
        html += '\n';
        
        // === ПЕРЕХОД В ДОКУМЕНТ ===
        html += 'function goToSelected() {\n';
        html += '  if (highlightedUid < 0) { alert("Сначала выделите картинку (кликните по названию под ней)"); return; }\n';
        html += '  try {\n';
        html += '    if (window.opener && !window.opener.closed && window.opener._thumbsElements) {\n';
        html += '      var targetEl = window.opener._thumbsElements[highlightedUid];\n';
        html += '      if (targetEl) {\n';
        html += '        targetEl.scrollIntoView(true);\n';
        html += '        var br = window.opener.document.body.createControlRange();\n';
        html += '        br.addElement(targetEl);\n';
        html += '        br.select();\n';
        html += '        window.opener.focus();\n';
        html += '      }\n';
        html += '    } else { alert("Нет связи с редактором"); }\n';
        html += '  } catch(e) { alert("Ошибка: " + e.message); }\n';
        // Возвращаем фокус в окно миниатюр
        html += '  window.setTimeout(function() { window.focus(); }, 300);\n';
        html += '}\n';
        html += '\n';
        
        // === DRAG-AND-DROP ===
        
        // Сброс состояния перетаскивания
        html += 'function resetDrag() {\n';
        html += '  clearAllIndicators();\n';
        html += '  if (dragInfo && dragInfo.el) {\n';
        html += '    dragInfo.el.className = dragInfo.el.className.replace(/ dragging/g, "");\n';
        html += '  }\n';
        html += '  document.getElementById("dragClone").style.display = "none";\n';
        html += '  dragInfo = null;\n';
        html += '}\n';
        
        // Начало перетаскивания
        html += 'function dragStart(uid, pos) {\n';
        html += '  var em = document.getElementById("editMode").checked;\n';
        html += '  if (!em) return;\n';
        html += '  if (dragInfo) { resetDrag(); return; }\n';
        html += '  if (uid < 0 || uid >= mainCount) return;\n';
        html += '  var imgData = allImagesData[uid];\n';
        html += '  if (!imgData || imgData.cv == 1 || imgData.ul == 1) return;\n';
        html += '  var el = document.getElementById("thumb_" + uid);\n';
        html += '  if (!el) return;\n';
        html += '  highlightedUid = -1;\n';
        html += '  dragInfo = { uid: uid, pos: pos, el: el, sx: event.clientX, sy: event.clientY, moved: false, lip: -1 };\n';
        html += '  el.className += " dragging";\n';
        html += '  event.returnValue = false;\n';
        html += '}\n';
        
        // Движение мыши при перетаскивании
        html += 'function dragMove() {\n';
        html += '  if (!dragInfo) return;\n';
        html += '  var dx = event.clientX - dragInfo.sx;\n';
        html += '  var dy = event.clientY - dragInfo.sy;\n';
        html += '  if (!dragInfo.moved && Math.abs(dx) < 5 && Math.abs(dy) < 5) return;\n';
        html += '  dragInfo.moved = true;\n';
        html += '  var clone = document.getElementById("dragClone");\n';
        html += '  if (clone.style.display == "none") {\n';
        html += '    clone.innerHTML = dragInfo.el.innerHTML;\n';
        html += '    clone.style.display = "block";\n';
        html += '    clone.style.width = dragInfo.el.offsetWidth + "px";\n';
        html += '  }\n';
        html += '  clone.style.left = (event.clientX - 30) + "px";\n';
        html += '  clone.style.top = (event.clientY - 30) + "px";\n';
        html += '  var ip = findInsertPos(event.clientX, event.clientY);\n';
        html += '  if (ip != dragInfo.lip) { dragInfo.lip = ip; highlightInsertPos(ip); }\n';
        html += '}\n';
        
        // Найти позицию для вставки
        html += 'function findInsertPos(x, y) {\n';
        html += '  var items = [];\n';
        html += '  var ads = document.getElementsByTagName("DIV");\n';
        html += '  for (var i = 0; i < ads.length; i++) {\n';
        html += '    if (ads[i].className && ads[i].className.indexOf("thumb-item") != -1 && ads[i].id.indexOf("thumb_") == 0) {\n';
        html += '      var uid = parseInt(ads[i].id.replace("thumb_",""), 10);\n';
        html += '      if (uid >= 0 && uid < mainCount) items.push(ads[i]);\n';
        html += '    }\n';
        html += '  }\n';
        html += '  var bp = -1;\n';
        html += '  var bd = 99999;\n';
        html += '  for (var i = 0; i < items.length; i++) {\n';
        html += '    var rect = getElementRect(items[i]);\n';
        html += '    if (rect) {\n';
        html += '      var cx = rect.left + rect.width / 2;\n';
        html += '      var cy = rect.top + rect.height / 2;\n';
        html += '      var dist = (x - cx) * (x - cx) + (y - cy) * (y - cy);\n';
        html += '      if (dist < bd) { bd = dist; bp = i; }\n';
        html += '    }\n';
        html += '  }\n';
        html += '  return bp;\n';
        html += '}\n';
        
        // Получить координаты элемента
        html += 'function getElementRect(el) {\n';
        html += '  var rect = { left: 0, top: 0, width: el.offsetWidth, height: el.offsetHeight };\n';
        html += '  var c = el;\n';
        html += '  while (c) {\n';
        html += '    rect.left += c.offsetLeft;\n';
        html += '    rect.top += c.offsetTop;\n';
        html += '    c = c.offsetParent;\n';
        html += '  }\n';
        html += '  return rect;\n';
        html += '}\n';
        
        // Очистить все индикаторы вставки
        html += 'function clearAllIndicators() {\n';
        html += '  var old = document.getElementsByTagName("SPAN");\n';
        html += '  for (var i = old.length - 1; i >= 0; i--) {\n';
        html += '    if (old[i].className == "drop-indicator") old[i].parentNode.removeChild(old[i]);\n';
        html += '  }\n';
        html += '}\n';
        
        // Подсветить позицию вставки
        html += 'function highlightInsertPos(pos) {\n';
        html += '  clearAllIndicators();\n';
        html += '  if (pos < 0 || !dragInfo) return;\n';
        html += '  var items = [];\n';
        html += '  var ads = document.getElementsByTagName("DIV");\n';
        html += '  for (var i = 0; i < ads.length; i++) {\n';
        html += '    if (ads[i].className && ads[i].className.indexOf("thumb-item") != -1 && ads[i].id.indexOf("thumb_") == 0) {\n';
        html += '      var uid = parseInt(ads[i].id.replace("thumb_",""), 10);\n';
        html += '      if (uid >= 0 && uid < mainCount) items.push(ads[i]);\n';
        html += '    }\n';
        html += '  }\n';
        html += '  if (pos < items.length) {\n';
        html += '    var ind = document.createElement("SPAN");\n';
        html += '    ind.className = "drop-indicator";\n';
        html += '    ind.style.height = items[pos].offsetHeight + "px";\n';
        html += '    items[pos].parentNode.insertBefore(ind, items[pos]);\n';
        html += '  }\n';
        html += '}\n';
        
        // Завершение перетаскивания
        html += 'function dragStop() {\n';
        html += '  if (!dragInfo) return;\n';
        html += '  clearAllIndicators();\n';
        html += '  document.getElementById("dragClone").style.display = "none";\n';
        html += '  if (dragInfo.el) dragInfo.el.className = dragInfo.el.className.replace(/ dragging/g, "");\n';
        html += '  if (dragInfo.moved) {\n';
        html += '    var ip = findInsertPos(event.clientX, event.clientY);\n';
        html += '    if (ip >= 0 && ip != dragInfo.pos) { moveThumb(dragInfo.pos, ip); }\n';
        html += '  }\n';
        html += '  dragInfo = null;\n';
        html += '}\n';
        
        // Назначаем обработчики мыши
        html += 'document.onmousemove = dragMove;\n';
        html += 'document.onmouseup = dragStop;\n';
        html += '\n';
        
        // === СТРЕЛКИ КЛАВИАТУРЫ (обычный режим и редактирование) ===
        html += 'function arrowKeyHandler() {\n';
        html += '  var em = document.getElementById("editMode").checked;\n';
        html += '  var key = event.keyCode;\n';
        html += '  if (highlightedUid < 0) return;\n';
        html += '  var mainIdx = getMainIndex(highlightedUid);\n';
        html += '  if (mainIdx >= 0) {\n';
        html += '    // В основной зоне\n';
        html += '    if (key == 36) { highlightedUid = allImagesData[currentOrderArray[0]].uid; refreshThumbs(); centerOnSelected(); return; }\n';
        html += '    if (key == 35) { highlightedUid = allImagesData[currentOrderArray[mainCount-1]].uid; refreshThumbs(); centerOnSelected(); return; }\n';
        html += '    if (key == 37 && mainIdx > 0) {\n';
        html += '      if (em) { moveThumb(mainIdx, mainIdx-1); highlightedUid = allImagesData[currentOrderArray[mainIdx-1]].uid; }\n';
        html += '      else { highlightedUid = allImagesData[currentOrderArray[mainIdx-1]].uid; refreshThumbs(); }\n';
        html += '      centerOnSelected();\n';
        html += '    } else if (key == 39 && mainIdx < mainCount-1) {\n';
        html += '      if (em) { moveThumb(mainIdx, mainIdx+1); highlightedUid = allImagesData[currentOrderArray[mainIdx+1]].uid; }\n';
        html += '      else { highlightedUid = allImagesData[currentOrderArray[mainIdx+1]].uid; refreshThumbs(); }\n';
        html += '      centerOnSelected();\n';
        html += '    } else if (key == 38 && mainIdx >= itemsPerRow) {\n';
        html += '      var toPos = mainIdx - itemsPerRow;\n';
        html += '      if (em) { moveThumb(mainIdx, toPos); highlightedUid = allImagesData[currentOrderArray[toPos]].uid; }\n';
        html += '      else { highlightedUid = allImagesData[currentOrderArray[toPos]].uid; refreshThumbs(); }\n';
        html += '      centerOnSelected();\n';
        html += '    } else if (key == 40 && mainIdx < mainCount-itemsPerRow) {\n';
        html += '      var toPos = mainIdx + itemsPerRow;\n';
        html += '      if (em) { moveThumb(mainIdx, toPos); highlightedUid = allImagesData[currentOrderArray[toPos]].uid; }\n';
        html += '      else { highlightedUid = allImagesData[currentOrderArray[toPos]].uid; refreshThumbs(); }\n';
        html += '      centerOnSelected();\n';
        html += '    }\n';
        html += '  } else {\n';
        html += '    // В зоне неприлинкованных/обложки — только пролистывание\n';
        html += '    var extraIdx = getExtraIndex(highlightedUid);\n';
        html += '    if (extraIdx >= 0) {\n';
        html += '      var extraLen = allImagesData.length - mainCount;\n';
        html += '      if (key == 36) { highlightedUid = allImagesData[mainCount].uid; refreshThumbs(); centerOnSelected(); return; }\n';
        html += '      if (key == 35) { highlightedUid = allImagesData[allImagesData.length-1].uid; refreshThumbs(); centerOnSelected(); return; }\n';
        html += '      if (key == 37 && extraIdx > 0) { highlightedUid = allImagesData[mainCount + extraIdx - 1].uid; refreshThumbs(); centerOnSelected(); }\n';
        html += '      else if (key == 39 && extraIdx < extraLen - 1) { highlightedUid = allImagesData[mainCount + extraIdx + 1].uid; refreshThumbs(); centerOnSelected(); }\n';
        html += '      else if (key == 38 && extraIdx >= itemsPerRow) { highlightedUid = allImagesData[mainCount + extraIdx - itemsPerRow].uid; refreshThumbs(); centerOnSelected(); }\n';
        html += '      else if (key == 40 && extraIdx + itemsPerRow < extraLen) { highlightedUid = allImagesData[mainCount + extraIdx + itemsPerRow].uid; refreshThumbs(); centerOnSelected(); }\n';
        html += '    }\n';
        html += '  }\n';
        html += '}\n';
        html += '\n';
        
        // === ЛАЙТБОКС ===
        
        // Открыть лайтбокс
        html += 'function showOverlay(fn) {\n';
        html += '  if (dragInfo) return;\n';
        html += '  document.getElementById("overlayImg").src = fn;\n';
        html += '  document.getElementById("overlay").style.display = "block";\n';
        html += '  overlayIndex = -1;\n';
        // Ищем картинку в основной зоне
        html += '  for (var i = 0; i < mainCount; i++) {\n';
        html += '    var idx = currentOrderArray[i];\n';
        html += '    if (allImagesData[idx].fn === fn) { overlayIndex = i; overlayZone = "main"; break; }\n';
        html += '  }\n';
        // Если не нашли — ищем в extra-зоне
        html += '  if (overlayIndex < 0) {\n';
        html += '    for (var i = mainCount; i < allImagesData.length; i++) {\n';
        html += '      if (allImagesData[i].fn === fn) { overlayIndex = i - mainCount; overlayZone = "extra"; break; }\n';
        html += '    }\n';
        html += '  }\n';
        html += '}\n';
        
        // Закрыть лайтбокс
        html += 'function closeOverlay() {\n';
        html += '  document.getElementById("overlay").style.display = "none";\n';
        html += '  document.getElementById("overlayImg").src = "";\n';
        html += '  overlayIndex = -1;\n';
        html += '}\n';
        
        // Обработчик клавиш в лайтбоксе
        html += 'function overlayKeyHandler() {\n';
        html += '  var key = event.keyCode;\n';
        html += '  if (key == 27) { closeOverlay(); return; }\n';
        html += '  if (overlayZone == "main") {\n';
        html += '    if (key == 36) { overlayIndex = 0; updateOverlayImage(); return; }\n';
        html += '    if (key == 35) { overlayIndex = mainCount - 1; updateOverlayImage(); return; }\n';
        html += '    if (key == 37 && overlayIndex > 0) { overlayIndex--; updateOverlayImage(); return; }\n';
        html += '    if (key == 39 && overlayIndex < mainCount - 1) { overlayIndex++; updateOverlayImage(); return; }\n';
        html += '  } else {\n';
        html += '    var extraLen = allImagesData.length - mainCount;\n';
        html += '    if (key == 36) { overlayIndex = 0; updateOverlayImage(); return; }\n';
        html += '    if (key == 35) { overlayIndex = extraLen - 1; updateOverlayImage(); return; }\n';
        html += '    if (key == 37 && overlayIndex > 0) { overlayIndex--; updateOverlayImage(); return; }\n';
        html += '    if (key == 39 && overlayIndex < extraLen - 1) { overlayIndex++; updateOverlayImage(); return; }\n';
        html += '  }\n';
        html += '}\n';
        
        // Обновить картинку в лайтбоксе
        html += 'function updateOverlayImage() {\n';
        html += '  if (overlayZone == "main") {\n';
        html += '    if (overlayIndex < 0 || overlayIndex >= mainCount) return;\n';
        html += '    var realIdx = currentOrderArray[overlayIndex];\n';
        html += '    if (realIdx >= 0 && realIdx < allImagesData.length) {\n';
        html += '      var fn = allImagesData[realIdx].fn;\n';
        html += '      if (fn) document.getElementById("overlayImg").src = fn;\n';
        html += '    }\n';
        html += '  } else {\n';
        html += '    var ei = mainCount + overlayIndex;\n';
        html += '    if (ei >= 0 && ei < allImagesData.length) {\n';
        html += '      var fn = allImagesData[ei].fn;\n';
        html += '      if (fn) document.getElementById("overlayImg").src = fn;\n';
        html += '    }\n';
        html += '  }\n';
        html += '}\n';
        
        // Листание мышью (стрелки ◄ ►)
        html += 'function overlayPrev() {\n';
        html += '  if (overlayZone == "main") {\n';
        html += '    if (overlayIndex > 0) { overlayIndex--; updateOverlayImage(); }\n';
        html += '  } else {\n';
        html += '    if (overlayIndex > 0) { overlayIndex--; updateOverlayImage(); }\n';
        html += '  }\n';
        html += '}\n';
        html += 'function overlayNext() {\n';
        html += '  if (overlayZone == "main") {\n';
        html += '    if (overlayIndex < mainCount - 1) { overlayIndex++; updateOverlayImage(); }\n';
        html += '  } else {\n';
        html += '    var extraLen = allImagesData.length - mainCount;\n';
        html += '    if (overlayIndex < extraLen - 1) { overlayIndex++; updateOverlayImage(); }\n';
        html += '  }\n';
        html += '}\n';
        html += '\n';
        
        // === ПЕРЕКЛЮЧЕНИЕ РАЗМЕРА И СОРТИРОВКИ ===
        html += 'function getMaxDim(s) {\n';
        html += '  if (s == "small") return 48;\n';
        html += '  if (s == "medium") return 96;\n';
        html += '  if (s == "large") return 192;\n';
        html += '  return 96;\n';
        html += '}\n';
        html += 'function changeSize(s) { currentSize = s; refreshThumbs(); }\n';
        html += 'function changeOrder(o) { currentOrder = o; refreshThumbs(); }\n';
        
        // === ПЕРЕРИСОВКА МИНИАТЮР ===
        html += 'function refreshThumbs() {\n';
        html += '  var c = document.getElementById("thumbsContainer");\n';
        html += '  var h = "";\n';
        html += '  var md = getMaxDim(currentSize);\n';
        html += '  var em = document.getElementById("editMode").checked;\n';
        html += '  var sd = document.getElementById("showDims").checked;\n';
        html += '  var ss = document.getElementById("showSize").checked;\n';
        html += '  var o = currentOrder;\n';
        // Включаем/выключаем режим редактирования
        html += '  if (em) { c.className = "edit-mode"; } else { c.className = ""; }\n';
        // Вычисляем количество миниатюр в строке адаптивно
        html += '  var cw = c.offsetWidth - 20;\n';
        html += '  var iw = md + 20;\n';
        html += '  itemsPerRow = Math.floor(cw / iw);\n';
        html += '  if (itemsPerRow < 1) itemsPerRow = 1;\n';
        // Рисуем в зависимости от режима сортировки
        html += '  if (o == "blockfirst") {\n';
        html += '    var bi = [];\n';
        html += '    var ii = [];\n';
        html += '    for (var i = 0; i < mainCount; i++) {\n';
        html += '      var idx = currentOrderArray[i];\n';
        html += '      if (allImagesData[idx].bl == 1) bi.push(idx); else ii.push(idx);\n';
        html += '    }\n';
        html += '    if (bi.length > 0) {\n';
        html += '      h += \'<div class="separator">▼ Блочные иллюстрации (\' + bi.length + \'):</div>\';\n';
        html += '      for (var i = 0; i < bi.length; i++) h += makeThumb(allImagesData[bi[i]], md, em, sd, ss, i, bi.length);\n';
        html += '    }\n';
        html += '    if (ii.length > 0) {\n';
        html += '      h += \'<div class="separator">▼ Инлайн иллюстрации (\' + ii.length + \'):</div>\';\n';
        html += '      for (var i = 0; i < ii.length; i++) h += makeThumb(allImagesData[ii[i]], md, em, sd, ss, i, ii.length);\n';
        html += '    }\n';
        html += '  } else {\n';
        html += '    for (var i = 0; i < mainCount; i++) {\n';
        html += '      h += makeThumb(allImagesData[currentOrderArray[i]], md, em, sd, ss, i, mainCount);\n';
        html += '    }\n';
        html += '  }\n';
        // Добавляем неприлинкованные и обложку
        html += '  var he = (unlinkedCount > 0 || hasCover == 1);\n';
        html += '  if (he) {\n';
        html += '    h += \'<div class="separator">▼ Неприлинкованные файлы / Обложка:</div>\';\n';
        html += '    for (var i = mainCount; i < allImagesData.length; i++) {\n';
        html += '      h += makeThumb(allImagesData[i], md, em, sd, ss, -1, 0);\n';
        html += '    }\n';
        html += '  }\n';
        html += '  c.innerHTML = h;\n';
        html += '}\n';
        
        // === РАСЧЁТ РАЗМЕРА МИНИАТЮРЫ С УЧЁТОМ ПРОПОРЦИЙ ===
        html += 'function calcImgSize(dims, md) {\n';
        html += '  if (!dims || dims == "") return {w: md, h: md};\n';
        html += '  var x = dims.indexOf("x");\n';
        html += '  if (x == -1) return {w: md, h: md};\n';
        html += '  var w = parseInt(dims.substring(0, x), 10) || 1;\n';
        html += '  var h = parseInt(dims.substring(x + 1), 10) || 1;\n';
        html += '  if (w >= h) {\n';
        html += '    if (w > md) { h = Math.round(h * md / w); w = md; }\n';
        html += '  } else {\n';
        html += '    if (h > md) { w = Math.round(w * md / h); h = md; }\n';
        html += '  }\n';
        html += '  if (w < 16) w = 16;\n';
        html += '  if (h < 16) h = 16;\n';
        html += '  return {w: w, h: h};\n';
        html += '}\n';
        
        // === ФОРМАТИРОВАНИЕ РАЗМЕРА ФАЙЛА ===
        html += 'function formatSize(b) {\n';
        html += '  if (b < 1024) return b + " б";\n';
        html += '  if (b < 1048576) return Math.round(b / 1024) + " Кб";\n';
        html += '  return (Math.round(b / 10485.76) / 100) + " Мб";\n';
        html += '}\n';
        
        // === ГЕНЕРАЦИЯ HTML ОДНОЙ МИНИАТЮРЫ ===
        html += 'function makeThumb(img, md, em, sd, ss, pos, tot) {\n';
        html += '  var sz = calcImgSize(img.ds, md);\n';
        html += '  var im = deletedUIDs[img.uid] ? true : false;\n';
        html += '  var isSel = (highlightedUid === img.uid);\n';
        html += '  var cm = !img.cv && !img.ul && pos >= 0;\n';
        html += '  var s = \'<div class="thumb-item\' + (im ? \' marked\' : \'\') + (isSel ? \' selected\' : \'\') + \'" id="thumb_\' + img.uid + \'" onmousedown="dragStart(\' + img.uid + \',\' + pos + \')">\';\n';
        // Крестик удаления (только в режиме редактирования и только для обычных картинок)
        html += '  if (em && !img.cv && !img.ul) {\n';
        html += '    s += \'<button class="edit-btn delete-btn" onclick="markForDelete(\' + img.uid + \')\" title="Пометить на удаление">\' + (im ? "↺" : "×") + \'</button>\';\n';
        html += '  }\n';
        // Картинка или заглушка
        html += '  if (img.em == 1) {\n';
        html += '    s += \'<div class="empty-placeholder" style="width:\' + sz.w + \'px;height:\' + sz.h + \'px;font-size:\' + Math.floor(Math.min(sz.w, sz.h) / 2) + \'px;display:flex;align-items:center;justify-content:center;">✕</div>\';\n';
        html += '  } else if (img.fn) {\n';
        html += '    s += \'<img src="\' + img.fn + \'" width="\' + sz.w + \'" height="\' + sz.h + \'" class="thumb-img" onclick="showOverlay(\\\'\' + img.fn + \'\\\')">\';\n';
        html += '  } else {\n';
        html += '    s += \'<div class="empty-placeholder" style="width:\' + sz.w + \'px;height:\' + sz.h + \'px;font-size:\' + Math.floor(Math.min(sz.w, sz.h) / 3) + \'px;display:flex;align-items:center;justify-content:center;">?</div>\';\n';
        html += '  }\n';
        // Подпись
        html += '  var cap = img.displayName || img.fn || "?";\n';
        html += '  if (img.em == 1) cap = "пустышка";\n';
        html += '  if (img.cv == 1) cap = "ОБЛОЖКА: " + cap;\n';
        html += '  if (img.ul == 1) cap = "не прилинкован: " + cap;\n';
        html += '  s += \'<div class="thumb-caption" onclick="selectThumb(\' + img.uid + \')\" title="Выделить картинку">\' + cap + \'</div>\';\n';
        // Дополнительная информация
        html += '  if (sd && img.ds && img.ds != "") s += \'<div class="thumb-info" onclick="selectThumb(\' + img.uid + \')\">\' + img.ds + \'px</div>\';\n';
        html += '  if (ss && img.sz && img.sz > 0) s += \'<div class="thumb-info" onclick="selectThumb(\' + img.uid + \')\">\' + formatSize(img.sz) + \'</div>\';\n';
        html += '  s += \'</div>\';\n';
        html += '  return s;\n';
        html += '}\n';
        html += '</script>\n';
        html += '</body>\n</html>';
        
        // === Сохраняем HTML во временную папку ===
        var htmlFilePath = tempDir + '_thumbs.html';
        try {
            var textFile = fso.CreateTextFile(htmlFilePath, true, true);
            textFile.Write(html);
            textFile.Close();
        } catch (e) {
            MsgBox(scriptName + "\nver. " + scriptVersion + "\n\nНе удалось сохранить HTML файл.");
            return;
        }
        
        // === Открываем HTML в отдельном окне ===
        var fileUrl = 'file:///' + editorPathDecoded.replace(/\\/g, '/') + 'temp_thumbs/_thumbs.html';
        fileUrl = fileUrl.replace(/ /g, '%20');
        
        var MsgWindow = window.open(fileUrl, '_blank', "height=680,width=850,status=no,toolbar=no,menubar=no,location=no,scrollbars=yes,resizable=yes");
        if (!MsgWindow) {
            MsgBox(scriptName + "\nver. " + scriptVersion + "\n\nНе удалось открыть окно.");
            return;
        }
        
        // ==================================================
        // ТАЙМЕР: ПОСЛЕ ЗАКРЫТИЯ ОКНА ПРИМЕНЯЕМ ИЗМЕНЕНИЯ
        // ==================================================
        
        function cleanupWhenClosed() {
            try {
                if (MsgWindow.closed) {
                    var hasChanges = false;
                    
                    try {
                        var fso2 = new ActiveXObject("Scripting.FileSystemObject");
                        var swEx = fso2.FileExists(swapsFilePath);
                        var delEx = fso2.FileExists(deletedFilePath);
                        
                        if (swEx || delEx) {
                            // Формируем единое окно подтверждения
                            var msg = scriptName + '\nver. ' + scriptVersion + '\n\nСделаны изменения:\n\n';
                            var doSwaps = false;
                            var doDeletes = false;
                            var deletedNames = [];
                            var finalOrderIdx = [];
                            
                            // Проверяем файл перестановок
                            if (swEx) {
                                var f = fso2.OpenTextFile(swapsFilePath, 1, false, 0);
                                var c = f.ReadAll();
                                f.Close();
                                
                                if (c && c.replace(/^\s+|\s+$/g, '') !== '') {
                                    var lines = c.split('\n');
                                    for (var p = 0; p < lines.length; p++) {
                                        var line = lines[p].replace(/^\s+|\s+$/g, '').replace(/[\uFEFF\u0000]/g, '');
                                        if (line !== '') finalOrderIdx.push(parseInt(line, 10));
                                    }
                                    
                                    var isChanged = false;
                                    for (var i = 0; i < finalOrderIdx.length; i++) {
                                        if (finalOrderIdx[i] !== i) { isChanged = true; break; }
                                    }
                                    if (isChanged) {
                                        msg += '• Изменён порядок иллюстраций\n';
                                        doSwaps = true;
                                    }
                                }
                            }
                            
                            // Проверяем файл удалений
                            if (delEx) {
                                var f = fso2.OpenTextFile(deletedFilePath, 1, false, 0);
                                var c = f.ReadAll();
                                f.Close();
                                
                                if (c && c.replace(/^\s+|\s+$/g, '') !== '') {
                                    var lines = c.split('\n');
                                    var uids = [];
                                    for (var p = 0; p < lines.length; p++) {
                                        var line = lines[p].replace(/^\s+|\s+$/g, '').replace(/[\uFEFF\u0000]/g, '');
                                        if (line !== '') uids.push(parseInt(line, 10));
                                    }
                                    
                                    if (uids.length > 0) {
                                        for (var i = 0; i < uids.length; i++) {
                                            if (uids[i] >= 0 && uids[i] < allImages.length) {
                                                deletedNames.push(allImages[uids[i]].displayName || allImages[uids[i]].fileName);
                                            }
                                        }
                                        msg += '• Помечены на удаление ссылки на иллюстрации (' + uids.length + ' шт.):\n  ' + deletedNames.join(', ') + '\n';
                                        doDeletes = true;
                                    }
                                }
                            }
                            
                            msg += '\nПрименить все изменения к документу?\n\n(Бинарные файлы при удалении не удаляются)';
                            
                            if ((doSwaps || doDeletes) && AskYesNo(msg)) {
                                window.external.BeginUndoUnit(document, scriptName + ' v.' + scriptVersion);
                                
                                // Шаг 1: Применяем перестановки (сначала!)
                                if (doSwaps && finalOrderIdx.length > 0) {
                                    var fbw3 = document.getElementById("fbw_body");
                                    if (fbw3) {
                                        var ae = [];
                                        var allEls = fbw3.getElementsByTagName('*');
                                        for (var i = 0; i < allEls.length; i++) {
                                            var el = allEls[i];
                                            var cls = el.className || '';
                                            if (typeof cls === 'string' && cls.toLowerCase() == 'image') {
                                                ae.push(el);
                                            }
                                        }
                                        for (var i = 0; i < Math.min(ae.length, finalOrderIdx.length); i++) {
                                            var srcIdx = finalOrderIdx[i];
                                            if (srcIdx >= 0 && srcIdx < allImages.length) {
                                                var newHref = '#' + (allImages[srcIdx].isEmpty ? 'undefined' : allImages[srcIdx].fileName);
                                                var oldHref = ae[i].getAttribute("href") || "";
                                                if (oldHref !== newHref) {
                                                    ae[i].setAttribute("href", newHref);
                                                    var imgs = ae[i].getElementsByTagName("IMG");
                                                    if (imgs.length > 0) {
                                                        imgs[0].src = "fbw-internal:" + newHref;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                
                                // Шаг 2: Применяем удаления (после перемещений!)
                                if (doDeletes && deletedNames.length > 0) {
                                    var fbw3 = document.getElementById("fbw_body");
                                    if (fbw3) {
                                        var allEls = fbw3.getElementsByTagName('*');
                                        var ae = [];
                                        for (var i = 0; i < allEls.length; i++) {
                                            var el = allEls[i];
                                            var cls = el.className || '';
                                            if (typeof cls === 'string' && cls.toLowerCase() == 'image') {
                                                ae.push(el);
                                            }
                                        }
                                        
                                        var uids = [];
                                        for (var i = 0; i < allImages.length; i++) {
                                            for (var d = 0; d < deletedNames.length; d++) {
                                                if ((allImages[i].displayName || allImages[i].fileName) === deletedNames[d]) {
                                                    uids.push(i);
                                                    break;
                                                }
                                            }
                                        }
                                        
                                        // Сортируем UID по убыванию для удаления с конца
                                        var sorted = [];
                                        for (var i = 0; i < uids.length; i++) sorted.push(uids[i]);
                                        for (var i = 0; i < sorted.length - 1; i++) {
                                            for (var j = i + 1; j < sorted.length; j++) {
                                                if (sorted[i] < sorted[j]) {
                                                    var t = sorted[i];
                                                    sorted[i] = sorted[j];
                                                    sorted[j] = t;
                                                }
                                            }
                                        }
                                        
                                        for (var d = 0; d < sorted.length; d++) {
                                            var uid = sorted[d];
                                            if (uid >= 0 && uid < ae.length) {
                                                try { ae[uid].removeNode(true); } catch(e4) {}
                                            }
                                        }
                                    }
                                }
                                
                                // Костыль для FBE: обновляем outerHTML всех картинок
                                try {
                                    var allImgs = document.getElementsByTagName("IMG");
                                    for (var i = allImgs.length - 1; i >= 0; i--) {
                                        try { allImgs[i].outerHTML = allImgs[i].outerHTML; } catch(e) {}
                                    }
                                } catch(e) {}
                                
                                window.external.EndUndoUnit(document);
                                hasChanges = true;
                                MsgBox(scriptName + '\nver. ' + scriptVersion + '\n\nИзменения применены к документу.');
                            }
                            
                            // Удаляем файлы изменений
                            if (swEx) fso2.DeleteFile(swapsFilePath, true);
                            if (delEx) fso2.DeleteFile(deletedFilePath, true);
                        }
                    } catch(e) {}
                    
                    // Обновляем списки обложек
                    if (hasChanges) {
                        try { if (typeof FillCoverList === "function") FillCoverList(); } catch(e) {}
                    }
                    
                    // Очищаем временную папку
                    try {
                        var fso4 = new ActiveXObject("Scripting.FileSystemObject");
                        if (fso4.FolderExists(tempDir)) {
                            var fld = fso4.GetFolder(tempDir);
                            var en = new Enumerator(fld.Files);
                            var rf = [];
                            for (; !en.atEnd(); en.moveNext()) {
                                rf.push(en.item());
                            }
                            for (var i = rf.length - 1; i >= 0; i--) {
                                try { rf[i].Delete(true); } catch(e) {}
                            }
                            fso4.DeleteFolder(tempDir, true);
                        }
                    } catch(e) {}
                    
                    // Очищаем ссылку на массив элементов
                    try { window._thumbsElements = null; } catch(e) {}
                    return;
                }
                window.setTimeout(cleanupWhenClosed, 500);
            } catch(e) {}
        }
        window.setTimeout(cleanupWhenClosed, 1000);
        
    } catch (error) {
        MsgBox(scriptName + "\nver. " + scriptVersion + "\n\nОшибка выполнения скрипта:\n" + error.message);
    }
}

// ==================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ (в основном скрипте FBE)
// ==================================================

// Преобразование объекта иллюстрации в JSON-строку для вставки в JavaScript
function thumbToJson(img) {
    var ds = '';
    var sz = 0;
    if (img.binInfo) {
        ds = img.binInfo.dims || '';
        sz = img.binInfo.size || 0;
    }
    var s = '{';
    s += 'uid:' + img.uid + ',';
    s += 'fn:"' + escapeJsString(img.fileName) + '",';
    s += 'dn:"' + escapeJsString(img.displayName) + '",';
    s += 'bl:' + (img.isBlock ? '1' : '0') + ',';
    s += 'em:' + (img.isEmpty ? '1' : '0') + ',';
    s += 'cv:' + (img.isCover ? '1' : '0') + ',';
    s += 'ul:' + (img.isUnlinked ? '1' : '0') + ',';
    s += 'ds:"' + escapeJsString(ds) + '",';
    s += 'sz:' + sz;
    s += '}';
    return s;
}

// Форматирование размера файла в читаемый вид
function formatFileSize(bytes) {
    if (bytes <= 0) return "0 байт";
    if (bytes < 1048576) {
        return Math.round(bytes / 1024) + " Кб";
    }
    return (Math.round(bytes / 10485.76) / 100) + " Мб";
}

// Экранирование строки для безопасной вставки в JavaScript
function escapeJsString(s) {
    if (!s) return '';
    var r = '';
    for (var i = 0; i < s.length; i++) {
        var c = s.charAt(i);
        if (c == '\\') r += '\\\\';
        else if (c == '"') r += '\\"';
        else if (c == '\n') r += '\\n';
        else if (c == '\r') r += '\\r';
        else if (c == '\t') r += '\\t';
        else r += c;
    }
    return r;
}

// Декодирование URI (упрощённая версия для IE6)
function decodeURIComponent(s) {
    if (!s) return '';
    return s.replace(/%20/g, ' ');
}

// Генерация HTML для начального отображения миниатюр
function generateThumbsHTML(all, unl, cov, sz, ord, bc, ic) {
    var md = 96;
    if (sz == 'small') md = 48;
    if (sz == 'large') md = 192;
    var h = '';
    
    if (ord === 'blockfirst') {
        var bi = [], ii = [];
        for (var i = 0; i < all.length; i++) {
            if (all[i].isBlock) bi.push(i); else ii.push(i);
        }
        if (bi.length > 0) {
            h += '<div class="separator">▼ Блочные иллюстрации (' + bi.length + '):</div>\n';
            for (var i = 0; i < bi.length; i++) h += genItem(all[bi[i]], md, false, i, bi.length);
        }
        if (ii.length > 0) {
            h += '<div class="separator">▼ Инлайн иллюстрации (' + ii.length + '):</div>\n';
            for (var i = 0; i < ii.length; i++) h += genItem(all[ii[i]], md, false, i, ii.length);
        }
    } else {
        for (var i = 0; i < all.length; i++) h += genItem(all[i], md, false, i, all.length);
    }
    
    var ex = (unl.length > 0 || cov);
    if (ex) {
        h += '<div class="separator">▼ Неприлинкованные файлы / Обложка:</div>\n';
        for (var i = 0; i < unl.length; i++) h += genItem(unl[i], md, false, -1, 0);
        if (cov) h += genItem(cov, md, false, -1, 0);
    }
    return h;
}

// Генерация HTML одного элемента миниатюры (для начальной отрисовки)
function genItem(img, md, em, pos, tot) {
    var w = md, h = md;
    if (img.binInfo && img.binInfo.dims) {
        var ds = img.binInfo.dims;
        var x = ds.indexOf('x');
        if (x !== -1) {
            var ow = parseInt(ds.substring(0, x), 10) || 1;
            var oh = parseInt(ds.substring(x + 1), 10) || 1;
            if (ow >= oh) {
                if (ow > md) { h = Math.round(oh * md / ow); w = md; }
                else { w = ow; h = oh; }
            } else {
                if (oh > md) { w = Math.round(ow * md / oh); h = md; }
                else { w = ow; h = oh; }
            }
            if (w < 16) w = 16;
            if (h < 16) h = 16;
        }
    }
    
    var s = '<div class="thumb-item" id="thumb_' + img.uid + '" onmousedown="dragStart(' + img.uid + ',' + pos + ')">';
    if (em && !img.isCover && !img.isUnlinked) {
        s += '<button class="edit-btn delete-btn" onclick="markForDelete(' + img.uid + ')" title="Пометить на удаление">×</button>';
    }
    
    if (img.isEmpty) {
        s += '<div class="empty-placeholder" style="width:' + w + 'px;height:' + h + 'px;font-size:' + Math.floor(Math.min(w, h) / 2) + 'px;display:flex;align-items:center;justify-content:center;">✕</div>';
    } else if (img.fileName) {
        s += '<img src="' + img.fileName + '" width="' + w + '" height="' + h + '" class="thumb-img" onclick="showOverlay(\'' + img.fileName + '\')">';
    } else {
        s += '<div class="empty-placeholder" style="width:' + w + 'px;height:' + h + 'px;font-size:' + Math.floor(Math.min(w, h) / 3) + 'px;display:flex;align-items:center;justify-content:center;">?</div>';
    }
    
    var cap = img.displayName || img.fileName || '?';
    if (img.isEmpty) cap = 'пустышка';
    if (img.isCover) cap = 'ОБЛОЖКА: ' + cap;
    if (img.isUnlinked) cap = 'не прилинкован: ' + cap;
    s += '<div class="thumb-caption" onclick="selectThumb(' + img.uid + ')" title="Выделить картинку">' + cap + '</div>';
    
    if (img.binInfo && img.binInfo.dims) {
        s += '<div class="thumb-info" onclick="selectThumb(' + img.uid + ')">' + img.binInfo.dims + 'px</div>';
    }
    
    s += '</div>\n';
    return s;
}
