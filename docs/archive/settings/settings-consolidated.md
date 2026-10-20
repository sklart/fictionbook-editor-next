# История доработки настроек: объединённый отчёт

> Исторические этапы. Содержимое всех трёх отчётов сохранено; их PASS/FAIL, базы и ограничения относятся к указанным сборкам.

Актуальная архитектура: [настройки](../../architecture/settings.md).

## Этап 1: settings-unification-report.txt

[Исходный файл](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/settings-unification-report.txt).

Отчёт по унификации настроек FictionBook Editor Next
Дата: 15.10.2026
Ветка: codex/settings-dialog-fixes
Основа: master 78d03025d2decf158af96a601830777302fc01ed.
Рабочее дерево: D:\Download\FBeditor\.worktrees\settings-unification.
Исходные незакоммиченные изменения основной рабочей папки сохранены отдельно.
SHA итогового коммита указан в сопроводительном сообщении; в этой ветке его показывает git log -1.

ПУНКТЫ ТЗ
1  Выполнено в коде. Новых зависимостей нет, COM/скриптовые API и vendor-контент не менялись. Проверка импортов Win7 пройдена; запуск на всех Windows 7–11 не выполнялся.
2  Выполнено. Отслеживаются ё, режим/атрибуты/ошибки парных тегов и все цветовые токены темы. Используются существующие адресные пути применения. Enter/Esc дочерних страниц и закрытие окна направляются через общую транзакцию.
3  Выполнено. Проверяется окончательное расширение; подтверждаются перезапись и переназначение занятой клавиши с названием команды. Экспорт UTF-8 TXT/HTML записывается во временный файл, завершается заменой; новое появившееся имя не перезаписывается без разрешения.
4  Выполнено. Слово меняется на месте с сохранением счётчика и служебных полей; проверка дубликатов не учитывает регистр и исключает редактируемую строку. Words.xml готовится до изменения глобальной модели; ошибки не закрывают диалог. Раздел переименован в «Слова с дефисом».
5  Выполнено. Явный JPEG/PNG соблюдается при включённом сохранении исходных данных. Пропуск кодирования разрешён лишь для совпадающего формата либо Авто. Подтверждение прозрачности сохранено. Действовавшее отключение качества для PNG оставлено: Авто и JPEG могут кодировать другие исходные форматы, поэтому качество там доступно. Подсказки уточнены, значения качества не менялись.
6  Выполнено. Импорт, создание и удаление тем откладываются до OK. Отмена сохраняет файлы и активную тему; цвета посещённых тем живут в черновиках. Сброс действует на выбранную тему. Добавлены цвет комментариев и его сохранение без смены идентификатора XMLSrcColorComment. Формат .fbetheme не менялся.
7  Выполнено. Недоступный встроенный фон имеет отдельный пункт; старый идентификатор сохраняется. Для ранее сохранённого недоступного изображения есть предупреждение в предпросмотре. Новый файл проверяется на доступность, JPEG/PNG и декодирование. NBSP имеет подписи, при сохранении используется символ. Поля выровнены, предпросмотр растягивается.
8  Выполнено. Настройка ё входит в применение; название и подсказка описывают подтверждённые ё-вместо-е случаи. Словарь только для чтения разрешён с предупреждением. Создание отсутствующего файла готовится при OK и сообщает об ошибке. Hunspell и пользовательские исключения не менялись.
9  Выполнено. Пользовательский путь скриптов сохраняется при временном выборе стандартного. Подсказка быстрого режима описывает упрощённое оформление; для загрузки скриптов и назначения клавиш отмечен перезапуск.
10 Частично по проверке. Компоновка переработана: шесть групп XML, одинаковые границы полей, более узкая базовая страница и растягиваемый предпросмотр. Все восемь страниц проверены при реальном 125% на ru-RU/en-US, компактные Editor/Source перепроверены после финальных правок. Синтетические WM_DPICHANGED 96/120/144 проверяют геометрию окна; полноценного визуального прогона при реальных 100% и 150%, включая перенос между разными мониторами, нет.
11 Выполнено. Поддерживаемые файловые операции настроек переведены на ModernFileDialog; выбор каталога скриптов оставлен штатному folder-dialog, поскольку общий Request не предоставляет режим выбора папки. Новые строки переведены на все 12 языков. Существующие оформление, высокая контрастность и всплывающие подсказки сохранены.
12 Частично по доказательствам: короткие адресные проверки и Release Win32 проведены; недостающая реальная DPI-матрица описана в п. 10. Длительный E2E, корпусные прогоны, полная языковая матрица и FullValidation не запускались.
13 Выполнено: функциональные изменения, сохранность данных, интерфейс/локализация и адресная проверка выполнены последовательными этапами. Итоговый коммит содержит только эту задачу.

