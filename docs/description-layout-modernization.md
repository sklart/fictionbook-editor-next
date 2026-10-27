# Описание документа: Modern и Classic

Дата проверки: 27 октября 2026 года. Итог приёмки: **PARTIAL**: визуальный desktop smoke выполнен через computer use, матрица DPI ещё не выполнена. FAST-контур покрыт поэтапно, с одним нестабильным capture меню и успешным контрольным повтором.

## Реализация

Оба режима используют один комплект полей из `runtime/fb2.xsl` и общую цепочку `GetDesc` / `Make*` / `GetBinaries` / native Save. Modern перемещает существующие узлы формы и добавляет сворачиваемые секции; Classic возвращает эти же узлы на исходные позиции. Переключение не клонирует metadata, не перезагружает документ и не меняет binary storage.

Общие XSL templates: `ti`, `sti`, `di`, `pi`, `custom-info`, `authors_main`, `authors`, `author_main`, `author`, `seq`. Отдельных Modern/Classic сериализаторов нет. `runtime/main.js` остаётся единым production-файлом.

Настройка хранится в используемом deployment-профилем `Settings.xml`, в `/FBE/Settings[@ID="0"]/DescriptionLayout`. Значения: `modern` и `classic`.

- Новая конфигурация получает `modern`.
- Успешно загруженная прежняя конфигурация без ключа получает `classic`.
- Сохранённый выбор имеет приоритет; неизвестное значение нормализуется в `modern`.
- Миграция использует результат штатной десериализации; отдельного формата или migration-файла нет.

В Settings → General добавлен ComboBox «Описание документа». После применения Settings новый режим действует сразу: снимок конфигурации учитывает layout, native bridge вызывает `apiSetDescriptionLayout`, форма меняет представление на месте. Перезапуск не требуется. Параметр `descriptionLayout` передаётся и в native XSLT processor, и в реально используемый JS XSLT processor.

Modern содержит семь секций. Книга и Документ FB2 раскрыты первоначально. Название и существующие cover controls вынесены вперёд, добавлены preview через `fbw-internal`, понятные действия добавления и редактирования повторяемых элементов. Состояние секций сохраняется в открытой форме при переключении layout.

Оба режима получают Segoe UI/Tahoma, читаемые размеры, адаптивные поля, текстовые действия, focus/hover/disabled и цвета существующего ThemeManager. Обновлён также Description в `main_fast.css`; BODY-правила Fast Mode сохранены.

Вложения используют существующие DIV-строки и native payload facade. Добавлены миниатюры, действия для произвольных файлов, сводка количества/байтов и вызов существующей команды добавления вложения. Изображения первых 30 строк запрашиваются после раскрытия секции; остальные — при наведении на строку. Собственная миниатюра не считается FB2-ссылкой при удалении. Настоящие Body/cover references остаются защищены.

Validation предупреждает о пустых обязательных значениях/авторах, некорректном match/номере серии, повторяющихся binary ID и потерянной ссылке cover. Она не меняет значения. Штатная проверка FB2 остаётся источником окончательного результата.

Независимый production round-trip выявил и покрыл исправления общей логики:

- defaults программы, даты, версии и ID инициализируются при создании нового документа; открытие существующего не дописывает `program-used`;
- отсутствующая optional date не создаётся при сохранении; существующая пустая дата сохраняется;
- ID автора/переводчика сохраняется также при nickname-only записи и занимает корректное место в FB2;
- indentation не добавляется внутрь текстового содержимого stylesheet;
- потерянная cover reference остаётся в ComboBox и доступна validation.
- языковые теги вне встроенного списка (`zh-Hant`, `de-DE`) сохраняются без потери;
- defaults нового документа применяются только к штатному шаблону по полному пути; существующая книга `blank.fb2` сохраняет свой ID.

Новых публичных COM/DISPIDs, изменений `fbe.idl`, формата FB2, формата binary storage, BODY/Source editors или разделения `main.js` нет. Внутреннее действие добавления вложения использует private dispatch bridge и существующий native command.

### Изменённые файлы

