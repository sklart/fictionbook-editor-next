# Локализация FictionBook Editor Next

Этот документ фиксирует текущую схему локализации и целевое направление, чтобы
позже можно было без болезненной переделки подключить Weblate или другой
веб-инструмент перевода.

## Текущее состояние

- Основной интерфейс FBE локализуется через ресурсные DLL:
  - `src/locales/res_rus`;
  - `src/locales/res_ukr`.
- Команда проверки FB2 в контекстном меню Windows локализуется через MUI-модуль
  `FBVVerbResources.dll` и спутниковые `.mui`-файлы. Целевое расположение этих
  файлов — `Lang\Shell`, потому что Windows Shell использует ресурсные строки
  вида `@module,-id`, а не JSON.
- Новые плагины пока содержат собственные Win32 resource-файлы:
  - `src/export-docx/ExportDOCX.rc`;
  - `src/export-epub/ExportEPUB.rc`;
  - `src/import-epub/ImportEPUB.rc`;
  - `src/export-html/ExportHTML.rc`.

## Целевые языки

Минимальный поддерживаемый набор для Next:

- English (`en-US`);
- Русский (`ru-RU`);
- Українська (`uk-UA`);
- Deutsch (`de-DE`);
- Français (`fr-FR`);
- Español (`es-ES`);
- Italiano (`it-IT`);
- Polski (`pl-PL`);
- Português (`pt-PT`);
- Nederlands (`nl-NL`);
- Čeština (`cs-CZ`);
- Български (`bg-BG`).

## Подход для будущего Weblate

Weblate лучше работает с текстовыми каталогами переводов, где каждая строка
имеет стабильный ключ. В качестве основного формата внешней runtime-локализации
выбран JSON. Поэтому дальнейшие изменения стоит делать так:

1. Не добавлять новые пользовательские строки только как безымянный текст внутри
   C++-кода или `.rc`.
2. Для новых строк сразу заводить стабильный идентификатор.
3. Для FBE, FBV и плагинов использовать единый JSON-каталог переводов, из
   которого при необходимости можно генерировать Win32 `.rc`/`.rc2` include-
   файлы.
4. В Weblate отдавать именно текстовые каталоги, а не полные `.rc` с координатами
   элементов управления.
5. Координаты и размеры диалогов оставлять в проекте, а переводимые подписи
   хранить отдельно.

Такой подход позволит переводчикам менять текст без риска случайно сломать
разметку диалога.

## Целевая runtime-схема JSON

Внешние переводы планируются в каталоге `Lang` рядом с программой:

```text
Lang\
  en-US\
    fbe.json
    fbv.json
    export-html.json
    export-docx.json
    export-epub.json
    import-epub.json
  ru-RU\
    fbe.json
    fbv.json
    export-html.json
    export-docx.json
    export-epub.json
    import-epub.json
  Shell\
    FBVVerbResources.dll
    ru-RU\FBVVerbResources.dll.mui
```

Отсутствие внешних JSON-файлов не должно мешать запуску. Порядок fallback:

1. `Lang/<текущий-язык>/<модуль>.json`;
2. `Lang/en-US/<модуль>.json`;
3. встроенная строка в `exe`/`dll`;
4. диагностический ключ строки, если даже встроенного fallback нет.

Текущий язык runtime JSON-слоя берётся из существующей настройки языка интерфейса
FBE. В настройке доступны `Определяется системой` и все 12 целевых языков. При
запуске редактор публикует выбранную локаль двумя способами: в переменную
окружения процесса `FBE_UI_LOCALE` и в файл
`%LOCALAPPDATA%\FBE\interface-locale.txt`. FBV и плагины сначала читают этот
общий контракт, затем откатываются к языку Windows и только после этого к
`en-US`. Такой файл нужен для отдельных процессов вроде FBV, которые могут
запускаться не из FBE.

Важное ограничение текущего этапа: для русского и украинского сохраняется
историческая модель resource DLL, поэтому старые меню и диалоги получают полный
Win32-ресурс. Для остальных целевых языков сейчас гарантирован runtime
JSON-overlay для уже перенесённых строк; полное покрытие старых Win32 меню и
диалогов через отдельные resource DLL/языковые пакеты остаётся следующим этапом.

Проверяемый контракт этой схемы лежит в `localization/runtime/contract.json`,
описание — в `localization/runtime/README.md`, smoke-тест —
`tools/tests/test-localization-runtime-contract.ps1`.


## Локализация FBE и FBV

Для основного интерфейса добавлен подготовительный каталог
`localization/app-ui/catalog.json`. Он пока не заменяет текущие Win32 resource-
файлы, а фиксирует карту перехода:

- FBE продолжает использовать `src/locales/res_rus` и `src/locales/res_ukr`;
- FBV продолжает использовать `src/fbv/FBV.rc`;
- shell-команда проверки FB2 остаётся на MUI-модели;
- каталог `localization/app-ui` задаёт целевые языки, существующие источники и
  первые стабильные ключи для будущей генерации `.rc`/`.mui`.

