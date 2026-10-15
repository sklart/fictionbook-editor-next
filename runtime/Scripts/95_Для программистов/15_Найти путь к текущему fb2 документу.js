/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Найти путь к текущему fb2 документу",
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
  "summary": "Определяет путь открытой книги через API и резервные способы.",
  "description": "Сначала обращается к GetDocumentFilePath/GetDocumentFileName/GetDocumentDirectory. При недоступности штатного пути пробует PowerShell и метод временного файла SaveBinary с последующим поиском/удалением. Книгу не редактирует, но способен писать файлы и запускать внешние команды.",
  "scope": "Контекст редактора и внешние файлы/команды.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Запустите для определения пути текущей книги. Результат и способ поиска выводятся в сообщении согласно showStatistics и showFullPath."
  ],
  "warnings": [
    "Файловые операции не отменяются обычным Ctrl+Z в книге.",
    "В резервном режиме скрипт запускает PowerShell для поиска пути к текущей книге.",
    "Для несохранённой книги штатный API может вернуть пустой путь. Резервный поиск по диску может быть длительным."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "30ec074c59db786e023c20e7fa8f7cbe9d3687f8eac49e4cbf9ce726e6bb616e",
    "reviewStatus": "generated"
  },
  "tags": [
    "Для программистов",
    "параметры"
  ],
  "undo": {
    "status": "no_explicit_grouping",
    "description": ""
  },
  "settings": [
    {
      "name": "showStatistics",
      "value": 1,
      "expression": "1",
      "description": "1 - показывать статистику, 0 - тихий режим",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя.",
      "title": "Показывать статистику"
    },
    {
      "name": "showFullPath",
      "value": 0,
      "expression": "0",
      "description": "1 - показывать полный путь к fb2-файлу, 0 - нет",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя.",
      "title": "Показывать полный путь книги"
    }
  ]
}
</FBE-SCRIPT-CARD>*/
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