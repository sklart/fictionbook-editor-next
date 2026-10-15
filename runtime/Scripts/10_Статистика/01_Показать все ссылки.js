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
  "summary": "Разделяет ссылки документа на внутренние и внешние и показывает их текст и адреса.",
  "description": "Внешними считает адреса с http:// или https://. Остальные относит к внутренним; адреса, не начинающиеся с #, помечает [!!!]. Результат выводится в сообщении.",
  "scope": "Все ссылки визуального документа; выделение не используется.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Запустите команду без выделения. В сообщении появятся отдельные списки внутренних и внешних ссылок с текстом и адресом."
  ],
  "warnings": [],
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
    "description": ""
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