Новый smoke-тест `tools/tests/test-app-localization-catalog.ps1` проверяет, что
карта локализации FBE/FBV не рассинхронизировалась с ресурсами проекта и что
стартовые строки заполнены по всем целевым языкам.
Для FBE этот тест также инвентаризирует критичный минимальный набор строк
обновления, предупреждения read-only и завершения поиска. Этот первый FBE-срез
уже заведён в `localization/app-ui/catalog.json` для всех целевых языков и
подключён к русской/украинской runtime-локализации через generated `.rc2`.
Следующий FBE-срез переносит в тот же контур часто видимые сообщения сохранения,
валидации, импорта/экспорта, внешнего изменения файла, нехватки памяти и
отсутствующих скриптов.
Третий FBE-срез добавляет script/XML/COM diagnostics, replace/search messages и
сообщения замены слов.
Четвёртый безопасный FBE-срез закрывает строки добавления изображения и binary-
ресурсов. Срезы status bar/context menu/document-tree требуют отдельной
аккуратной обработки из-за исторических пересечений числовых ID в Win32
`STRINGTABLE`-блоках по 16 строк.

Первый реальный подключённый потребитель JSON-каталога — FBV. Скрипт
`tools/localization/update-fbv-resource-strings.ps1` генерирует
`src/fbv/FBVStrings.generated.rc2` из `localization/app-ui/catalog.json`, а
`src/fbv/FBV.rc` подключает этот файл как встроенные Win32 `STRINGTABLE`.
FBV также подключён к внешнему runtime JSON-слою: если рядом с программой есть
`Lang/<локаль>/fbv.json`, описанные binding-строки перекрывают встроенные ресурсы;
если `Lang` отсутствует или повреждён, встроенные ресурсы продолжают работать как fallback.

FBE также подключён к внешнему runtime JSON-слою. Файл
`src/fbe/RuntimeLocalization.cpp` загружает `Lang/en-US/fbe.json`, затем
`Lang/<выбранный язык интерфейса>/fbe.json`, а `FbeLoadString` и `U::MessageBox`
сначала пробуют взять строку из JSON и только потом обращаются к встроенным
Win32-ресурсам. Поэтому отсутствие каталога `Lang` или отдельного ключа не
мешает запуску редактора. Настройка языка FBE теперь содержит `Определяется
системой`, English, русский, українську и остальные целевые европейские языки;
выбранная локаль используется тем же контрактом, что и для FBV/плагинов. На
текущем этапе внешний слой охватывает уже заведённые в
`localization/app-ui/catalog.json` строки сообщений, обновлений, статусов,
настроек, hotkey-названий и diagnostics; диалоговые layout-ресурсы по-прежнему
полностью собираются для ru/uk через generated `.rc2`. После применения настроек
`CMainFrame::RefreshLocalizedMainFrameUi()` уже перезагружает главное меню,
динамические пункты Import/Export/Script/MRU, hotkey-подписи и статические
подписи toolbar-полей. Окна поиска/замены и проверки орфографии закрываются
перед применением настроек и при следующем открытии создаются уже на новом
языке; уже созданные дочерние окна и plugin UI остаются отдельным этапом
live-переключения.
Для main toolbar добавлен собственный обработчик `OnRuntimeToolTipTextA/W`: он берёт строки через `FbeLoadString`, использует tooltip-часть после `\n` и намеренно не выставляет `TTF_DI_SETITEM`. Это не даёт стандартному WTL-кэшу закрепить старый язык после переключения интерфейса.

Для старых участков FBE, где исторически использовался `CString::LoadString`,
добавлен helper `FbeLoadCString()`. Он сначала обращается к runtime JSON-overlay,
а затем откатывается к встроенным ресурсам. Контракт
`test-runtime-interface-language-contract.ps1` проверяет, что в `src/fbe`
не остаётся прямых `.LoadString(...)`, иначе новая строка могла бы обойти
`Lang/<локаль>/fbe.json`.

Первый подключённый плагин — ExportHTML. Скрипт
`tools/localization/update-export-html-resource-strings.ps1` генерирует
`src/export-html/ExportHTMLStrings.generated.rc2` из
`localization/plugin-ui/catalog.json`, а `ExportHTML.rc` подключает generated-
файл вместо ручной runtime `STRINGTABLE`. Пока перенесены строки, которые уже
используются кодом через `IDS_*`/`IDR_EXPORTHTML`: имя команды экспорта, фильтр
сохранения, ошибки записи/чтения, XML/COM-сообщения и всплывающие подсказки
маленького custom save dialog. Подписи самого диалога остаются отдельным
следующим шагом, потому что в текущем layout они частично заданы как
`IDC_STATIC`.

Всплывающие подсказки плагинов считаются такими же пользовательскими строками,
как подписи кнопок и сообщения об ошибках. Новые tooltip-строки нужно заводить
в `localization/plugin-ui/catalog.json` с компонентом вида
`<plugin>.tooltip`, назначать им стабильный `IDS_*` и загружать в коде через
`LoadString`. Прямые русские/английские tooltip-строки в C++ допустимы только
как временный технический долг до переноса в JSON→generated `.rc2`.

Для новых плагинов уже подключены отдельные генераторы встроенных строк:

