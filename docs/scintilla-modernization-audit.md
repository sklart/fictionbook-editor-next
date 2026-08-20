# Аудит модернизации Scintilla/Lexilla

Дата аудита: 2026-08-14. База: Scintilla 5.6.6, Lexilla 5.5.3.

## Текущее решение

| Возможность | В 5.6.6 | Применение в FBE до аудита | Решение | Обоснование и риск | Win7 |
| --- | --- | --- | --- | --- | --- |
| `SCI_SETCOMMANDEVENTS` | Да | Не задавалась | Внедрено: `FALSE` | Source-команды FBE маршрутизирует сам; карта сообщений обрабатывает только `SCN_MODIFIED`, `SCN_MARGINCLICK`, `SCN_UPDATEUI`. Устраняет ненужные legacy command events. | Не влияет |
| `SCI_SETMODEVENTMASK` | Да | Не задавалась | Внедрено: `SC_MOD_CHANGEFOLD` | Единственный обработчик `SCN_MODIFIED` нужен для fold state. Modified/save state запрашивается через `SCI_GETMODIFY`, поэтому текстовые notifications не требуются. | Не влияет |
| Direct API | Да | Matched-tags через `SendMessage` | Внедрено в `XmlMatchedTagsHighlighter` | Cached direct function/pointer с fallback. Устраняет межоконный message dispatch в hot-path; wrapper освобождается после каждого highlighter instance. | ABI Scintilla, без WinAPI |
| Undo selection history | Да | Не задавалась | Внедрено: selection + scroll | Ограничено Source editor. Реальный smoke проверяет восстановление выделения после Undo. | Не влияет |
| DirectWrite | Да | Не используется | Отклонено | API smoke пройден, но wrap/reflow benchmark показал ухудшение ≈4.6× на 1 MB и ≈5.3× на 5 MB. Не включается; ручная матрица не требуется для отклонённого backend. | Не включено |
| `SCI_SETLAYOUTTHREADS` | Да | Не используется | Отклонено на текущем runtime | `SCI_SUPPORTSFEATURE(SC_SUPPORTS_THREAD_SAFE_MEASURE_WIDTHS)` не заявил поддержку, поэтому двухпоточный benchmark безопасно пропущен. | Не включено |
| Change History | Да | Не используется | Отклонено | Real smoke подтвердил marker API на пустой undo history после save point. Но `ShowSource` пересоздаёт XML при `DocRelChanged` и всегда вызывает `SCI_EMPTYUNDOBUFFER`, поэтому Body↔Source уничтожает историю; корректная реализация требует архитектурного изменения transfer lifecycle. | Не включено |
| EOL annotations | Да | Не используется | Внедрено | Первое сообщение SAX validator и его line/column выводятся возле проблемной Source-строки. Очистка: перед Validate, после успешной Validate, при первом Source edit; real Scintilla smoke проверяет set/get/style/visible/clear. | Не влияет |
| Special representations | Да | Не используется | Внедрено, выключено по умолчанию | В FBE Next → Source code сохранён переключатель. Для NBSP, soft hyphen, ZWSP, ZWNJ, ZWJ, narrow NBSP, word joiner и BOM Scintilla показывает ASCII-метки через `SCI_SETREPRESENTATION`; при выключении записи очищаются через `SCI_CLEARREPRESENTATION`. Representation не меняет UTF-8-байты документа, lexer, поиск или сохранение; real smoke проверяет round-trip и неизменность текста. Используется обычное theme-aware рисование Scintilla без принудительного цвета. | Не влияет |
| Idle styling | Да | Не используется | Отклонено | Документация Scintilla прямо указывает, что `SCI_SETIDLESTYLING` не действует при wrap. Source в FBE по умолчанию использует `SC_WRAP_WORD`; при отключённом wrap API может показать текст временно без подсветки. Ни throughput, ни UX не улучшаются стабильно. | Не включено |
| Modern folding API/display text | Да | Собственная логика folding | Отклонено после исследования | `SCI_TOGGLEFOLDSHOWTEXT`/`SCI_SETDEFAULTFOLDDISPLAYTEXT` могут добавлять текст к скрытым строкам, но FBE уже реализует обычный, Ctrl-, Shift- и Ctrl+Shift-click пути через `SCI_TOGGLEFOLD`, `SCI_SETFOLDEXPANDED`, `SCI_HIDELINES` и `ExpandFold`. Новая подмена меняет смысл свёртки и требует ручной UX-проверки на всех темах/DPI; без неё не включается. | Не влияет |
| `SCI_ALLOCATELINES` | Да | Source XML загружается одним `SCI_APPENDTEXT` после `SCI_CLEARALL` | Внедрено | Перед bulk append подсчитываются CRLF/CR/LF-строки исходного XML и резервируются line indices. Benchmark на идентичном сценарии подтвердил выигрыш; содержимое и line count проверяются в real smoke. | Не влияет |
| `SCI_REPLACETARGETMINIMAL` | Да | `SCI_REPLACETARGET` / `SCI_REPLACETARGETRE` | Отклонено | Real smoke сравнивает normal/minimal для XML, UTF-8 кириллицы, NBSP, вставки, удаления и Undo. Текст и Undo совпадают, но для replacement с общим префиксом/суффиксом Minimal возвращает другой length и оставляет другой target range — это ожидаемое следствие минимизации изменений. Source Find/Replace использует return/target contract обычной замены; поэтому без регрессии нельзя подменять API. Regex-путь корректно остаётся `SCI_REPLACETARGETRE`. | Не включено |
| XML embedded language properties | Lexilla XML | Не задаются | Внедрено: ASP/PHP/script = `0` | FB2 Source использует XML. Реальный lexer smoke проверяет fixture с PHP, ASP и script и отсутствие embedded-language styles; HTML lexer settings не меняются. | Не влияет |
| XML substyles | Частично | Не используется | Отклонено после real lexer smoke | `CreateEditorLexer("xml")` принимает allocation и применяет substyles к `SCE_H_ATTRIBUTE` (`id`, `l:href`), но зарегистрированные `section`/`image` и unknown XML tags остаются базовым `SCE_H_TAG`. Поэтому требуемую FB2-подсветку standard/structural/unknown tags этим API реализовать нельзя; Lexilla не форкается ради частичного эффекта. | Не включено |
| FB2 XML autocomplete | Да | Не используется | Внедрено | Production base implemented. Metadata, сгенерированные из XSD, предлагают допустимые FB2-элементы и атрибуты, closing tags, фильтруют дубли атрибутов и предлагают IDs для XLink `href="#…"`. Структурный context определяется read-only chunked backward resolver без fixed local window. Стандартные префиксы FBE `l:` и `xlink:` распознаются без локального `xmlns`; по умолчанию предлагается проектный `l:`. Произвольный префикс используется только при доступном объявлении `xmlns:*` с XLink URI. ID completion ограничено XLink href, XML-aware извлечение IDs выполняет full-document scan. ID cache, robust document-level namespace cache и sequence-position-aware XSD completion deferred. | Не влияет |

