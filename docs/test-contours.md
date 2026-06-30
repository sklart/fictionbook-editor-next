# Карта тестовых контуров

Этот файл фиксирует, какие сценарии в `tools/tests` входят в активный
контур проверки проекта, а какие используются точечно для диагностики,
ручных проверок или исторического сравнения.

Цель документа — не дать случайно удалить нужный smoke/regression-сценарий
во время дальнейшей cleanup-ревизии `tools/tests`.

## 1. Базовый release-gate

Эти сценарии относятся к основному build/test-контуру и должны считаться
первым кандидатом на обязательный прогон после заметных изменений в runtime,
релизной упаковке или редакторных зависимостях.

- `test-source-safety.ps1` — защита исходников и release-артефактов от
  опасных регрессий упаковки.
- `test-update-manifest.ps1` — проверка `update.xml`.
- `test-scintilla.ps1` — smoke/regression по редакторному движку.
- `test-pcre2.ps1` — базовые regex-кейсы текущего runtime.
- `test-pcre2-wrapper.ps1` — проверка wrapper-поведения режима `Дизайн`.
- `test-pcre2-replace.ps1` — сценарии замены и backreference.
- `test-spellcheck-dictionaries.ps1` — smoke орфографии и словарей.
- `test-fbe-startup.ps1` — GUI startup smoke.

Практическое правило:

- после обновления `Scintilla` / `Lexilla` / `PCRE2` / `Hunspell` этот набор
  нужно считать минимальным обязательным;
- если менялся только installer/layout, часть этого набора можно запускать
  выборочно, но `test-source-safety.ps1` и `test-update-manifest.ps1`
  остаются обязательными.

## 2. Shell / installer release-gate

Эти сценарии обязательны, если менялись shell-регистрация, installer-helper,
`FBShell`, `FBV`-verb, staging `out/package` или NSIS-контур.

- `test-fbe-specific-installer.ps1` — end-to-end install/uninstall smoke для
  schema, property handler, thumbnail provider и shell-verb `Validate`.
- `check-fbe-shell-registration-consistency.ps1` — согласованность shell-
  строк и регистрационных ожиданий.
- `check-fbe-specific-properties.ps1` — published FBE-specific properties.
- `check-fb2-shell-surfaces.ps1` — быстрая сводка по shell-поверхностям
  `.fb2`.
- `test-fbv-verb-muiverb.ps1` — проверка локализованного `MUIVerb` через
  `SHLoadIndirectString`.

Практическое правило:

- для обычных изменений только в `src/fbe` эти тесты не являются
  повседневным блокером;
- для любого изменения в shell/installer-контуре именно они становятся
  главным release-gate.

## 3. Thumbnail / shell-диагностика

Это активный и нужный контур, но он не весь одинаково обязателен в каждом
релизе. Его удобно делить на автоматический минимум и расширенную ручную
диагностику.

### 3.1. Автоматический минимум

- `test-fb2-thumbnail-provider.ps1` — прямой COM smoke provider.
- `test-fb2-thumbnail-provider-cocreate.ps1` — системная COM-активация.
- `test-fb2-shell-thumbnail.ps1` — shell API thumbnail smoke.
- `test-fb2-shell-thumbnail-matrix.ps1` — сводный сценарий: прямой COM,
  `CoCreateInstance`, shell API, forced extraction и negative cases.

Если нужно быстро понять, жив ли весь thumbnail-контур, канонический вход —
это именно `test-fb2-shell-thumbnail-matrix.ps1`.

### 3.2. Расширенная диагностика

- `test-fb2-shell-thumbnail-dump.ps1` — forced dump thumbnail в `.bmp` для
  анализа реального изображения.
- `test-fb2-shell-thumbnail-prime.ps1` — адресный прогрев / проверка shell
  cache для проблемных книг.
- `test-fbshell-loader.ps1` — низкоуровневая проверка загрузки `FBShell.dll`.
- `test-fb2-cover.ps1` — smoke reader-а embedded cover.
- `test-fb2-cover-thumbnail.ps1` — smoke декодирования / ресайза cover image.

Практическое правило:

- `dump` и `prime` — это диагностические шаги, а не обязательный release-gate;
- `cover`-сценарии полезны как локальный низкоуровневый разбор, когда нужно
  понять, проблема в reader/decoder или уже в shell-контуре Windows.

## 4. Metadata / property handler диагностика

- `test-fb2-metadata.ps1` — внутреннее чтение метаданных `.fb2`.
- `test-fb2-shell-properties.ps1` — запрос shell-свойств у конкретной книги.

Эти сценарии особенно полезны, когда smoke-fixture работает, а часть реальных
книг всё ещё возвращает пустые или неожиданные свойства.

## 5. Validate / FBV контур

- `test-fbv-fixture.ps1` — pre-flight проверка XSD-валидного fixture.

Сейчас это не большой автоматический набор, а точечный контрольный шаг перед
ручной shell-проверкой `Validate` через Explorer.

## 6. Legacy / reference-only контур

Эти сценарии больше не описывают основной runtime, но остаются полезными как
историческое сравнение и страховка против случайного отката поведения.

- `test-pcre.ps1`
- `test-pcre-wrapper.ps1`
- `test-regex-backend-compare.ps1`

Практическое правило:

- не удалять их без отдельного решения;
- не считать их обязательными для каждого релиза;
- помнить, что они могут условно пропускаться, если legacy `pcre.dll`
  отсутствует.

## 7. Fixture и C++ helper-файлы

Файлы `*.fb2`, `regex-fixtures.json` и `*.cpp` внутри `tools/tests` не стоит
оценивать только по прямым ссылкам из PowerShell-скриптов. Значительная часть
`*.cpp` используется как исходник для ad-hoc smoke-сборки, а fixture-файлы —
как воспроизводимые эталоны для ручной и автоматической проверки.

Поэтому перед удалением любого файла из `tools/tests` нужно проверить три
вещи:

1. Есть ли прямые ссылки в `docs`, `PROJECT_STATUS.md`, `tools/build` и
   PowerShell-обвязках.
2. Не играет ли файл роль fixture для ручной GUI-проверки.
3. Не перекрыт ли его сценарий более новым тестом полностью, а не частично.

## 8. Что уже можно считать упорядоченным

На текущем этапе из `tools/tests` уже убраны:

- три старых промежуточных `IThumbnailCache` smoke-файла;
- неиспользуемая ad-hoc пара `fb2-cover-inspect.cpp` /
  `test-fb2-cover-inspect.ps1`.

Дальше cleanup имеет смысл делать только точечно и после сверки с этим
документом.
