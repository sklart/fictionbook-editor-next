# Runtime-локализация

Этот каталог фиксирует и документирует используемую схему runtime-локализации
FictionBook Editor Next. Строки постепенно выносятся из C++/RC в JSON, а
shell-команда Проводника использует отдельные MUI-ресурсы.

## Основной формат

Основной формат внешних переводов — JSON. Планируемая структура установленной
программы:

```text
FictionBook Editor Next\
  FBE.exe
  FBV.exe
  Plugins\
    ExportHTML.dll
    ExportDOCX.dll
    ExportEPUB.dll
    ImportEPUB.dll
    ImportEPUBLunaSVG.dll

  Lang\
    en-US\
      fbe.json
      fbv.json
      export-html.json
      export-docx.json
      export-epub.json
      import-epub.json

    ru-RU\
      fbe.json
      fbv.json
      export-html.json
      export-docx.json
      export-epub.json
      import-epub.json

    Shell\
      FBVVerbResources.dll
      ru-RU\FBVVerbResources.dll.mui
      uk-UA\FBVVerbResources.dll.mui
```

## Fallback

Отсутствие внешнего JSON-файла не должно ломать запуск. Порядок поиска строки:

1. `Lang/<текущий-язык>/<модуль>.json`;
2. `Lang/en-US/<модуль>.json`;
3. встроенная строка в `exe`/`dll`;
4. диагностический ключ строки, если даже встроенный fallback отсутствует.

Иными словами, программа должна запускаться даже при минимальной portable-
раскладке без каталога `Lang`.

## Shell/MUI

Для команд Проводника Windows JSON не подходит напрямую: `MUIVerb` ожидает
ресурсную строку вида `@module,-id`. Поэтому shell-локализация остаётся на
MUI-ресурсах и физически размещается под общим каталогом `Lang`:

```text
Lang\Shell\FBVVerbResources.dll
Lang\Shell\ru-RU\FBVVerbResources.dll.mui
```

Если MUI-модуль недоступен, регистрация shell-команды должна использовать
обычную fallback-строку, а не оставлять сломанное меню.