## Direct API microbenchmark

Методика: `tools/tests/test-scintilla.ps1 -EditorRuntimeDirectory .\\out\\editor-runtime\\Modern -RunDirectBenchmark`.
Harness генерирует XML, делает один warm-up и семь измерений по 400 серий вызовов, характерных для поиска matched tags (`GETSTYLEAT`, target/search, target bounds). Это microbenchmark API boundary, а не ручный benchmark полного UI.

| XML bytes | Path | Median, ms | Min, ms | Max, ms |
| ---: | --- | ---: | ---: | ---: |
| 131,072 | SendMessage | 1.5507 | 1.5373 | 1.5693 |
| 131,072 | Direct | 1.0304 | 0.8876 | 1.1397 |
| 2,097,152 | SendMessage | 1.5586 | 1.5328 | 1.5700 |
| 2,097,152 | Direct | 1.0566 | 1.0235 | 1.1912 |
| 16,777,216 | SendMessage | 1.4122 | 1.3944 | 1.7289 |
| 16,777,216 | Direct | 0.9798 | 0.9495 | 1.5774 |

## Wrap/layout benchmark

Методика: `tools/tests/test-scintilla.ps1 -EditorRuntimeDirectory .\\out\\editor-runtime\\Modern -RunLayoutBenchmark`. Один warm-up, затем семь измерений полного wrap/reflow после resize; 20 MB вынесены в отдельный opt-in прогон, но не потребовались после устойчивой деградации на 5 MB.

