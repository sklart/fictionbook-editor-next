# Проба UAS_BLOCKED для Split/Undo — 28.10.2026

**NO-GO / LOCAL CUSTOM UNDO UNSAFE для исследованных вариантов.** Блокирующий parent действительно открывается, но при `nodeValue` и `replaceData` последующий Undo Q оставляет `Qtail` вместо `tail`. При `innerHTML` Redo X не восстанавливает X. Это не доказывает невозможность всех локальных решений: в частности, начальная DOM-мутация выполнена до регистрации custom unit и её вклад в нарушение прежнего native unit не изолирован. Production-кандидата нет.

## Контекст

Исходная ветка `codex/fbe-next-integration-3010`, HEAD `81e6e1a15da97b66c29420837dcfc709dd7f708c`, рабочее дерево и подмодули чистые. Предыдущие test-only изменения и доказательства сохранены коммитами `f8027597`, `93e69299`, `84c37107`, `81e6e1a1`. Предыдущие отчёты, в том числе `split-undo-text-mutation-probe-20261028.md`, изучены: доказательств прежней проверки именно `UAS_BLOCKED` не найдено. Ранее использовался обычный parent.

Изменения ограничены test-only `RuntimeTestPortableState.inl`, `RuntimeTestUndoAndContainers.inl` и `test-fbe-split-transaction-marker.ps1`. Пробы выполняются в отдельных процессах и на синтетической книге `<p>tail</p>`. Операции ввода Q и X синтезированы как WM_CHAR с проверенной границей; каждый шаг сравнивается с сохранёнными S0–S3, стек и выделение записаны в TSV. Следующие отчёт и требования являются документацией; master и production Split не менялись.

## Контракт и проверка области

