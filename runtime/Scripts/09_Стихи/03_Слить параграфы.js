/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Слить параграфы",
  "version": "Не указана",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Объединяет параграфы.",
  "description": "Проверяет непустое выделение, убирает границы абзацев в его HTML и вставляет слитый результат через pasteHTML. Применяется к выбранному диапазону, не ко всей книге.",
  "scope": "Выделенный диапазон либо полные абзацы, которые он пересекает; см. описание варианта.",
  "modifiesDocument": true,
  "requiresSelection": true,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Подготовьте выделение, описанное в scope и условиях запуска; некоторые команды работают с целыми абзацами.",
    "Запустите команду из меню или дерева скриптов.",
    "Просмотрите результат до сохранения. Для структурной правки проверьте книгу и связанные ссылки."
  ],
  "warnings": [
    "Скрипт содержит операции изменения документа. Перед массовой обработкой сохраните отдельную копию книги."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "b8be17725bffe2923c6b435395d413031e0cb3d38798ea3614aac39d85ab0278",
    "reviewStatus": "generated"
  },
  "tags": [
    "Стихи"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
  }
}
</FBE-SCRIPT-CARD>*/
//======================================
// Useful for merging lines each of
// which is a paragraph.
//======================================

function Run()
{
 var rng=document.selection.createRange();
 
 if(rng.compareEndPoints("StartToEnd", rng)!=0)
 {
  var str=rng.htmlText.replace(/([\-]\<\/P\>)\s*(\<P(\s+align=\"?\w*\"?)?\>)/gim, "-");

  str=str.replace(/(\<\/P\>)\s*(\<P(\s+align=\"?\w*\"?)?\>)/gim, " ");

  if(str!=rng.htmlText) // something has been replaced
  {
   window.external.BeginUndoUnit(document,"Слить параграфы");
   rng.pasteHTML(str); // replace selection in the document
    window.external.EndUndoUnit(document);
  }
 }
}