- `tools/localization/update-export-docx-resource-strings.ps1` →
  `src/export-docx/ExportDOCXStrings.generated.rc2`;
- `tools/localization/update-export-epub-resource-strings.ps1` →
  `src/export-epub/ExportEPUBStrings.generated.rc2`;
- `tools/localization/update-import-epub-resource-strings.ps1` →
  `src/import-epub/ImportEPUBStrings.generated.rc2`.

В ExportEPUB сейчас через JSON→generated `.rc2` идут tooltip-строки окна
настроек, подписи окна настроек, кнопка `Настройки экспорта...`, preflight-
предупреждения и итоговое окно успешного экспорта. В ImportEPUB через тот же
механизм идут подписи окна настроек, tooltip-строки, строки системного диалога
выбора EPUB, стадии импорта и основные MessageBox-сообщения COM-плагина.
Текст, который становится содержимым создаваемого EPUB/FB2 или подробного
диагностического лога, считается отдельным слоем и не смешивается с UI-
локализацией без отдельного решения.

## Общий runtime JSON-loader

Чтобы не держать копии JSON-парсера и логики выбора языка в каждом модуле, начато выделение общего helper-слоя `src/common/RuntimeLocalizationCommon.h`. В нём сосредоточены:

- проверка поддерживаемых локалей;
- чтение `FBE_UI_LOCALE` и `%LOCALAPPDATA%\FBE\interface-locale.txt`;
- fallback на язык Windows и `en-US`;
- безопасное чтение UTF-8 JSON;
- извлечение строк из `Lang/<locale>/<module>.json` по binding-таблице модуля.

На текущем этапе helper уже используют FBE, FBV, ExportHTML, ExportDOCX, ExportEPUB и ImportEPUB. Каждый модуль сохраняет собственную binding-таблицу и встроенный Win32 fallback, но порядок выбора языка и чтения JSON теперь единый. Контракт отслеживает `tools/tests/test-runtime-interface-language-contract.ps1`.

## Опциональные языковые пакеты установщика

Целевое состояние для будущего установщика: языковые ресурсы должны стать
отдельными компонентами. Базовый язык и язык, выбранный пользователем, должны
ставиться всегда, а дополнительные `res_*.dll` и `.mui`-файлы можно будет не
выбирать. Это уменьшит размер установки и сделает поведение понятнее для
пользователей, которым не нужны все языки.

Практически это означает, что перед runtime-подключением Weblate-каталогов нужно
разделить:

- обязательные language-neutral файлы программы;
- ресурсные DLL основного интерфейса;
- MUI-ресурсы FBV/shell-команд в `Lang\Shell`;
- локализацию плагинов;
- fallback-язык, который остаётся доступен даже при минимальной установке.


## Экспорт seed-файлов для переводчиков

Для передачи текущего набора строк переводчику или для подготовки будущего
Weblate-контура добавлен сценарий:

```powershell
.\tools\localization\export-weblate-seed.ps1
```

Он создаёт временный каталог `out\localization\weblate-seed` и кладёт туда
отдельный JSON-файл для каждого целевого языка. Эти файлы не являются runtime-
ресурсами и не должны вручную правиться как источник истины: пока источником
остаются `localization/app-ui/catalog.json` и `localization/plugin-ui/catalog.json`.

Формат seed-экспорта намеренно простой и проверяемый:

- `manifest.json` содержит `formatVersion`, `fallbackLanguage`, список языков,
  исходные каталоги, список файлов и ожидаемое число строк;
- каждый `<locale>.json` содержит `formatVersion`, `language`,
  `fallbackLanguage`, `stringCount` и объект `strings`;
- у каждой строки сохраняются стабильный ключ, область (`app-ui` или
  `plugin-ui`), переводимый текст, исходная строка `source`, `resourceId`,
  компонент и комментарий для переводчика, если он задан в исходном каталоге.

Это промежуточный формат для вычитки и будущей интеграции с Weblate. Для
запуска программы используются не эти seed-файлы напрямую, а сгенерированные
`Lang/<язык>/<модуль>.json`; при их отсутствии приложение продолжает использовать
встроенные Win32-ресурсы как безопасный fallback.

Автоматическая проверка `tools/tests/test-localization-export.ps1` запускает
экспорт во временный каталог и убеждается, что app/plugin-строки реально попали
во все языковые файлы.

Для упаковки добавлен отдельный package-gate `tools/tests/test-runtime-lang-package.ps1`. Он проверяет уже собранный `out/package/FictionBookEditor`, чтобы `Lang/<язык>/fbe.json`, `fbv.json`, `export-html.json`, `export-docx.json`, `export-epub.json` и `import-epub.json` реально попадали в portable/staging-каталог, а значит и во входные файлы NSIS-установщика.


## Инвентарь будущих языковых пакетов

Файл `localization/language-packs.json` описывает, какие ресурсы относятся к
какому языку: ресурсные DLL основного интерфейса, MUI-файлы FBV/shell-команды в
`Lang\Shell`, словари, лицензии, жанры, XSL и будущие локализованные шаблоны
`blank_*.fb2`.

