/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Разметить заголовки разделов либо подзаголовки, найденные по регэкспу",
  "version": "Не указана",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Размечает заголовки разделов либо подзаголовки, найденные по регэкспу.",
  "description": "Запускает HTML-инструмент, который анализирует строки по шаблону либо оглавлению и изменяет заголовки/секции. Внутренние функции применяют изменения к переданному документу FBE. Это не неизменяющий документ поисковый диалог.",
  "scope": "Область определяется условиями и обходом DOM в данном скрипте.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Сохраняйте структуру Scripts/HTML и связанные файлы; один JS может быть недостаточен для работы.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже.",
    "Просмотрите результат до сохранения. Для структурной правки проверьте книгу и связанные ссылки."
  ],
  "warnings": [
    "Скрипт предусматривает изменение книги. Перед массовой обработкой сохраните отдельную копию."
  ],
  "dependencies": [
    {
      "path": "HTML/Разметить заголовки разделов либо подзаголовки, найденные по регэкспу - задание параметров.htm",
      "sha256": "5a1f8d440d4dbc29b189218da8ec180f76444047b3bfb0fb8d194eb276a1e8ef",
      "optional": true
    }
  ],
  "dependenciesComplete": false,
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
    "description": "Прямых Undo-вызовов нет; действия могут выполняться связанным HTML/JS. Гарантия отмены не установлена."
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
