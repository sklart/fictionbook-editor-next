# 0005. Автодополнение XML выделяется как source-helper редактора

## Контекст

`Fb2SourceAutocomplete` и `Fb2SourceStructuralContext` выполняют разбор
текста XML для режима исходника. Они не владеют DOM, окном, selection или
undo/redo и уже имеют native поведенческие проверки, но находились в корне
`src/fbe` рядом с координаторами приложения.

## Решение

Четыре файла helper-ов расположены в `src/fbe/source`. Они по-прежнему
компилируются непосредственно в FBE без PCH. Autocomplete получает generated
schema metadata точным относительным include-путём; это не создаёт новый
public include-каталог и не переносит GUI-код.

## Последствия

`mainfrm` остаётся координатором вызова. Владелец документа, DOM, UI и
settings не меняются. `test-fbe-source-helpers-boundary.ps1` закрепляет
размещение и запрет private UI-зависимостей, а существующие structural-context
и autocomplete smoke-тесты проверяют поведение.
