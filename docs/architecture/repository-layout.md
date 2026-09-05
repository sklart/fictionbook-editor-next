# Карта структуры репозитория

Исходный audit baseline — `184952690f7ddc7bd19ae831b0e1b308a08a577`
(5 сентября 2026 года). Эта карта обновляется вместе со структурной
модернизацией и описывает целевое сопровождаемое дерево, а не разрешение на
механическое перемещение файлов. Правила происхождения runtime-файлов и
путь от staging к portable/installer описаны в [runtime-packaging.md](runtime-packaging.md).

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

Единый legacy/v2 COM-контракт определён в `src/contracts/fbe.idl`. Проект
`FBEContracts.vcxproj` вызывает MIDL и производит `FBE.h`, `FBE_i.c` и `FBE.tlb`
в `build/generated/<Platform>/<Configuration>/fbe-api`; эти выходные файлы исключены
из Git и не появляются в `src/fbe`. FBE — единственный consumer, который
компилирует `FBE_i.c`; FBE, ImportEPUB и тестовые harness-ы получают заголовок
через project dependency и точный generated include path. Экспортные IDL
импортируют контракт из `src/contracts`, не через private каталог GUI.

Перенос не изменяет IDL, GUID/IID/CLSID/DISPID, порядок методов, calling
convention или правила владения памятью.

## Действующие структурные границы

- Общая реализация чтения FB2 и shell-свойств находится в `src/common/fb2`:
  `Fb2Metadata`, `Fb2CoverImage`, `Fb2CoverThumbnail`, `Fb2ShellProperties`.
  FBE и FBShell компилируют один и тот же список исходников; отдельная DLL не
  вводится. `src/common/fb2` не является portable core: его Windows/COM
  зависимости намеренно остаются явными.
- Используемая этим компонентом ATL/GDI+ обёртка расположена в
  `src/common/win32/atlimage.h`; она больше не получается через private include
  каталог редактора.
- Собственные `.vcxproj` явно импортируют `tools/msbuild/FBE.Common.props`.
  Он вычисляет корень репозитория без текущего каталога и задаёт `v143` по
  умолчанию; официальный build и CI по-прежнему закрепляют VC Tools 14.44.
  Vendored LunaSVG/PlutoVG не импортируют этот файл. Фактическое значение
  проверяется через evaluated MSBuild properties, а не текстовый поиск XML.
- `FB::Doc` связан с `CFBEView`, HWND и MSHTML. Он остаётся частью приложения,
  пока отдельно не выделены операции с проверяемой границей без главного окна.
- Host плагинов (`PluginManager` и `PluginApiV2`) расположен в
  `src/fbe/plugins`. Это editor-only COM/MFC-код, а не public contract и не
  common-компонент; его границу фиксирует `test-fbe-plugin-host-boundary.ps1`.
- Standalone helpers автодополнения и structural context расположены в
  `src/fbe/source`. Они не владеют DOM или UI, остаются частью редактора и
  проверяются `test-fbe-source-helpers-boundary.ps1` вместе с native smoke.
- Regex wrapper и его PCRE2 cache/match-loop расположены в `src/fbe/search`.
  Это editor-only поисковая подсистема; boundary и PCRE2 fixture-проверки
  сохраняют её независимость от координаторов окна и документа.
- `EditorBackgrounds` расположен в `src/fbe/settings`: это каталог и
  валидация настроек/runtime-ресурсов, а не владелец document/view. Его
  callers остаются в FBDoc, main frame и settings page.
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
