/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Создать эпиграф из полных абзацев",
  "version": "1.2",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Создаёт эпиграф из абзацев вокруг курсора или выделения.",
  "description": "Расширяет текущий диапазон до целых элементов, проверяет контейнер body, section или poem, создаёт DIV.epigraph из затронутых абзацев и удаляет их исходные P. Последний абзац может быть помечен как автор текста. Затем переводит курсор к концу нового эпиграфа. Даже пустое выделение может обрабатываться как полный абзац под курсором.",
  "scope": "Абзацы, затронутые курсором или выделением, внутри допустимого контейнера body, section либо poem.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Сохраните копию книги, поставьте курсор в нужный абзац или выделите нужные абзацы целиком.",
    "Запустите создание и проверьте состав нового эпиграфа, авторскую строку и соседние блоки."
  ],
  "warnings": [
    "Диапазон расширяется до целых элементов: соседний текст может войти в эпиграф даже при частичном или пустом выделении.",
    "Алгоритм определения строки автора сравнивает индекс массива с именем тега и может ошибочно пометить последний абзац как text-author.",
    "Операция закрывается при обычном завершении без прямых ранних выходов после её начала; при исключении гарантированное закрытие и полный откат не проверены."
  ],
  "dependencies": [],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "2e85bdc39986080936b5b9f78828ca0eba9602167b80513623e83683bedceeef",
    "reviewStatus": "generated"
  },
  "tags": [
    "Эпиграфы"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Создание и удаление исходных абзацев заключены в операцию отмены; прямых досрочных выходов внутри неё нет, но обработчика исключений для гарантированного завершения нет."
  }
}
</FBE-SCRIPT-CARD>*/
// Скрипт "Создать эпиграф из полных абзацев"
// Версия 1.2
// Автор Sclex

function Run(cp,check) {

  function GoToEndOfElement(elem) {
   if(!elem) return;
   var b=elem.getBoundingClientRect();
   if (b.bottom-b.top<=window.external.getViewHeight())
    window.scrollBy(0,(b.top+b.bottom-window.external.getViewHeight())/2);
   else
    window.scrollBy(0,b.top);
   var r=document.selection.createRange();
   if (!r || !("compareEndPoints" in r)) return;
   r.moveToElementText(elem);
   r.collapse(false);
   r.select();
  }

  var check=false;
  var tr=document.selection.createRange();
  var rngStart=tr.duplicate();
  rngStart.collapse(true);
  var startEl=rngStart.parentElement();
  var rngEnd=tr.duplicate();
  rngEnd.collapse(false);
  var endEl=rngEnd.parentElement();
  //alert("endEl: "+endEl.outerHTML);
  var rng2=document.body.createTextRange();
  rng2.moveToElementText(startEl);
  tr.setEndPoint("StartToStart",rng2);
  rng2=document.body.createTextRange();
  rng2.moveToElementText(endEl);
  tr.setEndPoint("EndToEnd",rng2);
  //tr.select();
  //return;
  
  var cp=tr.parentElement();
  
  cp = GetCP(cp);
  if(!cp) return;
  //if (!check) alert("cp: "+cp.outerHTML);

  if(cp.className != "body" && cp.className != "section" && cp.className != "poem") return;

  var pp=cp.firstChild;
  if(cp.className == "body") // different order
    pp = SkipOver(pp, "title", "image", "epigraph");
  else
    pp = SkipOver(pp, "title", "epigraph", null);

  if(check) return true;

  if (document.selection.type && document.selection.type=="Control") {
   MsgBox("Вы используете не тот тип выделения, с которым работает вставка эпиграфа. Выделяйте текст для будущего эпиграфа не кликом по картинке, а движением мыши слева направо или справа налево. Либо задайте выделение, используя клавиатуру.");
   return;
  }

  var rng = tr.duplicate();
  
  var txt = "";
  var pps;

  if(rng && rng.text != "")
  {
    var dpps = document.createElement("DIV");
    dpps.innerHTML = rng.htmlText;
    pps = dpps.getElementsByTagName("P");
    if(pps.length == 0) {
     dpps.innerHTML = "<P>"+rng.htmlText+"</P>";
     pps = dpps.getElementsByTagName("P");
     if(pps.length == 0) {
      txt = rng.text;
     }
    }
  }

  window.external.BeginUndoUnit(document,"создание эпиграфа из полных абзацев");
  var ep=document.createElement("DIV");
  ep.className="epigraph";
  if(txt != "")
  {
    var pwt = document.createElement("P");
    pwt.innerHTML = txt;
    ep.appendChild(pwt);
  }
  else if(pps && pps.length > 0)
  {
    var upTag = "";
    for(i = 0; i < pps.length; ++i)
    {
      var pwt = document.createElement("P");
      if(i == pps.length - 2
         && pps[i].children
         && pps[i].children.length
         && pps[i].children.length == 1)
      {
        upTag = pps[i].children[0].tagName;
      }
      if(i == pps.length - 1)
      {
        var pptext = pps[i].innerText;
        var pptags = new Array();
        var ppall = pps[i].all;
        var j = 0;

        for(k = 0; k < ppall.length; ++k)
        {
          if(ppall[k].innerText && ppall[k].innerText == pptext)
            pptags[j++] = ppall[k].tagName;
        }

	if (pptags.length > 0 && pps.length > 1)
	{
          for(pptag in pptags)
          {
            if(pptag != upTag)
            {
              pwt.className = "text-author";
              break;
            }
          }
	}
      }
      pwt.innerHTML = pps[i].innerHTML;
      ep.appendChild(pwt);
    }
  }
  else
    ep.appendChild(document.createElement("P"));

  var el=startEl;
  var psArr=[];
  psArr.push(el);
  while (el!=endEl) {
   if (el.firstChild)
    el=el.firstChild;
  else {
    while (el.nextSibling==null)
     el=el.parentNode;                                                 
    el=el.nextSibling; //и переходим на соседний элемент
   }
   if (el.nodeName=="P") psArr.push(el);
  }

  InsBefore(cp, pp, ep);
  InflateIt(ep);

  //tr.pasteHTML("");
  var el2, parentOfEl2;
  while (psArr.length>0) {
   el2=psArr.pop();
   parentOfEl2=el2.parentNode;
   if (el2 && (el2.nextSibling || el2.parentNode.className!="section")) {
    el2.removeNode(true);
   }
   else {
    el2.innerHTML="";
    InflateIt(el2);
   }
   if (parentOfEl2.nodeName=="DIV" && 
       (parentOfEl2.className=="title" || parentOfEl2.className=="epigraph" || parentOfEl2.className=="cite") &&
       !parentOfEl2.firstChild)
    parentOfEl2.removeNode(true);
  }
  //alert("Точка 3.");
  //if(pp && (!pp.innerText || pp.innerText == "" || pp.innerText == " ") && pp.className!="image")
  //  pp.removeNode(true);
  window.external.EndUndoUnit(document);

  GoToEndOfElement(ep);
}
