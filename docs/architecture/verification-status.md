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

- Пользовательские script toolbars получили отдельные boundaries: `ScriptRegistry`
  владеет GUID UID и orphan records в settings directory, `ToolbarsV2Codec`
  — portable v1/v2 representation, `ScriptToolbarCollection` — persistent
  definitions, а `ScriptToolbarRuntimeCollection` — HWND/rebar state. Focused
  contracts `test-script-identity-contract.ps1`,
  `test-script-toolbars-v2-contract.ps1`,
  `test-portable-scripts-infrastructure.ps1`, customize-dialog и новый
  `test-script-toolbar-management-contract.ps1` выполнены локально.
  Проверены transactional persistence в installed/portable mode, empty custom
  toolbar, lifecycle rebar/HWND, dynamic View menu и сохранение позиции
  временно отсутствующего UID. Debug и Release Win32 `FBE.vcxproj` собраны
  локально 15.09.2026.

- Скрытые script toolbars редактируются напрямую по persistent definition:
  selector customize-диалога не зависит от `HWND`, а runtime toolbar является
  только projection этой модели. Добавлены focused UI/lifecycle contract и
  self-closing runtime scenario с пятью панелями, повторной инициализацией,
  restart-readback, отсутствующим UID и rebar/HWND leak checks; installed
  contour запускается только на изолированном CI worker.

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

- Save-контур отделён от frame: `DocumentSaveController` выполняет normal,
  archive и Save As persistence, committing `DocumentSession` только после
  успеха. `CMainFrame` сохраняет fail-closed source/XML preflight, UI,
  savepoint и recovery cleanup; failed/cancelled save не подтверждает их.
  Serialization и archive-write failures различаются в result, поэтому
  фиктивный archive error не показывается при ошибке сериализации.

- RecoveryStore остаётся storage-only, RecoveryService сохраняет snapshot, а
  RecoveryController владеет lifecycle/identity commit. Frame сохраняет timer,
  source extraction и restore prompt; normal restore сбрасывает session через
  `NewDocument`, archive restore возвращает archive location.
  Archive external-modification runtime получает реальный `ArchiveWrite` code
  на frame/test boundary, не загрязняя `DocumentSaveController` test logic.
  Dedicated normal-recovery runtime проверяет NewDocument session, Untitled
  identity, dirty state и удаление snapshot после commit restore.
  Save As удерживает encoding, filename и session identity до persistence
  success; MRU mutation проходит через `RecentDocumentsController`.

- `EditorViewController` координирует полный lifecycle перехода через узкий
  `IEditorViewHost`: policy остаётся в `EditorViewTransition`, state — в
  `EditorViewState`, selection — в `EditorSelectionState`, а conversion — в
  `SourceDocumentTransfer`. Он выполняет prerequisites и Source commit до
  `CommitTransition`; invalid Source возвращает `Rejected/InvalidSource` и
  сохраняет Source view. `CMainFrame::ShowView` — только adapter/result
  presentation, а отдельный `CommitSourceDocument` используется Save
  preflight без fake view switch. Контракты и BODY/DESC/SOURCE runtime,
  включая некорректный Source, выполнены на Release-сборке.

- `SourceViewSession` владеет XML snapshot и conversion exchange режима
  Source, тогда как `BodySourceSelectionCoordinator` владеет DOM/XML mapping
  выделения в обоих направлениях. `CMainFrame` оставляет только настройки и
  presentation adapter; отдельные boundary-контракты, transfer behavior и
  BODY/SOURCE runtime выполнены на Release-сборке.

- `PluginExecutionController` владеет COM execution protocol для import/export:
  instance creation, API negotiation, v2 interface, host, stream/DOM и
  snapshot. Он возвращает explicit result и сохраняет plugin diagnostic
  events. `PluginUiController` остаётся discovery/menu/LastCommand owner, а
  `CMainFrame` применяет возвращённый import DOM только после `DiscardChanges`.

- `FBEStatusBar::State` владеет queued/context/transient/validation logical
  state и сохраняет priority incremental search → transient → context.
  `CMultiPaneStatusBarCtrl` по-прежнему принадлежит frame: pane text/layout,
  DPI и click/double-click/clipboard presentation не перенесены в модель.

- `SourceViewDiagnostics` изолирует benchmark-only profile и memory snapshot:
  когда benchmark выключен, profiler не выполняет sampling. `DiagnosticCommandService`
  выполняет операции журнала, cleanup, package и next-launch preference через
  authoritative `StartupTrace`; frame оставляет confirmation, localized
  feedback и запись clipboard. Privacy backend и package content не менялись.

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
