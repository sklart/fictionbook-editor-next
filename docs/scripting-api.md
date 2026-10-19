# API пользовательских скриптов FictionBook Editor Next

> **Актуальный публичный контракт: API 1.2.0, 82 метода** (`window.external`). Справочник составлен по исходному `IExternalHelper` (DISPID 1–35), таблице расширения DISPID 100–146 и документации `master` по состоянию на 19 октября 2026 года. Реализация этапов 3Б–3Г по issue #45 пока не включена.

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

## Оглавление: все 82 метода

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

### Описание книги и жанры

- [`GenrePopup`](#api-genrepopup)
- [`GetExtendedStyle`](#api-getextendedstyle)
- [`DescShowElement`](#api-descshowelement)
- [`DescShowMenu`](#api-descshowmenu)

### Строка состояния, прогресс и отмена

- [`SetStatusBarText`](#api-setstatusbartext)
- [`BeginProgress`](#api-beginprogress)
- [`SetProgress`](#api-setprogress)
- [`IsProgressCancelled`](#api-isprogresscancelled)
- [`EndProgress`](#api-endprogress)

### Двоичные вложения, текстовые файлы и пути

- [`GetBinarySize`](#api-getbinarysize)
- [`SaveBinary`](#api-savebinary)
- [`GetImageDimsByPath`](#api-getimagedimsbypath)
- [`GetImageDimsByData`](#api-getimagedimsbydata)
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

[Общие правила](#общие-правила-и-совместимость) · [Практические сценарии](#практические-сценарии) · [Ограничения](#ограничения-контракты-и-ошибки) · [Не реализовано](#запланировано-но-не-доступно-для-вызова)


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

> `GetProgramVersion()` сообщает версию приложения; `GetScriptingApiVersion()` — **версию API**. Проверка только номера приложения недостаточна.


## Справочник методов


## Версия API, контекст скрипта и окружение

<a id="api-getscriptingapiversion"></a>

### `GetScriptingApiVersion()`

Определяет версию расширенного интерфейса независимо от номера релиза редактора.

**Результат:** Строка, в актуальной реализации — `"1.2.0"`.

**Особенности:** В старых версиях метода может не быть: для совместимости проверяйте вызов через `try/catch`.

```javascript
var version = window.external.GetScriptingApiVersion();
```

<a id="api-hasscriptingmethod"></a>

### `HasScriptingMethod(name)`

Проверяет, доступен ли публичный метод по строковому имени; сам метод не выполняет.

**Результат:** `true` или `false`.

**Особенности:** Проверяет и исходный интерфейс, и расширение. Имя нечувствительно к регистру. При работе со старым FBE отсутствие самого `HasScriptingMethod` перехватывайте через `try/catch`.

```javascript
if (window.external.HasScriptingMethod("GetSelectionText")) {
    var text = window.external.GetSelectionText();
}
```

<a id="api-getscriptingcapabilities"></a>

### `GetScriptingCapabilities()`

Получает сведения о поддерживаемых группах возможностей.

**Результат:** JSON-строка с `schemaVersion`, `apiVersion` и `capabilities`.

**Особенности:** В версии 1.2.0 доступны ключи: `scriptContext`, `environment`, `scriptSettings`, `fileDialogs`, `inputBoxEx`, `selection`, `documentUpdate`, `progress`, `textFiles`, `paths`, `documentValidation`, `logging`, `scriptingErrors`. Неизвестный ключ не считайте подтверждением возможности.

```javascript
var capsJson = window.external.GetScriptingCapabilities();
```

<a id="api-getcurrentscriptpath"></a>

### `GetCurrentScriptPath()`

Возвращает абсолютный путь выполняемого JS-файла.

**Результат:** Строка пути или `""`, если нет активного контекста.

**Особенности:** Вложенный запуск `apiRunCmd` временно переключает путь на вложенный скрипт.

```javascript
var current = window.external.GetCurrentScriptPath();
```

<a id="api-getcurrentscriptdirectory"></a>

### `GetCurrentScriptDirectory()`

Находит каталог, где лежит выполняемый скрипт.

**Результат:** Абсолютный путь **с завершающим `\`** или `""` без контекста.

**Особенности:** Подходит для хранения соседних ресурсов. Не путайте с рабочим каталогом FBE.

```javascript
var dir = window.external.GetCurrentScriptDirectory();
```

<a id="api-getcurrentscriptname"></a>

### `GetCurrentScriptName()`

Получает имя выполняемого JS-файла с расширением, но без каталога.

**Результат:** Строка либо `""` вне контекста.

```javascript
var name = window.external.GetCurrentScriptName();
```

<a id="api-getcurrentscriptid"></a>

### `GetCurrentScriptId()`

Получает UID скрипта из существующего реестра скриптов.

**Результат:** Строка UID; пустая строка для незарегистрированного скрипта или вне контекста.

**Особенности:** Не создаёт новый UID и не сканирует всю библиотеку.

```javascript
var id = window.external.GetCurrentScriptId();
```

<a id="api-getscriptsrootdirectory"></a>

### `GetScriptsRootDirectory()`

Возвращает настроенный корневой каталог пользовательских скриптов.

**Результат:** Абсолютный путь.

**Особенности:** Завершающий разделитель может присутствовать; для склеивания путей используйте `CombinePath`.

```javascript
var root = window.external.GetScriptsRootDirectory();
```

<a id="api-getinterfacelanguage"></a>

### `GetInterfaceLanguage()`

Получает активную эффективную локаль интерфейса.

**Результат:** Строка, например `"ru-RU"` или `"en-US"`.

```javascript
var lang = window.external.GetInterfaceLanguage();
```

<a id="api-getcurrentviewmode"></a>

### `GetCurrentViewMode()`

Определяет текущий режим отображения документа.

**Результат:** Одна из строк `"design"`, `"source"`, `"description"`.

**Особенности:** Позволяет не обращаться к MSHTML Body при активном Source.

```javascript
if (window.external.GetCurrentViewMode() === "source") {
    // В этом режиме работайте с Source API.
}
```

<a id="api-isdocumentmodified"></a>

### `IsDocumentModified()`

Проверяет несохранённые изменения в текущем документе.

**Результат:** `true` или `false`.

**Особенности:** Учитывает изменения Body, Description и несохранённый текст Source.

```javascript
var dirty = window.external.IsDocumentModified();
```


## Файл документа и общие сведения

<a id="getdocumentfilepath"></a>

<a id="api-getdocumentfilepath"></a>

### `GetDocumentFilePath()`

Возвращает абсолютный нормализованный путь к файлу текущей книги.

**Результат:** Строка пути или `""` для ещё не сохранённой книги / невозможности нормализации.

```javascript
var path = window.external.GetDocumentFilePath();
```

<a id="getdocumentfilename"></a>

<a id="api-getdocumentfilename"></a>

### `GetDocumentFileName()`

Возвращает имя текущего файла вместе с расширением, без каталога.

**Результат:** Строка имени либо `""` до первого сохранения.

```javascript
var fileName = window.external.GetDocumentFileName();
```

<a id="getdocumentdirectory"></a>

<a id="api-getdocumentdirectory"></a>

### `GetDocumentDirectory()`

Получает каталог текущей книги.

**Результат:** Строка пути без завершающего `\`, кроме корня диска; `""` до сохранения.

**Особенности:** Используйте для создания отчётов рядом с книгой, предварительно убедившись, что книга сохранена.

```javascript
var bookDir = window.external.GetDocumentDirectory();
if (!bookDir) { window.external.MsgBox("Сначала сохраните книгу."); return; }
```

<a id="getstylepath"></a>

<a id="api-getstylepath"></a>

### `GetStylePath()`

Возвращает каталог установленной программы FBE.

**Результат:** Строка абсолютного пути, обычно без завершающего `\`.

**Особенности:** Это расположение приложения, а не каталог текущего скрипта.

```javascript
var installDir = window.external.GetStylePath();
```

<a id="getprogramversion"></a>

<a id="api-getprogramversion"></a>

### `GetProgramVersion()`

Определяет версию запущенного редактора.

**Результат:** Строка вида `"3.0.9"` (значение зависит от релиза).

**Особенности:** Не используйте вместо `GetScriptingApiVersion` для проверки доступности новых методов.

```javascript
var appVersion = window.external.GetProgramVersion();
```

<a id="getuuid"></a>

<a id="api-getuuid"></a>

### `GetUUID()`

Генерирует новый UUID.

**Результат:** Строка UUID в верхнем регистре без фигурных скобок.

```javascript
var id = window.external.GetUUID();
```

<a id="getnbsp"></a>

<a id="api-getnbsp"></a>

### `GetNBSP()`

Получает используемый в настройках редактора символ неразрывного пробела.

**Результат:** Строка из одного символа.

**Особенности:** Для нормализации пробелов лучше использовать этот метод, а не жёстко зашитый код символа.

```javascript
var nbsp = window.external.GetNBSP();
paragraph.innerText = "Иван" + nbsp + "Иванов";
```

<a id="isfastmode"></a>

<a id="api-isfastmode"></a>

### `IsFastMode()`

Определяет, включён ли быстрый режим просмотра.

**Результат:** `true` или `false`.

```javascript
var fast = window.external.IsFastMode();
```

<a id="getviewwidth"></a>

<a id="api-getviewwidth"></a>

### `GetViewWidth()`

Измеряет ширину текущей рабочей области редактора.

**Результат:** Целое число пикселей.

```javascript
var width = window.external.GetViewWidth();
```

<a id="getviewheight"></a>

<a id="api-getviewheight"></a>

### `GetViewHeight()`

Измеряет высоту текущей рабочей области редактора.

**Результат:** Целое число пикселей.

```javascript
var height = window.external.GetViewHeight();
```


## Редактирование DOM, Undo и исходный XML

<a id="beginundounit"></a>

<a id="api-beginundounit"></a>

### `BeginUndoUnit(document, action)`

Начинает именованную группу Undo для изменений DOM визуального редактора.

**Результат:** Результат не используется.

**Особенности:** `document` — DOM текущего представления; `action` — текст действия в истории отмены. Обязательно закрывайте группу в `finally`.

```javascript
window.external.BeginUndoUnit(document, "Правка заголовков");
try {
    // Изменения DOM.
} finally {
    window.external.EndUndoUnit(document);
}
```

<a id="endundounit"></a>

<a id="api-endundounit"></a>

### `EndUndoUnit(document)`

Закрывает ранее начатую группу отмены.

**Результат:** Результат не используется.

**Особенности:** Передавайте тот же объект `document`, что и в `BeginUndoUnit`.

```javascript
window.external.EndUndoUnit(document);
```

<a id="inflateblock"></a>

<a id="api-inflateblock"></a>

### `inflateBlock(element)`

Индексированное COM-свойство расширенного отображения блочного элемента.

**Результат:** При чтении — `true`/`false`; также поддерживается запись свойства.

**Особенности:** Используется для совместимости с классическим FBE. Синтаксис записи индексированного COM-свойства зависит от JScript/COM; для массового включения используйте `InflateParagraphs`.

```javascript
var inflated = window.external.inflateBlock(paragraph);
```

<a id="inflateparagraphs"></a>

<a id="api-inflateparagraphs"></a>

### `InflateParagraphs(element)`

Включает `inflateBlock` для вложенных элементов `P` указанного контейнера.

**Результат:** Результат не используется.

```javascript
window.external.InflateParagraphs(section);
```

<a id="setstyleex"></a>

<a id="api-setstyleex"></a>

### `SetStyleEx(document, element, style)`

Устанавливает элементу CSS-класс (атрибут `class`).

**Результат:** Результат не используется.

**Особенности:** Первый аргумент сохранён для совместимости с историческим контрактом.

```javascript
window.external.SetStyleEx(document, paragraph, "subtitle");
```

<a id="api-begindocumentupdate"></a>

### `BeginDocumentUpdate(action, optionsJson)`

Начинает пакетную правку, возвращая токен группы Undo.

**Результат:** Строка-токен.

**Особенности:** `optionsJson`: `"{}"` или `"{\"suspendRedraw\":true}"`. Последний вариант временно приостанавливает перерисовку. Поддерживаются вложенные группы с закрытием в обратном порядке. **Это не транзакционный откат изменений**.

```javascript
var token = window.external.BeginDocumentUpdate("Исправление текста", "{}");
try {
    // Изменения документа.
} finally {
    window.external.EndDocumentUpdate(token);
}
```

<a id="api-enddocumentupdate"></a>

### `EndDocumentUpdate(token)`

Завершает последнюю открытую пакетную операцию текущего скрипта.

**Результат:** `true` при успехе.

**Особенности:** При неправильном порядке закрытия, чужом или устаревшем токене возникает ошибка. Перерисовка восстанавливается и при аварийном завершении контекста.

```javascript
window.external.EndDocumentUpdate(token);
```

<a id="работа-с-исходным-xml-документа"></a>

<a id="api-getsourcetext"></a>

### `GetSourceText()`

Читает **полный актуальный XML** книги FB2/FBD, включая `description`, все `body` и `binary`.

**Результат:** Строка XML.

**Особенности:** В Source учитывает даже ещё не применённые изменения, не переключая режим редактора. Большие бинарные вложения увеличивают память и объём строки.

```javascript
var xml = window.external.GetSourceText();
```

<a id="api-validatesourcetext"></a>

### `ValidateSourceText(xml)`

Проверяет полный XML FB2/FBD штатным механизмом редактора до изменения книги.

**Результат:** `true` для валидного XML, иначе `false`.

**Особенности:** После отказа детали доступны через `GetLastSourceDiagnostic()`.

```javascript
if (!window.external.ValidateSourceText(changedXml)) {
    window.external.MsgBox(window.external.GetLastSourceDiagnostic());
    return;
}
```

<a id="api-getlastsourcediagnostic"></a>

### `GetLastSourceDiagnostic()`

Получает подробности последней Source XML-операции.

**Результат:** JSON-строка с полями `valid`, `message`, `line`, `column`.

**Особенности:** Это отдельная диагностика Source API, не путайте с `GetLastScriptingError()`. Глобальный `JSON` доступен не во всех режимах MSHTML.

```javascript
var detailsJson = window.external.GetLastSourceDiagnostic();
```

<a id="api-applysourcetext"></a>

### `ApplySourceText(xml, action)`

Повторно проверяет и применяет XML целиком, создавая одну операцию Undo/Redo с названием `action`.

**Результат:** `true` при успешном применении, `false` при отказе.

**Особенности:** При невалидном XML документ остаётся неизменным. Не вызывайте внутри открытой группы `BeginDocumentUpdate`.

```javascript
if (!window.external.ApplySourceText(changedXml, "Исправление XML")) {
    window.external.MsgBox(window.external.GetLastSourceDiagnostic());
}
```

<a id="api-validatecurrentdocument"></a>

### `ValidateCurrentDocument(optionsJson)`

Выполняет штатную проверку текущего FB2/FBD **без передачи всего XML в JScript**.

**Результат:** JSON-строка с `schemaVersion`, `status`, `valid`, `diagnosticsComplete`, `errors`, `warnings`.

**Особенности:** Параметр обязателен и пока должен быть строкой `"{}"` (можно пробелы). В Source проверяется несохранённый текст; в Design/Description — detached DOM/stream. Не меняет Undo/Redo, выделение, режим и dirty-state. Для структурной проверки FBD действует его собственный контракт.

```javascript
var resultJson = window.external.ValidateCurrentDocument("{}");
```


## Выделение и закладки

<a id="api-createselectionbookmark"></a>

### `CreateSelectionBookmark()`

Сохраняет состояние текущего выделения либо каретки.

**Результат:** Непрозрачный строковый токен.

**Особенности:** До 128 закладок на контекст. Закладка живёт только во время текущего скрипта; не сохраняйте её в настройках.

```javascript
var mark = window.external.CreateSelectionBookmark();
```

<a id="api-restoreselectionbookmark"></a>

### `RestoreSelectionBookmark(token)`

Возвращает каретку/выделение к сохранённому состоянию, если оно ещё допустимо.

**Результат:** `true` при успешном восстановлении, иначе `false`.

**Особенности:** Не переключает режим. Смена документа, версии MSHTML, содержимого Source или значения соответствующих полей Description может инвалидировать закладку.

```javascript
if (!window.external.RestoreSelectionBookmark(mark)) {
    window.external.MsgBox("Выделение уже изменилось.");
}
```

<a id="api-releaseselectionbookmark"></a>

### `ReleaseSelectionBookmark(token)`

Явно освобождает сохранённую закладку.

**Результат:** `true` при освобождении, `false` для неизвестной или уже удалённой закладки.

**Особенности:** Оставшиеся закладки освобождаются автоматически в конце контекста скрипта.

```javascript
window.external.ReleaseSelectionBookmark(mark);
```

<a id="api-getselectiontext"></a>

### `GetSelectionText()`

Получает обычный текст выделения.

**Результат:** Строка, `""` при отсутствии текстового выделения.

**Особенности:** Работает с выделением Body, Source и полей Description. В Source читает только выделенный диапазон Scintilla, не полный XML.

```javascript
var text = window.external.GetSelectionText();
```

<a id="api-getselectionhtml"></a>

### `GetSelectionHtml()`

Получает HTML-представление выделения.

**Результат:** HTML-строка.

**Особенности:** Для Source результат содержит HTML-экранированный выбранный исходный текст. Поддерживается также control-выделение.

```javascript
var html = window.external.GetSelectionHtml();
```

<a id="api-getselectioninfo"></a>

### `GetSelectionInfo()`

Описывает тип и объём текущего выделения без передачи его содержимого.

**Результат:** JSON-строка с `schemaVersion`, `view`, `type`, `textLength`, `controlCount`.

**Особенности:** `view`: `design`, `description`, `source`; `type`: `text`, `control`, `none`; длина измеряется в единицах UTF-16, включая пары для supplementary-символов.

```javascript
var infoJson = window.external.GetSelectionInfo();
```


## Диалоги и пользовательский ввод

<a id="msgbox"></a>

<a id="api-msgbox"></a>

### `MsgBox(message)`

Показывает информационное окно с одной кнопкой «ОК».

**Результат:** Результат не используется.

```javascript
window.external.MsgBox("Обработка завершена.");
```

<a id="askyesno"></a>

<a id="api-askyesno"></a>

### `AskYesNo(message)`

Запрашивает у пользователя ответ «Да» или «Нет».

**Результат:** `true` только при выборе «Да».

```javascript
if (!window.external.AskYesNo("Продолжить?")) return;
```

<a id="inputbox"></a>

<a id="api-inputbox"></a>

### `InputBox(prompt, title, value)`

Отображает старое окно ввода текста.

**Результат:** Введённая строка после подтверждения; при отмене — `""`.

**Особенности:** Чтобы отличить отмену от подтверждения пустой строки, сразу вызывайте `GetModalResult()`.

```javascript
var name = window.external.InputBox("Имя", "Название", "");
if (window.external.GetModalResult() !== 6) return;
```

<a id="getmodalresult"></a>

<a id="api-getmodalresult"></a>

### `GetModalResult()`

Возвращает код последнего диалога `InputBox`.

**Результат:** Числовой код: `6` (`IDYES`) при подтверждении, обычно `2` (`IDCANCEL`) при отмене.

**Особенности:** Это состояние старого InputBox; новый `InputBoxEx` на него не влияет.

```javascript
var accepted = window.external.GetModalResult() === 6;
```

<a id="api-selectopenfile"></a>

### `SelectOpenFile(optionsJson)`

Открывает системный диалог выбора существующего файла или нескольких файлов.

**Результат:** JSON-строка `{"status":"ok","path":"..."}` либо с `paths`, при отмене `{"status":"cancelled"}`.

**Особенности:** Опции: `title`, `initialDirectory`, `initialFileName`, `defaultExtension`, `multiSelect`, `filters`. Выбор не открывает книгу в редакторе.

```javascript
var selected = window.external.SelectOpenFile(
    '{"title":"Выберите книгу","filters":[{"name":"FB2","pattern":"*.fb2"}]}');
```

<a id="api-selectsavefile"></a>

### `SelectSaveFile(optionsJson)`

Предлагает пользователю выбрать путь для сохранения файла.

**Результат:** JSON-строка с `status` и `path` (или `status:"cancelled"`).

**Особенности:** Сам файл **не записывает**. При существующем файле пользователь должен подтвердить замену. `multiSelect:true` не поддерживается.

```javascript
var targetJson = window.external.SelectSaveFile(
    '{"title":"Сохранить отчёт","defaultExtension":"txt"}');
```

<a id="api-selectfolder"></a>

### `SelectFolder(optionsJson)`

Выбирает существующий каталог или несколько каталогов.

**Результат:** JSON-строка с `status` и `path`/`paths`.

**Особенности:** Поддерживает `multiSelect`. Поле `filters` для каталогов недопустимо.

```javascript
var folderJson = window.external.SelectFolder(
    '{"title":"Выберите папку"}');
```

<a id="api-inputboxex"></a>

### `InputBoxEx(optionsJson)`

Показывает современный диалог ввода с отдельным признаком подтверждения.

**Результат:** JSON-строка: `{"accepted":true,"value":"..."}` либо `{"accepted":false,"value":""}`.

**Особенности:** Строковые поля `title`, `prompt`, `value` необязательны. При ошибке возвращается `status:"error"`, `code`, `hresult`, `message`. Старый `GetModalResult()` не изменяется.

```javascript
var answerJson = window.external.InputBoxEx(
    '{"title":"Имя","prompt":"Введите значение","value":""}');
```


## Описание книги и жанры

<a id="genrepopup"></a>

<a id="api-genrepopup"></a>

### `GenrePopup(element, x, y)`

Открывает меню выбора жанра FB2 в указанной точке экрана.

**Результат:** Идентификатор жанра строкой либо `null` при отмене.

**Особенности:** Координаты `x`/`y` — экранные пиксели, например из `event.screenX/screenY`; первый аргумент оставлен для совместимости.

```javascript
var genre = window.external.GenrePopup(paragraph, event.screenX, event.screenY);
```

<a id="getextendedstyle"></a>

<a id="api-getextendedstyle"></a>

### `GetExtendedStyle(elementId)`

Читает состояние расширенного показа элемента описания по его идентификатору.

**Результат:** `true` или `false`.

```javascript
var shown = window.external.GetExtendedStyle("annotation");
```

<a id="descshowelement"></a>

<a id="api-descshowelement"></a>

### `DescShowElement(elementId, show)`

Включает или выключает расширенный показ элемента описания, сохраняя настройку.

**Результат:** Результат не используется.

```javascript
window.external.DescShowElement("annotation", true);
```

<a id="descshowmenu"></a>

<a id="api-descshowmenu"></a>

### `DescShowMenu(button, x, y)`

Показывает меню элементов Description у кнопки в экранной точке.

**Результат:** Строка ID элемента или пустое значение при отсутствии выбора.

**Особенности:** Для координат также используется `event.screenX/screenY`.

```javascript
var elementId = window.external.DescShowMenu(button, event.screenX, event.screenY);
```


## Строка состояния, прогресс и отмена

<a id="setstatusbartext"></a>

<a id="api-setstatusbartext"></a>

### `SetStatusBarText(text)`

Устанавливает произвольный текст в строке состояния редактора.

**Результат:** Результат не используется.

```javascript
window.external.SetStatusBarText("Обработано: " + count);
```

<a id="api-beginprogress"></a>

### `BeginProgress(title, maximum)`

Создаёт системное окно прогресса с возможностью отмены.

**Результат:** Непрозрачный токен.

**Особенности:** `maximum` — целое от 1 до 4294967295. На UI-потоке может быть одно активное окно; вызовы требуют активного контекста.

```javascript
var progress = window.external.BeginProgress("Обработка", 100);
```

<a id="api-setprogress"></a>

### `SetProgress(token, value, message)`

Обновляет количество обработанных элементов и подпись в окне прогресса.

**Результат:** `true` при успехе.

**Особенности:** `value` — целое в диапазоне от 0 до `maximum`; метод не приводит к произвольному выполнению очереди UI.

```javascript
window.external.SetProgress(progress, 25, "25 из 100");
```

<a id="api-isprogresscancelled"></a>

### `IsProgressCancelled(token)`

Проверяет, нажал ли пользователь кнопку отмены.

**Результат:** `true` или `false`.

**Особенности:** Отмена **кооперативная**: именно скрипт должен прекратить работу; уже сделанные изменения автоматически не откатываются.

```javascript
if (window.external.IsProgressCancelled(progress)) break;
```

<a id="api-endprogress"></a>

### `EndProgress(token)`

Закрывает окно прогресса и освобождает связанные ресурсы.

**Результат:** `true` при успехе.

**Особенности:** Вызывайте в `finally`; при завершении контекста окно также закрывается автоматически.

```javascript
window.external.EndProgress(progress);
```


## Двоичные вложения, текстовые файлы и пути

<a id="getbinarysize"></a>

<a id="api-getbinarysize"></a>

### `GetBinarySize(data)`

Возвращает размер бинарного содержимого вложения в байтах.

**Результат:** Целое число.

**Особенности:** Передавайте `binary.base64data` — **двоичную строку**, а не текст кодировки base64.

```javascript
var bytes = window.external.GetBinarySize(binary.base64data);
```

<a id="savebinary"></a>

<a id="api-savebinary"></a>

### `SaveBinary(path, data, prompt)`

Сохраняет двоичные данные в отдельный файл.

**Результат:** `true` после полной записи либо `false` при отмене/ошибке.

**Особенности:** При `prompt:true` открывается Save As и подтверждение замены. При `prompt:false` файл по `path` создаётся **только если его ещё нет**; существующий не заменяется.

```javascript
var ok = window.external.SaveBinary("cover.jpg", binary.base64data, false);
```

<a id="getimagedimsbypath"></a>

<a id="api-getimagedimsbypath"></a>

### `GetImageDimsByPath(path)`

Читает ширину и высоту изображения по пути.

**Результат:** Строка вида `"640x480"`, либо `""` при ошибке.

```javascript
var dims = window.external.GetImageDimsByPath("C:\\Pictures\\cover.jpg");
```

<a id="getimagedimsbydata"></a>

<a id="api-getimagedimsbydata"></a>

### `GetImageDimsByData(data)`

Определяет размеры изображения по бинарному содержимому.

**Результат:** Строка вида `"640x480"`, либо `""`.

**Особенности:** Передавайте `base64data`, а не произвольный JS-текст.

```javascript
var dims = window.external.GetImageDimsByData(binary.base64data);
```

<a id="api-readtextfile"></a>

### `ReadTextFile(path[, encoding])`

Читает небольшой текстовый файл с диска.

**Результат:** Строка содержимого.

**Особенности:** Без кодировки распознаются BOM UTF-8/UTF-16LE, иначе читается UTF-8. При неверной кодировке, пути или доступе возникает COM-ошибка.

```javascript
var data = window.external.ReadTextFile(path, "utf-8");
```

<a id="api-writetextfile"></a>

### `WriteTextFile(path, text[, encoding])`

Записывает текстовый файл с атомарной заменой уже существующего файла.

**Результат:** `true` при успехе.

**Особенности:** По умолчанию UTF-8 без BOM. Метод **может перезаписать файл**; безопаснее `WriteTextFileEx` с `overwrite:false`.

```javascript
window.external.WriteTextFile(path, "Отчёт", "utf-8");
```

<a id="api-writetextfileex"></a>

### `WriteTextFileEx(path, text, overwrite[, encoding])`

Записывает текст с явным разрешением или запретом перезаписи.

**Результат:** `true` при успехе; при конфликте существующего файла с `overwrite:false` — COM-ошибка.

**Особенности:** `overwrite:false` использует безопасное создание без замены. `true` — атомарная замена. Относительные пути считаются относительно рабочего каталога FBE.

```javascript
try {
    window.external.WriteTextFileEx(path, "Отчёт", false, "utf-8");
} catch (e) {
    window.external.MsgBox("Записать файл не удалось.");
}
```

<a id="api-fileexists"></a>

### `FileExists(path)`

Проверяет наличие доступного обычного файла (не каталога).

**Результат:** `true` или `false`; отсутствие доступа также может дать `false`.

```javascript
var exists = window.external.FileExists(path);
```

<a id="api-directoryexists"></a>

### `DirectoryExists(path)`

Проверяет наличие доступного каталога.

**Результат:** `true` или `false`.

```javascript
var exists = window.external.DirectoryExists(path);
```

<a id="api-createdirectory"></a>

### `CreateDirectory(path)`

Создаёт указанный каталог и недостающие родительские каталоги.

**Результат:** `true` также для уже существующего каталога.

**Особенности:** Ограничения пути зависят от используемого Shell/Windows API, включая MAX_PATH в этой операции.

```javascript
window.external.CreateDirectory(targetDir);
```

<a id="api-combinepath"></a>

### `CombinePath(directory, name)`

Соединяет каталог и имя в нормализованный абсолютный Windows-путь.

**Результат:** Строка пути.

**Особенности:** Если второй аргумент уже абсолютный, он заменяет первый.

```javascript
var path = window.external.CombinePath(
    window.external.GetCurrentScriptDirectory(), "report.txt");
```

<a id="api-getfilename"></a>

### `GetFileName(path)`

Возвращает последний компонент пути, не обращаясь к диску.

**Результат:** Строка имени файла.

```javascript
var name = window.external.GetFileName(path);
```

<a id="api-getdirectoryname"></a>

### `GetDirectoryName(path)`

Возвращает каталог из полного пути.

**Результат:** Строка без завершающего разделителя, кроме корня, или `""` для имени без каталога.

```javascript
var dir = window.external.GetDirectoryName(path);
```

<a id="api-getextension"></a>

### `GetExtension(path)`

Выделяет расширение последнего компонента пути.

**Результат:** Строка с точкой (`".fb2"`) либо `""`.

```javascript
var extension = window.external.GetExtension(path);
```

<a id="api-gettempdirectory"></a>

### `GetTempDirectory()`

Возвращает системную временную папку.

**Результат:** Строка каталога с завершающим разделителем.

```javascript
var dir = window.external.GetTempDirectory();
```

<a id="api-gettempfilename"></a>

### `GetTempFileName([prefix[, extension]])`

Создаёт уникальный пустой файл во временном каталоге.

**Результат:** Абсолютный путь уже созданного файла.

**Особенности:** По умолчанию `prefix="fbe-"`, `extension=".tmp"`; файл требуется удалить самому скрипту. Префикс и расширение не должны содержать недопустимые символы пути, каждое ограничено 64 UTF-16 единицами.

```javascript
var tmp = window.external.GetTempFileName("fbe-", ".txt");
```


## Настройки пользовательского скрипта

<a id="api-getscriptsetting"></a>

### `GetScriptSetting(name, defaultValue)`

Читает персональную настройку выполняемого скрипта.

**Результат:** Сохранённое значение совместимого типа или `defaultValue`.

**Особенности:** Поддерживаются строка, boolean и конечное число. Значение не должно быть объектом/массивом или `null`. При повреждённом/недоступном файле может возвращаться defaultValue.

```javascript
var enabled = window.external.GetScriptSetting("enabled", true);
```

<a id="api-setscriptsetting"></a>

### `SetScriptSetting(name, value)`

Сохраняет настройку текущего скрипта в отдельном хранилище.

**Результат:** `true` при успешной атомарной записи, `false` при ошибке доступа или лимите.

**Особенности:** Имя — непустая строка до 256 UTF-16 единиц; значения: строка до 32768 единиц, конечное число или boolean. Нужен контекст скрипта.

```javascript
var ok = window.external.SetScriptSetting("enabled", true);
```

<a id="api-removescriptsetting"></a>

### `RemoveScriptSetting(name)`

Удаляет один ключ настроек текущего скрипта.

**Результат:** Boolean успеха; отсутствие ключа не считается ошибкой.

```javascript
window.external.RemoveScriptSetting("enabled");
```

<a id="api-clearscriptsettings"></a>

### `ClearScriptSettings()`

Удаляет **все** настройки только текущего скрипта.

**Результат:** Boolean успеха.

**Особенности:** Не затрагивает настройки других скриптов и переменные исходных JS-файлов.

```javascript
window.external.ClearScriptSettings();
```


## Диагностика, журнал и локализация

<a id="api-isdiagnostictraceenabled"></a>

### `IsDiagnosticTraceEnabled()`

Проверяет, включена ли штатная диагностическая трассировка редактора.

**Результат:** `true` или `false`.

```javascript
var enabled = window.external.IsDiagnosticTraceEnabled();
```

<a id="api-tracescript"></a>

### `TraceScript(code, message)`

Отправляет диагностическое событие в существующую трассировку.

**Результат:** Результат не используется.

**Особенности:** Код до 32 символов; сообщение проходит политику приватности и ограничено 512 символами. Не предназначено для хранения исходного текста книги.

```javascript
window.external.TraceScript("MY-SCRIPT", "Этап завершён");
```

<a id="api-getlocalizedstring"></a>

### `GetLocalizedString(key)`

Получает локализованную строку из существующего каталога интерфейса.

**Результат:** Строка по ключу.

**Особенности:** Работает с уже известными редактору ключами; не добавляет переводы произвольному скрипту.

```javascript
var label = window.external.GetLocalizedString("example-key");
```

<a id="api-logmessage"></a>

### `LogMessage(level, message)`

Отправляет событие в штатный диагностический журнал.

**Результат:** `true`, если событие принято; `false`, если журнал выключен или достигнут лимит.

**Особенности:** Уровни: `debug`, `info`, `warning`, `error` (с учётом регистра). Размер сообщения до 512 UTF-16 единиц. **Содержимое сообщения фильтруется политикой приватности**: журнал может содержать только токен контекста, факт сообщения и длину, а не сам текст.

```javascript
window.external.LogMessage("info", "Обработка завершена");
```

<a id="api-getlastscriptingerror"></a>

### `GetLastScriptingError()`

Получает последнюю техническую ошибку новых методов API в текущем контексте.

**Результат:** JSON-строка `{"schemaVersion":1,"hasError":false}` либо описание `method`, `hresult`, `code`, `message`.

**Особенности:** Отслеживает ошибки HRESULT методов DISPID 100–146 и технический отказ `ValidateCurrentDocument`. Старые методы DISPID 1–35 и обычный возврат `false` автоматически сюда не записываются. Успешные вызовы не стирают ошибку.

```javascript
var errJson = window.external.GetLastScriptingError();
```

<a id="api-clearlastscriptingerror"></a>

### `ClearLastScriptingError()`

Явно сбрасывает сохранённую ошибку новых методов текущего скрипта.

**Результат:** `true` при очистке.

**Особенности:** Вложенный скрипт имеет собственное состояние и не очищает ошибку родителя.

```javascript
window.external.ClearLastScriptingError();
```


## Практические сценарии

### 1. Сохранить отчёт рядом со скриптом, не заменяя существующий файл

```javascript
if (!HasApi("WriteTextFileEx") || !HasApi("CombinePath")) {
    window.external.MsgBox("Для этой операции нужен API 1.1 или новее.");
    return;
}
var target = window.external.CombinePath(
    window.external.GetCurrentScriptDirectory(), "report.txt");
try {
    window.external.WriteTextFileEx(target, "Найдено ошибок: " + count, false, "utf-8");
    window.external.MsgBox("Отчёт сохранён: " + target);
} catch (e) {
    window.external.MsgBox("Файл уже существует или произошла ошибка записи.");
}
```

При `overwrite=false` существующий файл не будет заменён, даже если его создал другой процесс непосредственно перед завершением записи.

### 2. Исправить полный XML с валидацией и Undo

```javascript
var xml = window.external.GetSourceText();
var updated = xml.replace("старый текст", "новый текст");
if (updated !== xml) {
    if (!window.external.ValidateSourceText(updated)) {
        window.external.MsgBox(window.external.GetLastSourceDiagnostic());
    } else if (!window.external.ApplySourceText(updated, "Правка XML")) {
        window.external.MsgBox(window.external.GetLastSourceDiagnostic());
    }
}
```

Сценарий демонстрирует порядок вызовов, но простая строковая замена не учитывает семантику XML и может затронуть ненужные фрагменты. Для сложных изменений используйте XML-aware обработку и проверяйте результат.

### 3. Обрабатывать выделение, не меняя книгу

```javascript
if (HasApi("GetSelectionText")) {
    var selected = window.external.GetSelectionText();
    window.external.MsgBox(selected || "Текст не выделен.");
}
```

### 4. Прогресс с добровольной отменой

```javascript
if (HasApi("BeginProgress")) {
    var progress = window.external.BeginProgress("Проверка абзацев", items.length);
    try {
        for (var i = 0; i < items.length; ++i) {
            if (window.external.IsProgressCancelled(progress)) break;
            // Проверяем items[i].
            window.external.SetProgress(progress, i + 1, "Обработано: " + (i + 1));
        }
    } finally {
        window.external.EndProgress(progress);
    }
}
```

`items.length` в этом примере должен быть положительным целым числом; при пустом списке не создавайте окно прогресса. Нажатие «Отмена» не откатывает уже выполненные изменения.

### 5. Настройки без изменения текста исходного скрипта

```javascript
if (HasApi("GetScriptSetting")) {
    var enabled = window.external.GetScriptSetting("normalize", true);
    if (enabled) {
        // Выполнить нормализацию.
    }
    window.external.SetScriptSetting("normalize", enabled);
}
```

Хранилище настроек скрипта отдельно от настроек его карточки и от переменных в исходном `.js`.


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

`GetLastScriptingError` отслеживает ошибки HRESULT **новых** методов (DISPID 100–146) в текущем контексте, но не обобщает возвраты `false` или ошибки старого интерфейса. Пример состояния:

```json
{"schemaVersion":1,"hasError":true,"method":"WriteTextFileEx","hresult":"0x80070005","code":"access-denied","message":"Отказано в доступе"}
```

Успешный вызов не стирает сохранённую ошибку — очистку выполняет `ClearLastScriptingError()`. Текст `message` локализован средствами Windows; для логики лучше использовать `code`/`hresult`. Вложенные скрипты не разделяют состояние ошибки с родителем. По завершении скрипта сведения уничтожаются.

### Совместимость с исходным COM-контрактом

Исходный `IExternalHelper` сохраняет IID, типы, порядок vtable и DISPID **1–35**; методы API 1.0.0/1.1.0/1.2.0 доступны через аддитивный `IDispatch` с DISPID **100–146**. Старый typelib не объявляет новые функции, поэтому для вызовов из скриптов следует обращаться через динамический `window.external`, а не ожидать их наличия в прежнем статически типизированном интерфейсе. Вызовы с неверным числом или типами параметров отвергаются через COM-ошибки (`DISP_E_BADPARAMCOUNT`, `DISP_E_TYPEMISMATCH`), вне контекста — где применимо — `E_UNEXPECTED`.

Сверяемые реализации: [`src/contracts/fbe.idl`](../src/contracts/fbe.idl), [`src/fbe/ExternalHelper.cpp`](../src/fbe/ExternalHelper.cpp), [`src/fbe/scripts/ScriptingApi.inl`](../src/fbe/scripts/ScriptingApi.inl), [`src/fbe/scripts/ScriptingStage2.inl`](../src/fbe/scripts/ScriptingStage2.inl) и [`src/fbe/scripts/ScriptingDiagnostics.inl`](../src/fbe/scripts/ScriptingDiagnostics.inl).

## Запланировано, но не доступно для вызова

Перечисленное ниже **не входит** в 82 метода и пока не должно попадать в основной указатель. Актуальный статус ведётся в [issue #45](https://github.com/sklart/fictionbook-editor-next/issues/45).

| Направление | Планируемые методы |
|---|---|
| Вложения | `GetBinaryInfo`, `AddBinaryFromFile`, `ReplaceBinaryFromFile`, `RemoveBinary`, `SaveAllBinaries`, `GenerateUniqueBinaryId` |
| Расширенные диалоги | `ShowMessage`, `SelectFromList`, `SelectMultipleFromList`, `ChooseColor` |
| Буфер обмена | `GetClipboardText`, `SetClipboardText`, `GetClipboardHtml`, `SetClipboardHtml` |
| Стек строки состояния | `PushStatusBarText`, `PopStatusBarText`, `ClearStatusBarText` |
| Сохранение книги | `SaveCurrentDocument`, `SaveCurrentDocumentAs`, `SaveDocumentCopy`, `SaveSerializedDocumentCopy` |
| Отложено | `ProcessPendingUiEvents`, автоматический откат произвольных DOM-изменений |

Другие задачи (например, интерфейс параметров карточек скриптов, issue #48) не следует трактовать как методы API `window.external`.

## Тестовые контуры в репозитории

Для диагностики реализации используются адресные тесты `tools/tests/test-fbe-scripting-stage1.ps1`, `test-fbe-scripting-stage2.ps1`, `test-fbe-scripting-stage3a.ps1` и `test-fbe-typelib-runtime.ps1`. Вторая и третья группы проверяют реальный JScript, Source/Body/Description, Undo, параметры вызова, диалоги и ошибки в пределах предусмотренных сценариев. Это не свидетельство полного тестирования всех пользовательских скриптов.

Проверку нужно проводить отдельно для portable и installed. Запуск на Windows 7 SP1 нельзя считать подтверждённым только по анализу импортов: для этого необходим отдельный реальный прогон на Windows 7.
