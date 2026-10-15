/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Встроенный таймер",
  "version": "Не указана",
  "authors": [
    {
      "name": "Forcosigan",
      "role": "author"
    },
    {
      "name": "wotti",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Включает напоминание о сохранении каждые 100 секунд.",
  "description": "После подтверждения запускает таймер с интервалом 100 секунд. В напоминании «Отмена» прекращает таймер при одиночном запуске. Скрипт не сохраняет книгу автоматически.",
  "scope": "Текущий сеанс окна редактора; действует, пока работает созданный таймер.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Запустите скрипт и подтвердите включение напоминания.",
    "При очередном сообщении нажмите «ОК», чтобы оставить один запущенный таймер, или «Отмена», чтобы его остановить. Не запускайте скрипт повторно до остановки таймера."
  ],
  "warnings": [
    "Это напоминание, а не автосохранение; сохраните книгу вручную.",
    "Повторный запуск создаёт ещё один таймер. Из-за общей переменной id отмена может остановить только последний таймер, оставив предыдущий работающим."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "099828550d3644192980ac518b50049060fb83ddaa9637ff4689dd55e040f451",
    "reviewStatus": "generated"
  },
  "tags": [
    "Общие инструменты"
  ],
  "undo": {
    "status": "no_explicit_grouping",
    "description": ""
  },
  "settings": []
}
</FBE-SCRIPT-CARD>*/
// Таймер с напоминанием Forcosigan,  wotti

var answer2="";
var id="";
function Run(){
	var answer = confirm ("Включить напоминание о сохранении?");
	if (answer)
	{
		id = setInterval(function(){answer2 = confirm ("Не забудьте сохраниться!\n\nДля отключения напоминания нажмите \"Отмена\""); if (!answer2) clearInterval(id);}, 100000);
	}
}
