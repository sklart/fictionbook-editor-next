# BinaryStore C3: владение payload и сравнение трёх ревизий

Статус: **финальный benchmark PASS; полная acceptance C3 — PARTIAL**. Release, compatibility и адресные sanitizer-проверки закрыты. External OLE / FullValidation и ручной GUI smoke остаются отдельными незакрытыми этапами.

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

## История предыдущего кандидата

Первое полное сравнение кандидата `96911354` сохранено под
`out/perf/results-96911354`: 63 measured + 9 warm-up runs, equivalence и
immutable checks PASS. Оно выявило стабильное удорожание mixed mutation:
115→159 мс на control, 191→820 мс на large относительно C2. Причина в коде —
полный сброс metadata cache при любом facade write, после которого
`RebuildImagesInfo` декодировал также неизменённые PNG. Это finding исправлено в финальном кандидате через
выборочную invalidation; прежние результаты не удаляются и не подменяются.
Экономии steady memory на large у этого кандидата практически нет
(private after Open: C2 249.08 MiB, C3 248.71 MiB).

ASan targeted и UBSan targeted для `96911354` прошли 5/5 каждый. Полный ASan
FAST завершил 84/84 invocations: 83 PASS, один toolbar size-switch timeout
120 секунд без sanitizer stack finding. Изолированный стандартный повтор
того же case на неизменённых модулях прошёл 1/1. Исходный полный report остаётся
FAIL; отдельный retry ledger сохраняет эту оговорку.

Финальные результаты приведены ниже. Старый benchmark не используется для итоговых performance-выводов; его steady-memory метрики также подвержены обнаруженному переполнению 32-битного накопителя.
В C2 DOM уже содержал decoded SAFEARRAY, поэтому архитектурный перенос bytes
сам по себе не доказывает экономию; её должен показать фактический benchmark.
Push без отдельного указания пользователя запрещён.

## Финальный benchmark кандидата 2f0113b2

Измерения завершены 26 октября 2026 года. **Benchmark PASS**: 9/9 warm-up, 63/63 measured runs, по 7 измерений для каждой пары fixture/implementation. Все 72 equivalence records и 360 проверочных сохранений прошли XSD, decoded binary SHA-256, MIME и references. Все 2551 файл snapshots и три fixture после измерений совпали с исходным прерванным прогоном. Источники FBE и DLL/runtime snapshots во время измерений не менялись. Повторных сборок FBE, ASan, UBSan или compatibility-проверок после перезапусков benchmark не было.

Локальное evidence: `out/perf/results-2f0113b2-final64-uiwait` — manifest.json, raw.json, summary.json, per-run TSV/equivalence, regression-analysis.json, acceptance-audit.json, controller.patch. Лог: `out/perf/measured-benchmark-2f0113b2-final64-uiwait.log`. Evidence под out не добавляется в Git; таблицы ниже сохраняют итоговую статистику.

### Исправления измерителя и сохранённые прогоны

Первоначальный `results-2f0113b2` остановился по timeout 0x80070102: 9 warm-up + 17 measured. Harness не поддерживает resume. Исходный manifest сохранён без перезаписи, interruption.json явно маркирует INTERRUPTED. `results-2f0113b2-retry1` остановился с E_FAIL на Baseline/many warm-up: 3 warm-up, 0 measured; также сохранён как INTERRUPTED.

`results-2f0113b2-retry2` завершил все 72 runs с функциональным PASS, но обнаружено переполнение измерителя: Win32 SIZE_T накапливал сумму 10 отсчётов Working Set и Private Bytes в 32 битах. Среднее выше 409.6 MiB заворачивалось; встречались невозможные значения около 0.13 MiB. Прогон помечен MEASUREMENT_INVALID в measurement-review.json и целиком исключён из финальных таблиц. Ранние steady-memory таблицы, включая 96911354, нельзя принимать как корректные измерения больших состояний Source/Body. Пики OS не используют этот накопитель, но итоговые сравнения ниже целиком взяты из одного нового полного прогона, без смешивания попыток.

После завершения тех прогонов изменён только внешний benchmark-контроллер: накопители заменены на 64-битный ULONGLONG. FBE snapshots не пересобраны. `results-2f0113b2-final64` остановился на C3/many warm-up (5 warm-up), `results-2f0113b2-final64-retry1` — на Baseline/many round 4 (9 warm-up + 30 measured). Диагностика локализовала E_FAIL в SendMessageTimeout при подготовительном Source edit, Win32 error 0; причина отказа сообщения точно не установлена. Попытки сохранены как INTERRUPTED и исключены из статистики.

Для неизмеряемого ввода пробела в Source ожидание увеличено с 30 до 180 секунд и убран ранний SMTO_ABORTIFHUNG: используются SMTO_NORMAL и тот же ввод SCI_DOCUMENTEND / WM_CHAR. Измеряемые Open/Save/Replace/Source→Body и Body→Source команды не изменены. Дополнены сообщения ошибочных веток. Итоговый полный прогон после коррекции прошёл без повторов отдельных runs. Все варианты использовали один исправленный контроллер. Его локальная правка отдельна от SHA кандидата FBE; патч и хеши сохранены. Штатная компиляция внешнего контроллера не является пересборкой FBE.

### Ревизии и неизменные входы

| Вариант | Git SHA | SHA-256 FBE.exe |
|---|---|---|
| Baseline | `a4145d832b9be02930400969328b2f5a0ec54182` | `9DC6A14406771FB64378B66332B3811C432B52AB47E0C05ED4872A30C770103D` |
| C2 | `3e44dc5f621819c5a4d50009ad367e24c8ac4fea` | `23877BC1DFCF167EF104F4A9B6093FDD4F379F17E967CB81F0B739D63DDBC56B` |
| C3 | `2f0113b2566a3510c74e7e1c4536e9d10e4be0c8` | `2334A56B06D8402625A5C7C9BF3F88CE30E2260C167837C14FC8BF0A6D968D0B` |

Snapshots: `out/perf/baseline`, `out/perf/c2`, `out/perf/c3-2f0113b2`. Release Win32, v143 / VC Tools 14.44.35207; DLL inventories и bytes одинаковы. Ротация: warm-up forward, measured rounds попеременно reverse/forward; каждый round содержит три fixture и три реализации.

| Fixture | Binaries | Decoded bytes | SHA-256 FB2 |
|---|---:|---:|---|
| control | 4 | 16384 | `CC7185478E46F167EE3B2DCC81165E879B803B54D8AF6C10388BFB9B30BD97D8` |
| many | 500 | 32768000 | `BE92439E63FEC2E07B3B7C7E8B344795DC3034523F88681549302DDA9EC9E518` |
| large | 10 | 31507926 | `5902154562155727C438C7A18C16D946286926882B8059E8946784AF78F01703` |

Контроллер EXE SHA-256: `1A420C6598D1F54334B90E889890B53E54D747CB575F3863291E6FF5B32BDF61`.
Контроллер source SHA-256: `BCEE2F30CF9931FF8D805184F198A6F2A6301B078D3EA55D6EBA4C408C9A1392`.

### Что дали C2 и C3

C2 дал основное ускорение metadata workload: 98.21% на control, 99.05% на many, 99.74% на large. На many Open быстрее на 46.53%, Source→Body на 35.04%, roundtrip на 37.96%. Mixed mutation на large быстрее на 81.25%.

C3 дополнительно ускорил mixed mutation large: 175.58→145.51 мс, 17.12%. Metadata workload быстрее C2 на 1.74–4.25%; control/many mutation не улучшились существенно: +0.35% / -3.71%. Open median хуже C2: control 1487.00→1566.24 мс (-5.33%), many 2671.45→2879.86 мс (-7.80%); large лучше на 2.06%. Эти отрицательные результаты сохранены, control warning не скрыт.

Общий эффект C2+C3: metadata быстрее Baseline на 98.26–99.74%; large mutation на 84.46%; many Open на 42.35%, Source→Body на 33.70%, roundtrip на 37.98%. Replace+Save быстрее на control на 10.95%, large на 13.20%, на many практически без изменения (-1.49%). Обычный Save, Body→Source и large roundtrip практически без изменения относительно Baseline (в пределах примерно 2%). Control roundtrip хуже Baseline на 5.47% по median.

Дополнительной существенной экономии памяти C3 относительно C2 не показал. Large Private Bytes после Open: 254.89→254.32 MiB (+0.23%); после Body: 777.12→776.26 MiB (+0.11%); общий Peak Private: 1375.34→1374.90 MiB (+0.03%). Many общий Peak Private: 1176.21→1175.89 MiB (+0.03%). Large Working Set после Open снижен на 1.96%, после Body на 0.78%; общий Peak WS на 0.45%. Это перенос владения и ускорение отдельных операций, а не доказанная крупная экономия памяти.

### Ухудшения более 5% и их повторяемость

Операционный критерий стабильности в этом отчёте: ухудшение median >5% и рост >5% не менее чем в 6 из 7 пар одинакового round. Это описание повторяемости семи отсчётов, не статистический тест значимости и не доказательство причины. Все median-ухудшения >5% перечислены ниже независимо от критерия. Пересечение min/max не отменяет предупреждение.

