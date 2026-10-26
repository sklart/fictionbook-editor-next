# C5: память Source / Scintilla

## Baseline и область изменения

Baseline C4.1: `c053b3a0ae682707f20a298c033ab7c257dd662f`.
Production fix PutBinaries и decoder regression зафиксированы отдельно в
`0b1b7d32fe75f41be037e1dc0ed675ed6d2a6e32`; reusable tooling и отчёт — в baseline.
Предыдущие результаты: [C4.1](performance-binary-store-c4-memory-attribution.md),
[C3](performance-binary-store-c3.md). Эти этапы не исследовались повторно.

C4.1 дал many/large Open+GC Private 173.70→91.76 / 164.32→83.32 MiB,
Source stable 791.89→685.14 / 758.14→677.77 MiB и maximum measured Peak Private
1000.93→894.15 / 958.96→878.58 MiB. Это отдельный binary benchmark;
его абсолютные значения нельзя напрямую вычитать из Source-specific C5.
Отрицательное наблюдение C4.1: many uncommitted reserve +11…18% в отдельных точках.

## Методика C5

Отдельный test-only сценарий `RuntimeTestSourceMemoryC5.inl`: Open Body,
Body→Source, принудительная стилизация, стабилизация, scroll, caret, Find,
Replace, небольшая правка, Source Undo/Redo, valid Apply в Body, idle,
второй Source, readonly возврат Body, Save, Close.
Scroll проходит начало/середину/конец, PageDown/PageUp; caret делает 120 перемещений;
Find использует настоящий Source search. Нет принудительного GC.

Каждый процесс изолирован, входы и runtime заморожены; many и large —
1 warm-up + 7 measured для каждой реализации, control — 1+3.
Word wrap явно включён (`XMLSrcWrap=true`, штатное значение по умолчанию).
Финальный runner вращает fixtures и порядок old/new отдельно для каждого fixture.
Physical input отсекается только в benchmark build; счётчик discarded input
сохраняется, текст Source проверяется на неизменность во время обеих стабилизаций.

После полного styling требуется три последовательных интервала по 100 ms
с вариацией Private Bytes ≤64 KiB (предел ожидания 30 s).
`first_layout` — дополнительный явный repaint после ShowView, часто почти пустой;
это **не** изолированная стоимость первоначального layout всего документа.
Полную работу и ожидание отражают `source_stabilization_wait` и
`time_to_source_stable`. Их нельзя заменять микровременем repaint.

Private Bytes, Working Set, OS Peak Private/Working Set, Virtual Size,
MEM_PRIVATE committed/reserved снимаются по стадиям; reserve не является commit.
Известные размеры Source/CString/XML отражают логические live objects, а не
полную атрибуцию heap Scintilla. Разница между cache policies — измеренная
атрибуция влияния layout, не точный счётчик outstanding allocations Scintilla.

Для статистики используются median, линейно интерполированный p90, min, max, N;
Δ = (new/old − 1) ×100%, paired Δ — медиана отношений одинаковых repeats.
Положительное Δ времени/памяти означает увеличение. Стабильный slowdown:
median >5% и ухудшение >5% минимум в 6/7 пар. Control N=3 этого критерия не доказывает.
Все equivalence checks независимо проверяют binary IDs, MIME, decoded SHA-256,
число binary, title, references и нормализованные paragraphs после Save.

## A. Layout cache experiment

`out/perf/c5/layout-matrix`: **80/80 PASS**, 12 warm-up +68 measured,
equivalence и hash audit PASS; полный набор статистики — `statistics.json`.
DOCUMENT/PAGE/CARET/NONE имеют N=7 на many/large, N=3 на control.

| Source stable Private, MiB | DOCUMENT | PAGE | CARET | NONE |
|---|---:|---:|---:|---:|
| many | 708.55 | 457.17 | 457.09 | 457.59 |
| large | 677.52 | 435.41 | 436.09 | 435.69 |
| control | 46.70 | 47.02 | 46.54 | 46.83 |

Память many/large снижается примерно на 35.5–35.7%, но каждый уменьшенный cache
даёт стабильную регрессию реального scroll workload:

| Fixture / policy | Scroll Δ | Дополнительные стабильные регрессии |
|---|---:|---|
| many PAGE | +260.39% (7/7) | — |
| many CARET | +2350.70% (7/7) | caret +159.40% (6/7) |
| many NONE | +10021.82% (7/7) | caret +25722.49% (7/7) |
| large PAGE | +9853.25% (7/7) | — |
| large CARET | +87525.67% (7/7) | caret +2496.30%, Find +162.16% (7/7) |
| large NONE | +397068.17% (7/7) | caret +403318.82%, Find +956.22% (7/7) |

Проценты велики из-за маленького исходного времени, но абсолютная задержка
тоже реальна: large PAGE scroll около 700 ms против около 7 ms DOCUMENT.
Выбран **SC_CACHE_DOCUMENT**, adaptive threshold не вводится.

## B. CString attribution

Исходный serialization cache сначала получал raw XML как CString, затем
добавление encoding в XML declaration увеличивало capacity примерно в 1.5 раза.
many: length 83.427 MiB, capacity 125.141 MiB, slack 41.714 MiB;
large: length 80.144 MiB, capacity 120.217 MiB, slack около 40.07 MiB.
Этот slack жил вместе с cached projection, а не только во время serialization.

Отдельный exact-capacity prototype: **8/8 PASS** (2 warm-up +6 measured,
N=3 на fixture), equivalence/hashes PASS. CString сохраняет только 10 bytes
allocator alignment сверх length на many/large вместо 40–42 MiB slack,
Source stable Private median 670.29 /637.79 MiB (many/large).
Это exploratory N=3, не финальная оценка эффекта C5.

## C. Serialization overlap и устаревший cache

Одновременно существуют XML DOM, его BSTR serialization, normalized UTF-16,
cached CString, временный UTF-8 для передачи Source и внутренний документ Scintilla.
После valid changed Apply новый XML заменяет saved snapshot, но старый
serialization cache всё ещё удерживал прежний XML и CString до следующего Source.
Этот cache уже не мог дать hit: его XML identity отличается от текущего saved XML.

Отдельный obsolete-cache prototype: **8/8 PASS**, N=3; Body idle Private
median 576.12 /573.16 MiB; cached capacity после Apply равна нулю.
Раннее освобождение BSTR как отдельный вариант не принято: серия остановлена
после 3 PASS из-за timeout стабилизации many; `serialization-early/progress.json`
имеет FAIL, оставшиеся запуски не выполнены. Этот результат не представлен как PASS.

## D. Release-on-Body: эксперимент и отклонение

Поздний test-only flush layout после Body idle:
`release-layout-paired-v3` **32/32 PASS** (4 warm-up +28 measured),
equivalence PASS, hash audit 3566/3566.
Body Private many 787.742→546.219 MiB (−30.66%),
large 759.715→515.715 MiB (−32.12%). Второй Source: −5.23% /−0.40%.
Source→Body плюс верхняя оценка flush: +8.00% (4/7), +8.76% (3/7),
не стабильные по принятому критерию. Верхняя оценка включает checkpoint overhead.
В этой отдельной серии порядок был сбалансирован глобально, но не внутри fixture;
финальный comparison исправляет это ограничение.

Первый combined candidate (exact CString + obsolete cache + немедленный flush)
прошёл 40/40 процессов и functional gates, но **отклонён**:
many Private сразу после Body 336.8 MiB →591.6 MiB после idle;
large 314.7→557.7 MiB. `end_styled` за это время растёт от 5 до полной длины Source:
скрытая idle-стилизация заново создаёт тяжёлый layout. Кратковременное уменьшение
памяти не является устойчивым выигрышем. Flush добавляет churn, поэтому в
окончательном production варианте его нет. Данные `final-comparison` относятся
к **отклонённому** варианту, окончательный результат находится в `accepted-comparison`.

Ранние серии v1/v2 также сохраняют свои FAIL: во время idle неожиданно изменялась
длина Source. Одного disable родительского frame оказалось недостаточно;
v3 отсекает physical input и проверяет no-edit invariant до и после обоих ожиданий.
Точная причина прежних изменений не объявляется доказанной.

## Final implementation

1. Сначала нормализовать declaration, затем создать свежий CString точной длины;
   не создавать и не расширять cached CString с raw XML. BSTR и normalization
   temporaries освобождаются по scope до публикации готовой cached projection.
   `FreeExtra`/shrink не используются; дополнительной posthoc shrink-копии нет.
2. После успешного Apply и переноса selection освободить serialization cache,
   только если его XML уже отличается от saved XML. Invalid Apply не затронут;
   readonly актуальный cache сохраняется для повторного входа без serialization.

Scintilla document, syntax state и whole-document layout policy сохранены.
IDL, Scintilla/Lexilla binaries, vendor content и production PutBinaries не меняются.

## Final acceptance

<!-- C5_FINAL_BEGIN -->
## Окончательные результаты C5

**PASS: 40/40**, warm-up 6/6, measured 34/34, equivalence 40/40, hash audit 3567/3567 PASS.
Many/large: по 7 measured на old и new; control: по 3. Warm-up исключены из статистики.
Все snapshots, fixtures и исходники benchmark оставались неизменны; сборки и sanitizers во время benchmark не запускались.

| Runtime | FBE.exe SHA-256 |
|---|---|
| C4.1 production baseline | `FAEB6E6894AF8CC5FE01935FD47CA589730855B92633D35449C64FEDFCE3C6F4` |
| old: C4.1 + одинаковый benchmark harness v3 | `AB4A0A570400D5AB50C4760280EF04A8E9BBDA1451D277EB8E494BF5F26AAB85` |
| new: окончательный C5 + harness v3 | `2C71F6C8F463A001487F1E7AD26D51C2248C772FA72C3743248D09E72479E9F2` |
| окончательный обычный Release C5 | `92F4672F76D69039A21C348F2619D7C2CB384768D1E7C3FA33B2AC725F3F1D2C` |

Build stamp до commit указывает C4.1 HEAD; фактически скомпилированные C5 исходники зафиксированы полным sourceHashes в `accepted-runtime/c5-build.json`.
Эти hashes, а не один старый build stamp, связывают локальные binaries с финальным исходным деревом. Внешний Build должен собирать уже точный C5 commit.

| Неизменённый вход | SHA-256 |
|---|---|
| main.js | `EC80D417412CBFF6E3B6E1305B86B2FD407553402D81A1DB0AC3D83B53E8D1A1` |
| Scintilla.dll | `65EF429ACF622537FFEEE7ACF1228AF4F7DD195A98D63D8A454C7BC24301858D` |
| Lexilla.dll | `3CE8D678D9F8E912B666C05FC702308A8BD08C9C205072428840168676F15A9B` |
| fixture control | `CC7185478E46F167EE3B2DCC81165E879B803B54D8AF6C10388BFB9B30BD97D8` |
| fixture many | `BE92439E63FEC2E07B3B7C7E8B344795DC3034523F88681549302DDA9EC9E518` |
| fixture large | `5902154562155727C438C7A18C16D946286926882B8059E8946784AF78F01703` |

### Главные показатели памяти

| Fixture / metric | C4.1, MiB | C5, MiB | Δ % | paired Δ % |
|---|---:|---:|---:|---:|
| many Source stable Private | 709.36 | 667.81 | -5.86 | -5.86 |
| many Source Peak Private | 710.46 | 667.81 | -6.00 | -5.97 |
| many Body after edited Source + idle | 801.62 | 590.23 | -26.37 | -26.44 |
| many Body after readonly second Source + idle | 746.23 | 703.32 | -5.75 | -5.69 |
| many overall scenario Peak Private | 1173.41 | 1047.48 | -10.73 | -10.71 |
| large Source stable Private | 677.77 | 637.73 | -5.91 | -5.90 |
| large Source Peak Private | 678.56 | 638.49 | -5.91 | -5.89 |
| large Body after edited Source + idle | 759.48 | 558.72 | -26.43 | -26.42 |
| large Body after readonly second Source + idle | 699.43 | 659.64 | -5.69 | -5.74 |
| large overall scenario Peak Private | 1120.71 | 1046.77 | -6.60 | -6.57 |

### Final functional / sanitizer gates

| Gate | Result / evidence |
|---|---|
| Release Win32 /W4 /WX | PASS; actual CL command log hashes: `accepted-release-evidence.json` |
| Source affected gates | 8/8 PASS: UI, Body/Source, malformed correction/fallback, XML declaration Save/Reopen, XML script backend Undo/Redo, theme, spellcheck scroll |
| Legacy / bundled | 46/46 +6/6 PASS |
| Binary runtime / decoder | PASS / PASS |
| ASan-targeted | 3/3 PASS coverage; `out/tests/asan/20261026-131843-f85e097beaa6457b87e4bae1d26cf1bb/summary.json`; infrastructure self-test PASS |
| ASan-FAST | 84/84 PASS coverage; `out/perf/c5/accepted-asan-fast-coverage.json`; infrastructure self-test PASS; initial FAST 82/84, affected observation retries 2/2 PASS; original FAIL receipts retained |
| UBSan | NOT RUN for C5: no new unchecked arithmetic/data representation; changed CString/COM lifetime covered by ASan. Closed C3 UBSan evidence remains separate. |

Source UI дополнительно проверяет syntax/styles, caret, selection и Undo; benchmark покрывает many/large Find/Replace/edit/second entry и независимый roundtrip после Save.
Исходное предположение caret harness не выполнялось и на замороженном C4.1 exe: тест требовал немедленного восстановления MSHTML caret без подготовки collapsed selection и ожидания асинхронной геометрии.
Теперь harness готовит selection и до 2 s ожидает настоящий visible/locatable caret и foreground window; все исходные 12 assertions сохранены, добавлены три строгие проверки. Source editor UI сохраняет все 17 исходных проверок.

