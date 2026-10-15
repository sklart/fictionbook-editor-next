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
  "summary": "Убирает оболочку ссылок с пустым адресом внутри книги.",
  "description": "Обходит элементы A внутри fbw_body и снимает ссылку, сохраняя её содержимое, когда атрибут href пуст либо равен базовому адресу текущего HTML-документа без main.html. Затем сообщает число изменённых ссылок.",
  "scope": "Все ссылки A внутри fbw_body; выделение не используется. Сравнение адреса выполняется со строкой document.location.href после удаления завершающего main.html.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Запустите скрипт, чтобы убрать ссылки без адреса в книге.",
    "После выполнения проверьте текст бывших ссылок и итоговое сообщение."
  ],
  "warnings": [
    "Ссылка с адресом, совпадающим с вычисленным базовым адресом HTML-документа, также будет расформатирована.",
    "Сравнение строковое; другие эквивалентные формы пустого адреса код отдельно не нормализует."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
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
    "description": ""
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
