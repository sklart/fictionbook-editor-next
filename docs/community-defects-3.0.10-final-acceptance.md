# Заключительная проверка FBE Next 3.0.10

Дата: 28.10.2026. Предыдущий отчёт не изменялся.

## Ревизии и объём

Исходный master/origin/master: `ebc4f62542654849a376d57e3a69236045acd13f`.
По последующему указанию пользователя работа выполнена прямо в master, без дополнительных веток; разрешение на push получено на завершение задачи. Пустая первоначально созданная follow-up ветка удалена до изменений.

Ревизия production-кода, тестов и чистой сборки: `fc60a33abdeea75915ab158d1ba5505feb7d92e0`.
Финальный документационный SHA сообщается отдельно после фиксации этого отчёта. Issues не закрывались.

Коммиты:

- `d5eedc8e`: точные границы многоабзацных выделений, независимые XML-позиции и дерево;
- `d64f0525`: сохранение запроса вставки при отказе PostMessage, Idle replay, безопасное уведомление и lifecycle-тесты;
- `a35ab2eb`, `241b0f67`, `fa67c42a`, `1ff532c9`: native benchmark, reuse snapshot вместо дополнительного DOM, XSD/проверка фактического переключения и устранение лишнего unmeasured round-trip.
- `ed2f8891`: уведомление при сочетании принятого запроса и следующего отказа захвата в том же Idle.
- `e393b624`, `3ad99126`, `30901a68`: выбор измеряемого build и CPU/priority-контроль; окончательная affinity применяется только к UI-потоку, без ограничения остальных потоков.
- `b824fd54` / `912aa4f6`: пробное изменение порядка selection и его откат после отсутствия подтверждённого выигрыша; net production diff presentation отсутствует.

- `a79861bb`: сохранение/восстановление исходного OLE clipboard в старом HTML runtime-тесте с проверкой HRESULT и flush;
- `fc60a33a`: независимые процессы caret/range для 50 МиБ, без изменения timeout 600 с или числа повторов.

## Подтверждённый дефект PostMessage

Новый реальный MSHTML/OLE сценарий `queue-post-idle` на прежнем обработчике дал RED: canceled paste не изменял DOM, но захваченный запрос удалялся и не выполнялся. Артефакты RED сохранены в `build/community-temp/fr-e1198f35df544bf5ac731d6ad695ef50` и соответствующем TSV, путь имеется в `build/followup-post-red-result.log`; также сохранён вывод tool/runtime RED.

Теперь при отказе публикации owning request остаётся в очереди. Штатный Idle после возврата COM callback завершает отменённый native event и исполняет запрос. Содержимое берётся из неизменяемого event-time snapshot, а не из более нового clipboard. Повторный Idle не вставляет его ещё раз. Записываются реальный HRESULT отказа и безопасные этап/маршрут CP100/CP315/CP402–403.

Ошибка захвата запроса не открывает modal внутри onpaste: callback сохраняет HRESULT и CP206, штатный U::ReportError вызывается из Idle. Runtime проверяет отсутствие U100 до возврата callback и его появление после Idle. Видимое окно ошибки требует ручного smoke; в unattended batch штатная реализация U::ReportError намеренно не показывает окно.

Ожидания старых queue-post-failure изменены после RED/GREEN-доказательства: теперь принятая вставка успешно выполняется, образует ровно одну Undo-запись и штатно очищает предшествующий Redo. Это обычная семантика нового редактирования; сохранение прежнего Redo требуется для отказа захвата без редактирования и отдельно проверено.

Тест `queue-post-clean-failure` дополнительно исправляет подготовку: предварительное создание/отмена Future edit изменяло версию и оставляло исходное состояние dirty. После подготовки теперь явно устанавливаются и проверяются clean/recovery checkpoints. Проверки конечного dirty/recovery состояния не ослаблялись.

Новый тест смены выделения сохраняет отдельные эталоны до следующей вставки и после предыдущей: Undo второй операции возвращает вручную перемещённую каретку, а Redo первой — каретку после первой вставки. Первоначально один эталон ошибочно использовался для обеих точек. Production-правки сверх подтверждённого отказа публикации/уведомления не потребовались.


