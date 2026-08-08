// Скрипт "Добавить авторов или переводчиков из выделенного фрагмента текста" для редактора FBE
// version 2.4
// Идея - TaKir
// Реализация - DeepSeek, TaKir
// version 2.4, 17.12.2025

// Скрипт добавляет авторов и переводчиков книги в соответствующие поля дескрипшена fb2 файла.
// Добавление происходит из выделенного фрагмента.
// Можно добавлять только авторов или только переводчиков или и тех и других сразу.
// Список авторов (переводчиков) может быть в виде столбика с нужными ФИО или одной строкой, через запятую с пробелом.
// М. Лермонтов, А. Пушкин, Владимир Высоцкий, Осип Мандельштам, Валентин Петрович Катаев (и т.д.)
// Скрипт умеет работать с дублями авторов (переводчиков).
// Если в дескрипшене уже есть А. Пушкин, а в выделенном списке добавляемых авторов есть А.С. Пушкин
// скрипт обновит данные такого автора на более полную версию и сообщит об этом.
// Для удобства добавления перед списком ФИО можно указать отдельной строкой
// Авторы:
// или
// Переводчики:
// В любом регистре, с двоеточием или без
// Тогда скрипт сразу понимает, кого в какие поля записывать и не задает дополнительных вопросов.
// Скрипт умеет определять, есть ли вообще ФИО в выделенном фрагменте.
// Скрипт также умеет определять строки с НЕ ФИО в выделенном фрагменте.

// Для того, чтобы на вкладке дескрипшена заработали "крестики"
// удаления свежедобавленных авторов (переводчиков), надо перейти в режим кода (S) и обратно.
//======================================

