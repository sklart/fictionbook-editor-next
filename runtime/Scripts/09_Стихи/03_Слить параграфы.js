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
  "summary": "Сливает HTML-границы абзацев внутри выделенного фрагмента.",
  "description": "В HTML непустого выделения заменяет переход </P><P> на пробел, а после дефиса — на дефис без пробела; распознаёт простые P и P с атрибутом align. Изменённый HTML вставляет обратно в выделенный диапазон. Другие варианты разметки P могут не совпасть с шаблоном.",
  "scope": "Только непустой выделенный диапазон; соседние абзацы вне выделения не обходятся.",
  "modifiesDocument": true,
  "requiresSelection": true,
  "usage": [
    "Выделите фрагмент с абзацами, которые нужно слить, и запустите скрипт.",
    "Проверьте границы и форматирование после вставки: скрипт обрабатывает только распознанные шаблоном теги P."
  ],
  "warnings": [
    "Скрипт заменяет HTML выбранного фрагмента. Границы P с другими атрибутами могут остаться, а форматирование на стыках требует проверки.",
    "При ошибке вставки операция отмены может остаться открытой; полный откат не проверен."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
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
    "description": "Подстановка изменённого HTML на штатном пути сгруппирована в одну операцию отмены редактора. При ошибке вставки группа может остаться открытой; полнота отката не проверена."
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
