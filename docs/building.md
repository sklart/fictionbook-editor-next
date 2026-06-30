# Сборка

## Требования

- Visual Studio 2026 с компонентами разработки классических приложений на C++.
- Windows 10/11 SDK.

Собранные бинарники поддерживают Windows 7 SP1 и новее. Windows XP и Vista
больше не поддерживаются.

Исполняемый файл использует режим DPI awareness Per-Monitor V2 на современных
версиях Windows. Legacy-ветка манифеста сохраняет режим System DPI awareness на
Windows 7 и включает помониторное масштабирование там, где оно поддерживается,
например на Windows 8.1.

Режим «Код» использует цвета Windows High Contrast и обновляет редактор и
дерево документа при изменении системных цветов или темы.

Когда Windows завершает пользовательский сеанс, FBE синхронно записывает
аварийную копию. В режиме «Код» текущий UTF-8 XML-текст сохраняется даже в том
случае, если он временно невалиден.

Диагностические файлы падений ротируются при запуске. Сохраняются десять
последних событий, при этом пара файлов `.dmp` и `.txt` считается одним
событием.

При создании релиза отдельно формируется архив `*-symbols.zip` с PDB-файлами,
соответствующими опубликованным бинарникам. Символы не устанавливаются на
машины пользователей, поэтому этот архив нужно сохранять для анализа minidump
от той же версии.

Пользовательские настройки и словарь пользовательской орфографии по умолчанию
хранятся в `%LOCALAPPDATA%\FBE`. При первом запуске находящийся рядом с
исполняемым файлом `custom.dic` копируется туда. Явно заданные абсолютные пути
к словарям по-прежнему поддерживаются.

Установщик регистрирует встроенную type library FBE на уровне всей системы.
Portable-копия регистрирует её только для текущего пользователя и только если
подходящей регистрации ещё нет.

Документы сначала записываются во временный файл в целевой папке, затем
сбрасываются на диск и после этого подменяются через Windows API замены файла.
При замене существующего документа рядом также создаётся файл `.bak` с его
предыдущим содержимым.

Legacy runtime-зависимости, поставляемые только в бинарном виде, зафиксированы
в `dependencies/runtime-binaries.json`. После осознанной замены одного из этих
файлов нужно запускать `tools/build/verify-runtime-binaries.ps1`. Проверка
релиза также валидирует копии, лежащие рядом с `FBE.exe`.

Редактор исходного кода использует Scintilla 5.6.3 и Lexilla 5.5.0 из
`third_party`. Скрипт `tools/build/build-scintilla.ps1` собирает их x86 DLL с
статическим C++ runtime до сборки solution и выкладывает их в `runtime`.
Скрипт `tools/tests/test-scintilla.ps1` проверяет загрузку Lexilla и синтаксис
регулярных выражений, используемый в поиске и замене в режиме «Код». Он также
проверяет контролируемое поведение при отсутствии одной из DLL редактора.
Сборка релиза запускает этот тест автоматически.

Для контроля upstream-обновлений исходников `Scintilla`, `Lexilla`, `PCRE2`
и `Hunspell`
добавлены отдельные сервисные сценарии:

```powershell
.\tools\build\check-third-party-updates.ps1
.\tools\build\download-third-party-updates.ps1
.\tools\build\apply-third-party-update.ps1 -Dependency scintilla
.\tools\build\apply-third-party-update-and-test.ps1 -Dependency pcre2
.\tools\build\build-hunspell.ps1
```

Первый сценарий показывает локальную и удалённую версии и сообщает, где
доступно обновление. Второй скачивает ZIP-архивы исходников в
`tmp\third-party-updates` и раскладывает их по отдельным каталогам вида
`<dependency>-<version>` вместе с `download-metadata.json`.

При необходимости можно ограничить набор зависимостей:

```powershell
.\tools\build\check-third-party-updates.ps1 -Dependency scintilla,lexilla
.\tools\build\download-third-party-updates.ps1 -Dependency pcre2,hunspell
```

По умолчанию скачиваются только реально более новые версии. Если нужно
принудительно скачать даже уже текущую версию, используйте
`-AllowCurrentVersion`.

