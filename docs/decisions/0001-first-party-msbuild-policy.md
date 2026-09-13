# 0001. Политика MSBuild подключается явно только собственными проектами

## Контекст

Официальная сборка и CI используют `v143` с VC Tools 14.44, а собственные
проекты по умолчанию содержали `v145`. Кроме того, standalone MSBuild назначает
`SolutionDir` каталогом самого проекта, что меняет пути вывода и include-пути.

## Решение

Собственные `.vcxproj` явно импортируют `tools/msbuild/FBE.Common.props`.
Файл вычисляет корень от собственного расположения, делает `SolutionDir`
корнем репозитория, задаёт `FbePlatformToolset=v143` по умолчанию и через
`FbeLanguageStandard=stdcpp17` фиксирует baseline C++17 для исходников с
расширениями C++. Внешнее значение `PlatformToolset` остаётся допустимым явным
override; `FbeLanguageStandard=stdcpp20` предназначен только для локального
экспериментального прогона.

Vendored LunaSVG/PlutoVG и generated-проекты этот файл не импортируют.

## Последствия

IDE, `FBE.sln` и отдельный запуск first-party `.vcxproj` получают один
repository-rooted путь. Проверка `test-first-party-msbuild-policy.ps1` читает
вычисленные свойства и metadata `ClCompile` MSBuild: она запрещает возврат
`v145`, проверяет C++17 в Debug и Release, C++20 override и отсутствие
политики у vendored-проектов. C-исходники, включая ручной `buildstamp.c`, не
получают `/std:c++17`.

Это не меняет v143 / VC Tools 14.44, Windows SDK, Win7-compatible release
контур, CRT, `/permissive-` или `/Zc:wchar_t`.

## Пробный C++20-прогон

На VC Tools 14.44 прогона `FbeLanguageStandard=stdcpp20` для strict
first-party FBE недостаточно для успешной сборки. Первые группы ошибок —
устаревшие WTL/ATL-шаблоны, неявные преобразования COM/CString, const-строки
MSXML и расширение `for each`. Это отдельная C++20/conformance-миграция;
baseline C++17 и release-политику она не меняет.
