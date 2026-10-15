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
  "summary": "Повторно задаёт адреса всех IMG и обновляет список обложек в редакторе.",
  "description": "По очереди временно очищает src каждого IMG документа, возвращает прежний адрес и вызывает FillCoverList. Предназначен для обновления отображения; записи в бинарные данные и перекодирования изображений нет.",
  "scope": "Все IMG в документе, включая элементы интерфейса вне fbw_body; затем список обложек. Выделение не используется.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Запустите скрипт, если иллюстрации или обложка отображаются неверно. Предварительное выделение не требуется.",
    "Проверьте отображение изображений и список обложек после обновления."
  ],
  "warnings": [
    "Скрипт временно меняет src у всех IMG документа, включая элементы интерфейса. При ошибке между очисткой и восстановлением адрес отдельного изображения может остаться пустым."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
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
    "description": ""
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