function Run() {
    var ScriptName = "Добавить авторов или переводчиков из выделенного фрагмента текста";
    var NumerusVersion = "2.4";
    var Ts = new Date().getTime();
    
    // НАСТРОЙКИ
    var DefaultChoice = 1; // 1 - Авторы, 2 - Переводчики
    var MinValidPercentage = 60; // Минимальный процент строк, которые выглядят как ФИО
    
    //======================================
    
    // Получаем неразрывный пробел из настроек FBE
    var nbspEntity;
    var nbspChar;
    try {
        nbspChar = window.external.GetNBSP();
        if (nbspChar.charCodeAt(0) == 160) {
            nbspEntity = "&nbsp;";
        } else {
            nbspEntity = nbspChar;
        }
    } catch(e) {
        nbspChar = String.fromCharCode(160);
        nbspEntity = "&nbsp;";
    }
    
    // Проверяем наличие выделенного текста
    if (!document.selection) {
        MsgBox(ScriptName + " v." + NumerusVersion + "\n\nПожалуйста, выделите текст с именами авторов перед запуском скрипта.", "FBE скрипт");
        return;
    }
    
    var myRange = document.selection.createRange();
    var selectedText = myRange.text;
    
    if (!selectedText) {
        MsgBox(ScriptName + " v." + NumerusVersion + "\n\nВыделенный текст пуст.", "FBE скрипт");
        return;
    }
    
    // ВАЖНО: Заменяем все неразрывные пробелы на обычные пробелы перед анализом
    // Создаем RegExp для замены всех неразрывных пробелов
    var nbspPattern;
    if (nbspChar) {
        nbspPattern = new RegExp(nbspChar, 'g');
        selectedText = selectedText.replace(nbspPattern, ' ');
    }
    // Также заменяем HTML entity неразрывного пробела
    selectedText = selectedText.replace(/&nbsp;/g, ' ');
    
    // Удаляем начальные и конечные пробелы (старый способ для IE6)
    selectedText = selectedText.replace(/^\s+|\s+$/g, '');
    
    // Удаляем все пробельные символы для проверки
    var checkText = selectedText.replace(/\s+/g, '');
    if (checkText == '') {
        MsgBox(ScriptName + " v." + NumerusVersion + "\n\nВыделенный текст содержит только пробельные символы.", "FBE скрипт");
        return;
    }
    
    // ПРОВЕРКА: действительно ли в тексте есть ФИО (НОВАЯ ПРОВЕРКА В 2.4)
    var validationResult = validateTextContainsNames(selectedText, MinValidPercentage);
    
    if (!validationResult.isValid) {
        var errorMsg = ScriptName + " v." + NumerusVersion + "\n\n" +
                      "В выделенном тексте не обнаружено ФИО авторов/переводчиков.\n\n";
        
        if (validationResult.suspiciousLines && validationResult.suspiciousLines.length > 0) {
            errorMsg += "Сомнительные строки:\n";
            for (var i = 0; i < Math.min(validationResult.suspiciousLines.length, 10); i++) {
                var lineInfo = validationResult.suspiciousLines[i];
                errorMsg += (i+1) + ". Строка " + lineInfo.lineNumber + ": \"" + lineInfo.text + "\"\n";
                if (lineInfo.reason) {
                    errorMsg += "   Причина: " + lineInfo.reason + "\n";
                }
            }
            if (validationResult.suspiciousLines.length > 10) {
                errorMsg += "... и еще " + (validationResult.suspiciousLines.length - 10) + " строк\n";
            }
        }
        
        errorMsg += "\nТребования к ФИО:\n" +
                   "• Должны содержать заглавные буквы\n" +
                   "• Могут быть инициалы с точками (А. С. Пушкин)\n" +
                   "• Разделители: запятые, точки с запятой, пробелы\n" +
                   "• Допускаются дефисы в фамилиях (Петров-Водкин)\n\n" +
                   "Пожалуйста, проверьте выделенный текст.";
        
        MsgBox(errorMsg, "FBE скрипт");
        return;
    }
    
    // Если есть сомнительные строки, но общий процент допустим - спросить пользователя
    if (validationResult.suspiciousLines && validationResult.suspiciousLines.length > 0) {
        var warningMsg = ScriptName + " v." + NumerusVersion + "\n\n" +
                        "Найдены строки, которые могут не быть ФИО:\n\n";
        
        for (var i = 0; i < Math.min(validationResult.suspiciousLines.length, 5); i++) {
            var lineInfo = validationResult.suspiciousLines[i];
            warningMsg += (i+1) + ". \"" + lineInfo.text + "\"\n";
            if (lineInfo.reason) {
                warningMsg += "   (" + lineInfo.reason + ")\n";
            }
        }
        
        if (validationResult.suspiciousLines.length > 5) {
            warningMsg += "... и еще " + (validationResult.suspiciousLines.length - 5) + " строк\n";
        }
        
        warningMsg += "\nПродолжить обработку этих строк как ФИО?";
        
        var continueAnyway = AskYesNo(warningMsg, "FBE скрипт");
        if (continueAnyway === null || !continueAnyway) {
            return;
        }
    }
    
    // Сохраняем диапазон выделения для возможного удаления
    var originalSelectionRange = myRange.duplicate();
    
    // Получаем доступ к description
    var fbwDesc = document.getElementById("fbw_desc");
    if (!fbwDesc) {
        MsgBox(ScriptName + " v." + NumerusVersion + "\n\nОшибка: не найден раздел description.", "FBE скрипт");
        return;
    }
    
    // Получаем все элементы description
    var mDesc = fbwDesc.all;
    
    // ПРОВЕРКА НА ЗАГОЛОВКИ В ВЫДЕЛЕННОМ ТЕКСТЕ
    var sections = detectSectionsImproved(selectedText);
    
    // Определяем, какие разделы найдены
    var hasAuthorsHeader = sections.authors.hasHeader;
    var hasTranslatorsHeader = sections.translators.hasHeader;
    
    // ЛОГИКА ВЫБОРА ТИПА ДОБАВЛЕНИЯ
    var processMode = 0; // 0 - не определен, 1 - только авторы, 2 - только переводчики, 3 - оба
    var authorsText = '';
    var translatorsText = '';
    var skipAuthorsHeader = false;
    var skipTranslatorsHeader = false;
    
    if (hasAuthorsHeader && hasTranslatorsHeader) {
        // ОБА ЗАГОЛОВКА ОБНАРУЖЕНЫ
        var confirmText = ScriptName + " v." + NumerusVersion + "\n\n" +
                         "Обнаружены заголовки: \"Авторы\" и \"Переводчики\"\n\n" +
                         "Добавляем авторов и переводчиков?";
        
        var confirmBoth = AskYesNo(confirmText, "FBE скрипт");
        
        if (confirmBoth === null) {
            return;
        }
        
        if (confirmBoth) {
            processMode = 3; // Оба
            authorsText = sections.authors.text;
            translatorsText = sections.translators.text;
            skipAuthorsHeader = true;
            skipTranslatorsHeader = true;
            
            // ПРЕДВАРИТЕЛЬНО извлекаем авторов и переводчиков для подсчета
            var authorsLines = authorsText.split(/\r\n|\n|\r/);
            var translatorsLines = translatorsText.split(/\r\n|\n|\r/);
            var authorsToCount = extractAuthorsFromLines(authorsLines, skipAuthorsHeader);
            var translatorsToCount = extractAuthorsFromLines(translatorsLines, skipTranslatorsHeader);
            
            if (authorsToCount.length > 0 || translatorsToCount.length > 0) {
                MsgBox(ScriptName + " v." + NumerusVersion + "\n\n" +
                      "Будет обработано:\n" +
                      "Авторов: " + authorsToCount.length + "\n" +
                      "Переводчиков: " + translatorsToCount.length + "\n\n" +
                      "Нажмите ОК для продолжения...", "FBE скрипт");
            } else {
                MsgBox(ScriptName + " v." + NumerusVersion + "\n\n" +
                      "Не найдено имен авторов или переводчиков для обработки.", "FBE скрипт");
                return;
            }
        } else {
            // Пользователь отказался
            return;
        }
        
    } else if (hasAuthorsHeader) {
        // ТОЛЬКО ЗАГОЛОВОК АВТОРОВ
        var confirmText = ScriptName + " v." + NumerusVersion + "\n\n" +
                         "Обнаружен заголовок: \"Авторы\"\n\n" +
                         "Добавляем авторов?";
        
        var confirmAuthors = AskYesNo(confirmText, "FBE скрипт");
        
        if (confirmAuthors === null) {
            return;
        }
        
        if (confirmAuthors) {
            processMode = 1; // Только авторы
            authorsText = sections.authors.text;
            skipAuthorsHeader = true;
        } else {
            return;
        }
        
    } else if (hasTranslatorsHeader) {
        // ТОЛЬКО ЗАГОЛОВОК ПЕРЕВОДЧИКОВ
        var confirmText = ScriptName + " v." + NumerusVersion + "\n\n" +
                         "Обнаружен заголовок: \"Переводчики\"\n\n" +
                         "Добавляем переводчиков?";
        
        var confirmTranslators = AskYesNo(confirmText, "FBE скрипт");
        
        if (confirmTranslators === null) {
            return;
        }
        
        if (confirmTranslators) {
            processMode = 2; // Только переводчики
            translatorsText = sections.translators.text;
            skipTranslatorsHeader = true;
        } else {
            return;
        }
        
    } else {
        // НИ ОДНОГО ЗАГОЛОВКА НЕ НАЙДЕНО - СПРАШИВАЕМ ПОЛЬЗОВАТЕЛЯ
        var typeChoiceText = ScriptName + " v." + NumerusVersion + "\n\n" +
                            "Кого добавляем в описание книги?\n\n" +
                            "1. Авторов книги (title-info > author)\n" +
                            "2. Переводчиков (title-info > translator)\n\n" +
                            "Выберите вариант (Да = Авторы, Нет = Переводчики):";
        
        var choice = AskYesNo(typeChoiceText, "FBE скрипт");
        
        if (choice === null) {
            return;
        }
        
        processMode = choice ? 1 : 2; // 1 - авторы, 2 - переводчики
        authorsText = selectedText;
        translatorsText = selectedText;
    }
    
    // Подготавливаем данные для обработки
    var authorsToProcess = [];
    var translatorsToProcess = [];
    
    if (processMode == 1 || processMode == 3) {
        // Подготовка авторов
        if (authorsText) {
            var authorsLines = authorsText.split(/\r\n|\n|\r/);
            authorsToProcess = extractAuthorsFromLines(authorsLines, skipAuthorsHeader);
        }
    }
    
    if (processMode == 2 || processMode == 3) {
        // Подготовка переводчиков
        if (translatorsText) {
            var translatorsLines = translatorsText.split(/\r\n|\n|\r/);
            translatorsToProcess = extractAuthorsFromLines(translatorsLines, skipTranslatorsHeader);
        }
    }
    
    // Проверяем, есть ли что обрабатывать
    if ((processMode == 1 || processMode == 3) && authorsToProcess.length == 0) {
        MsgBox(ScriptName + " v." + NumerusVersion + "\n\nНе удалось извлечь имена авторов из выделенного текста.", "FBE скрипт");
        return;
    }
    
    if ((processMode == 2 || processMode == 3) && translatorsToProcess.length == 0) {
        MsgBox(ScriptName + " v." + NumerusVersion + "\n\nНе удалось извлечь имена переводчиков из выделенного текста.", "FBE скрипт");
        return;
    }
    
    // Анализируем и разбираем имена
    var processedAuthors = [];
    var processedTranslators = [];
    
    if (authorsToProcess.length > 0) {
        var analysis = analyzeNamesList(authorsToProcess);
        for (var i = 0; i < authorsToProcess.length; i++) {
            var author = parseAuthorNameSmart(authorsToProcess[i], analysis);
            if (author) {
                author = normalizeAuthorInitials(author);
                processedAuthors[processedAuthors.length] = author;
            }
        }
    }
    
    if (translatorsToProcess.length > 0) {
        var analysis = analyzeNamesList(translatorsToProcess);
        for (var i = 0; i < translatorsToProcess.length; i++) {
            var translator = parseAuthorNameSmart(translatorsToProcess[i], analysis);
            if (translator) {
                translator = normalizeAuthorInitials(translator);
                processedTranslators[processedTranslators.length] = translator;
            }
        }
    }
    
    // Показываем результаты разбора для подтверждения
    var confirmText = ScriptName + " v." + NumerusVersion + "\n\n";
    var needConfirmation = false;
    
    if (processedAuthors.length > 0) {
        confirmText += "Результаты разбора авторов (" + processedAuthors.length + "):\n";
        for (var i = 0; i < Math.min(processedAuthors.length, 10); i++) {
            var a = processedAuthors[i];
            confirmText += (i+1) + ". ";
            if (a.lastName) confirmText += "Ф: " + a.lastName + " ";
            if (a.firstName) confirmText += "И: " + a.firstName + " ";
            if (a.middleName) confirmText += "О: " + a.middleName;
            confirmText += "\n";
        }
        if (processedAuthors.length > 10) {
            confirmText += "... и еще " + (processedAuthors.length - 10) + " авторов\n";
        }
        confirmText += "\n";
        needConfirmation = true;
    }
    
    if (processedTranslators.length > 0) {
        confirmText += "Результаты разбора переводчиков (" + processedTranslators.length + "):\n";
        for (var i = 0; i < Math.min(processedTranslators.length, 10); i++) {
            var t = processedTranslators[i];
            confirmText += (i+1) + ". ";
            if (t.lastName) confirmText += "Ф: " + t.lastName + " ";
            if (t.firstName) confirmText += "И: " + t.firstName + " ";
            if (t.middleName) confirmText += "О: " + t.middleName;
            confirmText += "\n";
        }
        if (processedTranslators.length > 10) {
            confirmText += "... и еще " + (processedTranslators.length - 10) + " переводчиков\n";
        }
        confirmText += "\n";
        needConfirmation = true;
    }
    
    if (needConfirmation) {
        confirmText += "Добавить в описание книги?";
        var confirmAdd = AskYesNo(confirmText, "FBE скрипт");
        
        if (!confirmAdd) {
            return;
        }
    }
    
    // Начинаем транзакцию для отмены
    window.external.BeginUndoUnit(document, ScriptName);
    
    var totalAddedAuthors = 0;
    var totalAddedTranslators = 0;
    var totalUpdatedAuthors = 0;
    var totalUpdatedTranslators = 0;
    var totalDuplicateAuthors = 0;
    var totalDuplicateTranslators = 0;
    
    try {
        // ОБРАБОТКА АВТОРОВ (С УЛУЧШЕННОЙ ПРОВЕРКОЙ ДУБЛЕЙ)
        if (processedAuthors.length > 0) {
            var targetElement = mDesc.tiAuthor;
            if (targetElement) {
                var existingData = getExistingAuthorsWithElements(targetElement);
                var existingAuthors = existingData.authors;
                var authorElements = existingData.elements;
                
                for (var i = 0; i < processedAuthors.length; i++) {
                    var newAuthor = processedAuthors[i];
                    
                    var foundIndex = -1;
                    var isExactDuplicate = false;
                    
                    for (var j = 0; j < existingAuthors.length; j++) {
                        if (areAuthorsSimilar(newAuthor, existingAuthors[j])) {
                            foundIndex = j;
                            if (areAuthorsEqualExact(newAuthor, existingAuthors[j])) {
                                isExactDuplicate = true;
                            }
                            break;
                        }
                    }
                    
                    if (foundIndex != -1) {
                        if (isExactDuplicate) {
                            totalDuplicateAuthors++;
                        } else {
                            if (updateAuthorElement(authorElements[foundIndex], newAuthor)) {
                                totalUpdatedAuthors++;
                            }
                        }
                    } else {
                        if (addNewAuthorProper(targetElement, newAuthor, existingAuthors.length == 0)) {
                            totalAddedAuthors++;
                        }
                    }
                }
            }
        }
        
        // ОБРАБОТКА ПЕРЕВОДЧИКОВ (С УЛУЧШЕННОЙ ПРОВЕРКОЙ ДУБЛЕЙ)
        if (processedTranslators.length > 0) {
            var targetElement = mDesc.tiTrans;
            if (targetElement) {
                var existingData = getExistingAuthorsWithElements(targetElement);
                var existingTranslators = existingData.authors;
                var translatorElements = existingData.elements;
                
                for (var i = 0; i < processedTranslators.length; i++) {
                    var newTranslator = processedTranslators[i];
                    
                    var foundIndex = -1;
                    var isExactDuplicate = false;
                    
                    for (var j = 0; j < existingTranslators.length; j++) {
                        if (areAuthorsSimilar(newTranslator, existingTranslators[j])) {
                            foundIndex = j;
                            if (areAuthorsEqualExact(newTranslator, existingTranslators[j])) {
                                isExactDuplicate = true;
                            }
                            break;
                        }
                    }
                    
                    if (foundIndex != -1) {
                        if (isExactDuplicate) {
                            totalDuplicateTranslators++;
                        } else {
                            if (updateAuthorElement(translatorElements[foundIndex], newTranslator)) {
                                totalUpdatedTranslators++;
                            }
                        }
                    } else {
                        if (addNewAuthorProper(targetElement, newTranslator, existingTranslators.length == 0)) {
                            totalAddedTranslators++;
                        }
                    }
                }
            }
        }
        
        // УДАЛЕНИЕ ШАБЛОНА "ПЕРЕВОДЧИК ОДИН"
        if (processMode == 1 && processedTranslators.length == 0) {
            var translatorsElement = mDesc.tiTrans;
            if (translatorsElement) {
                var existingTranslators = getExistingAuthorsProper(translatorsElement);
                if (existingTranslators.length == 1) {
                    var firstTranslator = existingTranslators[0];
                    var firstNameLower = (firstTranslator.firstName || '').toLowerCase();
                    var lastNameLower = (firstTranslator.lastName || '').toLowerCase();
                    
                    if ((firstNameLower == 'переводчик' && lastNameLower == 'один') ||
                        (firstNameLower == 'translator' && lastNameLower == 'one') ||
                        (firstNameLower == '' && lastNameLower == '')) {
                        try {
                            var divs = translatorsElement.getElementsByTagName('DIV');
                            if (divs.length > 0) {
                                var firstDiv = divs[0];
                                if (firstDiv.parentNode) {
                                    firstDiv.parentNode.removeChild(firstDiv);
                                }
                            }
                        } catch(e) {
                        }
                    }
                }
            }
        }
        
        // ЗАПРОС НА УДАЛЕНИЕ ИСХОДНОГО ТЕКСТА
        if (totalAddedAuthors > 0 || totalAddedTranslators > 0 || totalUpdatedAuthors > 0 || totalUpdatedTranslators > 0) {
            var deleteQuestion = ScriptName + " v." + NumerusVersion + "\n\n" +
                               "Результаты добавления:\n";
            
            if (processedAuthors.length > 0) {
                deleteQuestion += "Авторы:\n" +
                                "  Добавлено новых: " + totalAddedAuthors + "\n" +
                                "  Обновлено существующих: " + totalUpdatedAuthors + "\n" +
                                "  Пропущено (точные дубли): " + totalDuplicateAuthors + "\n";
            }
            
            if (processedTranslators.length > 0) {
                deleteQuestion += "Переводчики:\n" +
                                "  Добавлено новых: " + totalAddedTranslators + "\n" +
                                "  Обновлено существующих: " + totalUpdatedTranslators + "\n" +
                                "  Пропущено (точные дубли): " + totalDuplicateTranslators + "\n";
            }
            
            deleteQuestion += "\nУдалить исходный выделенный фрагмент с текстом?";
            
            var deleteOriginal = AskYesNo(deleteQuestion, "FBE скрипт");
            
            if (deleteOriginal) {
                try {
                    originalSelectionRange.select();
                    originalSelectionRange.text = "";
                } catch(e) {
                }
            }
            
            // ФИНАЛЬНЫЙ РЕЗУЛЬТАТ
            var resultMessage = ScriptName + " v." + NumerusVersion + "\n\n" +
                              "Результаты работы скрипта:\n" +
                              "---------------------------\n";
            
            if (processedAuthors.length > 0) {
                resultMessage += "Авторы:\n" +
                               "  Добавлено новых: " + totalAddedAuthors + "\n" +
                               "  Обновлено существующих: " + totalUpdatedAuthors + "\n" +
                               "  Пропущено (точные дубли): " + totalDuplicateAuthors + "\n";
            }
            
            if (processedTranslators.length > 0) {
                resultMessage += "Переводчики:\n" +
                               "  Добавлено новых: " + totalAddedTranslators + "\n" +
                               "  Обновлено существующих: " + totalUpdatedTranslators + "\n" +
                               "  Пропущено (точные дубли): " + totalDuplicateTranslators + "\n";
            }
            
            resultMessage += "Исходный текст: " + (deleteOriginal ? "удален" : "сохранен") + "\n" +
                           "---------------------------";
            
            MsgBox(resultMessage, "FBE скрипт");
        } else {
            MsgBox(ScriptName + " v." + NumerusVersion + "\n\nНе было добавлено ни одного автора или переводчика.", "FBE скрипт");
        }
        
    } catch (error) {
        MsgBox(ScriptName + " v." + NumerusVersion + "\n\nОшибка при добавлении авторов и переводчиков.", "FBE скрипт");
    }
    
    // Завершаем транзакцию
    window.external.EndUndoUnit(document);
}