Неполное наблюдение FAST не считается native PASS: исходные receipts остаются FAIL. Повторяются только затронутые cases, с идентичными scenario commands и hashes instrumented modules; aggregate receipt указывает источник каждого из всех 84 результатов.
`ensure-fbe-api.ps1` повторно использует MSBuild выбранного importer VS instance; прежний vswhere fallback сохраняется, если путь недоступен. Это убирает лишний короткий discovery process на каждый тест и удерживает MIDL/CL в одном VS instance. Строгие fail-closed проверки process monitor, masked-child/UAF self-tests и runtime assertions не менялись.

### Все median увеличения >5%

Ниже перечислены все увеличения реальных time/memory метрик; счётчики состояния/объектов не считаются временем или потреблением памяти. Control N=3 отмечен отдельно от критерия 6/7.

| Fixture | Stage / metric | old → new | Δ % | paired Δ % | >5% pairs | stable 6/7 |
|---|---|---:|---:|---:|---:|---|
| control | time / caret_workload | 5.541 → 6.058 ms | +9.32 | +12.19 | 3/3 | NO |
| control | time / first_layout | 0.002 → 0.003 ms | +13.64 | +4.17 | 1/3 | NO |
| control | time / open_body | 196.255 → 207.903 ms | +5.93 | +6.03 | 2/3 | NO |
| control | time / scroll_workload | 5.671 → 6.001 ms | +5.83 | +16.04 | 2/3 | NO |
| large | time / caret_workload | 19.745 → 21.438 ms | +8.57 | +3.47 | 3/7 | NO |
| large | time / first_layout | 0.003 → 0.003 ms | +24.00 | +22.73 | 6/7 | YES |
| many | body_after_source / private_reserve | 88.012 → 174.254 MiB | +97.99 | +98.58 | 7/7 | YES |
| many | body_idle / private_reserve | 87.211 → 173.254 MiB | +98.66 | +98.20 | 7/7 | YES |
| many | time / first_layout | 0.003 → 0.003 ms | +12.00 | +16.13 | 4/7 | NO |
| many | time / small_edit | 0.214 → 0.230 ms | +7.04 | +7.90 | 4/7 | NO |

### Полная статистика времени

Единица — ms. Каждая old/new ячейка: **median / p90 / min / max**. N относится отдельно к каждой реализации.

| Fixture | Operation | C4.1 | C5 | N | Δ % | paired Δ % | >5% pairs |
|---|---|---:|---:|---:|---:|---:|---:|
| control | body_to_source | 37.951 / 39.674 / 35.965 / 40.104 | 38.887 / 41.739 / 38.456 / 42.452 | 3 | +2.47 | +1.33 | 1/3 |
| control | caret_workload | 5.541 / 5.601 / 5.066 / 5.616 | 6.058 / 6.252 / 6.006 / 6.301 | 3 | +9.32 | +12.19 | 3/3 |
| control | find_workload | 1.386 / 1.400 / 1.379 / 1.403 | 1.417 / 1.534 / 1.400 / 1.563 | 3 | +2.24 | +2.24 | 1/3 |
| control | first_layout | 0.002 / 0.002 / 0.002 / 0.002 | 0.003 / 0.014 / 0.002 / 0.017 | 3 | +13.64 | +4.17 | 1/3 |
| control | open_body | 196.255 / 199.179 / 195.513 / 199.910 | 207.903 / 211.158 / 195.875 / 211.972 | 3 | +5.93 | +6.03 | 2/3 |
| control | replace | 0.635 / 1.372 / 0.594 / 1.556 | 0.581 / 0.590 / 0.577 / 0.592 | 3 | -8.49 | -6.70 | 0/3 |
| control | return_body | 49.503 / 50.754 / 46.789 / 51.067 | 46.682 / 48.340 / 45.112 / 48.755 | 3 | -5.70 | -8.59 | 0/3 |
| control | scroll_workload | 5.671 / 5.812 / 4.752 / 5.847 | 6.001 / 7.994 / 5.514 / 8.492 | 3 | +5.83 | +16.04 | 2/3 |
| control | second_body_to_source | 33.540 / 34.080 / 32.994 / 34.215 | 34.420 / 36.600 / 33.201 / 37.145 | 3 | +2.63 | +4.32 | 1/3 |
| control | second_source_stable | 763.045 / 785.718 / 756.646 / 791.386 | 764.393 / 764.912 / 759.466 / 765.042 | 3 | +0.18 | +0.18 | 0/3 |
| control | small_edit | 0.137 / 0.195 / 0.133 / 0.210 | 0.142 / 0.204 / 0.134 / 0.219 | 3 | +3.72 | +4.34 | 1/3 |
| control | source_stabilization_wait | 434.450 / 435.986 / 428.836 / 436.370 | 439.041 / 449.410 / 437.449 / 452.002 | 3 | +1.06 | +2.01 | 0/3 |
| control | source_to_body | 123.763 / 123.861 / 119.538 / 123.886 | 124.459 / 125.441 / 122.597 / 125.686 | 3 | +0.56 | +1.55 | 0/3 |
| control | time_to_source_stable | 477.407 / 479.662 / 475.153 / 480.225 | 484.117 / 497.460 / 482.614 / 500.796 | 3 | +1.41 | +1.57 | 0/3 |
| large | body_to_source | 829.745 / 842.089 / 820.888 / 846.688 | 775.213 / 784.119 / 767.573 / 792.697 | 7 | -6.57 | -6.47 | 0/7 |
| large | caret_workload | 19.745 / 21.262 / 17.977 / 21.789 | 21.438 / 21.621 / 18.671 / 21.636 | 7 | +8.57 | +3.47 | 3/7 |
| large | find_workload | 141.670 / 146.838 / 139.567 / 148.137 | 142.544 / 145.981 / 138.929 / 146.533 | 7 | +0.62 | -0.18 | 0/7 |
| large | first_layout | 0.003 / 0.003 / 0.002 / 0.004 | 0.003 / 0.004 / 0.002 / 0.004 | 7 | +24.00 | +22.73 | 6/7 |
| large | open_body | 1004.533 / 1019.559 / 996.744 / 1027.686 | 1011.011 / 1019.271 / 989.721 / 1021.180 | 7 | +0.64 | +0.23 | 0/7 |
| large | replace | 6.194 / 6.592 / 5.526 / 6.608 | 6.301 / 6.775 / 5.454 / 6.996 | 7 | +1.73 | +4.36 | 3/7 |
| large | return_body | 111.509 / 113.005 / 104.913 / 113.136 | 110.934 / 112.701 / 106.890 / 113.498 | 7 | -0.52 | -0.13 | 1/7 |
| large | scroll_workload | 7.852 / 8.621 / 7.126 / 9.023 | 7.671 / 8.499 / 6.997 / 8.565 | 7 | -2.30 | +0.04 | 2/7 |
| large | second_body_to_source | 837.112 / 862.507 / 830.862 / 868.812 | 806.257 / 819.125 / 803.300 / 822.489 | 7 | -3.69 | -4.00 | 0/7 |
| large | second_source_stable | 3509.077 / 3567.588 / 3357.969 / 3606.298 | 3496.637 / 3552.301 / 3370.827 / 3572.260 | 7 | -0.35 | +0.30 | 0/7 |
| large | small_edit | 0.172 / 0.268 / 0.147 / 0.269 | 0.154 / 0.215 / 0.147 / 0.256 | 7 | -9.97 | -0.29 | 2/7 |
| large | source_stabilization_wait | 3647.361 / 3671.657 / 3502.547 / 3679.232 | 3605.658 / 3729.597 / 3489.222 / 3754.405 | 7 | -1.14 | -0.62 | 0/7 |
| large | source_to_body | 970.819 / 980.544 / 957.672 / 991.253 | 974.275 / 988.076 / 966.983 / 1008.291 | 7 | +0.36 | +0.56 | 0/7 |
| large | time_to_source_stable | 4502.487 / 4507.695 / 4338.851 / 4510.453 | 4393.537 / 4516.765 / 4272.487 / 4556.883 | 7 | -2.42 | -2.08 | 0/7 |
| many | body_to_source | 776.224 / 783.346 / 765.403 / 786.850 | 710.419 / 728.816 / 707.696 / 740.154 | 7 | -8.48 | -7.80 | 0/7 |
| many | caret_workload | 5.964 / 6.345 / 5.325 / 6.346 | 5.261 / 5.979 / 4.753 / 5.981 | 7 | -11.78 | -5.73 | 1/7 |
| many | find_workload | 140.575 / 142.286 / 136.653 / 143.611 | 137.130 / 137.389 / 135.869 / 137.495 | 7 | -2.45 | -2.62 | 0/7 |
| many | first_layout | 0.003 / 0.003 / 0.002 / 0.003 | 0.003 / 0.005 / 0.002 / 0.006 | 7 | +12.00 | +16.13 | 4/7 |
| many | open_body | 1086.348 / 1110.635 / 1075.680 / 1118.357 | 1078.064 / 1147.684 / 1073.200 / 1166.827 | 7 | -0.76 | -0.28 | 1/7 |
| many | replace | 6.845 / 6.923 / 5.734 / 6.976 | 6.017 / 6.586 / 5.646 / 6.814 | 7 | -12.10 | -4.96 | 1/7 |
| many | return_body | 257.835 / 265.136 / 251.817 / 267.952 | 260.199 / 266.052 / 248.174 / 267.992 | 7 | +0.92 | -0.94 | 1/7 |
| many | scroll_workload | 5.198 / 5.569 / 4.331 / 5.622 | 5.272 / 5.745 / 4.343 / 5.947 | 7 | +1.44 | +0.97 | 3/7 |
| many | second_body_to_source | 802.496 / 823.428 / 782.521 / 841.172 | 758.560 / 765.411 / 740.545 / 768.029 | 7 | -5.47 | -6.54 | 0/7 |
| many | second_source_stable | 3672.359 / 3779.996 / 3640.312 / 3849.106 | 3798.927 / 3874.238 / 3635.272 / 3885.053 | 7 | +3.45 | +1.28 | 2/7 |
| many | small_edit | 0.214 / 0.220 / 0.199 / 0.222 | 0.230 / 0.624 / 0.189 / 1.196 | 7 | +7.04 | +7.90 | 4/7 |
| many | source_stabilization_wait | 3917.852 / 3966.870 / 3861.222 / 4002.753 | 3787.764 / 3857.195 / 3727.656 / 3907.784 | 7 | -3.32 | -3.58 | 0/7 |
| many | source_to_body | 1034.321 / 1049.308 / 1026.191 / 1058.211 | 1042.840 / 1059.695 / 1013.174 / 1066.915 | 7 | +0.82 | +0.82 | 0/7 |
| many | time_to_source_stable | 4709.851 / 4752.530 / 4637.439 / 4790.481 | 4507.311 / 4597.313 / 4447.048 / 4630.328 | 7 | -4.30 | -4.23 | 0/7 |

### Полная статистика памяти по стадиям

Единица — MiB. Каждая old/new ячейка: **median / p90 / min / max**; N — на реализацию. private_reserve — только uncommitted MEM_PRIVATE reserve, отдельно от private_commit и OS Private Bytes.

