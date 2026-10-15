/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Показать все-все ссылки",
  "version": "Не указана",
  "authors": [
    {
      "name": "mkb",
      "role": "modifier"
    },
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "partial_attribution",
  "summary": "Разделяет ссылки документа на внутренние и внешние и показывает их текст и адреса.",
  "description": "Внешними считает адреса с http:// или https://. Остальные относит к внутренним; адреса, не начинающиеся с #, помечает [!!!]. Результат выводится в отдельном окне с прокруткой.",
  "scope": "Все ссылки визуального документа; выделение не используется.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Запустите команду без выделения. Списки внутренних и внешних ссылок откроются в отдельном окне с прокруткой."
  ],
  "warnings": [
    "Записи в DOM могут относиться к временным маркерам, интерфейсу или отчёту; отсутствие постоянных правок книги — результат статического разбора, не runtime-гарантия."
  ],
  "dependencies": [
    {
      "path": "HTML/Показать все-все ссылки.html",
      "sha256": "0911ec8c8e7a60fc023c13359e156ee8a9a4780aa8209a24f66917d1cbf17f43",
      "optional": true
    }
  ],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "1f41affdd4c55408680f3d89cfa1314a2396aaceebd384bfe7fa2a15db782394",
    "reviewStatus": "generated"
  },
  "tags": [
    "Статистика"
  ],
  "undo": {
    "status": "may_be_delegated",
    "description": ""
  }
}
</FBE-SCRIPT-CARD>*/
//======================================
// Shows all links in browser
// modified by mkb
//======================================

function Run()
{
 var links=document.getElementsByTagName("A"); if(!links) return;

 if(links.length==0){ MsgBox("\nДокумент не содержит ссылок. \n\n"); return; }

 var msg="", cnt=1;

 msg+="ВНУТРЕННИЕ ССЫЛКИ:\n\n";

 for(var i=0; i<links.length; i++)
 {
  var href=links[i].getAttribute("href");

  if(href.indexOf("http://")!=-1 || href.indexOf("https://")!=-1) continue;

  var istr=""+cnt; if(istr.length<2) istr="0"+istr; 

  if(href.indexOf("#")!=0) istr="[!!!]  "+istr;
  
  cnt++; if(href=="") href="(null)";

  msg+=" "+istr+". "+links[i].innerText+" → "+href+" \n";
 }

 msg+="\n-------------------------------------------- \n\nВНЕШНИЕ ССЫЛКИ:\n\n";

 cnt=1;

 for(i=0; i<links.length; i++)
 {
  var href=links[i].getAttribute("href");

  if(href.indexOf("http://")==-1 && href.indexOf("https://")==-1) continue;

  var istr=""+cnt; if(istr.length<2) istr="0"+istr;

  cnt++; if(href=="") href="(null)";

  msg+=" "+istr+". "+links[i].innerText+" → "+href+" \n";
 }

 MyMsgWindow(msg);

}

function MyMsgWindow(msg)
{
 var MsgWindow = window.open("HTML/Показать все-все ссылки.html",null,"height=710,width=300,status=no,toolbar=no,menubar=no,location=no,scrollbars=yes,resizable=yes");
 MsgWindow.document.body.innerText = msg;
}