На текущем этапе этот файл ничего не меняет в поведении установщика. Его задача —
дать проверяемую карту для следующего шага, где языки станут отдельными
компонентами установки. Проверка `tools/tests/test-language-packs-inventory.ps1`
сверяет инвентарь с app/plugin-каталогами и ловит забытые языковые ресурсы.
Черновой NSIS-план будущих языковых компонентов можно сгенерировать командой:

```powershell
.\tools\localization\export-nsis-language-pack-plan.ps1
```

Результат появится в `out\localization\nsis-language-packs` как draft `.nsh`.
Он не подключается к `MakeInstaller.nsi`; это файл для ревизии будущего
разбиения языков на секции до изменения поведения установщика. Проверка
`tools/tests/test-nsis-language-pack-plan.ps1` следит, чтобы draft-план
генерировался из актуального инвентаря.

## Ближайший практический план

1. [x] Составить первый машинно-проверяемый каталог основных пользовательских строк новых плагинов (`localization/plugin-ui/catalog.json`).
2. [x] Добавить стартовые украинские переводы для основных строк окон `ExportDOCX`, `ExportEPUB`, `ImportEPUB` и `ExportHTML`.
3. [x] Добавить машинно-проверяемые черновые переводы для основных европейских языков с пометкой, что они требуют вычитки носителями языка.
4. [x] Добавить подготовительный каталог FBE/FBV (localization/app-ui/catalog.json) и smoke-проверку его полноты.
5. [x] Научить сборку генерировать подготовительные локализованные resource-фрагменты
   из каталога переводов.
6. [x] Добавить smoke-проверку, что в ресурсах плагинов нет mojibake и что основные
   строки доступны хотя бы для `ru-RU`, `uk-UA` и `en-US`.
7. [x] Расширить Weblate seed export: добавить версию формата, fallback-язык,
   счётчики строк, комментарии и проверку ключевых строк.
8. Подготовить следующий слой: вынести оставшиеся UI-строки FBE/FBV из старых
   resource DLL в общий JSON→generated pipeline без удаления существующего
   fallback.
9. [x] Для FBE первым практическим срезом перенести в
   `localization/app-ui/catalog.json` все `IDS_UPDATE_*`, `IDS_SEARCH_END_MSG`
   и `IDS_READONLY_SAVE_MSG`.
10. [x] Добавить отдельный строгий генератор FBE по модели FBV, который будет
    создавать существующие `IDS_*`, а не новые `IDS_L10N_*`, и только после
    этого аккуратно подключать generated `.rc2` к `res_rus`/`res_ukr`.
11. [x] Аккуратно подключить `FBEStrings.generated.rc2` к `res_rus`/`res_ukr`:
    сначала убрать или изолировать дублирующие ручные `STRINGTABLE`, затем
    собрать обе resource DLL и проверить окно «О программе», проверку обновлений,
    read-only warning и завершение поиска.
12. [x] Перенести вторым FBE-срезом сообщения сохранения/validation/import/export,
    внешнего изменения файла, нехватки памяти и отсутствующих скриптов в
    `localization/app-ui/catalog.json` и generated `FBEStrings.generated.rc2`.
13. [x] Перенести третьим FBE-срезом сообщения XML/script/COM, replace/search
    и замены слов в `localization/app-ui/catalog.json` и generated
    `FBEStrings.generated.rc2`.
14. [x] Перенести четвёртым FBE-срезом строки добавления изображения и binary-
    ресурсов в `localization/app-ui/catalog.json` и generated
    `FBEStrings.generated.rc2`.
15. [x] Пятым FBE-срезом закрыть 16-ID блок 176–191: перенести
    `IDS_SB_SAVED_NO_ERR` в `localization/app-ui/catalog.json` и generated
    `FBEStrings.generated.rc2`, а connect-скрипт научить удалять пустые
    `STRINGTABLE`.
16. [x] Шестым FBE-срезом перенести компактные блоки `IDS_MB_*`,
    `IDS_DOCUMENT_TREE_CAPTION` и `IDS_ENCODINGS`; блоки 592–607 и
    61392–61407 теперь также полностью generated.
17. [x] Седьмым FBE-срезом закрыть смешанные блоки 160–175, 288–303
    и 304–319: genres/reference/XML/Scintilla, update/about/language/recovery
    и subscript/superscript строки теперь генерируются из JSON-каталога.
18. [x] Восьмым FBE-срезом закрыть блок 144–159: command-line diagnostics
    и подписи table/image/section/style полей перенесены в JSON→generated `.rc2`.
19. [x] Девятым FBE-срезом закрыть блок 192–207: выбор папки скриптов,
    часть Edit/Navigation hotkey captions и сообщение о конфликте hotkey скрипта
    перенесены в JSON→generated `.rc2`.
20. [x] Десятым FBE-срезом закрыть блок 272–287: inline image, fast mode,
    навигация по footnote/matching/wrong tag, spell-check, tree view и завершение
    проверки орфографии перенесены в JSON→generated `.rc2`.
21. [x] Одиннадцатым FBE-срезом закрыть последний смешанный блок 256–271:
    контекстное меню, document-tree menu, hotkey collision/status и prompt
    перезапуска настроек перенесены в JSON→generated `.rc2`.
