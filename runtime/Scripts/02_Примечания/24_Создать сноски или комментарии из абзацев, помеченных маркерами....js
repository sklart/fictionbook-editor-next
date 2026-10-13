/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Создать сноски или комментарии из абзацев, помеченных маркерами",
  "version": "Не указана",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Создаёт сноски или комментарии из абзацев, помеченных маркерами.",
  "description": "Диалог получает документ/окно FBE и генерирует исполняемый код операций со сносками. Выбранные либо помеченные абзацы превращаются в разделы примечаний/комментариев и ссылки; настройки маркеров и форматирования задаются в связанном HTML. Изменения выполняются не в коротком JS-запускателе, а через его зависимость.",
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
      "path": "HTML/Создать сноски или комментарии из абзацев, помеченных маркерами - задание параметров.htm",
      "sha256": "4c74fa581625b72a4bc283dad4ee5bf69a6c32744ab9c745301e501cf8abecc1",
      "optional": true
    }
  ],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "940b3fc9f1e9322deda541f0742891e086b4d377d7fd48da628d65f1f1bc02c2",
    "reviewStatus": "generated"
  },
  "tags": [
    "Примечания",
    "сноски"
  ],
  "undo": {
    "status": "may_be_delegated",
    "description": "Прямых Undo-вызовов нет; действия могут выполняться связанным HTML/JS. Гарантия отмены не установлена."
  }
}
</FBE-SCRIPT-CARD>*/
//Скрипт «Создать сноски или комментарии из абзацев, помеченных маркерами»
//Автор Sclex

var notesFromSelectedParagraphs_versionNum="7.3";

function Run() {
 var dialogWidth="700px";
 var dialogHeight="570px";
 var fbwBody=document.getElementById("fbw_body");
 var coll=new Object();
 try { var nbspChar=window.external.GetNBSP(); }
 catch(e) { var nbspChar=String.fromCharCode(160); } 
 coll["fbwBody"]=fbwBody;
 coll["mainDocument"]=document;
 coll["window"]=window;
 coll["versionNum"]=notesFromSelectedParagraphs_versionNum;
 coll["nbspChar"]=nbspChar;
 var modes=window.showModelessDialog("HTML/Создать сноски или комментарии из абзацев, помеченных маркерами - задание параметров.htm",coll,
     "dialogHeight: "+dialogHeight+"; dialogWidth: "+dialogWidth+"; "+
     "center: Yes; help: No; resizable: Yes; status: No;");
}
