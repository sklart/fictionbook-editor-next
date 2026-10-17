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

Проверенный EXE: `build/diagnostics-validation/source/out/Release/FBE.exe`, SHA256 `e321d2d14f8032c169e14f41161057ee3d7dec418c19bd7ed97ad70597d40610`. Встроенный build commit в этой проверочной сборке: `dafefd1cd239-dirty`; это не бинарник, выпущенный из итогового коммита.

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

## Границы и оставшаяся работа

- Нет визуального подтверждения светлой/тёмной темы, разных DPI, клавиатурной навигации и геометрии. Computer Use недоступен: в сессии отсутствует обязательный для него node_repl.
- Запуск на Windows 7 и реальное аварийное завершение не проверены. Crash-сопоставление проверялось на контролируемых файлах; неблокирующий snapshot дополнительно проверен контрактом.
- Нет watchdog зависаний (по ТЗ отдельный этап), автоматической статистики длительных операций и отдельного поля активной вкладки редактора. Имеются исходные timing поля trace, память/DPI/theme и последние коды стадий.
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
