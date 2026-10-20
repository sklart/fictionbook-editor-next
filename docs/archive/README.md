# Исследования и исторические доказательства

В архиве остаются материалы с конкретной технической ценностью. Указанные в них даты, сборки и PASS/FAIL относятся к исходным прогонам; они не подтверждают текущий master. Действующие правила находятся в [основной документации](../README.md), текущие пробелы — в [реестре проверок](../testing/verification-status.md).

| Материал | Основание сохранения |
| --- | --- |
| [diagnostics-modernization-validation](diagnostics/diagnostics-modernization-validation.md) | Конкретные privacy/fault-injection сценарии, session/crash/hang и границы диагностического пакета. |
| [performance-phase-a](performance/performance-phase-a.md) | Измерения повторяемых сценариев производительности, исходные условия и ограничения сравнения. |
| [performance-phase-b](performance/performance-phase-b.md) | Измеренная фиксированная стоимость документа, runtime benchmark и cardinality-проверка. |
| [2026-09-22-script-toolbar-incremental-runtime-design](planning/2026-09-22-script-toolbar-incremental-runtime-design.md) | Техническое решение delta-apply панелей: runtime primitives, rollback и границы состояния. |
| [legacy-todo-residual-verification](planning/legacy-todo-residual-verification.md) | Воспроизводимые проверки Drop/OLE/binary и явные FAIL/ограничения. |
| [3.0.9-before-reorganization](releases/3.0.9-before-reorganization.md) | Оригинальный текст примечаний опубликованного выпуска с отдельными ограничениями предрелизной проверки. |
| [note-creation-undo-stage1-report](script-cards/note-creation-undo-stage1-report.md) | Методика и результаты 94 исторических сценариев для 12 скриптов; связь с защищёнными evidence. |
| [script64-literal-pairs-report](script-cards/script64-literal-pairs-report.md) | Разбор дефекта eval, буквальных замен, коллизий ID, TEXTAREA и воспроизводимые Undo/Redo-проверки. |
| [settings-consolidated](settings/settings-consolidated.md) | Сценарии частичных отказов файловой транзакции, кодирования изображений и границы реальной DPI-матрицы. |
| [property-handler-manual-test](shell/property-handler-manual-test.md) | Развёрнутая воспроизводимая проверка регистрации, published-свойств и Explorer PreviewDetails. |
| [thumbnail-provider-manual-test](shell/thumbnail-provider-manual-test.md) | Матрица ручных проверок миниатюр, повреждённых/отсутствующих обложек и разрядности Explorer. |
| [scintilla-modernization-audit](source-editor/scintilla-modernization-audit.md) | Воспроизводимые microbenchmarks Direct API, wrap/layout, памяти и full-process Source. |
| [spellcheck-goudron-1.0.9-report](spelling/spellcheck-goudron-1.0.9-report.md) | Корпусная методика, покрытия и сравнения словарей с указанием исходных наборов. |
| [html-paste-followup-report](undo/html-paste-followup-report.md) | Причины отказов PostMessage, очереди вставок и сценарии сохранения истории. |
| [html-paste-undo-report](undo/html-paste-undo-report.md) | Причины internal-history/plain-middle и соответствующие проверки настоящего MSHTML Undo. |
| [split-undo-mshtml-probe](undo/split-undo-mshtml-probe.md) | Исследование прямых MSHTML/OLE-последовательностей, отрицательные результаты и техническое решение Split. |
| [undo-atomic-failures-report](undo/undo-atomic-failures-report.md) | Матрица компенсации при COM/регистрационных отказах, предыдущая история и savepoint. |
| [undo-contract-completion-report](undo/undo-contract-completion-report.md) | Проверки completion/cleanup Undo-контракта и границы непроведённого runtime. |
| [manual-test-findings-2026-06-29](validation/manual-test-findings-2026-06-29.md) | Исходные наблюдения ручной проверки и не закрытые автоматически сценарии реальных книг/сред. |
| [script64-before-safe-pairs.json](script-cards/script64-before-safe-pairs.json) | Отрицательный runtime-прогон до исправления: непосредственное доказательство причины дефекта, на которое опирается исследование скрипта 64. |
