/*<FBE-NEXT-CARD version="1">
{
  "schema": 1,
  "title": "\u041d\u0430\u0439\u0442\u0438 \u043f\u0443\u0442\u044c \u043a \u0442\u0435\u043a\u0443\u0449\u0435\u043c\u0443 fb2 \u0434\u043e\u043a\u0443\u043c\u0435\u043d\u0442\u0443",
  "version": "4.1",
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
  "summary": "\u041e\u043f\u0440\u0435\u0434\u0435\u043b\u044f\u0435\u0442 \u043f\u0443\u0442\u044c \u043e\u0442\u043a\u0440\u044b\u0442\u043e\u0439 \u043a\u043d\u0438\u0433\u0438 \u0447\u0435\u0440\u0435\u0437 API \u0438 \u0440\u0435\u0437\u0435\u0440\u0432\u043d\u044b\u0435 \u0441\u043f\u043e\u0441\u043e\u0431\u044b.",
  "description": "\u0421\u043d\u0430\u0447\u0430\u043b\u0430 \u043e\u0431\u0440\u0430\u0449\u0430\u0435\u0442\u0441\u044f \u043a GetDocumentFilePath/GetDocumentFileName/GetDocumentDirectory. \u041f\u0440\u0438 \u043d\u0435\u0434\u043e\u0441\u0442\u0443\u043f\u043d\u043e\u0441\u0442\u0438 \u0448\u0442\u0430\u0442\u043d\u043e\u0433\u043e \u043f\u0443\u0442\u0438 \u043f\u0440\u043e\u0431\u0443\u0435\u0442 PowerShell \u0438 \u043c\u0435\u0442\u043e\u0434 \u0432\u0440\u0435\u043c\u0435\u043d\u043d\u043e\u0433\u043e \u0444\u0430\u0439\u043b\u0430 SaveBinary \u0441 \u043f\u043e\u0441\u043b\u0435\u0434\u0443\u044e\u0449\u0438\u043c \u043f\u043e\u0438\u0441\u043a\u043e\u043c/\u0443\u0434\u0430\u043b\u0435\u043d\u0438\u0435\u043c. \u041a\u043d\u0438\u0433\u0443 \u043d\u0435 \u0440\u0435\u0434\u0430\u043a\u0442\u0438\u0440\u0443\u0435\u0442, \u043d\u043e \u0441\u043f\u043e\u0441\u043e\u0431\u0435\u043d \u043f\u0438\u0441\u0430\u0442\u044c \u0444\u0430\u0439\u043b\u044b \u0438 \u0437\u0430\u043f\u0443\u0441\u043a\u0430\u0442\u044c \u0432\u043d\u0435\u0448\u043d\u0438\u0435 \u043a\u043e\u043c\u0430\u043d\u0434\u044b.",
  "scope": "\u041a\u043e\u043d\u0442\u0435\u043a\u0441\u0442 \u0440\u0435\u0434\u0430\u043a\u0442\u043e\u0440\u0430 \u0438 \u0432\u043d\u0435\u0448\u043d\u0438\u0435 \u0444\u0430\u0439\u043b\u044b/\u043a\u043e\u043c\u0430\u043d\u0434\u044b.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "\u0417\u0430\u043f\u0443\u0441\u0442\u0438\u0442\u0435 \u0441\u043a\u0440\u0438\u043f\u0442 \u0432 FBE Next \u0438 \u0441\u043b\u0435\u0434\u0443\u0439\u0442\u0435 \u0441\u043e\u043e\u0431\u0449\u0435\u043d\u0438\u044f\u043c; \u0442\u0440\u0435\u0431\u043e\u0432\u0430\u043d\u0438\u044f \u043a \u0434\u043e\u043a\u0443\u043c\u0435\u043d\u0442\u0443 \u0443\u043a\u0430\u0437\u0430\u043d\u044b \u0432 preconditions.",
    "\u041f\u0440\u043e\u0432\u0435\u0440\u044c\u0442\u0435 \u043f\u0435\u0440\u0435\u0447\u0438\u0441\u043b\u0435\u043d\u043d\u044b\u0435 \u043f\u0430\u0440\u0430\u043c\u0435\u0442\u0440\u044b: \u0447\u0430\u0441\u0442\u044c \u0437\u0430\u0434\u0430\u0451\u0442\u0441\u044f \u0432 \u0438\u0441\u0445\u043e\u0434\u043d\u043e\u043c JS, \u0447\u0430\u0441\u0442\u044c \u2014 \u0447\u0435\u0440\u0435\u0437 \u043e\u0431\u0449\u0438\u0439 \u0434\u0438\u0430\u043b\u043e\u0433 \u043d\u0430\u0441\u0442\u0440\u043e\u0435\u043a.",
    "\u0417\u0430\u043f\u0443\u0441\u0442\u0438\u0442\u0435 \u043a\u043e\u043c\u0430\u043d\u0434\u0443 \u0438 \u043e\u0442\u0432\u0435\u0442\u044c\u0442\u0435 \u043d\u0430 \u0435\u0451 \u0434\u0438\u0430\u043b\u043e\u0433\u0438. \u0422\u0435\u043a\u0441\u0442\u044b \u0438 \u0443\u0441\u043b\u043e\u0432\u0438\u044f \u043f\u0440\u043e\u0432\u0435\u0440\u043e\u043a \u043f\u0440\u0438\u0432\u0435\u0434\u0435\u043d\u044b \u043d\u0438\u0436\u0435."
  ],
  "warnings": [
    "\u0424\u0430\u0439\u043b\u043e\u0432\u044b\u0435 \u043e\u043f\u0435\u0440\u0430\u0446\u0438\u0438 \u043d\u0435 \u043e\u0442\u043c\u0435\u043d\u044f\u044e\u0442\u0441\u044f \u043e\u0431\u044b\u0447\u043d\u044b\u043c Ctrl+Z \u0432 \u043a\u043d\u0438\u0433\u0435.",
    "\u0415\u0441\u0442\u044c \u0437\u0430\u043f\u0443\u0441\u043a \u0432\u043d\u0435\u0448\u043d\u0435\u0433\u043e \u043f\u0440\u0438\u043b\u043e\u0436\u0435\u043d\u0438\u044f \u0438\u043b\u0438 \u043e\u0442\u043a\u0440\u044b\u0442\u0438\u0435 \u0441\u0435\u0442\u0435\u0432\u043e\u0433\u043e \u0430\u0434\u0440\u0435\u0441\u0430; \u043f\u0440\u043e\u0432\u0435\u0440\u044c\u0442\u0435 \u0441\u0444\u043e\u0440\u043c\u0438\u0440\u043e\u0432\u0430\u043d\u043d\u0443\u044e \u043a\u043e\u043c\u0430\u043d\u0434\u0443 \u0438 \u043f\u0435\u0440\u0435\u0434\u0430\u0432\u0430\u0435\u043c\u044b\u0435 \u0434\u0430\u043d\u043d\u044b\u0435.",
    "\u0414\u043b\u044f \u043d\u0435\u0441\u043e\u0445\u0440\u0430\u043d\u0451\u043d\u043d\u043e\u0439 \u043a\u043d\u0438\u0433\u0438 \u0448\u0442\u0430\u0442\u043d\u044b\u0439 API \u043c\u043e\u0436\u0435\u0442 \u0432\u0435\u0440\u043d\u0443\u0442\u044c \u043f\u0443\u0441\u0442\u043e\u0439 \u043f\u0443\u0442\u044c. \u0420\u0435\u0437\u0435\u0440\u0432\u043d\u044b\u0439 \u043f\u043e\u0438\u0441\u043a \u043f\u043e \u0434\u0438\u0441\u043a\u0443 \u043c\u043e\u0436\u0435\u0442 \u0431\u044b\u0442\u044c \u0434\u043b\u0438\u0442\u0435\u043b\u044c\u043d\u044b\u043c."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "30ec074c59db786e023c20e7fa8f7cbe9d3687f8eac49e4cbf9ce726e6bb616e",
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
      "name": "showStatistics",
      "value": 1,
      "expression": "1",
      "description": "1 - \u043f\u043e\u043a\u0430\u0437\u044b\u0432\u0430\u0442\u044c \u0441\u0442\u0430\u0442\u0438\u0441\u0442\u0438\u043a\u0443, 0 - \u0442\u0438\u0445\u0438\u0439 \u0440\u0435\u0436\u0438\u043c",
      "valueMeaning": "\u0417\u043d\u0430\u0447\u0435\u043d\u0438\u0435 \u043b\u0438\u0442\u0435\u0440\u0430\u043b\u0430/\u0440\u0435\u0437\u0435\u0440\u0432\u043d\u043e\u0435 \u0437\u043d\u0430\u0447\u0435\u043d\u0438\u0435 \u0432 \u0438\u0441\u0445\u043e\u0434\u043d\u0438\u043a\u0435, \u043d\u0435 \u0441\u043e\u0445\u0440\u0430\u043d\u0451\u043d\u043d\u0430\u044f \u043d\u0430\u0441\u0442\u0440\u043e\u0439\u043a\u0430 \u043f\u043e\u043b\u044c\u0437\u043e\u0432\u0430\u0442\u0435\u043b\u044f."
    },
    {
      "name": "showFullPath",
      "value": 0,
      "expression": "0",
      "description": "1 - \u043f\u043e\u043a\u0430\u0437\u044b\u0432\u0430\u0442\u044c \u043f\u043e\u043b\u043d\u044b\u0439 \u043f\u0443\u0442\u044c \u043a fb2-\u0444\u0430\u0439\u043b\u0443, 0 - \u043d\u0435\u0442",
      "valueMeaning": "\u0417\u043d\u0430\u0447\u0435\u043d\u0438\u0435 \u043b\u0438\u0442\u0435\u0440\u0430\u043b\u0430/\u0440\u0435\u0437\u0435\u0440\u0432\u043d\u043e\u0435 \u0437\u043d\u0430\u0447\u0435\u043d\u0438\u0435 \u0432 \u0438\u0441\u0445\u043e\u0434\u043d\u0438\u043a\u0435, \u043d\u0435 \u0441\u043e\u0445\u0440\u0430\u043d\u0451\u043d\u043d\u0430\u044f \u043d\u0430\u0441\u0442\u0440\u043e\u0439\u043a\u0430 \u043f\u043e\u043b\u044c\u0437\u043e\u0432\u0430\u0442\u0435\u043b\u044f."
    }
  ]
}
</FBE-NEXT-CARD>*/
// Скрипт "Найти путь к текущему fb2 документу" для редактора FBE
// version 4.1
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для определения пути к папке с текущим открытым fb2 документом.
// Метод 1 (штатный): использует новые методы API FBE —
// GetDocumentFilePath, GetDocumentFileName, GetDocumentDirectory.
// (Для нового (ещё не сохранённого) документа возвращает пустую строку).
// Метод 2 (PowerShell): читает заголовок окна FBE.
// Метод 3 (запасной): скрипт создаёт уникальный временный файл рядом с документом через SaveBinary,
// находит его через CMD (dir /s /b) и определяет папку по его пути.
// Временный файл автоматически удаляется после завершения поиска.

