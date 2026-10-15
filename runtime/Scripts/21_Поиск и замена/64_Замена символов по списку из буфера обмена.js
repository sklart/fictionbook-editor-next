/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Замена символов по списку из буфера обмена",
  "version": "1.0",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    },
    {
      "name": "Sclex",
      "role": "original_author"
    },
    {
      "name": "stokber",
      "role": "modifier"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Заменяет символы по парам из текста в буфере обмена.",
  "description": "Заменяет символы в непустом выделении по буквальным парам из буфера обмена. Каждая непустая строка после удаления обычных пробелов должна содержать две кодовые единицы UTF-16. Поддерживаются CRLF, LF и CR. Замены выполняются последовательно с совместимым порядком правил.",
  "scope": "Непустое текстовое выделение в книге или многострочном текстовом поле. Однострочные поля ввода не обрабатываются.",
  "modifiesDocument": true,
  "requiresSelection": true,
  "usage": [
    "Скопируйте в буфер обмена список замен: в каждой непустой строке исходный и новый символ; обычные пробелы игнорируются.",
    "Выделите текст, в котором нужно выполнить замены. Замены идут последовательно по списку, поэтому результат одной может участвовать в следующей."
  ],
  "warnings": [
    "Повтор исходного символа заменяет предыдущее значение, сохраняя место первого правила. Цифровые исходные символы обрабатываются первыми по возрастанию, как в прежнем JScript.",
    "При ошибке возможны частичные изменения. Скрипт пытается удалить каждый свой маркер и завершить свою группу отмены; автоматический откат не выполняется. При отказе обоих способов удаления DOM временная разметка может сохраниться."
  ],
  "dependencies": [],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "a2cdab24a97864b881a3356aa3373f993aac724316e9da7e27f0da22f4e61c4a",
    "reviewStatus": "generated"
  },
  "tags": [
    "Поиск и замена",
    "буфер обмена",
    "параметры"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Изменения выделения книги и TEXTAREA отменяются одним действием Ctrl+Z и возвращаются Ctrl+Y. Частичные изменения при исключении автоматически не отменяются."
  },
  "settings": []
}
</FBE-SCRIPT-CARD>*/
//скрипт "Замена символов по списку из буфера обмена"  by Sclex 
// доработка до заявленного функционала - stokber.
//v1.0
// создан на базе скрипта замены регистра за авторством Sclex-а.