// ===================================================================
// НОВЫЕ ФУНКЦИИ ДЛЯ ВЕРСИИ 2.4 (проверка на наличие ФИО)
// ===================================================================

// Проверить, содержит ли текст ФИО
function validateTextContainsNames(text, minValidPercentage) {
    var lines = text.split(/\r\n|\n|\r/);
    var validLines = 0;
    var totalLines = 0;
    var suspiciousLines = [];
    
    for (var i = 0; i < lines.length; i++) {
        var line = lines[i];
        line = line.replace(/^\s+|\s+$/g, '');
        
        if (line == '') continue;
        
        totalLines++;
        
        // Проверяем, похожа ли строка на ФИО
        var lineCheck = checkLineForNames(line);
        
        if (lineCheck.isValid) {
            validLines++;
        } else {
            suspiciousLines[suspiciousLines.length] = {
                lineNumber: i + 1,
                text: line,
                reason: lineCheck.reason
            };
        }
    }
    
    var result = {
        isValid: false,
        validPercentage: 0,
        suspiciousLines: suspiciousLines
    };
    
    if (totalLines > 0) {
        result.validPercentage = Math.round((validLines / totalLines) * 100);
        result.isValid = result.validPercentage >= minValidPercentage;
    }
    
    return result;
}

// Проверить одну строку на наличие ФИО
function checkLineForNames(line) {
    var result = {
        isValid: false,
        reason: ''
    };
    
    // 1. Проверка на слишком короткую строку
    if (line.length < 2) {
        result.reason = 'Слишком короткая строка';
        return result;
    }
    
    // 2. Проверка на цифры (ФИО не должно состоять в основном из цифр)
    var digitCount = (line.match(/\d/g) || []).length;
    if (digitCount > line.length / 2) {
        result.reason = 'Слишком много цифр';
        return result;
    }
    
    // 3. Проверка на наличие заглавных букв
    var hasUppercase = /[А-ЯA-Z]/.test(line);
    if (!hasUppercase) {
        result.reason = 'Нет заглавных букв';
        return result;
    }
    
    // 4. Проверка на инициалы (должны быть с точками)
    var hasInitials = /[А-ЯA-Z]\./.test(line);
    
    // 5. Проверка на запрещенные последовательности
    var forbiddenPatterns = [
        /и т\.д\./i,
        /и т\.п\./i,
        /и др\./i,
        /и другие/i,
        /и прочие/i,
        /^[^а-яёa-z]*$/i, // Только не-буквы
        /^\d+$/, // Только цифры
        /http:\/\//i,
        /https:\/\//i,
        /www\./i,
        /\.(com|ru|org|net)$/i
    ];
    
    for (var i = 0; i < forbiddenPatterns.length; i++) {
        if (forbiddenPatterns[i].test(line)) {
            result.reason = 'Содержит запрещенный паттерн';
            return result;
        }
    }
    
    // 6. Проверка на структуру ФИО
    var words = line.split(/\s+/);
    var wordCount = words.length;
    
    // Если одно слово - должно быть с заглавной буквы и не слишком короткое
    if (wordCount == 1) {
        var word = words[0];
        if (word.length >= 2 && /^[А-ЯA-Z]/.test(word)) {
            result.isValid = true;
        } else {
            result.reason = 'Одно слово, но не похоже на фамилию';
        }
        return result;
    }
    
    // Для нескольких слов проверяем структуру
    var uppercaseWords = 0;
    var hasInitialWithDot = false;
    var hasFullName = false;
    
    for (var j = 0; j < words.length; j++) {
        var word = words[j];
        
        // Слово начинается с заглавной
        if (/^[А-ЯA-Z]/.test(word)) {
            uppercaseWords++;
        }
        
        // Инициал с точкой
        if (/^[А-ЯA-Z]\.$/.test(word)) {
            hasInitialWithDot = true;
        }
        
        // Полное имя (больше 2 букв, начинается с заглавной)
        if (/^[А-ЯA-Z][а-яёa-z]{1,}/.test(word)) {
            hasFullName = true;
        }
    }
    
    // 7. Проверка на достаточное количество слов с заглавной буквы
    var uppercasePercentage = (uppercaseWords / wordCount) * 100;
    if (uppercasePercentage < 50) {
        result.reason = 'Мало слов с заглавной буквы (' + Math.round(uppercasePercentage) + '%)';
        return result;
    }
    
    // 8. Должны быть либо инициалы с точками, либо полные имена
    if (!hasInitialWithDot && !hasFullName) {
        result.reason = 'Нет ни инициалов с точками, ни полных имен';
        return result;
    }
    
    // 9. Проверка на разделители (допустимы запятые, точки с запятой, дефисы)
    var validSeparators = /^[а-яёa-zА-ЯЁA-Z\s\-\.\,\;]+$/i;
    if (!validSeparators.test(line.replace(/[А-ЯA-Z]\./g, ''))) {
        result.reason = 'Содержит недопустимые символы';
        return result;
    }
    
    // 10. Проверка на слишком много точек (кроме инициалов)
    var dotCount = (line.match(/\./g) || []).length;
    var initialDotCount = (line.match(/[А-ЯA-Z]\./g) || []).length;
    
    if (dotCount > initialDotCount * 2) {
        result.reason = 'Слишком много точек';
        return result;
    }
    
    // Если все проверки пройдены
    result.isValid = true;
    return result;
}


