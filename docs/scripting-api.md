# API пользовательских скриптов FictionBook Editor Next

> **Актуальный публичный контракт: API 1.5.0, 104 метода** (`window.external`). Справочник составлен по исходному `IExternalHelper` (DISPID 1–35), таблице расширения DISPID 100–168 и документации `master` по состоянию на 21 октября 2026 года. Этапы 3Б, 3В и 3Г по issue #45 включены.

Скрипты выполняются внутри FBE Next под **Microsoft JScript/MSHTML**. `document` здесь — DOM активного представления, а **полный FB2/FBD XML** доступен отдельно через Source API. Расширение API не равно версии самого редактора: используйте `GetScriptingApiVersion()` / `HasScriptingMethod()`.

Этот документ организован по задачам. В оглавлении перечислены все публичные методы — ссылки ведут непосредственно к описаниям. **Не реализованные** функции вынесены в конец, чтобы их нельзя было перепутать с доступными.

## Быстрый старт

Проверка доступности нового метода при работе с разными версиями редактора:

```javascript
function HasApi(name) {
    try { return window.external.HasScriptingMethod(name); }
    catch (e) { return false; }
}

function Run() {
    if (HasApi("GetSelectionText")) {
        window.external.MsgBox(window.external.GetSelectionText());
    }
}
```

Безопасная группа Undo для старого DOM API:

```javascript
function Run() {
    window.external.BeginUndoUnit(document, "Нормализация разметки");
    try {
        // Изменяйте DOM документа.
    } finally {
        window.external.EndUndoUnit(document);
    }
}
```

При использовании новых методов проверяйте их наличие, а вызовы, способные выдавать COM-ошибки, оборачивайте в `try/catch`. В старом MSHTML **глобальный объект `JSON` может отсутствовать**: строки JSON, возвращаемые API, обрабатывайте совместимым JSON-парсером; не применяйте `eval` к XML книги или произвольным внешним данным.

## Оглавление: все 104 метода

Переходы привязаны к явным якорям `api-<имя>`; изменение формулировок заголовков не должно ломать ссылки.

### Версия API, контекст скрипта и окружение

