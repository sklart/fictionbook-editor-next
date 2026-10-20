# Сервисные инструменты зависимостей

> Отдельная справка о служебных сценариях. Базовые команды сборки находятся в [building.md](../building.md).

Для контроля upstream-обновлений исходников `Scintilla`, `Lexilla`, `PCRE2`
и `Hunspell`
добавлены отдельные сервисные сценарии:

```powershell
.\tools\build\check-third-party-updates.ps1
.\tools\build\download-third-party-updates.ps1
.\tools\build\apply-third-party-update.ps1 -Dependency scintilla
.\tools\build\apply-third-party-update-and-test.ps1 -Dependency pcre2
.\tools\build\build-hunspell.ps1
.\tools\build\update-third-party-dependency.ps1 -Dependency aom
```

Первый сценарий показывает локальную и удалённую версии и сообщает, где
доступно обновление. Второй скачивает ZIP-архивы исходников в
`tmp\third-party-updates` и раскладывает их по отдельным каталогам вида
`<dependency>-<version>` вместе с `download-metadata.json`.

При необходимости можно ограничить набор зависимостей:

```powershell
.\tools\build\check-third-party-updates.ps1 -Dependency scintilla,lexilla
.\tools\build\download-third-party-updates.ps1 -Dependency pcre2,hunspell
```

По умолчанию скачиваются только реально более новые версии. Если нужно
принудительно скачать даже уже текущую версию, используйте
`-AllowCurrentVersion`.

Сценарий `apply-third-party-update.ps1` берёт уже скачанный каталог из
`tmp\third-party-updates` (или явный `-SourcePath`), валидирует структуру
исходников, копирует новый вариант в staging, переносит текущую копию
`third_party\<dependency>` в backup и только после этого подменяет рабочий
каталог. Для безопасной примерки используйте `-WhatIf`.

Сценарий `apply-third-party-update-and-test.ps1` автоматизирует следующий шаг:
после раскладки обновления он запускает правильный build/test-контур для этой
зависимости. Сейчас поддерживаются:

- `scintilla` / `lexilla` -> `build-scintilla.ps1` + `test-scintilla.ps1`;
- `pcre2` -> `build-pcre2.ps1` + `test-pcre2.ps1` +
  `test-pcre2-wrapper.ps1` + `test-pcre2-replace.ps1`;
- `hunspell` -> `build-hunspell.ps1` + `test-spellcheck-dictionaries.ps1`.

Для безопасной dry-run проверки orchestration-потока также можно использовать
`-WhatIf`.

Если нужен единый вход на весь цикл `check -> download -> apply -> build ->
test`, используйте:

```powershell
.\tools\build\update-third-party-dependency.ps1 -Dependency scintilla
.\tools\build\update-third-party-dependency.ps1 -Dependency lexilla
.\tools\build\update-third-party-dependency.ps1 -Dependency pcre2
.\tools\build\update-third-party-dependency.ps1 -Dependency hunspell
```

Этот сценарий сам:

- сверяет локальную и upstream-версии;
- при необходимости скачивает ZIP-архив в `tmp\third-party-updates`;
- передаёт распакованный каталог в `apply-third-party-update-and-test.ps1`;
- запускает правильный build/test-контур для выбранной зависимости.

Полезные флаги:

- `-AllowCurrentVersion` — прогнать тот же цикл даже без новой upstream-версии;
- `-ForceDownload` — перескачать архив заново;
- `-ForceApply` — разрешить повторную раскладку даже для той же версии;
- `-WhatIf` — безопасно посмотреть, что именно сценарий собирается сделать.


## Ограничения

Обновление зависимости — отдельная задача, а не часть реорганизации документации. Не повышайте baseline VC Tools/SDK/CRT и не меняйте vendor-исходники автоматически. Для изменений, затрагивающих Windows 7, нужны проверки импортов и реальный runtime-прогон на целевой системе.