// ===================================================================
// НОВЫЕ ФУНКЦИИ ДЛЯ ВЕРСИИ 2.4 (проверка на наличие ФИО)
// ===================================================================

// Проверить, содержит ли текст ФИО
function validateTextContainsNames(text, minValidPercentage) {
    var lines = text.split(/\r\n|\n|\r/);
    var validLines = 0;
    var totalLines = 0;
    var suspiciousLines = [];
    
    for (var i = 0; i < lines.length; i++) {
        var line = lines[i];
        line = line.replace(/^\s+|\s+$/g, '');
        
        if (line == '') continue;
        
        totalLines++;
        
        // Проверяем, похожа ли строка на ФИО
        var lineCheck = checkLineForNames(line);
        
        if (lineCheck.isValid) {
            validLines++;
        } else {
            suspiciousLines[suspiciousLines.length] = {
                lineNumber: i + 1,
                text: line,
                reason: lineCheck.reason
            };
        }
    }
    
    var result = {
        isValid: false,
        validPercentage: 0,
        suspiciousLines: suspiciousLines
    };
    
    if (totalLines > 0) {
        result.validPercentage = Math.round((validLines / totalLines) * 100);
        result.isValid = result.validPercentage >= minValidPercentage;
    }
    
    return result;
}

// Проверить одну строку на наличие ФИО
function checkLineForNames(line) {
    var result = {
        isValid: false,
        reason: ''
    };
    
    // 1. Проверка на слишком короткую строку
    if (line.length < 2) {
        result.reason = 'Слишком короткая строка';
        return result;
    }
    
    // 2. Проверка на цифры (ФИО не должно состоять в основном из цифр)
    var digitCount = (line.match(/\d/g) || []).length;
    if (digitCount > line.length / 2) {
        result.reason = 'Слишком много цифр';
        return result;
    }
    
    // 3. Проверка на наличие заглавных букв
    var hasUppercase = /[А-ЯA-Z]/.test(line);
    if (!hasUppercase) {
        result.reason = 'Нет заглавных букв';
        return result;
    }
    
    // 4. Проверка на инициалы (должны быть с точками)
    var hasInitials = /[А-ЯA-Z]\./.test(line);
    
    // 5. Проверка на запрещенные последовательности
    var forbiddenPatterns = [
        /и т\.д\./i,
        /и т\.п\./i,
        /и др\./i,
        /и другие/i,
        /и прочие/i,
        /^[^а-яёa-z]*$/i, // Только не-буквы
        /^\d+$/, // Только цифры
        /http:\/\//i,
        /https:\/\//i,
        /www\./i,
        /\.(com|ru|org|net)$/i
    ];
    
    for (var i = 0; i < forbiddenPatterns.length; i++) {
        if (forbiddenPatterns[i].test(line)) {
            result.reason = 'Содержит запрещенный паттерн';
            return result;
        }
    }
    
    // 6. Проверка на структуру ФИО
    var words = line.split(/\s+/);
    var wordCount = words.length;
    
    // Если одно слово - должно быть с заглавной буквы и не слишком короткое
    if (wordCount == 1) {
        var word = words[0];
        if (word.length >= 2 && /^[А-ЯA-Z]/.test(word)) {
            result.isValid = true;
        } else {
            result.reason = 'Одно слово, но не похоже на фамилию';
        }
        return result;
    }
    
    // Для нескольких слов проверяем структуру
    var uppercaseWords = 0;
    var hasInitialWithDot = false;
    var hasFullName = false;
    
    for (var j = 0; j < words.length; j++) {
        var word = words[j];
        
        // Слово начинается с заглавной
        if (/^[А-ЯA-Z]/.test(word)) {
            uppercaseWords++;
        }
        
        // Инициал с точкой
        if (/^[А-ЯA-Z]\.$/.test(word)) {
            hasInitialWithDot = true;
        }
        
        // Полное имя (больше 2 букв, начинается с заглавной)
        if (/^[А-ЯA-Z][а-яёa-z]{1,}/.test(word)) {
            hasFullName = true;
        }
    }
    
    // 7. Проверка на достаточное количество слов с заглавной буквы
    var uppercasePercentage = (uppercaseWords / wordCount) * 100;
    if (uppercasePercentage < 50) {
        result.reason = 'Мало слов с заглавной буквы (' + Math.round(uppercasePercentage) + '%)';
        return result;
    }
    
    // 8. Должны быть либо инициалы с точками, либо полные имена
    if (!hasInitialWithDot && !hasFullName) {
        result.reason = 'Нет ни инициалов с точками, ни полных имен';
        return result;
    }
    
    // 9. Проверка на разделители (допустимы запятые, точки с запятой, дефисы)
    var validSeparators = /^[а-яёa-zА-ЯЁA-Z\s\-\.\,\;]+$/i;
    if (!validSeparators.test(line.replace(/[А-ЯA-Z]\./g, ''))) {
        result.reason = 'Содержит недопустимые символы';
        return result;
    }
    
    // 10. Проверка на слишком много точек (кроме инициалов)
    var dotCount = (line.match(/\./g) || []).length;
    var initialDotCount = (line.match(/[А-ЯA-Z]\./g) || []).length;
    
    if (dotCount > initialDotCount * 2) {
        result.reason = 'Слишком много точек';
        return result;
    }
    
    // Если все проверки пройдены
    result.isValid = true;
    return result;
}

