# Выбор контура тестирования

> Тип: Действующая инструкция. Документационная база: FBE Next 3.0.9, `master` / `1ad7c507d1ed`; сверка 19 октября 2026 года.

## От правки к проверке

**LOCAL / affected**: собрать затронутый компонент и выполнить связанные адресные проверки. **STAGE**: после законченного логического блока подтвердить межкомпонентные связи и уместные runtime-сценарии. **RELEASE**: проверить авторитетную сборку и полный обязательный состав перед выпуском.

`verify-release.ps1` служит общим gate; его не нужно повторять после каждой строки документации. Отдельно запущенный source-contract подтверждает только своё условие, а не внешний вид GUI или работу на Windows 7.

## FAST и FULL

```powershell
pwsh ./tools/build/verify-release.ps1 -Configuration Release
pwsh ./tools/build/verify-release.ps1 -Configuration Release -FullValidation
```

Обычный контур проверяет обязательные release-контракты, staging/layout, импорты, локализацию и другие проверки, перечисленные в самом скрипте. FULL добавляет тяжёлые production round-trip, stress, performance, fault-injection и иные дополнительные сценарии. Наличие файла в `tools/tests` не означает его включения в gate.

Табличное подмножество можно запускать отдельно:

```powershell
pwsh ./tools/build/verify-release.ps1 -Configuration Release -RunTableTests
```

Не ведите здесь вручную второй полный список тестовых файлов: он быстро расходится с исполняемым кодом. Для машинного каталога предусмотрен:

```powershell
pwsh ./tools/tests/get-release-test-catalog.ps1 -AsJson
```

Контуры и конкретные ID можно выбирать поддерживаемыми параметрами каталога; неизвестный ID должен считаться ошибкой.

## Специализированные проверки

Installer/shell-проверки используют подготовленные stages и изолированную среду. Portable-проверки подтверждают собственные настройки и отсутствие нежелательных изменений профиля/реестра. Все плагины проверяются в современном каталоге `Plugins`.

Проект использует один универсальный Win32 runtime для Windows 7 SP1+; старую Modern/Win7 build-матрицу нельзя возвращать в инструкции. Нативные shell-компоненты проверяются с учётом разрядности Проводника.

[Ручная регрессия](testing/manual-regression.md) дополняет автоматические проверки. [Проверки Source](testing/source-editor.md), [DirectWrite](testing/source-directwrite.md), [Shell](testing/shell-integration.md), [regex](testing/regex-regression.md), [вставки](testing/html-paste.md) и [орфографии](testing/spellcheck.md) имеют отдельные чек-листы.

## Документация и контрольные данные

Документационный gate проверяет локальные ссылки, якоря, файл-карту и неизменность защищённых evidence-файлов. Он не запускает FBE, не объявляет пройденным Release и не выдаёт статическую сверку методов API за JScript runtime-тест.

[Реестр доказательств](testing/evidence.md) объясняет, почему некоторые JSON и один Markdown-источник остаются по прежним путям. Их нельзя удалять, форматировать или пересчитывать только ради чистоты каталога.

## Запись результата

Используйте `PASS`, `FAIL`, `NOT RUN`, `BLOCKED` с указанием commit, команд, окружения и артефактов. Для исторических прогонов дополнительно указывайте исходную ревизию и не заменяйте её текущим `master`. Реестр текущих пробелов — [статус верификации](testing/verification-status.md).

## Основания

- [docs/test-contours.md](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/test-contours.md)
- [docs/building.md](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/building.md)
- [tools/build/verify-release.ps1](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/tools/build/verify-release.ps1)
