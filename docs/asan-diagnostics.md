# MSVC AddressSanitizer

Это отдельный диагностический контур для Windows 10/11. Обычные Release Win32,
v143 / VC Tools 14.44, C++17 и политика Windows 7 SP1 сохраняются. ASan-бинарники
не являются релизными и не предназначены для Windows 7.

## Сборка и ручной запуск

Из корня репозитория:

```powershell
pwsh ./tools/build/build.ps1 -Configuration Release -Platform Win32 -Sanitizer Address
./out/asan/Release/FBE.exe --portable
```

Для обычной сборки параметр не требуется; его явное значение — `-Sanitizer None`.
Дополнительной MSBuild-конфигурации нет. В диагностическом режиме выходные файлы
находятся в `out/asan/Release`, плагины — в его `Plugins`; промежуточные файлы —
в `build/asan/obj/<Project>/Win32/Release`. Fingerprint включает sanitizer mode,
а ASan provenance хранится отдельно в `out/asan/build-provenance`.

Сохраняются `/O2` и существующий `/GL`; добавляются `/fsanitize=address /Zi`.
Для ASan выключены `/RTC`, Edit-and-Continue, incremental link и UPX. PDB остаются
рядом с бинарниками, включая собственный каталог PDB для buildstamp.c.

`clang_rt.asan_dynamic-i386.dll` и `llvm-symbolizer.exe` копируются из выбранного
VC 14.44 в диагностический output. Запуск не зависит от случайного локального PATH.
Runner также добавляет этот каталог в PATH для скопированных тестовых runtime.

Вручную можно открыть большой FB2, переключать Visual/Source, искать текст/regexp,
редактировать таблицы и картинки, выполнять структурные операции, Undo/Redo,
Save/Save As, EPUB import/export и DOCX export, затем закрыть и открыть документ.
Ручной просмотр дополняет автоматические проверки.

## Инструментирование

Инструментированы все собственные native-проекты текущего решения:

- FBE, FBV, FBShell;
- ExportHTML, ExportDOCX, ExportEPUB, ImportEPUB, ImportEPUBLunaSVG;
- ExportDOCXBatch, ExportEPUBBatch, ImportEPUBBatch.

Включены их C/C++ translation units, generated COM stubs и buildstamp.c.
MIDL source outputs остаются в штатном `build/generated/.../fbe-api`; sanitizer
объекты не смешиваются с обычными. `fbe-sanitizer.json` фиксирует хеши модулей,
PDB и фактические compiler tlogs с `/fsanitize=address`.

Обычные vendor-кэши переиспользуются. Scintilla, Lexilla, PCRE2, Hunspell,
libarchive, image stack, lunasvg/plutovg и системные Windows/COM/MSHTML-компоненты
не инструментированы. ASan перехватывает поддержанные allocation/CRT APIs,
включая process-heap вызовы, но это **не 100% инструментирование процесса**.

Для совместимости с non-ASan статическими vendor-библиотеками в диагностическом
контуре заданы `_DISABLE_VECTOR_ANNOTATION` и `_DISABLE_STRING_ANNOTATION`.
Иначе v143 отклоняет линковку с LNK2038 из-за разных `annotate_vector/string`.
Проверки памяти после границы allocation сохраняются; доступ внутри capacity,
но за логическим size STL-контейнера, этим контуром не покрывается. Это ограничение
смешанного инструментирования, а не подавление найденного дефекта.

