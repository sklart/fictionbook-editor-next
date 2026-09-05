# Runtime и упаковка

`runtime/` — вход сопровождаемых ресурсов, а не готовый выпуск. Официальный
поток строит отдельные Core и Integration staging-каталоги, затем из Core
материализует portable, а из обоих — вход для NSIS. Путь файла в установленном
продукте при этом не меняется.

| Категория | Источник | Назначение в staging | Правило происхождения |
| --- | --- | --- | --- |
| Редактор и просмотрщик | `out/<Configuration>/FBE.exe`, `FBV.exe`, `html.xsl` | корень Core | compiled artifact; хэш проверяется `build-provenance.ps1` |
| COM-плагины | `out/<Configuration>/Plugins/*.dll` | `Plugins/` | compiled artifact; список обязателен в manifest |
| Batch и ArchHandler | `out/<Configuration>/*.exe`, `out/archhandler/Win32/...` | корень, `Utilities/ArchHandler/` | compiled artifact; хэш проверяется provenance |
| Scintilla/Lexilla | сборка из `third_party` через `build-scintilla.ps1` | корень Core | runtime-копия заменяется результатом собственной сборки |
| Ресурсы runtime | `runtime/` | исходные относительные пути | сопровождаемые ресурсы; Core удаляет shell и plugin DLL из этой копии |
| Shell integration | `out/package/shell-build/<Platform>/...` | Integration: `FBShell.dll`, `FBShell64.dll` | отдельные Win32/x64 compiled artifacts |
| Property schema и installer tools | `packaging/property-schema`, `tools/build` | Integration | сопровождаемые inputs |
| Установщик и UAC | `packaging/nsis`, `third_party/uac` | NSIS build environment | внешняя бинарная/исходная зависимость, учтённая в third-party notices |

Пять исторических файлов первого запуска (`custom.dic`, `Hotkeys.xml`,
`languages.txt`, `root_genres.xml`, `Words.xml`) пока остаются в корне
репозитория, поскольку `stage-core.ps1` сохраняет их установочные имена и
первичный сценарий запуска. Их перенос в `runtime/defaults` допускается только
одним изменением карты источников, всех потребителей и проверок.

`packaging/layout.json` — единственная исполняемая карта `source →
destination`: её читают `stage-core.ps1` и `stage-integration.ps1`.
Обязательный состав и запреты staging-пакетов задаёт отдельный
`packaging/package-manifest.json`; `verify-package-stage.ps1` проверяет его
независимо от producer-скриптов. Это не список файлов для удаления: например,
отслеживаемые `FBShell.dll` и `ImportEPUB.dll` в `runtime/` заменяются или
исключаются при staging, а не считаются автоматически поставляемыми.
