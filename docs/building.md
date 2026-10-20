# Сборка из исходников

> Тип: Действующая инструкция. Документационная база: FBE Next 3.0.9, `master` / `1ad7c507d1ed`; сверка 19 октября 2026 года.

## Требования и границы

Среда разработки: Visual Studio 2022 с C++ desktop tools, toolset `v143`, VC Tools ветки `14.44`, Windows SDK и PowerShell 7 (`pwsh`). Для официальной упаковки нужен NSIS. Исходники подмодулей должны соответствовать зафиксированным ревизиям репозитория.

Целевая среда готового приложения и среда сборки — разные вещи. Приложение ориентировано на Windows 7 SP1 и новее; сборка на самой Windows 7 и средствами PowerShell 2.0 из этого не следуют. Поддержка XP/Vista в эту поставку не входит. Не заменяйте toolset или CRT без проверки импортов Windows 7.

Собственный C++-код использует централизованный baseline C++17. Документационная реорганизация не меняет этот baseline, COM-контракт, пользовательские JS или third-party исходники.

## Обычная сборка

Из корня подготовленного репозитория:

```powershell
pwsh ./tools/build/build.ps1 -Configuration Release -Platform Win32
```

Для строгого first-party-контура доступны используемые проектом параметры:

```powershell
pwsh ./tools/build/build.ps1 -PlatformToolset v143 -SkipUpx -WarningsAsErrors
```

Окончательный набор параметров берите из `param(...)` актуального `tools/build/build.ps1`. Наличие старой команды в архивном отчёте не делает её официальным способом сборки.

## Что проверять после изменения

| Масштаб | Действие |
| --- | --- |
| Только Markdown, навигация и перенос отчётов | Проверка ссылок, якорей, карты переноса и сохранности контрольных файлов. Полная сборка приложения не требуется сама по себе. |
| Локальная правка подсистемы | Сборка затронутого проекта и связанные адресные тесты. |
| Завершённый межмодульный этап, упаковка, build/release | Официальная Release-сборка и `verify-release.ps1`. |
| Выпуск/tag или крупное системное изменение | Уместный FULL-контур и ручная матрица; результаты фиксируются отдельно. |

```powershell
pwsh ./tools/build/verify-release.ps1 -Configuration Release
pwsh ./tools/build/verify-release.ps1 -Configuration Release -FullValidation
```

Вторая команда не является обязательной после каждой небольшой правки. Подробности: [контуры](test-contours.md), [ручная регрессия](testing/manual-regression.md).

## Результаты и упаковка

Редактор и штатные EXE собираются в `out/Release`, плагины — в `out/Release/Plugins`. Это результаты сборки, а не окончательная структура всех пакетов.

Официальный путь упаковки: результаты сборки → раздельные **Core** и **Integration** stages → portable из Core и вход NSIS из обоих stages. `packaging/layout.json` задаёт исполняемую карту раскладки, `packaging/package-manifest.json` — обязательный состав и запреты. Сведения происхождения находятся в `out/build-provenance`.

Не копируйте в установщик произвольное содержимое `runtime/` и не считайте portable-каталог единственным входом NSIS. Сопровождаемые ресурсы и скомпилированные DLL проходят разные проверки. См. [runtime и упаковка](architecture/runtime-packaging.md).

```powershell
pwsh ./tools/build/create-release.ps1 -Configuration Release -Platform Win32
```

Эта команда создаёт артефакты. Условия публикации, feeds и проверки хешей описаны в [чек-листе выпуска](release-checklist.md).

## Зависимости и диагностические процедуры

Обновление библиотек — отдельная инженерная задача, а не побочный эффект актуализации документации. Действующие сервисные команды и сохранённая подробная инструкция находятся в [сопровождении зависимостей](reference/build-tooling.md). Прежде чем выполнять разрушающий шаг `apply`, используйте поддерживаемый командой dry-run и проверьте изменения источников, лицензий и хешей.

Каталоги пользовательских данных описаны в [настройках](user-guide/settings.md), диагностика — в [сообщении об ошибке](reporting-issues.md). В частности, текущая installed-версия использует `%LOCALAPPDATA%\FBE Next`, а не исторический каталог `%LOCALAPPDATA%\FBE`.

## Основания

- [tools/build/build.ps1](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/tools/build/build.ps1)
- [tools/build/create-release.ps1](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/tools/build/create-release.ps1)
- [docs/architecture/runtime-packaging.md](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/architecture/runtime-packaging.md)
- [src/common/DeploymentContext.h](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/src/common/DeploymentContext.h)
