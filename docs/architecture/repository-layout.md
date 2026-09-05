# Карта структуры репозитория

Этот документ описывает состояние дерева на HEAD `184952690f7ddc7bd19ae831b0e1b308a08a577`
(аудит 5 сентября 2026 года). Это базовая карта для поэтапной структурной
модернизации, а не разрешение на механическое перемещение файлов. Состояние
рабочего дерева в карту не включено.

## Верхний уровень

| Каталог/файл | Назначение | Статус содержимого |
| --- | --- | --- |
| `FBE.sln` | основной вход Visual Studio для Win32 Debug/Release | исходный файл |
| `src` | приложения, плагины и shell-интеграция | исходный код |
| `runtime` | JS/XSL/CSS, справочники и runtime-входы | исходные ресурсы и учтённые бинарные исключения |
| `localization` | источники локализации | исходные данные |
| `packaging` | NSIS-установщик и его ресурсы | исходные данные упаковки |
| `third_party` | закреплённые внешние зависимости | vendor; не подпадает под общие правила собственных проектов |
| `tools` | сборка, упаковка, тесты и сервисные сценарии | исходные сценарии |
| `build` | generated-файлы, obj и подготовленные зависимости | игнорируемый вывод сборки |
| `out` | бинарники, staging, отчёты и артефакты | игнорируемый вывод сборки |

`build` и `out` не являются местом для отслеживаемых настроек. Путь репозитория
сценарии определяют от собственного расположения (`tools/build/../..`), поэтому
внешний текущий каталог не должен быть их входом.

## Поставляемые компоненты

| Компонент | Проект/вход | Потребители и граница | Конфигурации и выход |
| --- | --- | --- | --- |
| Редактор | `src/fbe/FBE.vcxproj` | GUI, документ, host плагинов, COM/type library | Win32 Debug/Release; `out/<Configuration>/FBE.exe` |
| Shell-интеграция | `src/fbshell/FBShell.vcxproj`, `tools/build/build-shell-integration.ps1` | Explorer thumbnail/property handler; собирается отдельно также x64 | Win32/x64 Debug/Release; staging Integration |
| Просмотрщик | `src/fbv/FBV.vcxproj`, MUI-сценарий | отдельный EXE и MUI-ресурсы | Win32/x64 Debug/Release; `FBV.exe` |
| HTML-плагин | `src/export-html/ExportHTML.vcxproj` | контракт FBE и экспортный COM-плагин | Win32/x64; `Plugins/ExportHTML.dll` |
| DOCX-плагин | `src/export-docx/ExportDOCX.vcxproj` и Batch-проект | контракт FBE, экспорт | Win32/x64; `Plugins/ExportDOCX.dll` |
| EPUB-плагин | `src/export-epub/ExportEPUB.vcxproj` и Batch-проект | контракт FBE, экспорт | Win32/x64; `Plugins/ExportEPUB.dll` |
| EPUB-импорт | `src/import-epub/ImportEPUB.vcxproj`, Batch и LunaSVG adapter | контракт FBE, импорт | Win32/x64; `Plugins/ImportEPUB.dll` |
| ArchHandler | `tools/build/build-archhandler.ps1` | вспомогательная shell-интеграция | Win32 output в `out/archhandler` |

Основной solution включает FBE, FBShell, FBV и плагины. Batch-проекты,
LunaSVG/PlutoVG, ArchHandler, shell x64 и FBV MUI дополняются официальными
сценариями; поэтому инвентарь нельзя получать только из `FBE.sln`.

## Граф сборки и артефактов

```text
tools/build/build.ps1 (v143, VC Tools 14.44)
  ├─ подготовка PCRE2, Hunspell, image-библиотек и Scintilla/Lexilla
  ├─ FBE.sln /m (Win32)
  ├─ serial Build обязательных Batch/LunaSVG-проектов
  └─ out/<Configuration> + Plugins + provenance
       ├─ stage-core.ps1 ──> Core staging
       ├─ stage-integration.ps1 ──> Integration staging
       ├─ package-portable.ps1 ──> portable staging/ZIP
       └─ prepare-installer.ps1 ──> вход NSIS
```

`verify-release.ps1` остаётся публичным входом проверки: по умолчанию это
FAST, а `-FullValidation` добавляет полный контур; table-проверки включаются
явно через `-RunTableTests` либо FULL. Проверки provenance и состава пакета
независимы от staging и не должны заменяться простым чтением его списка файлов.

## Контракт плагинов и generated outputs

Единый legacy/v2 COM-контракт сейчас определён в `src/fbe/fbe.idl`. MIDL
производит `FBE.h`, `FBE_i.c` и `FBE.tlb`; первые два выходных файла исключены
из Git, но в настоящее время размещаются рядом с исходниками FBE. FBE,
экспортные проекты, ImportEPUB и тестовые harness-ы используют этот контракт.

Это известная переходная граница, а не public include-каталог всего `src/fbe`.
Будущий отдельный producer должен выдавать один набор файлов в
`build/generated/<Platform>/<Configuration>/fbe-api/`, а потребители должны
получить project dependency и точный include path. Перенос не должен менять
IDL, GUID/IID/CLSID/DISPID, порядок методов, calling convention или правила
владения памятью.

## Известные структурные границы для следующих этапов

- `FBShell.vcxproj` пока компилирует четыре реализации из `src/fbe`:
  `Fb2Metadata`, `Fb2CoverImage`, `Fb2CoverThumbnail`, `Fb2ShellProperties`.
  Их перенос допускается только одной связной группой с единым списком исходников
  либо эквивалентной библиотечной границей.
- В собственных `.vcxproj` зафиксирован `PlatformToolset=v145`, тогда как
  официальная сборка и CI передают `v143` и закрепляют VC Tools 14.44. До
  устранения расхождения фактическое значение нужно проверять через evaluated
  MSBuild properties, а не текстовый поиск XML.
- `FB::Doc` связан с `CFBEView`, HWND и MSHTML. Он остаётся частью приложения,
  пока отдельно не выделены операции с проверяемой границей без главного окна.
- `runtime` содержит и сопровождаемые файлы, и бинарные входы. Их происхождение
  определяется build/provenance и package-проверками, а не одним фактом наличия
  в Git.

## Правила изменений

1. Не менять ABI плагинов, размещение установленного приложения и public
   PowerShell-входы в структурной задаче.
2. Не распространять новые MSBuild imports автоматически на `third_party` и
   generated-проекты.
3. Каждый новый generated output получает единственного производителя,
   отдельные Platform/Configuration-пути и проверяемые входы.
4. Каждый перенос сопровождается обновлением `.vcxproj`, `.filters`, тестов и
   этой карты; поведенческая проверка не заменяется текстовым поиском.
5. Перед удалением runtime-бинарника нужно доказать его производителя,
   потребителей и восстановление с чистого checkout.
