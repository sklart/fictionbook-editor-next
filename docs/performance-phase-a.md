# Производительность редактора: Phase A

## Что измеряется

Диагностический журнал FBE Next (только при `FBE_NEXT_TRACE=1` или включённой
пользовательской настройке) пишет агрегированное событие `performance/P410` на
каждые 256 вызовов `CMainFrame::OnIdle`. В штатном режиме этот счётчик и
измерения полностью выключены.

Событие содержит число вызовов, суммарное, максимальное и среднее время idle,
а также отдельные суммы для command state, SOURCE, selection context, link/table
attribute bars, toolbar, локализации toolbar, tree, проверки fingerprint файла, spellcheck, status bar
и заголовка. В нём также есть число `UIUpdateViewCmd`, JS/COM dispatch
(`js-com-calls`) и счётчики обновлений команд, selection, SOURCE, toolbar,
tree, файла и clipboard. Журнал не включает текст книги и пути к ней.

## Воспроизводимый замер

Перед каждым сравнением закрыть остальные экземпляры FBE, использовать одну
и ту же Release-сборку, разрешение/DPI, язык интерфейса и файл. Тестировать
файл с несколькими тысячами абзацев и не менее одним большим разделом.

1. Включить `FBE_NEXT_TRACE=1`, запустить FBE с этой книгой и дождаться
   стабилизации окна.
2. Оставить BODY без действий на 10 секунд; затем выполнить 20 перемещений
   caret/selection, 10 локальных правок, 10 переключений BODY/SOURCE и один
   внешний save файла. Для spellcheck прокрутить каждый видимый экран ровно
   один раз.
3. Закрыть приложение обычным способом и взять строки `performance/P410` из
   `%LOCALAPPDATA%\FBE Next\Diagnostics\fbe-trace*.log` только данного PID.
4. Сравнить последнюю полную строку каждого прогона. Значения `*-ms` — это
   накопленные миллисекунды, поэтому сравнивать нужно прогоны с одинаковым
   `idle-count`; `js-com-calls`, `command-state-updates`, `toolbar-updates` и
   `file-fingerprint-checks` сравниваются как абсолютные счётчики.

Сценарий повторяется трижды для baseline и трижды после изменения. В отчёт
вносятся медианы, версия/commit, размер fixture, DPI, режим BODY/SOURCE и
точные строки P410. Нулевое значение миллисекунд допустимо: таймер Windows
имеет миллисекундное разрешение; в этом случае главным критерием становятся
счётчики вызовов.

Автоматический after-guard запускается отдельно от FAST:

```powershell
pwsh .\tools\tests\test-fbe-idle-performance-runtime.ps1
```

Он создаёт реальные FB2 на 1 000 и 10 000 абзацев, выполняет первичный
event-driven update и затем 1 000 неизменных `OnIdle`: для BODY на обоих
размерах и для SOURCE на 10 000 абзацев. Отчёт включает фактический `view`,
поэтому тест не может незаметно проверить BODY вместо SOURCE. В последнем
Release-прогоне 21 сентября 2026 года все три случая прошли: BODY показал
`elapsed_ms=0` (ниже разрешения `GetTickCount64`), SOURCE — `elapsed_ms=0`;
а `command_state_updates`, `selection_context_builds`,
`toolbar_updates`, `clipboard_checks`, `check_command_calls`, все три
selection-query и `js_com_calls` были равны нулю. `file_fingerprint_checks`
был ограничен единицей throttle. Этот тест включён в `-FullValidation`.

Отдельный `test-fbe-idle-interaction-performance-runtime.ps1` выполняет на
реальном BODY среднего FB2 1 000 перемещений caret, 1 000 изменений
выделения и 10 правок текста через обычные notification handlers, затем 1 000
неизменных `OnIdle`. Последний полный Release-прогон (10 000 абзацев) занял 27 063 мс
для interaction-пакета и дал 2 010 command-state updates — ровно по одному на
реальное событие. В следующем неизменном idle-отрезке все счётчики command,
selection context, toolbar, `CheckCommand()` и JS/COM были равны нулю. Тест
включён в `-FullValidation`.

Тот же interaction-harness был запущен на изолированном baseline `dc043cd1`
(только test-only сценарий, без изменения его production idle-кода): на 10 000
абзацах baseline занял 28 094 мс, текущий Release — 28 031 мс; оба выполнили
ровно 2 010 необходимых command-state updates. Таким образом, ввод текста,
перемещение caret и изменение selection не получили регрессии. Отличие
проявляется после действий: текущий сценарий проверяет 1 000 последующих
неизменных idle с нулевыми UI/COM-счётчиками, тогда как baseline-сравнение
ниже фиксирует 1 000 command/toolbar/fingerprint обновлений за такой же idle
отрезок. Параметр `-Scenario idle-interaction-performance-baseline` у того же
runtime-теста оставлен для воспроизведения baseline-замера вне обычного CI.

