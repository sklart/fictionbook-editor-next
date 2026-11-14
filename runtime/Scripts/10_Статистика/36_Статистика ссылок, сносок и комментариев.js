// Скрипт "Статистика ссылок, сносок и комментариев" для редактора FBE
// version 4.4
// Идея - TaKir
// Реализация - DeepSeek, TaKir

// Скрипт предназначен для отображения максимально подробной статистики
// по внешним и внутренним ссылкам в fb2 документе, ведет подсчет сносок и комментариев.
// Скрипт умеет определять битые маркеры сносок и комментариев
// и недостающие секции сносок и комментариев.
// Скрипт сообщает о несовпадении количества и номеров маркеров и текстов сносок и комментариев,
// что значительно упрощает обнаружение проблем в подобных случаях.

// Никаких изменений в документе скрипт не производит.

// version 4.4, 11.01.2026
//======================================

function Run() {
    try {
        var startTime = new Date().getTime();
        var scriptName = "Статистика ссылок, сносок и комментариев";
        var scriptVersion = "4.4";
        
        // НАСТРОЙКИ:
        // 1 - выводить полную статистику (все строки, даже с нулями)
        // 0 - выводить сокращенную статистику (только ненулевые значения)
        var SHOW_FULL_STATS = 0; // по умолчанию полная статистика
        
        // Список доменных зон
        var domainZones = [
            // Международные
            "com", "net", "org", "edu", "gov", "mil", "int",
            
            // Страны СНГ
            "ru", "su", "рф", 
            "ua", "by", "kz", "az", "am", "ge", "kg", "md", "tj", "tm", "uz",
            
            // Европа
            "uk", "gb", "de", "fr", "it", "es", "pl", "cz", "sk", "nl", "be", "at", "ch",
            "se", "no", "fi", "dk", "is", "ie", "pt", "gr", "hu", "ro", "bg", "rs", "hr",
            "si", "mk", "al", "ba", "me", "mt", "cy", "lu", "li", "ee", "lv", "lt",
            
            // Азия
            "jp", "cn", "in", "kr", "tw", "hk", "sg", "my", "th", "vn", "id", "ph",
            "tr", "il", "sa", "ae", "ir", "iq", "sy", "jo", "lb", "ye", "om", "kw",
            "qa", "bh",
            
            // Америка
            "us", "ca", "mx", "br", "ar", "cl", "co", "pe", "ve", "ec", "bo", "py",
            "uy", "cr", "do", "gt", "hn", "ni", "pa", "sv", "pr", "jm", "tt", "bs",
            
            // Африка
            "za", "eg", "ma", "dz", "tn", "ly", "ng", "ke", "et", "gh", "ci", "cm",
            "ug", "mw", "zm", "zw", "sn", "ml", "ao", "mz",
            
            // Океания
            "au", "nz", "pg", "fj", "sb", "vu", "ws", "to", "ki",
            
            // Общие домены
            "info", "biz", "name", "pro", "aero", "coop", "museum", "mobi",
            "tel", "asia", "cat", "jobs", "travel",
            
            // Популярные новые домены
            "tv", "cc", "ws", "io", "ai", "me", "xxx", "xyz", "online", "site", "tech",
            "space", "store", "shop", "blog", "app", "dev", "cloud", "host", "digital",
            
            // Российские региональные
            "org.ru", "net.ru", "pp.ru", "msk.ru", "spb.ru", "com.ru", "edu.ru",
            "nov.ru", "perm.ru", "samara.ru", "volgograd.ru", "vrn.ru", "krasnoyarsk.ru",
            "nnov.ru", "irkutsk.ru", "khabarovsk.ru", "vladivostok.ru", "yekaterinburg.ru",
            "chel.ru", "tatarstan.ru", "bashkiria.ru", "kaluga.ru"
        ];
        
        // Инициализация счетчиков
        var counters = {
            // Общие счетчики
            totalLinks: 0,
            externalLinks: 0,
            internalElements: 0,
            
            // Счетчики по типам ссылок
            www: {total: 0, annotation: 0, history: 0, main: 0, notes: 0, comments: 0},
            http: {total: 0, annotation: 0, history: 0, main: 0, notes: 0, comments: 0},
            https: {total: 0, annotation: 0, history: 0, main: 0, notes: 0, comments: 0},
            domain: {total: 0, annotation: 0, history: 0, main: 0, notes: 0, comments: 0},
            mailto: {total: 0, annotation: 0, history: 0, main: 0, notes: 0, comments: 0},
            ftp: {total: 0, annotation: 0, history: 0, main: 0, notes: 0, comments: 0},
            file: {total: 0, annotation: 0, history: 0, main: 0, notes: 0, comments: 0},
            textLinks: {total: 0, annotation: 0, history: 0, main: 0, notes: 0, comments: 0},
            
            // Навигационные ссылки FBE
            navLinks: {total: 0, pairs: 0, annotation: 0, history: 0, main: 0, notes: 0, comments: 0},
            
            // Маркеры сносок и комментариев
            footnoteMarkers: {total: 0, annotation: 0, history: 0, main: 0, notes: 0, comments: 0},
            commentMarkers: {total: 0, annotation: 0, history: 0, main: 0, notes: 0, comments: 0},
            
            // Битые ссылки на сноски/комментарии
            brokenFootnoteMarkers: {total: 0, annotation: 0, history: 0, main: 0, notes: 0, comments: 0},
            brokenCommentMarkers: {total: 0, annotation: 0, history: 0, main: 0, notes: 0, comments: 0},
            
            // Типы представления ссылок
            pureLinks: {total: 0, annotation: 0, history: 0, main: 0, notes: 0, comments: 0},
            hiddenLinks: {total: 0, annotation: 0, history: 0, main: 0, notes: 0, comments: 0},
            
            // Домены в тексте
            textDomains: {total: 0, annotation: 0, history: 0, main: 0, notes: 0, comments: 0},
            
            // Внутренние элементы
            footnotes: 0,
            comments: 0
        };
        
        // Массивы для хранения номеров сносок и комментариев
        var footnoteMarkerNumbers = []; // Номера из маркеров сносок
        var footnoteNumbers = [];       // Номера из секций сносок
        var commentMarkerNumbers = [];  // Номера из маркеров комментариев  
        var commentNumbers = [];        // Номера из секций комментариев
        
        // Функция для нормализации текста
        function trimStr(str) {
            if (!str || str.length === 0) return "";
            var result = str.replace(/^\s+|\s+$/g, '');
            return result;
        }
        
        // Функция проверки наличия подстроки (для строк)
        function containsStr(str, search) {
            if (!str || !search) return false;
            return str.indexOf(search) !== -1;
        }
        
        // Функция проверки начала строки
        function startsWithStr(str, search) {
            if (!str || !search) return false;
            return str.indexOf(search) === 0;
        }
        
        // Функция для получения текста элемента
        function getElementText(element) {
            if (!element) return "";
            if (element.innerText !== undefined) {
                return element.innerText;
            }
            if (element.textContent !== undefined) {
                return element.textContent;
            }
            return "";
        }
        
        // Функция для извлечения номера из ID (например, из "n_123" или "c_45")
        function extractNumberFromId(id) {
            if (!id || id.length === 0) return null;
            
            // Ищем подчеркивание
            var underscoreIndex = id.indexOf('_');
            if (underscoreIndex === -1) return null;
            
            // Берем часть после подчеркивания
            var numberPart = id.substring(underscoreIndex + 1);
            
            // Пробуем преобразовать в число
            var number = parseInt(numberPart, 10);
            
            // Проверяем, что это действительно число
            if (isNaN(number)) return null;
            
            return number;
        }
        
        // Функция для поиска пропущенных номеров в массиве
        function findMissingNumbers(numbersArray) {
            if (!numbersArray || numbersArray.length === 0) return [];
            
            // Сортируем массив чисел
            var sortedNumbers = [];
            for (var i = 0; i < numbersArray.length; i++) {
                sortedNumbers.push(numbersArray[i]);
            }
            
            // Сортируем пузырьком (IE6 не имеет Array.sort())
            for (var i = 0; i < sortedNumbers.length - 1; i++) {
                for (var j = 0; j < sortedNumbers.length - i - 1; j++) {
                    if (sortedNumbers[j] > sortedNumbers[j + 1]) {
                        var temp = sortedNumbers[j];
                        sortedNumbers[j] = sortedNumbers[j + 1];
                        sortedNumbers[j + 1] = temp;
                    }
                }
            }
            
            // Находим пропущенные номера
            var missing = [];
            var minNumber = sortedNumbers[0];
            var maxNumber = sortedNumbers[sortedNumbers.length - 1];
            
            for (var num = minNumber; num <= maxNumber; num++) {
                var found = false;
                for (var i = 0; i < sortedNumbers.length; i++) {
                    if (sortedNumbers[i] === num) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    missing.push(num);
                }
            }
            
            return missing;
        }
        
        // Функция для форматирования списка пропущенных номеров
        function formatMissingNumbers(missingNumbers) {
            if (!missingNumbers || missingNumbers.length === 0) return "";
            
            if (missingNumbers.length === 1) {
                return "№ " + missingNumbers[0];
            }
            
            var result = "";
            for (var i = 0; i < missingNumbers.length; i++) {
                if (i > 0) {
                    result += ", ";
                }
                result += "№ " + missingNumbers[i];
            }
            
            return result;
        }
        
        // Функция проверки доменной зоны в тексте
        function isDomainInText(text) {
            if (!text || text.length < 3) return false;
            
            var domainPattern = /\b(?:[a-zA-Z0-9](?:[a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?\.)+[a-zA-Z]{2,}\b/g;
            
            var matches = text.match(domainPattern);
            if (!matches || matches.length === 0) return false;
            
            for (var i = 0; i < matches.length; i++) {
                var domain = matches[i].toLowerCase();
                
                if (/^\d+\.\d+\.\d+\.\d+$/.test(domain)) continue;
                
                var lastDotIndex = domain.lastIndexOf('.');
                if (lastDotIndex === -1) continue;
                
                var zone = domain.substring(lastDotIndex + 1);
                
                for (var j = 0; j < domainZones.length; j++) {
                    if (zone === domainZones[j]) {
                        return true;
                    }
                }
            }
            
            return false;
        }
        
        // Функция для извлечения доменов из текста
        function extractDomainsFromText(text) {
            var domains = [];
            if (!text || text.length < 3) return domains;
            
            var domainPattern = /\b(?:[a-zA-Z0-9](?:[a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?\.)+[a-zA-Z]{2,}\b/g;
            
            var matches = text.match(domainPattern);
            if (!matches) return domains;
            
            for (var i = 0; i < matches.length; i++) {
                var domain = matches[i].toLowerCase();
                
                if (/^\d+\.\d+\.\d+\.\d+$/.test(domain)) continue;
                
                var lastDotIndex = domain.lastIndexOf('.');
                if (lastDotIndex === -1) continue;
                
                var zone = domain.substring(lastDotIndex + 1);
                
                for (var j = 0; j < domainZones.length; j++) {
                    if (zone === domainZones[j]) {
                        domains.push(domain);
                        break;
                    }
                }
            }
            
            return domains;
        }
        
        // Функция для склонения слов
        function pluralize(number, one, two, five) {
            number = Math.abs(number) % 100;
            if (number >= 5 && number <= 20) {
                return five;
            }
            number = number % 10;
            if (number === 1) {
                return one;
            }
            if (number >= 2 && number <= 4) {
                return two;
            }
            return five;
        }
        
        // Функция для определения раздела
        function getSectionType(element) {
            if (!element) return "unknown";
            
            var parent = element;
            while (parent) {
                if (parent.tagName && parent.tagName.toLowerCase() === "body") {
                    var name = parent.getAttribute("name") || "";
                    name = trimStr(name.toLowerCase());
                    if (name === "notes") return "notes";
                    if (name === "comments") return "comments";
                }
                
                if (parent.tagName && parent.tagName.toLowerCase() === "div") {
                    var fbname = parent.getAttribute("fbname") || "";
                    fbname = trimStr(fbname.toLowerCase());
                    if (fbname === "notes") return "notes";
                    if (fbname === "comments") return "comments";
                }
                
                if (parent.className) {
                    var className = parent.className.toString().toLowerCase();
                    if (containsStr(className, "annotation")) return "annotation";
                    if (containsStr(className, "history")) return "history";
                }
                
                parent = parent.parentNode;
            }
            
            return "main";
        }
        
        // Функция проверки, является ли элемент сноской
        function isFootnoteElement(element) {
            if (!element) return false;
            
            // Проверяем класс
            if (element.className) {
                var className = element.className.toString().toLowerCase();
                if (containsStr(className, "section")) {
                    // Проверяем, находится ли элемент в разделе сносок
                    var parent = element.parentNode;
                    while (parent) {
                        if (parent.tagName && parent.tagName.toLowerCase() === "body") {
                            var name = parent.getAttribute("name") || "";
                            name = trimStr(name.toLowerCase());
                            if (name === "notes") return true;
                        }
                        
                        if (parent.tagName && parent.tagName.toLowerCase() === "div") {
                            var fbname = parent.getAttribute("fbname") || "";
                            fbname = trimStr(fbname.toLowerCase());
                            if (fbname === "notes") return true;
                        }
                        parent = parent.parentNode;
                    }
                }
            }
            
            return false;
        }
        
        // Функция проверки, является ли элемент комментарием
        function isCommentElement(element) {
            if (!element) return false;
            
            // Проверяем класс
            if (element.className) {
                var className = element.className.toString().toLowerCase();
                if (containsStr(className, "section")) {
                    // Проверяем, находится ли элемент в разделе комментариев
                    var parent = element.parentNode;
                    while (parent) {
                        if (parent.tagName && parent.tagName.toLowerCase() === "body") {
                            var name = parent.getAttribute("name") || "";
                            name = trimStr(name.toLowerCase());
                            if (name === "comments") return true;
                        }
                        
                        if (parent.tagName && parent.tagName.toLowerCase() === "div") {
                            var fbname = parent.getAttribute("fbname") || "";
                            fbname = trimStr(fbname.toLowerCase());
                            if (fbname === "comments") return true;
                        }
                        parent = parent.parentNode;
                    }
                }
            }
            
            return false;
        }
        
        // Функция для получения компактного описания расположения ссылок
        function getCompactLocation(counter) {
            var locations = [];
            
            if (counter.annotation > 0) locations.push("аннотации: " + counter.annotation);
            if (counter.history > 0) locations.push("хистори: " + counter.history);
            if (counter.main > 0) locations.push("основном разделе: " + counter.main);
            if (counter.notes > 0) locations.push("разделе сносок: " + counter.notes);
            if (counter.comments > 0) locations.push("разделе комментариев: " + counter.comments);
            
            if (locations.length === 0) return "";
            
            var total = counter.annotation + counter.history + counter.main + counter.notes + counter.comments;
            
            // Если всего одна ссылка и она в одном месте
            if (total === 1) {
                for (var i = 0; i < locations.length; i++) {
                    if (locations[i].indexOf(": 1") !== -1) {
                        var locationName = locations[i].split(":")[0];
                        return " (в " + locationName + ")";
                    }
                }
            }
            
            // Если несколько ссылок, но все в одном разделе
            var nonZeroCount = 0;
            var singleLocation = "";
            for (var i = 0; i < locations.length; i++) {
                if (locations[i].indexOf(": 0") === -1) {
                    nonZeroCount++;
                    if (nonZeroCount === 1) {
                        singleLocation = locations[i].split(":")[0];
                    }
                }
            }
            
            if (nonZeroCount === 1) {
                return " (все в " + singleLocation + ")";
            }
            
            return ""; // Возвращаем пустую строку, если нужно обычное детализированное отображение
        }
        
        // ================================
        // ФАЗА 1: Поиск всех элементов с ID для проверки пар
        // ================================
        
        // Собираем все элементы с ID, которые могут быть целями для ссылок
        var elementsWithId = {};
        var allElements = document.getElementsByTagName("*");
        
        for (var i = 0; i < allElements.length; i++) {
            var element = allElements[i];
            var elementId = element.getAttribute("id") || "";
            elementId = trimStr(elementId);
            
            if (elementId.length > 0) {
                // Сохраняем информацию об элементе
                var tagName = element.tagName ? element.tagName.toLowerCase() : "";
                var className = element.className || "";
                var isFootnote = isFootnoteElement(element);
                var isComment = isCommentElement(element);
                
                elementsWithId[elementId] = {
                    element: element,
                    tagName: tagName,
                    className: className,
                    isFootnote: isFootnote,
                    isComment: isComment,
                    id: elementId
                };
            }
        }
        
        // ================================
        // ФАЗА 2: Поиск и анализ всех ссылок <a>
        // ================================
        
        var allLinks = document.getElementsByTagName("a");
        counters.totalLinks = allLinks.length;
        
        // Массив для хранения навигационных ссылок, которые могут образовывать пары
        var potentialNavLinks = [];
        
        for (var i = 0; i < allLinks.length; i++) {
            var link = allLinks[i];
            var hrefValue = "";
            var linkText = getElementText(link);
            var sectionType = getSectionType(link);
            
            hrefValue = link.getAttribute("l:href") || link.getAttribute("href") || "";
            hrefValue = trimStr(hrefValue);
            
            if (hrefValue.length === 0) continue;
            
            var hrefLower = hrefValue.toLowerCase();
            
            // Извлекаем targetId из ссылки
            var targetId = "";
            var hashIndex = hrefLower.indexOf("#");
            if (hashIndex !== -1) {
                targetId = hrefLower.substring(hashIndex + 1); // Без #
                targetId = trimStr(targetId);
            }
            
            // Проверяем на маркеры сносок (#n_X)
            var isFootnoteMarker = false;
            var isBrokenFootnoteMarker = false;
            
            if (targetId.length > 0 && startsWithStr(targetId, "n_")) {
                // Извлекаем номер из маркера
                var markerNumber = extractNumberFromId(targetId);
                if (markerNumber !== null) {
                    footnoteMarkerNumbers.push(markerNumber);
                }
                
                // Проверяем, ведет ли ссылка на сноску
                if (elementsWithId[targetId] && elementsWithId[targetId].isFootnote) {
                    // Ссылка ведет на сноску - это маркер сноски
                    counters.footnoteMarkers.total++;
                    counters.footnoteMarkers[sectionType]++;
                    isFootnoteMarker = true;
                } else if (!elementsWithId[targetId]) {
                    // Ссылка имеет формат #n_, но элемент с таким ID не найден
                    counters.brokenFootnoteMarkers.total++;
                    counters.brokenFootnoteMarkers[sectionType]++;
                    isBrokenFootnoteMarker = true;
                }
            }
            
            // Проверяем на маркеры комментариев (#c_X)
            var isCommentMarker = false;
            var isBrokenCommentMarker = false;
            
            if (targetId.length > 0 && startsWithStr(targetId, "c_")) {
                // Извлекаем номер из маркера
                var markerNumber = extractNumberFromId(targetId);
                if (markerNumber !== null) {
                    commentMarkerNumbers.push(markerNumber);
                }
                
                // Проверяем, ведет ли ссылка на комментарий
                if (elementsWithId[targetId] && elementsWithId[targetId].isComment) {
                    counters.commentMarkers.total++;
                    counters.commentMarkers[sectionType]++;
                    isCommentMarker = true;
                } else if (!elementsWithId[targetId]) {
                    counters.brokenCommentMarkers.total++;
                    counters.brokenCommentMarkers[sectionType]++;
                    isBrokenCommentMarker = true;
                }
            }
            
            // Если это маркер сноски или комментария, пропускаем дальнейший анализ
            if (isFootnoteMarker || isCommentMarker || isBrokenFootnoteMarker || isBrokenCommentMarker) {
                continue;
            }
            
            // Проверяем на навигационные ссылки FBE
            var isNavLink = false;
            
            if (targetId.length > 0 && startsWithStr(targetId, "n_") && 
                (startsWithStr(hrefValue, "#") || 
                 (containsStr(hrefLower, "file:///") && containsStr(hrefLower, "main.html#")))) {
                
                if (elementsWithId[targetId] && !elementsWithId[targetId].isFootnote) {
                    isNavLink = true;
                }
            }
            
            if (isNavLink) {
                counters.navLinks.total++;
                counters.navLinks[sectionType]++;
                
                potentialNavLinks.push({
                    link: link,
                    targetId: targetId,
                    sectionType: sectionType,
                    href: hrefValue
                });
                continue;
            }
            
            // Определяем тип представления ссылки
            var isPureLink = false;
            var isHiddenLink = false;
            
            var linkTextLower = linkText.toLowerCase();
            if (linkTextLower === hrefLower || 
                startsWithStr(linkTextLower, "www.") ||
                startsWithStr(linkTextLower, "http://") ||
                startsWithStr(linkTextLower, "https://") ||
                isDomainInText(linkText)) {
                isPureLink = true;
            } else if (linkText.length > 0 && linkTextLower !== hrefLower) {
                isHiddenLink = true;
            }
            
            if (isPureLink) {
                counters.pureLinks.total++;
                counters.pureLinks[sectionType]++;
            } else if (isHiddenLink) {
                counters.hiddenLinks.total++;
                counters.hiddenLinks[sectionType]++;
            }
            
            // Анализ типа ссылки
            if (startsWithStr(hrefLower, "http://")) {
                if (containsStr(hrefLower, "www.")) {
                    counters.www.total++;
                    counters.www[sectionType]++;
                } else {
                    counters.http.total++;
                    counters.http[sectionType]++;
                }
            } 
            else if (startsWithStr(hrefLower, "https://")) {
                if (containsStr(hrefLower, "www.")) {
                    counters.www.total++;
                    counters.www[sectionType]++;
                } else {
                    counters.https.total++;
                    counters.https[sectionType]++;
                }
            }
            else if (startsWithStr(hrefLower, "mailto:")) {
                counters.mailto.total++;
                counters.mailto[sectionType]++;
            }
            else if (startsWithStr(hrefLower, "ftp://") || startsWithStr(hrefLower, "ftps://")) {
                counters.ftp.total++;
                counters.ftp[sectionType]++;
            }
            else if (startsWithStr(hrefLower, "file://")) {
                if (!isNavLink) {
                    counters.file.total++;
                    counters.file[sectionType]++;
                }
            }
            else if (startsWithStr(hrefLower, "www.")) {
                counters.www.total++;
                counters.www[sectionType]++;
            }
            else if (containsStr(hrefValue, "@") && !containsStr(hrefLower, "mailto:")) {
                counters.mailto.total++;
                counters.mailto[sectionType]++;
            }
            else if (isDomainInText(hrefValue)) {
                counters.domain.total++;
                counters.domain[sectionType]++;
            }
            else {
                counters.textLinks.total++;
                counters.textLinks[sectionType]++;
            }
        }
        
        // ================================
        // ФАЗА 3: Подсчет пар навигационных ссылок
        // ================================
        
        var foundPairs = {};
        
        for (var i = 0; i < potentialNavLinks.length; i++) {
            var navLink = potentialNavLinks[i];
            var targetId = navLink.targetId;
            
            if (elementsWithId[targetId] && !elementsWithId[targetId].isFootnote) {
                var targetElement = elementsWithId[targetId];
                
                if (targetElement.tagName === "p" || 
                    targetElement.tagName === "div" || 
                    targetElement.tagName === "span" ||
                    targetElement.tagName === "a") {
                    
                    if (!foundPairs[targetId]) {
                        foundPairs[targetId] = true;
                        counters.navLinks.pairs++;
                    }
                }
            }
        }
        
        // ================================
        // ФАЗА 4: Поиск доменов в тексте
        // ================================
        
        function findAllTextNodes(element) {
            if (!element) return;
            
            for (var i = 0; i < element.childNodes.length; i++) {
                var child = element.childNodes[i];
                
                if (child.nodeType === 3) { // TEXT_NODE
                    var text = child.nodeValue || "";
                    var trimmedText = trimStr(text);
                    if (trimmedText.length > 0) {
                        var parentElement = child.parentNode;
                        var sectionType = "main";
                        
                        if (parentElement) {
                            sectionType = getSectionType(parentElement);
                        }
                        
                        var domains = extractDomainsFromText(text);
                        if (domains.length > 0) {
                            counters.textDomains.total += domains.length;
                            counters.textDomains[sectionType] += domains.length;
                        }
                    }
                } else if (child.nodeType === 1) { // ELEMENT_NODE
                    if (child.tagName && child.tagName.toLowerCase() !== "a") {
                        findAllTextNodes(child);
                    }
                }
            }
        }
        
        findAllTextNodes(document.body);
        
        // ================================
        // ФАЗА 5: Поиск разделов сносок и комментариев
        // ================================
        
        var allBodies = document.getElementsByTagName("body");
        
        for (var i = 0; i < allBodies.length; i++) {
            var body = allBodies[i];
            var bodyName = body.getAttribute("name") || "";
            bodyName = trimStr(bodyName.toLowerCase());
            
            if (bodyName === "notes") {
                var sections = body.getElementsByTagName("section");
                
                for (var j = 0; j < sections.length; j++) {
                    var section = sections[j];
                    var sectionId = section.getAttribute("id") || "";
                    sectionId = trimStr(sectionId);
                    
                    if (startsWithStr(sectionId, "n_") || startsWithStr(sectionId, "_")) {
                        counters.footnotes++;
                        
                        // Извлекаем номер из ID сноски
                        var footnoteNumber = extractNumberFromId(sectionId);
                        if (footnoteNumber !== null) {
                            footnoteNumbers.push(footnoteNumber);
                        }
                    }
                }
                
                if (counters.footnotes === 0) {
                    var allElements = body.getElementsByTagName("*");
                    for (var j = 0; j < allElements.length; j++) {
                        var elem = allElements[j];
                        var elemId = elem.getAttribute("id") || "";
                        elemId = trimStr(elemId);
                        
                        if (startsWithStr(elemId, "n_") || startsWithStr(elemId, "_")) {
                            counters.footnotes++;
                            
                            var footnoteNumber = extractNumberFromId(elemId);
                            if (footnoteNumber !== null) {
                                footnoteNumbers.push(footnoteNumber);
                            }
                        }
                    }
                }
            }
            else if (bodyName === "comments") {
                var sections = body.getElementsByTagName("section");
                
                for (var j = 0; j < sections.length; j++) {
                    var section = sections[j];
                    var sectionId = section.getAttribute("id") || "";
                    sectionId = trimStr(sectionId);
                    
                    if (startsWithStr(sectionId, "c_")) {
                        counters.comments++;
                        
                        // Извлекаем номер из ID комментария
                        var commentNumber = extractNumberFromId(sectionId);
                        if (commentNumber !== null) {
                            commentNumbers.push(commentNumber);
                        }
                    }
                }
                
                if (counters.comments === 0) {
                    var allElements = body.getElementsByTagName("*");
                    for (var j = 0; j < allElements.length; j++) {
                        var elem = allElements[j];
                        var elemId = elem.getAttribute("id") || "";
                        elemId = trimStr(elemId);
                        
                        if (startsWithStr(elemId, "c_")) {
                            counters.comments++;
                            
                            var commentNumber = extractNumberFromId(elemId);
                            if (commentNumber !== null) {
                                commentNumbers.push(commentNumber);
                            }
                        }
                    }
                }
            }
        }
        
        // Альтернативный поиск через DIV с fbname
        if (counters.footnotes === 0 && counters.comments === 0) {
            var allDivs = document.getElementsByTagName("div");
            
            for (var i = 0; i < allDivs.length; i++) {
                var div = allDivs[i];
                var fbname = div.getAttribute("fbname") || "";
                fbname = trimStr(fbname.toLowerCase());
                
                if (fbname === "notes") {
                    var allElements = div.getElementsByTagName("*");
                    for (var j = 0; j < allElements.length; j++) {
                        var elem = allElements[j];
                        var elemId = elem.getAttribute("id") || "";
                        elemId = trimStr(elemId);
                        
                        if (startsWithStr(elemId, "n_") || startsWithStr(elemId, "_")) {
                            counters.footnotes++;
                            
                            var footnoteNumber = extractNumberFromId(elemId);
                            if (footnoteNumber !== null) {
                                footnoteNumbers.push(footnoteNumber);
                            }
                        }
                    }
                }
                else if (fbname === "comments") {
                    var allElements = div.getElementsByTagName("*");
                    for (var j = 0; j < allElements.length; j++) {
                        var elem = allElements[j];
                        var elemId = elem.getAttribute("id") || "";
                        elemId = trimStr(elemId);
                        
                        if (startsWithStr(elemId, "c_")) {
                            counters.comments++;
                            
                            var commentNumber = extractNumberFromId(elemId);
                            if (commentNumber !== null) {
                                commentNumbers.push(commentNumber);
                            }
                        }
                    }
                }
            }
        }
        
        // ================================
        // ФАЗА 6: Анализ несоответствий
        // ================================
        
        // Находим пропущенные номера
        var missingFootnoteMarkers = [];  // Номера, которые есть в сносках, но нет в маркерах
        var missingFootnotes = [];        // Номера, которые есть в маркерах, но нет в сносках
        var missingCommentMarkers = [];   // Номера, которые есть в комментариях, но нет в маркерах
        var missingComments = [];         // Номера, которые есть в маркерах, но нет в комментариях
        
        // Для сносок: находим номера, которые есть в сносках, но отсутствуют в маркерах
        for (var i = 0; i < footnoteNumbers.length; i++) {
            var found = false;
            for (var j = 0; j < footnoteMarkerNumbers.length; j++) {
                if (footnoteMarkerNumbers[j] === footnoteNumbers[i]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                missingFootnoteMarkers.push(footnoteNumbers[i]);
            }
        }
        
        // Для сносок: находим номера, которые есть в маркерах, но отсутствуют в сносках
        for (var i = 0; i < footnoteMarkerNumbers.length; i++) {
            var found = false;
            for (var j = 0; j < footnoteNumbers.length; j++) {
                if (footnoteNumbers[j] === footnoteMarkerNumbers[i]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                missingFootnotes.push(footnoteMarkerNumbers[i]);
            }
        }
        
        // Для комментариев: находим номера, которые есть в комментариях, но отсутствуют в маркерах
        for (var i = 0; i < commentNumbers.length; i++) {
            var found = false;
            for (var j = 0; j < commentMarkerNumbers.length; j++) {
                if (commentMarkerNumbers[j] === commentNumbers[i]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                missingCommentMarkers.push(commentNumbers[i]);
            }
        }
        
        // Для комментариев: находим номера, которые есть в маркерах, но отсутствуют в комментариях
        for (var i = 0; i < commentMarkerNumbers.length; i++) {
            var found = false;
            for (var j = 0; j < commentNumbers.length; j++) {
                if (commentNumbers[j] === commentMarkerNumbers[i]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                missingComments.push(commentMarkerNumbers[i]);
            }
        }
        
        // Сортируем массивы пропущенных номеров (пузырьковая сортировка)
        function sortNumbers(arr) {
            for (var i = 0; i < arr.length - 1; i++) {
                for (var j = 0; j < arr.length - i - 1; j++) {
                    if (arr[j] > arr[j + 1]) {
                        var temp = arr[j];
                        arr[j] = arr[j + 1];
                        arr[j + 1] = temp;
                    }
                }
            }
            return arr;
        }
        
        missingFootnoteMarkers = sortNumbers(missingFootnoteMarkers);
        missingFootnotes = sortNumbers(missingFootnotes);
        missingCommentMarkers = sortNumbers(missingCommentMarkers);
        missingComments = sortNumbers(missingComments);
        
        // ================================
        // ФАЗА 7: Вычисление общих сумм
        // ================================
        
        // ВСЕ ссылки
        var allLinksTotal = counters.www.total + counters.http.total + counters.https.total + 
                           counters.domain.total + counters.mailto.total + counters.ftp.total + 
                           counters.file.total + counters.textLinks.total + 
                           counters.footnoteMarkers.total + counters.commentMarkers.total +
                           counters.brokenFootnoteMarkers.total + counters.brokenCommentMarkers.total +
                           counters.textDomains.total + counters.navLinks.total;
        
        // Только ВНЕШНИЕ ссылки
        counters.externalLinks = counters.www.total + counters.http.total + counters.https.total + 
                                counters.domain.total + counters.mailto.total + counters.ftp.total + 
                                counters.file.total + counters.textLinks.total + counters.textDomains.total;
        
        counters.internalElements = counters.footnotes + counters.comments;
        
        // ================================
        // ФАЗА 8: Проверка, есть ли что-то вообще
        // ================================
        
        var hasAnyData = false;
        
        if (counters.totalLinks > 0) hasAnyData = true;
        if (counters.footnotes > 0) hasAnyData = true;
        if (counters.comments > 0) hasAnyData = true;
        if (counters.textDomains.total > 0) hasAnyData = true;
        
        var allLinkTypes = [counters.www, counters.http, counters.https, counters.domain, 
                           counters.mailto, counters.ftp, counters.file, counters.textLinks,
                           counters.navLinks, counters.footnoteMarkers, counters.commentMarkers,
                           counters.brokenFootnoteMarkers, counters.brokenCommentMarkers];
        
        for (var i = 0; i < allLinkTypes.length; i++) {
            if (allLinkTypes[i].total > 0) {
                hasAnyData = true;
                break;
            }
        }
        
        if (!hasAnyData) {
            var endTime = new Date().getTime();
            var executionTime = (endTime - startTime) / 1000;
            
            var message = "==============================\n";
            message += scriptName + "\n";
            message += "ver. " + scriptVersion + "\n";
            message += "==============================\n\n";
            message += "В документе не найдено ссылок, разделов примечаний и комментариев.\n\n";
            message += "Время выполнения: " + executionTime.toFixed(2) + " сек";
            
            MsgBox(message, "FBE скрипт");
            return;
        }
        
        // ================================
        // ФАЗА 9: Формирование отчета
        // ================================
        
        var message = "==============================\n";
        message += scriptName + "\n";
        message += "ver. " + scriptVersion + "\n";
        message += "==============================\n\n";
        
        // ОБЩАЯ СТАТИСТИКА ССЫЛОК
        if (SHOW_FULL_STATS === 1 || allLinksTotal > 0) {
            message += "ССЫЛОК ВСЕГО: " + allLinksTotal + "\n";
            message += "==============================\n";
            
            var hasLinkTypes = false;
            
            if (counters.pureLinks.total > 0 || SHOW_FULL_STATS === 1) {
                message += "• Чистых ссылок (URL как текст): " + counters.pureLinks.total + "\n";
                hasLinkTypes = true;
            }
            
            if (counters.textDomains.total > 0 || SHOW_FULL_STATS === 1) {
                message += "• Доменов в тексте (без тегов <a>): " + counters.textDomains.total + "\n";
                hasLinkTypes = true;
            }
            
            if (counters.navLinks.total > 0 || SHOW_FULL_STATS === 1) {
                if (counters.navLinks.pairs > 0) {
                    message += "• Навигационных ссылок FBE: " + counters.navLinks.total + " (" + counters.navLinks.pairs + " " + pluralize(counters.navLinks.pairs, "пара", "пары", "пар") + ")\n";
                } else {
                    message += "• Навигационных ссылок FBE: " + counters.navLinks.total + "\n";
                }
                hasLinkTypes = true;
            }
            
            if (counters.hiddenLinks.total > 0 || SHOW_FULL_STATS === 1) {
                message += "• Скрытых ссылок (текст отличается от URL): " + counters.hiddenLinks.total + "\n";
                hasLinkTypes = true;
            }
            
            if (counters.footnoteMarkers.total > 0 || SHOW_FULL_STATS === 1) {
                message += "• Маркеров сносок: " + counters.footnoteMarkers.total + "\n";
                hasLinkTypes = true;
            }
            
            if (counters.commentMarkers.total > 0 || SHOW_FULL_STATS === 1) {
                message += "• Маркеров комментариев: " + counters.commentMarkers.total + "\n";
                hasLinkTypes = true;
            }
            
            if (counters.brokenFootnoteMarkers.total > 0 || SHOW_FULL_STATS === 1) {
                message += "• Битых маркеров сносок: " + counters.brokenFootnoteMarkers.total + "\n";
                hasLinkTypes = true;
            }
            
            if (counters.brokenCommentMarkers.total > 0 || SHOW_FULL_STATS === 1) {
                message += "• Битых маркеров комментариев: " + counters.brokenCommentMarkers.total + "\n";
                hasLinkTypes = true;
            }
            
            if (hasLinkTypes) {
                message += "\n";
            } else {
                message = message.substring(0, message.length - 31);
            }
        }
        
        // ДЕТАЛИЗАЦИЯ ПО ТИПАМ ССЫЛОК
        var hasDetailedData = false;
        
        var detailedTypes = [
            // Обычные ссылки
            {name: "WWW ссылок", counter: counters.www},
            {name: "HTTP ссылок (без www)", counter: counters.http},
            {name: "HTTPS ссылок (без www)", counter: counters.https},
            {name: "E-mail ссылок", counter: counters.mailto},
            {name: "Доменов в тексте (без тегов <a>)", counter: counters.textDomains},
            {name: "Навигационных ссылок FBE", counter: counters.navLinks},
            {name: "Файл-ссылок", counter: counters.file},
            {name: "Доменных ссылок в тегах <a>", counter: counters.domain},
            {name: "FTP ссылок", counter: counters.ftp},
            {name: "Текстовых ссылок", counter: counters.textLinks},
            
            // Битые маркеры
            {name: "Битых маркеров сносок", counter: counters.brokenFootnoteMarkers},
            {name: "Битых маркеров комментариев", counter: counters.brokenCommentMarkers},
            
            // Рабочие маркеры
            {name: "Маркеров сносок", counter: counters.footnoteMarkers},
            {name: "Маркеров комментариев", counter: counters.commentMarkers}
        ];
        
        for (var i = 0; i < detailedTypes.length; i++) {
            if (detailedTypes[i].counter.total > 0 || SHOW_FULL_STATS === 1) {
                hasDetailedData = true;
                break;
            }
        }
        
        if (hasDetailedData) {
            message += "ДЕТАЛИЗАЦИЯ ПО ТИПАМ ССЫЛОК:\n";
            message += "==============================\n";
            
            var previousHadContent = false;
            
            for (var i = 0; i < detailedTypes.length; i++) {
                var typeName = detailedTypes[i].name;
                var counter = detailedTypes[i].counter;
                
                if (counter.total > 0 || SHOW_FULL_STATS === 1) {
                    var compactLocation = getCompactLocation(counter);
                    
                    if (compactLocation !== "") {
                        message += typeName + " всего: " + counter.total + compactLocation + "\n";
                        previousHadContent = true;
                    } else if (counter.total > 0) {
                        if (previousHadContent) {
                            message += "\n";
                        }
                        message += typeName + " всего: " + counter.total + "\n";
                        
                        var hasDetails = false;
                        var details = "";
                        
                        if (counter.annotation > 0 || SHOW_FULL_STATS === 1) {
                            if (!hasDetails) {
                                details = "Из них:\n";
                                hasDetails = true;
                            }
                            details += "• В аннотации: " + counter.annotation + "\n";
                        }
                        if (counter.history > 0 || SHOW_FULL_STATS === 1) {
                            if (!hasDetails) {
                                details = "Из них:\n";
                                hasDetails = true;
                            }
                            details += "• В хистори: " + counter.history + "\n";
                        }
                        if (counter.main > 0 || SHOW_FULL_STATS === 1) {
                            if (!hasDetails) {
                                details = "Из них:\n";
                                hasDetails = true;
                            }
                            details += "• В основном разделе: " + counter.main + "\n";
                        }
                        if (counter.notes > 0 || SHOW_FULL_STATS === 1) {
                            if (!hasDetails) {
                                details = "Из них:\n";
                                hasDetails = true;
                            }
                            details += "• В разделе сносок: " + counter.notes + "\n";
                        }
                        if (counter.comments > 0 || SHOW_FULL_STATS === 1) {
                            if (!hasDetails) {
                                details = "Из них:\n";
                                hasDetails = true;
                            }
                            details += "• В разделе комментариев: " + counter.comments + "\n";
                        }
                        
                        if (hasDetails) {
                            message += details;
                        }
                        previousHadContent = true;
                    }
                }
            }
            
            if (previousHadContent) {
                message += "\n";
            }
        }
        
        // СТАТИСТИКА ВНУТРЕННИХ ЭЛЕМЕНТОВ (с детализацией пропущенных номеров)
        var hasInternalElements = counters.footnotes > 0 || counters.comments > 0 || SHOW_FULL_STATS === 1;
        
        if (hasInternalElements) {
            if (hasDetailedData && (counters.footnotes > 0 || counters.comments > 0 || SHOW_FULL_STATS === 1)) {
                message += "СТАТИСТИКА ВНУТРЕННИХ ЭЛЕМЕНТОВ:\n";
                message += "==============================\n";
                
                var footnotesText = pluralize(counters.footnotes, "сноска", "сноски", "сносок");
                var commentsText = pluralize(counters.comments, "комментарий", "комментария", "комментариев");
                
                // Общее количество маркеров сносок (рабочие + битые)
                var totalFootnoteMarkers = counters.footnoteMarkers.total + counters.brokenFootnoteMarkers.total;
                
                // Сноски
                if (totalFootnoteMarkers > 0 || SHOW_FULL_STATS === 1) {
                    var footnoteMarkerText = "Маркеров сносок: " + totalFootnoteMarkers;
                    if (counters.brokenFootnoteMarkers.total > 0) {
                        footnoteMarkerText += " (из них битых: " + counters.brokenFootnoteMarkers.total + ")";
                    }
                    message += footnoteMarkerText + "\n";
                }
                if (counters.footnotes > 0 || SHOW_FULL_STATS === 1) {
                    message += "Сносок-примечаний: " + counters.footnotes + " " + footnotesText + "\n";
                }
                
                // Детализация пропущенных номеров для сносок
                var hasFootnoteDetails = false;
                var footnoteDetails = "";
                
                if (missingFootnotes.length > 0) {
                    footnoteDetails += "Отсутствуют сноски: " + formatMissingNumbers(missingFootnotes) + "\n";
                    hasFootnoteDetails = true;
                }
                if (missingFootnoteMarkers.length > 0) {
                    footnoteDetails += "Отсутствуют маркеры сносок: " + formatMissingNumbers(missingFootnoteMarkers) + "\n";
                    hasFootnoteDetails = true;
                }
                
                if (hasFootnoteDetails) {
                    message += footnoteDetails;
                }
                
                // Общее количество маркеров комментариев (рабочие + битые)
                var totalCommentMarkers = counters.commentMarkers.total + counters.brokenCommentMarkers.total;
                
                // Комментарии
                if (totalCommentMarkers > 0 || SHOW_FULL_STATS === 1) {
                    // Добавляем пустую строку перед комментариями, только если были сноски
                    if ((totalFootnoteMarkers > 0 || counters.footnotes > 0) && 
                        (totalCommentMarkers > 0 || counters.comments > 0)) {
                        message += "\n";
                    }
                    var commentMarkerText = "Маркеров комментариев: " + totalCommentMarkers;
                    if (counters.brokenCommentMarkers.total > 0) {
                        commentMarkerText += " (из них битых: " + counters.brokenCommentMarkers.total + ")";
                    }
                    message += commentMarkerText + "\n";
                }
                if (counters.comments > 0 || SHOW_FULL_STATS === 1) {
                    if (totalCommentMarkers === 0 && counters.comments > 0) {
                        message += "Комментариев: " + counters.comments + " " + commentsText + "\n";
                    } else {
                        message += "Комментариев: " + counters.comments + " " + commentsText + "\n";
                    }
                }
                
                // Детализация пропущенных номеров для комментариев
                var hasCommentDetails = false;
                var commentDetails = "";
                
                if (missingComments.length > 0) {
                    commentDetails += "Отсутствуют комментарии: " + formatMissingNumbers(missingComments) + "\n";
                    hasCommentDetails = true;
                }
                if (missingCommentMarkers.length > 0) {
                    commentDetails += "Отсутствуют маркеры комментариев: " + formatMissingNumbers(missingCommentMarkers) + "\n";
                    hasCommentDetails = true;
                }
                
                if (hasCommentDetails) {
                    message += commentDetails;
                }
                
                message += "\n";
            }
        }
        
        // ИТОГОВАЯ СТАТИСТИКА
        message += "ИТОГОВАЯ СТАТИСТИКА:\n";
        message += "==============================\n";
        
        message += "Всего ссылок: " + allLinksTotal + "\n";
        message += "Всего тегов <a> в документе: " + counters.totalLinks + "\n";
        message += "Всего внешних ссылок: " + counters.externalLinks + "\n";
        
        if (counters.footnotes > 0) {
            message += "Всего сносок: " + counters.footnotes + "\n";
        } else if (SHOW_FULL_STATS === 1) {
            message += "Раздел сносок: отсутствует\n";
        }
        
        if (counters.comments > 0) {
            message += "Всего комментариев: " + counters.comments + "\n";
        } else if (SHOW_FULL_STATS === 1) {
            message += "Раздел комментариев: отсутствует\n";
        }
        
        var totalProcessed = allLinksTotal + counters.footnotes + counters.comments;
        message += "Всего обработано элементов: " + totalProcessed + "\n\n";
        
        // Время выполнения
        var endTime = new Date().getTime();
        var executionTime = (endTime - startTime) / 1000;
        
        message += "Время выполнения: " + executionTime.toFixed(2) + " сек";
        
        MsgBox(message, "FBE скрипт");
        
    } catch (e) {
        MsgBox("Ошибка в скрипте: " + e.message, "FBE скрипт");
    }
}
