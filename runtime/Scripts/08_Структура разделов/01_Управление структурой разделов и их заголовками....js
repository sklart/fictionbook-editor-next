/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Управление структурой разделов и их заголовками",
  "version": "Не указана",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Открывает инструмент управления разделами и их заголовками.",
  "description": "Рабочий HTML-интерфейс получает структуру текущего документа и позволяет изменять заголовки/контейнеры. Это средство структурного редактирования, а не только отображение дерева.",
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
    "Скрипт содержит операции изменения документа. Перед массовой обработкой сохраните отдельную копию книги."
  ],
  "dependencies": [
    {
      "path": "HTML/Управление структурой разделов и их заголовками - набор фреймов.html",
      "sha256": "694341237d65101e829c67daffaac963c35f11ad267ab89095aff969ec3abba5",
      "optional": true
    }
  ],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "f33eaba0ac52dc762e55eb104dbea40b9c5fb8767ad16da491dc402a1d2f62ef",
    "reviewStatus": "generated"
  },
  "tags": [
    "Структура разделов",
    "параметры"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
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
      "value": "540px",
      "expression": "\"540px\"",
      "description": "высота диалогового окна",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    }
  ]
}
</FBE-SCRIPT-CARD>*/
function Run() {

 var dialogWidth="640px"; // ширина диалогового окна
 var dialogHeight="540px"; // высота диалогового окна
 var params=new Object();
 var rslt;
 body=document.getElementById("fbw_body");
 params["fbw_body"]=body;
 rslt=window.showModalDialog("HTML/Управление структурой разделов и их заголовками - набор фреймов.html",params,
       "dialogHeight: "+dialogHeight+"; dialogWidth: "+dialogWidth+"; "+
       "center: Yes; help: No; resizable: Yes; status: No;");
 if (rslt) {
  window.external.BeginUndoUnit(document,"section structure editing");      
  body.innerHTML=rslt;
  InflateIt(body);
  window.external.EndUndoUnit(document);
 }
}
