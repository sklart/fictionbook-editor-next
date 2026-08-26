# Чек-лист релиза

Короткая памятка для локального выпуска новой версии FBE.

## Перед началом

1. Проверить актуальную версию в [src/version.h](D:\Download\FBeditor\src\version.h).
2. Убедиться, что рабочее дерево в ожидаемом состоянии и случайные временные
   файлы не попадут в релиз.
3. При изменениях в поиске, замене, `Scintilla`, `Lexilla`, `PCRE2` или
   орфографии отдельно просмотреть:
   - [docs/regex-regression.md](D:\Download\FBeditor\docs\regex-regression.md);
   - [docs/spellcheck-regression.md](D:\Download\FBeditor\docs\spellcheck-regression.md).
4. Если релиз затрагивает shell-интеграцию, installer helper-скрипты,
   `FBShell`, `FBV` или состав `tools/tests`, свериться с
   [docs/test-contours.md](D:\Download\FBeditor\docs\test-contours.md),
   чтобы не пропустить shell/thumbnail release-gate и не удалить нужный
   диагностический сценарий.

## Обязательные локальные проверки

1. Прогнать основную проверку релиза:

```powershell
.\tools\build\verify-release.ps1 -Configuration Release
```

Table-regression suite намеренно opt-in и для обычной portable-проверки не
запускается. Если она нужна отдельно, передайте `-RunTableTests`.

2. При необходимости отдельно перепроверить манифест обновления:

```powershell
.\tools\tests\test-update-manifest.ps1
```

Эта проверка уже входит в `verify-release.ps1`, но бывает полезна отдельно,
если работа идёт именно вокруг `update.xml`.

## Сборка релизных артефактов

1. Запустить полный сценарий выпуска:

```powershell
.\tools\build\create-release.ps1 -Configuration Release -Platform Win32
```

2. После завершения проверить содержимое `out\artifacts`:
   - `FictionBookEditorNext-<version>-win32-portable.zip`;
   - `FictionBookEditorNext-<version>-win32-setup.exe`;
   - `FictionBookEditorNext-<version>-win32-symbols.zip`;
   - `SHA256SUMS.txt`.

3. При необходимости отдельно проверить staging-папки:
   - `out\Release` содержит свежие бинарники;
   - `out\package\FictionBookEditor` содержит portable-набор файлов и служит
     прямым входом для NSIS.

## Что теперь делается автоматически

Создание артефактов не меняет опубликованные `update.xml` и
`update-prerelease.xml`. Для tag workflow создаёт candidate manifest в
`out\release\update.xml`; только publish job после фактической публикации
обновляет соответствующий feed.

Также автоматически делается следующее:

- формируется portable staging в `out\package\FictionBookEditor`;
- NSIS читает `out\package\FictionBookEditor` напрямую;
- собираются ZIP portable, ZIP symbols и NSIS-установщик;
- записываются SHA-256 в `out\artifacts\SHA256SUMS.txt`.

Для релизов линии 3.0.8 дополнительно могут появиться `-win7-` setup и portable
файлы. Это временные byte-identical aliases универсальных артефактов для
обновления опубликованного `v3.0.8-rc.1`, а не второй build или пакет.

## Что проверить перед публикацией GitHub Release

Для тега `vX.Y.Z-beta.N` или `vX.Y.Z-rc.N` workflow публикует GitHub prerelease и обновляет только `update-prerelease.xml`. Для `vX.Y.Z` он обновляет оба feed: `update.xml` и `update-prerelease.xml`. Candidate manifest создаётся с `-ReleaseTag`; имена файлов остаются с базовой версией.

Описание GitHub Release — единственный актуальный источник пользовательских примечаний. `new-release-notes.ps1` задаёт лишь исходный текст для нового release; повторный workflow не передаёт `--notes-file` существующему release и сохраняет ручные правки body.

1. Тег должен иметь вид `v<version>`, например `v3.0.0`.
2. Candidate manifest должен пройти `validate-update-manifest.ps1`; после
   публикации `DownloadUrl` и `SHA256` stable feed должны ссылаться на setup
   того же тега.
3. `SHA256` в опубликованном stable feed должен совпадать с установщиком из
   `out\artifacts`.
4. Архив `*-symbols.zip` нужно сохранить вместе с релизом для разбора будущих
   minidump.

## Что считать блокером релиза

- `verify-release.ps1` завершился с ошибкой;
- `test-update-manifest.ps1` не проходит;
- `validate-update-manifest.ps1` не проходит для candidate manifest;
- `update.xml` не совпадает по версии, URL или SHA-256 с собранным установщиком;
- ручные регрессионные проверки выявили падение или явную несовместимость.
