/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Разметить курсивом буквы рядом со знаками подчеркивания",
  "version": "1.2",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Размечает курсивом буквы рядом со знаками подчеркивания.",
  "description": "Распознаёт последовательности букв рядом с подчёркиваниями регулярным выражением, убирает условные маркеры и добавляет курсивную разметку. Переписывает HTML подходящих абзацев; подчёркивание как буквальный символ может быть неоднозначно.",
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
    "sha256": "ecc4704c288100c1ff624aa0b8f51d0a066dd441efa7c681e881ea0145da3214",
    "reviewStatus": "generated"
  },
  "tags": [
    "Обработка форматирования",
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
function Run() {

var VersionNumber="1.2";

 //обрабатывать ли history
 var ObrabotkaHistory=true;
 //обрабатывать ли annotation
 var ObrabotkaAnnotation=true;

 try { var nbspChar=window.external.GetNBSP(); var nbspEntity; if (nbspChar.charCodeAt(0)==160) nbspEntity="&nbsp;"; else nbspEntity=nbspChar;}
 catch(e) { var nbspChar=String.fromCharCode(160); var nbspEntity="&nbsp;";}
 var nbspCharOrEntity="("+nbspChar+"|"+nbspEntity+")";
 var nbspCharOrEntityOrSpaces="("+nbspChar+"|"+nbspEntity+"| )";
 
 var codeToAddBefore="<EM>";
 var codeToAddAfter="</EM>";
 var shortWords="(без|в|до|для|за|из|к|на|над|о|об|от|по|под|пред|при|про|с|у|через|а|и|чтобы|если|или|ни|не|как|также|тоже|но|то|либо|раз|что|будто|едва|ибо|хотя|ах|ой|ба|да|фу|тьфу|эй|эх|я)";
 var regExp01=new RegExp("(«+|^—"+nbspCharOrEntityOrSpaces+"+|«…|«…"+nbspCharOrEntityOrSpaces+"+)?(_+[-а-яёa-z]|[-а-яёa-z]_+)+[-а-яёa-z]?(("+nbspCharOrEntityOrSpaces+"+("+shortWords+nbspCharOrEntityOrSpaces+"+)*(_+[-а-яёa-z]|[-а-яёa-z]_+)+[а-яёa-z]?)*_*([.,…?!»]+)?(?=[^>]*(<|$)))*","gi");

// if (PromptSnoska) {                                                                     // отключаю касательство к сноскам
//  Snoska=AskYesNo("    	      –=Jürgen Script=–\n\n	Обращать внимание [на] сноски?  		   \n");
// }

 var Ts=new Date().getTime();
 var TimeStr=0;

 
// М_ы _б

 var id;
 var s;
 var replaceCount=0;
 
 try { var nbspChar=window.external.GetNBSP(); var nbspEntity; if (nbspChar.charCodeAt(0)==160) nbspEntity="&nbsp;"; else nbspEntity=nbspChar; }
 catch(e) { var nbspChar=String.fromCharCode(160); var nbspEntity="&nbsp;";}

 function replaceFunction1(myMatch) {
  //alert(myMatch);
  replaceCount++;
  return codeToAddBefore+myMatch.replace(/_/g,"")+codeToAddAfter;
 }

 // функция, обрабатывающая абзац P
 function HandleP(ptr) {

  if (ptr.innerText.indexOf("_")==-1) return;

  ptr.innerHTML=ptr.innerHTML.replace(regExp01,replaceFunction1);
 }

 window.external.BeginUndoUnit(document,"разметку курсивом букв рядом со знаками подчеркивания");                               // ОТКАТ (UNDO) начало

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

 if (st2!="") st2="\n"+st2;

 MsgBox ('                  –= Sclex Script =– \n'+
        ' «Разметить курсивом буквы рядом со знаками подчеркивания» v.'+VersionNumber+'\n\n'+
        ' Добавлено тегов курсива (emphasis): '+replaceCount+'\n\n'+
        ' Время: ' +TimeStr+'.'+st2);


}
