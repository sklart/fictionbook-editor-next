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
  "summary": "Вставляет ссылку на MIDI-файл вокруг выбранного текста.",
  "description": "Диалог выбора получает путь к локальному MIDI-файлу; HTML-обработчик оборачивает выделенный текст в A href=\"file:…\" и SUP через pasteHTML. Это добавление локальной файловой ссылки, а не встраивание MIDI в binary.",
  "scope": "Выделенный диапазон либо полные абзацы, которые он пересекает; см. описание варианта.",
  "modifiesDocument": true,
  "requiresSelection": true,
  "usage": [
    "Откройте нужную книгу в FBE Next; скрипты этого комплекта работают с визуальным HTML-представлением, если в описании не указано иное.",
    "Подготовьте выделение, описанное в scope и условиях запуска; некоторые команды работают с целыми абзацами.",
    "Сохраняйте структуру Scripts/HTML и связанные файлы; один JS может быть недостаточен для работы.",
    "Запустите команду и ответьте на её диалоги. Тексты и условия проверок приведены ниже.",
    "Просмотрите результат до сохранения. Для структурной правки проверьте книгу и связанные ссылки."
  ],
  "warnings": [
    "Скрипт предусматривает изменение книги. Перед массовой обработкой сохраните отдельную копию.",
    "Локальная file:-ссылка зависит от расположения MIDI-файла на компьютере/устройстве; при передаче FB2 может перестать работать."
  ],
  "dependencies": [
    {
      "path": "HTML/Сделать выделенный текст ссылкой на MIDI-файл (для AlReader'а).html",
      "sha256": "0d2c19dac4b6d36fd5953fcf20e5d9efcebbef6d692f3da80c863c0a0def9751",
      "optional": true
    }
  ],
  "dependenciesComplete": false,
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
    "description": "Вызовы BeginUndoUnit/EndUndoUnit найдены. Это не доказывает корректный откат всех ветвей, внешних файлов и зависимостей."
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
