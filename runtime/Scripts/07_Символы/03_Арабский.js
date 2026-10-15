/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Арабский",
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
  "summary": "Открывает таблицу «Арабский»; выбранный символ вставляется в текст книги.",
  "description": "Открывает отдельную немодальную HTML-таблицу «Арабский». Сам запуск скрипта не меняет документ. При щелчке по ячейке таблица вставляет её символ в текущую позицию или вместо выделения, только если текстовый диапазон находится внутри книги.",
  "scope": "Текущий текстовый диапазон внутри fbw_body; без щелчка по ячейке книга не меняется.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор в текст книги или выделите фрагмент, который нужно заменить символом.",
    "Запустите таблицу «Арабский» и щёлкните нужный символ. Окно таблицы остаётся открытым для следующих вставок.",
    "Проверьте место вставки и, если заменяли выделение, получившийся текст."
  ],
  "warnings": [
    "Если текущий текстовый диапазон находится вне книги, щелчок по символу не вставляет его.",
    "Вставку выполняет связанная HTML-таблица."
  ],
  "dependencies": [
    {
      "path": "HTML/Арабский.html",
      "sha256": "f5925041492c25f21eba07111bc963e6c5a271aa8b92918fadf170cc55bf18ba",
      "optional": false
    }
  ],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "01f7e8679efad1c3ad2d170f4d257d5120da71c6c8682d4bb945941f0a719a4d",
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
//Таблица символов Арабского алфавита
//                                jurgennt™, май 2008 г.
//                          script engene by Sclex v.1.3

function Run() {
 var dialogWidth="125px"; //ширина окна таблицы символов
 var dialogHeight="720px"; //высота окна таблицы символов
 var dialogLeft="5px";                                       //координата X
 var params=new Object(); 
 params["fbw_body"]=document.getElementById("fbw_body");
 params["document"]=document;
 params["window"]=window;
 var rslt=window.showModelessDialog("HTML/Арабский.html",params,
      "dialogHeight: "+dialogHeight+"; dialogWidth: "+dialogWidth+"; dialogLeft: "+dialogLeft+"; "+
      "center: Yes; help: No; resizable: Yes; status: No;"); 
}