| Fixture | Stage / metric | C4.1 | C5 | N | Δ % | paired Δ % |
|---|---|---:|---:|---:|---:|---:|
| control | after_caret / peak_private_bytes | 47.105 / 49.818 / 47.039 / 50.496 | 47.238 / 47.248 / 47.020 / 47.250 | 3 | +0.28 | -0.18 |
| control | after_caret / peak_working_set | 81.973 / 81.991 / 81.801 / 81.996 | 81.852 / 81.992 / 81.754 / 82.027 | 3 | -0.15 | -0.06 |
| control | after_caret / private_bytes | 46.621 / 49.012 / 46.527 / 49.609 | 46.480 / 46.505 / 46.441 / 46.512 | 3 | -0.30 | -0.23 |
| control | after_caret / private_commit | 38.934 / 39.230 / 38.871 / 39.305 | 38.871 / 38.887 / 38.863 / 38.891 | 3 | -0.16 | -0.11 |
| control | after_caret / private_reserve | 78.484 / 78.606 / 78.203 / 78.637 | 78.574 / 78.668 / 78.465 / 78.691 | 3 | +0.11 | +0.07 |
| control | after_caret / virtual_size | 407.789 / 407.789 / 407.699 / 407.789 | 407.723 / 407.823 / 407.641 / 407.848 | 3 | -0.02 | -0.01 |
| control | after_caret / working_set | 81.793 / 81.955 / 81.656 / 81.996 | 81.723 / 81.748 / 81.707 / 81.754 | 3 | -0.09 | -0.09 |
| control | after_close / peak_private_bytes | 49.801 / 51.841 / 49.754 / 52.352 | 49.539 / 49.992 / 49.535 / 50.105 | 3 | -0.53 | -0.53 |
| control | after_close / peak_working_set | 85.117 / 85.242 / 84.941 / 85.273 | 85.184 / 85.427 / 85.125 / 85.488 | 3 | +0.08 | +0.22 |
| control | after_close / private_bytes | 49.559 / 51.549 / 49.125 / 52.047 | 49.117 / 49.208 / 48.922 / 49.230 | 3 | -0.89 | -0.89 |
| control | after_close / private_commit | 41.730 / 41.909 / 41.359 / 41.953 | 41.516 / 41.566 / 41.336 / 41.578 | 3 | -0.51 | -0.95 |
| control | after_close / private_reserve | 77.215 / 77.562 / 77.152 / 77.648 | 77.422 / 77.466 / 77.215 / 77.477 | 3 | +0.27 | +0.00 |
| control | after_close / virtual_size | 406.238 / 406.366 / 406.113 / 406.398 | 406.047 / 406.197 / 405.965 / 406.234 | 3 | -0.05 | -0.02 |
| control | after_close / working_set | 84.699 / 84.846 / 84.500 / 84.883 | 84.617 / 84.620 / 84.516 / 84.621 | 3 | -0.10 | -0.22 |
| control | after_find / peak_private_bytes | 47.105 / 49.818 / 47.039 / 50.496 | 47.238 / 47.248 / 47.020 / 47.250 | 3 | +0.28 | -0.18 |
| control | after_find / peak_working_set | 81.973 / 82.004 / 81.801 / 82.012 | 81.852 / 81.992 / 81.770 / 82.027 | 3 | -0.15 | -0.04 |
| control | after_find / private_bytes | 46.621 / 49.012 / 46.527 / 49.609 | 46.480 / 46.505 / 46.441 / 46.512 | 3 | -0.30 | -0.23 |
| control | after_find / private_commit | 38.934 / 39.230 / 38.871 / 39.305 | 38.871 / 38.887 / 38.863 / 38.891 | 3 | -0.16 | -0.11 |
| control | after_find / private_reserve | 78.484 / 78.606 / 78.203 / 78.637 | 78.574 / 78.668 / 78.465 / 78.691 | 3 | +0.11 | +0.07 |
| control | after_find / virtual_size | 407.789 / 407.789 / 407.699 / 407.789 | 407.723 / 407.823 / 407.641 / 407.848 | 3 | -0.02 | -0.01 |
| control | after_find / working_set | 81.809 / 81.971 / 81.688 / 82.012 | 81.738 / 81.763 / 81.723 / 81.770 | 3 | -0.09 | -0.09 |
| control | after_replace / peak_private_bytes | 47.105 / 49.818 / 47.039 / 50.496 | 47.238 / 47.248 / 47.020 / 47.250 | 3 | +0.28 | -0.18 |
| control | after_replace / peak_working_set | 81.973 / 82.010 / 81.801 / 82.020 | 81.852 / 81.992 / 81.773 / 82.027 | 3 | -0.15 | -0.03 |
| control | after_replace / private_bytes | 46.621 / 49.012 / 46.527 / 49.609 | 46.480 / 46.505 / 46.441 / 46.512 | 3 | -0.30 | -0.23 |
| control | after_replace / private_commit | 38.934 / 39.230 / 38.871 / 39.305 | 38.871 / 38.887 / 38.863 / 38.891 | 3 | -0.16 | -0.11 |
| control | after_replace / private_reserve | 78.484 / 78.606 / 78.203 / 78.637 | 78.574 / 78.668 / 78.465 / 78.691 | 3 | +0.11 | +0.07 |
| control | after_replace / virtual_size | 407.789 / 407.789 / 407.699 / 407.789 | 407.723 / 407.823 / 407.641 / 407.848 | 3 | -0.02 | -0.01 |
| control | after_replace / working_set | 81.816 / 81.979 / 81.695 / 82.020 | 81.742 / 81.767 / 81.727 / 81.773 | 3 | -0.09 | -0.09 |
| control | after_scroll / peak_private_bytes | 47.105 / 49.818 / 47.039 / 50.496 | 47.238 / 47.248 / 47.020 / 47.250 | 3 | +0.28 | -0.18 |
| control | after_scroll / peak_working_set | 81.973 / 81.991 / 81.801 / 81.996 | 81.852 / 81.992 / 81.754 / 82.027 | 3 | -0.15 | -0.06 |
| control | after_scroll / private_bytes | 46.621 / 49.012 / 46.527 / 49.609 | 46.480 / 46.505 / 46.441 / 46.512 | 3 | -0.30 | -0.23 |
| control | after_scroll / private_commit | 38.934 / 39.230 / 38.871 / 39.305 | 38.871 / 38.887 / 38.863 / 38.891 | 3 | -0.16 | -0.11 |
| control | after_scroll / private_reserve | 78.484 / 78.606 / 78.203 / 78.637 | 78.574 / 78.668 / 78.465 / 78.691 | 3 | +0.11 | +0.07 |
| control | after_scroll / virtual_size | 407.789 / 407.789 / 407.699 / 407.789 | 407.723 / 407.823 / 407.641 / 407.848 | 3 | -0.02 | -0.01 |
| control | after_scroll / working_set | 81.793 / 81.955 / 81.656 / 81.996 | 81.723 / 81.748 / 81.707 / 81.754 | 3 | -0.09 | -0.09 |
| control | after_small_edit / peak_private_bytes | 47.105 / 49.818 / 47.039 / 50.496 | 47.238 / 47.248 / 47.020 / 47.250 | 3 | +0.28 | -0.18 |
| control | after_small_edit / peak_working_set | 81.973 / 82.016 / 81.801 / 82.027 | 81.852 / 81.992 / 81.781 / 82.027 | 3 | -0.15 | -0.02 |
| control | after_small_edit / private_bytes | 46.621 / 49.012 / 46.527 / 49.609 | 46.480 / 46.505 / 46.441 / 46.512 | 3 | -0.30 | -0.23 |
| control | after_small_edit / private_commit | 38.934 / 39.230 / 38.871 / 39.305 | 38.871 / 38.887 / 38.863 / 38.891 | 3 | -0.16 | -0.11 |
| control | after_small_edit / private_reserve | 78.484 / 78.606 / 78.203 / 78.637 | 78.574 / 78.668 / 78.465 / 78.691 | 3 | +0.11 | +0.07 |
| control | after_small_edit / virtual_size | 407.789 / 407.789 / 407.699 / 407.789 | 407.723 / 407.823 / 407.641 / 407.848 | 3 | -0.02 | -0.01 |
| control | after_small_edit / working_set | 81.824 / 81.987 / 81.703 / 82.027 | 81.750 / 81.775 / 81.734 / 81.781 | 3 | -0.09 | -0.09 |
| control | after_verification_save / peak_private_bytes | 49.055 / 51.523 / 48.828 / 52.141 | 49.039 / 49.492 / 48.762 / 49.605 | 3 | -0.03 | -0.14 |
| control | after_verification_save / peak_working_set | 84.496 / 84.690 / 84.469 / 84.738 | 84.672 / 84.984 / 84.379 / 85.062 | 3 | +0.21 | -0.08 |
| control | after_verification_save / private_bytes | 48.578 / 50.884 / 47.910 / 51.461 | 48.773 / 49.227 / 48.375 / 49.340 | 3 | +0.40 | +0.97 |
| control | after_verification_save / private_commit | 40.824 / 41.090 / 40.316 / 41.156 | 41.195 / 41.598 / 40.785 / 41.699 | 3 | +0.91 | +1.16 |
| control | after_verification_save / private_reserve | 76.684 / 77.018 / 76.227 / 77.102 | 76.801 / 77.054 / 76.445 / 77.117 | 3 | +0.15 | +0.15 |
| control | after_verification_save / virtual_size | 407.699 / 407.771 / 407.664 / 407.789 | 408.598 / 408.748 / 407.516 / 408.785 | 3 | +0.22 | +0.23 |
| control | after_verification_save / working_set | 84.203 / 84.362 / 83.570 / 84.402 | 84.590 / 84.868 / 84.184 / 84.938 | 3 | +0.46 | +0.73 |
| control | before_close / peak_private_bytes | 49.055 / 51.523 / 48.828 / 52.141 | 49.039 / 49.492 / 48.762 / 49.605 | 3 | -0.03 | -0.14 |
| control | before_close / peak_working_set | 84.496 / 84.690 / 84.469 / 84.738 | 84.672 / 84.984 / 84.379 / 85.062 | 3 | +0.21 | -0.08 |
| control | before_close / private_bytes | 48.320 / 50.702 / 47.438 / 51.297 | 48.496 / 48.959 / 48.062 / 49.074 | 3 | +0.36 | +1.32 |
| control | before_close / private_commit | 40.566 / 40.907 / 39.844 / 40.992 | 40.918 / 41.330 / 40.473 / 41.434 | 3 | +0.87 | +1.58 |
| control | before_close / private_reserve | 76.941 / 77.448 / 76.391 / 77.574 | 77.066 / 77.329 / 76.758 / 77.395 | 3 | +0.16 | +0.16 |
| control | before_close / virtual_size | 407.699 / 407.771 / 407.664 / 407.789 | 408.598 / 408.748 / 407.516 / 408.785 | 3 | +0.22 | +0.23 |
| control | before_close / working_set | 83.941 / 84.032 / 83.145 / 84.055 | 84.254 / 84.566 / 83.859 / 84.645 | 3 | +0.37 | +0.84 |
| control | body_after_source / peak_private_bytes | 48.934 / 51.499 / 48.785 / 52.141 | 48.902 / 48.949 / 48.762 / 48.961 | 3 | -0.06 | -0.05 |
| control | body_after_source / peak_working_set | 84.301 / 84.535 / 84.215 / 84.594 | 84.316 / 84.323 / 84.297 / 84.324 | 3 | +0.02 | +0.03 |
| control | body_after_source / private_bytes | 48.934 / 51.455 / 48.496 / 52.086 | 48.629 / 48.860 / 48.574 / 48.918 | 3 | -0.62 | -0.03 |
| control | body_after_source / private_commit | 41.180 / 41.661 / 40.902 / 41.781 | 41.051 / 41.232 / 40.984 / 41.277 | 3 | -0.31 | +0.20 |
| control | body_after_source / private_reserve | 76.453 / 76.603 / 75.727 / 76.641 | 76.371 / 76.384 / 76.285 / 76.387 | 3 | -0.11 | -0.22 |
| control | body_after_source / virtual_size | 407.824 / 407.896 / 407.789 / 407.914 | 407.723 / 407.823 / 407.641 / 407.848 | 3 | -0.02 | -0.02 |
| control | body_after_source / working_set | 84.301 / 84.535 / 84.098 / 84.594 | 84.242 / 84.308 / 84.211 / 84.324 | 3 | -0.07 | +0.03 |
| control | body_idle / peak_private_bytes | 48.934 / 51.499 / 48.785 / 52.141 | 48.902 / 48.949 / 48.762 / 48.961 | 3 | -0.06 | -0.05 |
| control | body_idle / peak_working_set | 84.309 / 84.580 / 84.215 / 84.648 | 84.316 / 84.329 / 84.297 / 84.332 | 3 | +0.01 | +0.03 |
| control | body_idle / private_bytes | 48.934 / 51.412 / 48.496 / 52.031 | 48.629 / 48.860 / 48.520 / 48.918 | 3 | -0.62 | -0.03 |
| control | body_idle / private_commit | 41.180 / 41.617 / 40.902 / 41.727 | 41.051 / 41.232 / 40.930 / 41.277 | 3 | -0.31 | +0.07 |
| control | body_idle / private_reserve | 76.453 / 76.603 / 75.781 / 76.641 | 76.387 / 76.418 / 76.285 / 76.426 | 3 | -0.09 | -0.22 |
| control | body_idle / virtual_size | 407.824 / 407.896 / 407.789 / 407.914 | 407.723 / 407.823 / 407.641 / 407.848 | 3 | -0.02 | -0.02 |
| control | body_idle / working_set | 84.309 / 84.580 / 84.105 / 84.648 | 84.285 / 84.323 / 84.223 / 84.332 | 3 | -0.03 | +0.03 |
| control | first_paint_complete / peak_private_bytes | 47.105 / 49.818 / 47.039 / 50.496 | 47.238 / 47.248 / 47.020 / 47.250 | 3 | +0.28 | -0.18 |
| control | first_paint_complete / peak_working_set | 81.855 / 81.949 / 81.801 / 81.973 | 81.852 / 81.992 / 81.727 / 82.027 | 3 | -0.00 | -0.09 |
| control | first_paint_complete / private_bytes | 46.609 / 49.000 / 46.516 / 49.598 | 46.461 / 46.492 / 46.430 / 46.500 | 3 | -0.32 | -0.23 |
| control | first_paint_complete / private_commit | 38.922 / 39.219 / 38.859 / 39.293 | 38.859 / 38.869 / 38.852 / 38.871 | 3 | -0.16 | -0.13 |
| control | first_paint_complete / private_reserve | 78.496 / 78.618 / 78.215 / 78.648 | 78.586 / 78.680 / 78.484 / 78.703 | 3 | +0.11 | +0.07 |
| control | first_paint_complete / virtual_size | 407.789 / 407.789 / 407.699 / 407.789 | 407.723 / 407.823 / 407.641 / 407.848 | 3 | -0.02 | -0.01 |
| control | first_paint_complete / working_set | 81.574 / 81.740 / 81.445 / 81.781 | 81.512 / 81.527 / 81.488 / 81.531 | 3 | -0.08 | -0.08 |
| control | initial_layout_complete / peak_private_bytes | 47.105 / 49.818 / 47.039 / 50.496 | 47.238 / 47.248 / 47.020 / 47.250 | 3 | +0.28 | -0.18 |
| control | initial_layout_complete / peak_working_set | 81.973 / 81.985 / 81.801 / 81.988 | 81.852 / 81.992 / 81.746 / 82.027 | 3 | -0.15 | -0.07 |
| control | initial_layout_complete / private_bytes | 46.621 / 49.012 / 46.527 / 49.609 | 46.480 / 46.505 / 46.441 / 46.512 | 3 | -0.30 | -0.23 |
| control | initial_layout_complete / private_commit | 38.934 / 39.230 / 38.871 / 39.305 | 38.871 / 38.887 / 38.863 / 38.891 | 3 | -0.16 | -0.11 |
| control | initial_layout_complete / private_reserve | 78.484 / 78.606 / 78.203 / 78.637 | 78.574 / 78.668 / 78.465 / 78.691 | 3 | +0.11 | +0.07 |
| control | initial_layout_complete / virtual_size | 407.789 / 407.789 / 407.699 / 407.789 | 407.723 / 407.823 / 407.641 / 407.848 | 3 | -0.02 | -0.01 |
| control | initial_layout_complete / working_set | 81.785 / 81.948 / 81.648 / 81.988 | 81.715 / 81.740 / 81.699 / 81.746 | 3 | -0.09 | -0.09 |
| control | open_body / peak_private_bytes | 47.105 / 49.818 / 46.945 / 50.496 | 47.238 / 47.248 / 47.020 / 47.250 | 3 | +0.28 | -0.18 |
| control | open_body / peak_working_set | 81.723 / 81.813 / 81.699 / 81.836 | 81.852 / 81.992 / 81.727 / 82.027 | 3 | +0.16 | +0.16 |
| control | open_body / private_bytes | 46.914 / 49.189 / 46.754 / 49.758 | 46.809 / 46.837 / 46.777 / 46.844 | 3 | -0.22 | -0.15 |
| control | open_body / private_commit | 39.164 / 39.395 / 39.160 / 39.453 | 39.203 / 39.216 / 39.199 / 39.219 | 3 | +0.10 | +0.10 |
| control | open_body / private_reserve | 78.258 / 78.327 / 78.055 / 78.344 | 78.238 / 78.335 / 78.137 / 78.359 | 3 | -0.02 | +0.02 |
| control | open_body / virtual_size | 407.789 / 407.789 / 407.699 / 407.789 | 407.723 / 407.823 / 407.641 / 407.848 | 3 | -0.02 | -0.01 |
| control | open_body / working_set | 81.723 / 81.795 / 81.590 / 81.812 | 81.695 / 81.714 / 81.645 / 81.719 | 3 | -0.03 | -0.00 |
| control | process_initialized / peak_private_bytes | 44.367 / 46.570 / 44.328 / 47.121 | 44.441 / 44.948 / 44.312 / 45.074 | 3 | +0.17 | -0.04 |
| control | process_initialized / peak_working_set | 78.531 / 78.550 / 78.449 / 78.555 | 78.523 / 79.108 / 78.344 / 79.254 | 3 | -0.01 | -0.04 |
| control | process_initialized / private_bytes | 44.176 / 46.382 / 44.133 / 46.934 | 44.250 / 44.756 / 44.117 / 44.883 | 3 | +0.17 | -0.04 |
| control | process_initialized / private_commit | 36.566 / 36.635 / 36.449 / 36.652 | 36.629 / 37.185 / 36.555 / 37.324 | 3 | +0.17 | +0.49 |
| control | process_initialized / private_reserve | 76.605 / 76.718 / 76.602 / 76.746 | 76.621 / 76.815 / 76.551 / 76.863 | 3 | +0.02 | -0.07 |
| control | process_initialized / virtual_size | 403.430 / 403.480 / 403.402 / 403.492 | 403.488 / 404.238 / 403.344 / 404.426 | 3 | +0.01 | +0.01 |
| control | process_initialized / working_set | 78.422 / 78.438 / 78.340 / 78.441 | 78.410 / 79.004 / 78.230 / 79.152 | 3 | -0.01 | -0.04 |
| control | return_body_idle / peak_private_bytes | 49.055 / 51.523 / 48.828 / 52.141 | 49.039 / 49.492 / 48.762 / 49.605 | 3 | -0.03 | -0.14 |
| control | return_body_idle / peak_working_set | 84.496 / 84.690 / 84.469 / 84.738 | 84.672 / 84.984 / 84.379 / 85.062 | 3 | +0.21 | -0.08 |
| control | return_body_idle / private_bytes | 48.320 / 50.702 / 47.438 / 51.297 | 48.496 / 48.959 / 48.062 / 49.074 | 3 | +0.36 | +1.32 |
| control | return_body_idle / private_commit | 40.566 / 40.907 / 39.844 / 40.992 | 40.918 / 41.330 / 40.473 / 41.434 | 3 | +0.87 | +1.58 |
| control | return_body_idle / private_reserve | 76.941 / 77.448 / 76.391 / 77.574 | 77.066 / 77.329 / 76.758 / 77.395 | 3 | +0.16 | +0.16 |
| control | return_body_idle / virtual_size | 407.699 / 407.771 / 407.664 / 407.789 | 408.598 / 408.748 / 407.516 / 408.785 | 3 | +0.22 | +0.23 |
| control | return_body_idle / working_set | 83.941 / 84.032 / 83.145 / 84.055 | 84.254 / 84.566 / 83.859 / 84.645 | 3 | +0.37 | +0.84 |
| control | second_source_generated / peak_private_bytes | 49.055 / 51.523 / 48.828 / 52.141 | 48.957 / 49.326 / 48.762 / 49.418 | 3 | -0.20 | -0.14 |
| control | second_source_generated / peak_working_set | 84.496 / 84.690 / 84.469 / 84.738 | 84.496 / 84.727 / 84.379 / 84.785 | 3 | +0.00 | -0.11 |
| control | second_source_generated / private_bytes | 49.055 / 51.445 / 48.828 / 52.043 | 48.957 / 49.326 / 48.598 / 49.418 | 3 | -0.20 | -0.47 |
| control | second_source_generated / private_commit | 41.301 / 41.651 / 41.234 / 41.738 | 41.379 / 41.698 / 41.008 / 41.777 | 3 | +0.19 | -0.55 |
| control | second_source_generated / private_reserve | 76.309 / 76.327 / 75.770 / 76.332 | 76.848 / 77.016 / 76.348 / 77.059 | 3 | +0.71 | +0.68 |
| control | second_source_generated / virtual_size | 407.824 / 407.896 / 407.789 / 407.914 | 408.723 / 408.873 / 407.641 / 408.910 | 3 | +0.22 | +0.23 |
| control | second_source_generated / working_set | 84.496 / 84.690 / 84.469 / 84.738 | 84.496 / 84.727 / 84.379 / 84.785 | 3 | +0.00 | -0.11 |
| control | second_source_stable / peak_private_bytes | 49.055 / 51.523 / 48.828 / 52.141 | 48.957 / 49.326 / 48.762 / 49.418 | 3 | -0.20 | -0.14 |
| control | second_source_stable / peak_working_set | 84.496 / 84.690 / 84.469 / 84.738 | 84.496 / 84.727 / 84.379 / 84.785 | 3 | +0.00 | -0.11 |
| control | second_source_stable / private_bytes | 48.801 / 51.191 / 48.574 / 51.789 | 48.703 / 49.072 / 48.344 / 49.164 | 3 | -0.20 | -0.47 |
| control | second_source_stable / private_commit | 41.047 / 41.397 / 40.980 / 41.484 | 41.125 / 41.444 / 40.754 / 41.523 | 3 | +0.19 | -0.55 |
| control | second_source_stable / private_reserve | 76.562 / 76.581 / 76.023 / 76.586 | 77.102 / 77.270 / 76.602 / 77.312 | 3 | +0.70 | +0.67 |
| control | second_source_stable / virtual_size | 407.824 / 407.896 / 407.789 / 407.914 | 408.723 / 408.873 / 407.641 / 408.910 | 3 | +0.22 | +0.23 |
| control | second_source_stable / working_set | 84.242 / 84.436 / 84.215 / 84.484 | 84.242 / 84.473 / 84.125 / 84.531 | 3 | +0.00 | -0.11 |
| control | source_generated / peak_private_bytes | 47.105 / 49.818 / 47.039 / 50.496 | 47.238 / 47.248 / 47.020 / 47.250 | 3 | +0.28 | -0.18 |
| control | source_generated / peak_working_set | 81.855 / 81.949 / 81.801 / 81.973 | 81.852 / 81.992 / 81.727 / 82.027 | 3 | -0.00 | -0.09 |
| control | source_generated / private_bytes | 46.609 / 49.000 / 46.516 / 49.598 | 46.461 / 46.492 / 46.430 / 46.500 | 3 | -0.32 | -0.23 |
| control | source_generated / private_commit | 38.922 / 39.219 / 38.859 / 39.293 | 38.859 / 38.869 / 38.852 / 38.871 | 3 | -0.16 | -0.13 |
| control | source_generated / private_reserve | 78.496 / 78.618 / 78.215 / 78.648 | 78.586 / 78.680 / 78.484 / 78.703 | 3 | +0.11 | +0.07 |
| control | source_generated / virtual_size | 407.789 / 407.789 / 407.699 / 407.789 | 407.723 / 407.823 / 407.641 / 407.848 | 3 | -0.02 | -0.01 |
| control | source_generated / working_set | 81.574 / 81.740 / 81.445 / 81.781 | 81.512 / 81.527 / 81.488 / 81.531 | 3 | -0.08 | -0.08 |
| control | source_stable / peak_private_bytes | 47.105 / 49.818 / 47.039 / 50.496 | 47.238 / 47.248 / 47.020 / 47.250 | 3 | +0.28 | -0.18 |
| control | source_stable / peak_working_set | 81.973 / 81.985 / 81.801 / 81.988 | 81.852 / 81.992 / 81.746 / 82.027 | 3 | -0.15 | -0.07 |
| control | source_stable / private_bytes | 46.621 / 49.012 / 46.527 / 49.609 | 46.480 / 46.505 / 46.441 / 46.512 | 3 | -0.30 | -0.23 |
| control | source_stable / private_commit | 38.934 / 39.230 / 38.871 / 39.305 | 38.871 / 38.887 / 38.863 / 38.891 | 3 | -0.16 | -0.11 |
| control | source_stable / private_reserve | 78.484 / 78.606 / 78.203 / 78.637 | 78.574 / 78.668 / 78.465 / 78.691 | 3 | +0.11 | +0.07 |
| control | source_stable / virtual_size | 407.789 / 407.789 / 407.699 / 407.789 | 407.723 / 407.823 / 407.641 / 407.848 | 3 | -0.02 | -0.01 |
| control | source_stable / working_set | 81.785 / 81.948 / 81.648 / 81.988 | 81.715 / 81.740 / 81.699 / 81.746 | 3 | -0.09 | -0.09 |
| large | after_caret / peak_private_bytes | 678.559 / 678.935 / 678.230 / 679.172 | 638.488 / 640.574 / 638.062 / 641.020 | 7 | -5.91 | -5.89 |
| large | after_caret / peak_working_set | 680.207 / 680.445 / 679.719 / 680.473 | 680.199 / 680.607 / 679.828 / 680.961 | 7 | -0.00 | +0.02 |
| large | after_caret / private_bytes | 677.773 / 677.991 / 677.496 / 678.012 | 637.730 / 639.749 / 637.301 / 640.246 | 7 | -5.91 | -5.90 |
| large | after_caret / private_commit | 668.797 / 668.996 / 668.516 / 669.031 | 628.699 / 629.316 / 628.430 / 629.504 | 7 | -6.00 | -5.98 |
| large | after_caret / private_reserve | 177.750 / 178.188 / 176.625 / 178.258 | 176.719 / 177.838 / 176.133 / 178.035 | 7 | -0.58 | -0.55 |
| large | after_caret / virtual_size | 1145.016 / 1145.286 / 1144.066 / 1145.328 | 1104.012 / 1104.944 / 1103.832 / 1105.047 | 7 | -3.58 | -3.57 |
| large | after_caret / working_set | 679.625 / 679.938 / 679.336 / 679.945 | 679.719 / 680.102 / 679.348 / 680.418 | 7 | +0.01 | +0.00 |
| large | after_close / peak_private_bytes | 1120.715 / 1121.291 / 1120.328 / 1121.469 | 1046.773 / 1048.959 / 1046.406 / 1049.672 | 7 | -6.60 | -6.57 |
| large | after_close / peak_working_set | 1081.988 / 1082.698 / 1081.645 / 1082.996 | 1075.945 / 1076.216 / 1075.496 / 1076.570 | 7 | -0.56 | -0.57 |
| large | after_close / private_bytes | 382.074 / 382.866 / 381.887 / 383.129 | 382.359 / 383.905 / 381.738 / 384.219 | 7 | +0.07 | +0.08 |
| large | after_close / private_commit | 373.676 / 374.493 / 373.484 / 374.711 | 373.867 / 374.282 / 373.164 / 374.289 | 7 | +0.05 | +0.03 |
| large | after_close / private_reserve | 181.578 / 181.845 / 180.488 / 181.957 | 181.387 / 181.944 / 180.113 / 182.059 | 7 | -0.11 | -0.21 |
| large | after_close / virtual_size | 842.680 / 842.829 / 842.594 / 842.871 | 842.645 / 842.684 / 841.824 / 842.695 | 7 | -0.00 | -0.01 |
| large | after_close / working_set | 406.582 / 407.183 / 406.062 / 407.305 | 406.527 / 407.249 / 406.066 / 407.301 | 7 | -0.01 | +0.02 |
| large | after_find / peak_private_bytes | 678.559 / 678.935 / 678.230 / 679.172 | 638.488 / 640.574 / 638.062 / 641.020 | 7 | -5.91 | -5.89 |
| large | after_find / peak_working_set | 680.207 / 680.445 / 679.719 / 680.473 | 680.199 / 680.607 / 679.828 / 680.961 | 7 | -0.00 | +0.02 |
| large | after_find / private_bytes | 677.773 / 677.991 / 677.496 / 678.012 | 637.730 / 639.749 / 637.301 / 640.246 | 7 | -5.91 | -5.90 |
| large | after_find / private_commit | 668.797 / 668.996 / 668.516 / 669.031 | 628.699 / 629.316 / 628.430 / 629.504 | 7 | -6.00 | -5.98 |
| large | after_find / private_reserve | 177.750 / 178.188 / 176.625 / 178.258 | 176.719 / 177.838 / 176.133 / 178.035 | 7 | -0.58 | -0.55 |
| large | after_find / virtual_size | 1145.016 / 1145.286 / 1144.066 / 1145.328 | 1104.012 / 1104.944 / 1103.832 / 1105.047 | 7 | -3.58 | -3.57 |
| large | after_find / working_set | 679.641 / 679.959 / 679.352 / 679.973 | 679.734 / 680.117 / 679.363 / 680.434 | 7 | +0.01 | +0.00 |
| large | after_replace / peak_private_bytes | 678.559 / 678.935 / 678.230 / 679.172 | 638.488 / 640.574 / 638.062 / 641.020 | 7 | -5.91 | -5.89 |
| large | after_replace / peak_working_set | 680.207 / 680.445 / 679.719 / 680.473 | 680.199 / 680.607 / 679.828 / 680.961 | 7 | -0.00 | +0.02 |
| large | after_replace / private_bytes | 677.773 / 677.991 / 677.496 / 678.012 | 637.730 / 639.749 / 637.301 / 640.246 | 7 | -5.91 | -5.90 |
| large | after_replace / private_commit | 668.797 / 668.996 / 668.516 / 669.031 | 628.699 / 629.316 / 628.430 / 629.504 | 7 | -6.00 | -5.98 |
| large | after_replace / private_reserve | 177.750 / 178.188 / 176.625 / 178.258 | 176.719 / 177.838 / 176.133 / 178.035 | 7 | -0.58 | -0.55 |
| large | after_replace / virtual_size | 1145.016 / 1145.286 / 1144.066 / 1145.328 | 1104.012 / 1104.944 / 1103.832 / 1105.047 | 7 | -3.58 | -3.57 |
| large | after_replace / working_set | 679.648 / 679.966 / 679.359 / 679.980 | 679.738 / 680.121 / 679.367 / 680.438 | 7 | +0.01 | +0.00 |
| large | after_scroll / peak_private_bytes | 678.559 / 678.935 / 678.230 / 679.172 | 638.488 / 640.574 / 638.062 / 641.020 | 7 | -5.91 | -5.89 |
| large | after_scroll / peak_working_set | 680.207 / 680.445 / 679.719 / 680.473 | 680.199 / 680.607 / 679.828 / 680.961 | 7 | -0.00 | +0.02 |
| large | after_scroll / private_bytes | 677.773 / 677.991 / 677.496 / 678.012 | 637.730 / 639.749 / 637.301 / 640.246 | 7 | -5.91 | -5.90 |
| large | after_scroll / private_commit | 668.797 / 668.996 / 668.516 / 669.031 | 628.699 / 629.316 / 628.430 / 629.504 | 7 | -6.00 | -5.98 |
| large | after_scroll / private_reserve | 177.750 / 178.188 / 176.625 / 178.258 | 176.719 / 177.838 / 176.133 / 178.035 | 7 | -0.58 | -0.55 |
| large | after_scroll / virtual_size | 1145.016 / 1145.286 / 1144.066 / 1145.328 | 1104.012 / 1104.944 / 1103.832 / 1105.047 | 7 | -3.58 | -3.57 |
| large | after_scroll / working_set | 679.625 / 679.938 / 679.336 / 679.945 | 679.719 / 680.102 / 679.348 / 680.418 | 7 | +0.01 | +0.00 |
| large | after_small_edit / peak_private_bytes | 678.559 / 678.935 / 678.230 / 679.172 | 638.488 / 640.574 / 638.062 / 641.020 | 7 | -5.91 | -5.89 |
| large | after_small_edit / peak_working_set | 680.207 / 680.445 / 679.719 / 680.473 | 680.199 / 680.607 / 679.828 / 680.961 | 7 | -0.00 | +0.02 |
| large | after_small_edit / private_bytes | 677.773 / 677.991 / 677.496 / 678.012 | 637.730 / 639.749 / 637.301 / 640.246 | 7 | -5.91 | -5.90 |
| large | after_small_edit / private_commit | 668.797 / 668.996 / 668.516 / 669.031 | 628.699 / 629.316 / 628.430 / 629.504 | 7 | -6.00 | -5.98 |
| large | after_small_edit / private_reserve | 177.750 / 178.188 / 176.625 / 178.258 | 176.719 / 177.838 / 176.133 / 178.035 | 7 | -0.58 | -0.55 |
| large | after_small_edit / virtual_size | 1145.016 / 1145.286 / 1144.066 / 1145.328 | 1104.012 / 1104.944 / 1103.832 / 1105.047 | 7 | -3.58 | -3.57 |
| large | after_small_edit / working_set | 679.656 / 679.974 / 679.367 / 679.988 | 679.746 / 680.129 / 679.375 / 680.445 | 7 | +0.01 | +0.00 |
| large | after_verification_save / peak_private_bytes | 1120.715 / 1121.291 / 1120.328 / 1121.469 | 1046.773 / 1048.959 / 1046.406 / 1049.672 | 7 | -6.60 | -6.57 |
| large | after_verification_save / peak_working_set | 1081.988 / 1082.698 / 1081.645 / 1082.996 | 1075.945 / 1076.216 / 1075.496 / 1076.570 | 7 | -0.56 | -0.57 |
| large | after_verification_save / private_bytes | 810.176 / 810.459 / 809.328 / 810.656 | 770.031 / 772.210 / 768.887 / 772.754 | 7 | -4.96 | -4.99 |
| large | after_verification_save / private_commit | 800.922 / 801.212 / 800.078 / 801.402 | 760.867 / 761.334 / 759.688 / 761.672 | 7 | -5.00 | -5.03 |
| large | after_verification_save / private_reserve | 175.953 / 176.783 / 175.031 / 176.891 | 175.062 / 176.426 / 174.324 / 176.730 | 7 | -0.51 | -0.26 |
| large | after_verification_save / virtual_size | 1275.312 / 1275.403 / 1274.531 / 1275.492 | 1234.383 / 1235.241 / 1234.074 / 1235.320 | 7 | -3.21 | -3.21 |
| large | after_verification_save / working_set | 802.227 / 802.508 / 801.359 / 802.660 | 802.227 / 802.695 / 801.117 / 803.000 | 7 | +0.00 | -0.05 |
| large | before_close / peak_private_bytes | 1120.715 / 1121.291 / 1120.328 / 1121.469 | 1046.773 / 1048.959 / 1046.406 / 1049.672 | 7 | -6.60 | -6.57 |
| large | before_close / peak_working_set | 1081.988 / 1082.698 / 1081.645 / 1082.996 | 1075.945 / 1076.216 / 1075.496 / 1076.570 | 7 | -0.56 | -0.57 |
| large | before_close / private_bytes | 699.434 / 699.753 / 698.484 / 699.816 | 659.637 / 661.401 / 658.406 / 662.266 | 7 | -5.69 | -5.74 |
| large | before_close / private_commit | 690.379 / 690.702 / 689.445 / 690.742 | 650.641 / 650.798 / 649.418 / 650.848 | 7 | -5.76 | -5.81 |
| large | before_close / private_reserve | 175.520 / 176.375 / 174.270 / 176.434 | 175.039 / 175.586 / 174.117 / 175.621 | 7 | -0.27 | -0.41 |
| large | before_close / virtual_size | 1164.422 / 1164.545 / 1163.480 / 1164.629 | 1123.512 / 1124.333 / 1123.355 / 1124.375 | 7 | -3.51 | -3.51 |
| large | before_close / working_set | 691.754 / 692.074 / 690.578 / 692.133 | 691.988 / 692.203 / 690.902 / 692.215 | 7 | +0.03 | -0.02 |
| large | body_after_source / peak_private_bytes | 1087.137 / 1087.312 / 1086.621 / 1087.434 | 1046.773 / 1048.959 / 1046.406 / 1049.672 | 7 | -3.71 | -3.67 |
| large | body_after_source / peak_working_set | 1075.949 / 1076.182 / 1075.512 / 1076.285 | 1075.945 / 1076.216 / 1075.496 / 1076.570 | 7 | -0.00 | -0.00 |
| large | body_after_source / private_bytes | 760.086 / 760.382 / 759.586 / 760.586 | 559.484 / 561.330 / 558.812 / 561.777 | 7 | -26.39 | -26.35 |
| large | body_after_source / private_commit | 750.910 / 751.277 / 750.453 / 751.445 | 550.695 / 551.149 / 550.047 / 551.273 | 7 | -26.66 | -26.65 |
| large | body_after_source / private_reserve | 176.188 / 176.572 / 175.383 / 176.727 | 174.941 / 175.603 / 174.113 / 175.746 | 7 | -0.71 | -0.72 |
| large | body_after_source / virtual_size | 1225.391 / 1225.770 / 1224.801 / 1225.805 | 1023.859 / 1024.627 / 1023.691 / 1024.676 | 7 | -16.45 | -16.43 |
| large | body_after_source / working_set | 761.914 / 762.209 / 761.551 / 762.363 | 601.688 / 601.926 / 601.074 / 601.938 | 7 | -21.03 | -21.02 |
| large | body_idle / peak_private_bytes | 1087.137 / 1087.312 / 1086.621 / 1087.434 | 1046.773 / 1048.959 / 1046.406 / 1049.672 | 7 | -3.71 | -3.67 |
| large | body_idle / peak_working_set | 1075.949 / 1076.182 / 1075.512 / 1076.285 | 1075.945 / 1076.216 / 1075.496 / 1076.570 | 7 | -0.00 | -0.00 |
| large | body_idle / private_bytes | 759.484 / 759.873 / 758.957 / 759.953 | 558.723 / 560.920 / 557.984 / 560.980 | 7 | -26.43 | -26.42 |
| large | body_idle / private_commit | 750.402 / 750.742 / 749.809 / 750.836 | 549.934 / 550.624 / 549.238 / 551.121 | 7 | -26.71 | -26.69 |
| large | body_idle / private_reserve | 176.430 / 177.070 / 175.562 / 177.363 | 175.473 / 176.254 / 174.266 / 176.418 | 7 | -0.54 | -0.44 |
| large | body_idle / virtual_size | 1225.301 / 1225.645 / 1224.676 / 1225.680 | 1023.801 / 1024.502 / 1023.566 / 1024.551 | 7 | -16.44 | -16.43 |
| large | body_idle / working_set | 761.270 / 761.834 / 760.828 / 762.113 | 600.918 / 601.383 / 600.559 / 601.500 | 7 | -21.06 | -21.06 |
| large | first_paint_complete / peak_private_bytes | 676.938 / 677.508 / 676.387 / 677.531 | 556.824 / 558.334 / 556.129 / 559.109 | 7 | -17.74 | -17.77 |
| large | first_paint_complete / peak_working_set | 638.254 / 638.840 / 637.871 / 638.957 | 558.121 / 558.398 / 557.797 / 558.543 | 7 | -12.56 | -12.59 |
| large | first_paint_complete / private_bytes | 436.266 / 436.736 / 435.777 / 436.961 | 396.527 / 398.036 / 395.852 / 398.840 | 7 | -9.11 | -9.10 |
| large | first_paint_complete / private_commit | 427.770 / 428.212 / 427.273 / 428.449 | 387.906 / 388.146 / 387.504 / 388.219 | 7 | -9.32 | -9.33 |
| large | first_paint_complete / private_reserve | 177.570 / 177.910 / 176.391 / 177.934 | 176.367 / 177.622 / 176.129 / 177.898 | 7 | -0.68 | -0.28 |
| large | first_paint_complete / virtual_size | 903.738 / 903.903 / 902.914 / 904.039 | 862.820 / 863.755 / 862.652 / 863.875 | 7 | -4.53 | -4.51 |
| large | first_paint_complete / working_set | 438.395 / 438.674 / 437.828 / 438.906 | 438.434 / 438.679 / 438.113 / 438.836 | 7 | +0.01 | +0.01 |
| large | initial_layout_complete / peak_private_bytes | 678.559 / 678.935 / 678.230 / 679.172 | 638.488 / 640.574 / 638.062 / 641.020 | 7 | -5.91 | -5.89 |
| large | initial_layout_complete / peak_working_set | 680.207 / 680.445 / 679.719 / 680.473 | 680.199 / 680.607 / 679.828 / 680.961 | 7 | -0.00 | +0.02 |
| large | initial_layout_complete / private_bytes | 677.773 / 677.991 / 677.496 / 678.012 | 637.730 / 639.749 / 637.301 / 640.246 | 7 | -5.91 | -5.90 |
| large | initial_layout_complete / private_commit | 668.797 / 668.996 / 668.516 / 669.031 | 628.699 / 629.316 / 628.430 / 629.504 | 7 | -6.00 | -5.98 |
| large | initial_layout_complete / private_reserve | 177.750 / 178.188 / 176.625 / 178.258 | 176.719 / 177.838 / 176.133 / 178.035 | 7 | -0.58 | -0.55 |
| large | initial_layout_complete / virtual_size | 1145.016 / 1145.286 / 1144.066 / 1145.328 | 1104.012 / 1104.944 / 1103.832 / 1105.047 | 7 | -3.58 | -3.57 |
| large | initial_layout_complete / working_set | 679.617 / 679.930 / 679.328 / 679.938 | 679.711 / 680.094 / 679.340 / 680.410 | 7 | +0.01 | +0.00 |
| large | open_body / peak_private_bytes | 205.617 / 205.788 / 205.156 / 205.828 | 205.727 / 206.840 / 190.344 / 207.895 | 7 | +0.05 | -0.02 |
| large | open_body / peak_working_set | 236.477 / 236.971 / 236.137 / 237.004 | 236.320 / 236.877 / 221.309 / 237.008 | 7 | -0.07 | -0.09 |
| large | open_body / private_bytes | 84.238 / 84.836 / 83.629 / 84.848 | 84.395 / 86.018 / 83.961 / 86.625 | 7 | +0.19 | -0.05 |
| large | open_body / private_commit | 76.453 / 77.051 / 75.918 / 77.062 | 76.582 / 76.872 / 76.199 / 76.961 | 7 | +0.17 | -0.27 |
| large | open_body / private_reserve | 90.289 / 90.728 / 88.961 / 90.871 | 89.285 / 90.672 / 89.000 / 90.695 | 7 | -1.11 | -0.57 |
| large | open_body / virtual_size | 465.258 / 465.306 / 464.445 / 465.332 | 464.434 / 465.370 / 464.219 / 465.375 | 7 | -0.18 | -0.18 |
| large | open_body / working_set | 126.895 / 127.370 / 126.281 / 127.527 | 126.824 / 127.145 / 126.598 / 127.316 | 7 | -0.06 | -0.16 |
| large | process_initialized / peak_private_bytes | 45.023 / 45.216 / 44.316 / 45.246 | 44.766 / 46.000 / 44.414 / 47.066 | 7 | -0.57 | +0.59 |
| large | process_initialized / peak_working_set | 78.844 / 79.279 / 78.398 / 79.328 | 78.617 / 78.820 / 78.523 / 79.078 | 7 | -0.29 | -0.22 |
| large | process_initialized / private_bytes | 44.828 / 45.028 / 44.125 / 45.059 | 44.582 / 45.811 / 44.219 / 46.875 | 7 | -0.55 | +0.61 |
| large | process_initialized / private_commit | 37.184 / 37.404 / 36.488 / 37.430 | 36.789 / 37.161 / 36.453 / 37.379 | 7 | -1.06 | -1.06 |
| large | process_initialized / private_reserve | 76.785 / 77.030 / 76.535 / 77.203 | 76.656 / 77.298 / 76.195 / 77.570 | 7 | -0.17 | -0.19 |
| large | process_initialized / virtual_size | 404.379 / 404.435 / 403.395 / 404.449 | 403.449 / 404.452 / 403.336 / 404.520 | 7 | -0.23 | -0.01 |
| large | process_initialized / working_set | 78.730 / 79.175 / 78.285 / 79.227 | 78.512 / 78.714 / 78.418 / 78.977 | 7 | -0.28 | -0.23 |
| large | return_body_idle / peak_private_bytes | 1120.715 / 1121.291 / 1120.328 / 1121.469 | 1046.773 / 1048.959 / 1046.406 / 1049.672 | 7 | -6.60 | -6.57 |
| large | return_body_idle / peak_working_set | 1081.988 / 1082.698 / 1081.645 / 1082.996 | 1075.945 / 1076.216 / 1075.496 / 1076.570 | 7 | -0.56 | -0.57 |
| large | return_body_idle / private_bytes | 699.434 / 699.753 / 698.484 / 699.816 | 659.637 / 661.401 / 658.406 / 662.266 | 7 | -5.69 | -5.74 |
| large | return_body_idle / private_commit | 690.379 / 690.702 / 689.445 / 690.742 | 650.641 / 650.798 / 649.418 / 650.848 | 7 | -5.76 | -5.81 |
| large | return_body_idle / private_reserve | 175.520 / 176.375 / 174.270 / 176.434 | 175.039 / 175.586 / 174.117 / 175.621 | 7 | -0.27 | -0.41 |
| large | return_body_idle / virtual_size | 1164.422 / 1164.545 / 1163.480 / 1164.629 | 1123.512 / 1124.333 / 1123.355 / 1124.375 | 7 | -3.51 | -3.51 |
| large | return_body_idle / working_set | 691.754 / 692.074 / 690.578 / 692.133 | 691.988 / 692.203 / 690.902 / 692.215 | 7 | +0.03 | -0.02 |
| large | second_source_generated / peak_private_bytes | 1120.715 / 1121.291 / 1120.328 / 1121.469 | 1046.773 / 1048.959 / 1046.406 / 1049.672 | 7 | -6.60 | -6.57 |
| large | second_source_generated / peak_working_set | 1081.988 / 1082.698 / 1081.645 / 1082.996 | 1075.945 / 1076.216 / 1075.496 / 1076.570 | 7 | -0.56 | -0.57 |
| large | second_source_generated / private_bytes | 728.695 / 729.245 / 728.387 / 729.414 | 688.605 / 690.798 / 687.926 / 691.527 | 7 | -5.50 | -5.53 |
| large | second_source_generated / private_commit | 719.621 / 720.138 / 719.293 / 720.328 | 679.656 / 680.041 / 678.922 / 680.297 | 7 | -5.55 | -5.60 |
| large | second_source_generated / private_reserve | 176.719 / 177.016 / 175.379 / 177.133 | 176.242 / 176.598 / 175.184 / 176.715 | 7 | -0.27 | -0.26 |
| large | second_source_generated / virtual_size | 1194.789 / 1195.009 / 1193.859 / 1195.117 | 1153.953 / 1154.763 / 1153.773 / 1154.863 | 7 | -3.42 | -3.42 |
| large | second_source_generated / working_set | 720.867 / 721.485 / 720.508 / 721.738 | 720.840 / 721.323 / 720.395 / 721.492 | 7 | -0.00 | -0.02 |
| large | second_source_stable / peak_private_bytes | 1120.715 / 1121.291 / 1120.328 / 1121.469 | 1046.773 / 1048.959 / 1046.406 / 1049.672 | 7 | -6.60 | -6.57 |
| large | second_source_stable / peak_working_set | 1081.988 / 1082.698 / 1081.645 / 1082.996 | 1075.945 / 1076.216 / 1075.496 / 1076.570 | 7 | -0.56 | -0.57 |
| large | second_source_stable / private_bytes | 729.152 / 729.398 / 728.410 / 729.457 | 689.246 / 691.106 / 688.078 / 691.992 | 7 | -5.47 | -5.51 |
| large | second_source_stable / private_commit | 720.059 / 720.298 / 719.379 / 720.336 | 680.223 / 680.416 / 679.043 / 680.500 | 7 | -5.53 | -5.57 |
| large | second_source_stable / private_reserve | 176.195 / 177.007 / 175.055 / 177.105 | 175.828 / 176.396 / 174.941 / 176.457 | 7 | -0.21 | -0.37 |
| large | second_source_stable / virtual_size | 1194.789 / 1195.009 / 1193.859 / 1195.117 | 1153.953 / 1154.763 / 1153.773 / 1154.863 | 7 | -3.42 | -3.42 |
| large | second_source_stable / working_set | 721.375 / 721.652 / 720.598 / 721.711 | 721.531 / 721.823 / 720.512 / 721.879 | 7 | +0.02 | -0.02 |
| large | source_generated / peak_private_bytes | 676.938 / 677.508 / 676.387 / 677.531 | 556.824 / 558.334 / 556.129 / 559.109 | 7 | -17.74 | -17.77 |
| large | source_generated / peak_working_set | 638.254 / 638.840 / 637.871 / 638.957 | 558.121 / 558.398 / 557.797 / 558.543 | 7 | -12.56 | -12.59 |
| large | source_generated / private_bytes | 436.266 / 436.736 / 435.777 / 436.961 | 396.527 / 398.036 / 395.852 / 398.840 | 7 | -9.11 | -9.10 |
| large | source_generated / private_commit | 427.770 / 428.212 / 427.273 / 428.449 | 387.906 / 388.146 / 387.504 / 388.219 | 7 | -9.32 | -9.33 |
| large | source_generated / private_reserve | 177.570 / 177.910 / 176.391 / 177.934 | 176.367 / 177.622 / 176.129 / 177.898 | 7 | -0.68 | -0.28 |
| large | source_generated / virtual_size | 903.738 / 903.903 / 902.914 / 904.039 | 862.820 / 863.755 / 862.652 / 863.875 | 7 | -4.53 | -4.51 |
| large | source_generated / working_set | 438.395 / 438.674 / 437.828 / 438.906 | 438.434 / 438.679 / 438.113 / 438.836 | 7 | +0.01 | +0.01 |
| large | source_stable / peak_private_bytes | 678.559 / 678.935 / 678.230 / 679.172 | 638.488 / 640.574 / 638.062 / 641.020 | 7 | -5.91 | -5.89 |
| large | source_stable / peak_working_set | 680.207 / 680.445 / 679.719 / 680.473 | 680.199 / 680.607 / 679.828 / 680.961 | 7 | -0.00 | +0.02 |
| large | source_stable / private_bytes | 677.773 / 677.991 / 677.496 / 678.012 | 637.730 / 639.749 / 637.301 / 640.246 | 7 | -5.91 | -5.90 |
| large | source_stable / private_commit | 668.797 / 668.996 / 668.516 / 669.031 | 628.699 / 629.316 / 628.430 / 629.504 | 7 | -6.00 | -5.98 |
| large | source_stable / private_reserve | 177.750 / 178.188 / 176.625 / 178.258 | 176.719 / 177.838 / 176.133 / 178.035 | 7 | -0.58 | -0.55 |
| large | source_stable / virtual_size | 1145.016 / 1145.286 / 1144.066 / 1145.328 | 1104.012 / 1104.944 / 1103.832 / 1105.047 | 7 | -3.58 | -3.57 |
| large | source_stable / working_set | 679.617 / 679.930 / 679.328 / 679.938 | 679.711 / 680.094 / 679.340 / 680.410 | 7 | +0.01 | +0.00 |
| many | after_caret / peak_private_bytes | 710.457 / 716.345 / 709.492 / 724.859 | 667.812 / 668.830 / 667.301 / 669.992 | 7 | -6.00 | -5.97 |
| many | after_caret / peak_working_set | 700.852 / 701.096 / 700.457 / 701.270 | 700.844 / 701.204 / 700.645 / 701.258 | 7 | -0.00 | +0.02 |
| many | after_caret / private_bytes | 709.363 / 715.420 / 708.734 / 724.035 | 667.812 / 668.830 / 667.301 / 669.992 | 7 | -5.86 | -5.86 |
| many | after_caret / private_commit | 700.238 / 700.583 / 699.707 / 700.730 | 658.758 / 659.087 / 658.391 / 659.137 | 7 | -5.92 | -5.94 |
| many | after_caret / private_reserve | 177.969 / 178.272 / 177.492 / 178.480 | 177.660 / 178.125 / 177.258 / 178.242 | 7 | -0.17 | -0.05 |
| many | after_caret / virtual_size | 1168.402 / 1168.545 / 1168.223 / 1168.605 | 1126.742 / 1126.880 / 1126.680 / 1126.953 | 7 | -3.57 | -3.56 |
| many | after_caret / working_set | 700.852 / 701.096 / 700.457 / 701.270 | 700.844 / 701.204 / 700.645 / 701.258 | 7 | -0.00 | +0.02 |
| many | after_close / peak_private_bytes | 1173.410 / 1179.853 / 1172.324 / 1188.504 | 1047.484 / 1048.645 / 1047.086 / 1049.828 | 7 | -10.73 | -10.71 |
| many | after_close / peak_working_set | 1123.129 / 1123.652 / 1122.441 / 1123.816 | 1078.289 / 1078.445 / 1077.957 / 1078.496 | 7 | -3.99 | -4.01 |
| many | after_close / private_bytes | 415.969 / 423.485 / 406.145 / 433.254 | 417.895 / 419.086 / 414.145 / 419.777 | 7 | +0.46 | +0.46 |
| many | after_close / private_commit | 407.328 / 408.938 / 397.414 / 409.793 | 409.105 / 410.321 / 405.504 / 411.102 | 7 | +0.44 | +0.47 |
| many | after_close / private_reserve | 405.227 / 413.783 / 392.094 / 416.586 | 362.035 / 364.227 / 360.070 / 364.602 | 7 | -10.66 | -11.07 |
| many | after_close / virtual_size | 1089.871 / 1105.756 / 1088.762 / 1105.824 | 1058.223 / 1058.388 / 1057.340 / 1058.406 | 7 | -2.90 | -2.98 |
| many | after_close / working_set | 432.410 / 432.966 / 428.465 / 433.027 | 432.621 / 433.624 / 430.457 / 433.723 | 7 | +0.05 | +0.30 |
| many | after_find / peak_private_bytes | 710.457 / 716.345 / 709.492 / 724.859 | 667.871 / 668.848 / 667.348 / 670.039 | 7 | -5.99 | -5.97 |
| many | after_find / peak_working_set | 700.996 / 701.157 / 700.551 / 701.305 | 700.957 / 701.273 / 700.746 / 701.309 | 7 | -0.01 | +0.02 |
| many | after_find / private_bytes | 709.363 / 715.471 / 708.781 / 724.094 | 667.871 / 668.848 / 667.348 / 670.039 | 7 | -5.85 | -5.86 |
| many | after_find / private_commit | 700.297 / 700.602 / 699.746 / 700.777 | 658.809 / 659.087 / 658.438 / 659.137 | 7 | -5.92 | -5.93 |
| many | after_find / private_reserve | 177.922 / 178.228 / 177.445 / 178.441 | 177.609 / 178.078 / 177.258 / 178.195 | 7 | -0.18 | -0.05 |
| many | after_find / virtual_size | 1168.402 / 1168.545 / 1168.223 / 1168.605 | 1126.742 / 1126.880 / 1126.680 / 1126.953 | 7 | -3.57 | -3.56 |
| many | after_find / working_set | 700.996 / 701.157 / 700.551 / 701.305 | 700.957 / 701.273 / 700.746 / 701.309 | 7 | -0.01 | +0.02 |
| many | after_replace / peak_private_bytes | 710.457 / 716.345 / 709.492 / 724.859 | 667.871 / 668.848 / 667.348 / 670.039 | 7 | -5.99 | -5.97 |
| many | after_replace / peak_working_set | 701.004 / 701.165 / 700.559 / 701.312 | 700.961 / 701.277 / 700.750 / 701.312 | 7 | -0.01 | +0.02 |
| many | after_replace / private_bytes | 709.363 / 715.471 / 708.781 / 724.094 | 667.871 / 668.848 / 667.348 / 670.039 | 7 | -5.85 | -5.86 |
| many | after_replace / private_commit | 700.297 / 700.602 / 699.746 / 700.777 | 658.809 / 659.087 / 658.438 / 659.137 | 7 | -5.92 | -5.93 |
| many | after_replace / private_reserve | 177.922 / 178.228 / 177.445 / 178.441 | 177.609 / 178.078 / 177.258 / 178.195 | 7 | -0.18 | -0.05 |
| many | after_replace / virtual_size | 1168.402 / 1168.545 / 1168.223 / 1168.605 | 1126.742 / 1126.880 / 1126.680 / 1126.953 | 7 | -3.57 | -3.56 |
| many | after_replace / working_set | 701.004 / 701.165 / 700.559 / 701.312 | 700.961 / 701.277 / 700.750 / 701.312 | 7 | -0.01 | +0.02 |
| many | after_scroll / peak_private_bytes | 710.457 / 716.345 / 709.492 / 724.859 | 667.812 / 668.830 / 667.301 / 669.992 | 7 | -6.00 | -5.97 |
| many | after_scroll / peak_working_set | 700.852 / 701.096 / 700.457 / 701.270 | 700.844 / 701.204 / 700.645 / 701.258 | 7 | -0.00 | +0.02 |
| many | after_scroll / private_bytes | 709.363 / 715.420 / 708.734 / 724.035 | 667.812 / 668.830 / 667.301 / 669.992 | 7 | -5.86 | -5.86 |
| many | after_scroll / private_commit | 700.238 / 700.583 / 699.707 / 700.730 | 658.758 / 659.087 / 658.391 / 659.137 | 7 | -5.92 | -5.94 |
| many | after_scroll / private_reserve | 177.969 / 178.272 / 177.492 / 178.480 | 177.660 / 178.125 / 177.258 / 178.242 | 7 | -0.17 | -0.05 |
| many | after_scroll / virtual_size | 1168.402 / 1168.545 / 1168.223 / 1168.605 | 1126.742 / 1126.880 / 1126.680 / 1126.953 | 7 | -3.57 | -3.56 |
| many | after_scroll / working_set | 700.852 / 701.096 / 700.457 / 701.270 | 700.844 / 701.204 / 700.645 / 701.258 | 7 | -0.00 | +0.02 |
| many | after_small_edit / peak_private_bytes | 710.457 / 716.345 / 709.492 / 724.859 | 667.871 / 668.848 / 667.348 / 670.039 | 7 | -5.99 | -5.97 |
| many | after_small_edit / peak_working_set | 701.012 / 701.173 / 700.566 / 701.320 | 700.969 / 701.285 / 700.758 / 701.320 | 7 | -0.01 | +0.02 |
| many | after_small_edit / private_bytes | 709.363 / 715.471 / 708.781 / 724.094 | 667.871 / 668.848 / 667.348 / 670.039 | 7 | -5.85 | -5.86 |
| many | after_small_edit / private_commit | 700.297 / 700.602 / 699.746 / 700.777 | 658.809 / 659.087 / 658.438 / 659.137 | 7 | -5.92 | -5.93 |
| many | after_small_edit / private_reserve | 177.922 / 178.228 / 177.445 / 178.441 | 177.609 / 178.078 / 177.258 / 178.195 | 7 | -0.18 | -0.05 |
| many | after_small_edit / virtual_size | 1168.402 / 1168.545 / 1168.223 / 1168.605 | 1126.742 / 1126.880 / 1126.680 / 1126.953 | 7 | -3.57 | -3.56 |
| many | after_small_edit / working_set | 701.012 / 701.173 / 700.566 / 701.320 | 700.969 / 701.285 / 700.758 / 701.320 | 7 | -0.01 | +0.02 |
| many | after_verification_save / peak_private_bytes | 1173.410 / 1179.853 / 1172.324 / 1188.504 | 1047.484 / 1048.645 / 1047.086 / 1049.828 | 7 | -10.73 | -10.71 |
| many | after_verification_save / peak_working_set | 1123.129 / 1123.652 / 1122.441 / 1123.816 | 1078.289 / 1078.445 / 1077.957 / 1078.496 | 7 | -3.99 | -4.01 |
| many | after_verification_save / private_bytes | 850.023 / 856.627 / 849.402 / 865.930 | 808.527 / 809.202 / 807.621 / 809.891 | 7 | -4.88 | -4.87 |
| many | after_verification_save / private_commit | 840.777 / 841.406 / 840.172 / 841.828 | 798.902 / 799.529 / 798.047 / 799.613 | 7 | -4.98 | -4.92 |
| many | after_verification_save / private_reserve | 170.254 / 186.341 / 168.266 / 186.676 | 138.410 / 138.965 / 137.133 / 139.223 | 7 | -18.70 | -19.45 |
| many | after_verification_save / virtual_size | 1301.359 / 1317.111 / 1300.082 / 1317.207 | 1227.969 / 1228.004 / 1226.965 / 1228.027 | 7 | -5.64 | -5.71 |
| many | after_verification_save / working_set | 831.320 / 831.691 / 830.223 / 831.719 | 830.883 / 830.917 / 830.230 / 830.934 | 7 | -0.05 | -0.05 |
| many | before_close / peak_private_bytes | 1173.410 / 1179.853 / 1172.324 / 1188.504 | 1047.484 / 1048.645 / 1047.086 / 1049.828 | 7 | -10.73 | -10.71 |
| many | before_close / peak_working_set | 1123.129 / 1123.652 / 1122.441 / 1123.816 | 1078.289 / 1078.445 / 1077.957 / 1078.496 | 7 | -3.99 | -4.01 |
| many | before_close / private_bytes | 746.227 / 752.401 / 744.934 / 761.117 | 703.336 / 704.438 / 702.586 / 705.246 | 7 | -5.75 | -5.69 |
| many | before_close / private_commit | 736.930 / 737.265 / 735.797 / 737.375 | 693.715 / 694.592 / 693.523 / 694.836 | 7 | -5.86 | -5.80 |
| many | before_close / private_reserve | 274.152 / 290.192 / 272.609 / 290.527 | 195.848 / 196.631 / 194.656 / 196.781 | 7 | -28.56 | -28.63 |
| many | before_close / virtual_size | 1301.359 / 1317.111 / 1300.082 / 1317.207 | 1180.531 / 1180.566 / 1179.594 / 1180.590 | 7 | -9.28 | -9.36 |
| many | before_close / working_set | 727.152 / 727.559 / 725.902 / 727.758 | 726.566 / 726.933 / 725.938 / 727.195 | 7 | -0.08 | -0.02 |
| many | body_after_source / peak_private_bytes | 1088.957 / 1095.645 / 1088.430 / 1103.484 | 1047.484 / 1048.645 / 1047.086 / 1049.828 | 7 | -3.81 | -3.80 |
| many | body_after_source / peak_working_set | 1078.180 / 1078.447 / 1077.785 / 1078.578 | 1078.289 / 1078.445 / 1077.957 / 1078.496 | 7 | +0.01 | +0.02 |
| many | body_after_source / private_bytes | 801.180 / 801.726 / 795.555 / 802.410 | 589.230 / 590.469 / 588.398 / 590.680 | 7 | -26.45 | -26.47 |
| many | body_after_source / private_commit | 792.051 / 792.589 / 771.609 / 793.309 | 579.965 / 581.314 / 579.234 / 581.527 | 7 | -26.78 | -26.78 |
| many | body_after_source / private_reserve | 88.012 / 96.163 / 86.852 / 107.652 | 174.254 / 174.865 / 173.387 / 174.996 | 7 | +97.99 | +98.58 |
| many | body_after_source / virtual_size | 1170.250 / 1170.420 / 1169.277 / 1170.441 | 1045.172 / 1045.250 / 1044.148 / 1045.262 | 7 | -10.69 | -10.70 |
| many | body_after_source / working_set | 791.117 / 791.373 / 770.121 / 791.375 | 622.508 / 623.617 / 621.844 / 623.898 | 7 | -21.31 | -21.31 |
| many | body_idle / peak_private_bytes | 1088.957 / 1095.645 / 1088.430 / 1103.484 | 1047.484 / 1048.645 / 1047.086 / 1049.828 | 7 | -3.81 | -3.80 |
| many | body_idle / peak_working_set | 1078.180 / 1078.447 / 1077.785 / 1078.578 | 1078.289 / 1078.445 / 1077.957 / 1078.496 | 7 | +0.01 | +0.02 |
| many | body_idle / private_bytes | 801.621 / 802.548 / 794.969 / 803.289 | 590.230 / 591.470 / 589.273 / 591.684 | 7 | -26.37 | -26.44 |
| many | body_idle / private_commit | 792.469 / 793.462 / 771.023 / 794.184 | 580.961 / 582.241 / 580.234 / 582.527 | 7 | -26.69 | -26.71 |
| many | body_idle / private_reserve | 87.211 / 96.453 / 85.852 / 108.113 | 173.254 / 173.740 / 172.262 / 173.871 | 7 | +98.66 | +98.20 |
| many | body_idle / virtual_size | 1170.125 / 1170.295 / 1169.152 / 1170.316 | 1045.047 / 1045.125 / 1044.023 / 1045.137 | 7 | -10.69 | -10.70 |
| many | body_idle / working_set | 791.914 / 792.141 / 769.523 / 792.230 | 623.539 / 624.580 / 622.836 / 624.934 | 7 | -21.26 | -21.22 |
| many | first_paint_complete / peak_private_bytes | 710.457 / 716.345 / 709.492 / 724.859 | 585.023 / 586.575 / 584.477 / 587.695 | 7 | -17.66 | -17.62 |
| many | first_paint_complete / peak_working_set | 660.062 / 660.600 / 659.648 / 660.691 | 576.984 / 577.436 / 576.484 / 577.539 | 7 | -12.59 | -12.55 |
| many | first_paint_complete / private_bytes | 460.113 / 466.111 / 459.531 / 474.762 | 418.500 / 419.598 / 417.980 / 420.648 | 7 | -9.04 | -9.05 |
| many | first_paint_complete / private_commit | 451.453 / 451.781 / 450.949 / 451.887 | 410.039 / 410.339 / 409.566 / 410.473 | 7 | -9.17 | -9.20 |
| many | first_paint_complete / private_reserve | 174.414 / 174.643 / 173.961 / 174.863 | 174.035 / 174.585 / 173.547 / 174.719 | 7 | -0.22 | -0.03 |
| many | first_paint_complete / virtual_size | 916.027 / 916.170 / 915.848 / 916.230 | 874.367 / 874.505 / 874.305 / 874.578 | 7 | -4.55 | -4.54 |
| many | first_paint_complete / working_set | 452.211 / 452.519 / 451.945 / 452.617 | 452.254 / 452.656 / 452.121 / 452.715 | 7 | +0.01 | +0.03 |
| many | initial_layout_complete / peak_private_bytes | 710.457 / 716.345 / 709.492 / 724.859 | 667.812 / 668.830 / 667.301 / 669.992 | 7 | -6.00 | -5.97 |
| many | initial_layout_complete / peak_working_set | 700.844 / 701.088 / 700.449 / 701.262 | 700.832 / 701.195 / 700.637 / 701.246 | 7 | -0.00 | +0.02 |
| many | initial_layout_complete / private_bytes | 709.363 / 715.420 / 708.734 / 724.035 | 667.812 / 668.830 / 667.301 / 669.992 | 7 | -5.86 | -5.86 |
| many | initial_layout_complete / private_commit | 700.238 / 700.583 / 699.707 / 700.730 | 658.758 / 659.087 / 658.391 / 659.137 | 7 | -5.92 | -5.94 |
| many | initial_layout_complete / private_reserve | 177.969 / 178.272 / 177.492 / 178.480 | 177.660 / 178.125 / 177.258 / 178.242 | 7 | -0.17 | -0.05 |
| many | initial_layout_complete / virtual_size | 1168.402 / 1168.545 / 1168.223 / 1168.605 | 1126.742 / 1126.880 / 1126.680 / 1126.953 | 7 | -3.57 | -3.56 |
| many | initial_layout_complete / working_set | 700.844 / 701.088 / 700.449 / 701.262 | 700.832 / 701.195 / 700.637 / 701.246 | 7 | -0.00 | +0.02 |
| many | open_body / peak_private_bytes | 208.297 / 215.013 / 208.086 / 223.629 | 208.969 / 209.952 / 208.012 / 210.898 | 7 | +0.32 | +0.11 |
| many | open_body / peak_working_set | 242.547 / 243.196 / 242.348 / 243.449 | 242.781 / 243.443 / 242.207 / 243.469 | 7 | +0.10 | -0.01 |
| many | open_body / private_bytes | 96.555 / 102.913 / 96.289 / 111.734 | 97.152 / 98.094 / 96.418 / 99.242 | 7 | +0.62 | +0.56 |
| many | open_body / private_commit | 88.781 / 89.097 / 88.438 / 89.113 | 89.094 / 89.479 / 88.641 / 89.488 | 7 | +0.35 | +0.31 |
| many | open_body / private_reserve | 191.668 / 191.822 / 190.676 / 191.941 | 191.363 / 191.953 / 191.016 / 191.953 | 7 | -0.16 | -0.10 |
| many | open_body / virtual_size | 570.660 / 570.962 / 569.738 / 571.035 | 570.789 / 570.940 / 570.719 / 570.973 | 7 | +0.02 | +0.04 |
| many | open_body / working_set | 131.207 / 131.570 / 130.801 / 131.605 | 131.352 / 131.633 / 130.926 / 131.691 | 7 | +0.11 | +0.10 |
| many | process_initialized / peak_private_bytes | 44.719 / 50.911 / 44.133 / 59.215 | 44.855 / 46.086 / 44.551 / 47.059 | 7 | +0.31 | +0.25 |
| many | process_initialized / peak_working_set | 78.609 / 79.270 / 78.281 / 79.383 | 78.660 / 79.170 / 78.367 / 79.207 | 7 | +0.06 | +0.10 |
| many | process_initialized / private_bytes | 44.527 / 50.720 / 43.945 / 59.023 | 44.660 / 45.893 / 44.363 / 46.863 | 7 | +0.30 | +0.26 |
| many | process_initialized / private_commit | 36.859 / 37.435 / 36.324 / 37.484 | 36.867 / 37.475 / 36.586 / 37.613 | 7 | +0.02 | +0.18 |
| many | process_initialized / private_reserve | 76.602 / 76.800 / 76.172 / 76.910 | 76.539 / 77.230 / 76.277 / 77.348 | 7 | -0.08 | -0.08 |
| many | process_initialized / virtual_size | 403.430 / 404.382 / 403.375 / 404.410 | 404.359 / 404.427 / 403.363 / 404.430 | 7 | +0.23 | +0.00 |
| many | process_initialized / working_set | 78.500 / 79.170 / 78.176 / 79.277 | 78.562 / 79.066 / 78.254 / 79.105 | 7 | +0.08 | +0.10 |
| many | return_body_idle / peak_private_bytes | 1173.410 / 1179.853 / 1172.324 / 1188.504 | 1047.484 / 1048.645 / 1047.086 / 1049.828 | 7 | -10.73 | -10.71 |
| many | return_body_idle / peak_working_set | 1123.129 / 1123.652 / 1122.441 / 1123.816 | 1078.289 / 1078.445 / 1077.957 / 1078.496 | 7 | -3.99 | -4.01 |
| many | return_body_idle / private_bytes | 746.227 / 752.398 / 744.934 / 761.117 | 703.316 / 704.438 / 702.586 / 705.246 | 7 | -5.75 | -5.69 |
| many | return_body_idle / private_commit | 736.930 / 737.263 / 735.797 / 737.371 | 693.691 / 694.592 / 693.523 / 694.836 | 7 | -5.87 | -5.81 |
| many | return_body_idle / private_reserve | 274.152 / 290.192 / 272.609 / 290.527 | 195.848 / 196.645 / 194.656 / 196.781 | 7 | -28.56 | -28.63 |
| many | return_body_idle / virtual_size | 1301.359 / 1317.111 / 1300.082 / 1317.207 | 1180.531 / 1180.566 / 1179.594 / 1180.590 | 7 | -9.28 | -9.36 |
| many | return_body_idle / working_set | 727.152 / 727.556 / 725.902 / 727.758 | 726.551 / 726.933 / 725.938 / 727.195 | 7 | -0.08 | -0.03 |
| many | second_source_generated / peak_private_bytes | 1173.410 / 1179.853 / 1172.324 / 1188.504 | 1047.484 / 1048.645 / 1047.086 / 1049.828 | 7 | -10.73 | -10.71 |
| many | second_source_generated / peak_working_set | 1123.129 / 1123.652 / 1122.441 / 1123.816 | 1078.289 / 1078.445 / 1077.957 / 1078.496 | 7 | -3.99 | -4.01 |
| many | second_source_generated / private_bytes | 766.340 / 772.534 / 765.809 / 780.793 | 723.555 / 725.653 / 723.258 / 727.230 | 7 | -5.58 | -5.54 |
| many | second_source_generated / private_commit | 756.992 / 757.559 / 756.672 / 757.852 | 714.492 / 715.523 / 714.242 / 715.539 | 7 | -5.61 | -5.59 |
| many | second_source_generated / private_reserve | 253.840 / 269.794 / 253.191 / 269.836 | 174.945 / 176.059 / 173.988 / 176.148 | 7 | -31.08 | -31.08 |
| many | second_source_generated / virtual_size | 1301.484 / 1317.236 / 1300.207 / 1317.332 | 1180.656 / 1180.691 / 1179.652 / 1180.715 | 7 | -9.28 | -9.36 |
| many | second_source_generated / working_set | 748.121 / 748.584 / 747.672 / 748.758 | 747.273 / 748.373 / 746.836 / 748.406 | 7 | -0.11 | -0.09 |
| many | second_source_stable / peak_private_bytes | 1173.410 / 1179.853 / 1172.324 / 1188.504 | 1047.484 / 1048.645 / 1047.086 / 1049.828 | 7 | -10.73 | -10.71 |
| many | second_source_stable / peak_working_set | 1123.129 / 1123.652 / 1122.441 / 1123.816 | 1078.289 / 1078.445 / 1077.957 / 1078.496 | 7 | -3.99 | -4.01 |
| many | second_source_stable / private_bytes | 765.520 / 771.766 / 764.984 / 779.969 | 722.730 / 724.858 / 722.465 / 726.402 | 7 | -5.59 | -5.55 |
| many | second_source_stable / private_commit | 756.262 / 756.816 / 755.848 / 757.121 | 713.668 / 714.716 / 713.449 / 714.766 | 7 | -5.63 | -5.60 |
| many | second_source_stable / private_reserve | 254.465 / 270.449 / 253.785 / 270.461 | 175.641 / 176.725 / 174.691 / 176.816 | 7 | -30.98 | -30.98 |
| many | second_source_stable / virtual_size | 1301.359 / 1317.111 / 1300.082 / 1317.207 | 1180.531 / 1180.566 / 1179.527 / 1180.590 | 7 | -9.28 | -9.36 |
| many | second_source_stable / working_set | 747.410 / 747.859 / 746.910 / 748.059 | 746.500 / 747.596 / 746.070 / 747.617 | 7 | -0.12 | -0.10 |
| many | source_generated / peak_private_bytes | 710.457 / 716.345 / 709.492 / 724.859 | 585.023 / 586.575 / 584.477 / 587.695 | 7 | -17.66 | -17.62 |
| many | source_generated / peak_working_set | 660.062 / 660.600 / 659.648 / 660.691 | 576.984 / 577.436 / 576.484 / 577.539 | 7 | -12.59 | -12.55 |
| many | source_generated / private_bytes | 460.113 / 466.111 / 459.531 / 474.762 | 418.500 / 419.598 / 417.980 / 420.648 | 7 | -9.04 | -9.05 |
| many | source_generated / private_commit | 451.453 / 451.781 / 450.949 / 451.887 | 410.039 / 410.339 / 409.566 / 410.473 | 7 | -9.17 | -9.20 |
| many | source_generated / private_reserve | 174.414 / 174.643 / 173.961 / 174.863 | 174.035 / 174.585 / 173.547 / 174.719 | 7 | -0.22 | -0.03 |
| many | source_generated / virtual_size | 916.027 / 916.170 / 915.848 / 916.230 | 874.367 / 874.505 / 874.305 / 874.578 | 7 | -4.55 | -4.54 |
| many | source_generated / working_set | 452.211 / 452.519 / 451.941 / 452.617 | 452.250 / 452.656 / 452.121 / 452.715 | 7 | +0.01 | +0.03 |
| many | source_stable / peak_private_bytes | 710.457 / 716.345 / 709.492 / 724.859 | 667.812 / 668.830 / 667.301 / 669.992 | 7 | -6.00 | -5.97 |
| many | source_stable / peak_working_set | 700.844 / 701.088 / 700.449 / 701.262 | 700.836 / 701.196 / 700.637 / 701.250 | 7 | -0.00 | +0.02 |
| many | source_stable / private_bytes | 709.363 / 715.420 / 708.734 / 724.035 | 667.812 / 668.830 / 667.301 / 669.992 | 7 | -5.86 | -5.86 |
| many | source_stable / private_commit | 700.238 / 700.583 / 699.707 / 700.730 | 658.758 / 659.087 / 658.391 / 659.137 | 7 | -5.92 | -5.94 |
| many | source_stable / private_reserve | 177.969 / 178.272 / 177.492 / 178.480 | 177.660 / 178.125 / 177.258 / 178.242 | 7 | -0.17 | -0.05 |
| many | source_stable / virtual_size | 1168.402 / 1168.545 / 1168.223 / 1168.605 | 1126.742 / 1126.880 / 1126.680 / 1126.953 | 7 | -3.57 | -3.56 |
| many | source_stable / working_set | 700.844 / 701.088 / 700.449 / 701.262 | 700.836 / 701.196 / 700.637 / 701.250 | 7 | -0.00 | +0.02 |

