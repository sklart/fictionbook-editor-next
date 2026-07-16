# FictionBook Editor

Модернизация FictionBook Editor для актуальной сборки и эксплуатации в Windows.

# Назначение проекта

Проект развивает настольный редактор книг формата FictionBook 2 (`.fb2`).
Основные направления работы:

- сохранение и развитие существующего редактора FBE;
- сборка исходников современным Visual Studio и Windows SDK;
- обновление библиотек поиска, редактора кода и проверки орфографии;
- повышение надёжности сохранения, восстановления и диагностики сбоев;
- модернизация интеграции `.fb2` с Проводником Windows;
- формирование воспроизводимого релизного контура, portable-пакета и установщика.

Текущая версия исходников: `3.0.6` (подготовка к выпуску).

# Текущий статус

Снимок состояния: 16 июля 2026 года.

## Завершено

- Реализован первый этап улучшения диагностики пользовательских JavaScript-скриптов: для ошибок чтения, синтаксического разбора и выполнения выводятся файл, путь, координаты, контекст строки, исходное сообщение движка и код ошибки; сведения можно скопировать в буфер обмена.

- В рамках #24 base64-содержимое `binary` при сохранении FB2 переводится в
  компактный вид без добавляемых MSXML переносов строк, а допустимое тире в
  автоматически создаваемом `binary id` больше не отбрасывается. Нужна
  отдельная ручная проверка на больших иллюстрированных книгах и оценка
  индикации прогресса сохранения.

- В рамках #11 проверка орфографии после локальной правки больше не строит
  коллекцию всех абзацев документа и не очищает все накопленные подчёркивания:
  обновляется содержащий выделение абзац, а при прокрутке обход начинается от
  видимой области. Нужна ручная проверка на длинных разделах.

- Репозиторий перенесён в современную структуру `src/runtime/third_party/tools/packaging/docs`.
- Основное приложение собирается с Visual Studio 2026.
- Выполнена миграция режима «Дизайн» с PCRE 7.9 на PCRE2 10.47.
- Обновлены Scintilla до 5.6.4 и Lexilla до 5.5.1; добавлены regression-тесты.
- Обновлён Hunspell, устранено выявленное падение орфографии в `suggestmgr.cxx`.
- Механизм версионирования синхронизирован с `update.xml`.
- Добавлены аварийное восстановление, minidump-диагностика и безопасная запись документов.
- Упорядочен релизный поток `out/Release -> out/package -> out/artifacts`.
- Формируются setup, portable ZIP, архив символов и SHA-256 checksums.
- Legacy `ColumnProvider`, `ContextMenu` и `IconExtractor` исключены из стандартного modern-контура.
- Введён единый JSON-контур локализации для FBE, FBV и поставляемых плагинов:
  12 языков лежат в `Lang/<locale>`, а отсутствие пакета безопасно откатывается
  к английскому JSON и встроенным ресурсам.
- Установщик позволяет выбирать те же языки; English, русский и украинский
  включены по умолчанию. Шаблоны `blank_*.fb2` отложены в отдельный этап.
- Экспортные плагины `ExportHTML`, `ExportDOCX` и `ExportEPUB` регистрируются
  без повышения прав в пользовательском разделе реестра. Для сборки setup
  обязателен Unicode-вариант NSIS, иначе UTF-8 сообщения установщика будут
  повреждены.
- Страница компонентов установщика сохраняет широкое дерево выбора и имеет
  увеличенное поле описания; тексты сокращены для русской, английской,
  украинской и fallback-локализаций.
- Созданы modern property handler, FBE-specific property schema и experimental thumbnail provider.
- Создан широкий набор автоматических smoke/regression-проверок.
- `CHANGELOG.md` приведён к формату Keep a Changelog.
- Пожелания пользователей из Google Groups включены в `docs/todo.md`.

## Выполняется

- Локальная уборка и дальнейшая модернизация исходников `src/fbv` без смены общей Win32/ATL-архитектуры.

## Ожидает выполнения

- Исследование split view редактора.
- Решение по возможной миграции установщика с NSIS на Inno Setup.
- Оценка и реализация актуальных пожеланий пользователей из Google Groups.
- Дальнейшее выравнивание документации и комментариев на русском языке.

# Исходные материалы

