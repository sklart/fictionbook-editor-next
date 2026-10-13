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
  "summary": "Превращает внешние ссылки в текст.",
  "description": "Обходит document.links, проверяет href и снимает оболочку внешней ссылки через removeNode(false), сохраняя её видимый текст. Не превращает адрес в отдельное примечание и не скачивает содержимое сайта.",
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
    "sha256": "8701564537796783bd0b617725b51011179da394d7d10228396a57b500a2bbaa",
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
