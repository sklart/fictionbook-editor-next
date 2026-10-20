# Интеграция документации 20 октября 2026 года

Интеграция архива `FBE-Next-docs-complete-PASS.zip` выполнена поверх актуального master `aa781b6f591980b3199a3a81798607b03364e4f8`. База архива — `1ad7c507d1edcd0ca132589fe5a9f16a5574253d`. Состояние удалённого master проверено через GitHub API перед работой. Отправка в origin не выполнялась.

## Более новые документы и сохранность API

После базы архива в docs изменился только [scripting-api.md](../scripting-api.md). Локальных незакоммиченных правок документации при начале интеграции не было. Более новая редакция master сохранена: API 1.3.0, 88 методов (35 legacy и 53 аддитивных), все описания, примеры, таблицы и оглавление, включая шесть binary-методов DISPID 147–152.

Единственная правка этого файла: ссылка оглавления `#практические-сценарии` заменена на существующий `#практические-задачи-и-готовые-решения`. Обратная замена восстанавливает исходные байты master. IDL, ExternalHelper.cpp, ScriptingApi.inl, ScriptingBinary.inl, ScriptBinaryEdit.h и связанные реализации не изменялись ради документации. Имена, число методов, версия и capabilities сверены с кодом; выполнение примеров и ABI не проверялись.

В новых инструкциях устранены устаревшие 82 метода, добавлена ссылка на binary API и отделён текущий master от опубликованного 3.0.9. Этап 3Б реализован по коду; 3В–3Г остаются планом. Issues #43, #44, #45, #46, #49 и #59 проверены через GitHub API и остаются открытыми.

## Переносы и контрольные данные

Учтены все 124 исходных файла по [карте переносов](file-map.md) и [JSON](file-map.json). Интегрированы user-guide, architecture, reference, testing, plugins, decisions, release-notes, archive и maintenance. Обновлены индексы и ссылки корневого README; ссылки CHANGELOG проверены. Старые опубликованные пути сохранены файлами-указателями, где они нужны навигации.

Все 24 защищённых файла, включая baseline, reviewed proposals, runtime evidence, классификацию Undo, исходный defects Markdown и изображения, сохранили исходные байты. Пять перемещённых JSON также побайтно совпадают с прежними файлами. Исходные SHA-256 не пересчитывались. Удалены из docs только два производных content-audit JSON/HTML; их прежние байты перенесены в игнорируемый out/reports/script-cards, история доступна в Git.

Архивные тексты сохраняют информацию, исходные даты и статусы; добавлены исторические пометки и исправлены ссылки. В двух архивных Markdown удалены конечные пробелы для diff --check. Все 321 отметка выполнения TODO сохранены в архиве. В пакете было заявлено 35 незакрытых пунктов; фактически сохранены все 36 флажков в 35 тематических группах. Счётчик исправлен, статусы задач не менялись.

Изменения исходников дерева документа, уже присутствовавшие в рабочем дереве, исключены из коммита. Во время работы параллельно изменились TreeView.cpp, TreeView.h и RuntimeTestDocumentTree.inl; они не редактировались в рамках интеграции. Все прочие исходники и runtime, включая исполняемые пользовательские скрипты, их комментарии, кодировки, BOM, окончания строк и dependency DLL, сохранили байты начального состояния.

## Инструменты и разрешение расхождений

Оба исходных патча прошли git apply --check. Полная замена docs не выполнялась: файлы интегрированы выборочно по карте, с приоритетом текущего master. Текстовых конфликтов Git не возникло. Семантические расхождения решены сохранением API 1.3.0, актуализацией индексов, исправлением TODO-счётчика и отделением исторических инструкций внешнего сборщика от действующих команд.

Генератор аудита использует Git из PATH, вывод в out/reports/script-cards и относительные HTML-ссылки; запись внутрь docs запрещена. Старые baseline не обходятся. Проверки tools/docs охватывают навигацию, root README/CHANGELOG, API и capabilities. Workflow документации выделен отдельно и учитывает изменения включаемых файлов src/fbe/scripts.

Каталог release notes сверен с 13 опубликованными GitHub Releases: теги, UTC-даты, RC и отсутствие стабильного v3.0.8 согласованы. Конспекты не объявляются буквальной копией body; исходные тексты сохранены. Возможности master после релиза не приписываются старым пакетам.

## Проверки и оставшиеся ограничения

