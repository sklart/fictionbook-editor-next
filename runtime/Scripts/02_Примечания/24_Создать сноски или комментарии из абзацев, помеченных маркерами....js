/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Создать сноски или комментарии из абзацев, помеченных маркерами",
  "version": "7.3",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Создаёт примечания или комментарии из абзацев с заданными маркерами и вставляет ссылки по маркерам в тексте.",
  "description": "Открывает окно настройки маркеров ссылки и первого абзаца примечания. Ищет исходные абзацы и места ссылок по выбранным образцам, сверяет их количество и предлагает создать примечания либо комментарии. После создания отдельно спрашивает, удалять ли исходные абзацы.",
  "scope": "Маркированные абзацы и места ссылок во всём тексте книги; выделение не требуется.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Задайте в книге маркеры будущих ссылок и начала абзацев примечаний, затем запустите скрипт.",
    "В окне выберите образцы обоих маркеров, проверьте флаги регулярных выражений и при необходимости откройте предпросмотр.",
    "Создайте сноски либо комментарии, подтвердите совпадение числа маркеров и текстов и отдельно решите, удалять ли исходные абзацы. Проверьте результат."
  ],
  "warnings": [
    "Окно может удалить абзацы-источники после отдельного подтверждения; проверьте предпросмотр перед созданием."
  ],
  "dependencies": [
    {
      "path": "HTML/Создать сноски или комментарии из абзацев, помеченных маркерами - задание параметров.htm",
      "sha256": "4c74fa581625b72a4bc283dad4ee5bf69a6c32744ab9c745301e501cf8abecc1",
      "optional": false
    },
    {
      "path": "HTML/Создать сноски или комментарии из абзацев, помеченных маркерами - показ текстов сносок.htm",
      "sha256": "86e4f796ead809b5c3acec243e89b52389d11299f482c781e0653f57c8f0533b",
      "optional": true
    }
  ],
  "dependenciesComplete": true,
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
    "description": ""
  },
  "settings": [
    {
      "name": "noteSign",
      "title": "Вид маркера ссылки",
      "value": "надстрочное число",
      "description": "По умолчанию надстрочное число; можно выбрать готовый или собственный образец.",
      "source": "dialog"
    },
    {
      "name": "refMarkerStr",
      "title": "Свой маркер ссылки",
      "value": "\\*+",
      "description": "Исходный образец для собственного маркера ссылки.",
      "source": "dialog"
    },
    {
      "name": "interpretAsRegExpCheckbox",
      "title": "Шаблон ссылки как выражение",
      "value": true,
      "description": "Свой образец ссылки по умолчанию рассматривается как регулярное выражение.",
      "source": "dialog"
    },
    {
      "name": "matchNotFromBeginning",
      "title": "Маркер ссылки после текста",
      "value": true,
      "description": "По умолчанию совпадение для ссылки ищется не в начале абзаца.",
      "source": "dialog"
    },
    {
      "name": "firstNoteP",
      "title": "Маркер первого абзаца",
      "value": "надстрочное число",
      "description": "По умолчанию надстрочное число; можно выбрать готовый или собственный образец.",
      "source": "dialog"
    },
    {
      "name": "firstNotePMarkerStr",
      "title": "Свой маркер первого абзаца",
      "value": "\\*+",
      "description": "Исходный образец для первого абзаца текста примечания.",
      "source": "dialog"
    },
    {
      "name": "interpretAsRegExpCheckbox2",
      "title": "Шаблон абзаца как выражение",
      "value": true,
      "description": "Свой образец первого абзаца по умолчанию рассматривается как регулярное выражение.",
      "source": "dialog"
    },
    {
      "name": "notFirstParagraphStr",
      "title": "Продолжение примечания",
      "value": "++",
      "description": "Префикс второго и следующих абзацев текста одного примечания или комментария.",
      "source": "dialog"
    }
  ]
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
