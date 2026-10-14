/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Превратить внешние ссылки в текст",
  "version": "1.2",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Снимает разметку у ссылок с внешними адресами, сохраняя текст.",
  "description": "Обходит ссылки HTML-документа с конца и убирает элемент A, оставляя дочерний текст и разметку, если href не начинается с file:// или # либо содержит после # протокол http/https. Адреса не открывает и не загружает; после обработки сообщает число снятых ссылок.",
  "scope": "Все элементы document.links текущего HTML-документа, включая элементы вне fbw_body, если они присутствуют; выделение не используется.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Запустите скрипт для снятия разметки внешних ссылок в книге.",
    "Проверьте получившийся обычный текст и ссылки, которые должны остаться действующими."
  ],
  "warnings": [
    "Критерий опирается на строку href: обычные внешние URL и другие нелокальные схемы теряют ссылку, остаётся её содержимое.",
    "Обход идёт по document.links без ограничения fbw_body; связанные элементы интерфейса HTML, если они есть, также могут попасть в выборку."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "8701564537796783bd0b617725b51011179da394d7d10228396a57b500a2bbaa",
    "reviewStatus": "generated"
  },
  "tags": [
    "Чистка"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Снятие ссылок выполняется внутри группы отмены, которая закрывается на штатном пути. Полнота отката при ошибке обработки не проверена."
  }
}
</FBE-SCRIPT-CARD>*/
//скрипт "Превращение внешних ссылок в текст"
//Автор Sclex
//Версия 1.2
//Топик про скрипты FBE: http://www.fictionbook.org/forum/viewtopic.php?t=4412
//Мой сайт про мои скрипты: http://scripts.fictionbook.org

function Run() {
  /**
   * Возвращает единицу измерения с правильным окончанием
   * 
   * @param {Number} num      Число
   * @param {Object} cases    Варианты слова {nom: 'час', gen: 'часа', plu: 'часов'}
   * @return {String}            
   */
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
  
 window.external.BeginUndoUnit(document,"превращение внешних ссылок в текст");
 var re1=new RegExp("^ *?(file://|#)","i");
 var re2=new RegExp("# *https?:","i");
 var removesCnt=0;
 for (i=document.links.length-1;i>=0;i--)
  if (document.links[i].getAttribute("href").search(re1)<0 ||
      document.links[i].getAttribute("href").search(re2)!=-1) {
    document.links[i].removeNode(false);
    removesCnt++;
  }
 window.external.EndUndoUnit(document);
 if (removesCnt==0)
  var msgStr="В документе не нашлось ни одной ссылки, подлежащей расформатированию.";
 else
  var msgStr=units(removesCnt, {nom: 'Была превращена в простой текст ', gen: 'Были превращены в простой текст ', plu: 'Было превращено в простой текст '})+
            removesCnt+
            units(removesCnt, {nom: ' ссылка.', gen: ' ссылки.', plu: ' ссылок.'});
 MsgBox(msgStr);
}
