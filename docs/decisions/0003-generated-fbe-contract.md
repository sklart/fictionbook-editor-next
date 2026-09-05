# 0003. COM-контракт FBE генерируется централизованно вне исходников продукта

## Контекст

`fbe.idl` принадлежит одновременно редактору и plug-in API. Генерация MIDL в
`src/fbe` смешивала источник контракта с производными `FBE.h`, `FBE_i.c` и
`FBE.tlb`; потребители не имели самостоятельной границы зависимости.

## Решение

Источник перенесён в `src/contracts/fbe.idl`. Проект
`src/contracts/FBEContracts.vcxproj` генерирует все три артефакта в
`build/generated/<Platform>/<Configuration>/fbe-api`.

FBE компилирует единственный generated `FBE_i.c`, а FBE, ImportEPUB и его
batch-проект получают заголовок через project reference и
`FbeApiOutputDirectory`. Runtime-harness-ы вызывают ту же генерацию через
`ensure-fbe-api.ps1`.

## Последствия

В `src/fbe` не хранятся ни IDL, ни generated API-артефакты. Контракт остаётся
тот же по UUID, GUID и layout: изменено только происхождение файлов и порядок
сборки. `test-fbe-contract-generation.ps1` проверяет границу и фактический
вывод MIDL.