- `D:\Download\FBeditor` — единственная рабочая папка проекта, которую следует использовать.
- `https://github.com/sklart/fictionbook-editor-next` — основной рабочий GitHub-репозиторий и единственная цель для push/release.
- `https://github.com/sensboston/fictionbookeditor` — исторический исходный репозиторий; допустим только как read-only upstream/reference, публиковать изменения в него нельзя.
- `https://github.com/evpobr/fictionbookeditor` — дополнительный upstream/reference для `ExportHTML` и возможной сверки `FBV`.
- `https://github.com/hunspell/hunspell` — источник Hunspell.
- `third_party/pcre2` — исходники PCRE2 10.47.
- `third_party/scintilla` — исходники Scintilla 5.6.4.
- `third_party/lexilla` — исходники Lexilla 5.5.1.
- `third_party/hunspell` — интегрированные исходники Hunspell; точная версия в текущей документации не определена.
- `third_party/uac` — полный комплект NSIS UAC plugin 0.2.4c, перенесённый из ранее скачанного `UAC.zip`.
- `tools/upx/upx.exe` — локальная копия UPX 5.2.0.
- `third_party/wtl` — WTL, подключаемый как внешняя зависимость/submodule.
- `runtime` — схемы FB2, XSL, словари, скрипты, справка, локализации и прочие runtime-материалы.
- `tools/tests/*.fb2` и `tools/tests/regex-fixtures.json` — тестовые документы и fixture-наборы.
- Реальные книги из `Книжная серия - Библиотека фонда «Династия»` использовались для ручной проверки shell-метаданных и миниатюр.
- `FBE_GoogleGroups_Analysis.md` использовался как источник выжимки пожеланий; исходный файл в текущей файловой системе отсутствует, его актуальное содержание перенесено в `docs/todo.md`.
- Используются официальные контракты Windows Shell/Property System и документация Microsoft Learn.

# Структура проекта

- `FBE.sln` — основное решение Visual Studio.
- `src/fbe` — главное приложение `FBE.exe`, редактор, UI, поиск/замена, орфография, обновления, восстановление и диагностика.
- `src/fbshell` — modern property handler и experimental thumbnail provider для `.fb2`.
- `src/fbv` — локально восстановленные и уже модернизированные исходники `FBV.exe` на `MSXML6`.
- `src/export-html` — модуль `ExportHTML.dll`.
- `src/locales` — русская и украинская resource DLL.
- `runtime` — файлы, устанавливаемые или копируемые рядом с `FBE.exe`.
- `third_party` — исходники внешних зависимостей: PCRE2, Scintilla, Lexilla, Hunspell, WTL, NSIS UAC.
- `dependencies` — дополнительные подготовленные зависимости проекта.
- `build` — промежуточные файлы и результаты сборки зависимостей.
- `tools/build` — сборка, упаковка, регистрация shell-компонентов, сборка MUI-ресурсов для `Validate`, проверка релиза и обслуживание кэша Проводника.
- `tools/tests` — smoke/regression-тесты PCRE2, Scintilla, Hunspell, startup, metadata, property handler, thumbnail provider и `Validate`/`FBV`.
- `tools/version` — синхронизация версии исходников и `update.xml`.
- `packaging/nsis` — NSIS-установщик, локализации, ресурсы и встроенная копия UAC plugin.
- `docs` — актуальная техническая документация, планы и ручные чеклисты.
- `out/Release` — рабочие Release-бинарники приложения.
- `out/package/FictionBookEditor` — единый staging-каталог файлов поставки.
- `out/package/shell-build` — промежуточные Win32/x64 сборки modern shell DLL.
- `out/artifacts` — публикуемые артефакты релиза.
- `tmp` — временные диагностические материалы; не является источником релиза.
- `CHANGELOG.md` — журнал изменений локальной ветки модернизации в формате Keep a Changelog.
- `docs/todo.md` — основной актуальный backlog.
- `update.xml` — онлайн-манифест проверки обновлений из меню «Справка -> О программе».

# Выполненные работы

## Репозиторий и сборка

