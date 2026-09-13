# Split: MSHTML Undo probe

Дата прогона: 2026-09-13.  Базовая ревизия перед test-only изменениями:
`bc7c8cbe385ec34f5b01352b45f8537124197f1b`.

Цель probe — доказать либо опровергнуть возможность сохранить исторический
контракт Split без изменения production-кода:

```text
AAA [123] ZZZ
old section = AAA
new section.title = 123
new section.body = ZZZ
Undo #1 = точный исходный DOM
Redo #1 = точный результат Split
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

## Следующий допустимый prototype

Нужен command-specific `IOleUndoUnit` только для Split, а не общий Undo
framework. До его реализации prototype обязан отдельно доказать следующее:

1. До Split сохранить точное представление минимальной затронутой области и
   после Split — её точное представление, включая id, title, inline-разметку и
   ссылки.
2. Выполнить native Split и убрать его промежуточные MSHTML undo-units только
   в пределах этой команды; затем добавить один собственный unit в тот же
   manager.
3. В `Do` атомарно переключать pre/post представления, не создавая новые
   MSHTML undo entries. Повторный `Do` должен добавлять только самого себя,
   как требует контракт `IOleUndoUnit`.
4. Проверить стек: `Split`, обычный `WM_CHAR`, Undo, Undo, Redo, Redo. Ввод
   должен отменяться первым, Split — вторым; повторный ввод не вправе
   использовать stale markup pointers.
5. Привязать unit к текущему document/session generation. При `LoadFile`,
   закрытии документа или смене MSHTML document старый unit должен быть
   уничтожен вместе со старым undo manager, а не применён к новому FB2.

Пока эти пункты не доказаны runtime-тестом, production Split оставлен без
изменений, а строгий контракт `AAA [123] ZZZ` остаётся красным.

## Первый текущий строгий runtime-отказ

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

То есть `new_title_text` пуст, `fragments_preserved=0`; это первый реальный
отказ строгого регресса, а не ошибка selection assertion.  Артефакт
воспроизведения: `%TEMP%\fbe-split-container-7a8251aa7c7e41ddb41e8dba99482368`.
