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
