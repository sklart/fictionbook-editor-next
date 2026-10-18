# Модернизация диагностики: реализация и проверка

Дата проверки: 2026-10-18. Исходная база: `dafefd1cd239cedd885c68c88fb0e920e7e36561`.

## Реализовано

- GUID сеанса создаётся при каждом запуске, в том числе без расширенного журнала. Метаданные окружения относятся к выбранному сеансу; штатное завершение и неизвестный исход различаются.
- Сегменты журнала выбираются по точной границе имени и session_id, сортируются численно. Crash с явным session_id сопоставляется только по нему; приблизительное PID/time-сопоставление оставлено для legacy и явно помечено. Неоднозначный crash не подставляется.
- Свободный текст диагностических сообщений заменён типизированной проекцией разрешённых чисел и технических перечислений. Схема источников и конечные байты проверяются до экспорта; непроверенные обязательные данные блокируют сохранение.
- Ошибки плагинов через существующий HResult поступают в backend FBE. Самостоятельный процесс использует ограниченный fallback без текста операции. Portable-копии пишут в собственный Data/Diagnostics, installed имеет явный TEMP fallback.
- Единый native-диалог: состояние трассировки, сеансы, состав/размеры/пропуски, исключение компонентов, текстовый предпросмотр, сохранение, открытие исходников, двухшаговая очистка. Тема, DPI, геометрия и 12 языков подключены к существующей инфраструктуре. Идентификаторы прежних команд сохранены.
- Базовый пакет работает без трассировки. После предпросмотра неизменяемый снимок владеет всеми байтами; сохранение не перечитывает источники и не перезаписывает существующий файл. Сохранены текстовые отчёты, добавлен manifest.json v2 с составом и размерами, включая сами manifests.
- Crash handler устанавливается сразу после запуска диагностики; session_id сохраняется даже без получения trace-lock. Сохранены текстовый отчёт и MiniDumpNormal. Предыдущий crash отмечается в заголовке и выбирается в диалоге.
- Очистка учитывает crash, текущий сеанс и живые процессы, удаляет только известные диагностические файлы, сообщает размер и частичные ошибки. Добавлены текущие DPI/theme/memory/последние коды стадий, явно помеченные как данные текущего сеанса.

## Адресные проверки

Сборка проверялась на отдельном снимке исходной базы с наложением только перечисленных диагностических файлов: `build/diagnostics-validation/source`. Это исключает параллельные изменения HTML-вставки из проверяемой реализации. Использованы v143, VC Tools 14.44.35207, Release Win32, `FbeStrictWarnings=true` (/W4 /WX), C++17. Vendor-зависимости взяты из уже подготовленных build-каталогов; их новая сборка и release provenance не проверялись.

Проверенный EXE первого этапа (исторический результат): `build/diagnostics-validation/source/out/Release/FBE.exe`, SHA256 `e321d2d14f8032c169e14f41161057ee3d7dec418c19bd7ed97ad70597d40610`. Встроенный build commit в этой проверочной сборке: `dafefd1cd239-dirty`; это не бинарник, выпущенный из итогового коммита.

Успешно собраны FBE, ExportHTML, ExportDOCX, ExportEPUB и ImportEPUB. Логи: `build/diagnostics-validation/build.log`, `ExportHTML-build.log`, `ExportDOCX-build.log`, `ExportEPUB-build.log`, `ImportEPUB-build.log` в той же папке.

`tools/tests/test-diagnostic-sessions.ps1 -Executable <проверенный FBE.exe>` проверяет:

1. Базовый пакет без trace и уникальность сеансов.
2. Выбранный прошлый сеанс, числовой порядок сегментов и отказ от соседнего имени/чужого session_id.
3. Приоритет crash session_id над повторно используемым PID, безопасную проекцию crash, явный пропуск неоднозначного crash и отсутствие подстановки текущего окружения вместо потерянных старых метаданных.
4. Отказ от произвольного текста в trace и обязательных метаданных.
5. Маршрутизацию публичного HResult helper в backend хоста.
6. Побайтовое совпадение всех ZIP entries с предпросмотром, отсутствие записей после снимка, размеры manifests.
7. Исключение необязательного компонента и сохранность существующего ZIP при коллизии имени.
8. Реальное удаление старых диагностических файлов с сохранением crash, текущего сеанса и книги-сентинела.

Последний PASS production-сборщика: `build/diagnostic-session-test-c1779895fa3b49068c23f993450f8eec`. В этом сценарии настройки, TEMP и TMP изолированы; очистка удалила только данные теста.

