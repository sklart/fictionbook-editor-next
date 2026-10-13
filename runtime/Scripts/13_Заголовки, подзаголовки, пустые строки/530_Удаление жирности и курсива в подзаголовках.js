/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Удаление жирности и курсива в подзаголовках",
  "version": "1.0",
  "authors": [
    {
      "name": "jurgennt",
      "role": "author"
    },
    {
      "name": "Sclex",
      "role": "original_author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Скрипт удаляет жирный и основной курсив из подзаголовков.",
  "description": "Скрипт удаляет жирный и основной курсив из подзаголовков.",
  "scope": "Область определяется условиями и обходом DOM в данном скрипте.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Проверьте перечисленные параметры: часть задаётся в исходном JS, часть — через общий диалог настроек.",
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
    "sha256": "74eeb627436d83f307d1fe8dcfa0d085bc57ee63838cb01f09b84667e2a721d5",
    "reviewStatus": "generated"
  },
  "tags": [
    "Заголовки, подзаголовки, пустые строки",
    "параметры"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
  },
  "settings": [
    {
      "name": "ObrabotkaHistory",
      "value": true,
      "expression": "true",
      "description": "обрабатывать ли history",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "ObrabotkaAnnotation",
      "value": true,
      "expression": "true",
      "description": "обрабатывать ли annotation",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    }
  ]
}
</FBE-SCRIPT-CARD>*/
//======================================
//            "Удаление жирности и курсива в подзаголовках. v.1.02"
//                                             Автор скрипта - jurgennt
//                                             Engine by ©Sclex
//                                                    01.05.2007 – 07.05.2008
//~~~~~~~~~~~~~~~~~~
// Скрипт удаляет жирный и основной курсив из подзаголовков.

var VersionNumber="1.0";

//обрабатывать ли history
var ObrabotkaHistory=true;
//обрабатывать ли annotation
var ObrabotkaAnnotation=true;

/* var sIB="<EM>|<STRONG>|<EM><STRONG>|<STRONG><EM>";
var fIB="</EM>|</STRONG>|</EM></STRONG>|</STRONG></EM>";
var aIB="<EM>|<STRONG>|<EM><STRONG>|<STRONG><EM>|</EM>|</STRONG>|</EM></STRONG>|</STRONG></EM>"; */

// var alter = confirm("Подзаголовки цитат и стихов тоже чистим?");

