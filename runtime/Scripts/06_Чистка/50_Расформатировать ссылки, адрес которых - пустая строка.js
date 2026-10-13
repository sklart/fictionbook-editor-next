/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Расформатировать ссылки, адрес которых - пустая строка",
  "version": "1.2",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Снимает разметку: ссылки, адрес которых - пустая строка.",
  "description": "Снимает A-оболочки с пустым адресом через removeNode(false), сохраняя их дочерний текст. Учитывает представление адреса относительно document.location.",
  "scope": "Область определяется условиями и обходом DOM в данном скрипте.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже.",
    "Просмотрите результат до сохранения. Для структурной правки проверьте книгу и связанные ссылки."
  ],
  "warnings": [
    "Скрипт содержит операции изменения документа. Перед массовой обработкой сохраните отдельную копию книги."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "0b69bc82082867344f5aff6a4a78fe7b1ee0bfdedf44edc1cd0d700843e8dccd",
    "reviewStatus": "generated"
  },
  "tags": [
    "Чистка"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
  }
}
</FBE-SCRIPT-CARD>*/
// Скритп "Расформатировать ссылки, адрес которых - пустая строка"
// Версия 1.2

function Run() {

 function units(num, cases) {
     num = Math.abs(num);
     
     var word = '';
     
     if (num.toString().indexOf('.') > -1) {
         word = cases.gen;
     } else { 
         word = (
             num % 10 == 1 && num % 100 != 11 
                 ? cases.nom
                 : num % 10 >= 2 && num % 10 <= 4 && (num % 100 < 10 || num % 100 >= 20) 
                     ? cases.gen
                     : cases.plu
         );
     }
     return word;
 }
 
  window.external.BeginUndoUnit(document,"расформатирование ссылок, адрес которых - пустая строка");
  var fbwBody=document.getElementById("fbw_body");
  var links=fbwBody.getElementsByTagName("A");
  var i;
  var removedLinksCnt=0;
  var docHref=document.location.href.replace(/main[.]html$/,"");
  for (i=links.length-1; i>=0; i--)
    if (links[i].getAttribute("href")=="" || links[i].getAttribute("href")==docHref) {
      removedLinksCnt++;
      links[i].removeNode(false);
    }
  window.external.EndUndoUnit(document);
  if (removedLinksCnt!=0)
    MsgBox(units(removedLinksCnt,{nom: 'Была расформатирована', gen: 'Были расформатированы', plu: 'Было расформатировано'})+
           " "+removedLinksCnt+" "+
           units(removedLinksCnt,{nom: 'ссылка', gen: 'ссылки', plu: 'ссылок'})+".");
  else
    MsgBox("Ссылок с пустым адресом — в документе не нашлось.");
}
