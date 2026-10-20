# API пользовательских скриптов FictionBook Editor Next

> **Актуальный публичный контракт: API 1.4.0, 99 методов** (`window.external`). Справочник составлен по исходному `IExternalHelper` (DISPID 1–35), таблице расширения DISPID 100–163 и документации `master` по состоянию на 20 октября 2026 года. Этапы 3Б и 3В по issue #45 включены; этап 3Г пока не реализован.

Скрипты выполняются внутри FBE Next под **Microsoft JScript/MSHTML**. `document` здесь — DOM активного представления, а **полный FB2/FBD XML** доступен отдельно через Source API. Расширение API не равно версии самого редактора: используйте `GetScriptingApiVersion()` / `HasScriptingMethod()`.

Этот документ организован по задачам. В оглавлении перечислены все публичные методы — ссылки ведут непосредственно к описаниям. **Не реализованные** функции вынесены в конец, чтобы их нельзя было перепутать с доступными.

## Быстрый старт

Проверка доступности нового метода при работе с разными версиями редактора:

```javascript
function HasApi(name) {
    try { return window.external.HasScriptingMethod(name); }
    catch (e) { return false; }
}

if (HasApi("GetSelectionText")) {
    window.external.MsgBox(window.external.GetSelectionText());
}
```

Безопасная группа Undo для старого DOM API:

```javascript
window.external.BeginUndoUnit(document, "Нормализация разметки");
try {
    // Изменяйте DOM документа.
} finally {
    window.external.EndUndoUnit(document);
}
```

При использовании новых методов проверяйте их наличие, а вызовы, способные выдавать COM-ошибки, оборачивайте в `try/catch`. В старом MSHTML **глобальный объект `JSON` может отсутствовать**: строки JSON, возвращаемые API, обрабатывайте совместимым JSON-парсером; не применяйте `eval` к XML книги или произвольным внешним данным.

## Оглавление: все 99 методов

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

## Общие правила и совместимость

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

> `GetProgramVersion()` сообщает версию приложения; `GetScriptingApiVersion()` — **версию API**. Проверка только номера приложения недостаточна.


> **Как запускать примеры:** каждый блок «Пример → результат» — самостоятельный JS-скрипт с `function Run()`; сохраните его в отдельный `.js` и запустите внутри FBE Next. Не вставляйте сразу 88 функций `Run()` в один файл. Примеры, изменяющие книгу или создающие файлы, снабжены предупреждениями. Указанные пути, размеры и значения — образцы: реальные зависят от книги и окружения.

## Справочник методов

## Версия API, контекст скрипта и окружение

<a id="api-getscriptingapiversion"></a>

### `GetScriptingApiVersion()`

Определяет версию расширенного интерфейса независимо от номера релиза редактора.

**Результат:** Строка, в актуальной реализации — `"1.4.0"`.

**Особенности:** В старых версиях метода может не быть: для совместимости проверяйте вызов через `try/catch`.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var v = ext.GetScriptingApiVersion();
    ext.MsgBox('API скриптов: ' + v);
}
```

**Ожидаемый результат:** Окно **«API скриптов: 1.4.0»** в актуальной реализации. Это не номер версии редактора.

<a id="api-hasscriptingmethod"></a>

### `HasScriptingMethod(name)`

Проверяет, доступен ли публичный метод по строковому имени; сам метод не выполняет.

**Результат:** `true` или `false`.

**Особенности:** Проверяет и исходный интерфейс, и расширение. Имя нечувствительно к регистру. При работе со старым FBE отсутствие самого `HasScriptingMethod` перехватывайте через `try/catch`.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var available = ext.HasScriptingMethod('GetSelectionText');
    var planned = ext.HasScriptingMethod('GetClipboardText');
    ext.MsgBox('Выделение: ' + available + '; буфер обмена: ' + planned);
}
```

**Ожидаемый результат:** **«Выделение: true; буфер обмена: true»** для API 1.4.0. Методы не вызываются, проверяется лишь их наличие.

<a id="api-getscriptingcapabilities"></a>

### `GetScriptingCapabilities()`

Получает сведения о поддерживаемых группах возможностей.

**Результат:** JSON-строка с `schemaVersion`, `apiVersion` и `capabilities`.

