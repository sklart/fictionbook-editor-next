/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Заглавные и строчные буквы",
  "version": "1.0",
  "authors": [
    {
      "name": "Александр Ка",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Показывает списки букв с заглавной и строчной формой для диапазона Unicode или заданной строки.",
  "description": "В основном режиме перебирает символы U+0001–U+FFFF и выводит все найденные буквы, а также списки их заглавных и строчных форм. Во втором режиме сортирует заданную в исходнике строку, удаляет повторы и выводит те же списки. В обоих режимах может сокращать последовательности символов до диапазонов заданной длины. Книгу не меняет.",
  "scope": "Все символы U+0001–U+FFFF при стандартном режиме; заданная в исходнике строка при режиме 2. Содержимое книги не используется.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Запустите скрипт и введите минимальную длину диапазонов в появившемся поле (по умолчанию 4).",
    "Просмотрите в сообщении списки всех букв, заглавных и строчных форм. Для вывода без сокращения диапазонов введите нечисловое значение.",
    "Чтобы обработать собственную строку, задайте её в исходнике и переключите режим работы на 2."
  ],
  "warnings": [],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "e7742493dd0b24efdf046af44ee62bcde106d42fcafc48234da3b0ade4241393",
    "reviewStatus": "generated"
  },
  "tags": [
    "Регистр",
    "параметры"
  ],
  "undo": {
    "status": "no_explicit_grouping",
    "description": ""
  },
  "settings": [
    {
      "name": "metod",
      "title": "Режим работы",
      "value": 1,
      "expression": "1",
      "description": "1 — перебрать символы U+0001–U+FFFF; 2 — обработать заданную строку.",
      "source": "code"
    },
    {
      "name": "userLine",
      "title": "Строка для режима 2",
      "value": "абвгдеёжзийклмнопрстуфхцчшщъыьэюя",
      "expression": "\"абвгдеёжзийклмнопрстуфхцчшщъыьэюя\"",
      "description": "Символы для анализа при режиме 2; повторы удаляются, строка сортируется.",
      "source": "code"
    },
    {
      "name": "interval",
      "title": "Минимальная длина диапазонов",
      "value": 4,
      "description": "Запрашивается при запуске. Числовое значение включает сокращение последовательностей; нечисловое значение выводит символы полностью.",
      "source": "dialog"
    }
  ]
}
</FBE-SCRIPT-CARD>*/
//             «Заглавные и строчные буквы»
// v.1.0 — Создание скрипта — Александр Ка (16.07.2026)

//  В основном режиме выбираются только те символы, у которых есть два вида написания: заглавные и строчные буквы.   Диапазон символов: U+0001 - U+FFFF

