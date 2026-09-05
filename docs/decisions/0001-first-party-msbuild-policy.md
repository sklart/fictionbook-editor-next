# 0001. Политика MSBuild подключается явно только собственными проектами

## Контекст

Официальная сборка и CI используют `v143` с VC Tools 14.44, а собственные
проекты по умолчанию содержали `v145`. Кроме того, standalone MSBuild назначает
`SolutionDir` каталогом самого проекта, что меняет пути вывода и include-пути.

## Решение

Собственные `.vcxproj` явно импортируют `tools/msbuild/FBE.Common.props`.
Файл вычисляет корень от собственного расположения, делает `SolutionDir`
корнем репозитория и задаёт `FbePlatformToolset=v143` по умолчанию. Внешнее
значение `PlatformToolset` остаётся допустимым явным override.

Vendored LunaSVG/PlutoVG и generated-проекты этот файл не импортируют.

## Последствия

IDE, `FBE.sln` и отдельный запуск first-party `.vcxproj` получают один
repository-rooted путь. Проверка `test-first-party-msbuild-policy.ps1` читает
вычисленные свойства MSBuild и запрещает возврат `v145` в собственные проекты.

