# DirectWrite и layout threads: ручная проверка

Этот чек-лист обязателен до включения `SC_TECHNOLOGY_DIRECTWRITE` в FBE по умолчанию. Автоматический smoke подтверждает доступность API, но не заменяет визуальную проверку.

## Матрица

Проверить на Windows 7 SP1, Windows 10 и Windows 11, при DPI 100%, 125%, 150% и 200%, в Light, Dark и High Contrast.

Для каждого варианта проверить:

- кириллицу, italic и bold в Source;
- caret, selection и current-line highlighting;
- line-number и folding margins;
- XML indicators и EOL validation annotation;
- wrap длинных абзацев, resize окна и scroll;
- переходы Body ↔ Source.

## Layout benchmark

Для isolated Scintilla measurement запустить `tools/tests/test-scintilla.ps1 -EditorRuntimeDirectory .\\out\\editor-runtime\\Modern -RunLayoutBenchmark`. Harness создаёт XML 1/5 MB, делает warm-up и семь измерений wrap/reflow при resize для default, DirectWrite+1 и (при `SCI_SUPPORTSFEATURE`) DirectWrite+2 threads. 20 MB — отдельный длительный прогон с `-RunLayoutBenchmarkLarge`, чтобы не блокировать обычную диагностику. Затем подтвердить результат тем же сценарием в FBE: открытие Source, resize, scroll и переключение Body/Source. Включать threads только если median не ухудшается и память Win32 остаётся приемлемой.

## Результат

До заполнения этой матрицы FBE остаётся на `SC_TECHNOLOGY_DEFAULT` и с однопоточным layout, даже если runtime API сообщает поддержку.
