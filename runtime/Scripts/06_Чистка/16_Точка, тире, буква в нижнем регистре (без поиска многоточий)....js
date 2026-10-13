/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Точка, тире, буква в нижнем регистре (без поиска многоточий)",
  "version": "Не указана",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Точка, тире, буква в нижнем регистре (без поиска многоточий).",
  "description": "Ищет характерную последовательность точки/тире и строчной буквы через связанный HTML-интерфейс; позволяет исправлять найденные места. Вариант без поиска многоточий исключает соответствующую группу случаев. Короткий JS только запускает рабочий диалог.",
  "scope": "Область определяется условиями и обходом DOM в данном скрипте.",
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
    "Скрипт предусматривает изменение книги. Перед массовой обработкой сохраните отдельную копию."
  ],
  "dependencies": [
    {
      "path": "HTML/Точка, тире, буква в нижнем регистре (без поиска многоточий) - набор фреймов.html",
      "sha256": "7634903d42c440ac78376fc642102ae588da7935776158b6e793fed2e15240d1",
      "optional": true
    }
  ],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "6ae02311147b51db18455fb100e625c72eab0835d691fab29db64c980fb3df38",
    "reviewStatus": "generated"
  },
  "tags": [
    "Чистка",
    "параметры"
  ],
  "undo": {
    "status": "may_be_delegated",
    "description": "Прямых Undo-вызовов нет; действия могут выполняться связанным HTML/JS. Гарантия отмены не установлена."
  },
  "settings": [
    {
      "name": "dialogWidth",
      "value": "640px",
      "expression": "\"640px\"",
      "description": "ширина диалогового окна",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "dialogHeight",
      "value": "480px",
      "expression": "\"480px\"",
      "description": "высота диалогового окна",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    }
  ]
}
</FBE-SCRIPT-CARD>*/
function Run() {
 var dialogWidth="640px"; // ширина диалогового окна
 var dialogHeight="480px"; // высота диалогового окна
 var params=new Object();
 var rslt;
 var fbwBody=document.getElementById("fbw_body");
 try { var nbspChar=window.external.GetNBSP(); var nbspEntity; if (nbspChar.charCodeAt(0)==160) nbspEntity="&nbsp;"; else nbspEntity=nbspChar;}
 catch(e) { var nbspChar=String.fromCharCode(160); var nbspEntity="&nbsp;";} 
 params["fbwBody"]=fbwBody;
 params["myDoc"]=document;
 params["myWin"]=window;
 params["nbspChar"]=nbspChar;
 params["nbspEntity"]=nbspEntity;
 rslt=window.showModalDialog("HTML/Точка, тире, буква в нижнем регистре (без поиска многоточий) - набор фреймов.html",params,
       "dialogHeight: "+dialogHeight+"; dialogWidth: "+dialogWidth+"; "+
       "center: Yes; help: No; resizable: Yes; status: No;");
}
