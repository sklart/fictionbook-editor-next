/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Показать весь код",
  "version": "Не указана",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Показывает HTML выбранного контейнера в отдельном окне.",
  "description": "Берёт outerHTML элемента, найденного относительно курсора, и помещает код в окно отчёта. Название «весь код» не означает, что получен полный XML с description и binary.",
  "scope": "Текущая книга или определённые кодом её разделы.",
  "modifiesDocument": false,
  "requiresSelection": null,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Сохраняйте структуру Scripts/HTML и связанные файлы; один JS может быть недостаточен для работы.",
    "Запустите команду из меню или дерева скриптов."
  ],
  "warnings": [
    "Записи в DOM могут относиться к временным маркерам, интерфейсу или отчёту; отсутствие постоянных правок книги — результат статического разбора, не runtime-гарантия."
  ],
  "dependencies": [
    {
      "path": "HTML/Показать весь код.html",
      "sha256": "9c1a17d59af89945467bd2a60fc66a6a2379b8beab22a2fa4cd197524bff21ee",
      "optional": true
    }
  ],
  "dependenciesComplete": false,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "1f93a95ff568e912e4a625d0ad9e64f7a59f16088b61be3fa1d7eee4a154d8ef",
    "reviewStatus": "generated"
  },
  "tags": [
    "Для программистов"
  ],
  "undo": {
    "status": "may_be_delegated",
    "description": "Прямых Undo-вызовов нет; действия могут выполняться связанным HTML/JS. Гарантия отмены не установлена."
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
