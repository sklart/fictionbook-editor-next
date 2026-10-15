/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Создать сноски или комментарии из абзацев, помеченных знаками решетки",
  "version": "4.4",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Создаёт примечания или комментарии из абзацев, начинающихся серией решёток, и ссылки по таким же маркерам в тексте.",
  "description": "В связанном окне серии символов «#» в начале абзаца считаются текстом нового примечания или комментария, а после текста — местом ссылки. Окно сверяет число маркеров и текстов, предлагает создать разделы и ссылки, затем отдельно спрашивает, удалять ли исходные абзацы.",
  "scope": "Абзацы и маркеры «#» во всём тексте книги; выделение не требуется.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Подготовьте серии «#» в начале абзацев с текстами примечаний и после текста в местах будущих ссылок.",
    "Запустите скрипт, при необходимости измените префикс продолжения «++» и проверьте предпросмотр.",
    "Выберите создание сносок либо комментариев, подтвердите совпадение числа текстов и маркеров и отдельно решите, удалять ли исходные абзацы. После этого проверьте ссылки и разделы."
  ],
  "warnings": [
    "Окно может удалить абзацы-источники после отдельного подтверждения; проверьте предпросмотр перед созданием."
  ],
  "dependencies": [
    {
      "path": "HTML/Создать сноски или комментарии из абзацев, помеченных знаками решетки - задание параметров.htm",
      "sha256": "55d288368ac1d5da09419aed63b1999e07a35f9a7e8fa14f2b55b86b732170a4",
      "optional": false
    },
    {
      "path": "HTML/Создать сноски или комментарии из абзацев, помеченных знаками решетки - показ текстов сносок.htm",
      "sha256": "f497ac50b2e025671f10d057c7600385cb9891c5254efa7282f2909b0bcfdab5",
      "optional": true
    }
  ],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "63986f2969ff2449cb7c8145270f875b438138a26ca810418ac9871f2778d953",
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
      "name": "notFirstParagraphStr",
      "title": "Продолжение примечания",
      "value": "++",
      "description": "Видимый в окне префикс второго и следующих абзацев текста одного примечания или комментария.",
      "source": "dialog"
    }
  ]
}
</FBE-SCRIPT-CARD>*/
//Скрипт «Создать сноски или комментарии из абзацев, помеченных знаками решетки»
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
 var modes=window.showModelessDialog("HTML/Создать сноски или комментарии из абзацев, помеченных знаками решетки - задание параметров.htm",coll,
     "dialogHeight: "+dialogHeight+"; dialogWidth: "+dialogWidth+"; "+
     "center: Yes; help: No; resizable: Yes; status: No;");
}
