/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Уменьшить номера сносок в ссылках на 1",
  "version": "1.2",
  "authors": [
    {
      "name": "TaKir",
      "role": "author",
      "contribution": "идея и реализация"
    },
    {
      "name": "DeepSeek",
      "role": "ai_assistance",
      "contribution": "реализация"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "По подтверждению уменьшает на единицу номер ссылки на примечание и её видимый номер.",
  "description": "Ищет в HTML книги ссылки точной формы с классом note, адресом #n_ЧИСЛО и текстом [ЧИСЛО]. Для каждой найденной ссылки показывает прежний и предлагаемый варианты; выбранные замены вносит одним действием. Разделы самих примечаний не перенумеровывает.",
  "scope": "Ссылки, соответствующие точному HTML-шаблону во всём fbw_body; выделение не используется. Ссылки иной разметки пропускаются.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Запустите скрипт и по каждой найденной ссылке подтвердите или пропустите уменьшение номера.",
    "После серии ответов проверьте изменённые ссылки и соответствующие разделы примечаний: скрипт не меняет их номера.",
    "При включённой статистике прочитайте итоговое количество замен."
  ],
  "warnings": [
    "Скрипт не проверяет существование целевого раздела и не перенумеровывает его; подтверждённая замена может создать неверную ссылку.",
    "После хотя бы одной замены скрипт заново записывает HTML всего текста книги, поэтому проверьте форматирование и остальные ссылки."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "24353ae85179348e6a829939e30e0a97d139be4ebe0e04880342f24c32acf510",
    "reviewStatus": "generated"
  },
  "tags": [
    "Примечания",
    "сноски"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": ""
  },
  "settings": [
    {
      "name": "showStatistics",
      "title": "Показывать итог",
      "value": 1,
      "expression": "1",
      "description": "1 — показать число замен и время работы; 0 — завершить без итогового сообщения.",
      "source": "code"
    }
  ]
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Уменьшить номера сносок в ссылках на 1" для редактора FBE
// version 1.2
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для уменьшения на -1 стандартных номеров сносок в ссылках в fb2 документах.
// Скрипт пошагово находит ссылки вида <A class=note href="...#n_3">[3]</A>
// и предлагает уменьшить номер на 1 (3 -> 2).
// Каждая конкретная замена производится с подтверждением пользователя, т.е. по принципу обычного поиска и замены.
// Можно пропускать (оставлять без изменения) отдельные номера.
// Обрабатывается ТОЛЬКО основной раздел документа (fbw_body).

// version 1.2, 09.05.2026
//======================================

function Run() {
    // ==================================================
    // НАСТРОЙКИ СКРИПТА
    // ==================================================
    
    var scriptName = "Уменьшить номера сносок в ссылках на 1";
    var version = "1.2";
    
    var showStatistics = 1;
    
    // ==================================================
    // НАЧАЛО ОСНОВНОЙ ЧАСТИ СКРИПТА
    // ==================================================
    
    var startTime = new Date().getTime();
    var changes = 0;
    
    var body = document.getElementById("fbw_body");
    if (!body) {
        MsgBox(scriptName + "\nver. " + version + "\n\nОшибка: не найден элемент fbw_body", scriptName + " ver." + version);
        return;
    }
    
    var html = body.innerHTML;
    // Ищет <A class=note href="... path ...#n_ЧИСЛО">[ЧИСЛО]</A>
    var regex = /<A class=note href="[^"]*#n_(\d+)">\[(\d+)\]<\/A>/gi;
    var newHtml = "";
    var lastIndex = 0;
    var match;
    
    function shortenHref(href) {
        // найти в href последнее вхождение "#n_..."
        var parts = href.split('#');
        if (parts.length > 1) {
            return '...' + parts[parts.length - 1];
        }
        return href;
    }
    
    function getDisplayLink(fullMatch, origNum) {
        // Извлекаем href
        var hrefMatch = fullMatch.match(/href="([^"]*)"/);
        if (hrefMatch && hrefMatch[1]) {
            var shortHref = shortenHref(hrefMatch[1]);
            return fullMatch.replace(/href="[^"]*"/, 'href="' + shortHref + '"');
        }
        return fullMatch;
    }
    
    while ((match = regex.exec(html)) !== null) {
        var fullMatch = match[0];
        var oldNum = parseInt(match[1], 10);
        var newNum = oldNum - 1;
        
        var replacement = fullMatch.replace('#n_' + oldNum, '#n_' + newNum)
                                   .replace('[' + oldNum + ']', '[' + newNum + ']');
        
        // Показываем в диалоге сокращённые ссылки
        var displayFull = getDisplayLink(fullMatch, oldNum);
        var displayNew = getDisplayLink(replacement, newNum);
        
        var confirmMsg = scriptName + "\nver. " + version + "\n\nНайдена ссылка:\n" + displayFull +
                         "\n\nЗаменить на:\n" + displayNew + "?";
        
        if (AskYesNo(confirmMsg, scriptName + " ver." + version)) {
            newHtml += html.substring(lastIndex, match.index) + replacement;
            changes++;
        } else {
            newHtml += html.substring(lastIndex, match.index) + fullMatch;
        }
        lastIndex = match.index + fullMatch.length;
    }
    newHtml += html.substring(lastIndex);
    
    if (changes > 0) {
        window.external.BeginUndoUnit(document, scriptName);
        body.innerHTML = newHtml;
        window.external.EndUndoUnit(document);
    }
    
    if (showStatistics) {
        var endTime = new Date().getTime();
        var elapsed = (endTime - startTime) / 1000;
        var msg = scriptName + "\nver. " + version + "\n\n";
        if (changes > 0) {
            msg += "✓ Замен произведено: " + changes + "\n";
        } else {
            msg += "✓ Ссылок для замены не найдено.\n";
        }
        msg += "\nВремя выполнения: " + elapsed.toFixed(3) + " сек.";
        MsgBox(msg, scriptName + " ver." + version);
    }
}
