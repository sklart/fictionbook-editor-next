# Восстановление CI и предрелизной проверки

База: `9cb126e5eeae26fb802d6a91c3dcd3c890f6461e`. Работа ведётся в отдельной ветке `codex/fix-ci-validation-gates`; публикация и merge не выполнялись.

## Исходные проблемы и адресные исправления

| Проблема | Причина и изменение | Проверка |
|---|---|---|
| Runtime dialog coverage | Проверка не распознавала массив литералов, элементы которого передаются в `DescriptionText`; устаревший `binary_files` заменён используемым `binary_file_forms`. Добавлены отрицательные проверки привязки и переводов. | До: FAIL на `binary_id`; после: PASS, 736 ключей; localization preflight PASS. |
| Документация | Три ссылки указывали на локальный cache Codex, которого нет в репозитории. Исторические записи сохранены как текст. | PASS: 172 Markdown-файла, 499 локальных ссылок. |
| ASan dump | VCAsan 14.44 фактически обрезал длинный путь и сохранял dump в родительском каталоге; harness искал только в каталоге сценария. Теперь используется уникальный короткий путь с переносом готового dump в артефакты сценария. | Самостоятельный instrumented probe с коротким путём создал dump; инфраструктурная проверка PASS, UAF остаётся FAIL. |
| UBSan Fast | На обоих CI-коммитах 65 вызовов завершились на `ExternalHelper.cpp:317`: MSHTML передавал результат `VARIANT` с выравниванием 4 байта, а типизированное чтение Clang требовало 8. Ещё один случай `release.search-core-suite.1` искал обычный `out/Release/FBE.exe`. Чтение дискриминатора выполняется через `const void*` и `memcpy`, вложенному тесту передаётся профильный путь. | До: 66 FAIL, без timeout; отдельный UBSan-пробник на невыровненном указателе и отрицательные sanitizer-пробы PASS. Полный Fast см. ниже. |
| Release provenance | Ранее авторитетная сборка в отдельном worktree не выполнялась, поэтому `CommonCore-Release.json` отсутствовал. | См. результаты сборки ниже; provenance вручную не создаётся. |

`binary_thumb` присутствует в массиве заголовков, но намеренно имеет пустой fallback и отсутствует в каталоге: это пустой заголовок столбца миниатюр. Другие пять ключей существуют и переведены на 12 языков. Добавление видимого перевода изменило бы существующий интерфейс; это расхождение с предположением ТЗ оставлено явно обозначенным.

