/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Регистр заголовков и подзаголовков",
  "version": "Не указана",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Открывает редактор регистра заголовков и подзаголовков.",
  "description": "Сам JS передаёт документ связанному HTML-интерфейсу. Изменение текстов выполняется обработчиками HTML через mainDoc/mainWin и оборачивается в Undo. Отсутствие прямых записей в коротком JS не означает режим «только просмотр».",
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
      "path": "HTML/Регистр заголовков и подзаголовков - набор фреймов.html",
      "sha256": "0481e9922c381cb15bff64066235fda29a65f6cae9af90c7366a727b60767190",
      "optional": true
    }
  ],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "4cd7bd2aaecec345b7af4b841ae534c13d2522bf6f77ebb15af5d00d367e0879",
    "reviewStatus": "generated"
  },
  "tags": [
    "Регистр"
  ],
  "undo": {
    "status": "may_be_delegated",
    "description": "Прямых Undo-вызовов нет; действия могут выполняться связанным HTML/JS. Гарантия отмены не установлена."
  }
}
</FBE-SCRIPT-CARD>*/
//скрипт "Регистр заголовков и подзаголовков"
//автор Sclex
//сайт скриптов: http://scripts.fictionbook.org

function Run() {
 var elementBrowser_versionNum="1.9";
 var dialogWidth="800px";
 var dialogHeight="540px";
 var fbwBody=document.getElementById("fbw_body");
 var coll=new Object();
 coll["fbwBody"]=fbwBody;
 coll["document"]=document;
 coll["window"]=window;
 coll["versionNum"]=elementBrowser_versionNum;
 window.showModalDialog("HTML/Регистр заголовков и подзаголовков - набор фреймов.html",coll,
   "dialogHeight: "+dialogHeight+"; dialogWidth: "+dialogWidth+"; "+
   "center: Yes; help: No; resizable: Yes; status: No;"); 
}