Полные машинные данные всех дополнительных счётчиков, CString/XML/Scintilla shapes и paired ratios: `out/perf/c5/accepted-comparison/statistics.json` и `.csv`.
Полные roundtrip FB2 сохраняются один раз по SHA в `out/perf/c5/roundtrip-objects`, process manifests ссылаются на них. Это дедупликация одинаковых bytes; все equivalence checks сохранены.
<!-- C5_FINAL_END -->

## Оценка окончательного candidate

Приняты только две локальные production правки. Source stable Private:
many 709.36→667.81 MiB (−5.86%), large 677.77→637.73 MiB (−5.91%).
Это умеренный выигрыш active Source, а не уменьшение всей Scintilla representation.
Source stable Working Set практически не изменился (<0.02%): устранённый CString
slack был committed, но не обязательно resident. CString slack после изменения
равен 10 bytes alignment на каждую many/large projection; после edited Apply
старый cached CString освобождён полностью.

Более сильный практический эффект — после edited Apply: Body idle Private
801.62→590.23 MiB (−26.37%) /759.48→558.72 MiB (−26.43%);
Working Set −21.26% /−21.06%. После readonly второго Source cache сохраняется,
поэтому Private уменьшается лишь на 5.75% /5.69%. Overall scenario Peak Private
1173.41→1047.48 MiB (−10.73%) /1120.71→1046.77 MiB (−6.60%);
Peak Working Set −3.99% /−0.56%.