| Fixture | Метрика | Сравнение | Рост median % | Пар >5% / 7 | Пар с любым ростом / 7 | Стабильно по критерию |
|---|---|---|---:|---:|---:|---|
| control | `open` | C2 → C3 | 5.33 | 2 | 5 | нет |
| control | `save_close_process_reopen` | Baseline → C2 | 6.36 | 5 | 7 | нет |
| control | `save_close_process_reopen` | Baseline → C3 | 5.47 | 4 | 4 | нет |
| large | `after_open_peak_private_commit` | C2 → C3 | 8.06 | 5 | 6 | нет |
| large | `after_open_sampled_peak_private_bytes` | C2 → C3 | 8.06 | 5 | 6 | нет |
| many | `after_open_peak_private_commit` | Baseline → C2 | 8.17 | 7 | 7 | да |
| many | `after_open_peak_private_commit` | Baseline → C3 | 8.10 | 7 | 7 | да |
| many | `after_open_peak_working_set` | Baseline → C2 | 7.28 | 7 | 7 | да |
| many | `after_open_peak_working_set` | Baseline → C3 | 7.38 | 7 | 7 | да |
| many | `after_open_sampled_peak_private_bytes` | Baseline → C2 | 8.05 | 7 | 7 | да |
| many | `after_open_sampled_peak_private_bytes` | Baseline → C3 | 8.17 | 7 | 7 | да |
| many | `after_reopen_peak_private_commit` | Baseline → C2 | 8.35 | 6 | 7 | да |
| many | `after_reopen_peak_private_commit` | Baseline → C3 | 8.23 | 6 | 7 | да |
| many | `after_reopen_peak_working_set` | Baseline → C2 | 7.51 | 5 | 7 | нет |
| many | `after_reopen_peak_working_set` | Baseline → C3 | 7.32 | 5 | 7 | нет |
| many | `after_reopen_sampled_peak_private_bytes` | Baseline → C2 | 8.25 | 6 | 7 | да |
| many | `after_reopen_sampled_peak_private_bytes` | Baseline → C3 | 8.25 | 6 | 7 | да |
| many | `open` | C2 → C3 | 7.80 | 2 | 3 | нет |

Стабильные регрессии памяти есть: many, пики после Open относительно Baseline — Peak Private +8.17% у C2 / +8.10% у C3; Peak Working Set +7.28% / +7.38%, во всех 7 парах рост >5%, диапазоны min/max не пересекаются. После Reopen Peak Private +8.35% / +8.23% (6/7 пар >5%). Это уже эффект C2; C3 его практически сохраняет. Sampled private peak подтверждает направление, но это связанный счётчик того же процесса, не независимое доказательство.

Дополнительно large Peak Private после Open у C3 хуже C2 на 8.06% (259.50→280.41 MiB, 5/7 пар >5%, 6/7 с ростом); это остаётся предупреждением. Общий пик за полный сценарий практически одинаков. Временных регрессий >5%, удовлетворяющих критерию 6/7, не обнаружено. Open control и many хуже по median, но >5% рост встречается в 2/7 парах каждый (любой рост 5/7 и 3/7 соответственно). Нельзя заявлять стабильное ускорение Open у C3 или считать предупреждения закрытыми.

### Полные сравнительные таблицы

Каждая ячейка варианта: **median / p90 / min / max**. N показано отдельно для обоих вариантов. Время — ms, память — MiB (2^20 bytes). Δ% по median: `(old-new)/old*100`; плюс — быстрее/меньше, минус — медленнее/больше. Округление только при выводе; исходные значения сохранены в JSON/TSV. p90 nearest rank, при N=7 совпадает с max. Warm-up исключён. Пики после этапов накопительные для данного процесса; final_* относится к исходному процессу, reopen_final_* — к новому процессу после roundtrip. Sampled peak выводится отдельно от OS Peak Private.

#### Baseline → C2

Время, ms:

| Fixture | Метрика | Baseline median / p90 / min / max | C2 median / p90 / min / max | N old/new | Δ median % |
|---|---|---:|---:|---:|---:|
| control | Body→Source | 28.24 / 30.31 / 27.52 / 30.31 | 27.61 / 29.96 / 26.66 / 29.96 | 7/7 | +2.24 |
| control | metadata workload (1000 queries) | 3139.18 / 3610.03 / 3105.66 / 3610.03 | 56.12 / 60.93 / 53.97 / 60.93 | 7/7 | +98.21 |
| control | mutation workload (5 API + 5 raw writes) | 165.48 / 176.28 / 159.40 / 176.28 | 114.52 / 122.39 / 112.13 / 122.39 | 7/7 | +30.80 |
| control | Open | 1837.51 / 1916.52 / 1461.80 / 1916.52 | 1487.00 / 1893.98 / 1464.81 / 1893.98 | 7/7 | +19.08 |
| control | Replace+Save | 105.60 / 113.45 / 102.49 / 113.45 | 93.30 / 103.03 / 91.05 / 103.03 | 7/7 | +11.65 |
| control | Save | 25.22 / 25.77 / 24.78 / 25.77 | 26.16 / 28.76 / 24.95 / 28.76 | 7/7 | -3.71 |
| control | Save→Close→process restart→ready | 1930.55 / 2187.03 / 1863.31 / 2187.03 | 2053.33 / 2259.12 / 2014.34 / 2259.12 | 7/7 | -6.36 |
| control | Source→Body | 117.44 / 1397.82 / 111.24 / 1397.82 | 112.32 / 114.95 / 109.77 / 114.95 | 7/7 | +4.36 |
| large | Body→Source | 743.43 / 753.99 / 728.39 / 753.99 | 731.63 / 739.09 / 725.83 / 739.09 | 7/7 | +1.59 |
| large | metadata workload (1000 queries) | 20243.20 / 20847.90 / 20122.00 / 20847.90 | 52.71 / 53.84 / 51.34 / 53.84 | 7/7 | +99.74 |
| large | mutation workload (5 API + 5 raw writes) | 936.67 / 997.86 / 925.33 / 997.86 | 175.58 / 183.98 / 173.51 / 183.98 | 7/7 | +81.25 |
| large | Open | 2767.12 / 3307.08 / 2661.86 / 3307.08 | 2761.70 / 3056.51 / 2627.51 / 3056.51 | 7/7 | +0.20 |
| large | Replace+Save | 1675.87 / 1800.39 / 1567.17 / 1800.39 | 1491.94 / 1543.97 / 1391.04 / 1543.97 | 7/7 | +10.98 |
| large | Save | 1508.43 / 1550.32 / 1438.27 / 1550.32 | 1525.62 / 1584.69 / 1432.69 / 1584.69 | 7/7 | -1.14 |
| large | Save→Close→process restart→ready | 3901.71 / 4166.12 / 3631.00 / 4166.12 | 3810.34 / 4147.94 / 3689.68 / 4147.94 | 7/7 | +2.34 |
| large | Source→Body | 1039.10 / 4434.47 / 969.39 / 4434.47 | 979.41 / 4394.21 / 968.78 / 4394.21 | 7/7 | +5.74 |
| many | Body→Source | 781.44 / 793.40 / 761.61 / 793.40 | 773.87 / 794.47 / 754.75 / 794.47 | 7/7 | +0.97 |
| many | metadata workload (1000 queries) | 7036.81 / 7889.22 / 6911.83 / 7889.22 | 67.08 / 72.41 / 62.83 / 72.41 | 7/7 | +99.05 |
| many | mutation workload (5 API + 5 raw writes) | 422.98 / 469.49 / 408.78 / 469.49 | 398.13 / 607.80 / 391.27 / 607.80 | 7/7 | +5.87 |
| many | Open | 4995.85 / 5746.18 / 4847.70 / 5746.18 | 2671.45 / 3004.69 / 2584.46 / 3004.69 | 7/7 | +46.53 |
| many | Replace+Save | 1179.81 / 1240.37 / 1083.48 / 1240.37 | 1185.74 / 1229.93 / 1082.10 / 1229.93 | 7/7 | -0.50 |
| many | Save | 902.44 / 1001.78 / 791.15 / 1001.78 | 882.95 / 907.96 / 789.87 / 907.96 | 7/7 | +2.16 |
| many | Save→Close→process restart→ready | 6188.17 / 7071.47 / 6000.60 / 7071.47 | 3839.42 / 4177.82 / 3716.37 / 4177.82 | 7/7 | +37.96 |
| many | Source→Body | 6957.77 / 7307.87 / 6783.61 / 7307.87 | 4519.85 / 4747.35 / 4493.14 / 4747.35 | 7/7 | +35.04 |

Память, MiB: Private Bytes, Working Set, OS peaks и sampled private peak на всех этапах:

