// Диагностический скрипт "Проверка методов записи файлов FBE"
// version 1.0
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// version 1.0, 30.06.2026
//======================================

function Run() {
    var scriptName = "Проверка методов записи файлов";
    var version = "1.0";
    var showStatistics = 1;
    
    var result = [];
    result.push("Диагностика методов записи файлов FBE");
    result.push("ver. " + version);
    result.push("----------------------------------------");
    
    // Метод 1: window.external.WriteFile
    result.push("");
    result.push("Метод 1: window.external.WriteFile");
    try {
        if (typeof window.external.WriteFile != "undefined") {
            result.push("✓ Метод существует");
            result.push("  Тип: " + typeof window.external.WriteFile);
            // Пробуем записать тестовый файл
            try {
                var testPath1 = "D:\\FBE_Compare\\_test_writefile.txt";
                var testContent1 = "Тест WriteFile: " + new Date();
                window.external.WriteFile(testPath1, testContent1);
                result.push("✓ Запись выполнена без ошибок");
                result.push("  Проверь файл: " + testPath1);
            } catch(e) {
                result.push("✗ Ошибка при записи: " + e.message);
            }
        } else {
            result.push("✗ Метод НЕ НАЙДЕН");
        }
    } catch(e) {
        result.push("✗ Ошибка при проверке: " + e.message);
    }
    
    // Метод 2: window.external.SaveFile
    result.push("");
    result.push("Метод 2: window.external.SaveFile");
    try {
        if (typeof window.external.SaveFile != "undefined") {
            result.push("✓ Метод существует");
            result.push("  Тип: " + typeof window.external.SaveFile);
        } else {
            result.push("✗ Метод НЕ НАЙДЕН");
        }
    } catch(e) {
        result.push("✗ Ошибка при проверке: " + e.message);
    }
    
    // Метод 3: window.external.ReadFile (для проверки чтения)
    result.push("");
    result.push("Метод 3: window.external.ReadFile");
    try {
        if (typeof window.external.ReadFile != "undefined") {
            result.push("✓ Метод существует");
            result.push("  Тип: " + typeof window.external.ReadFile);
        } else {
            result.push("✗ Метод НЕ НАЙДЕН");
        }
    } catch(e) {
        result.push("✗ Ошибка при проверке: " + e.message);
    }
    
    // Метод 4: Объект FileSystemObject через ActiveX
    result.push("");
    result.push("Метод 4: FileSystemObject (ActiveX)");
    try {
        var fso = new ActiveXObject("Scripting.FileSystemObject");
        result.push("✓ ActiveX доступен");
        // Пробуем записать
        try {
            var testPath2 = "D:\\FBE_Compare\\_test_fso.txt";
            var testContent2 = "Тест FSO: " + new Date();
            var file = fso.CreateTextFile(testPath2, true);
            file.Write(testContent2);
            file.Close();
            result.push("✓ Запись через FSO выполнена");
            result.push("  Проверь файл: " + testPath2);
        } catch(e) {
            result.push("✗ Ошибка записи через FSO: " + e.message);
        }
    } catch(e) {
        result.push("✗ ActiveX недоступен: " + e.message);
    }
    
    // Метод 5: ADODB.Stream
    result.push("");
    result.push("Метод 5: ADODB.Stream");
    try {
        var stream = new ActiveXObject("ADODB.Stream");
        result.push("✓ ADODB.Stream доступен");
    } catch(e) {
        result.push("✗ ADODB.Stream недоступен: " + e.message);
    }
    
    // Метод 6: Перебор всех свойств window.external
    result.push("");
    result.push("Метод 6: Все доступные методы window.external");
    result.push("  (содержащие 'file', 'File', 'write', 'Write', 'read', 'Read', 'save', 'Save')");
    try {
        var found = [];
        for (var prop in window.external) {
            var propLower = prop.toLowerCase();
            if (propLower.indexOf("file") >= 0 || 
                propLower.indexOf("write") >= 0 || 
                propLower.indexOf("read") >= 0 || 
                propLower.indexOf("save") >= 0) {
                found.push("  • " + prop + " (тип: " + typeof window.external[prop] + ")");
            }
        }
        if (found.length > 0) {
            result.push("✓ Найдены методы:");
            for (var i = 0; i < found.length; i++) {
                result.push(found[i]);
            }
        } else {
            result.push("  Ничего не найдено с такими ключевыми словами");
        }
    } catch(e) {
        result.push("✗ Ошибка перебора: " + e.message);
    }
    
    // Метод 7: navigator.userAgent (для информации)
    result.push("");
    result.push("Метод 7: Информация о среде");
    result.push("  UserAgent: " + navigator.userAgent);
    
    // Итог
    result.push("");
    result.push("========================================");
    result.push("Проверка завершена.");
    result.push("Если папки D:\\FBE_Compare нет — создай её вручную перед тестом.");
    
    if (showStatistics == 1) {
        MsgBox(result.join("\n"));
    }
}
