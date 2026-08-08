// Скрипт "Диагностика структуры fb2 документа" для редактора FBE
// version 1.1
// Реализация - DeepSeek, TaKir

// version 1.1, 25.12.2025
//======================================

function Run() {
    var diagnosticInfo = "FBE ДИАГНОСТИКА СТРУКТУРЫ ДОКУМЕНТА\n";
    diagnosticInfo += "===========================================\n\n";
    
    diagnosticInfo += "1. ИНФОРМАЦИЯ О ДОКУМЕНТЕ:\n";
    diagnosticInfo += "• Document type: " + document.doctype + "\n";
    diagnosticInfo += "• Document charset: " + document.charset + "\n";
    diagnosticInfo += "• Body exists: " + (document.body ? "ДА" : "НЕТ") + "\n\n";
    
    if (!document.body) {
        diagnosticInfo += "ОШИБКА: document.body не найден!\n";
        window.external.MsgBox(diagnosticInfo);
        return;
    }
    
    // Проверяем body
    diagnosticInfo += "2. ИНФОРМАЦИЯ О BODY:\n";
    diagnosticInfo += "• Tag name: " + document.body.tagName + "\n";
    diagnosticInfo += "• Class name: " + (document.body.className || "нет") + "\n";
    diagnosticInfo += "• fbname attribute: " + (document.body.getAttribute('fbname') || "нет") + "\n";
    diagnosticInfo += "• Child nodes count: " + document.body.childNodes.length + "\n\n";
    
    // Анализируем все элементы в body
    diagnosticInfo += "3. АНАЛИЗ ЭЛЕМЕНТОВ В BODY (первые 50 элементов):\n";
    
    var allElements = document.body.childNodes;
    var elementCount = Math.min(allElements.length, 50);
    var sectionsFound = 0;
    var imagesFound = 0;
    var otherElements = 0;
    
    for (var i = 0; i < elementCount; i++) {
        var element = allElements[i];
        diagnosticInfo += i + ": ";
        
        if (element.nodeType == 1) { // ELEMENT_NODE
            var tagName = element.tagName.toLowerCase();
            var className = element.className || "";
            
            diagnosticInfo += "ELEMENT <" + tagName + ">";
            diagnosticInfo += " class='" + className + "'";
            
            // Проверяем атрибуты
            var href = element.getAttribute('href');
            var contentEditable = element.getAttribute('contenteditable');
            var onresizestart = element.getAttribute('onresizestart');
            
            if (href) diagnosticInfo += " href='" + href + "'";
            if (contentEditable) diagnosticInfo += " contenteditable='" + contentEditable + "'";
            if (onresizestart) diagnosticInfo += " onresizestart='" + onresizestart + "'";
            
            // Считаем типы элементов
            if (className.indexOf('section') !== -1) {
                diagnosticInfo += " ← СЕКЦИЯ";
                sectionsFound++;
            } else if (className.indexOf('image') !== -1) {
                diagnosticInfo += " ← ИЛЛЮСТРАЦИЯ";
                imagesFound++;
                
                // Дополнительная информация об иллюстрации
                var imgs = element.getElementsByTagName('img');
                if (imgs.length > 0) {
                    var img = imgs[0];
                    diagnosticInfo += " (img src: " + (img.src || "нет") + ")";
                }
            } else if (tagName == 'p' || tagName == 'div' || tagName == 'span') {
                diagnosticInfo += " ← ДРУГОЙ ЭЛЕМЕНТ";
                otherElements++;
            }
            
        } else if (element.nodeType == 3) { // TEXT_NODE
            var text = element.nodeValue || "";
            var trimmedText = text.replace(/^\s+|\s+$/g, '');
            
            if (trimmedText.length > 0) {
                diagnosticInfo += "TEXT: '" + trimmedText.substring(0, 50) + 
                    (trimmedText.length > 50 ? "..." : "") + "'";
            } else {
                diagnosticInfo += "TEXT (пустой или пробелы)";
            }
        } else if (element.nodeType == 8) { // COMMENT_NODE
            diagnosticInfo += "COMMENT";
        } else {
            diagnosticInfo += "Node type: " + element.nodeType;
        }
        
        diagnosticInfo += "\n";
    }
    
    diagnosticInfo += "\n4. СВОДНАЯ СТАТИСТИКА:\n";
    diagnosticInfo += "• Всего элементов проанализировано: " + elementCount + "\n";
    diagnosticInfo += "• Секций найдено: " + sectionsFound + "\n";
    diagnosticInfo += "• Иллюстраций найдено: " + imagesFound + "\n";
    diagnosticInfo += "• Других элементов: " + otherElements + "\n\n";
    
    // Дополнительный анализ: поиск всех иллюстраций в документе
    diagnosticInfo += "5. ПОИСК ВСЕХ ИЛЛЮСТРАЦИЙ В ДОКУМЕНТЕ:\n";
    
    // Способ 1: По тегу div с классом image
    var allDivs = document.getElementsByTagName('div');
    var imagesByDiv = 0;
    
    for (var j = 0; j < Math.min(allDivs.length, 100); j++) {
        var div = allDivs[j];
        if (div.className && div.className.indexOf('image') !== -1) {
            imagesByDiv++;
        }
    }
    
    diagnosticInfo += "• Div с классом image: " + imagesByDiv + " (первые 100)\n";
    
    // Способ 2: По тегу img
    var allImgs = document.getElementsByTagName('img');
    diagnosticInfo += "• Тегов img всего: " + allImgs.length + "\n";
    
    if (allImgs.length > 0) {
        diagnosticInfo += "Первые 5 img src:\n";
        for (var k = 0; k < Math.min(allImgs.length, 5); k++) {
            var img = allImgs[k];
            diagnosticInfo += "  " + k + ": " + (img.src || "нет src") + "\n";
        }
    }
    
    // Проверяем конкретную структуру иллюстрации
    diagnosticInfo += "\n6. ПРОВЕРКА СТРУКТУРЫ ИЛЛЮСТРАЦИЙ:\n";
    
    if (imagesFound > 0) {
        // Находим первую иллюстрацию
        var firstImage = null;
        for (var m = 0; m < allElements.length; m++) {
            var elem = allElements[m];
            if (elem.nodeType == 1 && elem.className && elem.className.indexOf('image') !== -1) {
                firstImage = elem;
                break;
            }
        }
        
        if (firstImage) {
            diagnosticInfo += "Первая найденная иллюстрация:\n";
            diagnosticInfo += "• Tag: " + firstImage.tagName + "\n";
            diagnosticInfo += "• Class: " + firstImage.className + "\n";
            diagnosticInfo += "• href: " + (firstImage.getAttribute('href') || "нет") + "\n";
            diagnosticInfo += "• contenteditable: " + (firstImage.getAttribute('contenteditable') || "нет") + "\n";
            
            // Проверяем вложенные элементы
            var childNodes = firstImage.childNodes;
            diagnosticInfo += "• Дочерних узлов: " + childNodes.length + "\n";
            
            for (var n = 0; n < childNodes.length; n++) {
                var child = childNodes[n];
                diagnosticInfo += "  Дочерний " + n + ": ";
                
                if (child.nodeType == 1) {
                    diagnosticInfo += "ELEMENT <" + child.tagName.toLowerCase() + ">";
                    if (child.tagName.toLowerCase() == 'img') {
                        diagnosticInfo += " src='" + (child.src || "нет") + "'";
                    }
                } else if (child.nodeType == 3) {
                    var text = child.nodeValue || "";
                    diagnosticInfo += "TEXT: '" + text.substring(0, 20) + "'";
                }
                diagnosticInfo += "\n";
            }
        }
    } else {
        diagnosticInfo += "Иллюстрации не найдены!\n";
    }
    
    // Проверка функции isImageElement
    diagnosticInfo += "\n7. ПРОВЕРКА ФУНКЦИИ isImageElement():\n";
    
    if (imagesFound > 0) {
        var testElement = null;
        for (var p = 0; p < allElements.length; p++) {
            var elem = allElements[p];
            if (elem.nodeType == 1 && elem.tagName.toLowerCase() == 'div') {
                testElement = elem;
                break;
            }
        }
        
        if (testElement) {
            var result = testIsImageElement(testElement);
            diagnosticInfo += "Тест элемента <" + testElement.tagName + " class='" + 
                (testElement.className || "") + "'>:\n";
            diagnosticInfo += "• isImageElement(): " + (result ? "ДА" : "НЕТ") + "\n";
            diagnosticInfo += "• Причина: " + (result ? "Это иллюстрация" : 
                (testElement.className && testElement.className.indexOf('image') !== -1 ? 
                "Класс содержит 'image', но функция вернула false" : 
                "Класс не содержит 'image'")) + "\n";
        }
    }
    
    // ============ ДОБАВЛЕННЫЙ КОД (ПЕРЕД ЭТОЙ СТРОКОЙ) ============
    
    diagnosticInfo += "\n8. ПРОВЕРКА findContentRoot() из скрипта 1.3:\n";
    
    // Функция findContentRoot из скрипта 1.3
    function findContentRoot() {
        // Вариант 1: Ищем div с contenteditable='true' (основной редактор)
        var allDivs = document.getElementsByTagName('div');
        
        for (var i = 0; i < allDivs.length; i++) {
            var div = allDivs[i];
            var contentEditable = div.getAttribute('contenteditable');
            if (contentEditable === 'true') {
                return div;
            }
        }
        
        // Вариант 2: Ищем div с классом section или содержащий секции
        for (var j = 0; j < allDivs.length; j++) {
            var div = allDivs[j];
            if (div.className && div.className.indexOf('section') !== -1) {
                // Ищем родителя всех секций
                var parent = div.parentNode;
                while (parent && parent.tagName.toLowerCase() === 'div') {
                    parent = parent.parentNode;
                }
                return parent || div.parentNode;
            }
        }
        
        // Вариант 3: Возвращаем body
        return document.body;
    }
    
    var root = findContentRoot();
    if (root) {
        diagnosticInfo += "• Найден корневой элемент: " + root.tagName + "\n";
        diagnosticInfo += "• contenteditable: " + (root.getAttribute('contenteditable') || "нет") + "\n";
        diagnosticInfo += "• class: " + (root.className || "нет") + "\n";
        diagnosticInfo += "• id: " + (root.getAttribute('id') || "нет") + "\n";
        diagnosticInfo += "• name: " + (root.getAttribute('name') || "нет") + "\n";
        diagnosticInfo += "• fbname: " + (root.getAttribute('fbname') || "нет") + "\n";
        diagnosticInfo += "• Дочерних элементов: " + root.childNodes.length + "\n\n";
        
        // Анализируем дочерние элементы корневого элемента
        diagnosticInfo += "9. АНАЛИЗ ДОЧЕРНИХ ЭЛЕМЕНТОВ КОРНЕВОГО ЭЛЕМЕНТА (первые 20):\n";
        
        var rootChildren = root.childNodes;
        var rootChildrenCount = Math.min(rootChildren.length, 20);
        
        for (var r = 0; r < rootChildrenCount; r++) {
            var child = rootChildren[r];
            diagnosticInfo += r + ": ";
            
            if (child.nodeType == 1) {
                var tagName = child.tagName.toLowerCase();
                var className = child.className || "";
                
                diagnosticInfo += "ELEMENT <" + tagName + ">";
                diagnosticInfo += " class='" + className + "'";
                
                if (className.indexOf('section') !== -1) {
                    diagnosticInfo += " ← СЕКЦИЯ";
                } else if (className.indexOf('image') !== -1) {
                    diagnosticInfo += " ← ИЛЛЮСТРАЦИЯ";
                }
                
                // Проверяем несколько атрибутов
                var attrs = ['contenteditable', 'href', 'onresizestart'];
                for (var a = 0; a < attrs.length; a++) {
                    var attrValue = child.getAttribute(attrs[a]);
                    if (attrValue) {
                        diagnosticInfo += " " + attrs[a] + "='" + attrValue + "'";
                    }
                }
                
            } else if (child.nodeType == 3) {
                var text = child.nodeValue || "";
                var trimmedText = text.replace(/^\s+|\s+$/g, '');
                if (trimmedText.length > 0) {
                    diagnosticInfo += "TEXT: '" + trimmedText.substring(0, 30) + "'";
                } else {
                    diagnosticInfo += "TEXT (пустой)";
                }
            } else {
                diagnosticInfo += "Node type: " + child.nodeType;
            }
            
            diagnosticInfo += "\n";
        }
    } else {
        diagnosticInfo += "• Корневой элемент НЕ НАЙДЕН!\n";
    }
    
    // ============ КОНЕЦ ДОБАВЛЕННОГО КОДА ============
    
    // Сохраняем результат в переменную для отображения
    var finalMessage = diagnosticInfo;
    
    // Показываем в окне сообщения (первые 2000 символов для IE6)
    var displayMessage = finalMessage.length > 2000 ? 
        finalMessage.substring(0, 2000) + "\n\n... (сообщение обрезано, полная информация в консоли)" : 
        finalMessage;
    
    window.external.MsgBox(displayMessage);
    
    // Также выводим в консоль, если доступно
    try {
        console.log(finalMessage);
    } catch(e) {}
}

// Тестовая функция для проверки
function testIsImageElement(element) {
    if (element.nodeType != 1) return false;
    if (element.tagName.toLowerCase() != 'div') return false;
    if (!element.className) return false;
    
    var className = element.className;
    if (className.indexOf('image') === -1) return false;
    
    return true;
}