| Fixture | Метрика | Baseline median / p90 / min / max | C2 median / p90 / min / max | N old/new | Δ median % |
|---|---|---:|---:|---:|---:|
| control | `after_body_peak_private_commit` | 51.15 / 52.14 / 50.71 / 52.14 | 51.01 / 53.66 / 50.84 / 53.66 | 7/7 | +0.27 |
| control | `after_body_peak_working_set` | 91.41 / 91.60 / 90.80 / 91.60 | 91.00 / 91.17 / 90.78 / 91.17 | 7/7 | +0.44 |
| control | `after_body_private_bytes` | 51.10 / 52.06 / 50.68 / 52.06 | 50.93 / 53.55 / 50.68 / 53.55 | 7/7 | +0.34 |
| control | `after_body_sampled_peak_private_bytes` | 51.10 / 52.07 / 50.68 / 52.07 | 50.96 / 53.55 / 50.68 / 53.55 | 7/7 | +0.28 |
| control | `after_body_working_set` | 91.23 / 91.44 / 90.66 / 91.44 | 90.83 / 91.00 / 90.62 / 91.00 | 7/7 | +0.45 |
| control | `after_close_peak_private_commit` | 51.20 / 65.64 / 50.39 / 65.64 | 50.82 / 65.25 / 50.47 / 65.25 | 7/7 | +0.73 |
| control | `after_close_peak_working_set` | 89.97 / 90.41 / 89.38 / 90.41 | 89.79 / 90.15 / 89.45 / 90.15 | 7/7 | +0.21 |
| control | `after_close_private_bytes` | 50.52 / 64.65 / 49.52 / 64.65 | 50.28 / 64.68 / 49.87 / 64.68 | 7/7 | +0.47 |
| control | `after_close_sampled_peak_private_bytes` | 51.00 / 65.12 / 50.18 / 65.12 | 50.71 / 65.16 / 50.05 / 65.16 | 7/7 | +0.57 |
| control | `after_close_working_set` | 89.62 / 89.95 / 88.76 / 89.95 | 89.47 / 89.86 / 89.11 / 89.86 | 7/7 | +0.17 |
| control | `after_open_peak_private_commit` | 47.94 / 48.73 / 47.45 / 48.73 | 47.95 / 51.21 / 47.71 / 51.21 | 7/7 | -0.02 |
| control | `after_open_peak_working_set` | 86.38 / 86.61 / 85.89 / 86.61 | 86.39 / 86.50 / 86.18 / 86.50 | 7/7 | -0.01 |
| control | `after_open_private_bytes` | 47.79 / 48.68 / 47.26 / 48.68 | 47.76 / 50.78 / 47.56 / 50.78 | 7/7 | +0.05 |
| control | `after_open_sampled_peak_private_bytes` | 47.93 / 48.72 / 47.45 / 48.72 | 47.95 / 51.21 / 47.71 / 51.21 | 7/7 | -0.04 |
| control | `after_open_working_set` | 86.28 / 86.50 / 85.82 / 86.50 | 86.23 / 86.41 / 86.06 / 86.41 | 7/7 | +0.05 |
| control | `after_reopen_peak_private_commit` | 48.20 / 62.73 / 47.66 / 62.73 | 48.00 / 63.18 / 47.57 / 63.18 | 7/7 | +0.42 |
| control | `after_reopen_peak_working_set` | 86.36 / 86.63 / 86.17 / 86.63 | 86.36 / 86.68 / 85.84 / 86.68 | 7/7 | 0.00 |
| control | `after_reopen_private_bytes` | 47.86 / 62.69 / 47.41 / 62.69 | 47.71 / 62.42 / 47.44 / 62.42 | 7/7 | +0.33 |
| control | `after_reopen_sampled_peak_private_bytes` | 48.20 / 62.72 / 47.65 / 62.72 | 47.99 / 63.17 / 47.57 / 63.17 | 7/7 | +0.44 |
| control | `after_reopen_working_set` | 86.25 / 86.43 / 86.08 / 86.43 | 86.15 / 86.41 / 85.72 / 86.41 | 7/7 | +0.11 |
| control | `after_source_peak_private_commit` | 49.42 / 50.47 / 49.17 / 50.47 | 49.80 / 52.69 / 49.12 / 52.69 | 7/7 | -0.78 |
| control | `after_source_peak_working_set` | 89.16 / 89.37 / 88.93 / 89.37 | 89.11 / 89.16 / 88.76 / 89.16 | 7/7 | +0.06 |
| control | `after_source_private_bytes` | 49.23 / 50.33 / 48.81 / 50.33 | 49.29 / 52.27 / 48.93 / 52.27 | 7/7 | -0.12 |
| control | `after_source_sampled_peak_private_bytes` | 49.38 / 50.33 / 49.17 / 50.33 | 49.59 / 52.57 / 49.12 / 52.57 | 7/7 | -0.43 |
| control | `after_source_working_set` | 89.16 / 89.37 / 88.80 / 89.37 | 88.98 / 89.16 / 88.76 / 89.16 | 7/7 | +0.20 |
| control | `final_peak_private_commit` | 53.42 / 54.52 / 53.20 / 54.52 | 53.36 / 56.27 / 52.97 / 56.27 | 7/7 | +0.12 |
| control | `final_peak_working_set` | 93.10 / 93.77 / 92.80 / 93.77 | 92.91 / 93.17 / 92.49 / 93.17 | 7/7 | +0.21 |
| control | `reopen_final_peak_private_commit` | 51.20 / 65.64 / 50.39 / 65.64 | 50.82 / 65.25 / 50.47 / 65.25 | 7/7 | +0.73 |
| control | `reopen_final_peak_working_set` | 89.97 / 90.41 / 89.38 / 90.41 | 89.79 / 90.15 / 89.45 / 90.15 | 7/7 | +0.21 |
| large | `after_body_peak_private_commit` | 1402.61 / 1417.17 / 1311.93 / 1417.17 | 1375.34 / 1393.32 / 1371.09 / 1393.32 | 7/7 | +1.94 |
| large | `after_body_peak_working_set` | 1399.91 / 1414.25 / 1309.71 / 1414.25 | 1372.67 / 1390.61 / 1367.00 / 1390.61 | 7/7 | +1.95 |
| large | `after_body_private_bytes` | 779.24 / 779.82 / 749.21 / 779.82 | 777.12 / 856.93 / 775.88 / 856.93 | 7/7 | +0.27 |
| large | `after_body_sampled_peak_private_bytes` | 1401.81 / 1417.14 / 1300.88 / 1417.14 | 1375.25 / 1393.26 / 1363.85 / 1393.26 | 7/7 | +1.90 |
| large | `after_body_working_set` | 785.94 / 786.17 / 755.85 / 786.17 | 783.32 / 863.44 / 782.77 / 863.44 | 7/7 | +0.33 |
| large | `after_close_peak_private_commit` | 326.24 / 339.82 / 325.65 / 339.82 | 325.52 / 330.90 / 325.38 / 330.90 | 7/7 | +0.22 |
| large | `after_close_peak_working_set` | 367.02 / 367.20 / 366.48 / 367.20 | 366.96 / 367.24 / 366.88 / 367.24 | 7/7 | +0.02 |
| large | `after_close_private_bytes` | 124.39 / 137.38 / 122.69 / 137.38 | 122.99 / 128.90 / 122.54 / 128.90 | 7/7 | +1.13 |
| large | `after_close_sampled_peak_private_bytes` | 326.23 / 339.78 / 323.00 / 339.78 | 325.34 / 330.86 / 320.03 / 330.86 | 7/7 | +0.28 |
| large | `after_close_working_set` | 162.11 / 163.25 / 161.86 / 163.25 | 162.12 / 163.56 / 161.84 / 163.56 | 7/7 | -0.01 |
| large | `after_open_peak_private_commit` | 272.77 / 281.39 / 263.70 / 281.39 | 259.50 / 282.04 / 259.02 / 282.04 | 7/7 | +4.86 |
| large | `after_open_peak_working_set` | 309.93 / 321.21 / 303.49 / 321.21 | 303.73 / 322.19 / 303.54 / 322.19 | 7/7 | +2.00 |
| large | `after_open_private_bytes` | 254.86 / 270.04 / 164.76 / 270.04 | 254.89 / 273.33 / 250.04 / 273.33 | 7/7 | -0.01 |
| large | `after_open_sampled_peak_private_bytes` | 272.71 / 281.32 / 260.23 / 281.32 | 259.50 / 282.04 / 259.02 / 282.04 | 7/7 | +4.84 |
| large | `after_open_working_set` | 301.20 / 316.35 / 211.61 / 316.35 | 301.36 / 319.68 / 295.18 / 319.68 | 7/7 | -0.05 |
| large | `after_reopen_peak_private_commit` | 259.70 / 272.89 / 257.98 / 272.89 | 258.33 / 263.45 / 258.10 / 263.45 | 7/7 | +0.53 |
| large | `after_reopen_peak_working_set` | 298.71 / 298.91 / 298.21 / 298.91 | 295.71 / 295.93 / 295.62 / 295.93 | 7/7 | +1.00 |
| large | `after_reopen_private_bytes` | 254.78 / 268.21 / 253.95 / 268.21 | 251.09 / 256.61 / 250.88 / 256.61 | 7/7 | +1.45 |
| large | `after_reopen_sampled_peak_private_bytes` | 258.61 / 272.89 / 257.98 / 272.89 | 258.27 / 263.45 / 253.94 / 263.45 | 7/7 | +0.13 |
| large | `after_reopen_working_set` | 296.27 / 296.44 / 295.77 / 296.44 | 293.39 / 293.51 / 293.15 / 293.51 | 7/7 | +0.97 |
| large | `after_source_peak_private_commit` | 1026.88 / 1041.56 / 936.22 / 1041.56 | 999.70 / 1017.73 / 995.09 / 1017.73 | 7/7 | +2.65 |
| large | `after_source_peak_working_set` | 1001.02 / 1015.55 / 910.68 / 1015.55 | 973.68 / 991.89 / 967.86 / 991.89 | 7/7 | +2.73 |
| large | `after_source_private_bytes` | 838.14 / 852.79 / 747.87 / 852.79 | 810.78 / 829.13 / 806.44 / 829.13 | 7/7 | +3.26 |
| large | `after_source_sampled_peak_private_bytes` | 1026.83 / 1041.50 / 936.17 / 1041.50 | 999.68 / 1017.70 / 995.09 / 1017.70 | 7/7 | +2.64 |
| large | `after_source_working_set` | 848.88 / 863.29 / 758.89 / 863.29 | 821.54 / 839.74 / 815.73 / 839.74 | 7/7 | +3.22 |
| large | `final_peak_private_commit` | 1402.61 / 1417.17 / 1311.93 / 1417.17 | 1375.34 / 1393.32 / 1371.09 / 1393.32 | 7/7 | +1.94 |
| large | `final_peak_working_set` | 1399.91 / 1414.25 / 1309.71 / 1414.25 | 1372.67 / 1390.61 / 1367.00 / 1390.61 | 7/7 | +1.95 |
| large | `reopen_final_peak_private_commit` | 326.24 / 339.82 / 325.65 / 339.82 | 325.52 / 330.90 / 325.38 / 330.90 | 7/7 | +0.22 |
| large | `reopen_final_peak_working_set` | 367.02 / 367.20 / 366.48 / 367.20 | 366.96 / 367.24 / 366.88 / 367.24 | 7/7 | +0.02 |
| many | `after_body_peak_private_commit` | 1176.35 / 1262.68 / 1161.81 / 1262.68 | 1176.21 / 1176.85 / 1163.59 / 1176.85 | 7/7 | +0.01 |
| many | `after_body_peak_working_set` | 1169.09 / 1253.22 / 1154.25 / 1253.22 | 1169.64 / 1170.05 / 1157.16 / 1170.05 | 7/7 | -0.05 |
| many | `after_body_private_bytes` | 872.44 / 956.50 / 870.80 / 956.50 | 888.62 / 889.71 / 884.99 / 889.71 | 7/7 | -1.85 |
| many | `after_body_sampled_peak_private_bytes` | 1173.36 / 1261.33 / 1160.13 / 1261.33 | 1172.68 / 1175.80 / 1159.04 / 1175.80 | 7/7 | +0.06 |
| many | `after_body_working_set` | 865.87 / 947.95 / 863.59 / 947.95 | 881.62 / 882.18 / 879.01 / 882.18 | 7/7 | -1.82 |
| many | `after_close_peak_private_commit` | 388.43 / 402.15 / 387.05 / 402.15 | 380.54 / 383.60 / 380.43 / 383.60 | 7/7 | +2.03 |
| many | `after_close_peak_working_set` | 425.88 / 426.27 / 423.79 / 426.27 | 418.46 / 419.04 / 418.30 / 419.04 | 7/7 | +1.74 |
| many | `after_close_private_bytes` | 153.48 / 169.84 / 150.11 / 169.84 | 145.90 / 151.94 / 142.49 / 151.94 | 7/7 | +4.94 |
| many | `after_close_sampled_peak_private_bytes` | 388.12 / 402.14 / 365.75 / 402.14 | 380.50 / 383.56 / 357.40 / 383.56 | 7/7 | +1.96 |
| many | `after_close_working_set` | 191.90 / 192.74 / 187.86 / 192.74 | 181.84 / 185.49 / 181.12 / 185.49 | 7/7 | +5.24 |
| many | `after_open_peak_private_commit` | 269.62 / 270.93 / 269.18 / 270.93 | 291.65 / 292.63 / 291.04 / 292.63 | 7/7 | -8.17 |
| many | `after_open_peak_working_set` | 301.26 / 306.96 / 301.12 / 306.96 | 323.20 / 324.16 / 323.02 / 324.16 | 7/7 | -7.28 |
| many | `after_open_private_bytes` | 184.87 / 267.91 / 183.76 / 267.91 | 180.29 / 181.70 / 179.21 / 181.70 | 7/7 | +2.48 |
| many | `after_open_sampled_peak_private_bytes` | 269.45 / 270.93 / 269.02 / 270.93 | 291.14 / 292.38 / 290.60 / 292.38 | 7/7 | -8.05 |
| many | `after_open_working_set` | 222.98 / 306.23 / 222.07 / 306.23 | 219.13 / 219.43 / 218.10 / 219.43 | 7/7 | +1.73 |
| many | `after_reopen_peak_private_commit` | 268.46 / 282.87 / 267.92 / 282.87 | 290.89 / 293.75 / 290.75 / 293.75 | 7/7 | -8.35 |
| many | `after_reopen_peak_working_set` | 300.47 / 310.25 / 299.85 / 310.25 | 323.05 / 323.28 / 322.61 / 323.28 | 7/7 | -7.51 |
| many | `after_reopen_private_bytes` | 186.21 / 270.59 / 183.63 / 270.59 | 180.38 / 183.59 / 179.62 / 183.59 | 7/7 | +3.13 |
| many | `after_reopen_sampled_peak_private_bytes` | 268.18 / 282.72 / 267.67 / 282.72 | 290.29 / 293.73 / 289.92 / 293.73 | 7/7 | -8.25 |
| many | `after_reopen_working_set` | 224.85 / 308.69 / 221.73 / 308.69 | 218.70 / 219.43 / 218.29 / 219.43 | 7/7 | +2.74 |
| many | `after_source_peak_private_commit` | 803.40 / 886.57 / 789.58 / 886.57 | 796.19 / 797.13 / 783.45 / 797.13 | 7/7 | +0.90 |
| many | `after_source_peak_working_set` | 758.08 / 841.29 / 744.09 / 841.29 | 751.57 / 751.99 / 738.92 / 751.99 | 7/7 | +0.86 |
| many | `after_source_private_bytes` | 571.28 / 653.41 / 556.31 / 653.41 | 564.63 / 566.10 / 552.27 / 566.10 | 7/7 | +1.16 |
| many | `after_source_sampled_peak_private_bytes` | 803.34 / 886.53 / 789.56 / 886.53 | 796.16 / 797.11 / 783.40 / 797.11 | 7/7 | +0.89 |
| many | `after_source_working_set` | 568.33 / 650.58 / 553.27 / 650.58 | 562.35 / 564.05 / 550.21 / 564.05 | 7/7 | +1.05 |
| many | `final_peak_private_commit` | 1176.35 / 1262.68 / 1161.81 / 1262.68 | 1176.21 / 1176.85 / 1173.91 / 1176.85 | 7/7 | +0.01 |
| many | `final_peak_working_set` | 1169.09 / 1253.22 / 1154.25 / 1253.22 | 1169.64 / 1170.05 / 1167.60 / 1170.05 | 7/7 | -0.05 |
| many | `reopen_final_peak_private_commit` | 388.43 / 402.15 / 387.05 / 402.15 | 380.54 / 383.60 / 380.43 / 383.60 | 7/7 | +2.03 |
| many | `reopen_final_peak_working_set` | 425.88 / 426.27 / 423.79 / 426.27 | 418.46 / 419.04 / 418.30 / 419.04 | 7/7 | +1.74 |

