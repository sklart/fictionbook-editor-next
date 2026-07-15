# API пользовательских скриптов FBE Next

## Путь к открытому документу

Скрипты, запускаемые из FBE Next, могут получить сведения об открытой книге
через `window.external`:

```js
var fullPath = window.external.GetDocumentFilePath();
var fileName = window.external.GetDocumentFileName();
var directory = window.external.GetDocumentDirectory();
```

`GetDocumentFilePath()` возвращает абсолютный путь к текущему FB2-файлу.
`GetDocumentFileName()` возвращает только имя файла вместе с расширением, а
`GetDocumentDirectory()` — абсолютный путь к его каталогу без завершающего
обратного слеша (кроме корня диска).

До первого сохранения новой книги все три метода возвращают пустую строку.
Скрипту следует проверить это условие и при необходимости предложить
пользователю сохранить документ.

```js
var directory = window.external.GetDocumentDirectory();
if (!directory) {
  MsgBox("Сначала сохраните книгу.");
  return;
}

var reportPath = directory + "\\report.txt";
```

Этот API не зависит от внутренних DOM-объектов редактора и предназначен для
совместимого использования в пользовательских скриптах.
