/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Запуск скриптов по списку",
  "version": "1.5",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Повторяет поиск ошибок текста и выделение найденного курсивом, пока поиск возвращает результат.",
  "description": "Цикл сначала запускает поисковый скрипт, а затем скрипт форматирования курсивом для каждого найденного места. Файл поиска по прописанному пути в текущем комплекте отсутствует, поэтому исходный сценарий нельзя считать работоспособным без исправления пути.",
  "scope": "Место поиска и форматирования определяется вызываемыми скриптами и текущим положением курсора.",
  "modifiesDocument": null,
  "requiresSelection": false,
  "usage": [
    "Проверьте доступность поискового скрипта по пути из строки 19 и сохраните копию книги.",
    "Запустите список; при каждом найденном совпадении второй скрипт применяет курсив. Остановку цикла определяет результат поиска."
  ],
  "warnings": [
    "Основной поисковый файл отсутствует по указанному пути; в текущем комплекте цикл не обеспечивает заявленный поиск.",
    "При работе с исправленным путём возможны повторные правки или бесконечный цикл, если поиск не продвигает курсор. Общей операции отмены нет."
  ],
  "dependencies": [
    {
      "path": "Scripts/19_Обработка форматирования/41_Сформатировать абзац(ы) курсивом.js",
      "sha256": "897794a598a3a189b08f5fbe8fced3828286fcfb248095d26410e53fa41aac1d",
      "optional": true
    },
    {
      "path": "Scripts/31_Автоматизация/06_Поиск (без замены) ошибок текста (версия с исключениями).js",
      "optional": true
    }
  ],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "84ad55123d65e8e86ab742c0037e33314c1235684e917e4e7933672ebdba42cc",
    "reviewStatus": "generated"
  },
  "unverifiedDependencyCount": 1,
  "tags": [
    "Автоматизация"
  ],
  "undo": {
    "status": "may_be_delegated",
    "description": ""
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Запуск скриптов по списку"
// Версия 1.5
// Автор Sclex

function Run() {
 window.RunSubscripts=function() {
   // Команда, автоматически задающая папку скриптов:
   scriptsFolder(document.getElementById("userCmd").getAttribute("src").replace("file:///", "").replace(/\\[^\\]+?\\[^\\]+?$/, ""));
   // Команда, позволяющая вручную задать папку скриптов (закомментируйте строку выше
   // и раскомментируйте строку ниже):
   // scriptsFolder("c:\\program files\\fbe 2.6.7\\scripts");
   
   // Команда, запускающая скрипт из заданной папки скриптов
   // (можно сделать несколько таких строк):

//   while (runScriptInFolder("16_Поиск форматирования\\52_Перейти на следующую цитату.js")=="Found")
//    runScriptInFolder("\\19_Обработка форматирования\\41_Сформатировать абзац(ы) курсивом.js");
    
   while (runScriptInFolder("31_Автоматизация\\06_Поиск (без замены) ошибок текста (версия с исключениями).js")=="Found")
    runScriptInFolder("19_Обработка форматирования\\41_Сформатировать абзац(ы) курсивом.js");
   
   // Команда, запускающая скрипт по полному пути
   // (можно сделать несколько таких строк):
   //runScript("c:\\program files\\fbe 2.6.7\\scripts\\06_Чистка\\03_Латиница в Кириллице.js");
   
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
     return myRunCmd(fullPath);
   }
   
   function runScript(path) {
     return myRunCmd(path);
   }
   
   function myRunCmd(path) {
    window.msgText="Скрипт\n\n"+path+"\n\nне был запущен, т.к. не удалось его успешно загрузить.\n\n"+
                   "Возможная причина – файл скрипта отсутствует по тому пути, где он должен находиться.";
    window.Run=function() {
     if (window.msgText) alert(window.msgText);
    };
    var script=document.getElementById("userCmd");
    if(!script)	return;
    script.src="file:///"+path;
    var runResult=Run();
    window.Run=function() {
     if (window.msgText) alert(window.msgText);
    }
    return runResult;
   }
 }
 setTimeout("RunSubscripts()",0);
}