- Исходники разложены по каталогам ответственности; старые корневые копии отмечены Git как удалённые, новые каталоги пока не зафиксированы коммитом.
- Подготовлены PowerShell-сценарии сборки основного решения и зависимостей.
- Настроена сборка Visual Studio 2026 и Windows SDK.
- Поддерживается 32-битное основное приложение и установка на x86 Windows.
- Для 64-битного Проводника формируется отдельная `FBShell64.dll`; для 32-битного — `FBShell.dll`.
- Заявленная совместимость собранного приложения: Windows 7 SP1 и новее; Windows XP/Vista не поддерживаются.
- Добавлены DPI awareness Per-Monitor V2, поддержка High Contrast и обновление цветов при смене системной темы.

## Поиск, регулярные выражения и редактор кода

- Режим «Дизайн» переведён с PCRE 7.9 на PCRE2 10.47.
- Основная сборка, runtime, setup и portable-пакет очищены от обязательной `pcre.dll`.
- Legacy PCRE оставлен только для сравнительных тестов.
- Сохранён контракт `AU::IRegExp2`; runtime идёт напрямую через PCRE2.
- Реализованы и протестированы группы захвата, backreference, multiline, ignore-case, zero-length matches и преобразования регистра в замене.
- Исправлена регрессия Scintilla, при которой замена `\((\d+)\)` на `[\1]` давала пустую группу.
- Добавлены проверки повторного поиска, `not found`, некорректного regex, Unicode/UTF-8 индексов и устойчивости после замен.
- Пользовательская GUI-проверка основных сценариев режимов «Код» и «Дизайн» пройдена.

## Орфография

- Hunspell интегрирован из актуализированного исходного дерева.
- Устранено падение при открытии «Сервис -> Орфография» в `SuggestMgr::leftcommonsubstring`.
- Добавлены проверки словарей и regression-документация.
- Словари `en_US`, `ru_RU`, `uk_UA` входят в runtime; немецкий словарь доступен как опция установщика.

## Надёжность и диагностика

- Аварийная копия документа сохраняется в `%LOCALAPPDATA%\FBE Next\Recovery\Recovery.fb2`.
- Необработанные падения сохраняют `.dmp` и текстовый отчёт в `%LOCALAPPDATA%\FBE Next\Crashes`.
- Хранятся десять последних событий сбоев.
- Для релиза формируется отдельный архив PDB-символов.
- Документы записываются через временный файл и замену Windows API; создание
  `.bak` при перезаписи настраивается на вкладке «Настройки FBE Next» и
  включено по умолчанию; отключение резервной копии не отменяет безопасную
  атомарную замену файла.
- Сохранение аварийной копии учитывает временно невалидный XML в режиме «Код».

## Версионирование и обновления

- Версия централизована и проверяется `tools/version/sync-version.ps1`.
- До публикации 3.0.6 `update.xml` сохраняет сведения о последнем выпущенном
  релизе 3.0.5; обновлять манифест до 3.0.6 следует только вместе с готовыми
  setup-артефактами и их SHA-256.
- Проверка обновлений в FBE должна учитывать онлайн-файл `update.xml`.
- Добавлены проверки согласованности версии и update manifest.

## Релиз и установщик

- Зафиксирован единый staging-каталог `out/package/FictionBookEditor`; отдельные копии файлов для portable и setup не поддерживаются.
- Portable-вариант выпускается ZIP-архивом, поэтому portable-режим из setup удалён.
- NSIS использует `ManifestDPIAware true` и `SetCompressor /SOLID lzma`.
- UAC plugin обновлён до 0.2.4c и хранится в `third_party/uac`.
- Системная интеграция оформлена отдельной опцией; повышение прав должно требоваться только при её выборе.
- Подтверждено на реальной GUI-установке, что деинсталлятор создаётся всегда, независимо от выбора системной интеграции; при этом conditional UAC на удалении работает по ожидаемой модели: без системной интеграции запрос elevation не появляется, с системной интеграцией появляется.
- Добавлены русская, английская и украинская локализации установщика, а также локализация на основные европейские языки.
- В «Установленные приложения» добавлена иконка FBE через `DisplayIcon`.
- В uninstall metadata добавлен `EstimatedSize`; подтверждено на реальной машине, что в «Программы и компоненты» / «Установленные приложения» теперь отображается размер установленной программы.
- Обновлено изображение `packaging/nsis/res/Fb2-shema.bmp`.
- Добавлены проверки staging, NSIS layout, metadata бинарников и SHA256SUMS.
- Сформированы актуальные артефакты:
  - `FictionBookEditorNext-X.X.X-win32-setup.exe`;
  - `FictionBookEditorNext-X.X.X-win32-portable.zip`;
  - `FictionBookEditorNext-X.X.X-win32-symbols.zip`;
  - `FictionBookEditorNext-X.X.X-win7-win32-setup.exe`;
  - `FictionBookEditorNext-X.X.X-win7-win32-portable.zip`;
  - `FictionBookEditorNext-X.X.X-win7-win32-symbols.zip`;
  - `SHA256SUMS.txt`.