22. [x] Двенадцатым FBE-срезом начать перенос полностью ручных блоков:
    блок 96–111 с File hotkey captions, insert/overwrite status и document-tree
    cleanup перенесён в JSON→generated `.rc2`.
23. [x] Тринадцатым FBE-срезом закрыть блок 112–127: команды добавления
    epigraph/image/text-author/title, базовые Edit hotkeys и вставка cite/image/poem
    перенесены в JSON→generated `.rc2`.
24. [x] Четырнадцатым FBE-срезом закрыть блок 128–143: document metadata
    captions, English/Russian language labels, status `No errors`, View/Other
    settings captions и table href/id captions перенесены в JSON→generated `.rc2`.
25. [x] Пятнадцатым FBE-срезом закрыть блок 208–223: hotkey-подписи
    сворачивания/разворачивания дерева на уровни 1–9 и выбор `Href` перенесены
    в JSON→generated `.rc2`.
26. [x] Шестнадцатым FBE-срезом закрыть блок 224–239: hotkey-подписи
    style/view/navigation и вставки таблицы перенесены в JSON→generated `.rc2`.
27. [x] Семнадцатым FBE-срезом закрыть блок 240–255: настройки hotkey-групп
    «Скрипты/Символы», окно слов и пункт «Вырезать» перенесены в JSON→generated
    `.rc2`. По текущему анализатору ручных FBE `IDS_*` STRINGTABLE-блоков не осталось.
