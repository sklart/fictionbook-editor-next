/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Управление структурой разделов и их заголовками",
  "version": "Не указана",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Открывает дерево разделов и заголовков для перестройки структуры книги в отдельном диалоге.",
  "description": "Передаёт всю область книги в HTML-диалог с деревом элементов. В диалоге можно менять структуру и заголовки, раскрывать дерево, показывать текстовые узлы и копировать HTML. Кнопка «Сохранить» возвращает собранный HTML; скрипт заменяет им содержимое всей области книги и восстанавливает служебную разметку. Закрытие без сохранения не применяет результат.",
  "scope": "Весь fbw_body при сохранении результата из диалога; выделение и положение курсора не ограничивают перестройку.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Запустите скрипт и отредактируйте дерево разделов и заголовков в открывшемся окне.",
    "Нажмите «Сохранить», чтобы перенести собранную структуру в книгу, или «Закрыть», чтобы выйти без её применения.",
    "После сохранения проверьте заголовки, вложенность разделов и ссылки до записи книги на диск."
  ],
  "warnings": [
    "Сохранение заменяет HTML всей области книги; проверьте структуру, форматирование и ссылки перед записью файла.",
    "Для диалога необходимы связанные HTML, CSS и JavaScript файлы; изображения используются для отображения дерева, меню и полосы прокрутки."
  ],
  "dependencies": [
    {
      "path": "HTML/Управление структурой разделов и их заголовками - набор фреймов.html",
      "sha256": "694341237d65101e829c67daffaac963c35f11ad267ab89095aff969ec3abba5",
      "optional": false
    },
    {
      "path": "HTML/Управление структурой разделов и их заголовками - фрейм 1.html",
      "sha256": "967fb981e6f771810032a3732aae157ebb1366032682368a1066c3681491580f",
      "optional": false
    },
    {
      "path": "HTML/Управление структурой разделов и их заголовками - фрейм 3.html",
      "sha256": "6ba3c2dbb27385226684b05baebd7c01f33902fbefb51236bb27480563098aaa",
      "optional": false
    },
    {
      "path": "HTML/css/Управление структурой разделов и их заголовками - дерево элементов.css",
      "sha256": "6753b577ec55a5d2c193f1e3d6702edfcf469e2be4c7d9c5c6e7e22038c25fd8",
      "optional": false
    },
    {
      "path": "HTML/css/Управление структурой разделов и их заголовками - контекстное меню.css",
      "sha256": "fa954669ef8498da790e690aecd0c28825eae78b742615b99da53b53d21dccf3",
      "optional": false
    },
    {
      "path": "HTML/css/Управление структурой разделов и их заголовками - фрейм 3.css",
      "sha256": "cd4a43bdf822003c3d8364165968b0c91ec76cb448cd7e94e20cc1d05b8252c3",
      "optional": false
    },
    {
      "path": "HTML/context-menu.js",
      "sha256": "034ad039a3b9ae427e3d0505b24035c2e15190ef0d8958eb58d5ee82aae150ea",
      "optional": false
    },
    {
      "path": "HTML/images/ScrollBlack.png",
      "sha256": "5eb264b0628d267c9722e5f74c28b469225355a64a94831ab48cf0ba6d1b583e",
      "optional": true
    },
    {
      "path": "HTML/images/minus.png",
      "sha256": "d9909e69a95b62bfbbab3a5e100163acc8d5d7800f5dc6a49fe207eeb8c56947",
      "optional": true
    },
    {
      "path": "HTML/images/paper.png",
      "sha256": "e1de45bc504067a62046cce1534b760826399c7acc9fe7558497b7d72c85ba61",
      "optional": true
    },
    {
      "path": "HTML/images/plus.png",
      "sha256": "e27436c0d5259a60833c18a9f9aca5b15cd9454fe0177239571d9eeb53c79b98",
      "optional": true
    },
    {
      "path": "HTML/images/dragDrop_ind1.gif",
      "sha256": "03c8f3ddf5aed200423eb3224655e73916190461e53cce9431ab0e543a8ff051",
      "optional": true
    },
    {
      "path": "HTML/images/dragDrop_ind2.gif",
      "sha256": "1bd45f981b63b1eeac9e7e6435b44c6d48bd434241bd5a943a479a340447642f",
      "optional": true
    },
    {
      "path": "HTML/images/ScrollLine.png",
      "sha256": "d5014bfa003ed90bf582d0f4dc4ae4365ef9dca8a90f7c23ae007fa1b4edfb85",
      "optional": true
    },
    {
      "path": "HTML/images/ScrollRunnerTop.png",
      "sha256": "350ca0b3a0fa38aeec4a72e9830fcece949f962c303a12ae7e2e1de1846ab39c",
      "optional": true
    },
    {
      "path": "HTML/images/ScrollRunnerCenter.png",
      "sha256": "05773cee44a7005be6b0a149bc94619f51465df2214385c87c181adc0f2243b9",
      "optional": true
    },
    {
      "path": "HTML/images/ScrollRunnerBottom.png",
      "sha256": "c4bd6a4dce662f920d7f38733da5584e61c8e424940069c9e024af9cc9e3cfa9",
      "optional": true
    },
    {
      "path": "HTML/images/scrollarrowup.png",
      "sha256": "846de1fc285b37101a4c2499324d68b63717c06bee1382025cdee6a2c96aa103",
      "optional": true
    },
    {
      "path": "HTML/images/scrollarrowdown.png",
      "sha256": "bebaea987e4d4840425a8a2cca8c73a8870f1c765151faa26498d98e8330a5df",
      "optional": true
    },
    {
      "path": "HTML/images/context-menu-gradient.gif",
      "sha256": "9bdafbb498fd98e646419fc5955e51ea030474f8cf4af3c6fe40623358ffb014",
      "optional": true
    }
  ],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "f33eaba0ac52dc762e55eb104dbea40b9c5fb8767ad16da491dc402a1d2f62ef",
    "reviewStatus": "generated"
  },
  "tags": [
    "Структура разделов",
    "параметры"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "При сохранении возвращённой структуры замена содержимого книги помещена в группу отмены. Закрытие диалога без сохранения группу не открывает; полный откат через редактор не проверен."
  },
  "settings": [
    {
      "name": "dialogWidth",
      "value": "640px",
      "expression": "\"640px\"",
      "description": "Ширина диалога управления структурой.",
      "valueMeaning": "Исходное значение в коде скрипта."
    },
    {
      "name": "dialogHeight",
      "value": "540px",
      "expression": "\"540px\"",
      "description": "Высота диалога управления структурой.",
      "valueMeaning": "Исходное значение в коде скрипта."
    }
  ]
}
</FBE-SCRIPT-CARD>*/
function Run() {

 var dialogWidth="640px"; // ширина диалогового окна
 var dialogHeight="540px"; // высота диалогового окна
 var params=new Object();
 var rslt;
 body=document.getElementById("fbw_body");
 params["fbw_body"]=body;
 rslt=window.showModalDialog("HTML/Управление структурой разделов и их заголовками - набор фреймов.html",params,
       "dialogHeight: "+dialogHeight+"; dialogWidth: "+dialogWidth+"; "+
       "center: Yes; help: No; resizable: Yes; status: No;");
 if (rslt) {
  window.external.BeginUndoUnit(document,"section structure editing");      
  body.innerHTML=rslt;
  InflateIt(body);
  window.external.EndUndoUnit(document);
 }
}