Комбинированный `queue-capture-after-post` дал RED на первоначальной реализации deferred notice: ранний return после успешного replay откладывал уведомление о следующем отказе захвата до нового Idle. `ed2f8891` устранил этот ранний выход. GREEN доказывает, что принятый запрос исполнен один раз, отказ второго запроса сообщён в первом безопасном Idle, а повторный Idle не дублирует уведомление. Артефакты: `build/followup-single-selection-and-notification-red.log`, `build/community-temp/fbe-html-paste-36090b2d40234adbb2e6a508f3a74a5f`.

## Методика и измерения

Оборудование: Intel Core i5-13400F (гибридные P/E-ядра, 16 logical CPU), 34 142 842 880 байт физической RAM, Windows 11 Pro 10.0.26300. Окончательное сравнение: одинаковая affinity mask 1 (CPU 0) только для UI-потока, остальные потоки не ограничены; Normal priority, priority boost disabled, один прогрев и пять измеряемых повторов отдельно для caret/range. Условия фиксируются в environment.json; defaults не отключают никакие release-проверки.

Предварительные прогоны без фиксированного CPU показали двукратные скачки даже в неизменном constructor индекса внутри одного процесса. Это не позволяет приписать разницу продуктовой регрессии. На фиксированном CPU 0 контроль 1 МБ: Source→Body median базы 537/560 мс (caret/range), исходной исправленной сборки 522/524 мс, пробного изменения 532/553 мс. Выигрыш пробного ordering не подтверждён, оно полностью отменено; прежняя гарантия восстановления после focus сохранена. Связь скачков с P/E scheduling — объяснение по косвенным данным, а не результат CPU trace.

Не используются для итоговых выводов: предварительные fixtures с невалидной mixed section; probe с дополнительным XML DOM; probe с дополнительным unmeasured view round-trip; неприкреплённые к CPU результаты; пробы с ограничением всего процесса одним ядром, которое вмешивалось в работу MSHTML/сборку памяти. Они оставлены как промежуточные артефакты, а не выданы за PASS. Финальный generator проверяет каждый FB2 по XSD и actual Source/Body mode. Legacy b8 используется только для времени/памяти, его прежние функциональные ошибки mapping не считаются правильным поведением.

Сборка сравнения b8: исходники `b8c0611e8947bd1c68beb5f983ef156375ad095c` плюс test-only named benchmark, read-only snapshot getter и заголовок измерительного индекса. Production алгоритмы b8 не переносились из новой версии. Patch и копии instrumentation сохраняются в build; для legacy index_ms=-1 (индекса в старом production нет). Один и тот же детерминированный файл используется обеими сборками, SHA256 каждого fixture сохраняется.

index_live_delta — разность committed private bytes при живом индексе, а не полный размер его allocation: повторное использование heap может давать ноль. PeakPagefileUsage отражает process high-water mark. WM_NULL heartbeat показывает native responsiveness в batch; это не ручной GUI smoke.

## Сравнение 1/20 МиБ

Формат: median [min; max], пять измерений после прогрева; время в мс. Raw TSV и summary.csv: `build/followup-performance-ui-final`. Candidate этих замеров — `30901a68d8dbfd6cc6a2e39429759dc96afbeadf`, EXE `8983C56A0129BA0473A039C6A777C5D270A24A29A5A8DE08F63DBA645987D707`; b8 instrumented EXE `DE1ED5E392CE6F5DDFD3D275CC140CC2B68B34E0D7E9F946ECFF5350D75D5B89`. Изменения 309..fc60 затрагивают только тестовое восстановление clipboard и разделение 50 МиБ на процессы; production mapping, индекс и переключение режимов одинаковы. Повторять завершённые 1/20 МиБ замеры после этих test-only изменений не требовалось.