Body→Source ускорился на 8.48% /6.57%, повторный Source — на 5.47% /3.69%.
Time to Source stable уменьшился на 4.30% /2.42%. Scroll many +1.44%, large −2.30%;
Find −2.45% /+0.62%; Source→Body +0.82% /+0.36% — без заметной стабильной регрессии.
Many Replace −12.10%; large Replace +1.73%.

Отрицательные результаты не исключены из acceptance:

- **Формально стабильный timing slowdown**: large `first_layout` +24.00%,
  >5% в 6/7 пар, 0.0025→0.0031 ms (**+0.6 µs**). Это явный дополнительный repaint
  после ShowView, не время первоначального layout документа. Полное ожидание
  Source stable при этом ускорилось. Поэтому микрозадержка не перевешивает
  снижение Body memory и ускорение входа в Source.
- Many `first_layout` +12.00% (4/7), `small_edit` +7.04% (4/7,
  0.2144→0.2295 ms); large caret +8.57% (3/7, 19.7450→21.4377 ms).
  По принятому правилу эти increases не стабильны.
- Control N=3: caret +9.32% (3/3), repaint +13.64% (1/3), Open +5.93% (2/3),
  scroll +5.83% (2/3). N=3 не позволяет применить критерий 6/7;
  результат не объявляется доказательством отсутствия slowdown на control.
