/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Добавить последнее примечание, введя его текст в окошке",
  "version": "2.8",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Создаёт последнее примечание и ссылку в позиции курсора, если ниже нет других ссылок на примечания.",
  "description": "Вставляет ссылку в тексте и создаёт раздел в body notes; при необходимости создаёт этот body. Проверяет, что после новой ссылки нет других ссылок на примечания, и добавляет раздел в конец без перенумерации существующих. Текст запрашивается в отдельном окне. После вставки остаётся на ссылке.",
  "scope": "Точка вставки у курсора и связанные структуры во всей книге.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор после последней ссылки на примечание и запустите скрипт.",
    "Введите текст примечания в появившемся окне; отмена ввода может оставить частичные изменения.",
    "Проверьте новый раздел и связанные ссылки, особенно если в книге уже есть примечания."
  ],
  "warnings": [
    "Если после места вставки есть другая ссылка на примечание, скрипт прекращает работу.",
    "При исключении во время применения или перенумерации возможен частичный результат; его можно отменить одной операцией. При отказе DOM/COM в самой очистке возможно сохранение созданной разметки."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "09443abfa8eba73a33069b21e56304a15502ad440a5f5ecdb44f3312f898c4cb",
    "reviewStatus": "generated"
  },
  "tags": [
    "Примечания",
    "сноски",
    "параметры"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Штатное создание отменяется одним Ctrl+Z и возвращается Ctrl+Y. Отмена ввода и отказы проверок не создают операцию отмены; при исключении своя группа завершается без автоматического отката."
  },
  "settings": [
    {
      "name": "strConst1",
      "value": "n_%N",
      "expression": "\"n_%N\"",
      "description": "ID создаваемого раздела; %N заменяется номером.",
      "title": "Шаблон ID примечания",
      "source": "script"
    },
    {
      "name": "strConst2",
      "value": "%N",
      "expression": "\"%N\"",
      "description": "Текст заголовка нового раздела; %N заменяется номером.",
      "title": "Заголовок примечания",
      "source": "script"
    },
    {
      "name": "strConst3",
      "value": "",
      "expression": "\"\"",
      "description": "Шаблон содержимого без запроса текста; пустая строка создаёт пустой абзац.",
      "title": "Содержимое примечания",
      "source": "script"
    },
    {
      "name": "strConst4",
      "value": "[%N]",
      "expression": "\"[%N]\"",
      "description": "Разметка ссылки на примечание; %N заменяется номером.",
      "title": "Текст ссылки",
      "source": "script"
    },
    {
      "name": "strConst5",
      "value": "Примечания",
      "expression": "\"Примечания\"",
      "description": "Заголовок body notes, если его нужно создать.",
      "title": "Заголовок раздела примечаний",
      "source": "script"
    },
    {
      "name": "strConst6",
      "value": "notes",
      "expression": "\"notes\"",
      "description": "Значение fbname для body notes.",
      "title": "Имя раздела примечаний",
      "source": "script"
    },
    {
      "name": "addSnoska",
      "value": true,
      "expression": "true",
      "description": "При false упорядочивает существующие разделы без вставки новой ссылки.",
      "title": "Добавлять примечание",
      "source": "script"
    },
    {
      "name": "InputSnoskaText",
      "value": true,
      "expression": "true",
      "description": "При true открывает ввод текста; при false использует шаблон содержимого.",
      "title": "Запрашивать текст примечания",
      "source": "script"
    },
    {
      "name": "MoveFocusToNote",
      "value": false,
      "expression": "false",
      "description": "При true переводит видимую позицию к новому разделу; при false остаётся на ссылке.",
      "title": "Переходить к новому примечанию",
      "source": "script"
    },
    {
      "name": "lastSnoskaMode",
      "value": true,
      "expression": "true",
      "description": "При true допускает вставку только после последней существующей ссылки этого типа.",
      "title": "Добавлять в конец списка",
      "source": "script"
    },
    {
      "name": "forsazh",
      "value": false,
      "expression": "false",
      "description": "Отключает перенумерацию, использует случайный ID; проверки безопасной вставки сохраняются.",
      "title": "Ускоренный режим",
      "source": "script"
    }
  ]
}
</FBE-SCRIPT-CARD>*/
// добавление сноски между уже имеющимися
// написал Sclex
// версия 2.8