// ===================================================================
// ФУНКЦИИ ИЗ ВЕРСИИ 2.3 (улучшенная обработка дублей)
// ===================================================================

// Получить существующих авторов вместе с DOM-элементами
function getExistingAuthorsWithElements(authorElement) {
    var result = {
        authors: [],
        elements: []
    };
    
    try {
        var mDiv = authorElement.getElementsByTagName('DIV');
        
        for (var k = 0; k < mDiv.length; k++) {
            var div = mDiv[k];
            
            if (div.all) {
                var author = {
                    firstName: div.all.first ? div.all.first.value || '' : '',
                    middleName: div.all.middle ? div.all.middle.value || '' : '',
                    lastName: div.all.last ? div.all.last.value || '' : ''
                };
                
                var isEmpty = true;
                if (author.firstName && author.firstName.replace(/\s+/g, '') != '') isEmpty = false;
                if (author.middleName && author.middleName.replace(/\s+/g, '') != '') isEmpty = false;
                if (author.lastName && author.lastName.replace(/\s+/g, '') != '') isEmpty = false;
                
                if (!isEmpty) {
                    result.authors[result.authors.length] = author;
                    result.elements[result.elements.length] = div;
                }
            }
        }
        
    } catch(e) {
        try {
            var divs = authorElement.getElementsByTagName('DIV');
            for (var i = 0; i < divs.length; i++) {
                var div = divs[i];
                var inputs = div.getElementsByTagName('INPUT');
                
                var author = {
                    firstName: '',
                    middleName: '',
                    lastName: ''
                };
                
                for (var j = 0; j < inputs.length; j++) {
                    var input = inputs[j];
                    var name = input.getAttribute('name');
                    var value = input.value || '';
                    
                    if (name == 'first') author.firstName = value;
                    else if (name == 'middle') author.middleName = value;
                    else if (name == 'last') author.lastName = value;
                }
                
                var isEmpty = true;
                if (author.firstName && author.firstName.replace(/\s+/g, '') != '') isEmpty = false;
                if (author.middleName && author.middleName.replace(/\s+/g, '') != '') isEmpty = false;
                if (author.lastName && author.lastName.replace(/\s+/g, '') != '') isEmpty = false;
                
                if (!isEmpty) {
                    result.authors[result.authors.length] = author;
                    result.elements[result.elements.length] = div;
                }
            }
        } catch(e2) {
        }
    }
    
    return result;
}

// Проверить, являются ли авторы ПОХОЖИМИ (по фамилии и возможно имени)
function areAuthorsSimilar(author1, author2) {
    function normalize(str) {
        if (!str) return '';
        var temp = str.toLowerCase();
        temp = temp.replace(/\./g, '');
        temp = temp.replace(/\s+/g, '');
        temp = temp.replace(/ё/g, 'е');
        temp = temp.replace(/[^а-яa-z0-9]/g, '');
        return temp;
    }
    
    var lastName1 = normalize(author1.lastName);
    var lastName2 = normalize(author2.lastName);
    
    if (lastName1 != lastName2) {
        return false;
    }
    
    var firstName1 = normalize(author1.firstName);
    var firstName2 = normalize(author2.firstName);
    
    if (firstName1 == '' && firstName2 == '') {
        return true;
    }
    
    if (firstName1 == '' || firstName2 == '') {
        return true;
    }
    
    if (firstName1.charAt(0) == firstName2.charAt(0)) {
        return true;
    }
    
    return false;
}

// Проверить, являются ли авторы ТОЧНО ОДИНАКОВЫМИ
function areAuthorsEqualExact(author1, author2) {
    function normalize(str) {
        if (!str) return '';
        var temp = str.toLowerCase();
        temp = temp.replace(/\./g, '');
        temp = temp.replace(/\s+/g, '');
        temp = temp.replace(/ё/g, 'е');
        temp = temp.replace(/[^а-яa-z0-9]/g, '');
        return temp;
    }
    
    var lastName1 = normalize(author1.lastName);
    var lastName2 = normalize(author2.lastName);
    var firstName1 = normalize(author1.firstName);
    var firstName2 = normalize(author2.firstName);
    var middleName1 = normalize(author1.middleName);
    var middleName2 = normalize(author2.middleName);
    
    if (lastName1 != lastName2) return false;
    
    if (firstName1 != firstName2) {
        if ((firstName1 == '' && firstName2.length == 1) || 
            (firstName2 == '' && firstName1.length == 1)) {
            return false;
        }
        if (firstName1 != firstName2) return false;
    }
    
    if (middleName1 != middleName2) {
        if ((middleName1 == '' && middleName2.length == 1) || 
            (middleName2 == '' && middleName1.length == 1)) {
            return false;
        }
        if (middleName1 != middleName2) return false;
    }
    
    return true;
}

// Обновить существующий элемент автора
function updateAuthorElement(authorElement, newAuthor) {
    try {
        if (authorElement.all) {
            var currentFirstName = authorElement.all.first ? authorElement.all.first.value || '' : '';
            var currentMiddleName = authorElement.all.middle ? authorElement.all.middle.value || '' : '';
            var currentLastName = authorElement.all.last ? authorElement.all.last.value || '' : '';
            
            var needUpdate = false;
            
            if (newAuthor.firstName && newAuthor.firstName.replace(/\s+/g, '') != '') {
                var newFirstNameNorm = newAuthor.firstName.replace(/\./g, '').replace(/\s+/g, '').toLowerCase();
                var currentFirstNameNorm = currentFirstName.replace(/\./g, '').replace(/\s+/g, '').toLowerCase();
                
                if (currentFirstName == '' || 
                    (newFirstNameNorm.length > currentFirstNameNorm.length && 
                     newFirstNameNorm.charAt(0) == currentFirstNameNorm.charAt(0))) {
                    if (authorElement.all.first) {
                        authorElement.all.first.value = newAuthor.firstName;
                        needUpdate = true;
                    }
                }
            }
            
            if (newAuthor.middleName && newAuthor.middleName.replace(/\s+/g, '') != '') {
                var newMiddleNameNorm = newAuthor.middleName.replace(/\./g, '').replace(/\s+/g, '').toLowerCase();
                var currentMiddleNameNorm = currentMiddleName.replace(/\./g, '').replace(/\s+/g, '').toLowerCase();
                
                if (currentMiddleName == '' || 
                    (newMiddleNameNorm.length > currentMiddleNameNorm.length && 
                     newMiddleNameNorm.charAt(0) == currentMiddleNameNorm.charAt(0))) {
                    if (authorElement.all.middle) {
                        authorElement.all.middle.value = newAuthor.middleName;
                        needUpdate = true;
                    }
                }
            }
            
            if (newAuthor.lastName && newAuthor.lastName.replace(/\s+/g, '') != '') {
                if (currentLastName == '' || 
                    (newAuthor.lastName.toLowerCase().replace(/\s+/g, '') == 
                     currentLastName.toLowerCase().replace(/\s+/g, ''))) {
                    if (authorElement.all.last) {
                        authorElement.all.last.value = newAuthor.lastName;
                        needUpdate = true;
                    }
                }
            }
            
            return needUpdate;
        }
    } catch(e) {
        return false;
    }
    return false;
}

