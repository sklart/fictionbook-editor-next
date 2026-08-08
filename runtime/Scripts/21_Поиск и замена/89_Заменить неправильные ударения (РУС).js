// Скрипт "Заменить неправильные ударения" (в кириллице)  для редактора FBE
// version 1.0
// Идея, регэкспы - TaKir

//  Основано на скрипте "Генеральная уборка" (версия 2.8)

// Скрипт предназначен для замены различных вариантов неправильных обозначений ударений в fb2 документах.
// Обрабатывается только кириллица.
// Во избежание ошибочных срабатываний замена производится только внутри слова (не первая и не последняя буквы).

// version 1.0, 27.04.2026
//======================================

var VersionNumber="1.0";

//обрабатывать ли history
var ObrabotkaHistory=true;
//обрабатывать ли annotation
var ObrabotkaAnnotation=true;
// обрабатывать ли сноски                       // отключаю касательство к сноскам
// var Snoska=false;
//  var PromptSnoska=true;

var sIB="<EM>|<STRONG>|<EM><STRONG>|<STRONG><EM>";
var fIB="</EM>|</STRONG>|</EM></STRONG>|</STRONG></EM>";
var aIB="<EM>|<STRONG>|<EM><STRONG>|<STRONG><EM>|</EM>|</STRONG>|</EM></STRONG>|</STRONG></EM>";

function Run() {

 try { var nbspChar=window.external.GetNBSP(); var nbspEntity; if (nbspChar.charCodeAt(0)==160) nbspEntity="&nbsp;"; else nbspEntity=nbspChar;}
 catch(e) { var nbspChar=String.fromCharCode(160); var nbspEntity="&nbsp;";}

  var Ts=new Date().getTime();
  var TimeStr=0;

//~~~~~~~~~~ Регулярные выражения ~~~~~~~~~~~~

// Замена ударений производится только внутри слова (не первая и не последняя буквы)!

// замена символа приватной зоны U+E098 на ударение для предшествующей гласной, только внутри слова (TaKir)
 var re01 = new RegExp("([аеёиоуыэюя])(?=[а-яё])","gi");
 var re01_ = "$1́";
 var count_01 = 0;


// замена ошибочно переданных ударений в виде цифры "2" после строчной гласной, только внутри слова (TaKir)
 var re02 = new RegExp("([аеёиоуыэюя])2(?=[а-яё])","gi");
 var re02_ = "$1$2́";
 var count_02 = 0;
 
 
// замена ошибочных ударений в виде апострофа после строчной гласной, только внутри слова (TaKir)
 var re03 = new RegExp("([а-яё])([аеёиоуыэюя])'(?=[а-яё])","gi");
 var re03_ = "$1$2́";
 var count_03 = 0;
 

// замена ошибочных ударений у'же ===> у́же (TaKir)
 var re04 = new RegExp("(у'же)","gi");
 var re04_ = "у́же";
 var count_04 = 0;


//~~~~~~~~~~~~~~ Конец шаблонов ~~~~~~~~~~~~~~~~~~


 var id;
 var s="";

 // функция, обрабатывающая абзац P
 function HandleP(ptr) {



  s=ptr.innerHTML;

	   if (s.search(re01)!=-1)       { count_01+=s.match(re01).length;s=s.replace(re01, re01_); }
	   if (s.search(re02)!=-1)       { count_02+=s.match(re02).length;s=s.replace(re02, re02_); }
	   if (s.search(re03)!=-1)       { count_03+=s.match(re03).length;s=s.replace(re03, re03_); }
	   if (s.search(re04)!=-1)       { count_04+=s.match(re04).length;s=s.replace(re04, re04_); }


   ptr.innerHTML=s;      
  } 

    window.external.BeginUndoUnit(document,"«Заменить неправильные ударения»");                               // ОТКАТ (UNDO) начало

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

 if (count_01!=0 || count_02!=0 || count_03!=0 || count_04!=0)   {st2+='Статистика:\n'}


 if (count_01!=0)   {st2+='\n• Замена символа U+E098 после гласной на гласную с ударением:	'+count_01;}
 if (count_02!=0)   {st2+='\n• Замена цифры 2 внутри слова после гласной на гласную с ударением:	'+count_02;}
 if (count_03!=0)   {st2+='\n• Замена ударений-апострофов после строчной гласной на гласную с ударением:	'+count_03;}
 if (count_04!=0)   {st2+='\n• Замена ударений уже ===> у́же:	'+count_04;}
 


 if (st2!="") st2="\n"+st2;

 MsgBox (' Заменить неправильные ударения (кириллица)\n ver. '+VersionNumber+'        \n'+


        st2+'\n\n Время выполнения: ' +TimeStr); 


}
