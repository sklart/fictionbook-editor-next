/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Показать весь код",
  "version": "Не указана",
  "authors": [
    {
      "role": "unknown"
    },
    {
      "name": "mkb (Konstantin Matveev)",
      "role": "contributor",
      "contribution": "idea"
    }
  ],
  "authorshipStatus": "partial_attribution",
  "summary": "Открывает HTML ближайшего контейнера у курсора в отдельном окне.",
  "description": "Находит ближайший DIV или элемент с классом section у позиции курсора и выводит его outerHTML в отдельное окно по локальному HTML-шаблону. Несмотря на название, полный XML с описанием и бинарными вложениями не собирается.",
  "scope": "Ближайший контейнер DIV у позиции курсора; весь документ не обходится.",
  "modifiesDocument": false,
  "requiresSelection": false,
  "usage": [
    "Поставьте курсор в интересующий контейнер книги и запустите скрипт.",
    "Откроется локальное окно с HTML этого контейнера; файл HTML/Показать весь код.html должен находиться рядом с комплектом скриптов."
  ],
  "warnings": [
    "Если отдельное окно не откроется, скрипт не сможет вывести код; в исходнике это не проверяется."
  ],
  "dependencies": [
    {
      "path": "HTML/Показать весь код.html",
      "sha256": "9c1a17d59af89945467bd2a60fc66a6a2379b8beab22a2fa4cd197524bff21ee",
      "optional": false
    }
  ],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "1f93a95ff568e912e4a625d0ad9e64f7a59f16088b61be3fa1d7eee4a154d8ef",
    "reviewStatus": "generated"
  },
  "tags": [
    "Для программистов"
  ],
  "undo": {
    "status": "no_explicit_grouping",
    "description": "Скрипт только читает HTML книги и записывает текст в отдельное окно; изменений книги и операции отмены в нём нет."
  }
}
</FBE-SCRIPT-CARD>*/
//======================================
// Отображение HTML кода текущего раздела (only for script debugging)
// THX to mkb (Konstantin Matveev) for ingenious idea
//======================================

function Run()
{
 var rng=document.selection.createRange(); if(!rng) return false;

 var pe=rng.parentElement();

     var msg="";

 while(pe.parentElement && pe.tagName!="DIV" && pe.className!="section") pe=pe.parentElement;  

 if(pe) MyMsgWindow(pe);
}

function MyMsgWindow(pe)
{
 msg = pe.outerHTML;
 var MsgWindow = window.open("HTML/Показать весь код.html",null,"height=680,width=1014,status=no,toolbar=no,menubar=no,location=no,scrollbars=yes,resizable=yes");
 MsgWindow.document.body.innerText = msg;
}
