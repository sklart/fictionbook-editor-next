// Скрипт «Просмотр готовых маркеров сносок (примечаний и комментариев)...» для редактора Fiction Book Editor (FBE).
// Последовательная навигация (вперед/назад) в тексте книги с ПОДСВЕТКОЙ готовых маркеров сносок
// для визуального контроля их нумерации и положения, начиная с текущей позиции курсора.
// Интерактивный выбор режима просмотра: 
// – Смешанный (без разделения на notes и comments), либо Примечания или Комментарии, либо Отмена.
// – Выбор: Примечания либо Комментарии.
// Обрабатываются примечания class="note" и комментарии с типом ссылки "#c_"
// Папка скриптов: 32_Маркеры сносок
// version 1.2
// Идея - Lancer
// Реализация - Gemini, Lancer

function Run() {
    // В FBE объект document уже доступен глобально в контексте окна
	if (!document) {
        alert("Документ не найден.");
        return;
    }

    // Переменные описания скрипта для вывода в диалоговых окнах (version 1.2)
    var SCRIPT_NAME = "Просмотр готовых маркеров сносок (примечаний и комментариев)";
    var SCRIPT_VERSION = "version 1.2";

    // Настройка цвета ПОДСВЕТКИ (описания оценочно-приблизительные)(раскомментируйте нужную строку):
    // var HIGHLIGHT_COLOR = "lightgreen";	 	// зеленый
	// var HIGHLIGHT_COLOR = "lime";		 	// кислотно-зеленый (очень яркий)
    // var HIGHLIGHT_COLOR = "limegreen"; 	 	// насыщенный зеленый (ярче обычного)
    // var HIGHLIGHT_COLOR = "cyan"; 		 	// голубой
	// var HIGHLIGHT_COLOR = "cadetblue"; 	 	// сине-голубой
	// var HIGHLIGHT_COLOR = "blue"; 		 	// глубокий синий (символы "тонут", т.к. сами синие)
	// var HIGHLIGHT_COLOR = "Highlight"; 	 	// системный Windows (бледный)
    // var HIGHLIGHT_COLOR = "ActiveCaption"; 	// системный сине-голубой (пастельный, бледный)
	// var HIGHLIGHT_COLOR = "red"; 			// красный
	// var HIGHLIGHT_COLOR = "crimson"; 		// благородный малиново-красный/тёмно-пурпурный (мягче обычного)
    // var HIGHLIGHT_COLOR = "yellow"; 			// желтый
    // var HIGHLIGHT_COLOR = "orange"; 			// жёлто-оранжевый
	var HIGHLIGHT_COLOR = "darkorange"; 		// насыщенный оранжевый (не кислотный)
    
    var WshShell = new ActiveXObject("WScript.Shell");

    // Шаг 0. Выбор режима проверки через стартовые диалоговые окна (изменено в version 1.2)
    var isMixMode = false;
    var isCommentMode = false;
    var windowTitle = "Выбор объекта проверки";

    // Первый бокс: выбор между смешанным и раздельным режимами
    // 3 (Да/Нет/Отмена), 32 (Иконка знака вопроса)
    var firstBoxMsg = SCRIPT_NAME + "\n" + 
                      SCRIPT_VERSION + "\n\n\n" + 
                      "• Выбор цвета подсветки маркера сноски в коде скрипта.\n" + 
					  "• Старт скрипта – с текущей позиции курсора.\n\n\n" + 
                      "[Да]\t • Смешанный режим\n" + 
                      "[Нет]\t • Примечания или Комментарии\n" + 
                      "[Отмена]\t • Выход";
                      
    var firstResponse = WshShell.Popup(firstBoxMsg, 0, windowTitle, 3 + 32);

    if (firstResponse == 6) { // Кнопка «Да» -> Смешанный режим
        isMixMode = true;
    } else if (firstResponse == 7) { // Кнопка «Нет» -> Переходим ко второму боксу
        isMixMode = false;
        
        // Второй бокс: выбор конкретного объекта (кнопка Отмена исключена флагом 4)
        // 4 (Да/Нет), 32 (Иконка знака вопроса)
        var secondBoxMsg = "   [Да]\t • Примечания\n\n   [Нет]\t • Комментарии";
        var secondResponse = WshShell.Popup(secondBoxMsg, 0, windowTitle, 4 + 32);
        
        if (secondResponse == 6) {
            isCommentMode = false;
        } else if (secondResponse == 7) {
            isCommentMode = true;
        }
    } else { // Кнопка «Отмена» или Esc или закрытие крестиком на первом боксе
        alert("Просмотр отменён.");
        return;
    }
        
	// Шаг 1. Собираем все нужные ссылки внутри fbw_body (основной текст)
    var fbwBody = document.getElementById("fbw_body");
    if (!fbwBody) {
        alert("Поиск работает только в режиме текста (кнопка B).");
        return;
    }

    var allLinks = fbwBody.getElementsByTagName("a");
    var noteLinks = [];
    
    // Сбор маркеров в зависимости от выбранного режима (изменено в version 1.2)
    if (isMixMode) {
        // Смешанный режим: собираем ВСЁ подряд по ходу текста книги
        for (var i = 0; i < allLinks.length; i++) {
            var hrefAttr = allLinks[i].getAttribute("href") || "";
            if (allLinks[i].className === "note" || hrefAttr.indexOf("#c_") !== -1) {
                noteLinks.push(allLinks[i]);
            }
        }
        
        if (noteLinks.length === 0) {
            alert("Маркеры примечаний и комментариев в тексте отсутствуют.");
            return;
        }
    } else {
        // Раздельный режим (оригинальная логика из version 1.0)
        if (!isCommentMode) {
            // Фильтруем по шаблону тега примечаний: class="note"
            for (var i = 0; i < allLinks.length; i++) {
                if (allLinks[i].className === "note") {
                    noteLinks.push(allLinks[i]);
                }
            }

            if (noteLinks.length === 0) {
                alert("Маркеры примечаний с классом \"note\" отсутствуют.\nВыполните унификацию примечаний скриптами FBE\nи повторите проверку.");
                return;
            }
        } else {
            // Фильтруем по шаблону тега для комментариев: наличие префикса "#c_" в href.
            for (var i = 0; i < allLinks.length; i++) {
                var hrefAttr = allLinks[i].getAttribute("href") || "";
                if (hrefAttr.indexOf("#c_") !== -1) {
                    noteLinks.push(allLinks[i]);
                }
            }

            if (noteLinks.length === 0) {
                alert("Маркеры комментариев со ссылкой типа '#c_' отсутствуют.\nВыполните унификацию комментариев скриптами FBE\nи повторите проверку.");
                return;
            }
        }
    }

    // Шаг 2. Определяем текущую позицию курсора, чтобы начать поиск от нее
	var selection = document.selection;
    var range = selection.createRange();
    var currentIndex = 0;

	// Ищем ближайший маркер, который находится ниже курсора
    for (var i = 0; i < noteLinks.length; i++) {
        var nodeRange = document.body.createTextRange();
        nodeRange.moveToElementText(noteLinks[i]);
        
		// Сравниваем начальные точки текущего выделения и маркера
		if (range.compareEndPoints("StartToStart", nodeRange) <= 0) {
            currentIndex = i;
            break;
        }
        if (i === noteLinks.length - 1) {
            currentIndex = 0;  // Если курсор ниже всех элементов, зацикливаем на первый
        }
    }

    // Шаг 3. Цикл интерактивной навигации
    var lastHighlighted = null;
    var originalColor = "";

    while (true) {
        var currentLink = noteLinks[currentIndex];

        // Восстанавливаем цвет предыдущего проверенного маркера
		if (lastHighlighted) {
            lastHighlighted.style.backgroundColor = originalColor;
        }

        // Запоминаем исходный цвет текущего и красим в выбранный пользователем
		originalColor = currentLink.style.backgroundColor;
        currentLink.style.backgroundColor = HIGHLIGHT_COLOR;
        lastHighlighted = currentLink;

        // Центрируем экран на маркере и выделяем его
		currentLink.scrollIntoView(true);
		
        var resRange = document.body.createTextRange();
        resRange.moveToElementText(currentLink);
        resRange.select();

        // Принудительный возврат фокуса в HTML-документ после переключения в другие окна Windows
        try {
            currentLink.focus();
        } catch(e) {
            try { document.body.focus(); } catch(err) {}
        }
				
        // На лету определяем тип текущего маркера для корректного отображения (добавлено в version 1.2)
        var hrefValue = currentLink.getAttribute("href") || "";
        var isCurrentComment = (hrefValue.indexOf("#c_") !== -1);
        var currentTypeStr = isCurrentComment ? "комментария " : "примечания ";

		// Вывод информации в статус-бар FBE (как в скриптах Sclex)
		try {
            var statusBarTitle = isMixMode ? "Смешанный просмотр: " : "Просмотр ";
            window.external.SetStatusBarText(statusBarTitle + currentTypeStr + (currentIndex + 1) + " из " + noteLinks.length);
        } catch(e) {}

		// Диалоговое окно управления: 3 (Да/Нет/Отмена), без иконок, чтобы убрать звук
        // Возвращает: Да = 6, Нет = 7, Отмена = 2
        // Получаем только хвостовую часть ссылки (например, #n_12 или #c_5)
        var shortHref = hrefValue.substring(hrefValue.indexOf("#"));

        var msgType = isCurrentComment ? "Найден маркер комментария  –  " : "Найден маркер примечания  –  ";
        var msg = msgType + (currentIndex + 1) + "  из  " + noteLinks.length + "  маркеров\n\n" +
                  "Маркер: " + currentLink.innerText + "\n" +
                  "Ссылка: " + shortHref + "\n\n" +
                  "[Да]\t • Вперед\n" +
                  "[Нет]\t • Назад\n" +
                  "[Отмена]\t • Завершить просмотр";
        
        // Установка заголовка окна навигации в зависимости от режима (изменено в version 1.2)
        var currentWindowTitle = "";
        if (isMixMode) {
            currentWindowTitle = "Смешанный просмотр маркеров сносок";
        } else {
            currentWindowTitle = isCommentMode ? "Просмотр готовых маркеров комментариев" : "Просмотр готовых маркеров примечаний";
        }

        // Передаем флаг 3 (Да/Нет/Отмена) в тихом режиме без системных звуков
        var response = WshShell.Popup(msg, 0, currentWindowTitle, 3);

        if (response == 6) { // Кнопка «Да» -> Вперед
            currentIndex++;
            if (currentIndex >= noteLinks.length) currentIndex = 0; 
        } else if (response == 7) { // Кнопка «Нет» -> Назад
            currentIndex--;
            if (currentIndex < 0) currentIndex = noteLinks.length - 1; 
        } else { // Кнопка «Отмена» или Esc или закрытие крестиком
            
			// Убираем временную подсветку, возвращая родной стиль HTML
			currentLink.style.backgroundColor = originalColor;
            
            // Уведомление на выходе в зависимости от режима (изменено в version 1.2)
			var exitMessage = "Просмотр завершен.";
            if (isMixMode) {
                exitMessage = "Смешанный просмотр маркеров сносок завершен.";
            } else {
                exitMessage = isCommentMode ? "Просмотр маркеров комментариев завершен." : "Просмотр маркеров примечаний завершен.";
            }

            try {
                window.external.SetStatusBarText(exitMessage);
            } catch(e) {}
            
            // Надежное уведомление через alert с кнопкой ОК
            alert(exitMessage);
            break; // Курсор остается стоять на текущем маркере
        }
    }
}