#### C2 → C3

Время, ms:

| Fixture | Метрика | C2 median / p90 / min / max | C3 median / p90 / min / max | N old/new | Δ median % |
|---|---|---:|---:|---:|---:|
| control | Body→Source | 27.61 / 29.96 / 26.66 / 29.96 | 28.44 / 30.79 / 27.41 / 30.79 | 7/7 | -3.00 |
| control | metadata workload (1000 queries) | 56.12 / 60.93 / 53.97 / 60.93 | 54.73 / 56.77 / 53.58 / 56.77 | 7/7 | +2.47 |
| control | mutation workload (5 API + 5 raw writes) | 114.52 / 122.39 / 112.13 / 122.39 | 114.11 / 124.93 / 111.33 / 124.93 | 7/7 | +0.35 |
| control | Open | 1487.00 / 1893.98 / 1464.81 / 1893.98 | 1566.24 / 1671.83 / 1500.78 / 1671.83 | 7/7 | -5.33 |
| control | Replace+Save | 93.30 / 103.03 / 91.05 / 103.03 | 94.04 / 99.03 / 92.88 / 99.03 | 7/7 | -0.79 |
| control | Save | 26.16 / 28.76 / 24.95 / 28.76 | 24.99 / 26.86 / 23.29 / 26.86 | 7/7 | +4.44 |
| control | Save→Close→process restart→ready | 2053.33 / 2259.12 / 2014.34 / 2259.12 | 2036.06 / 2117.75 / 1959.52 / 2117.75 | 7/7 | +0.84 |
| control | Source→Body | 112.32 / 114.95 / 109.77 / 114.95 | 115.69 / 117.42 / 111.95 / 117.42 | 7/7 | -3.01 |
| large | Body→Source | 731.63 / 739.09 / 725.83 / 739.09 | 735.05 / 751.07 / 733.35 / 751.07 | 7/7 | -0.47 |
| large | metadata workload (1000 queries) | 52.71 / 53.84 / 51.34 / 53.84 | 51.79 / 53.47 / 49.65 / 53.47 | 7/7 | +1.74 |
| large | mutation workload (5 API + 5 raw writes) | 175.58 / 183.98 / 173.51 / 183.98 | 145.51 / 151.20 / 141.04 / 151.20 | 7/7 | +17.12 |
| large | Open | 2761.70 / 3056.51 / 2627.51 / 3056.51 | 2704.91 / 2924.17 / 2631.18 / 2924.17 | 7/7 | +2.06 |
| large | Replace+Save | 1491.94 / 1543.97 / 1391.04 / 1543.97 | 1454.69 / 1533.79 / 1403.33 / 1533.79 | 7/7 | +2.50 |
| large | Save | 1525.62 / 1584.69 / 1432.69 / 1584.69 | 1534.81 / 1610.28 / 1448.91 / 1610.28 | 7/7 | -0.60 |
| large | Save→Close→process restart→ready | 3810.34 / 4147.94 / 3689.68 / 4147.94 | 3900.24 / 4173.32 / 3580.34 / 4173.32 | 7/7 | -2.36 |
| large | Source→Body | 979.41 / 4394.21 / 968.78 / 4394.21 | 961.20 / 4488.33 / 933.18 / 4488.33 | 7/7 | +1.86 |
| many | Body→Source | 773.87 / 794.47 / 754.75 / 794.47 | 784.84 / 789.21 / 761.27 / 789.21 | 7/7 | -1.42 |
| many | metadata workload (1000 queries) | 67.08 / 72.41 / 62.83 / 72.41 | 64.23 / 66.40 / 63.01 / 66.40 | 7/7 | +4.25 |
| many | mutation workload (5 API + 5 raw writes) | 398.13 / 607.80 / 391.27 / 607.80 | 412.90 / 601.46 / 391.09 / 601.46 | 7/7 | -3.71 |
| many | Open | 2671.45 / 3004.69 / 2584.46 / 3004.69 | 2879.86 / 3047.05 / 2621.48 / 3047.05 | 7/7 | -7.80 |
| many | Replace+Save | 1185.74 / 1229.93 / 1082.10 / 1229.93 | 1197.42 / 1243.14 / 1096.65 / 1243.14 | 7/7 | -0.99 |
| many | Save | 882.95 / 907.96 / 789.87 / 907.96 | 889.86 / 925.16 / 791.89 / 925.16 | 7/7 | -0.78 |
| many | Save→Close→process restart→ready | 3839.42 / 4177.82 / 3716.37 / 4177.82 | 3837.84 / 4142.43 / 3752.63 / 4142.43 | 7/7 | +0.04 |
| many | Source→Body | 4519.85 / 4747.35 / 4493.14 / 4747.35 | 4612.96 / 4756.23 / 4489.54 / 4756.23 | 7/7 | -2.06 |

Память, MiB: Private Bytes, Working Set, OS peaks и sampled private peak на всех этапах:

