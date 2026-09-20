# Производительность редактора: Phase A

## Что измеряется

Диагностический журнал FBE Next (только при `FBE_NEXT_TRACE=1` или включённой
пользовательской настройке) пишет агрегированное событие `performance/P410` на
каждые 256 вызовов `CMainFrame::OnIdle`. В штатном режиме этот счётчик и
измерения полностью выключены.

Событие содержит число вызовов, суммарное, максимальное и среднее время idle,
а также отдельные суммы для command state, selection context, toolbar, tree,
проверки fingerprint файла, spellcheck и заголовка. В нём также есть число
JS/COM dispatch (`js-com-calls`) и счётчики обновлений команд, selection,
toolbar, tree, файла и clipboard. Журнал не включает текст книги и пути к ней.

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
   `%LOCALAPPDATA%\FBE Next\fbe-trace*.log` только данного PID.
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
event-driven update и затем 1 000 неизменных `OnIdle`. В Release-прогоне
21 сентября 2026 года оба fixture прошли: `elapsed_ms=0` (ниже разрешения
`GetTickCount64`), а `command_state_updates`, `selection_context_builds`,
`toolbar_updates`, `clipboard_checks`, `check_command_calls`, все три
selection-query и `js_com_calls` были равны нулю. `file_fingerprint_checks`
был ограничен единицей throttle. Этот тест включён в `-FullValidation`.

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
стабильного idle на real FBE. Исторический baseline `dc043cd1` не удалось
собрать в этом automation-окружении: созданный checkout не содержал pinned
submodules, а их восстановление остановилось на DNS для upstream aom/libwebp.
Поэтому before/after цифры не подменены предположениями. Реальные шесть
интерактивных запусков по протоколу выше остаются отдельной ручной проверкой,
для которой нужен доступный baseline checkout и конкретная книга, дисплей и
Windows/MSHTML окружение.