ФАКТИЧЕСКИЕ ПРОВЕРКИ
PASS — Release Win32 FBE.vcxproj и FBEContracts, v143, VC Tools 14.44.35207, C++17, FbeStrictWarnings=true (/W4 /WX).
Команда: MSBuild.exe src/fbe/FBE.vcxproj /t:Build /m:2 /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:FbeStrictWarnings=true.
Журнал: build/settings-release.log. Финальная сборка: 0 ошибок, 0 предупреждений.
PASS — test-settings-file-transaction.ps1: немедленный откат после частичной ошибки, отмена подготовки, read-only, замена, удаление, очистка временных файлов.
PASS — test-image-import-native.ps1: реальное кодирование и определение JPEG/PNG, совпадающий формат без потерь, предупреждение прозрачного PNG -> JPEG при включённом keepSupportedImages.
PASS — test-fbe-hotkey-export-runtime.ps1: TXT/HTML ru-RU/en-US; ошибка замены заблокированного файла сохраняет прежние байты.
PASS — test-fbe-settings-general-layout-runtime.ps1 ru-RU/en-US: восемь страниц при 125%; затем ChangedPagesOnly после уменьшения ширины Source.
PASS — test-fbe-settings-dialog-runtime.ps1 -LayoutOnly: сохранение и повторный запуск, позиция/выделение/доступность Undo исходного редактора, счётчик слова, черновики цветов, resize и синтетические DPI 96/120/144.
PASS — test-settings-editor-page-runtime.ps1: 15 случаев страницы редактора, Automatic/BODY, сохранение недоступного фона и явный выбор «Нет»; 24 прозрачных PNG и 2 случая обновления кеша. Отдельные страницы вызывают Validate/Commit напрямую; общий OK проверяет сценарий полного диалога.
PASS — test-fbe-xml-source-theme-v1-runtime.ps1: формат/метаданные, импорт -> отмена, импорт -> фиксация, удаление -> отмена, удаление -> фиксация.
PASS — check-win7-imports.ps1 -IncludeNames FBE.exe (проверка импортов, не испытание на Windows 7).
PASS — контракты settings-lifecycle, hotkey-export, settings-tooltips, settings-semantic-localization, xml-source-themes, image-import, settings-background-boundary, keyboard-settings-ownership.
PASS — git diff --check.
UI-артефакты: out/tests/settings-layout-final и out/tests/settings-layout-compact.
Аудитор получает метрики текста из процесса FBE, не пытаясь выбирать чужой HFONT; координаты и снимки используют согласованный DPI-контекст.

ИЗВЕСТНЫЕ ГРАНИЦЫ
Нет полной проверки на реальных Windows 7–11, реальных 100%/150% и нескольких DPI-мониторах. Нет ручного аудита всех 12 языков и всех тем высокой контрастности.
Настройка цвета XML-комментариев не меняет существующую сохранность comment nodes после прохождения через визуальную модель документа.
Транзакция файлов защищает от проверенных ошибок записи/замены; восстановительные копии сохраняются, если внешняя блокировка мешает их восстановить. Устойчивость всей группы файлов к отключению питания не заявляется.
Новая проверка импорта не меняет предпочтение JPEG-качества и не вводит миграцию данных.
Релизный пакет, установщик, подпись и публикация в задачу не входили. Push не выполнялся.

ТЕСТИРОВАННЫЙ БИНАРНИК
out/Release/FBE.exe
SHA256: 2ec2e0a81a0424d6874191e1013feab241d49618de82b1e6374d09bcd5761b66
Собран из рабочего дерева до финальной фиксации; buildstamp содержит базовую ревизию и признак локальных изменений.

