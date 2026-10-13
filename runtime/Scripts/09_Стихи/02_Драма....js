/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Драма",
  "version": "1.2",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Драма.",
  "description": "Размечает имена говорящих в пьесе в начале абзацев: спрашивает допустимую длину до точки/скобки, проверяет строку функцией HandleP и добавляет соответствующее оформление. Это эвристика по тексту, не полноценный анализ драматургической структуры.",
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
    "sha256": "15d0c198cf037b62933a35d8b7d5e706f1335a3c541a7bce2080f5dd2a160a82",
    "reviewStatus": "generated"
  },
  "tags": [
    "Стихи",
    "параметры"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
  },
  "settings": [
    {
      "name": "ObrabotkaHistory",
      "value": false,
      "expression": "false",
      "description": "обрабатывать ли history",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "ObrabotkaAnnotation",
      "value": false,
      "expression": "false",
      "description": "обрабатывать ли annotation",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "strongRegExp_",
      "value": "",
      "expression": "\"\"",
      "description": "var emptyTagRegExp=new RegExp(\"<([-a-z]*)( [^>]*)?></$1>\",\"gi\");",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    },
    {
      "name": "limit",
      "value": 10,
      "expression": "10",
      "description": "var emptyTagRegExp_=\"\";",
      "valueMeaning": "Значение литерала/резервное значение в исходнике, не сохранённая настройка пользователя."
    }
  ]
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Разметка имен говорящих в пьесах"
// v.1.2 от 20.03.2010
//                                                 Автор Sclex

//обрабатывать ли history
var ObrabotkaHistory=false;
//обрабатывать ли annotation
var ObrabotkaAnnotation=false;