| МиБ | Режим | Build | Body→Source | Source→Body |
|---|---|---|---|---|
| 1 | caret | baseline | 4.3 [4.1; 518.7] | 529.6 [511.0; 543.5] |
| 1 | caret | candidate | 12.8 [12.4; 513.4] | 541.8 [527.8; 556.7] |
| 1 | range | baseline | 39.0 [37.7; 41.0] | 619.6 [546.4; 673.7] |
| 1 | range | candidate | 13.0 [12.2; 13.9] | 561.0 [549.1; 564.3] |
| 20 | caret | baseline | 28.6 [26.4; 11164.5] | 11224.6 [10705.1; 11540.7] |
| 20 | caret | candidate | 121.1 [98.8; 11673.0] | 11220.3 [10179.2; 11777.3] |
| 20 | range | baseline | 563.6 [558.1; 639.9] | 11493.1 [11410.3; 11763.4] |
| 20 | range | candidate | 97.5 [91.3; 103.8] | 11281.5 [10859.4; 12344.3] |

| МиБ | Режим | Build | Индекс | Map Body | Map Source |
|---|---|---|---|---|---|
| 1 | caret | baseline | N/A | 1.5 [1.5; 4.1] | 0.6 [0.6; 1.1] |
| 1 | caret | candidate | 1.9 [1.6; 2.2] | 4.9 [4.7; 5.8] | 3.1 [2.9; 5.7] |
| 1 | range | baseline | N/A | 30.7 [28.5; 32.8] | 44.4 [43.7; 62.6] |
| 1 | range | candidate | 1.8 [1.8; 2.1] | 4.1 [4.0; 4.4] | 3.2 [2.8; 3.3] |
| 20 | caret | baseline | N/A | 24.7 [23.6; 29.5] | 13.6 [12.8; 18.7] |
| 20 | caret | candidate | 42.0 [41.0; 46.6] | 98.3 [91.1; 127.4] | 71.2 [65.6; 91.6] |
| 20 | range | baseline | N/A | 563.4 [551.6; 578.1] | 896.6 [879.4; 926.0] |
| 20 | range | candidate | 44.9 [43.4; 48.0] | 83.9 [80.9; 105.0] | 71.4 [69.8; 98.2] |

Память в МиБ; heartbeat max в мс, таймауты — число неответивших запросов за переход. Это native batch-метрика, не ручная оценка.

| МиБ | Режим | Build | Private | Peak private | Живой индекс Δ | Heartbeat max | Таймауты |
|---|---|---|---|---|---|---|---|
| 1 | caret | baseline | 84.3 [83.5; 86.3] | 97.7 [97.7; 97.7] | N/A | 545.3 [519.3; 1029.7] | 0.0 [0.0; 0.0] |
| 1 | caret | candidate | 84.9 [84.5; 86.8] | 97.6 [97.6; 97.6] | 0.0 [0.0; 0.8] | 555.0 [540.5; 1063.7] | 0.0 [0.0; 0.0] |
| 1 | range | baseline | 84.8 [83.0; 84.9] | 109.2 [109.1; 109.2] | N/A | 658.4 [584.1; 715.0] | 0.0 [0.0; 0.0] |
| 1 | range | candidate | 84.9 [84.2; 85.5] | 97.6 [97.6; 97.6] | 0.5 [0.0; 0.5] | 574.5 [561.6; 576.9] | 0.0 [0.0; 0.0] |
| 20 | caret | baseline | 749.5 [740.5; 915.9] | 1096.3 [1096.3; 1096.3] | N/A | 10010.1 [10002.6; 10014.0] | 92.0 [56.0; 755.0] |
| 20 | caret | candidate | 707.1 [701.0; 887.1] | 1031.9 [1031.9; 1031.9] | 12.1 [11.9; 13.6] | 10006.4 [10003.2; 10011.9] | 116.0 [17.0; 815.0] |
| 20 | range | baseline | 833.6 [824.0; 835.9] | 1351.5 [1351.5; 1351.5] | N/A | 10002.0 [10000.7; 10005.1] | 132.0 [125.0; 151.0] |
| 20 | range | candidate | 793.9 [788.4; 798.7] | 1031.9 [1031.9; 1031.9] | 11.9 [11.9; 13.3] | 10012.0 [10007.8; 10013.8] | 88.0 [60.0; 155.0] |

Первый проход (прогрев, не включён в median):

