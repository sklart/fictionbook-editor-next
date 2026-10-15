/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Автоматическая чистка простых текстов",
  "version": "1.3",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Последовательно запускает девять скриптов очистки и правки заголовков.",
  "description": "Получает папку скриптов из элемента userCmd и передаёт девять жёстко заданных путей обработчику запуска. Первые семь файлов есть в комплекте; два последних имени в исходном списке не совпадают с файлами текущего комплекта.",
  "scope": "Вся текущая книга в объёме действий вызываемых скриптов; отдельного обхода документа этот файл не выполняет.",
  "modifiesDocument": null,
  "requiresSelection": false,
  "usage": [
    "Сохраните копию книги и проверьте список из девяти вызовов в исходном JS.",
    "Запустите скрипт и просмотрите результаты каждой доступной команды очистки; последние два пути в комплекте не найдены."
  ],
  "warnings": [
    "Последние два указанных файла отсутствуют под этими именами: соответствующие этапы не могут выполниться в данном комплекте.",
    "Первые этапы меняют структуру, разметку, ссылки и заголовки книги. Отмена зависит от каждого вызываемого скрипта и не гарантируется общей операцией."
  ],
  "dependencies": [
    {
      "path": "Scripts/08_Структура разделов/13_Почистить структуру.js",
      "sha256": "b1be0e5d5548183e92781d7e08bb6a5b4c1b5dfd309f0fce8226c2bcb2a50042",
      "optional": true
    },
    {
      "path": "Scripts/06_Чистка/02_Генеральная уборка.js",
      "sha256": "bb53493213735dbd03d53238259e6699ed787a212defaa69e28ee6b4f6ce8729",
      "optional": true
    },
    {
      "path": "Scripts/06_Чистка/40_Удалить теги style, id разделов, id абзацев и id картинок.js",
      "sha256": "dcfd8bd36e3f0f925361e16d6c103f29f98bfa475aa216c8888d54bd9e11ef3a",
      "optional": true
    },
    {
      "path": "Scripts/06_Чистка/50_Расформатировать ссылки, адрес которых - пустая строка.js",
      "sha256": "0b69bc82082867344f5aff6a4a78fe7b1ee0bfdedf44edc1cd0d700843e8dccd",
      "optional": true
    },
    {
      "path": "Scripts/06_Чистка/17_Превратить внешние ссылки в текст.js",
      "sha256": "8701564537796783bd0b617725b51011179da394d7d10228396a57b500a2bbaa",
      "optional": true
    },
    {
      "path": "Scripts/06_Чистка/55_Удалить ссылки 'bookmark', созданные FBD.js",
      "sha256": "6caaae762f44f0568331a56c6fab130221671643700c51b16684a8f2e15f0278",
      "optional": true
    },
    {
      "path": "Scripts/13_Заголовки, подзаголовки, пустые строки/528_Разметка подзаголовков, чистка пустых строк, удаление жирности и курсива в заголовках.js",
      "sha256": "aeda88a7f3ff62ce739a9897b650c62a54ac2cdd1603b67dfb6c1eb1e12a7b3d",
      "optional": true
    },
    {
      "path": "Scripts/13_Заголовки, подзаголовки, пустые строки/529_Удаление жирности и курсива в заголовках и подзаголовках.js",
      "optional": true
    },
    {
      "path": "Scripts/06_Чистка/68_Генеральная уборка (версия Александра Ка).js",
      "optional": true
    }
  ],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "33a123e7852372d93d31c1ff57ba0f1b50b626917e155204ea26b3930c8d216e",
    "reviewStatus": "generated"
  },
  "unverifiedDependencyCount": 2,
  "tags": [
    "Автоматизация"
  ],
  "undo": {
    "status": "may_be_delegated",
    "description": ""
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Автоматическая чистка простых текстов"
// Версия 1.3
// Автор Sclex

function Run() {
 window.RunSubscripts=function() {
   // Команда, задающая папку скриптов:
   // scriptsFolder(document.location.href.replace("file:///", "").replace(/main\.html$/, "Scripts"));
   scriptsFolder(document.getElementById("userCmd").getAttribute("src").replace("file:///", "").replace(/\\[^\\]+?\\[^\\]+?$/, ""));
   

   // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!======================!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    
   // Тут сами пользовательские команды поиска и обработки:

   // Команда, запускающая скрипт из заданной папки скриптов
   // (можно сделать несколько таких строк):
   runScriptInFolder("08_Структура разделов\\13_Почистить структуру.js");
   runScriptInFolder("06_Чистка\\02_Генеральная уборка.js");
   runScriptInFolder("06_Чистка\\40_Удалить теги style, id разделов, id абзацев и id картинок.js");
   runScriptInFolder("06_Чистка\\50_Расформатировать ссылки, адрес которых - пустая строка.js");
   runScriptInFolder("06_Чистка\\17_Превратить внешние ссылки в текст.js");
   runScriptInFolder("06_Чистка\\55_Удалить ссылки 'bookmark', созданные FBD.js");
   runScriptInFolder("13_Заголовки, подзаголовки, пустые строки\\528_Разметка подзаголовков, чистка пустых строк, удаление жирности и курсива в заголовках.js");
   runScriptInFolder("13_Заголовки, подзаголовки, пустые строки\\529_Удаление жирности и курсива в заголовках и подзаголовках.js");
   runScriptInFolder("06_Чистка\\68_Генеральная уборка (версия Александра Ка).js");

   // Конец пользовательских строк с командами

   // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!======================!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  
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
