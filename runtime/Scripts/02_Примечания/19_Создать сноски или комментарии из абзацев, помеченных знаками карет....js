/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Создать сноски или комментарии из абзацев, помеченных знаками карет",
  "version": "4.4",
  "authors": [
    {
      "name": "Sclex",
      "role": "author"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Создаёт примечания или комментарии из абзацев, начинающихся серией карет, и ссылки по таким же маркерам в тексте.",
  "description": "В связанном окне серии символов «^» в начале абзаца считаются текстом нового примечания или комментария, а после текста — местом ссылки. Окно сверяет число маркеров и текстов, предлагает создать разделы и ссылки, затем отдельно спрашивает, удалять ли исходные абзацы.",
  "scope": "Абзацы и маркеры «^» во всём тексте книги; выделение не требуется.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Подготовьте серии «^» в начале абзацев с текстами примечаний и после текста в местах будущих ссылок.",
    "Запустите скрипт, при необходимости измените префикс продолжения «++» и проверьте предпросмотр.",
    "Выберите создание сносок либо комментариев, подтвердите совпадение числа текстов и маркеров и отдельно решите, удалять ли исходные абзацы. После этого проверьте ссылки и разделы."
  ],
  "warnings": [
    "Окно может удалить абзацы-источники после отдельного подтверждения; проверьте предпросмотр перед созданием."
  ],
  "dependencies": [
    {
      "path": "HTML/Создать сноски или комментарии из абзацев, помеченных знаками карет - задание параметров.htm",
      "sha256": "372bfab2e06f82de76540e83f402b32f24e4f0b6d7a30a1a5efd35fd6f0f75bd",
      "optional": false
    },
    {
      "path": "HTML/Создать сноски или комментарии из абзацев, помеченных знаками карет - показ текстов сносок.htm",
      "sha256": "653690a89d21493bef82cf115a404320a8ec08e6e9124cd115bfe02f8191b65d",
      "optional": true
    }
  ],
  "dependenciesComplete": true,
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
