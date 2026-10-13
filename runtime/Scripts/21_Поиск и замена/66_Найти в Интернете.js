/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Найти в Интернете",
  "version": "1.2",
  "authors": [
    {
      "name": "stokber",
      "role": "contributor",
      "contribution": "declared_credit"
    },
    {
      "name": "DeepSeek",
      "role": "ai_assistance"
    },
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "partial_attribution",
  "summary": "Открывает интернет-поиск по выделенному тексту.",
  "description": "Получает выделение, очищает пробелы/переносы, формирует поисковый адрес и открывает его во внешнем браузере/оболочке. Книгу не редактирует, но передаёт выбранный текст поисковому сервису.",
  "scope": "Область определяется условиями и обходом DOM в данном скрипте.",
  "modifiesDocument": false,
  "requiresSelection": true,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Подготовьте выделение, описанное в scope и условиях запуска; некоторые команды работают с целыми абзацами.",
    "Проверьте перечисленные параметры: часть задаётся в исходном JS, часть — через общий диалог настроек.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже."
  ],
  "warnings": [
    "Выделенный фрагмент покидает локальный редактор при открытии поискового запроса. Не используйте на конфиденциальном тексте."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "f8a7a88ef9bc9091b9cc4901302ff9eff99875875e39c71a13d222160701ebe1",
    "reviewStatus": "generated"
  },
  "tags": [
    "Поиск и замена",
    "параметры"
  ],
  "undo": {
    "status": "no_explicit_grouping",
    "description": "Явная группировка Undo в этом файле не найдена. Это не означает ни гарантированное отсутствие Undo, ни безопасное выполнение."
  },
  "settings": [
    {
      "name": "cleanedText",
      "value": "",
      "expression": "\"\"",
      "description": "Очищаем пробелы и переносы строк:",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    }
  ]
}
</FBE-SCRIPT-CARD>*/

// Скрипт «Найти в Интернете v.1.2»
// stokber + DeepSeek (апрель, 2026)