`test-diagnostic-privacy-native.ps1`: компиляция /W4 /WX, типизированные поля, две отдельные portable-копии, отсутствие свободного текста, отказ при заполненном fallback без усечения старых данных. Доказательства: `build/diagnostic-privacy-native-65077e6bca3a49f6bee4d718199acbd2`.

Успешны адресные контракты: resource-id-safety, diagnostic-dialog-contract, diagnostic-localization, diagnostic-log-segments, diagnostic-trace-privacy, diagnostic-snapshot-contract, diagnostic-cleanup-contract, diagnostic-package, fbe-diagnostic-command-boundary, fbe-main-menu-catalog, diagnostic-runner-contract, fbe-trace-bridge, diagnostic-feature-control, fbe-typelib-diagnostics. Контракты UI подтверждают структуру и подключения, а не внешний вид.

Два общих UI-контракта имеют одинаковые ошибки в рабочем дереве и исходной базе:

- `test-fbe-runtime-dialog-coverage.ps1`: отсутствующая привязка `fbe.settings.spelling.write_failed` в IDD_SETTINGS_SPELLING.
- `test-fbe-dialog-layout-contract.ps1`: исходный размер страницы Source не удовлетворяет старому контракту.

Эти посторонние страницы в рамках задачи не исправлялись. Полный набор и verify-release.ps1 не запускались: задание ограничивает проверку короткими адресными сценариями. Это не release/package PASS.

## Дополнение: операции и активный режим редактора

Родитель изменения: `58eac66d6b582f86840e77cb01a3e0ce72849947`.

Добавлены фиксированные счётчики load/save/validate/script/view-switch:
started, completed (все завершения), failed (подмножество завершений), active,
total-ms, last-ms и max-ms. RAII завершает измерение на раннем возврате и при
C++-исключении. Отдельно сохраняются последняя завершённая операция с исходом
и самая недавно начатая из ещё выполняющихся. Время — GetTickCount64; оно
включает ожидания COM и диалогов. Успех script означает успешный HRESULT
вызова apiRunCmd, а не проверку результата произвольного пользовательского кода.

Одновременно отслеживается до 32 операций; переполнение увеличивает
stats-dropped. Новых потоков, таймеров, watchdog и фонового опроса нет. Сбор
работает без расширенного trace, не принимает произвольные имена операций,
пути и текст документов. Снимок форматируется вне блокировки счётчиков.

Интеграция: DocumentLoader::Load, оба метода DocumentSaveController,
Doc::SaveToFile для Validate, Doc::RunScript и CMainFrame::ShowView. Повторный
запрос уже активного режима не считается переключением. Активный режим
синхронизируется с EditorViewState после успешного/отклонённого перехода и при
исключении, включая случай фиксации состояния до исключения презентации.
Начальная установка документа помечает body; до неё значение unknown.

В пакет добавлен необязательный operation-statistics.txt; active-editor-mode
также включён в текущий runtime-state.txt. Статистика предпросмотра имеет
источник selected-current-session-at-preview, при штатном выходе итог
записывается в метаданные своего сеанса как selected-session-at-shutdown.
Старый сеанс без итога отмечается historical-session-unavailable. Исторические
поля проверяются строгой схемой; произвольный текст вместо числа блокирует
экспорт обязательных метаданных. Байты статистики входят в тот же неизменяемый
снимок, что и остальные компоненты ZIP.

### Проверки второго этапа

FBE собран Release Win32, v143 / VC Tools 14.44.35207, C++17, /W4 /WX.
Сначала собран весь затронутый проект, последующие уточнения проверялись
инкрементально только для изменённых translation units. Финальный лог:
`build/diagnostics-validation/operations-runtime-build.log`. Общие заголовки
плагинов не менялись; повторная сборка плагинов не требовалась.

Финальный EXE: `build/diagnostics-validation/source/out/Release/FBE.exe`.
SHA256: `49e2257058c1a789184850c3425c08b3acd8ce2e0388cf034cea4ca7bd8986b9`.
Встроенный build commit: `58eac66d6b58-dirty`; это проверочная сборка, а не
release-артефакт из окончательного коммита.

- **PASS** `test-diagnostic-sessions.ps1`: вложенность, активная операция,
  счётчики успеха/ошибки, исключение, измеримая длительность, исчерпание 32 мест,
  повторное завершение одного token, enum режимов, работа без trace;
  сохранение итога прошлого сеанса, отсутствие подстановки текущего,
  отклонение небезопасной статистики и совпадение ZIP с предпросмотром,
  включая изменение режима/операций после снимка. Доказательства:
  `build/diagnostic-session-test-e66919cbf71a4aa2ac50030b9c8b140f`.