| МиБ | Режим | Build | Body→Source, мс | Source→Body, мс |
|---|---|---|---|---|
| 1 | caret | baseline | 941.9 | 515.0 |
| 1 | caret | candidate | 999.0 | 531.2 |
| 1 | range | baseline | 38.8 | 583.5 |
| 1 | range | candidate | 13.2 | 555.5 |
| 20 | caret | baseline | 25921.5 | 10290.5 |
| 20 | caret | candidate | 27283.5 | 10352.1 |
| 20 | range | baseline | 648.8 | 12478.8 |
| 20 | range | candidate | 95.0 | 11150.5 |

На 20 МиБ range mapping и Body→Source быстрее базы, Source→Body остаётся примерно 11 с и блокирует UI в обеих версиях. Для caret новая точная карта дороже legacy: median 121 против 29 мс на 20 МиБ, 13 против 4 мс на 1 МиБ. Это измеренное увеличение стоимости; старый нулевой/приблизительный caret не считается эквивалентной проверкой точности. Изолированное построение индекса около 42–45 мс и 12 МиБ живого private delta на 20 МиБ. Пики памяти кандидата ниже; нет оснований приписывать весь 11-секундный переход построению индекса. Полная приёмка производительности остаётся INCONCLUSIVE из-за незавершённого парного сравнения 50 МиБ и отсутствия ручного GUI smoke.

## Нагрузочный контур 50 МиБ

Candidate измерен на финальном EXE fc60a33a / SHA256 `0F5FA8F0EAA1C0A47D11959D77EE1E347B15D760307BF47FEB9A87ECAEE1F22F`. В `build/followup-performance-50-separated` сохраняются raw TSV, идентичный XSD-valid fixture, хеши и условия. Ниже caret: median [min; max] пяти измерений после прогрева.

| Метрика | b8 | Candidate |
|---|---|---|
| Body→Source, мс | 68.8 [66.4; 87.3] | 271.7 [248.7; 315.9] |
| Source→Body, мс | 39125.0 [37655.2; 40654.8] | 36424.7 [36163.4; 39341.7] |
| Индекс, мс | N/A | 112.8 [105.7; 150.6] |
| Map Body, мс | 62.1 [59.6; 66.7] | 259.1 [249.7; 386.4] |
| Map Source, мс | 35.1 [32.5; 35.5] | 188.6 [174.1; 242.7] |
| Private, МиБ | 1858.0 [1854.4; 1868.9] | 1837.3 [1835.5; 1864.9] |
| Peak private, МиБ | 2158.2 [2158.2; 2158.2] | 2075.4 [2075.4; 2075.4] |
| Живой индекс Δ, МиБ | N/A | 26.8 [26.8; 27.5] |
| Heartbeat max, мс | 10009.0 [10004.4; 10014.7] | 10007.6 [10001.2; 10014.5] |
| Heartbeat timeouts | 1888.0 [1782.0; 1985.0] | 1700.0 [1689.0; 1899.0] |

Прогрев caret: b8 114089/43108 мс, candidate 111576/41857 мс (Body→Source / Source→Body). Холодное переключение порядка двух минут существовало уже в b8; измеренный индекс кандидата около 0,1 с сам по себе этого не объясняет.

Первый b8-процесс завершил все пять caret, затем упал при начале range: exit `-529697949` / `0xE06D7363`, реальный C++ allocation failure. Crash TXT и непустой DMP сохранены в `build/community-temp/fr-9850b88afc7c407f93044e63f4663eb0`. Стек: `_CxxThrowException` → `operator new` → `std::vector::_Emplace_reallocate` → `FBEBodySourceTransfer::FindVisibleXmlTextRange` → `SourceDocumentTransfer::FindVisibleXmlTextRange` → `BodySourceSelectionCoordinator::MapBodySelectionToSource` → `SourceViewSession::PrepareSourceDocument`. Это не timeout и не сбой нового индекса.

Новый кандидат range в отдельном свежем процессе прошёл прогрев и **4/5** измерений, затем штатный runner остановил его по **600-секундному таймауту** (PID 4120). Полный log: `build/followup-performance-50-separated.log`; runtime `build/community-temp/fr-0d0234e15fd642a9909568a2f5abc5fb`. Незавершённый ряд не считается требуемыми пятью измерениями и не получает PASS. Наблюдаемые диапазоны четырёх повторов: Body→Source 222–321 мс, Source→Body 39018–39424 мс, индекс 104–113 мс, private 1852–1889 МиБ, peak 2289 МиБ. Прогрев: 115515/48352 мс. Это частичные данные, не доказательство отсутствия регрессии на 50 МиБ.

