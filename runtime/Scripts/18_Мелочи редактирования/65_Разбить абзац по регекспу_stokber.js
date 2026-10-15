/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Разбить абзац по регекспу_stokber",
  "version": "1.0",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Разбивает выделенный текст на абзацы перед совпадениями с введённым регулярным выражением.",
  "description": "Запрашивает регулярное выражение, находит все совпадения в текущем выделенном тексте и по очереди вставляет разрыв абзаца перед найденной строкой. Для каждого совпадения повторно ищет его буквальный текст от текущего курсора, перебирая не более 1000 позиций. В конце сообщает число выполненных разрывов и время. При отсутствии выделенного текста совпадений не будет. Исходное выделение может охватывать несколько абзацев; команда не проверяет, что это один P.",
  "scope": "Совпадения вычисляются только в текущем выделенном тексте. После сворачивания выделения их буквальные тексты ищутся от курсора без повторной проверки границ выделения, не далее 1000 проверяемых смещений на совпадение.",
  "modifiesDocument": true,
  "requiresSelection": true,
  "usage": [
    "Выделите текст, в котором нужно вставить разрывы абзацев, и запустите команду.",
    "Введите регулярное выражение. Для литеральной записи обратных слешей в шаблоне проверяйте фактическое содержимое поля: предложенный шаблон в исходнике записан некорректно.",
    "Сверьте итог «выполнено из найденных» и проверьте позиции разрывов, особенно при повторяющихся совпадениях и встроенном оформлении."
  ],
  "warnings": [
    "Автоматически предложенный шаблон не соответствует задуманному поиску сносочных маркеров: обратные слеши в строковом литерале не удвоены.",
    "Совпадение дальше 1000 символов от текущего курсора может не получить разрыва; итоговое число выполненных операций следует сверять с числом совпадений.",
    "Повторный поиск по буквальному тексту не ограничен исходным выделением: при одинаковых строках разрыв может попасть в другое место, в том числе за его пределами.",
    "Операция отмены начинается перед вставкой разрывов, но при ошибке завершающий вызов пропускается; гарантированный откат через Ctrl+Z не установлен."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "85a6d70fecb8b25f1546cf14c1d5d60317fb706dcdcb66866590b40519ae5245",
    "reviewStatus": "generated"
  },
  "tags": [
    "Мелочи редактирования",
    "параметры"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Разрывы обрамлены одной операцией отмены, если цикл завершается штатно. При исключении выполнение переходит в обработчик ошибки без завершения операции; возможность отката нужно проверять."
  },
  "settings": []
}
</FBE-SCRIPT-CARD>*/
// Скрипт «Разбить абзац по регекспу» v.1.0.

function Run()
{
var name = 'Разбить абзац по регекспу';
var version = '1.0';
function minimalWorking() {
    try {
        // Запрашиваем регулярное выражение у пользователя
        var regexPattern = prompt("Введите регулярное выражение для поиска совпадения, перед которым будет разбиваться абзац:", "\\[\\d+\\]");
        
        if (regexPattern === null) {
            alert("Операция отменена");
            return;
        }
        
        if (!regexPattern) {
            alert("Регулярное выражение не может быть пустым");
            return;
        }
        
        // Создаем RegExp объект из строки
        var regex;
        try {
            regex = new RegExp(regexPattern, "g");
        } catch(e) {
            alert("Ошибка в регулярном выражении: " + e.message);
            return;
        }
        
        var r = document.selection.createRange();
        var t = r.text;
        
        // Подсчет совпадений
        var matches = [];
        var match;
        while ((match = regex.exec(t)) !== null) {
            matches.push({
                text: match[0],
                index: match.index,
                length: match[0].length
            });
            // Избегаем бесконечного цикла
            if (regex.lastIndex === match.index) {
                regex.lastIndex++;
            }
        }
        
        var n = matches.length;
        
        if (n == 0) {
            alert("Совпадений с шаблоном: '" + regexPattern + "' не найдено!");
            return;
        }
        
         alert("Найдено совпадений: " + n);
		 
		 // Запоминаем время начала (в мс)
        var startTime = new Date().getTime();

        
        // Начинаем блок отмены действий
        window.external.BeginUndoUnit(document,"разбивку на параграфы"); 

        // Снимаем выделение - перемещаем курсор в начало
        r.collapse(true);
        r.select();
        
        var c = 0;
        
        for (var i = 0; i < n; i++) {
            var cur = document.selection.createRange();
            var found = false;
            
            // Ручной поиск позиции совпадения
            for (var p = 0; p < 1000; p++) {
                var test = cur.duplicate();
                test.moveStart("character", p);
                test.collapse(true);
                test.moveEnd("character", matches[i].text.length);
                
                // Проверяем, совпадает ли текст
                if (test.text === matches[i].text) {
                    // Устанавливаем курсор в начало совпадения
                    test.collapse(true);
                    test.select();
                    
                    // Вставляем параграф
                    document.execCommand('insertParagraph', false, null);
                    c++;
                    
                    // Перемещаем курсор на 1 символ вправо
                    var after = document.selection.createRange();
                    after.move("character", 1);
                    after.collapse(true);
                    after.select();
                    
                    found = true;
                    break;
                }
            }
            
            // Небольшая пауза для стабильности
            var start = new Date().getTime();
            while (new Date().getTime() - start < 10) {}
        }
        
        // Завершаем блок отмены действий
        window.external.EndUndoUnit(document);
        
        //------------------------------------------------
        // Запоминаем время окончания (в мс)
        var endTime = new Date().getTime();
        // Вычисляем затраченное время (в мс)
        var elapsedTimeMs = endTime - startTime;
        // Преобразуем в минуты, секунды и миллисекунды
        var totalMs = elapsedTimeMs;
        var minutes = Math.floor(totalMs / 60000); // 60 000 мс = 1 мин
        totalMs -= minutes * 60000;
        var seconds = Math.floor(totalMs / 1000); // 1 000 мс = 1 сек
        var milliseconds = totalMs - seconds * 1000;
        // Форматируем компоненты (добавляем ведущие нули при необходимости)
        var formattedMinutes = minutes < 10 ? "0" + minutes : minutes;
        var formattedSeconds = seconds < 10 ? "0" + seconds : seconds;
        var formattedMilliseconds = milliseconds < 10 ? "00" + milliseconds :
                               milliseconds < 100 ? "0" + milliseconds : milliseconds;
        // Собираем строку результата
        var resultString = formattedMinutes + ":" +
                          formattedSeconds + ":" +
                          formattedMilliseconds;
        
        alert("Выполнено замен: "+c+" из "+n+"\nВремя выполнения: "+resultString+" (мин:сек:мс)\n\nСкрипт «"+name+"» v."+version);
        //------------------------------------------
    } catch(e) {
        alert("Произошла ошибка: " + e.message);
    }
}
minimalWorking();
 }
