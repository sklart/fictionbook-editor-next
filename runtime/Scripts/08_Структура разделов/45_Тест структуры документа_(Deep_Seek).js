/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Тест структуры документа_(Deep_Seek)",
  "version": "1.0",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Показывает элемент у курсора и цепочку его предков до BODY.",
  "description": "Считывает имя тега и класс элемента под курсором, первые 200 символов его innerHTML и список предков до BODY. Это локальная диагностика позиции курсора, а не проверка структуры или валидности всей книги.",
  "scope": "Текущий элемент у курсора и его предки до BODY; выделение текста не требуется.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор в интересующий элемент книги и запустите скрипт.",
    "Прочитайте сведения об элементе и его предках; результат не является проверкой всего FB2-документа."
  ],
  "warnings": [],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "b664a2e5135fd9ecf7b589bf37296b9782b97be4abcc25a9d0a589b0a31453ff",
    "reviewStatus": "generated"
  },
  "tags": [
    "Структура разделов"
  ],
  "undo": {
    "status": "no_explicit_grouping",
    "description": ""
  }
}
</FBE-SCRIPT-CARD>*/
// Название: Тест структуры документа
// Версия: 1.0

function Run() {
    try {
        var range = document.selection.createRange();
        var parent = range.parentElement();
        
        var info = "Информация о выделении:\n\n";
        info += "parentElement.nodeName: " + parent.nodeName + "\n";
        info += "parentElement.className: " + parent.className + "\n";
        info += "parentElement.innerHTML: " + parent.innerHTML.substring(0, 200) + "\n\n";
        
        // Поднимаемся по дереву
        var current = parent;
        info += "Путь к корню:\n";
        while (current && current.nodeName != "BODY") {
            info += "-> " + current.nodeName + " (class: " + current.className + ")\n";
            current = current.parentNode;
        }
        
        MsgBox(info, "Тест структуры");
        
    } catch (e) {
        MsgBox("Ошибка: " + e.message, "Тест структуры");
    }
}
