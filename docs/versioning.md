# Версия приложения, API и выпусков

> Тип: Действующая инструкция. Документационная база: FBE Next 3.0.9, `master` / `1ad7c507d1ed`; сверка 19 октября 2026 года.

## Версия продукта

Источником версии приложения остаётся `src/version.h`. После изменения используйте:

```powershell
pwsh ./tools/version/sync-version.ps1
```

Проверяйте согласованность About, file/product versions бинарников и установщика штатными release-проверками. Не редактируйте независимо сгенерированные представления одной версии.

## Теги и архивная история

Стабильные выпуски имеют теги `vX.Y.Z`, предварительные — суффикс Beta/RC. Исторические написания тегов сохраняются буквально: например `v3.0.8-rc.1` и `v3.0.8-rc2`. Ни даты публикации, ни существующие URL не унифицируются задним числом.

Для каждого опубликованного RC ведётся собственная запись в [истории выпусков](release-notes/README.md). Отсутствие стабильного 3.0.8 нельзя компенсировать документом, который ошибочно называет RC1 стабильным выпуском.

## Не смешивать три контракта

| Обозначение | Что означает |
| --- | --- |
| FBE Next 3.0.9 | Версия продукта и набора поставки. |
| Scripting API 1.5.0 (104 метода) | Возможности `window.external`; наличие метода проверяется через публичный API. |
| FBELib 1.0 | Версия type library со своими инвариантами совместимости. |

Из расширения scripting API не следует автоматическое повышение версии type library. IID, существующие DISPID и бинарная совместимость управляются правилами [FBELib](reference/fbelib-typelib-versioning.md). Архитектура XML backend вынесена в [отдельный документ](architecture/xml-scripting-api.md), полный публичный справочник — в [scripting-api.md](scripting-api.md).

## Feeds и release notes

Candidate manifest создаётся для проверяемого релиза, а опубликованные feeds обновляются на шаге публикации. Не подставляйте в feed локальный путь, непроверенный URL или SHA другого установщика. Точный сценарий — [чек-лист выпуска](release-checklist.md).

`docs/release-notes` и опубликованный GitHub Release имеют разные роли: репозиторный текст помогает готовить выпуск, а опубликованное body может содержать последующие ручные уточнения. Эти уточнения нельзя затирать повторным workflow.

## Основания

- [src/version.h](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/src/version.h)
- [docs/fbelib-typelib-versioning.md](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/fbelib-typelib-versioning.md)
- [tools/build/new-release-notes.ps1](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/tools/build/new-release-notes.ps1)
- [docs/versioning.md](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/versioning.md)