- **Стабильный рост reserve many**: сразу после Apply +97.99% (7/7),
  после Body idle 87.21→173.25 MiB, **+98.66%** (7/7).
  Это uncommitted MEM_PRIVATE reserve. На той же Body idle точке private_commit
  792.47→580.96 MiB (−26.69%), Virtual Size 1170.13→1045.05 MiB (−10.69%).
  Рост reserve не выдаётся за уменьшение памяти; его поблочное происхождение
  текущим C5 не атрибутируется. Large reserve на Body idle −0.54%.

Все 10 median increases >5% приведены в таблице выше, включая reserve и control.
Менять candidate или повторять benchmark ради устранения отрицательного наблюдения
не требуется: выбраны доказанные существенные выигрыши с явно указанными trade-offs.

## Воспроизведение и evidence

PowerShell 7; явный Windows Python; не менять исходники, fixtures или runtimes
между freeze и финальным hash audit. Для текущих сохранённых snapshots:

```powershell
./tools/perf/compare-source-memory-c5.ps1 `
  -PythonExe C:/Users/Sklyarov/AppData/Local/Programs/Python/Python314/python.exe `
  -OldRuntimeDirectory out/perf/c5/baseline-harness-v3-runtime `
  -NewRuntimeDirectory out/perf/c5/accepted-runtime `
  -OutputDirectory out/perf/c5/accepted-comparison
& C:/Users/Sklyarov/AppData/Local/Programs/Python/Python314/python.exe `
  ./tools/perf/analyze-source-memory-c5.py out/perf/c5/accepted-comparison
