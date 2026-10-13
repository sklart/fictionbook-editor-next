/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Обновить иллюстрации",
  "version": "1.1",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Принудительно обновляет отображение изображений в редакторе.",
  "description": "Для IMG временно очищает src и возвращает прежнее значение, затем обновляет список обложек через FillCoverList. Набор бинарных данных не перекодирует.",
  "scope": "Область определяется условиями и обходом DOM в данном скрипте.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Запустите команду из меню или дерева скриптов."
  ],
  "warnings": [
    "Меняет свойства DOM для обновления отображения; «без постоянных правок» не равно «без вмешательства в DOM». Явные Undo-вызовы в этом файле закомментированы.",
    "Записи в DOM могут относиться к временным маркерам, интерфейсу или отчёту; отсутствие постоянных правок книги — результат статического разбора, не runtime-гарантия."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "a09789267d877676f17bdea0ddda22275eb06967ac857cf7e7d8c62da6980609",
    "reviewStatus": "generated"
  },
  "tags": [
    "Иллюстрации",
    "изображения"
  ],
  "undo": {
    "status": "no_explicit_grouping",
    "description": "Явная группировка Undo в этом файле не найдена. Это не означает ни гарантированное отсутствие Undo, ни безопасное выполнение."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Обновить иллюстрации"
// Версия 1.1

function Run() {
 //window.external.BeginUndoUnit(document,"обновление иллюстраций");
 var imgs=document.getElementsByTagName("IMG");
 for (var i=imgs.length-1; i>=0; i--) {
  var MyImg=imgs[i];
  var pic_id=MyImg.src;
  MyImg.src="";
  MyImg.src=pic_id;
 }
 FillCoverList();
 //window.external.EndUndoUnit(document);
}
