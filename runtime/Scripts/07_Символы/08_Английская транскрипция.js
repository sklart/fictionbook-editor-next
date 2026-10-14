/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Английская транскрипция",
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
    },
    {
      "name": "stokber",
      "role": "contributor",
      "contribution": "character_table"
    }
  ],
  "authorshipStatus": "declared",
  "summary": "Открывает таблицу «Английская транскрипция»; выбранный символ вставляется в текст книги.",
  "description": "Открывает отдельную немодальную HTML-таблицу «Английская транскрипция». Сам запуск скрипта не меняет документ. При щелчке по ячейке таблица вставляет её символ в текущую позицию или вместо выделения, только если текстовый диапазон находится внутри книги.",
  "scope": "Текущий текстовый диапазон внутри fbw_body; без щелчка по ячейке книга не меняется.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор в текст книги или выделите фрагмент, который нужно заменить символом.",
    "Запустите таблицу «Английская транскрипция» и щёлкните нужный символ. Окно таблицы остаётся открытым для следующих вставок.",
    "Проверьте место вставки и, если заменяли выделение, получившийся текст."
  ],
  "warnings": [
    "Если текущий текстовый диапазон находится вне книги, щелчок по символу не вставляет его.",
    "Вставку выполняет связанная HTML-таблица. При ошибке во время вставки завершение операции отмены не гарантировано."
  ],
  "dependencies": [
    {
      "path": "HTML/Английская транскрипция.html",
      "sha256": "5e19ea1be4e97a555102813cbbf4cc64fb50c322966343c1ae120ed5b408391a",
      "optional": false
    }
  ],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "50370506db8414bea5a88792063e260a8eaacbd51d2cc6534071c08c53e36c22",
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
//Таблица символов английской транскрипции
//                                jurgennt™, май 2008 г.
//                          script engene by Sclex v.1.3
//                          таблица: stokber, май 2024

function Run() {
 var dialogWidth="280px"; //ширина окна таблицы символов
 var dialogHeight="170px"; //высота окна таблицы символов
 var dialogLeft="5px";                                   //координата X
 var params=new Object(); 
 params["fbw_body"]=document.getElementById("fbw_body");
 params["document"]=document;
 params["window"]=window;
 var rslt=window.showModelessDialog("HTML/Английская транскрипция.html",params,
      "dialogHeight: "+dialogHeight+"; dialogWidth: "+dialogWidth+"; dialogLeft: "+dialogLeft+"; "+
      "center: Yes; help: No; resizable: Yes; status: No; scroll: No;"); 
}