function Run() {

 try { var nbspChar=window.external.GetNBSP(); var nbspEntity; if (nbspChar.charCodeAt(0)==160) nbspEntity="&nbsp;"; else nbspEntity=nbspChar;}
 catch(e) { var nbspChar=String.fromCharCode(160); var nbspEntity="&nbsp;";}
 
  var Ts=new Date().getTime();
  var TimeStr=0;

//~~~~~~~~~~ Регулярные выражения ~~~~~~~~~~~~

/*  // удаление жирности и курсива
 var re110 = new RegExp("</?(STRONG|EM)>","gi");
 var re110_ = "";
 var count_110 = 0; */
 
 // удаление жирного из подзаголовков:
  var re111 = new RegExp("</?(STRONG)>","gi");
 var re111_ = "";
 var count_111 = 0;
 
  // удаление основного курсива из подзаголовков:
  var re112 = new RegExp("^<EM>([^>]+?)</EM>$","gi");
 var re112_ = "$1";
 var count_112 = 0;
 
 
//~~~~~~~~~~~~~~ Конец шаблонов ~~~~~~~~~~~~~~~~~~

 var id;
 var s="";

 // функция, обрабатывающая абзац P
 function HandleP(ptr) {

  s=ptr.innerHTML;
  
  // if (alter == true) {
 // во всех подзаголовках:
  // if (s.search(re110)!=-1 && ptr.className=="subtitle")         { s=s.replace(re110, re110_); count_110++}
  if (s.search(re111)!=-1 && ptr.className=="subtitle")         { s=s.replace(re111, re111_); count_111++}
  if (s.search(re112)!=-1 && ptr.className=="subtitle")         { s=s.replace(re112, re112_); count_112++}
  // }
  // else
  // {
 // кроме подзаголовков в стихах и цитатах:
  // if (s.search(re110)!=-1 && ptr.className=="subtitle" && ptr.parentNode.className!=="cite" && ptr.parentNode.className!=="poem" && ptr.parentNode.className!=="stanza")         { s=s.replace(re110, re110_); count_110++}
  // }
  
   ptr.innerHTML=s;      
  } 

    window.external.BeginUndoUnit(document,"«Удаление жирности и курсива в подзаголовках»");                               // ОТКАТ (UNDO) начало

 var body=document.getElementById("fbw_body");
 var ptr=body;
 var ProcessingEnding=false;
 while (!ProcessingEnding && ptr) {
  SaveNext=ptr;
  if (SaveNext.firstChild!=null && SaveNext.nodeName!="P" && 
      !(SaveNext.nodeName=="DIV" && 
        ((SaveNext.className=="history" && !ObrabotkaHistory) || 
         (SaveNext.className=="annotation" && !ObrabotkaAnnotation))))
  {    SaveNext=SaveNext.firstChild; }                                                         // либо углубляемся...

  else {
    while (SaveNext.nextSibling==null)  {
     SaveNext=SaveNext.parentNode;                                                           // ...либо поднимаемся (если уже сходили вглубь)
                                                                                                                // поднявшись до элемента P, не забудем поменять флаг
     if (SaveNext==body) {ProcessingEnding=true;}
                                                         }
   SaveNext=SaveNext.nextSibling; //и переходим на соседний элемент
         }
  if (ptr.nodeName=="P") HandleP(ptr);
  ptr=SaveNext;
 }

    window.external.EndUndoUnit(document);                                             // undo конец

var Tf=new Date().getTime();
var Thour = Math.floor((Tf-Ts)/3600000);
var Tmin  = Math.floor((Tf-Ts)/60000-Thour*60);
var Tsec = Math.ceil((Tf-Ts)/1000-Tmin*60-Thour*3600);
var Tsec1 = Math.ceil(10*((Tf-Ts)/1000-Tmin*60))/10;
var Tsec2 = Math.ceil(100*((Tf-Ts)/1000-Tmin*60))/100;
var Tsec3 = Math.ceil(1000*((Tf-Ts)/1000-Tmin*60))/1000;

           if (Tsec3<1 && Tmin<1)    TimeStr=Tsec3+ " сек"
 else { if (Tsec2<10 && Tmin<1)   TimeStr=Tsec2+ " сек"
 else { if (Tsec1<30 && Tmin<1)   TimeStr=Tsec1+ " сек"
 else { if (Tmin<1)                       TimeStr=Tsec+ " сек" 
 else { if (Tmin>=1 && Thour<1)   TimeStr=Tmin+ " мин " +Tsec+ " с"
 else { if (Thour>=1)                    TimeStr=Thour+ " ч " +Tmin+ " мин " +Tsec+ " с" }}}}}

// вывод статистики, если она, конечно, есть
 var st2="";
 if (count_111!=0 || count_111!=0 )   {st2+='\n Статистика'}
 // if (count_110!=0)   {st2+='\n• обработано подзаголовков:                             	'+count_110;}
 if (count_111!=0)   {st2+='\n• обработано жирных подзаголовков:              	'+count_111;}
 if (count_112!=0)   {st2+='\n• обработано курсивных подзаголовков:        	'+count_112;}
 if (st2!="") st2="\n"+st2;

 MsgBox ('                  –= Jurgen Script =– \n'+
        ' "Удаление жирности и курсива в подзаголовках" v.'+VersionNumber+' stokber Edition 2024       \n\n'+
        ' Время: ' +TimeStr+'.'+st2); 

} 
