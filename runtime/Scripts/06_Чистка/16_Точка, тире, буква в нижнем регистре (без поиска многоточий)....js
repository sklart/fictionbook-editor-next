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
  "summary": "Находит точку или запятую перед тире и буквой не того регистра; многоточия пропускает.",
  "description": "Открывает диалог из двух HTML-фреймов. От конца текущего выделения или позиции курсора ищет сочетания знака пунктуации, тире и буквы. Для точки и многоточия проверяет строчную букву, для запятой — прописную; предлагает отдельно заменить знак либо изменить регистр буквы. Можно пропустить находку или перейти к ней без замены; многоточие этот вариант не ищет.",
  "scope": "Текстовые узлы книги от конца текущего выделения или позиции курсора до конца fbw_body; выделять весь документ не требуется.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор перед местом поиска или выделите фрагмент, после которого нужно начать.",
    "В диалоге для каждой находки выберите исправление знака, регистра, пропуск или переход без замены.",
    "Проверьте изменённые предложения после закрытия диалога."
  ],
  "warnings": [
    "Поиск зависит от структуры текстовых узлов и шаблона знака, тире и буквы; каждую найденную фразу следует проверить по смыслу.",
    "Оформление набора фреймов ссылается на отсутствующий HTML/main.css; основной фрейм использует два существующих CSS-файла."
  ],
  "dependencies": [
    {
      "path": "HTML/Точка, тире, буква в нижнем регистре (без поиска многоточий) - набор фреймов.html",
      "sha256": "7634903d42c440ac78376fc642102ae588da7935776158b6e793fed2e15240d1",
      "optional": false
    },
    {
      "path": "HTML/Точка, тире, буква в нижнем регистре (без поиска многоточий) - фрейм 1.html",
      "sha256": "8a5d64603bcb0be76f9ca1be84dfcca981bb141384bfbad52043481095a949fe",
      "optional": false
    },
    {
      "path": "HTML/Точка, тире, буква в нижнем регистре (без поиска многоточий) - фрейм 2.html",
      "sha256": "3c22052bc1ab31c415605983ece75ddd303d926d4ea81d6053443541f46736d7",
      "optional": false
    },
    {
      "path": "HTML/css/Точка, тире, буква в нижнем регистре - main.css",
      "sha256": "2fcbad106955502350815bd01fb9220af97bd4db1509839f439bdf6ee8bf9a02",
      "optional": false
    },
    {
      "path": "HTML/css/Точка, тире, буква в нижнем регистре - add.css",
      "sha256": "afdeaa3289b6c3382190c421f13b4e4f60546d0fd67683eacb90590395ce6281",
      "optional": false
    },
    {
      "path": "HTML/main.css",
      "sha256": null,
      "optional": true
    }
  ],
  "dependenciesComplete": true,
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
    "description": "Диалог группирует отдельные подтверждённые правки знака и регистра для отмены. Полный откат всей сессии не проверен."
  },
  "settings": [
    {
      "name": "dialogWidth",
      "value": "640px",
      "expression": "\"640px\"",
      "description": "Начальная ширина диалога поиска и исправления.",
      "valueMeaning": "Исходное значение в коде скрипта."
    },
    {
      "name": "dialogHeight",
      "value": "480px",
      "expression": "\"480px\"",
      "description": "Начальная высота диалога поиска и исправления.",
      "valueMeaning": "Исходное значение в коде скрипта."
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
