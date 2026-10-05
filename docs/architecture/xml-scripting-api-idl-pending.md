# XML scripting API: реализованная COM-экспозиция

Внутренний backend XML API реализован и проверяется без COM. Он получает
актуальный Source XML, проверяет кандидат штатным SAX/schema/FBD-контуром и
применяет его через `LoadFromDOM`. До успешной валидации документ не меняется.
После применения backend синхронизирует Body, Source, дерево документа,
dirty/recovery/UI-состояние. Каждая внутренняя операция регистрирует ровно
один `IOleUndoUnit` в штатном MSHTML/FBE undo manager: обычные Ctrl+Z/Ctrl+Y
возвращают XML и исходное dirty-состояние. Отдельного пользовательского undo
стека или специальной команды отката для XML API нет; переданный `action`
становится описанием стандартной undo-операции.

Статус: **Implemented**. `ExternalHelper` — тонкий COM-адаптер к
`XmlScriptBackend`; parsing, validation, применение и undo остаются в backend.

## Реализованный контракт `src/contracts/fbe.idl`

В `IExternalHelper` после `GetLocalizedString` с DISPID 31 добавлены методы
32–35. IID, старые DISPID, vtable и правила владения не менялись.

```idl
[id(32)] HRESULT GetSourceText([out, retval] BSTR* text);
[id(33)] HRESULT ValidateSourceText([in] BSTR text, [out, retval] BOOL* valid);
[id(34)] HRESULT GetLastSourceDiagnostic([out, retval] BSTR* diagnosticJson);
[id(35)] HRESULT ApplySourceText([in] BSTR text, [in] BSTR action, [out, retval] BOOL* applied);
```

`GetLastSourceDiagnostic()` возвращает JSON с полями `valid`, `message`,
`line` и `column`. Это оставляет `ValidateSourceText()` и
`ApplySourceText()` естественными булевыми вызовами JScript, при этом
диагностика доступна без параметров-by-reference, которых нет в обычном
сценарии JavaScript.

## MIDL

Штатный MIDL генерирует результаты только в
только в `build/generated/<Platform>/<Configuration>/fbe-api`:

- `FBE.h`;
- `FBE_i.c`;
- `FBE.tlb` (и сопутствующие MIDL-outputs).

Их не следует редактировать вручную и не следует коммитить, если отдельная
политика tracked-generated не будет явно утверждена.

Контрактные и runtime-проверки покрывают MIDL, COM dispatch, настоящий JScript
`window.external`, FB2/FBD validation и `Apply → Undo → Redo`. Generated MIDL
outputs не являются tracked source artifacts.