28. [x] Добавить инвентарь оставшихся FBE `MENU`/`DIALOGEX` UI-литералов: `tools/localization/analyze-fbe-rc-ui-literals.ps1` формирует JSON-отчёт по русской и украинской `.rc`, а `tools/tests/test-fbe-rc-ui-literals-inventory.ps1` проверяет базовый контракт перед переносом меню и диалогов в JSON/Weblate pipeline.
29. [x] Подготовить Weblate-friendly seed-каталог главного меню FBE `IDR_MAINFRAME`: 68 пунктов меню заведены в `localization/app-ui/fbe-idr-mainframe-menu.json` с 12 языками и проверяются `tools/tests/test-fbe-main-menu-catalog.ps1`. Runtime-подключение MENU-генерации остаётся отдельным следующим шагом.
30. [x] Добавить генератор generated `.rc2` для главного меню FBE `IDR_MAINFRAME`: `tools/localization/update-fbe-main-menu-resource.ps1` создаёт ru/ukr MENU-фрагменты из JSON, а `tools/tests/test-fbe-main-menu-generated-resource.ps1` проверяет структуру. Замена ручного MENU-блока в `.rc` остаётся следующим отдельным шагом.
31. [x] Подключить generated главное меню FBE `IDR_MAINFRAME` в русскую и украинскую runtime-локализации: ручной MENU-блок заменён на `FBEIdrMainframeMenu.generated.rc2`, добавлен `tools/tests/test-fbe-main-menu-connected-resource.ps1`, `res_rus.dll`/`res_ukr.dll` собираются. Инвентарь оставшихся ручных FBE `MENU`/`DIALOGEX` UI-литералов уменьшился до 278 строк.
32. [x] Перенести малые меню FBE `IDR_DOCUMENT_TREE` и `IDR_TOOLBAR_MENU` на JSON→generated pipeline: `localization/app-ui/fbe-secondary-menus.json`, `tools/localization/update-fbe-secondary-menu-resources.ps1` и `tools/tests/test-fbe-secondary-menus.ps1` закрывают последние ручные `MENU`-литералы. В FBE-инвентаре осталось 264 ручных `DIALOGEX` строки.
33. [x] Перенести малые DIALOGEX-диалоги FBE `IDD_TABLE`, `IDD_INPUTBOX` и `IDD_ADDIMAGE` на JSON→generated pipeline: `localization/app-ui/fbe-small-dialogs.json`, `tools/localization/update-fbe-small-dialog-resources.ps1` и `tools/tests/test-fbe-small-dialogs.ps1` закрывают первые 30 DIALOGEX-литералов. В FBE-инвентаре осталось 234 ручных `DIALOGEX` строки.
34. [x] Перенести следующий компактный DIALOGEX-срез FBE `IDD_TOOLS_SETTINGS`, `IDD_ABOUTBOX` и `IDD_CUSTOMSAVEDLG` в тот же JSON→generated pipeline: `fbe-small-dialogs.json` теперь содержит 24 строки на 12 языков, generated-ресурс расширяет окно «О программе» под GitHub-ссылку, а в FBE-инвентаре осталось 216 ручных `DIALOGEX` строк.
35. [x] Перенести страницу настроек слов FBE `IDD_SETTINGS_WORDS` в JSON→generated pipeline: `fbe-small-dialogs.json` теперь содержит 29 строк на 12 языков, а в FBE-инвентаре осталось 206 ручных `DIALOGEX` строк.
36. [x] Перенести страницу настроек горячих клавиш FBE `IDD_HOTKEYS` в JSON→generated pipeline: `fbe-small-dialogs.json` теперь содержит 38 строк на 12 языков, а в FBE-инвентаре осталось 188 ручных `DIALOGEX` строк.
37. [x] Перенести диалог поиска FBE `IDD_FIND` в JSON→generated pipeline: `fbe-small-dialogs.json` теперь содержит 48 строк на 12 языков, а в FBE-инвентаре осталось 168 ручных `DIALOGEX` строк.
38. [x] Перенести диалог замены FBE `IDD_REPLACE` в JSON→generated pipeline: `fbe-small-dialogs.json` теперь содержит 61 строку на 12 языков, а в FBE-инвентаре осталось 142 ручных `DIALOGEX` строки.
39. [x] Перенести диалог проверки орфографии FBE `IDD_SPELL_CHECK` в JSON→generated pipeline: `fbe-small-dialogs.json` теперь содержит 73 строки на 12 языков, а в FBE-инвентаре осталось 118 ручных `DIALOGEX` строк.
40. [x] Перенести диалог списка слов FBE `IDD_WORDS` в JSON→generated pipeline: `fbe-small-dialogs.json` теперь содержит 91 строку на 12 языков, а в FBE-инвентаре осталось 82 ручных `DIALOGEX` строки.
41. [x] Перенести страницу прочих настроек FBE `IDD_SETTING_OTHER` в JSON→generated pipeline: `fbe-small-dialogs.json` теперь содержит 111 строк на 12 языков, а в FBE-инвентаре остался только `IDD_OPTIONS` на 42 ручных `DIALOGEX` строки.
42. [x] Перенести страницу основных параметров FBE `IDD_OPTIONS` в JSON→generated pipeline: `fbe-small-dialogs.json` теперь содержит 132 строки на 12 языков, а ручной FBE-инвентарь `MENU`/`DIALOGEX` показывает 0 оставшихся UI-литералов.
43. [x] Подключить FBE MENU/DIALOGEX-проверки к release-gate: `verify-release.ps1` запускает тесты главного меню, малых меню, generated DIALOGEX-диалогов и нулевого ручного UI-инвентаря.
44. [x] Добавить продуктовый аудит hardcoded C/C++ строк: `tools/localization/analyze-product-hardcoded-cyrillic.ps1` ищет кириллицу в строковых литералах исходников продукта, не считая русские комментарии нарушением. Текущая сводка показывает основной следующий фронт: `ExportDOCXPlugin.cpp`, `EpubImport.cpp`, `ExportEPUBPlugin.cpp`, shell/metadata diagnostics.
45. [x] Начать уменьшение DOCX hardcoded-хвоста: вкладки окна настроек, значения комбобоксов примечаний/empty-line/языка/профиля и подпись кнопки сохранения перенесены в `IDS_DOCX_*`, генерируются в `ExportDOCXStrings.generated.rc2` из `localization/plugin-ui/catalog.json` и проверяются `test-export-docx-localization-resources.ps1`.
46. [x] Перенести основной пользовательский runtime-хвост ExportDOCX: титульная страница, отчёт экспорта, TOC-текст и предупреждения validation теперь заведены в `localization/plugin-ui/catalog.json`, генерируются в `ExportDOCXStrings.generated.rc2` и проверяются `test-export-docx-localization-resources.ps1`; аудит `ExportDOCXPlugin.cpp` уменьшен до технических эвристик подписей иллюстраций.
47. [x] Перенести первый безопасный runtime-срез ImportEPUB: ошибки XML/ZIP/container/OPF/navigation/spine, плейсхолдеры изображений/примечаний и заголовок диагностического отчёта заведены в `localization/plugin-ui/catalog.json`, генерируются в `ImportEPUBStrings.generated.rc2` и проверяются `test-import-epub-localization-resources.ps1`; аудит `EpubImport.cpp` уменьшен с 69 до 35 hardcoded-строк.
48. [x] Перенести второй безопасный runtime-срез ImportEPUB: SVG/LunaSVG/GDI+, XHTML, spine-изображения, mimetype/encryption и финальная FB2 validation заведены в `IDS_IMPORT_RUNTIME_*`, generated-файл вырос до 151 строки на язык, а аудит `EpubImport.cpp` уменьшен до 7 строк жанровых эвристик, которые пока намеренно оставлены как поведенческая логика импорта.
49. [x] Перенести основной plugin-runtime срез ExportEPUB: preflight/summary fallback-строки очищены от кириллицы, заголовки body/chapter/annotation и поля титульной страницы заведены в `IDS_EXPORT_*`, `ExportEPUBStrings.generated.rc2` вырос до 82 строк на язык, а `ExportEPUBPlugin.cpp` больше не содержит hardcoded-кириллицы.
50. [x] Закрыть остаток ExportEPUB в `FbeEpubExport.cpp`: название обложки передаётся через `EpubExportLabels` в `EpubExportOptions`, ресурс `IDS_EXPORT_COVER_TITLE` генерируется из JSON, а низкоуровневый exporter остаётся без прямой зависимости от Win32-ресурсов.