## Shell-интеграция

- Реализован независимый reader FB2-метаданных.
- Реализованы `IPropertyStore`, `IPropertyStoreCapabilities` и `IInitializeWithStream`.
- Стандартные Windows properties используются для `Author`, `Title`, `Language`.
- Создана FBE-specific schema для `Genre`, `Sequence`, `Keywords`, `DocumentId`, `DocumentVersion`, `DocumentDate`.
- Отображаемые имена собственных колонок имеют префикс `FBE:`.
- Ранее на реальной Windows 11 подтверждалась работа колонок, tooltip и правой панели после регистрации experimental-контура и `PreviewDetails`.
- Команда `Validate` вынесена из legacy context-menu DLL в обычный shell verb.
- Обычная иконка `.fb2` задаётся через `DefaultIcon` из `FBE.exe`.
- Для shell-команды `Validate` используется отдельный language-neutral модуль `FBVVerbResources.dll` и спутники `*.mui`, чтобы подпись пункта меню переключалась вместе с языком интерфейса Windows.
- Legacy `ColumnProvider`, `ContextMenu` и `IconExtractor` не входят в стандартный modern-проект.
- Experimental thumbnail provider читает PNG/JPEG/BMP-обложки, масштабирует их под запрошенный размер и обрабатывает негативные сценарии.
- Добавлены скрипты регистрации, отката, диагностики, сброса `thumbcache`/`iconcache` и адресного прогрева thumbnail cache.
- Добавлены автоматические тесты COM activation, shell API, property schema, install/uninstall и работы с fixture-файлами.
- Полная thumbnail-матрица подтверждена сценариями `test-fb2-shell-thumbnail-matrix.ps1` и forced dump-проверками на реальных книгах: прямой COM-контур, `CoCreateInstance`, shell API, forced extraction и fallback-отказы на битых/неполных `.fb2` воспроизводимо работают.
- По ручной проверке на Windows 11 подтверждено, что оставшиеся белые поля вокруг части обложек в preview/details pane объясняются уже не квадратной подложкой provider, а естественными пропорциями embedded cover image или поведением самого Explorer при масштабировании.

## Validate / FBV

- Из upstream `evpobr/fictionbookeditor` импортированы актуальные исходники `FBV` в `src/fbv`; локальный `FBV.exe` больше не зависит от исторического бинаря с `MSXML4`.
- Подтверждено, что `Validate` открывает новый XSD-валидный fixture `tools/tests/fb2-validate-valid-smoke.fb2` без ошибок (`No errors.`).
- Административный installer smoke подтверждает, что setup регистрирует `Validate\Command`, `Icon` и `MUIVerb = @...\FBVVerbResources.dll,-109;v2`, а деинсталляция корректно убирает этот shell-verb.
- В исходники `src/fbv/FBV.cpp` внесены точечные улучшения устойчивости: исправлен lifecycle `errmsg` при `Revalidate`, устранена потенциальная утечка, чтение размеров файла переведено на `GetFileSizeEx`, ослаблен sharing при `CreateFile`, а список файлов перенесён из глобалов в поля `CMainDlg`.

## Документация и локализация

- `docs/building.md` и `docs/versioning.md` переведены и актуализированы на русском языке.
- Подготовлены планы и ручные чеклисты PCRE2, Scintilla, spellcheck, property handler, FBE-specific schema и thumbnail provider.
- Новые сообщения build/test/release-скриптов преимущественно переведены на русский язык.
- `CHANGELOG.md` ведётся в формате Keep a Changelog.
- `docs/todo.md` содержит технический backlog и краткую выжимку пожеланий Google Groups.

