# Технические отличия FictionBook Editor Next от sensboston/fictionbookeditor

Документ описывает основные инженерные отличия текущей ветки от исходного проекта <https://github.com/sensboston/fictionbookeditor>.  
Пользовательское описание изменений находится в [README.md](../README.md); здесь перечислены именно технические направления.

## 1. Структура проекта

- Исходники разнесены по каталогам `src/*`:
  - `src/fbe` — основное приложение;
  - `src/fbv` — FictionBook Validator;
  - `src/fbshell` — shell-интеграция Windows;
  - `src/export-html`, `src/export-docx`, `src/export-epub`, `src/import-epub` — плагины.
- Runtime-файлы отделены от исходников в `runtime`.
- Упаковка и NSIS-скрипты вынесены в `packaging`.
- Вспомогательные build/test/release-сценарии вынесены в `tools`.

## 2. Версионирование и branding

- Продукт переименован в `FictionBook Editor Next`.
- Версия новой ветки начинается с `3.0.0`.
- Общие version-макросы находятся в `src/version.h`.
- `tools/version/sync-version.ps1` синхронизирует `src/version.h` и NSIS `version.nsh`.
- Проверка обновлений переведена на GitHub Releases fork-репозитория `sklart/fictionbook-editor-next`.

## 3. Release pipeline

- Добавлен GitHub Actions workflow `.github/workflows/build.yml`.
- На push и pull request выполняется сборка Win32 Release и публикация временных artifacts.
- На tag `v*` создаётся GitHub Release с:
  - установщиком;
  - portable ZIP;
  - ZIP с PDB/debug-символами;
  - `SHA256SUMS.txt`;
  - человекочитаемыми release notes из `CHANGELOG.md`.
- `tools/build/create-release.ps1` формирует артефакты и обновляет `update.xml` реальной SHA-256 суммой установщика.

## 4. NSIS и установщик

- Установщик переведён на staged input из `out/package/FictionBookEditor`.
- Добавлена синхронизация UAC-плагина из `third_party/uac`.
- Системная интеграция Windows вынесена в отдельную опциональную секцию.
- Деинсталлятор создаётся независимо от выбора системной интеграции.
- В uninstall registry пишутся `DisplayVersion`, `InstallLocation`, `DisplayIcon`, `Publisher`, `URLInfoAbout`, `EstimatedSize`.
- Плагины импорта/экспорта и batch-конвертеры оформлены отдельными секциями.
- `ImportEPUBLunaSVG.dll` устанавливается как дополнительная библиотека ImportEPUB для SVG→PNG/JPEG conversion.

## 5. Shell-интеграция Windows

- Старый shell-контур заменён modern property handler / thumbnail provider.
- Поддерживаются:
  - FBE-specific published properties;
  - `InfoTip`, `TileInfo`, `Details`, `PreviewDetails`;
  - thumbnail provider для обложек FB2;
  - регистрация на `.fb2` и `FictionBook.2`.
- Схема свойств `FBE.Sequence.propdesc` размещается в shared shell directory.
- Добавлены диагностические и smoke-тесты:
  - `check-fb2-shell-surfaces.ps1`;
  - `test-fb2-shell-thumbnail-matrix.ps1`;
  - `test-fbe-specific-installer.ps1`;
  - дополнительные fixture FB2 для кириллицы, свойств и обложек.

## 6. FBV

- Исходники `FBV` импортированы в `src/fbv`.
- `FBV.exe` собирается вместе с проектом и больше не берётся как старый runtime-бинарник.
- Добавлены MUI-ресурсы для локализованного shell verb `Validate`.
- Shell-команда проверки FB2 использует `FBV.exe` и локализованную подпись.
- Release-gate проверяет наличие и версию `FBV.exe`/`FBV.pdb`.

## 7. Плагины импорта и экспорта

- В solution подключены:
  - `ExportDOCX`;
  - `ExportEPUB`;
  - `ImportEPUB`;
  - `ImportEPUBLunaSVG`;
  - batch-утилиты `ExportDOCXBatch`, `ExportEPUBBatch`, `ImportEPUBBatch`.
- Плагины и batch-утилиты выводят DLL/EXE/PDB в общий `out/Release`.
- Portable/package/release-gate учитывает новые бинарники и символы.
- Для ExportEPUB исправлены UTF-8/mojibake проблемы в окнах и EPUB metadata.

## 8. Зависимости

- Legacy PCRE не возвращается в runtime.
- Добавлен сервисный контур обновления сторонних исходников:
  - Scintilla;
  - Lexilla;
  - PCRE2;
  - Hunspell.
- Сценарии находятся в `tools/build/*third-party*`.
- Hunspell собирается через отдельный build-контур и проверяется smoke/regression тестами.

## 9. Устойчивость FBE

- Добавлена обработка read-only FB2 перед сохранением.
- Исправлена загрузка файлов по относительным путям из командной строки.
- Добавлена crash/recovery диагностика:
  - recovery-копии;
  - crash dumps;
  - отдельный symbols ZIP для релиза.
- Постепенно закрываются PVS-Studio warnings без архитектурной переписи.

## 10. Тестирование

- Добавлен документ [test-contours.md](test-contours.md) с картой тестовых контуров.
- Добавлен [manual-test-plan.md](manual-test-plan.md) для ручного тестирования.
- Release-gate включает проверки сборки, runtime-бинарников, installer/package layout, shell/FBV/export smoke-сценариев и artifacts ZIP.

## 11. Принципы сопровождения

- Не переписывать проект с нуля.
- Не возвращать legacy PCRE в runtime.
- Сохранять структуру каталогов и принятые решения проекта.
- Сначала изучать существующую реализацию, затем менять.
- Новые изменения фиксировать в `CHANGELOG.md` и `docs/todo.md`.