// Скрипт автоматически выбирает доступный метод.
// Скрипт не вносит никаких изменений в документ.

// version 4.1, 02.08.2026
//======================================

function Run() {
    var scriptName = "Найти путь к текущему fb2 документу";
    var version = "4.1";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА
    // ==================================================
    var showStatistics = 1; // 1 - показывать статистику, 0 - тихий режим
    var showFullPath = 0;   // 1 - показывать полный путь к fb2-файлу, 0 - нет
    var showFileName = 1;   // 1 - показывать имя fb2-файла, 0 - нет
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    var folderPath = "";
    var fb2Path = "";
    var fileName = "";
    var methodUsed = "";
    var Tf = 0;
    var Ts = 0;
    
    // ==================================================
    // МЕТОД 1: Штатные методы API FBE (версии FBE: начиная с FBE 2.8.5 или FBE Next)
    // ==================================================
    
    Ts = new Date().getTime();
    
    try {
        // В IE6 typeof для COM-методов возвращает "unknown", а не "function"
        // Поэтому просто пробуем вызвать — если ошибка, значит методов нет
        fb2Path = window.external.GetDocumentFilePath();
        fileName = window.external.GetDocumentFileName();
        folderPath = window.external.GetDocumentDirectory();
        
        Tf = new Date().getTime();
        
        if (folderPath != "") {
            methodUsed = "штатное API FBE";
        }
    } catch(e) {
        // Методов нет — идём дальше
    }
    
    // ==================================================
    // МЕТОД 2: PowerShell (заголовок окна)
    // ==================================================
    
    if (folderPath == "") {
        var shell, fso;
        try {
            shell = new ActiveXObject("WScript.Shell");
            fso = new ActiveXObject("Scripting.FileSystemObject");
        } catch(e) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Ошибка доступа к системным объектам:\n" + e.message);
            return;
        }
        
        var tempFile = "C:\\__fbe_path_result.txt";
        
        Ts = new Date().getTime();
        
        try {
            var psCmd = "powershell -ExecutionPolicy Bypass -command \"(Get-Process -Name 'fbe' -ErrorAction SilentlyContinue).MainWindowTitle\" > \"" + tempFile + "\" 2>&1";
            shell.Run(psCmd, 0, true);
            
            Tf = new Date().getTime();
            
            if (fso.FileExists(tempFile)) {
                var file = fso.OpenTextFile(tempFile, 1, false, -1);
                var title = "";
                if (!file.AtEndOfStream) {
                    title = file.ReadLine();
                    title = title.replace(/^\s+|\s+$/g, "");
                }
                file.Close();
                
                try { fso.DeleteFile(tempFile); } catch(e) {}
                
                if (title.length > 0) {
                    var pathMatch = title.match(/[A-Za-z]:\\.+?\.fb2/i);
                    if (pathMatch) {
                        fb2Path = pathMatch[0];
                        var lastSlash = fb2Path.lastIndexOf("\\");
                        if (lastSlash != -1) {
                            folderPath = fb2Path.substring(0, lastSlash);
                            fileName = fb2Path.substring(lastSlash + 1);
                        }
                        methodUsed = "PowerShell (заголовок окна)";
                    }
                }
            }
        } catch(e) {
            // PowerShell не сработал
        }
    }
    
    // ==================================================
    // МЕТОД 3: Временный файл + CMD-поиск (запасной) для старых версий FBE (до FBE 2.8.5 или FBE Next)
    // ==================================================
    
    if (folderPath == "") {
        var binObjects = document.all.binobj.getElementsByTagName("DIV");
        var binData = "";
        
        if (binObjects.length > 0) {
            binData = binObjects[0].base64data;
        } else {
            binData = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPj/HwADBwIAMCbHYQAAAABJRU5ErkJggg==";
        }
        
        var testId = "__fbe_path_test_" + new Date().getTime();
        
        try {
            window.external.SaveBinary(testId, binData, 0);
        } catch(e) {
            MsgBox(scriptName + "\nver. " + version + "\n\n" +
                   "Все методы не сработали.\n" +
                   "Штатное API: недоступно.\n" +
                   "PowerShell: недоступен.\n" +
                   "Временный файл: ошибка сохранения.\n\n" +
                   "Ошибка: " + e.message);
            return;
        }
        
        var drivesToCheck = [];
        try {
            var allDrives = fso.Drives;
            for (var dc = new Enumerator(allDrives); !dc.atEnd(); dc.moveNext()) {
                var drv = dc.item();
                if (drv.IsReady) {
                    drivesToCheck.push(drv.DriveLetter + ":");
                }
            }
        } catch(e) {
            drivesToCheck = ["C:"];
        }
        
        Ts = new Date().getTime();
        
        var foundPath = "";
        
        for (var d = 0; d < drivesToCheck.length; d++) {
            var disk = drivesToCheck[d];
            
            var cmd = "cmd /u /c dir /s /b \"" + disk + "\\" + testId + "*\" > \"" + tempFile + "\" 2>nul";
            
            try {
                shell.Run(cmd, 0, true);
                
                if (fso.FileExists(tempFile)) {
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
                    
                    try { fso.DeleteFile(tempFile); } catch(e) {}
                    
                    if (foundPath != "") break;
                }
            } catch(ec) {}
        }
        
        Tf = new Date().getTime();
        
        if (foundPath != "") {
            var lastSlash2 = foundPath.lastIndexOf("\\");
            if (lastSlash2 != -1) {
                folderPath = foundPath.substring(0, lastSlash2);
                fileName = "(неизвестно)";
                methodUsed = "CMD-поиск (запасной)";
            }
            
            try { fso.DeleteFile(foundPath); } catch(e) {}
        }
    }
    
    // ==================================================
    // ВЫВОД РЕЗУЛЬТАТА
    // ==================================================
    
    if (folderPath != "") {
        var message = scriptName + "\nver. " + version + "\n\n";
        
        message += "Папка с документом:\n" + folderPath + "\n";
        
        if (showFullPath == 1 && fb2Path != "" && methodUsed != "CMD-поиск (запасной)") {
            message += "\nПолный путь к файлу:\n" + fb2Path + "\n";
        }
        
        if (showFileName == 1 && fileName != "" && methodUsed != "CMD-поиск (запасной)") {
            message += "\nИмя файла:\n" + fileName + "\n";
        }
        
        if (showStatistics == 1 && Ts > 0 && Tf > 0) {
            var Tsssek = Math.ceil(1000 * ((Tf - Ts) / 1000)) / 1000;
            var timeStr = Tsssek.toFixed(3).replace(".", ",") + " сек.";
            message += "\nВремя выполнения: " + timeStr;
            message += "\nМетод: " + methodUsed;
        }
        
        MsgBox(message);
        
    } else {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "Не удалось определить путь к документу.\n" +
               "Возможно, это новый (ещё не сохранённый) документ.");
    }
}