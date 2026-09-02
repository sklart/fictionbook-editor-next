//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//             Сверка «дизайна»
//  Скрипт тестировался в FBE v.2.8.5 (win XP, IE8 и win 7, IE11)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

function Run() {

 var ScriptName="Сверка «Дизайна»";
 var NumerusVersion="1.0";
 var Ts = new Date().getTime();


///////////////   ДОПОЛНИТЕЛЬНЫЕ ПАРАМЕТРЫ

//     Размер детализации при поиске базовых отличий.
 var mainMinSim = 1;      // "1" ; "2" ; "3" ; "4"  и т.д. //
//   ("1" — Стандарт,  "2", "3" и т.д. — В очень сложных случаях убирает "кашу", но снижает количество строк с полноценной построчной сверкой)
//  * Не тестировалось.



///////////////   ЗАГРУЗКА ПАРАМЕТРОВ из файла "Настройка скриптов.html"

 //  Запуск модального окна (Адрес, Импортируемые данные, Настройки: высота, ширина, центрирование, help, изменение размера, строка состояния)
 var mSettings = window.showModalDialog( "HTML/Настройка скриптов.html", ScriptName, "dialogHeight: 0; dialogWidth: 0; center: Yes; help: No; resizable: No; status: No");

 //  Параметры

 var histVers = +extracting(0, "История и версия файла", 0, "1");   //  0 - включить, 1 - включить при изменении файла книги, 2 - отключить.
 var displayVers = +extracting(0, "История и версия файла", 1, "1");      //  0 - скрыть версию скрипта, 1 - вписывать версию скрипта.
 var myName = extracting(0, "История и версия файла", 2, "");      //  Имя используемое в добавленной записи.

 var metod = +extracting(1, "Метод", 0, "0");     //  Метод сверки
 //  "0" — Точный,
 //  "1" — Быстрый (качественнее работает при крупной детализации),
 //  "2" — Очень быстрый (находит только один несовпадающий участок).
 var minSim = +extracting(1, "Размер детализации", 0, "0");       //  Минимальный размер внутренних совпадающих участков
 //  "0" - 1 символ, "1" - 2 символа, "2" - 3, "3" - 4, "4" - 5, "5" - 6, "6" - 8, "7" - 10, "8" - 12, "9" - 15, "10" - 20 символов

 var font_family = extracting(1, "Основной шрифт", 0, "2");      //  Имя шрифта
 //  "0" - Arial Unicode MS, "1" - Bookman Old Style, "2" - Calibri, "3" - Cambria, "4" - Candara, "5" - Consolas, "6" - Palatino Linotype, "7" - Segoe UI, "8" - Tahoma, "9" - Trebuchet MS
 var font_size = extracting(1, "Основной шрифт", 1, "5");      //  Размер шрифта
 //  "0" - 9pt, "1" - 10pt, "2" - 11pt, "3" - 12pt, "4" - 13pt, "5" - 14pt, "6" - 15pt, "7" - 16pt, "8" - 18pt, "9" - 20pt, "10" - 22pt, "11" - 24pt

 var fon = extracting(1, "Фон", 0, "#F4F1E8");      //  код цвета ("#RGB", "#RRGGBB", "rgb(R, G, B)", "rgb(R%, G%, B%)")

 var txtPosition = +extracting(1, "Расположение текстов", 0, "0");     //  "0" - Не показывать, "1" - Показывать

 var hints = +extracting(1, "Подсказки к кнопкам", 0, "1");     //  "0" - Не показывать, "1" - Показывать

 //  * (0/1 - общие/местные параметры, Заголовок группы параметров, Номер параметра в группе, Значение по умолчанию).
 //  **  Если нет скрипта "Настройка скриптов", то можно настроить параметры здесь, изменив значения по умолчанию.



///////////////   Адаптация положение выбранного шрифта к блоку html-окна.

 switch (+font_family) {              //  Выбор для номера в списке настроек.
         case 0:  font_family = 3;  break;
         case 1:  font_family = 0;  break;
         case 2:  font_family = 4;  break;
         case 3:  font_family = 1;  break;
         case 4:  font_family = 5;  break;
         case 5:  font_family = 9;  break;
         case 6:  font_family = 2;  break;
         case 7:  font_family = 6;  break;
         case 8:  font_family = 7;  break;
         case 9:  font_family = 8;  break;
         default:  font_family = 1;
         }



///////////////   ЗАГРУЗКА МОДУЛЯ "Дополнительные функции.js"  (здесь используется: "historyChange")

 if (!document.getElementById("Module040126")) {   //  Если модуль не установлен...
         window.external.BeginUndoUnit(document, "Загрузка модуля \"Дополнительные функции.js\"");    //  Начинаем запись в систему отмен FBE.
         var module=document.createElement("SCRIPT");       //  Создаем элемент <SCRIPT>.
         module.id = "Module040126";  module.language = "JavaScript";  module.type = "text/javascript";  module.src = "HTML/Дополнительные функции.js";   //  Добавляем атрибуты.
         document.getElementById("userCmd").insertAdjacentElement("afterEnd",  module);    //  Вставляем полученный модуль после "userCmd".
         window.external.EndUndoUnit(document);    //  Завершаем запись в систему отмен FBE.
         }




///////////////   ОБЩИЕ ПЕРЕМЕННЫЕ


 var nbspEntity=getNbspEntity();   //  Неразрывный пробел из настроек FBE.
 var fbwBody=document.getElementById("fbw_body");   //  Элемент с html-текстом книги.
 var scrH = screen.availHeight;   //  Доступная высота экрана.
 var scrW = screen.availWidth;   //  Доступная ширина экрана.

 var k=0;    //  Счетчики в циклах
 var h=0;
 var i=0;
 var j=0;

 var otvet;         //  Ответ пользователя в диалогах.
 var mDialog = [];   //  Массив данных для импорта в окно замены.
 var fileName;         //  Имя файла.

 var codeTxt = "Текст строк файла №1, для скрипта «Сверка „дизайна“»\n\n";     //  Опознавательный код.
 var LCode = codeTxt.length;        //  Длина опознавательного кода.

 var mem = window.clipboardData.getData("Text");        //  Получение текста из буфера обмена.
 if (mem == null)  mem = "";   //  Коррекция.

 var userMem = "";        //  Пользовательский текст из буфера обмена.

 var LUserMem = mem.search(/\t\t\t/);        //  Длина пользовательского текста из буфера обмена.
 if (LUserMem != -1)  LUserMem += 3;  else  LUserMem = 0;   //  Коррекция.

 var mTxt = [];      //  Массив текста для окна сообщения.
 var style1 = "17 жирный центр линия#444 #444 (Cambria, Georgia, Liberation Serif)";   //  Стили текста окна сообщения.
 var style2 = "13 жирный курсив центр #444 (Cambria, Georgia, Liberation Serif)";
 var style3 = "17 фон#335EA8 #FFF";




///////////////   ДИАЛОГ :  Предварительный (для 1-го файла)

 if (mem == ""  ||  mem.substring(LUserMem, LUserMem + LCode) != codeTxt) {             //  Если буфер обмена пуст,   или в памяти только пользовательские данные...
         mTxt = [["Выбор текстов для сверки",, style1],   ["Копирование в буфер обмена – аннотации, истории и всех <body> из первого файла",, style2],   [],   ["Скопировать текст первого файла?",, style3],   []];
         //  Открываем окно с предложением скопировать текст 1-го файла.
         otvet=window.showModalDialog( "HTML/АК-скрипт - Сообщение.html",
             [ScriptName, mTxt, "Скопировать", "Отмена", 1],
             "dialogHeight: 160px; dialogWidth: 500px; center: Yes; help: No; resizable: Yes; status: No;");
         if (otvet != 1)   return;                //  Если получен отказ -- выходим из скрипта.
         if (LUserMem <= 10000003)     //  Если текст в памяти не превышает 10 миллионов символов...
                 userMem = mem + "\t\t\t";     //  то сохраняем его в переменной.
         fileName = GetFileName();                  //  Получаем имя файла.
         if (! fileName)  fileName = "файл №1";     //  Если в FBE нет команды для получения имени файла -- то используем имя "файл №1".
         //  Отправляем собранные данные в буфер обмена.
         window.clipboardData.setData("text", userMem + codeTxt + fileName + "\t\t" + document.getElementById("fbw_body").innerHTML);
         return;      //  Выходим из скрипта.
         }




///////////////   ДИАЛОГ :  Основной (для 2-го файла)

 mTxt = [["Выбор текстов для сверки",, style1],   ["Сверка текста текущего файла и текста ранее скопированного файла",, style2],   [],   ["Запустить процесс сверки текстов?",, style3],   []];
 //  Открываем окно с предложением сверить текст открытого файла с текстом 1-го файла.
 otvet=window.showModalDialog( "HTML/АК-скрипт - Сообщение.html",
     [ScriptName, mTxt, "Сверка", "Отмена", 1],
     "dialogHeight: 160px; dialogWidth: 500px; center: Yes; help: No; resizable: Yes; status: No;");

 if (otvet != 1) {                   //  Если получен отказ...
         mTxt = [["Очистка буфера обмена",, style1],   ["Предыдущие текстовые данные, если они не были огромного размера, будут восстановлены",, style2],   [],   ["Удалить скопированный текст?",, style3],   []];
         //  Открываем окно с предложением удалить или сохранить этот текст.
         otvet=window.showModalDialog( "HTML/АК-скрипт - Сообщение.html",
             [ScriptName, mTxt, "Удалить", "Сохранить", 1],
             "dialogHeight: 160px; dialogWidth: 500px; center: Yes; help: No; resizable: Yes; status: No;");
         if (otvet == 1) {                        //  Если получено разрешение на удаление...
                 if (LUserMem)     //  Если сохранялись пользовательские данные...
                         window.clipboardData.setData("text", mem.substring(0, LUserMem-3));      //  то восстанавливаем их.
                     else  window.clipboardData.clearData("Text");     //  Иначе -- очищаем память.
                 }
         return;      //  Выходим из скрипта.
         }




///////////////   Восстановление пользовательских данных   и   коррекция данных из 1-го файла

  if (LUserMem)     //  Если сохранялись пользовательские данные...
         window.clipboardData.setData("text", mem.substring(0, LUserMem-3));      //  то восстанавливаем их.
     else   window.clipboardData.clearData("Text");     //  Иначе -- очищаем память.

 mem = mem.substring(LUserMem + LCode);        //  Изменяем переменную с данными 1-го файла.



// ---------------------------------------------------------------
 window.external.BeginUndoUnit(document, ScriptName + " v."+NumerusVersion);    // Начало записи в систему отмен FBE.
// ---------------------------------------------------------------



///////////////   ЗАПУСК ОКНА СВЕРКИ

  fileName = GetFileName();                  //  Получаем имя файла.
  if (! fileName)  fileName = "файл №2";     //  Если в FBE нет команды для получения имени файла -- то используем имя "файл №2".

 //  Заполненяем массив с импортируемыми данными.
 mDialog[0] = window.external;      //  Встроенные операции в FBE.
 mDialog[1] = mem;     //  Сохраненные имя и текст первого файла.
 mDialog[2] = document;           //  "Документ" открытого (2-го) файла.
 mDialog[3] = fileName;             //  Имя открытого (2-го) файла.
 mDialog[4] = nbspEntity;     //  Символ неразрывного пробела, принятый в FBE.
 mDialog[5] = font_family;    //  Имя шрифта.
 mDialog[6] = font_size;         //  Размер шрифта.
 mDialog[7] = fon;          //  Цвет фона.
 mDialog[8] = metod;       //  Метод сверки.
 mDialog[9] = minSim;    //  Минимальный размер совпадающего участка (детализация).
 mDialog[10] = mainMinSim;    //  Минимальный размер совпадающего участка (при сверке индексов строк).
 mDialog[11] = ScriptName;       //  Имя скрипта.
 mDialog[12] = hints;       //  Показ подсказок (0/1).
 mDialog[13] = txtPosition;       //  Расположение текстов (0/1).

 //  Открываем окно сверки текстов.
 otvet=window.showModalDialog( "HTML/Сверка «Дизайна».html", mDialog,
     "dialogHeight: "+scrH+"px; dialogWidth: "+scrW+"px; center: Yes; help: No; resizable: Yes; status: No; scroll: No;");

 if (!otvet)  return;      //  Если скрипт html-окна завершился аварийно -- Выходим из основного скрипта.

 //  Записываем данные отправленные html-окном.
 var exit = otvet[0];                      //  Сохраняем код причины выхода (не используется).
 var changeBook = otvet[1];    //  Сохраняем индикатор сохранения текста в FBE.
 var firstLine = otvet[2];           //  Элемент первой строки в видимой области, и её расположение.
 var hBar = otvet[3];                  //  Высота верхней панели.

 GoTo4(fbwBody.getElementsByTagName("P")[firstLine[2]], firstLine[1] - hBar);    //  Выставляем текст книги как в html-окне.




///////////////   Изменения версии и истории файла

 var versionFile=document.getElementById("diVersion").value; //  Извлечение значения версии файла.
 var versionUp=false;                      //  Индикатор повышения версии.
 var histCh=0;                                     //  Код изменения истории.
 var newVersion = versionFile;   //  Значение новой версии.

 try  {
         //  Если включено автоматическое повышение версии, или если есть измененные строки  и в этом случае разрешено включение...
          if (histVers == 0  ||  (changeBook  &&  histVers == 1)) {
                  var NumerusVersion_ = NumerusVersion;    //  Получаем копию версии скрипта.
                  if (!displayVers)  NumerusVersion_ = "";    //  Если запрещено отображение версии -- то удаляем копию.
                  var mR = historyChange(ScriptName, NumerusVersion_, myName, nbspEntity);   //  Запускаем функцию изменения версии и истории файла, и получаем результаты:
                  versionUp=mR[0];   //  Индикатор повышения версии.
                  histCh=mR[1];             //  Код изменения истории.
                  newVersion=mR[2];   //  Значение новой версии.
                  }
         }
 catch(e) {}   //  Если модуль "Дополнительные функции.js" не загружен и запуск функции "historyChange" вызывает ошибку -- то ничего не делаем.


// ---------------------------------------------------------------
 window.external.EndUndoUnit(document);    // Конец записи в систему отмен FBE.
// ---------------------------------------------------------------



///////////////    ОКНО РЕЗУЛЬТАТОВ :  Демонстрационный режим

 var VseStroki_on_off = 0;      // 0 ; 1 //      ("0" — отключить, "1" — включить)
 var d=0;
 if (VseStroki_on_off == 1)  d="показать нули";




///////////////    ОКНО РЕЗУЛЬТАТОВ :  Сборка массива с результатами обработки

 var Tf=new Date().getTime();    //  Момент окончания работы скрипта.
 var mSt=[];          //  Массив строк статистики.
 var ind=0;           //  Индекс строк.
 var sTable="линия#CFCFD1 фон#F0EFF2";    //  Стиль таблицы.

 mSt[ind++]=[ScriptName, "<FONT  style=\"font-family: Arial, Tahoma; vertical-align: bottom; font-size: 15px; color: #843B3B\">версия "+NumerusVersion+"</FONT>", "25 97%центр  #9E4747 (Cambria, Georgia, Liberation Serif)"];   //  Заголовок.

                                 mSt[ind++]=[];                                     //  Пустая строка.
 if (d) {                   mSt[ind++]=["Демонстрационный режим",, "15"];    //  Подзаголовок при включении демонстрационного режима.
                                 mSt[ind++]=[];    }
                                 mSt[ind++]=["Время выполнения", time(Tf - Ts), sTable];
                                 mSt[ind++]=[];                                     //  Пустая строка.

 if (changeBook  ||  d)       //  Если производилось сохранение...
         mSt[ind++]=["   >> Произведена перезапись текста",, "15"];    //  то добавляем соответствующий текст.

 if (!changeBook  ||  d)     //  Если изменений нет...
         mSt[ind++]=["   >> Исправлений нет",, "15"];   //  то добавляем соответствующий текст.

//  История
 if (versionUp ||  histCh  ||  d) { mSt[ind++]=[];
                                                             mSt[ind++]=["Изменение истории и версии fb2-файла",, "17 линия#CFCFD1 центр #1A544F (Cambria, Georgia, Liberation Serif)"];    }
 if (versionUp  ||  d)                      mSt[ind++]=["Версия файла:  "+versionFile+"  ›››  "+newVersion,, sTable];
 if (histCh==1  ||  d)                      mSt[ind++]=["Добавлена новая строка в историю",, sTable];
 if (histCh==2  ||  d)                      mSt[ind++]=["Добавлены две строки в историю",, sTable];
 if (histCh==3  ||  d)                      mSt[ind++]=["Изменены данные в строке истории",, sTable];

 mSt[ind++]= [];
 mSt[ind++]= ["~ ~ ~ • ~ ~ ~",, "центр #2D2D2D (Arial, Tahoma)"];
 mSt[ind++]=["цитата-html",, "75%центр центр #2D2D2D (Arial, Tahoma)"];    //  Добавляем цитату.




///////////////    ОКНО РЕЗУЛЬТАТОВ :  Запуск окна

 var statH = 300 + (mSt.length-9)*19;   //  Примерная высота окна результатов.
 var statW = 580;                                                //  Ширина окна.

 if (statH > scrH * 0.9)     //  Если высота получилась больше установленного максимума,
         statH = Math.round(scrH * 0.9);     //  то выбираем для окна максимальную высоту.

 //  Запуск модального окна (Адрес, Импортируемые данные, Параметры)
 window.showModalDialog(
         "HTML/АК-скрипт - Результаты.html",
         [mSt, true],
         "dialogHeight: "+statH+"px; dialogWidth: "+statW+"px; center: Yes; help: No; resizable: Yes; status: No; scroll: No;");










//////////////////////////////////    ФУНКЦИИ    //////////////////////////////////



///////////////   Функция получения html-написания неразрывного пробела из настроек FBE

function getNbspEntity() {
         try  {
                 var nbspEntity=window.external.GetNBSP();   //  Получаем выбранный символ неразрывного пробела.
                 if (nbspEntity != " ")  return nbspEntity;   //  Если используется нестандартный символ -- возвращаем его.
                 }
         catch(e) {}
         return "&nbsp;";   //  Во всех остальных случаях возвращаем стандартный код.
         }



///////////////   Функция получения имени файла из настроек FBE

 function GetFileName() {
         try  {
                 var fileName=window.external.GetDocumentFileName();
                 if (fileName == "")  return "Новый файл";
                 return fileName;
                 }
         catch(e) { return  false; }
         }



///////////////   Функция извлечения параметров

 function extracting(part, group, N, Default) {
         var k=0;
         if (!mSettings)   return Default;         //  Если нет загруженных параметров -- возвращаем значение по умолчанию.
         for (k=0; k<mSettings.length; k++)       //  Запускаем цикл для всех загруженных параметров.
                 if (mSettings[k][0]==part  &&  mSettings[k][1]==group  &&  mSettings[k][2]==N)   //  Если в запросе и в загруженном параметре полностью совпадает описание,
                         return mSettings[k][3];               //  то возвращаем значение загруженного параметра.
         return Default;             //  В случае, если запрошенный параметр не найден -- возвращаем значение по умолчанию.
         }



///////////////   Функция конвертации времени  (мс  => мин., с)

function time(T) {   //  * Исходные данные: "Промежуток времени (в миллисекундах)".
         var tempus="";    //  Начальное значение для результата.
         var min  = Math.floor(T/60000);   //  Получение минут.
         var secD = (T%60000)/1000;    //  Получение секунд с дробной частью.
         var sec = Math.floor(secD);    //  Получение секунд.
         if (min==0)          //  Если в промежутке времени "0" минут...
                 //  то результат приравниваем к первым 5 символам секунд (с дробной частью), удаляем последние нули (из дробной части), и заменяем точку на запятую.
                 return   (+(secD+"").replace(/(.{1,5}).*/g, "$1")+"").replace(".", ",")+" сек";
             else              //  Если же минут больше "0"...
                 return   min+" мин" + (sec != 0  ?  tempus+=" "+ sec+" с"  :  "");    //  Если же минут больше "0" -- то результат = число минут + число секунд (если они есть).
         }



///////////////   Функция перехода к элементу книги (расширенная версия "GoTo(elem)", но без перемещения курсора)

function GoTo4(elem, Y) {   //  * Исходные данные: "Элемент на который следует сделать переход", "Высота элемента в окне текста книги (в процентах)".

         if (!elem)  return;   //  Если элемент удален  --  выходим из функции.

         var fbeW = document.documentElement.clientWidth;   //  Ширина окна с текстом книги.
         var fbeH = document.documentElement.clientHeight;   //  Высота окна с текстом книги.

         var wW=fbeW;       //  Ширина окна с текстом книги.
         var wH=fbeH - 6;   //  Высота окна с текстом книги (с запасом 3 пикселя по краям).
         var b=elem.getBoundingClientRect();                   //  Получение координат элемента.
         var c=fbwBody.parentNode.getBoundingClientRect();    //  Получение координат раздела <BODY>.
         var H=b.bottom-b.top;                  //  Высота элемента (в пикселях).
         var Width=c.left;         //  Сдвиг вбок.

         if (b.right-c.left > wW) {      //  Если правый край элемента может выйти за границу окна...
                 if (b.right-b.left < wW)     //  то проверяем длину элемента, и если она меньше длины окна,
                         Width = b.right-wW;   //  то выставляем элемент впритык к правому краю окна,
                     else  Width = b.left;      //  а если больше - то выставляем элемент впритык к левому краю.
                 }
         if (H <= wH)          //  Если высота элемента меньше высоты окна...
                 window.scrollBy(Width, b.top - Y);   //  то выставляем элемент согласно указанному расположению,
             else  window.scrollBy(Width, b.top - 3);       //  а если нет - то выставляем элемент почти впритык к верхнему краю.
         }

}






///////////////   ИСТОРИЯ ИЗМЕНЕНИЙ

//  v.1.0 — Создание скрипта — Александр Ка (19.08.2026)