function Run() {
 var versionStr="1.2";

 try { var nbspChar=window.external.GetNBSP(); var nbspEntity; if (nbspChar.charCodeAt(0)==160) nbspEntity="&nbsp;"; else nbspEntity=nbspChar;}
 catch(e) { var nbspChar=String.fromCharCode(160); var nbspEntity="&nbsp;";}

 var deleteSpacing=true; 
 var s,m1,count=0,i,ch,myTagName,wholeTag,whileFlag,accumStr,prevCh,closing,chCnt;
 var level,flag1,s2,s3,lev,tagCnt,tagsOnEndRegExp,tagsOnEndRegExp_,accumTags;
 var bigLetterRegExp=new RegExp("[А-ЯЁA-Z(]","");
 var strongRegExp=new RegExp("<STRONG>|</STRONG>","gi");
 var strongRegExp_="";
 //var emptyTagRegExp=new RegExp("<([-a-z]*)( [^>]*)?></$1>","gi");
 //var emptyTagRegExp_="";
 var coll=new Object();
 var limit=10;

 // функция, обрабатывающая абзац P
 function HandleP(ptr) {
  if (ptr.parentNode.className!="section" || ptr.className=="subtitle") return;
  s=ptr.innerHTML;
  i=0;
  whileFlag=true;
  accumStr="";
  prevCh="";
  chCnt=0;
  level=0;
  flag1=false;
  tagCnt=false;
  accumTags="";
  while (i<s.length && whileFlag) {
   ch=s.substring(i,i+1);
   if (ch=="<") {
    //эта ветка выполняется, если наткнулись на тэг
    closing=false;
    i++;
    myTagName="";
    wholeTag="<";
    //прочитаем тэг до закрывающей угловой скобки или до пробела
    ch=s.substring(i,i+1);
    if (ch=="/") {
     closing=true;
     wholeTag+=ch;
     i++;
     ch=s.substring(i,i+1);
    }
    while (ch!=">" && ch!=" ") {
     myTagName+=ch;
     i++;
     ch=s.substring(i,i+1);     
    }
    wholeTag+=myTagName;
    //прочитаем остаток тэга
    while (ch!=">") {
     wholeTag+=ch;
     i++;
     ch=s.substring(i,i+1);     
    }
    wholeTag+=">";
    i++;
    accumTags+=wholeTag;
    if (closing) level--
    else {
     level++;
     coll["tagName_"+level]=myTagName;
     coll["wholeTag_"+level]=wholeTag;
    }
    if (myTagName!="STRONG" && !closing) tagCnt++;
   } else {
    // эта ветка выполняется, если встреченный символ - не открывающая угловая скобка
    // то есть когда нужно обработать простой символ текста
    if (ch=="!" || ch=="?" || ch=="…") return;
    if (deleteSpacing && ch.search(bigLetterRegExp)>=0 && (prevCh==" " || prevCh==nbspChar)) accumStr+=prevCh;        
    accumStr+=accumTags;
    accumTags=""; 
    if ((prevCh=="." || ch=="(") && chCnt>0) {
     if (ch=="(" && chCnt>limit) return;
     s2="";
     s3="";
     tagsOnEndRegExp=new RegExp("(<[^>]*?>){"+tagCnt.toString()+"}$","i");     
     tagsOnEndRegExp_="";     
     s2=accumStr.replace(tagsOnEndRegExp,tagsOnEndRegExp_);
     for (lev=level-tagCnt;lev>0;lev--)
      s2+="</"+coll["tagName_"+lev]+">";     
     for (lev=level;lev>0;lev--)
      s3=coll["wholeTag_"+lev]+s3;
     s2=s2.replace(strongRegExp,strongRegExp_);
     //s2=s2.replace(emptyTagRegExp,emptyTagRegExp_);
     s2="<STRONG>"+s2+"</STRONG>";
//     alert("tagCnt:"+tagCnt+"\ns:\n"+s+"\ns2+s3+s.substr(i):\n"+s2+s3+s.substr(i)+"\ns2:\n"+s2+"\naccumStr:\n"+'"'+accumStr+'"');
     ptr.innerHTML=s2+s3+s.substr(i);
     return;
    }
    else if (ch!=" " && ch!=nbspChar) {
     accumStr+=ch;
     chCnt++;     
    } else if (!deleteSpacing) accumStr+=ch;    
    prevCh=ch;
    i++;
    if (chCnt>limit+1) return;
    tagCnt=0;        
   }
  }
 }

 function obhod() {
  var body=document.getElementById("fbw_body");
  var ptr=body;
  var ProcessingEnding=false;
  while (!ProcessingEnding && ptr) {
   SaveNext=ptr;
   if (SaveNext.firstChild!=null && SaveNext.nodeName!="P" &&
       !(SaveNext.nodeName=="DIV" &&
         ((SaveNext.className=="history" && !ObrabotkaHistory) ||
          (SaveNext.className=="annotation" && !ObrabotkaAnnotation)))) {
     SaveNext=SaveNext.firstChild; // либо углубляемся...
   } else {
     while (SaveNext.nextSibling==null) {
      SaveNext=SaveNext.parentNode; // ...либо поднимаемся (если уже сходили вглубь)
      // поднявшись до элемента P, не забудем поменять флаг
      if (SaveNext==body) {ProcessingEnding=true;}
     }
    SaveNext=SaveNext.nextSibling; //и переходим на соседний элемент
   }
   if (ptr.nodeName=="P") HandleP(ptr);
   ptr=SaveNext;
  }
 }

 window.external.BeginUndoUnit(document,"Разметка имен говорящих в пьесах");
 limit=prompt("Сколько символов может быть до точки или скобки?",limit);
 deleteSpacing=AskYesNo("Удалять разрядку?");
 if (!limit) return;
 try {
  limit=eval(limit);
 }
 catch (e) {return} 
 obhod();
 window.external.EndUndoUnit(document);
 MsgBox('           –= Sclex Script =– \n'+
    '    «Действующие лица в пьесах»\n'+
    '                      v.'+versionStr+'\n'+
    '   ...............................................   \n\n'+
    '         Обработка завершена.');
}