Сценарий `apply-third-party-update.ps1` берёт уже скачанный каталог из
`tmp\third-party-updates` (или явный `-SourcePath`), валидирует структуру
исходников, копирует новый вариант в staging, переносит текущую копию
`third_party\<dependency>` в backup и только после этого подменяет рабочий
каталог. Для безопасной примерки используйте `-WhatIf`.

Сценарий `apply-third-party-update-and-test.ps1` автоматизирует следующий шаг:
после раскладки обновления он запускает правильный build/test-контур для этой
зависимости. Сейчас поддерживаются:

- `scintilla` / `lexilla` -> `build-scintilla.ps1` + `test-scintilla.ps1`;
- `pcre2` -> `build-pcre2.ps1` + `test-pcre2.ps1` +
  `test-pcre2-wrapper.ps1` + `test-pcre2-replace.ps1`;
- `hunspell` -> `build-hunspell.ps1` + `test-spellcheck-dictionaries.ps1`.

Для безопасной dry-run проверки orchestration-потока также можно использовать
`-WhatIf`.

Если нужен единый вход на весь цикл `check -> download -> apply -> build ->
test`, используйте:

```powershell
.\tools\build\update-third-party-dependency.ps1 -Dependency scintilla
.\tools\build\update-third-party-dependency.ps1 -Dependency lexilla
.\tools\build\update-third-party-dependency.ps1 -Dependency pcre2
.\tools\build\update-third-party-dependency.ps1 -Dependency hunspell
```

Этот сценарий сам:

- сверяет локальную и upstream-версии;
- при необходимости скачивает ZIP-архив в `tmp\third-party-updates`;
- передаёт распакованный каталог в `apply-third-party-update-and-test.ps1`;
- запускает правильный build/test-контур для выбранной зависимости.

Полезные флаги:

- `-AllowCurrentVersion` — прогнать тот же цикл даже без новой upstream-версии;
- `-ForceDownload` — перескачать архив заново;
- `-ForceApply` — разрешить повторную раскладку даже для той же версии;
- `-WhatIf` — безопасно посмотреть, что именно сценарий собирается сделать.

Для GUI smoke-теста используйте
`tools/tests/test-fbe-startup.ps1 -Configuration Release`. Он ждёт завершения
legacy-инициализации MSHTML и проверяет, что восстановленное главное окно
остаётся видимым и отвечает на сообщения.

Ручной чек-лист регрессии орфографии находится в
`docs/spellcheck-regression.md`. Его стоит проходить после обновления
`Hunspell`, словарей или логики орфографии.

Ручной чек-лист регрессии поиска и замены находится в
`docs/regex-regression.md`. Его стоит проходить после изменений в
`Scintilla`, `Lexilla`, `PCRE2` или логике regex-поиска.

Канонический набор regex-кейсов хранится в `tools/tests/regex-fixtures.json`.
Его описание и правила пополнения находятся в `docs/regex-fixtures.md`.
Практические заметки по совместимости `PCRE 7.9` и `PCRE2` для FBE собраны в
`docs/pcre2-compatibility.md`.

Актуальный список отложенных и следующих задач хранится в `docs/todo.md`.
Карта активных тестовых контуров и их ролей собрана в
`docs/test-contours.md`: перед cleanup-ревизией `tools/tests` и перед
выборочным прогоном shell/regression-сценариев лучше сверяться именно с ней.

В `third_party/pcre2` подключён исходный код `PCRE2 10.47`.
Скрипт `tools/build/build-pcre2.ps1` собирает статическую x86-библиотеку
`pcre2-8` через CMake и выкладывает её в `build/pcre2/install/<Configuration>`.
Скрипт `tools/tests/test-pcre2.ps1` проверяет эту сборку на базовых fixture-кейсах.
Основной runtime FBE уже использует `PCRE2`, а legacy `PCRE 7.9` сохранён только
в `third_party/pcre` как внешний reference-контур для compare- и regression-тестов миграции.
Если `third_party/pcre/pcre.dll` отсутствует, legacy smoke- и compare-тесты
автоматически пропускаются, а стандартная релизная проверка опирается только
на основной `PCRE2`-контур.

- NSIS для создания установщика.

UPX 5.1.1 хранится в `tools/upx`. Hunspell подключён как Git submodule в
`third_party/hunspell`.