Использованы официальные документы Microsoft: [UASFLAGS](https://learn.microsoft.com/en-us/windows/win32/api/ocidl/ne-ocidl-uasflags), [parent unit](https://learn.microsoft.com/en-us/windows/win32/api/ocidl/nn-ocidl-ioleparentundounit), [Open](https://learn.microsoft.com/en-us/windows/win32/api/ocidl/nf-ocidl-ioleundomanager-open), [Close](https://learn.microsoft.com/en-us/windows/win32/api/ocidl/nf-ocidl-ioleundomanager-close), [Add](https://learn.microsoft.com/en-us/windows/win32/api/ocidl/nf-ocidl-ioleundomanager-add), [Do](https://learn.microsoft.com/en-us/windows/win32/api/ocidl/nf-ocidl-ioleundounit-do). `UAS_NORMAL` равен нулю; `S_OK` при Open или Add сам по себе не подтверждает создание записи. `Close(parent,FALSE)` отбрасывает временный parent, а обратный custom unit регистрируется после закрытия.

В test-only реализации `GetParentState` возвращает `UAS_BLOCKED`; вложенные Open/Add отбрасываются, закрытие самого parent возвращает S_FALSE. Перед открытием проверяются состояние и HRESULT; существующие blocked или NOPARENTENABLE не принимаются. Для каждого наблюдавшегося входа: исходно `GetOpenParentState=S_FALSE (0x00000001)`; `Open=S_OK`; затем `GetOpenParentState=S_OK`, flags `0x1`; `Close(FALSE)=S_OK`; после него снова `S_FALSE`. Снимки после операции показывают обратный custom unit в нужном стеке, а не просто успешный `Add`. RAII пытается закрыть открытый scope при выходе из `Do`.

Счётчик обращений к `CBlockingUndoProbe::Add` во всех проведённых сценариях — 0. Это не доказывает, что MSHTML не создавал внутренние units или что каждый внутренний вызов был перехвачен. Трасса показывает активное состояние и внешний стек; непосредственно внутри `Do` перечисление менеджера возвращает `0x8000FFFF`. Нет проверки времени жизни всех COM-объектов или инъекций отказов; корректность обработки исключений и неуспешного Close не подтверждена экспериментом.

## Результаты

S0=`tail`, S1=`Qtail`; для C1–C3 S2=`Rtail`, S3=`RXtail`. C0 меняет только внутреннее состояние и использует S2=`Qtail`, S3=`QXtail`.

| Сценарий | Redo X после Undo X/custom | Полный цикл: Undo Q → Redo Q/custom/X → повторные три Undo | Результат |
|---|---|---|---|
| C0: внутреннее состояние | PASS | Все девять сравнений PASS, итог `tail` | PASS для текстового контроля |
| C1: nodeValue | PASS, `RXtail` | Первый и последний Undo Q FAIL: остаётся `Qtail` | NO-GO |
| C2: replaceData | PASS, `RXtail` | Первый и последний Undo Q FAIL: остаётся `Qtail` | NO-GO |
| C3: innerHTML абзаца | FAIL, остаётся `Rtail` без X | Undo Q и Redo X FAIL; остальные значения сохранены в TSV | NO-GO |

Для C1/C2 исходный unit Q виден в Undo до его вызова, после вызова отсутствует, но DOM остаётся `Qtail`. Обычный цикл с Redo X и последующими тремя Undo отдельно дал `undoQ=0`; расширенный цикл дал `undoQ=0`, `finalQ=0`. В C3 после Undo custom Redo содержит X и custom, а ранее зарегистрированный при исходной мутации unit «Действие с текстом» остаётся в Undo. После Redo custom следующий Redo выполняет unit X, но текст остаётся `Rtail`: наличие нужного unit в стеке недостаточно для доказательства его корректного исполнения после пересоздания DOM. В сравнительном опыте без blocking parent дополнительный unit попадал в Redo; здесь такого верхнеуровневого unit после Undo custom не видно, однако нельзя заключать, что все внутренние записи подавлены.

Точные `GetUnitType`/описания и удержанные identity представлены в `unit-map`; в C3 прежний text unit `{25336920-03F9-11CF-8FD0-00AA00686F13}:2262`, X и custom являются отдельными объектами. Сохранение текста, порядка узлов, активного элемента и выделения ограничено синтетическим контуром. Dirty проверялся независимо и остался нерешённым.

## Ошибки, дальнейшая работа и ограничения

Отказ Open, отказ DOM-мутации, отказ Close, отказ Add и исключение до Close — **NOT RUN**. Проверки закрытия книги и редактирования после них также не выполнялись. Следовательно нельзя объявить проверку надёжности блокирующего parent завершённой, но уже наблюдаемое нарушение Undo Q достаточно для NO-GO данного кандидата. Структурный прототип и 16 Split-сценариев не запускались: текстовая матрица не прошла. Требования к следующему этапу вынесены в `split-undo-history-adapter-requirements-20261028.md`; общий адаптер не реализован.

`SPLIT_STRUCTURAL — BLOCKED`; `DIRTY_SAVEPOINT — POLICY UNRESOLVED`; `FILE_CLOSE — INCONCLUSIVE`; `ASan/UBSan Fast`, `verify-release`, `FullValidation` — **NOT RUN**. Отказ Q в этом тесте не является основанием считать всю архитектуру MSHTML принципиально неисправимой.

## Сборка и доказательства

Release Win32, v143 / MSVC 14.44.35207, `/W4 /WX`, `FbeSanitizer=None`, раздельные `out/blocking-parent/<project>/Release` и `build/blocking-parent/<project>/Release`; сборка успешна, MSB8028 не обнаружен. EXE SHA-256 `21358B72F96D83039C87005C884E8C239947E4FCCD9EBF9FD289164B79002A04`; PDB `23DD14616C2C8C54D4F2ED7527CB741B1DF588EB3E2D679B2E2BD9BA193894F8`; GUID `{6962D16A-FDAC-45EC-A812-D2283F8FBDBA}`, Age 30 в обоих. Исходники сборки — исходный HEAD плюс сохранённый `evidence/split-undo-blocking-parent-20261028/source.patch`. Доказательства в `evidence/split-undo-blocking-parent-20261028/` нормализованы по окончаниям строк и конечным пробелам для Git; оригинальные локальные журналы остаются в `out/blocking-parent/`.
