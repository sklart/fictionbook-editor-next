// Скрипт "Найти путь к текущему fb2 документу" для редактора FBE
// version 1.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для определения пути к папке с текущим открытым fb2 документом.
// Корректно определяет путь к документу, только если файл открыт:
// двойным щелчком по файлу или в редакторе через меню файл-открыть...
// Если файл открыт через меню редактора "Предыдущие документы",  или ПКМ "Открыть с помощью", или
// перетаскиванием файла в окно редактора - то путь к нему определяется некорректно.
// Скрипт может работать с документами как с иллюстрациями, так и без них.
// Способ нахождения пути: скрипт создаёт уникальный временный файл рядом с книгой через SaveBinary,
// находит его через CMD (dir /s /b) и определяет папку по его пути.
// Временный файл автоматически удаляется после завершения поиска.
// Скрипт не вносит никаких изменений в документ (только чтение бинарных данных).

// version 1.1, 12.07.2026
//======================================

function Run() {
    var scriptName = "Найти путь к текущему fb2 документу";
    var version = "1.1";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА
    // ==================================================
    var showStatistics = 1; // 1 - показывать статистику, 0 - тихий режим
    var showTempFilePath = 0; // 1 - показывать путь к временному файлу, 0 - нет
    var showTempFileName = 0; // 1 - показывать имя временного файла, 0 - нет
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    var Ts = new Date().getTime();
    
    // 1. Получаем данные для тестового файла
    var binData = "";
    var dataSource = "";
    
    var binObjects = document.all.binobj.getElementsByTagName("DIV");
    
    if (binObjects.length > 0) {
        // Берём первый бинарник из документа
        binData = binObjects[0].base64data;
        dataSource = "документ с иллюстрациями";
    } else {
        // Создаём минимальный PNG 1x1 (прозрачный)
        binData = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPj/HwADBwIAMCbHYQAAAABJRU5ErkJggg==";
        dataSource = "документ без иллюстраций";
    }
    
    // 2. Генерируем уникальное имя
    var testId = "__fbe_path_test_" + Ts;
    
    // 3. Сохраняем тестовый файл рядом с книгой
    try {
        window.external.SaveBinary(testId, binData, 0);
    } catch(e) {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "Ошибка при сохранении тестового файла:\n" + e.message + "\n\n" +
               "Источник данных: " + dataSource);
        return;
    }
    
    // 4. Собираем список доступных дисков
    var drivesToCheck = [];
    var fso;
    try {
        fso = new ActiveXObject("Scripting.FileSystemObject");
        var allDrives = fso.Drives;
        for (var dc = new Enumerator(allDrives); !dc.atEnd(); dc.moveNext()) {
            var drv = dc.item();
            if (drv.IsReady) {
                drivesToCheck.push(drv.DriveLetter + ":");
            }
        }
    } catch(e) {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "Ошибка доступа к дискам:\n" + e.message);
        return;
    }
    
    // 5. Ищем файл через CMD
    var foundPath = "";
    
    try {
        var shell = new ActiveXObject("WScript.Shell");
        
        for (var d = 0; d < drivesToCheck.length; d++) {
            var disk = drivesToCheck[d];
            
            var tempFile = "C:\\__fbe_search_result.txt";
            var cmd = "cmd /u /c dir /s /b \"" + disk + "\\" + testId + "*\" > \"" + tempFile + "\" 2>nul";
            
            try {
                shell.Run(cmd, 0, true);
                
                var resultFile = fso.OpenTextFile(tempFile, 1, false, -1);
                var line = "";
                if (!resultFile.AtEndOfStream) {
                    line = resultFile.ReadLine();
                    line = line.replace(/^\s+|\s+$/g, "");
                    if (line.length > 0 && line.indexOf(testId) != -1) {
                        foundPath = line;
                    }
                }
                resultFile.Close();
                
                // Удаляем временный файл
                try {
                    fso.DeleteFile(tempFile);
                } catch(e3) {}
                
                if (foundPath != "") {
                    break;
                }
                
            } catch(ec) {}
        }
        
    } catch(e2) {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "Ошибка при поиске файла:\n" + e2.message);
        return;
    }
    
    // 6. Обрабатываем результат
    if (foundPath != "") {
        var lastSlash = foundPath.lastIndexOf("\\");
        var folderPath = "";
        var fileNameOnly = foundPath;
        if (lastSlash != -1) {
            folderPath = foundPath.substring(0, lastSlash);
            fileNameOnly = foundPath.substring(lastSlash + 1);
        }
        
        // Удаляем тестовый файл
        try {
            fso.DeleteFile(foundPath);
        } catch(ed) {}
        
        // 7. Таймер
        var Tf = new Date().getTime();
        var Tsssek = Math.ceil(1000 * ((Tf - Ts) / 1000)) / 1000;
        var timeStr = Tsssek.toFixed(3).replace(".", ",") + " сек.";
        
        // 8. Формируем сообщение
        var message = scriptName + "\nver. " + version + "\n\n";
        
        message += "Папка с документом:\n" + folderPath + "\n";
        
        if (showTempFilePath == 1) {
            message += "\nПуть к временному файлу:\n" + foundPath + "\n";
        }
        
        if (showTempFileName == 1) {
            message += "\nИмя временного файла:\n" + fileNameOnly + "\n";
        }
        
        if (showStatistics == 1) {
            message += "\nВремя выполнения: " + timeStr;
            message += "\nИсточник данных: " + dataSource;
        }
        
        MsgBox(message);
        
    } else {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "Не удалось определить путь к документу.\n" +
               "Попробуйте ещё раз.\n\n" +
               "Источник данных: " + dataSource);
    }
}
