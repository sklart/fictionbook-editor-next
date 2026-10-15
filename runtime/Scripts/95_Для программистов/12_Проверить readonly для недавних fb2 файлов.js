/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Проверить readonly для недавних fb2 файлов",
  "version": "1.2",
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
  "summary": "Ищет свежие временные файлы fbe*.tmp в заданных папках.",
  "description": "Через файловую систему Windows обходит заданные папки и при включённой рекурсии их вложения. Показывает fbe*.tmp с временем изменения не старше заданного порога. Атрибут «Только чтение» у открытого FB2 не проверяет и документ не меняет.",
  "scope": "Три указанные в исходнике папки и, по умолчанию, все доступные вложенные папки; открытый документ не анализируется.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Укажите в настройке исходного скрипта папки, где FBE может оставить временные файлы.",
    "Запустите команду и прочитайте список найденных fbe*.tmp и отсутствующих исходных папок.",
    "При проблемах сохранения дополнительно проверьте права на исходный FB2 и результат последнего сохранения."
  ],
  "warnings": [
    "Наличие fbe*.tmp не доказывает атрибут «Только чтение», а отсутствие файлов не подтверждает успешное сохранение.",
    "Недоступные вложенные папки пропускаются без отдельного сообщения; проверка может быть неполной."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "9e0158fd7b56bbfc32e25d271c89ea3eb93301852cd2a848ed49f2b8e3fbb979",
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
      "name": "foldersToCheck",
      "title": "Папки для проверки",
      "value": "D:\\Work\\!!2026; D:\\Ваш путь к папке; %USERPROFILE%\\Desktop",
      "expression": "[\"D:\\\\Work\\\\!!2026\", \"D:\\\\Ваш путь к папке\", \"%USERPROFILE%\\\\Desktop\"]",
      "description": "Список исходных папок; путь с шаблонным названием замените на свой.",
      "source": "script"
    },
    {
      "name": "searchSubfolders",
      "title": "Проверять вложенные папки",
      "value": 1,
      "expression": "1",
      "description": "1 — обходить вложенные папки; 0 — только указанные папки.",
      "source": "script"
    },
    {
      "name": "maxDepth",
      "title": "Максимальная глубина",
      "value": 0,
      "expression": "0",
      "description": "0 — без ограничения глубины; иначе число уровней рекурсии.",
      "source": "script"
    },
    {
      "name": "maxAgeMinutes",
      "title": "Возраст временного файла",
      "value": 10,
      "expression": "10",
      "description": "Файлы старше этого числа минут не показываются.",
      "source": "script"
    }
  ]
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Проверить readonly для недавних fb2 файлов" для редактора FBE
// (Проверить указанные папки на наличие временных файлов сохранения)
// version 1.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт проверяет указанные папки (включая вложенные) на наличие
// свежих fbe*.tmp файлов, которые FBE создаёт при неудачном сохранении защищенных от записи файлов.
// (например, когда fb2 файл имеет атрибут «только чтение»).
// По умолчанию "свежесть" tmp файлов - 10 минут, можно настроить любое время.
// Если такие файлы найдены — выдаётся предупреждение.
// Можно просто указать ваш рабочий диск целиком (например D:), скрипт найдет нужное.
// Количество путей к разным папкам можно задать любое.
// Ищет только файлы fbe*.tmp (формат имени временных файлов FBE).
// Запускать скрипт можно из любого fb2 документа.
// Скрипт не вносит никаких изменений в документ.

// version 1.2, 29.06.2026
//======================================