- `runtime/fb2.xsl`, `runtime/main.js`, `runtime/main.css`, `runtime/main_fast.css`;
- `src/fbe/Settings.h`, `src/fbe/settings/SettingsDefaults.cpp`, `SettingsSerialization.cpp`, `SettingsStore.cpp`, `SettingsStore.h`;
- `src/fbe/settings/ui/SettingsGeneralPage.cpp`, `SettingsGeneralPage.h`, `src/fbe/FBE.rc`, `src/fbe/resource.h`;
- `src/fbe/FBDoc.cpp`, `src/fbe/mainfrm.cpp`, `src/fbe/ExternalHelper.cpp`;
- `localization/app-ui/fbe-small-dialogs.json`: новые строки во всех 12 production-языках;
- `src/fbe/testing/SettingsFollowupProbe.inl`, `RuntimeTestBinaryScriptingCompatibility.inl`;
- `tools/tests/test-description-layout.ps1`, `test-description-layout-runtime.ps1`, `test-description-roundtrip.ps1`, `description-layout-behavior.js`, `test-fbe-main-js-binary-behavior.ps1`, `test-fbe-main-js-reliability.ps1`;
- семь FB2 fixtures в `tools/tests/fixtures/description/`;
- `tools/build/verify-release.ps1`: XSL/fixture contract в FAST; production UI/stress и round-trip в FULL;
- `tools/tests/test-fbe-runtime-dialog-coverage.ps1`: конкретный JS consumer и отрицательные проверки динамических ключей Description;
- этот отчёт.

## Проверки

| Проверка | Результат и граница доказательства |
|---|---|
| Modern layout | PASS — MSXML 6 и production MSHTML DOM |
| Classic layout | PASS — общие поля, восстановление исходных позиций |
| Modern → Classic | PASS — значения, identity controls, общая сериализация |
| Classic → Modern | PASS — повторные циклы без дублирования секций |
| FB2 round-trip | PASS — шесть корректных fixtures, два Save/reopen в каждом layout; независимое сравнение XML metadata, stylesheet и binary bytes |
| Binary | PASS — production add/rename/remove, payloads, references; non-image Save handler; native store/ABI и JS behavior |
| Cover | PASS — rename references, сохранение ссылки, миниатюра и полный просмотр через GUI |
| Light theme | PASS — вычисленные MSHTML цвета формы/полей и сохранность данных |
| Dark theme | PASS — Light → Dark → Light для обоих layouts |
| Native Settings | PASS — новая/прежняя конфигурация, modern/classic/неизвестное значение, сохранение и повторная загрузка |
| DPI Description 100/125/150/200% | NOT RUN |
| Localization preflight | PASS — 12 production-языков и runtime JSON/export contracts |
| Release Win32 `/W4 /WX` | PASS — affected FBE после migration helper; лог `out/description-strict-build.log` |
| Existing affected regression suite | PASS — Settings lifecycle, BinaryStore/ABI, binary JS behavior/serialization, themes/idle, New Document safety и FBD round-trip; binary scripting 46/46 и bundled corpus 6/6 |
| Release FAST | PASS поэтапно — пройденные runtime-наборы переиспользованы; UI menu capture потребовал контрольного повтора; заключительные PE/version/manifest проверки PASS. Единого непрерывного зелёного запуска нет |
| Визуальный desktop smoke | PASS в описанном ниже контуре — Modern/Classic, Light/Dark, редактирование, импорт/экспорт вложения, Save/reopen |

`test-description-layout-runtime.ps1` запускает реальный изолированный FBE, применяет Light/Dark/Light и проверяет оба layout на A–G fixtures плюс 100/500 binary и 30 авторах. Это не mock браузера. Сравниваются значения INPUT/SELECT/TEXTAREA, identity полей, количество секций, результат общей сериализации, validation и binary операции.

`test-description-roundtrip.ps1` использует настоящий CLI Save/reopen и независимый XML-оракул, учитывающий namespace URI, порядок элементов, атрибуты и значения. Binary сравниваются после base64 decode. Проверяются пустая/отсутствующая дата, nickname-only ID, translator/source author ID, Unicode, вложенные sequence, custom-info и stylesheet.

Измерения текущего ПК находятся в `out/tests/description-layout/description-timings.txt`; отчёт и хеши producer/assets — рядом в `report.txt` и `summary.json`. Для 500 малых binary и 30 авторов загрузка занимает примерно 2,5 с; три переключения с validation/сериализацией — примерно 0,6 с. Это измерения данного fixture, без оценки субъективной отзывчивости на других документах и DPI.

Сборка и gate используют штатный v143 / VC Tools 14.44. Generated outputs, vendor sources и dependency DLL не включены в коммиты. Штатная сборка проверяет неизменность tracked Scintilla/Lexilla.

Source-safety выявил смешанные окончания строк в 17 ранее чистых исходных файлах. Выполнена только нормализация CRLF после проверки совпадения канонического содержимого с HEAD; содержательные изменения и dirty-файлы не затронуты. Повторный source-safety — PASS.