function Run() {

 var ScriptName="Заглавные и строчные буквы";
 var NumerusVersion="1.0";


///////////////   НАСТРОЙКИ

 //  Режим работы
 var metod = 1;      // 1 ; 2 //      ("1" — Все символы "2" — Произвольная строка)
 var userLine = "абвгдеёжзийклмнопрстуфхцчшщъыьэюя";   //  Текст произвольной строки

 //  * Для режима произвольной строки есть удаление дублей символов, сортировка, вывод диапазонов.
 //  * Отсутствует преобразование диапазонов в ряд символов, удаление/добавление экранирования для управляющих символов. Т.е. строка "A-z\\n\\.\\-" совсем не обрабатывается.



///////////////   ПЕРЕМЕННЫЕ

 var k = 0;
 var h = 0;
 var char = "";
 var allChar = "";
 var upperChar = "";
 var lowerChar = "";
 var interval = window.external.InputBox("Введите минимальную длину диапазонов", "FBE script message", 4);



///////////////   ВСЕ СИМВОЛЫ

 if (metod == 1) {

 //  Сборка строк из всех символов
 for (k=1; k<65536; k++) {
         char = String.fromCharCode(k);
         if (char.toUpperCase() == char.toLowerCase()
             ||  char != char.toUpperCase()  &&  char != char.toLowerCase()  ||
             char.toUpperCase() == char.toUpperCase().toLowerCase()  ||  char.toLowerCase() == char.toLowerCase().toUpperCase())
             //  Если регистр символа не меняется   или   это символ в "среднем" регистре
             //  или   символ с измененным регистром становится символом, у которого нельзя изменить регистр...
                 continue;    //  то пропускаем этот символ.
         allChar += char;
         if (char == char.toLowerCase())
                 lowerChar += char;
         if (char == char.toUpperCase())
                 upperChar += char;
         }

 //  Открываем окно с результатами (без сокращений)
 if ((interval+"").search(/^\d+$/) == -1) {
         MsgBox (" _____ Все буквы _____\n"+
             "["+allChar+"]\n\n"+
             " _____ Заглавные буквы _____\n"+
             "["+upperChar+"]\n\n"+
             " _____ Строчные буквы _____\n"+
             "["+lowerChar+"]\n\n"+
             "* Скопировать текст -- Ctrl+Ins");
         return;
         }

 //  Открываем окно с результатами (с использованием диапазонов)
 MsgBox (" _____ Все буквы _____\n"+
     "["+addInterval(allChar)+"]\n\n"+
     " _____ Заглавные буквы _____\n"+
     "["+addInterval(upperChar)+"]\n\n"+
     " _____ Строчные буквы _____\n"+
     "["+addInterval(lowerChar)+"]\n\n"+
     "* Скопировать текст -- Ctrl+Ins");
 }



///////////////   ВВЕДЕННАЯ СТРОКА

 if (metod == 2) {
 userLine = userLine;
 var Line = "";

 var L1;
 var L2;

 //  Сортировка и удаление дублей символов
LBL:
 for (k=userLine.length-1; k>=0; k--) {
         L1 = -1;
         L2 = Line.length+1;
         char = userLine.charAt(k);

         while (L2-L1 > 1) {
                 LMidi = Math.floor((L2+L1)/2);
                 if (char == Line.charAt(LMidi))
                         continue LBL;
                 if (char > Line.charAt(LMidi))
                         L1 = LMidi;
                     else  L2 = LMidi;
                 }
         Line = Line.substring(0, L2) + char + Line.substring(L2);
         }

 var LLine = Line.length;

 //  Сборка строк из всех символов
 for (k=0; k<LLine; k++) {
         char = Line.charAt(k);
         if (char.toUpperCase() == char.toLowerCase()
             ||  char != char.toUpperCase()  &&  char != char.toLowerCase()  ||
             char.toUpperCase() == char.toUpperCase().toLowerCase()  ||  char.toLowerCase() == char.toLowerCase().toUpperCase())
             //  Если регистр символа не меняется   или   это символ в "среднем" регистре
             //  или   символ с измененным регистром становится символом, у которого нельзя изменить регистр...
                 continue;    //  то пропускаем этот символ.
         allChar += char;
         if (char == char.toUpperCase())
                 upperChar += char;
         if (char == char.toLowerCase())
                 lowerChar += char;
         }

 //  Открываем окно с результатами (без сокращений)
 if ((interval+"").search(/^\d+$/) == -1) {
         MsgBox (
             " _____ Все символы _____\n"+
             "["+Line+"]\n\n"+
             " _____ Все буквы _____\n"+
             "["+allChar+"]\n\n"+
             " _____ Заглавные буквы _____\n"+
             "["+upperChar+"]\n\n"+
             " _____ Строчные буквы _____\n"+
             "["+lowerChar+"]\n\n"+
             "* Скопировать текст -- Ctrl+Ins");
         return;
         }

 //  Открываем окно с результатами (с использованием диапазонов)
 MsgBox (
      " _____ Все символы _____\n"+
      "["+addInterval(Line)+"]\n\n"+
     " _____ Все буквы _____\n"+
     "["+addInterval(allChar)+"]\n\n"+
     " _____ Заглавные буквы _____\n"+
     "["+addInterval(upperChar)+"]\n\n"+
     " _____ Строчные буквы _____\n"+
     "["+addInterval(lowerChar)+"]\n\n"+
     "* Скопировать текст -- Ctrl+Ins");
 }



///////////////   ФУНКЦИЯ ДЛЯ РАССТАНОВКИ ДИАПАЗОНОВ

 function addInterval(line) {
         line = "\u0000" + line;
         var lastPos = line.length-1;
         for (k=line.length-2; k>=0; k--) {
                 if (line.charCodeAt(k) + 1 == line.charCodeAt(k+1))
                         continue;
                 if (line.charCodeAt(lastPos) - line.charCodeAt(k+1) > interval-2) {
                         line = line.substring(0, k+2) + "-" + line.substring(lastPos);
                         }
                 lastPos = k;
                 }
         return line.substring(1);
         }
}
