# Статус структурной верификации

Этот документ отделяет проведённые проверки от непроведённых. Он не заменяет
release notes и должен обновляться при новом authoritative прогоне.

## Последующие исправления аудита

После `0b62e29e` изолирован FULL-сценарий отказа сохранения: он запускает
portable-копию FBE в `out/tests` и не использует пользовательские HKCU
настройки или регистрацию COM. Также удалены tracked локальные результаты
сборки, а ABI fixture направляет OBJ/PDB/LIB/EXP в `out/tests`. Эти точечные
скрипты и contract-проверка общего API выполнены локально. Генерация
ImportEPUB resources переведена с unconditional PreBuildEvent на
инкрементальную MSBuild-цель с явными входами/выходом; экспортные потребители
получают общий COM API только от `FBEContracts`.

## Подтверждено локально

- Extension startup разделён на scripts, bundled plugins и MRU/recent documents;
  `InitializeExtensionUi` остаётся только тонким coordinator. Runtime test
  scenarios подключают тематические части из малого umbrella-файла.
  `ScriptUiController` и `PluginUiController` действительно владеют своим
  state/lifecycle (меню, visual resources, catalog/manager и command state),
  а не только вызываются как разнесённые startup methods.

- Первый lifecycle-контур документа отделён структурно: `DocumentLoader` не
  зависит от `CMainFrame`, `PendingDocument` восстанавливает active-document
  при rollback, а `FbeRecentDocuments::Controller` владеет normal/archive MRU
  resolution и мутациями. Проверены boundary-контракт и runtime New/Open/Reload.

- Presentation-слой runtime-локализации `CMainFrame` (меню, toolbar и tooltip)
  размещён в `src/fbe/ui/MainFrameRuntimeUi.inl` и остаётся в том же translation
  unit, поэтому private-граница frame не расширена.

- Test-only runtime orchestration живёт в `src/fbe/testing/RuntimeTestScenarios.inl`;
  `mainfrm.cpp` включает её в тот же translation unit, сохраняя private-boundary
  CMainFrame без public-полей или широкого `friend`.

- Evaluated MSBuild policy: собственные проекты используют `v143`, а прямые
  сборки FBE, ImportEPUB и shell-интеграции используют VC Tools 14.44.
  C++-исходники получают централизованный `stdcpp17` в Debug и Release;
  контракт отдельно подтверждает C++20 override и то, что C/vendored/generated
  код не становится потребителем этой policy.
- FBE COM contract генерируется в `build/generated/.../fbe-api`; ABI v2
  harness, ImportEPUB и FBE временные Release-link проверки проходят.
- Общая FB2/shell реализация проходит metadata, cover, thumbnail и boundary
  проверки; shell-проект собирается отдельно.
- Карта упаковки проходит contract, copy и изолированные Core/Integration
  staging fixture-проверки.
- Перенесённые editor-only plugin, XML-source, search и settings сервисы
  проходят соответствующие boundary и поведенческие тесты.

## Официальная Release-сборка

Прогон 5 сентября 2026 года команды

```powershell
.\tools\build\build.ps1 -Configuration Release -Platform Win32 -PlatformToolset v143
```

первоначально остановился в подготовке `third_party/aom`, вызываемой из
`build-libheif.ps1`: после прерванной CMake-конфигурации generated
`build/aom/Release-v143/config/*rtcd.h` имели нулевой размер, а
`aom_rtcd.vcxproj` сообщал `C2065: setup_rtcd_internal`.

После удаления только этого generated build-каталога и повторного штатного
`build-aom.ps1` заголовки RTCD были заново созданы, а AOM успешно собран и
установлен. Повторный authoritative Win32 Release build завершился успешно;
сформированы `FBE.exe`, bundled plugins, editor runtime и CommonCore/runtime
provenance. Запущенный на этих артефактах `verify-release.ps1` успешно прошёл
FAST- и полный `-FullValidation` контуры: подтверждены GUI, FB2, packaging,
COM/ABI, PCRE2, plugins, portable/NSIS-контракты, Huge structural tables,
fail-closed Save, image/EPUB/HTML E2E и Win7 import gate. Полная проверка
завершилась успешной проверкой релиза 3.0.8.

После замечаний аудита повторно выполнены `test-fbe-table-failure-safety.ps1`
для обоих fault-вариантов, `test-fbe-contract-generation.ps1`,
`test-first-party-msbuild-policy.ps1`, `test-import-epub-localization-resources.ps1`,
`test-release-test-catalog.ps1` и `test-no-tracked-local-build-artifacts.ps1`.
Новый FULL запуск `verify-release.ps1 -Configuration Release -FullValidation`
завершился успешно; его stdout/stderr сохранены в
`out/tests/audit-verify-full.log` и `out/tests/audit-verify-full.err.log`.
Повторный штатный `build.ps1` завершился с валидным CommonCore provenance
(`out/tests/audit-repeat-build.log`): ImportEPUB выполнялся через `Build` и
не запускал повторную компиляцию плагина. Полный clean/parallel прогон после
последней правки графа ImportEPUB подтверждён также MSBuild
`FBE.sln /m /t:Rebuild` (журнал `out/tests/audit-clean-solution.log`);
после него contract generation и политика отсутствия tracked локальных
артефактов прошли повторно.

После полной проверки штатный `create-release.ps1` сформировал и проверил
актуальные `FictionBookEditorNext-3.0.8-win32-portable.zip`, setup.exe,
symbols.zip и `SHA256SUMS.txt` в `out/artifacts`.

Fail-closed Save выполняется из изолированной portable-копии с приватным
`Data`, включая `Data\Diagnostics`; он не использует `--installed`, не
читает и не меняет HKCU COM/settings пользователя. Retention trace-логов
portable-копии ограничен её собственной `DiagnosticsDirectory`.