### Зафиксированное сравнение idle

Для фактического before/after был собран isolated baseline `dc043cd1` тем же
v143/14.44 toolchain и с тем же pinned `third_party` tree, что у текущего
HEAD. Диагностическая трассировка была включена в обоих запусках. Test-only
harness не меняет `OnIdle`: после загрузки он вызывает существующий `OnIdle()`
ровно 1 000 раз и записывает агрегированные counters. 21 сентября 2026 года
одинаковые fixtures дали следующие результаты:

| Сценарий (1 000 неизменных idle) | Размер FB2 | До: `dc043cd1`, мс | После: `629c5c17`, мс | До: command / toolbar / fingerprint | После: command / toolbar / fingerprint |
| --- | ---: | ---: | ---: | ---: | ---: |
| пустой BODY | 454 Б | 1 891 | 0 | 1 000 / 1 000 / 1 000 | 0 / 0 / 0 |
| малый BODY, 1 000 абзацев | 230 347 Б | 797 | 0 | 1 000 / 1 000 / 1 000 | 0 / 0 / 0 |
| средний BODY, 10 000 абзацев | 2 309 348 Б | 1 000 | 0 | 1 000 / 1 000 / 1 000 | 0 / 0 / 0 |
| средний SOURCE, 10 000 абзацев | 2 309 348 Б | 1 438 | 15 | 1 000 / 1 000 / 1 000 | 0 / 0 / 0 |

Во всех after-строках также равны нулю `selection_context_builds`,
`clipboard_checks`, `check_command_calls` и `js_com_calls`. Старый baseline
ещё не записывал последние два счётчика, поэтому они сознательно не
реконструируются задним числом. `0 ms` означает «меньше разрешения
`GetTickCount64`», а не утверждение о нулевом CPU. Главный наблюдаемый
результат — устранение повторных UI/file/COM операций на неизменном состоянии.

Замер воспроизводится скриптом `tools/tests/measure-fbe-idle-performance.ps1`:
его нужно дважды запустить с одинаковыми параметрами fixture, сначала с
`-Scenario idle-performance-baseline` и собранным baseline, затем с
`-Scenario idle-performance` и текущим `out/Release/FBE.exe`. Два TSV образуют
проверяемый исходный материал отчёта; сам baseline не является частью обычной
CI-сборки.

## Ожидаемые инварианты Phase A

- После стабилизации idle не вызывает command-state, toolbar, tree или
  selection update без соответствующего dirty-события.
- В SOURCE `UIUpdateToolBar` не локализует toolbar на каждом idle-вызове.
- Fingerprint внешнего файла вызывается не чаще одного раза в секунду.
- Состояние bitmap clipboard приходит через `WM_CLIPBOARDUPDATE`; редкий
  polling оставлен лишь как fallback, если WinAPI-регистрация недоступна.
- Selection context — короткоживущий snapshot; DOM-мутация и смена режима
  немедленно делают его недействительным.

## Статус результатов

Автоматически подтверждены контракты инвалидации, throttle, clipboard
listener, selection cache, SOURCE idle, диагностических полей P410 и
стабильного idle на real FBE в BODY и SOURCE. Synthetic runtime-контур также
подтверждает event-driven BODY typing, caret navigation и selection updates.
Получено числовое baseline/current сравнение пустого, малого и среднего BODY,
а также среднего SOURCE idle. Synthetic runtime-контур также покрывает BODY
typing и navigation каретки. Ручные серии BODY/SOURCE-переходов и внешнего
save по протоколу выше остаются отдельной проверкой: они зависят от конкретной
книги, дисплея и Windows/MSHTML окружения.

## Corrective validation

Корректирующий проход Phase A добавляет отдельный `UiDirtyScroll`: реальное
событие прокрутки BODY запускает только `CheckScroll()` spellcheck и не
обновляет command-state, selection context или toolbar. Контракт
`test-fbe-spellcheck-scroll-contract.ps1` проверяет полный путь
MSHTML scroll -> message -> dirty flag -> spellcheck.

Fallback clipboard теперь запускается до ветвления BODY/SOURCE даже при
стабильном `m_ui_dirty`, но инвалидирует paste UI только после фактической
смены доступности `CF_BITMAP`; проверяется
`test-clipboard-listener-contract.ps1`. В DESCRIPTION обновление заголовка
переведено с idle polling на события изменения input; тест
`medium-description` в `test-fbe-idle-performance-runtime.ps1` добавлен к
контур 1 000 стабильных idle. Все эти контракты включены в FAST
`verify-release.ps1`; runtime idle-сценарии остаются частью `-FullValidation`.