ИЗМЕНЁННЫЕ ФАЙЛЫ
docs/settings-unification-report.txt
docs/xml-source-themes.md
localization/app-ui/catalog.json
localization/app-ui/fbe-small-dialogs.json
src/fbe/FBE.rc
src/fbe/FBE.vcxproj
src/fbe/FBE.vcxproj.filters
src/fbe/ImageImport.cpp
src/fbe/XmlSourceThemes.cpp
src/fbe/XmlSourceThemes.h
src/fbe/mainfrm.cpp
src/fbe/resource.h
src/fbe/settings/SettingsFileTransaction.h
src/fbe/settings/SettingsSerialization.cpp
src/fbe/settings/hotkeys/HotkeyExport.cpp
src/fbe/settings/hotkeys/HotkeyExport.h
src/fbe/settings/ui/SettingsAdvancedPage.cpp
src/fbe/settings/ui/SettingsAdvancedPage.h
src/fbe/settings/ui/SettingsDlg.cpp
src/fbe/settings/ui/SettingsDlg.h
src/fbe/settings/ui/SettingsEditorPage.cpp
src/fbe/settings/ui/SettingsEditorPage.h
src/fbe/settings/ui/SettingsGeneralPage.cpp
src/fbe/settings/ui/SettingsHotkeysDlg.cpp
src/fbe/settings/ui/SettingsImagesPage.cpp
src/fbe/settings/ui/SettingsPageLifecycle.h
src/fbe/settings/ui/SettingsSourcePage.cpp
src/fbe/settings/ui/SettingsSourcePage.h
src/fbe/settings/ui/SettingsSpellingPage.cpp
src/fbe/settings/ui/SettingsSpellingPage.h
src/fbe/settings/ui/SettingsWordsDlg.cpp
src/fbe/settings/ui/SettingsWordsDlg.h
src/fbe/settings/words/WordsStore.cpp
src/fbe/settings/words/WordsStore.h
src/fbe/testing/RuntimeTestArchiveAndLifecycle.inl
src/fbe/testing/RuntimeTestPortableState.inl
tools/tests/image-import-smoke.cpp
tools/tests/settings-file-transaction-test.cpp
tools/tests/test-fbe-hotkey-export-contract.ps1
tools/tests/test-fbe-settings-general-layout-runtime.ps1
tools/tests/test-fbe-settings-lifecycle.ps1
tools/tests/test-settings-file-transaction.ps1
tools/tests/test-xml-source-themes.ps1
tools/ui/UiLayoutAudit.cs

## Этап 2: settings-followup-report.txt

[Исходный файл](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/settings-followup-report.txt).

ОСТАВШИЕСЯ ЗАМЕЧАНИЯ К НАСТРОЙКАМ FBE NEXT — ИТОГ
Дата: 2026-10-15
База: 80a97e47 (содержит исходные исправления dc1ea311).
Ветка: codex/settings-followup-fixes.
Рабочая папка: .worktrees/settings-unification.

1. ПОЛЬЗОВАТЕЛЬСКИЙ СЛОВАРЬ — ВЫПОЛНЕНО
Поиск прежнего источника выделен из U::GetUserDataFile и совместно используется существующей миграцией и страницей орфографии. Сохранён приоритет: каталог документа, файл рядом с программой, defaults. Установленная и portable-версии используют прежний U::GetSettingsDir / DeploymentContext. Каталог документа передаётся диалогу явно.
При отсутствии пользовательского файла содержимое старого копируется во временный файл. Источник не удаляется. Пустой файл создаётся только при отсутствии источника. У копии снимается атрибут только для чтения; исходник неизменен. Создание включено в общую SettingsFileTransaction. Режим createOnly не позволяет перезаписать словарь, появившийся после подготовки. Ошибки чтения и создания диагностируются.

2. УДАЛЕНИЕ XML-ТЕМ — ВЫПОЛНЕНО
Удаление неактивной пользовательской темы возвращает последний сохранившийся выбор с его черновиком цветов, включая тему при открытии настроек. При удалении исходной активной темы выбирается встроенная светлая или тёмная схема. Если удаляемая тема не выбрана, текущая сохраняется. Удаляется только черновик удалённой темы. Импорт и создание продолжают работать в существующей сессии редактирования тем. Отмена сбрасывает сессию без изменения файлов и глобальных настроек. Формат .fbetheme и идентификаторы тем не изменялись.

