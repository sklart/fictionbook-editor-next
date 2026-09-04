# Тестовые контуры

Проект собирает один универсальный Win32 runtime для Windows 7 SP1+; отдельных Modern/Win7 деревьев, batch-профилей и ArchHandler артефактов нет.

## FAST

Обычный `tools/build/verify-release.ps1` — обязательный PR/master gate. Он проверяет staging/layout `out\Release` и `out\Release\Plugins`, безопасность PE и импорты Win7, локализацию, dictionaries, PCRE2, batch-конвертеры и контракты редактора. В него также входят дешёвые table contracts `test-fbe-table-visual-mode.ps1` и `test-table-toolbar-contract.ps1`, а также contracts scripting document path, backup settings, AutoUrlDetect, XML themes, current line, filename state и XML declaration.

Локальный запуск: `pwsh ./tools/build/verify-release.ps1 -Configuration Release`.

## FULL

`-FullValidation` добавляет настоящие GUI/production round-trip, huge binary и table fixtures, structural table matrix, toolbar rendering, performance, fault-injection, portable isolation и stress tests. В этом контуре выполняется `test-fbe-spellcheck-local-edit-performance.ps1`: реальный FBE редактирует последний абзац длинной section, а test-only diagnostic counter доказывает bounded work spellcheck.

Локальный запуск: `pwsh ./tools/build/verify-release.ps1 -Configuration Release -FullValidation`.

`-RunTableTests` запускает только тяжёлый table subset без остальных FULL сценариев: `pwsh ./tools/build/verify-release.ps1 -Configuration Release -RunTableTests`.

## Tables

FAST содержит статические/transform contracts. FULL содержит toolbar rendering и production round-trip (включая Huge), structural operations, performance, failure safety и fault injection. Ни один table test не подавляет ошибку: toolbar rendering является строгим blocker.

Отдельный toolbar smoke: `pwsh ./tools/tests/test-fbe-table-toolbar-rendering.ps1 -FbeExe ./out/Release/FBE.exe`.

## CI-special

Workflow отдельно строит и проверяет ArchHandler из `out\archhandler\Win32\Release`: `test-archhandler-pe-contract.ps1` читает фактический PE32 GUI artifact, VERSIONINFO, embedded asInvoker manifest, ASLR и DEP. Installer upgrade/uninstall, shell/property-handler и keyboard-layout native checks также вызываются специализированными workflow steps.

Локальный PE contract: `pwsh ./tools/tests/test-archhandler-pe-contract.ps1 -HandlerDirectory ./out/archhandler/Win32/Release`.

## Installer, shell, portable и plugins

Installer/shell tests используют staged package, а portable tests проверяют изоляцию Settings/registry/copies. Plugins всегда проверяются в современном `Plugins` layout; ImportEPUB batch smoke загружает `Plugins\ImportEPUB.dll`.

## Manual/diagnostic

`test-diagnostics.ps1`, shell thumbnail dump/prime, отдельные fixture inspectors и corpus tools — диагностические входы, а не неявное покрытие release gate. Перед удалением такого файла следует проверить его ссылки в PowerShell wrappers, workflow и документации.

Файловая backup regression запускается отдельно: `pwsh ./tools/tests/test-fbe-backup-file-commit.ps1`.

## Карта и self-test

`test-release-pipeline-deduplication.ps1` закрепляет единый pipeline, отсутствие устаревшей Modern/Win7 матрицы и обязательные FAST/FULL контуры. Полный список активных release tests является последовательностью вызовов в `tools/build/verify-release.ps1`; наличие файла само по себе не означает, что он входит в gate.