| Fixture | Метрика | C2 median / p90 / min / max | C3 median / p90 / min / max | N old/new | Δ median % |
|---|---|---:|---:|---:|---:|
| control | `after_body_peak_private_commit` | 51.01 / 53.66 / 50.84 / 53.66 | 50.77 / 53.81 / 50.56 / 53.81 | 7/7 | +0.48 |
| control | `after_body_peak_working_set` | 91.00 / 91.17 / 90.78 / 91.17 | 90.88 / 91.21 / 90.62 / 91.21 | 7/7 | +0.13 |
| control | `after_body_private_bytes` | 50.93 / 53.55 / 50.68 / 53.55 | 50.74 / 53.63 / 50.49 / 53.63 | 7/7 | +0.38 |
| control | `after_body_sampled_peak_private_bytes` | 50.96 / 53.55 / 50.68 / 53.55 | 50.75 / 53.63 / 50.49 / 53.63 | 7/7 | +0.41 |
| control | `after_body_working_set` | 90.83 / 91.00 / 90.62 / 91.00 | 90.75 / 91.04 / 90.48 / 91.04 | 7/7 | +0.09 |
| control | `after_close_peak_private_commit` | 50.82 / 65.25 / 50.47 / 65.25 | 50.79 / 65.25 / 50.59 / 65.25 | 7/7 | +0.06 |
| control | `after_close_peak_working_set` | 89.79 / 90.15 / 89.45 / 90.15 | 89.70 / 89.98 / 89.40 / 89.98 | 7/7 | +0.09 |
| control | `after_close_private_bytes` | 50.28 / 64.68 / 49.87 / 64.68 | 50.55 / 64.46 / 49.81 / 64.46 | 7/7 | -0.54 |
| control | `after_close_sampled_peak_private_bytes` | 50.71 / 65.16 / 50.05 / 65.16 | 50.61 / 65.00 / 50.51 / 65.00 | 7/7 | +0.21 |
| control | `after_close_working_set` | 89.47 / 89.86 / 89.11 / 89.86 | 89.36 / 89.98 / 89.08 / 89.98 | 7/7 | +0.13 |
| control | `after_open_peak_private_commit` | 47.95 / 51.21 / 47.71 / 51.21 | 47.95 / 50.54 / 47.64 / 50.54 | 7/7 | -0.01 |
| control | `after_open_peak_working_set` | 86.39 / 86.50 / 86.18 / 86.50 | 86.27 / 86.54 / 86.14 / 86.54 | 7/7 | +0.14 |
| control | `after_open_private_bytes` | 47.76 / 50.78 / 47.56 / 50.78 | 47.70 / 50.50 / 47.57 / 50.50 | 7/7 | +0.14 |
| control | `after_open_sampled_peak_private_bytes` | 47.95 / 51.21 / 47.71 / 51.21 | 47.95 / 50.54 / 47.64 / 50.54 | 7/7 | 0.00 |
| control | `after_open_working_set` | 86.23 / 86.41 / 86.06 / 86.41 | 86.16 / 86.45 / 86.05 / 86.45 | 7/7 | +0.09 |
| control | `after_reopen_peak_private_commit` | 48.00 / 63.18 / 47.57 / 63.18 | 48.05 / 62.48 / 47.58 / 62.48 | 7/7 | -0.11 |
| control | `after_reopen_peak_working_set` | 86.36 / 86.68 / 85.84 / 86.68 | 86.34 / 86.43 / 85.87 / 86.43 | 7/7 | +0.02 |
| control | `after_reopen_private_bytes` | 47.71 / 62.42 / 47.44 / 62.42 | 47.87 / 62.43 / 47.54 / 62.43 | 7/7 | -0.35 |
| control | `after_reopen_sampled_peak_private_bytes` | 47.99 / 63.17 / 47.57 / 63.17 | 48.05 / 62.48 / 47.58 / 62.48 | 7/7 | -0.13 |
| control | `after_reopen_working_set` | 86.15 / 86.41 / 85.72 / 86.41 | 86.26 / 86.34 / 85.81 / 86.34 | 7/7 | -0.13 |
| control | `after_source_peak_private_commit` | 49.80 / 52.69 / 49.12 / 52.69 | 49.64 / 52.67 / 49.14 / 52.67 | 7/7 | +0.32 |
| control | `after_source_peak_working_set` | 89.11 / 89.16 / 88.76 / 89.16 | 89.04 / 89.27 / 88.75 / 89.27 | 7/7 | +0.07 |
| control | `after_source_private_bytes` | 49.29 / 52.27 / 48.93 / 52.27 | 49.20 / 52.25 / 48.93 / 52.25 | 7/7 | +0.19 |
| control | `after_source_sampled_peak_private_bytes` | 49.59 / 52.57 / 49.12 / 52.57 | 49.61 / 52.54 / 49.11 / 52.54 | 7/7 | -0.06 |
| control | `after_source_working_set` | 88.98 / 89.16 / 88.76 / 89.16 | 88.89 / 89.27 / 88.75 / 89.27 | 7/7 | +0.10 |
| control | `final_peak_private_commit` | 53.36 / 56.27 / 52.97 / 56.27 | 53.07 / 55.86 / 52.32 / 55.86 | 7/7 | +0.53 |
| control | `final_peak_working_set` | 92.91 / 93.17 / 92.49 / 93.17 | 92.62 / 92.87 / 92.18 / 92.87 | 7/7 | +0.32 |
| control | `reopen_final_peak_private_commit` | 50.82 / 65.25 / 50.47 / 65.25 | 50.79 / 65.25 / 50.59 / 65.25 | 7/7 | +0.06 |
| control | `reopen_final_peak_working_set` | 89.79 / 90.15 / 89.45 / 90.15 | 89.70 / 89.98 / 89.40 / 89.98 | 7/7 | +0.09 |
| large | `after_body_peak_private_commit` | 1375.34 / 1393.32 / 1371.09 / 1393.32 | 1374.90 / 1389.01 / 1365.28 / 1389.01 | 7/7 | +0.03 |
| large | `after_body_peak_working_set` | 1372.67 / 1390.61 / 1367.00 / 1390.61 | 1366.49 / 1385.05 / 1362.81 / 1385.05 | 7/7 | +0.45 |
| large | `after_body_private_bytes` | 777.12 / 856.93 / 775.88 / 856.93 | 776.26 / 790.94 / 748.97 / 790.94 | 7/7 | +0.11 |
| large | `after_body_sampled_peak_private_bytes` | 1375.25 / 1393.26 / 1363.85 / 1393.26 | 1374.89 / 1389.00 / 1360.80 / 1389.00 | 7/7 | +0.03 |
| large | `after_body_working_set` | 783.32 / 863.44 / 782.77 / 863.44 | 777.22 / 783.21 / 755.49 / 783.21 | 7/7 | +0.78 |
| large | `after_close_peak_private_commit` | 325.52 / 330.90 / 325.38 / 330.90 | 325.61 / 326.61 / 325.00 / 326.61 | 7/7 | -0.03 |
| large | `after_close_peak_working_set` | 366.96 / 367.24 / 366.88 / 367.24 | 366.97 / 367.15 / 366.40 / 367.15 | 7/7 | 0.00 |
| large | `after_close_private_bytes` | 122.99 / 128.90 / 122.54 / 128.90 | 122.91 / 123.71 / 122.61 / 123.71 | 7/7 | +0.07 |
| large | `after_close_sampled_peak_private_bytes` | 325.34 / 330.86 / 320.03 / 330.86 | 325.04 / 326.55 / 322.75 / 326.55 | 7/7 | +0.09 |
| large | `after_close_working_set` | 162.12 / 163.56 / 161.84 / 163.56 | 162.17 / 162.73 / 161.61 / 162.73 | 7/7 | -0.03 |
| large | `after_open_peak_private_commit` | 259.50 / 282.04 / 259.02 / 282.04 | 280.41 / 295.12 / 265.41 / 295.12 | 7/7 | -8.06 |
| large | `after_open_peak_working_set` | 303.73 / 322.19 / 303.54 / 322.19 | 306.52 / 313.62 / 306.25 / 313.62 | 7/7 | -0.92 |
| large | `after_open_private_bytes` | 254.89 / 273.33 / 250.04 / 273.33 | 254.32 / 267.73 / 245.79 / 267.73 | 7/7 | +0.23 |
| large | `after_open_sampled_peak_private_bytes` | 259.50 / 282.04 / 259.02 / 282.04 | 280.41 / 295.12 / 265.41 / 295.12 | 7/7 | -8.06 |
| large | `after_open_working_set` | 301.36 / 319.68 / 295.18 / 319.68 | 295.46 / 313.53 / 292.68 / 313.53 | 7/7 | +1.96 |
| large | `after_reopen_peak_private_commit` | 258.33 / 263.45 / 258.10 / 263.45 | 258.30 / 259.38 / 258.16 / 259.38 | 7/7 | +0.01 |
| large | `after_reopen_peak_working_set` | 295.71 / 295.93 / 295.62 / 295.93 | 293.25 / 293.49 / 292.76 / 293.49 | 7/7 | +0.83 |
| large | `after_reopen_private_bytes` | 251.09 / 256.61 / 250.88 / 256.61 | 250.82 / 252.11 / 250.41 / 252.11 | 7/7 | +0.11 |
| large | `after_reopen_sampled_peak_private_bytes` | 258.27 / 263.45 / 253.94 / 263.45 | 258.30 / 259.38 / 250.98 / 259.38 | 7/7 | -0.01 |
| large | `after_reopen_working_set` | 293.39 / 293.51 / 293.15 / 293.51 | 293.16 / 293.43 / 292.64 / 293.43 | 7/7 | +0.08 |
| large | `after_source_peak_private_commit` | 999.70 / 1017.73 / 995.09 / 1017.73 | 999.79 / 1013.28 / 990.21 / 1013.28 | 7/7 | -0.01 |
| large | `after_source_peak_working_set` | 973.68 / 991.89 / 967.86 / 991.89 | 967.61 / 986.16 / 964.58 / 986.16 | 7/7 | +0.62 |
| large | `after_source_private_bytes` | 810.78 / 829.13 / 806.44 / 829.13 | 810.98 / 824.44 / 800.86 / 824.44 | 7/7 | -0.02 |
| large | `after_source_sampled_peak_private_bytes` | 999.68 / 1017.70 / 995.09 / 1017.70 | 999.76 / 1013.23 / 990.19 / 1013.23 | 7/7 | -0.01 |
| large | `after_source_working_set` | 821.54 / 839.74 / 815.73 / 839.74 | 815.41 / 834.07 / 811.72 / 834.07 | 7/7 | +0.75 |
| large | `final_peak_private_commit` | 1375.34 / 1393.32 / 1371.09 / 1393.32 | 1374.90 / 1389.01 / 1365.28 / 1389.01 | 7/7 | +0.03 |
| large | `final_peak_working_set` | 1372.67 / 1390.61 / 1367.00 / 1390.61 | 1366.49 / 1385.05 / 1362.81 / 1385.05 | 7/7 | +0.45 |
| large | `reopen_final_peak_private_commit` | 325.52 / 330.90 / 325.38 / 330.90 | 325.61 / 326.61 / 325.00 / 326.61 | 7/7 | -0.03 |
| large | `reopen_final_peak_working_set` | 366.96 / 367.24 / 366.88 / 367.24 | 366.97 / 367.15 / 366.40 / 367.15 | 7/7 | 0.00 |
| many | `after_body_peak_private_commit` | 1176.21 / 1176.85 / 1163.59 / 1176.85 | 1175.89 / 1176.56 / 1165.14 / 1176.56 | 7/7 | +0.03 |
| many | `after_body_peak_working_set` | 1169.64 / 1170.05 / 1157.16 / 1170.05 | 1169.48 / 1169.73 / 1158.14 / 1169.73 | 7/7 | +0.01 |
| many | `after_body_private_bytes` | 888.62 / 889.71 / 884.99 / 889.71 | 887.57 / 888.49 / 886.23 / 888.49 | 7/7 | +0.12 |
| many | `after_body_sampled_peak_private_bytes` | 1172.68 / 1175.80 / 1159.04 / 1175.80 | 1172.52 / 1175.83 / 1165.14 / 1175.83 | 7/7 | +0.01 |
| many | `after_body_working_set` | 881.62 / 882.18 / 879.01 / 882.18 | 880.42 / 882.03 / 880.13 / 882.03 | 7/7 | +0.14 |
| many | `after_close_peak_private_commit` | 380.54 / 383.60 / 380.43 / 383.60 | 379.89 / 383.24 / 379.55 / 383.24 | 7/7 | +0.17 |
| many | `after_close_peak_working_set` | 418.46 / 419.04 / 418.30 / 419.04 | 417.69 / 418.30 / 417.41 / 418.30 | 7/7 | +0.18 |
| many | `after_close_private_bytes` | 145.90 / 151.94 / 142.49 / 151.94 | 144.49 / 146.88 / 143.31 / 146.88 | 7/7 | +0.96 |
| many | `after_close_sampled_peak_private_bytes` | 380.50 / 383.56 / 357.40 / 383.56 | 379.75 / 383.21 / 336.30 / 383.21 | 7/7 | +0.20 |
| many | `after_close_working_set` | 181.84 / 185.49 / 181.12 / 185.49 | 182.28 / 182.80 / 181.93 / 182.80 | 7/7 | -0.25 |
| many | `after_open_peak_private_commit` | 291.65 / 292.63 / 291.04 / 292.63 | 291.46 / 292.43 / 290.68 / 292.43 | 7/7 | +0.07 |
| many | `after_open_peak_working_set` | 323.20 / 324.16 / 323.02 / 324.16 | 323.48 / 324.24 / 322.77 / 324.24 | 7/7 | -0.09 |
| many | `after_open_private_bytes` | 180.29 / 181.70 / 179.21 / 181.70 | 180.18 / 181.40 / 179.43 / 181.40 | 7/7 | +0.06 |
| many | `after_open_sampled_peak_private_bytes` | 291.14 / 292.38 / 290.60 / 292.38 | 291.46 / 292.17 / 290.51 / 292.17 | 7/7 | -0.11 |
| many | `after_open_working_set` | 219.13 / 219.43 / 218.10 / 219.43 | 218.97 / 219.35 / 218.29 / 219.35 | 7/7 | +0.07 |
| many | `after_reopen_peak_private_commit` | 290.89 / 293.75 / 290.75 / 293.75 | 290.55 / 293.37 / 290.12 / 293.37 | 7/7 | +0.12 |
| many | `after_reopen_peak_working_set` | 323.05 / 323.28 / 322.61 / 323.28 | 322.48 / 323.28 / 322.15 / 323.28 | 7/7 | +0.18 |
| many | `after_reopen_private_bytes` | 180.38 / 183.59 / 179.62 / 183.59 | 179.86 / 182.69 / 179.30 / 182.69 | 7/7 | +0.28 |
| many | `after_reopen_sampled_peak_private_bytes` | 290.29 / 293.73 / 289.92 / 293.73 | 290.29 / 292.92 / 289.92 / 292.92 | 7/7 | 0.00 |
| many | `after_reopen_working_set` | 218.70 / 219.43 / 218.29 / 219.43 | 218.68 / 218.82 / 218.15 / 218.82 | 7/7 | +0.01 |
| many | `after_source_peak_private_commit` | 796.19 / 797.13 / 783.45 / 797.13 | 795.66 / 796.13 / 783.93 / 796.13 | 7/7 | +0.07 |
| many | `after_source_peak_working_set` | 751.57 / 751.99 / 738.92 / 751.99 | 751.45 / 751.49 / 739.29 / 751.49 | 7/7 | +0.02 |
| many | `after_source_private_bytes` | 564.63 / 566.10 / 552.27 / 566.10 | 563.57 / 564.97 / 552.45 / 564.97 | 7/7 | +0.19 |
| many | `after_source_sampled_peak_private_bytes` | 796.16 / 797.11 / 783.40 / 797.11 | 795.62 / 796.10 / 783.89 / 796.10 | 7/7 | +0.07 |
| many | `after_source_working_set` | 562.35 / 564.05 / 550.21 / 564.05 | 561.75 / 562.99 / 549.99 / 562.99 | 7/7 | +0.11 |
| many | `final_peak_private_commit` | 1176.21 / 1176.85 / 1173.91 / 1176.85 | 1175.89 / 1176.56 / 1165.14 / 1176.56 | 7/7 | +0.03 |
| many | `final_peak_working_set` | 1169.64 / 1170.05 / 1167.60 / 1170.05 | 1169.48 / 1169.73 / 1158.14 / 1169.73 | 7/7 | +0.01 |
| many | `reopen_final_peak_private_commit` | 380.54 / 383.60 / 380.43 / 383.60 | 379.89 / 383.24 / 379.55 / 383.24 | 7/7 | +0.17 |
| many | `reopen_final_peak_working_set` | 418.46 / 419.04 / 418.30 / 419.04 | 417.69 / 418.30 / 417.41 / 418.30 | 7/7 | +0.18 |