Отдельная проверка b8 range со свежим heap повторно завершилась с `0xE06D7363` до первого измерения; артефакты `build/community-temp/fr-5228886c7d6149599dadd02053c0aebd`, log `build/followup-baseline50-range.log`. Итог сравнения производительности **INCONCLUSIVE**; условия, число повторов и таймауты не ослаблялись.

## Точность выделения

Native matrix расширена с 180 до **252** случаев. Для новых диапазонов сравниваются `StartToStart` и `EndToEnd` исходного/восстановленного IHTMLTxtRange, обе позиции Scintilla, ближайший соответствующий элемент дерева и отсутствие лишнего dirty. Независимый XML oracle использует fixture ID и явно заданные сериализованные префиксы; production XmlSelectionPosition для oracle не используется.

Покрыты середина соседних абзацев, три абзаца и пустой внутри, заголовок, граница вложенных разделов, одинаковые фразы, кириллица/NBSP/emoji/entities, strong/emphasis/a, прямой и обратный SCI_SETSEL, три цикла, повтор после Source edit и Undo/Redo. MSHTML TextRange нормализует границы; направление проверяется на Source anchor/caret, сохранение визуального направления Body не заявляется.

Приблизительная позиция внутри XML-разметки по-прежнему проверяется отдельно как approximate. Новые точные сценарии прошли на существующем coordinator; production mapping не изменялся.

## Сборка и комплект

Чистая first-party Release Win32 пересборка выполнена публичным build.ps1 с `-ForceRebuildRequiredProjects -StrictFirstPartyWarnings -SkipUpx -SkipVersionSync`. Pinned dependency caches проверены штатными механизмами; сторонние версии и gitlinks не менялись.

Toolchain: v143 / VC Tools 14.44.35207 (cl.exe FileVersion 19.44.35229.0), SDK 10.0.26100.0; версия FBE 3.0.10.
Лог: `D:\Download\FBeditor\build\followup-accepted-source-clean-build.log`.
CommonCore/Runtime provenance указывают на `fc60a33abdeea75915ab158d1ba5505feb7d92e0`.
Scintilla/Lexilla остались неизменными, все подмодули соответствуют gitlinks.

Полный новый каталог: `D:\Download\FBeditor\build\community-3010-final-fc60a33a\runtime`.
EXE SHA256: `0F5FA8F0EAA1C0A47D11959D77EE1E347B15D760307BF47FEB9A87ECAEE1F22F`.
Синтетический FB2: `D:\Download\FBeditor\build\community-3010-final-fc60a33a\COMMUNITY_SMOKE_3010.fb2`.
Рядом сохранены version.txt, runtime-sha256.json, submodules.txt и оба provenance JSON. Комплект содержит main.js, XSL, DLL, Plugins, Lang, Dict, Scripts и остальные staged ресурсы; это полный runtime, а не отдельный EXE. Профиль приватный; startup-проверка макросов в ручном комплекте выключена, сами Scripts включены. Native-тесты используют отдельный приватный пустой каталог макросов. Проверка всего пользовательского набора макросов не заявляется.

## Результаты (заполняются после завершения)

| Направление | Статус |
|---|---|
| Надёжность Copy/Paste, исходный периодический случай | INCONCLUSIVE; подтверждённые случаи проверены отдельно |
| Отказ PostMessage | PASS, 28 реальных MSHTML/OLE сценариев |
| Перенос выделения | PASS, 252 на финальном EXE и непосредственно из portable |
| Производительность | INCONCLUSIVE — 50 МиБ: эталон crash, candidate range 4/5 до timeout |
| Release Win32 | PASS |
| FullValidation | FAIL / INCOMPLETE — source-safety остановился на смешанных окончаниях строк; исправлено и адресный тест PASS, полный gate не повторялся |
| Ручной GUI smoke | PENDING MANUAL |
| Импорт пользовательского EPUB | PENDING FILE |

## Итог автоматических проверок