function Run() {

// --------------------------------------------------------------------
 var help = "\n\nСправка\n\nСкрипт производит замену любых символов (кроме пробелов) в выделенном тексте. Перед запуском скопируйте в буфер обмена список замен. Список представляет собой любое количество строк с парой символов — тот, который меняем и тот, на который меняем. Перед, после и между ними может находиться любое количество пробелов, включая их полное отсутствие. Пустые строки между таких строк замен игнорируются. В качестве списка замен может выступать любой скопированный текст из любой программы, включая FBE.\n\nПример строки замен:\nаА\nоО\n#+";
var rng=document.selection.createRange();
 if(rng.compareEndPoints("StartToEnd", rng)==0){ MsgBox("Выделите текст!\nСкрипт производит замену символов только в выделенном тексте!"+help); return false; }

 // Список остаётся данными: ровно две кодовые единицы UTF-16 в строке.
 var str = window.clipboardData.getData('Text');
 if (str == null) {
  alert("Буфер обмена пуст или в нем нет текстовых данных!"+help);
  return;
 }
 var lines = str.replace(/ +/g, "").replace(/\r\n|\r/g, "\n").split("\n");
 var sumbol = [];
 for (var lineIndex=0; lineIndex<lines.length; lineIndex++) {
  var line = lines[lineIndex];
  if (line.length==0) continue;
  if (line.length!=2) {
   alert("По меньшей мере одна строка в вашем списке из буфера обмена имеет меньше или больше чем два символа. Проверьте свой список."+help);
   return;
  }
  var from = line.charAt(0), to = line.charAt(1), previous = -1;
  for (var pairIndex=0; pairIndex<sumbol.length; pairIndex++) {
   if (sumbol[pairIndex].from==from) { previous=pairIndex; break; }
  }
  if (previous>=0) sumbol[previous].to=to;
  else sumbol.push({from:from, to:to, order:sumbol.length});
 }
 if (sumbol.length==0) {
  alert("Буфер обмена пуст или в нем нет текстовых данных!"+help);
  return;
 }
 // JScript перечислял собственные цифровые ключи объекта первыми, по возрастанию.
 // Остальные ключи сохраняют место первого появления; дубль меняет только значение.
 sumbol.sort(function(a,b) {
  var aDigit = a.from>="0" && a.from<="9";
  var bDigit = b.from>="0" && b.from<="9";
  if (aDigit && bDigit) return a.from.charCodeAt(0)-b.from.charCodeAt(0);
  if (aDigit!=bDigit) return aDigit ? -1 : 1;
  return a.order-b.order;
 });
// ========================================================

 //вид обработки, которой подвергается выделенный текст
 var ObrabotkaType=2;
 //имя тэга, который будет использован для маркеров начала и конца выделения
 var MyTagName="B";
 
// ----------------------------------------------------------------------------------
   function replacChar() {
   for (var pairIndex=0; pairIndex<sumbol.length; pairIndex++) {
    s = s.split(sumbol[pairIndex].from).join(sumbol[pairIndex].to);
   }
   }
// ================================================

 var tr;
 var errMsg="Нет выделения.\n\nПеред запуском скрипта нужно выделить текст, который будет обработан.";
 tr=document.selection.createRange();
 if (!tr) {
  MsgBox(errMsg);
  return;
 }
 if (tr.parentElement().nodeName=="INPUT") { MsgBox(errMsg); return; }
 var BlockStartNode=null, BlockEndNode=null, cursorPos=null;
 // Записи и ссылки принадлежат только этому запуску, ID заранее свободны.
 var markers=[], markerOwner={}, markerSequence=0;
 function insertMarker(range) {
  var id;
  do {
   if (++markerSequence>10000) throw new Error("Не удалось выделить ID маркера скрипта 64.");
   id="fbe-script64-"+markerSequence;
  } while (document.getElementById(id));
  var marker={id:id, node:null};
  markers.push(marker);
  // Даже если pasteHTML выбросит исключение после вставки, сохраним ссылку.
  var pasteFailed=false, pasteFailure=null;
  try {
   range.pasteHTML('<'+MyTagName+' id="'+id+'" data-fbe-script64="'+id+'"></'+MyTagName+'>');
  } catch (pasteError) { pasteFailed=true; pasteFailure=pasteError; }
  try {
   var node=document.getElementById(id);
   if (node && node.nodeName==MyTagName && node.getAttribute("id")==id &&
       node.getAttribute("data-fbe-script64")==id) {
    marker.node=node;
    node.__fbeScript64Owner=markerOwner;
   }
  } catch (captureError) { if (!pasteFailed) throw captureError; }
  if (pasteFailed) throw pasteFailure;
  if (!marker.node) throw new Error("Не удалось определить маркер скрипта 64.");
  return marker.node;
 }
 function ownsMarker(node) {
  for (var i=0; i<markers.length; i++) {
   if (markers[i].node===node && node.__fbeScript64Owner===markerOwner &&
       node.getAttribute("id")==markers[i].id) return true;
  }
  return false;
 }
 var undoOpened=false, failed=false, failure=null, cleanupFailed=false, cleanupFailure=null;
 function rememberCleanupError(error) {
  if (!cleanupFailed) { cleanupFailed=true; cleanupFailure=error; }
 }
 try {
 window.external.BeginUndoUnit(document,"Замена символов по списку из буфера обмена");
 undoOpened=true;
 if (tr.parentElement().nodeName=="TEXTAREA") {
  s=tr.text;
  replacChar();
  tr.text=s;
 }
 else if (tr.parentElement().nodeName!="INPUT") {
  var body=document.getElementById("fbw_body");
  var coll=tr.getClientRects();
  var ttr1 = body.document.selection.createRange();
  var el=body.document.elementFromPoint(coll[0].left, coll[0].top);
  cursorPos=null;
  if (tr.compareEndPoints("StartToEnd",tr)==0) {
   cursorPos=insertMarker(ttr1);
   ttr1.expand("word");
  } 
  // поставим маркеры блока в виде пустых ссылок
  tr=ttr1.duplicate();
  tr.collapse();
  BlockStartNode=insertMarker(tr);
  tr=ttr1.duplicate();
  tr.collapse(false);
  BlockEndNode=insertMarker(tr);
  // поднимаемся вверх по дереву, пока не найдем DIV или P,
  // в который входит начало выделения
  while (el && el.nodeName!="DIV" && el.nodeName!="P") { el=el.parentNode; }
  var InsideP = false; // true, если находимся внутри тэга P
  var InsideSelection = false; // true, когда текущая позиция внутри выделенного текста
  var ProcessingEnded=false; // true, когда обработка закончена и пора выходить
  var ptr=el;
  while (!ProcessingEnded && ptr) {
   // напомню: nodeType=1 для элемента (тэга) и nodeType=3 для текста
   // если встретили тэг P, меняем флаг, что мы внутри P
   if (ptr.nodeType==1 && ptr.nodeName=="P") {InsideP=true};
   // если встретили маркер начала блока, ...
   if (ptr===BlockStartNode && ownsMarker(ptr)) {
    // меняем флаг, т.к. попали внутрь выделения
    InsideSelection=true;
    // запомним ноду ссылки, чтобы потом удалить ее
    BlockStartNode=ptr;
   }
   // аналогично для маркера конца выделения
   if (ptr===BlockEndNode && ownsMarker(ptr)) {
    InsideSelection=false;
    ProcessingEnded=true;
    BlockEndNode=ptr;
   }
   // если нашли текст и находимся внутри P и внутри выделения...
   if (ptr.nodeType==3 && InsideP && InsideSelection) {
     // получаем текстовое содержимое узла
     var s=ptr.nodeValue;
     // обрабатываем как надо
     // if (ObrabotkaType==1);}
     if (ObrabotkaType==2) {replacChar();}
     // и возвращаем на место
     ptr.nodeValue=s;
   }
   if (ProcessingEnded) break;
   // теперь надо найти следующий по дереву узел для обработки
   if (ptr.firstChild!=null) {
     ptr=ptr.firstChild; // либо углубляемся...
   } else {
     while (ptr && ptr.nextSibling==null) {
      ptr=ptr.parentNode; // ...либо поднимаемся (если уже сходили вглубь)
      // поднявшись до элемента P, не забудем поменять флаг
      if (ptr && ptr.nodeType==1 && ptr.nodeName=="P") {InsideP=false}
     }
    if (ptr) ptr=ptr.nextSibling; //и переходим на соседний элемент
   }
  }
  // удаляем маркеры блока
  if (!ownsMarker(BlockStartNode) || !ownsMarker(BlockEndNode) ||
      (cursorPos && !ownsMarker(cursorPos))) throw new Error("Потерян маркер скрипта 64.");
  var tr1=document.body.createTextRange();
  if (!cursorPos) {
   tr1.moveToElementText(BlockStartNode);
   var tr2=document.body.createTextRange();
   tr2.moveToElementText(BlockEndNode);
   tr1.setEndPoint("StartToStart",tr2);
   tr1.select();
  } else {
   tr1.moveToElementText(cursorPos);
   tr1.select();
  }
 }
 } catch (error) {
  failed=true;
  failure=error;
 } finally {
  // Каждый собственный элемент очищается независимо. Чужие ID не трогаем.
  for (var markerIndex=0; markerIndex<markers.length; markerIndex++) {
   try {
    var markerNode=markers[markerIndex].node;
    if (markerNode && ownsMarker(markerNode) && markerNode.parentNode) {
     try { markerNode.parentNode.removeChild(markerNode); }
     catch (removeError) {
      // MSHTML: резервное удаление оболочки сохраняет возможных потомков.
      markerNode.removeNode(false);
     }
    }
   } catch (cleanupError) { rememberCleanupError(cleanupError); }
  }
  if (undoOpened) {
   undoOpened=false;
   try { window.external.EndUndoUnit(document); }
   catch (undoError) { rememberCleanupError(undoError); }
  }
 }
 // Ошибка обработки имеет приоритет над ошибкой очистки, без Ctrl+Z.
 if (failed) throw failure;
 if (cleanupFailed) throw cleanupFailure;
}
