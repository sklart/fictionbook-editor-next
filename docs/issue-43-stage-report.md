# Issue #43: локальная стадия интерфейсных исправлений

Состояние локальной ветки `master` относительно `origin/master` (`0db1a75a`). Это отчёт о реализации и коротких проверках, **не акт приёмки**. Изменения остаются локальными: публикация и общий CI запланированы вместе с issues #44, #49 и #45 согласно ТЗ.

## Исправленные компоненты и причины

| Компонент | Причина и исправление |
| --- | --- |
| Settings и восемь страниц | Минимум окна ранее смешивал клиентские и внешние размеры; фиксированные строки и списки перекрывались при узком окне, длинном переводе и повышенном DPI. Минимум теперь учитывает рамки и рабочую область; страницы перестраивают содержимое, длинные группы прокручиваются, положение сохраняется и ограничивается монитором. |
| Import EPUB | Исходные 700 × 920 не помещались на низком экране; ошибка создания окна могла приводить к импорту с настройками по умолчанию. Размер ограничен рабочей областью, параметры прокручиваются отдельно от нижних кнопок, подтверждение, отмена и ошибка различаются. |
| Export DOCX, EPUB и HTML | Фиксированная компоновка делала локализованные настройки недоступными при малой высоте и изменении DPI. Диалоги и страницы подстраиваются под доступный размер с сохранением прежней логики экспорта. Дополнительно в HTML Options добавлена прокрутка содержимого вкладок при рабочей области меньше исходной страницы; область tooltip поля структуры документа обновляется после компоновки и прокрутки. |
| Главное окно, Scripts Tree и toolbar | Размеры и сроки жизни изображений и шрифтов расходились при обновлении DPI. Ресурсы остаются живыми до привязки новых; размеры берутся из локальных UI-метрик окна. |
| Поиск, результаты и штатные диалоги | Длинные подписи, подсветка совпадений и подсказки выходили за поля или усекались; часть мнемоник совпадала. Исправлены геометрия, обрезка рисования, доступ к полному тексту через tooltip и переводы. Очень длинный `IDD_INPUTBOX` использует прокручиваемое поле. |
| FBV | Колонка имени и строка состояния не использовали новое пространство; кнопки не вмещали длинные переводы. Их размеры пересчитываются вместе с окном; ошибка загрузки схемы останавливает инициализацию. |
| UI Layout Audit | Отсутствовал структурированный аудит. Добавлен сбор HWND, текста, языка, DPI, прямоугольников, возможных перекрытий и обрезания с измерением шрифта. Намеренно выведенные за viewport элементы больше не отмечаются как выход за монитор, если у родителя есть действующий диапазон прокрутки по нужной оси. |

## Коммиты

| Блок | SHA |
| --- | --- |
| Settings | `0e0b7e81`, `4fb97f47`, `f0e94eeb`, `bfd2a1a6`, `2ab34d15`, `cf180de4`, `84913aad`, `ae6edb8f` |
| Import и Export | `ef0d88fe`, `d665b219`, `7799f91c`, `ecfa3488`, `852313d7`, `911d0eff` |
| Главное окно, Scripts Tree, GDI | `b5c5108c` |
| Поиск, диалоги и подсказки | `7adf806b`, `c9dc301a`, `fc36c593`, `4f597036`, `10640bed`, `0f4b2c1e`, `6fb73dad`, `a40c70fb`, `f45419ea`, `06a80efa`, `d843c180`, `eec8090b`, `757ffc02`, `0fec2126`, `657d422e` |
| FBV | `f6d750f9`, `11f5ed1f`, `b1a019fb` |
| UI Layout Audit | `221794be`, `f3ec249a` |

## Изменённые файлы

