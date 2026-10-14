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
  "summary": "Показывает номер IE или метку Edge и строку User-Agent текущего окна.",
  "description": "Ищет MSIE, Trident с rv: или Edge/ в navigator.userAgent. При распознавании показывает номер IE либо слово Edge и саму строку User-Agent; установленную версию браузера и режим совместимости отдельно не проверяет.",
  "scope": "Браузерное окружение текущего окна редактора; книгу не анализирует.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Запустите скрипт и прочитайте номер или метку браузера вместе со строкой User-Agent.",
    "Результат отражает строку User-Agent окна, а не независимую проверку установленного IE."
  ],
  "warnings": [
    "При строке User-Agent с Edge/ результат формулируется как «Internet Explorer версии Edge», хотя Edge не является версией IE; ориентируйтесь на показанную строку User-Agent."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
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
    "description": "Скрипт читает браузерную строку и показывает сообщение; книгу не меняет."
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