| Path | XML bytes | Median, ms | Min, ms | Max, ms |
| --- | ---: | ---: | ---: | ---: |
| Default, 1 layout thread | 1,048,576 | 140.268 | 134.864 | 144.994 |
| DirectWrite, 1 layout thread | 1,048,576 | 643.091 | 631.933 | 655.748 |
| Default, 1 layout thread | 5,242,880 | 613.376 | 599.501 | 629.399 |
| DirectWrite, 1 layout thread | 5,242,880 | 3269.170 | 3193.950 | 3288.160 |

`SC_SUPPORTS_THREAD_SAFE_MEASURE_WIDTHS` returned false, so `SCI_SETLAYOUTTHREADS(2)` was not attempted.

## Source load / `SCI_ALLOCATELINES` benchmark

Методика: `tools/tests/test-scintilla.ps1 -EditorRuntimeDirectory .\out\editor-runtime\Modern -RunAllocateLinesBenchmark`. Для каждого размера создаётся новый Scintilla control; замеряется `SCI_APPENDTEXT` XML с плотными строками, один warm-up и семь измерений. В варианте preallocated время включает `SCI_ALLOCATELINES`, а smoke подтверждает равенство длины и числа строк.

| Path | XML bytes | Median, ms | Min, ms | Max, ms |
| --- | ---: | ---: | ---: | ---: |
| Bulk append | 1,048,576 | 3.5635 | 3.5356 | 4.2331 |
| `ALLOCATELINES` + bulk append | 1,048,576 | 3.4526 | 3.4018 | 3.7965 |
| Bulk append | 5,242,880 | 18.2338 | 18.0818 | 18.6030 |
| `ALLOCATELINES` + bulk append | 5,242,880 | 17.5236 | 16.8191 | 18.0496 |
| Bulk append | 20,971,520 | 75.0786 | 72.1230 | 78.4476 |
| `ALLOCATELINES` + bulk append | 20,971,520 | 69.9725 | 66.3957 | 71.5129 |

## Source memory benchmark

Методика: `tools/tests/test-scintilla.ps1 -EditorRuntimeDirectory .\out\editor-runtime\Modern -RunMemoryBenchmark`. Каждый размер запускается в отдельном процессе. Baseline снимается после создания входной UTF-8 строки, поэтому `text delta` отражает только Scintilla document + line indices, а `style delta` — добавочную private committed memory после полного XML lexing (`SCI_COLOURISE`). Это не измерение полного FBE process working set: DOM, Body view, conversion buffer и UI в него не входят.

| XML bytes | Text + line indices, MiB | XML styling, MiB | Scintilla total, MiB |
| ---: | ---: | ---: | ---: |
| 1,048,606 | 3.15 | 0.13 | 3.29 |
| 5,242,906 | 15.69 | 0.65 | 16.34 |
| 20,971,531 | 62.72 | 2.59 | 65.30 |

## Full-process FBE Source benchmark

