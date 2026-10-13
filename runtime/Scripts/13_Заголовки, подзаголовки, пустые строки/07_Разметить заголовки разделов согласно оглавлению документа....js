/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Разметить заголовки разделов согласно оглавлению документа",
  "version": "Не указана",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Размечает заголовки разделов согласно оглавлению документа.",
  "description": "Запускает HTML-инструмент, который анализирует строки по шаблону либо оглавлению и изменяет заголовки/секции. Внутренние функции применяют изменения к переданному документу FBE. Это не неизменяющий документ поисковый диалог.",
  "scope": "Текущая книга или определённые кодом её разделы.",
  "modifiesDocument": true,
  "requiresSelection": true,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Подготовьте выделение, описанное в scope и условиях запуска; некоторые команды работают с целыми абзацами.",
    "Сохраняйте структуру Scripts/HTML и связанные файлы; один JS может быть недостаточен для работы.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже.",
    "Просмотрите результат до сохранения. Для структурной правки проверьте книгу и связанные ссылки."
  ],
  "warnings": [
    "Скрипт предусматривает изменение книги. Перед массовой обработкой сохраните отдельную копию."
  ],
  "dependencies": [
    {
      "path": "HTML/Разметить заголовки разделов согласно оглавлению документа - основной диалог.htm",
      "sha256": "cb190b82f21462da4b74f526b86888d5c4cba0533829255892f164dd7042f1f7",
      "optional": true
    }
  ],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "ba59dbb2b23cb0d5828019bbe4491349b92b8af5cbbeb899fc9687c267f33cce",
    "reviewStatus": "generated"
  },
  "tags": [
    "Заголовки, подзаголовки, пустые строки"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
  }
}
</FBE-SCRIPT-CARD>*/
//Скрипт «Разметить заголовки разделов согласно оглавлению документа»
//Автор Sclex

var markupTitlesByContents_versionNum="4.4";

function myBeginUndoUnit(s) {
 window.external.BeginUndoUnit(document,s);
}
 
function myEndUndoUnit() {
  window.external.EndUndoUnit(document);
}

function Run() {

 try { var nbspChar=window.external.GetNBSP(); var nbspEntity; if (nbspChar.charCodeAt(0)==160) nbspEntity="&nbsp;"; else nbspEntity=nbspChar; }
 catch(e) { var nbspChar=String.fromCharCode(160); var nbspEntity="&nbsp;";};
 
 // Нижеследующая команда задает список необычных пробелов,
 // которые в заголовках должны обрабатываться наравне
 // с обычными пробелами:
 var unusualSpaces=String.fromCharCode(160)+
  String.fromCharCode(8194)+
  String.fromCharCode(8195)+
  String.fromCharCode(8196)+
  String.fromCharCode(8197)+
  String.fromCharCode(8198)+
  String.fromCharCode(8239)+
  String.fromCharCode(8201)+
  String.fromCharCode(8202)+
  nbspChar;
  
 var dialogWidth="700px";
 var dialogHeight="1000px";
 var fbwBody=document.getElementById("fbw_body"); 
 
 var coll={};
 coll["fbwBody"]=fbwBody;
 coll["mainDocument"]=document;
 coll["window"]=window;
 coll["versionNum"]=markupTitlesByContents_versionNum;
 coll["unusualSpaces"]=unusualSpaces;
 coll["nbspChar"]=nbspChar;
 coll["nbspEntity"]=nbspEntity;
 if (!document.selection || document.selection.type!="Text") {
   MsgBox("Вы ничего не выделили.\n\n"+
          "Перед запуском данного скрипта, пожалуйста, выделите абзацы оглавления.");
   return;
 }
 var myRange=document.selection.createRange();
 //myRange.expand("sentence");
 coll["mySelection"]=myRange;
 var modes=window.showModalDialog("HTML/Разметить заголовки разделов согласно оглавлению документа - основной диалог.htm",coll,
     "dialogHeight: "+dialogHeight+"; dialogWidth: "+dialogWidth+"; "+
     "center: Yes; help: No; resizable: Yes; status: No;");
}