# Последние изменения

- Clean-room и installer smoke подтвердили, что обычный `setup.exe` ставит `FBShell.dll`, `FBShell64.dll`, `FBE.Sequence.propdesc`, регистрирует shell-строки `InfoTip` / `TileInfo` / `Details` / `PreviewDetails` и возвращает shell-команду `Validate` после установки.
- Для `Validate` реализован отдельный MUI-контур: вместо чтения строки из `FBV.exe` используется language-neutral модуль `FBVVerbResources.dll` со спутниками `*.mui`; `SHLoadIndirectString` для `@...\FBVVerbResources.dll,-109;v2` возвращает корректную русскую строку без кракозябр.
- Административный smoke `tools/tests/test-fbe-specific-installer.ps1` подтверждает end-to-end регистрацию `Validate\Command`, `Icon` и `MUIVerb`, а также корректную очистку shell-verb после деинсталляции.
- Experimental thumbnail provider переведён на tight-scaling без
  искусственной квадратной подложки; forced dump на реальной книге подтвердил,
  что прежний баг с маленькой обложкой в центре большого белого квадрата
  устранён.
- Сводный сценарий `test-fb2-shell-thumbnail-matrix.ps1` проходит весь
  автоматический контур shell-miniatures, включая прямой COM,
  `CoCreateInstance`, shell API, forced extraction и negative cases.
- Query-тест на `tools/tests/fb2-metadata-smoke.fb2` возвращает все свойства корректно.
- Та же query-утилита на реальной книге `Даймонд. Ружья, микробы и сталь.fb2`, в том числе на ASCII-копии `tmp/diamond-guns-germs-steel.fb2`, после починки диагностического вывода возвращает ожидаемые свойства; generic XML parsing этого файла и shell-property query теперь согласованы.
- Основной незакрытый технический хвост shell-контура сейчас сосредоточен вокруг real-world property handler, а не вокруг `Validate` или базовой thumbnail-матрицы.

# Принятые решения

## Рабочая папка

- Что принято: всегда работать в `D:\Download\FBeditor`.
- Почему принято: это актуальное дерево со всеми изменениями и артефактами.
- Последствия: путь `C:\Users\Sklyarov\Documents\FBeditor` не использовать для продолжения проекта.

## Структура репозитория

- Что принято: использовать каталоги `src`, `runtime`, `third_party`, `tools`, `packaging`, `docs`, `build`, `out`.
- Почему принято: разделение исходников, runtime, зависимостей, упаковки и результатов соответствует современному сопровождаемому проекту.
- Последствия: не возвращать старые корневые копии файлов и `packaging/nsis/Installer/Input`.

## Regex

- Что принято: PCRE2 является единственным runtime-backend режима «Дизайн»; PCRE 7.9 нужен только для сравнительных тестов.
- Почему принято: PCRE устарел, а совместимость PCRE2 подтверждена regression-набором.
- Последствия: `pcre.dll` не должна возвращаться в runtime, setup или portable ZIP.

## Shell properties

- Что принято: использовать modern property handler вместо legacy `IColumnProvider`.
- Почему принято: `IColumnProvider` не является подходящей архитектурой для Windows 10/11.
- Последствия: legacy `ColumnProvider` остаётся только reference/opt-in контуром.

## Маппинг FB2-свойств

- Что принято: `Author`, `Title`, `Language` публикуются как стандартные Windows properties; книжные `Genre`, `Sequence`, `Keywords`, `DocumentId`, `DocumentVersion`, `DocumentDate` — как FBE-specific properties.
- Почему принято: системные аналоги части полей имеют неверную семантику, например музыкальный жанр не подходит книжному жанру; `sequence` означает серию и номер книги.
- Последствия: пользовательские имена колонок начинаются с `FBE:` и локализуются Windows property schema.

## Архитектуры shell DLL

- Что принято: поставлять `FBShell.dll` для Win32 Explorer и `FBShell64.dll` для x64 Explorer.
- Почему принято: in-process shell extension должна совпадать по разрядности с Explorer, при этом FBE должен работать и на x86 Windows.
- Последствия: установщик выбирает нужную DLL по разрядности ОС; staging содержит обе DLL.

## Иконка и миниатюры