Методика: `tools/tests/test-fbe-source-memory.ps1`. Скрипт создаёт валидный FB2 с обычными
`body`/`section`/`title`/`p`/`a`/`notes` (рост размера обеспечивается текстом абзацев, а не
`binary` или десятками тысяч искусственных узлов; fixture содержит один валидный 1×1 PNG cover
для проверки image metadata), дважды запускает `FBE.exe` и забирает TSV,
который пишет сам процесс. Режим `FBE.exe -b report.tsv fixture.fb2` измеряет private bytes,
working set, committed/reserved virtual memory: после открытия документа, после полного
Body→Source+XML styling, после 500 циклов selection/edit/Undo и после прохода XML matched-tags
по 100 позициям. Ключ `-u` выключает только `SCI_SETUNDOSELECTIONHISTORY` для контрольного
прохода. Результаты ниже — один холодный запуск каждого режима; для абсолютного сравнения
нужны несколько повторов, поскольку базовая память процесса заметно варьирует между запусками.

| Fixture, MiB | Source style, ms (history on/off) | Private MiB после Source (on/off) | Доп. private MiB после 500 edit/Undo (on/off) | Matched-tags +ms (on/off) |
| ---: | ---: | ---: | ---: | ---: |
| 1 | 219 / 218 | 220.15 / 211.46 | 3.80 / 3.91 | 31 / 16 |
| 5 | 1094 / 1078 | 244.23 / 232.34 | 5.38 / 5.57 | 31 / 47 |
| 20 | 5594 / 5594 | 391.66 / 393.84 | 5.63 / 6.19 | 47 / 47 |
| 50 | 23563 / 21406 | 803.74 / 807.45 | 6.13 / 6.31 | 31 / 47 |

Вывод: benchmark покрывает полный процесс, а не только Scintilla. После сериализации Source
память растёт прежде всего из-за полного FBE документа + Source UTF-8/Scintilla buffers;
на 50 MiB checkpoint составляет примерно 804–807 MiB private memory. В контролируемой серии
500 edit/Undo разница режима истории выделений не превышает межпроцессный шум (и не даёт
устойчивого выигрыша при отключении), поэтому включённый пользовательский режим остаётся
принятым. XML matched-tags после прогрева выполняется за 16–47 ms для 100 позиций;
direct API остаётся оправданным hot-path улучшением. Полные TSV: `out/tests/fbe-source-memory/`.

### Corrected Undo and matched-tags stress

Diagnostic harness now performs 10,000 independent Source edits, Undo all and Redo all, then
100,000 real `XmlMatchedTagsHighlighter` updates at 10k/50k/100k checkpoints. Caret movement
uses `SCI_SETCURRENTPOS`: an earlier harness used `SCI_GOTOPOS`, which deliberately applies
viewport scroll policy and fills the document layout cache across thousands of positions. That
was a valid scrolling stress but not a matched-tags leak test and produced misleading retained
memory growth. The production matched-tags lifecycle keeps and clears only its previous short
indicator ranges instead of clearing the entire document.

| Fixture | Undo mode | Private MiB after Source | after 10k edits | after tags 10k / 50k / 100k | tags time, 10k→100k |
| ---: | --- | ---: | ---: | ---: | ---: |
| 20 MiB | on | 430.82 | 440.17 | 440.17 / 440.17 / 440.17 | 688 ms |
| 20 MiB | off | 388.77 | 397.87 | 397.87 / 397.87 / 397.87 | 703 ms |
| 50 MiB | on | 802.61 | 835.73 | 835.73 / 835.73 / 835.73 | 485 ms |
| 50 MiB | off | 803.14 | 836.45 | 836.45 / 836.45 / 836.45 | 453 ms |

The ON/OFF difference after 10k edits is within cold-process variation (and is below 1 MiB at
50 MiB). Undo/Redo completes within the measurement resolution (15–16 ms after the edit phase).
There is no linear retained-memory growth from 10k to 100k matched-tags updates. The much larger
memory caused by forced `SCI_GOTOPOS` remains documented as a scrolling/layout-cache stress,
not attributed to the highlighter.

### Body → Source cycles

