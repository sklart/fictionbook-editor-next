# Импорт, экспорт и пакетные конвертеры

> Тип: Действующая инструкция. Документационная база: FBE Next 3.0.9, `master` / `1ad7c507d1ed`; сверка 19 октября 2026 года.

| Операция | Документ |
| --- | --- |
| FB2 → HTML | [ExportHTML](export-html.md) |
| FB2 → DOCX | [ExportDOCX](export-docx.md) |
| FB2 → EPUB 2/3 | [ExportEPUB](export-epub.md) |
| EPUB 2/3 → FB2 | [ImportEPUB](import-epub.md) |

Штатные DLL размещаются в `Plugins`, пакетные EXE имеют отдельные точки запуска. Для обычной работы встроенных плагинов в portable не требуется вручную регистрировать каждую DLL как legacy COM-компонент.

Открытие FB2/FBD и книги из поддерживаемого архива — не тот же самый контур, что импорт EPUB. Результат конвертации проверяйте в целевом формате и отдельно после обратного открытия, если такой round-trip предусмотрен.

У каждой утилиты собственные параметры. Не конструируйте командную строку одной batch-утилиты по аналогии с другой: используйте её актуальную справку и примеры соответствующего документа.

**Сохранность:** при ошибке или отмене экспорта прежний существующий результат не должен объявляться успешно заменённым. У многофайлового HTML отдельно учитываются частичный отказ и отсутствие гарантии атомарности всего набора при отключении питания.

## Основания

- [docs/plugins/export-docx.md](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/plugins/export-docx.md)
- [docs/plugins/export-epub.md](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/plugins/export-epub.md)
- [docs/plugins/import-epub.md](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/plugins/import-epub.md)
- [docs/release-notes/3.0.9.md](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/release-notes/3.0.9.md)