- **PASS** `test-diagnostic-operation-runtime.ps1`: отдельная portable-копия,
  trace выключен, нативный сценарий до 25 секунд. Шесть реальных переходов
  Body/Description/Source сверяют диагностический режим с EditorViewState;
  реальная загрузка, успешная валидация и неудачный Save As со сохранностью
  имени/encoding/session. Итог: load completed=1, validate completed=1,
  save completed=1/failed=1, view-switch completed=8/failed=0 (включая два
  стартовых перехода), active-editor-mode=body. Доказательства:
  `build/diagnostic-operation-runtime-48059922d7bc49208b6ea4d65f2ea678`.
- **PASS** diagnostic-package, diagnostic-snapshot-contract,
  diagnostic-cleanup-contract, diagnostic-localization,
  fbe-document-save-boundary, fbe-editor-view-controller-boundary и
  git diff --check.

Дополнительный запуск старого editor-view-lifecycle-runtime завершился с
exit=1: его проверки identity/HTML-host дали нули, при этом mode/state и
счётчики записались. Артефакты:
`%TEMP%/fr-564f1bd6dd6d4881834ffc192fd2bd99`. На этапе статистики причина ещё не была изолирована; результаты расследования
и исправления приведены ниже. Новый целевой сценарий не заменяет его и не
подтверждает внешний вид диалога, Windows 7 или полный lifecycle редактора.

## Исправление старого lifecycle-теста (18.10.2026)

Падение воспроизведено на исходной базе
`dafefd1cd239cedd885c68c88fb0e920e7e36561`, до обоих диагностических коммитов.
Исходники извлечены через git archive в `build/lifecycle-baseline/source`;
использованы подготовленные зависимости, собран только FBE в Release Win32,
v143 / VC Tools 14.44, strict `/W4 /WX`. Сборка прошла. Лог:
`build/lifecycle-baseline/lifecycle-build.log`.
SHA256 исходного EXE: `A83F3640B9A0CF865F9E2D8996E8CABDF112589F0433605EAE94492257E36F8F`.
Тот же portable-сценарий с таймаутом 25 секунд вернул exit=1; значения
body_desc/desc_body/source_body/source_desc равны 0, как до исправления.
Сохранённые отчёт и SHA: `build/editor-view-lifecycle-5b26b7c04d014d809d922caec159994f`.

Причина: CContainerWnd искал активное окно через EnumChildWindows и
IsWindowVisible, учитывающий видимость предков. При скрытом окне редактора
поиск возвращал первый дочерний HWND вместо активной панели, а переключения
могли скрывать другую панель. Теперь выбираются только непосредственные
дочерние окна с собственным WS_VISIBLE; если все панели скрыты, возвращается
NULL. Изменение не затрагивает document ownership или Source transfer.

В старом CStringA::Format также было 14 аргументов при 13 полях: пропущено
source_desc_body, поэтому значения cycles/document/tree были смещены.
Восстановлено поле и его проверка в PowerShell. Ранее записанный document=0
сам по себе не доказывал потерю identity документа. Добавлена проверка
скрытого контейнера, скрытия всех панелей и повторной активации Body.
Runner запускается Hidden, сохраняет отчёт и SHA EXE, ограничен 25 секундами
по умолчанию и корректно цитирует пути.

Проверенная сборка содержит диагностику из `97351f2c141f` и это исправление,
без посторонних изменений текущей рабочей ветки. Release Win32 strict PASS;
лог `build/diagnostics-validation/lifecycle-final-build.log`, метка
`97351f2c141f-dirty`. SHA256 EXE:
`6AD60F77B5F16A77405AE12DE0A9448D588958758E4E2F0A1C53BA078AAB8DBB`.
Это проверочный EXE, не релизный пакет.

- PASS editor-view-lifecycle-runtime: все 15 полей равны 1, в том числе
  hidden_container, оба порядка Source/Body/Description, три цикла,
  identity документа и предпочтение дерева. Доказательства:
  `build/editor-view-lifecycle-d1b87291276d4222b2cb652152934d28`.
- PASS diagnostic-sessions (trace off/on, сеансы, privacy, операции,
  immutable ZIP): `build/diagnostic-session-test-976b363914b24c439950cbc0cd9b32d5`.
