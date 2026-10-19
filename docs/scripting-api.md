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

- [Работа с исходным XML документа](#работа-с-исходным-xml-документа)
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
истории отмены. На вход передаются DOM-документ и строка названия действия;
результат не используется.

```js
window.external.BeginUndoUnit(document, "Нормализация заголовков");
// Изменение DOM.
window.external.EndUndoUnit(document);
```

<a id="endundounit"></a>
### `EndUndoUnit(document)`

Закрывает текущую группу отмены, начатую `BeginUndoUnit`. На вход передаётся
тот же DOM-документ; результат не используется.

```js
window.external.BeginUndoUnit(document, "Смена класса абзаца");
paragraph.className = "subtitle";
window.external.EndUndoUnit(document);
```

<a id="inflateblock"></a>
### `inflateBlock(element)`

Свойство расширенного отображения блочного элемента. Чтение возвращает
булево значение, запись включает или выключает разворачивание блока. На вход
передаётся DOM-элемент; результат чтения — `true` или `false`.

```js
if (!window.external.inflateBlock(paragraph)) {
  window.external.inflateBlock(paragraph) = true;
}
```

<a id="inflateparagraphs"></a>
### `InflateParagraphs(element)`

Включает `inflateBlock` у всех вложенных элементов `P` переданного контейнера.
На вход передаётся DOM-контейнер; результат не используется.

```js
window.external.InflateParagraphs(section);
```

<a id="setstyleex"></a>
### `SetStyleEx(document, element, style)`

Устанавливает значение атрибута `class` элемента. `document` передаётся для
совместимости с историческим контрактом. Вход: DOM-документ, DOM-элемент и
строка CSS-класса; результат не используется.

```js
window.external.SetStyleEx(document, paragraph, "subtitle");
```

## Диалоги и строка состояния

<a id="msgbox"></a>
### `MsgBox(message)`

Показывает информационное окно с кнопкой «ОК». На вход передаётся строка;
результат не используется.

```js
window.external.MsgBox("Обработка завершена.");
```

<a id="askyesno"></a>
### `AskYesNo(message)`

Показывает окно с кнопками «Да» и «Нет» и возвращает `true` только для «Да».
На вход передаётся строка; результат — `true` или `false`.

```js
if (window.external.AskYesNo("Продолжить?")) {
  // ...
}
```

<a id="inputbox"></a>
### `InputBox(prompt, title, value)`

Показывает окно ввода. Возвращает введённый текст при подтверждении; при
отмене возвращает пустую строку. Код закрытия окна доступен сразу после вызова
через `GetModalResult()`. На вход передаются текст приглашения, заголовок и
начальное значение; результат — строка ввода или пустая строка.

```js
var input = window.external.InputBox("Введите заголовок", "Свойства", "");
if (window.external.GetModalResult() !== 6) { // 6 — IDYES
  return;
}
// input содержит введённый пользователем текст.
```

<a id="getmodalresult"></a>
### `GetModalResult()`

Возвращает код последнего окна `InputBox`: при подтверждении — `6` (`IDYES`),
при отмене обычно `2` (`IDCANCEL`). Значение сохраняется до следующего
`InputBox`. Входных параметров нет; результат — числовой код кнопки.

```js
window.external.InputBox("Введите значение", "Параметр", "по умолчанию");
var resultCode = window.external.GetModalResult(); // 6 — подтверждено
```

<a id="setstatusbartext"></a>
### `SetStatusBarText(text)`

Заменяет текст строки состояния главного окна FBE. На вход передаётся строка;
результат не используется.

```js
window.external.SetStatusBarText("Обработано: " + count);
```

## Документ и окружение

<a id="работа-с-исходным-xml-документа"></a>
### Работа с исходным XML документа

`document` — это DOM визуального Body, предоставляемый MSHTML. Он не является
полным XML книги. Для работы со всем документом FB2 или FBD через Source API
используйте `window.external`: получаемая строка включает `description`, все
`body` и `binary`.

```js
window.external.GetSourceText()
window.external.ValidateSourceText(xml)
window.external.GetLastSourceDiagnostic()
window.external.ApplySourceText(xml, action)
```

`window.external.GetSourceText()` возвращает полный актуальный XML. Если
пользователь оставил несохранённые изменения в режиме Source, метод читает
именно этот текст и не переключает редактор в другой режим. Большие `binary`
существенно увеличивают размер возвращаемой строки.

Перед изменением вызывайте `window.external.ValidateSourceText(xml)`. Это
штатная проверка полного FB2/FBD. Если XML невалиден, вызов
`window.external.ApplySourceText(xml, action)` также отвергает его: документ
не меняется. При успехе Apply создаёт одну стандартную операцию Undo/Redo с
описанием `action`.

`window.external.GetLastSourceDiagnostic()` возвращает JSON последней XML
операции. Его структура: `valid` (boolean), `message` (string), `line`
(number) и `column` (number). В JScript JSON можно разобрать через `eval`:

```js
function ReadSourceDiagnostic()
{
    return eval("(" + window.external.GetLastSourceDiagnostic() + ")");
}

function Run()
{
    var xml = window.external.GetSourceText();
    var changed = xml.replace("старый текст", "новый текст");

    if (!window.external.ValidateSourceText(changed))
    {
        var diagnostic = ReadSourceDiagnostic();
        window.external.MsgBox(
            diagnostic.message + " (" + diagnostic.line + ":" + diagnostic.column + ")");
        return;
    }

    // Apply повторно валидирует XML и добавляет одну обычную операцию Undo/Redo.
    window.external.ApplySourceText(changed, "Замена текста в XML");
}
```
<a id="getdocumentfilepath"></a>
### `GetDocumentFilePath()`

Возвращает абсолютный путь к текущему FB2-файлу. До первого сохранения новой
книги возвращает пустую строку. Метод возвращает абсолютный нормализованный
путь; если нормализовать путь невозможно, возвращает пустую строку. Входных
параметров нет; результат — строка.

```js
var fullPath = window.external.GetDocumentFilePath();
// Например: "D:\\Books\\Example.fb2" или "" для несохранённой книги.
```

<a id="getdocumentfilename"></a>
### `GetDocumentFileName()`

Возвращает имя текущего файла вместе с расширением, без пути. До первого
сохранения возвращает пустую строку. Входных параметров нет; результат —
строка.

```js
var fileName = window.external.GetDocumentFileName();
// Например: "Example.fb2".
```

<a id="getdocumentdirectory"></a>
### `GetDocumentDirectory()`

Возвращает абсолютный путь к каталогу текущей книги без завершающего `\`,
кроме корня диска. До первого сохранения возвращает пустую строку. Входных
параметров нет; результат — строка.

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

Возвращает каталог установленной программы FBE без завершающего `\`. Входных
параметров нет; результат — строка.

```js
var programDir = window.external.GetStylePath();
// Например: "C:\\Program Files (x86)\\FictionBook Editor Next".
```

<a id="getprogramversion"></a>
### `GetProgramVersion()`

Возвращает строку версии работающего FBE. Входных параметров нет; результат —
строка.

```js
var version = window.external.GetProgramVersion();
// Например: "3.0.5".
```

<a id="getuuid"></a>
### `GetUUID()`

Создаёт и возвращает новый UUID в верхнем регистре, без фигурных скобок.
Входных параметров нет; результат — строка UUID.

```js
var id = window.external.GetUUID();
```

<a id="getnbsp"></a>
### `GetNBSP()`

Возвращает символ неразрывного пробела, выбранный в настройках FBE. Входных
параметров нет; результат — строка из одного символа.

```js
var nbsp = window.external.GetNBSP();
paragraph.innerText = "Иван" + nbsp + "Иванов";
```

<a id="isfastmode"></a>
### `IsFastMode()`

Возвращает `true`, если в FBE включён быстрый режим просмотра. Входных
параметров нет; результат — `true` или `false`.

```js
if (window.external.IsFastMode()) {
  window.external.MsgBox("Скрипт запущен в быстром режиме.");
}
```

<a id="getviewwidth"></a>
### `GetViewWidth()`

Возвращает текущую ширину рабочей области редактора в пикселях. Входных
параметров нет; результат — целое число.

```js
var width = window.external.GetViewWidth();
```

<a id="getviewheight"></a>
### `GetViewHeight()`

Возвращает текущую высоту рабочей области редактора в пикселях. Входных
параметров нет; результат — целое число.

```js
var height = window.external.GetViewHeight();
```

## Изображения и файлы

<a id="getbinarysize"></a>
### `GetBinarySize(data)`

Возвращает размер двоичных данных в байтах. Параметр должен быть строкой с
двоичным содержимым, например `base64data` элемента `binary`, а не строкой
base64-представления. На вход передаётся двоичная строка; результат — целое
число байтов.

```js
var size = window.external.GetBinarySize(binary.base64data);
// Например: 15342.
```

<a id="savebinary"></a>
### `SaveBinary(path, data, prompt)`

Сохраняет двоичные данные `data` в файл и возвращает `true` только после полной
успешной записи.

При `prompt = true` открывается стандартный диалог Save As. Если выбран
существующий файл, Windows запрашивает подтверждение замены; после
подтверждения файл заменяется. Отмена диалога возвращает `false`, как и
реальная ошибка записи (она может быть показана пользователю).

При `prompt = false` диалог не открывается: используется путь `path`. Если
целевой файл отсутствует, он создаётся. Если он уже существует, метод не
заменяет его и возвращает `false`; существующий файл остаётся неизменным.
`FILE_EXISTS` в этом режиме — ожидаемый результат для вызывающего скрипта, а
не повод показывать сообщение пользователю. Любая ошибка записи также
возвращает `false` без показа диалога.

Вход: путь, двоичные данные и флаг показа диалога; результат — `true` или
`false`.

```js
var saved = window.external.SaveBinary("cover.jpg", binary.base64data, false);
```

<a id="getimagedimsbypath"></a>
### `GetImageDimsByPath(path)`

Возвращает размеры изображения по пути строкой вида `"640x480"`. Если файл
не удалось прочитать или формат не поддержан, возвращает пустую строку. На
вход передаётся строка пути; результат — строка размера или `""`.

```js
var dims = window.external.GetImageDimsByPath("D:\\Pictures\\cover.jpg");
// "640x480" при успехе, "" при ошибке.
```

<a id="getimagedimsbydata"></a>
### `GetImageDimsByData(data)`

Возвращает размеры изображения строкой `"ширинаxвысота"` по двоичным данным.
Передавайте значение `base64data` элемента `binary`; для произвольной строки
JavaScript метод не предназначен. При ошибке возвращается пустая строка. На
вход передаются двоичные данные `base64data`; результат — строка размера или
`""`.

```js
var dims = window.external.GetImageDimsByData(binary.base64data);
```

## Элементы описания и жанры

<a id="genrepopup"></a>
### `GenrePopup(element, x, y)`

Открывает меню жанров в экранной точке `(x, y)` и возвращает идентификатор
выбранного жанра FB2. Если меню закрыто без выбора, результат — `null`.
Параметр `element` сохранён для совместимости и в текущей реализации не
используется. Вход: DOM-элемент и экранные координаты; результат — строковый
идентификатор жанра или `null`.

```js
var genre = window.external.GenrePopup(paragraph, event.screenX, event.screenY);
if (genre !== null) {
  window.external.MsgBox("Выбран жанр: " + genre);
}
```

<a id="getextendedstyle"></a>
### `GetExtendedStyle(elementId)`

Возвращает состояние настройки расширенного показа элемента описания с данным
идентификатором. На вход передаётся строковый идентификатор; результат —
`true` или `false`.

```js
var isShown = window.external.GetExtendedStyle("annotation");
```

<a id="descshowelement"></a>
### `DescShowElement(elementId, show)`

Включает или выключает расширенный показ элемента описания и сохраняет это
значение в настройках FBE. На вход передаются идентификатор и булево значение;
результат не используется.

```js
window.external.DescShowElement("annotation", true);
```

<a id="descshowmenu"></a>
### `DescShowMenu(button, x, y)`

Открывает меню доступных компонентов описания в экранной точке `(x, y)` и
возвращает идентификатор выбранного элемента. Если выбор отменён, результат
не определён; проверяйте его перед использованием. Параметр `button`
сохранён для совместимости и в текущей реализации не используется. Вход:
произвольный объект кнопки и экранные координаты; результат — строковый
идентификатор выбранного элемента либо пустое/неопределённое значение при
отмене.

```js
var elementId = window.external.DescShowMenu(button, event.screenX, event.screenY);
if (elementId) {
  window.external.MsgBox("Выбран элемент: " + elementId);
}
```


## Версионированное расширение (API 1.0.0, issue #45)

Версия API независима от версии редактора. Старый dual-интерфейс
`IExternalHelper`, IID, порядок vtable и DISPID 1–35 не меняются. Новые методы
доступны через `IDispatch::GetIDsOfNames/Invoke` у того же `window.external`;
старый typelib намеренно описывает только прежний dual-контракт. Расширение
не требует регистрации нового COM-сервера. Вызовы позиционные, без именованных
COM-аргументов. Имена методов нечувствительны к регистру.

| Метод (впервые в 1.0.0) | Параметры и результат |
|---|---|
| `GetScriptingApiVersion()` | Без параметров; текущая версия строкой (`1.1.0`). |
| `HasScriptingMethod(name)` | Строковое имя; boolean фактической доступности публичного метода, включая старые методы; неизвестное имя даёт false. Метод не вызывается. |
| `GetScriptingCapabilities()` | JSON-строка со `schemaVersion: 1`, `apiVersion: "1.1.0"` и объектом `capabilities`. |
| `GetCurrentScriptPath()` | Абсолютный путь выполняемого JS-файла; без контекста пустая строка. |
| `GetCurrentScriptDirectory()` | Каталог этого файла с завершающим `\`; без контекста пустая строка. |
| `GetCurrentScriptName()` | Имя этого JS-файла с расширением; без контекста пустая строка. |
| `GetCurrentScriptId()` | UID существующего ScriptRegistry/ScriptDescriptor; незарегистрированный скрипт и отсутствие контекста дают пустую строку. Новый UID не создаётся. |
| `GetScriptsRootDirectory()` | Абсолютный настроенный каталог Scripts; завершающий разделитель может сохраняться. |
| `GetInterfaceLanguage()` | Текущая эффективная локаль интерфейса, например `ru-RU` или `en-US`. |
| `GetCurrentViewMode()` | Строка `design`, `source` или `description`. |
| `IsDocumentModified()` | Boolean штатного dirty-state, включая Body, Description и несохранённый Source. |

Стабильные ключи capabilities: `scriptContext` (сведения и стек контекстов),
`environment` (окружение и состояние документа). Значение true означает
реализованную возможность; отсутствующий/неизвестный ключ нельзя считать
поддерживаемой возможностью. Новые ключи могут добавляться без изменения схемы.
Служебные методы хоста и их DISPID не являются публичным API.

Контекст действует при загрузке и выполнении скрипта. Вложенный `apiRunCmd`
временно переключает его на вложенный файл, после возврата/исключения
восстанавливается внешний контекст. Нативный запуск дополнительно защищён
RAII. Чтение UID не запускает обнаружение, хеширование или проверку всей
библиотеки. Перенос зарегистрированного скрипта отслеживается штатным реестром
при обновлении каталога. Произвольный вызов ранее сохранённой JS-функции вне
служебных точек запуска не создаёт контекст файла.

Пример для старого Microsoft JScript (включая безопасный fallback в старом FBE):

```javascript
function HasApi(name) {
    try { return window.external.HasScriptingMethod(name); }
    catch (error) { return false; }
}
if (HasApi("GetCurrentScriptDirectory")) {
    var resources = window.external.GetCurrentScriptDirectory();
}
```

Неверное число аргументов новых методов даёт `DISP_E_BADPARAMCOUNT`, неверный
тип — `DISP_E_TYPEMISMATCH`; старое поведение методов при этом не меняется.
Предел вложенности контекстов — 256; ошибочный путь даёт `E_INVALIDARG`.

### Диагностика существующего контракта

- `window.external.IsDiagnosticTraceEnabled()` возвращает boolean включённой
  диагностической трассировки.
- `window.external.TraceScript(code, message)` записывает диагностическое
  событие; не изменяет документ. Код ограничен 32 символами, сообщение
  очищается штатной диагностической подсистемой и ограничено 512 символами.
- `window.external.GetLocalizedString(key)` возвращает локализованную строку
  по ключу существующего каталога интерфейса. Это существующие DISPID 29–31.

### Покрытие issue #45

| Группа / методы | Статус |
|---|---|
| Версия, HasScriptingMethod, capabilities, контекст и сведения об окружении (этап 1А) | Implemented |
| GetScriptSetting, SetScriptSetting, RemoveScriptSetting, ClearScriptSettings (этап 1Б) | Implemented |
| SelectOpenFile, SelectSaveFile, SelectFolder, InputBoxEx (этап 1В) | Implemented |
| Закладки/текст/HTML выделения, Begin/EndDocumentUpdate, прогресс/отмена, текстовые файлы и операции путей (этап 2) | Implemented |
| Дополнительные binary, validation, diagnostics, dialogs, clipboard, status-stack, save API (этап 3) | Planned |
| GetSourceText, ValidateSourceText, GetLastSourceDiagnostic, ApplySourceText; существующие Undo, InputBox/GetModalResult, SaveBinary, пути документа | Already covered by existing API |
| ProcessPendingUiEvents и автоматический откат произвольных DOM-изменений | Deferred |

Issue #45 остаётся открытым до реализации и приёмки последующих этапов.
Интерфейс настройки карточек issue #48 в этот этап не входит; исходные карточки
и переменные в пользовательских JS-файлах не изменяются.


### Настройки скрипта (API 1.0.0)

| Метод | Параметры и результат |
|---|---|
| `GetScriptSetting(name, defaultValue)` | Строковое имя и значение по умолчанию; возвращает сохранённое значение совместимого типа либо defaultValue. |
| `SetScriptSetting(name, value)` | Имя и строка, конечное число либо boolean; boolean успешной атомарной записи. |
| `RemoveScriptSetting(name)` | Удаляет один параметр; boolean успеха. Отсутствующий параметр также можно удалить. |
| `ClearScriptSettings()` | Удаляет параметры только выполняемого скрипта; boolean успеха. |

`capabilities.scriptSettings = true` означает наличие этих четырёх методов.
Требуется активный контекст скрипта; вне него возвращается COM-ошибка
`E_UNEXPECTED`. Имя непустое, без нулевого символа, до 256 UTF-16 кодовых единиц. Строковое значение
и строковое значение по умолчанию ограничены 32768 UTF-16 кодовыми единицами.
Разные виды чисел совместимы как числа JavaScript; boolean и строка с ними не
смешиваются. NaN, Infinity, объекты, массивы, функции, null и undefined не
сериализуются и отклоняются с `E_INVALIDARG`. Для отсутствующего параметра,
несовместимого типа или недоступного/повреждённого файла чтение возвращает
переданный defaultValue. Unicode, переводы строк и вложенные нулевые символы
строк сохраняются без потерь.

Файл `ScriptSettings.xml` расположен в `DeploymentContext::SettingsDirectory()`:
пользовательский каталог установленной версии либо Data/Settings в portable.
Файл содержит не больше 1024 параметров для всех скриптов и занимает не больше
1 MiB после сериализации. Превышение общего лимита даёт false при записи.
Имена и строковые значения хранятся в кодировке hex UTF-16 внутри XML, не в
исходных JS. Изоляция — `uid:<существующий UID>`; резервная область
`path:<абсолютный путь с нормализованными разделителями и регистром>` хранит
сам путь, без хеша и коллизий хеширования. В резервном режиме переименование
создаёт новую область настроек. С зарегистрированным прежним UID область
сохраняется после штатного обновления каталога.

Запись сериализуется межпроцессным файлом `ScriptSettings.xml.lock`, выполняется
через временный файл в том же каталоге, flush и атомарное переименование.
Ошибка доступа/блокировки/записи возвращает false; прежний файл не затирается.
Повреждённый файл автоматически не исправляется и не перезаписывается.
Настройки карточек issue #48 и исходные переменные скриптов не затрагиваются.

```javascript
if (HasApi("GetScriptSetting")) {
    var enabled = window.external.GetScriptSetting("option", true);
    if (!window.external.SetScriptSetting("option", enabled)) {
        window.external.MsgBox("Не удалось сохранить настройку");
    }
}
```


### Файловые диалоги (API 1.0.0)

| Метод | Параметр и результат |
|---|---|
| `SelectOpenFile(optionsJson)` | JSON-объект строкой; выбирает существующий файл, либо несколько при multiSelect=true. |
| `SelectSaveFile(optionsJson)` | JSON-объект строкой; выбирает путь сохранения. Файл не записывается. Подтверждение существующего файла обязательно. |
| `SelectFolder(optionsJson)` | JSON-объект строкой; выбирает существующий файловый каталог, либо несколько при multiSelect=true. |

`capabilities.fileDialogs = true` означает эти три метода. Общие поля:

| Поле | Тип / поведение |
|---|---|
| `title` | Строка заголовка, необязательная. |
| `initialDirectory` | Строка начального каталога; ошибка Shell при его установке возвращается как error. |
| `initialFileName` | Строка начального имени файла, необязательная. |
| `defaultExtension` | Строка расширения, например `fb2`, необязательная. |
| `multiSelect` | Boolean, по умолчанию false; для SelectSaveFile значение true отклоняется. |
| `filters` | Массив до 32 объектов `{ "name": "Книги", "pattern": "*.fb2;*.zip" }`; строки непустые, до 256 UTF-16 единиц. Для SelectFolder фильтры отклоняются. |

При успехе возвращается JSON-строка `{"status":"ok","path":"..."}` либо
`{"status":"ok","paths":["...","..."]}` для multiSelect=true. При отмене:
`{"status":"cancelled"}`. Ошибка отличается от отмены:

```json
{"status":"error","code":"invalid-options","hresult":"0x80070057","message":"..."}
```

Стабильные машинные коды: `invalid-options` (неверный JSON/параметры),
`owner-unavailable` (нет живого окна-владельца), `dialog-failed` (ошибка Shell).
`hresult` — строка hexadecimal HRESULT; `message` — диагностическая строка
Windows, на её текст/язык нельзя опираться в коде. Вход — JSON-объект до
65536 UTF-16 единиц. Неизвестные и повторные поля, неверные типы и нулевые
символы в строках отклоняются. Одна строка ограничена 32767 UTF-16 единицами.
Unicode, UNC и длинные пути передаются Shell без буфера MAX_PATH; фактическая
допустимость пути определяется оболочкой и файловой системой Windows.

Окно-владелец — редактор, используется ModernFileDialog и штатная защита его
системного оформления. Старые диалоги не переводятся в новый режим параметров.
Выбор файла/каталога не открывает его в редакторе и не изменяет документ.
Выбор пути сохранения сам по себе не создаёт и не заменяет файл.

### InputBoxEx (API 1.0.0)

`window.external.InputBoxEx(optionsJson)` принимает JSON-объект строкой с
необязательными строковыми полями `title`, `prompt`, `value` (по умолчанию
пустые). Ограничения JSON/строк — как у файловых диалогов. Есть кнопки OK и
отмены; начальное значение и подтверждённый ввод могут быть пустыми.
`capabilities.inputBoxEx = true` означает наличие этого метода.

Подтверждённый пустой ввод возвращает `{"accepted":true,"value":""}`;
отмена — `{"accepted":false,"value":""}`. Ошибка возвращает
`accepted:false`, `status:"error"`, `code`, `hresult`, `message` по общей схеме.
Результат не записывается в старый GetModalResult; поведение старого InputBox
не меняется. Используются штатные раскладка/DPI/оформление поля ввода и
явный владелец-редактор.

Примеры без современной синтаксики JavaScript и без обязательного глобального
JSON (он отсутствует в некоторых режимах MSHTML):

```javascript
if (HasApi("SelectOpenFile")) {
    var selectionJson = window.external.SelectOpenFile(
        '{"title":"Выберите книгу","filters":[{"name":"FB2","pattern":"*.fb2"}]}');
}
if (HasApi("InputBoxEx")) {
    var inputJson = window.external.InputBoxEx(
        '{"title":"Имя","prompt":"Введите значение","value":""}');
}
```

Результаты — текст JSON; их можно разобрать уже используемым в скрипте
JSON-парсером. API не добавляет/не заменяет глобальный JSON и не ограничивает
ActiveX, COM или DOM. Не используйте eval для текста FB2/XML или произвольных
внешних данных.

### Статусы методов issue #45

| Методы / семейство | Статус |
|---|---|
| CreateSelectionBookmark, RestoreSelectionBookmark, ReleaseSelectionBookmark, GetSelectionText, GetSelectionHtml, GetSelectionInfo | Implemented |
| BeginDocumentUpdate, EndDocumentUpdate | Implemented |
| BeginProgress, SetProgress, IsProgressCancelled, EndProgress | Implemented |
| ReadTextFile, WriteTextFile, WriteTextFileEx, FileExists, DirectoryExists, CreateDirectory | Implemented |
| CombinePath, GetFileName, GetDirectoryName, GetExtension, GetTempDirectory, GetTempFileName | Implemented |
| GetBinaryInfo, AddBinaryFromFile, ReplaceBinaryFromFile, RemoveBinary, SaveAllBinaries, GenerateUniqueBinaryId | Planned |
| ValidateCurrentDocument, LogMessage, GetLastScriptingError, ClearLastScriptingError | Planned |
| ShowMessage, SelectFromList, SelectMultipleFromList, ChooseColor | Planned |
| GetClipboardText, SetClipboardText, GetClipboardHtml, SetClipboardHtml | Planned |
| PushStatusBarText, PopStatusBarText, ClearStatusBarText | Planned |
| SaveCurrentDocument, SaveCurrentDocumentAs, SaveDocumentCopy, SaveSerializedDocumentCopy | Planned |
| ProcessPendingUiEvents, автоматический откат произвольного DOM | Deferred |
| GetSourceText, ValidateSourceText, GetLastSourceDiagnostic, ApplySourceText | Already covered by existing API |

Наличие этих названий в плане не делает их доступными. Проблема зависаний
issue #41 и интерфейс параметров карточек issue #48 не включены в этап 1.

## Этап 2: API 1.1.0

Новые методы опубликованы через аддитивный IDispatch, DISPID 119–142.
WriteTextFileEx добавлен с новым DISPID 142; опубликованные DISPID 100–141
и допустимые вызовы WriteTextFile не изменены.
Прежние IID, typelib, сигнатуры и DISPID 1–35, а также методы API 1.0.0
сохранены. GetScriptingApiVersion возвращает `1.1.0`. В capabilities добавлены
булевы ключи `selection`, `documentUpdate`, `progress`, `textFiles`, `paths`.
Методы этапа 3 не объявляются доступными. Issue #45 остаётся открытым.

### Проверка этапов 1–2

Адресный runtime-контур запускается на Release Win32 отдельно в режимах
`portable` и `installed`. Для каждого режима предусмотрены четыре проверки:

```powershell
foreach ($mode in 'portable', 'installed') {
    ./tools/tests/test-fbe-scripting-stage1.ps1 -Mode $mode
    ./tools/tests/test-fbe-scripting-stage1.ps1 -Mode $mode -Dialogs
    ./tools/tests/test-fbe-scripting-stage1.ps1 -Mode $mode -Environment
    ./tools/tests/test-fbe-scripting-stage2.ps1 -Mode $mode
}
```

При отдельном каталоге сборки всем командам передаётся `-FbeExe` с путём к
проверяемому FBE.exe. `installed` перенаправляет файлы настроек в отдельный
тестовый каталог; этот контур не проверяет установщик.
Проверка диалогов ожидает появления фокуса с ограниченным таймаутом, затем
проверяет владельца, блокировку редактора, фокус, монитор, подтверждение и отмену.

Контур совместимости включает настоящие JScript/MSHTML, три штатных скрипта
изменения регистра, прежние Undo/Redo, запуск команды меню и вложенные вызовы.
F2 проверяется с явным назначением команды последнего скрипта; штатная
раскладка клавиш при этом не меняется. Это адресная регрессия, а не проверка
всего каталога пользовательских скриптов.

`test-fbe-typelib-runtime.ps1` проверяет встроенную typelib без регистрации COM:
IID IExternalHelper, DISPID 1–35, виды вызовов, типы параметров и результата.
Несовпадение числа параметров отклоняется до обращения к таблице их типов.
Неизменность исходного IDL дополнительно проверяют runtime-скрипты.

Проверка импортов бинарника не заменяет запуск на Windows 7 SP1. При проверке
2026-10-19 запуск на Windows 7 имеет статус `NOT RUN`; поддержка этой ОС по
одной проверке импортов не подтверждается. Отложенная орфография, обновление
дерева и автоматический откат произвольного DOM по-прежнему не реализованы.

### Выделение и закладки

| Метод | Параметры / результат |
|---|---|
| CreateSelectionBookmark() | Непрозрачная строка-токен текущего выделения/каретки. |
| RestoreSelectionBookmark(token) | Boolean; восстанавливает существующую допустимую закладку. |
| ReleaseSelectionBookmark(token) | Boolean; освобождает закладку, false для неизвестной/освобождённой. |
| GetSelectionText() | Строка текста; пустая для отсутствующего текстового выделения. |
| GetSelectionHtml() | HTML текстового/control-выделения; в Source — HTML-экранированный исходный текст. |
| GetSelectionInfo() | JSON-строка: schemaVersion=1, view, type, textLength (UTF-16), controlCount. |

В Body/Description используется штатный MSHTML range, в Source — байтовые
позиции Scintilla с преобразованием текста из UTF-8. Тип: `text`, `control`
или `none`; view: `design`, `description`, `source`. Чтение и создание
закладок не меняют документ/Undo. Восстановление меняет только выделение.

В Description поддерживается также текстовое выделение внутри полей формы
(например, названия книги); оно читается и восстанавливается в том же режиме.
Обычные GetSelectionText/GetSelectionHtml/GetSelectionInfo в Source читают
только выделенный диапазон Scintilla, без копирования полного XML. UTF-8 позиции
переводятся в текст UTF-16; supplementary Unicode-символ занимает две единицы
в textLength. Это не меняет правила хранения и проверки закладок ниже.

Закладка принадлежит текущему контексту выполнения скрипта и не переносится
во вложенный скрипт. В конце контекста все закладки освобождаются. В одном
контексте допускается до 128 закладок. Restore не переключает режим редактора:
при несовпадении режима, идентичности документа/корня активного представления или версии MSHTML
возвращается false и закладка освобождается. В Source дополнительно сравниваются
полные исходные байты. Поэтому DOM-изменение консервативно инвалидирует
закладку; автоматическое следование за произвольными DOM-правками не обещается.
Полная замена документа через Source/ApplySourceText также инвалидирует её.
Суммарный объём Source-снимков контекста ограничен 64 МиБ. Закладки не предназначены для постоянного хранения.

Для Description дополнительно сравниваются текущие значения полей
формы: изменение input.value может не менять версию Body. При изменении
Description старая закладка отклоняется. Для снимков значений Description
действует отдельный лимит 64 МиБ на контекст; прежний бюджет Source не уменьшается.

### Пакетные изменения

`BeginDocumentUpdate(action, optionsJson)` возвращает строковый токен.
`action` — строка названия Undo, `optionsJson` — `{}` либо
`{"suspendRedraw":true}` (по умолчанию false). Неизвестные поля/типы
отклоняются. `EndDocumentUpdate(token)` завершает последнюю операцию текущего
контекста и возвращает true. Вложенные операции закрываются в обратном порядке;
чужой, устаревший или нарушающий порядок токен вызывает ошибку.

Используются существующие MSHTML undo units или SCI_BEGIN/ENDUNDOACTION в
Source. При suspendRedraw временно подавляется перерисовка активной поверхности;
вложенные операции совместно учитывают её состояние. Исходная видимость
сохраняется, перерисовка восстанавливается при End и при выходе из скрипта,
включая исключение. Подсветка опечаток, настройки и обработка DOM не отключаются.
Пакетный режим не гарантирует устранения зависаний issue #41.

Отложенная проверка орфографии и отложенное обновление дерева **не реализованы**
как возможности API 1.1. Пакет не подавляет соответствующие уведомления или
фоновые проверки; гарантируются группировка Undo и учёт suspendRedraw.
Вложенные suspendRedraw включают перерисовку и запрашивают её обновление только
при закрытии последней подавляющей группы для данного окна. В Description
используется общий с Body MSHTML undo manager; группа может менять поля
Description без изменения содержимого Body.

Это группировка Undo, а не транзакционный откат произвольного DOM. При ошибке
уже сделанные изменения остаются и могут быть отменены пользователем через
штатный Undo. Не переключайте документ/режим внутри открытого пакета. ApplySourceText
возвращает false с диагностикой, пока открыт хотя бы один пакет.

```javascript
if (HasApi("BeginDocumentUpdate")) {
    var token = window.external.BeginDocumentUpdate("Моя правка", "{}");
    try {
        // Изменения существующим DOM/Scintilla-механизмом скрипта.
    } finally {
        window.external.EndDocumentUpdate(token);
    }
}
```

### Прогресс и отмена

| Метод | Параметры / результат |
|---|---|
| BeginProgress(title, maximum) | Строка title, целое maximum 1…4294967295; возвращает строковый токен. |
| SetProgress(token, value, message) | Целое value 0…maximum, строка message; true при успехе. |
| IsProgressCancelled(token) | Boolean; пользователь нажал отмену. |
| EndProgress(token) | Закрывает окно и возвращает true. |

Используется системный IProgressDialog с владельцем-редактором. Разрешено одно
активное окно на UI-поток; токен принадлежит текущему контексту. Окно закрывается
также при выходе/исключении, владелец разблокируется. Отмена кооперативная:
скрипт должен проверить IsProgressCancelled и прекратить работу самостоятельно.
Опрос отмены не выполняет произвольный message pump редактора.
ProcessPendingUiEvents не добавлен. Отмена не откатывает сделанные изменения.

```javascript
if (HasApi("BeginProgress")) {
    var progress = window.external.BeginProgress("Обработка", 100);
    try {
        for (var i = 0; i < 100; ++i) {
            if (window.external.IsProgressCancelled(progress)) break;
            window.external.SetProgress(progress, i + 1, "Шаг " + (i + 1));
        }
    } finally { window.external.EndProgress(progress); }
}
```

### Текстовые файлы и пути

| Метод | Параметры / результат |
|---|---|
| ReadTextFile(path[, encoding]) | Строка содержимого; без encoding распознаются BOM UTF-8/UTF-16LE, иначе UTF-8. |
| WriteTextFile(path, text[, encoding]) | Полная атомарная замена через временный файл рядом с назначением, flush и переименование; true при успехе. По умолчанию UTF-8 без BOM. |
| WriteTextFileEx(path, text, overwrite[, encoding]) | Аддитивный безопасный вариант: overwrite — обязательный Boolean. При false создаёт только новый файл; существующий файл сохраняется, вызов выдаёт COM-ошибку. При true разрешает атомарную замену. Кодировки и результат как у WriteTextFile. |
| FileExists(path) | Boolean: доступный существующий файл (не каталог). |
| DirectoryExists(path) | Boolean: доступный существующий каталог. |
| CreateDirectory(path) | Создаёт каталог с родителями; true и для уже существующего каталога. |
| CombinePath(directory, name) | Абсолютный нормализованный Windows-путь; абсолютный второй аргумент заменяет первый. |
| GetFileName(path) | Последний компонент пути, без доступа к диску. |
| GetDirectoryName(path) | Каталог без завершающего разделителя, кроме корня диска; пустая строка для имени без каталога. |
| GetExtension(path) | Суффикс последнего компонента с точкой, либо пустая строка. |
| GetTempDirectory() | Системный временный каталог с завершающим разделителем. |
| GetTempFileName([prefix[, extension]]) | Создаёт уникальный пустой файл во временном каталоге и возвращает путь; удаление — ответственность вызывающего. Без аргументов сохранены prefix=`fbe-` и extension=`.tmp`. |

Кодировки: `utf-8`, `utf-8-bom`, `utf-16le` (запись с BOM), `windows-1251`.
При явной кодировке соответствующий BOM пропускается. Невалидный UTF-8/UTF-16
и непредставимые символы Windows-1251 отклоняются без потери прежнего файла.
NUL в содержимом сохраняется, NUL в путях/названиях кодировки недопустим.
Предел файла/результата записи — 8 МиБ; входного текста — 8 Ми UTF-16 единиц.
Каталог назначения WriteTextFile должен существовать. Файл read-only не
перезаписывается. Создание каталогов использует существующий Shell API;
ограничения путей определяются Windows (включая MAX_PATH для этой операции).

Старые вызовы WriteTextFile с двумя или тремя параметрами сохраняют поведение
API 1.1: существующий файл заменяется. Для новых скриптов рекомендуется
WriteTextFileEx с overwrite=false; флаг false проверяется атомарным переименованием
без замены, поэтому файл, созданный другим процессом непосредственно перед
завершением записи, также не перезаписывается. При ошибке преобразования,
записи, flush или переименования исходный файл сохраняется, временный файл
удаляется. overwrite=true следует передавать после осознанного выбора пользователя.

Кратковременные отказы доступа/совместного использования при завершении записи
текстового файла и захвате блокировки настроек скрипта повторяются до 10 раз
с паузой 10 мс. При сохранении настроек временный файл записывается и сбрасывается
один раз; повторяется только атомарное переименование, до 50 раз с паузой 10 мс.
Ожидание между попытками ограничено 500 мс; время самой файловой операции
определяется Windows. Постоянный
отказ не скрывается: файловые методы выдают COM-ошибку, методы записи настроек
возвращают false по прежнему контракту. Проверки существующего файла при
overwrite=false не заменяются предварительной проверкой FileExists.

Параметры GetTempFileName — строки длиной до 64 UTF-16 единиц каждая. Extension
может начинаться с точки; при её отсутствии точка добавляется. Пустая строка
extension означает имя без расширения. Разделители каталогов, управляющие
символы, `..`, недопустимые символы имени Windows и завершающие пробел/точка
отклоняются. Уникальный GUID добавляется между prefix и extension; файл
создаётся через CREATE_NEW, а не только подбирается свободное имя.
Относительные пути разрешаются относительно текущего рабочего каталога FBE,
а не каталога скрипта; для последнего используйте GetCurrentScriptDirectory.

При ошибке типов, параметров, кодировки, чтения или записи возникает COM-ошибка
с HRESULT, доступная JScript через try/catch. FileExists/DirectoryExists
возвращают false также при недоступности объекта. Операции не открывают FB2
в редакторе и не меняют текущий документ/dirty-state.

```javascript
if (HasApi("WriteTextFileEx")) {
    var path = window.external.CombinePath(
        window.external.GetCurrentScriptDirectory(), "result.txt");
    try { window.external.WriteTextFileEx(path, "Результат", false, "utf-8"); }
    catch (error) { window.external.MsgBox("Не удалось записать результат"); }
}
```

Методы ресурсов выделения/пакета/прогресса требуют активного контекста скрипта
(E_UNEXPECTED вне него). Все новые методы доступны из настоящего JScript,
типизированные COM-клиенты прежнего интерфейса продолжают использовать прежний
неизменённый контракт. На старом FBE проверяйте HasApi через try/catch, как в
разделе API 1.0.0; не полагайтесь на наличие нового метода без проверки.