Справка Microsoft: [сборка ASan](https://learn.microsoft.com/en-us/cpp/sanitizers/asan-building),
[STL container annotations](https://learn.microsoft.com/en-us/cpp/sanitizers/error-container-overflow),
[runtime и Windows heap interceptors](https://learn.microsoft.com/en-us/cpp/sanitizers/asan-runtime).

## Автоматические контуры

```powershell
pwsh ./tools/tests/test-asan-infrastructure.ps1
pwsh ./tools/tests/test-sanitizer-build-policy.ps1
pwsh ./tools/tests/test-asan-regressions-native.ps1 -Sanitizer Address
pwsh ./tools/tests/run-sanitizers.ps1 -Sanitizer Address -Contour Fast
pwsh ./tools/tests/run-sanitizers.ps1 -Sanitizer Address -Contour Full
pwsh ./tools/tests/run-sanitizers.ps1 -Contour Fast -List
```

Источник сценариев — существующий `get-release-test-catalog.ps1`, дополненный
sanitizer metadata из параметров harnesses. Runner переиспользует настоящие
команды и матрицы режимов из release verifier и CI. Второго списка из сотен
тестов нет. Обычные FAST/FULL release-профили не меняются.

FAST включает существующие runtime-сценарии запуска/завершения, lifecycle
документа/editor view, FB2/FBD, ZIP/RAR, Source/Visual, поиска/regexp, структурных
команд, table smoke, clipboard, Undo/Redo, scripts, plugins, import/export и
save/reopen. В DOCX v2 harness доступен дополнительный `-IncludeBatch`; sanitizer
runner включает его, чтобы проверить реальный DOCX batch-конвертер.

FULL добавляет TABLE/FULL runtime-варианты: huge/production round-trip,
сложные таблицы, большие бинарные данные, repeated lifecycle, stress,
fault injection и расширенные scripting матрицы. Performance/benchmark-тесты
исключены; обычные Release thresholds не изменены. Package-only проверки и
отдельный disposable-user/private-window-station OLE CI-контур не объявляются
покрытыми этим дочерним process tree. Для каждого исключения summary содержит причину.

`-Id` позволяет отладить выбранные записи каталога. Такой отчёт имеет
`selectionComplete=false` и не доказывает прохождение полного FAST/FULL.
`-SkipInfrastructureSelfTest` предназначен для локальной отладки, CI его не задаёт.

Для будущего corpus feeder предусмотрен `-AdditionalCases <json>`: массив объектов
`name`, `command`, `expectedChildExitCodes`. Команда может вызывать harness с
внешним документом; она получает `$repoRoot` и `$outputDir`. Приватные книги
не добавляются в репозиторий. Это точка расширения, а не утверждение о уже
выполненном массовом corpus testing.

## Как определяется FAIL

Каждый harness запускается отдельным worker в Windows Job Object, назначенном
до возобновления worker. Монитор удерживает process handles, проверяет exit
statuses и сверяет наблюдённые процессы с job accounting; потерянное наблюдение
не превращается в PASS. WMI/администраторский Process Trace не требуется.
Worker наследует уже подготовленную VC 14.44 x86 environment; повторные запросы
этой среды из harness переиспользуют её. Оставшийся после компиляции VCTIP из
точного каталога выбранного toolchain очищается только после завершения всех
остальных наблюдённых процессов и отмечается в `infrastructureCleanup`.
После выхода worker монитор ожидает завершения всего Job Object в пределах
тайм-аута сценария, включая дочерние процессы dump/symbolizer. Если соединение
онлайн-символизации оборвалось, сохранённые offsets собственных модулей повторно
разрешаются через `llvm-symbolizer` и PDB в отдельный `.symbolized.log`;
исходный diagnostic сохраняется. Self-test отдельно проверяет этот путь на
настоящем process-heap allocator FBE.
Разворачивание inline-кадров выключено из-за воспроизводимого падения PDB reader
в `llvm-symbolizer` VC 14.44; обычные source/function frames сохраняются. Только
allocator-объект отрицательного self-test компилируется с `/Ob0`, чтобы проверить
его отдельный frame. Оптимизация и inlining продуктовых модулей не меняются.

Проверяются exit worker, аварии first-party/инструментированных native children,
ASan diagnostic, sanitizer crash и созданные dumps. Обычные ожидаемые коды ошибок
CLI (например, конфликт ключей) явно описаны метаданными сценария; diagnostic
или dump всё равно означает FAIL.
Контрактные отрицательные Cite/Poem-проверки допускают `1`, а проверки сохранения
`WM_QUIT` — `73`; эти коды разрешены только соответствующим сценариям.
Успешные GUI probes ожидают штатный exit после `WM_CLOSE`, а не принудительный kill.
FBE/batch без ASan не могут случайно заменить диагностический продуктовый процесс.
Инструментированные native helpers,
компилируемые существующими тестами, также отслеживаются.

Self-test проверяет compiler macro, чистый инструментированный probe, скрытый
ненулевой child exit и настоящий heap-use-after-free с символизированным стеком
и dump. Внешний worker намеренно возвращает 0: runner обязан обнаружить ошибку.
Заведомо дефектный probe создаётся только в generated output и не входит в FBE.
Его ожидаемый dump проверяется и удаляется; реальные findings сохраняются.

Каждый запуск получает собственный `out/tests/asan/<timestamp>-<guid>`.
`ASAN_SAVE_DUMPS` и ASan log prefix задаются отдельно для сценария; runtime
добавляет PID. Отчёт содержит test name, executable, commit, sanitizer, exit code,
stdout/stderr, stack trace files и dump paths. Summary сохраняется и при ошибке.

## CI и release boundary

Отдельный `.github/workflows/asan.yml` выполняет build, policy checks и FAST на
windows-2022 / Win32 / v143 / VC 14.44. Finding завершает job ошибкой, без
`continue-on-error`. FULL доступен для tag и workflow_dispatch.
Summary загружается всегда; failure logs и dumps — при ошибке.

Release packagers вызывают production-payload guard: отклоняются ASan marker,
runtime DLL и инструментированные EXE/DLL, даже если marker/runtime были удалены.
Состав installer/portable и обычные release-проверки не ослаблены. Этот контур
не публикует диагностические бинарники.

После изменения сборочной инфраструктуры обязательны обычные команды:

```powershell
pwsh ./tools/build/build.ps1 -Configuration Release -Platform Win32
pwsh ./tools/build/verify-release.ps1 -Configuration Release
```

UBSan в этот этап не входит.

## Дефекты первого baseline

В реальных runtime-сценариях ASan подтвердил передачу borrowed SAFEARRAY storage
в CreateStreamOnHGlobal и use-after-free текста кнопки при восстановлении toolbar.
Первый путь теперь копирует байты в собственный movable HGLOBAL с передачей
владения IStream; сохраняется поддержка Clone на Windows 7. Каталог toolbar
хранит подписи в неизменяемом intern pool на время жизни frame, независимо от
удаления кнопок и перестроения контролов.

Минимальные native-регрессии проверяют границы/тип/locks SAFEARRAY, отказы Win32,
освобождение исходного массива, настоящий image decoder и повторное создание
кнопок с Unicode-подписью после удаления исходной кнопки. Оба прежних дефекта
воспроизводятся на временных копиях исходного кода. Сводные результаты текущего
этапа записываются отдельно от исходного неуспешного baseline.

## Локальная валидация 2026-10-24

Продолжение выполнено из текущего рабочего дерева на базе
`5c4502259fe32dd524e4451fbb876ff615bf9997`. Ранее внесённые ASan-изменения,
два исправления памяти и их регрессии сохранены. Исходный длительный baseline
не повторялся: проверены сохранённые summary и отрицательные воспроизведения.
Бинарники проверялись до итогового commit; SHA-256 всех 11 first-party модулей
записаны в `out/asan/Release/fbe-sanitizer.json` и runtime-отчётах.

| Проверка | Результат | Свидетельство в `out/tests/asan/` |
|---|---|---|
| ASan Release Win32, strict warnings | PASS | `continuation-build-settings.log` |
| Обычный Release Win32, strict warnings | PASS | `continuation-release-build-settings.log` |
| Обычный `verify-release.ps1 -Configuration Release` | PASS | `continuation-verify-release-settings.log` |
| Нативные регрессии, Address и None | PASS: 54 + 167 проверок в каждом режиме | `continuation-regressions-final.log`, `continuation-release-regressions-final.log` |
| Infrastructure self-test | PASS | `self-test-f32ff7ebacd34c519baf82176d25032f/summary.json` |
| Build/package policy | PASS: 438 TU/mode checks, три отклонения diagnostic payload | `continuation-policy-settings.log` |
| ASan FAST | PASS: 84/84, audited union | `continuation-fast-current-audited/summary.json` |
| ASan FULL | PASS: 147/147, audited union | `continuation-full-current-audited/summary.json` |
| Реальные production Core/portable/Integration stages | PASS | `continuation-production-final.log` |
| Settings file transaction regression | PASS; прежний код FAIL 28 | `continuation-settings-regression.log`, `continuation-settings-old-negative.log` |
| Settings seed contract | PASS: default, XML escaping, validation=true, запрет внешнего каталога | `continuation-seed-contract.log` |

FAST/FULL имеют режим доказательства `AUDITED_CATALOG_UNION`. На окончательной
нативной сборке последовательно выполнены все текущие команды каталога:

- FAST `20261024-135312-339e73dfbb70437bb14cb94774b81239/summary.json`:
  84 команды, 83 PASS, один runtime FAIL, без ASan diagnostics;
- дополнительный FULL `20261024-152328-c2963faaf99f4e5aa2df2ffb4e61534d/summary.json`:
  оставшиеся 63 команды, все PASS. Включены huge table round-trip (4984 ячейки),
  500 x 64 KiB binary stress, table-матрица и scripting stage3c/stage3d;
- полный адресный повтор Visual DOM normalizer
  `20261024-155111-4941ee71fc764492938228acf4be1a65/summary.json`: PASS.
  Прежний FAIL касался только первой обычной вставки; восемь остальных строк
  прошли, FBE завершился с exit 0. Причина единичного отказа не установлена;
  код, условия проверки и expected exit ради него не менялись.

Generated `assemble-continuation-evidence.ps1` сопоставляет точные команды,
SHA-256 нативных модулей, свежесть harness и хеши исходных result.json.
В итоговом текущем аудите нет пропущенных команд, переклассифицированных exit,
ASan diagnostics или dumps. Raw FAIL сохранён; это не один монолитный
безошибочный FULL и не результат удалённого CI.

Предыдущий FULL на промежуточной исправленной сборке также покрывал 147 команд
после адресных повторов (`continuation-full-audited/summary.json`). Он сохранён
как историческое свидетельство и не подменяет покрытие окончательных бинарников.

### Дополнительные подтверждённые файловые отказы

Обычный Release gate выявил два отказа, не являющихся ASan memory findings:

- concurrent preset Save: transient Win32 error 5 при flushed rename.
  Бюджет повтора увеличен с 150 мс до одной секунды. Реальная reader-lock
  регрессия на 250 мс отвергает старый алгоритм с кодом 26; исправление прошло
  20 повторов и search-core suite в ASan и обычном release gate;
- CSettings Save: реальная блокировка Settings.xml читателем на 250 мс
  превышала прежние 70 мс повторов. На одноразовой копии подтверждены модальный
  диалог sharing violation и сохранность оригинала
  (`continuation-settings-lock-proof.log`). `SettingsFileTransaction` теперь
  повторяет только временные access/sharing отказы не более секунды с паузой
  до 100 мс. Реальный reader-thread regression отвергает прежний код с exit 28;
  новый код проходит его вместе с permanent lock, readonly, rollback,
  cancellation, create-only, replacement/deletion и cleanup проверками.

Форматы, атомарность и rollback сохранены. После второго исправления обе
нативные сборки пересобраны и всё перечисленное текущее ASan-покрытие выполнено
заново. Обычные CSettings portable Save/Load и binary rename → Save → reopen
прошли отдельно и внутри точного release gate.

### Инфраструктура и границы доказательства

В portable-режиме installed test-settings override не подменяет DataPath.
Cite/Poem и structural harness используют настоящий одноразовый runtime и seed
в DataPath/Settings, сохраняют артефакты ошибок и ожидают завершение child.
Их полные текущие результаты — 32/32 и 16/16 соответственно.
В прежнем custom TEMP `out/at` наблюдались timeout; снимок потока показывал
ReplaceFileW / BackupFileCommit.h:54. Окончательные прогоны выполнены в штатном
TEMP на C:. На основании этого снимка BackupFileCommit не менялся.

Диагностический стек portable GUI выявил долгую загрузку bundled startup
catalog и обычный запрос сохранения Untitled.fb2. Fixtures DOM/Undo используют
пустой startup catalog, registry fixture — один настоящий JScript с включённой
startup validation. Необязательный параметр seed не меняет default других
тестов. Registry harness работает без test-mode: ожидает production trace S230,
посылает WM_CLOSE своему frame и отвечает No только на собственный Untitled.fb2
Yes/No/Cancel prompt. Неизвестный диалог, timeout, авария или ненулевой exit
остаются FAIL. Acceptance-прогоны выполнены без debugger.

Self-test проверил свежую компиляцию внутри наблюдаемого job, чистый ASan child,
скрытый child exit 13, настоящий UAF и allocator FBE с source frames. Ожидаемые
dumps проверены и удалены. Очистка собственного VCTIP выбранного toolchain
после завершения остальных процессов job отдельно подтверждена холодным
`self-test-3b7911a4c89f4adaa2cb32847995b6c2/summary.json`.
Один ранний CRT abort в File Close не воспроизвёлся в последовательных повторах
и четырёх вариантах текущего FAST; exit 3 не разрешён.

Production stages созданы из окончательного None output в новом каталоге
`production-final-0c8af5ae0e404c919c7691f00871fe43/`. Core, portable, Integration
и существующий InstallerInput проверены на отсутствие ASan marker/runtime
и инструментированных PE. Negative policy проверяет также настоящий ASan PE
после удаления marker и runtime DLL. Обычный release gate подтвердил неизменность
tracked Scintilla/Lexilla DLL. Установка через реальный installer здесь не
проверялась.

CI workflow добавлен; удалённый CI **NOT RUN**. Продуктовые vendor-библиотеки
остаются неинструментированными; ограничения смешанного контура и STL container
annotations описаны выше. UBSan не включён.