- PASS diagnostic-operations-runtime: реальные переключения режима, загрузка,
  валидация и безопасный неудачный Save As; trace отключён. Доказательства:
  `build/diagnostic-operation-runtime-9ce6a18fdc5d41faa850485c685b164e`.
- PASS lifecycle/controller/source-view-session boundary, diagnostic-package,
  diagnostic-cleanup-contract и git diff --check.
- Контракт editor-view-presentation-boundary падает на требовании
  CSplitterWindow как на исходной базе, так и сейчас: код использует
  IEditorViewSplitter. Этот исходный дефект контракта не исправлялся.

При первом запуске diagnostic-sessions проверочная сборка с произвольной
текстовой меткой build_commit была отвергнута privacy-проверкой (exit=94).
Метка заменена на допустимый SHA с -dirty, после чего тест прошёл;
проверки privacy не ослаблялись.

Диагностика зависаний остаётся отдельным этапом. Полный release gate,
визуальная проверка GUI/DPI и Windows 7 не выполнялись.

## Границы и оставшаяся работа

- Нет визуального подтверждения светлой/тёмной темы, разных DPI, клавиатурной навигации и геометрии. Computer Use недоступен: в сессии отсутствует обязательный для него node_repl.
- Запуск на Windows 7 и реальное аварийное завершение не проверены. Crash-сопоставление проверялось на контролируемых файлах; неблокирующий snapshot дополнительно проверен контрактом.
- Watchdog зависаний остаётся отдельным этапом. Статистика операций и активный режим добавлены во втором этапе ниже. При crash до штатного выхода итоговые счётчики прошлого сеанса могут отсутствовать; текущие значения вместо них не подставляются.
- Реальная загрузка каждого собранного DLL-плагина и ошибка его GUI-диалога не воспроизводились; проверены сборка потребителей общего заголовка и вызов того же публичного helper из headless FBE.
- Нет сетевой отправки. Дампы остаются отдельными файлами и требуют отдельной проверки перед передачей.

## Изменённые файлы

- `docs/building.md`
- `docs/reporting-issues.md`
- `docs/diagnostics-modernization-validation.md`
- `localization/app-ui/fbe-idr-mainframe-menu.json`
- `src/common/DeploymentContext.h`
- `src/common/DiagnosticLog.h`
- `src/common/DiagnosticPrivacy.h`
- `src/fbe/CrashHandler.cpp`
- `src/fbe/DiagnosticPackage.cpp`
- `src/fbe/FBE.cpp`
- `src/fbe/FBE.rc`
- `src/fbe/FBE.vcxproj`
- `src/fbe/StartupTrace.cpp`
- `src/fbe/StartupTrace.h`
- `src/fbe/diagnostics/DiagnosticCommandService.cpp`
- `src/fbe/diagnostics/DiagnosticCommandService.h`
- `src/fbe/diagnostics/DiagnosticDialog.h`
- `src/fbe/mainfrm.cpp`
- `src/fbe/resource.h`
- `src/fbe/testing/DiagnosticPackageTest.inl`
- `tools/tests/diagnostic-privacy-native.cpp`
- `tools/tests/test-diagnostic-cleanup-contract.ps1`
- `tools/tests/test-diagnostic-dialog-contract.ps1`
- `tools/tests/test-diagnostic-localization.ps1`
- `tools/tests/test-diagnostic-log-segments.ps1`
- `tools/tests/test-diagnostic-package-behavior.ps1`
- `tools/tests/test-diagnostic-package-without-trace.ps1`
- `tools/tests/test-diagnostic-package.ps1`
- `tools/tests/test-diagnostic-privacy-native.ps1`
- `tools/tests/test-diagnostic-sessions.ps1`
- `tools/tests/test-diagnostics.ps1`
- `tools/tests/test-fbe-diagnostic-command-boundary.ps1`
- `tools/tests/test-fbe-main-menu-catalog.ps1`
- `tools/tests/test-fbe-runtime-dialog-coverage.ps1`
- `tools/tests/test-fbe-trace-bridge.ps1`

- `src/fbe/FBDoc.cpp`
- `src/fbe/document/DocumentLoader.cpp`
- `src/fbe/document/DocumentSaveController.cpp`
- `src/fbe/testing/RuntimeTestArchiveAndLifecycle.inl`
- `tools/tests/test-diagnostic-operation-runtime.ps1`

- `src/fbe/ContainerWnd.h`
- `tools/tests/test-fbe-editor-view-lifecycle-runtime.ps1`