Для отдельной пересборки только статической библиотеки Hunspell используйте:

```powershell
.\tools\build\build-hunspell.ps1 -Configuration Release
```

## Команда

```powershell
.\tools\build\build.ps1 -Configuration Release -Platform Win32
```

Для CI с установленным Visual Studio 2022:

```powershell
.\tools\build\build.ps1 -PlatformToolset v143 -SkipUpx -WarningsAsErrors
```

Параметр `-WarningsAsErrors` запрещает появление новых предупреждений
компилятора. Предупреждения legacy-заголовков WTL подавлены только на границе
подключения сторонней библиотеки.

Скрипт ищет MSBuild через `vswhere.exe`. Ручной эквивалент:

```powershell
msbuild FBE.sln /m /t:Build /p:Configuration=Release /p:Platform=Win32
```

## Результат

- Бинарники и runtime-файлы: `out/Release`.
- Промежуточные объектные файлы: `build/obj`.
- Статическая библиотека Hunspell: `build/hunspell/lib/Release`.

Проверка и подготовка portable-артефакта:

```powershell
.\tools\build\verify-release.ps1
.\tools\build\package-portable.ps1
```

Основное приложение остаётся 32-битным, поскольку использует legacy-компоненты
Windows и существующую Win32-интеграцию.

## Диагностика запуска

При необходимости можно включить поэтапную трассировку запуска:

```powershell
$env:FBE_STARTUP_TRACE = "1"
.\out\Release\FBE.exe
Remove-Item Env:FBE_STARTUP_TRACE
```

Лог создаётся в `%LOCALAPPDATA%\FBE\startup-trace.log`. В обычном режиме
трассировка полностью выключена и файл не открывается.

Автоматический тест запуска с выводом этой трассы:

```powershell
.\tools\tests\test-fbe-startup.ps1 -Configuration Release -Trace
```

Быстрые проверки безопасности исходников без сборки и запуска приложения:

```powershell
.\tools\tests\test-source-safety.ps1
```

Smoke-тест внутреннего reader-а метаданных `.fb2` для будущей shell-интеграции:

```powershell
.\tools\tests\test-fb2-metadata.ps1
```

## Установщик

Официальный источник файлов для упаковки теперь такой:

- `out\Release` — сырые бинарники после сборки;
- `out\package\FictionBookEditor` — staging portable-пакета и основной вход
  для NSIS-установщика;
- `out\artifacts` — итоговые ZIP- и setup-артефакты релиза.

После сборки Release штатный сценарий сам формирует
`out\package\FictionBookEditor` и использует его напрямую как вход для NSIS:

```powershell
.\packaging\nsis\Installer\MakeInstaller.bat
```

Отдельно подготовить входные файлы установщика можно командой:

```powershell
.\tools\build\prepare-installer.ps1
```

Эта команда не собирает состав релиза вручную: она просто подтверждает, что
уже готовый portable staging в `out\package\FictionBookEditor` можно читать
напрямую из NSIS.

## Релизные артефакты

Полный локальный сценарий собирает проект, проверяет версии, создаёт portable ZIP,
NSIS-установщик и файл контрольных сумм:

```powershell
.\tools\build\create-release.ps1
```

Проверить состав уже созданных архивов и их контрольные суммы:

```powershell
.\tools\build\verify-artifacts.ps1 -Platform Win32
```

Результаты помещаются в `out\artifacts`:

- `FictionBookEditorNext-<version>-win32-portable.zip`;
- `FictionBookEditorNext-<version>-win32-setup.exe`;
- `FictionBookEditorNext-<version>-win32-symbols.zip`;
- `SHA256SUMS.txt`.

Для уже собранных бинарников используйте `-SkipBuild`. Перед публикацией релиза
добавьте `-ValidateUpdateManifest`, чтобы версия в `update.xml` совпадала с
`src\version.h`.
Тег вида `v3.0.0` запускает тот же сценарий в GitHub Actions. Перед публикацией
проверяются соответствие тега исходной версии и версия в `update.xml`, после чего
ZIP, установщик и контрольные суммы прикрепляются к GitHub Release.

Практический пошаговый сценарий выпуска собран в
[docs/release-checklist.md](D:\Download\FBeditor\docs\release-checklist.md).


