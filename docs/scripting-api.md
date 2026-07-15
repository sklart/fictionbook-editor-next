# API пользовательских скриптов FBE Next

Скрипты FBE выполняются в DOM-документе редактора. Дополнительные возможности
приложения доступны через объект `window.external`. В этой справке `document`
означает текущий `window.document`, а `element` — DOM-элемент этого документа.

## Оглавление

### Отмена и разметка

- [BeginUndoUnit](#beginundounit)
- [EndUndoUnit](#endundounit)
- [inflateBlock](#inflateblock)
- [InflateParagraphs](#inflateparagraphs)
- [SetStyleEx](#setstyleex)

### Диалоги и строка состояния

- [MsgBox](#msgbox)
- [AskYesNo](#askyesno)
- [InputBox](#inputbox)
- [GetModalResult](#getmodalresult)
- [SetStatusBarText](#setstatusbartext)

### Документ и окружение

- [GetDocumentFilePath](#getdocumentfilepath)
- [GetDocumentFileName](#getdocumentfilename)
- [GetDocumentDirectory](#getdocumentdirectory)
- [GetStylePath](#getstylepath)
- [GetProgramVersion](#getprogramversion)
- [GetUUID](#getuuid)
- [GetNBSP](#getnbsp)
- [IsFastMode](#isfastmode)
- [GetViewWidth](#getviewwidth)
- [GetViewHeight](#getviewheight)

### Изображения и файлы

- [GetBinarySize](#getbinarysize)
- [SaveBinary](#savebinary)
- [GetImageDimsByPath](#getimagedimsbypath)
- [GetImageDimsByData](#getimagedimsbydata)

### Элементы описания и жанры

- [GenrePopup](#genrepopup)
- [GetExtendedStyle](#getextendedstyle)
- [DescShowElement](#descshowelement)
- [DescShowMenu](#descshowmenu)

## Общие правила

- Методы доступны только при запуске скрипта внутри FBE. В обычном браузере
  объекта `window.external` с этим контрактом нет.
- Изменения DOM, которые должны отменяться одним действием, заключайте в
  `BeginUndoUnit` и `EndUndoUnit`. Для выхода по ошибке также обязательно
  завершайте начатую группу отмены.
- Параметры `x` и `y` в методах всплывающих меню — экранные координаты в
  пикселях; обычно их берут из события мыши.
- `BSTR` в COM-контракте соответствует строке JavaScript. Булевы результаты
  доступны как `true` и `false`.

## Отмена и разметка

<a id="beginundounit"></a>
### `BeginUndoUnit(document, action)`

Открывает именованную группу отмены для изменений DOM. `action` показывается в
истории отмены.

```js
window.external.BeginUndoUnit(document, "Нормализация заголовков");
// Изменение DOM.
window.external.EndUndoUnit(document);
```

<a id="endundounit"></a>
### `EndUndoUnit(document)`

Закрывает текущую группу отмены, начатую `BeginUndoUnit`.

<a id="inflateblock"></a>
### `inflateBlock(element)`

Свойство расширенного отображения блочного элемента. Чтение возвращает
булево значение, запись включает или выключает разворачивание блока.

```js
if (!window.external.inflateBlock(paragraph)) {
  window.external.inflateBlock(paragraph) = true;
}
```

<a id="inflateparagraphs"></a>
### `InflateParagraphs(element)`

Включает `inflateBlock` у всех вложенных элементов `P` переданного контейнера.

```js
window.external.InflateParagraphs(section);
```

<a id="setstyleex"></a>
### `SetStyleEx(document, element, style)`

Устанавливает значение атрибута `class` элемента. `document` передаётся для
совместимости с историческим контрактом.

```js
window.external.SetStyleEx(document, paragraph, "subtitle");
```

## Диалоги и строка состояния

<a id="msgbox"></a>
### `MsgBox(message)`

Показывает информационное окно с кнопкой «ОК».

```js
window.external.MsgBox("Обработка завершена.");
```

<a id="askyesno"></a>
### `AskYesNo(message)`

Показывает окно с кнопками «Да» и «Нет» и возвращает `true` только для «Да».

```js
if (window.external.AskYesNo("Продолжить?")) {
  // ...
}
```

<a id="inputbox"></a>
### `InputBox(prompt, title, value)`

Показывает окно ввода. Возвращает введённый текст при подтверждении; при
отмене возвращает пустую строку. Код закрытия окна доступен сразу после вызова
через `GetModalResult()`.

```js
var title = window.external.InputBox("Введите заголовок", "Свойства", "");
if (window.external.GetModalResult() !== 6) { // 6 — IDYES
  return;
}
```

<a id="getmodalresult"></a>
### `GetModalResult()`

Возвращает код последнего окна `InputBox`: при подтверждении — `6` (`IDYES`),
при отмене обычно `2` (`IDCANCEL`). Значение сохраняется до следующего
`InputBox`.

<a id="setstatusbartext"></a>
### `SetStatusBarText(text)`

Заменяет текст строки состояния главного окна FBE.

```js
window.external.SetStatusBarText("Обработано: " + count);
```

## Документ и окружение

<a id="getdocumentfilepath"></a>
### `GetDocumentFilePath()`

Возвращает абсолютный путь к текущему FB2-файлу. До первого сохранения новой
книги возвращает пустую строку.

<a id="getdocumentfilename"></a>
### `GetDocumentFileName()`

Возвращает имя текущего файла вместе с расширением, без пути. До первого
сохранения возвращает пустую строку.

<a id="getdocumentdirectory"></a>
### `GetDocumentDirectory()`

Возвращает абсолютный путь к каталогу текущей книги без завершающего `\`,
кроме корня диска. До первого сохранения возвращает пустую строку.

```js
var directory = window.external.GetDocumentDirectory();
if (!directory) {
  window.external.MsgBox("Сначала сохраните книгу.");
  return;
}
var reportPath = directory + "\\report.txt";
```

<a id="getstylepath"></a>
### `GetStylePath()`

Возвращает каталог установленной программы FBE без завершающего `\`.

<a id="getprogramversion"></a>
### `GetProgramVersion()`

Возвращает строку версии работающего FBE.

<a id="getuuid"></a>
### `GetUUID()`

Создаёт и возвращает новый UUID в верхнем регистре, без фигурных скобок.

```js
var id = window.external.GetUUID();
```

<a id="getnbsp"></a>
### `GetNBSP()`

Возвращает символ неразрывного пробела, выбранный в настройках FBE.

<a id="isfastmode"></a>
### `IsFastMode()`

Возвращает `true`, если в FBE включён быстрый режим просмотра.

<a id="getviewwidth"></a>
### `GetViewWidth()`

Возвращает текущую ширину рабочей области редактора в пикселях.

<a id="getviewheight"></a>
### `GetViewHeight()`

Возвращает текущую высоту рабочей области редактора в пикселях.

## Изображения и файлы

<a id="getbinarysize"></a>
### `GetBinarySize(data)`

Возвращает размер двоичных данных в байтах. Параметр должен быть строкой с
двоичным содержимым, например `base64data` элемента `binary`, а не строкой
base64-представления.

<a id="savebinary"></a>
### `SaveBinary(path, data, prompt)`

Сохраняет двоичные данные `data` в новый файл и возвращает `true` при успехе.
При `prompt = true` сначала открывается диалог сохранения; при `false` берётся
путь из `path`. Метод не перезаписывает уже существующий файл, поэтому
`false` означает также отказ из-за совпадения имени или отмены диалога.

```js
var saved = window.external.SaveBinary("cover.jpg", binary.base64data, false);
```

<a id="getimagedimsbypath"></a>
### `GetImageDimsByPath(path)`

Возвращает размеры изображения по пути строкой вида `"640x480"`. Если файл
не удалось прочитать или формат не поддержан, возвращает пустую строку.

<a id="getimagedimsbydata"></a>
### `GetImageDimsByData(data)`

Возвращает размеры изображения строкой `"ширинаxвысота"` по двоичным данным.
Передавайте значение `base64data` элемента `binary`; для произвольной строки
JavaScript метод не предназначен. При ошибке возвращается пустая строка.

```js
var dims = window.external.GetImageDimsByData(binary.base64data);
```

## Элементы описания и жанры

<a id="genrepopup"></a>
### `GenrePopup(element, x, y)`

Открывает меню жанров в экранной точке `(x, y)` и возвращает идентификатор
выбранного жанра FB2. Если меню закрыто без выбора, результат — `null`.
Параметр `element` сохранён для совместимости и в текущей реализации не
используется.

<a id="getextendedstyle"></a>
### `GetExtendedStyle(elementId)`

Возвращает состояние настройки расширенного показа элемента описания с данным
идентификатором.

<a id="descshowelement"></a>
### `DescShowElement(elementId, show)`

Включает или выключает расширенный показ элемента описания и сохраняет это
значение в настройках FBE.

<a id="descshowmenu"></a>
### `DescShowMenu(button, x, y)`

Открывает меню доступных компонентов описания в экранной точке `(x, y)` и
возвращает идентификатор выбранного элемента. Если выбор отменён, результат
не определён; проверяйте его перед использованием. Параметр `button`
сохранён для совместимости и в текущей реализации не используется.