function Run() {

    try {

        // Получаем выделенный текст:
        var selectedText = "";
        if (window.getSelection) {
            selectedText = window.getSelection().toString();
        } else if (document.selection && document.selection.type != "Control") {
            selectedText = document.selection.createRange().text;
        }
        
        // Очищаем пробелы и переносы строк:
        var cleanedText = "";
        if (selectedText) {
            // Удаляем пробелы в начале
            var i = 0;
            while (i < selectedText.length && (selectedText.charAt(i) == ' ' || selectedText.charAt(i) == '\n' || selectedText.charAt(i) == '\r' || selectedText.charAt(i) == '\t')) {
                i++;
            }
            
            // Удаляем пробелы в конце:
            var j = selectedText.length - 1;
            while (j >= 0 && (selectedText.charAt(j) == ' ' || selectedText.charAt(j) == '\n' || selectedText.charAt(j) == '\r' || selectedText.charAt(j) == '\t')) {
                j--;
            }
            
            // Извлекаем очищенный текст:
            if (i <= j) {
                cleanedText = selectedText.substring(i, j + 1);
            }
        }
        
        // Проверяем, есть ли выделенный текст:
        if (!cleanedText) {
            MsgBox("Не выделен текст для поиска");
            return;
        }
        
        // Создаем GUI интерфейс:
        createSearchGUI(cleanedText);
        
    } catch (e) {
        MsgBox("Ошибка выполнения скрипта: " + e.message);
    }
    
    function createSearchGUI(text) {
        // Кодируем текст для URL
        var encodedText = encodeURIComponent(text);
        
        // Создаем диалоговое окно с кнопками:
        var dialogHtml = '<!DOCTYPE html><html><head>' +
                         '<meta http-equiv="Content-Type" content="text/html; charset=utf-8">' +
                         '<title>Найти: GUI</title>' +
                         '<style>' +
                         'body {font-family: Arial; margin: 0; padding: 0;}' +
                         'button {width: 250px; height: 20px; display: block; margin: 0; padding: 2px; border: 1px solid #ccc; background: #f0f0f0; cursor: pointer; text-align: left;}' +
                         'button:hover {background: #e0e0e0;}' +
                         '</style>' +
                         '</head><body>' +
						 '<button onclick="searchGoogle()">Найти в Гугл</button>' +
                         '<button onclick="searchYandex()">Найти в Яндекс</button>' +
                         '<button onclick="searchWikiRu()">Найти в Вики</button>' +
                         '<button onclick="searchWikiEn()">Найти в Wiki</button>' +
                         '<button onclick="searchTranslate()">Перевести</button>' +
                         '<button onclick="searchSpelling()">Как пишется</button>' +
                         '<button onclick="searchTogetherOrSeparate()">Слитно или раздельно</button>' +

                         '<script>' +
                         'var searchText = "' + encodedText.replace(/"/g, '\\"') + '";' +
                         
                        'function searchGoogle() {' +
                         '  try {' +
                         '    var shell = new ActiveXObject("WScript.Shell");' +
                         '    shell.Run("http://www.google.com/search?q="+searchText);' +
                         '  } catch(e) {' +
                         '    alert("Ошибка при открытии Гугл: " + e.message);' +
                         '  }' +
                         '  window.close();' +
                         '}' +

                         'function searchYandex() {' +
                         '  try {' +
                         '    var shell = new ActiveXObject("WScript.Shell");' +
                         '    shell.Run("http://www.yandex.ru/search?text=" + searchText + "&lr=24876");' +
                         '  } catch(e) {' +
                         '    alert("Ошибка при открытии Яндекс: " + e.message);' +
                         '  }' +
                         '  window.close();' +
                         '}' +
                         
                         'function searchWikiRu() {' +
                         '  try {' +
                         '    var shell = new ActiveXObject("WScript.Shell");' +
                         '    shell.Run("https://ru.wikipedia.org/wiki/" + searchText);' +
                         '  } catch(e) {' +
                         '    alert("Ошибка при открытии Википедии: " + e.message);' +
                         '  }' +
                         '  window.close();' +
                         '}' +
                         
                         'function searchWikiEn() {' +
                         '  try {' +
                         '    var shell = new ActiveXObject("WScript.Shell");' +
                         '    shell.Run("https://en.wikipedia.org/wiki/" + searchText);' +
                         '  } catch(e) {' +
                         '    alert("Ошибка при открытии Wikipedia: " + e.message);' +
                         '  }' +
                         '  window.close();' +
                         '}' +
                         
                         'function searchTranslate() {' +
                         '  try {' +
                         '    var shell = new ActiveXObject("WScript.Shell");' +
                         '    shell.Run("https://translate.google.com/#auto/ru/" + searchText);' +
                         '  } catch(e) {' +
                         '    alert("Ошибка при открытии Переводчик: " + e.message);' +
                         '  }' +
                         '  window.close();' +
                         '}' +
                         
                         'function searchSpelling() {' +
                         '  try {' +
                         '    var shell = new ActiveXObject("WScript.Shell");' +
                         '    shell.Run("https://www.yandex.ru/search?text=" + searchText + encodeURIComponent(": как правильно пишется?") + "&lr=24876");' +
                         '  } catch(e) {' +
                         '    alert("Ошибка при открытии Яндекс: " + e.message);' +
                         '  }' +
                         '  window.close();' +
                         '}' +
                         
                         'function searchTogetherOrSeparate() {' +
                         '  try {' +
                         '    var shell = new ActiveXObject("WScript.Shell");' +
                         '    shell.Run("https://www.yandex.ru/search?text=" + searchText + encodeURIComponent(": слитно или раздельно?") + "&lr=24876");' +
                         '  } catch(e) {' +
                         '    alert("Ошибка при открытии Яндекс: " + e.message);' +
                         '  }' +
                         '  window.close();' +
                         '}' +
                         
                         '</script>' +
                         '</body></html>';
        
        // Открываем новое окно с интерфейсом:
        var searchWindow = window.open("", "SearchGUI", "width=250,height=240,resizable=no,scrollbars=no,toolbar=no,menubar=no");
        if (searchWindow) {
            searchWindow.document.write(dialogHtml);
            searchWindow.document.close();
            searchWindow.focus();
        } else {
            MsgBox("Не удалось открыть окно поиска. Возможно, блокировщик всплывающих окон запрещает открытие новых окон.");
        }
    }
}