// ===================================================================
// ФУНКЦИИ ИЗ ПРЕДЫДУЩИХ ВЕРСИЙ (без изменений)
// ===================================================================

// Обнаружить разделы в тексте (ИСПРАВЛЕННАЯ В 2.2)
function detectSectionsImproved(text) {
    var lines = text.split(/\r\n|\n|\r/);
    var result = {
        authors: {
            text: '',
            hasHeader: false,
            startLine: -1,
            endLine: -1
        },
        translators: {
            text: '',
            hasHeader: false,
            startLine: -1,
            endLine: -1
        }
    };
    
    var authorsStart = -1;
    var translatorsStart = -1;
    
    for (var i = 0; i < lines.length; i++) {
        var line = lines[i].replace(/^\s+|\s+$/g, '');
        var lineLower = line.toLowerCase();
        
        if (lineLower.search(/^авторы\s*:?\s*$/) != -1 && authorsStart == -1) {
            authorsStart = i;
        } else if (lineLower.search(/^переводчики\s*:?\s*$/) != -1 && translatorsStart == -1) {
            translatorsStart = i;
        }
    }
    
    if (authorsStart != -1 && translatorsStart != -1) {
        result.authors.hasHeader = true;
        result.translators.hasHeader = true;
        
        if (authorsStart < translatorsStart) {
            result.authors.startLine = authorsStart;
            result.authors.endLine = translatorsStart;
            
            result.translators.startLine = translatorsStart;
            result.translators.endLine = lines.length;
        } else {
            result.translators.startLine = translatorsStart;
            result.translators.endLine = authorsStart;
            
            result.authors.startLine = authorsStart;
            result.authors.endLine = lines.length;
        }
    } else if (authorsStart != -1) {
        result.authors.hasHeader = true;
        result.authors.startLine = authorsStart;
        result.authors.endLine = lines.length;
    } else if (translatorsStart != -1) {
        result.translators.hasHeader = true;
        result.translators.startLine = translatorsStart;
        result.translators.endLine = lines.length;
    }
    
    if (result.authors.hasHeader && result.authors.startLine != -1) {
        var authorsLines = [];
        for (var i = result.authors.startLine; i < result.authors.endLine; i++) {
            authorsLines[authorsLines.length] = lines[i];
        }
        result.authors.text = authorsLines.join('\n');
    } else {
        result.authors.text = text;
    }
    
    if (result.translators.hasHeader && result.translators.startLine != -1) {
        var translatorsLines = [];
        for (var i = result.translators.startLine; i < result.translators.endLine; i++) {
            translatorsLines[translatorsLines.length] = lines[i];
        }
        result.translators.text = translatorsLines.join('\n');
    } else {
        result.translators.text = text;
    }
    
    return result;
}

// Старая функция для обратной совместимости
function detectSections(text) {
    return detectSectionsImproved(text);
}

// Извлечь авторов из строк с возможностью пропуска заголовка
function extractAuthorsFromLines(lines, skipHeader) {
    var authors = [];
    
    for (var i = 0; i < lines.length; i++) {
        var line = lines[i];
        line = line.replace(/^\s+|\s+$/g, '');
        
        if (i == 0 && skipHeader) {
            var lineLower = line.toLowerCase();
            if (lineLower.search(/^авторы\s*:?\s*$/) != -1 || 
                lineLower.search(/^переводчики\s*:?\s*$/) != -1) {
                continue;
            }
        }
        
        if (line == '') continue;
        
        if (line.search(/[,;]/) != -1) {
            var authorsInLine = line.split(/[,;]/);
            for (var j = 0; j < authorsInLine.length; j++) {
                var author = authorsInLine[j].replace(/^\s+|\s+$/g, '');
                if (author != '') {
                    authors[authors.length] = author;
                }
            }
        } else {
            authors[authors.length] = line;
        }
    }
    
    return authors;
}

// Нормализовать инициалы - добавить точки где нужно
function normalizeAuthorInitials(author) {
    if (!author) return author;
    
    function normalizeInitials(str) {
        if (!str) return str;
        
        var trimmed = str.replace(/^\s+|\s+$/g, '');
        
        if (/^[А-ЯA-Z](\s*[А-ЯA-Z])?\.?$/.test(trimmed)) {
            var noSpaces = trimmed.replace(/\s+/g, '');
            
            var result = '';
            for (var k = 0; k < noSpaces.length; k++) {
                result += noSpaces.charAt(k);
                var charCode = noSpaces.charCodeAt(k);
                if ((charCode >= 65 && charCode <= 90) || 
                    (charCode >= 1040 && charCode <= 1071)) {
                    if (k < noSpaces.length - 1 && noSpaces.charAt(k + 1) != '.') {
                        result += '.';
                    }
                }
            }
            
            if (result.charAt(result.length - 1) != '.') {
                result += '.';
            }
            
            return result;
        }
        
        return str;
    }
    
    if (author.firstName) {
        author.firstName = normalizeInitials(author.firstName);
    }
    
    if (author.middleName) {
        author.middleName = normalizeInitials(author.middleName);
    }
    
    return author;
}

// Анализировать весь список имен для определения паттернов
function analyzeNamesList(authors) {
    var analysis = {
        likelyOrder: 1,
        hasPatronymics: false,
        hasInitials: false,
        likelyRussian: false,
        englishNamesCount: 0,
        russianNamesCount: 0
    };
    
    var checkCount = authors.length < 5 ? authors.length : 5;
    
    for (var i = 0; i < checkCount; i++) {
        var name = authors[i];
        var words = name.split(' ');
        
        for (var j = 0; j < words.length; j++) {
            if (/(ович|евич|ич|овна|евна|ична|инична)$/i.test(words[j])) {
                analysis.hasPatronymics = true;
                analysis.likelyRussian = true;
                analysis.russianNamesCount++;
            }
        }
        
        for (var j = 0; j < words.length; j++) {
            if (/^[А-ЯA-Z]\.?(\s*[А-ЯA-Z]\.?)*$/.test(words[j])) {
                analysis.hasInitials = true;
            }
        }
        
        var hasEnglishLetters = /[A-Z]/.test(name) && !/[А-Яа-яЁё]/.test(name);
        var hasRussianLetters = /[А-Яа-яЁё]/.test(name);
        
        if (hasEnglishLetters && !hasRussianLetters) {
            analysis.englishNamesCount++;
        } else if (hasRussianLetters) {
            analysis.russianNamesCount++;
        }
        
        if (words.length == 2) {
            var word1 = words[0];
            var word2 = words[1];
            
            var word1IsLastName = false;
            var word2IsFirstName = false;
            
            if (/^[А-ЯA-Z]\.?$/.test(word2)) {
                word1IsLastName = true;
            }
            
            var lastNameEndings = [
                'ов$', 'ев$', 'ин$', 'ын$', 'ский$', 'цкий$', 'ской$', 'цкой$',
                'ова$', 'ева$', 'ина$', 'ына$', 'ская$', 'цкая$', 'ая$', 'яя$',
                'ко$', 'ук$', 'юк$', 'ак$', 'ик$', 'ек$', 'ёк$'
            ];
            
            for (var k = 0; k < lastNameEndings.length; k++) {
                var regex = new RegExp(lastNameEndings[k], 'i');
                if (regex.test(word1)) {
                    word1IsLastName = true;
                    break;
                }
            }
            
            var firstNameEndings = ['ий$', 'ей$', 'ай$', 'ой$', 'ль$', 'ор$', 'ир$', 'дим$', 'мир$', 'слав$', 'полк$'];
            for (var k = 0; k < firstNameEndings.length; k++) {
                var regex = new RegExp(firstNameEndings[k], 'i');
                if (regex.test(word2)) {
                    word2IsFirstName = true;
                    break;
                }
            }
            
            var commonRussianFirstNames = [
                'александр', 'алексей', 'анатолий', 'андрей', 'антон', 'аркадий', 'арсений', 'артем',
                'борис', 'вадим', 'валентин', 'валерий', 'василий', 'виктор', 'владимир', 'владислав',
                'всеволод', 'геннадий', 'георгий', 'григорий', 'даниил', 'денис', 'дмитрий', 'евгений',
                'егор', 'иван', 'игорь', 'илья', 'кирилл', 'константин', 'леонид', 'максим',
                'михаил', 'николай', 'олег', 'павел', 'петр', 'рома', 'руслан', 'сергей',
                'станислав', 'степан', 'тимур', 'федор', 'юрий', 'яков', 'ян'
            ];
            
            var word2Lower = word2.toLowerCase();
            for (var k = 0; k < commonRussianFirstNames.length; k++) {
                if (commonRussianFirstNames[k] == word2Lower) {
                    word2IsFirstName = true;
                    break;
                }
            }
            
            if (word1IsLastName && word2IsFirstName) {
                analysis.likelyOrder = 2;
            }
        }
    }
    
    if (analysis.russianNamesCount > analysis.englishNamesCount) {
        analysis.likelyRussian = true;
    }
    
    return analysis;
}