3. ОШИБКИ SETTINGS.XML — ВЫПОЛНЕНО
Сохранены void CSettings::Save и публичные COM/JavaScript-контракты. Добавлен TrySave с результатом и причиной ошибки. SettingsStore умеет подготовить Settings.xml в существующей файловой транзакции. Удалена неявная повторная запись из деструктора CXMLSerializer; явная сериализация использует уникальный временный файл с очисткой и сохранением кода Win32-ошибки.
Общий OK валидирует страницы, подготавливает изменённые файлы словаря, слов, тем и хоткеев, временно применяет значения в памяти с подавлением промежуточных Save, сериализует изменённые настройки и фиксирует весь пакет. При неудаче диалог остаётся открытым; значения, слова, признак перезапуска, локализация и тема интерфейса возвращаются к состоянию до попытки. Hotkeys.xml сохраняется в этом же пакете только при изменении сочетаний.
Применение настроек к редактору после OK больше не повторяет файловое сохранение. Геометрия входит в пакет OK; отмена и разрушение окна не выполняют скрытую запись Settings.xml. При отсутствии изменений сериализуемых значений Settings.xml не сериализуется заново. Сбой восстановления отдельно диагностируется; сохранённые восстановительные копии не удаляются. Новое сообщение и уточнение общего сообщения локализованы для всех 12 языков.

4. ПОВТОРНЫЕ ПРЕДУПРЕЖДЕНИЯ — ВЫПОЛНЕНО
Запоминаются исходный, текущий и уже предупреждённый путь. Неизменённый исходный словарь не вызывает предупреждения на каждом OK. Для нового пути предупреждение появляется при выборе или первой проверке и не повторяется до смены пути. Доступный для чтения словарь не блокирует сохранение; невозможность чтения остаётся ошибкой.

ПРОВЕРКИ
PASS — итоговая Release Win32 сборка FBE.vcxproj с FbeStrictWarnings=true (/W4 /WX), v143 / VC Tools 14.44.35207, C++17.
Команда: MSBuild.exe src/fbe/FBE.vcxproj /t:Build /m:2 /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:FbeStrictWarnings=true /v:minimal /nologo.
Лог итоговой сборки: out/tests/settings-followup-build-complete.log.
После первого запуска выполнялись только необходимые инкрементальные дособорки: исправлены неверное место условия в mainfrm.cpp и отсутствующий include теста; затем уточнялась автоматизация закрытия предупреждения. Повторных полных Rebuild не было.
PASS — test-settings-file-transaction.ps1: частичный отказ, немедленный откат, отмена подготовки, read-only, замена/удаление, очистка и createOnly с ERROR_FILE_EXISTS.
PASS — test-fbe-settings-dialog-runtime.ps1 -LayoutOnly: расширенный followup=1 проверяет копирование всех байтов старого словаря, защиту существующего файла, отмену подготовки, отсутствие предупреждения для исходного пути, ровно одно предупреждение для изменённого и повторное после смены; возврат темы и её цветов, последовательное удаление и отмену; отказ записи Settings.xml с восстановлением Words.xml, удаляемой темы и настроек в памяти; ошибку standalone TrySave при блокировке файла. Сохранены прежние проверки применения/повторного открытия, позиции и undo документа.
Два первых запуска runtime завершились тайм-аутом автоматизации предупреждения: системная единственная кнопка OK имела IDCANCEL. Тест теперь закрывает оба допустимых идентификатора таймером. Итоговый запуск полностью прошёл.
PASS — test-fbe-settings-lifecycle.ps1, test-xml-source-themes.ps1, test-fbe-hotkey-export-contract.ps1, test-fbe-settings-semantic-localization.ps1.
PASS — check-win7-imports.ps1 -IncludeNames FBE.exe.
PASS — git diff --check. runtime/Scintilla.dll и runtime/Lexilla.dll не изменены.
Полные E2E, корпусные проверки, verify-release и FullValidation не запускались.

