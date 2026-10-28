# Split/Undo: техническое заключение для приёмки 3.0.10

База: `9cb126e5eeae26fb802d6a91c3dcd3c890f6461e`. Строгий `split-section-middle` на чистой Release Win32 сборке и на сохранённой сборке `fc60a33a` завершается FAIL с `before_equals_undo=0`. Исходные TSV и structural trace сохранены локально в `build/evidence/split-current.log`, `build/evidence/split-baseline.log` и указанных там каталогах. Тест не изменён и gate не отключён.

Отдельный test-only probe `test-fbe-split-undo-timeline.ps1` выполняет те же реальные MSHTML операции и фиксирует DOM, выделение, dirty и размеры OLE Undo/Redo. Это наблюдение, а не новый критерий PASS. При Split секции с ID и обратной ссылкой:

1. После Split две секции содержат ожидаемые фрагменты, ID перенесён во вторую.
2. Первый Undo восстанавливает исходный текст в первой секции, но вторая остаётся; в обеих секциях одинаковый ID. Undo-стек уже пуст.
3. Ещё 11 вызовов Undo не восстанавливают исходный DOM. Redo возвращает DOM после Split.
4. В последовательности `Split → ввод X` первый Undo удаляет X, второй оставляет описанный дубликат; дальнейшие Undo не восстанавливают исходное состояние. Redo возвращает измеренное состояние с X. Не утверждается, что промежуточный DOM пригоден для сохранения или редактирования.

Полные TSV и trace для базового и текущего бинарников, а также двух timeline-сценариев сохранены в [docs/evidence/split-undo-3010](evidence/split-undo-3010/). Данные относятся к синтетической книге; локальный путь runtime заменён постоянным маркером. Хеши бинарников и состав instrumentation записаны в `identity.json`. После Redo результат Split на исходной сборке сохранён и повторно открыт: две секции, один ID, одна обратная ссылка, XSD PASS; это не исправляет неверное состояние после Undo.

Все 16 строгих сценариев запущены отдельно, чтобы первый отказ не скрывал остальные: 13 PASS, 3 FAIL (`split-section-middle`, `split-wrapper-success`, `split-section-multi-paragraph-selection`). Во всех трёх `before_equals_undo=0`; данные после Split и Redo сохранены. Локальная матрица и полные артефакты — `build/evidence/split-matrix.json` и соответствующие логи. Статус блока: **BLOCKED**, а не PASS.

Отдельные результаты и структурные трассы каждого отказа: [split-section-middle.tsv](evidence/split-undo-3010/split-section-middle.tsv), [trace](evidence/split-undo-3010/split-section-middle.trace.tsv); [split-wrapper-success.tsv](evidence/split-undo-3010/split-wrapper-success.tsv), [trace](evidence/split-undo-3010/split-wrapper-success.trace.tsv); [split-section-multi-paragraph-selection.tsv](evidence/split-undo-3010/split-section-multi-paragraph-selection.tsv), [trace](evidence/split-undo-3010/split-section-multi-paragraph-selection.trace.tsv). Это синтетические документы; локальные пути к runtime нормализованы. Значения тестов не изменены.

Это подтверждённый функциональный дефект: штатная последовательность исчерпывает Undo и не удаляет второй узел, а дублирующийся ID нарушает целостность документа. Историческое отсутствие гарантии одного Ctrl+Z не делает это поведение допустимым. Архивный [probe](archive/undo/split-undo-mshtml-probe.md) исследовал операции вставки, `MarkupServices` и OLE parent; прежний command-specific prototype восстанавливал DOM одним Undo, однако ломал Redo последующего ввода.

Решение владельца от 28.10.2026: оставить Split/Undo **BLOCKED**, сохранить строгий gate и продолжить независимые проверки EPUB. Глобальный механизм Undo/Redo и ранее отвергнутый прототип не изменяются. Отдельное ТЗ на последующее исследование: [split-undo-safe-fix-spec.md](split-undo-safe-fix-spec.md).

Рассмотренные варианты:

- Оставить строгий gate и Split как **BLOCKED**, продолжив независимые исправления EPUB и проверку остальных подсистем.
- Разрешить отдельное исследование архитектуры Undo/Redo с оценкой влияния на другие команды, сохранность истории и откатом при нарушении смешанного сценария.
- Рассмотреть изменение контракта только после решения о приемлемой пользовательской семантике и проверки сохранения данных; существующее наблюдение дублирования ID не даёт оснований считать проблему решённой или просто ослабить текущий тест.
