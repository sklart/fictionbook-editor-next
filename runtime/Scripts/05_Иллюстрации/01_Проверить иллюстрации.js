/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Проверить иллюстрации",
  "version": "1.11",
  "authors": [
    {
      "name": "Alex Saveliev",
      "role": "author"
    },
    {
      "name": "Alex Saveliev",
      "role": "contributor",
      "contribution": "declared_credit"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Находит отсутствующие и неиспользуемые иллюстрации/вложения.",
  "description": "Сопоставляет IMG, binary и ссылку на обложку, формирует перечни неиспользуемых объектов и отсутствующих изображений. Не удаляет автоматически найденные объекты.",
  "scope": "Область определяется условиями и обходом DOM в данном скрипте.",
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
    "sha256": "ddd2ebd362b76fc57bfa633c91824caf0cc890430446b02952a5fa8bdfac7fd4",
    "reviewStatus": "generated"
  },
  "tags": [
    "Иллюстрации",
    "изображения"
  ],
  "undo": {
    "status": "no_explicit_grouping",
    "description": "Явная группировка Undo в этом файле не найдена. Это не означает ни гарантированное отсутствие Undo, ни безопасное выполнение."
  }
}
</FBE-SCRIPT-CARD>*/
//======================================
// Поиск отсутствующих или неиспользуемых изображений
//                  Alex Saveliev script for FBW2         version 1.11
//======================================

// TO DO: Go to the first broken image if any

function Run()
{
 var lists=document.all.tiCover.getElementsByTagName("select");

 var covers=""; for(var i=0; i<lists.length; i++) if(lists[i].id=='href') covers+=lists[i].value+'|';

 // Find unused images

 var imgs=fbw_body.getElementsByTagName("IMG"), imgs_len=imgs.length;
 var bins=document.all.binobj.getElementsByTagName("DIV"), bins_len=bins.length;

 var unused="";


 for(var i=0; i<bins_len; i++)
 {
  var found=false; var bin="fbw-internal:#"+bins[i].all.id.value;

  for(var j=0; j<imgs_len; j++) if(imgs[j].src==bin){ found=true; break; }

  if(!found && covers.indexOf(bins[i].all.id.value)==-1) unused+=" • "+bins[i].all.id.value+"\n";
 }

 if(unused==''){ if(bins && bins.length>0) unused=' • все используются\n'; else unused=' • нет бинарных объектов\n'; }

 unused="\nНеиспользуемые бинарные объекты: \n"+unused;

 // Find broken images (no binary)
 


   // if(!imgs || imgs.length==0, !bins || bins.length==0){ MsgBox("\nВ этой книге нет ни обложки, ни иллюстраций. \n\n"); return; }
   
   //-----------------------------------------------------------------
   // с закомментированной выше строкой скрипт не указывал на наличие пустых ссылок если в документе не было ни одного бинарника.(stokber)
   // MsgBox("imgs.length: "+imgs.length+"\nbins.length: "+bins.length);
   if(imgs.length==0 && bins.length==0){ MsgBox("\nВ этой книге нет ни обложки, ни иллюстраций. \n\n"); return; }
   //========================================
   
 var broken="";

 for(var i=0; i<imgs_len; i++)
 {
  var found=false; var img=imgs[i].src.substr(14);

  for(var j=0; j<bins_len; j++) if(bins[j].all.id.value==img){ found=true; break; } 

  if(!found) broken+=" • "+img+"\n";
 }

 if(broken=='') broken=" • не найдено\n";

 MsgBox("           —= AlexS Script=— \n   «Проверка иллюстраций» v.1.11\n\n" +unused+"\nОтсутствующие иллюстрации: \n"+broken+"\n");

}