ГРАНИЦЫ ПРОВЕРКИ
Runtime выполнен в изолированном portable-каталоге на текущей Windows. Проверка импортов подтверждает отсутствие обнаруженных прямых WinAPI-импортов, несовместимых с Win7; запуск на каждой Windows 7–11 и реальная установленная версия отдельно не испытывались. Общая маршрутизация установленной/portable-версии сохранена.
Файловый пакет восстанавливает согласованное состояние при проверенных ошибках. При внешней блокировке восстановления пользователь получает предупреждение, копии остаются для восстановления. Атомарность всей группы при отключении питания не заявляется.
Несвязанные незакоммиченные изменения основной папки не включены. Merge в master и push в рамках этого ТЗ не выполнялись.

ТЕСТИРОВАННЫЙ БИНАРНИК
out/Release/FBE.exe
SHA256: 5b2bcd5454d9d87c1552bd72ea4fe2716fd5c9609cca1fd40348eb0ef4646ce7
Собран до итогового коммита из рабочей копии на базе 80a97e47; buildstamp содержит базовую ревизию и локальные изменения.

ИЗМЕНЁННЫЕ ФАЙЛЫ
docs/settings-followup-report.txt
localization/app-ui/catalog.json
src/fbe/FBE.vcxproj
src/fbe/FBE.vcxproj.filters
src/fbe/Settings.cpp
src/fbe/Settings.h
src/fbe/XMLSerializer/XMLSerializer.cpp
src/fbe/mainfrm.cpp
src/fbe/mainfrm.h
src/fbe/settings/SettingsEditSession.h
src/fbe/settings/SettingsFileTransaction.h
src/fbe/settings/SettingsStore.cpp
src/fbe/settings/SettingsStore.h
src/fbe/settings/hotkeys/HotkeyStore.cpp
src/fbe/settings/hotkeys/HotkeyStore.h
src/fbe/settings/ui/SettingsDlg.cpp
src/fbe/settings/ui/SettingsDlg.h
src/fbe/settings/ui/SettingsHotkeysDlg.cpp
src/fbe/settings/ui/SettingsHotkeysDlg.h
src/fbe/settings/ui/SettingsSourcePage.cpp
src/fbe/settings/ui/SettingsSourcePage.h
src/fbe/settings/ui/SettingsSpellingPage.cpp
src/fbe/settings/ui/SettingsSpellingPage.h
src/fbe/testing/RuntimeTestArchiveAndLifecycle.inl
src/fbe/testing/SettingsFollowupProbe.inl
src/fbe/utils/Utils.cpp
src/fbe/utils/utils.h
tools/tests/settings-file-transaction-test.cpp
tools/tests/test-fbe-settings-dialog-runtime.ps1
tools/tests/test-fbe-settings-lifecycle.ps1
tools/tests/test-xml-source-themes.ps1

## Этап 3: settings-errors-nbsp-report.txt

[Исходный файл](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/settings-errors-nbsp-report.txt).

ОСТАТОЧНЫЕ ЗАМЕЧАНИЯ: СООБЩЕНИЯ ОБ ОШИБКАХ И NBSP
База: master 7c9a86af. Ветка: codex/settings-errors-nbsp.

1. ДУБЛИРОВАНИЕ ОШИБОК — ВЫПОЛНЕНО
Внутренний ISettingsPage::PrepareCommit принимает выходной CString с причиной. Страницы орфографии, слов, горячих клавиш и XML-тем возвращают ошибку общему диалогу без собственных окон сообщений. SettingsDlg очищает причину перед попыткой и показывает единственное локализованное сообщение, добавляя сведения страницы. При отказе окно настроек остаётся открытым, ошибочная страница выбирается.
Для файловых ошибок передаются путь и системная причина; при отсутствии доступного кода используется ERROR_WRITE_FAULT. Страница орфографии сохраняет прежние локализованные пояснения. XML-страница передаёт диагностическую строку существующего механизма тем. Новых строк локализации не требуется; прежние переводы и системный FormatMessage сохранены.
Порядок и механизмы копирования словаря, подготовки хоткеев, редактирования тем, файловой транзакции и сериализации не изменены. Изменены только передача и вывод диагностики.

