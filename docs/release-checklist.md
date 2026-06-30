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

2. При необходимости отдельно перепроверить манифест обновления:

```powershell
.\tools\tests\test-update-manifest.ps1
```

Эта проверка уже входит в `verify-release.ps1`, но бывает полезна отдельно,
если работа идёт именно вокруг `update.xml`.

## Сборка релизных артефактов

1. Запустить полный сценарий выпуска:

```powershell
.\tools\build\create-release.ps1 -Configuration Release -Platform Win32 -ValidateUpdateManifest
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

При создании установщика сценарий выпуска сам синхронизирует в
[update.xml](D:\Download\FBeditor\update.xml):

- `Name`;
- `Date`;
- `Version`;
- `Beta`;
- `DownloadUrl`;
- `SHA256`.

Ручное редактирование этих полей перед обычным релизом не требуется.

Также автоматически делается следующее:

- формируется portable staging в `out\package\FictionBookEditor`;
- NSIS читает `out\package\FictionBookEditor` напрямую;
- собираются ZIP portable, ZIP symbols и NSIS-установщик;
- записываются SHA-256 в `out\artifacts\SHA256SUMS.txt`.

## Что проверить перед публикацией GitHub Release

1. Тег должен иметь вид `v<version>`, например `v3.0.0`.
2. `DownloadUrl` в `update.xml` должен указывать на тот же тег и имя
   установщика.
3. `SHA256` в `update.xml` должен совпадать с установщиком из `out\artifacts`.
4. Архив `*-symbols.zip` нужно сохранить вместе с релизом для разбора будущих
   minidump.

## Что считать блокером релиза

- `verify-release.ps1` завершился с ошибкой;
- `test-update-manifest.ps1` не проходит;
- `create-release.ps1 -ValidateUpdateManifest` не проходит;
- `update.xml` не совпадает по версии, URL или SHA-256 с собранным установщиком;
- ручные регрессионные проверки выявили падение или явную несовместимость.
