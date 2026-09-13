# Split: MSHTML Undo probe

Дата прогона: 2026-09-13.  Базовая ревизия перед test-only изменениями:
`bc7c8cbe385ec34f5b01352b45f8537124197f1b`.

Цель probe — установить фактические границы MSHTML Undo и отличить их от
исторической пользовательской семантики Split:

```text
AAA [123] ZZZ
old section = AAA
new section.title = 123
new section.body = ZZZ
Undo/Redo исследуются отдельно от преобразования DOM.
```

`[123]` обозначает выделение; текстовый fixture хранит его как `AAA 123 ZZZ`.

## Результаты прямых MSHTML последовательностей

Каждый вариант выполнялся на новом реальном MSHTML-документе внутри
`BeginUndoUnit`/`EndUndoUnit`.  Результат `3` означает, что ни первый, ни
второй Undo не восстановили точный исходный DOM; это не утверждение, что
трёх Undo достаточно.

| Последовательность | HRESULT | DOM после операции | Undo #1 / #2 | Итог |
| --- | --- | --- | --- | --- |
| `insertAdjacentElement` | `S_OK` | верный | sibling остаётся / теряет class и id | fail |
| `insertBefore` | `S_OK` | верный | sibling остаётся / теряет class и id | fail |
| `appendChild` | `S_OK` | верный | sibling остаётся / теряет class и id | fail |
| `pasteHTML` в содержимое | `S_OK` | пустой root | пустой root | fail |
| `pasteHTML` в общий root | `S_OK` | пустой root | пустой root | fail |
| `IMarkupServices::InsertElement` `AfterEnd` | `0x80070057` | исходный | исходный | fail |
| `IMarkupServices::InsertElement` `BeforeBegin` | `0x80070057` | исходный | исходный | fail |
| `ParseString` + `remove` + `Copy` | `S_OK` | верный | операция не попадает в Undo | fail |
| заранее сформированное detached subtree | `S_OK` | верный | sibling остаётся / теряет class и id | fail |
| замена `root.innerHTML` | `S_OK` | верный | операция не попадает в Undo | fail |

Полный TSV: `tools/tests/test-fbe-split-undo-probe.ps1` с runtime scenario
`split-undo-probe`.  Он подключён только к `verify-release.ps1 -FullValidation`
и сохраняет артефакты, поэтому отрицательный результат не превращается в skip.

## OLE parent-unit

На текущем MSHTML-документе `IServiceProvider::QueryService` с
`SID_SOleUndoManager` / `IID_IOleUndoManager` вернул `S_OK`.  Test-only
`IOleParentUndoUnit` был открыт и закрыт с `S_OK` вокруг исторической
последовательности DOM-мутаций.

| Поле | Наблюдение |
| --- | --- |
| `Open` / `Close` | `S_OK` / `S_OK` |
| Дочерних units в parent | 6 |
| `GetOpenParentState` до / после / Undo / Redo | `0` / `0` / `0` / `0` |
| Последние descriptions | пустые |
| DOM после Split | верный |
| DOM после Undo #1 | без изменений, остаётся результат Split |
| DOM после Redo #1 | без изменений, остаётся результат Split |

Следовательно, `IOleParentUndoUnit` не объединяет эти MSHTML units в
пользовательский единый Undo и не является основанием менять
`BodyStructuralEditor::SplitContainer()`.

## Command-specific prototype: результат исследования

Был выполнен отдельный production prototype только для `SplitContainer`:
он хранил detached deep-clone исходного контейнера и detached пару
post-Split containers, удалял только верхний native Split unit через
`DiscardFrom(topUnit)` и добавлял свой `IOleUndoUnit`. Никакой snapshot
всей книги и очистка всего undo-stack не использовались.

Базовый контракт прошёл: `AAA [123] ZZZ` дал правильный title/body, а один
Undo и один Redo восстановили exact DOM. Однако обязательная смешанная
последовательность выявила несовместимость менеджера MSHTML:

```text
Split → WM_CHAR X → Undo X → Undo Split → Redo Split → Redo X
```

* при стандартном `Do() { ...; undoManager->Add(this); }` Redo Split
  возвращает Split DOM, но очищает ещё ожидающий native Redo для `X`;
  второй Redo не возвращает `X`;
* при `Do() { ...; return S_OK; }` native Redo сохраняется, но сам custom
  Split unit не попадает в Redo и первый Redo не возвращает Split.

Это прямо нарушает требование сохранения обычного MSHTML Undo. Согласно
границе prototype, production-изменения удалены; оставлена только test-only
диагностика последовательности. Исправление потребовало бы глобального
перехвата/перестройки обоих стеков Undo, что не разрешено для Split-specific
решения.

Следовательно, глобальный Undo layer и Split-specific compound undo не
являются допустимым продолжением этой задачи.

## Исторический runtime и финальное решение

Исторический binary, собранный из `6705abf2^`
(`0498862487c5d31fb787b8b75c00439746d4b1e9`), проверен на реальном MSHTML.
Он является oracle поведения, а не только источником старого C++ кода:

| Сценарий | Фактический результат |
| --- | --- |
| `AAA [123] ZZZ → Split` | old = `AAA`, new title = `123`, new body = `ZZZ` |
| Undo #1 | не восстанавливает точный исходный DOM одной операцией |
| `abc|def → Split → WM_CHAR X` | `Xdef` |

Таким образом, historical Split **сохранял title**, но атомарный один Undo не
был историческим контрактом: фактический вариант — C (title сохраняется,
native MSHTML создаёт несколько Undo entries). Требование сохранить title не
требует и не оправдывает создание нового глобального Undo/Redo механизма.

На `bc7c8cbe` strict runtime-regression обнаружил потерю title при сохранении
native Undo/Redo. Диагностика показала, что title уже создан в новом sibling,
но `source-cleanup` удаляет его, когда sibling предварительно присоединён к
живому DOM.

Финальное исправление `fbf108b0` оставляет новый sibling detached до полного
завершения `source-cleanup`, и только затем вставляет его рядом с исходным
контейнером. Это возвращает историческую семантику `title = selected text`,
не меняя архитектуру Undo и не создавая placeholder из пользовательского
текста.

`test-fbe-split-container-production.ps1` успешно прошёл 13/13 runtime
сценариев: section, stanza, существующий title, пустой tail, inline markup,
id/ссылки, сохранение/XSD/reopen, Undo/Redo и штатный ввод в текущую каретку
(`abc|def → Split → WM_CHAR X → Xdef`). Также успешно пройден
`verify-release.ps1 -FullValidation` на Release Win32/v143.

## Исходный строгий runtime-отказ

На test-only ревизии `42a93d79` первый по порядку строгий Split-case,
`split-section-selection`, воспроизводится командой:

```powershell
& .\tools\tests\test-fbe-split-container-production.ps1 `
  -FbeExe .\out\Release\FBE.exe -CaseId split-section-selection -KeepArtifacts
```

Фактическая диагностика подтверждает, что до команды выбрано именно `123`
(`selection_text=123`, range не collapsed). Команда возвращает корректные
`StructuralOperationResult` и сохраняет Undo/Redo, но её DOM после операции
таков:

```html
<DIV class=section><P>AAA&nbsp;123</P></DIV>
<DIV id=target-section-selection class=section><P>ZZZ</P></DIV>
```

То есть `new_title_text` пуст, `fragments_preserved=0`; это был первый реальный
отказ строгого регресса, а не ошибка selection assertion. Он устранён
`fbf108b0` описанным выше порядком DOM-мутаций. Артефакт воспроизведения:
`%TEMP%\fbe-split-container-7a8251aa7c7e41ddb41e8dba99482368`.