2. ПОЛНЫЙ ОТКАТ NBSP — ВЫПОЛНЕНО
SettingsEditSession явно запоминает m_nbsp_char и m_old_nbsp. При отказе оба поля восстанавливаются вместе с прежним снимком. При успехе сохраняется обычная работа CSettings::SetNBSPChar: прежний текущий символ становится предыдущим для замены. Сам SetNBSPChar в Settings.cpp, форматы пользовательских файлов и публичные COM/JavaScript API не менялись.

ПРОВЕРКИ
PASS — Release Win32 FBE.vcxproj, v143 / VC Tools 14.44.35207, C++17, FbeStrictWarnings=true (/W4 /WX).
Команда: MSBuild.exe src/fbe/FBE.vcxproj /t:Build /m:2 /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:FbeStrictWarnings=true /v:minimal /nologo.
Основной запуск сборки прошёл сразу; выполнена одна необходимая инкрементальная дособорка после исправления тестовых данных. Логи: out/tests/settings-errors-nbsp-build.log и out/tests/settings-errors-nbsp-build-test-fix.log.
PASS — test-settings-errors-nbsp-runtime.ps1: один изолированный сценарий без layout, тем и словарных операций. Реальный PrepareCommit страницы слов получает некорректный XML; перехватчик считает ровно одно показанное сообщение, проверяет наличие пути Words.xml и открытое окно настроек. Отказ записи read-only Settings.xml после изменения NBSP сохраняет байты файла и восстанавливает различающиеся текущий и предыдущий символы. Повторная успешная попытка обновляет файл и оставляет правильную пару для последующей замены NBSP.
Результат: single_error=1, nbsp_rollback=1, nbsp_retry=1.
Первый запуск теста дал single_error=0 при успешных двух NBSP-проверках: управляющий символ не оказался надёжным XML-примером. Он заменён заведомо несогласованным тегом; итоговый запуск полностью прошёл.
PASS — test-fbe-settings-lifecycle.ps1: расширен контроль отсутствия MessageBox/DictionaryError внутри PrepareCommit страниц.
PASS — test-xml-source-themes.ps1: сохранены адресные контракты существующих тем.
PASS — git diff --check.
Длительные E2E, прежний расширенный followup-сценарий, корпусные проверки, verify-release и FullValidation не запускались.

ГРАНИЦЫ
Проверки выполнены на текущей Windows в изолированном portable-каталоге. Матрица Windows 7–11 заново не прогонялась. Новых зависимостей, WinAPI или форматов файлов нет. Несвязанные правки основной папки не затронуты. Merge и push в этой задаче не выполнялись.

ТЕСТИРОВАННЫЙ БИНАРНИК
out/Release/FBE.exe
SHA256: 4970ab2792d64a464dbee96dc7b4edffd4a9b2d2f18c21e69d9e33e99c8e5a60
Сборка до коммита на базе 7c9a86af; buildstamp содержит базовую ревизию и локальные изменения.

ИЗМЕНЁННЫЕ ФАЙЛЫ
docs/settings-errors-nbsp-report.txt
src/fbe/settings/SettingsEditSession.h
src/fbe/settings/ui/SettingsDlg.cpp
src/fbe/settings/ui/SettingsDlg.h
src/fbe/settings/ui/SettingsHotkeysDlg.cpp
src/fbe/settings/ui/SettingsHotkeysDlg.h
src/fbe/settings/ui/SettingsPageLifecycle.h
src/fbe/settings/ui/SettingsSourcePage.cpp
src/fbe/settings/ui/SettingsSourcePage.h
src/fbe/settings/ui/SettingsSpellingPage.cpp
src/fbe/settings/ui/SettingsSpellingPage.h
src/fbe/settings/ui/SettingsWordsDlg.cpp
src/fbe/settings/ui/SettingsWordsDlg.h
src/fbe/testing/RuntimeTestArchiveAndLifecycle.inl
src/fbe/testing/SettingsFollowupProbe.inl
tools/tests/test-fbe-settings-lifecycle.ps1
tools/tests/test-settings-errors-nbsp-runtime.ps1
