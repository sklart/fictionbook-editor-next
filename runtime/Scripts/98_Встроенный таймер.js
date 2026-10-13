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
      "name": "Forcosigan",
      "role": "contributor",
      "contribution": "declared_credit"
    },
    {
      "name": "wotti",
      "role": "author"
    },
    {
      "name": "wotti",
      "role": "contributor",
      "contribution": "declared_credit"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Показывает периодическое напоминание сохранить книгу.",
  "description": "После подтверждения устанавливает setInterval на 100000 миллисекунд. В очередном напоминании «Отмена» снимает таймер. Сам файл не сохраняется.",
  "scope": "Сеанс и интерфейс редактора.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Запустите скрипт в FBE Next и следуйте сообщениям; требования к документу указаны в preconditions.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже."
  ],
  "warnings": [
    "Это напоминание, а не автосохранение; подтверждение сообщения не записывает книгу на диск."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
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
    "description": "Явная группировка Undo в этом файле не найдена. Это не означает ни гарантированное отсутствие Undo, ни безопасное выполнение."
  },
  "settings": [
    {
      "name": "interval",
      "value": 100000,
      "expression": "100000",
      "description": "Период напоминания в миллисекундах (100 секунд); задан аргументом setInterval.",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    }
  ]
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
