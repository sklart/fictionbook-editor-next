# Performance Phase B — fixed document overhead

## Реализовано

| Область | Что кэшируется | Ключ | Инвалидация |
| --- | --- | --- | --- |
| XSLT | готовый `Msxml2.XSLTemplate.6.0` | путь stylesheet + язык | другой путь или язык; неудачные загрузки не кэшируются |
| XSD | `Msxml2.XMLSchemaCache.6.0` | текущий COM thread/apartment | завершение потока/apartment |
| BODY → SOURCE | сериализованный XML-текст | DOM-снимок + encoding + document type | BODY mutation, SOURCE commit/reload (новый DOM-снимок), смена encoding или типа |
| Design search | `SearchTextSnapshot` | identity DOM-документа + document generation | `Invalidate`, другой DOM или generation |
| Search mapping | source id/sourceIndex → vector index | rebuild snapshot | rebuild snapshot |

Кэш XSL хранится в сохраняемом runtime JS context. Каждый language variant
создаёт свой immutable template; один mutable XSL DOM между языками не
используется. Кэш XSD остаётся apartment-local и не передаёт COM-объекты между
потоками.

## Проверенная производительная семантика

- Повторный transform с тем же путём и языком создаёт один template; смена
  языка создаёт независимый template.
- Повторный BODY → SOURCE без изменения документа читает сохранённый
  сериализованный текст. Перемещение caret, selection и scroll его не
  инвалидируют.
- Четыре изменения search query на одном документе используют один snapshot;
  поиск выполняется заново только над уже построенным текстом. Scoped search
  использует тот же snapshot для преобразования scope и для query.
- Lookup source range по id и MSHTML `sourceIndex` не выполняет линейный scan.

При включённом existing diagnostic trace фиксируются aggregate counters:
`xslt_template_builds`, `xsd_schema_loads` и `source_serializations`;
`DocumentSearchCoordinator` хранит проверяемые счётчики snapshot builds и
query runs. Они не пишут содержимое документа и не добавляют per-paragraph
логирование.

## Targeted evidence

Release-проект FBE собран после изменений Phase B. Пройдены целевые проверки:
`test-fbe-main-js-reliability.ps1`,
`test-fbe-body-source-transition-runtime.ps1`,
`test-fbd-support-contract.ps1`,
`test-source-serialization-cache-contract.ps1`,
`test-search-snapshot-cache-contract.ps1`,
`test-search-session.ps1` и MSHTML SearchDocumentAdapter regression.
Контракт счётчиков — `test-phase-b-performance-counters-contract.ps1`.

### Runtime benchmark

Один реальный lifecycle run на fixture малого размера (1 000 paragraphs,
381 428 bytes) выполнил open, BODY initialization, source validation и
несколько BODY ↔ SOURCE переходов за `8 358 ms`. Средний fixture (10 000
paragraphs, 3 837 429 bytes) прошёл тот же маршрут за `18 229 ms`. Оба
прогона завершили semantic assertions BODY/SOURCE transition.

Cache cardinality проверяется отдельно от wall-clock: JS harness создаёт один
XSL template для повторного path/language и отдельный для другого языка;
MSHTML search regression выполняет четыре incremental queries при ровно одном
snapshot build. Source XML cache и schema cache проверяются их targeted
контрактами и реальным BODY/SOURCE validation route.

Phase B устраняет повторную фиксированную работу; задача не задаёт и не
заявляет искусственный процент ускорения. Полный release gate и ручной smoke
остаются отдельными этапами по scope задачи.

## Ограничения

Не менялись BinaryStore, DOM virtualization, background DOM/validation,
MSHTML hosting и архитектура Source/Document model. UTF-8 буфер Scintilla не
кэшируется: он нужен только при реальной повторной загрузке содержимого.
