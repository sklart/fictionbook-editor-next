# Ручная проверка experimental thumbnail provider

Этот документ нужен для управляемой GUI-проверки experimental thumbnail
provider для `.fb2`, который читает встроенную обложку книги и отдаёт её в
Проводник как миниатюру.

## Что уже есть в проекте

К моменту этой проверки уже готовы:

- внутренний reader обложки: `src/fbe/Fb2CoverImage.*`;
- внутренний decoder изображения обложки через локальный `ATL::CImage`:
  `src/fbe/Fb2CoverThumbnail.*`;
- experimental COM-класс thumbnail provider:
  `src/fbshell/Fb2ThumbnailProvider.*`.

Smoke-проверки для подготовительного слоя:

- `tools/tests/test-fb2-cover.ps1`
- `tools/tests/test-fb2-cover-thumbnail.ps1`
- `tools/tests/test-fb2-thumbnail-provider.ps1`
- `tools/tests/test-fb2-shell-thumbnail-matrix.ps1`

## Подготовка

Скрипт регистрации нужно запускать из PowerShell с правами администратора.

```powershell
.\tools\build\register-experimental-property-handler.ps1 -Configuration Release -Platform x64
```

Этот сценарий сейчас регистрирует сразу оба экспериментальных компонента:

- modern property handler для свойств `.fb2`;
- thumbnail provider для миниатюр `.fb2`.

Быстрая диагностика состояния shell-поверхностей:

```powershell
.\tools\tests\check-fb2-shell-surfaces.ps1
```

Если shell API уже видит миниатюру, а в окне Проводника всё ещё остаётся
старый пустой значок, после регистрации experimental shell-контура имеет смысл
отдельно сбросить thumbnail cache:

```powershell
.\tools\build\reset-explorer-thumbnail-cache.ps1 -Confirm:$false
```

Если после полного сброса `thumbcache` проблемный `.fb2` всё равно остаётся
без миниатюры, а принудительная shell-проверка уже показывает, что provider
читает обложку корректно, следующий практический шаг — насильно прогреть
системный thumbnail cache именно для этого файла:

```powershell
.\tools\build\prime-fb2-thumbnail-cache.ps1 `
    'E:\Путь\К\Проблемной\Книге.fb2'
```

Этот сценарий полезен для случаев, когда Windows успела закэшировать неудачную
попытку извлечения миниатюры и продолжает показывать пустой значок даже после
исправления shell-регистрации.

Для проверки миниатюры можно использовать:

- `tools/tests/fb2-cover-smoke.fb2`
- `tools/tests/fb2-cover-visible-smoke.fb2`
- `tools/tests/fb2-cover-jpeg-smoke.fb2`
- `tools/tests/fb2-cover-bmp-smoke.fb2`

Для проверки fallback-сценария с битой обложкой:

- `tools/tests/fb2-cover-broken.fb2`

Для проверки сценариев отсутствующей обложки:

- `tools/tests/fb2-cover-missing-coverpage.fb2`
- `tools/tests/fb2-cover-missing-binary.fb2`

## Что именно проверять

### 1. Базовый сценарий

Для именно визуальной GUI-проверки лучше брать
`tools/tests/fb2-cover-visible-smoke.fb2`, потому что старый
`fb2-cover-smoke.fb2` содержит tiny PNG `1x1` и может визуально выглядеть как
пустой белый значок даже при полностью рабочем thumbnail provider.

Открыть папку с `fb2-cover-visible-smoke.fb2` в режиме крупных или очень крупных
значков.

Ожидание:

- Проводник не падает;
- у файла появляется миниатюра по встроенной обложке, а не только общий значок
  `.fb2`.

### 2. Повторное открытие папки

Переоткрыть папку или переключить режим отображения несколько раз.

Ожидание:

- `explorer.exe` остаётся стабильным;
- миниатюра не исчезает случайно после обновления вида.

Если после успешной регистрации и положительного результата
`tools/tests/test-fb2-shell-thumbnail.ps1` Проводник всё ещё показывает старый
пустой значок, это уже сильный кандидат на устаревший thumbcache. В этом случае
нужно выполнить `reset-explorer-thumbnail-cache.ps1`, снова открыть папку и
повторить проверку.

Если и после этого отдельные книги остаются без миниатюры, а
`tools/tests/test-fbshell-loader.ps1` и/или ручная forced-проверка
подтверждают, что provider обложку читает, это уже похоже на stale negative
cache внутри shell thumbnail-контура. Для такого случая и добавлен
`prime-fb2-thumbnail-cache.ps1`.

### 3. Файл с битой обложкой

Проверить `tools/tests/fb2-cover-broken.fb2`.

Ожидание:

- Проводник не падает;
- для файла остаётся обычный fallback-значок без миниатюры обложки.

### 4. Разные типы обложек

Отдельно проверить уже подготовленные варианты:

- `image/png`
- `image/jpeg` / `image/jpg`
- `image/bmp`

На текущем шаге это уже подтверждено автоматическими smoke-тестами, но
финальное GUI-подтверждение в Проводнике ещё нужно зафиксировать руками.

### 5. Файлы без доступной обложки

Проверить:

- `tools/tests/fb2-cover-missing-coverpage.fb2`
- `tools/tests/fb2-cover-missing-binary.fb2`

Ожидание:

- Проводник не падает;
- вместо миниатюры обложки остаётся обычный fallback-значок `.fb2`.

## Что уже покрыто автоматически

До GUI-проверки уже автоматически подтверждены:

- COM-сценарий `IStream -> thumbnail provider -> HBITMAP`;
- системная COM-активация зарегистрированного thumbnail provider;
- shell API `IShellItemImageFactory` на валидных fixture `png/jpeg/bmp`;
- принудительное извлечение через `IThumbnailCache` на валидных fixture;
- корректные обложки `png`, `jpeg`, `bmp`;
- мягкий отказ на битой base64-обложке;
- мягкий отказ при отсутствии `coverpage`;
- мягкий отказ при отсутствии нужного `binary`;
- базовая COM-устойчивость (`E_POINTER`, повторный `Initialize`).
- shell API Windows реально получает thumbnail для валидного `.fb2`
  через `tools/tests/test-fb2-shell-thumbnail.ps1`.

Для полного автоматического прогона актуальной матрицы можно использовать:

```powershell
.\tools\tests\test-fb2-shell-thumbnail-matrix.ps1
```

Опционально можно добавить и диагностический шаг прогрева shell cache:

```powershell
.\tools\tests\test-fb2-shell-thumbnail-matrix.ps1 -IncludePrimeDiagnostic
```

Важно: `prime`-шаг сейчас трактуется как диагностический, а не как жёсткий
gate. На части систем он может возвращать отказ `IThumbnailCache` даже при
исправно работающем обычном shell thumbnail-пути.

## Откат после проверки

```powershell
.\tools\build\unregister-experimental-property-handler.ps1 -Configuration Release -Platform x64
```

## Что фиксировать по результатам

- появилась ли миниатюра у валидного `.fb2`;
- не падает ли `explorer.exe` на битой обложке;
- одинаково ли работает повторное открытие папки;
- есть ли различия для `png`, `jpeg/jpg`, `bmp`;
- не остаётся ли после отката странного кэша миниатюр.
