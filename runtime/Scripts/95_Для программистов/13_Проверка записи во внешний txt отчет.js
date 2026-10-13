/*<FBE-NEXT-CARD version="1">
{
  "schema": 1,
  "title": "\u041f\u0440\u043e\u0432\u0435\u0440\u043a\u0430 \u0437\u0430\u043f\u0438\u0441\u0438 \u0432\u043e \u0432\u043d\u0435\u0448\u043d\u0438\u0439 txt \u043e\u0442\u0447\u0435\u0442",
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
  "summary": "\u041f\u0440\u043e\u0432\u0435\u0440\u044f\u0435\u0442 \u043d\u0435\u0441\u043a\u043e\u043b\u044c\u043a\u043e \u0441\u043f\u043e\u0441\u043e\u0431\u043e\u0432 \u0437\u0430\u043f\u0438\u0441\u0438 \u0442\u0435\u043a\u0441\u0442\u043e\u0432\u043e\u0433\u043e \u0444\u0430\u0439\u043b\u0430.",
  "description": "\u041f\u0440\u043e\u0431\u0443\u0435\u0442 \u0434\u043e\u0441\u0442\u0443\u043f\u043d\u043e\u0441\u0442\u044c/\u0432\u044b\u0437\u043e\u0432 WriteFile, FileSystemObject \u0438 ADODB.Stream, \u0437\u0430\u043f\u0438\u0441\u044b\u0432\u0430\u0435\u0442 \u0442\u0435\u0441\u0442\u043e\u0432\u044b\u0435 \u0434\u0430\u043d\u043d\u044b\u0435 \u0438 \u0432\u044b\u0432\u043e\u0434\u0438\u0442 \u0440\u0435\u0437\u0443\u043b\u044c\u0442\u0430\u0442\u044b. \u042d\u0442\u043e \u0440\u0435\u0430\u043b\u044c\u043d\u0430\u044f \u0437\u0430\u043f\u0438\u0441\u044c \u0432\u043d\u0435\u0448\u043d\u0438\u0445 \u0444\u0430\u0439\u043b\u043e\u0432, \u043d\u0435 \u043f\u0440\u043e\u0432\u0435\u0440\u043a\u0430 \u0442\u043e\u043b\u044c\u043a\u043e \u043d\u0430\u043b\u0438\u0447\u0438\u044f \u043c\u0435\u0442\u043e\u0434\u043e\u0432.",
  "scope": "\u0424\u0430\u0439\u043b\u044b \u0438 \u043a\u0430\u0442\u0430\u043b\u043e\u0433\u0438 \u0432\u043d\u0435 \u0442\u0435\u043a\u0443\u0449\u0435\u0433\u043e \u0434\u043e\u043a\u0443\u043c\u0435\u043d\u0442\u0430.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "\u0417\u0430\u043f\u0443\u0441\u0442\u0438\u0442\u0435 \u0441\u043a\u0440\u0438\u043f\u0442 \u0432 FBE Next \u0438 \u0441\u043b\u0435\u0434\u0443\u0439\u0442\u0435 \u0441\u043e\u043e\u0431\u0449\u0435\u043d\u0438\u044f\u043c; \u0442\u0440\u0435\u0431\u043e\u0432\u0430\u043d\u0438\u044f \u043a \u0434\u043e\u043a\u0443\u043c\u0435\u043d\u0442\u0443 \u0443\u043a\u0430\u0437\u0430\u043d\u044b \u0432 preconditions.",
    "\u041f\u0440\u043e\u0432\u0435\u0440\u044c\u0442\u0435 \u043f\u0435\u0440\u0435\u0447\u0438\u0441\u043b\u0435\u043d\u043d\u044b\u0435 \u043f\u0430\u0440\u0430\u043c\u0435\u0442\u0440\u044b: \u0447\u0430\u0441\u0442\u044c \u0437\u0430\u0434\u0430\u0451\u0442\u0441\u044f \u0432 \u0438\u0441\u0445\u043e\u0434\u043d\u043e\u043c JS, \u0447\u0430\u0441\u0442\u044c \u2014 \u0447\u0435\u0440\u0435\u0437 \u043e\u0431\u0449\u0438\u0439 \u0434\u0438\u0430\u043b\u043e\u0433 \u043d\u0430\u0441\u0442\u0440\u043e\u0435\u043a.",
    "\u0417\u0430\u043f\u0443\u0441\u0442\u0438\u0442\u0435 \u043a\u043e\u043c\u0430\u043d\u0434\u0443 \u0438 \u043e\u0442\u0432\u0435\u0442\u044c\u0442\u0435 \u043d\u0430 \u0435\u0451 \u0434\u0438\u0430\u043b\u043e\u0433\u0438. \u0422\u0435\u043a\u0441\u0442\u044b \u0438 \u0443\u0441\u043b\u043e\u0432\u0438\u044f \u043f\u0440\u043e\u0432\u0435\u0440\u043e\u043a \u043f\u0440\u0438\u0432\u0435\u0434\u0435\u043d\u044b \u043d\u0438\u0436\u0435."
  ],
  "warnings": [
    "\u0424\u0430\u0439\u043b\u043e\u0432\u044b\u0435 \u043e\u043f\u0435\u0440\u0430\u0446\u0438\u0438 \u043d\u0435 \u043e\u0442\u043c\u0435\u043d\u044f\u044e\u0442\u0441\u044f \u043e\u0431\u044b\u0447\u043d\u044b\u043c Ctrl+Z \u0432 \u043a\u043d\u0438\u0433\u0435."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "53b09569a8bee14530adfb9a2f5c5e87fd0773206c4d283b08ed8a4eb719e26c",
    "reviewStatus": "generated"
  },
  "tags": [
    "\u0414\u043b\u044f \u043f\u0440\u043e\u0433\u0440\u0430\u043c\u043c\u0438\u0441\u0442\u043e\u0432",
    "\u043f\u0430\u0440\u0430\u043c\u0435\u0442\u0440\u044b"
  ],
  "undo": {
    "status": "no_explicit_grouping",
    "description": "\u042f\u0432\u043d\u0430\u044f \u0433\u0440\u0443\u043f\u043f\u0438\u0440\u043e\u0432\u043a\u0430 Undo \u0432 \u044d\u0442\u043e\u043c \u0444\u0430\u0439\u043b\u0435 \u043d\u0435 \u043d\u0430\u0439\u0434\u0435\u043d\u0430. \u042d\u0442\u043e \u043d\u0435 \u043e\u0437\u043d\u0430\u0447\u0430\u0435\u0442 \u043d\u0438 \u0433\u0430\u0440\u0430\u043d\u0442\u0438\u0440\u043e\u0432\u0430\u043d\u043d\u043e\u0435 \u043e\u0442\u0441\u0443\u0442\u0441\u0442\u0432\u0438\u0435 Undo, \u043d\u0438 \u0431\u0435\u0437\u043e\u043f\u0430\u0441\u043d\u043e\u0435 \u0432\u044b\u043f\u043e\u043b\u043d\u0435\u043d\u0438\u0435."
  },
  "settings": [
    {
      "name": "testPath2",
      "value": "D:\\FBE_Compare\\_test_fso.txt",
      "expression": "\"D:\\\\FBE_Compare\\\\_test_fso.txt\"",
      "description": "\u041f\u0440\u043e\u0431\u0443\u0435\u043c \u0437\u0430\u043f\u0438\u0441\u0430\u0442\u044c",
      "valueMeaning": "\u0417\u043d\u0430\u0447\u0435\u043d\u0438\u0435 \u043b\u0438\u0442\u0435\u0440\u0430\u043b\u0430/\u0440\u0435\u0437\u0435\u0440\u0432\u043d\u043e\u0435 \u0437\u043d\u0430\u0447\u0435\u043d\u0438\u0435 \u0432 \u0438\u0441\u0445\u043e\u0434\u043d\u0438\u043a\u0435, \u043d\u0435 \u0441\u043e\u0445\u0440\u0430\u043d\u0451\u043d\u043d\u0430\u044f \u043d\u0430\u0441\u0442\u0440\u043e\u0439\u043a\u0430 \u043f\u043e\u043b\u044c\u0437\u043e\u0432\u0430\u0442\u0435\u043b\u044f."
    }
  ]
}
</FBE-NEXT-CARD>*/
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