- [`GetScriptingApiVersion`](#api-getscriptingapiversion)
- [`HasScriptingMethod`](#api-hasscriptingmethod)
- [`GetScriptingCapabilities`](#api-getscriptingcapabilities)
- [`GetCurrentScriptPath`](#api-getcurrentscriptpath)
- [`GetCurrentScriptDirectory`](#api-getcurrentscriptdirectory)
- [`GetCurrentScriptName`](#api-getcurrentscriptname)
- [`GetCurrentScriptId`](#api-getcurrentscriptid)
- [`GetScriptsRootDirectory`](#api-getscriptsrootdirectory)
- [`GetInterfaceLanguage`](#api-getinterfacelanguage)
- [`GetCurrentViewMode`](#api-getcurrentviewmode)
- [`IsDocumentModified`](#api-isdocumentmodified)

### Файл документа и общие сведения

- [`GetDocumentFilePath`](#api-getdocumentfilepath)
- [`GetDocumentFileName`](#api-getdocumentfilename)
- [`GetDocumentDirectory`](#api-getdocumentdirectory)
- [`GetStylePath`](#api-getstylepath)
- [`GetProgramVersion`](#api-getprogramversion)
- [`GetUUID`](#api-getuuid)
- [`GetNBSP`](#api-getnbsp)
- [`IsFastMode`](#api-isfastmode)
- [`GetViewWidth`](#api-getviewwidth)
- [`GetViewHeight`](#api-getviewheight)

### Редактирование DOM, Undo и исходный XML

- [`BeginUndoUnit`](#api-beginundounit)
- [`EndUndoUnit`](#api-endundounit)
- [`inflateBlock`](#api-inflateblock)
- [`InflateParagraphs`](#api-inflateparagraphs)
- [`SetStyleEx`](#api-setstyleex)
- [`BeginDocumentUpdate`](#api-begindocumentupdate)
- [`EndDocumentUpdate`](#api-enddocumentupdate)
- [`GetSourceText`](#api-getsourcetext)
- [`ValidateSourceText`](#api-validatesourcetext)
- [`GetLastSourceDiagnostic`](#api-getlastsourcediagnostic)
- [`ApplySourceText`](#api-applysourcetext)
- [`ValidateCurrentDocument`](#api-validatecurrentdocument)

### Сохранение и сериализация

- [`SaveCurrentDocument`](#api-savecurrentdocument)
- [`SaveCurrentDocumentAs`](#api-savecurrentdocumentas)
- [`SaveDocumentCopy`](#api-savedocumentcopy)
- [`SerializeCurrentDocument`](#api-serializecurrentdocument)
- [`SaveSerializedDocumentCopy`](#api-saveserializeddocumentcopy)

### Выделение и закладки

- [`CreateSelectionBookmark`](#api-createselectionbookmark)
- [`RestoreSelectionBookmark`](#api-restoreselectionbookmark)
- [`ReleaseSelectionBookmark`](#api-releaseselectionbookmark)
- [`GetSelectionText`](#api-getselectiontext)
- [`GetSelectionHtml`](#api-getselectionhtml)
- [`GetSelectionInfo`](#api-getselectioninfo)

### Диалоги и пользовательский ввод

- [`MsgBox`](#api-msgbox)
- [`AskYesNo`](#api-askyesno)
- [`InputBox`](#api-inputbox)
- [`GetModalResult`](#api-getmodalresult)
- [`SelectOpenFile`](#api-selectopenfile)
- [`SelectSaveFile`](#api-selectsavefile)
- [`SelectFolder`](#api-selectfolder)
- [`InputBoxEx`](#api-inputboxex)
- [`ShowMessage`](#api-showmessage)
- [`SelectFromList`](#api-selectfromlist)
- [`SelectMultipleFromList`](#api-selectmultiplefromlist)
- [`ChooseColor`](#api-choosecolor)

### Буфер обмена

- [`GetClipboardText`](#api-getclipboardtext)
- [`SetClipboardText`](#api-setclipboardtext)
- [`GetClipboardHtml`](#api-getclipboardhtml)
- [`SetClipboardHtml`](#api-setclipboardhtml)

### Описание книги и жанры

- [`GenrePopup`](#api-genrepopup)
- [`GetExtendedStyle`](#api-getextendedstyle)
- [`DescShowElement`](#api-descshowelement)
- [`DescShowMenu`](#api-descshowmenu)

### Строка состояния, прогресс и отмена

- [`SetStatusBarText`](#api-setstatusbartext)
- [`PushStatusBarText`](#api-pushstatusbartext)
- [`PopStatusBarText`](#api-popstatusbartext)
- [`ClearStatusBarText`](#api-clearstatusbartext)
- [`BeginProgress`](#api-beginprogress)
- [`SetProgress`](#api-setprogress)
- [`IsProgressCancelled`](#api-isprogresscancelled)
- [`EndProgress`](#api-endprogress)

### Двоичные вложения, текстовые файлы и пути

- [`GetBinarySize`](#api-getbinarysize)
- [`SaveBinary`](#api-savebinary)
- [`GetImageDimsByPath`](#api-getimagedimsbypath)
- [`GetImageDimsByData`](#api-getimagedimsbydata)
- [`GetBinaryInfo`](#api-getbinaryinfo)
- [`GenerateUniqueBinaryId`](#api-generateuniquebinaryid)
- [`AddBinaryFromFile`](#api-addbinaryfromfile)
- [`ReplaceBinaryFromFile`](#api-replacebinaryfromfile)
- [`RemoveBinary`](#api-removebinary)
- [`SaveAllBinaries`](#api-saveallbinaries)
- [`ReadTextFile`](#api-readtextfile)
- [`WriteTextFile`](#api-writetextfile)
- [`WriteTextFileEx`](#api-writetextfileex)
- [`FileExists`](#api-fileexists)
- [`DirectoryExists`](#api-directoryexists)
- [`CreateDirectory`](#api-createdirectory)
- [`CombinePath`](#api-combinepath)
- [`GetFileName`](#api-getfilename)
- [`GetDirectoryName`](#api-getdirectoryname)
- [`GetExtension`](#api-getextension)
- [`GetTempDirectory`](#api-gettempdirectory)
- [`GetTempFileName`](#api-gettempfilename)

### Настройки пользовательского скрипта

- [`GetScriptSetting`](#api-getscriptsetting)
- [`SetScriptSetting`](#api-setscriptsetting)
- [`RemoveScriptSetting`](#api-removescriptsetting)
- [`ClearScriptSettings`](#api-clearscriptsettings)

### Диагностика, журнал и локализация

- [`IsDiagnosticTraceEnabled`](#api-isdiagnostictraceenabled)
- [`TraceScript`](#api-tracescript)
- [`GetLocalizedString`](#api-getlocalizedstring)
- [`LogMessage`](#api-logmessage)
- [`GetLastScriptingError`](#api-getlastscriptingerror)
- [`ClearLastScriptingError`](#api-clearlastscriptingerror)

[Общие правила](#общие-правила-и-совместимость) · [Практические сценарии](#практические-задачи-и-готовые-решения) · [Ограничения](#ограничения-контракты-и-ошибки) · [Не реализовано](#запланировано-но-не-доступно-для-вызова)

### Готовые сложные скрипты

- [А. Аудит книги с отчётом](#complex-report)
- [Б. Неразрывные пробелы, пакетный Undo и прогресс](#complex-nbsp)
- [В. Инвентаризация изображений с экспортом TSV](#complex-images)
- [Г. Изменение названия книги через XML/MSXML6](#complex-title)
- [Д. Экспорт выделенной цитаты](#complex-selection)
- [Е. Резервная копия перед правкой](#complex-backup)
- [Ж. Проверка и экспорт Source](#complex-source-export)
- [З. Отчёт о выбранном вложении](#complex-binary-report)
- [И. Безопасный HTML в Clipboard](#complex-safe-clipboard)
- [К. Обработка с сохранением предпочтений](#complex-preferences)

## Общие правила и совместимость

- Заняты DISPIDs **1–35** и **100–168**; диапазон **36–99** не используется и не означает пропущенные методы справочника.
- Имена расширенных методов нечувствительны к регистру при поиске через IDispatch. Параметры передаются позиционно, не именованными COM-аргументами.
- `BSTR` соответствует строкам JScript; `BOOL` — `true`/`false`. Из-за различий COM/JScript не полагайтесь на современный синтаксис JavaScript (`let`, `const`, стрелочные функции и т.п.). Примеры ниже используют `var`, `try/finally` и простые функции.
- Всплывающие меню принимают **экранные** координаты `x`/`y` в пикселях.
- Старый `IExternalHelper` и DISPID 1–35 оставлены для совместимости. Новые методы присоединяются к тому же `window.external` через аддитивный `IDispatch`; регистрация нового COM-сервера не требуется.
- Для создания **одного** Undo используйте `BeginUndoUnit`/`EndUndoUnit` или `BeginDocumentUpdate`/`EndDocumentUpdate`. Ни один из этих методов не гарантирует автоматического отката произвольных уже сделанных DOM-правок при исключении. Завершайте открытую группу в `finally`.
- Для изменения XML всей книги берите `GetSourceText` → проверяйте `ValidateSourceText` → вызывайте `ApplySourceText`. Внутри открытого пакета `BeginDocumentUpdate` последняя операция недопустима.
- Часть методов (настройки, закладки, пакетный режим, прогресс, диагностика) требует активного контекста скрипта; вне него возможен `E_UNEXPECTED`. Вложенный запуск скрипта получает собственные контекст, ресурсы и состояние последней ошибки.
- Пути в текстовом API разрешаются относительно рабочего каталога FBE, **а не** папки скрипта. Для соседних файлов используйте `CombinePath(GetCurrentScriptDirectory(), name)`.

### Границы версий

| Версия / DISPID | Реализованные возможности |
|---|---|
| Старый интерфейс, **1–35** | Undo, разметка, базовые диалоги, свойства документа, изображения, Source XML, локализация и диагностика. |
| **1.0.0**, **100–118** | Проверка версии и capabilities, контекст скрипта, настройки скрипта, современные диалоги. |
| **1.1.0**, **119–142** | Выделение и закладки, пакетный Undo, прогресс, текстовые файлы и работа с путями. |
| **1.2.0**, **143–146** | Проверка текущего документа, логирование, последняя ошибка API. |
| **1.3.0**, **147–152** | Сведения о вложениях, свободный ID, добавление/замена/удаление с Undo и безопасный экспорт. |
| **1.4.0**, **153–163** | Расширенные диалоги, текстовый/HTML-буфер обмена и стек строки состояния текущего скрипта. |
| **1.5.0**, **164–168** | Сохранение текущей книги, Save As, независимые копии и сериализация без изменения документа. |

> `GetProgramVersion()` сообщает версию приложения; `GetScriptingApiVersion()` — **версию API**. Проверка только номера приложения недостаточна.

> **Как запускать примеры:** каждый блок «Пример → результат» — самостоятельный JS-скрипт с `function Run()`; сохраните его в отдельный `.js` и запустите внутри FBE Next. Не вставляйте сразу все функции `Run()` в один файл. Примеры, изменяющие книгу или создающие файлы, снабжены предупреждениями. Указанные пути, размеры и значения — образцы: реальные зависят от книги и окружения.

## Справочник методов

## Версия API, контекст скрипта и окружение

<a id="api-getscriptingapiversion"></a>

### `GetScriptingApiVersion()`

**Назначение:** Определяет версию расширенного интерфейса независимо от номера релиза редактора.

**Сигнатура и параметры:** `window.external.GetScriptingApiVersion()`.

DISPID **100** · API 1.0.0.

Параметров нет.

**Возвращаемое значение:** Строка, в актуальной реализации — `"1.5.0"`.

**Особенности, ограничения и ошибки:** В старых версиях метода может не быть: для совместимости проверяйте вызов через `try/catch`.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var v = ext.GetScriptingApiVersion();
    ext.MsgBox('API скриптов: ' + v);
}
```

**Ожидаемый результат:** Окно **«API скриптов: 1.5.0»** в актуальной реализации. Это не номер версии редактора.

<a id="api-hasscriptingmethod"></a>

### `HasScriptingMethod(name)`

**Назначение:** Проверяет, доступен ли публичный метод по строковому имени; сам метод не выполняет.

**Сигнатура и параметры:** `window.external.HasScriptingMethod(name)`.

DISPID **101** · API 1.0.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `name` | Да | Строка: имя публичного метода для проверки доступности. |

**Возвращаемое значение:** `true` или `false`.

**Особенности, ограничения и ошибки:** Проверяет и исходный интерфейс, и расширение. Имя нечувствительно к регистру. При работе со старым FBE отсутствие самого `HasScriptingMethod` перехватывайте через `try/catch`.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var available = ext.HasScriptingMethod('GetSelectionText');
    var planned = ext.HasScriptingMethod('GetClipboardText');
    ext.MsgBox('Выделение: ' + available + '; буфер обмена: ' + planned);
}
```

**Ожидаемый результат:** **«Выделение: true; буфер обмена: true»** для API 1.5.0. Методы не вызываются, проверяется лишь их наличие.

<a id="api-getscriptingcapabilities"></a>

### `GetScriptingCapabilities()`

**Назначение:** Получает сведения о поддерживаемых группах возможностей.

**Сигнатура и параметры:** `window.external.GetScriptingCapabilities()`.

DISPID **102** · API 1.0.0.

Параметров нет.

**Возвращаемое значение:** JSON-строка с `schemaVersion`, `apiVersion` и `capabilities`.

**Особенности, ограничения и ошибки:** В версии 1.5.0 доступны ключи: `scriptContext`, `environment`, `scriptSettings`, `fileDialogs`, `inputBoxEx`, `selection`, `documentUpdate`, `progress`, `textFiles`, `paths`, `documentValidation`, `logging`, `scriptingErrors`, `binaryReadExport`, `binaryObjects`, `extendedDialogs`, `clipboardText`, `clipboardHtml`, `statusStack`, `documentSave`, `documentCopies`, `documentSerialization`. Неизвестный ключ не считайте подтверждением возможности.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var caps = ext.GetScriptingCapabilities();
    ext.MsgBox(caps);
}
```

**Ожидаемый результат:** JSON с `"apiVersion":"1.5.0"` и `"capabilities"`, включая `"selection":true`, `"progress":true`, `"scriptingErrors":true` и возможности этапов 3В–3Г. Содержимое может расширяться в новых версиях.

<a id="api-getcurrentscriptpath"></a>

### `GetCurrentScriptPath()`

**Назначение:** Возвращает абсолютный путь выполняемого JS-файла.

**Сигнатура и параметры:** `window.external.GetCurrentScriptPath()`.

DISPID **103** · API 1.0.0.

Параметров нет.

**Возвращаемое значение:** Строка пути или `""`, если нет активного контекста.

**Особенности, ограничения и ошибки:** Вложенный запуск `apiRunCmd` временно переключает путь на вложенный скрипт.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var path = ext.GetCurrentScriptPath();
    ext.MsgBox(path || 'Контекст файла не определён');
}
```

**Ожидаемый результат:** Например, `C:\FBE\Scripts\Check.js`. При обычном запуске зарегистрированного скрипта показывается его путь.

<a id="api-getcurrentscriptdirectory"></a>

### `GetCurrentScriptDirectory()`

**Назначение:** Находит каталог, где лежит выполняемый скрипт.

**Сигнатура и параметры:** `window.external.GetCurrentScriptDirectory()`.

DISPID **104** · API 1.0.0.

Параметров нет.

**Возвращаемое значение:** Абсолютный путь **с завершающим `\`** или `""` без контекста.

**Особенности, ограничения и ошибки:** Подходит для хранения соседних ресурсов. Не путайте с рабочим каталогом FBE.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var dir = ext.GetCurrentScriptDirectory();
    ext.MsgBox('Каталог: ' + dir);
}
```

**Ожидаемый результат:** Например, `C:\FBE\Scripts\` — **с завершающим обратным слешем**.

<a id="api-getcurrentscriptname"></a>

### `GetCurrentScriptName()`

**Назначение:** Получает имя выполняемого JS-файла с расширением, но без каталога.

**Сигнатура и параметры:** `window.external.GetCurrentScriptName()`.

DISPID **105** · API 1.0.0.

Параметров нет.

**Возвращаемое значение:** Строка либо `""` вне контекста.

**Особенности, ограничения и ошибки:** Зависит от активного скриптового контекста; имя вложенного скрипта временно отличается от имени вызывающего.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var name = ext.GetCurrentScriptName();
    ext.MsgBox(name || '(без имени)');
}
```

**Ожидаемый результат:** Для файла `C:\FBE\Scripts\Check.js` выводится **`Check.js`**.

<a id="api-getcurrentscriptid"></a>

### `GetCurrentScriptId()`

**Назначение:** Получает UID скрипта из существующего реестра скриптов.

**Сигнатура и параметры:** `window.external.GetCurrentScriptId()`.

DISPID **106** · API 1.0.0.

Параметров нет.

**Возвращаемое значение:** Строка UID; пустая строка для незарегистрированного скрипта или вне контекста.

**Особенности, ограничения и ошибки:** Не создаёт новый UID и не сканирует всю библиотеку.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var uid = ext.GetCurrentScriptId();
    ext.MsgBox(uid ? 'UID: ' + uid : 'Скрипт не зарегистрирован');
}
```

**Ожидаемый результат:** UID из реестра скриптов; для незарегистрированного файла — «Скрипт не зарегистрирован». Новый UID не создаётся.

<a id="api-getscriptsrootdirectory"></a>

### `GetScriptsRootDirectory()`

**Назначение:** Возвращает настроенный корневой каталог пользовательских скриптов.

**Сигнатура и параметры:** `window.external.GetScriptsRootDirectory()`.

DISPID **107** · API 1.0.0.

Параметров нет.

**Возвращаемое значение:** Абсолютный путь.

**Особенности, ограничения и ошибки:** Завершающий разделитель может присутствовать; для склеивания путей используйте `CombinePath`.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var root = ext.GetScriptsRootDirectory();
    ext.MsgBox('Библиотека скриптов: ' + root);
}
```

**Ожидаемый результат:** Абсолютный путь к настроенному каталогу Scripts; в разных установках он различается.

<a id="api-getinterfacelanguage"></a>

### `GetInterfaceLanguage()`

**Назначение:** Получает активную эффективную локаль интерфейса.

**Сигнатура и параметры:** `window.external.GetInterfaceLanguage()`.

DISPID **108** · API 1.0.0.

Параметров нет.

**Возвращаемое значение:** Строка, например `"ru-RU"` или `"en-US"`.

**Особенности, ограничения и ошибки:** Локаль интерфейса не определяет язык книги и не меняет `<lang>`.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var lang = ext.GetInterfaceLanguage();
    ext.MsgBox('Язык интерфейса: ' + lang);
}
```

**Ожидаемый результат:** Например, **`ru-RU`** при русском интерфейсе или **`en-US`** при английском.

<a id="api-getcurrentviewmode"></a>

### `GetCurrentViewMode()`

**Назначение:** Определяет текущий режим отображения документа.

**Сигнатура и параметры:** `window.external.GetCurrentViewMode()`.

DISPID **109** · API 1.0.0.

Параметров нет.

**Возвращаемое значение:** Одна из строк `"design"`, `"source"`, `"description"`.

**Особенности, ограничения и ошибки:** Позволяет не обращаться к MSHTML Body при активном Source.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var mode = ext.GetCurrentViewMode();
    if (mode == 'source') ext.MsgBox('Открыт исходный XML');
    else ext.MsgBox('Текущий режим: ' + mode);
}
```

**Ожидаемый результат:** В режиме Source — «Открыт исходный XML»; иначе возвращается `design` либо `description`.

<a id="api-isdocumentmodified"></a>

### `IsDocumentModified()`

**Назначение:** Проверяет несохранённые изменения в текущем документе.

**Сигнатура и параметры:** `window.external.IsDocumentModified()`.

DISPID **110** · API 1.0.0.

Параметров нет.

**Возвращаемое значение:** `true` или `false`.

**Особенности, ограничения и ошибки:** Учитывает изменения Body, Description и несохранённый текст Source.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var modified = ext.IsDocumentModified();
    ext.MsgBox(modified ? 'Есть несохранённые изменения' : 'Изменений нет');
}
```

**Ожидаемый результат:** После сохранения книги — **«Изменений нет»**; после правки без сохранения — **«Есть несохранённые изменения»**.

## Файл документа и общие сведения

<a id="getdocumentfilepath"></a>

<a id="api-getdocumentfilepath"></a>

### `GetDocumentFilePath()`

**Назначение:** Возвращает абсолютный нормализованный путь к файлу текущей книги.

**Сигнатура и параметры:** `window.external.GetDocumentFilePath()`.

DISPID **26** · исходный интерфейс.

Параметров нет.

**Возвращаемое значение:** Строка пути или `""` для ещё не сохранённой книги / невозможности нормализации.

**Особенности, ограничения и ошибки:** Чтение пути не сохраняет книгу. Для новой книги возможна пустая строка; архивные пути интерпретируйте с учётом текущего контейнера.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var path = ext.GetDocumentFilePath();
    ext.MsgBox(path || 'Книга ещё не сохранена');
}
```

**Ожидаемый результат:** Например, `D:\Книги\Повесть.fb2`; для новой книги — «Книга ещё не сохранена».

<a id="getdocumentfilename"></a>

<a id="api-getdocumentfilename"></a>

### `GetDocumentFileName()`

**Назначение:** Возвращает имя текущего файла вместе с расширением, без каталога.

**Сигнатура и параметры:** `window.external.GetDocumentFileName()`.

DISPID **27** · исходный интерфейс.

Параметров нет.

**Возвращаемое значение:** Строка имени либо `""` до первого сохранения.

**Особенности, ограничения и ошибки:** Возвращает имя без каталога. Для выбора типа FB2/FBD внутри ZIP/RAR расширения контейнера недостаточно.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var file = ext.GetDocumentFileName();
    ext.MsgBox(file || '(имени пока нет)');
}
```

**Ожидаемый результат:** Если открыт `D:\Книги\Повесть.fb2`, выводится **`Повесть.fb2`**.

<a id="getdocumentdirectory"></a>

<a id="api-getdocumentdirectory"></a>

### `GetDocumentDirectory()`

**Назначение:** Получает каталог текущей книги.

**Сигнатура и параметры:** `window.external.GetDocumentDirectory()`.

DISPID **28** · исходный интерфейс.

Параметров нет.

**Возвращаемое значение:** Строка пути без завершающего `\`, кроме корня диска; `""` до сохранения.

**Особенности, ограничения и ошибки:** Используйте для создания отчётов рядом с книгой, предварительно убедившись, что книга сохранена.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var dir = ext.GetDocumentDirectory();
    ext.MsgBox(dir || 'Сначала сохраните книгу');
}
```

**Ожидаемый результат:** Например, `D:\Книги` без завершающего слеша; для новой книги — просьба сохранить.

<a id="getstylepath"></a>

<a id="api-getstylepath"></a>

### `GetStylePath()`

**Назначение:** Возвращает каталог установленной программы FBE.

**Сигнатура и параметры:** `window.external.GetStylePath()`.

DISPID **5** · исходный интерфейс.

Параметров нет.

**Возвращаемое значение:** Строка абсолютного пути, обычно без завершающего `\`.

**Особенности, ограничения и ошибки:** Это расположение приложения, а не каталог текущего скрипта.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var dir = ext.GetStylePath();
    ext.MsgBox('Каталог FBE: ' + dir);
}
```

**Ожидаемый результат:** Например, `C:\Program Files (x86)\FictionBook Editor Next`; это не путь к открытому документу.

<a id="getprogramversion"></a>

<a id="api-getprogramversion"></a>

### `GetProgramVersion()`

**Назначение:** Определяет версию запущенного редактора.

**Сигнатура и параметры:** `window.external.GetProgramVersion()`.

DISPID **22** · исходный интерфейс.

Параметров нет.

**Возвращаемое значение:** Строка вида `"3.0.9"` (значение зависит от релиза).

**Особенности, ограничения и ошибки:** Не используйте вместо `GetScriptingApiVersion` для проверки доступности новых методов.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    ext.MsgBox('Версия приложения: ' + ext.GetProgramVersion());
}
```

**Ожидаемый результат:** Например, **«Версия приложения: 3.0.9»**. Номер зависит от установленного релиза.

<a id="getuuid"></a>

<a id="api-getuuid"></a>

### `GetUUID()`

**Назначение:** Генерирует новый UUID.

**Сигнатура и параметры:** `window.external.GetUUID()`.

DISPID **8** · исходный интерфейс.

Параметров нет.

**Возвращаемое значение:** Строка UUID в верхнем регистре без фигурных скобок.

**Особенности, ограничения и ошибки:** Создание UUID не создаёт и не резервирует файл. Для записи без перезаписи всё равно передавайте `false` в безопасный файловый метод.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var id = ext.GetUUID();
    ext.MsgBox('UUID: ' + id);
}
```

**Ожидаемый результат:** Например, **`E89CE0E8-38D7-4FA4-9052-1CD9650C6D2F`**; при каждом вызове создаётся новое значение.

<a id="getnbsp"></a>

<a id="api-getnbsp"></a>

### `GetNBSP()`

**Назначение:** Получает используемый в настройках редактора символ неразрывного пробела.

**Сигнатура и параметры:** `window.external.GetNBSP()`.

DISPID **19** · исходный интерфейс.

Параметров нет.

**Возвращаемое значение:** Строка из одного символа.

**Особенности, ограничения и ошибки:** Для нормализации пробелов лучше использовать этот метод, а не жёстко зашитый код символа.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var nbsp = ext.GetNBSP();
    ext.MsgBox('Иван' + nbsp + 'Иванов; код символа: ' + nbsp.charCodeAt(0));
}
```

**Ожидаемый результат:** Между словами отображается неразрывный пробел; код соответствует выбранной настройке FBE (часто **160**).

<a id="isfastmode"></a>

<a id="api-isfastmode"></a>

### `IsFastMode()`

**Назначение:** Определяет, включён ли быстрый режим просмотра.

**Сигнатура и параметры:** `window.external.IsFastMode()`.

DISPID **15** · исходный интерфейс.

Параметров нет.

**Возвращаемое значение:** `true` или `false`.

**Особенности, ограничения и ошибки:** Читает текущий режим, но не переключает его и не запускает обработку книги.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    ext.MsgBox(ext.IsFastMode() ? 'Быстрый режим включён' : 'Обычный режим');
}
```

**Ожидаемый результат:** Одна из двух указанных строк — в зависимости от текущей настройки.

<a id="getviewwidth"></a>

<a id="api-getviewwidth"></a>

### `GetViewWidth()`

**Назначение:** Возвращает последнюю известную ширину рабочей области редактора.

**Сигнатура и параметры:** `window.external.GetViewWidth()`.

DISPID **20** · исходный интерфейс.

Параметров нет.

**Возвращаемое значение:** Целое число пикселей.

**Особенности, ограничения и ошибки:** Возвращает сохранённую редактором ширину области прокрутки в пикселях; вызов сам не измеряет DOM и не обрабатывает очередь UI.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var pixels = ext.GetViewWidth();
    ext.MsgBox('Ширина рабочей области: ' + pixels + ' px');
}
```

**Ожидаемый результат:** Число пикселей, например **`980 px`**; изменяется при изменении размеров редактора.

<a id="getviewheight"></a>

<a id="api-getviewheight"></a>

### `GetViewHeight()`

**Назначение:** Возвращает последнюю известную высоту рабочей области редактора.

**Сигнатура и параметры:** `window.external.GetViewHeight()`.

DISPID **21** · исходный интерфейс.

Параметров нет.

**Возвращаемое значение:** Целое число пикселей.

**Особенности, ограничения и ошибки:** Возвращает сохранённую редактором высоту области прокрутки в пикселях; значение обновляется при обработке изменений интерфейса.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var pixels = ext.GetViewHeight();
    ext.MsgBox('Высота рабочей области: ' + pixels + ' px');
}
```

**Ожидаемый результат:** Число пикселей, например **`640 px`**; зависит от размеров окна и панелей.

## Редактирование DOM, Undo и исходный XML

<a id="beginundounit"></a>

<a id="api-beginundounit"></a>

### `BeginUndoUnit(document, action)`

**Назначение:** Начинает именованную группу Undo для изменений DOM визуального редактора.

**Сигнатура и параметры:** `window.external.BeginUndoUnit(document, action)`.

DISPID **1** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `document` | Да | DOM-документ MSHTML текущего представления. |
| `action` | Да | Строка: название операции в истории Undo. |

**Возвращаемое значение:** Результат не используется.

**Особенности, ограничения и ошибки:** `document` — DOM текущего представления; `action` — текст действия в истории отмены. Обязательно закрывайте группу в `finally`.

**Пример → результат**

**Условие:** Только Design/редактируемый DOM; пример **изменяет** открытый документ.

```javascript
function FirstTextParagraph(root) {
    if (!root) return null;
    var paragraphs = root.getElementsByTagName('P');
    for (var i = 0; i < paragraphs.length; i++) {
        if (paragraphs[i].parentNode.className == 'section' &&
            /\S/.test(paragraphs[i].innerText)) return paragraphs[i];
    }
    return null;
}
function Run() {
    var ext = window.external;
    var root = document.getElementById('fbw_body');
    if (!root) { ext.MsgBox('Переключитесь в режим редактирования'); return; }
    var p = FirstTextParagraph(root);
    if (!p) { ext.MsgBox('Абзацев нет'); return; }
    ext.BeginUndoUnit(document, 'Пометить первый абзац');
    try { ext.SetStyleEx(document, p, 'subtitle'); }
    finally { ext.EndUndoUnit(document); }
    ext.MsgBox('Пометка поставлена. При необходимости нажмите Ctrl+Z.');
}
```

**Ожидаемый результат:** Первый абзац получает класс `subtitle`; правка группируется в **одно Undo**. Не запускайте на рабочей книге без необходимости.

<a id="endundounit"></a>

<a id="api-endundounit"></a>

### `EndUndoUnit(document)`

**Назначение:** Закрывает ранее начатую группу отмены.

**Сигнатура и параметры:** `window.external.EndUndoUnit(document)`.

DISPID **2** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `document` | Да | DOM-документ MSHTML текущего представления. |

**Возвращаемое значение:** Результат не используется.

**Особенности, ограничения и ошибки:** Передавайте тот же объект `document`, что и в `BeginUndoUnit`.

**Пример → результат**

**Условие:** Меняет DOM; запускайте в тестовой книге.

```javascript
function FirstTextParagraph(root) {
    if (!root) return null;
    var paragraphs = root.getElementsByTagName('P');
    for (var i = 0; i < paragraphs.length; i++) {
        if (paragraphs[i].parentNode.className == 'section' &&
            /\S/.test(paragraphs[i].innerText)) return paragraphs[i];
    }
    return null;
}
function Run() {
    var ext = window.external;
    var root = document.getElementById('fbw_body');
    if (!root) return;
    var p = FirstTextParagraph(root);
    if (!p) return;
    ext.BeginUndoUnit(document, 'Поставить пометку');
    try { p.appendChild(document.createTextNode(' [демо]')); }
    finally { ext.EndUndoUnit(document); }
    ext.MsgBox('Группа закрыта, Ctrl+Z отменит пометку.');
}
```

**Ожидаемый результат:** В конце первого абзаца появляется текст **` [демо]`**; группа отмены закрывается даже при исключении, а изменение можно отменить.

<a id="inflateblock"></a>

<a id="api-inflateblock"></a>

### `inflateBlock(element)`

**Назначение:** Индексированное COM-свойство расширенного отображения блочного элемента.

**Сигнатура и параметры:** `window.external.inflateBlock(element)`.

DISPID **3** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `element` | Да | DOM-элемент MSHTML, к которому применяется операция. |

**Возвращаемое значение:** При чтении — `true`/`false`; также поддерживается запись свойства.

**Особенности, ограничения и ошибки:** Используется для совместимости с классическим FBE. Синтаксис записи индексированного COM-свойства зависит от JScript/COM; для массового включения используйте `InflateParagraphs`.

**Пример → результат**

**Условие:** В редакторе должен быть хотя бы один абзац; для массовой работы предпочтителен `InflateParagraphs`.

```javascript
function FirstTextParagraph(root) {
    if (!root) return null;
    var paragraphs = root.getElementsByTagName('P');
    for (var i = 0; i < paragraphs.length; i++) {
        if (paragraphs[i].parentNode.className == 'section' &&
            /\S/.test(paragraphs[i].innerText)) return paragraphs[i];
    }
    return null;
}
function Run() {
    var ext = window.external;
    var root = document.getElementById('fbw_body');
    if (!root) return;
    var p = FirstTextParagraph(root);
    if (!p) return;
    var before = ext.inflateBlock(p);
    ext.inflateBlock(p) = true; // Только Microsoft JScript + COM, не Node.js!
    var after = ext.inflateBlock(p);
    ext.MsgBox('До: ' + before + '; после: ' + after);
}
```

**Ожидаемый результат:** После присваивания значение становится **`true`**. Запись `ext.inflateBlock(p) = true` — допустимый специальный синтаксис индексированного COM-свойства в Microsoft JScript; обычный JS-парсер его отвергнет.

<a id="inflateparagraphs"></a>

<a id="api-inflateparagraphs"></a>

### `InflateParagraphs(element)`

**Назначение:** Включает `inflateBlock` для вложенных элементов `P` указанного контейнера.

**Сигнатура и параметры:** `window.external.InflateParagraphs(element)`.

DISPID **7** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `element` | Да | DOM-элемент MSHTML, к которому применяется операция. |

**Возвращаемое значение:** Результат не используется.

**Особенности, ограничения и ошибки:** Работает с живым DOM; для одной отменяемой операции обрамляйте правку парой `BeginUndoUnit` / `EndUndoUnit`.

**Пример → результат**

```javascript
function FirstTextParagraph(root) {
    if (!root) return null;
    var paragraphs = root.getElementsByTagName('P');
    for (var i = 0; i < paragraphs.length; i++) {
        if (paragraphs[i].parentNode.className == 'section' &&
            /\S/.test(paragraphs[i].innerText)) return paragraphs[i];
    }
    return null;
}
function Run() {
    var ext = window.external;
    var root = document.getElementById('fbw_body');
    if (!root) return;
    var p = FirstTextParagraph(root);
    if (!p) return;
    ext.InflateParagraphs(root);
    ext.MsgBox('Первый P развёрнут: ' + ext.inflateBlock(p));
}
```

**Ожидаемый результат:** При успешной операции выводится **«Первый P развёрнут: true»**. Вызов применяется ко всем `P` переданного контейнера.

<a id="setstyleex"></a>

<a id="api-setstyleex"></a>

### `SetStyleEx(document, element, style)`

**Назначение:** Устанавливает элементу CSS-класс (атрибут `class`).

**Сигнатура и параметры:** `window.external.SetStyleEx(document, element, style)`.

DISPID **16** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `document` | Да | DOM-документ MSHTML текущего представления. |
| `element` | Да | DOM-элемент MSHTML, к которому применяется операция. |
| `style` | Да | Строка: имя CSS-класса FBE. |

**Возвращаемое значение:** Результат не используется.

**Особенности, ограничения и ошибки:** Первый аргумент сохранён для совместимости с историческим контрактом.

**Пример → результат**

**Условие:** Изменяет форматирование книги; сначала попробуйте в копии.

```javascript
function FirstTextParagraph(root) {
    if (!root) return null;
    var paragraphs = root.getElementsByTagName('P');
    for (var i = 0; i < paragraphs.length; i++) {
        if (paragraphs[i].parentNode.className == 'section' &&
            /\S/.test(paragraphs[i].innerText)) return paragraphs[i];
    }
    return null;
}
function Run() {
    var ext = window.external;
    var root = document.getElementById('fbw_body');
    if (!root) return;
    var p = FirstTextParagraph(root);
    if (!p) return;
    ext.BeginUndoUnit(document, 'Сделать подзаголовком');
    try { ext.SetStyleEx(document, p, 'subtitle'); }
    finally { ext.EndUndoUnit(document); }
    ext.MsgBox('Новый class: ' + p.className);
}
```

**Ожидаемый результат:** Первый абзац получает класс **`subtitle`**. Переоформление можно отменить штатным Undo.

<a id="api-begindocumentupdate"></a>

### `BeginDocumentUpdate(action, optionsJson)`

**Назначение:** Начинает пакетную правку, возвращая токен группы Undo.

**Сигнатура и параметры:** `window.external.BeginDocumentUpdate(action, optionsJson)`.

DISPID **125** · API 1.1.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `action` | Да | Строка: название операции в истории Undo. |
| `optionsJson` | Да | Строка JSON с параметрами операции; поля, значения по умолчанию и ограничения описаны ниже. |

**Возвращаемое значение:** Строка-токен.

**Особенности, ограничения и ошибки:** `optionsJson`: `"{}"` или `"{\"suspendRedraw\":true}"`. Последний вариант временно приостанавливает перерисовку. Поддерживаются вложенные группы с закрытием в обратном порядке. **Это не транзакционный откат изменений**.

**Пример → результат**

**Условие:** Для старого FBE нужен API 1.1+. Изменяет DOM.

```javascript
function FirstTextParagraph(root) {
    if (!root) return null;
    var paragraphs = root.getElementsByTagName('P');
    for (var i = 0; i < paragraphs.length; i++) {
        if (paragraphs[i].parentNode.className == 'section' &&
            /\S/.test(paragraphs[i].innerText)) return paragraphs[i];
    }
    return null;
}
function Run() {
    var ext = window.external;
    var root = document.getElementById('fbw_body');
    if (!root) return;
    var p = FirstTextParagraph(root);
    if (!p) return;
    var token = ext.BeginDocumentUpdate('Отметить абзац', '{"suspendRedraw":true}');
    try { ext.SetStyleEx(document, p, 'subtitle'); }
    finally { ext.EndDocumentUpdate(token); }
    ext.MsgBox('Пакет завершён; история Undo сохранена');
}
```

**Ожидаемый результат:** Первый абзац получает класс `subtitle`, операция оформляется как **один Undo**, после завершения восстанавливается перерисовка.

<a id="api-enddocumentupdate"></a>

### `EndDocumentUpdate(token)`

**Назначение:** Завершает последнюю открытую пакетную операцию текущего скрипта.

**Сигнатура и параметры:** `window.external.EndDocumentUpdate(token)`.

DISPID **126** · API 1.1.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `token` | Да | Строка: непрозрачный токен, полученный соответствующим методом в этом контексте. |

**Возвращаемое значение:** `true` при успехе.

**Особенности, ограничения и ошибки:** При неправильном порядке закрытия, чужом или устаревшем токене возникает ошибка. Перерисовка восстанавливается и при аварийном завершении контекста.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var token = ext.BeginDocumentUpdate('Пустой демонстрационный пакет', '{}');
    try { ext.SetStatusBarText('Пакет открыт'); }
    finally {
        var closed = ext.EndDocumentUpdate(token);
        ext.MsgBox('EndDocumentUpdate: ' + closed);
    }
}
```

**Ожидаемый результат:** Появляется **«EndDocumentUpdate: true»**; группа закрыта. Поскольку DOM не менялся, полезной Undo-правки не создаётся.

<a id="работа-с-исходным-xml-документа"></a>

<a id="api-getsourcetext"></a>

### `GetSourceText()`

**Назначение:** Читает **полный актуальный XML** книги FB2/FBD, включая `description`, все `body` и `binary`.

**Сигнатура и параметры:** `window.external.GetSourceText()`.

DISPID **32** · исходный интерфейс.

Параметров нет.

**Возвращаемое значение:** Строка XML.

**Особенности, ограничения и ошибки:** В Source учитывает даже ещё не применённые изменения, не переключая режим редактора. Большие бинарные вложения увеличивают память и объём строки.

**Пример → результат**

**Условие:** Большие вложения существенно увеличивают возвращаемую строку.

```javascript
function Run() {
    var ext = window.external;
    var xml = ext.GetSourceText();
    ext.MsgBox('Символов в XML: ' + xml.length +
               '; есть description: ' + (xml.indexOf('<description') >= 0));
}
```

**Ожидаемый результат:** Выводится длина полного XML и признак наличия `description` (обычно **`true`**). Книга не изменяется.

<a id="api-validatesourcetext"></a>

### `ValidateSourceText(xml)`

**Назначение:** Проверяет полный XML FB2/FBD штатным механизмом редактора до изменения книги.

**Сигнатура и параметры:** `window.external.ValidateSourceText(xml)`.

DISPID **33** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `xml` | Да | Строка: полный XML FB2/FBD. |

**Возвращаемое значение:** `true` для валидного XML, иначе `false`.

**Особенности, ограничения и ошибки:** После отказа детали доступны через `GetLastSourceDiagnostic()`.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var valid = ext.ValidateSourceText('Это не XML');
    ext.MsgBox('Проверка: ' + valid);
}
```

**Ожидаемый результат:** Гарантированный отрицательный пример: **«Проверка: false»**. Открытая книга при этом не меняется.

<a id="api-getlastsourcediagnostic"></a>

### `GetLastSourceDiagnostic()`

**Назначение:** Получает подробности последней Source XML-операции.

**Сигнатура и параметры:** `window.external.GetLastSourceDiagnostic()`.

DISPID **34** · исходный интерфейс.

Параметров нет.

**Возвращаемое значение:** JSON-строка с полями `valid`, `message`, `line`, `column`.

**Особенности, ограничения и ошибки:** Это отдельная диагностика Source API, не путайте с `GetLastScriptingError()`. Глобальный `JSON` доступен не во всех режимах MSHTML.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    ext.ValidateSourceText('Это не XML');
    var json = ext.GetLastSourceDiagnostic();
    ext.MsgBox(json);
}
```

**Ожидаемый результат:** JSON с **`"valid":false`**, сообщением об ошибке и координатами `line`/`column`. Сообщение зависит от штатного парсера.

<a id="api-applysourcetext"></a>

### `ApplySourceText(xml, action)`

**Назначение:** Повторно проверяет и применяет XML целиком, создавая одну операцию Undo/Redo с названием `action`.

**Сигнатура и параметры:** `window.external.ApplySourceText(xml, action)`.

DISPID **35** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `xml` | Да | Строка: полный XML FB2/FBD. |
| `action` | Да | Строка: название операции в истории Undo. |

**Возвращаемое значение:** `true` при успешном применении, `false` при отказе.

**Особенности, ограничения и ошибки:** При невалидном XML документ остаётся неизменным. Не вызывайте внутри открытой группы `BeginDocumentUpdate`.

**Пример → результат**

**Условие:** Это учебная **точная строковая замена**; для реальных структурных изменений предпочтителен XML-aware подход, например [сценарий Г](#complex-title). Не используйте внутри `BeginDocumentUpdate`.

```javascript
function Run() {
    var ext = window.external;
    var xml = ext.GetSourceText();
    var from = '<book-title>Черновик</book-title>';
    var to = '<book-title>Готово</book-title>';
    if (xml.indexOf(from) < 0) { ext.MsgBox('Нет точного совпадения'); return; }
    var changed = xml.replace(from, to);
    if (!ext.AskYesNo('Заменить название Черновик на Готово?')) return;
    if (!ext.ValidateSourceText(changed)) {
        ext.MsgBox(ext.GetLastSourceDiagnostic()); return;
    }
    var ok = ext.ApplySourceText(changed, 'Переименовать книгу');
    ext.MsgBox('Применение XML: ' + ok);
}
```

**Ожидаемый результат:** Для книги с точно указанной разметкой меняет заголовок на **«Готово»** и добавляет **одно Undo**. Без совпадения или при отмене ничего не делает.

<a id="api-validatecurrentdocument"></a>

### `ValidateCurrentDocument(optionsJson)`

**Назначение:** Выполняет штатную проверку текущего FB2/FBD **без передачи всего XML в JScript**.

**Сигнатура и параметры:** `window.external.ValidateCurrentDocument(optionsJson)`.

DISPID **143** · API 1.2.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `optionsJson` | Да | Строка JSON с параметрами операции; поля, значения по умолчанию и ограничения описаны ниже. |

**Возвращаемое значение:** JSON-строка с `schemaVersion`, `status`, `valid`, `diagnosticsComplete`, `errors`, `warnings`.

**Особенности, ограничения и ошибки:** Параметр обязателен и пока должен быть строкой `"{}"` (можно пробелы). В Source проверяется несохранённый текст; в Design/Description — detached DOM/stream. Не меняет Undo/Redo, выделение, режим и dirty-state. Для структурной проверки FBD действует его собственный контракт.

**Пример → результат**

**Условие:** Не затрагивает dirty-state, выделение и Undo; в Source учитываются несохранённые изменения.

```javascript
function Run() {
    var ext = window.external;
    var json = ext.ValidateCurrentDocument('{}');
    ext.MsgBox(json);
}
```

**Ожидаемый результат:** Для корректного FB2, например: `{"schemaVersion":1,"status":"ok","valid":true,"diagnosticsComplete":true,"errors":[],"warnings":[]}`. При ошибке — `valid:false` и сведения о первой ошибке.

## Выделение и закладки

<a id="api-createselectionbookmark"></a>

### `CreateSelectionBookmark()`

**Назначение:** Сохраняет состояние текущего выделения либо каретки.

**Сигнатура и параметры:** `window.external.CreateSelectionBookmark()`.

DISPID **119** · API 1.1.0.

Параметров нет.

**Возвращаемое значение:** Непрозрачный строковый токен.

**Особенности, ограничения и ошибки:** До 128 закладок на контекст. Закладка живёт только во время текущего скрипта; не сохраняйте её в настройках.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var mark = ext.CreateSelectionBookmark();
    ext.MsgBox('Закладка создана: ' + (mark.length > 0));
    ext.ReleaseSelectionBookmark(mark);
}
```

**Ожидаемый результат:** Выводится **`true`**; токен не имеет полезного для пользователя текстового представления и живёт до конца запуска скрипта.

<a id="api-restoreselectionbookmark"></a>

### `RestoreSelectionBookmark(token)`

**Назначение:** Возвращает каретку/выделение к сохранённому состоянию, если оно ещё допустимо.

**Сигнатура и параметры:** `window.external.RestoreSelectionBookmark(token)`.

DISPID **120** · API 1.1.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `token` | Да | Строка: непрозрачный токен, полученный соответствующим методом в этом контексте. |

**Возвращаемое значение:** `true` при успешном восстановлении, иначе `false`.

**Особенности, ограничения и ошибки:** Не переключает режим. Смена документа, версии MSHTML, содержимого Source или значения соответствующих полей Description может инвалидировать закладку.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var mark = ext.CreateSelectionBookmark();
    try {
        var restored = ext.RestoreSelectionBookmark(mark);
        ext.MsgBox('Восстановлено: ' + restored);
    } finally { ext.ReleaseSelectionBookmark(mark); }
}
```

**Ожидаемый результат:** Если между вызовами документ и режим не менялись, **«Восстановлено: true»**; при инвалидированной закладке `false`.

<a id="api-releaseselectionbookmark"></a>

### `ReleaseSelectionBookmark(token)`

**Назначение:** Явно освобождает сохранённую закладку.

**Сигнатура и параметры:** `window.external.ReleaseSelectionBookmark(token)`.

DISPID **121** · API 1.1.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `token` | Да | Строка: непрозрачный токен, полученный соответствующим методом в этом контексте. |

**Возвращаемое значение:** `true` при освобождении, `false` для неизвестной или уже удалённой закладки.

**Особенности, ограничения и ошибки:** Оставшиеся закладки освобождаются автоматически в конце контекста скрипта.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var mark = ext.CreateSelectionBookmark();
    var first = ext.ReleaseSelectionBookmark(mark);
    var second = ext.ReleaseSelectionBookmark(mark);
    ext.MsgBox('Первый вызов: ' + first + '; повторный: ' + second);
}
```

**Ожидаемый результат:** **«Первый вызов: true; повторный: false»**. Это позволяет проверить освобождение временного ресурса.

<a id="api-getselectiontext"></a>

### `GetSelectionText()`

**Назначение:** Получает обычный текст выделения.

**Сигнатура и параметры:** `window.external.GetSelectionText()`.

DISPID **122** · API 1.1.0.

Параметров нет.

**Возвращаемое значение:** Строка, `""` при отсутствии текстового выделения.

**Особенности, ограничения и ошибки:** Работает с выделением Body, Source и полей Description. В Source читает только выделенный диапазон Scintilla, не полный XML.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var selected = ext.GetSelectionText();
    ext.MsgBox(selected ? 'Выделено: ' + selected : 'Текст не выделен');
}
```

**Ожидаемый результат:** При выделении слова «Москва» выводится **«Выделено: Москва»**; без текстового выделения — соответствующее сообщение.

<a id="api-getselectionhtml"></a>

### `GetSelectionHtml()`

**Назначение:** Получает HTML-представление выделения.

**Сигнатура и параметры:** `window.external.GetSelectionHtml()`.

DISPID **123** · API 1.1.0.

Параметров нет.

**Возвращаемое значение:** HTML-строка.

**Особенности, ограничения и ошибки:** Для Source результат содержит HTML-экранированный выбранный исходный текст. Поддерживается также control-выделение.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var html = ext.GetSelectionHtml();
    ext.MsgBox(html || '(нет HTML-выделения)');
}
```

**Ожидаемый результат:** Для выделенного форматированного фрагмента выводится его HTML, например с тегами `<EM>`/`<STRONG>`; в Source — HTML-экранированный текст XML.

<a id="api-getselectioninfo"></a>

### `GetSelectionInfo()`

**Назначение:** Описывает тип и объём текущего выделения без передачи его содержимого.

**Сигнатура и параметры:** `window.external.GetSelectionInfo()`.

DISPID **124** · API 1.1.0.

Параметров нет.

**Возвращаемое значение:** JSON-строка с `schemaVersion`, `view`, `type`, `textLength`, `controlCount`.

**Особенности, ограничения и ошибки:** `view`: `design`, `description`, `source`; `type`: `text`, `control`, `none`; длина измеряется в единицах UTF-16, включая пары для supplementary-символов.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var json = ext.GetSelectionInfo();
    ext.MsgBox(json);
}
```

**Ожидаемый результат:** Для шести выделенных букв в Design — JSON наподобие `{"schemaVersion":1,"view":"design","type":"text","textLength":6,"controlCount":0}`. Книга не изменяется.

## Диалоги и пользовательский ввод

<a id="msgbox"></a>

<a id="api-msgbox"></a>

### `MsgBox(message)`

**Назначение:** Показывает информационное окно с одной кнопкой «ОК».

**Сигнатура и параметры:** `window.external.MsgBox(message)`.

DISPID **9** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `message` | Да | Строка сообщения. |

**Возвращаемое значение:** Результат не используется.

**Особенности, ограничения и ошибки:** Модальное сообщение приостанавливает выполнение до закрытия. Для выбора нескольких вариантов и явного статуса отмены используйте `ShowMessage`.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    ext.MsgBox('Проверка выполнена: найдено 3 совпадения.');
}
```

**Ожидаемый результат:** Появится модальное информационное окно с приведённым текстом и кнопкой **ОК**.

<a id="askyesno"></a>

<a id="api-askyesno"></a>

### `AskYesNo(message)`

**Назначение:** Запрашивает у пользователя ответ «Да» или «Нет».

**Сигнатура и параметры:** `window.external.AskYesNo(message)`.

DISPID **10** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `message` | Да | Строка сообщения. |

**Возвращаемое значение:** `true` только при выборе «Да».

**Особенности, ограничения и ошибки:** Выполняйте действие только при результате `true`. Для отдельного статуса отмены используйте `ShowMessage` с `yesnocancel`.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var accepted = ext.AskYesNo('Создать отчёт?');
    ext.MsgBox(accepted ? 'Пользователь согласился' : 'Пользователь отказался');
}
```

**Ожидаемый результат:** Нажатие **«Да»** возвращает `true`, **«Нет»** — `false`; второй диалог это покажет.

<a id="inputbox"></a>

<a id="api-inputbox"></a>

### `InputBox(prompt, title, value)`

**Назначение:** Отображает старое окно ввода текста.

**Сигнатура и параметры:** `window.external.InputBox(prompt, title, value)`.

DISPID **23** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `prompt` | Да | Строка приглашения. |
| `title` | Да | Строка заголовка окна. |
| `value` | Да | Строка: начальный текст или новое значение. |

**Возвращаемое значение:** Введённая строка после подтверждения; при отмене — `""`.

**Особенности, ограничения и ошибки:** Чтобы отличить отмену от подтверждения пустой строки, сразу вызывайте `GetModalResult()`.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var text = ext.InputBox('Введите автора', 'Метаданные', 'А. Пушкин');
    var ok = (ext.GetModalResult() == 6);
    ext.MsgBox(ok ? 'Введено: ' + text : 'Ввод отменён');
}
```

**Ожидаемый результат:** Если нажать подтверждение, не меняя поле: **«Введено: А. Пушкин»**. При отмене — **«Ввод отменён»**.

<a id="getmodalresult"></a>

<a id="api-getmodalresult"></a>

### `GetModalResult()`

**Назначение:** Возвращает код последнего диалога `InputBox`.

**Сигнатура и параметры:** `window.external.GetModalResult()`.

DISPID **24** · исходный интерфейс.

Параметров нет.

**Возвращаемое значение:** Числовой код: `6` (`IDYES`) при подтверждении, обычно `2` (`IDCANCEL`) при отмене.

**Особенности, ограничения и ошибки:** Это состояние старого InputBox; новый `InputBoxEx` на него не влияет.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var answer = ext.InputBox('Введите число', 'Проверка', '10');
    var code = ext.GetModalResult();
    ext.MsgBox('Код диалога: ' + code);
}
```

**Ожидаемый результат:** Для подтверждения — **`6`** (`IDYES`), для отмены обычно **`2`** (`IDCANCEL`).

<a id="api-selectopenfile"></a>

### `SelectOpenFile(optionsJson)`

**Назначение:** Открывает системный диалог выбора существующего файла или нескольких файлов.

**Сигнатура и параметры:** `window.external.SelectOpenFile(optionsJson)`.

DISPID **115** · API 1.0.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `optionsJson` | Да | Строка JSON с параметрами операции; поля, значения по умолчанию и ограничения описаны ниже. |

**Возвращаемое значение:** JSON-строка `{"status":"ok","path":"..."}` либо с `paths`, при отмене `{"status":"cancelled"}`.

**Особенности, ограничения и ошибки:** Опции: `title`, `initialDirectory`, `initialFileName`, `defaultExtension`, `multiSelect`, `filters`. Выбор не открывает книгу в редакторе.

**Пример → результат**

**Условие:** Метод только выбирает путь, **не открывает книгу** в FBE.

```javascript
function Run() {
    var ext = window.external;
    var json = ext.SelectOpenFile(
        '{"title":"Открыть FB2","filters":[{"name":"Книги FB2","pattern":"*.fb2"}]}');
    ext.MsgBox(json);
}
```

**Ожидаемый результат:** После выбора книги — JSON наподобие `{"status":"ok","path":"D:\\Books\\Book.fb2"}`; при закрытии окна — `{"status":"cancelled"}`.

<a id="api-selectsavefile"></a>

### `SelectSaveFile(optionsJson)`

**Назначение:** Предлагает пользователю выбрать путь для сохранения файла.

**Сигнатура и параметры:** `window.external.SelectSaveFile(optionsJson)`.

DISPID **116** · API 1.0.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `optionsJson` | Да | Строка JSON с параметрами операции; поля, значения по умолчанию и ограничения описаны ниже. |

**Возвращаемое значение:** JSON-строка с `status` и `path` (или `status:"cancelled"`).

**Особенности, ограничения и ошибки:** Сам файл **не записывает**. При существующем файле пользователь должен подтвердить замену. `multiSelect:true` не поддерживается.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var json = ext.SelectSaveFile(
        '{"title":"Куда сохранить отчёт","defaultExtension":"txt"}');
    ext.MsgBox(json);
}
```

**Ожидаемый результат:** После выбора пути — `status:"ok"` и `path`; при отмене — `status:"cancelled"`. **Никакого файла пока не создано.**

<a id="api-selectfolder"></a>

### `SelectFolder(optionsJson)`

**Назначение:** Выбирает существующий каталог или несколько каталогов.

**Сигнатура и параметры:** `window.external.SelectFolder(optionsJson)`.

DISPID **117** · API 1.0.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `optionsJson` | Да | Строка JSON с параметрами операции; поля, значения по умолчанию и ограничения описаны ниже. |

**Возвращаемое значение:** JSON-строка с `status` и `path`/`paths`.

**Особенности, ограничения и ошибки:** Поддерживает `multiSelect`. Поле `filters` для каталогов недопустимо.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var json = ext.SelectFolder('{"title":"Каталог для отчётов"}');
    ext.MsgBox(json);
}
```

**Ожидаемый результат:** При выборе папки — JSON с `status:"ok"` и полем `path`, при отмене — `{"status":"cancelled"}`.

<a id="api-inputboxex"></a>

### `InputBoxEx(optionsJson)`

**Назначение:** Показывает современный диалог ввода с отдельным признаком подтверждения.

**Сигнатура и параметры:** `window.external.InputBoxEx(optionsJson)`.

DISPID **118** · API 1.0.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `optionsJson` | Да | Строка JSON с параметрами операции; поля, значения по умолчанию и ограничения описаны ниже. |

**Возвращаемое значение:** JSON-строка: `{"accepted":true,"value":"..."}` либо `{"accepted":false,"value":""}`.

**Особенности, ограничения и ошибки:** Строковые поля `title`, `prompt`, `value` необязательны. При ошибке возвращается `status:"error"`, `code`, `hresult`, `message`. Старый `GetModalResult()` не изменяется.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var json = ext.InputBoxEx(
        '{"title":"Подпись","prompt":"Введите имя","value":"Михаил"}');
    ext.MsgBox(json);
}
```

**Ожидаемый результат:** При ОК без изменений — `{"accepted":true,"value":"Михаил"}`; при отмене — `{"accepted":false,"value":""}`. Состояние старого `GetModalResult()` не меняется.

## Описание книги и жанры

<a id="genrepopup"></a>

<a id="api-genrepopup"></a>

### `GenrePopup(element, x, y)`

**Назначение:** Открывает меню выбора жанра FB2 в указанной точке экрана.

**Сигнатура и параметры:** `window.external.GenrePopup(element, x, y)`.

DISPID **4** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `element` | Да | DOM-элемент MSHTML, к которому применяется операция. |
| `x` | Да | Число: горизонтальная координата меню в пикселях. |
| `y` | Да | Число: вертикальная координата меню в пикселях. |

**Возвращаемое значение:** Идентификатор жанра строкой либо `null` при отмене.

**Особенности, ограничения и ошибки:** Координаты `x`/`y` — экранные пиксели, например из `event.screenX/screenY`; первый аргумент оставлен для совместимости.

**Пример → результат**

**Условие:** В реальном обработчике нажатия мыши используйте `event.screenX`/`event.screenY`; центр экрана здесь только для демонстрации.

```javascript
function Run() {
    var ext = window.external;
    var x = Math.floor(window.screen.width / 2);
    var y = Math.floor(window.screen.height / 2);
    var genre = ext.GenrePopup(document.body, x, y);
    ext.MsgBox(genre == null ? 'Жанр не выбран' : 'Код жанра: ' + genre);
}
```

**Ожидаемый результат:** Появляется меню жанров. После выбора, например «Фэнтези», возвращается код жанра (часто `sf_fantasy`); при отмене — `null`.

<a id="getextendedstyle"></a>

<a id="api-getextendedstyle"></a>

### `GetExtendedStyle(elementId)`

**Назначение:** Читает состояние расширенного показа элемента описания по его идентификатору.

**Сигнатура и параметры:** `window.external.GetExtendedStyle(elementId)`.

DISPID **12** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `elementId` | Да | Строка: ID элемента Description. |

**Возвращаемое значение:** `true` или `false`.

**Особенности, ограничения и ошибки:** Читает настройку отображения дополнительного поля Description; не возвращает CSS-стиль произвольного абзаца.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var shown = ext.GetExtendedStyle('annotation');
    ext.MsgBox('Расширенный показ annotation: ' + shown);
}
```

**Ожидаемый результат:** `true` или `false` в зависимости от сохранённой настройки показа аннотации.

<a id="descshowelement"></a>

<a id="api-descshowelement"></a>

### `DescShowElement(elementId, show)`

**Назначение:** Включает или выключает расширенный показ элемента описания, сохраняя настройку.

**Сигнатура и параметры:** `window.external.DescShowElement(elementId, show)`.

DISPID **13** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `elementId` | Да | Строка: ID элемента Description. |
| `show` | Да | Логическое значение: показать (`true`) или скрыть (`false`). |

**Возвращаемое значение:** Результат не используется.

**Особенности, ограничения и ошибки:** Работает с известными идентификаторами дополнительных полей Description. Это управление формой, а не произвольное добавление XML-элементов.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var before = ext.GetExtendedStyle('annotation');
    try {
        ext.DescShowElement('annotation', !before);
        ext.MsgBox('Новая настройка: ' + ext.GetExtendedStyle('annotation'));
    } finally { ext.DescShowElement('annotation', before); }
}
```

**Ожидаемый результат:** В окне отображается **значение, противоположное исходному**. Затем настройка возвращается к исходному состоянию.

<a id="descshowmenu"></a>

<a id="api-descshowmenu"></a>

### `DescShowMenu(button, x, y)`

**Назначение:** Показывает меню элементов Description у кнопки в экранной точке.

**Сигнатура и параметры:** `window.external.DescShowMenu(button, x, y)`.

DISPID **14** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `button` | Да | DOM-элемент кнопки Description. |
| `x` | Да | Число: горизонтальная координата меню в пикселях. |
| `y` | Да | Число: вертикальная координата меню в пикселях. |

**Возвращаемое значение:** Строка ID элемента или пустое значение при отсутствии выбора.

**Особенности, ограничения и ошибки:** Для координат также используется `event.screenX/screenY`.

**Пример → результат**

**Условие:** В обработчике мыши берите экранные координаты из события; здесь выбран центр первичного экрана.

```javascript
function Run() {
    var ext = window.external;
    var x = Math.floor(window.screen.width / 2);
    var y = Math.floor(window.screen.height / 2);
    var chosen = ext.DescShowMenu(document.body, x, y);
    ext.MsgBox(chosen ? 'Выбран элемент: ' + chosen : 'Элемент не выбран');
}
```

**Ожидаемый результат:** Открывается меню элементов Description. При выборе возвращается идентификатор элемента; при закрытии без выбора — пустое значение.

## Строка состояния, прогресс и отмена

<a id="setstatusbartext"></a>

<a id="api-setstatusbartext"></a>

### `SetStatusBarText(text)`

**Назначение:** Устанавливает произвольный текст в строке состояния редактора.

**Сигнатура и параметры:** `window.external.SetStatusBarText(text)`.

DISPID **25** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `text` | Да | Строка текста. |

**Возвращаемое значение:** Результат не используется.

**Особенности, ограничения и ошибки:** Штатное временное сообщение может смениться контекстным текстом редактора. Для вложенной работы с восстановлением актуального статуса используйте статусный стек API 1.4.0.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    ext.SetStatusBarText('Скрипт: обработано 10 из 20 абзацев');
}
```

**Ожидаемый результат:** Надпись **«Скрипт: обработано 10 из 20 абзацев»** появляется в строке состояния FBE; текст может позднее смениться штатным сообщением.

<a id="api-beginprogress"></a>

### `BeginProgress(title, maximum)`

**Назначение:** Создаёт системное окно прогресса с возможностью отмены.

**Сигнатура и параметры:** `window.external.BeginProgress(title, maximum)`.

DISPID **127** · API 1.1.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `title` | Да | Строка заголовка окна. |
| `maximum` | Да | Число: целое от 1 до 4294967295, общий объём работы. |

**Возвращаемое значение:** Непрозрачный токен.

**Особенности, ограничения и ошибки:** `maximum` — целое от 1 до 4294967295. На UI-потоке может быть одно активное окно; вызовы требуют активного контекста.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var token = ext.BeginProgress('Проверка примера', 3);
    try { ext.SetProgress(token, 1, 'Первый шаг'); }
    finally { ext.EndProgress(token); }
}
```

**Ожидаемый результат:** Открывается стандартное окно прогресса с максимумом **3**, после завершения окна оно закрывается. `token` — внутренний идентификатор, а не числовой процент.

<a id="api-setprogress"></a>

### `SetProgress(token, value, message)`

**Назначение:** Обновляет количество обработанных элементов и подпись в окне прогресса.

**Сигнатура и параметры:** `window.external.SetProgress(token, value, message)`.

DISPID **128** · API 1.1.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `token` | Да | Строка: непрозрачный токен, полученный соответствующим методом в этом контексте. |
| `value` | Да | Число: целое от 0 до заданного `maximum`. |
| `message` | Да | Строка сообщения. |

**Возвращаемое значение:** `true` при успехе.

**Особенности, ограничения и ошибки:** `value` — целое в диапазоне от 0 до `maximum`; метод не приводит к произвольному выполнению очереди UI.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var token = ext.BeginProgress('Обработка страниц', 4);
    try {
        var ok = ext.SetProgress(token, 2, 'Страница 2 из 4');
        ext.MsgBox('Обновление прогресса: ' + ok);
    } finally { ext.EndProgress(token); }
}
```

**Ожидаемый результат:** Прогресс показывает **2 из 4**, подпись «Страница 2 из 4», вызов возвращает **`true`**.

<a id="api-isprogresscancelled"></a>

### `IsProgressCancelled(token)`

**Назначение:** Проверяет, нажал ли пользователь кнопку отмены.

**Сигнатура и параметры:** `window.external.IsProgressCancelled(token)`.

DISPID **129** · API 1.1.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `token` | Да | Строка: непрозрачный токен, полученный соответствующим методом в этом контексте. |

**Возвращаемое значение:** `true` или `false`.

**Особенности, ограничения и ошибки:** Отмена **кооперативная**: именно скрипт должен прекратить работу; уже сделанные изменения автоматически не откатываются.

**Пример → результат**

**Условие:** Это не обработчик событий и не универсальная «отзывчивая пауза»: вызывайте проверку в естественных точках длительного алгоритма.

```javascript
function Run() {
    var ext = window.external;
    var token = ext.BeginProgress('Отмена', 100);
    try {
        var cancelled = ext.IsProgressCancelled(token);
        ext.SetStatusBarText(cancelled ? 'Запрошена отмена' : 'Отмена не запрошена');
    } finally { ext.EndProgress(token); }
}
```

**Ожидаемый результат:** Сразу после открытия обычно **`false`**. Если пользователь успевает нажать «Отмена» и событие доставлено диалогу, последующая проверка выдаст `true`.

<a id="api-endprogress"></a>

### `EndProgress(token)`

**Назначение:** Закрывает окно прогресса и освобождает связанные ресурсы.

**Сигнатура и параметры:** `window.external.EndProgress(token)`.

DISPID **130** · API 1.1.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `token` | Да | Строка: непрозрачный токен, полученный соответствующим методом в этом контексте. |

**Возвращаемое значение:** `true` при успехе.

**Особенности, ограничения и ошибки:** Вызывайте в `finally`; при завершении контекста окно также закрывается автоматически.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var token = ext.BeginProgress('Завершение', 1);
    try { ext.SetProgress(token, 1, 'Готово'); }
    finally { ext.MsgBox('EndProgress: ' + ext.EndProgress(token)); }
}
```

**Ожидаемый результат:** **`EndProgress: true`**; окно прогресса закрыто, ресурсы освобождены.

## Двоичные вложения, текстовые файлы и пути

<a id="getbinarysize"></a>

<a id="api-getbinarysize"></a>

### `GetBinarySize(data)`

**Назначение:** Возвращает размер бинарного содержимого вложения в байтах.

**Сигнатура и параметры:** `window.external.GetBinarySize(data)`.

DISPID **6** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `data` | Да | Исходное COM-значение `binary.base64data` из DOM FBE, а не текст Base64, путь или результат `String(data)`. |

**Возвращаемое значение:** Целое число.

**Особенности, ограничения и ошибки:** Передавайте `binary.base64data` — **двоичную строку**, а не текст кодировки base64.

**Пример → результат**

**Условие:** Откройте FB2 с хотя бы одним binary-вложением.

```javascript
function Run() {
    var ext = window.external;
    var binbox = document.all.binobj;
    if (!binbox) { ext.MsgBox('Контейнер binary отсутствует'); return; }
    var nodes = binbox.getElementsByTagName('DIV');
    if (!nodes.length) { ext.MsgBox('У книги нет вложений'); return; }
    var data = nodes[0].base64data;
    ext.MsgBox('Размер первого вложения: ' + ext.GetBinarySize(data) + ' Б');
}
```

**Ожидаемый результат:** Если первое вложение занимает **15342 байта**, появится «Размер первого вложения: 15342 Б». Считываются именно декодированные бинарные данные, не текст base64.

<a id="savebinary"></a>

<a id="api-savebinary"></a>

### `SaveBinary(path, data, prompt)`

**Назначение:** Сохраняет двоичные данные в отдельный файл.

**Сигнатура и параметры:** `window.external.SaveBinary(path, data, prompt)`.

DISPID **11** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `path` | Да | Строка пути к файлу; требования к абсолютности и существованию приведены ниже. |
| `data` | Да | Исходное COM-значение `binary.base64data` из DOM FBE, а не текст Base64, путь или результат `String(data)`. |
| `prompt` | Да | Логическое значение: разрешить диалог выбора файла. |

**Возвращаемое значение:** `true` после полной записи либо `false` при отмене/ошибке.

**Особенности, ограничения и ошибки:** При `prompt = true` открывается стандартный диалог Save As и подтверждение замены. При `prompt = false` файл по `path` создаётся без показа диалога. Если он уже существует, метод не заменяет его и возвращает `false`.

**Пример → результат**

**Условие:** Выберите действительно нужное вложение; расширение и формат файла должны соответствовать его MIME-типу. При `prompt:true` возможно подтверждение перезаписи.

```javascript
function Run() {
    var ext = window.external;
    var binbox = document.all.binobj;
    if (!binbox) return;
    var nodes = binbox.getElementsByTagName('DIV');
    if (!nodes.length) { ext.MsgBox('Вложений нет'); return; }
    var suggestedName = String(nodes[0].all.id.value);
    var ok = ext.SaveBinary(suggestedName, nodes[0].base64data, true);
    ext.MsgBox(ok ? 'Сохранено выбранное вложение' : 'Отменено или ошибка записи');
}
```

**Ожидаемый результат:** Откроется диалог Save As; после подтверждённой записи — **`true`**, при отмене — **`false`**.

<a id="getimagedimsbypath"></a>

<a id="api-getimagedimsbypath"></a>

### `GetImageDimsByPath(path)`

**Назначение:** Читает ширину и высоту изображения по пути.

**Сигнатура и параметры:** `window.external.GetImageDimsByPath(path)`.

DISPID **17** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `path` | Да | Строка пути к файлу; требования к абсолютности и существованию приведены ниже. |

**Возвращаемое значение:** Строка вида `"640x480"`, либо `""` при ошибке.

**Особенности, ограничения и ошибки:** Читает изображение с диска без добавления в книгу. Перед вызовом проверяйте путь и доступность файла; формат результата сохраняет исторический контракт.

**Пример → результат**

**Условие:** Замените путь на реальный путь к PNG/JPEG или другому поддерживаемому изображению.

```javascript
function Run() {
    var ext = window.external;
    var path = 'C:\\Pictures\\cover.jpg';
    var dims = ext.GetImageDimsByPath(path);
    ext.MsgBox(dims ? 'Размер: ' + dims : 'Нет файла / неподдерживаемый формат');
}
```

**Ожидаемый результат:** Для существующего изображения **600×900** — **`600x900`**. При отсутствии указанного файла — пустая строка и сообщение о невозможности определить размер.

<a id="getimagedimsbydata"></a>

<a id="api-getimagedimsbydata"></a>

### `GetImageDimsByData(data)`

**Назначение:** Определяет размеры изображения по бинарному содержимому.

**Сигнатура и параметры:** `window.external.GetImageDimsByData(data)`.

DISPID **18** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `data` | Да | Исходное COM-значение `binary.base64data` из DOM FBE, а не текст Base64, путь или результат `String(data)`. |

**Возвращаемое значение:** Строка вида `"640x480"`, либо `""`.

**Особенности, ограничения и ошибки:** Передавайте `base64data`, а не произвольный JS-текст.

**Пример → результат**

**Условие:** Откройте книгу, содержащую изображение в разделе `binary`.

```javascript
function Run() {
    var ext = window.external;
    var binbox = document.all.binobj;
    if (!binbox) return;
    var nodes = binbox.getElementsByTagName('DIV');
    for (var i = 0; i < nodes.length; i++) {
        var type = nodes[i].all.type.value;
        if (type.indexOf('image/') !== 0) continue;
        ext.MsgBox('Первое изображение: ' + ext.GetImageDimsByData(nodes[i].base64data));
        return;
    }
    ext.MsgBox('Изображений среди вложений нет');
}
```

**Ожидаемый результат:** Например, **«Первое изображение: 600x900»**; если вложения не являются изображениями — отдельное сообщение.

<a id="api-getbinaryinfo"></a>

### `GetBinaryInfo(id)`

**Назначение:** Получает фактические сведения о вложении текущей книги без изменения документа.

**Сигнатура и параметры:** `window.external.GetBinaryInfo(id)`.

DISPID **147** · API 1.3.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `id` | Да | Строка: ID вложения без начального `#`. |

**Возвращаемое значение:** JSON-строка с `schemaVersion:1`, `id`, `mimeType`, размером `size` в байтах, `width`, `height` и `referenceCount`.

**Особенности, ограничения и ошибки:** Размер берётся из исходных бинарных данных. Для неподдерживаемого изображения и не-изображения размеры равны `null`; `referenceCount:null` означает, что достоверный подсчёт ссылок пока не предоставляется. Отсутствующий ID вызывает COM-ошибку `0x80070490`. В Source метод явно отклоняется, чтобы не читать устаревший Body. Выделение, dirty-state и Undo/Redo не меняются.

**Пример → результат**

**Условие:** API 1.3.0, режим Design/Description, вложение `обложка.png` существует.

```javascript
function Run() {
    var ext = window.external;
    try {
        ext.MsgBox(ext.GetBinaryInfo('обложка.png'));
    } catch (e) {
        ext.MsgBox('Не удалось получить сведения: ' + e.description);
    }
}
```

**Ожидаемый результат:** Например, `{"schemaVersion":1,"id":"обложка.png","mimeType":"image/png","size":15342,"width":600,"height":800,"referenceCount":null}`. Значения зависят от исходного вложения; байты не передаются строкой в JScript.

<a id="api-generateuniquebinaryid"></a>

### `GenerateUniqueBinaryId(preferredName)`

**Назначение:** Подбирает допустимый свободный XML ID для будущего вложения.

**Сигнатура и параметры:** `window.external.GenerateUniqueBinaryId(preferredName)`.

DISPID **148** · API 1.3.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `preferredName` | Да | Строка: желаемое имя нового ID. |

**Возвращаемое значение:** Строка с нормализованным именем и, при конфликте, суффиксом `_0`, `_1` и т. д.

**Особенности, ограничения и ошибки:** Сохраняет кириллицу и допустимый Unicode, заменяет недопустимые символы и некорректные суррогатные пары. Путь превращается в имя файла. Пустое имя нормализуется в `image`. Метод не создаёт и не резервирует объект; фактическое добавление ещё раз проверит конфликт. В Source возвращает `E_NOTIMPL`.

**Пример → результат**

**Условие:** API 1.3.0, Design/Description; вложение `обложка.png` уже есть, а `обложка.png_0` свободно.

```javascript
function Run() {
    var ext = window.external;
    try {
        ext.MsgBox(ext.GenerateUniqueBinaryId('обложка.png'));
    } catch (e) {
        ext.MsgBox('Не удалось подобрать ID: ' + e.description);
    }
}
```

**Ожидаемый результат:** **`обложка.png_0`**. Книга не меняется; если исходное имя свободно, результатом будет само имя.

<a id="api-addbinaryfromfile"></a>

### `AddBinaryFromFile(path, preferredId[, mimeType])`

**Назначение:** Читает файл и добавляет его исходные байты в текущую книгу.

**Сигнатура и параметры:** `window.external.AddBinaryFromFile(path, preferredId[, mimeType])`.

DISPID **149** · API 1.3.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `path` | Да | Строка пути к файлу; требования к абсолютности и существованию приведены ниже. |
| `preferredId` | Да | Строка: желаемый ID; пустая строка использует имя файла. |
| `mimeType` | Нет | Строка MIME-типа; при отсутствии используется определение по содержимому. |

**Возвращаемое значение:** Фактический ID добавленного объекта строкой.

**Особенности, ограничения и ошибки:** Пустой `preferredId` использует имя файла; конфликт получает свободный суффикс. MIME поддерживаемого изображения определяется по содержимому, для неоднозначного не-изображения его нужно указать явно. Перекодирование не выполняется. Успех создаёт одну операцию штатного Undo и обновляет вложения, размеры изображений и списки обложек. В Source и внутри открытой Undo-группы вызов отклоняется до изменений. Общие лимиты приведены в [ограничениях бинарного API](#binary-api-limits).

**Пример → результат**

**Условие:** API 1.3.0, Design/Description; файл `D:\Images\обложка.png` существует. Метод меняет книгу; сначала проверьте на тестовом FB2/FBD.

```javascript
function Run() {
    var ext = window.external;
    try {
        var id = ext.AddBinaryFromFile('D:\\Images\\обложка.png', 'обложка.png');
        ext.MsgBox('Добавлено вложение: ' + id);
    } catch (e) {
        ext.MsgBox('Добавление не выполнено: ' + e.description);
    }
}
```

**Ожидаемый результат:** **«Добавлено вложение: обложка.png»**, либо ID с суффиксом при конфликте. Книга становится изменённой, Undo удаляет добавленный объект, Redo возвращает его. Сохранение книги на диск автоматически не запускается.

<a id="api-replacebinaryfromfile"></a>

### `ReplaceBinaryFromFile(id, path[, mimeType])`

**Назначение:** Заменяет данные существующего вложения, сохраняя его ID и ссылки в книге.

**Сигнатура и параметры:** `window.external.ReplaceBinaryFromFile(id, path[, mimeType])`.

DISPID **150** · API 1.3.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `id` | Да | Строка: ID вложения без начального `#`. |
| `path` | Да | Строка пути к файлу; требования к абсолютности и существованию приведены ниже. |
| `mimeType` | Нет | Строка MIME-типа; при отсутствии используется определение по содержимому. |

**Возвращаемое значение:** `true` после успешной замены; ошибки выдаются через COM.

**Особенности, ограничения и ошибки:** Проверяет файл, MIME и содержимое так же, как `AddBinaryFromFile`. Используемое изображение нельзя заменить не-изображением. Обновляет размеры, ImagesInfo, списки обложек и отображение изображений. Совпадающие байты и MIME не создают лишнего Undo. Отмена восстанавливает прежние байты и метаданные; Source и открытые Undo-группы не поддерживаются.

**Пример → результат**

**Условие:** API 1.3.0, Design/Description; объект `обложка.png` и файл `D:\Images\новая.png` существуют. Метод меняет книгу; используйте тестовую копию.

```javascript
function Run() {
    var ext = window.external;
    try {
        var ok = ext.ReplaceBinaryFromFile('обложка.png', 'D:\\Images\\новая.png');
        ext.MsgBox('Вложение заменено: ' + ok);
    } catch (e) {
        ext.MsgBox('Замена не выполнена: ' + e.description);
    }
}
```

**Ожидаемый результат:** **«Вложение заменено: true»**. Ссылки по-прежнему указывают на `#обложка.png`, но отображаются новые данные. Одно Undo возвращает прежнее вложение; одинаковый файл не добавляет операцию отмены.

<a id="api-removebinary"></a>

### `RemoveBinary(id[, removeReferences])`

**Назначение:** Удаляет вложение с явным контролем ссылок на него.

**Сигнатура и параметры:** `window.external.RemoveBinary(id[, removeReferences])`.

DISPID **151** · API 1.3.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `id` | Да | Строка: ID вложения без начального `#`. |
| `removeReferences` | Нет | Логическое значение; по умолчанию `false`. Значение `true` разрешает удалить поддерживаемые ссылки. |

**Возвращаемое значение:** `true` при успешном удалении; отсутствие ID и небезопасные ссылки вызывают COM-ошибку.

**Особенности, ограничения и ошибки:** По умолчанию `removeReferences:false`: используемый объект не удаляется. С `true` допускается удалить вместе с ним поддерживаемые блочные/строчные изображения и очистить обложки. Неизвестные типы ссылок и обёртки с дополнительным содержимым приводят к отказу до изменений. Подробные правила — в [ограничениях бинарного API](#binary-api-limits). Объект, ссылки, обложки и выделение восстанавливаются одним Undo; Redo повторяет удаление. Source и открытые Undo-группы отклоняются.

**Пример → результат**

**Условие:** API 1.3.0, Design/Description; объект `unused.png` существует. Метод меняет книгу; используйте тестовую копию.

```javascript
function Run() {
    var ext = window.external;
    try {
        var ok = ext.RemoveBinary('unused.png');
        ext.MsgBox('Вложение удалено: ' + ok);
    } catch (e) {
        ext.MsgBox('Удаление не выполнено: ' + e.description);
    }
}
```

**Ожидаемый результат:** **«Вложение удалено: true»**, если ссылок нет. Если объект используется, вызов выдаст ошибку и сохранит книгу. Передача `true` вторым аргументом — отдельное явное разрешение удалить поддерживаемые изображения/обложки вместе с объектом.

<a id="api-saveallbinaries"></a>

### `SaveAllBinaries(directory[, overwritePolicy])`

**Назначение:** Экспортирует все вложения, включая не-изображения, без изменения книги.

**Сигнатура и параметры:** `window.external.SaveAllBinaries(directory[, overwritePolicy])`.

DISPID **152** · API 1.3.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `directory` | Да | Строка пути к каталогу. |
| `overwritePolicy` | Нет | Строка: `skip` (по умолчанию), `fail` или `replace`. |

**Возвращаемое значение:** JSON-строка с `schemaVersion:1`, `status`, счётчиками `saved`, `skipped`, `errors` и массивом `objects`. Для каждого объекта возвращаются `id`, `fileName`, `status` и `hresult`.

**Особенности, ограничения и ошибки:** Каталог должен существовать. Политика `skip` по умолчанию пропускает существующие файлы; `fail` считает конфликт ошибкой; `replace` разрешает атомарную замену. Байты сохраняются без перекодирования. Имена защищены от выхода за каталог, недопустимых символов, DOS device names и совпадений без учёта регистра. Частичный экспорт имеет `status:"error"`; уже экспортированные файлы не удаляются из-за следующей ошибки. В Source возвращается `E_NOTIMPL`.

**Пример → результат**

**Условие:** API 1.3.0, Design/Description; каталог `D:\Export` существует. Пример создаёт файлы, но не перезаписывает существующие и не меняет книгу.

```javascript
function Run() {
    var ext = window.external;
    try {
        ext.MsgBox(ext.SaveAllBinaries('D:\\Export', 'skip'));
    } catch (e) {
        ext.MsgBox('Экспорт не выполнен: ' + e.description);
    }
}
```

**Ожидаемый результат:** Например, `{"schemaVersion":1,"status":"ok","saved":1,"skipped":0,"errors":0,"objects":[{"id":"обложка.png","fileName":"_обложка.png","status":"saved","hresult":"0x00000000"}]}`. Повторный вызов с `skip` пропустит этот файл. При частичной ошибке нужно проверять `status`, `errors` и результаты объектов, а не считать сам факт получения строки признаком полного успеха.

<a id="api-readtextfile"></a>

### `ReadTextFile(path[, encoding])`

**Назначение:** Читает небольшой текстовый файл с диска.

**Сигнатура и параметры:** `window.external.ReadTextFile(path[, encoding])`.

DISPID **131** · API 1.1.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `path` | Да | Строка пути к файлу; требования к абсолютности и существованию приведены ниже. |
| `encoding` | Нет | Строка имени кодировки; доступные значения и выбор по умолчанию приведены ниже. |

**Возвращаемое значение:** Строка содержимого.

**Особенности, ограничения и ошибки:** Без кодировки распознаются BOM UTF-8/UTF-16LE, иначе читается UTF-8. При неверной кодировке, пути или доступе возникает COM-ошибка.

**Пример → результат**

**Условие:** Не запускайте чтение больших файлов без учёта ограничения 8 МиБ.

```javascript
function Run() {
    var ext = window.external;
    var dir = ext.GetCurrentScriptDirectory();
    var path = ext.CombinePath(dir, 'example.txt');
    if (!ext.FileExists(path)) {
        ext.MsgBox('Положите example.txt рядом со скриптом'); return;
    }
    try { ext.MsgBox(ext.ReadTextFile(path, 'utf-8')); }
    catch (e) { ext.MsgBox('Не удалось прочитать UTF-8 файл'); }
}
```

**Ожидаемый результат:** Если рядом со скриптом находится `example.txt` с текстом **«Привет, FBE!»** в UTF-8, появляется окно с этой строкой.

<a id="api-writetextfile"></a>

### `WriteTextFile(path, text[, encoding])`

**Назначение:** Записывает текстовый файл с атомарной заменой уже существующего файла.

**Сигнатура и параметры:** `window.external.WriteTextFile(path, text[, encoding])`.

DISPID **132** · API 1.1.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `path` | Да | Строка пути к файлу; требования к абсолютности и существованию приведены ниже. |
| `text` | Да | Строка текста. |
| `encoding` | Нет | Строка имени кодировки; доступные значения и выбор по умолчанию приведены ниже. |

**Возвращаемое значение:** `true` при успехе.

**Особенности, ограничения и ошибки:** По умолчанию UTF-8 без BOM. Метод **может перезаписать файл**; безопаснее `WriteTextFileEx` с `overwrite:false`.

**Пример → результат**

**Условие:** Метод умеет **заменять существующий файл**. В примере имя уникально, чтобы не затронуть чужие файлы; учебный файл остаётся в `%TEMP%`.

```javascript
function Run() {
    var ext = window.external;
    var filename = 'fbe-example-' + ext.GetUUID() + '.txt';
    var path = ext.CombinePath(ext.GetTempDirectory(), filename);
    var ok = ext.WriteTextFile(path, 'Строка 1\r\nСтрока 2', 'utf-8');
    ext.MsgBox(ok ? 'Создан файл: ' + path : 'Запись не выполнена');
}
```

**Ожидаемый результат:** Во временном каталоге появляется уникальный UTF-8-файл из двух строк; вызов возвращает **`true`**.

<a id="api-writetextfileex"></a>

### `WriteTextFileEx(path, text, overwrite[, encoding])`

**Назначение:** Записывает текст с явным разрешением или запретом перезаписи.

**Сигнатура и параметры:** `window.external.WriteTextFileEx(path, text, overwrite[, encoding])`.

DISPID **142** · API 1.1.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `path` | Да | Строка пути к файлу; требования к абсолютности и существованию приведены ниже. |
| `text` | Да | Строка текста. |
| `overwrite` | Да | Логическое значение: явное разрешение перезаписи. `false` запрещает замену существующего файла. |
| `encoding` | Нет | Строка имени кодировки; доступные значения и выбор по умолчанию приведены ниже. |

**Возвращаемое значение:** `true` при успехе; при конфликте существующего файла с `overwrite:false` — COM-ошибка.

**Особенности, ограничения и ошибки:** `overwrite:false` использует безопасное создание без замены. `true` — атомарная замена. Относительные пути считаются относительно рабочего каталога FBE.

**Пример → результат**

**Условие:** Файл остаётся во временном каталоге; `overwrite:false` — безопасный вариант для новых скриптов.

```javascript
function Run() {
    var ext = window.external;
    var path = ext.CombinePath(ext.GetTempDirectory(),
                               'fbe-api-' + ext.GetUUID() + '.txt');
    try {
        var ok = ext.WriteTextFileEx(path, 'Новый отчёт', false, 'utf-8');
        ext.MsgBox(ok ? 'Создано без перезаписи: ' + path : 'Ошибка');
    } catch (e) { ext.MsgBox('Не удалось создать файл'); }
}
```

**Ожидаемый результат:** Создаётся новый файл с текстом **«Новый отчёт»**, возвращается **`true`**. При повторном создании **того же** пути с `overwrite:false` возникнет COM-ошибка, старый файл останется целым.

<a id="api-fileexists"></a>

### `FileExists(path)`

**Назначение:** Проверяет наличие доступного обычного файла (не каталога).

**Сигнатура и параметры:** `window.external.FileExists(path)`.

DISPID **133** · API 1.1.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `path` | Да | Строка пути к файлу; требования к абсолютности и существованию приведены ниже. |

**Возвращаемое значение:** `true` или `false`; отсутствие доступа также может дать `false`.

**Особенности, ограничения и ошибки:** Не резервирует файл и не гарантирует отсутствие гонки после проверки. Для запрета перезаписи используйте атомарный файловый метод с `overwrite:false`.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var path = ext.CombinePath(ext.GetCurrentScriptDirectory(), 'example.txt');
    ext.MsgBox('example.txt существует: ' + ext.FileExists(path));
}
```

**Ожидаемый результат:** **`true`** при наличии обычного доступного файла `example.txt` рядом со скриптом и **`false`** при его отсутствии.

<a id="api-directoryexists"></a>

### `DirectoryExists(path)`

**Назначение:** Проверяет наличие доступного каталога.

**Сигнатура и параметры:** `window.external.DirectoryExists(path)`.

DISPID **134** · API 1.1.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `path` | Да | Строка пути к файлу; требования к абсолютности и существованию приведены ниже. |

**Возвращаемое значение:** `true` или `false`.

**Особенности, ограничения и ошибки:** Проверяет существование каталога; не создаёт его и не подтверждает права записи.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var dir = ext.GetCurrentScriptDirectory();
    ext.MsgBox('Каталог скрипта существует: ' + ext.DirectoryExists(dir));
}
```

**Ожидаемый результат:** При штатном запуске скрипта из существующего каталога — **`true`**.

<a id="api-createdirectory"></a>

### `CreateDirectory(path)`

**Назначение:** Создаёт указанный каталог и недостающие родительские каталоги.

**Сигнатура и параметры:** `window.external.CreateDirectory(path)`.

DISPID **135** · API 1.1.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `path` | Да | Строка пути к файлу; требования к абсолютности и существованию приведены ниже. |

**Возвращаемое значение:** `true` также для уже существующего каталога.

**Особенности, ограничения и ошибки:** Ограничения пути зависят от используемого Shell/Windows API, включая MAX_PATH в этой операции.

**Пример → результат**

**Условие:** Созданный учебный каталог не удаляется автоматически; удалите его после проверки.

```javascript
function Run() {
    var ext = window.external;
    var path = ext.CombinePath(ext.GetTempDirectory(), 'FBE-API-Demo-' + ext.GetUUID());
    var created = ext.CreateDirectory(path);
    ext.MsgBox('Каталог создан: ' + created + '\n' + path);
}
```

**Ожидаемый результат:** Создаётся новая папка в `%TEMP%`; выводится **`true`** и её путь.

<a id="api-combinepath"></a>

### `CombinePath(directory, name)`

**Назначение:** Соединяет каталог и имя в нормализованный абсолютный Windows-путь.

**Сигнатура и параметры:** `window.external.CombinePath(directory, name)`.

DISPID **136** · API 1.1.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `directory` | Да | Строка пути к каталогу. |
| `name` | Да | Строка: имя файла для объединения с каталогом. |

**Возвращаемое значение:** Строка пути.

**Особенности, ограничения и ошибки:** Если второй аргумент уже абсолютный, он заменяет первый.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var p = ext.CombinePath('C:\\Books', 'reports\\list.txt');
    ext.MsgBox(p);
}
```

**Ожидаемый результат:** Результат: **`C:\Books\reports\list.txt`** (пути нормализуются; сам файл не создаётся).

<a id="api-getfilename"></a>

### `GetFileName(path)`

**Назначение:** Возвращает последний компонент пути, не обращаясь к диску.

**Сигнатура и параметры:** `window.external.GetFileName(path)`.

DISPID **137** · API 1.1.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `path` | Да | Строка пути к файлу; требования к абсолютности и существованию приведены ниже. |

**Возвращаемое значение:** Строка имени файла.

**Особенности, ограничения и ошибки:** Операция над строкой пути; файл не открывается и его существование не проверяется.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var name = ext.GetFileName('C:\\Books\\Novel.fb2');
    ext.MsgBox(name);
}
```

**Ожидаемый результат:** Строка **`Novel.fb2`**; существование файла на диске не проверяется.

<a id="api-getdirectoryname"></a>

### `GetDirectoryName(path)`

**Назначение:** Возвращает каталог из полного пути.

**Сигнатура и параметры:** `window.external.GetDirectoryName(path)`.

DISPID **138** · API 1.1.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `path` | Да | Строка пути к файлу; требования к абсолютности и существованию приведены ниже. |

**Возвращаемое значение:** Строка без завершающего разделителя, кроме корня, или `""` для имени без каталога.

**Особенности, ограничения и ошибки:** Не проверяет существование каталога. Для объединения частей пути используйте `CombinePath`.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var dir = ext.GetDirectoryName('C:\\Books\\Novel.fb2');
    ext.MsgBox(dir);
}
```

**Ожидаемый результат:** Строка **`C:\Books`**; файл может не существовать.

<a id="api-getextension"></a>

### `GetExtension(path)`

**Назначение:** Выделяет расширение последнего компонента пути.

**Сигнатура и параметры:** `window.external.GetExtension(path)`.

DISPID **139** · API 1.1.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `path` | Да | Строка пути к файлу; требования к абсолютности и существованию приведены ниже. |

**Возвращаемое значение:** Строка с точкой (`".fb2"`) либо `""`.

**Особенности, ограничения и ошибки:** Расширение не доказывает тип или корректность содержимого файла; при сравнении при необходимости приводите строку к нижнему регистру.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var extName = ext.GetExtension('C:\\Books\\Novel.fb2');
    ext.MsgBox(extName);
}
```

**Ожидаемый результат:** Строка **`.fb2`** (с точкой). Для имени без расширения вернётся `""`.

<a id="api-gettempdirectory"></a>

### `GetTempDirectory()`

**Назначение:** Возвращает системную временную папку.

**Сигнатура и параметры:** `window.external.GetTempDirectory()`.

DISPID **140** · API 1.1.0.

Параметров нет.

**Возвращаемое значение:** Строка каталога с завершающим разделителем.

**Особенности, ограничения и ошибки:** Чтение пути не создаёт файл. Временный файл или отчёт нужно создать отдельно и при необходимости удалить после использования.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var dir = ext.GetTempDirectory();
    ext.MsgBox('Временный каталог: ' + dir);
}
```

**Ожидаемый результат:** Реальный путь вроде **`C:\Users\User\AppData\Local\Temp\`** с завершающим разделителем.

<a id="api-gettempfilename"></a>

### `GetTempFileName([prefix[, extension]])`

**Назначение:** Создаёт уникальный пустой файл во временном каталоге.

**Сигнатура и параметры:** `window.external.GetTempFileName([prefix[, extension]])`.

DISPID **141** · API 1.1.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `prefix` | Нет | Строка: префикс имени; по умолчанию `fbe-`. |
| `extension` | Нет | Строка: расширение; по умолчанию `.tmp`. |

**Возвращаемое значение:** Абсолютный путь уже созданного файла.

**Особенности, ограничения и ошибки:** По умолчанию `prefix="fbe-"`, `extension=".tmp"`; файл требуется удалить самому скрипту. Префикс и расширение не должны содержать недопустимые символы пути, каждое ограничено 64 UTF-16 единицами.

**Пример → результат**

**Условие:** Метод **создаёт файл**, а не только придумывает его имя. Удаление временного файла остаётся задачей вызывающего скрипта.

```javascript
function Run() {
    var ext = window.external;
    var path = ext.GetTempFileName('fbe-api-', '.txt');
    ext.MsgBox('Уже создан пустой временный файл: ' + path);
}
```

**Ожидаемый результат:** Возвращается путь уникального **уже существующего пустого** файла `fbe-api-<GUID>.txt` в `%TEMP%`.

## Настройки пользовательского скрипта

<a id="api-getscriptsetting"></a>

### `GetScriptSetting(name, defaultValue)`

**Назначение:** Читает персональную настройку выполняемого скрипта.

**Сигнатура и параметры:** `window.external.GetScriptSetting(name, defaultValue)`.

DISPID **111** · API 1.0.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `name` | Да | Строка: ключ настройки текущего скрипта. |
| `defaultValue` | Да | Строка, число или логическое значение: результат при отсутствии ключа либо несовпадении типа. |

**Возвращаемое значение:** Сохранённое значение совместимого типа или `defaultValue`.

**Особенности, ограничения и ошибки:** Поддерживаются строка, boolean и конечное число. Значение не должно быть объектом/массивом или `null`. При повреждённом/недоступном файле может возвращаться defaultValue.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var limit = ext.GetScriptSetting('docs.demo.limit', 25);
    ext.MsgBox('Лимит: ' + limit);
}
```

**Ожидаемый результат:** Если ключ не записывался, возвращается значение по умолчанию **`25`**; иначе сохранённое числовое значение.

<a id="api-setscriptsetting"></a>

### `SetScriptSetting(name, value)`

**Назначение:** Сохраняет настройку текущего скрипта в отдельном хранилище.

**Сигнатура и параметры:** `window.external.SetScriptSetting(name, value)`.

DISPID **112** · API 1.0.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `name` | Да | Строка: ключ настройки текущего скрипта. |
| `value` | Да | Строка, число или логическое значение; тип сохраняется вместе со значением. |

**Возвращаемое значение:** `true` при успешной атомарной записи, `false` при ошибке доступа или лимите.

**Особенности, ограничения и ошибки:** Имя — непустая строка до 256 UTF-16 единиц; значения: строка до 32768 единиц, конечное число или boolean. Нужен контекст скрипта.

**Пример → результат**

**Условие:** Настройка сохраняется между запусками скрипта, пока её не удалить.

```javascript
function Run() {
    var ext = window.external;
    var ok = ext.SetScriptSetting('docs.demo.enabled', true);
    ext.MsgBox('Настройка сохранена: ' + ok);
}
```

**Ожидаемый результат:** При успешной записи — **`true`**. При последующем запуске того же скрипта `GetScriptSetting` увидит `true`.

<a id="api-removescriptsetting"></a>

### `RemoveScriptSetting(name)`

**Назначение:** Удаляет один ключ настроек текущего скрипта.

**Сигнатура и параметры:** `window.external.RemoveScriptSetting(name)`.

DISPID **113** · API 1.0.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `name` | Да | Строка: ключ настройки текущего скрипта. |

**Возвращаемое значение:** Boolean успеха; отсутствие ключа не считается ошибкой.

**Особенности, ограничения и ошибки:** Удаляет только ключ текущего скрипта. Вне контекста или при ошибке сохранения проверяйте возвращаемое логическое значение.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    ext.SetScriptSetting('docs.demo.enabled', true);
    var removed = ext.RemoveScriptSetting('docs.demo.enabled');
    var value = ext.GetScriptSetting('docs.demo.enabled', false);
    ext.MsgBox('Удалено: ' + removed + '; значение по умолчанию: ' + value);
}
```

**Ожидаемый результат:** При успешных операциях — **«Удалено: true; значение по умолчанию: false»**.

<a id="api-clearscriptsettings"></a>

### `ClearScriptSettings()`

**Назначение:** Удаляет **все** настройки только текущего скрипта.

**Сигнатура и параметры:** `window.external.ClearScriptSettings()`.

DISPID **114** · API 1.0.0.

Параметров нет.

**Возвращаемое значение:** Boolean успеха.

**Особенности, ограничения и ошибки:** Не затрагивает настройки других скриптов и переменные исходных JS-файлов.

**Пример → результат**

**Условие:** Это потенциально разрушительная операция для пользовательских настроек данного `.js`. Перед запуском убедитесь, что это тестовый скрипт.

```javascript
function Run() {
    var ext = window.external;
    if (!ext.AskYesNo('Удалить ВСЕ сохранённые настройки этого скрипта?')) return;
    var ok = ext.ClearScriptSettings();
    ext.MsgBox('Настройки скрипта очищены: ' + ok);
}
```

**Ожидаемый результат:** Только после подтверждения удаляются **все** ключи текущего скрипта. При успехе выводится **`true`**.

## Диагностика, журнал и локализация

<a id="api-isdiagnostictraceenabled"></a>

### `IsDiagnosticTraceEnabled()`

**Назначение:** Проверяет, включена ли штатная диагностическая трассировка редактора.

**Сигнатура и параметры:** `window.external.IsDiagnosticTraceEnabled()`.

DISPID **29** · исходный интерфейс.

Параметров нет.

**Возвращаемое значение:** `true` или `false`.

**Особенности, ограничения и ошибки:** Проверяет состояние трассировки, но не включает её. Не выводите содержимое книги или Clipboard в диагностические сообщения.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var enabled = ext.IsDiagnosticTraceEnabled();
    ext.MsgBox(enabled ? 'Трассировка включена' : 'Трассировка выключена');
}
```

**Ожидаемый результат:** Одна из двух строк в зависимости от состояния штатной диагностики FBE.

<a id="api-tracescript"></a>

### `TraceScript(code, message)`

**Назначение:** Отправляет диагностическое событие в существующую трассировку.

**Сигнатура и параметры:** `window.external.TraceScript(code, message)`.

DISPID **30** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `code` | Да | Строка технического кода события. |
| `message` | Да | Строка сообщения. |

**Возвращаемое значение:** Результат не используется.

**Особенности, ограничения и ошибки:** Код до 32 символов; сообщение проходит политику приватности и ограничено 512 символами. Не предназначено для хранения исходного текста книги.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    ext.TraceScript('API-DEMO', 'Шаг проверки завершён');
    ext.MsgBox('Событие передано подсистеме трассировки');
}
```

**Ожидаемый результат:** При включённой трассировке создаётся диагностическое событие с кодом `API-DEMO`; это **не** запись произвольного текста книги.

<a id="api-getlocalizedstring"></a>

### `GetLocalizedString(key)`

**Назначение:** Получает локализованную строку из существующего каталога интерфейса.

**Сигнатура и параметры:** `window.external.GetLocalizedString(key)`.

DISPID **31** · исходный интерфейс.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `key` | Да | Строка ключа локализации. |

**Возвращаемое значение:** Строка по ключу.

**Особенности, ограничения и ошибки:** Работает с уже известными редактору ключами; не добавляет переводы произвольному скрипту.

**Пример → результат**

**Условие:** Содержимое локализованных ресурсов зависит от текущего языка и состава каталога.

```javascript
function Run() {
    var ext = window.external;
    var value = ext.GetLocalizedString('fbe.script.run_failed');
    ext.MsgBox(value ? value.substring(0, 160) : '(нет строки по ключу)');
}
```

**Ожидаемый результат:** Для известного ключа редактора выводится локализованный шаблон ошибки запуска скрипта (возможно, с `{path}`). Для неизвестного ключа — пустая строка.

<a id="api-logmessage"></a>

### `LogMessage(level, message)`

**Назначение:** Отправляет событие в штатный диагностический журнал.

**Сигнатура и параметры:** `window.external.LogMessage(level, message)`.

DISPID **144** · API 1.2.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `level` | Да | Строка: уровень `debug`, `info`, `warning` или `error`. |
| `message` | Да | Строка сообщения. |

**Возвращаемое значение:** `true`, если событие принято; `false`, если журнал выключен или достигнут лимит.

**Особенности, ограничения и ошибки:** Уровни: `debug`, `info`, `warning`, `error` (с учётом регистра). Размер сообщения до 512 UTF-16 единиц. **Содержимое сообщения фильтруется политикой приватности**: журнал может содержать только токен контекста, факт сообщения и длину, а не сам текст.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var accepted = ext.LogMessage('info', 'Проверка API выполнена');
    ext.MsgBox('Сообщение принято журналом: ' + accepted);
}
```

**Ожидаемый результат:** При включённом журнале и соблюдении лимитов — **`true`**; иначе **`false`**. Сам текст события может не сохраняться из-за фильтра приватности.

<a id="api-getlastscriptingerror"></a>

### `GetLastScriptingError()`

**Назначение:** Получает последнюю техническую ошибку новых методов API в текущем контексте.

**Сигнатура и параметры:** `window.external.GetLastScriptingError()`.

DISPID **145** · API 1.2.0.

Параметров нет.

**Возвращаемое значение:** JSON-строка `{"schemaVersion":1,"hasError":false}` либо описание `method`, `hresult`, `code`, `message`.

**Особенности, ограничения и ошибки:** Отслеживает ошибки HRESULT методов DISPID 100–168 и технический отказ `ValidateCurrentDocument`. Старые методы DISPID 1–35 и обычный возврат `false` автоматически сюда не записываются. Успешные вызовы не стирают ошибку.

**Пример → результат**

**Условие:** Это демонстрация последней COM-ошибки **новых** методов; диагностический вызов намеренно неверный.

```javascript
function Run() {
    var ext = window.external;
    try { ext.GetTempFileName('bad/name', '.tmp'); }
    catch (e) { /* Намеренно вызываем ошибку аргумента. */ }
    var json = ext.GetLastScriptingError();
    ext.MsgBox(json);
}
```

**Ожидаемый результат:** JSON с **`"hasError":true`**, `"method":"GetTempFileName"` и кодом `invalid-argument` (текст на языке Windows).

<a id="api-clearlastscriptingerror"></a>

### `ClearLastScriptingError()`

**Назначение:** Явно сбрасывает сохранённую ошибку новых методов текущего скрипта.

**Сигнатура и параметры:** `window.external.ClearLastScriptingError()`.

DISPID **146** · API 1.2.0.

Параметров нет.

**Возвращаемое значение:** `true` при очистке.

**Особенности, ограничения и ошибки:** Вложенный скрипт имеет собственное состояние и не очищает ошибку родителя.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    try { ext.GetTempFileName('bad/name', '.tmp'); } catch (e) {}
    var cleared = ext.ClearLastScriptingError();
    var json = ext.GetLastScriptingError();
    ext.MsgBox('Очищено: ' + cleared + '\n' + json);
}
```

**Ожидаемый результат:** После очистки: **`true`** и JSON **`{"schemaVersion":1,"hasError":false}`**.

## Практические задачи и готовые решения

Небольшие вызовы отдельных методов приведены **непосредственно в их описаниях**. Ниже — подборка задач, которые требуют нескольких методов API. Полный код и пример результата находятся [в последнем разделе документа](#сложные-скрипты-сочетание-методов-api).

| Практическая задача | Ключевые методы | Полный скрипт |
|---|---|---|
| Проверить книгу и сохранить отчёт без перезаписи | `ValidateCurrentDocument`, `GetDocumentFileName`, `WriteTextFileEx` | [А. Аудит книги](#complex-report) |
| Массово заменить пробел, сохранив Undo | `GetNBSP`, `BeginDocumentUpdate`, `BeginProgress`, `EndDocumentUpdate` | [Б. Неразрывный пробел](#complex-nbsp) |
| Проанализировать размер и разрешение встроенных изображений | `GetBinarySize`, `GetImageDimsByData`, `SelectFolder` | [В. Таблица изображений](#complex-images) |
| Поменять название в XML с проверкой | `InputBoxEx`, `GetSourceText`, `ValidateSourceText`, `ApplySourceText` | [Г. Название книги](#complex-title) |
| Сохранить выделенный текст с диагностикой ошибок | `GetSelectionText`, `SelectSaveFile`, `WriteTextFileEx` | [Д. Экспорт цитаты](#complex-selection) |

## Ограничения, контракты и ошибки

### Файловые диалоги и InputBoxEx

Диалоги используют текстовый JSON-объект опций размером до **65 536 UTF-16 единиц**. Поля `title`, `initialDirectory`, `initialFileName`, `defaultExtension` (строки), `multiSelect` (boolean) и `filters` (до 32 объектов `{"name":"...","pattern":"*.fb2;*.zip"}`) поддерживаются применительно к соответствующему диалогу. Неизвестные и повторные поля, неверные типы и NUL в строках отклоняются. Для `SelectSaveFile` нельзя задавать `multiSelect:true`, для `SelectFolder` — `filters`.

При успехе результат имеет `status:"ok"` и `path` либо `paths`; при отмене `status:"cancelled"`; при ошибке — `status:"error"`, `code`, `hresult`, `message`. Возможные коды: `invalid-options`, `owner-unavailable`, `dialog-failed`. Текст `message` определяется Windows — **не сравнивайте его строкой**. При выборе файлов и папок редактор не открывает документ и не меняет dirty-state. Выбор места сохранения не записывает файл.

`InputBoxEx` различает подтверждение даже пустого ввода (`accepted:true`) и отмену (`accepted:false`) и не изменяет результат исторического `GetModalResult()`.

### Настройки скриптов

Четыре метода настроек работают только при активном контексте. Хранение выполняется в файле `ScriptSettings.xml` (каталог настроек установленной или portable-версии). Для зарегистрированного скрипта область привязана к его UID; иначе — к нормализованному абсолютному пути, и переименование может создать другую область. Максимум: **1024 настройки на все скрипты**, **1 МиБ сериализованного файла**. Изменения сохраняются посредством временного файла и атомарного переименования; ошибки записи возвращают `false`, без перезаписи повреждённого исходного файла. Старые переменные пользовательских JS и настройки карточек не изменяются.

### Закладки, пакетный Undo, прогресс

Закладки выделения принадлежат только текущему контексту скрипта (до **128** одновременно). После смены режима/документа, изменений DOM или Source они могут стать недействительными. В Source используются позиции Scintilla, корректно преобразуемые между UTF-8 и UTF-16; для снимков Source есть бюджет 64 МиБ на контекст и отдельный бюджет 64 МиБ для Description. Не пытайтесь хранить токены закладок между запусками.

`BeginDocumentUpdate` поддерживает только пустой объект опций или `{"suspendRedraw":true}`; открытые группы закрываются в порядке LIFO. Подавление перерисовки не отключает орфографию и не откладывает обновление дерева; соответствующая оптимизация **не реализована**. Это группировка Undo, а не безопасная транзакция отката. Одновременно допускается только одно активное окно прогресса на UI-поток; пользовательскую отмену необходимо проверять явным вызовом `IsProgressCancelled`.

### Текстовые файлы

Поддерживаемые кодировки: `utf-8`, `utf-8-bom`, `utf-16le` (с BOM при записи), `windows-1251`. При чтении без явно указанной кодировки BOM UTF-8/UTF-16LE распознаётся автоматически, а без BOM применяется UTF-8. Некорректный UTF и символы, которые нельзя представить в Windows-1251, не записываются с потерями.

Предел файла/записанного результата — **8 МиБ**, входного текста — **8 Ми UTF-16 единиц**. Директория назначения должна существовать. Запись проходит через временный файл, сброс на диск и атомарное переименование. `WriteTextFile` заменяет существующий файл; `WriteTextFileEx` требует явного `overwrite`. Повторные попытки предусмотрены для кратковременных конфликтов доступа, но постоянный отказ заканчивается COM-ошибкой.

У `GetTempFileName` префикс и расширение не длиннее 64 UTF-16 единиц; запрещены недопустимые для Windows символы и сегменты путей. Файл создаётся сразу с уникальным GUID в имени; удаление лежит на скрипте.

### Валидация и диагностика API 1.2.0

`ValidateCurrentDocument("{}")` не меняет состояние книги. Типовой успешный ответ:

```json
{"schemaVersion":1,"status":"ok","valid":true,"diagnosticsComplete":true,"errors":[],"warnings":[]}
```

При невалидном XML `status` остаётся `ok`, `valid:false`; штатный обработчик сообщает только первую ошибку (`diagnosticsComplete:false`), `warnings` пока пуст. При техническом отказе — `status:"error"`, `valid:null`, ошибка `no-document`/`validation-unavailable`. В FBD `valid:true` означает корректность структуры FBD, не полную схему FB2. Максимум аргумента `optionsJson` — 1024 UTF-16 единицы, допустим только пустой объект. Source-диагностика (`GetLastSourceDiagnostic`) от этого вызова не затирается.

`LogMessage` принимает уровни `debug`, `info`, `warning`, `error`. Лимиты — **32 события за 1000 мс** на UI-поток и **256 событий** на контекст. При выключенной трассировке или превышении лимита возвращает `false`. Для приватности произвольный текст записи не сохраняется как есть: журнал хранит структурированные сведения, а не содержимое книги, путь скрипта или персональные данные.

`GetLastScriptingError` отслеживает ошибки HRESULT **новых** методов (DISPID 100–168) в текущем контексте, но не обобщает возвраты `false` или ошибки старого интерфейса. Пример состояния:

```json
{"schemaVersion":1,"hasError":true,"method":"WriteTextFileEx","hresult":"0x80070005","code":"access-denied","message":"Отказано в доступе"}
```

Успешный вызов не стирает сохранённую ошибку — очистку выполняет `ClearLastScriptingError()`. Текст `message` локализован средствами Windows; для логики лучше использовать `code`/`hresult`. Вложенные скрипты не разделяют состояние ошибки с родителем. По завершении скрипта сведения уничтожаются.

<a id="binary-api-limits"></a>

### Бинарные объекты API 1.3.0

Все шесть методов работают в Design/Description. В Source они возвращают `E_NOTIMPL` (`0x80004001`), без переключения представления, чтения старого Body или потери несохранённого XML. Add/Replace/Remove также отвергаются при открытом BeginDocumentUpdate (включая вложенные вызовы), старом BeginUndoUnit или другой открытой группе IOleUndoManager: `E_UNEXPECTED`, до изменений. Сначала завершите группу. несохранённые изменения в режиме Source остаются нетронутыми.

Входные файлы читаются без конвертации, с запретом изменения/удаления на время чтения. Допустимы размеры **1 байт — 64 МиБ**; пустые и более крупные файлы отклоняются с `ERROR_FILE_TOO_LARGE`. PNG, JPEG, GIF, BMP и TIFF распознаются по сигнатуре и проверяются CImage. Лимиты изображения — 16384 по каждой оси и 32 миллиона пикселей; явно заданный MIME изображения должен совпадать с содержимым. PDF определяется по заголовку. Для прочих не-изображений неоднозначный MIME задаётся явно, например `application/octet-stream`; произвольные форматы не декодируются. Исходные байты сохраняются без перекодирования. Эти методы не меняют прежний импорт с пользовательскими настройками преобразования.

**Ошибки:** нет файла — `0x80070002`; нет ID — `0x80070490`; неверный MIME/формат — `E_INVALIDARG`; память — `E_OUTOFMEMORY`. Ошибки доступа и блокировки сохраняют Win32/COM HRESULT. GetLastScriptingError регистрирует ошибки вызовов, а успешные вызовы предыдущую ошибку не стирают.

**Удаление ссылок:** `removeReferences:false` не разрешает удалить используемое вложение (`E_ACCESSDENIED`). При `true` поддерживаются только DIV.image и SPAN.image с `href="#id"` в Body, содержащие IMG `src="fbw-internal:#id"` и пробельные узлы, а также выбранные обложки tiCover/stiCover. Удаляются соответствующие обёртки изображений, обложки очищаются. Произвольные ссылки A, неизвестные SELECT, дополнительные узлы внутри обёртки и другие неподдерживаемые ссылки приводят к отказу до изменений. Дополнительно применяется прежняя консервативная проверка BinaryIsReferenced; существующий UI удаления не изменён.

**Undo/Redo:** каждая успешная изменяющая операция создаёт одну единицу штатного IOleUndoManager. Сохраняются исходные SAFEARRAY, метаданные, узлы, позиции изображений, обложки и выделение. Предшествующая история остаётся доступной; новая успешная правка обычным образом заменяет ветку Redo. При ошибке выполняется компенсация и закрываются временные группы. Сохранение между операцией и Undo устанавливает новую точку: Undo делает книгу изменённой, Redo к сохранённому состоянию — чистой. GetBinaryInfo, GenerateUniqueBinaryId и SaveAllBinaries не изменяют книгу, dirty-state или историю.

**Экспорт:** файлы записываются во временный файл в том же каталоге, сбрасываются на диск и атомарно переименовываются. Имена начинаются с `_`; недопустимые Windows-символы заменяются, длинные ID сокращаются до 100 UTF-16 единиц без разрыва пары, совпадения получают суффикс. Это защищает также DOS device names и выход за выбранный каталог. Слишком длинный путь явно отклоняется с `ERROR_FILENAME_EXCED_RANGE`, каталог не усекается. Существующий каталог вместо целевого файла считается ошибкой. `status:"error"` и пообъектные HRESULT обозначают частичную ошибку; успешные файлы сохраняются. StartupTrace не получает исходные байты и имена пользовательских файлов.

### Совместимость с исходным COM-контрактом

Исходный `IExternalHelper` сохраняет IID, типы, порядок vtable и DISPID **1–35**; методы API 1.0.0/1.1.0/1.2.0/1.3.0/1.4.0/1.5.0 доступны через аддитивный `IDispatch` с DISPID **100–168**. Старый typelib не объявляет новые функции, поэтому для вызовов из скриптов следует обращаться через динамический `window.external`, а не ожидать их наличия в прежнем статически типизированном интерфейсе. Вызовы с неверным числом или типами параметров отвергаются через COM-ошибки (`DISP_E_BADPARAMCOUNT`, `DISP_E_TYPEMISMATCH`), вне контекста — где применимо — `E_UNEXPECTED`.

Сверяемые реализации: [`src/contracts/fbe.idl`](../src/contracts/fbe.idl), [`src/fbe/ExternalHelper.cpp`](../src/fbe/ExternalHelper.cpp), [`src/fbe/scripts/ScriptingApi.inl`](../src/fbe/scripts/ScriptingApi.inl), [`src/fbe/scripts/ScriptingStage2.inl`](../src/fbe/scripts/ScriptingStage2.inl) и [`src/fbe/scripts/ScriptingDiagnostics.inl`](../src/fbe/scripts/ScriptingDiagnostics.inl), [`src/fbe/scripts/ScriptingBinary.inl`](../src/fbe/scripts/ScriptingBinary.inl), [`src/fbe/scripts/ScriptBinaryDocument.h`](../src/fbe/scripts/ScriptBinaryDocument.h) и [`src/fbe/scripts/ScriptBinaryEdit.h`](../src/fbe/scripts/ScriptBinaryEdit.h).

## Диалоги, буфер обмена и статусный стек API 1.4.0

Реализация: [`ScriptExtendedDialogs.h`](../src/fbe/scripts/ScriptExtendedDialogs.h), [`ScriptClipboard.h`](../src/fbe/scripts/ScriptClipboard.h), [`ScriptStatus.h`](../src/fbe/scripts/ScriptStatus.h), [`ScriptingStage3C.inl`](../src/fbe/scripts/ScriptingStage3C.inl). Существующие пути вставки HTML и сохранения книги эти методы не заменяют.

Все методы этого раздела доступны начиная с **API 1.4.0** в Design, Description и Source. Они не вставляют данные в книгу и не создают операций Undo. Параметры — позиционные строки `BSTR`; неверные типы/число аргументов дают COM-ошибку. Строки должны содержать корректный UTF-16 без встроенных NUL.

Диалоги принадлежат главному окну, возвращают фокус после закрытия и блокируют повторный вход через COM до закрытия. Ошибки параметров возвращаются как JSON `{"status":"error","code":"invalid-options","hresult":"0x80070057","message":"..."}` без открытия пустого окна; отказ создания диалога — `dialog-failed`, недоступный владелец — `owner-unavailable`. Отмена является обычным результатом.

<a id="api-showmessage"></a>

### `ShowMessage(message, title, buttons, icon)`

**Назначение:** Показывает сообщение с заданными кнопками и значком.

**Сигнатура и параметры:** `window.external.ShowMessage(message, title, buttons, icon)`.

DISPID **153** · API 1.4.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `message` | Да | Строка сообщения. |
| `title` | Да | Строка заголовка окна. |
| `buttons` | Да | Строка: `ok`, `okcancel`, `yesno` или `yesnocancel`. |
| `icon` | Да | Строка: `none`, `info`, `warning`, `error` или `question`. |

**Возвращаемое значение:** JSON-строка со `status` и `button`; отмена имеет `status:"cancelled"`.

**Особенности, ограничения и ошибки:** Сообщение и заголовок — до 32767 UTF-16 единиц каждый.

`buttons`: `ok`, `okcancel`, `yesno`, `yesnocancel`. `icon`: `none`, `info`, `warning`, `error`, `question`. Используется штатная инфраструктура сообщений и темы FBE.

Подтверждение возвращает `button: "ok"` или `"yes"`; «Нет» не считается отменой. Старые `MsgBox`, `AskYesNo` и `GetModalResult` сохраняют свой контракт.

**Пример → результат**

```javascript
function Run() {
    try {
        var result = window.external.ShowMessage("Продолжить?", "Проверка", "yesnocancel", "question");
        // «Нет»: {"status":"ok","button":"no"}
        // Отмена: {"status":"cancelled","button":"cancel"}
        window.external.MsgBox(result);
    } catch (e) { window.external.MsgBox("Ошибка: " + (e.description || e.message)); }
}
```

**Ожидаемый результат:** После выбора «Нет» возвращается `{"status":"ok","button":"no"}`; Esc или «Отмена» возвращают `{"status":"cancelled","button":"cancel"}`.

<a id="api-selectfromlist"></a>

### `SelectFromList(optionsJson)`

**Назначение:** Предлагает выбрать один элемент по стабильному ID.

**Сигнатура и параметры:** `window.external.SelectFromList(optionsJson)`.

DISPID **154** · API 1.4.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `optionsJson` | Да | Строка JSON с параметрами операции; поля, значения по умолчанию и ограничения описаны ниже. |

**Возвращаемое значение:** JSON-строка: `{"status":"ok","selectedId":"..."}`, отмена или ошибка.

**Особенности, ограничения и ошибки:** Выбор одной строки стандартного Win32-списка с Enter/Esc, клавиатурной навигацией, изменением размера и прокруткой длинных подписей.

`items` обязателен и не пуст. Каждый элемент содержит только `id` и `label`; `id` непустой, уникальный, до 256 UTF-16 единиц. Подпись, заголовок и приглашение — до 4096 единиц; JSON — до 1 МиБ единиц, список — до 2048 элементов. `title`, `prompt`, `selectedId` необязательны. Неизвестный предварительный ID, дубликаты ключей/ID, некорректный Unicode и неизвестные поля дают `invalid-options`. Без предварительного выбора выделяется первый элемент. Возвращается стабильный ID, а не текст или индекс. Отмена: `{"status":"cancelled"}`.

**Пример → результат**

```javascript
function Run() {
    try {
        var result = window.external.SelectFromList(
            '{"title":"Жанр","prompt":"Выберите жанр","items":[' +
            '{"id":"sf","label":"Фантастика"},{"id":"prose","label":"Проза"}],' +
            '"selectedId":"sf"}');
        // Например: {"status":"ok","selectedId":"prose"}
        window.external.MsgBox(result);
    } catch (e) { window.external.MsgBox("Ошибка: " + (e.description || e.message)); }
}
```

**Ожидаемый результат:** При выборе «Проза» возвращается `{"status":"ok","selectedId":"prose"}`; при отмене — `{"status":"cancelled"}`.

<a id="api-selectmultiplefromlist"></a>

### `SelectMultipleFromList(optionsJson)`

**Назначение:** Предлагает выбрать несколько элементов по стабильным ID.

**Сигнатура и параметры:** `window.external.SelectMultipleFromList(optionsJson)`.

DISPID **155** · API 1.4.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `optionsJson` | Да | Строка JSON с параметрами операции; поля, значения по умолчанию и ограничения описаны ниже. |

**Возвращаемое значение:** JSON-строка со `status` и массивом `selectedIds`; допустим пустой выбор.

**Особенности, ограничения и ошибки:** Пределы и структура элементов совпадают с `SelectFromList`; вместо `selectedId` используется необязательный массив `selectedIds`. Допустим пустой выбор; массив `items` должен содержать хотя бы один элемент.

Стандартный список поддерживает Ctrl/Shift и навигацию клавиатурой. ID возвращаются в порядке исходного списка. Подтверждение пустого выбора: `{"status":"ok","selectedIds":[]}`; отмена: `{"status":"cancelled"}`. Отсутствующие и повторяющиеся предварительные ID недопустимы.

**Пример → результат**

```javascript
function Run() {
    try {
        var result = window.external.SelectMultipleFromList(
            '{"title":"Жанры","items":[{"id":"sf","label":"Фантастика"},' +
            '{"id":"prose","label":"Проза"}],"selectedIds":["prose","sf"]}');
        // {"status":"ok","selectedIds":["sf","prose"]}
        window.external.MsgBox(result);
    } catch (e) { window.external.MsgBox("Ошибка: " + (e.description || e.message)); }
}
```

**Ожидаемый результат:** Если подтвердить оба жанра, возвращается `{"status":"ok","selectedIds":["sf","prose"]}` в порядке исходного списка.

<a id="api-choosecolor"></a>

### `ChooseColor(initialColor)`

**Назначение:** Открывает системный диалог выбора цвета.

**Сигнатура и параметры:** `window.external.ChooseColor(initialColor)`.

DISPID **156** · API 1.4.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `initialColor` | Да | Строка цвета в формате `#RRGGBB`. |

**Возвращаемое значение:** JSON-строка с `color` в верхнем регистре либо статусом отмены/ошибки.

**Особенности, ограничения и ошибки:** Регистр входных шестнадцатеричных цифр произвольный; результат нормализован к верхнему регистру.

Используется системный диалог, совместимый с Windows 7. Его оформление остаётся системным; этот метод сам не меняет цвет текста или оформление книги. Неверная строка цвета даёт `invalid-options`.

**Пример → результат**

```javascript
function Run() {
    try {
        var result = window.external.ChooseColor("#3366cc");
        // Подтверждение: {"status":"ok","color":"#3366CC"}
        // Отмена: {"status":"cancelled"}
        window.external.MsgBox(result);
    } catch (e) { window.external.MsgBox("Ошибка: " + (e.description || e.message)); }
}
```

**Ожидаемый результат:** Подтверждение исходного цвета возвращает `{"status":"ok","color":"#3366CC"}`. Книга не меняется.

<a id="api-getclipboardtext"></a>

### `GetClipboardText()`

**Назначение:** Читает Unicode-текст системного буфера обмена.

**Сигнатура и параметры:** `window.external.GetClipboardText()`.

DISPID **157** · API 1.4.0.

Параметров нет.

**Возвращаемое значение:** Строка `BSTR`; `""`, если текстового формата нет.

**Особенности, ограничения и ошибки:** Читает `CF_UNICODETEXT`.

Отсутствие текстового формата возвращает `""`. Занятый буфер, повреждённая/незавершённая строка, неверный Unicode и превышение размера дают COM-ошибку. Метод не меняет буфер. Отмены диалога нет.

**Пример → результат**

```javascript
function Run() {
    try {
        var text = window.external.GetClipboardText();
        // Например: "Первая строка\r\nВторая строка 😀"
        window.external.MsgBox(text);
    } catch (e) { window.external.MsgBox("Ошибка: " + (e.description || e.message)); }
}
```

**Ожидаемый результат:** Если в буфере «Первая строка\r\nВторая строка 😀», показан этот текст с переносом строки. Буфер и книга не меняются.

<a id="api-setclipboardtext"></a>

### `SetClipboardText(text)`

**Назначение:** Публикует Unicode-текст в системном буфере обмена.

**Сигнатура и параметры:** `window.external.SetClipboardText(text)`.

DISPID **158** · API 1.4.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `text` | Да | Строка текста. |

**Возвращаемое значение:** `true` после успешной публикации; отказ вызывает COM-ошибку.

**Особенности, ограничения и ошибки:** Публикуется `CF_UNICODETEXT`.

Предел — 1 МиБ UTF-16 единиц; память включает завершающий NUL. Проверка и подготовка завершаются до замены буфера, поэтому неверный текст сохраняет прежнее содержимое. Ошибка доступа/публикации даёт COM-ошибку. Форматы предыдущего содержимого заменяются, вставка в книгу не производится.

**Пример → результат**

```javascript
function Run() {
    try {
        window.external.SetClipboardText("Привет, мир!\r\n😀");
    } catch (e) { window.external.MsgBox("Ошибка: " + (e.description || e.message)); }
}
```

**Ожидаемый результат:** Буфер содержит «Привет, мир!» и 😀 на следующей строке. Прежнее содержимое заменяется; книга не меняется.

<a id="api-getclipboardhtml"></a>

### `GetClipboardHtml()`

**Назначение:** Читает и очищает HTML-фрагмент из системного буфера.

**Сигнатура и параметры:** `window.external.GetClipboardHtml()`.

DISPID **159** · API 1.4.0.

Параметров нет.

**Возвращаемое значение:** Очищенная HTML-строка либо `""`, если HTML нет.

**Особенности, ограничения и ошибки:** Читает формат `HTML Format`.

Используется общий `ReadClipboardFragment`/`HtmlPasteSanitizer`: проверяются байтовые диапазоны CF_HTML, UTF-8, размер до 2 МиБ и поддерживаемая разметка. Отсутствие HTML возвращает `""`; повреждение, занятость и неподдерживаемый небезопасный фрагмент дают COM-ошибку. `Legacy` не означает безопасность. Чтение не создаёт DOM, не выполняет скрипты и не загружает внешние ресурсы.

**Пример → результат**

```javascript
function Run() {
    try {
        var html = window.external.GetClipboardHtml();
        // Например: "<p><strong>Цитата 😀</strong></p>"
        window.external.MsgBox(html);
    } catch (e) { window.external.MsgBox("Ошибка: " + (e.description || e.message)); }
}
```

**Ожидаемый результат:** Для безопасной цитаты показан, например, `<p><strong>Цитата 😀</strong></p>`. Чтение не вставляет её в книгу.

<a id="api-setclipboardhtml"></a>

### `SetClipboardHtml(html, plainText)`

**Назначение:** Публикует безопасный HTML вместе с его текстовым представлением.

**Сигнатура и параметры:** `window.external.SetClipboardHtml(html, plainText)`.

DISPID **160** · API 1.4.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `html` | Да | Строка HTML-фрагмента для очистки и публикации. |
| `plainText` | Да | Строка: явно заданное текстовое представление HTML. |

**Возвращаемое значение:** `true` после успешной публикации обоих форматов.

**Особенности, ограничения и ошибки:** Публикует нормализованный `HTML Format` и явно переданный `CF_UNICODETEXT` одним OLE `IDataObject`.

Оба аргумента обязательны; текст ограничен 1 МиБ UTF-16 единиц, итоговый CF_HTML вместе с заголовком — 2 МиБ. Смещения вычисляются по UTF-8 байтам. Пустой HTML допустим. Активное содержимое удаляется штатным санитайзером, если возможно безопасное нормализованное представление; опасные URL, неподдерживаемые сложные/внутренние фрагменты и `Legacy` отвергаются. Ошибки проверки и подготовки не меняют прежний буфер. Отказ публикации даёт COM-ошибку. Маркер внутренней копии FBE не выставляется; обычный Ctrl+V и его Undo остаются штатными. Данные буфера не записываются в журнал или файлы диагностики.

**Пример → результат**

```javascript
function Run() {
    try {
        window.external.SetClipboardHtml("<p><b>Цитата 😀</b></p>", "Цитата 😀"); // true
    } catch (e) { window.external.MsgBox("Ошибка: " + (e.description || e.message)); }
}
```

**Ожидаемый результат:** В буфере находятся нормализованный HTML `<p><strong>Цитата 😀</strong></p>` и текст «Цитата 😀». Книга и Undo не меняются.

<a id="api-pushstatusbartext"></a>

### `PushStatusBarText(text)`

**Назначение:** Временно перекрывает текст строки состояния сообщением скрипта.

**Сигнатура и параметры:** `window.external.PushStatusBarText(text)`.

DISPID **161** · API 1.4.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `text` | Да | Строка текста. |

**Возвращаемое значение:** Строковый токен, необходимый для `PopStatusBarText`.

**Особенности, ограничения и ошибки:** Сообщение — до 4096 UTF-16 единиц, в том числе пустое; глубина — до 64 сообщений одного контекста и до 256 во всех активных контекстах потока. Вне контекста — `E_UNEXPECTED`, превышение лимита — COM-ошибка. Соседние поля статусной строки не меняются. Приоритет: инкрементальный поиск → верхнее скриптовое сообщение → актуальное штатное временное сообщение → контекст документа. Очередь общих UI-сообщений вручную не прокачивается.

**Пример → результат**

```javascript
function Run() {
    try {
        var token = window.external.PushStatusBarText("Обработка книги...");
        try {
            // Работа скрипта.
        } finally {
            window.external.PopStatusBarText(token);
        }
    } catch (e) { window.external.MsgBox("Ошибка: " + (e.description || e.message)); }
}
```

**Ожидаемый результат:** Во время работы видно «Обработка книги...», после `finally` возвращается актуальное состояние редактора.

<a id="api-popstatusbartext"></a>

### `PopStatusBarText(token)`

**Назначение:** Снимает верхнее сообщение текущего скрипта из статусного стека.

**Сигнатура и параметры:** `window.external.PopStatusBarText(token)`.

DISPID **162** · API 1.4.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `token` | Да | Строка: непрозрачный токен, полученный соответствующим методом в этом контексте. |

**Возвращаемое значение:** `true`; неверный или не верхний токен вызывает COM-ошибку.

**Особенности, ограничения и ошибки:** Используется порядок LIFO. Чужой, повторный, очищенный или не верхний токен даёт COM-ошибку без изменения стека. Завершение скрипта, включая исключение JScript/COM, автоматически снимает оставшиеся сообщения. Вложенный скрипт владеет собственным стеком и не может снять сообщения родителя. Закрытие/замена документа обнуляет относящиеся к окну сообщения.

**Пример → результат**

```javascript
function Run() {
    try {
        var first = window.external.PushStatusBarText("Первое");
        var second = window.external.PushStatusBarText("Второе");
        window.external.PopStatusBarText(second); // true; снова «Первое»
        window.external.PopStatusBarText(first);  // true; актуальное состояние редактора
    } catch (e) { window.external.MsgBox("Ошибка: " + (e.description || e.message)); }
}
```

**Ожидаемый результат:** После снятия второго сообщения снова видно «Первое», после снятия первого — штатное состояние редактора.

<a id="api-clearstatusbartext"></a>

### `ClearStatusBarText()`

**Назначение:** Очищает все статусные сообщения текущего скрипта.

**Сигнатура и параметры:** `window.external.ClearStatusBarText()`.

DISPID **163** · API 1.4.0.

Параметров нет.

**Возвращаемое значение:** `true`, в том числе для пустого стека.

**Особенности, ограничения и ошибки:** Сообщения родительского скрипта и системное состояние не удаляются; пустой стек допустим. Вне контекста — COM-ошибка. После очистки отображается актуальное состояние, а не сохранённая копия старой надписи. Прежний `SetStatusBarText` сохраняет своё действие; его временное сообщение становится видимым после снятия скриптового перекрытия.

**Пример → результат**

```javascript
function Run() {
    try {
        window.external.PushStatusBarText("Временное сообщение");
        window.external.ClearStatusBarText(); // true
    } catch (e) { window.external.MsgBox("Ошибка: " + (e.description || e.message)); }
}
```

**Ожидаемый результат:** Временное сообщение исчезает; сообщения родительского скрипта и штатное состояние сохраняются.

## Сохранение и сериализация API 1.5.0

Все пять методов требуют контекст выполняющегося скрипта и открытый документ.
Пакеты `BeginDocumentUpdate`, открытые Undo-группы и повторный вход в сохранение
запрещены. Методы не открывают диалогов: выбор имени выполняется отдельным
`SelectSaveFile()`. Ошибка передаётся COM HRESULT и остаётся доступной через
`GetLastScriptingError()`; успешный вызов не очищает предыдущую ошибку.

| Операция | Текущий путь и savepoint | Undo/Redo | Изменённый Source |
|---|---|---|---|
| Save / Save As | Обновляются после успешной записи | Сохраняются | Отказ до записи; сначала примените Source штатным переключением |
| Copy / файловый экспорт | Сохраняются без изменений | Сохраняются | Читается актуальный Scintilla, без применения к Body |
| Сериализация в строку | Сохраняются без изменений | Сохраняются | Читается актуальный Scintilla |

Снимок использует штатный сериализатор FBE и MSXML6/SAX, включая проверку таблиц,
FBD и бинарных данных. Невалидный Source отклоняется; старый Body вместо него
не подставляется. Снимок не нормализует живой DOM, не меняет выделение, вид,
кодировку и контрольные точки восстановления. Копия не обновляет MRU и рабочий
каталог. Запись готовится во временном файле целевого каталога, проверяется по
объёму и сбрасывается на диск перед созданием или заменой назначения.

`overwrite=false` запрещает замену, в том числе при конкурентном создании файла.
После безопасного отказа, включая конфликт имени при `overwrite:false`,
временный `fbs` удаляется. При неоднозначной ошибке замены, в частности 1176/1177,
завершённый временный результат сохраняется для восстановления: Win32 `ReplaceFileW` не гарантирует полный откат всех отказных
состояний. Не считайте исключение подтверждением, что файл гарантированно не
затронут, если система сообщила частичный результат замены.

Пути должны быть абсолютными, без встроенного NUL или альтернативного потока,
с расширением `.fb2`/`.fbd`. На этом этапе действует ограничение обычных Win32
путей `MAX_PATH`. ZIP/RAR не являются допустимым назначением копии.
Совпадение с исходным файлом/контейнером проверяется также по идентичности файла
Windows, включая hard link и пути через junction. Независимая копия исходного
файла запрещена даже с `overwrite=true`.

<a id="api-savecurrentdocument"></a>

### `SaveCurrentDocument()`

**Назначение:** Сохраняет открытую именованную книгу по текущему пути.

**Сигнатура и параметры:** `window.external.SaveCurrentDocument()`.

DISPID **164** · API 1.5.0.

Параметров нет.

**Возвращаемое значение:** `true` после успешной записи; ошибки передаются через COM.

**Особенности, ограничения и ошибки:** Сохраняет уже именованную книгу через `DocumentSaveController`. Для новой книги
нужен Save As (`save-as-required`). Проверяет доступность и fingerprint файла;
внешние изменения дают ошибку `document-modified-externally`. Для ZIP используется
штатное обновление выбранной записи, сохраняются остальные записи, путь,
occurrence и новый fingerprint. Обратная запись RAR не поддерживается
(`save-not-supported`); используйте Save As в обычный файл.

**Пример → результат**

```javascript
function Run() {
    try { window.external.SaveCurrentDocument(); }
    catch (e) { window.external.MsgBox(window.external.GetLastScriptingError()); }
}
```

**Ожидаемый результат:** Валидная именованная книга сохраняется, dirty-state сбрасывается, история Undo/Redo остаётся. При отказе показана диагностика.

<a id="api-savecurrentdocumentas"></a>

### `SaveCurrentDocumentAs(path, overwrite)`

**Назначение:** Сохраняет книгу в обычный FB2/FBD и делает новый путь текущим.

**Сигнатура и параметры:** `window.external.SaveCurrentDocumentAs(path, overwrite)`.

DISPID **165** · API 1.5.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `path` | Да | Строка пути к файлу; требования к абсолютности и существованию приведены ниже. |
| `overwrite` | Да | Логическое значение: явное разрешение перезаписи. `false` запрещает замену существующего файла. |

**Возвращаемое значение:** `true` после успешной записи и обновления состояния документа.

**Особенности, ограничения и ошибки:** Для COM требуются `BSTR` и `VT_BOOL`; число `1` вместо `true` не принимается. После записи обычного FB2/FBD новый файл становится текущим: обновляются путь,
тип, session, fingerprint, savepoint, MRU, заголовок и статус. Save As из ZIP/RAR
сохраняет исходный архив. Формат определяется расширением и проверяется до записи
без удаления элементов. Для совпадающего с текущим файла действуют проверки Save;
при `overwrite=false` существующее назначение отклоняется.

**Пример → результат**

```javascript
function Run() {
    window.external.SaveCurrentDocumentAs("D:\\Books\\Новое имя.fb2", false);
}
```

**Ожидаемый результат:** При существующем каталоге `D:\Books` и новом имени создаётся файл «Новое имя.fb2»; он становится текущим. Существующий файл не заменяется.

<a id="api-savedocumentcopy"></a>

### `SaveDocumentCopy(path, overwrite)`

**Назначение:** Создаёт независимую копию актуальной книги без изменения открытого документа.

**Сигнатура и параметры:** `window.external.SaveDocumentCopy(path, overwrite)`.

DISPID **166** · API 1.5.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `path` | Да | Строка пути к файлу; требования к абсолютности и существованию приведены ниже. |
| `overwrite` | Да | Логическое значение: явное разрешение перезаписи. `false` запрещает замену существующего файла. |

**Возвращаемое значение:** `true` после успешной записи; не означает сброс dirty-state открытой книги.

**Особенности, ограничения и ошибки:** Для COM требуются `BSTR` и `VT_BOOL`; число `1` вместо `true` не принимается. Сохраняет актуальную независимую копию в текущем типе FB2/FBD и кодировке.
Расширение должно совпадать с типом; скрытого преобразования нет. Исходная книга,
её имя, dirty-state, Undo/Redo и восстановление не меняются. Значение `true`
не означает, что открытая книга стала сохранённой.

**Пример → результат**

```javascript
function Run() {
    window.external.SaveDocumentCopy("D:\\Books\\Копия.fb2", false);
}
```

**Ожидаемый результат:** Для открытого FB2 и нового имени создаётся «Копия.fb2». Текущий путь, изменения, выделение и Undo/Redo сохраняются.

<a id="api-serializecurrentdocument"></a>

### `SerializeCurrentDocument()`

**Назначение:** Получает полный актуальный XML книги без сохранения файла.

**Сигнатура и параметры:** `window.external.SerializeCurrentDocument()`.

DISPID **167** · API 1.5.0.

Параметров нет.

**Возвращаемое значение:** Полный XML `BSTR`; при превышении лимита — COM-ошибка без обрезания.

**Особенности, ограничения и ошибки:** XML включает декларацию, description,
всеми body, пространствами имён, ссылками и binary.

Лимит результата — **8 МиБ UTF-16**, то есть 4 194 304 кодовых единицы.
Превышение даёт COM-ошибку `message-too-long`, XML не обрезается.
**Для крупных книг используйте файловый `SaveSerializedDocumentCopy()`**: его
результат не передаёт XML в JScript и не ограничен строковым лимитом. Внутренняя
память Win32 всё равно ограничена; ошибки выделения памяти отклоняют операцию.
Файловый буфер ограничен 512 МиБ; это отдельный предел защиты памяти процесса.

**Пример → результат**

```javascript
function Run() {
    var xml = window.external.SerializeCurrentDocument();
    window.external.MsgBox("Символов XML: " + xml.length);
}
```

**Ожидаемый результат:** Окно показывает число символов полного XML. Файл не создаётся, открытая книга и история не меняются.

<a id="api-saveserializeddocumentcopy"></a>

### `SaveSerializedDocumentCopy(path, optionsJson)`

**Назначение:** Экспортирует актуальную книгу с явно выбранными форматом и кодировкой.

**Сигнатура и параметры:** `window.external.SaveSerializedDocumentCopy(path, optionsJson)`.

DISPID **168** · API 1.5.0.

| Параметр | Обязателен | Тип и назначение |
|---|---|---|
| `path` | Да | Строка пути к файлу; требования к абсолютности и существованию приведены ниже. |
| `optionsJson` | Да | Строка JSON с параметрами операции; поля, значения по умолчанию и ограничения описаны ниже. |

**Возвращаемое значение:** `true` после успешной записи независимой копии.

**Особенности, ограничения и ошибки:** Оба параметра строго строковые (`BSTR`).

| Ключ JSON | Допустимые значения | По умолчанию |
|---|---|---|
| `format` | `current`, `fb2`, `fbd` | `current` |
| `encoding` | `current`, `utf-8` | `current` |
| `overwrite` | JSON boolean | `false` |

Пустой объект допустим. Неизвестные/повторяющиеся ключи, другие значения и неверные
типы отклоняются. Формат должен соответствовать расширению. Используются
существующие структурные правила FBD: совместимый экспорт FB2→FBD сохраняет body,
а не удаляет их; описание FBD без обязательного для FB2 body нельзя экспортировать
в FB2 посредством выдуманного содержимого. Кодировка исходной книги не меняется.

В Source `encoding:"current"` использует актуальную кодировку XML-декларации
редактируемого текста; без декларации применяется кодировка открытой книги.
Невалидная декларация или неподдерживаемая MSXML кодировка отклоняется до записи
файла. `SerializeCurrentDocument()` отражает ту же текущую декларацию в `BSTR`;
сам `BSTR` остаётся строкой UTF-16. `encoding:"utf-8"` явно устанавливает декларацию
UTF-8 и записывает байты UTF-8. Эти операции не применяют Source к документу и не
меняют его кодировку, текст, выделение, dirty-state или Undo/Redo.

**Пример → результат**

```javascript
function Run() {
    window.external.SaveSerializedDocumentCopy(
        "D:\\Books\\Экспорт.fb2",
        '{"format":"fb2","encoding":"utf-8","overwrite":false}'
    );
}
```

**Ожидаемый результат:** При существующем каталоге `D:\Books` и совместимом FB2 создаётся новый «Экспорт.fb2» в UTF-8; исходная книга и её кодировка не меняются.

## Запланировано, но не доступно для вызова

Перечисленное ниже **не входит** в 104 метода и пока не должно попадать в основной указатель. [Issue #45](https://github.com/sklart/fictionbook-editor-next/issues/45) остаётся открытым до финальной приёмки всех обязательных пунктов.

| Направление | Планируемые методы |
|---|---|
| Отложено | `ProcessPendingUiEvents`, автоматический откат произвольных DOM-изменений |

Другие задачи (например, интерфейс параметров карточек скриптов, issue #48) не следует трактовать как методы API `window.external`.

## Тестовые контуры в репозитории

Для диагностики реализации используются адресные тесты `tools/tests/test-fbe-scripting-stage1.ps1`, `test-fbe-scripting-stage2.ps1`, `test-fbe-scripting-stage3a.ps1`, `test-fbe-scripting-stage3b.ps1`, `test-fbe-scripting-stage3c.ps1`, `test-fbe-scripting-stage3d.ps1` и `test-fbe-typelib-runtime.ps1`. Группы этапов 2–3Б проверяют реальный JScript, Source/Body/Description, Undo, параметры вызова, диалоги и ошибки в пределах предусмотренных сценариев. Это не свидетельство полного тестирования всех пользовательских скриптов.

Проверку нужно проводить отдельно для portable и installed. Запуск на Windows 7 SP1 нельзя считать подтверждённым только по анализу импортов: для этого необходим отдельный реальный прогон на Windows 7.

## Сложные скрипты: сочетание методов API

Здесь приведены **полные скрипты**, а не фрагменты: каждый блок можно сохранить отдельным `.js` в каталоге Scripts FBE Next. Скрипты написаны для **Microsoft JScript**, без `let`, стрелочных функций и `async/await`. Запускайте их по одному. Примеры, меняющие книгу, сначала проверьте на тестовом FB2; сохранение книги на диск не производится автоматически.

> **Чтение JSON.** Несколько новых методов возвращают JSON **строкой**. Там, где результат нужно разобрать, используется `JSON.parse`, если он доступен. Резервный `eval` применяется **только к JSON, непосредственно возвращённому `window.external` в том же вызове**, а не к FB2/XML, внешним файлам или произвольному пользовательскому тексту. Для распространяемого скрипта предпочтительнее включить отдельный проверенный JSON-парсер, не требующий `eval`.

<a id="complex-report"></a>
### Сценарий А. Аудит текущей книги: проверка XML и отчёт без изменения документа

**Задача:** одной командой получить сведения об открытой книге, проверить её валидность и сохранить текстовый отчёт с уникальным именем. Исходный FB2 не меняется. Комбинация: `HasScriptingMethod`, `GetDocumentFileName`, `GetCurrentViewMode`, `IsDocumentModified`, `ValidateCurrentDocument`, `GetSelectionInfo`, `GetDocumentDirectory`, `GetCurrentScriptDirectory`, `CombinePath`, `GetUUID`, `WriteTextFileEx`, `SetStatusBarText`, `GetLastScriptingError`.

```javascript
function HasApi(name) {
    try { return window.external.HasScriptingMethod(name); }
    catch (e) { return false; }
}

function Run() {
    var ext = window.external;
    if (!HasApi('ValidateCurrentDocument') ||
        !HasApi('WriteTextFileEx') || !HasApi('GetSelectionInfo')) {
        ext.MsgBox('Нужен FBE Next с API 1.2.0.');
        return;
    }
    if (!ext.AskYesNo('Проверить книгу и сохранить отчёт?')) return;

    var name = ext.GetDocumentFileName() || '(новая книга)';
    var mode = ext.GetCurrentViewMode();
    var dirty = ext.IsDocumentModified();
    var validation = ext.ValidateCurrentDocument('{}');
    var selection = ext.GetSelectionInfo();

    var report = 'Отчёт FBE Next\r\n' +
        'Книга: ' + name + '\r\n' +
        'Режим: ' + mode + '\r\n' +
        'Несохранённые изменения: ' + dirty + '\r\n' +
        'Валидация: ' + validation + '\r\n' +
        'Информация о выделении: ' + selection + '\r\n';

    // У сохранённой книги отчёт пишется рядом с ней, иначе — рядом со скриптом.
    var dir = ext.GetDocumentDirectory() || ext.GetCurrentScriptDirectory();
    if (!dir) { ext.MsgBox('Не удалось определить каталог отчёта.'); return; }
    var file = 'fbe-audit-' + ext.GetUUID() + '.txt';
    var path = ext.CombinePath(dir, file);

    try {
        ext.WriteTextFileEx(path, report, false, 'utf-8');
        ext.SetStatusBarText('Аудит завершён: ' + name);
        ext.MsgBox('Отчёт сохранён:\n' + path);
    } catch (e) {
        ext.MsgBox('Сбой записи. Диагностика:\n' +
                   ext.GetLastScriptingError());
    }
}
```

**Пример → результат:** если открыта `Повесть.fb2` в режиме Design и есть несохранённые изменения, появится файл `fbe-audit-<UUID>.txt`. В нём будут `Книга: Повесть.fb2`, `Режим: design`, `Несохранённые изменения: true` и JSON результата штатной проверки. **Текст самой книги не экспортируется.** При отказе в правах отчёт не создаётся, книга остаётся неизменной.

<a id="complex-nbsp"></a>
### Сценарий Б. Массовая правка «г. Москва» → «г. Москва» с одним Undo

**Задача:** заменить обычный пробел в строго заданной текстовой последовательности на выбранный в FBE неразрывный пробел, **не уничтожая вложенные теги форматирования**. Комбинация: `GetCurrentViewMode`, `GetNBSP`, `GetScriptSetting`, `AskYesNo`, `BeginDocumentUpdate`, `BeginProgress`, `SetProgress`, `IsProgressCancelled`, `EndProgress`, `EndDocumentUpdate`, `SetStatusBarText`.

```javascript
function ReplaceInTextNodes(node, nbsp) {
    var changed = 0;
    var children = node.childNodes;
    for (var i = 0; i < children.length; i++) {
        var item = children[i];
        if (item.nodeType == 3) { // Текстовый узел, без изменения HTML-тегов.
            var oldText = String(item.nodeValue);
            var matches = oldText.match(/г\. Москва/g);
            if (matches) {
                item.nodeValue = oldText.replace(/г\. Москва/g, 'г.' + nbsp + 'Москва');
                changed += matches.length;
            }
        } else if (item.nodeType == 1) {
            changed += ReplaceInTextNodes(item, nbsp);
        }
    }
    return changed;
}

function Run() {
    var ext = window.external;
    try {
        if (!ext.HasScriptingMethod('BeginDocumentUpdate') ||
            !ext.HasScriptingMethod('BeginProgress')) {
            ext.MsgBox('Требуется API 1.1.0 или новее.');
            return;
        }
    } catch (e) { ext.MsgBox('Новый API недоступен.'); return; }

    if (ext.GetCurrentViewMode() != 'design') {
        ext.MsgBox('Откройте книгу в режиме Design.'); return;
    }
    var enabled = ext.GetScriptSetting('docs.example.fixMoscow', true);
    if (!enabled) { ext.MsgBox('Сценарий отключён в настройках скрипта.'); return; }
    var root = document.getElementById('fbw_body');
    if (!root) return;
    var paragraphs = root.getElementsByTagName('P');
    if (!paragraphs.length) { ext.MsgBox('Нет абзацев.'); return; }
    if (!ext.AskYesNo('Исправить «г. Москва» во всех абзацах книги?')) return;

    var nbsp = ext.GetNBSP();
    var changes = 0;
    var cancelled = false;
    var token = ext.BeginDocumentUpdate('Неразрывный пробел после г.',
                                        '{"suspendRedraw":true}');
    try {
        var progress = null;
        try {
            progress = ext.BeginProgress('Нормализация текста', paragraphs.length);
            for (var i = 0; i < paragraphs.length; i++) {
                if (ext.IsProgressCancelled(progress)) { cancelled = true; break; }
                changes += ReplaceInTextNodes(paragraphs[i], nbsp);
                if (i % 20 == 0 || i + 1 == paragraphs.length) {
                    ext.SetProgress(progress, i + 1, 'Абзац ' + (i + 1));
                }
            }
        } finally {
            if (progress !== null) ext.EndProgress(progress);
        }
    } finally {
        ext.EndDocumentUpdate(token);
    }
    ext.SetStatusBarText('Замен пробела: ' + changes);
    ext.MsgBox('Заменено: ' + changes +
               (cancelled ? '\nОтменено пользователем (частичная правка).' : '') +
               '\nПравку можно отменить одним Ctrl+Z.');
}
```

**Пример → результат:** при наличии трёх совпадений **`г. Москва`** станет **`г. Москва`** (неразрывный пробел); окно покажет **«Заменено: 3»**. `Ctrl+Z` отменит все изменения данной пакетной операции. Если пользователь отменил процесс, уже сделанная часть правок **остаётся**, но также отменяется штатным Undo. Метод `IsProgressCancelled` не выполняет произвольный message pump: в длительных CPU-циклах отклик кнопки отмены может быть не мгновенным.

<a id="complex-images"></a>
### Сценарий В. Инвентаризация изображений FB2 с экспортом TSV

**Задача:** собрать таблицу ID, MIME, размера в байтах и разрешения встроенных изображений, выбрать папку и сохранить TSV для открытия в Excel/Calc. Комбинация: `GetBinarySize`, `GetImageDimsByData`, `GetDocumentFileName`, `SelectFolder`, `CombinePath`, `GetUUID`, `WriteTextFileEx`, `GetLastScriptingError`.

```javascript
function ParseHostJson(value) {
    if (typeof JSON != 'undefined' && JSON && typeof JSON.parse == 'function')
        return JSON.parse(value);
    // Только доверенная JSON-строка, непосредственно возвращённая API FBE.
    return eval('(' + value + ')');
}
function TsvCell(value) {
    return String(value).replace(/[\t\r\n]/g, ' ');
}

function Run() {
    var ext = window.external;
    try {
        if (!ext.HasScriptingMethod('SelectFolder') ||
            !ext.HasScriptingMethod('WriteTextFileEx')) {
            ext.MsgBox('Нужен API 1.1.0 или новее.'); return;
        }
    } catch (e) { ext.MsgBox('Новый API недоступен.'); return; }

    if (!document.all || !document.all.binobj) {
        ext.MsgBox('Контейнер вложений недоступен в этом представлении.'); return;
    }
    var nodes = document.all.binobj.getElementsByTagName('DIV');
    var output = 'ID\tMIME\tРазмер (Б)\tШирина x высота\r\n';
    var images = 0;
    for (var i = 0; i < nodes.length; i++) {
        var binary = nodes[i];
        var mime = String(binary.all.type.value);
        if (mime.toLowerCase().indexOf('image/') != 0) continue;
        var id = String(binary.all.id.value);
        var bytes = ext.GetBinarySize(binary.base64data);
        var dims = ext.GetImageDimsByData(binary.base64data);
        output += TsvCell(id) + '\t' + TsvCell(mime) + '\t' +
                  bytes + '\t' + TsvCell(dims || 'не определено') + '\r\n';
        images++;
    }
    if (!images) { ext.MsgBox('Встроенных изображений нет.'); return; }

    var chosen = ParseHostJson(ext.SelectFolder('{"title":"Куда сохранить список изображений"}'));
    if (chosen.status != 'ok' || !chosen.path) return;
    var path = ext.CombinePath(chosen.path,
                               'fbe-images-' + ext.GetUUID() + '.tsv');
    try {
        ext.WriteTextFileEx(path, output, false, 'utf-8-bom');
        ext.MsgBox('Книга: ' + (ext.GetDocumentFileName() || '(новая)') +
                   '\nИзображений: ' + images + '\nОтчёт: ' + path);
    } catch (e) {
        ext.MsgBox('Не удалось создать отчёт.\n' + ext.GetLastScriptingError());
    }
}
```

**Пример → результат:** для книги с `cover.jpg` (`image/jpeg`, 600×900, 15342 Б) и `illustration.png` программа создаст TSV с двумя строками данных. Кодировка **UTF-8 с BOM** удобна для открытия кириллицы табличными редакторами. Вложенные картинки и сам FB2 **не меняются**; при отмене выбора папки отчёт не сохраняется.

<a id="complex-title"></a>
### Сценарий Г. Изменение названия книги в XML через MSXML6 с валидацией и Undo

**Задача:** запросить новое название, отредактировать именно элемент `description/title-info/book-title`, проверить готовый XML штатным валидатором и применить одной операцией Undo. Используются `GetSourceText`, `InputBoxEx`, `ValidateSourceText`, `GetLastSourceDiagnostic`, `ApplySourceText`, `AskYesNo`, `GetCurrentViewMode` и COM MSXML6 (как во внутреннем JS FBE).

```javascript
function ParseHostJson(value) {
    if (typeof JSON != 'undefined' && JSON && typeof JSON.parse == 'function')
        return JSON.parse(value);
    // Только JSON, сформированный самим API FBE, не ввод пользователя и не XML.
    return eval('(' + value + ')');
}

function Run() {
    var ext = window.external;
    try {
        if (!ext.HasScriptingMethod('InputBoxEx')) {
            ext.MsgBox('Нужен API 1.0.0 или новее.'); return;
        }
    } catch (e) { ext.MsgBox('Новый API недоступен.'); return; }

    var xml = ext.GetSourceText();
    var dom;
    try { dom = new ActiveXObject('Msxml2.DOMDocument.6.0'); }
    catch (e) { ext.MsgBox('MSXML6 недоступен на этом ПК.'); return; }
    dom.async = false;
    dom.validateOnParse = false;
    dom.resolveExternals = false;
    if (!dom.loadXML(xml)) {
        ext.MsgBox('Исходный XML некорректен: ' + dom.parseError.reason);
        return;
    }
    var title = dom.selectSingleNode(
        "/*[local-name()='FictionBook']" +
        "/*[local-name()='description']" +
        "/*[local-name()='title-info']" +
        "/*[local-name()='book-title']");
    if (!title) {
        ext.MsgBox('Тег book-title в title-info не найден (это FB2?).'); return;
    }
    var oldTitle = String(title.text);
    var opts = '{"title":"Название книги","prompt":"Новое название","value":""}';
    var result = ParseHostJson(ext.InputBoxEx(opts));
    if (!result.accepted) return;
    var newTitle = String(result.value).replace(/^\s+|\s+$/g, '');
    if (!newTitle || newTitle == oldTitle) {
        ext.MsgBox('Новое название пустое или не отличается.'); return;
    }
    if (!ext.AskYesNo('Изменить название «' + oldTitle + '» на «' +
                      newTitle + '»?')) return;

    title.text = newTitle; // MSXML экранирует спецсимволы XML сам.
    var updated = dom.xml;
    if (!ext.ValidateSourceText(updated)) {
        ext.MsgBox('Не прошло проверку: ' + ext.GetLastSourceDiagnostic());
        return;
    }
    if (!ext.ApplySourceText(updated, 'Изменить название книги')) {
        ext.MsgBox('Применить XML не удалось: ' + ext.GetLastSourceDiagnostic());
        return;
    }
    ext.MsgBox('Название изменено. Доступен Ctrl+Z; книгу ещё нужно сохранить.');
}
```

**Пример → результат:** для книги с названием «Черновик» пользователь вводит «Готово» и подтверждает; текст `<book-title>` меняется, операция попадает в историю Undo, но **файл на диске сам не сохраняется**. MSXML6 при сериализации `dom.xml` может нормализовать пробелы/форматирование XML и способ представления вложений. Поэтому такой способ подходит, когда вы осознанно допускаете повторную сериализацию **всей книги**. Для точечного редактирования без переписывания остального XML требуется более сложное решение.

<a id="complex-selection"></a>
### Сценарий Д. Экспорт выделенной цитаты в новый TXT с безопасной записью

**Задача:** взять только выделенный текст из Design/Source/Description, запросить имя автора через `InputBoxEx`, предложить путь сохранения, записать UTF-8-файл без перезаписи существующих и показать диагностику ошибки. Комбинация: `GetSelectionText`, `GetSelectionInfo`, `InputBoxEx`, `SelectSaveFile`, `WriteTextFileEx`, `GetDocumentFileName`, `GetLastScriptingError`, `ClearLastScriptingError`.

```javascript
function ParseHostJson(value) {
    if (typeof JSON != 'undefined' && JSON && typeof JSON.parse == 'function')
        return JSON.parse(value);
    // Используйте здесь только JSON, непосредственно полученный от FBE.
    return eval('(' + value + ')');
}

function Run() {
    var ext = window.external;
    try {
        if (!ext.HasScriptingMethod('GetSelectionText') ||
            !ext.HasScriptingMethod('SelectSaveFile') ||
            !ext.HasScriptingMethod('WriteTextFileEx')) {
            ext.MsgBox('Нужен API 1.1.0 или новее.'); return;
        }
    } catch (e) { ext.MsgBox('Новый API недоступен.'); return; }

    var selected = ext.GetSelectionText();
    if (!selected) { ext.MsgBox('Сначала выделите текст.'); return; }
    var selectionInfo = ext.GetSelectionInfo();

    var author = ParseHostJson(ext.InputBoxEx(
        '{"title":"Цитата","prompt":"Укажите автора цитаты","value":""}'));
    if (!author.accepted) return;

    var target = ParseHostJson(ext.SelectSaveFile(
        '{"title":"Сохранить выделенную цитату","defaultExtension":"txt",' +
        '"filters":[{"name":"Текстовый файл","pattern":"*.txt"}]}'));
    if (target.status != 'ok' || !target.path) return;

    var text = 'Книга: ' + (ext.GetDocumentFileName() || '(новая)') + '\r\n' +
               'Автор: ' + author.value + '\r\n' +
               'Сведения о выделении: ' + selectionInfo + '\r\n' +
               '----\r\n' + selected + '\r\n';
    ext.ClearLastScriptingError();
    try {
        ext.WriteTextFileEx(target.path, text, false, 'utf-8');
        ext.MsgBox('Цитата сохранена:\n' + target.path);
    } catch (e) {
        ext.MsgBox('Файл не записан и не перезаписан.\n' +
                   ext.GetLastScriptingError());
    }
}
```

**Пример → результат:** если выделить «В начале было слово», указать автора «Автор книги» и выбрать **новое** имя `citation.txt`, будет создан файл с источником, автором, сведениями о выделении и самой цитатой. Если в диалоге выбран существующий файл, `WriteTextFileEx(..., false)` откажет с COM-ошибкой и **не заменит его**, даже если в системном диалоге ранее было подтверждение существующего имени. Исходная книга не меняется.

<a id="complex-backup"></a>
### Сценарий Е. Резервная копия перед изменением книги

**Задача и методы:** перед оформлением первого непустого обычного абзаца раздела как подзаголовка создать независимую копию: `SaveDocumentCopy`, `GetUUID`, `BeginDocumentUpdate`, `EndDocumentUpdate`, `ShowMessage`, `PushStatusBarText` / `PopStatusBarText`.

**Условия и ограничения:** API 1.5.0, режим Design, непустой обычный абзац раздела, именованный обычный FB2/FBD и доступный для записи каталог книги. Для новой книги и ZIP/RAR сначала выберите обычный файл через Save As. Копия получает текущие несохранённые изменения. Если копию создать нельзя, правка не начинается. `overwrite:false` защищает даже при конфликте уникального имени.

**Пример → результат**

```javascript
function FirstTextParagraph(root) {
    if (!root) return null;
    var paragraphs = root.getElementsByTagName('P');
    for (var i = 0; i < paragraphs.length; i++) {
        if (paragraphs[i].parentNode.className == 'section' &&
            /\S/.test(paragraphs[i].innerText)) return paragraphs[i];
    }
    return null;
}
function HostJson(value) {
    // Только JSON, непосредственно возвращённый FBE, не произвольные данные.
    return eval('(' + value + ')');
}
function Run() {
    var ext = window.external, update = null, status = null;
    try {
        try {
            ext.ClearLastScriptingError();
            if (ext.GetCurrentViewMode() != 'design') {
                ext.MsgBox('Переключитесь в Design.'); return;
            }
            var extension = ext.GetExtension(ext.GetDocumentFileName()).toLowerCase();
            var directory = ext.GetDocumentDirectory();
            if (!directory || (extension != '.fb2' && extension != '.fbd')) {
                ext.MsgBox('Нужна именованная обычная книга FB2/FBD.'); return;
            }
            var body = document.getElementById('fbw_body');
            var p = FirstTextParagraph(body);
            if (!p || p.className == 'subtitle') {
                ext.MsgBox('Нет подходящего абзаца для изменения.'); return;
            }
            var answer = HostJson(ext.ShowMessage(
                'Создать копию и сделать первый абзац подзаголовком?',
                'Резервная копия', 'yesnocancel', 'question'));
            if (answer.status == 'cancelled' || answer.button == 'no') return;
            if (answer.status != 'ok' || answer.button != 'yes')
                throw new Error('Подтверждение недоступно: ' + answer.code);
            status = ext.PushStatusBarText('Создание резервной копии...');
            var backup = ext.CombinePath(directory, 'fbe-backup-' + ext.GetUUID() + extension);
            ext.SaveDocumentCopy(backup, false); // До открытия Undo-группы.
            update = ext.BeginDocumentUpdate('Первый абзац — подзаголовок', '{}');
            ext.SetStyleEx(document, p, 'subtitle');
            ext.MsgBox('Копия: ' + backup + '\nПравка доступна через одно Undo.');
        } finally {
            try { if (update !== null) ext.EndDocumentUpdate(update); }
            finally { if (status !== null) ext.PopStatusBarText(status); }
        }
    } catch (e) {
        ext.MsgBox('Операция остановлена: ' + (e.description || e.message) +
                   '\n' + ext.GetLastScriptingError());
    }
}
```

**Исходные данные → ожидаемый результат:** книга `D:\Books\Черновик.fb2`, первый абзац «Вступление», обычный класс. После «Да» появляется `fbe-backup-<UUID>.fb2` с прежним оформлением, открытый абзац получает `subtitle`. **Меняет книгу и создаёт файл.** Один Undo отменяет оформление, но не удаляет резервную копию. Отмена до подтверждения не создаёт файл. При ошибке после записи копия остаётся; пакетный Undo не обещает автоматического отката частичной правки.

<a id="complex-source-export"></a>
### Сценарий Ж. Проверка и экспорт несохранённого Source в FB2/FBD

**Задача и методы:** проверить текущий редактируемый XML и выгрузить независимую копию: `ValidateCurrentDocument`, `SaveSerializedDocumentCopy`, `GetLastScriptingError`, `SelectSaveFile`, `PushStatusBarText` / `PopStatusBarText`.

**Условия и ограничения:** API 1.5.0, активен Source. Валидатор должен вернуть одновременно `status:"ok"` и `valid:true`. Выбранное расширение определяет формат; FB2→FBD сохраняет body, FBD без body→FB2 отклоняется. В примере явно выбирается UTF-8; `encoding:"current"` вместо него учитывает декларацию Source. Каталог назначения должен существовать. Подтверждение перезаписи в системном диалоге не разрешает замену в API.

**Пример → результат**

```javascript
function HostJson(value) { return eval('(' + value + ')'); } // Только ответ FBE.
function Run() {
    var ext = window.external, status = null;
    try {
        try {
            ext.ClearLastScriptingError();
            if (ext.GetCurrentViewMode() != 'source') {
                ext.MsgBox('Сначала откройте Source и внесите правки.'); return;
            }
            status = ext.PushStatusBarText('Проверка XML Source...');
            var validation = HostJson(ext.ValidateCurrentDocument('{}'));
            if (validation.status != 'ok' || validation.valid !== true) {
                ext.MsgBox('Экспорт отменён: ' +
                    (validation.errors.length ? validation.errors[0].message : 'XML не подтверждён'));
                return;
            }
            var target = HostJson(ext.SelectSaveFile(
                '{"title":"Экспорт Source без перезаписи","defaultExtension":"fb2",' +
                '"filters":[{"name":"FB2 / FBD","pattern":"*.fb2;*.fbd"}]}'));
            if (target.status == 'cancelled') return;
            if (target.status != 'ok' || !target.path) throw new Error('Не выбран путь экспорта');
            var extension = ext.GetExtension(target.path).toLowerCase();
            if (extension != '.fb2' && extension != '.fbd')
                throw new Error('Выберите расширение .fb2 или .fbd');
            var format = extension.substr(1);
            ext.SaveSerializedDocumentCopy(target.path,
                '{"format":"' + format + '","encoding":"utf-8","overwrite":false}');
            ext.MsgBox('Экспорт создан: ' + target.path + '\nSource не применён к Body.');
        } finally { if (status !== null) ext.PopStatusBarText(status); }
    } catch (e) {
        ext.MsgBox('Экспорт не выполнен: ' + (e.description || e.message) +
                   '\n' + ext.GetLastScriptingError());
    }
}
```

**Исходные данные → ожидаемый результат:** в несохранённом Source название изменено с «Черновик» на «Правка». Выбор нового `D:\Export\Правка.fbd` создаёт UTF-8 FBD с названием «Правка» и всеми body. Невалидный XML, несовместимый формат, исходный путь или существующий файл дают отказ. **Книгу не меняет, создаёт только новый файл; Undo не добавляется.** Source, курсор, выделение и dirty-state сохраняются; отмена диалога не создаёт файл.

<a id="complex-binary-report"></a>
### Сценарий З. Интерактивный отчёт о выбранном вложении

**Задача и методы:** выбрать вложение по ID, получить фактические сведения и записать новый отчёт: `SelectFromList`, `GetBinaryInfo`, `WriteTextFileEx`, `GetUUID`, `PushStatusBarText` / `PopStatusBarText`.

**Условия и ограничения:** API 1.4.0+, Design/Description, книга содержит вложения. Список берётся из текущего `binobj`, сведения — через бинарный API; содержимое не декодируется в скрипте. Более 2048 объектов требуют отдельного интерфейса поиска. ID/подписи экранируются как JSON. Значение `referenceCount:null` означает отсутствие достоверного подсчёта, а не ноль ссылок. Отчёт сохраняется в доступный временный каталог и намеренно остаётся там.

**Пример → результат**

```javascript
function HostJson(value) { return eval('(' + value + ')'); } // Только ответ FBE.
function JsonString(value) {
    return '"' + String(value).replace(/[\\"\x00-\x1f]/g, function (c) {
        var hex = c.charCodeAt(0).toString(16);
        return '\\u' + ('0000' + hex).slice(-4);
    }) + '"';
}
function Run() {
    var ext = window.external, status = null;
    try {
        try {
            ext.ClearLastScriptingError();
            if (ext.GetCurrentViewMode() == 'source') {
                ext.MsgBox('Переключитесь в Design или Description.'); return;
            }
            var container = document.getElementById('binobj');
            var nodes = container ? container.getElementsByTagName('DIV') : [];
            if (!nodes.length) { ext.MsgBox('Вложений нет.'); return; }
            if (nodes.length > 2048) throw new Error('Слишком много вложений для этого списка');
            var items = [];
            for (var i = 0; i < nodes.length; i++) {
                var id = String(nodes[i].all.id.value);
                items.push('{"id":' + JsonString(id) + ',"label":' + JsonString(id) + '}');
            }
            var choice = HostJson(ext.SelectFromList(
                '{"title":"Отчёт о вложении","items":[' + items.join(',') + ']}'));
            if (choice.status == 'cancelled') return;
            if (choice.status != 'ok') throw new Error('Выбор не выполнен: ' + choice.code);
            status = ext.PushStatusBarText('Получение сведений о вложении...');
            var info = HostJson(ext.GetBinaryInfo(choice.selectedId));
            var report = 'ID: ' + info.id + '\r\nMIME: ' + info.mimeType +
                '\r\nРазмер: ' + info.size + ' байт\r\nРазмеры: ' +
                (info.width === null ? 'не определены' : info.width + ' x ' + info.height) +
                '\r\nСсылки: ' + (info.referenceCount === null ? 'не подсчитаны' : info.referenceCount) + '\r\n';
            var path = ext.CombinePath(ext.GetTempDirectory(), 'fbe-binary-' + ext.GetUUID() + '.txt');
            ext.WriteTextFileEx(path, report, false, 'utf-8-bom');
            ext.MsgBox('Отчёт создан: ' + path);
        } finally { if (status !== null) ext.PopStatusBarText(status); }
    } catch (e) {
        ext.MsgBox('Отчёт не создан: ' + (e.description || e.message) +
                   '\n' + ext.GetLastScriptingError());
    }
}
```

**Исходные данные → ожидаемый результат:** выбран `cover.png`, `image/png`, 15342 байта, 600×800. Новый `%TEMP%\fbe-binary-<UUID>.txt` содержит эти поля; `referenceCount:null` записывается как «не подсчитаны». **Книгу не меняет, создаёт новый TXT с BOM, Undo не добавляется.** Отмена до выбора не создаёт отчёт; ошибку нельзя принимать за успешный пустой отчёт.

<a id="complex-safe-clipboard"></a>
### Сценарий И. Копирование выделения с безопасным HTML

**Задача и методы:** скопировать выделенный текст с переносами строк в два формата буфера: `GetSelectionText`, `SetClipboardHtml`, `ShowMessage`, `PushStatusBarText` / `PopStatusBarText`.

**Условия и ограничения:** API 1.4.0+, непустое выделение. Предыдущее содержимое **системного Clipboard заменяется только после подтверждения**. Пример намеренно переносит текст и переводы строк, а не исходную сложную HTML-разметку. Спецсимволы экранируются до передачи штатному санитайзеру. Действуют лимиты Clipboard API; при ошибке подготовки буфер сохраняется. Отказ OLE-публикации обрабатывается как ошибка.

**Пример → результат**

```javascript
function HostJson(value) { return eval('(' + value + ')'); } // Только ответ FBE.
function HtmlText(text) {
    return String(text).replace(/&/g, '&amp;').replace(/</g, '&lt;')
        .replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}
function Run() {
    var ext = window.external, status = null;
    try {
        try {
            var text = ext.GetSelectionText();
            if (!text) { ext.ShowMessage('Сначала выделите текст.', 'Копирование', 'ok', 'info'); return; }
            var answer = HostJson(ext.ShowMessage('Заменить буфер обмена выделенным текстом?',
                'Копирование', 'yesnocancel', 'question'));
            if (answer.status == 'cancelled' || answer.button == 'no') return;
            if (answer.status != 'ok' || answer.button != 'yes') throw new Error('Нет подтверждения');
            status = ext.PushStatusBarText('Подготовка безопасного HTML...');
            var html = '<p>' + HtmlText(text).replace(/\r\n|\r|\n/g, '<br>') + '</p>';
            ext.SetClipboardHtml(html, text);
            ext.ShowMessage('Текст и безопасный HTML скопированы.', 'Копирование', 'ok', 'info');
        } finally { if (status !== null) ext.PopStatusBarText(status); }
    } catch (e) {
        ext.ShowMessage('Копирование не выполнено: ' + (e.description || e.message),
            'Копирование', 'ok', 'error');
    }
}
```

**Исходные данные → ожидаемый результат:** выделено `A < B & C` и следующая строка «Цитата». После «Да» текстовый формат содержит исходное выделение, HTML — абзац с `A &lt; B &amp; C` и переносом строки. Текст `<script>` внутри выделения остаётся текстом и не исполняется. **Книгу не меняет, файлов не создаёт, Undo не добавляет; заменяет Clipboard.** «Нет»/«Отмена» оставляют буфер прежним. Локальные автоматические проверки используют изолированный `TestStorage`, а не реальный буфер пользователя.

<a id="complex-preferences"></a>
### Сценарий К. Настраиваемая обработка с сохранением предпочтений

**Задача и методы:** выбрать правила нормализации пробелов, запомнить выбор и применить его одним Undo: `GetScriptSetting`, `SetScriptSetting`, `SelectMultipleFromList`, `BeginDocumentUpdate`, `EndDocumentUpdate`, `GetNBSP` и статусный стек.

**Условия и ограничения:** API 1.4.0+, режим Design. Правила применяются к отдельным текстовым узлам `fbw_body`, сохраняя вложенную разметку; совпадение, разделённое тегом, не изменяется. Наличие U+00A0 проверяйте по `nodeValue` или XML: MSHTML может нормализовать `innerText`. Поддерживаются схлопывание повторных обычных пробелов и неразрывный пробел в «г. Москва». Идентичность файла скрипта определяет область настроек; сохраните пример в постоянный отдельный JS-файл. Настройки записываются до правки, их запись не входит в Undo и не является общей транзакцией с документом.

**Пример → результат**

```javascript
function HostJson(value) { return eval('(' + value + ')'); } // Только ответ FBE.
function Run() {
    var ext = window.external, update = null, status = null;
    try {
        try {
            ext.ClearLastScriptingError();
            if (ext.GetCurrentViewMode() != 'design') { ext.MsgBox('Нужен Design.'); return; }
            var selected = [];
            if (ext.GetScriptSetting('cleanup.spaces', false)) selected.push('spaces');
            if (ext.GetScriptSetting('cleanup.nbsp', true)) selected.push('nbsp');
            var answer = HostJson(ext.SelectMultipleFromList(
                '{"title":"Нормализация пробелов","items":[' +
                '{"id":"spaces","label":"Схлопнуть повторные обычные пробелы"},' +
                '{"id":"nbsp","label":"Неразрывный пробел после г."}],' +
                '"selectedIds":[' + (selected.length ? '"' + selected.join('","') + '"' : '') + ']}'));
            if (answer.status == 'cancelled') return;
            if (answer.status != 'ok') throw new Error('Выбор не выполнен: ' + answer.code);
            var spaces = false, nbsp = false;
            for (var i = 0; i < answer.selectedIds.length; i++) {
                if (answer.selectedIds[i] == 'spaces') spaces = true;
                if (answer.selectedIds[i] == 'nbsp') nbsp = true;
            }
            if (!ext.SetScriptSetting('cleanup.spaces', spaces) ||
                !ext.SetScriptSetting('cleanup.nbsp', nbsp))
                throw new Error('Не удалось сохранить предпочтения; книга не изменена');
            var body = document.getElementById('fbw_body');
            if (!body) return;
            var stack = [body], edits = [], nonbreaking = ext.GetNBSP();
            while (stack.length) {
                var node = stack.pop();
                if (node.nodeType == 3) {
                    var before = node.nodeValue, after = before;
                    if (spaces) after = after.replace(/ {2,}/g, ' ');
                    if (nbsp) after = after.replace(/г\. (?=[А-ЯЁ])/g, 'г.' + nonbreaking);
                    if (after != before) edits.push({node:node, value:after});
                } else {
                    for (var child = node.lastChild; child; child = child.previousSibling) stack.push(child);
                }
            }
            if (!edits.length) { ext.MsgBox('Настройки сохранены; изменений текста нет.'); return; }
            status = ext.PushStatusBarText('Нормализация пробелов...');
            update = ext.BeginDocumentUpdate('Настраиваемая нормализация пробелов', '{"suspendRedraw":true}');
            for (var j = 0; j < edits.length; j++) edits[j].node.nodeValue = edits[j].value;
            ext.MsgBox('Изменено текстовых узлов: ' + edits.length + '. Доступно одно Undo.');
        } finally {
            try { if (update !== null) ext.EndDocumentUpdate(update); }
            finally { if (status !== null) ext.PopStatusBarText(status); }
        }
    } catch (e) {
        ext.MsgBox('Обработка остановлена: ' + (e.description || e.message) +
                   '\n' + ext.GetLastScriptingError());
    }
}
```

**Исходные данные → ожидаемый результат:** абзац `г. Москва  — столица`, выбраны оба правила. Результат — `г. Москва — столица`: после `г.` символ из `GetNBSP()` (при стандартной настройке U+00A0), перед тире один обычный пробел. Следующий запуск предварительно отмечает оба правила. **Меняет книгу, записывает только настройки, отдельных файлов отчёта не создаёт.** Один Undo возвращает текст, но не предпочтения. Отмена выбора не меняет ни текст, ни настройки; подтверждённый пустой выбор сохраняет выключенные правила и не создаёт Undo. При ошибке посреди правки пакет закрывается, уже сделанное доступно для Undo; автоматический rollback не обещается.

### Краткая таблица проверки составных сценариев

| Сценарий | Меняет книгу | Создаёт файл | Пример ожидаемого результата |
|---|---|---|---|
| А. Аудит книги | Нет | Да, новый TXT | `fbe-audit-<UUID>.txt` со статусом валидации |
| Б. Неразрывные пробелы | **Да**, одно Undo | Нет | 3 замены, Ctrl+Z отменяет результат |
| В. Вложения | Нет | Да, новый TSV | ID, MIME, байты, разрешение изображений |
| Г. Название FB2 | **Да**, одно Undo | Нет | `<book-title>Готово</book-title>` |
| Д. Цитата | Нет | Да, только новый TXT | Текст выделения и метаданные |
| Е. Копия перед правкой | **Да**, одно Undo | Да, новый FB2/FBD | Копия прежнего состояния, затем `subtitle` |
| Ж. Экспорт Source | Нет | Да, новый FB2/FBD | Актуальный XML в UTF-8 без применения к Body |
| З. Выбранное вложение | Нет | Да, новый TXT | ID, MIME, байты и размеры |
| И. Безопасный Clipboard | Нет; заменяет буфер | Нет | Экранированный HTML и исходный текст |
| К. Предпочтения | **Да**, одно Undo | Только хранилище настроек | `г. Москва — столица`, выбор сохранён |

**Что проверять при адаптации:** наличие `HasScriptingMethod`, контекст запуска, режим редактора, отмену диалога, права на каталог, результат `false` и исключения COM. Методы группы Undo не обеспечивают автоматический транзакционный откат уже внесённых изменений; бинарный API используется в сценарии З, сохранение этапа 3Г — в Е и Ж. Отмена Undo не удаляет созданные файлы и не отменяет запись настроек или Clipboard.