// Умный разбор имени автора с учетом анализа
function parseAuthorNameSmart(nameStr, analysis) {
    if (!nameStr || nameStr.replace(/\s+/g, '') == '') {
        return null;
    }
    
    nameStr = nameStr.replace(/\s+/g, ' ').replace(/^\s+|\s+$/g, '');
    var words = nameStr.split(' ');
    
    var cleanWords = [];
    for (var i = 0; i < words.length; i++) {
        if (words[i] != '') {
            cleanWords[cleanWords.length] = words[i];
        }
    }
    
    if (cleanWords.length == 0) {
        return null;
    }
    
    var result = {
        firstName: '',
        middleName: '',
        lastName: ''
    };
    
    function isInitials(word) {
        return /^[А-ЯA-Z]\.?(\s*[А-ЯA-Z]\.?)*$/.test(word);
    }
    
    function isPatronymic(word) {
        return /(ович|евич|ич|овна|евна|ична|инична)$/i.test(word);
    }
    
    function isRussianLastName(word) {
        if (isInitials(word)) return false;
        if (isPatronymic(word)) return false;
        if (word.length < 3) return false;
        
        var commonNames = [
            'валентин', 'валентина', 'максим', 'кирилл', 'данил', 'даниил',
            'артем', 'артём', 'станислав', 'владислав', 'всеволод'
        ];
        
        var wordLower = word.toLowerCase();
        for (var i = 0; i < commonNames.length; i++) {
            if (commonNames[i] == wordLower) {
                return false;
            }
        }
        
        var endings = [
            'ов$', 'ев$', 'ин$', 'ын$', 'ский$', 'цкий$', 'ской$', 'цкой$',
            'ова$', 'ева$', 'ина$', 'ына$', 'ская$', 'цкая$', 'ая$', 'яя$',
            'ко$', 'ук$', 'юк$', 'ак$', 'ик$', 'ек$', 'ёк$', 'енко$', 'юк$'
        ];
        
        for (var i = 0; i < endings.length; i++) {
            var regex = new RegExp(endings[i], 'i');
            if (regex.test(word)) {
                return true;
            }
        }
        
        return false;
    }
    
    function isRussianFirstName(word) {
        if (isInitials(word)) return false;
        if (word.length < 2) return false;
        
        var commonNames = [
            'александр', 'алексей', 'анатолий', 'андрей', 'антон', 'аркадий', 'арсений', 'артем', 'артём',
            'борис', 'вадим', 'валентин', 'валерий', 'василий', 'виктор', 'владимир', 'владислав',
            'всеволод', 'геннадий', 'георгий', 'григорий', 'даниил', 'денис', 'дмитрий', 'евгений',
            'егор', 'иван', 'игорь', 'илья', 'кирилл', 'константин', 'леонид', 'максим',
            'михаил', 'николай', 'олег', 'павел', 'петр', 'пётр', 'рома', 'руслан', 'сергей',
            'станислав', 'степан', 'тимур', 'федор', 'фёдор', 'юрий', 'яков', 'ян',
            'анна', 'мария', 'елена', 'ольга', 'наталья', 'ирина', 'светлана', 'татьяна',
            'екатерина', 'надежда', 'любовь', 'валентина', 'юлия', 'александра', 'вероника'
        ];
        
        var wordLower = word.toLowerCase();
        for (var i = 0; i < commonNames.length; i++) {
            if (commonNames[i] == wordLower) {
                return true;
            }
        }
        
        var endings = ['ий$', 'ей$', 'ай$', 'ой$', 'ль$', 'ор$', 'ир$', 'дим$', 'мир$', 'слав$', 'полк$', 'на$', 'та$', 'ра$', 'ла$'];
        for (var i = 0; i < endings.length; i++) {
            var regex = new RegExp(endings[i], 'i');
            if (regex.test(word)) {
                return true;
            }
        }
        
        return false;
    }
    
    for (var i = 0; i < cleanWords.length; i++) {
        var word = cleanWords[i];
        if (word.indexOf('.') != -1 && word.length <= 4) {
            var parts = word.split('.');
            var newWord = '';
            for (var j = 0; j < parts.length; j++) {
                if (parts[j] != '') {
                    newWord += parts[j].charAt(0).toUpperCase() + '.';
                    if (j < parts.length - 1 && parts[j+1] != '') {
                        newWord += ' ';
                    }
                }
            }
            cleanWords[i] = newWord.replace(/^\s+|\s+$/g, '');
        }
    }
    
    if (cleanWords.length == 1) {
        result.lastName = cleanWords[0];
        
    } else if (cleanWords.length == 2) {
        var word1 = cleanWords[0];
        var word2 = cleanWords[1];
        
        if (isInitials(word2)) {
            result.lastName = word1;
            result.firstName = word2;
        }
        else if (isInitials(word1)) {
            result.firstName = word1;
            result.lastName = word2;
        }
        else {
            var word1IsLastName = isRussianLastName(word1);
            var word2IsFirstName = isRussianFirstName(word2);
            var word1IsFirstName = isRussianFirstName(word1);
            var word2IsLastName = isRussianLastName(word2);
            
            if (word1IsLastName && word2IsFirstName) {
                result.lastName = word1;
                result.firstName = word2;
            }
            else if (word1IsLastName && !word2IsLastName) {
                result.lastName = word1;
                result.firstName = word2;
            }
            else if (word2IsLastName && !word1IsLastName) {
                result.firstName = word1;
                result.lastName = word2;
            }
            else if (analysis.likelyOrder == 2) {
                result.lastName = word1;
                result.firstName = word2;
            }
            else if (analysis.likelyRussian) {
                result.firstName = word1;
                result.lastName = word2;
            }
            else if (analysis.englishNamesCount > analysis.russianNamesCount) {
                result.firstName = word1;
                result.lastName = word2;
            }
            else {
                result.firstName = word1;
                result.lastName = word2;
            }
        }
        
    } else if (cleanWords.length == 3) {
        var patronymicIndex = -1;
        for (var i = 0; i < cleanWords.length; i++) {
            if (isPatronymic(cleanWords[i])) {
                patronymicIndex = i;
                break;
            }
        }
        
        var hasInitials = false;
        for (var i = 0; i < cleanWords.length; i++) {
            if (isInitials(cleanWords[i])) {
                hasInitials = true;
                break;
            }
        }
        
        if (patronymicIndex != -1) {
            if (patronymicIndex == 1) {
                result.firstName = cleanWords[0];
                result.middleName = cleanWords[1];
                result.lastName = cleanWords[2];
            } else if (patronymicIndex == 2) {
                result.lastName = cleanWords[0];
                result.firstName = cleanWords[1];
                result.middleName = cleanWords[2];
            }
        } else if (hasInitials) {
            if (isInitials(cleanWords[0]) && isInitials(cleanWords[1])) {
                result.firstName = cleanWords[0];
                result.middleName = cleanWords[1];
                result.lastName = cleanWords[2];
            } else if (isInitials(cleanWords[1]) && isInitials(cleanWords[2])) {
                result.lastName = cleanWords[0];
                result.firstName = cleanWords[1];
                result.middleName = cleanWords[2];
            } else {
                result.firstName = cleanWords[0];
                result.middleName = cleanWords[1];
                result.lastName = cleanWords[2];
            }
        } else {
            var hasRussian = false;
            for (var i = 0; i < cleanWords.length; i++) {
                if (/[А-Яа-яЁё]/.test(cleanWords[i])) {
                    hasRussian = true;
                    break;
                }
            }
            
            if (!hasRussian && analysis.englishNamesCount > 0) {
                result.firstName = cleanWords[0];
                result.middleName = cleanWords[1];
                result.lastName = cleanWords[2];
            } else if (analysis.likelyRussian && analysis.likelyOrder == 2) {
                result.lastName = cleanWords[0];
                result.firstName = cleanWords[1];
                result.middleName = cleanWords[2];
            } else {
                result.firstName = cleanWords[0];
                result.middleName = cleanWords[1];
                result.lastName = cleanWords[2];
            }
        }
        
    } else if (cleanWords.length > 3) {
        var patronymicIndex = -1;
        for (var i = 0; i < cleanWords.length; i++) {
            if (isPatronymic(cleanWords[i])) {
                patronymicIndex = i;
                break;
            }
        }
        
        if (patronymicIndex != -1 && analysis.likelyRussian) {
            if (patronymicIndex == cleanWords.length - 1) {
                result.lastName = cleanWords[0];
                var firstNameParts = [];
                for (var j = 1; j < cleanWords.length - 1; j++) {
                    firstNameParts[firstNameParts.length] = cleanWords[j];
                }
                result.firstName = firstNameParts.join(' ');
                result.middleName = cleanWords[cleanWords.length - 1];
            } else {
                result.firstName = cleanWords[0];
                result.middleName = cleanWords[patronymicIndex];
                result.lastName = cleanWords[cleanWords.length - 1];
            }
        } else {
            result.lastName = cleanWords[cleanWords.length - 1];
            var firstNameParts = [];
            for (var j = 0; j < cleanWords.length - 1; j++) {
                firstNameParts[firstNameParts.length] = cleanWords[j];
            }
            result.firstName = firstNameParts.join(' ');
        }
    }
    
    if (result.firstName && result.firstName.search(/[А-ЯA-Z]\.\s+[А-ЯA-Z]\./) != -1) {
        var parts = result.firstName.split(/\s+/);
        if (parts.length >= 2) {
            result.firstName = parts[0];
            if (!result.middleName) {
                result.middleName = parts[1];
            }
        }
    }
    
    result.firstName = (result.firstName || '').replace(/^\s+|\s+$/g, '');
    result.middleName = (result.middleName || '').replace(/^\s+|\s+$/g, '');
    result.lastName = (result.lastName || '').replace(/^\s+|\s+$/g, '');
    
    return result;
}