function Run() {
    var scriptName = "Проверить readonly для недавних fb2 файлов";
    var version = "1.2";
    
    // ==================================================
    // НАСТРОЙКИ СКРИПТА ====== можно менять по необходимости ======
    // ==================================================
    
    // Список папок для проверки
    // Можно использовать %USERPROFILE%, %TEMP% и другие переменные Windows
    // Прописные буквы диска (D:) и слеши (\\ или \) допустимы
    var foldersToCheck = [
        "D:\\Work\\!!2026",
        "D:\\Ваш путь к папке",
        "%USERPROFILE%\\Desktop"
    ];
    
    // Проверять вложенные папки: 0 - нет, 1 - да
    var searchSubfolders = 1;
    
    // Максимальная глубина вложенности (0 = без ограничений)
    var maxDepth = 0;
    
    // Максимальный возраст файла в минутах (файлы старше игнорируются)
    var maxAgeMinutes = 10;
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    // Исправление пути: гарантирует наличие слеша после диска
    function fixDriveLetter(rawPath) {
        if (!rawPath) return rawPath;
        var path = rawPath;
        if (path.length >= 2 && path.charAt(1) == ":" && path.charAt(2) != "\\" && path.charAt(2) != "/") {
            path = path.substring(0, 2) + "\\" + path.substring(2);
        }
        return path;
    }
    
    // Нормализация пути: убирает кавычки, невидимые символы
    function normalizePath(rawPath) {
        if (!rawPath) return rawPath;
        var path = rawPath;
        path = path.replace(/^["']|["']$/g, "");
        path = path.replace(/[\u200B\u200C\u200D\uFEFF]/g, "");
        path = path.replace(/^\s+|\s+$/g, "");
        return path;
    }
    
    try {
        var fso = new ActiveXObject("Scripting.FileSystemObject");
        var wshShell = new ActiveXObject("WScript.Shell");
        var now = new Date();
        var foundFiles = [];
        var foldersScanned = 0;
        var foldersNotFound = [];
        
        // Рекурсивный обход папок
        function scanFolder(folderPath, currentDepth) {
            if (maxDepth > 0 && currentDepth > maxDepth) return;
            
            try {
                var folder = fso.GetFolder(folderPath);
                foldersScanned++;
                
                var files = new Enumerator(folder.Files);
                for (; !files.atEnd(); files.moveNext()) {
                    var file = files.item();
                    var fileName = file.Name;
                    var ext = fso.GetExtensionName(fileName).toLowerCase();
                    
                    // Проверяем только .tmp файлы, начинающиеся с fbe
                    if (ext != "tmp") continue;
                    if (fileName.toLowerCase().substring(0, 3) != "fbe") continue;
                    
                    var fileDate = file.DateLastModified;
                    var diffMs = now - fileDate;
                    var diffMin = diffMs / (1000 * 60);
                    
                    if (diffMin <= maxAgeMinutes) {
                        foundFiles.push({
                            name: fileName,
                            path: file.Path,
                            date: fileDate,
                            ageMinutes: Math.round(diffMin)
                        });
                    }
                }
                
                if (searchSubfolders == 1) {
                    var subfolders = new Enumerator(folder.SubFolders);
                    for (; !subfolders.atEnd(); subfolders.moveNext()) {
                        scanFolder(subfolders.item().Path, currentDepth + 1);
                    }
                }
                
            } catch(e) {
                // Пропускаем папки без доступа
            }
        }
        
        // Обходим все указанные папки
        for (var i = 0; i < foldersToCheck.length; i++) {
            var folderPath = normalizePath(foldersToCheck[i]);
            folderPath = fixDriveLetter(folderPath);
            
            try {
                folderPath = wshShell.ExpandEnvironmentStrings(folderPath);
            } catch(e) {}
            
            folderPath = fixDriveLetter(folderPath);
            
            if (fso.FolderExists(folderPath)) {
                scanFolder(folderPath, 1);
            } else {
                foldersNotFound.push(foldersToCheck[i]);
            }
        }
        
        // Вывод результата
        var msg = scriptName + "\nver. " + version + "\n\n";
        
        if (foundFiles.length == 0) {
            msg += "✓ Свежих fbe .tmp файлов не найдено.\n";
            msg += "  (проверено папок: " + foldersScanned + ")\n\n";
            
            if (foldersNotFound.length > 0) {
                msg += "✓ Не найдены указанные папки:\n";
                for (var n = 0; n < foldersNotFound.length; n++) {
                    msg += "  • " + foldersNotFound[n] + "\n";
                }
                msg += "\nПроверьте правильность путей.\n";
                msg += "Используйте D:\\\\Папка или D:\\Папка\n\n";
            }
            
            msg += "Если файл не сохраняется:\n";
            msg += "• проверьте свойства fb2 файла\n";
            msg += "• убедитесь, что он не открыт в другой программе";
        } else {
            msg += "✓ Найдены свежие fbe .tmp файлы!\n\n";
            msg += "Возможно, FBE не смог сохранить изменения\n";
            msg += "(fb2 файл закрыт для записи).\n\n";
            
            for (var j = 0; j < foundFiles.length; j++) {
                var f = foundFiles[j];
                msg += "• " + f.name + " (~" + f.ageMinutes + " мин.)\n";
                msg += "  " + f.path + "\n\n";
            }
            
            msg += "Проверено папок: " + foldersScanned + "\n\n";
            msg += "Рекомендация:\n";
            msg += "1. Закройте FBE\n";
            msg += "2. Снимите «Только чтение» в свойствах редактируемого fb2 файла\n";
            msg += "3. Откройте файл заново";
        }
        
        MsgBox(msg);
        
    } catch(e) {
        MsgBox(scriptName + "\nver. " + version + "\n\n" +
               "Ошибка: " + e.message + "\n\n" +
               "Возможно, ActiveX (FileSystemObject) недоступен.");
    }
}
