/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Разметить заголовки разделов либо подзаголовки, найденные по регэкспу",
  "version": "4.9",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Открывает основной диалог для разметки заголовков разделов или подзаголовков по регулярному выражению.",
  "description": "Передаёт HTML-диалогу текущую книгу и таблицу нестандартных пробелов. В диалоге пользователь задаёт регулярное выражение и параметры захвата соседних абзацев; по команде диалог превращает совпавшие строки в заголовки разделов или подзаголовки. Разметку выполняет код обязательного HTML-файла; сама JS-оболочка только открывает окно.",
  "scope": "Разделы основного текста в fbw_body; код диалога пропускает тела сносок и комментариев. Выделение не требуется.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте книгу и запустите команду без выделения; дождитесь окна настройки.",
    "Задайте регулярное выражение, чувствительность к регистру и число соседних строк; в расширенном варианте проверьте дополнительные условия. Выберите разметку заголовков или подзаголовков и проверьте результат в книге."
  ],
  "warnings": [
    "Диалог применяет пользовательское регулярное выражение к абзацам книги и может захватывать соседние строки; перед массовой разметкой сохраните копию книги.",
    "Изменения выполняет отдельный HTML-диалог. При ошибке обработки надёжное завершение группы отмены не подтверждено.",
    "В основном диалоге проверки полей идут после начала группы отмены; неверный шаблон или отсутствие поля может прервать обработку с незавершённой группой."
  ],
  "dependencies": [
    {
      "path": "HTML/Разметить заголовки разделов либо подзаголовки, найденные по регэкспу - задание параметров.htm",
      "sha256": "5a1f8d440d4dbc29b189218da8ec180f76444047b3bfb0fb8d194eb276a1e8ef",
      "optional": false
    }
  ],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "3978abdec37f17a550d4b3d6783213ae7cb476b0c137392f4e6d807b997bfeab",
    "reviewStatus": "generated"
  },
  "tags": [
    "Заголовки, подзаголовки, пустые строки"
  ],
  "undo": {
    "status": "may_be_delegated",
    "description": "Изменения выполняет связанный диалог, который начинает и завершает группу отмены при штатной обработке. При ошибке или раннем выходе откат не подтверждён; проверьте книгу до сохранения."
  }
}
</FBE-SCRIPT-CARD>*/
//Скрипт «Разметить заголовки разделов либо подзаголовки, найденные по регэкспу»
//Автор Sclex

var markupTitlesOrSubtitles_versionNum="4.9";

function Run() {
 var dialogWidth="700px";
 var dialogHeight="335px";

 // Нижеследующая команда задает список необычных пробелов,
 // которые в заголовках и подзаголовках должны обрабатываться
 // наравне с обычными пробелами:
 var unusualSpaces=String.fromCharCode(8194)+
  String.fromCharCode(8195)+
  String.fromCharCode(8196)+
  String.fromCharCode(8197)+
  String.fromCharCode(8198)+
  String.fromCharCode(8239)+
  String.fromCharCode(8201)+
  String.fromCharCode(8202);

 var fbwBody=document.getElementById("fbw_body");
 var coll=new Object();
 coll["fbwBody"]=fbwBody;
 coll["mainDocument"]=document;
 coll["window"]=window;
 coll["unusualSpaces"]=unusualSpaces;
 coll["versionNum"]=markupTitlesOrSubtitles_versionNum;
 var modes=window.showModelessDialog("HTML/Разметить заголовки разделов либо подзаголовки, найденные по регэкспу - задание параметров.htm",coll,
     "dialogHeight: "+dialogHeight+"; dialogWidth: "+dialogWidth+"; "+
     "center: Yes; help: No; resizable: Yes; status: No;");
}