```

Полностью завершённый comparison только сверяет сохранённые PASS processes и
frozen inputs; для нового измерения нужен новый OutputDirectory. Analyzer следует
запускать тем же явным Windows Python, а не случайным `python` из PATH.
`build-source-memory-c5-diagnostic.ps1` собирает только FBE и включает три
compile-time diagnostic defines; штатный Release их не содержит.
Baseline harness v3 собран из production sources C4.1 с одинаковыми test-only
benchmark overlays; sourceHashes сохранены в обоих runtime metadata.

`accepted-release-commands.json` подтверждает /W4 /WX и отсутствие benchmark define
именно у двух изменённых lifetime translation units.
`accepted-native-hash-audit.json` подтверждает совпадение всех 456 native source
файлов с принятым compiled snapshot. Close invariants проверены во всех 28 measured
many/large processes: Source bytes, CString capacity, saved XML и canonical bytes
равны нулю. Локальные evidence остаются под ignored `out/perf/c5`.

## Remaining architectural limits

Полная XML/UTF-16/UTF-8/Scintilla representation и DOCUMENT layout остаются нужны.
После Close логические Source/cache/saved XML/canonical bytes равны нулю, однако
Private Bytes могут оставаться высокими из-за retained allocator/third-party memory;
это не доказательство живого Source или утечки само по себе.
Future work: исследование скрытой idle-стилизации возможно отдельной задачей только
при конкретной потребности; дальнейшей реализации в рамках C5 нет.

## External acceptance и manual GUI

После локального commit требуется отдельное разрешение на push точного final SHA
в `codex/c5-source-memory`, затем hosted Build/OLE receipt того же SHA и
`pwsh ./tools/build/verify-release.ps1 -Configuration Release -FullValidation`.
FullValidation должен закончиться exit 0 без skipped mandatory gates и принять
exact-head receipt. Локальные тесты не заменяют этот external gate.
Push и запуск внешнего workflow в рамках текущего разрешения не выполняются.

Ручной smoke на окончательной Release-сборке (пока **PENDING**):

1. Открыть большую реальную FB2 с изображениями.
2. Body → Source.
3. Быстро прокрутить начало, середину, конец.
4. Выполнить поиск.
5. Внести небольшую правку Source.
6. Source → Body.
7. Проверить Undo/Redo.
8. Заменить или добавить изображение.
9. Save.
10. Close → Reopen.
11. Повторно открыть Source.
12. Выполнить один старый script.
