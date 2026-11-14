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
