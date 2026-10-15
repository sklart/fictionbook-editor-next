/*<FBE-SCRIPT-CARD version="1">
{
  "schema": 1,
  "title": "Сделать выделенный текст ссылкой на MIDI-файл (для AlReader'а)",
  "version": "Не указана",
  "authors": [
    {
      "role": "unknown"
    }
  ],
  "authorshipStatus": "not_stated",
  "summary": "Оборачивает выделенный текст ссылкой на выбранное имя MIDI-файла.",
  "description": "Открывает окно с полем имени файла и кнопкой выбора. При выборе файла окно оставляет в поле только его имя, затем заменяет выделенный HTML на ссылку file: с этим именем и элементом SUP внутри. Существующие теги SUP в выделении удаляются. Файл не встраивается в книгу.",
  "scope": "Только непустое HTML-выделение в книге; изменение выполняет связанное HTML-окно после нажатия кнопки создания ссылки.",
  "modifiesDocument": true,
  "requiresSelection": true,
  "usage": [
    "Выделите текст, который должен стать ссылкой.",
    "Запустите команду, выберите MIDI-файл или введите нужное имя в поле окна.",
    "Нажмите кнопку создания ссылки и проверьте результат в книге и путь к файлу."
  ],
  "warnings": [
    "В ссылке сохраняется имя выбранного файла без полного пути. Доступность MIDI зависит от расположения файла в среде чтения.",
    "Существующие теги SUP внутри выделения удаляются перед вставкой нового SUP.",
    "При запуске без выделения скрипт выходит после открытия группы отмены, не закрывая её."
  ],
  "dependencies": [
    {
      "path": "HTML/Сделать выделенный текст ссылкой на MIDI-файл (для AlReader'а).html",
      "sha256": "0d2c19dac4b6d36fd5953fcf20e5d9efcebbef6d692f3da80c863c0a0def9751",
      "optional": false
    }
  ],
  "dependenciesComplete": true,
  "sourceRevision": {
    "algorithm": "sha256-exclude-fbe-card-v1",
    "sha256": "2dbf1399d01502a24d966ac439d75afb53026c138d2ee5b605284ffa32af91d9",
    "reviewStatus": "generated"
  },
  "tags": [
    "Мелочи редактирования"
  ],
  "undo": {
    "status": "explicit_calls_found",
    "description": "Пустое выделение оставляет начатую группу отмены открытой."
  }
}
</FBE-SCRIPT-CARD>*/
function Run() {
 window.external.BeginUndoUnit(document,"создание ссылки на MIDI-файл (для AlReader'а)");
 var coll={};
 var rng=document.selection.createRange();
 if (rng.htmlText=="") {
   MsgBox("Вы ничего не выделили.\n\nПеред запуском скрипта нужно выделить текст, который требуется превратить в ссылку.");
   return;
 }
 coll["document"]=document;
 var dialogWidth="600px";
 var dialogHeight="50px";
 var modes=window.showModalDialog("HTML/Сделать выделенный текст ссылкой на MIDI-файл (для AlReader'а).html",coll,
     "dialogHeight: "+dialogHeight+"; dialogWidth: "+dialogWidth+"; "+
     "center: Yes; help: No; resizable: Yes; status: No; scroll: No;");
 window.external.EndUndoUnit(document);
}
