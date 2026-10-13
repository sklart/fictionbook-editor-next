/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Точка, тире, буква в нижнем регистре",
  "version": "Не указана",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Точка, тире, буква в нижнем регистре.",
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
      "path": "HTML/Точка, тире, буква в нижнем регистре - набор фреймов.html",
      "sha256": "266ea0d45d8ae165e03bc33a96bc2c4667d725c04b724cf48dd54e8ad7f15407",
      "optional": true
    }
  ],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "c73dcb6e83b4e5bb097f2ef7799565dc10b48f09ce25da60789360c7a61213cc",
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
 rslt=window.showModalDialog("HTML/Точка, тире, буква в нижнем регистре - набор фреймов.html",params,
       "dialogHeight: "+dialogHeight+"; dialogWidth: "+dialogWidth+"; "+
       "center: Yes; help: No; resizable: Yes; status: No;");
}