### Воспроизведение

Из корня с `FBE.sln`:

```powershell
& .\tools\tests\test-description-layout.ps1
& .\tools\tests\test-description-layout-runtime.ps1 -FbeExe .\out\Release\FBE.exe -TimeoutSeconds 90
& .\tools\tests\test-description-roundtrip.ps1 -FbeExe .\out\Release\FBE.exe
& .\tools\tests\test-fbe-settings-dialog-runtime.ps1 -FbeExe .\out\Release\FBE.exe -LayoutOnly
```

Логи сборок и release gate: `out/description-*.log`. Эти файлы и `out/tests` являются generated evidence.

Первый полный FAST-проход остановился на ошибочной привязке новых JS-строк к Settings dialog. Каталог исправлен на `runtime/main.js`; runtime dialog coverage (725 ключей) и localization preflight повторно прошли. Повторный gate выполнялся со штатным `-SkipEarlyRuntimeSuites`, используя уже пройденные runtime-наборы из первого лога. Три оставшиеся исключаемые проверки (`test-portable-scripts-infrastructure`, `test-nsis-install-scopes`, `test-prerelease-nsis-state`) выполнены отдельно — PASS. Повторный gate остановился раньше на `test-fbe-ui-defects-runtime.ps1`: `menu_display_capture=0`, хотя в первом проходе этот контур был PASS. Отдельный повтор с теми же assertions — PASS. Это нестабильность capture меню, а не доказательство устойчивого зелёного непрерывного gate.

Для завершения без повтора уже пройденных длинных runtime-наборов созданы временные runners в `out`: исходные начальные проверки/инициализация и неизменённый хвост `verify-release.ps1` после runtime dialog coverage. Первый хвост прошёл остальные runtime/NSIS/Scintilla шаги и выявил отсутствующий generated `Lang/Shell/FBVVerbResources.dll`. Штатные `build-fbv-verb-mui.ps1` и `build-shell-integration.ps1` подготовили MUI и Win32/x64 shell binaries без установки/регистрации. Заключительный хвост PE imports, ASLR/DEP/CFG, версий, symbols, manifest и DLL snapshot завершился успешно. Production gate/assertions не менялись. Результат обозначен как поэтапное покрытие FAST, а не один непрерывный зелёный запуск.

Логи: `out/description-release-gate.log`, `out/description-release-gate-resumed.log`, `out/description-ui-defects-retry.log`, `out/description-release-gate-remaining.log`, `out/description-shell-preparation.log`, `out/description-release-gate-final.log`. Временные runners: `out/description-fast-remaining.ps1`, `out/description-fast-final.ps1`.

### Визуальный smoke через computer use

После перезапуска Codex native computer use заработал. Проверялось окно именно `out/Release/FBE.exe`; путь запущенного процесса подтверждён отдельно через CIM. Использована disposable-копия `out/manual-description-smoke/description-full-20261027-074918.fb2`, исходные fixtures не редактировались.

Через интерфейс выполнены:

- Modern → Classic → Modern и смена Light/Dark при открытом Description; данные автора, названия и выбранной обложки сохранились, перекрытия формой Source после применения Settings нет;
- редактирование названия в Modern, добавление и заполнение автора, редактирование имени в Classic; Save, закрытие и повторное открытие книги, независимое чтение сохранённого XML;
- импорт тестового BMP 160×220 штатной командой: FBE преобразовал его в JPEG `visual-smoke-cover.jpg`, показал размеры/миниатюру и обновил число вложений до трёх; прежние `cover.png` и `blob.dat` сохранились;
- выбор нового JPEG обложкой, полный просмотр и закрытие просмотрщика кликом по изображению; после закрытия лишняя горизонтальная полоса исчезает;
- сохранение JPEG из списка вложений; экспортированный `out/manual-description-smoke/visual-smoke-cover.jpg` побайтно совпал с decoded binary книги (2530 байт);
- добавление пустого автора вызывает предупреждение; ввод имени и уход из поля снимают его. Кнопка добавления остаётся после последней строки. Итоговый Save через toolbar завершился сообщением «Документ сохранён. Ошибки не обнаружены.»