- Что принято: обычная иконка `.fb2` задаётся `DefaultIcon`; обложки реализуются отдельным modern thumbnail provider.
- Почему принято: legacy `IconExtractor` зависит от устаревших bundled `zlib 1.1.4`, `libpng 1.2.8`, `libjpeg 6b` и загружает рискованный код в Explorer.
- Последствия: legacy image stack не возвращать в стандартную сборку; thumbnail provider доводить отдельно.

## Validate

- Что принято: `Validate` должен быть обычной shell-командой, запускающей `FBV.exe`, а не legacy COM context-menu extension.
- Почему принято: обычный shell verb проще, безопаснее и не загружает DLL в `explorer.exe`.
- Последствия: пункту меню нужна собственная иконка и отдельный локализуемый MUI-ресурс; для этого используется `FBVVerbResources.dll` и `*.mui`.

## Установщик и portable

- Что принято: один setup без portable-режима и отдельный portable ZIP из того же staging-каталога.
- Почему принято: исключается дублирование двух наборов файлов и неоднозначность пользовательского выбора.
- Последствия: setup выполняет обычную установку; portable распространяется отдельным архивом.

## UAC

- Что принято: повышение прав требуется только при выборе компонента «Системная интеграция».
- Почему принято: базовой per-user установке административные права не нужны.
- Последствия: setup вызывает UAC при выходе со страницы компонентов только для выбранной системной интеграции; uninstall ориентируется на явный маркер `SystemIntegrationInstalled` в uninstall-ключе и запрашивает elevation только для установленного system-integration контура. Ручной GUI-прогон подтверждён: без системной интеграции удаление идёт без UAC, с системной интеграцией — с UAC.

## Язык проекта

- Что принято: новые комментарии, памятки, документация и пользовательские сообщения делать на русском языке.
- Почему принято: это прямое требование пользователя и текущий язык сопровождения ветки.
- Последствия: английский допустим для API, идентификаторов, имён форматов и внешних исходников.

## CHANGELOG и TODO

- Что принято: каждое заметное изменение сразу записывать в `CHANGELOG.md`, планы и отложенные задачи — в `docs/todo.md`.
- Почему принято: работа ведётся длинными итерациями и должна продолжаться после смены чата.
- Последствия: изменение не считается полностью оформленным без актуализации этих файлов.

# Текущие задачи

1. Оценить миграцию NSIS -> Inno Setup после завершения текущей shell-задачи.
2. Исследовать split view: два независимых вида одного документа и более сложный `Body + Source`.
3. Сверить пожелания Google Groups с уже реализованными возможностями перед добавлением в план разработки.
4. Продолжить русификацию активной документации и комментариев.

# Открытые вопросы

- Сохранять NSIS или перейти на Inno Setup?
- Нужен ли split view только сверху/снизу или также слева/справа?
- Должны ли два view одного документа иметь независимый scroll, синхронный scroll или оба режима?
- Какие пожелания Google Groups уже закрыты текущей веткой? Требуется отдельная инвентаризация.

# Известные проблемы

- Иконка/миниатюра в малых режимах и правой панели зависит от корректности ассоциации, thumbnail registration и кэшей Explorer; при повторных ручных проверках нужно учитывать, что Explorer может сохранять старый визуальный результат даже после исправления provider.
- Explorer активно кэширует как успешные, так и отрицательные thumbnail/icon результаты; без clean-room процедуры ручной результат недостоверен.
- Антивирус Касперского ранее блокировал или удалял бинарники и мог удерживать DLL; папка проекта добавлялась в исключения, но актуальное состояние исключения не определено.
- Modern shell DLL не упаковываются UPX, потому что UPX не поддерживает используемые GUARD_CF-бинарники.

# План дальнейших работ

1. Вернуться к установщику: проверить условный UAC, uninstall и целесообразность Inno Setup.
2. Очистить `tmp` и устаревшие диагностические артефакты после фиксации нужных regression-fixtures.
3. Разбить масштабную модернизацию на логические Git-коммиты и обновить `CHANGELOG.md`, `docs/todo.md`, release checklist.
4. Перейти к крупным продуктовым задачам: split view и отобранные пожелания Google Groups.

# Важные требования пользователя

