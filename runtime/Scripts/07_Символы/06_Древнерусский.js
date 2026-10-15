/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Древнерусский",
  "version": "1.3",
  "authors": [
    {
      "name": "Sclex",
      "role": "author",
      "contribution": "script_engine"
    },
    {
      "name": "jurgennt",
      "role": "contributor",
      "contribution": "character_table"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Открывает таблицу «Древнерусский»; выбранный символ вставляется в текст книги.",
  "description": "Открывает отдельную немодальную HTML-таблицу «Древнерусский». Сам запуск скрипта не меняет документ. При щелчке по ячейке таблица вставляет её символ в текущую позицию или вместо выделения, только если текстовый диапазон находится внутри книги.",
  "scope": "Текущий текстовый диапазон внутри fbw_body; без щелчка по ячейке книга не меняется.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор в текст книги или выделите фрагмент, который нужно заменить символом.",
    "Запустите таблицу «Древнерусский» и щёлкните нужный символ. Окно таблицы остаётся открытым для следующих вставок.",
    "Проверьте место вставки и, если заменяли выделение, получившийся текст."
  ],
  "warnings": [
    "Если текущий текстовый диапазон находится вне книги, щелчок по символу не вставляет его.",
    "Вставку выполняет связанная HTML-таблица."
  ],
  "dependencies": [
    {
      "path": "HTML/Древнерусский.html",
      "sha256": "fb3cfb78bace935c43d4e4a31238a0345c7aae85d6150d550b343a663bd67c3a",
      "optional": false
    }
  ],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "f798106d6c3c8cea101fb02b65fda20da6c7a854f05d02cf6a79c085d4d41b95",
    "reviewStatus": "generated"
  },
  "tags": [
    "Символы"
  ],
  "undo": {
    "status": "may_be_delegated",
    "description": ""
  },
  "settings": []
}
</FBE-SCRIPT-CARD>*/
//Таблица символов Древнерусского алфавита
//                                jurgennt™, май 2008 г.
//                          script engene by Sclex v.1.3

function Run() {
 var dialogWidth="135px"; //ширина окна таблицы символов
 var dialogHeight="397px"; //высота окна таблицы символов
 var dialogLeft="5px";                                       //координата X
 var params=new Object(); 
 params["fbw_body"]=document.getElementById("fbw_body");
 params["document"]=document;
 params["window"]=window;
 var rslt=window.showModelessDialog("HTML/Древнерусский.html",params,
      "dialogHeight: "+dialogHeight+"; dialogWidth: "+dialogWidth+"; dialogLeft: "+dialogLeft+"; "+
      "center: Yes; help: No; resizable: Yes; status: No; scroll: No;"); 
}
