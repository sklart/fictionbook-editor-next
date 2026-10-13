/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Удалить все иллюстрации, их тэги и пустые строки (комбо)",
  "version": "1.3",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Скрипт предназначен для полного удаления всех иллюстраций, их тэгов, пустых строк вокруг иллюстраций",
  "description": "Скрипт предназначен для полного удаления всех иллюстраций, их тэгов, пустых строк вокруг иллюстраций",
  "scope": "Текущая книга или определённые кодом её разделы.",
  "modifiesDocument": null,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Сохраняйте структуру Scripts/HTML и связанные файлы; один JS может быть недостаточен для работы.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже."
  ],
  "warnings": [
    "По статическому анализу нельзя однозначно отделить все изменения книги от временной разметки, окна отчёта или вызванных скриптов. Не считать скрипт гарантированно неизменяющим документ.",
    "Действия и возможность отката зависят от запускаемых дочерних скриптов."
  ],
  "dependencies": [
    {
      "path": "Scripts/05_Иллюстрации/06_Добавить либо удалить пустые строки возле иллюстраций....js",
      "sha256": "0c5bd46082293606e38d1ff0359c7c46a8c847d059c17894226817519bcf1dc9",
      "optional": true
    },
    {
      "path": "Scripts/05_Иллюстрации/05_Удалить все вложения.js",
      "sha256": "dae072c905e2fc841842324a748de6e977bb5854483963955bd63b391e917768",
      "optional": true
    },
    {
      "path": "Scripts/05_Иллюстрации/02_Унифицировать иллюстрации и вложения.js",
      "sha256": "4eaa8cbae4eb2adb8c0c536002c6f94edb9efcec704dc434b8c6c4fd54c93ccc",
      "optional": true
    },
    {
      "path": "Scripts/05_Иллюстрации/07_Удалить неиспользуемые вложения....js",
      "sha256": "bd94f4a28717f218104646a88a5287f65ce7bede6de888aaacdfd2344fe15758",
      "optional": true
    },
    {
      "path": "Scripts/05_Иллюстрации/10_Удалить все теги иллюстраций - теги image.js",
      "sha256": "d2aaff2be265b4dfa11801a2ac52891f79e924365fe784c5fa5c97b5341a888d",
      "optional": true
    },
    {
      "path": "Scripts/06_Чистка/40_Удалить теги style, id разделов, id абзацев и id картинок.js",
      "sha256": "dcfd8bd36e3f0f925361e16d6c103f29f98bfa475aa216c8888d54bd9e11ef3a",
      "optional": true
    },
    {
      "path": "Scripts/13_Заголовки, подзаголовки, пустые строки/528_Разметка подзаголовков, чистка пустых строк, удаление жирности и курсива в заголовках.js",
      "sha256": "aeda88a7f3ff62ce739a9897b650c62a54ac2cdd1603b67dfb6c1eb1e12a7b3d",
      "optional": true
    }
  ],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "978570f09599cfd53b39cd15fb575023c8a67d1ff63034f685c0c8d2c606615c",
    "reviewStatus": "generated"
  },
  "tags": [
    "Иллюстрации",
    "изображения"
  ],
  "undo": {
    "status": "may_be_delegated",
    "description": "Прямых Undo-вызовов нет; действия могут выполняться связанным HTML/JS. Гарантия отмены не установлена."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Удаление картинок и чистка тэгов (комбо)"
// Скрипт предназначен для полного удаления всех иллюстраций, их тэгов, пустых строк вокруг иллюстраций

// Основано на скрипте "Запуск скриптов по списку"
// Версия 1.3
// Автор Sclex

function Run() {
 window.RunSubscripts=function() {
   // Команда, задающая папку скриптов:
   // scriptsFolder(document.location.href.replace("file:///", "").replace(/main\.html$/, "Scripts"));
   scriptsFolder(document.getElementById("userCmd").getAttribute("src").replace("file:///", "").replace(/\\[^\\]+?\\[^\\]+?$/, ""));
   
   // Команда, запускающая скрипт из заданной папки скриптов
   // (можно сделать несколько таких строк):

   runScriptInFolder("05_Иллюстрации\\06_Добавить либо удалить пустые строки возле иллюстраций....js");
   runScriptInFolder("05_Иллюстрации\\05_Удалить все вложения.js");
   runScriptInFolder("05_Иллюстрации\\02_Унифицировать иллюстрации и вложения.js");
   runScriptInFolder("05_Иллюстрации\\07_Удалить неиспользуемые вложения....js");
   runScriptInFolder("05_Иллюстрации\\10_Удалить все теги иллюстраций - теги image.js");
   runScriptInFolder("06_Чистка\\40_Удалить теги style, id разделов, id абзацев и id картинок.js");
   runScriptInFolder("13_Заголовки, подзаголовки, пустые строки\\528_Разметка подзаголовков, чистка пустых строк, удаление жирности и курсива в заголовках.js");
  
   // Команда, запускающая скрипт по полному пути
   // (можно сделать несколько таких строк):
   // runScript("C:\\Program Files (x86)\\FictionBook Editor_2.7.4\\Scripts\\06_Чистка\\03_Латиница в Кириллице.js");
   
   setTimeout("window.RunSubscripts=undefined; window.scriptsFolderVar=undefined;",0);
   
   function scriptsFolder(path) {
     window.scriptsFolderVar=path;
   }
   
   function runScriptInFolder(path) {
     var fullPath=window.scriptsFolderVar;
     if (!fullPath) {
       MsgBox("В скрипте для запуска скриптов вы используете команду runScriptInFolder, но вы не задали путь к папке скриптов командой scriptsFolder.");
       return;
     }
     if (fullPath.substr(fullPath.length-1,1)!="\\" && path.substr(0,1)!="\\")
       fullPath+="\\";
     fullPath+=path;
     apiRunCmd(fullPath);
   }
   
   function runScript(path) {
     apiRunCmd(path);
   }
 }
 setTimeout("RunSubscripts()",0);
}
