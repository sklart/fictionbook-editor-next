/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Создать сноски или комментарии из выделенных абзацев",
  "version": "4.2",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Создаёт примечания или комментарии из выделенных абзацев и соответствующие ссылки.",
  "description": "Открывает окно настройки, где можно выбрать вид маркера ссылки и создать примечания либо комментарии из выделенных абзацев. Окно извлекает тексты, сверяет число маркеров с числом будущих примечаний, добавляет ссылки и разделы, затем перенумеровывает соответствующие записи.",
  "scope": "Непустое выделение в тексте книги: используются целые абзацы, затронутые выделением. Выделение в поле ввода не поддерживается.",
  "modifiesDocument": true,
  "requiresSelection": true,
  "usage": [
    "Выделите абзацы с текстами будущих примечаний или комментариев и запустите скрипт.",
    "В открывшемся окне выберите вид маркеров, при необходимости измените символ продолжения абзаца и запустите предпросмотр.",
    "Нажмите «Создать сноски» или «Создать комментарии», сверьте число маркеров и текстов, подтвердите создание и проверьте ссылки и новые разделы."
  ],
  "warnings": [
    "При несовпадении числа маркеров и текстов либо отказе от создания окно может завершить операцию отмены не полностью.",
    "Создание затрагивает целые абзацы и может перенумеровать существующие примечания; проверьте ссылки после выполнения."
  ],
  "dependencies": [
    {
      "path": "HTML/Создать сноски из выделенных абзацев - задание параметров.htm",
      "sha256": "0829d070078f6d846d312e3541ad45f514aa3dee1bc56cded7e4f4369aaef5af",
      "optional": false
    }
  ],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "0d6a686ede260be21e0ce68c1bdfe93312bc5d94f8899c5a89f3377fa7f03f30",
    "reviewStatus": "generated"
  },
  "tags": [
    "Примечания",
    "сноски"
  ],
  "undo": {
    "status": "may_be_delegated",
    "description": ""
  },
  "settings": [
    {
      "name": "noteSign",
      "title": "Вид маркера ссылки",
      "value": "6 ",
      "description": "По умолчанию номер надстрочным текстом; в окне доступны [1], {1}, [~1~], {~1~}, звёздочка и свой вариант.",
      "source": "dialog"
    },
    {
      "name": "refMarkerStr",
      "title": "Свой маркер",
      "value": "[*]",
      "description": "Образец для поиска маркера при выборе собственного варианта.",
      "source": "dialog"
    },
    {
      "name": "interpretAsRegExpCheckbox",
      "title": "Регулярное выражение",
      "value": true,
      "description": "По умолчанию свой образец трактуется как регулярное выражение.",
      "source": "dialog"
    },
    {
      "name": "notFirstParagraphStr",
      "title": "Продолжение примечания",
      "value": "++",
      "description": "Префикс абзаца, который относится к предыдущему примечанию.",
      "source": "dialog"
    }
  ]
}
</FBE-SCRIPT-CARD>*/
//Скрипт «Создать сноски или комментарии из выделенных абзацев»
//Автор Sclex

var notesFromSelectedParagraphs_versionNum="4.2";

function Run() {
 var dialogWidth="700px";
 var dialogHeight="380px";
 var fbwBody=document.getElementById("fbw_body");
 var coll=new Object();
 coll["fbwBody"]=fbwBody;
 coll["mainDocument"]=document;
 coll["window"]=window;
 coll["versionNum"]=notesFromSelectedParagraphs_versionNum;
 var modes=window.showModelessDialog("HTML/Создать сноски из выделенных абзацев - задание параметров.htm",coll,
     "dialogHeight: "+dialogHeight+"; dialogWidth: "+dialogWidth+"; "+
     "center: Yes; help: No; resizable: Yes; status: No;");
}