#### Baseline → C3

Время, ms:

| Fixture | Метрика | Baseline median / p90 / min / max | C3 median / p90 / min / max | N old/new | Δ median % |
|---|---|---:|---:|---:|---:|
| control | Body→Source | 28.24 / 30.31 / 27.52 / 30.31 | 28.44 / 30.79 / 27.41 / 30.79 | 7/7 | -0.69 |
| control | metadata workload (1000 queries) | 3139.18 / 3610.03 / 3105.66 / 3610.03 | 54.73 / 56.77 / 53.58 / 56.77 | 7/7 | +98.26 |
| control | mutation workload (5 API + 5 raw writes) | 165.48 / 176.28 / 159.40 / 176.28 | 114.11 / 124.93 / 111.33 / 124.93 | 7/7 | +31.04 |
| control | Open | 1837.51 / 1916.52 / 1461.80 / 1916.52 | 1566.24 / 1671.83 / 1500.78 / 1671.83 | 7/7 | +14.76 |
| control | Replace+Save | 105.60 / 113.45 / 102.49 / 113.45 | 94.04 / 99.03 / 92.88 / 99.03 | 7/7 | +10.95 |
| control | Save | 25.22 / 25.77 / 24.78 / 25.77 | 24.99 / 26.86 / 23.29 / 26.86 | 7/7 | +0.89 |
| control | Save→Close→process restart→ready | 1930.55 / 2187.03 / 1863.31 / 2187.03 | 2036.06 / 2117.75 / 1959.52 / 2117.75 | 7/7 | -5.47 |
| control | Source→Body | 117.44 / 1397.82 / 111.24 / 1397.82 | 115.69 / 117.42 / 111.95 / 117.42 | 7/7 | +1.49 |
| large | Body→Source | 743.43 / 753.99 / 728.39 / 753.99 | 735.05 / 751.07 / 733.35 / 751.07 | 7/7 | +1.13 |
| large | metadata workload (1000 queries) | 20243.20 / 20847.90 / 20122.00 / 20847.90 | 51.79 / 53.47 / 49.65 / 53.47 | 7/7 | +99.74 |
| large | mutation workload (5 API + 5 raw writes) | 936.67 / 997.86 / 925.33 / 997.86 | 145.51 / 151.20 / 141.04 / 151.20 | 7/7 | +84.46 |
| large | Open | 2767.12 / 3307.08 / 2661.86 / 3307.08 | 2704.91 / 2924.17 / 2631.18 / 2924.17 | 7/7 | +2.25 |
| large | Replace+Save | 1675.87 / 1800.39 / 1567.17 / 1800.39 | 1454.69 / 1533.79 / 1403.33 / 1533.79 | 7/7 | +13.20 |
| large | Save | 1508.43 / 1550.32 / 1438.27 / 1550.32 | 1534.81 / 1610.28 / 1448.91 / 1610.28 | 7/7 | -1.75 |
| large | Save→Close→process restart→ready | 3901.71 / 4166.12 / 3631.00 / 4166.12 | 3900.24 / 4173.32 / 3580.34 / 4173.32 | 7/7 | +0.04 |
| large | Source→Body | 1039.10 / 4434.47 / 969.39 / 4434.47 | 961.20 / 4488.33 / 933.18 / 4488.33 | 7/7 | +7.50 |
| many | Body→Source | 781.44 / 793.40 / 761.61 / 793.40 | 784.84 / 789.21 / 761.27 / 789.21 | 7/7 | -0.43 |
| many | metadata workload (1000 queries) | 7036.81 / 7889.22 / 6911.83 / 7889.22 | 64.23 / 66.40 / 63.01 / 66.40 | 7/7 | +99.09 |
| many | mutation workload (5 API + 5 raw writes) | 422.98 / 469.49 / 408.78 / 469.49 | 412.90 / 601.46 / 391.09 / 601.46 | 7/7 | +2.38 |
| many | Open | 4995.85 / 5746.18 / 4847.70 / 5746.18 | 2879.86 / 3047.05 / 2621.48 / 3047.05 | 7/7 | +42.35 |
| many | Replace+Save | 1179.81 / 1240.37 / 1083.48 / 1240.37 | 1197.42 / 1243.14 / 1096.65 / 1243.14 | 7/7 | -1.49 |
| many | Save | 902.44 / 1001.78 / 791.15 / 1001.78 | 889.86 / 925.16 / 791.89 / 925.16 | 7/7 | +1.39 |
| many | Save→Close→process restart→ready | 6188.17 / 7071.47 / 6000.60 / 7071.47 | 3837.84 / 4142.43 / 3752.63 / 4142.43 | 7/7 | +37.98 |
| many | Source→Body | 6957.77 / 7307.87 / 6783.61 / 7307.87 | 4612.96 / 4756.23 / 4489.54 / 4756.23 | 7/7 | +33.70 |

