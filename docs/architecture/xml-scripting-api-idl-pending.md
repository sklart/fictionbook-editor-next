# XML scripting API: подготовленная COM-экспозиция

Внутренний backend XML API реализован и проверяется без COM. Он получает
актуальный Source XML, проверяет кандидат штатным SAX/schema/FBD-контуром и
применяет его через `LoadFromDOM`. До успешной валидации документ не меняется.
После применения backend синхронизирует Body, Source, дерево документа,
dirty/recovery/UI-состояние. Для внутреннего вызова он ведёт один снимок на
операцию и умеет откатить последний вызов, возвращая зафиксированное до него
dirty-состояние.

Публичные методы `window.external` намеренно **не доступны**: локальная
граница не разрешает изменять `src/contracts/fbe.idl`. Поэтому
`docs/scripting-api.md` не рекламирует эти методы, а production-код не вводит
никакой альтернативный публичный транспорт.

## Неприменённый точный diff `src/contracts/fbe.idl`

Ниже — минимальное добавление к `IExternalHelper` после существующего
`GetLocalizedString` с DISPID 31. Существующие идентификаторы, vtable,
GUID и правила владения не меняются.

```diff
diff --git a/src/contracts/fbe.idl b/src/contracts/fbe.idl
--- a/src/contracts/fbe.idl
+++ b/src/contracts/fbe.idl
@@
  [id(31), helpstring("method GetLocalizedString")] HRESULT GetLocalizedString([in] BSTR key, [out, retval] BSTR* text);
+	[id(32), helpstring("method GetSourceText")] HRESULT GetSourceText([out, retval] BSTR* text);
+	[id(33), helpstring("method ValidateSourceText")] HRESULT ValidateSourceText([in] BSTR text, [out, retval] BOOL* valid);
+	[id(34), helpstring("method GetLastSourceDiagnostic")] HRESULT GetLastSourceDiagnostic([out, retval] BSTR* diagnosticJson);
+	[id(35), helpstring("method ApplySourceText")] HRESULT ApplySourceText([in] BSTR text, [in] BSTR action, [out, retval] BOOL* applied);
   };
 };
```

`GetLastSourceDiagnostic()` возвращает JSON с полями `valid`, `message`,
`line` и `column`. Это оставляет `ValidateSourceText()` и
`ApplySourceText()` естественными булевыми вызовами JScript, при этом
диагностика доступна без параметров-by-reference, которых нет в обычном
сценарии JavaScript.

## После снятия блокировки IDL

Нужно применить ровно этот diff, реализовать тонкий адаптер в
`ExternalHelper` к `XmlScriptBackend`, обновить таблицы имён диспетчеризации
для DISPID 32–35 и выполнить штатный MIDL. Генерируемые результаты остаются
только в `build/generated/<Platform>/<Configuration>/fbe-api`:

- `FBE.h`;
- `FBE_i.c`;
- `FBE.tlb` (и сопутствующие MIDL-outputs).

Их не следует редактировать вручную и не следует коммитить, если отдельная
политика tracked-generated не будет явно утверждена.

После подключения COM API нужны: Release Win32 build; внутренний contract и
runtime XML backend test; COM dispatch/runtime test из JScript для всех четырёх
методов; проверка валидного и невалидного FB2/FBD; `Apply → Undo`; и обычные
Body/Source/Save/Document Tree smoke-проверки. Полный `verify-release` для
одной этой внутренней подготовки не запускался.
