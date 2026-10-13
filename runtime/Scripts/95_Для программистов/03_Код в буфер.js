/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Код в буфер",
  "version": "Не указана",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Копирует outerHTML всего HTML-документа редактора в буфер.",
  "description": "Читает document.documentElement.outerHTML. В буфер попадает внутреннее HTML-представление FBE, включая оболочку, а не сериализованный FB2/XML.",
  "scope": "Область определяется условиями и обходом DOM в данном скрипте.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Запустите команду из меню или дерева скриптов."
  ],
  "warnings": [
    "Текущее содержимое буфера обмена может быть заменено или очищено."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "0d16977493fc147cc3af837fd0d49ebaeac45460f5ce3803ff338f7453c9922d",
    "reviewStatus": "generated"
  },
  "tags": [
    "Для программистов",
    "буфер обмена"
  ],
  "undo": {
    "status": "no_explicit_grouping",
    "description": "Явная группировка Undo в этом файле не найдена. Это не означает ни гарантированное отсутствие Undo, ни безопасное выполнение."
  }
}
</FBE-SCRIPT-CARD>*/
function Run()
{
 var body=document.documentElement; //getElementById("fbw_body");
// var body=document.all.binobj;
 if(!body) return;
 var s1 = body.outerHTML;
 window.clipboardData.setData("text",s1);
}
