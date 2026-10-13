/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Сохранить вложения на накопитель",
  "version": "Не указана",
  "authors": [
    {
      "name": "Alex Saveliev",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Извлекает все бинарные вложения через SaveBinary.",
  "description": "После подтверждения перебирает binobj и передаёт идентификатор и base64data каждого вложения штатному SaveBinary. Сохраняет файлы, не удаляя вложения из книги. Реальная база относительного пути определяется реализацией SaveBinary, поэтому каталог не обещает конкретный каталог только по тексту диалога.",
  "scope": "Область определяется условиями и обходом DOM в данном скрипте.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже."
  ],
  "warnings": [
    "Файловые операции не отменяются обычным Ctrl+Z в книге.",
    "При совпадении имён поведение зависит от SaveBinary и переданного флага prompt=0; перед извлечением проверьте каталог назначения."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "02a47ae9e7c2efb0a51d98242078694e65821c017e63520704fe99994f29e66c",
    "reviewStatus": "generated"
  },
  "tags": [
    "Иллюстрации"
  ],
  "undo": {
    "status": "no_explicit_grouping",
    "description": "Явная группировка Undo в этом файле не найдена. Это не означает ни гарантированное отсутствие Undo, ни безопасное выполнение."
  }
}
</FBE-SCRIPT-CARD>*/
//======================================
// Save all binaries
//======================================

function Run()
{
 var Ts=new Date().getTime();
 var bin_objects=document.all.binobj.getElementsByTagName("DIV");

 if(bin_objects.length==0){
MsgBox('	–= Alex Saveliev Script =– \n\n'+
         'В этом документе нет бинарных объектов. \n\n'); return; }

 if(!AskYesNo('	–= Alex Saveliev Script =– \n\n'+
            'Подтвердите ваше желание извлечь все бинарные объекты \n'+
            'из этого документа и сохранить их рядом с книгой,\n'+
            'в тот же самом каталоге. \n\n')) return false;

 for(var i=0, cnt=0; i<bin_objects.length; i++)
 {
  var ret=window.external.SaveBinary(bin_objects[i].all.id.value, bin_objects[i].base64data, 0);
  if(ret) cnt++;
 }


 var Tf=new Date().getTime();
 var Tmin = Math.floor((Tf-Ts)/60000);
 var Tsec = Math.ceil((Tf-Ts)/1000-Tmin*60);
 var Tsek = Math.ceil(10*((Tf-Ts)/1000-Tmin*60))/10;
 var Tssek = Math.ceil(100*((Tf-Ts)/1000-Tmin*60))/100;
 var Tsssek = Math.ceil(1000*((Tf-Ts)/1000-Tmin*60))/1000;
 if (Tssek<1 && Tmin<1)  var TimeStr=Tsssek+ " сек"
 else { if (Tsek<10 && Tmin<1)  var TimeStr=Tssek+ " сек"
 else { if (Tmin<1) var TimeStr=Tsek+ " сек"
 else if (Tmin>=1) var TimeStr=Tmin+ " мин " +Tsec+ " с" }}

 if(cnt==0)
  MsgBox('	–= Alex Saveliev Script =– \n\n'+
              ' Ни один бинарник не сохранён!\n\n'+
              'Возможно файлы уже существуют?   \n');
 else
  MsgBox('	–= Alex Saveliev Script =– \n\n'+
              'Было сохранено ' +cnt+ ' из ' +i+ ' бинарных объектов. \n\n'+
              'Время выполнения: '+TimeStr );
}
