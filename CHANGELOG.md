# Changelog

Все заметные изменения локальной ветки модернизации FictionBookEditor фиксируются в этом файле.

Формат основан на принципах [Keep a Changelog](https://keepachangelog.com/ru/1.1.0/).

## Unreleased

### Добавлено

- В общий контур сторонних зависимостей добавлена WTL: `check-third-party-updates.ps1`,
  `download-third-party-updates.ps1` и `update-third-party-dependency.ps1` теперь
  умеют проверять SourceForge и работать с WTL 10.0.1.
- В README добавлено описание поддерживаемых версий Windows: основной целевой
  контур — Windows 10/11, Windows 7 SP1/8.1 остаются ожидаемо совместимыми, а
  Windows XP/Vista не входят в стандартную сборку Next.
- Проект подготовлен к публикации как `FictionBook Editor Next 3.0.0`: обновлены имя продукта, ссылка в окне `О программе`, URL проверки обновлений, GitHub release URL, имена релизных артефактов `FictionBookEditorNext-*`, публичный `README.md`, техническое описание отличий от `sensboston/fictionbookeditor` и GitHub Actions workflow с человекочитаемыми release notes.
- `ImportEPUBLunaSVG.dll` перенесён в подсекцию `ImportEPUB` установщика как дополнительная библиотека для преобразования SVG-обложек EPUB в PNG/JPEG; компонент теперь входит в portable/setup/symbols release-gate вместе с `ImportEPUB`.
- Добавлен curated файл заметок релиза `docs/release-notes/3.0.0.md`; `tools/build/new-release-notes.ps1` использует его для GitHub Release и добавляет ссылку на коммиты, если workflow запущен по тегу.
- В пользовательских заметках релиза подробнее описаны обновления внутренних компонентов: Hunspell для проверки орфографии, PCRE2 вместо устаревшего PCRE и переход от исторического SciLexer к современной связке Scintilla + Lexilla, включая ожидаемые пользовательские преимущества — совместимость со словарями, более поддерживаемые регулярные выражения и более надёжную основу source-view.
- Исправлен запуск FBE с относительным путём к файлу из командной строки: стартовый аргумент теперь нормализуется в полный путь до загрузки документа, FileAge/MRU и fallback в source-view используют один и тот же путь.
- В установщик добавлена необязательная секция `Batch-конвертеры`: она устанавливает консольные `ExportDOCXBatch.exe`, `ExportEPUBBatch.exe`, `ImportEPUBBatch.exe` вместе с обязательными DLL-зависимостями `ExportDOCX.dll`, `ExportEPUB.dll`, `ImportEPUB.dll`; optional `ImportEPUBLunaSVG.dll` включается только если собран. Batch-проекты подключены к build/package/release-gate, а portable/symbols/setup-артефакты проверяются с учётом новых файлов.
- Исправлены кракозябры в ExportEPUB: проект плагина и диагностический batch-runner теперь компилируются с `/utf-8`, поэтому русские C++-строки корректно попадают в окна плагина, XHTML/NCX navigation и `<title>` внутри EPUB. В `verify-release.ps1` добавлен регрессионный тест `test-export-epub-cyrillic.ps1`, проверяющий кириллический EPUB без mojibake.
- Исправлен UX/data-loss сценарий с read-only FB2: при обычном сохранении FBE теперь заранее предупреждает, что файл доступен только для чтения, и предлагает сохранить копию через `Сохранить как...`; отказ от сохранения не сбрасывает dirty-state.
- Закрыт release-блокер по symbols ZIP: `verify-artifacts.ps1` теперь ожидает PDB новых плагинов `ExportDOCX`, `ExportEPUB` и `ImportEPUB`, синхронно с тем, что кладёт `create-release.ps1`; проверка текущего `out/artifacts` проходит.
- Зафиксированы итоги ручного тестирования от 2026-06-29 в `docs/manual-test-findings-2026-06-29.md`: базовый smoke/regression FBE пройден с замечаниями, отдельно выделены release-блокер по symbols ZIP, проблемы ExportEPUB/ImportEPUB и UX-хвосты.
- Добавлена программа ручного тестирования `docs/manual-test-plan.md` с чеклистами `Да` / `Нет` / `Замечания` для проверки FBE, FBV, shell-интеграции, installer/uninstaller и плагинов импорта/экспорта.
- Обновлено изображение, используемое в инсталяторе, на более качественную версию.
- В сборку FBE подключены новые высококачественные иконки приложения `FBE2_new.ico` и `FBE_new.ico`; старые `.ico` оставлены в дереве как резервные исходники.
- В `ExportEPUB` интегрирован корректирующий пакет для EPUB 2/XHTML 1.1: прямые текстовые фрагменты внутри `div`/`blockquote` оборачиваются в `p.inline-fragment`, `cite`/`epigraph` гарантируют блочное содержимое, блочные элементы больше не вкладываются внутрь `<a>`, локальные `file://` ссылки отбрасываются, а HTTP/HTTPS-ссылки с обратными слэшами нормализуются.
- В `ExportEPUB` заменён рискованный MSXML-декодер `bin.base64` на собственный безопасный base64-декодер: корректные `binary` с пробелами и переносами строк экспортируются штатно, а повреждённые неиспользуемые binary больше не валят весь экспорт.
- В batch-диагностику `ExportEPUB` добавлены стадии обработки (`loading FB2`, `setting MSXML namespaces`, `building EPUB model`, `applying export settings`, `running preflight`, `writing EPUB`, `writing diagnostic log`), чтобы ошибки пакетной конвертации было проще локализовать.
- Релизная проверка усилена новыми smoke-тестами `test-export-epub-xhtml11.ps1` и `test-plugin-mojibake.ps1`: первый покрывает EPUB 2/XHTML 1.1, `file://`, нормализацию HTTP-ссылок и base64 с переносами, второй проверяет пользовательские строки плагинов на типичные признаки кракозябр. Из `verify-release.ps1` удалены устаревшие legacy PCRE smoke-вызовы; релизный контур теперь проверяет только актуальный PCRE2 и отсутствие `pcre.dll` в runtime.
- Clean CI-сборка больше не зависит от локально подготовленного `build\hunspell`: `build.ps1` сам подготавливает Hunspell перед сборкой solution, `build-hunspell.ps1` создаёт минимальный служебный `libhunspell.vcxproj` при его отсутствии, а `build-pcre2.ps1` выбирает CMake generator по `PlatformToolset` (`v143` → Visual Studio 2022 для GitHub Actions, локально сохраняется VS 2026).
- Обновлён встроенный комплект WTL с исторической версии 8.1 до WTL 10.0.1;
  в список проектных заголовков добавлены новые `atldwm.h` и `atlribbon.h`.
  Обновление не меняет пользовательский сценарий напрямую, но убирает часть
  технического долга старых UI-заголовков: окна, меню, панели инструментов и
  диалоги теперь собираются на более свежей WTL-основе, лучше совместимой с
  современными ATL/Visual Studio и будущими UI-доработками.
- Обновление WTL 10.0.1 проверено автоматическим smoke-контуром: FBE startup,
  FBV fixture, FBShell loader, shell-surface diagnostics, ExportEPUB regression,
  plugin mojibake check, полный `build.ps1` и `verify-release.ps1`.
- Проверка обновлений в окне `О программе` стала устойчивее к GitHub/raw-ответам
  без явного `Content-Length`: манифест `update.xml` больше не должен ошибочно
  показывать `Ошибка загрузки` только из-за формата HTTP-ответа.
- По свежему `D:\FBE.plog` подтверждено снижение PVS-отчёта `FBE` до 175 сообщений: `V809` ушёл полностью, Level 2 отсутствует, Level 1 содержит только два лицензионных `V1042`. Закрыт безопасный пакет `V821`: временные COM/BSTR-переменные в `FBEview.cpp`, `mainfrm.cpp` и `XMLSerializer.cpp` перенесены в более узкие области видимости без изменения пользовательской логики. Подтверждены `build.ps1 -Configuration Release -Platform Win32 -SkipUpx` и `verify-release.ps1 -Configuration Release`; для прогона использовано отключение MSBuild node reuse, чтобы избежать повторного PDB-lock после зависшего build-узла.
- По свежему `D:\FBE.plog` подтверждено снижение PVS-отчёта `FBE` до 185 сообщений: `V815` и `V801` ушли полностью, Level 2 отсутствует, Level 1 по-прежнему содержит только два лицензионных `V1042`. Закрыт следующий безопасный пакет `V809`: убраны избыточные проверки перед `delete`/`free` в `FBEview.h`, `extras/ColorButton.*`, `extras/http_download.h`, `mainfrm.cpp` и `Speller.cpp` без изменения владения объектами. Подтверждены `build.ps1 -Configuration Release -Platform Win32 -SkipUpx` и `verify-release.ps1 -Configuration Release`.
- По новому `D:\FBE.plog` подтверждено снижение PVS-отчёта `FBE` до 194 сообщений: Level 2 отсутствует, Level 1 содержит только два лицензионных `V1042` по `xmlMatchedTagsHighlighter.*`. Закрыт следующий безопасный хвост `V815`/`V801`: инициализация и проверки `CString` в `FBEview.h`/`SettingsHotkeysDlg.cpp` переведены на `CString()`/`IsEmpty()`, а оставшиеся `CString`-аргументы в `Settings`, `Splitter` и `utils` передаются по `const &`. Подтверждены `build.ps1 -Configuration Release -Platform Win32 -SkipUpx` и `verify-release.ps1 -Configuration Release`.
- Закрыт следующий строковый PVS-пакет Level 3 в `src/fbe`: очевидные `CString`-очистки переведены на `Empty()`, проверки пустоты — на `IsEmpty()`, `AboutBox::PrepareHeader()` теперь принимает URL по `const CString&`, а повторный расчёт текста горячей клавиши в `SettingsHotkeysDlg.cpp` заменён локальным кэшем. Также восстановлена корректная однобайтная кодировка `FBEview.h` после ошибочного BOM. Подтверждены `build.ps1 -Configuration Release -Platform Win32 -SkipUpx` и `verify-release.ps1 -Configuration Release`.
- Закрыт следующий PVS-пакет Level 3: оставшиеся `V1003` в `apputils.h` и `atlctrlsext.h`, а также текущие `V559` в `GLLogo.cpp`, `Utils.cpp` и `TreeView.cpp`. Присваивания в условиях вынесены в явные шаги без изменения обхода, а в `CGLLogoView::OnTimer()` исправлен реальный дефект `if (wParam = m_Timer)` на сравнение `if (wParam == m_Timer)`. Подтверждены `build.ps1 -Configuration Release -Platform Win32 -SkipUpx` и `verify-release.ps1 -Configuration Release`.
- Закрыт следующий безопасный пакет PVS Level 3 в `src/fbe`: арифметические resource-макросы в `res1.h` обёрнуты в скобки (`V1003`), а очевидные `CString::Format` с unsigned/DWORD-аргументами приведены к ожидаемому signed-типу (`V576`) в `SearchReplace.h`, `FBDoc.cpp`, `Settings.cpp`, `SettingsOtherDlg.cpp` и `mainfrm.cpp`. Подтверждены `build.ps1 -Configuration Release -Platform Win32 -SkipUpx` и `verify-release.ps1 -Configuration Release`.
- По повторному `D:\FBE.plog` подтверждено снижение до 238 предупреждений: Level 1 — только два лицензионных `V1042` по `xmlMatchedTagsHighlighter.*`, Level 2 — один `V802`. `V802` закрыт перестановкой полей внутренней структуры `CFBEView::FindReplaceOptions` без изменения поведения; `build.ps1 -Configuration Release -Platform Win32 -SkipUpx` и `verify-release.ps1 -Configuration Release` прошли.
- По свежему `D:\FBE.plog` закрыт безопасный пакет PVS Level 1/2 в `src/fbe`: `FCCrypt::Get_SHA256()` переведён с deprecated CryptoAPI на CNG/BCrypt, `SetCurrentDirectoryToFile()` — с `PathRemoveFileSpecW` на `PathCchRemoveFileSpec`, исправлены mojibake Unicode-shortcut в `ModelessDialog.h`, убрана лишняя локальная копия toolbar buttons и объединены одинаковые ветки `Simple`/`SimpleList` в `XMLSerializer`. Подтверждены `build.ps1 -Configuration Release -Platform Win32 -SkipUpx` и `verify-release.ps1 -Configuration Release`.
- Закрыта build-гигиеническая проблема старого `FBV.exe`: `FBE.vcxproj` больше не копирует `runtime\FBV.exe` поверх свежего релизного валидатора, `build.ps1` после общего solution build явно пересобирает `src/fbv/FBV.vcxproj` и `src/import-epub/ImportEPUB.vcxproj`, а release-gate проверяет локально собираемый `FBV.exe` без нестабильной SHA-256-привязки. Подтверждено удалением `out\Release\FBV.exe`/`ImportEPUB.dll`, повторной сборкой и успешным `verify-release.ps1 -Configuration Release`.
- По свежему `D:\FBE.plog` после малого PVS-пакета подтверждено снижение до 269 сообщений: Level 1 — 10, Level 2 — 24, Level 3 — 235; `V817` ушёл полностью, а `V807`/`V815` остались только как Level 3.
- Закрыта безопасная часть `V801` в descriptor-контуре `src/fbe`: функции `ElementDescriptor`/`ElementDescMnr` и `CFBEView::GetRangePos()` больше не копируют тяжёлые COM smart pointers и `CString` в сигнатурах, а принимают их по `const &` без изменения поведения.
- После `V801`-пакета подтверждены `build.ps1 -Configuration Release -Platform Win32 -SkipUpx` и `verify-release.ps1 -Configuration Release`; актуальный `FBV.exe` снова восстановлен прямым `Rebuild` из-за известной проблемы общего build.
- По новому `D:\FBE.plog` подтверждено, что после `FbeLoadString()` предупреждения `V530` ушли полностью; отчёт снизился до 275 сообщений: Level 1 — 12, Level 2 — 28, Level 3 — 235.
- Закрыт следующий небольшой безопасный PVS-пакет в `src/fbe`: `CString`-очистка заменена на `Empty()`, односимвольный поиск XML-тега — на `wcschr`, повторные обращения к `g_desc_elements`, hotkey-группам и `Document()->all` закэшированы локально без изменения пользовательской логики.
- После пакета подтверждены `build.ps1 -Configuration Release -Platform Win32 -SkipUpx` и `verify-release.ps1 -Configuration Release`; сохраняющаяся проблема общего build с устаревшим `FBV.exe` снова обойдена прямым `Rebuild` проекта `src/fbv`.
- По актуальному `D:\FBE.plog` закрыт крупный безопасный блок PVS `V530` в `src/fbe`: добавлена общая обёртка `FbeLoadString()` для строковых ресурсов, прямые вызовы `LoadStringW` в собственном коде переведены на неё или на `CString::LoadString`, а участок `FillMenuWithHkeys()` дополнительно очищен от повторных `at()` и некорректного освобождения `CString`-буфера после `GetMenuString`.
- После пакета подтверждены `build.ps1 -Configuration Release -Platform Win32 -SkipUpx` и `verify-release.ps1 -Configuration Release`; из-за сохраняющейся build-гигиенической проблемы общий build снова потребовал прямой `Rebuild` актуального `FBV.exe` и обновление `dependencies/runtime-binaries.json`.
- По новому `D:\FBE.plog` после укрупнённого PVS-пакета количество предупреждений снизилось до 337: Level 1 — 15, Level 2 — 86, Level 3 — 236. Дополнительно закрыты механические хвосты: `m_selBandID`, `pElAdjacent`, `extras/http_download.h`, `Speller`, оставшиеся `Tokenize(...).IsEmpty()`, кэширование длины image-title и word-reference в `Words.cpp`.
- После второго укрупнённого PVS-пакета снова подтверждены `build.ps1 -Configuration Release -Platform Win32 -SkipUpx` и `verify-release.ps1 -Configuration Release`; актуальные `FBV.exe` и `ImportEPUB.dll` перед release-gate восстановлены прямыми `Rebuild` из-за сохраняющейся build-гигиенической проблемы общего build.
- Укрупнён следующий механический PVS-пакет в `src/fbe`: простые конструкторы переведены на initializer lists / `= default`, точечные контейнерные вставки заменены на `emplace(_back)`, часть строковых проверок переведена на `IsEmpty()`, итераторы — на prefix increment/decrement, а `wcslen(MyStr.c_str())` заменён на `MyStr.length()` там, где это не меняет поведение.
- Дополнена инициализация `CMainFrame` и `CGLLogoView`: заданы начальные значения view-state, file-age, статусных строк, OpenGL texture/shadow buffers. После пакета штатный `build.ps1 -Configuration Release -Platform Win32 -SkipUpx` и `verify-release.ps1 -Configuration Release` проходят.
- По свежему `D:\FBE.plog` закрыт следующий небольшой пакет PVS-замечаний в `src/fbe`: проверен результат `LoadKeyboardLayout`, явно инициализированы поля `CMainFrame`/`COptDlg`, а `CGLLogoView` получил безопасные начальные значения Win32/OpenGL-ресурсов и ASCII-имя `bCrystallize` вместо исторического идентификатора с не-ASCII символом.
- После PVS-пакета подтверждены `build.ps1 -Configuration Release -Platform Win32 -SkipUpx` и `verify-release.ps1 -Configuration Release`; для корректного release-gate дополнительно пересобраны актуальные `FBV.exe` и `ImportEPUB.dll` прямыми проектными сборками.
- В `src/fbe` закрыта первая безопасная волна функциональных PVS-замечаний из свежего отчёта: убрано лишнее присваивание `BSTR` в `FBEview.cpp`, усилен путь `SciSelection()` при пустом/ошибочном UTF-8 преобразовании, упрощён цикл чтения UTF-8 символа в `DisplayCharCode()`, а `XMLSerializer` теперь явно обрабатывает `PropertyType::Blank` во всех трёх switch-блоках.
- После пересборки обновлена контрольная сумма актуального `FBV.exe` в `dependencies/runtime-binaries.json`; `verify-release.ps1 -Configuration Release` снова проходит на Win32 Release.
- После повторного PVS-прогона малых проектов нормализован `ExportDOCX.vcxproj`: прямой build больше не зависит от `$(SolutionDir)`, выводит DLL/PDB в общий `out\Release`, а служебные `None`-элементы проекта приведены к обычной структуре до `Microsoft.Cpp.targets`.
- В `ImportEPUB` закрыты безопасные PVS-замечания `V823` и `V821`: `set::insert(std::wstring(...))` заменён на `emplace(...)`, а временный id heading-узла ограничен нужной веткой без изменения логики extra markers.
- Закрыта проблема попадания старого `FBV.exe` в installer/package: `FBV` теперь имеет общий VersionInfo `2.7.9`, собирается с Control Flow Guard, включён в обязательные release-проверки вместе с `FBV.pdb`, а baseline `dependencies/runtime-binaries.json` обновлён на локально собранный Win32 Release-бинарник.
- Исправлен прямой build-путь `ImportEPUB.vcxproj`: проект больше не зависит от `$(SolutionDir)` для `OutDir`/`IntDir`, поэтому при сборке самого `.vcxproj` DLL и PDB попадают в общий `out\Release`, а не в локальный `src\import-epub\out`.
- Интегрированы новые plugin-проекты в общий `FBE.sln` и release/build-контур:
  - `src/export-docx/ExportDOCX.vcxproj`;
  - `src/export-epub/ExportEPUB.vcxproj` — только подключён к общей сборке и пакету, исходники работающего плагина не менялись;
  - `src/import-epub/ImportEPUB.vcxproj`.
- Portable/release-скрипты теперь учитывают `ExportDOCX.dll`, `ExportEPUB.dll` и `ImportEPUB.dll`, проверяют их наличие в staging/release-пакете и включают соответствующие `.pdb` в symbol-package.
- `ImportEPUB.dll` выровнен по общей версии продукта `2.7.9`, а промежуточные `.lib/.exp` новых DLL-плагинов добавлены в штатную очистку `out\Release` после сборки.
- Продолжен точечный аудит `src/fbe` без архитектурных перестроек:
  - `src/fbe/FBDoc.cpp`: `TransformXML()` теперь проверяет ошибки
    `CreatePipe` / `CreateThread` и корректно освобождает ресурсы при сбое;
  - `src/fbe/FBDoc.cpp`: SAX/DOM-путь больше не использует ручной
    `malloc/free` для основного UTF-8 буфера, а переведён на безопасный
    `std::vector<char>` с явной обработкой `out-of-memory`;
  - `src/fbe/FBDoc.cpp`: временный wide-string cleanup в `GetSavedPos()`
    упрощён без ручного `new[]/delete[]`.
  - `src/fbe/mainfrm.cpp`: локальные временные буферы в `SciSelection()` и
    в пути синхронизации source-view переведены с ручного `malloc/free` на
    `std::vector<char>`;
  - `src/fbe/FBEview.cpp`: временный UTF-8 буфер поиска в Scintilla тоже
    переведён с `malloc/free` на `std::vector<char>` без изменения
    пользовательской логики поиска.
- Добавлен отдельный диагностический сценарий
  `tools/tests/diagnose-fbe-property-schema-baseline.ps1`, который помогает
  разбирать «грязный baseline» published FBE-specific свойств: показывает
  состояние repo/shared schema-файлов, related shell-registration и набор
  `PSGetPropertyDescriptionByName` до и после попытки снять регистрацию
  repo-схемы.
- Добавлен документ `docs/test-contours.md` с картой тестовых контуров `tools/tests`: что относится к базовому release-gate, что к shell/thumbnail-диагностике, что к Validate/FBV и что остаётся legacy/reference-only набором для безопасной дальнейшей cleanup-ревизии.
- Добавлен документ `docs/upstream-evpobr-analysis.md` с разбором
  reference-форка `evpobr/fictionbookeditor`: какие старые изменения уже
  учтены в нашей ветке, какие полезны только как исторический reference и
  какие не стоит переносить из-за принятых решений по modern shell-контуру.

- Добавлены сервисные сценарии автоматической проверки и скачивания
  upstream-исходников сторонних зависимостей:
  - `tools/build/ThirdPartySources.ps1` — общий helper для локальных версий,
    удалённых источников и распаковки архивов;
  - `tools/build/check-third-party-updates.ps1` — проверка обновлений
    `Scintilla`, `Lexilla`, `PCRE2`, `Hunspell`;
  - `tools/build/download-third-party-updates.ps1` — скачивание ZIP-архивов
    новых версий в `tmp/third-party-updates`;
  - `tools/build/apply-third-party-update.ps1` — безопасная раскладка уже
    скачанного обновления в `third_party` через staging и backup;
  - `tools/build/apply-third-party-update-and-test.ps1` — orchestration-
    сценарий, который после раскладки зависимости сразу запускает
    связанный build/test-контур;
  - `tools/build/update-third-party-dependency.ps1` — единая точка входа на
    полный цикл `check -> download -> apply -> build -> test`;
  - `tools/build/build-hunspell.ps1` — отдельная пересборка
    `libhunspell.vcxproj` без полного прогона всего solution.
- Добавлен отдельный XSD-валидный FB2-fixture для проверки shell-команды
  `Validate` и локально восстановленного `FBV.exe`:
  `tools/tests/fb2-validate-valid-smoke.fb2`.
- Добавлен pre-flight smoke-скрипт `tools/tests/test-fbv-fixture.ps1`,
  который валидирует FB2-fixture по штатным схемам из `runtime` перед
  ручной проверкой `FBV`.
- В локальную кодовую базу импортированы исходники `FBV` из upstream
  `evpobr/fictionbookeditor` в `src/fbv`, чтобы `Validate` больше не зависел
  от исторического внешнего бинарника без исходников.
- Добавлен сводный smoke-сценарий
  `tools/tests/test-fb2-shell-thumbnail-matrix.ps1`, который одним прогоном
  покрывает прямой COM thumbnail provider, системную COM-активацию,
  shell API на валидных `png/jpeg/bmp` fixture и негативные forced-сценарии
  для битой/неполной обложки.
- Добавлен второй эталонный FB2-fixture для shell-диагностики реальных книг:
  `tools/tests/fb2-metadata-cyrillic-smoke.fb2` с кириллицей, несколькими
  `genre`, заполненными `keywords`, `sequence` и `document-info`, чтобы
  отдельно проверять published shell-свойства на русскоязычном содержимом.
- Для experimental thumbnail provider добавлены системные shell-проверки:
  - регистрация thumbnail handler теперь готовит ветку не только для `.fb2`,
    но и для `FictionBook.2`, чтобы проверить более совместимый путь для
    Проводника Windows;
  - в `tools/tests/check-fb2-shell-surfaces.ps1` добавлена отдельная
    проверка thumbnail ShellEx на `FictionBook.2`;
  - добавлен smoke-тест `tools/tests/test-fb2-thumbnail-provider-cocreate.ps1`,
    который проверяет активацию thumbnail provider через `CoCreateInstance`
    в разрядности `x64`;
  - добавлен `tools/tests/test-fb2-shell-thumbnail.ps1`, который проверяет,
    что shell API Windows реально получает thumbnail для `.fb2`;
  - добавлен `tools/build/reset-explorer-thumbnail-cache.ps1` для
    управляемого сброса thumbnail cache Проводника во время GUI-проверок.

- Полный комплект `NSIS UAC` перенесён в `third_party\uac`, а
  `prepare-installer.ps1` теперь автоматически синхронизирует рабочие
  `UAC.nsh` / `UAC.dll` в `packaging\nsis\NSIS` перед сборкой установщика.
- Добавлен сервисный сценарий принудительного прогрева shell thumbnail cache
  для проблемных `.fb2`, которые застряли на старом пустом результате
  Проводника:
  `tools/build/prime-fb2-thumbnail-cache.ps1`.
- Добавлена отдельная smoke-обвязка forced/prime-сценария thumbnail cache:
  `tools/tests/fb2-shell-thumbnail-prime-smoke.cpp`,
  `tools/tests/test-fb2-shell-thumbnail-prime.ps1`.
- Experimental thumbnail provider для `.fb2` доведён до реально собираемого и
  автоматически проверяемого COM-контура:
  `src/fbshell/Fb2ThumbnailProvider.*`,
  `src/fbshell/AtlImageStatics.cpp`.
- Для modern `cover / thumbnail extraction` добавлены fixture-файлы и
  smoke-проверки не только для `png`, но и для `jpeg` и `bmp`, плюс для
  негативных сценариев:
  - `tools/tests/fb2-cover-jpeg-smoke.fb2`
  - `tools/tests/fb2-cover-bmp-smoke.fb2`
  - `tools/tests/fb2-cover-missing-coverpage.fb2`
  - `tools/tests/fb2-cover-missing-binary.fb2`
  - `tools/tests/fb2-thumbnail-provider-smoke.cpp`
  - `tools/tests/test-fb2-thumbnail-provider.ps1`
- Отдельный рабочий план по modern `cover / thumbnail extraction`:
  `docs/thumbnail-provider-plan.md`.
- В релизный контур добавлена отдельная упаковка modern property handler для
  обеих архитектур Проводника:
  - `FBShell.dll` для `Win32 Explorer`;
  - `FBShell64.dll` для `64-bit Explorer`.
  Изменения затрагивают:
  `tools/build/create-release.ps1`,
  `tools/build/package-portable.ps1`,
  `tools/build/verify-artifacts.ps1`.
- В NSIS-установщик добавлена штатная попытка регистрации modern property
  handler:
  - `FBShell.dll` на `x86` системах;
  - `FBShell64.dll` на `x64` системах;
  с записью в `PropertySystem\PropertyHandlers` и корректным снятием
  регистрации при деинсталляции.
- Внутренний reader метаданных `.fb2` без зависимости от legacy shell API:
  `src/fbe/Fb2Metadata.h`, `src/fbe/Fb2Metadata.cpp`.
- Внутренняя модель shell-свойств `.fb2` с явными статусами маппинга:
  `src/fbe/Fb2ShellProperties.h`, `src/fbe/Fb2ShellProperties.cpp`.
- Явный слой MVP-свойств для будущего modern property handler:
  - признаки `UsesWindowsProperty(...)`,
    `IsConfirmedWindowsProperty(...)`,
    `IsMvpWindowsProperty(...)`;
  - перечисление MVP-дескрипторов;
  - прямой маппинг `PropertyId -> PROPERTYKEY` для
    `Author`, `Title`, `Language` в `src/fbshell/Fb2PropertyKeys.*`.
- Внутренний кэш значений будущего `IPropertyStore` для MVP-полей:
  `src/fbshell/Fb2PropertyStore.*`.
- Тестируемый COM-каркас будущего modern property handler:
  `src/fbshell/Fb2PropertyStoreCom.*`
  с поддержкой `IPropertyStore`, `IPropertyStoreCapabilities` и
  `IInitializeWithStream`.
- Отдельный экспериментальный класс `Fb2PropertyHandler` и его регистрационная
  заготовка:
  `src/fbshell/Fb2PropertyHandler.*`,
  `src/fbshell/Fb2PropertyHandler.rgs`.
- Compile-time режим для отдельной experimental-сборки modern property
  handler:
  `tools/build/build-experimental-property-handler.ps1`,
  `docs/experimental-property-handler.md`.
- Скрипт `build-experimental-property-handler.ps1` переведён на сборку только
  `src/fbshell/FBShell.vcxproj`, чтобы experimental-контур не зависел от
  отдельных старых ошибок вне `FBShell`.
- Отдельные скрипты регистрации и отката experimental property handler, плюс
  чеклист ручной GUI-проверки:
  `tools/build/register-experimental-property-handler.ps1`,
  `tools/build/unregister-experimental-property-handler.ps1`,
  `docs/experimental-property-handler-manual-test.md`.
- Experimental property handler переведён на `ThreadingModel=Both`, а сценарий
  ручной регистрации дополнен ассоциацией `.fb2` через
  `PropertySystem\PropertyHandlers`.
- Smoke-тесты для чтения метаданных и shell-property-модели:
  `tools/tests/fb2-metadata-smoke.cpp`,
  `tools/tests/test-fb2-metadata.ps1`,
  `tools/tests/fb2-metadata-smoke.fb2`.
- Документация по modern shell-контуру и маппингу свойств:
  `docs/fbshell-roadmap.md`,
  `docs/fb2-property-mapping.md`,
  `docs/shell-extension-modernization.md`.
- Актуальный рабочий backlog:
  `docs/todo.md`.

### Изменено

- `tools/tests/test-fbe-specific-installer.ps1` теперь явнее различает
  обычную недоочистку shell-контура и residual machine baseline Property
  System: если после деинсталляции `PropertyHandlers\.fb2`, `ShellEx` и
  `CLSID` уже сняты, а `FBE.*` всё ещё published, smoke пишет
  специализированное warning-пояснение вместо почти-ошибочного общего
  впечатления о регрессии setup/uninstall.
- Документация по сборке и релизному контуру теперь явно ссылается на
  `docs/test-contours.md`, а третья cleanup-волна `tools/tests` зафиксировала,
  что после удаления старых `IThumbnailCache` smoke-файлов и пары
  `cover-inspect` оставшиеся малоссылочные сценарии всё ещё несут
  диагностическую или reference-нагрузку и не должны удаляться без
  отдельного решения по сужению покрытия.
- Experimental thumbnail provider доведён до более предсказуемого
  shell-поведения:
  - `Fb2CoverThumbnail` больше не рисует искусственный квадратный canvas с
    маленькой обложкой по центру, а возвращает bitmap, масштабированный под
    запрошенный размер с сохранением пропорций;
  - `tools/tests/test-fb2-thumbnail-provider-cocreate.ps1` и
    `fb2-thumbnail-provider-cocreate-smoke.cpp` дополнены checkpoint-
    диагностикой и отладочной сборкой для разбора падений после успешного
    `GetThumbnail`;
  - `tools/tests/test-fb2-shell-thumbnail-dump.ps1` теперь корректно
    нормализует относительные пути входного `.fb2` и выходного `.bmp`,
    поэтому forced dump над реальными книгами больше не ломается из-за
    внутреннего `Push-Location`.

- Наведён порядок в файловой структуре репозитория: исторические локальные
  каталоги сборки внутри src переведены в игнорируемые артефакты, чтобы
  рабочее дерево не засорялось старыми 	log, 
ecipe, pdb, dll и
  промежуточными объектами Visual Studio.
- `tools/tests/test-fb2-shell-thumbnail.ps1` и
  `tools/tests/test-fb2-shell-thumbnail-prime.ps1` теперь нормализуют путь к
  fixture-файлу и корректно работают не только с абсолютными, но и с
  относительными путями от корня репозитория.
- В документации по thumbnail provider зафиксировано, что `prime`-сценарий
  остаётся диагностическим шагом, а не обязательным release-gate, потому что
  `IThumbnailCache` может отказывать на части систем даже при исправно
  работающем обычном shell thumbnail-пути.

- `tools/build/prepare-installer.ps1` теперь не только синхронизирует
  NSIS/UAC-ресурсы, но и автоматически пересобирает `Win32`/`x64` modern
  shell binaries перед упаковкой installer staging, а затем валидирует
  `out\package\FictionBookEditor` через `verify-package-stage.ps1`.
- `packaging/nsis/Installer/MakeInstaller.bat` отвязан от текущей рабочей
  папки: все пути теперь считаются от каталога самого bat-файла, а запуск
  PowerShell идёт через `pwsh.exe` с fallback на `powershell.exe`, чтобы
  ручная сборка setup из корня репозитория не ломалась на относительных путях
  и UTF-8-скриптах.
- Smoke-проверка `tools/tests/test-fbe-specific-installer.ps1` приведена к
  актуальной модели shell-установки: `FBE.Sequence.propdesc` теперь
  проверяется в `C:\ProgramData\FictionBook Editor\Shell`, а не в `$INSTDIR`.
- Диагностическая утилита `tools/tests/fb2-shell-properties-query.cpp`
  переведена на явный UTF-8 вывод и теперь печатает `<пусто>` для пустых
  строковых shell-свойств, чтобы не терять последующий дамп на проблемных
  реальных книгах.
- Диагностика `tools/tests/check-fb2-shell-surfaces.ps1` теперь проверяет не
  только `ShellEx`-ветки thumbnail provider, но и наличие его
  `CLSID\...\InprocServer32`, чтобы не считать shell-интеграцию исправной при
  отсутствующей COM-активации.
- Штатный installer-helper `tools/build/register-modern-property-handler.ps1`
  теперь регистрирует не только `PropertySystem\PropertyHandlers\.fb2`, но и
  thumbnail provider через `ShellEx` для `.fb2` и `FictionBook.2`, чтобы
  обычная установка не отставала от experimental shell-контура.
- Парный helper `tools/build/unregister-modern-property-handler.ps1`
  теперь снимает не только property handler, но и thumbnail provider
  регистрации для `.fb2` / `FictionBook.2`, а также жёстче валидирует
  очистку modern shell-веток при деинсталляции.
- Smoke-проверка `tools/tests/test-fbe-specific-installer.ps1` расширена:
  теперь она подтверждает, что `setup.exe` и `uninst.exe` ставят и снимают
  не только `PropertyHandlers\.fb2` и FBE-specific schema, но и thumbnail
  provider для `.fb2` и `FictionBook.2`, включая COM-регистрацию CLSID.

- `tools/build/reset-explorer-thumbnail-cache.ps1` теперь сбрасывает не только
  `thumbcache`, но и `iconcache`, чтобы clean-room проверка shell-интеграции
  начиналась с действительно чистого состояния Проводника.

- Регистрация modern property handler в NSIS-установщике больше не вызывается
  напрямую из самого `MakeInstaller.nsi`: `regsvr32` сохранён, но перенесён в
  отдельные helper-скрипты `InstallerTools\register-modern-property-handler.ps1`
  и `InstallerTools\unregister-modern-property-handler.ps1`, чтобы shell-
  интеграция выполнялась в том же стабильном PowerShell-контуре, что и
  регистрация property schema.
- Экспериментальный shell-контур больше не складывает отдельные сборочные
  результаты в `tmp\property-handler`: обе архитектуры теперь собираются в
  `out\package\shell-build`, откуда затем напрямую попадают в staging-пакет
  `out\package\FictionBookEditor`.
- Обновлено изображение схемы/иллюстрации в инсталляторе:
  `packaging/nsis/res/Fb2-shema.bmp`.
- Experimental shell-скрипты и диагностика теперь умеют работать не только с
  modern property handler, но и с thumbnail ShellEx:
  `tools/build/build-experimental-property-handler.ps1`,
  `tools/build/register-experimental-property-handler.ps1`,
  `tools/build/unregister-experimental-property-handler.ps1`,
  `tools/tests/check-fb2-shell-surfaces.ps1`,
  `docs/experimental-thumbnail-provider-manual-test.md`.
- В документации по experimental thumbnail provider отдельно зафиксировано
  практическое поведение Windows 11: часть `.fb2` может застревать не на
  ошибке provider, а на stale negative shell thumbnail cache, поэтому после
  обычного сброса `thumbcache` иногда нужен адресный forced/prime-прогрев.
- В `Fb2ThumbnailProvider.cpp` разведены сценарии:
  - `обложка отсутствует` -> мягкий `not found`;
  - `данные FB2 или обложки повреждены` -> `bad format`.
- Команда `Validate` для `.fb2` перенесена из legacy shell extension в обычную
  shell-команду `Software\Classes\FictionBook.2\shell\Validate\Command`,
  которая запускает `FBV.exe` без загрузки DLL в `explorer.exe`.
- Portable staging и NSIS input теперь несут два отдельных modern
  shell-артефакта: `FBShell.dll` и `FBShell64.dll`.
- Modern property handler теперь публикует подтверждённый Windows MVP
  (`Author`, `Title`, `Language`) и отдельный FBE-specific слой книжных
  свойств для `Keywords`, `Genre`, `Sequence`, `DocumentId`,
  `DocumentVersion`, `DocumentDate`.
- Внутренний reader и modern property model расширены полями `Keywords` и
  `Document ID`.
- Для отдельной FBE-specific schema подготовлены:
  `.propdesc`-schema, PowerShell-скрипты `PSRegisterPropertySchema` /
  `PSUnregisterPropertySchema` и отдельная документация по эксперименту.
- `PKEY_FBE_Sequence`, `PKEY_FBE_Genre`, `PKEY_FBE_Keywords`,
  `PKEY_FBE_DocumentId`, `PKEY_FBE_DocumentVersion`,
  `PKEY_FBE_DocumentDate` подключены в published-набор modern property
  handler.
- `FBE.Sequence.propdesc` включён в общий staging `out\package`, а schema
  теперь описывает FBE-specific поля `Keywords`, `Genre`, `Sequence`,
  `DocumentId`, `DocumentVersion`, `DocumentDate`.
- Добавлен отдельный smoke-скрипт
  `tools\tests\test-fbe-specific-installer.ps1`, который проверяет install /
  uninstall сценарий FBE-specific schema через обычный `setup.exe`.
- Добавлен диагностический скрипт
  `tools\tests\check-fbe-specific-properties.ps1` для быстрой проверки
  published-состояния `FBE.Genre`, `FBE.Sequence`,
  `FBE.DocumentVersion`, `FBE.DocumentDate` без ручного вызова `propsys`.
- Тот же диагностический скрипт расширен выводом shell-строк
  `FictionBook.2/InfoTip`, `FictionBook.2/TileInfo` и
  `FictionBook.2/Details`, чтобы быстрее проверять полный shell-контур без
  GUI.
- Добавлен отдельный non-admin smoke-скрипт
  `tools\tests\check-fbe-shell-registration-consistency.ps1`, который
  проверяет согласованность `InfoTip`, `TileInfo`, `Details` между
  `MakeInstaller.nsi` и PowerShell-проверками shell-контура.
- В release-контур добавлена отдельная проверка staging-каталога
  `out\package\FictionBookEditor`, которая валидирует присутствие
  `FBShell.dll`, `FBShell64.dll`, `FBE.Sequence.propdesc` и отсутствие
  устаревших runtime-файлов ещё до сборки финальных артефактов.
- В release-контур добавлена отдельная проверка NSIS-layout, которая
  подтверждает, что `MakeInstaller.bat` и `MakeInstaller.nsi` используют
  `out\package\FictionBookEditor`, а удалённый `Installer\Input` не вернулся
  в дерево проекта.
- `MakeInstaller.bat` теперь после ручной сборки setup.exe автоматически
  пересчитывает `SHA256SUMS.txt`, чтобы каталог `out\artifacts` не уходил в
  рассинхрон по контрольным суммам.
- `verify-release.ps1` расширен проверкой `FileVersion`, `ProductVersion` и
  `FileDescription` для `FBShell.dll`, `res_rus.dll` и `res_ukr.dll`, чтобы
  metadata shell- и language-DLL не деградировали незаметно.
- NSIS-контур переведён на прямое чтение staging-папки
  `out\package\FictionBookEditor` как основного `INPUTDIR`, а историческая
  папка `packaging\nsis\Installer\Input` удалена из репозитория.
- Новые служебные сообщения release/NSIS-контура переведены на русский в
  `prepare-installer.ps1`, `verify-nsis-layout.ps1` и `MakeInstaller.bat`.
- Расширен `tools\tests\fb2-metadata-smoke.cpp`: теперь он проверяет не
  только базовый MVP, но и текущий published-набор свойств, включая
  `Genre`, `Sequence`, `Keywords`, `DocumentId`, `DocumentVersion`,
  `DocumentDate`.
- FBE-specific поля `Keywords`, `Genre`, `Sequence`, `DocumentId`,
  `DocumentVersion`, `DocumentDate` получили явные отображаемые названия с
  префиксом `FBE:`.
- Для ассоциации `FictionBook.2` в NSIS-установщике возвращены shell-строки
  `InfoTip`, `TileInfo` и `Details`, теперь уже поверх modern property
  handler и FBE-specific schema, без возврата legacy `ColumnProvider`.
- В документации отдельно зафиксировано, что legacy `.rgs` в `src\fbshell`
  теперь являются только историческим reference-контуром, а не описанием
  актуальной регистрации shell-интеграции.
- NSIS-установщик переведён на `RequestExecutionLevel highest`, чтобы на
  администраторских системах автоматически получать права, нужные для
  регистрации shell/property handler.
- Legacy `ColumnProvider` переведён в opt-in режим и больше не регистрируется
  в стандартной поставке по умолчанию.
- Legacy `ContextMenu` переведён в opt-in режим и больше не регистрируется в
  стандартной поставке по умолчанию.
- В NSIS-инсталляторе modern property handler вынесен в обычную
  `System integration`, а legacy shell-контур убран из стандартной поставки.
- Для `.fb2` оставлена обычная иконка файла через `DefaultIcon`, а
  `cover extraction` отложен как отдельная будущая задача.
- Стандартная сборка `FBShell` больше не подтягивает legacy `IconExtractor`,
  `ColumnProvider`, `ContextMenu` и их старый image-stack без явных
  build-флагов.
- Зафиксировано, что legacy `IconExtractor` всё ещё тянет встроенные
  `zlib 1.1.4`, `libpng 1.2.8` и `libjpeg 6b`, поэтому этот контур оставлен в
  отдельном списке дальнейшей ревизии.
- Режим `Дизайн` переведён с legacy `PCRE` на `PCRE2` с сохранением
  regression-покрытия для поиска и замены.
- Release/runtime/installer очищены от обязательной зависимости на `pcre.dll`.
- Ветка обновления `PCRE2` и базового regression-контура `Scintilla` доведена
  до рабочего завершения: текущий набор smoke/regression-проверок признан
  достаточным для штатных релизов, а оставшиеся идеи переведены в разряд
  необязательных будущих улучшений.
- Упорядочен релизный контур `out\Release`, `out\package`, `out\artifacts` и
  связанные проверки сборки.
- Обновлена документация по сборке, versioning и regression-проверкам.
- Дополнительно русифицированы пользовательские сообщения в smoke/runtime-
  сценариях `PCRE2`, `Scintilla`, `source-safety`, `verify-runtime-binaries`
  и legacy `PCRE`-проверках, чтобы локальная диагностика и release-проверки
  были единообразны по языку.
- На русский переведены служебные сообщения `tools\version\sync-version.ps1`,
  включая проверки `update.xml`, чтобы versioning-контур вёл себя так же
  единообразно, как release- и smoke-сценарии.
- В `tools\build\create-release.ps1` переведены оставшиеся активные
  англоязычные сообщения, а также автоматически генерируемый `README.txt`
  для пакета debug symbols.
- Сообщения в скриптах experimental property handler
  (`build-experimental-property-handler.ps1`,
  `register-experimental-property-handler.ps1`,
  `unregister-experimental-property-handler.ps1`) выровнены по русскому
  стилю остального build/shell-контура.
- `package-portable.ps1` и
  `tools\tests\check-fbe-specific-properties.ps1` дополнительно приведены к
  русскому пользовательскому выводу, включая заголовки диагностических
  таблиц shell-свойств.
- Скрипты `register-sequence-property-schema.ps1` и
  `unregister-sequence-property-schema.ps1` выровнены по русским сообщениям,
  чтобы registration/unregistration-контур схемы свойств выглядел единообразно.
- В `tools\tests\test-fbe-specific-installer.ps1` дочищены последние
  смешанные англо-русские формулировки вокруг install/uninstall smoke-
  проверки FBE-specific схемы свойств.
- Добавлен `tools\build\repair-fb2-shell-registration.ps1` — отдельный
  per-user сценарий восстановления shell-регистрации `.fb2` без полной
  переустановки FBE: ProgID, `DefaultIcon`, `InfoTip`, `TileInfo`,
  `Details`, а также команд `Edit` и `Validate`.
- Добавлен `tools\tests\check-fb2-shell-surfaces.ps1` — диагностический
  сценарий по поверхностям shell-интеграции `.fb2`, который показывает
  состояние `PropertyHandlers`, ProgID, shell-строк `InfoTip/TileInfo/Details`
  и даёт быструю оценку готовности колонок, tooltip и правой панели сведений.
- По ручной проверке на Windows 11 уточнено реальное поведение shell-поверхностей:
  после полной корректной регистрации tooltip уже показывает данные из `.fb2`
  (`Авторы`, `Название`, `Язык`, `FBE:Серия`, `FBE:Версия документа`,
  `FBE:Дата документа`), а правая панель сведений по-прежнему остаётся на
  уровне базовых файловых полей.
- Для правой панели сведений добавлена отдельная shell-строка
  `PreviewDetails`: она теперь прописывается в NSIS-установщике, repair-
  скрипте и диагностических сценариях наряду с `Details`, `InfoTip` и
  `TileInfo`.
- По итогам повторной ручной проверки на Windows 11 подтверждено, что после
  добавления `PreviewDetails` правая панель сведений тоже начинает показывать
  данные из `.fb2`, включая `Авторы`, `Название`, `Язык`, `FBE:Жанр`,
  `FBE:Серия`, `FBE:Версия документа`, `FBE:Дата документа`,
  `FBE:Ключевые слова`, `FBE:Идентификатор документа`.
- `tools\build\build-pcre2.ps1` теперь использует именованную mutex-
  блокировку на конфигурацию сборки, чтобы параллельные smoke-тесты не
  конфликтовали между собой из-за общего каталога `build\pcre2`.
- `tools\tests\scintilla-smoke.cpp` переведён в тихий режим на успехе:
  итоговый статус теперь печатает PowerShell-обвязка, без многократного
  англоязычного spam-вывода на каждую фикстуру.
- Дочищены оставшиеся активные release/shell-сообщения в
  `verify-release.ps1`, `create-release.ps1`,
  `register-experimental-property-handler.ps1`,
  `unregister-experimental-property-handler.ps1`,
  `test-fb2-shell-properties.ps1` и вспомогательных query/smoke-бинарниках.
- Для `tools\build\build-pcre2.ps1` добавлен тихий режим `-Quiet`: полный
  вывод `cmake`/`msbuild` теперь скрывается в штатных smoke-сценариях и
  показывается только при ошибке, из-за чего `PCRE2`-проверки стали заметно
  чище и удобнее в повседневной диагностике.
- Дополнительно вычищены англоязычные сообщения из вспомогательных скриптов
  и smoke-утилит Visual Studio / startup / metadata, чтобы служебный контур
  почти целиком использовал русские тексты ошибок и статусов.
- Исправлена битая кодировка и возвращена читаемость заметной части
  `tools\tests\regex-fixtures.json`, включая описания и заметки для
  `Scintilla`- и `PCRE`-регрессий.
- `tools\tests\regex-fixtures.json` доведён до целостного читаемого состояния:
  описания и заметки по активным `Scintilla`/`PCRE`/`PCRE2`-кейсам больше не
  содержат мусорной кириллицы.
- Внутренний adapter-слой regex режима `Дизайн` дополнительно упрощён:
  `RegexBackend` больше не держит отдельный runtime-dispatcher, а `IRegExp2`
  идёт напрямую в `PCRE2`-реализацию с тем же поведением синтаксических
  ошибок и уже подтверждённым regression-набором.

### Исправлено
- Исправлен приоритет логических операций в обработчике modeless-диалогов:
  клавиатурные сообщения теперь фильтруются строго внутри соответствующих
  `WM_KEYDOWN`/`WM_CHAR`. Удалены недостижимая ветвь сохранения selection и
  несколько доказанно избыточных условий. Сборка `pvs8-Release`, source-
  safety и startup smoke прошли.
- Усилен следующий набор core-путей по результатам PVS-аудита:
  инициализированы поля document tree и диалогов, `CSpeller` теперь освобождает
  список подсказок и `CSplitter`, а `ScriptLoad()` корректно обрабатывает
  нехватку памяти, переполнение размера, ошибку и короткое чтение файла без
  потери `GetLastError()`. Удалены лишние DOM-присваивание и недостижимая
  проверка фиксированного `vector`. Сборка `pvs7-Release`, source-safety,
  startup smoke и regression-тест орфографии прошли.
- Выполнена дополнительная безопасная чистка результатов PVS-Studio:
  удалены неиспользуемые локальные объекты в core-модулях, пустые
  деструкторы script-объектов заменены на `= default`, глобальная константа
  `Tokens` перенесена из заголовка в `Speller.cpp`, а очистка `CString` в
  `Serializable.h` переведена на `Empty()`. Изолированная сборка
  `pvs6-Release`, проверка source-safety, видимый startup smoke и
  regression-тест словарей орфографии прошли.
- Контрольный PVS-Studio прогон после исправлений подтвердил очистку
  вспомогательных проектов: `FBV`, `FBShell` и `ExportHTML` больше не имеют
  Level 1/2; оставшиеся три/одна диагностики относятся к ATL macro/style.
- В `FBE` дополнительно исправлены функционально значимые Level 2:
  - префикс `file://` теперь сравнивается полностью, без ошибочного
    шестисимвольного `memcmp`;
  - уточнён приоритет условий для обычных и inline-изображений;
  - `InsertCode()` гарантированно закрывает начатый undo-unit при раннем
    выходе и COM-исключении и не индексирует пустую HTML-строку;
  - восстановление find/replace после двух диалогов настроек больше не
    зависит от неоднозначного приоритета тернарного оператора;
  - добавлены явные скобки в export filename и GLLogo без изменения
    фактической логики.
- После пакета успешно прошли чистая сборка `FBE Release|Win32`,
  `test-source-safety.ps1` и видимый startup smoke отдельной PVS-сборки.
- Проведён PVS-Studio-аудит остальных C++-проектов решения:
  - `FBV`: форма цикла `FindFirstFile` переписана с явной инициализацией
    search handle; существующий `FindClose` сохранён;
  - `FBShell`: неиспользуемый legacy `ptr.h` исключён из precompiled header,
    COM-фабрики используют `new (std::nothrow)` и реально возвращают
    `E_OUTOFMEMORY`, пустой деструктор property store объявлен `default`;
  - общий `Serializable.h` полностью инициализирует тип, объект и factory во
    всех конструкторах `CProperty`;
  - `ExportHTML`: намеренное продолжение экспорта после ошибки отдельного
    изображения выражено явно, без пустого `catch`;
  - оставшиеся V1096/V1048 в `ExportHTML` классифицированы как срабатывания
    на штатные ATL `BEGIN_COM_MAP` / message-map макросы.
- После этого этапа успешно собраны `FBV` Win32, `FBShell` Win32/x64,
  `ExportHTML` Win32 и `FBE` Win32; smoke-тесты метаданных и thumbnail
  provider прошли.
- По результатам первого полного прогона PVS-Studio для `FBE` устранена
  группа подтверждённых дефектов в собственном коде:
  - исправлено удаление адреса итератора вместо объекта `CHotkey`;
  - устранено неопределённое поведение двух `T2A` в одном выражении при
    загрузке словарей Hunspell;
  - локальные `CWaitCursor` и временный `TreeNode` переведены на RAII;
  - исправлена передача C++ `bool` в параметры MSHTML `VARIANT_BOOL` и
    проверка результата `loadXML`;
  - `CImage::Create` больше не трактуется как функция, возвращающая
    `HRESULT`;
  - устранена утечка COM-ссылки в `Doc::InvokeFunc`, а имя вызываемой
    функции теперь принимается как `LPCOLESTR`, без ложного BSTR-контракта;
  - `CElementDescriptor` получил полную инициализацию, виртуальный
    деструктор и освобождение загруженных bitmap/icon;
  - исправлены неинициализированные счётчики замены и поля диалогов;
  - фабрики popup-меню возвращают `HMENU` и больше не копируют владеющий
    `CMenu`; меню корректно уничтожается и при отмене выбора.
- Проверочная чистая сборка `FBE` после исправлений успешно выполнена для
  `Release|Win32` в отдельном промежуточном каталоге.

- В основном коде `src/fbe` устранены два хрупких места, выявленных при
  локальном аудите:
  - в `mainfrm.cpp` у `CMainFrame::SourceToHTML()` исправлено освобождение
    временного `char[]`-буфера (`delete[]` вместо `delete`) и добавлен cleanup
    на ранних ошибочных выходах, чтобы не оставлять heap corruption и утечки
    на parse-failure путях;
  - в `utils/Utils.cpp` функция `LoadFile()` переведена с `GetFileSize` на
    `GetFileSizeEx` с явной защитой от oversized-файлов, чтобы чтение в
    `SAFEARRAY` не зависело от старого 32-битного пути размера файла.
- Во второй волне точечного аудита `src/fbe` дополнительно усилены ещё два
  узких места:
  - в `script.cpp` загрузка пользовательских скриптов переведена на
    `GetFileSizeEx`, а создание `IMultiLanguage2` теперь проверяется явно,
    чтобы не рисковать работой через неинициализированный COM-объект;
  - в `xmlMatchedTagsHighlighter.cpp` убран фиксированный буфер `malloc(1024)`
    для `SCI_GETTEXTRANGE`: длина буфера теперь зависит от фактической длины
    найденного XML-тега, что убирает риск переполнения на длинных тегах.
- В NSIS-установщике исправлено описание optional-секции системной
  интеграции: теперь оно больше не вводит в заблуждение и не утверждает, что
  создание деинсталлятора зависит от выбора этой секции.
- В uninstall metadata для FBE добавлена запись `EstimatedSize`, поэтому в
  «Программы и компоненты» / «Установленные приложения» теперь должен
  отображаться размер установленной программы.
- Ручной GUI-прогон подтвердил итоговую модель NSIS/UAC:
  `uninst.exe` создаётся и без системной интеграции, удаление без системной
  интеграции идёт без UAC, а при установленной системной интеграции
  conditional elevation на удалении срабатывает как ожидается.
- Исправлена логическая ошибка NSIS-контура, из-за которой `uninst.exe` и
  uninstall-key создавались только внутри optional-секции
  `System_Integration`. В результате silent smoke проходил, а реальная
  GUI-установка с UAC-relaunch могла остаться без деинсталлятора. Теперь
  uninstall metadata и `WriteUninstaller` выполняются всегда в основном
  install-контуре, а маркер `SystemIntegrationInstalled` по-прежнему
  переключается только при выборе системной интеграции.
- `tools/build/cleanup-shell-test-state.ps1` больше не обрывает весь clean-room
  цикл на первом занятом файле или каталоге: теперь скрипт старается удалить
  максимум возможного и вместо фатального падения оставляет явные warning-
  сообщения о реально заблокированных хвостах.
- `tools/build/unregister-sequence-property-schema.ps1` сделан идемпотентнее:
  `PSUnregisterPropertySchema -> 0x80070002` теперь трактуется как допустимый
  сценарий «схема уже отсутствует в активной регистрации», а не как фатальная
  ошибка, поэтому pre-clean шаг больше не ломает installer smoke на уже
  очищенной машине.
- Доведён conditional UAC-контур деинсталлятора: `MakeInstaller.nsi` теперь
  записывает в uninstall-ключ явный маркер `SystemIntegrationInstalled=1`,
  а `un.onInit` использует его для uninstall-side elevation через NSIS UAC
  только тогда, когда установлена системная интеграция. Smoke-
  сценарий `test-fbe-specific-installer.ps1` дополнительно проверяет
  наличие этого маркера после установки и отсутствие uninstall-ключа после
  деинсталляции.
- Уточнён статус старой диагностики shell-свойств на реальной книге
  `tmp/diamond-guns-germs-steel.fb2`: повторный прогон
  `test-fb2-shell-properties.ps1` подтвердил, что modern property handler
  сейчас возвращает корректные `Author/Title/Language/Genre` и
  document-info поля, а пустые `Sequence` / `Keywords` объясняются
  содержимым самой книги, а не оставшейся parser-регрессией.
- Из `tools/tests` удалена неиспользуемая ad-hoc пара
  `fb2-cover-inspect.cpp` / `test-fb2-cover-inspect.ps1`: этот точечный
  инспектор больше не входил ни в активный build/release-контур, ни в
  документацию, а его место заняли более целевые smoke- и shell-сценарии.
- В `tools/tests` удалены три устаревших промежуточных C++ smoke-файла
  shell-thumbnail-контура (`fb2-shell-thumbnail-cache-smoke.cpp`,
  `fb2-shell-thumbnail-force-smoke.cpp`,
  `fb2-shell-thumbnail-force-outproc-smoke.cpp`): они больше не участвовали
  ни в активных скриптах, ни в документации, а их сценарии уже перекрыты
  сводным `test-fb2-shell-thumbnail-matrix.ps1`, forced dump и prime-
  диагностикой.
- Шумный служебный вывод `muirct.exe` в `tools/build/build-fbv-verb-mui.ps1`
  приглушён: на успешной MUI-сборке лог больше не засоряется длинными
  повторяющимися блоками, но при ошибке нативный вывод по-прежнему
  показывается целиком через `Write-Warning`.
- `tools/build/verify-nsis-layout.ps1` больше не требует только историческую
  относительную форму `INPUTDIR` в `MakeInstaller.bat`: теперь проверка
  принимает и актуальную схему через `%REPO_ROOT%`, чтобы release-контур не
  падал на корректном bat-файле.
- Дочищены оставшиеся build/release-хвосты после обновления редакторных
  зависимостей: `tools/build/build-scintilla.ps1` и
  `tools/build/verify-artifacts.ps1` переведены на актуальные версии
  `Scintilla 5.6.3` и `Lexilla 5.5.0`, чтобы сообщения сборки и проверка
  portable-артефактов не расходились с фактическим состоянием `third_party`.
- Полная `Release|Win32` сборка `FBE.exe` и `FBShell.dll` больше не падает
  на post-build шаге `UPX`: упаковка переведена в явный opt-in режим
  (`EnableUpx=true`), потому что текущие `GUARD_CF`-бинарники заведомо
  несовместимы с `UPX` и не должны делать обычный build ложнопадающим.
- По итогам подробного прогона FBE после крупного обновления Hunspell актуализированы ожидаемые версии Scintilla.dll и Lexilla.dll в 	ools/build/verify-release.ps1 (5.6.3 и 5.5.0 соответственно), а также синхронизированы связанные упоминания в PROJECT_STATUS.md и docs/building.md, чтобы релизный smoke не падал на устаревших значениях.
- В src/fbe/Fb2CoverThumbnail.cpp добавлен #include "stdafx.h", чтобы полный Release|Win32 build FBE.exe снова проходил этап компиляции и не блокировал тестирование орфографии на новой версии Hunspell.
- Подтверждено clean-room smoke-проверкой, что обычный `setup.exe` теперь:
  - собирается штатным bat-сценарием из корня репозитория;
  - ставит `FBShell.dll`, `FBShell64.dll` и `FBE.Sequence.propdesc` в shared
    shell dir;
  - регистрирует modern property handler, thumbnail provider и shell-строки
    `InfoTip` / `TileInfo` / `Details` / `PreviewDetails`;
  - корректно возвращает shell-команду `Validate` для `.fb2` после финальной
    установки.
- Подтверждено, что локально восстановленный `FBV.exe` попадает в
  `out\Release`, `out\package\FictionBookEditor` и `setup.exe`, а shell-
  команда `Validate` открывает новый fixture
  `tools/tests/fb2-validate-valid-smoke.fb2` без ошибок (`No errors.`).
- Для shell-команды `Validate` добавлена динамическая локализация через
  `MUIVerb` (`@$INSTDIR\FBV.exe,-109`) и исправлен формат `Icon`
  (`"$INSTDIR\FBV.exe",0`), чтобы подпись пункта и значок корректно
  подтягивались из `FBV.exe` и переключались вместе с языком интерфейса
  Windows. В `src/fbv/FBV.rc` добавлены переводы для русского, английского,
  украинского, немецкого, французского, испанского, итальянского, польского,
  чешского, болгарского, португальского и нидерландского языков.
- Локальная сборка `src/fbv/FBV.vcxproj` повторно проверена после добавления
  MUI-ресурсов: `Release|Win32` и `Release|x64` проходят с `0 warnings /
  0 errors`.
- Исправлена кодировка локализованной подписи shell-команды `Validate`:
  строковые ресурсы в `src/fbv/FBV.rc` переведены на Unicode `\xNNNN`
  escape-последовательности, чтобы `MUIVerb` больше не показывал
  кракозябры в Explorer на русской и других не-ASCII локализациях.
- Локализация shell-команды `Validate` переведена на поддерживаемую MUI-модель
  по документации Microsoft: вместо чтения строки из `FBV.exe` добавлен
  отдельный language-neutral модуль `FBVVerbResources.dll` и спутники
  `*.mui` для `en-US`, `ru-RU`, `uk-UA`, `de-DE`, `fr-FR`, `es-ES`,
  `it-IT`, `pl-PL`, `cs-CZ`, `bg-BG`, `pt-PT` и `nl-NL`. Сборка выполняется
  новым скриптом `tools/build/build-fbv-verb-mui.ps1`, а `MUIVerb`
  переключён на `@...\FBVVerbResources.dll,-109;v2`.
- Добавлен smoke-тест `tools/tests/test-fbv-verb-muiverb.ps1`, который
  вызывает `SHLoadIndirectString` и подтверждает корректную локализацию
  подписи `Validate` прямо из собранного MUI-модуля.
- Административный installer smoke `tools/tests/test-fbe-specific-installer.ps1`
  подтверждает новый MUI-контур `Validate`: setup регистрирует
  `Validate\Command`, `Icon` и
  `MUIVerb = @...\FBVVerbResources.dll,-109;v2`, а деинсталляция
  корректно очищает shell-verb.
- В `src/fbv/FBV.cpp` внесены точечные улучшения устойчивости без смены
  архитектуры: убраны глобальные `g_file_list/g_list_items/g_list_max` в
  поля `CMainDlg`, исправлена повторная валидация с очисткой старых
  `errmsg`, устранена потенциальная утечка и ложное сохранение старой ошибки
  после успешного `Revalidate`, `AddFileInfo` больше не увеличивает счётчик
  до успешного `SysAllocString`, чтение метаданных файла переведено на
  `GetFileSizeEx`, а `CreateFile` использует безопасный sharing
  (`FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE`). Также
  `SetItemState` теперь ищет элемент списка через `LVM_FINDITEM` по `lParam`,
  а не предполагает равенство индекса и `lParam`.
- Исправлен штатный helper регистрации modern shell-контура:
  `register-modern-property-handler.ps1` теперь достраивает COM-регистрацию
  thumbnail provider, если `regsvr32` не создал её полностью, из-за чего
  ранее `CoCreateInstance` падал с `0x80040154`, а Explorer не мог получить
  миниатюру даже при наличии `ShellEx`-веток.
- Исправлен helper снятия регистрации modern shell-контура:
  `unregister-modern-property-handler.ps1` теперь удаляет не только
  property handler и `ShellEx`, но и COM-ветку CLSID thumbnail provider.
- Исправлен helper-сценарий снятия регистрации modern property handler из
  NSIS-контура: `unregister-modern-property-handler.ps1` больше не считает
  фатальной ошибкой ситуацию, когда `regsvr32 /u` вернул код ошибки, но
  нужные shell-ветки и CLSID уже были корректно очищены вручную.

- Исправлен запуск helper-скриптов регистрации modern property handler из
  NSIS-установщика под `Windows PowerShell 5.1`: из
  `register-modern-property-handler.ps1` и
  `unregister-modern-property-handler.ps1` убраны не-ASCII строки, из-за
  которых helper раньше мог падать ещё на этапе парсинга и возвращать
  безликий `exit=1`.
- Исправлен shell-контракт experimental thumbnail provider:
  `Fb2ThumbnailProvider` больше не возвращает полноразмерную обложку как есть,
  а масштабирует bitmap под запрошенный `cx`. Это убрало сбой для части книг,
  у которых out-of-proc shell-путь Windows раньше падал уже после успешного
  чтения обложки.
- Исправлен apply-контур новых сценариев обновления third-party исходников:
  `apply-third-party-update.ps1` теперь корректно принимает не только каталог,
  где исходники лежат сразу в корне, но и layout после распаковки вида
  `tmp\third-party-updates\scintilla-<version>\scintilla\...` /
  `lexilla-<version>\lexilla\...`, из-за которого первый боевой прогон
  `update-third-party-dependency.ps1` раньше падал на проверке `version.txt`.
- Пользовательский вывод новых сценариев обновления third-party исходников
  (`check-third-party-updates.ps1`, `download-third-party-updates.ps1`,
  `apply-third-party-update.ps1`, `apply-third-party-update-and-test.ps1`,
  `update-third-party-dependency.ps1`) переведён с временного транслита на
  нормальные русские сообщения без регрессии совместимости с
  `Windows PowerShell 5.1`.
- В активных build/update/test-скриптах, затронутых новым dependency-контуром,
  добавлены короткие заголовочные комментарии `.SYNOPSIS`, чтобы при открытии
  файла сразу было видно его назначение.
- Контур обновления зависимостей расширен на `Hunspell`: новая зависимость
  определяется по версии из `third_party\hunspell\configure.ac`, upstream
  версия и ZIP-архив берутся из тегов `hunspell/hunspell`, а build/test-
  шаг после раскладки идёт через `build-hunspell.ps1` и
  `test-spellcheck-dictionaries.ps1`.
- `update-third-party-dependency.ps1` и
  `apply-third-party-update-and-test.ps1` больше не требуют обязательной
  повторной раскладки уже актуальной версии при сценарии
  `-AllowCurrentVersion`: теперь такой прогон можно использовать как
  способ повторно пройти build/test-контур без `-ForceApply`.
- Подтверждено полным `test-fb2-shell-thumbnail-matrix.ps1`, что текущий
  modern thumbnail provider проходит весь автоматический контур:
  прямой COM smoke, системную COM-активацию, shell API на валидных
  `png/jpeg/bmp` fixture, forced extraction dump и ожидаемые мягкие отказы
  на битых/неполных `.fb2`.
- Forced dump на реальной книге подтвердил, что оставшиеся белые поля вокруг
  части обложек объясняются уже не старой квадратной подложкой provider, а
  естественными пропорциями или самим содержимым embedded cover image.
- Подтверждено на реальной машине, что новый modern property handler
  показывает метаданные `.fb2` не только в колонках Проводника, но и в
  tooltip, а после добавления `PreviewDetails` ещё и в правой панели
  сведений Windows 11.
- Подтверждено на реальной машине, что FBE-specific поле `Sequence`
  отображается в Проводнике через отдельную schema `FBE.Sequence.propdesc`.
- Подтверждено на реальной машине, что FBE-specific поля `Keywords`,
  `Genre`, `Sequence`, `DocumentId`, `DocumentVersion`, `DocumentDate`
  отображаются в колонках Проводника с корректными значениями.
- Автоматический smoke-тест published property model переведён на новые
  FBE-specific ключи `Keywords`, `Genre`, `Sequence`, `DocumentId`,
  `DocumentVersion`, `DocumentDate`.
- Исправлена лишняя зависимость `src\fbshell\FbeCustomProperties.cpp` от
  `stdafx.h`, чтобы этот модуль проще собирался в изолированных smoke-тестах.
- Исправлены PowerShell-скрипты регистрации и снятия регистрации
  `FBE.Sequence.propdesc`: положительные HRESULT-статусы `propsys.dll`
  больше не считаются фатальной ошибкой.
- Скрипты `register-sequence-property-schema.ps1` и
  `unregister-sequence-property-schema.ps1` теперь можно безопасно запускать
  повторно в одной и той же PowerShell-сессии без ошибки повторного `Add-Type`.
- Исправлены скрипты регистрации experimental property handler: теперь они
  действительно берут DLL из `out\property-handler\...`, а не из обычного
  релизного каталога.
- Подтверждено smoke-проверкой `setup.exe`, что обычная установка теперь:
  - ставит modern property handler под нужное имя в зависимости от разрядности
    системы;
  - регистрирует `.fb2` property handler в `HKLM`;
  - снимает эту регистрацию при silent-деинсталляции.
- После добавления `VERSIONINFO` заново пересобраны `FBShell.dll`,
  `res_rus.dll` и `res_ukr.dll`; подтверждено, что в бинарниках реально
  появились `FileVersion`, `ProductVersion` и `FileDescription`.
- Исправлена поломка локализованных `FBE.rc` после добавления `VERSIONINFO`:
  из `src\locales\res_rus\FBE.rc` и `src\locales\res_ukr\FBE.rc`
  убраны дублирующиеся version-блоки, которые ломали сборку ресурсов.
- Для experimental property handler отключена попытка упаковки через `UPX`,
  потому что `GUARD_CF`-бинарники этим инструментом не поддерживаются и на
  `Win32`, и на `x64`.
- Исправлено падение орфографии после обновления `hunspell`: найден и устранён
  сбой в `suggestmgr.cxx`.
- Исправлены regression-сценарии regex/Scintilla, включая кейсы поиска и
  замены в режиме `Код`.

### Заметки

- Пока это changelog именно локальной ветки модернизации, а не официальный
  исторический журнал всех релизов проекта.
- Дальше новые технические изменения нужно сразу отражать в этом файле.
