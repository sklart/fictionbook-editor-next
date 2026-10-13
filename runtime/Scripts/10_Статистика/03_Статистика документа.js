/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Статистика документа",
  "version": "Не указана",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Статистика документа.",
  "description": "Собирает сведения из текущего документа и выводит результат сообщением, в буфер обмена или HTML-отчёт согласно коду варианта. Не выполняет автоматического исправления анализируемых элементов.",
  "scope": "Текущая книга или определённые кодом её разделы.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже."
  ],
  "warnings": [],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "c350f0e671b3096727ecfa7c4b10ef3094f81819ee582352222c954ddf933b5a",
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
// Returns total size for binary objects
//======================================

function divisors(no) // put comma each 3 characters
{
 var out="", str=no.toString();

 for(var i=str.length-1, j=0; i>=0; i--, j++)
 {
  var a=str.charAt(i); if(j && (j%3)==0) a=a+",";

  out=a+out;
 }

 while(out.length<12) out="   "+out; //alight string right

 return out;
}
//--------------------------------------

function Run()
{
 var msg="";

 var body = document.getElementById("fbw_body");
 var text_size=body.innerText.length;

 var ps=document.getElementsByTagName("P");
/*
 for(var i=0; i<ps.length; i++)  // slow
 {
  text_size+=ps.item(i).innerText.length;
 }
*/
 msg+="Настоящий документ содержит "+ps.length+" параграф"+(ps.length!=1?"ов":"")+".\n\n";

 msg+="Общий размер текста: "+divisors(text_size)+" символов\n\n";

 //---- 
  var lists=document.all.tiCover.getElementsByTagName("select");

 for(var i=0; i<lists.length; i++)
  if(lists[i].id=='href')
  {
   var cover=lists[i].value; // preserve current cover
  }

  msg+=(cover.length>1 ? "Обложка, " : "Нет обложки, ");


 var imgs=document.getElementsByTagName("IMG");

 msg+=(imgs.length==1 ? "одна иллюстрация, " : "иллюстраций - "+imgs.length+", ");

 var notes=0; var as=document.getElementsByTagName("A");

 for(var i=0; i<as.length; i++)  // slow
 {
  if(as[i].className=="note") notes++;
 }

 msg+=(notes==1 ? "одна сноска\n" : "сносок - "+notes+"");

 msg+="\n\n------------------------------------------------------  \n\n";

 var objects=document.all.binobj.getElementsByTagName("DIV");

 var total=0, totalBase64=0;

 for(var i=0; i<objects.length; i++)
 {
  var len=window.external.GetBinarySize(objects[i].base64data);

  total+=len; 
  
  var b64=Math.floor(len*4/3); totalBase64+=b64+Math.floor(b64/72)*2+60;
 }


 if(objects.length==1)
  msg+="Присутствует один присоединённый бинарник.\n\n";
 else
  msg+="Прикреплённые бинарные объекты:     "+objects.length+" штук.\n\n";

 msg+="Общий объём бинарников:      "+divisors(total)+" байт\n";
 msg+="Общий размер в формате FB2: "+divisors(totalBase64)+" байт\n";

 MsgBox(msg);
}