Визуальная проверка выявила и привела к исправлениям: переносы и разъезд подписей автора/даты, расположение cover controls, нечитаемые тёмные LEGEND, неверная обработка изменения layout как Source-only, перекрытие Description редактором Source после Settings, зависшее advisory validation, положение кнопок добавления после вставки строки и лишняя полоса прокрутки после полного preview. Общая DOM-модель и сериализация сохранены. Поля автора и даты получили общие обёртки в XSL; CSS одинаково обновлён в обычном и Fast Mode stylesheet.

Осмотрены обычное и максимизированное окно; в очень узком окне поля остаются доступны, но сохраняется горизонтальная прокрутка. Полный проход всех секций в узком окне и матрица DPI не заявляются PASS. Удаление автора/жанра/binary в GUI в этот smoke не входило; соответствующие production behavior tests прошли отдельно.

После исправлений прошли affected Release `/W4 /WX`, штатная Release-сборка, MSXML/XSL contracts, JS reliability/binary behavior, XML Source themes, production MSHTML behavior и round-trip шести fixtures в обоих layouts. Новые regression assertions проверяют цвет LEGEND, validation через настоящий `focusout` и закрытие полного preview. Логи: `out/description-smoke-strict-final.log`, `out/description-smoke-build-final.log`, `out/description-smoke-build-assets.log`, `out/description-smoke-runtime-preview.log`, `out/description-smoke-roundtrip.log`. Полный release gate повторно не запускался.

## Оставшиеся gates и технические долги

Остаётся ручная матрица узкое/обычное/максимизированное окно при 100%, 125%, 150% и по возможности 200% DPI. Дополнительный GUI-контур удаления автора/жанра/вложения также остаётся вне выполненного smoke.

### Дополнение: плотная компоновка

В обоих режимах уменьшены вертикальные отступы групп и полей, подписи авторов и дат на широком экране размещены рядом с полями. Повторяющиеся действия представлены компактными символами с локализованными `title` и доступным именем; однократные действия сохраняют текст. Миниатюры обложки и binary и полный просмотр получили рамку с цветами текущей темы. Для подписи и списка настройки компоновки добавлена общая локализованная подсказка. Modern сохраняет собственный порядок и сворачиваемые секции, Classic — исходный порядок. На узком экране классы, обновляемые при resize, заменяют неподдерживаемые в MSHTML 7 media queries; длинные заголовки переносятся.

Проверены Release Win32 `/W4 /WX`, штатная Release-сборка, localization preflight, Settings layout, XSL, JS reliability/binary behavior и production MSHTML. Последний проверяет Light/Dark, Modern/Classic, обычный/Fast stylesheet, геометрию 320/480/640/960 px, длинные заголовки, подписи в одной строке на широкой форме, icon actions и рамки, а также сохранность полей и сериализации. Логи: `out/description-compact-strict.log`, `out/description-compact-build.log`, `out/description-compact-localization.log`, `out/description-compact-settings.log`, `out/description-compact-runtime-final.log`. После сборки последняя CSS-правка скопирована в `out/Release`; хеши ресурсов совпадают с исходниками.

Визуальная приёмка дополнения остаётся **PARTIAL**: установленный старый FBE показал ошибку MSXML6 `0x80070005` (доступ запрещён), а при открытии Description в сохранённой предмодернизационной сборке и текущем Release computer use дважды вернул `computer-use request timed out: click`. Поэтому три сопоставимых снимка и критерий высоты Classic относительно исходного интерфейса не подтверждены. DPI-матрица и визуальная проверка последней компоновки также не выполнены; предыдущий smoke относится к состоянию до этого дополнения.

Нестабильный capture меню в общем UI-defects runtime требует отдельного наблюдения: один FAIL между двумя PASS. Тест и его assertions не ослаблялись; изменение меню не включено в scope модернизации Description.

Предыдущий блокер computer-use (`failed to write kernel assets: ... (os error 3)`) снят после перезапуска Codex; он больше не препятствует desktop smoke. DPI-проверки не подменяются ни этим осмотром, ни Settings/runtime тестами.

Состояние раскрытия секций пока не сохраняется между запусками. Legacy `document.all` и существующие повторяющиеся ID оставлены в общей модели. `main.js` остаётся монолитным намеренно. Полная schema-driven генерация и отдельный валидатор не вводились.

Полный `-FullValidation`, реальный installer lifecycle, release/tag, публикация пакета и remote CI не выполнялись. Push не выполнялся. Независимые изменения `spellcheck-corpus` и появившиеся параллельно изменения binary/perf сохраняются вне коммитов этой задачи. Итоговая локальная сборка использует общее рабочее дерево; результаты не являются доказательством чистой сборки только коммитов Description.