function Run () {
 var Ts=new Date().getTime();
 var commentRegExp=new RegExp("^c_",""); 
 //НАСТРОЙКИ - начало
 //здесь шаблоны, которые используются при работе скрипта
 //используйте макрос %N для указания номера примечания
 var strConst1="n_%N"; //шаблон ID разделов примечаний
 var strConst2="%N";//шаблон заголовка примечания
 var strConst3="";//шаблон содержания вставленного примечания
 var strConst4="[%N]"; //шаблон текста ссылки
 var strConst5="Примечания"; //шаблон заголовка боди нотесов
 var strConst6="notes"; //значение атрибута name у body примечаний
 var strConst7="Note adding";
 //функция, которая определяет, является ли элемент el, который
 //заведомо является ссылкой, ссылкой на тот тип примечаний,
 //который обрабатывается данным скриптом
 var isItNote=function (el) {return el.className=="note" ? true : false};
 var makeNoteFromHref=function (el) {el.className="note"};
 // выводить ли окно, извещающее о конце работы скрипта?
 // true - выводить. false - не выводить
 var EndWindow=true;
 //добавлять ли новую сноску или только провести работы по упорядочению существующих
 //true - добавлять. false - не добавлять
 var addSnoska=true;
 //показывать ли форму для ввода текста примечания
 //true - показывать. false - не показывать
 var InputSnoskaText=true;
 //перемещать ли фокус видимости на раздел свежесозданного примечания
 var MoveFocusToNote=false;
 //true, если это скрипт добавления последней сноски
 var lastSnoskaMode=true;
 //режим ускоренной работы
 var forsazh=false;
 //НАСТРОЙКИ - конец
 
 function PoShablonu(s,n) {
   var ttt3 = new RegExp("\%N");
   return(s.replace(ttt3,n));
 }
 
 function getLocalHref(name) {
   var i=1;
   var name1=name;
   var thg2=new RegExp("#");
   if (name1.search(thg2)==-1) {return(-1)} //ссылка не может начинаться с 1
   var thg=new RegExp("main\.html\#","i");
   var srch10=name1.search(thg);
   if (srch10==-1) {
    name1 = name1.substring(1,name1.length);
   } else {
    name1 = name1.substring(srch10+10,name1.length);
   }
   return(name1);
 } 
 
 function miniValidate(s) {
 //возвращаемые значения:
 //1 - недопустимый символ в имени тэга
 //2 - неразрешенное имя тэга
 //3 - закрывающий тэг не соответствует открывающему
 //4 - незакрытый тэг (нет правой угловой скобки)
 //5 - для открытого тэга нет соответствующего закрывающего
  var i=0;
  var InsideTag=false;
  var TagStek="";
  while (i<s.length) {
   var ch=s.substring(i,i+1);
   if (InsideTag) {
     var ffg=new RegExp("[/a-z]","gi");
     if (ch=="<") {return 4}
     if (ch.search(ffg)!=-1) {
       TagName=TagName+ch;
     } else if (ch==">") {
       InsideTag=false;
       if (TagName!="STRONG" && TagName!="EM" && TagName!="P" && TagName!="/P" &&
           TagName!="/STRONG" && TagName!="/EM" && TagName!="SUP" && TagName!="SUB"
           && TagName!="/SUP" && TagName!="/SUB") {return 2}
       if (TagName=="STRONG") {TagStek=TagStek+"1"}
       if (TagName=="EM") {TagStek=TagStek+"2"}
       if (TagName=="SUP") {TagStek=TagStek+"3"}
       if (TagName=="SUB") {TagStek=TagStek+"4"}
       if (TagName.substr(0,1)=="/") {
         var TagCodeFromStek=TagStek.substr(TagStek.length-1,1);
 //        MsgBox("TagCodeFromStek:"+TagCodeFromStek+"\nTagName:"+TagName);
         if (TagName=="/STRONG" && TagCodeFromStek!="1") {return 3}
         if (TagName=="/EM" && TagCodeFromStek!="2") {return 3}
         if (TagName=="/SUP" && TagCodeFromStek!="3") {return 3}
         if (TagName=="/SUB" && TagCodeFromStek!="4") {return 3}
         if (TagName=="/STRONG" || TagName=="/EM" || TagName=="/SUP" || TagName=="/SUB") {
          TagStek=TagStek.substr(0,TagStek.length-1);
         }
       }
     } else {return 1}
   }
   if (!InsideTag && ch=="<") {
    InsideTag = true;
    var TagName="";
   }
   i++;
  }
  if (InsideTag) {return 4}
  if (TagStek!="") {return 5}
  return 0;
 }
 
 function makeGoodSnoska (snoskaTextSrc) {
    snoskaText="<P>"+snoskaTextSrc+"</P>";
    var ffg1=new RegExp("<b>","gi");
    snoskaText=snoskaText.replace(ffg1,"<STRONG>");
    ffg1=new RegExp("</b>","gi");
    snoskaText=snoskaText.replace(ffg1,"</STRONG>");
    ffg1=new RegExp("<i>","gi");
    snoskaText=snoskaText.replace(ffg1,"<EM>");
    ffg1=new RegExp("</i>","gi");
    snoskaText=snoskaText.replace(ffg1,"</EM>");
    ffg1=new RegExp("<emphasis>","gi");
    snoskaText=snoskaText.replace(ffg1,"<EM>");
    ffg1=new RegExp("</emphasis>","gi");
    snoskaText=snoskaText.replace(ffg1,"</EM>");
    ffg1=new RegExp("<strong>","gi");
    snoskaText=snoskaText.replace(ffg1,"<STRONG>");
    ffg1=new RegExp("</strong>","gi");
    snoskaText=snoskaText.replace(ffg1,"</STRONG>");
    ffg1=new RegExp("<sup>","gi");
    snoskaText=snoskaText.replace(ffg1,"<SUP>");
    ffg1=new RegExp("</sup>","gi");
    snoskaText=snoskaText.replace(ffg1,"</SUP>");
    ffg1=new RegExp("<sub>","gi");
    snoskaText=snoskaText.replace(ffg1,"<SUB>");
    ffg1=new RegExp("</sub>","gi");
    snoskaText=snoskaText.replace(ffg1,"</SUB>");
    ffg1=new RegExp("<br>","gi");
    snoskaText=snoskaText.replace(ffg1,"</P>\n<P>");
    return snoskaText;
 }
 
 function getRandomNum(n) {
  var s="";
  for (var i=1;i<=n;i++) {
    s=s+Math.floor(Math.random()*10);
  }
  return s;
 } 
 
 // Подготовка не изменяет книгу и не открывает Undo.
 if (typeof document=="undefined" || !document || !document.body || !document.selection) { MsgBox("Нет доступного документа книги."); return; }
 var body=document.getElementById("fbw_body");
 if (!body) { MsgBox("Ошибка. body не найден!"); return; }
 var range=null, parent=null;
 if (addSnoska) {
  try { range=document.selection.createRange().duplicate(); parent=range.parentElement(); }
  catch (rangeError) { MsgBox("Поставьте курсор в текст книги."); return; }
  var paragraph=null, ancestor=parent;
  while (ancestor && ancestor!==body) {
   if (ancestor.nodeName=="A" || ancestor.nodeName=="INPUT" || ancestor.nodeName=="TEXTAREA" ||
       (ancestor.className=="body" && (ancestor.getAttribute("fbname")=="notes" || ancestor.getAttribute("fbname")=="comments"))) {
    MsgBox("Ссылку нужно вставлять в текст книги вне ссылки и разделов примечаний/комментариев."); return;
   }
   if (ancestor.nodeName=="P") paragraph=ancestor;
   ancestor=ancestor.parentNode;
  }
  if (ancestor!==body || !paragraph) { MsgBox("Поставьте курсор в абзац книги."); return; }
 }
 var bodyNotes=null, child=body.firstChild;
 while (child) {
  if (child.nodeName=="DIV" && child.className=="body" && child.getAttribute("fbname")==strConst6) {
   if (bodyNotes) { MsgBox("Найдено несколько body одного типа. Исправьте структуру книги."); return; }
   bodyNotes=child;
  }
  child=child.nextSibling;
 }
 var sections=[], ids=[], idToIndex={}, sectionPlans=[], linkPlans=[], insertN=1, previous=-1, newSnoskaId="";
 function has(map,key) { return Object.prototype.hasOwnProperty.call(map,"$"+key); }
 function sectionIndex(id) {
  return has(idToIndex,id) ? idToIndex["$"+id] : -1;
 }
 if (bodyNotes) {
  var divs=bodyNotes.getElementsByTagName("DIV");
  for (var i=0; i<divs.length; i++) if (divs[i].className=="section") {
   if (divs[i].parentNode!==bodyNotes || !divs[i].id || sectionIndex(divs[i].id)>=0) {
    MsgBox("В body есть вложенные разделы, пустые или повторяющиеся ID. Исправьте структуру книги."); return;
   }
   idToIndex["$"+divs[i].id]=sections.length;
   sections.push(divs[i]); ids.push(divs[i].id);
  }
 }
 var links=[];
 for (i=0; i<document.links.length; i++) links.push(document.links[i]);
 for (i=0; i<links.length; i++) {
  var index=sectionIndex(getLocalHref(links[i].href)), note=isItNote(links[i]);
  if (note && index<0) { MsgBox("Ссылка не связана с разделом в body "+strConst6+". Исправьте ссылку."); return; }
  if (addSnoska) {
   var linkRange=document.body.createTextRange(); linkRange.moveToElementText(links[i]);
   var before=range.compareEndPoints("StartToEnd",linkRange)>=0;
   if (!before && range.compareEndPoints("EndToStart",linkRange)>0) {
    MsgBox("Выделение пересекает существующую ссылку. Поставьте курсор вне ссылки."); return;
   }
   if (note) {
    if (before) previous=index;
    else if (lastSnoskaMode) { MsgBox("Ошибка. Это получается не последняя сноска в документе!"); return; }
   }
  }
 }
 if (addSnoska && !forsazh) insertN=previous+2;
 var renumber=!forsazh && !lastSnoskaMode;
 for (i=0; i<sections.length; i++) {
  var number=i+1+(addSnoska && i+1>=insertN ? 1 : 0);
  sectionPlans.push({node:sections[i], id:renumber ? PoShablonu(strConst1,number) : ids[i],
   title:"<P>"+PoShablonu(strConst2,number)+"</P>"});
 }
 var finalIds=[];
 for (i=0; i<sectionPlans.length; i++) finalIds.push(sectionPlans[i].id);
 if (forsazh && addSnoska) {
  var randomAttempts=0;
  do {
   if (++randomAttempts>100) { MsgBox("Не удалось выбрать свободный ID сноски."); return; }
   newSnoskaId=PoShablonu(strConst1,getRandomNum(32));
  } while (document.getElementById(newSnoskaId));
  insertN="";
 } else if (addSnoska) newSnoskaId=PoShablonu(strConst1,insertN);
 if (addSnoska) finalIds.push(newSnoskaId);
 var all=document.getElementsByTagName("*"), occupiedIds={}, futureIds={};
 for (i=0; i<all.length; i++) if (all[i].id) {
  var key="$"+all[i].id;
  if (!has(occupiedIds,all[i].id)) occupiedIds[key]=[];
  occupiedIds[key].push(all[i]);
 }
 for (i=0; i<finalIds.length; i++) {
  var id=finalIds[i];
  if (!id) { MsgBox("Пустой ID сноски. Проверьте шаблон ID."); return; }
  if (has(futureIds,id)) { MsgBox("Шаблон создаёт повторяющиеся ID сносок."); return; }
  futureIds["$"+id]=true;
  if (has(occupiedIds,id)) {
   var matches=occupiedIds["$"+id];
   for (var j=0; j<matches.length; j++) {
    index=sectionIndex(matches[j].id);
    if (index<0 || sections[index]!==matches[j]) { MsgBox("Будущий ID сноски уже занят другим элементом книги."); return; }
   }
  }
 }
 for (i=0; i<ids.length; i++) {
  matches=occupiedIds["$"+ids[i]];
  if (!matches || matches.length!=1 || matches[0]!==sections[i]) { MsgBox("ID раздела сноски повторяется в книге."); return; }
 }
 if (renumber) for (i=0; i<links.length; i++) {
  index=sectionIndex(getLocalHref(links[i].href));
  if (index>=0) linkPlans.push({node:links[i], href:"#"+sectionPlans[index].id,
   html:PoShablonu(strConst4,index+1+(addSnoska && index+1>=insertN ? 1 : 0))});
 }
 // Ввод, нормализация и miniValidate полностью завершаются до первой мутации.
 var snoskaText="";
 if (addSnoska && InputSnoskaText) {
  var promptText="Текст "+(strConst6=="comments" ? "комментария" : "примечания")+
   ". Используйте <b>...</b> <i>...</i> <br> <strong>...</strong> <emphasis>...</emphasis> <em>...</em>";
  var entered="&nbsp;", code=0;
  do {
   entered=prompt(promptText,entered);
   if (entered==null) { range.select(); return; }
   snoskaText=makeGoodSnoska(entered);
   code=miniValidate(snoskaText);
   if (code) MsgBox("Ошибка разметки текста сноски ("+code+"). Исправьте текст или отмените ввод.");
  } while (code);
 }
 if (!addSnoska && !bodyNotes) return;
 // Новые элементы готовятся отсоединёнными: до Begin книга остаётся прежней.
 var ownedNodes=[], newBody=null, newSection=null, newLink=null, bodyTitle=null, titleBefore=null;
 if (!bodyNotes) {
  newBody=document.createElement("DIV"); ownedNodes.push(newBody);
  newBody.className="body"; newBody.setAttribute("fbname",strConst6);
  newBody.setAttribute("xlmns:l","http://www.w3.org/1999/xlink");
  newBody.setAttribute("xlmns:f","http://www.gribuser.ru/xml/fictionbook/2.0");
  newBody.innerHTML="<DIV class=title><P>"+strConst5+"</P></DIV>";
  bodyNotes=newBody;
 } else if (!forsazh) {
  var first=bodyNotes.firstChild;
  while (first && !(first.nodeName=="DIV" && (first.className=="title" || first.className=="epigraph" || first.className=="section"))) first=first.nextSibling;
  if (!first || first.className!="title") {
   bodyTitle=document.createElement("DIV"); bodyTitle.className="title";
   bodyTitle.innerHTML="<P>"+strConst5+"</P>"; titleBefore=first;
  }
 }
 for (i=0; i<sectionPlans.length && renumber; i++) {
  first=sectionPlans[i].node.firstChild;
  if (first && first.nodeName=="DIV" && first.className=="title") sectionPlans[i].titleNode=first;
  else {
   var title=document.createElement("DIV"); title.className="title";
   title.innerHTML=sectionPlans[i].title; sectionPlans[i].newTitle=title;
  }
 }
 var markerId="", serial=0, markerAttribute="data-fbe-note-create";
 if (addSnoska) {
  var occupied;
  do {
   markerId="fbe-note-create-"+(++serial); occupied=!!document.getElementById(markerId);
   if (has(futureIds,markerId)) occupied=true;
  } while (occupied);
  newSection=document.createElement("DIV"); ownedNodes.push(newSection);
  newSection.id=newSnoskaId; newSection.className="section";
  newSection.innerHTML="<DIV class=title><P>"+(forsazh ? "&nbsp;" : PoShablonu(strConst2,insertN))+"</P></DIV>"+
   (InputSnoskaText ? snoskaText : "<P>"+PoShablonu(strConst3,insertN)+"</P>");
  if (newBody) newBody.appendChild(newSection);
  var link=document.createElement("A"); link.id=markerId; link.setAttribute(markerAttribute,markerId);
  link.setAttribute("href","#"+newSnoskaId); makeNoteFromHref(link);
  link.innerHTML=PoShablonu(strConst4,forsazh ? "*" : insertN);
  var linkHTML=link.outerHTML;
 }
 var opened=false, failed=false, failure=null, cleanupFailed=false, cleanupFailure=null;
 function cleanupError(error) { if (!cleanupFailed) { cleanupFailed=true; cleanupFailure=error; } }
 function removeOwn(node) {
  var own=false;
  for (var i=0; i<ownedNodes.length; i++) if (ownedNodes[i]===node) own=true;
  if (own && node.parentNode) {
   try { node.parentNode.removeChild(node); }
   catch (removeError) { node.removeNode(true); }
  }
 }
 try {
  window.external.BeginUndoUnit(document,strConst7); opened=true;
  if (addSnoska) {
   // Свободный ID и признак вставки, затем только сохранённая ссылка.
   var pasteFailed=false, pasteFailure=null;
   try { range.pasteHTML(linkHTML); }
   catch (pasteError) { pasteFailed=true; pasteFailure=pasteError; }
   try {
    var inserted=document.getElementById(markerId);
    if (inserted && inserted.nodeName=="A" && inserted.getAttribute("id")===markerId && inserted.getAttribute(markerAttribute)===markerId) {
     newLink=inserted; ownedNodes.push(newLink);
    }
   } catch (captureError) { if (!pasteFailed) throw captureError; }
   if (pasteFailed) throw pasteFailure;
   if (!newLink) throw new Error("Созданная ссылка сноски не найдена.");
   if (newBody) body.appendChild(newBody);
   else if (!lastSnoskaMode && !forsazh && insertN<=sections.length) bodyNotes.insertBefore(newSection,sections[insertN-1]);
   else bodyNotes.appendChild(newSection);
  }
  if (bodyTitle) bodyNotes.insertBefore(bodyTitle,titleBefore || (newSection && newSection.parentNode===bodyNotes ? newSection : null));
  if (renumber) {
   for (i=0; i<sectionPlans.length; i++) {
    var plan=sectionPlans[i];
    if (plan.node.id!==plan.id) plan.node.id=plan.id;
   }
   for (i=0; i<linkPlans.length; i++) {
    plan=linkPlans[i];
    if (plan.node.getAttribute("href")!==plan.href) plan.node.setAttribute("href",plan.href);
    if (plan.node.innerHTML!==plan.html) plan.node.innerHTML=plan.html;
   }
   for (i=0; i<sectionPlans.length; i++) {
    plan=sectionPlans[i];
    if (plan.newTitle) plan.node.insertBefore(plan.newTitle,plan.node.firstChild);
    else if (plan.titleNode.innerHTML!==plan.title) plan.titleNode.innerHTML=plan.title;
   }
  }
  if (addSnoska && strConst3=="") window.external.inflateBlock(newSection.lastChild)=true;
  if (addSnoska && MoveFocusToNote) {
   var target=newSection.firstChild;
   while (target && target.nodeName!="P") target=target.nextSibling;
   if (target) GoTo(target);
   else if (newSection.firstChild) GoTo(newSection.firstChild);
  }
 } catch (error) { failed=true; failure=error; }
 finally {
  try {
  // Сохраняем завершённую пару для Ctrl+Z; незавершённую пару убираем по ссылкам.
  if (addSnoska) {
   var paired=false;
   try {
    paired=newLink && newLink.parentNode && newSection.parentNode && body.contains(newSection);
    if (failed && paired) {
     matches=0; var nodes=document.getElementsByTagName("*");
     for (i=0; i<nodes.length; i++) if (nodes[i].id===newSnoskaId) matches++;
     paired=matches==1 && getLocalHref(newLink.href)===newSnoskaId;
    }
   }
   catch (error) { cleanupError(error); }
   if (!paired) {
    try { removeOwn(newLink); } catch (error) { cleanupError(error); }
    try { removeOwn(newSection); } catch (error) { cleanupError(error); }
    try { removeOwn(newBody); } catch (error) { cleanupError(error); }
   }
   if (newLink) {
    try { if (newLink.id===markerId) newLink.removeAttribute("id"); } catch (error) { cleanupError(error); }
    try { if (newLink.getAttribute(markerAttribute)===markerId) newLink.removeAttribute(markerAttribute); } catch (error) { cleanupError(error); }
   }
  }
  } catch (error) { cleanupError(error); }
  finally {
  if (opened) {
   opened=false;
   try { window.external.EndUndoUnit(document); } catch (error) { cleanupError(error); }
  }
  }
 }
 if (failed) throw failure;
 if (cleanupFailed) throw cleanupFailure;
 var seconds=Math.ceil((new Date().getTime()-Ts)/100)/10;
 window.external.SetStatusBarText(addSnoska ? "Добавлена сноска № "+insertN+". Время работы скрипта: "+seconds+" с" : "Перенумерация сносок успешно завершена.");
}