| Проверка | Результат |
| --- | --- |
| tools/docs/doc_links.py --root . | Результат текущего прогона — в [validation.md](validation.md). |
| tools/docs/check_scripting_api.py --root . | PASS; API 1.3.0, 88 методов, версия и capabilities. |
| tools/tests/test-docs-tools.py | PASS; 16 адресных тестов парсеров и изолированного вывода аудита. |
| tools/tests/test-audit-script-cards.py | PASS; карточки, 91 запись классификации и согласованность script64 evidence. |
| tools/tests/test-fbe-xml-scripting-api-docs.ps1 | PASS; статический контракт XML и binary API. |
| tools/tests/test-note-creation-evidence.py | FAIL до интеграции и после неё: исторический SHA RuntimeTestPortableState.inl не совпадает с HEAD. |
| Реальный запуск tools/audit-script-cards.py | FAIL до интеграции и после неё: текущий исполняемый код/кодировки/EOL расходятся с историческим baseline. |
| Проверка сохранности | PASS; 124 источника учтены, 24 защищённых файла и 5 перенесённых JSON сохранены побайтно. |
| YAML workflow и его команды | Локальная структура YAML, три команды и границы прав проверены; удалённый CI NOT RUN. |
| git diff --check | Результат после подготовки коммита — в validation.md. |
| Windows/GUI/COM/runtime, запуск примеров, verify-release.ps1 | NOT RUN; приложение и сборочные/упаковочные границы не изменялись. |

Расхождение note evidence подтверждено в отдельном снимке HEAD с корректными checkout EOL пользовательских скриптов. Ожидаемый исторический SHA RuntimeTestPortableState.inl: `2c32539e9e78b4614f655dc1bc3cf70c605309ffb6aa15999556e41cd2485ad2`; SHA Git-объекта HEAD: `8e69dd5369c19149c0f5cbc972069a6ed4fe80623766e43d63d9e2409a0c80d3`. Это не новый runtime-отказ и не основание менять сохранённое доказательство. Исходный генератор из HEAD также останавливается на baseline до формирования отчётов.

Для нового PASS этих двух проверок нужна отдельная сверка эволюции исходников с историческими доказательствами и, где требуется, адресный повторный runtime-прогон. В этой интеграции контрольные хеши и исходные тесты не изменены для сокрытия расхождения. Совместимость реальных книг, Windows 7, GUI/DPI и корректность всех примеров остаются за границами статической проверки.

## Полный перечень файлов интеграции

Список относительно aa781b6f. A — создание, M — обновление/объединение, R — перенос, D — удаление производного. Смысловые связи объединённых документов раскрыты в [карте](file-map.md).

