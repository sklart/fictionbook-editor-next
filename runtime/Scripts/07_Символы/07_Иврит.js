/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Иврит",
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
  "summary": "Открывает таблицу «Иврит»; выбранный символ вставляется в текст книги.",
  "description": "Открывает отдельную немодальную HTML-таблицу «Иврит». Сам запуск скрипта не меняет документ. При щелчке по ячейке таблица вставляет её символ в текущую позицию или вместо выделения, только если текстовый диапазон находится внутри книги.",
  "scope": "Текущий текстовый диапазон внутри fbw_body; без щелчка по ячейке книга не меняется.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор в текст книги или выделите фрагмент, который нужно заменить символом.",
    "Запустите таблицу «Иврит» и щёлкните нужный символ. Окно таблицы остаётся открытым для следующих вставок.",
    "Проверьте место вставки и, если заменяли выделение, получившийся текст."
  ],
  "warnings": [
    "Если текущий текстовый диапазон находится вне книги, щелчок по символу не вставляет его.",
    "Вставку выполняет связанная HTML-таблица. При ошибке во время вставки завершение операции отмены не гарантировано."
  ],
  "dependencies": [
    {
      "path": "HTML/Иврит.html",
      "sha256": "20fbacdec9fc88d19962c188fa96e833335e7b681defce8799a67c1cf9098b08",
      "optional": false
    }
  ],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "9f9cc32eac7fd5eed623d624f17fc32e3328d3bdd51d628f70af10ae16c29c69",
    "reviewStatus": "generated"
  },
  "tags": [
    "Символы"
  ],
  "undo": {
    "status": "may_be_delegated",
    "description": "При щелчке по символу связанная таблица выполняет вставку в рамках одной операции отмены редактора. При ошибке её завершение и полнота отката не проверены."
  },
  "settings": []
}
</FBE-SCRIPT-CARD>*/
//Таблица символов Еврейского алфавита
//                                jurgennt™, май 2008 г.
//                          script engene by Sclex v.1.3

function Run() {
 var dialogWidth="80px";                                 //ширина окна таблицы символов
 var dialogHeight="719px";                                //высота окна таблицы символов
 var dialogLeft="5px";                                       //координата X
 var dialogTop="5px";                                       //координата Y
 var params=new Object(); 
 params["fbw_body"]=document.getElementById("fbw_body");
 params["document"]=document;
 params["window"]=window;
 var rslt=window.showModelessDialog("HTML/Иврит.html",params,
      "dialogHeight: "+dialogHeight+"; dialogWidth: "+dialogWidth+"; dialogLeft: "+dialogLeft+"; dialogTop: "+dialogTop+"; "+
      "center: Yes; help: No; resizable: Yes; status: No; scroll: No"); 
}
