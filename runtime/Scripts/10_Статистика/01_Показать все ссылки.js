/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Показать все ссылки",
  "version": "Не указана",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Показывает все ссылки.",
  "description": "Собирает сведения из текущего документа и выводит результат сообщением, в буфер обмена или HTML-отчёт согласно коду варианта. Не выполняет автоматического исправления анализируемых элементов.",
  "scope": "Текущая книга или определённые кодом её разделы.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже."
  ],
  "warnings": [
    "Есть запуск внешнего приложения или открытие сетевого адреса; проверьте сформированную команду и передаваемые данные."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "7df03223a5715ffd5c73bb1833d8eb7515eaaf42bceb7e5f2deb0f6073a4fa7b",
    "reviewStatus": "generated"
  },
  "tags": [
    "Статистика"
  ],
  "undo": {
    "status": "no_explicit_grouping",
    "description": "Явная группировка Undo в этом файле не найдена. Это не означает ни гарантированное отсутствие Undo, ни безопасное выполнение."
  }
}
</FBE-SCRIPT-CARD>*/
//======================================
// Shows all the link in the document
//======================================

function Run()
{
 var links=document.getElementsByTagName("A"); if(!links) return;

 if(links.length==0){ MsgBox("\nВ этом документе нет ссылок. \n\n"); return; }

 var msg="", cnt=1;

 msg+="Внутренние ссылки:\n\n";

 for(var i=0; i<links.length; i++)
 {
  var href=links[i].getAttribute("href");

  if(href.indexOf("http://")!=-1 || href.indexOf("https://")!=-1) continue;

  var istr=""+cnt; if(istr.length<2) istr="0"+istr; 

  if(href.indexOf("#")!=0) istr="[!!!]  "+istr;
  
  cnt++; if(href=="") href="(null)";

  msg+=" "+istr+". "+links[i].innerText+" -> "+href+" \n";
 }

 msg+="\n------------------------------------------------------------------- \n\nВнешние ссылки:\n\n";

 cnt=1;

 for(i=0; i<links.length; i++)
 {
  var href=links[i].getAttribute("href");

  if(href.indexOf("http://")==-1 && href.indexOf("https://")==-1) continue;

  var istr=""+cnt; if(istr.length<2) istr="0"+istr;

  cnt++; if(href=="") href="(null)";

  msg+=" "+istr+". "+links[i].innerText+" -> "+href+" \n";
 }

 MsgBox(msg);

}