- Локализация: `localization/app-ui/fbe-small-dialogs.json`.
- Импорт и экспорт: `src/import-epub/ImportEPUBPlugin.cpp`, `ImportOptionsDialog.cpp`, `ImportOptionsDialog.h`; `src/export-docx/ExportDOCX.rc`, `ExportDOCXPlugin.cpp`; `src/export-epub/ExportEPUB.rc`, `ExportEPUBPlugin.cpp`; `src/export-html/ExportHTML.rc`, `HtmlExportOptionsDialog.cpp`, `HtmlExportOptionsDialog.h`.
- Редактор: `src/fbe/ArchiveEntryPicker.cpp`, `DocumentTree.cpp`, `FBE.cpp`, `FBE.rc`, `FBEview.h`, `FindResultsPane.cpp`, `FindResultsPane.h`, `ModelessDialog.h`, `Settings.cpp`, `Settings.h`, `Speller.cpp`, `Speller.h`, `TreeView.cpp`, `UiMetrics.cpp`, `UiMetrics.h`, `apputils.cpp`, `mainfrm.cpp`, `search/ui/RegexQuickReferencePopup.cpp`, `testing/RuntimeTestArchiveAndLifecycle.inl`, `toolbars/ToolbarFactory.cpp`, `ui/ContextAttributeBars.cpp`.
- Страницы настроек: `src/fbe/settings/SettingsDefaults.cpp`, `SettingsSerialization.cpp`; `src/fbe/settings/ui/SettingsDlg.cpp`, `SettingsDlg.h`, `SettingsGeneralPage.cpp`, `SettingsGeneralPage.h`, `SettingsTooltips.h`.
- FBV: `src/fbv/FBV.cpp`.
- Проверки и инструмент: `tools/build/verify-release.ps1`; `tools/ui/README.md`, `UiLayoutAudit.cs`, `invoke-ui-layout-audit.ps1`; `tools/tests/test-export-html-options-layout-runtime.ps1`, `test-fbe-about-dialog-layout-runtime.ps1`, `test-fbe-add-image-dialog-layout-runtime.ps1`, `test-fbe-archive-document-support.ps1`, `test-fbe-context-attribute-bars.ps1`, `test-fbe-dialog-layout-contract.ps1`, `test-fbe-dialog-mnemonics.ps1`, `test-fbe-inputbox-layout-runtime.ps1`, `test-fbe-navigation-scripts-contract.ps1`, `test-fbe-runtime-dialog-coverage.ps1`, `test-fbe-settings-dialog-runtime.ps1`, `test-fbe-settings-general-layout-runtime.ps1`, `test-fbe-spell-dialog-layout-runtime.ps1`, `test-fbv-layout-runtime.ps1`, `test-ui-layout-audit.ps1`.

## Выполненные короткие проверки