| Действие | Файл |
| --- | --- |
| A | [.github/workflows/docs.yml](../../.github/workflows/docs.yml) |
| M | [README.md](../../README.md) |
| M | [docs/README.md](../README.md) |
| A | [docs/architecture/README.md](../architecture/README.md) |
| A | [docs/architecture/diagnostics.md](../architecture/diagnostics.md) |
| A | [docs/architecture/performance.md](../architecture/performance.md) |
| A | [docs/architecture/settings.md](../architecture/settings.md) |
| M | [docs/architecture/split-undo-mshtml-probe.md](../architecture/split-undo-mshtml-probe.md) |
| M | [docs/architecture/structural-commands.md](../architecture/structural-commands.md) |
| A | [docs/architecture/undo.md](../architecture/undo.md) |
| M | [docs/architecture/verification-status.md](../architecture/verification-status.md) |
| A | [docs/archive/README.md](../archive/README.md) |
| A | [docs/archive/building/README.md](../archive/building/README.md) |
| A | [docs/archive/building/building-before-reorganization.md](../archive/building/building-before-reorganization.md) |
| A | [docs/archive/building/release-checklist-before-reorganization.md](../archive/building/release-checklist-before-reorganization.md) |
| A | [docs/archive/building/versioning-before-reorganization.md](../archive/building/versioning-before-reorganization.md) |
| A | [docs/archive/diagnostics/README.md](../archive/diagnostics/README.md) |
| A | [docs/archive/diagnostics/diagnostics-modernization-validation.md](../archive/diagnostics/diagnostics-modernization-validation.md) |
| A | [docs/archive/performance/README.md](../archive/performance/README.md) |
| A | [docs/archive/performance/performance-phase-a.md](../archive/performance/performance-phase-a.md) |
| A | [docs/archive/performance/performance-phase-b.md](../archive/performance/performance-phase-b.md) |
| A | [docs/archive/planning/2026-09-22-script-toolbar-incremental-runtime-design.md](../archive/planning/2026-09-22-script-toolbar-incremental-runtime-design.md) |
| A | [docs/archive/planning/README.md](../archive/planning/README.md) |
| A | [docs/archive/planning/index-before-reorganization.md](../archive/planning/index-before-reorganization.md) |
| A | [docs/archive/planning/legacy-todo-residual-verification.md](../archive/planning/legacy-todo-residual-verification.md) |
| A | [docs/archive/planning/legacy-todo.txt](../archive/planning/legacy-todo.txt) |
| A | [docs/archive/planning/todo-before-reorganization.md](../archive/planning/todo-before-reorganization.md) |
| A | [docs/archive/releases/3.0.9-before-reorganization.md](../archive/releases/3.0.9-before-reorganization.md) |
| A | [docs/archive/releases/README.md](../archive/releases/README.md) |
| A | [docs/archive/script-cards/README.md](../archive/script-cards/README.md) |
| A | [docs/archive/script-cards/note-creation-undo-stage1-report.md](../archive/script-cards/note-creation-undo-stage1-report.md) |
| A | [docs/archive/script-cards/script-card-completion-audit.md](../archive/script-cards/script-card-completion-audit.md) |
| R100 | `docs/script-card-executable-repairs.json` → [docs/archive/script-cards/script-card-executable-repairs.json](../archive/script-cards/script-card-executable-repairs.json) |
| R100 | `docs/script-card-final-text-fixes.json` → [docs/archive/script-cards/script-card-final-text-fixes.json](../archive/script-cards/script-card-final-text-fixes.json) |
| R100 | `docs/script-card-followup-proposals.json` → [docs/archive/script-cards/script-card-followup-proposals.json](../archive/script-cards/script-card-followup-proposals.json) |
| A | [docs/archive/script-cards/script-card-followup-report.md](../archive/script-cards/script-card-followup-report.md) |
| A | [docs/archive/script-cards/script-card-migration-audit.md](../archive/script-cards/script-card-migration-audit.md) |
| R100 | `docs/script-card-refinement-proposals.json` → [docs/archive/script-cards/script-card-refinement-proposals.json](../archive/script-cards/script-card-refinement-proposals.json) |
| A | [docs/archive/script-cards/script-card-refinement-report.md](../archive/script-cards/script-card-refinement-report.md) |
| A | [docs/archive/script-cards/script-card-stage-verification.md](../archive/script-cards/script-card-stage-verification.md) |
| A | [docs/archive/script-cards/script-card-undo-classification.md](../archive/script-cards/script-card-undo-classification.md) |
| A | [docs/archive/script-cards/script-card-undo-next-steps.md](../archive/script-cards/script-card-undo-next-steps.md) |
| R100 | `docs/script64-before-safe-pairs.json` → [docs/archive/script-cards/script64-before-safe-pairs.json](../archive/script-cards/script64-before-safe-pairs.json) |
| A | [docs/archive/script-cards/script64-literal-pairs-report.md](../archive/script-cards/script64-literal-pairs-report.md) |
| A | [docs/archive/script-cards/script64-safe-pairs-proposal.md](../archive/script-cards/script64-safe-pairs-proposal.md) |
| A | [docs/archive/settings/README.md](../archive/settings/README.md) |
| A | [docs/archive/settings/settings-consolidated.md](../archive/settings/settings-consolidated.md) |
| A | [docs/archive/shell/README.md](../archive/shell/README.md) |
| A | [docs/archive/shell/fbshell-roadmap.md](../archive/shell/fbshell-roadmap.md) |
| A | [docs/archive/shell/property-handler-manual-test.md](../archive/shell/property-handler-manual-test.md) |
| A | [docs/archive/shell/property-handler.md](../archive/shell/property-handler.md) |
| A | [docs/archive/shell/sequence-property-schema-plan.md](../archive/shell/sequence-property-schema-plan.md) |
| A | [docs/archive/shell/sequence-property-schema.md](../archive/shell/sequence-property-schema.md) |
| A | [docs/archive/shell/shell-extension-modernization.md](../archive/shell/shell-extension-modernization.md) |
| A | [docs/archive/shell/thumbnail-provider-manual-test.md](../archive/shell/thumbnail-provider-manual-test.md) |
| A | [docs/archive/shell/thumbnail-provider-plan.md](../archive/shell/thumbnail-provider-plan.md) |
| A | [docs/archive/source-editor/README.md](../archive/source-editor/README.md) |
| A | [docs/archive/source-editor/scintilla-modernization-audit.md](../archive/source-editor/scintilla-modernization-audit.md) |
| A | [docs/archive/spelling/README.md](../archive/spelling/README.md) |
| A | [docs/archive/spelling/spellcheck-goudron-1.0.9-report.md](../archive/spelling/spellcheck-goudron-1.0.9-report.md) |
| A | [docs/archive/undo/README.md](../archive/undo/README.md) |
| A | [docs/archive/undo/html-paste-followup-report.md](../archive/undo/html-paste-followup-report.md) |
| A | [docs/archive/undo/html-paste-undo-report.md](../archive/undo/html-paste-undo-report.md) |
| A | [docs/archive/undo/split-undo-mshtml-probe.md](../archive/undo/split-undo-mshtml-probe.md) |
| A | [docs/archive/undo/undo-atomic-failures-report.md](../archive/undo/undo-atomic-failures-report.md) |
| A | [docs/archive/undo/undo-contract-completion-report.md](../archive/undo/undo-contract-completion-report.md) |
| A | [docs/archive/upstream/README.md](../archive/upstream/README.md) |
| A | [docs/archive/upstream/technical-changes-from-sensboston.md](../archive/upstream/technical-changes-from-sensboston.md) |
| A | [docs/archive/upstream/upstream-evpobr-analysis.md](../archive/upstream/upstream-evpobr-analysis.md) |
| A | [docs/archive/validation/README.md](../archive/validation/README.md) |
| A | [docs/archive/validation/manual-test-findings-2026-06-29.md](../archive/validation/manual-test-findings-2026-06-29.md) |
| A | [docs/archive/validation/manual-test-plan-before-reorganization.md](../archive/validation/manual-test-plan-before-reorganization.md) |
| A | [docs/archive/validation/test-contours-before-reorganization.md](../archive/validation/test-contours-before-reorganization.md) |
| A | [docs/archive/validation/verification-status-before-reorganization.md](../archive/validation/verification-status-before-reorganization.md) |
| M | [docs/building.md](../building.md) |
| M | [docs/decisions/0002-shared-fb2-shell-sources.md](../decisions/0002-shared-fb2-shell-sources.md) |
| M | [docs/diagnostics-modernization-validation.md](../diagnostics-modernization-validation.md) |
| M | [docs/fb2-property-mapping.md](../fb2-property-mapping.md) |
| M | [docs/fbelib-typelib-versioning.md](../fbelib-typelib-versioning.md) |
| M | [docs/fbshell-roadmap.md](../fbshell-roadmap.md) |
| M | [docs/html-paste-compatibility-smoke.md](../html-paste-compatibility-smoke.md) |
| M | [docs/html-paste-followup-report.md](../html-paste-followup-report.md) |
| M | [docs/html-paste-undo-report.md](../html-paste-undo-report.md) |
| M | [docs/installer-shell-maintenance.md](../installer-shell-maintenance.md) |
| M | [docs/issue-43-stage-report.md](../issue-43-stage-report.md) |
| M | [docs/issue-49-stage-report.md](../issue-49-stage-report.md) |
| M | [docs/legacy-todo-residual-verification.md](../legacy-todo-residual-verification.md) |
| M | [docs/legacy-todo.txt](../legacy-todo.txt) |
| M | [docs/localization.md](../localization.md) |
| A | [docs/maintenance/README.md](README.md) |
| A | [docs/maintenance/file-map.json](file-map.json) |
| A | [docs/maintenance/file-map.md](file-map.md) |
| A | [docs/maintenance/integration-2026-10-20.md](integration-2026-10-20.md) |
| A | [docs/maintenance/migration.md](migration.md) |
| A | [docs/maintenance/policy.md](policy.md) |
| A | [docs/maintenance/protected-evidence.md](protected-evidence.md) |
| A | [docs/maintenance/structure.md](structure.md) |
| A | [docs/maintenance/validation.md](validation.md) |
| M | [docs/manual-test-findings-2026-06-29.md](../manual-test-findings-2026-06-29.md) |
| M | [docs/manual-test-plan.md](../manual-test-plan.md) |
| M | [docs/mshtml-image-selection-scrollbar-regression.md](../mshtml-image-selection-scrollbar-regression.md) |
| M | [docs/note-creation-undo-stage1-report.md](../note-creation-undo-stage1-report.md) |
| M | [docs/pcre2-compatibility.md](../pcre2-compatibility.md) |
| M | [docs/performance-phase-a.md](../performance-phase-a.md) |
| M | [docs/performance-phase-b.md](../performance-phase-b.md) |
| A | [docs/plugins/README.md](../plugins/README.md) |
| A | [docs/plugins/export-html.md](../plugins/export-html.md) |
| M | [docs/property-handler-manual-test.md](../property-handler-manual-test.md) |
| M | [docs/property-handler.md](../property-handler.md) |
| A | [docs/reference/README.md](../reference/README.md) |
| A | [docs/reference/build-tooling.md](../reference/build-tooling.md) |
| A | [docs/reference/diagnostics.md](../reference/diagnostics.md) |
| A | [docs/reference/fb2-property-mapping.md](../reference/fb2-property-mapping.md) |
| A | [docs/reference/fbelib-typelib-versioning.md](../reference/fbelib-typelib-versioning.md) |
| A | [docs/reference/installer-shell-maintenance.md](../reference/installer-shell-maintenance.md) |
| A | [docs/reference/localization.md](../reference/localization.md) |
| A | [docs/reference/pcre2-compatibility.md](../reference/pcre2-compatibility.md) |
| A | [docs/reference/russian-yo-usage.md](../reference/russian-yo-usage.md) |
| A | [docs/reference/sequence-property-schema.md](../reference/sequence-property-schema.md) |
| A | [docs/reference/thumbnail-provider.md](../reference/thumbnail-provider.md) |
| A | [docs/reference/translation-workflow.md](../reference/translation-workflow.md) |
| A | [docs/reference/xml-source-theme-sources.md](../reference/xml-source-theme-sources.md) |
| A | [docs/reference/xml-source-themes.md](../reference/xml-source-themes.md) |
| M | [docs/regex-fixtures.md](../regex-fixtures.md) |
| M | [docs/regex-regression.md](../regex-regression.md) |
| M | [docs/release-checklist.md](../release-checklist.md) |
| M | [docs/release-notes/3.0.7-rc.1.md](../release-notes/3.0.7-rc.1.md) |
| A | [docs/release-notes/3.0.8-rc.1.md](../release-notes/3.0.8-rc.1.md) |
| A | [docs/release-notes/3.0.8-rc2.md](../release-notes/3.0.8-rc2.md) |
| M | [docs/release-notes/3.0.8.md](../release-notes/3.0.8.md) |
| M | [docs/release-notes/3.0.9.md](../release-notes/3.0.9.md) |
| A | [docs/release-notes/README.md](../release-notes/README.md) |
| M | [docs/reporting-issues.md](../reporting-issues.md) |
| M | [docs/russian-yo-usage.md](../russian-yo-usage.md) |
| M | [docs/scintilla-directwrite-manual-checklist.md](../scintilla-directwrite-manual-checklist.md) |
| M | [docs/scintilla-manual-verification-checklist.md](../scintilla-manual-verification-checklist.md) |
| M | [docs/scintilla-modernization-audit.md](../scintilla-modernization-audit.md) |
| A | [docs/screenshots/README.md](../screenshots/README.md) |
| M | [docs/script-card-completion-audit.md](../script-card-completion-audit.md) |
| D | `docs/script-card-content-audit.html` |
| D | `docs/script-card-content-audit.json` |
| M | [docs/script-card-content-audit.md](../script-card-content-audit.md) |
| M | [docs/script-card-followup-report.md](../script-card-followup-report.md) |
| M | [docs/script-card-migration-audit.md](../script-card-migration-audit.md) |
| M | [docs/script-card-refinement-report.md](../script-card-refinement-report.md) |
| M | [docs/script-card-stage-verification.md](../script-card-stage-verification.md) |
| M | [docs/script-card-undo-classification.md](../script-card-undo-classification.md) |
| M | [docs/script-card-undo-next-steps.md](../script-card-undo-next-steps.md) |
| A | [docs/script-cards.md](../script-cards.md) |
| M | [docs/script64-literal-pairs-report.md](../script64-literal-pairs-report.md) |
| M | [docs/script64-safe-pairs-proposal.md](../script64-safe-pairs-proposal.md) |
| M | [docs/scripting-api.md](../scripting-api.md) |
| M | [docs/sequence-property-schema-plan.md](../sequence-property-schema-plan.md) |
| M | [docs/sequence-property-schema.md](../sequence-property-schema.md) |
| M | [docs/settings-errors-nbsp-report.txt](../settings-errors-nbsp-report.txt) |
| M | [docs/settings-followup-report.txt](../settings-followup-report.txt) |
| M | [docs/settings-unification-report.txt](../settings-unification-report.txt) |
| M | [docs/shell-extension-modernization.md](../shell-extension-modernization.md) |
| A | [docs/shell-integration.md](../shell-integration.md) |
| M | [docs/spellcheck-goudron-1.0.9-report.md](../spellcheck-goudron-1.0.9-report.md) |
| M | [docs/spellcheck-regression.md](../spellcheck-regression.md) |
| M | [docs/superpowers/specs/2026-09-22-script-toolbar-incremental-runtime-design.md](../superpowers/specs/2026-09-22-script-toolbar-incremental-runtime-design.md) |
| M | [docs/technical-changes-from-sensboston.md](../technical-changes-from-sensboston.md) |
| M | [docs/test-contours.md](../test-contours.md) |
| A | [docs/testing/README.md](../testing/README.md) |
| A | [docs/testing/evidence.md](../testing/evidence.md) |
| A | [docs/testing/html-paste.md](../testing/html-paste.md) |
| A | [docs/testing/image-selection-scrollbar.md](../testing/image-selection-scrollbar.md) |
| A | [docs/testing/manual-regression.md](../testing/manual-regression.md) |
| A | [docs/testing/regex-fixtures.md](../testing/regex-fixtures.md) |
| A | [docs/testing/regex-regression.md](../testing/regex-regression.md) |
| A | [docs/testing/reports/README.md](../testing/reports/README.md) |
| A | [docs/testing/reports/issue-43-stage-report.md](../testing/reports/issue-43-stage-report.md) |
| A | [docs/testing/reports/issue-49-stage-report.md](../testing/reports/issue-49-stage-report.md) |
| A | [docs/testing/shell-integration.md](../testing/shell-integration.md) |
| A | [docs/testing/source-directwrite.md](../testing/source-directwrite.md) |
| A | [docs/testing/source-editor.md](../testing/source-editor.md) |
| A | [docs/testing/spellcheck.md](../testing/spellcheck.md) |
| A | [docs/testing/verification-status.md](../testing/verification-status.md) |
| M | [docs/thumbnail-provider-manual-test.md](../thumbnail-provider-manual-test.md) |
| M | [docs/thumbnail-provider-plan.md](../thumbnail-provider-plan.md) |
| M | [docs/todo.md](../todo.md) |
| M | [docs/undo-atomic-failures-report.md](../undo-atomic-failures-report.md) |
| M | [docs/undo-contract-completion-report.md](../undo-contract-completion-report.md) |
| M | [docs/upstream-evpobr-analysis.md](../upstream-evpobr-analysis.md) |
| A | [docs/user-guide/README.md](../user-guide/README.md) |
| A | [docs/user-guide/archives.md](../user-guide/archives.md) |
| A | [docs/user-guide/compatibility.md](../user-guide/compatibility.md) |
| A | [docs/user-guide/fb2-quality.md](../user-guide/fb2-quality.md) |
| A | [docs/user-guide/getting-started.md](../user-guide/getting-started.md) |
| A | [docs/user-guide/recovery-and-backups.md](../user-guide/recovery-and-backups.md) |
| A | [docs/user-guide/scripts-and-toolbars.md](../user-guide/scripts-and-toolbars.md) |
| A | [docs/user-guide/search-and-replace.md](../user-guide/search-and-replace.md) |
| A | [docs/user-guide/settings.md](../user-guide/settings.md) |
| M | [docs/versioning.md](../versioning.md) |
| M | [docs/xml-source-theme-sources.md](../xml-source-theme-sources.md) |
| M | [docs/xml-source-themes.md](../xml-source-themes.md) |
| M | [tools/audit-script-cards.py](../../tools/audit-script-cards.py) |
| A | [tools/docs/.gitignore](../../tools/docs/.gitignore) |
| A | [tools/docs/README.md](../../tools/docs/README.md) |
| A | [tools/docs/check_scripting_api.py](../../tools/docs/check_scripting_api.py) |
| A | [tools/docs/doc_links.py](../../tools/docs/doc_links.py) |
| A | [tools/tests/test-docs-tools.py](../../tools/tests/test-docs-tools.py) |