**Особенности:** В версии 1.4.0 доступны ключи: `scriptContext`, `environment`, `scriptSettings`, `fileDialogs`, `inputBoxEx`, `selection`, `documentUpdate`, `progress`, `textFiles`, `paths`, `documentValidation`, `logging`, `scriptingErrors`, `binaryReadExport`, `binaryObjects`, `extendedDialogs`, `clipboardText`, `clipboardHtml`, `statusStack`. Неизвестный ключ не считайте подтверждением возможности.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var caps = ext.GetScriptingCapabilities();
    ext.MsgBox(caps);
}
```

**Ожидаемый результат:** JSON с `"apiVersion":"1.4.0"` и `"capabilities"`, включая `"selection":true`, `"progress":true`, `"scriptingErrors":true` и четыре возможности этапа 3В. Содержимое может расширяться в новых версиях.

<a id="api-getcurrentscriptpath"></a>

### `GetCurrentScriptPath()`

Возвращает абсолютный путь выполняемого JS-файла.

**Результат:** Строка пути или `""`, если нет активного контекста.

**Особенности:** Вложенный запуск `apiRunCmd` временно переключает путь на вложенный скрипт.

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

Находит каталог, где лежит выполняемый скрипт.

**Результат:** Абсолютный путь **с завершающим `\`** или `""` без контекста.

**Особенности:** Подходит для хранения соседних ресурсов. Не путайте с рабочим каталогом FBE.

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

Получает имя выполняемого JS-файла с расширением, но без каталога.

**Результат:** Строка либо `""` вне контекста.

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

Получает UID скрипта из существующего реестра скриптов.

**Результат:** Строка UID; пустая строка для незарегистрированного скрипта или вне контекста.

**Особенности:** Не создаёт новый UID и не сканирует всю библиотеку.

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

Возвращает настроенный корневой каталог пользовательских скриптов.

**Результат:** Абсолютный путь.

**Особенности:** Завершающий разделитель может присутствовать; для склеивания путей используйте `CombinePath`.

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

Получает активную эффективную локаль интерфейса.

**Результат:** Строка, например `"ru-RU"` или `"en-US"`.

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

Определяет текущий режим отображения документа.

**Результат:** Одна из строк `"design"`, `"source"`, `"description"`.

**Особенности:** Позволяет не обращаться к MSHTML Body при активном Source.

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

Проверяет несохранённые изменения в текущем документе.

**Результат:** `true` или `false`.

**Особенности:** Учитывает изменения Body, Description и несохранённый текст Source.

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

Возвращает абсолютный нормализованный путь к файлу текущей книги.

**Результат:** Строка пути или `""` для ещё не сохранённой книги / невозможности нормализации.

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

Возвращает имя текущего файла вместе с расширением, без каталога.

**Результат:** Строка имени либо `""` до первого сохранения.

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

Получает каталог текущей книги.

**Результат:** Строка пути без завершающего `\`, кроме корня диска; `""` до сохранения.

**Особенности:** Используйте для создания отчётов рядом с книгой, предварительно убедившись, что книга сохранена.

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

Возвращает каталог установленной программы FBE.

**Результат:** Строка абсолютного пути, обычно без завершающего `\`.

**Особенности:** Это расположение приложения, а не каталог текущего скрипта.

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

Определяет версию запущенного редактора.

**Результат:** Строка вида `"3.0.9"` (значение зависит от релиза).

**Особенности:** Не используйте вместо `GetScriptingApiVersion` для проверки доступности новых методов.

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

Генерирует новый UUID.

**Результат:** Строка UUID в верхнем регистре без фигурных скобок.

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

Получает используемый в настройках редактора символ неразрывного пробела.

**Результат:** Строка из одного символа.

**Особенности:** Для нормализации пробелов лучше использовать этот метод, а не жёстко зашитый код символа.

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

Определяет, включён ли быстрый режим просмотра.

**Результат:** `true` или `false`.

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

Измеряет ширину текущей рабочей области редактора.

**Результат:** Целое число пикселей.

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

Измеряет высоту текущей рабочей области редактора.

**Результат:** Целое число пикселей.

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

Начинает именованную группу Undo для изменений DOM визуального редактора.

**Результат:** Результат не используется.

**Особенности:** `document` — DOM текущего представления; `action` — текст действия в истории отмены. Обязательно закрывайте группу в `finally`.

**Пример → результат**

**Условие:** Только Design/редактируемый DOM; пример **изменяет** открытый документ.

```javascript
function Run() {
    var ext = window.external;
    var root = document.getElementById('fbw_body');
    if (!root) { ext.MsgBox('Переключитесь в режим редактирования'); return; }
    var p = root.getElementsByTagName('P')[0];
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

Закрывает ранее начатую группу отмены.

**Результат:** Результат не используется.

**Особенности:** Передавайте тот же объект `document`, что и в `BeginUndoUnit`.

**Пример → результат**

**Условие:** Меняет DOM; запускайте в тестовой книге.

```javascript
function Run() {
    var ext = window.external;
    var root = document.getElementById('fbw_body');
    if (!root) return;
    var p = root.getElementsByTagName('P')[0];
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

Индексированное COM-свойство расширенного отображения блочного элемента.

**Результат:** При чтении — `true`/`false`; также поддерживается запись свойства.

**Особенности:** Используется для совместимости с классическим FBE. Синтаксис записи индексированного COM-свойства зависит от JScript/COM; для массового включения используйте `InflateParagraphs`.

**Пример → результат**

**Условие:** В редакторе должен быть хотя бы один абзац; для массовой работы предпочтителен `InflateParagraphs`.

```javascript
function Run() {
    var ext = window.external;
    var root = document.getElementById('fbw_body');
    if (!root) return;
    var p = root.getElementsByTagName('P')[0];
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

Включает `inflateBlock` для вложенных элементов `P` указанного контейнера.

**Результат:** Результат не используется.

**Пример → результат**

```javascript
function Run() {
    var ext = window.external;
    var root = document.getElementById('fbw_body');
    if (!root) return;
    var p = root.getElementsByTagName('P')[0];
    if (!p) return;
    ext.InflateParagraphs(root);
    ext.MsgBox('Первый P развёрнут: ' + ext.inflateBlock(p));
}
```

**Ожидаемый результат:** При успешной операции выводится **«Первый P развёрнут: true»**. Вызов применяется ко всем `P` переданного контейнера.

<a id="setstyleex"></a>

<a id="api-setstyleex"></a>

### `SetStyleEx(document, element, style)`

Устанавливает элементу CSS-класс (атрибут `class`).

**Результат:** Результат не используется.

**Особенности:** Первый аргумент сохранён для совместимости с историческим контрактом.

**Пример → результат**

**Условие:** Изменяет форматирование книги; сначала попробуйте в копии.

```javascript
function Run() {
    var ext = window.external;
    var root = document.getElementById('fbw_body');
    if (!root) return;
    var p = root.getElementsByTagName('P')[0];
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

Начинает пакетную правку, возвращая токен группы Undo.

**Результат:** Строка-токен.

**Особенности:** `optionsJson`: `"{}"` или `"{\"suspendRedraw\":true}"`. Последний вариант временно приостанавливает перерисовку. Поддерживаются вложенные группы с закрытием в обратном порядке. **Это не транзакционный откат изменений**.

**Пример → результат**

**Условие:** Для старого FBE нужен API 1.1+. Изменяет DOM.

```javascript
function Run() {
    var ext = window.external;
    var root = document.getElementById('fbw_body');
    if (!root) return;
    var p = root.getElementsByTagName('P')[0];
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

Завершает последнюю открытую пакетную операцию текущего скрипта.

**Результат:** `true` при успехе.

**Особенности:** При неправильном порядке закрытия, чужом или устаревшем токене возникает ошибка. Перерисовка восстанавливается и при аварийном завершении контекста.

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

Читает **полный актуальный XML** книги FB2/FBD, включая `description`, все `body` и `binary`.

**Результат:** Строка XML.

**Особенности:** В Source учитывает даже ещё не применённые изменения, не переключая режим редактора. Большие бинарные вложения увеличивают память и объём строки.

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

Проверяет полный XML FB2/FBD штатным механизмом редактора до изменения книги.

**Результат:** `true` для валидного XML, иначе `false`.

**Особенности:** После отказа детали доступны через `GetLastSourceDiagnostic()`.

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

Получает подробности последней Source XML-операции.

**Результат:** JSON-строка с полями `valid`, `message`, `line`, `column`.

**Особенности:** Это отдельная диагностика Source API, не путайте с `GetLastScriptingError()`. Глобальный `JSON` доступен не во всех режимах MSHTML.

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

Повторно проверяет и применяет XML целиком, создавая одну операцию Undo/Redo с названием `action`.

**Результат:** `true` при успешном применении, `false` при отказе.

**Особенности:** При невалидном XML документ остаётся неизменным. Не вызывайте внутри открытой группы `BeginDocumentUpdate`.

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

Выполняет штатную проверку текущего FB2/FBD **без передачи всего XML в JScript**.

**Результат:** JSON-строка с `schemaVersion`, `status`, `valid`, `diagnosticsComplete`, `errors`, `warnings`.

**Особенности:** Параметр обязателен и пока должен быть строкой `"{}"` (можно пробелы). В Source проверяется несохранённый текст; в Design/Description — detached DOM/stream. Не меняет Undo/Redo, выделение, режим и dirty-state. Для структурной проверки FBD действует его собственный контракт.

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

Сохраняет состояние текущего выделения либо каретки.

**Результат:** Непрозрачный строковый токен.

**Особенности:** До 128 закладок на контекст. Закладка живёт только во время текущего скрипта; не сохраняйте её в настройках.

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

Возвращает каретку/выделение к сохранённому состоянию, если оно ещё допустимо.

**Результат:** `true` при успешном восстановлении, иначе `false`.

**Особенности:** Не переключает режим. Смена документа, версии MSHTML, содержимого Source или значения соответствующих полей Description может инвалидировать закладку.

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

Явно освобождает сохранённую закладку.

**Результат:** `true` при освобождении, `false` для неизвестной или уже удалённой закладки.

**Особенности:** Оставшиеся закладки освобождаются автоматически в конце контекста скрипта.

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

Получает обычный текст выделения.

**Результат:** Строка, `""` при отсутствии текстового выделения.

**Особенности:** Работает с выделением Body, Source и полей Description. В Source читает только выделенный диапазон Scintilla, не полный XML.

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

Получает HTML-представление выделения.

**Результат:** HTML-строка.

**Особенности:** Для Source результат содержит HTML-экранированный выбранный исходный текст. Поддерживается также control-выделение.

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

Описывает тип и объём текущего выделения без передачи его содержимого.

**Результат:** JSON-строка с `schemaVersion`, `view`, `type`, `textLength`, `controlCount`.

**Особенности:** `view`: `design`, `description`, `source`; `type`: `text`, `control`, `none`; длина измеряется в единицах UTF-16, включая пары для supplementary-символов.

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

Показывает информационное окно с одной кнопкой «ОК».

**Результат:** Результат не используется.

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

Запрашивает у пользователя ответ «Да» или «Нет».

**Результат:** `true` только при выборе «Да».

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

Отображает старое окно ввода текста.

**Результат:** Введённая строка после подтверждения; при отмене — `""`.

**Особенности:** Чтобы отличить отмену от подтверждения пустой строки, сразу вызывайте `GetModalResult()`.

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

Возвращает код последнего диалога `InputBox`.

**Результат:** Числовой код: `6` (`IDYES`) при подтверждении, обычно `2` (`IDCANCEL`) при отмене.

**Особенности:** Это состояние старого InputBox; новый `InputBoxEx` на него не влияет.

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

Открывает системный диалог выбора существующего файла или нескольких файлов.

**Результат:** JSON-строка `{"status":"ok","path":"..."}` либо с `paths`, при отмене `{"status":"cancelled"}`.

**Особенности:** Опции: `title`, `initialDirectory`, `initialFileName`, `defaultExtension`, `multiSelect`, `filters`. Выбор не открывает книгу в редакторе.

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

Предлагает пользователю выбрать путь для сохранения файла.

**Результат:** JSON-строка с `status` и `path` (или `status:"cancelled"`).

**Особенности:** Сам файл **не записывает**. При существующем файле пользователь должен подтвердить замену. `multiSelect:true` не поддерживается.

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

Выбирает существующий каталог или несколько каталогов.

**Результат:** JSON-строка с `status` и `path`/`paths`.

**Особенности:** Поддерживает `multiSelect`. Поле `filters` для каталогов недопустимо.

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

Показывает современный диалог ввода с отдельным признаком подтверждения.

**Результат:** JSON-строка: `{"accepted":true,"value":"..."}` либо `{"accepted":false,"value":""}`.

**Особенности:** Строковые поля `title`, `prompt`, `value` необязательны. При ошибке возвращается `status:"error"`, `code`, `hresult`, `message`. Старый `GetModalResult()` не изменяется.

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

Открывает меню выбора жанра FB2 в указанной точке экрана.

**Результат:** Идентификатор жанра строкой либо `null` при отмене.

**Особенности:** Координаты `x`/`y` — экранные пиксели, например из `event.screenX/screenY`; первый аргумент оставлен для совместимости.

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

Читает состояние расширенного показа элемента описания по его идентификатору.

**Результат:** `true` или `false`.

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

Включает или выключает расширенный показ элемента описания, сохраняя настройку.

**Результат:** Результат не используется.

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

Показывает меню элементов Description у кнопки в экранной точке.

**Результат:** Строка ID элемента или пустое значение при отсутствии выбора.

**Особенности:** Для координат также используется `event.screenX/screenY`.

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

Устанавливает произвольный текст в строке состояния редактора.

**Результат:** Результат не используется.

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

Создаёт системное окно прогресса с возможностью отмены.

**Результат:** Непрозрачный токен.

**Особенности:** `maximum` — целое от 1 до 4294967295. На UI-потоке может быть одно активное окно; вызовы требуют активного контекста.

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

Обновляет количество обработанных элементов и подпись в окне прогресса.

**Результат:** `true` при успехе.

**Особенности:** `value` — целое в диапазоне от 0 до `maximum`; метод не приводит к произвольному выполнению очереди UI.

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

Проверяет, нажал ли пользователь кнопку отмены.

**Результат:** `true` или `false`.

**Особенности:** Отмена **кооперативная**: именно скрипт должен прекратить работу; уже сделанные изменения автоматически не откатываются.

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

Закрывает окно прогресса и освобождает связанные ресурсы.

**Результат:** `true` при успехе.

**Особенности:** Вызывайте в `finally`; при завершении контекста окно также закрывается автоматически.

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

Возвращает размер бинарного содержимого вложения в байтах.

**Результат:** Целое число.

**Особенности:** Передавайте `binary.base64data` — **двоичную строку**, а не текст кодировки base64.

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

Сохраняет двоичные данные в отдельный файл.

**Результат:** `true` после полной записи либо `false` при отмене/ошибке.

**Особенности:** При `prompt = true` открывается стандартный диалог Save As и подтверждение замены. При `prompt = false` файл по `path` создаётся без показа диалога. Если он уже существует, метод не заменяет его и возвращает `false`.

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

Читает ширину и высоту изображения по пути.

**Результат:** Строка вида `"640x480"`, либо `""` при ошибке.

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

Определяет размеры изображения по бинарному содержимому.

**Результат:** Строка вида `"640x480"`, либо `""`.

**Особенности:** Передавайте `base64data`, а не произвольный JS-текст.

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

Получает фактические сведения о вложении текущей книги без изменения документа.

**Результат:** JSON-строка с `schemaVersion:1`, `id`, `mimeType`, размером `size` в байтах, `width`, `height` и `referenceCount`.

**Особенности:** Размер берётся из исходных бинарных данных. Для неподдерживаемого изображения и не-изображения размеры равны `null`; `referenceCount:null` означает, что достоверный подсчёт ссылок пока не предоставляется. Отсутствующий ID вызывает COM-ошибку `0x80070490`. В Source метод явно отклоняется, чтобы не читать устаревший Body. Выделение, dirty-state и Undo/Redo не меняются.

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

Подбирает допустимый свободный XML ID для будущего вложения.

**Результат:** Строка с нормализованным именем и, при конфликте, суффиксом `_0`, `_1` и т. д.

**Особенности:** Сохраняет кириллицу и допустимый Unicode, заменяет недопустимые символы и некорректные суррогатные пары. Путь превращается в имя файла. Пустое имя нормализуется в `image`. Метод не создаёт и не резервирует объект; фактическое добавление ещё раз проверит конфликт. В Source возвращает `E_NOTIMPL`.

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

Читает файл и добавляет его исходные байты в текущую книгу.

**Результат:** Фактический ID добавленного объекта строкой.

**Особенности:** Пустой `preferredId` использует имя файла; конфликт получает свободный суффикс. MIME поддерживаемого изображения определяется по содержимому, для неоднозначного не-изображения его нужно указать явно. Перекодирование не выполняется. Успех создаёт одну операцию штатного Undo и обновляет вложения, размеры изображений и списки обложек. В Source и внутри открытой Undo-группы вызов отклоняется до изменений. Общие лимиты приведены в [ограничениях бинарного API](#binary-api-limits).

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

Заменяет данные существующего вложения, сохраняя его ID и ссылки в книге.

**Результат:** `true` после успешной замены; ошибки выдаются через COM.

**Особенности:** Проверяет файл, MIME и содержимое так же, как `AddBinaryFromFile`. Используемое изображение нельзя заменить не-изображением. Обновляет размеры, ImagesInfo, списки обложек и отображение изображений. Совпадающие байты и MIME не создают лишнего Undo. Отмена восстанавливает прежние байты и метаданные; Source и открытые Undo-группы не поддерживаются.

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

Удаляет вложение с явным контролем ссылок на него.

**Результат:** `true` при успешном удалении; отсутствие ID и небезопасные ссылки вызывают COM-ошибку.

**Особенности:** По умолчанию `removeReferences:false`: используемый объект не удаляется. С `true` допускается удалить вместе с ним поддерживаемые блочные/строчные изображения и очистить обложки. Неизвестные типы ссылок и обёртки с дополнительным содержимым приводят к отказу до изменений. Подробные правила — в [ограничениях бинарного API](#binary-api-limits). Объект, ссылки, обложки и выделение восстанавливаются одним Undo; Redo повторяет удаление. Source и открытые Undo-группы отклоняются.

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

Экспортирует все вложения, включая не-изображения, без изменения книги.

**Результат:** JSON-строка с `schemaVersion:1`, `status`, счётчиками `saved`, `skipped`, `errors` и массивом `objects`. Для каждого объекта возвращаются `id`, `fileName`, `status` и `hresult`.

**Особенности:** Каталог должен существовать. Политика `skip` по умолчанию пропускает существующие файлы; `fail` считает конфликт ошибкой; `replace` разрешает атомарную замену. Байты сохраняются без перекодирования. Имена защищены от выхода за каталог, недопустимых символов, DOS device names и совпадений без учёта регистра. Частичный экспорт имеет `status:"error"`; уже экспортированные файлы не удаляются из-за следующей ошибки. В Source возвращается `E_NOTIMPL`.

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

Читает небольшой текстовый файл с диска.

**Результат:** Строка содержимого.

**Особенности:** Без кодировки распознаются BOM UTF-8/UTF-16LE, иначе читается UTF-8. При неверной кодировке, пути или доступе возникает COM-ошибка.

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

Записывает текстовый файл с атомарной заменой уже существующего файла.

**Результат:** `true` при успехе.

**Особенности:** По умолчанию UTF-8 без BOM. Метод **может перезаписать файл**; безопаснее `WriteTextFileEx` с `overwrite:false`.

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

Записывает текст с явным разрешением или запретом перезаписи.

**Результат:** `true` при успехе; при конфликте существующего файла с `overwrite:false` — COM-ошибка.

**Особенности:** `overwrite:false` использует безопасное создание без замены. `true` — атомарная замена. Относительные пути считаются относительно рабочего каталога FBE.

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

Проверяет наличие доступного обычного файла (не каталога).

**Результат:** `true` или `false`; отсутствие доступа также может дать `false`.

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

Проверяет наличие доступного каталога.

**Результат:** `true` или `false`.

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

Создаёт указанный каталог и недостающие родительские каталоги.

**Результат:** `true` также для уже существующего каталога.

**Особенности:** Ограничения пути зависят от используемого Shell/Windows API, включая MAX_PATH в этой операции.

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

Соединяет каталог и имя в нормализованный абсолютный Windows-путь.

**Результат:** Строка пути.

**Особенности:** Если второй аргумент уже абсолютный, он заменяет первый.

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

Возвращает последний компонент пути, не обращаясь к диску.

**Результат:** Строка имени файла.

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

Возвращает каталог из полного пути.

**Результат:** Строка без завершающего разделителя, кроме корня, или `""` для имени без каталога.

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

Выделяет расширение последнего компонента пути.

**Результат:** Строка с точкой (`".fb2"`) либо `""`.

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

Возвращает системную временную папку.

**Результат:** Строка каталога с завершающим разделителем.

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

Создаёт уникальный пустой файл во временном каталоге.

**Результат:** Абсолютный путь уже созданного файла.

**Особенности:** По умолчанию `prefix="fbe-"`, `extension=".tmp"`; файл требуется удалить самому скрипту. Префикс и расширение не должны содержать недопустимые символы пути, каждое ограничено 64 UTF-16 единицами.

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

Читает персональную настройку выполняемого скрипта.

**Результат:** Сохранённое значение совместимого типа или `defaultValue`.

**Особенности:** Поддерживаются строка, boolean и конечное число. Значение не должно быть объектом/массивом или `null`. При повреждённом/недоступном файле может возвращаться defaultValue.

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

Сохраняет настройку текущего скрипта в отдельном хранилище.

**Результат:** `true` при успешной атомарной записи, `false` при ошибке доступа или лимите.

**Особенности:** Имя — непустая строка до 256 UTF-16 единиц; значения: строка до 32768 единиц, конечное число или boolean. Нужен контекст скрипта.

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

Удаляет один ключ настроек текущего скрипта.

**Результат:** Boolean успеха; отсутствие ключа не считается ошибкой.

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

Удаляет **все** настройки только текущего скрипта.

**Результат:** Boolean успеха.

**Особенности:** Не затрагивает настройки других скриптов и переменные исходных JS-файлов.

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

Проверяет, включена ли штатная диагностическая трассировка редактора.

**Результат:** `true` или `false`.

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

Отправляет диагностическое событие в существующую трассировку.

**Результат:** Результат не используется.

**Особенности:** Код до 32 символов; сообщение проходит политику приватности и ограничено 512 символами. Не предназначено для хранения исходного текста книги.

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

Получает локализованную строку из существующего каталога интерфейса.

**Результат:** Строка по ключу.

**Особенности:** Работает с уже известными редактору ключами; не добавляет переводы произвольному скрипту.

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

Отправляет событие в штатный диагностический журнал.

**Результат:** `true`, если событие принято; `false`, если журнал выключен или достигнут лимит.

**Особенности:** Уровни: `debug`, `info`, `warning`, `error` (с учётом регистра). Размер сообщения до 512 UTF-16 единиц. **Содержимое сообщения фильтруется политикой приватности**: журнал может содержать только токен контекста, факт сообщения и длину, а не сам текст.

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

Получает последнюю техническую ошибку новых методов API в текущем контексте.

**Результат:** JSON-строка `{"schemaVersion":1,"hasError":false}` либо описание `method`, `hresult`, `code`, `message`.

**Особенности:** Отслеживает ошибки HRESULT методов DISPID 100–163 и технический отказ `ValidateCurrentDocument`. Старые методы DISPID 1–35 и обычный возврат `false` автоматически сюда не записываются. Успешные вызовы не стирают ошибку.

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

Явно сбрасывает сохранённую ошибку новых методов текущего скрипта.

**Результат:** `true` при очистке.

**Особенности:** Вложенный скрипт имеет собственное состояние и не очищает ошибку родителя.

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

`GetLastScriptingError` отслеживает ошибки HRESULT **новых** методов (DISPID 100–163) в текущем контексте, но не обобщает возвраты `false` или ошибки старого интерфейса. Пример состояния:

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

Исходный `IExternalHelper` сохраняет IID, типы, порядок vtable и DISPID **1–35**; методы API 1.0.0/1.1.0/1.2.0/1.3.0/1.4.0 доступны через аддитивный `IDispatch` с DISPID **100–163**. Старый typelib не объявляет новые функции, поэтому для вызовов из скриптов следует обращаться через динамический `window.external`, а не ожидать их наличия в прежнем статически типизированном интерфейсе. Вызовы с неверным числом или типами параметров отвергаются через COM-ошибки (`DISP_E_BADPARAMCOUNT`, `DISP_E_TYPEMISMATCH`), вне контекста — где применимо — `E_UNEXPECTED`.

Сверяемые реализации: [`src/contracts/fbe.idl`](../src/contracts/fbe.idl), [`src/fbe/ExternalHelper.cpp`](../src/fbe/ExternalHelper.cpp), [`src/fbe/scripts/ScriptingApi.inl`](../src/fbe/scripts/ScriptingApi.inl), [`src/fbe/scripts/ScriptingStage2.inl`](../src/fbe/scripts/ScriptingStage2.inl) и [`src/fbe/scripts/ScriptingDiagnostics.inl`](../src/fbe/scripts/ScriptingDiagnostics.inl), [`src/fbe/scripts/ScriptingBinary.inl`](../src/fbe/scripts/ScriptingBinary.inl), [`src/fbe/scripts/ScriptBinaryDocument.h`](../src/fbe/scripts/ScriptBinaryDocument.h) и [`src/fbe/scripts/ScriptBinaryEdit.h`](../src/fbe/scripts/ScriptBinaryEdit.h).

## Диалоги, буфер обмена и статусный стек API 1.4.0

Реализация: [`ScriptExtendedDialogs.h`](../src/fbe/scripts/ScriptExtendedDialogs.h), [`ScriptClipboard.h`](../src/fbe/scripts/ScriptClipboard.h), [`ScriptStatus.h`](../src/fbe/scripts/ScriptStatus.h), [`ScriptingStage3C.inl`](../src/fbe/scripts/ScriptingStage3C.inl). Существующие пути вставки HTML и сохранения книги эти методы не заменяют.

Все методы этого раздела доступны начиная с **API 1.4.0** в Design, Description и Source. Они не вставляют данные в книгу и не создают операций Undo. Параметры — позиционные строки `BSTR`; неверные типы/число аргументов дают COM-ошибку. Строки должны содержать корректный UTF-16 без встроенных NUL.

Диалоги принадлежат главному окну, возвращают фокус после закрытия и блокируют повторный вход через COM до закрытия. Ошибки параметров возвращаются как JSON `{"status":"error","code":"invalid-options","hresult":"0x80070057","message":"..."}` без открытия пустого окна; отказ создания диалога — `dialog-failed`, недоступный владелец — `owner-unavailable`. Отмена является обычным результатом.

<a id="api-showmessage"></a>
### `ShowMessage(message, title, buttons, icon)`

**DISPID:** 153. Четыре строки → JSON-строка. Сообщение и заголовок — до 32767 UTF-16 единиц каждый.

`buttons`: `ok`, `okcancel`, `yesno`, `yesnocancel`. `icon`: `none`, `info`, `warning`, `error`, `question`. Используется штатная инфраструктура сообщений и темы FBE.

```javascript
var result = window.external.ShowMessage("Продолжить?", "Проверка", "yesnocancel", "question");
// «Нет»: {"status":"ok","button":"no"}
// Отмена: {"status":"cancelled","button":"cancel"}
```

Подтверждение возвращает `button: "ok"` или `"yes"`; «Нет» не считается отменой. Старые `MsgBox`, `AskYesNo` и `GetModalResult` сохраняют свой контракт.

<a id="api-selectfromlist"></a>
### `SelectFromList(optionsJson)`

**DISPID:** 154. Строка JSON → JSON-строка. Выбор одной строки стандартного Win32-списка с Enter/Esc, клавиатурной навигацией, изменением размера и прокруткой длинных подписей.

```javascript
var result = window.external.SelectFromList(
    '{"title":"Жанр","prompt":"Выберите жанр","items":[' +
    '{"id":"sf","label":"Фантастика"},{"id":"prose","label":"Проза"}],' +
    '"selectedId":"sf"}');
// Например: {"status":"ok","selectedId":"prose"}
```

`items` обязателен и не пуст. Каждый элемент содержит только `id` и `label`; `id` непустой, уникальный, до 256 UTF-16 единиц. Подпись, заголовок и приглашение — до 4096 единиц; JSON — до 1 МиБ единиц, список — до 2048 элементов. `title`, `prompt`, `selectedId` необязательны. Неизвестный предварительный ID, дубликаты ключей/ID, некорректный Unicode и неизвестные поля дают `invalid-options`. Без предварительного выбора выделяется первый элемент. Возвращается стабильный ID, а не текст или индекс. Отмена: `{"status":"cancelled"}`.

<a id="api-selectmultiplefromlist"></a>
### `SelectMultipleFromList(optionsJson)`

**DISPID:** 155. Строка JSON → JSON-строка. Пределы и структура элементов совпадают с `SelectFromList`; вместо `selectedId` используется необязательный массив `selectedIds`. Допустим пустой список.

```javascript
var result = window.external.SelectMultipleFromList(
    '{"title":"Жанры","items":[{"id":"sf","label":"Фантастика"},' +
    '{"id":"prose","label":"Проза"}],"selectedIds":["prose","sf"]}');
// {"status":"ok","selectedIds":["sf","prose"]}
```

Стандартный список поддерживает Ctrl/Shift и навигацию клавиатурой. ID возвращаются в порядке исходного списка. Подтверждение пустого выбора: `{"status":"ok","selectedIds":[]}`; отмена: `{"status":"cancelled"}`. Отсутствующие и повторяющиеся предварительные ID недопустимы.

<a id="api-choosecolor"></a>
### `ChooseColor(initialColor)`

**DISPID:** 156. Строка `#RRGGBB` → JSON-строка. Регистр входных шестнадцатеричных цифр произвольный; результат нормализован к верхнему регистру.

```javascript
var result = window.external.ChooseColor("#3366cc");
// Подтверждение: {"status":"ok","color":"#3366CC"}
// Отмена: {"status":"cancelled"}
```

Используется системный диалог, совместимый с Windows 7. Его оформление остаётся системным; этот метод сам не меняет цвет текста или оформление книги. Неверная строка цвета даёт `invalid-options`.

<a id="api-getclipboardtext"></a>
### `GetClipboardText()`

**DISPID:** 157. Без параметров → строка `BSTR` из `CF_UNICODETEXT`.

```javascript
var text = window.external.GetClipboardText();
// Например: "Первая строка\r\nВторая строка 😀"
```

Отсутствие текстового формата возвращает `""`. Занятый буфер, повреждённая/незавершённая строка, неверный Unicode и превышение размера дают COM-ошибку. Метод не меняет буфер. Отмены диалога нет.

<a id="api-setclipboardtext"></a>
### `SetClipboardText(text)`

**DISPID:** 158. Строка → `true` после успешной публикации `CF_UNICODETEXT`.

```javascript
window.external.SetClipboardText("Привет, мир!\r\n😀"); // true
window.external.SetClipboardText(""); // true, корректный пустой текст
```

Предел — 1 МиБ UTF-16 единиц; память включает завершающий NUL. Проверка и подготовка завершаются до замены буфера, поэтому неверный текст сохраняет прежнее содержимое. Ошибка доступа/публикации даёт COM-ошибку. Форматы предыдущего содержимого заменяются, вставка в книгу не производится.

<a id="api-getclipboardhtml"></a>
### `GetClipboardHtml()`

**DISPID:** 159. Без параметров → очищенный HTML-фрагмент `BSTR`.

```javascript
var html = window.external.GetClipboardHtml();
// Например: "<p><strong>Цитата 😀</strong></p>"
```

Используется общий `ReadClipboardFragment`/`HtmlPasteSanitizer`: проверяются байтовые диапазоны CF_HTML, UTF-8, размер до 2 МиБ и поддерживаемая разметка. Отсутствие HTML возвращает `""`; повреждение, занятость и неподдерживаемый небезопасный фрагмент дают COM-ошибку. `Legacy` не означает безопасность. Чтение не создаёт DOM, не выполняет скрипты и не загружает внешние ресурсы.

<a id="api-setclipboardhtml"></a>
### `SetClipboardHtml(html, plainText)`

**DISPID:** 160. Две строки → `true`. Публикует нормализованный `HTML Format` и явно переданный `CF_UNICODETEXT` одним OLE `IDataObject`.

```javascript
window.external.SetClipboardHtml("<p><b>Цитата 😀</b></p>", "Цитата 😀"); // true
```

Оба аргумента обязательны; текст ограничен 1 МиБ UTF-16 единиц, итоговый CF_HTML вместе с заголовком — 2 МиБ. Смещения вычисляются по UTF-8 байтам. Пустой HTML допустим. Активное содержимое удаляется штатным санитайзером, если возможно безопасное нормализованное представление; опасные URL, неподдерживаемые сложные/внутренние фрагменты и `Legacy` отвергаются. Ошибки проверки и подготовки не меняют прежний буфер. Отказ публикации даёт COM-ошибку. Маркер внутренней копии FBE не выставляется; обычный Ctrl+V и его Undo остаются штатными. Данные буфера не записываются в журнал или файлы диагностики.

<a id="api-pushstatusbartext"></a>
### `PushStatusBarText(text)`

**DISPID:** 161. Строка → непрозрачный строковый токен текущего контекста скрипта.

```javascript
var token = window.external.PushStatusBarText("Обработка книги...");
try {
    // Работа скрипта.
} finally {
    window.external.PopStatusBarText(token);
}
```

Сообщение — до 4096 UTF-16 единиц, в том числе пустое; глубина — до 64 сообщений одного контекста и до 256 во всех активных контекстах потока. Вне контекста — `E_UNEXPECTED`, превышение лимита — COM-ошибка. Соседние поля статусной строки не меняются. Приоритет: инкрементальный поиск → верхнее скриптовое сообщение → актуальное штатное временное сообщение → контекст документа. Очередь общих UI-сообщений вручную не прокачивается.

<a id="api-popstatusbartext"></a>
### `PopStatusBarText(token)`

**DISPID:** 162. Токен-строка → `true`. Снимает только верхнее сообщение текущего контекста (LIFO).

```javascript
var first = window.external.PushStatusBarText("Первое");
var second = window.external.PushStatusBarText("Второе");
window.external.PopStatusBarText(second); // true; снова «Первое»
window.external.PopStatusBarText(first);  // true; актуальное состояние редактора
```

Чужой, повторный, очищенный или не верхний токен даёт COM-ошибку без изменения стека. Завершение скрипта, включая исключение JScript/COM, автоматически снимает оставшиеся сообщения. Вложенный скрипт владеет собственным стеком и не может снять сообщения родителя. Закрытие/замена документа обнуляет относящиеся к окну сообщения.

<a id="api-clearstatusbartext"></a>
### `ClearStatusBarText()`

**DISPID:** 163. Без параметров → `true`. Очищает только сообщения текущего скриптового контекста.

```javascript
window.external.PushStatusBarText("Временное сообщение");
window.external.ClearStatusBarText(); // true
```

Сообщения родительского скрипта и системное состояние не удаляются; пустой стек допустим. Вне контекста — COM-ошибка. После очистки отображается актуальное состояние, а не сохранённая копия старой надписи. Прежний `SetStatusBarText` сохраняет своё действие; его временное сообщение становится видимым после снятия скриптового перекрытия.

## Запланировано, но не доступно для вызова

Перечисленное ниже **не входит** в 99 методов и пока не должно попадать в основной указатель. Актуальный статус ведётся в [issue #45](https://github.com/sklart/fictionbook-editor-next/issues/45), который остаётся открытым для этапа 3Г.

| Направление | Планируемые методы |
|---|---|
| Сохранение книги | `SaveCurrentDocument`, `SaveCurrentDocumentAs`, `SaveDocumentCopy`, `SaveSerializedDocumentCopy` |
| Отложено | `ProcessPendingUiEvents`, автоматический откат произвольных DOM-изменений |

Другие задачи (например, интерфейс параметров карточек скриптов, issue #48) не следует трактовать как методы API `window.external`.

## Тестовые контуры в репозитории

Для диагностики реализации используются адресные тесты `tools/tests/test-fbe-scripting-stage1.ps1`, `test-fbe-scripting-stage2.ps1`, `test-fbe-scripting-stage3a.ps1`, `test-fbe-scripting-stage3b.ps1`, `test-fbe-scripting-stage3c.ps1` и `test-fbe-typelib-runtime.ps1`. Группы этапов 2–3Б проверяют реальный JScript, Source/Body/Description, Undo, параметры вызова, диалоги и ошибки в пределах предусмотренных сценариев. Это не свидетельство полного тестирования всех пользовательских скриптов.

Проверку нужно проводить отдельно для portable и installed. Запуск на Windows 7 SP1 нельзя считать подтверждённым только по анализу импортов: для этого необходим отдельный реальный прогон на Windows 7.


## Сложные скрипты: сочетание методов API

Здесь приведены **полные скрипты**, а не фрагменты: каждый блок можно сохранить отдельным `.js` в каталоге Scripts FBE Next. Скрипты написаны для **Microsoft JScript**, без `let`, стрелочных функций и `async/await`. Запускайте их по одному. Примеры, меняющие книгу, сначала проверьте на тестовом FB2; сохранение книги на диск не производится автоматически.

> **Чтение JSON.** Несколько новых методов возвращают JSON **строкой**. Там, где результат нужно разобрать, используется `JSON.parse`, если он доступен. Резервный `eval` применяется **только к JSON, непосредственно возвращённому `window.external` в том же вызове**, а не к FB2/XML, внешним файлам или произвольному пользовательскому тексту. Для распространяемого скрипта предпочтительнее включить отдельный проверенный JSON-парсер, не требующий `eval`.

<a id="complex-report"></a>
### Сценарий A. Аудит текущей книги: проверка XML и отчёт без изменения документа

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

### Краткая таблица проверки составных сценариев

| Сценарий | Меняет книгу | Создаёт файл | Пример ожидаемого результата |
|---|---|---|---|
| А. Аудит книги | Нет | Да, новый TXT | `fbe-audit-<UUID>.txt` со статусом валидации |
| Б. Неразрывные пробелы | **Да**, одно Undo | Нет | 3 замены, Ctrl+Z отменяет результат |
| В. Вложения | Нет | Да, новый TSV | ID, MIME, байты, разрешение изображений |
| Г. Название FB2 | **Да**, одно Undo | Нет | `<book-title>Готово</book-title>` |
| Д. Цитата | Нет | Да, только новый TXT | Текст выделения и метаданные |

**Что проверять при адаптации:** наличие `HasScriptingMethod`, контекст запуска, режим редактора, отмену диалога, права на каталог, результат `false` и исключения COM. Методы группы Undo не обеспечивают автоматический транзакционный откат уже внесённых изменений; методы этапа 3Б в этих скриптах не используются, а этап 3Г пока не реализован.
