# Статус структурной верификации

Этот документ отделяет проведённые проверки от непроведённых. Он не заменяет
release notes и должен обновляться при новом authoritative прогоне.

## Подтверждено локально

- Evaluated MSBuild policy: собственные проекты используют `v143`, а прямые
  сборки FBE, ImportEPUB и shell-интеграции используют VC Tools 14.44.
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

После полной проверки штатный `create-release.ps1` сформировал и проверил
актуальные `FictionBookEditorNext-3.0.8-win32-portable.zip`, setup.exe,
symbols.zip и `SHA256SUMS.txt` в `out/artifacts`.

Перед этим исправлен тестовый запуск fail-closed Save: при наличии
`out/Release/portable.ini` он теперь явно выбирает `--installed`, а legacy
разбор аргументов корректно пропускает deployment-switches. Это сохраняет
реальную проверку fault injection, а не отключает её.
