/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Превратить таблицы в простой текст",
  "version": "1.3",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Превращает таблицы в простой текст.",
  "description": "Удаляет табличные контейнеры и превращает ячейки/строки в обычные абзацы по заменам HTML. Текст переносится в линейное представление; сетка, объединения ячеек и табличное выравнивание теряются.",
  "scope": "Область определяется условиями и обходом DOM в данном скрипте.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Запустите команду из меню или дерева скриптов.",
    "Просмотрите результат до сохранения. Для структурной правки проверьте книгу и связанные ссылки."
  ],
  "warnings": [
    "Скрипт содержит операции изменения документа. Перед массовой обработкой сохраните отдельную копию книги.",
    "Сложные таблицы после уплощения нужно проверить на правильный порядок чтения."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "f128f8533d5588c774ce2c2c589a0753fd19b476efbf25fa16cf8a249ff0f0ee",
    "reviewStatus": "generated"
  },
  "tags": [
    "Обработка форматирования"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Превратить таблицы в простой текст v1.3". Автор Sclex

function Run() {
  var fbwBody=document.getElementById("fbw_body");
  window.external.BeginUndoUnit(document,"превращение таблиц в текст"); 
  var allDivs=fbwBody.getElementsByTagName("DIV");
  var re0=new RegExp("<SPAN(( [^>]+)? class=image( [^>]+)?)>(<IMG( [^>]*)?>)<\/SPAN>","gi");
  var re0_="</P><DIV$1>$4</DIV><P>";
  var re1=new RegExp("<P( [^>]*)?></P>","gi");
  var re1_="";
  var myOuterHTML;
  for (var i=allDivs.length-1; i>=0; i--) {
    if (
         (allDivs[i].className && allDivs[i].className.toUpperCase()=="TABLE") || 
         (allDivs[i].className && allDivs[i].className.toUpperCase()=="TR")
       ) allDivs[i].removeNode(false);
  }
  var allPs=fbwBody.getElementsByTagName("P");
  for (var i=allPs.length-1; i>=0; i--) {
    if (allPs[i].className && (allPs[i].className.toUpperCase()=="TD" || allPs[i].className.toUpperCase()=="TH")) {
      allPs[i].removeAttribute("className");
      allPs[i].removeAttribute("fbalign");
      allPs[i].removeAttribute("fbvalign");
      allPs[i].removeAttribute("fbcolspan");
      allPs[i].removeAttribute("fbrowspan");
      allPs[i].removeAttribute("id");
      allPs[i].removeAttribute("fbstyle");
      myOuterHTML=allPs[i].outerHTML;
//      alert("Найден абзац:\n\n"+myOuterHTML)
      if (myOuterHTML.search(re0)>=0) {
//       alert("Найден абзац, подлежащий замене:\n\n"+myOuterHTML)
//       alert("Заменено на:\n\n"+myOuterHTML.replace(re0,re0_).replace(re1,re1_));
       allPs[i].outerHTML=myOuterHTML.replace(re0,re0_).replace(re1,re1_);
      }
    }
  }
  window.external.EndUndoUnit(document);
  try {
    window.external.SetStatusBarText("Все таблицы успешно превращены в текст.");
  }
  catch (e)
  {}
}
