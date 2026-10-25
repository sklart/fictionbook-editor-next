# BinaryStore C3: владение payload и сравнение трёх ревизий

Статус acceptance: **IN PROGRESS**. Smoke не заменяет семь измеряемых
повторов, sanitizer-проверки или FullValidation. До их завершения C3 не закрыт.

## Хранение и совместимость

`FB::Doc::m_binaryStore.payloads` владеет каноническими значениями binary.
Идентичность определяется COM identity элемента, поэтому разные binary с одним
ID сохраняют независимые bytes. Индекс ID по-прежнему выбирает первый DOM-entry;
после его удаления становится виден следующий.

К каждому binary DIV присоединяется нативное MSHTML behavior через переданную
фабрику, без регистрации COM и внешнего HTC. Его `base64data` обслуживает прежние
чтение/запись. Behavior содержит только слабую ссылку на состояние документа и
числовой ключ. Сам DOM не хранит SAFEARRAY или base64 payload. Нативный механизм
описан в [документации Microsoft о binary behaviors](https://learn.microsoft.com/en-us/archive/msdn-magazine/2001/january/cutting-edge-binary-behaviors-in-internet-explorer-5-5).

Чтение возвращает временную копию массива прежнего типа `VT_ARRAY | VT_UI1`.
Запись заменяет канонический снимок и увеличивает revision, из-за чего метаданные
пересогласуются перед следующим запросом. При неизменном каталоге сверяется
revision каждого payload: size/dimensions сбрасываются только у изменённых
записей. Изменение DOM-каталога и произвольный `RunScript` по-прежнему требуют
полного reconciliation. Сохраняется прежняя возможность
записать неподдерживаемое значение expando: отказ происходит при операции,
потребляющей binary, как и в C2. Чтение не меняет dirty state и Undo.

Публичный IDL, API 1.5, его DISPIDs и сигнатуры не меняются. Прямой legacy write
сохраняет прежнюю семантику без создания нового Undo. Поддерживаемые операции
Add/Replace/Remove используют существующую OLE-историю. История и текущий store
используют общие неизменяемые снимки. Их bytes принадлежат документному store;
Undo edit хранит слабые leases, поэтому даже удержание старого Undo manager
после Close не удерживает payload. Текущий payload не дублируется отдельным
массивом в Undo edit. Временные массивы загрузки/сравнения освобождаются до
фиксации edit.
Повторная facade-запись того же одномерного UI1-массива (с теми же bounds и
bytes) сохраняет существующий canonical snapshot. В частности, служебный
`BuildBinaryControls` не оставляет отдельную decoded-копию текущего значения
рядом с Undo snapshot. Входная материализация при таком put освобождается.

`GetBinaries`, обычный Save, Source и экспорт продолжают читать legacy facade.
Материализация полного XML/base64 допускается на время этих операций. Source
Apply загружает новое документное состояние. Reload DOM внутри того же
документа сохраняет leases для стандартного XML Undo; полное закрытие документа
освобождает store независимо от удержания старой COM history.
Сохранённый FB2 остаётся обычным FB2.

Если старый скрипт самостоятельно создаёт DIV с expando, значение переносится
в store при reconciliation и обязательно при выходе из `RunScript`, включая
исключение. Перед удалением expando устанавливается и заполняется behavior;
ошибка его установки оставляет исходное значение нетронутым. DOM-узлы,
удерживаемые Undo или скриптом после reset/close, не удерживают прежний store.

## Адресные проверки

`test-fbe-binary-store-contract.ps1` запускает прежний индексный/ABI unit и новый
`binary-payload-store-test.cpp` на настоящем MSHTML. Новый unit проверяет
владение, transient reads, невозможность изменить owner через возвращённый
массив, немедленную запись, adoption expando, общие снимки, reset, document
separation и освобождение при удержании старого элемента.

`test-fbe-binary-store-runtime.ps1` дополнительно сравнивает канонические bytes
с legacy reads в процессе FBE и проверяет adoption вручную созданного binary
до возврата из скрипта, без дополнительного запроса или cache reset.
Существующий compatibility suite остаётся 46/46 + 6/6; исходники bundled scripts
не меняются. Проверки API Undo/Redo, Source/Visual и Save/Reopen принадлежат этим
реальным runtime-контурам, а не текстовым контрактам.

## Воспроизводимый benchmark

Подготовить три самостоятельных Release Win32 runtime-каталога. Каждый должен
содержать соответствующие `FBE.exe`, `main.js`, ресурсы и одинаковые версии
зависимостей. Сборки не перезаписываются во время benchmark:

- Baseline: `a4145d832b9be02930400969328b2f5a0ec54182`;
- C2: `3e44dc5f621819c5a4d50009ad367e24c8ac4fea`;
- C3: точный SHA отдельного implementation commit.

```powershell
./tools/perf/run-binary-store-benchmark.ps1 `
    -Baseline ./out/perf/baseline/FBE.exe `
    -C2 ./out/perf/c2/FBE.exe `
    -C3 ./out/perf/c3/FBE.exe `
    -C3Revision <40-character-commit-SHA> `
    -OutputDirectory ./out/perf/results-<unique-name>
```

По умолчанию выполняется один warm-up и семь измерений на каждый вариант и
fixture. Порядок чередуется forward/reverse. `-Smoke` выполняет только один
функциональный прогон и записывает `SMOKE_PASS`, без performance acceptance.
Обычный измеряемый запуск требует committed SHA C3.

Fixtures генерируются детерминированно и проверяются XSD до запуска:

- control: 4 PNG по 4 KiB, обычный текст, несколько общих references;
- many: 500 binaries по 64 KiB, включая 2 PNG и 498 непросматриваемых вложений;
- large: 10 PNG 1024×1024 с детерминированным шумом, около 30 MiB decoded payload.

Временные runtime-копии и настройки изолированы. Каталог стартовых макросов
пустой и одинаковый для трёх реализаций; bundled scripts отдельно проверяются
compatibility suite. Сам контроллер одинаков для всех сборок и не внедряет
инструментированный код в FBE. Сборки остаются обычными Release, без sanitizers.

Время измеряется внешним QPC-контроллером. Готовность — доступность полного
binary inventory через MSHTML и завершение синхронной UI-команды. Open включает
запуск процесса. Save использует обычную команду `ID_FILE_SAVE`, а не scripting
Save с дополнительной XSD-валидацией. Replace вызывает существующий API, затем
обычный Save. Проверочные копии файлов создаются вне измеряемого Save.

Отдельные сценарии: Open, Save без binary mutation, Replace+Save, Body→Source,
Source→Body, 1000 metadata queries, 5 последовательных замен и roundtrip.
`mutation_5` смешивает пять вызовов Replace с пятью raw legacy-восстановлениями
из `photo.png`; первый Replace может быть no-op, остальные четыре меняют bytes.
Это mixed legacy/API workload, его нельзя выдавать за пять чистых API-замен.
Перед Source→Body контроллер вводит пробел в конце XML через обычный `WM_CHAR`,
чтобы выполнить настоящий Apply без изменения decoded binary. Сохранённое
состояние после Apply и финальное состояние mutation-серии проверяются отдельно.
Roundtrip явно включает Save→document close→process close→process start→ready;
его нельзя представлять как чистое время повторного открытия в живом процессе.
Metadata workload измеряет stable-generation native API через общий scripting
context, без `Doc::RunScript` и его консервативной invalidation на каждом запросе.
Он не обещает такой же hit rate произвольным пользовательским скриптам.

После Open, Source, Body, Reopen и Close записываются steady Working Set и
Private Bytes (среднее 10 отсчётов через 50 мс), OS Peak Working Set, OS peak
private commit и sampled private peak с интервалом 10 мс. Значение
`PeakPagefileUsage` обозначает пик private commit согласно
[PROCESS_MEMORY_COUNTERS_EX](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex).
Steady и transient peaks выводятся раздельно. Рост RSS после Close сам по себе
не означает удержание payload: MSHTML и аллокаторы могут удерживать страницы.

Для каждой проверочной записи harness независимо проверяет XSD, decoded
SHA-256 всех binary, MIME и references. Результаты сохранения нельзя использовать
для performance-выводов, если эта проверка не прошла. После проверки большие
копии удаляются, остаются их хеши, fixture hashes, per-run equivalence records
и raw TSV. Source fixtures и runtime snapshots повторно хешируются.

Проверка раннего C3 кандидата с удержанием старого DOM после реального Reopen
выявила удержание canonical bytes старым Undo manager. Сильные ссылки edit на
снимки заменены leases: документное состояние удерживает history bytes и
освобождает их независимо от времени жизни COM history. Regression проверяет
истечение weak snapshot после Close/Reopen; исправленный Release и прежние
46/46 + 6/6 прошли. Исходный FAIL и бинарник сохранены локально под
`out/perf/c3-lifetime-finding`. Старый ASan FAST прерван как superseded и не
является финальной acceptance-проверкой.

FAST также обнаружил регрессию XML scripting Undo: native history восстанавливает
старые DOM nodes, а прежний reset C3 отсоединял их payload state. На C2 исходный
XML backend runtime прошёл; на обычном C3 Release воспроизвёлся timeout после
ошибки `GetBinaries`. Reload теперь перестраивает ID-проекцию без уничтожения
document state. `XmlVisualState` удерживает weak leases вместо постоянных
SAFEARRAY-копий и восстанавливает facade binding после native markup replay.
Контрольный FAIL сохранён под `out/perf/c3-xml-undo-finding`.
Расширенный runtime проверяет освобождение canonical bytes при настоящем
Reopen и отказ старого XML parent unit менять новый документ. Его отчёт
сохраняется по executable SHA под `out/tests/binary-store/xml-undo`.
Цепочка XML Apply также меняет payload
A→B→C и сверяет bytes после каждого Undo/Redo. Расширенный probe вынесен
в отдельный `noinline` handler по образцу остальных C3 script probes.
Удержание полного старого MSHTML DOM/Undo context через Reopen нестабильно:
dump показывает AV при финальном COM `Release` уже после успешных assertions.
Выделение handler и варианты порядка cleanup не устранили сбой; отдельные
PASS этих вариантов не принимаются как финальная acceptance-проверка.
Отдельный диагностический C2 control с тем же удержанием handles, без C3
ownership assertions и без replay старой history против нового документа,
воспроизвёл тот же AV в MSHTML при финальном `Release` (`c2-close-control-result.json`).
Он не является контрольным performance snapshot. Итоговый probe удерживает
production XML parent unit с настоящими `XmlVisualState` snapshots, но без
внутренних native markup child units. Он проверяет освобождение bytes и guard
против нового документа, не заявляя исправление прежнего ограничения MSHTML.

`manifest.json` фиксирует ревизии, executable/JS/dependency hashes, compiler,
Release Win32, OS, машину и power profile. `raw.json` содержит отдельные отсчёты;
`summary.json` — median, p90, min/max, N и три сравнения; `summary.md` — таблицу.
Положительный delta = `(old-new)/old*100`. Control slowdown более 5% против C2
помечается warning и требует разбора разброса, а не автоматического FAIL.
Generated evidence и build outputs остаются под `out`, в Git не добавляются.

## Acceptance

Первое полное сравнение кандидата `96911354` сохранено под
`out/perf/results-96911354`: 63 measured + 9 warm-up runs, equivalence и
immutable checks PASS. Оно выявило стабильное удорожание mixed mutation:
115→159 мс на control, 191→820 мс на large относительно C2. Причина в коде —
полный сброс metadata cache при любом facade write, после которого
`RebuildImagesInfo` декодировал также неизменённые PNG. Это finding исправляется
выборочной invalidation; прежние результаты не удаляются и не подменяются.
Экономии steady memory на large у этого кандидата практически нет
(private after Open: C2 249.08 MiB, C3 248.71 MiB).

ASan targeted и UBSan targeted для `96911354` прошли 5/5 каждый. Полный ASan
FAST завершил 84/84 invocations: 83 PASS, один toolbar size-switch timeout
120 секунд без sanitizer stack finding. Изолированный стандартный повтор
того же case на неизменённых модулях прошёл 1/1. Исходный полный report остаётся
FAIL; отдельный retry ledger сохраняет эту оговорку.

Итоговые SHA, результаты sanitizer/FullValidation и измерений добавляются после
их получения. До этого нет утверждения об экономии памяти или ускорении C3.
В C2 DOM уже содержал decoded SAFEARRAY, поэтому архитектурный перенос bytes
сам по себе не доказывает экономию; её должен показать фактический benchmark.
Push без отдельного указания пользователя запрещён.
