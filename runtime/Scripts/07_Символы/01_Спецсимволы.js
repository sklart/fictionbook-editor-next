/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Спецсимволы",
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
  "summary": "Открывает таблицу «Спецсимволы»; выбранный символ вставляется в текст книги.",
  "description": "Открывает отдельную немодальную HTML-таблицу «Спецсимволы». Сам запуск скрипта не меняет документ. При щелчке по ячейке таблица вставляет её символ в текущую позицию или вместо выделения, только если текстовый диапазон находится внутри книги.",
  "scope": "Текущий текстовый диапазон внутри fbw_body; без щелчка по ячейке книга не меняется.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор в текст книги или выделите фрагмент, который нужно заменить символом.",
    "Запустите таблицу «Спецсимволы» и щёлкните нужный символ. Окно таблицы остаётся открытым для следующих вставок.",
    "Проверьте место вставки и, если заменяли выделение, получившийся текст."
  ],
  "warnings": [
    "Если текущий текстовый диапазон находится вне книги, щелчок по символу не вставляет его.",
    "Вставку выполняет связанная HTML-таблица. При ошибке во время вставки завершение операции отмены не гарантировано."
  ],
  "dependencies": [
    {
      "path": "HTML/Спецсимволы.html",
      "sha256": "f49a59d4a30e2d90cdd9ae55e33ae3ca6153135e4862303b1daa09a8a39e9f98",
      "optional": false
    }
  ],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "f6477f079ff89309d6a740ddd0d40a586ec171bcd27a0f694f0993e690d5d6a7",
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
//Таблица специальных символов
//                                jurgennt™, май 2008 г.
//                          script engene by Sclex v.1.3

function Run() {
 var dialogWidth="200px"; //ширина окна таблицы символов
 var dialogHeight="352px"; //высота окна таблицы символов
 var dialogLeft="5px";                                   //координата X
 var params=new Object(); 
 params["fbw_body"]=document.getElementById("fbw_body");
 params["document"]=document;
 params["window"]=window;
 var rslt=window.showModelessDialog("HTML/Спецсимволы.html",params,
      "dialogHeight: "+dialogHeight+"; dialogWidth: "+dialogWidth+"; dialogLeft: "+dialogLeft+"; "+
      "center: Yes; help: No; resizable: Yes; status: No; scroll: No;"); 
}