- После любого сжатия контекста возвращаться в `D:\Download\FBeditor` и всегда использовать эту папку.
- Все комментарии, памятки, документацию и новые пользовательские сообщения писать на русском языке.
- Все планы записывать в `docs/todo.md`.
- Все заметные изменения записывать в `CHANGELOG.md`.
- Сохранять возможность установки и работы FBE на x86 Windows.
- На x86 системе должны работать и shell-колонки; на x64 системе должен использоваться 64-битный shell handler.
- Не возвращать legacy PCRE в runtime.
- Не смешивать portable-режим с setup: portable выпускается отдельным ZIP.
- Файлы для setup и portable готовить один раз в едином staging-каталоге.
- Повышение прав запрашивать только после выбора опции системной интеграции.
- Не выполнять лишние действия, провоцирующие частые запросы антивируса, когда у пользователя нет локального доступа к компьютеру.
- Для сложных падений использовать Visual Studio и запускать отладку в нужном месте; пользователь готов вручную сообщать exception/call stack.
- При работе с Windows Shell сверяться с официальной документацией Microsoft.
- Названия FBE-specific полей должны соответствовать локализации Windows и иметь префикс `FBE:`.
- `Sequence` трактовать как серию и номер книги, а не как жанр.
- Книжный жанр не сопоставлять с `System.Music.Genre`.
- Обычная иконка `.fb2`, команда `Validate`, колонки, tooltip, правая панель и thumbnail extraction являются отдельными поверхностями и должны проверяться отдельно.
- Сохранять чистоту `out`: release, staging, artifacts и tests не смешивать.
- Не плодить дополнительные копии `FBShell.dll`/`FBShell64.dll`; целевой staging — `out/package`.

# Полезная справочная информация

## Версии и форматы

- Продукт: FictionBook Editor 3.0.6.
- Формат документов: FictionBook 2 XML (`.fb2`).
- Основная архитектура приложения: Win32.
- Shell DLL: Win32 и x64.
- PCRE2: 10.47.
- Scintilla: 5.6.4.
- Lexilla: 5.5.1.
- NSIS UAC plugin: 0.2.4c.
- UPX: 5.2.0; для modern shell DLL не применяется.
- Hunspell: 1.7.3.

## Стандарты и соглашения

- Changelog: Keep a Changelog 1.1.0.
- Версии и download metadata: `update.xml`.
- FBE-specific property schema: `packaging/property-schema/FBE.Sequence.propdesc`.
- Стандартные свойства Windows: `System.Author`, `System.Title`, `System.Language`.
- Собственные свойства: `FBE.Genre`, `FBE.Sequence`, `FBE.Keywords`, `FBE.DocumentId`, `FBE.DocumentVersion`, `FBE.DocumentDate`.
- Thumbnail provider реализуется через modern Windows Shell contract с `IInitializeWithStream` и thumbnail handler ShellEx.
- Property handler регистрируется через CLSID и `PropertySystem\PropertyHandlers`.
- Shell in-process DLL обязана совпадать с разрядностью Explorer.
- Кэш Explorer нужно учитывать при любых ручных тестах.

## Основные команды

```powershell
cd D:\Download\FBeditor
git submodule update --init --recursive
.\tools\build\build.ps1 -Configuration Release -Platform Win32
.\tools\build\create-release.ps1
.\tools\build\verify-release.ps1 -Configuration Release
```

Диагностика shell-интеграции:

```powershell
.\tools\tests\check-fb2-shell-surfaces.ps1
.\tools\tests\check-fbe-specific-properties.ps1
.\tools\tests\check-fbe-shell-registration-consistency.ps1
```

Обслуживание clean-room теста:

```powershell
.\tools\build\cleanup-shell-test-state.ps1
.\tools\build\reset-explorer-thumbnail-cache.ps1 -Confirm:$false
```

Полная release-команда и точные параметры описаны в `docs/building.md` и `docs/release-checklist.md`.

# Быстрый старт для нового исполнителя

1. Перейти строго в локальную часть FBeditor Next и прочитать `PROJECT_STATUS.md`, `CHANGELOG.md`, `docs/todo.md`, `docs/building.md` и `docs/release-checklist.md`.
2. Проверить `git status`.
3. Переходить к продуктовым пожеланиям.