На финальном EXE прошли все 20 контуров `build/followup-final-targeted-results.json`: 28 real HTML paste, 252 selection, native Copy/Cut (9 сценариев, 10 процессов), href, clipboard, Body/Source, malformed fallback/correction, таблицы, поведенческие и boundary тесты, sanitizer, diagnostic privacy; прямой запуск из portable также прошёл 252 случая. Лог `build/followup-final-targeted.log`. Проверены 1475 файлов runtime manifest, расхождений нет; изменяемый профиль Data исключён.

Единственный запуск `verify-release.ps1 -Configuration Release -FullValidation` завершился кодом 1 на `test-source-safety.ps1`: смешанные CRLF/LF в `SourceViewSession.h`. Аналогичные смешанные окончания обнаружены и нормализованы в трёх изменённых файлах; git содержимое после нормализации не отличается, production семантика и EXE прежние. Отдельный повтор `test-source-safety.ps1` PASS. Полный контроль после этой точки не выполнялся, поэтому остальные проверки gate не подтверждены; известный UI defects light/ru-RU в этом запуске не достигнут. Лог `build/followup-fullvalidation.log`.

## Ограничение GUI

В этом этапе skill `computer-use` (версия 26.1023.50603) не смог инициализировать JavaScript kernel: `failed to write kernel assets: Системе не удается найти указанный путь. (os error 3)`. После js_reset и одной повторной инициализации результат тот же. Контроль окон даже не начался; прежняя ошибка foreground PID в этом запуске не подтверждалась.

Обязательный guidance skill предписывает при неудачной recovery “stop and report that the Windows Computer Use helper may have failed”. Остановлена только GUI-автоматизация. Ограничения инструмента не обходились; build/native/runtime проверки продолжены.

## Ручная матрица Windows 10/11

1. Запустить EXE из нового runtime и открыть приложенный синтетический FB2. Личные книги проверять на копиях.
2. Copy/Cut/Paste символа, слова, абзаца, форматированной ссылки/сноски: внутри абзаца, между абзацами, разделами и двумя документами. Повторить Ctrl+Z/Y 20 раз. Добавить внешний Unicode/NBSP/многострочный текст. Проверить текст и признак изменений после Undo/Redo.
3. Редактировать «Ссылка» (#note, HTTP/HTTPS): начало/середина/конец, вставка/удаление/выделение, Idle, реальный href и Undo/Redo.
4. Тело → Код → Тело без выделения и с ним: одинаковые фразы, заголовок, Unicode, notes/comments, вложенные разделы. Три цикла без dirty.
5. Выделить от середины одного абзаца до середины следующего, затем три абзаца с пустым внутри. Проверить обе границы, XML, обратный переход и дерево. Повторить после правки в Body/Source и Undo/Redo.
6. Сохранить под новым именем, закрыть и открыть: текст, ссылки, сноски, форматирование и FB2 validation должны сохраниться.
7. При воспроизведении сбоя сразу открыть Справка → Диагностика FBE Next и сохранить ZIP соответствующего сеанса. Повторить после перезапуска; наличие сообщения «Журнал отсутствует» само по себе не означает отсутствие crash/error данных. Приложить точные действия и файл, если допустимо. Пользовательский EPUB пока не предоставлен.

## Изменённые файлы

- src/fbe/FBEview.cpp, FBEview.h — сохранение request, безопасное уведомление и guarded failure injection;
- src/fbe/source/SourceViewSession.h — read-only диагностический доступ к существующему XML snapshot без клонирования;
- src/fbe/mainfrm.cpp — include измерительного индекса для test-only benchmark;
- src/fbe/testing/RuntimeTestEditorAndExport.inl, RuntimeTestNavigationAndStructure.inl;
- src/fbe/testing/RuntimeTestCommunityPasteLifecycle.inl, RuntimeTestCommunitySelection.inl, RuntimeTestCommunitySelectionRanges.inl, RuntimeTestCommunityPerformance.inl;
- tools/tests/test-fbe-html-paste-runtime.ps1, test-fbe-community-selection-runtime.ps1, test-fbe-community-transition-performance.ps1;
- этот отчёт.

Исторический verification.md, vendor, COM IDL, версии, CI, GUI/import production и пользовательские книги не менялись.
