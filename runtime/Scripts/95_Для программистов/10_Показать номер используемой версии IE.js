/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Показать номер используемой версии IE",
  "version": "1.0",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Показывает номер IE или метку Edge по строке User-Agent встроенного окна.",
  "description": "Разбирает navigator.userAgent: ищет MSIE, Trident с rv: или Edge/ и выводит найденный номер либо сообщение о другом браузере. Не проверяет установленную версию IE или режим совместимости редактора.",
  "scope": "Браузерное окружение текущего окна редактора; содержимое книги не читается.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Запустите скрипт в редакторе и прочитайте диагностическое сообщение.",
    "Результат основан на строке User-Agent окна и не является проверкой установленной версии браузера или режима совместимости."
  ],
  "warnings": [],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "12a7846f580e7061b45966301e341f0f5feaa07ccbdee1fb5f9c0f4bd49bd4de",
    "reviewStatus": "generated"
  },
  "tags": [
    "Для программистов"
  ],
  "undo": {
    "status": "no_explicit_grouping",
    "description": ""
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Показать номер используемой версии IE" для редактора FBE
// version 1.0
// Автор: Sclex

// Скрипт предназначен для показа версии IE, используемой редатором FBE.

// version 1.0, 27.05.2026
//======================================


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
    if (ieVersion) {
        MsgBox('Используется Internet Explorer версии ' + ieVersion);
    } else {
        MsgBox('Это не Internet Explorer');
    }
}