Генератор `tools/localization/export-win32-resource-fragments.ps1` создаёт
проверяемые `.rc2`-фрагменты и общий `l10n_resource_ids.h` из JSON-каталогов.
Пока эти файлы не подключаются к реальным `.rc`: это промежуточный этап, чтобы
сначала стабилизировать ключи, идентификаторы и кодировку.
Для FBV используется отдельный более строгий генератор
`tools/localization/update-fbv-resource-strings.ps1`, потому что он создаёт не
новые `IDS_L10N_*`, а существующие `IDS_*`, уже используемые кодом валидатора.
Для FBE добавлен аналогичный генератор
`tools/localization/update-fbe-resource-strings.ps1`. Он создаёт
`src/locales/res_rus/FBEStrings.generated.rc2` и
`src/locales/res_ukr/FBEStrings.generated.rc2`; `res_rus/FBE.rc` и
`res_ukr/FBE.rc` подключают эти файлы через `#include`, а ручные дубли первого
FBE-среза удалены из локализованных `STRINGTABLE`.
Для ExportHTML используется аналогичный
`tools/localization/update-export-html-resource-strings.ps1`: он также создаёт
существующие `IDR_*`/`IDS_*`, уже используемые кодом плагина.

## Важно

Переводы плагинов нельзя считать завершёнными, пока они не проверены в реальном
окне: в Win32-диалогах длинный перевод может не поместиться даже при корректной
строке. Поэтому после каждого крупного языкового обновления нужен ручной
GUI-smoke по настройкам импорта/экспорта.



### Внешний runtime-слой `Lang`

Подготовлен первый проверяемый экспорт будущих внешних JSON-файлов локализации. Скрипт `tools/localization/export-runtime-lang.ps1` читает `localization/app-ui/catalog.json`, `localization/plugin-ui/catalog.json` и контракт `localization/runtime/contract.json`, после чего формирует файлы вида `Lang/<язык>/<модуль>.json`.

Текущий формат файла:

```json
{
  "formatVersion": 1,
  "module": "export-epub",
  "locale": "ru-RU",
  "fallbackLocale": "en-US",
  "strings": {
    "export_epub.content.navigation_title": "Навигация"
  }
}
```

Отсутствие внешнего JSON-файла не должно ломать запуск приложения: чтение `Lang/<язык>` подключается поверх уже существующего встроенного fallback в ресурсах. Проверка `tools/tests/test-runtime-lang-export.ps1` добавлена в release-gate и фиксирует, что все 12 языков и все модули из контракта получают валидные JSON-файлы. Скрипт `tools/build/package-portable.ps1` уже экспортирует этот каталог в `out/package/FictionBookEditor/Lang`, а NSIS-установщик забирает `Lang` из входного portable-каталога.

### Общий runtime JSON-loader

FBE, FBV, ExportHTML, ExportDOCX, ExportEPUB и ImportEPUB используют общий helper `src/common/RuntimeLocalizationCommon.h`. В нём сосредоточены проверка поддерживаемых локалей, чтение выбранного языка из `FBE_UI_LOCALE` и `%LOCALAPPDATA%\FBE\interface-locale.txt`, UTF-8 JSON-парсер и загрузка `Lang/<locale>/<module>.json`. Порядок fallback теперь один для всех runtime-потребителей: сначала `en-US`, затем выбранная локаль, затем встроенные Win32-ресурсы конкретного модуля. Это убирает дублирование парсера между модулями и делает будущие изменения формата/порядка fallback централизованными.

Контракт общего loader проверяется `tools/tests/test-runtime-interface-language-contract.ps1`, а работоспособность overlay для каждого модуля — отдельными тестами `test-*-runtime-lang-overlay.ps1`.

При смене языка в настройках FBE редактор сразу публикует новую locale в общий runtime-контракт и сбрасывает cache `fbe.json`. Плагины ExportHTML, ExportDOCX, ExportEPUB и ImportEPUB дополнительно перечитывают свои runtime JSON-файлы при входе в экспорт/импорт, поэтому новое окно плагина уже использует свежий язык даже если DLL была загружена раньше. Уже открытые окна поиска/замены и проверки орфографии перед применением настроек закрываются и затем открываются заново на актуальном языке. Полное обновление уже созданных дочерних окон и уже открытого plugin UI на лету требует отдельного этапа: старые Win32-ресурсы и уже созданные HWND-контролы сами по себе не перечитывают тексты.

### Первый runtime-потребитель: FBV

FBV подключён к внешнему JSON-слою первым. При запуске `FBV.exe` загружает `Lang/en-US/fbv.json`, затем файл текущей системной локали, например `Lang/ru-RU/fbv.json`. Найденные строки перекрывают встроенные `IDS_*` ресурсы только для уже описанных binding-ключей. Если каталог `Lang`, конкретный JSON-файл или отдельный ключ отсутствует, приложение продолжает использовать встроенный ресурс.

Такой порядок важен для portable и неполных установок: внешний язык можно удалить или не установить, но FBV не должен падать и не должен терять базовый интерфейс. Контракт синхронизации кода и JSON проверяет `tools/tests/test-fbv-runtime-lang-overlay.ps1`.

### Второй runtime-потребитель: ExportHTML