Память, MiB: Private Bytes, Working Set, OS peaks и sampled private peak на всех этапах:

| Fixture | Метрика | Baseline median / p90 / min / max | C3 median / p90 / min / max | N old/new | Δ median % |
|---|---|---:|---:|---:|---:|
| control | `after_body_peak_private_commit` | 51.15 / 52.14 / 50.71 / 52.14 | 50.77 / 53.81 / 50.56 / 53.81 | 7/7 | +0.75 |
| control | `after_body_peak_working_set` | 91.41 / 91.60 / 90.80 / 91.60 | 90.88 / 91.21 / 90.62 / 91.21 | 7/7 | +0.58 |
| control | `after_body_private_bytes` | 51.10 / 52.06 / 50.68 / 52.06 | 50.74 / 53.63 / 50.49 / 53.63 | 7/7 | +0.71 |
| control | `after_body_sampled_peak_private_bytes` | 51.10 / 52.07 / 50.68 / 52.07 | 50.75 / 53.63 / 50.49 / 53.63 | 7/7 | +0.69 |
| control | `after_body_working_set` | 91.23 / 91.44 / 90.66 / 91.44 | 90.75 / 91.04 / 90.48 / 91.04 | 7/7 | +0.53 |
| control | `after_close_peak_private_commit` | 51.20 / 65.64 / 50.39 / 65.64 | 50.79 / 65.25 / 50.59 / 65.25 | 7/7 | +0.79 |
| control | `after_close_peak_working_set` | 89.97 / 90.41 / 89.38 / 90.41 | 89.70 / 89.98 / 89.40 / 89.98 | 7/7 | +0.30 |
| control | `after_close_private_bytes` | 50.52 / 64.65 / 49.52 / 64.65 | 50.55 / 64.46 / 49.81 / 64.46 | 7/7 | -0.07 |
| control | `after_close_sampled_peak_private_bytes` | 51.00 / 65.12 / 50.18 / 65.12 | 50.61 / 65.00 / 50.51 / 65.00 | 7/7 | +0.77 |
| control | `after_close_working_set` | 89.62 / 89.95 / 88.76 / 89.95 | 89.36 / 89.98 / 89.08 / 89.98 | 7/7 | +0.29 |
| control | `after_open_peak_private_commit` | 47.94 / 48.73 / 47.45 / 48.73 | 47.95 / 50.54 / 47.64 / 50.54 | 7/7 | -0.03 |
| control | `after_open_peak_working_set` | 86.38 / 86.61 / 85.89 / 86.61 | 86.27 / 86.54 / 86.14 / 86.54 | 7/7 | +0.13 |
| control | `after_open_private_bytes` | 47.79 / 48.68 / 47.26 / 48.68 | 47.70 / 50.50 / 47.57 / 50.50 | 7/7 | +0.19 |
| control | `after_open_sampled_peak_private_bytes` | 47.93 / 48.72 / 47.45 / 48.72 | 47.95 / 50.54 / 47.64 / 50.54 | 7/7 | -0.04 |
| control | `after_open_working_set` | 86.28 / 86.50 / 85.82 / 86.50 | 86.16 / 86.45 / 86.05 / 86.45 | 7/7 | +0.14 |
| control | `after_reopen_peak_private_commit` | 48.20 / 62.73 / 47.66 / 62.73 | 48.05 / 62.48 / 47.58 / 62.48 | 7/7 | +0.31 |
| control | `after_reopen_peak_working_set` | 86.36 / 86.63 / 86.17 / 86.63 | 86.34 / 86.43 / 85.87 / 86.43 | 7/7 | +0.01 |
| control | `after_reopen_private_bytes` | 47.86 / 62.69 / 47.41 / 62.69 | 47.87 / 62.43 / 47.54 / 62.43 | 7/7 | -0.02 |
| control | `after_reopen_sampled_peak_private_bytes` | 48.20 / 62.72 / 47.65 / 62.72 | 48.05 / 62.48 / 47.58 / 62.48 | 7/7 | +0.31 |
| control | `after_reopen_working_set` | 86.25 / 86.43 / 86.08 / 86.43 | 86.26 / 86.34 / 85.81 / 86.34 | 7/7 | -0.01 |
| control | `after_source_peak_private_commit` | 49.42 / 50.47 / 49.17 / 50.47 | 49.64 / 52.67 / 49.14 / 52.67 | 7/7 | -0.46 |
| control | `after_source_peak_working_set` | 89.16 / 89.37 / 88.93 / 89.37 | 89.04 / 89.27 / 88.75 / 89.27 | 7/7 | +0.14 |
| control | `after_source_private_bytes` | 49.23 / 50.33 / 48.81 / 50.33 | 49.20 / 52.25 / 48.93 / 52.25 | 7/7 | +0.07 |
| control | `after_source_sampled_peak_private_bytes` | 49.38 / 50.33 / 49.17 / 50.33 | 49.61 / 52.54 / 49.11 / 52.54 | 7/7 | -0.48 |
| control | `after_source_working_set` | 89.16 / 89.37 / 88.80 / 89.37 | 88.89 / 89.27 / 88.75 / 89.27 | 7/7 | +0.30 |
| control | `final_peak_private_commit` | 53.42 / 54.52 / 53.20 / 54.52 | 53.07 / 55.86 / 52.32 / 55.86 | 7/7 | +0.64 |
| control | `final_peak_working_set` | 93.10 / 93.77 / 92.80 / 93.77 | 92.62 / 92.87 / 92.18 / 92.87 | 7/7 | +0.52 |
| control | `reopen_final_peak_private_commit` | 51.20 / 65.64 / 50.39 / 65.64 | 50.79 / 65.25 / 50.59 / 65.25 | 7/7 | +0.79 |
| control | `reopen_final_peak_working_set` | 89.97 / 90.41 / 89.38 / 90.41 | 89.70 / 89.98 / 89.40 / 89.98 | 7/7 | +0.30 |
| large | `after_body_peak_private_commit` | 1402.61 / 1417.17 / 1311.93 / 1417.17 | 1374.90 / 1389.01 / 1365.28 / 1389.01 | 7/7 | +1.98 |
| large | `after_body_peak_working_set` | 1399.91 / 1414.25 / 1309.71 / 1414.25 | 1366.49 / 1385.05 / 1362.81 / 1385.05 | 7/7 | +2.39 |
| large | `after_body_private_bytes` | 779.24 / 779.82 / 749.21 / 779.82 | 776.26 / 790.94 / 748.97 / 790.94 | 7/7 | +0.38 |
| large | `after_body_sampled_peak_private_bytes` | 1401.81 / 1417.14 / 1300.88 / 1417.14 | 1374.89 / 1389.00 / 1360.80 / 1389.00 | 7/7 | +1.92 |
| large | `after_body_working_set` | 785.94 / 786.17 / 755.85 / 786.17 | 777.22 / 783.21 / 755.49 / 783.21 | 7/7 | +1.11 |
| large | `after_close_peak_private_commit` | 326.24 / 339.82 / 325.65 / 339.82 | 325.61 / 326.61 / 325.00 / 326.61 | 7/7 | +0.19 |
| large | `after_close_peak_working_set` | 367.02 / 367.20 / 366.48 / 367.20 | 366.97 / 367.15 / 366.40 / 367.15 | 7/7 | +0.01 |
| large | `after_close_private_bytes` | 124.39 / 137.38 / 122.69 / 137.38 | 122.91 / 123.71 / 122.61 / 123.71 | 7/7 | +1.19 |
| large | `after_close_sampled_peak_private_bytes` | 326.23 / 339.78 / 323.00 / 339.78 | 325.04 / 326.55 / 322.75 / 326.55 | 7/7 | +0.37 |
| large | `after_close_working_set` | 162.11 / 163.25 / 161.86 / 163.25 | 162.17 / 162.73 / 161.61 / 162.73 | 7/7 | -0.04 |
| large | `after_open_peak_private_commit` | 272.77 / 281.39 / 263.70 / 281.39 | 280.41 / 295.12 / 265.41 / 295.12 | 7/7 | -2.80 |
| large | `after_open_peak_working_set` | 309.93 / 321.21 / 303.49 / 321.21 | 306.52 / 313.62 / 306.25 / 313.62 | 7/7 | +1.10 |
| large | `after_open_private_bytes` | 254.86 / 270.04 / 164.76 / 270.04 | 254.32 / 267.73 / 245.79 / 267.73 | 7/7 | +0.21 |
| large | `after_open_sampled_peak_private_bytes` | 272.71 / 281.32 / 260.23 / 281.32 | 280.41 / 295.12 / 265.41 / 295.12 | 7/7 | -2.82 |
| large | `after_open_working_set` | 301.20 / 316.35 / 211.61 / 316.35 | 295.46 / 313.53 / 292.68 / 313.53 | 7/7 | +1.91 |
| large | `after_reopen_peak_private_commit` | 259.70 / 272.89 / 257.98 / 272.89 | 258.30 / 259.38 / 258.16 / 259.38 | 7/7 | +0.54 |
| large | `after_reopen_peak_working_set` | 298.71 / 298.91 / 298.21 / 298.91 | 293.25 / 293.49 / 292.76 / 293.49 | 7/7 | +1.83 |
| large | `after_reopen_private_bytes` | 254.78 / 268.21 / 253.95 / 268.21 | 250.82 / 252.11 / 250.41 / 252.11 | 7/7 | +1.55 |
| large | `after_reopen_sampled_peak_private_bytes` | 258.61 / 272.89 / 257.98 / 272.89 | 258.30 / 259.38 / 250.98 / 259.38 | 7/7 | +0.12 |
| large | `after_reopen_working_set` | 296.27 / 296.44 / 295.77 / 296.44 | 293.16 / 293.43 / 292.64 / 293.43 | 7/7 | +1.05 |
| large | `after_source_peak_private_commit` | 1026.88 / 1041.56 / 936.22 / 1041.56 | 999.79 / 1013.28 / 990.21 / 1013.28 | 7/7 | +2.64 |
| large | `after_source_peak_working_set` | 1001.02 / 1015.55 / 910.68 / 1015.55 | 967.61 / 986.16 / 964.58 / 986.16 | 7/7 | +3.34 |
| large | `after_source_private_bytes` | 838.14 / 852.79 / 747.87 / 852.79 | 810.98 / 824.44 / 800.86 / 824.44 | 7/7 | +3.24 |
| large | `after_source_sampled_peak_private_bytes` | 1026.83 / 1041.50 / 936.17 / 1041.50 | 999.76 / 1013.23 / 990.19 / 1013.23 | 7/7 | +2.64 |
| large | `after_source_working_set` | 848.88 / 863.29 / 758.89 / 863.29 | 815.41 / 834.07 / 811.72 / 834.07 | 7/7 | +3.94 |
| large | `final_peak_private_commit` | 1402.61 / 1417.17 / 1311.93 / 1417.17 | 1374.90 / 1389.01 / 1365.28 / 1389.01 | 7/7 | +1.98 |
| large | `final_peak_working_set` | 1399.91 / 1414.25 / 1309.71 / 1414.25 | 1366.49 / 1385.05 / 1362.81 / 1385.05 | 7/7 | +2.39 |
| large | `reopen_final_peak_private_commit` | 326.24 / 339.82 / 325.65 / 339.82 | 325.61 / 326.61 / 325.00 / 326.61 | 7/7 | +0.19 |
| large | `reopen_final_peak_working_set` | 367.02 / 367.20 / 366.48 / 367.20 | 366.97 / 367.15 / 366.40 / 367.15 | 7/7 | +0.01 |
| many | `after_body_peak_private_commit` | 1176.35 / 1262.68 / 1161.81 / 1262.68 | 1175.89 / 1176.56 / 1165.14 / 1176.56 | 7/7 | +0.04 |
| many | `after_body_peak_working_set` | 1169.09 / 1253.22 / 1154.25 / 1253.22 | 1169.48 / 1169.73 / 1158.14 / 1169.73 | 7/7 | -0.03 |
| many | `after_body_private_bytes` | 872.44 / 956.50 / 870.80 / 956.50 | 887.57 / 888.49 / 886.23 / 888.49 | 7/7 | -1.73 |
| many | `after_body_sampled_peak_private_bytes` | 1173.36 / 1261.33 / 1160.13 / 1261.33 | 1172.52 / 1175.83 / 1165.14 / 1175.83 | 7/7 | +0.07 |
| many | `after_body_working_set` | 865.87 / 947.95 / 863.59 / 947.95 | 880.42 / 882.03 / 880.13 / 882.03 | 7/7 | -1.68 |
| many | `after_close_peak_private_commit` | 388.43 / 402.15 / 387.05 / 402.15 | 379.89 / 383.24 / 379.55 / 383.24 | 7/7 | +2.20 |
| many | `after_close_peak_working_set` | 425.88 / 426.27 / 423.79 / 426.27 | 417.69 / 418.30 / 417.41 / 418.30 | 7/7 | +1.92 |
| many | `after_close_private_bytes` | 153.48 / 169.84 / 150.11 / 169.84 | 144.49 / 146.88 / 143.31 / 146.88 | 7/7 | +5.86 |
| many | `after_close_sampled_peak_private_bytes` | 388.12 / 402.14 / 365.75 / 402.14 | 379.75 / 383.21 / 336.30 / 383.21 | 7/7 | +2.15 |
| many | `after_close_working_set` | 191.90 / 192.74 / 187.86 / 192.74 | 182.28 / 182.80 / 181.93 / 182.80 | 7/7 | +5.01 |
| many | `after_open_peak_private_commit` | 269.62 / 270.93 / 269.18 / 270.93 | 291.46 / 292.43 / 290.68 / 292.43 | 7/7 | -8.10 |
| many | `after_open_peak_working_set` | 301.26 / 306.96 / 301.12 / 306.96 | 323.48 / 324.24 / 322.77 / 324.24 | 7/7 | -7.38 |
| many | `after_open_private_bytes` | 184.87 / 267.91 / 183.76 / 267.91 | 180.18 / 181.40 / 179.43 / 181.40 | 7/7 | +2.54 |
| many | `after_open_sampled_peak_private_bytes` | 269.45 / 270.93 / 269.02 / 270.93 | 291.46 / 292.17 / 290.51 / 292.17 | 7/7 | -8.17 |
| many | `after_open_working_set` | 222.98 / 306.23 / 222.07 / 306.23 | 218.97 / 219.35 / 218.29 / 219.35 | 7/7 | +1.80 |
| many | `after_reopen_peak_private_commit` | 268.46 / 282.87 / 267.92 / 282.87 | 290.55 / 293.37 / 290.12 / 293.37 | 7/7 | -8.23 |
| many | `after_reopen_peak_working_set` | 300.47 / 310.25 / 299.85 / 310.25 | 322.48 / 323.28 / 322.15 / 323.28 | 7/7 | -7.32 |
| many | `after_reopen_private_bytes` | 186.21 / 270.59 / 183.63 / 270.59 | 179.86 / 182.69 / 179.30 / 182.69 | 7/7 | +3.41 |
| many | `after_reopen_sampled_peak_private_bytes` | 268.18 / 282.72 / 267.67 / 282.72 | 290.29 / 292.92 / 289.92 / 292.92 | 7/7 | -8.25 |
| many | `after_reopen_working_set` | 224.85 / 308.69 / 221.73 / 308.69 | 218.68 / 218.82 / 218.15 / 218.82 | 7/7 | +2.74 |
| many | `after_source_peak_private_commit` | 803.40 / 886.57 / 789.58 / 886.57 | 795.66 / 796.13 / 783.93 / 796.13 | 7/7 | +0.96 |
| many | `after_source_peak_working_set` | 758.08 / 841.29 / 744.09 / 841.29 | 751.45 / 751.49 / 739.29 / 751.49 | 7/7 | +0.88 |
| many | `after_source_private_bytes` | 571.28 / 653.41 / 556.31 / 653.41 | 563.57 / 564.97 / 552.45 / 564.97 | 7/7 | +1.35 |
| many | `after_source_sampled_peak_private_bytes` | 803.34 / 886.53 / 789.56 / 886.53 | 795.62 / 796.10 / 783.89 / 796.10 | 7/7 | +0.96 |
| many | `after_source_working_set` | 568.33 / 650.58 / 553.27 / 650.58 | 561.75 / 562.99 / 549.99 / 562.99 | 7/7 | +1.16 |
| many | `final_peak_private_commit` | 1176.35 / 1262.68 / 1161.81 / 1262.68 | 1175.89 / 1176.56 / 1165.14 / 1176.56 | 7/7 | +0.04 |
| many | `final_peak_working_set` | 1169.09 / 1253.22 / 1154.25 / 1253.22 | 1169.48 / 1169.73 / 1158.14 / 1169.73 | 7/7 | -0.03 |
| many | `reopen_final_peak_private_commit` | 388.43 / 402.15 / 387.05 / 402.15 | 379.89 / 383.24 / 379.55 / 383.24 | 7/7 | +2.20 |
| many | `reopen_final_peak_working_set` | 425.88 / 426.27 / 423.79 / 426.27 | 417.69 / 418.30 / 417.41 / 418.30 | 7/7 | +1.92 |

