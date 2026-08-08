//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//             «Дополнительные функции»
//  Модуль предназначен для упрощения применения и обновления распространенных функций
//  Модуль тестировался в FBE v.2.7.9 (win XP, IE8 и win 7, IE11)
//  v.1.0 — Создание модуля. Функция "historyChange" — Александр Ка (05.01.2026)
//  v.1.1 — Небольшая правка функции "historyChange" — Александр Ка (15.01.2026)
//  v.1.2 — Небольшая правка функции "historyChange" — Александр Ка (05.05.2026)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


///////////////   Функция изменения версии и истории файла

function historyChange(ScriptName, NumerusVersion, myName, nbspEntity) {
         //  * Исходные данные: "Имя скрипта", "Версия скрипта", "Имя пользователя", "Символ неразрывного пробела".
         //  **  "Версия скрипта" и "Имя пользователя" можно не указывать

 var versionFile=document.getElementById("diVersion").value; //  Извлечение значения версии файла.
 var versionUp=false;   //  Индикатор повышения версии.
 var histCh=0;                            //  Код изменения истории.
 var newVersion=versionFile;   //  Начальное значение новой версии.

         // Получение раздела истории

 var fbwBody=document.getElementById("fbw_body");
 var History=fbwBody.firstChild;   //  Предполагаемый раздел истории.

 //  Поиск раздела "истории"
 while (History != null  &&  History.className != "history")    //  Пока не найдем настоящий раздел истории, или окажется, что истории нет в тексте...
         History = History.nextSibling;         //  переходим на следующий раздел.

//  Добавление раздела истории
 if (History==null)  {                 //   Если нет истории...
         History = document.createElement("DIV");     //   Создание нового раздела
         var Annotation=fbwBody.firstChild;
         while (Annotation!=null  &&  Annotation.className!="annotation") Annotation=Annotation.nextSibling;   //  Поиск аннотации к книге
         if (Annotation!=null)  Annotation.insertAdjacentElement("afterEnd",History);      //  Размещаем новый раздел    или после аннотации (если она есть)...
                 else  fbwBody.insertAdjacentElement("afterBegin",History);                //  ...или в начале "fbwBody".
         History.className = "history";                  //  Присваиваем новому разделу    класс "история" и необходимые атрибуты.
         History.setAttribute("xmlns:l", xlNS);    //  *  "xlNS" и "fbNS" - переменные (адреса) из "main.js".
         History.setAttribute("xmlns:f", fbNS);
         History.insertAdjacentElement("beforeEnd",document.createElement("P"));     //  Добавляем пустую строку
         window.external.inflateBlock(History.lastChild)=true;   //  и делаем её реально пустой.
         }

         //  Создание массива с прошедшими датами

 var mReDate=[];         //  Массив с прошедшими датами.
 var D = new Date().getTime();   //  Начальное значение даты.
 var dat;            //  Текущая дата.
 var fullDate;   //  Объект дата-время.
 var Day;       //  День.
 var Month;   //  Месяц.
 var Year;    //  Год.

 for (var j=0; j<10; j++) {              //  Запускаем цикл для получения недавних дат, в котором...
         fullDate = new Date(D);                         //  получаем полную дату,
         Day = fullDate.getDate();                            //  день,
         Month = ("0" + (1+fullDate.getMonth())).replace(/^.*(..)$/g, "$1");   //  месяц,
         Year = fullDate.getFullYear();                 //  год,
         if (j==0)  dat = Day + "." + Month + "." + Year;      //  сохраняем текущую дату
         mReDate[j] = Day + "[\\\.\\\-\/]" + Month + "[\\\.\\\-\/](" + (""+Year).replace(/^(.*)..$/g, "$1") + ")?" + (""+Year).replace(/^.*(..)$/g, "$1");
                                             //  и заполняем массив текстом очередной даты с учетом разнообразия её записи.
         D -= 86400000;   //  При этом каждый раз уменьшаем проверяемую дату на один день.
         }

         //  Поиск недавней записи в "истории"

 var povtorD = false;   //  Индикатор повторной обработки за последние 10 дней.
 var mP = History.getElementsByTagName("P");   //  Получение всех строк в "Истории".
 var s="";               //  Содержимое строки.
 var k=0;               //  Счетчик цикла.

fff:
 for (j=mP.length-1;  j>=0;  j--) {    //  Последовательный просмотр строк истории (с конца).
         s = mP[j].innerHTML;                //  Содержимое строки.
         for (k=0; k<10; k++) {                //  Запускаем цикл для проверки даты.
                 if (s.search(new RegExp(mReDate[k], "")) !=-1) {   //  Если проверяемая дата есть в строке истории...
                         povtorD = true;                    //  то отмечаем это,
                         break fff;                            //  и прерываем оба цикла проверки.
                         }
                 }
         }

         //  Обновление записи в истории изменений

 var textMyName = inCode1(myName);    //  Имя в тексте.
 if (textMyName!="")                           //  Если есть заполненное имя...
         textMyName += ", ";   //  то добавляем к текстовой записи запятую.

 NumerusVersion = inCode1(NumerusVersion);    //  Версия скрипта текстом.
 ScriptName = inCode1(ScriptName);    //  Имя скрипта текстом.

 if (NumerusVersion)
         ScriptName += " " + NumerusVersion;    //  Добавление к имени текст версии скрипта.

         //  Стартовая формула
 var reHist00s = new RegExp("[^А-яЁёA-Za-z0-9]"+ScriptName+"[^А-яЁёA-Za-z0-9]","");
         //  Добавление точки с запятой
 var reHist01 = new RegExp("(.[^…\\\?!\\\.,;:—])(\\\s|"+nbspEntity+")([–—] "+textMyName+mReDate[k]+")","");
 var reHist01_ = "$1; $3";
         //  Добавление точки
 var reHist02 = new RegExp("(.[^…\\\?!\\\.,;:—])[,;:]{0,1}(\\\s|"+nbspEntity+")([–—] "+textMyName+mReDate[k]+")","");
 var reHist02_ = "$1. $3";
         //  Добавление слова "Скрипт"
 var reHist03 = new RegExp("(.)(\\\s|"+nbspEntity+")([–—] "+textMyName+mReDate[k]+")","");
 var reHist03_ = "$1 Скрипт: $3";
         //  Добавление имени скрипта
 var reHist04 = new RegExp("(.)(\\\s|"+nbspEntity+")([–—] "+textMyName+mReDate[k]+")","");
 var reHist04_ = "$1 "+ScriptName+" $3";

 if (povtorD) {                                         //  Если найдена запись с недавней датой...
         if (s.search(reHist04) !=-1) {    //  и если в строке имя пользователя и дата записаны по форме: "— (Имя, Дата)"...
                 if (s.search(reHist00s) ==-1) {    //  Проверяем строку на наличие записи имени скрипта, и если этой записи нет...
                         if (s.search(/([Сс]крипт):/) !=-1)  s = s.replace(/([Сс]крипт):/g, "$1ы:");   //  то заменяем при необходимости слово "Скрипт" на "Скрипты",
                         if (s.search(reHist01) !=-1)  s = s.replace(reHist01, reHist01_);                   //  добавляем при необходимости точку с запятой,
                         if (s.search(/[Сс]крипты?:/) ==-1)  s = s.replace(reHist02, reHist02_).replace(reHist03, reHist03_);   //  добавляем при необходимости слово "Скрипт"
                         s = s.replace(reHist04, reHist04_);      //  и добавляем имя скрипта.
                         }
                 if (k!=0)                           //  Проверяем дату, и если она не сегодняшняя...
                         s = s.replace(new RegExp(mReDate[k], ""), dat);   //  то заменяем на сегодняшнюю.
                 if (mP[j].innerHTML != s) {      //  Проверяем изменилась ли строка истории, и если она изменилась...
                         mP[j].innerHTML = s;    //  то сохраняем её в тексте
                         histCh=3;   //  и отмечаем это на индикаторе.
                         }
                 return [versionUp, histCh, newVersion];   //  Прерываем обработку и возвращаем результат.
                 //  *  Массив: "Индикатор повышения версии [true/false]", "Код изменения истории", "Значение новой версии").
                 //  *  Код изменения истории:  0 - без изменений, 1 - добавлена одна строка, 2 - добавлены две строки, 3 - изменена одна строка.
                 }
         }


         //  Повышение версии

 var versionText = "";   //  Текст с версией в истории изменений.

 //  Проверка на валидность версии файла
 var ValidationVersion=(versionFile.length <=10  &&  versionFile.search(/^(\d{1,10}(\.\d{1,8})?)?$/g) !=-1);    //  сравнение с шаблоном:  "цифры + (точка + цифры)"

 //   Изменение версии файла
 if (ValidationVersion) {      //  Если версия валидна...
         if (versionFile =="")          //  Если версия не заполнена...
                 versionFile = "1.0";    //  то изменяем начальную версию на "1.0".
         if (versionFile.search(/^\d+$/g) !=-1)   //  Если версия без точки...
                 newVersion = versionFile + ".1";     //  то для новой версии добавляем ".1".
             else {                                                                              //  Если в версии есть точка...
                     newVersion = +versionFile.replace(/^\d+\./g, "");  //  извлекаем цифры после точки,
                     newVersion++;                                                                                           //  увеличиваем полученное число на единицу
                     newVersion = versionFile.replace(/\.\d+$/g, "")+"."+newVersion;   //   и добавляем к нему первую группу цифр.
                     }
         if (newVersion.length <=10)                                        //  Если новая версия валидна...
                 document.getElementById("diVersion").value=newVersion;   //  то изменяем версию в файле,
                 versionUp=true;                                                          //  отмечаем это на индикаторе
                 var versionText="v."+newVersion+" — ";    //  и создаем текст для истории.
         }

         //   Добавление строк в историю изменений

 var reHist11 = new RegExp("^(\\\s|"+nbspEntity+"){0,}$","");   //  Признак пустой строки.
 var reHist12 = new RegExp("(^|\\n)[^0-9]{0,12}"+versionFile.replace(/\./, "\\.")+"([^0-9]|$)","");   //  Поиск старой версии.

 //   Добавление строки с информацией о старой версии
 if (ValidationVersion  &&  History.innerText.search(reHist12)==-1) {       //  Если в истории нет записи о старой версии...
         if (History.lastChild.innerHTML.search(reHist11)==-1)                                               //  то проверяем наличие пустой строки в конце истории
                 History.insertAdjacentElement("beforeEnd",document.createElement("P"));       //  и если ее нет - добавляем новую.
         History.lastChild.innerHTML = "v."+versionFile+" — ?";  //  Затем добавляем в строку информацию о старой версии
         histCh++;                                        //  и изменяем индикатор истории.
         }

 //   Добавление строки с информацией о новой версии
 if (History.lastChild.innerHTML.search(reHist11)==-1)                                   //  Если в конце истории нет готовой пустой строки...
         History.insertAdjacentElement("beforeEnd", document.createElement("P"));   //  то добавляем новую строку.
 History.lastChild.innerHTML = versionText+" Скрипт: "+ScriptName+" — "+textMyName+dat;  //  Добавляем в строку информацию о новой версии.
 histCh++;                       //  Изменяем индикатор истории.

 return [versionUp, histCh, newVersion];   //  Возвращаем результат. (массив: "Индикатор повышения версии [true/false]", "Код изменения истории", "Значение новой версии").
         //  *  Код изменения истории:  0 - без изменений, 1 - добавлена одна строка, 2 - добавлены две строки, 3 - изменена одна строка.

//   Функция преобразования стандартного fb2-текста в html-текст (без преобразования тегов)
function inCode1(text) {         //  * Исходные данные: "Текст без тегов".
         if (text == undefined)   //  Если "text" не определяется...
                 text = "";                 //  то заменяем его на пустой текст.
             else  text += "";    //  В противном случае, преобразуем "text", который может оказаться числом, в текст.
         text = text.replace(/&/g, "&amp;").replace(/ /g, nbspEntity).replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/­/g, "&shy;");  //  Заменяем пять символов на коды.
         return  text;    //  Возвращаем результат.
         }
 }