The optional `-RunViewCycles` harness switch performed 100 full Body→Source cycles on the 1 MiB
fixture after the edit and matched-tags stress. The document remained valid and the process exited
normally. Private memory was 220.23 MiB before cycles, 234.91 MiB after 10, 235.54 MiB after 50,
and 236.74 MiB after 100 with Undo selection history enabled (OFF: 206.13, 220.22, 220.50,
222.31 MiB). The initial conversion cache is retained as expected; the 50→100 change is only
1.2–1.8 MiB and shows no sustained linear trend. 100 cycles took about 53 seconds. Larger fixtures
are intentionally not cycled 100 times in the default benchmark because that would measure hours
of repeated XML/DOM conversion rather than interactive behavior.

## XML diagnostics

Проверен существующий MSXML6 SAX validation pipeline. Хотя reader включает
`exhaustive-errors`, `SAXErrorHandler::SetMsg` сохраняет только первое сообщение, а каждый
из `raw_error`, `raw_fatalError` и `raw_ignorableWarning` возвращает `E_FAIL`. Следовательно,
текущий проход намеренно прекращается на первой диагностике и не предоставляет безопасного
списка ошибок для нескольких EOL annotations. Новый parser не добавлялся: сохраняются первая
фактическая SAX/schema ошибка, её line/column и существующая навигация к ней через Source.

## Folding profiling

Full-process checkpoint на репрезентативном 20 MiB FB2 (4,042 sections) измерил существующие
`FoldAll()`/повторный `FoldAll()` для восстановления. После полного Source layout оба действия
заняли по ~312–313 ms; private memory при collapse не выросла, а после expand слегка снизилась
из-за allocator поведения. Это не указывает на bottleneck Win32 message boundary и не достигает
критерия для механической замены на `SCI_FOLDALL`/`SCI_FOLDCHILDREN`/`SCI_EXPANDCHILDREN`.
Сохраняется текущая реализация, воспроизводящая click/Shift/Ctrl/Ctrl+Shift semantics.

## Remaining Source hot paths

Full-process 20 MiB benchmark additionally executes 1,000 sequential `SCI_SEARCHINTARGET`
lookups of `<section`, 100 same-text `SCI_REPLACETARGET` operations, and navigation to 1,000
Source lines with `SCI_POSITIONFROMLINE`/`SCI_SETCURRENTPOS`. The elapsed checkpoints show
respectively <16 ms, 16 ms and <16 ms (the `GetTickCount64` resolution); private memory does not
grow. Against the same input Fold All/Expand All cost 328/328 ms, Source initialisation about
5.6 s, 10,000 edits about 3.6 s, and 100,000 matched-tag updates about 0.7 s. No operation has
evidence that Win32 `SendMessage` dispatch itself accounts for 5%+ of latency, so Direct API is
not extended outside the already-profiled matched-tags wrapper.

## Проверенная совместимость

- Modern: Scintilla smoke, Direct API (включая regex target-search на вложенном XML), Undo selection history и отключение embedded XML languages прошли.
- Win7 target: Scintilla/Lexilla собраны с v143 / 14.44; smoke прошёл после autocomplete/benchmark изменений; import table `FBE.exe`, `Scintilla.dll` и `Lexilla.dll` не содержит запрещённых Win7 API.
- Не тестировалось вручную: Windows 7 GUI, Windows 10/11 GUI, DPI 100/125/150/200, Light/Dark/High Contrast. Поэтому DirectWrite, layout threads и UX-функции не включались.

## Manual DPI/theme verification

Status: **Deferred — interactive Windows evidence required**. The executable checklist is
`docs/scintilla-manual-verification-checklist.md`; it covers special representations, EOL
annotations, Source chrome at every required DPI/theme, and autocomplete boundaries. Automated
contracts and release verification do not claim completion of this manual matrix.

## Memory и полный release gate

- Memory полного FBE Source view измеряется диагностическим режимом выше; результаты включают DOM, Body/UI, Source conversion buffer и Scintilla. `SCI_SETUNDOSELECTIONHISTORY` проверен также функционально в изолированном Scintilla control и на full-process нагрузке.
- Full Modern `verify-release` прошёл после пересборки устаревшего локального ImportEPUB артефакта; 32-bit COM registration smoke также завершился успешно. Код EPUB в этом этапе не менялся.
