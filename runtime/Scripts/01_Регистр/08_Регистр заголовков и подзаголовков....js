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
  "summary": "Открывает окно для выбора заголовков и подзаголовков и изменения регистра их текста.",
  "description": "Показывает заголовки и подзаголовки выбранных категорий в отдельном окне. Изменения регистра выполняются в этом окне и сохраняются в исходную книгу после нажатия «ОК»; «Отмена» закрывает окно без сохранения.",
  "scope": "Заголовки и подзаголовки выбранных в окне категорий; по умолчанию заголовки body/section и подзаголовки section, без разделов примечаний и комментариев.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Запустите скрипт и выберите в окне категории заголовков и подзаголовков.",
    "Нажмите «Отобразить», измените регистр нужных фрагментов и нажмите «ОК» для записи в книгу.",
    "Проверьте заголовки со сложной разметкой и ссылки после сохранения изменений."
  ],
  "warnings": [
    "При нажатии «ОК» окно переписывает HTML изменённых заголовков в книге; проверьте сложную разметку и ссылки внутри них."
  ],
  "dependencies": [
    {
      "path": "HTML/Регистр заголовков и подзаголовков - набор фреймов.html",
      "sha256": "0481e9922c381cb15bff64066235fda29a65f6cae9af90c7366a727b60767190",
      "optional": false
    },
    {
      "path": "HTML/Регистр заголовков и подзаголовков - фрейм 1.html",
      "sha256": "aa058a9c1d42d6293202507467b98aa039635282d3d61e5e36204f0316e26fd8",
      "optional": false
    },
    {
      "path": "HTML/Регистр заголовков и подзаголовков - фрейм 2.html",
      "sha256": "e35f4b1e3de088dfe14da60e6127ca751274cd5766cadb75c5244fa4dc728e3e",
      "optional": false
    },
    {
      "path": "HTML/CSS/Регистр заголовков и подзаголовков - фрейм 1.css",
      "sha256": "b6d9474e7a0779839b8e3ae0015ebe2a752ace925ef1853637dbe4afecddfeb6",
      "optional": false
    }
  ],
  "dependenciesComplete": true,
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
    "description": "Окно предусматривает отмену сохранённых изменений, однако полный откат не проверен."
  },
  "settings": [
    {
      "name": "t1",
      "title": "Заголовки body и section",
      "value": true,
      "description": "Показывать заголовки основного раздела и разделов книги. Начальное состояние флажка в окне.",
      "source": "dialog"
    },
    {
      "name": "t2",
      "title": "Заголовки стихов и цитат",
      "value": false,
      "description": "Показывать заголовки poem и cite. Начальное состояние флажка в окне.",
      "source": "dialog"
    },
    {
      "name": "t4",
      "title": "Подзаголовки разделов",
      "value": true,
      "description": "Показывать подзаголовки section. Начальное состояние флажка в окне.",
      "source": "dialog"
    },
    {
      "name": "t5",
      "title": "Подзаголовки стихов и цитат",
      "value": false,
      "description": "Показывать подзаголовки poem и cite. Начальное состояние флажка в окне.",
      "source": "dialog"
    },
    {
      "name": "t3",
      "title": "Разделы примечаний и комментариев",
      "value": false,
      "description": "Включать заголовки и подзаголовки из разделов notes и comments. Начальное состояние флажка в окне.",
      "source": "dialog"
    }
  ]
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
