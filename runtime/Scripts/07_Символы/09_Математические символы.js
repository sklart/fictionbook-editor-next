/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Математические символы",
  "version": "Не указана",
  "authors": [
    {
      "name": "Sclex",
      "role": "original_author"
    },
    {
      "name": "stokber",
      "role": "contributor",
      "contribution": "declared_credit"
    },
    {
      "name": "stokber",
      "role": "contributor",
      "contribution": "table_compiler"
    },
    {
      "name": "jurgennt",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Открывает таблицу «Математические символы» для вставки символов в текст.",
  "description": "Немодальное HTML-окно получает документ и окно FBE. Нажатие на ячейку формирует код вставки через диапазон/pasteHTML в исходном окне и группирует его в Undo. Открытие таблицы само по себе текст не меняет; нажатие на символ — меняет.",
  "scope": "Область определяется обходом DOM и условиями запуска; подробности в правилах и функциях.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Проверьте перечисленные параметры: часть задаётся в исходном JS, часть — через общий диалог настроек.",
    "Сохраняйте структуру Scripts/HTML и связанные файлы; один JS может быть недостаточен для работы.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже.",
    "Просмотрите результат до сохранения. Для структурной правки проверьте книгу и связанные ссылки."
  ],
  "warnings": [
    "Скрипт предусматривает изменение книги. Перед массовой обработкой сохраните отдельную копию.",
    "Вставка выполняется HTML-зависимостью через eval в окне FBE; короткий файл запуска не содержит самих DOM-записей."
  ],
  "dependencies": [
    {
      "path": "HTML/Математические символы.html",
      "sha256": "7e95cbd7b5e3b742846a28b725eb7a19da98170ca7362ef49f94d4d5166f2e0a",
      "optional": true
    }
  ],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "3660b36890147a5fd579c574b8974f72c219135ea97daa72fa4d84434b788664",
    "reviewStatus": "generated"
  },
  "tags": [
    "Символы",
    "параметры"
  ],
  "undo": {
    "status": "may_be_delegated",
    "description": "Прямых Undo-вызовов нет; действия могут выполняться связанным HTML/JS. Гарантия отмены не установлена."
  },
  "settings": [
    {
      "name": "dialogWidth",
      "value": "200px",
      "expression": "\"200px\"",
      "description": "ширина окна таблицы символов",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "dialogHeight",
      "value": "352px",
      "expression": "\"352px\"",
      "description": "высота окна таблицы символов",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "dialogLeft",
      "value": "5px",
      "expression": "\"5px\"",
      "description": "координата X",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    }
  ]
}
</FBE-SCRIPT-CARD>*/
//Таблица математических символов
//                                jurgennt™, май 2008 г.
//                          script engene by Sclex v.1.3
//                          таблица: stokber, май 2024
function Run() {
 var dialogWidth="200px"; //ширина окна таблицы символов
 var dialogHeight="352px"; //высота окна таблицы символов
 var dialogLeft="5px";                                   //координата X
 var params=new Object(); 
 params["fbw_body"]=document.getElementById("fbw_body");
 params["document"]=document;
 params["window"]=window;
 var rslt=window.showModelessDialog("HTML/Математические символы.html",params,
      "dialogHeight: "+dialogHeight+"; dialogWidth: "+dialogWidth+"; dialogLeft: "+dialogLeft+"; "+
      "center: Yes; help: No; resizable: Yes; status: No; scroll: No;"); 
}