// Получить существующих авторов (старая версия для обратной совместимости)
function getExistingAuthorsProper(authorElement) {
    var existing = [];
    
    try {
        var mDiv = authorElement.getElementsByTagName('DIV');
        
        for (var k = 0; k < mDiv.length; k++) {
            var div = mDiv[k];
            
            if (div.all) {
                var author = {
                    firstName: div.all.first ? div.all.first.value || '' : '',
                    middleName: div.all.middle ? div.all.middle.value || '' : '',
                    lastName: div.all.last ? div.all.last.value || '' : ''
                };
                
                var isEmpty = true;
                if (author.firstName && author.firstName.replace(/\s+/g, '') != '') isEmpty = false;
                if (author.middleName && author.middleName.replace(/\s+/g, '') != '') isEmpty = false;
                if (author.lastName && author.lastName.replace(/\s+/g, '') != '') isEmpty = false;
                
                if (!isEmpty) {
                    existing[existing.length] = author;
                }
            }
        }
        
    } catch(e) {
        try {
            var divs = authorElement.getElementsByTagName('DIV');
            for (var i = 0; i < divs.length; i++) {
                var div = divs[i];
                var inputs = div.getElementsByTagName('INPUT');
                
                var author = {
                    firstName: '',
                    middleName: '',
                    lastName: ''
                };
                
                for (var j = 0; j < inputs.length; j++) {
                    var input = inputs[j];
                    var name = input.getAttribute('name');
                    var value = input.value || '';
                    
                    if (name == 'first') author.firstName = value;
                    else if (name == 'middle') author.middleName = value;
                    else if (name == 'last') author.lastName = value;
                }
                
                var isEmpty = true;
                if (author.firstName && author.firstName.replace(/\s+/g, '') != '') isEmpty = false;
                if (author.middleName && author.middleName.replace(/\s+/g, '') != '') isEmpty = false;
                if (author.lastName && author.lastName.replace(/\s+/g, '') != '') isEmpty = false;
                
                if (!isEmpty) {
                    existing[existing.length] = author;
                }
            }
        } catch(e2) {
        }
    }
    
    return existing;
}

// Проверить, одинаковые ли авторы (простая проверка для обратной совместимости)
function areAuthorsEqual(author1, author2) {
    function normalize(str) {
        if (!str) return '';
        var temp = str.toLowerCase();
        temp = temp.replace(/\./g, '');
        temp = temp.replace(/\s+/g, '');
        temp = temp.replace(/[^а-яёa-z0-9]/g, '');
        return temp;
    }
    
    var lastName1 = normalize(author1.lastName);
    var lastName2 = normalize(author2.lastName);
    
    if (lastName1 != lastName2) {
        return false;
    }
    
    return true;
}

// Добавить нового автора
function addNewAuthorProper(authorElement, author, isFirstAuthor) {
    try {
        var mDiv = authorElement.getElementsByTagName('DIV');
        var templateDiv = null;
        
        for (var i = 0; i < mDiv.length; i++) {
            var div = mDiv[i];
            
            if (div.all) {
                var hasContent = false;
                if (div.all.first && div.all.first.value && div.all.first.value.replace(/\s+/g, '') != '') hasContent = true;
                if (div.all.middle && div.all.middle.value && div.all.middle.value.replace(/\s+/g, '') != '') hasContent = true;
                if (div.all.last && div.all.last.value && div.all.last.value.replace(/\s+/g, '') != '') hasContent = true;
                
                if (hasContent) {
                    templateDiv = div;
                    break;
                }
            }
        }
        
        if (!templateDiv && mDiv.length > 0) {
            templateDiv = mDiv[0];
        }
        
        if (!templateDiv) {
            var newDiv = document.createElement('DIV');
            
            var inputFirst = document.createElement('INPUT');
            inputFirst.setAttribute('type', 'text');
            inputFirst.setAttribute('name', 'first');
            inputFirst.value = author.firstName || '';
            newDiv.appendChild(inputFirst);
            newDiv.appendChild(document.createTextNode(' '));
            
            var inputMiddle = document.createElement('INPUT');
            inputMiddle.setAttribute('type', 'text');
            inputMiddle.setAttribute('name', 'middle');
            inputMiddle.value = author.middleName || '';
            newDiv.appendChild(inputMiddle);
            newDiv.appendChild(document.createTextNode(' '));
            
            var inputLast = document.createElement('INPUT');
            inputLast.setAttribute('type', 'text');
            inputLast.setAttribute('name', 'last');
            inputLast.value = author.lastName || '';
            newDiv.appendChild(inputLast);
            
            authorElement.appendChild(newDiv);
            return true;
        }
        
        var newDiv = templateDiv.cloneNode(true);
        
        if (newDiv.all.first) newDiv.all.first.value = author.firstName || '';
        if (newDiv.all.middle) newDiv.all.middle.value = author.middleName || '';
        if (newDiv.all.last) newDiv.all.last.value = author.lastName || '';
        
        if (isFirstAuthor) {
            var firstDiv = mDiv[0];
            if (firstDiv && firstDiv.all) {
                var isEmpty = true;
                if (firstDiv.all.first && firstDiv.all.first.value && firstDiv.all.first.value.replace(/\s+/g, '') != '') isEmpty = false;
                if (firstDiv.all.middle && firstDiv.all.middle.value && firstDiv.all.middle.value.replace(/\s+/g, '') != '') isEmpty = false;
                if (firstDiv.all.last && firstDiv.all.last.value && firstDiv.all.last.value.replace(/\s+/g, '') != '') isEmpty = false;
                
                if (isEmpty) {
                    firstDiv.parentNode.replaceChild(newDiv, firstDiv);
                    return true;
                }
            }
        }
        
        authorElement.appendChild(newDiv);
        
        return true;
        
    } catch(e) {
        return false;
    }
}
