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
  "summary": "Тест структуры документа_(Deep_Seek).",
  "description": "Обходит структурные элементы и строит диагностические сведения или HTML-карту/статистику. Не исправляет автоматически структуру книги. XML/HTML в названии описывает представление, которое собирает именно этот скрипт; это не отдельный вызов штатного XSD-валидатора.",
  "scope": "Текущая книга или определённые кодом её разделы.",
  "modifiesDocument": false,
  "requiresSelection": null,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже."
  ],
  "warnings": [],
  "dependencies": [],
  "dependenciesComplete": false,
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
    "description": "Явная группировка Undo в этом файле не найдена. Это не означает ни гарантированное отсутствие Undo, ни безопасное выполнение."
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