CI-источники: ASan [до](https://github.com/sklart/fictionbook-editor-next/actions/runs/40661888972), [после issue #71](https://github.com/sklart/fictionbook-editor-next/actions/runs/40667911076); UBSan [до](https://github.com/sklart/fictionbook-editor-next/actions/runs/40661888722), [после](https://github.com/sklart/fictionbook-editor-next/actions/runs/40667911111). [Документация Microsoft](https://learn.microsoft.com/en-us/cpp/sanitizers/asan-offline-crash-dumps?view=msvc-170) подтверждает поддержку dump; ограничение длинного пути установлено локальным воспроизведением и stderr CI.

Коммиты: `e86f0488` — локализация и документация; `f7c7fcda` — ASan, COM и путь Search Core; `2f855a40` — runtime-проверка шести заголовков. Исходники продакшена issue #71 не менялись.

Изменены `tools/tests/test-fbe-runtime-dialog-coverage.ps1`, `tools/tests/description-layout-behavior.js`, два документа community-defects, `tools/tests/SanitizerHarness.ps1`, `tools/tests/test-ubsan-infrastructure.ps1`, `tools/tests/test-search-core-suite.ps1`, `src/fbe/ExternalHelper.cpp` и `src/fbe/scripts/VariantDiscriminator.h`. Этот отчёт добавлен отдельно.

## Сборки и оставшиеся проверки

| Проверка | Результат |
|---|---|
| Runtime dialog coverage, localization preflight, Markdown links | PASS; runtime coverage до исправления FAIL на `binary_id`; ссылки до исправления FAIL. |
| Production MSHTML: шесть ячеек вложений | PASS в `test-description-layout-runtime.ps1` на Release-сборке в Modern; общий тест также охватывает Classic. Пустой заголовок миниатюр намеренный. |
| ASan infrastructure | PASS; исходный длинный путь сохранял dump вне директории сценария. Два самостоятельных dump сохранены в `out/tests/asan/self-test-4156f0cc65b745c2b4426f78eafbc856/diagnostic-dumps.zip` с проверкой SHA-256. Отдельный полный ASan Fast не выполнялся. |
| UBSan infrastructure | PASS, включая намеренно невыровненный адрес и отрицательные пробы overflow, alignment, null, shift и function. |
| UBSan Fast | FAIL, код 1: 85 из 85 выполнены, 82 PASS, три FAIL ниже. Старый сбой выравнивания не обнаружен; Search Core после исправления пути PASS. Отчёт: `out/tests/ubsan/20261028-113924-e662c121d6c34a138439d738b52d6970/summary.json`. |
| Release Win32 /W4 /WX | PASS: `tools/build/build.ps1 -Configuration Release -Platform Win32 -StrictFirstPartyWarnings`. Бинарники собраны в этом worktree, sanitizer-профиль отдельно. |
| CommonCore и Runtime provenance | PASS: штатный конвейер создал манифесты, `-Action Validate` проверил SHA-256 восьми и пяти артефактов соответственно. `Release`, `Win32`, `v143`, MSVC 14.44.35207, `sanitizer=None`. Исходная проверенная сборка: `f7c7fcda482d8c474cada96f4c2e39ea7b902ec2`; последующие коммиты содержат тест и отчёт. |
| `verify-release.ps1 -Configuration Release -FullValidation` | FAIL, код 1: остановка на `test-fbe-split-container-production.ps1`, `split-section-middle`, exit 1. Следующие за этим тесты gate не выполнялись. Журнал: `out/ci-recovery-logs/verify-release-full.log`. |
| GitHub Actions для исправлений | NOT RUN: по ТЗ публикация и merge запрещены. |

Release и UBSan используют отдельно собранные бинарники, а не артефакты другого worktree. Полный gate выполнялся до коммитов теста и отчёта; фактический `gitCommit` последней авторитетной Release-сборки указан в её манифестах. Подмодули инициализированы на закреплённых gitlink, включая `pcre2/deps/sljit`; их версии не менялись. Общая готовность к релизу: **BLOCKED**.

### Новые блокеры, выявленные после устранения массового сбоя UBSan

1. `release.fbe-file-close-runtime.1`: после импорта EPUB и первого сохранения процесс завершился с `-529697949` в UBSan; повтор в обычном Release завершился с `-1073740791` на следующем этапе сохранения. Отчёт не содержит sanitizer-трассировки; первопричина аварийного завершения не установлена. Воспроизведение: `./tools/tests/test-fbe-file-close-runtime.ps1 -FbeExe ./out/Release/FBE.exe`. Логи `out/ci-recovery-logs/file-close-release.log` и результат сценария UBSan; тестовый профиль Release сохранён в системном Temp.
2. `release.fbe-image-document-inserter-runtime.1`: тест не обнаружил ссылку на `#cover-part-01.jpg` в результирующем документе. Воспроизводится также в обычном Release: `./tools/tests/test-fbe-image-document-inserter-runtime.ps1 -FbeExe ./out/Release/FBE.exe`. Sanitizer-трассировки нет; причина несовпадения пока не установлена. Лог `out/ci-recovery-logs/image-inserter-release.log`.
3. `release.fbe-split-container-production.1`: `split-section-middle` возвращает exit 1 и `before_equals_undo=0`. Адресный повтор в обычном Release подтвердил: после Undo в DOM остаётся второй раздел, поэтому исходное состояние не восстановлено. Начальная точка нарушения в реализации Undo не установлена. Воспроизведение: `./tools/tests/test-fbe-split-container-production.ps1 -FbeExe ./out/Release/FBE.exe -CaseId split-section-middle -KeepArtifacts`. Отчёт и структурная трасса сохранены в `out/ci-recovery-logs/split-middle-artifacts/`.

Каждый блокер требует отдельной диагностики и поведенческого исправления с регрессионным тестом; расширять ignorelist или отключать проверки нельзя. В UBSan ни один из этих трёх результатов не был timeout, и в них нет зарегистрированной UBSan-трассировки. Успешная компиляция и адресные проверки не означают готовности к релизу.

## Неуспешные сценарии исходного UBSan Fast

Во всех случаях код 1 и timeout=false. У 65 указан дефект выравнивания; у release.search-core-suite.1 отсутствовал файл обычной Release-сборки.

- `release.editor-background-runtime.1`
- `release.export-html-split-e2e.1`
- `release.fb2-quality-checker-runtime.1`
- `release.fbd-production-roundtrip.1`
- `release.fbe-archive-mru-runtime.1`
- `release.fbe-archive-runtime.1`
- `release.fbe-auto-url-detect.1`
- `release.fbe-binary-production-roundtrip.1`
- `release.fbe-binary-rename-runtime.1`
- `release.fbe-body-source-transition-runtime.1`
- `release.fbe-body-theme-runtime.1`
- `release.fbe-cite-poem-undo-production.1`
- `release.fbe-cli-argument-parsing.1`
- `release.fbe-clipboard-fallback-runtime.1`
- `release.fbe-context-attribute-bars-runtime.1`
- `release.fbe-document-lifecycle-runtime.1`
- `release.fbe-document-tree-window-runtime.1`
- `release.fbe-document-tree-window-runtime.2`
- `release.fbe-editor-view-lifecycle-runtime.1`
- `release.fbe-empty-image-placeholder-runtime.1`
- `release.fbe-failed-open-runtime.1`
- `release.fbe-fb2-binary-inspection-runtime.1`
- `release.fbe-file-close-runtime.1`
- `release.fbe-file-close-runtime.2`
- `release.fbe-file-close-runtime.3`
- `release.fbe-file-close-runtime.4`
- `release.fbe-image-document-inserter-runtime.1`
- `release.fbe-inputbox-layout-runtime.1`
- `release.fbe-link-navigation-runtime.1`
- `release.fbe-malformed-source-fallback-runtime.1`
- `release.fbe-navigation-scripts-runtime.1`
- `release.fbe-navigation-startup-mode-runtime.1`
- `release.fbe-new-document-safety-runtime.1`
- `release.fbe-new-document-safety-runtime.2`
- `release.fbe-reference-navigation-runtime.1`
- `release.fbe-save-as-mru-runtime.1`
- `release.fbe-save-decision-runtime.1`
- `release.fbe-script-document-path-runtime.1`
- `release.fbe-script-startup-validation-runtime.1`
- `release.fbe-settings-dialog-runtime.1`
- `release.fbe-source-xml-declaration.1`
- `release.fbe-spellcheck-russian-yo.1`
- `release.fbe-split-container-production.1`
- `release.fbe-status-bar-long-paint-runtime.1`
- `release.fbe-structural-commands-runtime.1`
- `release.fbe-structural-trace-contract.1`
- `release.fbe-table-attributes-runtime.1`
- `release.fbe-table-structural-production.plain.operation-insert-row-below.1`
- `release.fbe-theme-application-runtime.1`
- `release.fbe-themed-message-runtime.1`
- `release.fbe-ui-defects-runtime.1`
- `release.fbe-visual-dom-normalizer-runtime.1`
- `release.fbe-xml-script-backend-runtime.1`
- `release.fbe-xml-script-com-runtime.1`
- `release.fbe-xml-script-com-runtime.2`
- `release.fbe-xml-source-theme-v1-runtime.1`
- `release.interface-theme-policy-runtime.1`
- `release.issue43-menu-quality-visual-runtime.1`
- `release.issue64-runtime.1`
- `release.portable-diagnostic-cleanup-isolation.1`
- `release.prerelease-serialization.1`
- `release.regex-help-runtime.1`
- `release.script-toolbar-lifecycle-runtime.1`
- `release.script-toolbar-tooltips-runtime.1`
- `release.search-core-suite.1`
- `release.settings-editor-page-runtime.1`