- Изолированное окно Import EPUB при 125% DPI: исходное и уменьшенное окна внутри рабочей области, вертикальная прокрутка `0 → 746`, нижние кнопки не перемещаются. Нижний Tab-порядок: Defaults → Import → Cancel; Esc отменяет только настройки. После прокрутки UI Layout Audit не нашёл замечаний в элементах этого окна. Снимок уменьшенного окна подтверждён визуально.
- `test-fbe-inputbox-layout-runtime.ps1`: длинный текст полностью остаётся в `Edit` с рабочей вертикальной прокруткой; окно и кнопки внутри рабочей области. PASS.
- Актуальный `out/Release/FBE.exe` собран из текущего `master`: затронутый проект FBE, Release Win32, v143 / VC Tools 14.44, строгие предупреждения — PASS. `test-fbe-settings-dialog-runtime.ps1` на этой сборке — PASS (Apply/OK, сохранение без резервной копии, переносимые настройки после перезапуска).
- `test-fbe-settings-general-layout-runtime.ps1` на актуальной сборке: восемь страниц Settings, подписи и кнопки General/Hotkeys для `ru-RU`, `en-US`, `de-DE` и `uk-UA` при текущем DPI — PASS, 0 замечаний UI Layout Audit на каждой проверенной странице. Для `es-ES` и `bg-BG` страницы General/Hotkeys — также PASS без замечаний. Снимки страниц «Изображения» и «Исходный код» просмотрены. Параметр `-Locales` позволяет выполнять короткую профильную проверку, не изменяя полный режим теста по умолчанию.
- `test-ui-layout-audit.ps1`: измерение текста, мнемоник, намеренных перекрытий и границ; различает доступное прокручиванием содержимое и недоступный элемент. PASS.
- `test-fbv-layout-runtime.ps1`: колонка имени, статус и изменение размера, 0 замечаний аудита. PASS.
- Контракты локализации: `test-runtime-interface-language-contract.ps1`, `test-export-docx-localization-resources.ps1`, `test-export-epub-localization-resources.ps1`, `test-fbe-dialog-mnemonics.ps1`, `test-fbe-main-menu-mnemonics.ps1`, `test-fbe-settings-tooltips.ps1`, `test-fbe-document-tree-tooltips-localization.ps1`. PASS, 12 локалей там, где это заявлено в самих проверках.
- Контракты интерфейса: `test-export-html-options-dialog.ps1`, `test-fbe-dialog-layout-contract.ps1`, `test-fbe-find-results-pane-contract.ps1`, `test-fbe-navigation-scripts-contract.ps1`, `test-fbe-context-attribute-bars.ps1`, `test-release-pipeline-deduplication.ps1`. PASS.
- После доработки HTML Options: `test-export-html-options-dialog.ps1` и инкрементальная сборка ExportHTML Release Win32 с v143 / VC Tools 14.44 и строгими предупреждениями — PASS. Прокрутка на малой рабочей области и tooltip после перемещения между мониторами остаются предметом общей GUI-приёмки.
- `test-export-html-options-layout-runtime.ps1`: в изолированном FBE при текущем DPI 120 отсутствуют лишние полосы прокрутки; ограниченный сценарий `WM_DPICHANGED` для 240 DPI даёт горизонтальную и вертикальную прокрутку с перемещением содержимого, а OK/Отмена остаются внутри окна — PASS. Это синтетическая проверка компоновки, не замена реального переноса между мониторами и DPI-матрицы.
- `git diff --check` для изменённых файлов при фиксации блоков. PASS.

Контрактные проверки доказывают структуру и полноту строк, а не внешний вид на каждом DPI. Изолированные GUI smoke доказывают только перечисленные окна и состояние машины, на которой они выполнялись.

## Отложенная общая приёмка

1. После интеграции issues #43, #44, #49 и #45 — общий `tools/build/verify-release.ps1` и GitHub Actions. Локальная сборка проекта FBE со строгими предупреждениями уже выполнена; согласованный общий Release build для всех затронутых проектов остаётся впереди.
2. GUI-матрица 100, 125, 150, 175 и 200% DPI для главного окна, восьми страниц Settings, поиска/замены, Full Help, Import EPUB, Export DOCX/EPUB/HTML, штатных диалогов и FBV; для основных окон — русский, английский, немецкий и украинский.
3. Перемещение окон между мониторами с разным DPI; сохранение и восстановление положения Settings; проверка малой рабочей области, минимального размера, прокрутки и достижимости кнопок.
4. Light, Dark и High Contrast: чёткость иконок, контраст, фокус, tooltip на отключённых элементах и после изменения размеров, доступные подписи для экранного доступа.
5. Клавиатура и файлы: Tab/Enter/Esc в основных диалогах, чтение/сохранение параметров, отмена и ошибки импорта/экспорта, длинные имена файлов и сообщения, отсутствие регрессий редактора и поиска.
6. Отдельная проверка на Windows 7 и анализ структурированных отчётов UI Layout Audit; снимки при найденных визуальных дефектах.

До выполнения этих пунктов нет доказательства полной приёмки и issue #43 остаётся открытым. Отдельные ограничения Win32/WTL и системных файловых диалогов не обходились ручным перепозиционированием элементов Windows Shell.