### Закрытые проверки кандидата и оставшаяся граница

Для неизменённого FBE `2f0113b2566a3510c74e7e1c4536e9d10e4be0c8` уже подтверждены: Release /W4 /WX PASS; legacy compatibility 46/46; bundled scripts 6/6; native ownership, XML Undo runtime PASS; targeted UBSan 5/5, targeted ASan 5/5; native ASan ownership unit PASS; ASan FAST 84/84 PASS без retry. Эти этапы после benchmark не запускались повторно. Изменения внешнего контроллера не меняют бинарники FBE, на которых получено это evidence.

Полная acceptance остаётся **PARTIAL**. External OLE evidence для текущего SHA / FullValidation — **NOT RUN / pending**; evidence другой ревизии не принимается за текущий SHA. По указанию пользователя работа остановлена перед этой границей, поскольку для внешнего evidence нужен push. Ручной GUI smoke остаётся **pending**; автоматические UI-команды benchmark его не заменяют. Push не выполнялся. Изменения документа и исправление benchmark-контроллера оставлены в текущем checkout без нового коммита; SHA кандидата FBE не изменён.
### Повторная сверка сохранённого финального evidence

При продолжении из текущего checkout подтверждено: исходный benchmark-процесс уже завершился, неполная попытка сохранена как INTERRUPTED; завершённый `results-2f0113b2-final64-uiwait` повторно запускать не требуется. Независимая сверка raw.json с summary.json подтвердила median, p90, min, max и N всех 111 групп: расхождений 0. По raw Open records подтверждены 9 warm-up и 63 measured; все 72 equivalence records имеют PASS для XSD, binary SHA-256, MIME и references. Повторное хеширование всех 2551 файлов snapshots и трёх fixtures подтвердило исходные значения, включая совпадение fixture hashes с первоначальным interrupted manifest.

Текущий checkout также содержит отдельные незакоммиченные изменения диагностики C4. Они сохранены без изменений и не являются измеренной реализацией C3: приведённые результаты относятся строго к неизменённым snapshots указанных SHA. При этой сверке исходники, runtime snapshots и benchmark evidence не изменялись; сборки, sanitizers, compatibility и benchmark повторно не запускались. External OLE / FullValidation и ручной GUI smoke остаются pending; push не выполнялся.