ExportHTML подключён к той же схеме после FBV. При загрузке `ExportHTML.dll` читает `Lang/en-US/export-html.json`, затем файл текущей системной локали, например `Lang/ru-RU/export-html.json`. Через этот слой перекрываются сообщения ошибок, фильтр сохранения, заголовок команды экспорта и tooltip-строки custom save dialog.

Fallback остаётся тем же: если внешнего JSON нет, плагин использует встроенные `IDR_*`/`IDS_*` ресурсы из `ExportHTML.rc` и generated `.rc2`. Синхронизацию binding-ключей с JSON проверяет `tools/tests/test-export-html-runtime-lang-overlay.ps1`.

### Третий runtime-потребитель: ImportEPUB

ImportEPUB подключён к внешнему runtime JSON-слою после ExportHTML. При загрузке
`ImportEPUB.dll` читает `Lang/en-US/import-epub.json`, затем файл текущей
системной локали, например `Lang/uk-UA/import-epub.json`. Через этот слой
перекрываются строки окна настроек импорта, tooltip-строки, системный диалог
выбора EPUB, стадии импорта, сообщения COM-плагина и runtime diagnostics
XML/ZIP/container/OPF/navigation/spine/SVG/XHTML/validation.

Fallback остаётся безопасным: если внешнего JSON нет, файл повреждён или
конкретная строка не найдена, ImportEPUB использует встроенные `IDS_IMPORT_*`
ресурсы из `ImportEPUB.rc` и generated `.rc2`. Контракт C++ binding-ключей и
JSON-файлов проверяет `tools/tests/test-import-epub-runtime-lang-overlay.ps1`.

### Четвёртый runtime-потребитель: ExportEPUB

ExportEPUB подключён к той же схеме после ImportEPUB. При загрузке
`ExportEPUB.dll` читает `Lang/en-US/export-epub.json`, затем файл текущей
системной локали, например `Lang/de-DE/export-epub.json`. Через этот слой
перекрываются строки окна настроек экспорта, tooltip-строки, фильтр сохранения,
preflight-предупреждения, summary-окно и текстовые EPUB labels, которые уже
были заведены в `localization/plugin-ui/catalog.json`.

Fallback остаётся безопасным: если внешнего JSON нет, файл повреждён или
конкретная строка не найдена, ExportEPUB использует встроенные `IDS_*` ресурсы
из `ExportEPUB.rc` и generated `.rc2`. Контракт C++ binding-ключей и JSON-файлов
проверяет `tools/tests/test-export-epub-runtime-lang-overlay.ps1`.

### Пятый runtime-потребитель: ExportDOCX

ExportDOCX подключён к внешнему runtime JSON-слою после ExportEPUB. При загрузке
`ExportDOCX.dll` читает `Lang/en-US/export-docx.json`, затем файл текущей
системной локали, например `Lang/pl-PL/export-docx.json`. Через этот слой
перекрываются строки окна настроек DOCX-экспорта, tooltip-строки, фильтр
сохранения, diagnostic report, title-page labels, TOC-текст и validation
warnings.

Fallback остаётся безопасным: если внешнего JSON нет, файл повреждён или
конкретная строка не найдена, ExportDOCX использует встроенные `IDS_*` ресурсы
из `ExportDOCX.rc` и generated `.rc2`. Контракт C++ binding-ключей и JSON-файлов
проверяет `tools/tests/test-export-docx-runtime-lang-overlay.ps1`.

### Шестой runtime-потребитель: FBE

Основной редактор FBE подключён к тому же внешнему runtime JSON-слою после
плагинов. При запуске он читает `Lang/en-US/fbe.json`, затем выбранную локаль
из общей настройки языка интерфейса. Через этот слой перекрываются строки,
которые уже обслуживаются `FbeLoadString` и `U::MessageBox`: сообщения
обновлений, read-only warning, статусы, настройки, hotkey-названия и часть
diagnostics.

Fallback остаётся встроенным: если внешний JSON отсутствует, повреждён или
не содержит конкретный ключ, FBE продолжает использовать обычные Win32-ресурсы.
Контракт binding-ключей и JSON-файлов проверяет
`tools/tests/test-fbe-runtime-lang-overlay.ps1`.

### Runtime-аудит C/C++ строк

Текущий локализационный pipeline дополнен строгой проверкой `tools/localization/analyze-product-hardcoded-cyrillic.ps1`: после переноса основных строк FBE/FBV/плагинов и очистки shell/metadata diagnostics в C/C++ строковых литералах продукта не должно оставаться зашитой кириллицы.

Для пользовательских строк используется JSON→generated `.rc2` контур. Для внутренних диагностических fallback-сообщений shell/thumbnail/metadata допустим ASCII fallback: эти строки нужны для логов, HRESULT-smoke и отладки, а не для пользовательского интерфейса. Поведенческие русскоязычные эвристики, которые должны продолжать распознавать старые отчёты или подписи (`нет`, `илл.`, `таблица`, жанровые фрагменты EPUB), записываются Unicode escape-последовательностями, чтобы сохранить совместимость и не возвращать mojibake в исходники.
