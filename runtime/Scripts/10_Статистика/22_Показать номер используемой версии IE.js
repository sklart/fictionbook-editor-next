/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Показать номер используемой версии IE",
  "version": "1.1",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Показывает номер используемой версии IE.",
  "description": "Собирает сведения из текущего документа и выводит результат сообщением, в буфер обмена или HTML-отчёт согласно коду варианта. Не выполняет автоматического исправления анализируемых элементов.",
  "scope": "Текущая книга или определённые кодом её разделы.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже."
  ],
  "warnings": [],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "8991a6208fdfa972601c9f690843cd89c565fde596c66df47a1fc23096b8472e",
    "reviewStatus": "generated"
  },
  "tags": [
    "Статистика"
  ],
  "undo": {
    "status": "no_explicit_grouping",
    "description": "Явная группировка Undo в этом файле не найдена. Это не означает ни гарантированное отсутствие Undo, ни безопасное выполнение."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Показать номер используемой версии IE"
// Версия 1.1

function Run() {

    function getIEVersion() {
        var ua = window.navigator.userAgent;
        
        // Проверяем наличие MSIE или Trident
        var msie = ua.indexOf('MSIE ');
        var trident = ua.indexOf('Trident/');
        
        if (msie > 0) {
            // IE 10 или старше (возвращает номер версии)
            return parseInt(ua.substring(msie + 5, ua.indexOf('.', msie)), 10);
        }
        
        if (trident > 0) {
            // IE 11 (определяем по rv:)
            var rv = ua.indexOf('rv:');
            return parseInt(ua.substring(rv + 3, ua.indexOf('.', rv)), 10);
        }
        
        // Проверка для Edge (не IE)
        var edge = ua.indexOf('Edge/');
        if (edge > 0) {
            return 'Edge';
        }
        
        return false; // Не IE
    }
    
    // Использование:
    var ieVersion = getIEVersion();
    var ua = window.navigator.userAgent;
    if (ieVersion) {
        MsgBox('Используется Internet Explorer версии ' + ieVersion+".\n\n"+
               'Значение window.navigator.userAgent:\n"'+ua+'"');
    } else {
        MsgBox('Это не Internet Explorer');
    }
}
