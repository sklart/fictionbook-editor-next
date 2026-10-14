/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Диакритика",
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
  "summary": "Открывает таблицу «Диакритика»; выбранный символ вставляется в текст книги.",
  "description": "Открывает отдельную немодальную HTML-таблицу «Диакритика». Сам запуск скрипта не меняет документ. При щелчке по ячейке таблица вставляет её символ в текущую позицию или вместо выделения, только если текстовый диапазон находится внутри книги.",
  "scope": "Текущий текстовый диапазон внутри fbw_body; без щелчка по ячейке книга не меняется.",
  "modifiesDocument": true,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор в текст книги или выделите фрагмент, который нужно заменить символом.",
    "Запустите таблицу «Диакритика» и щёлкните нужный символ. Окно таблицы остаётся открытым для следующих вставок.",
    "Проверьте место вставки и, если заменяли выделение, получившийся текст."
  ],
  "warnings": [
    "Если текущий текстовый диапазон находится вне книги, щелчок по символу не вставляет его.",
    "Вставку выполняет связанная HTML-таблица. При ошибке во время вставки завершение операции отмены не гарантировано."
  ],
  "dependencies": [
    {
      "path": "HTML/Диакритика.html",
      "sha256": "323a3922b341bd93d90e2d262b7f51d273d2429a844e2adc746a5d3e879c1534",
      "optional": false
    }
  ],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "537b18a619f0161d78d4f6ac1b63c915f2355fef58176ed1ec7d843ede0359d2",
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
//Таблица дополнительных символов европейских языков алфавита
//                                jurgennt™, май 2008 г.
//                          script engene by Sclex v.1.3

function Run() {
 var dialogWidth="280px"; //ширина окна таблицы символов
 var dialogHeight="652px"; //высота окна таблицы символов
 var dialogLeft="5px";                                   //координата X
 var params=new Object(); 
 params["fbw_body"]=document.getElementById("fbw_body");
 params["document"]=document;
 params["window"]=window;
 var rslt=window.showModelessDialog("HTML/Диакритика.html",params,
      "dialogHeight: "+dialogHeight+"; dialogWidth: "+dialogWidth+"; dialogLeft: "+dialogLeft+"; "+
      "center: Yes; help: No; resizable: Yes; status: No; scroll: No;"); 
}
