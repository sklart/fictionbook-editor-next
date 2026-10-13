/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Спецсимволы",
  "version": "Не указана",
  "authors": [
    {
      "name": "Sclex",
      "role": "original_author"
    },
    {
      "name": "jurgennt",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Открывает таблицу «Спецсимволы» для вставки символов в текст.",
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
      "path": "HTML/Спецсимволы.html",
      "sha256": "f49a59d4a30e2d90cdd9ae55e33ae3ca6153135e4862303b1daa09a8a39e9f98",
      "optional": true
    }
  ],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "f6477f079ff89309d6a740ddd0d40a586ec171bcd27a0f694f0993e690d5d6a7",
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
//Таблица специальных символов
//                                jurgennt™, май 2008 г.
//                          script engene by Sclex v.1.3

function Run() {
 var dialogWidth="200px"; //ширина окна таблицы символов
 var dialogHeight="352px"; //высота окна таблицы символов
 var dialogLeft="5px";                                   //координата X
 var params=new Object(); 
 params["fbw_body"]=document.getElementById("fbw_body");
 params["document"]=document;
 params["window"]=window;
 var rslt=window.showModelessDialog("HTML/Спецсимволы.html",params,
      "dialogHeight: "+dialogHeight+"; dialogWidth: "+dialogWidth+"; dialogLeft: "+dialogLeft+"; "+
      "center: Yes; help: No; resizable: Yes; status: No; scroll: No;"); 
}
