/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Создать сноски или комментарии из абзацев, помеченных знаками карет",
  "version": "Не указана",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Создаёт сноски или комментарии из абзацев, помеченных знаками карет.",
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
      "path": "HTML/Создать сноски или комментарии из абзацев, помеченных знаками карет - задание параметров.htm",
      "sha256": "372bfab2e06f82de76540e83f402b32f24e4f0b6d7a30a1a5efd35fd6f0f75bd",
      "optional": true
    }
  ],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "51572ab17987586994d09bcf9f704496b9895c88d72d50267e928fdb3b32e7ea",
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
//Скрипт «Создать сноски или комментарии из абзацев, помеченных знаками карет»
//Автор Sclex

var notesFromSelectedParagraphs_versionNum="4.4";

function Run() {
 var dialogWidth="700px";
 var dialogHeight="340px";
 var fbwBody=document.getElementById("fbw_body");
 var coll=new Object();
 coll["fbwBody"]=fbwBody;
 coll["mainDocument"]=document;
 coll["window"]=window;
 coll["versionNum"]=notesFromSelectedParagraphs_versionNum;
 var modes=window.showModelessDialog("HTML/Создать сноски или комментарии из абзацев, помеченных знаками карет - задание параметров.htm",coll,
     "dialogHeight: "+dialogHeight+"; dialogWidth: "+dialogWidth+"; "+
     "center: Yes; help: No; resizable: Yes; status: No;");
}
