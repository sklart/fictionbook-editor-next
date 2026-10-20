> **Исторический материал.** Сохранён из `docs/thumbnail-provider-plan.md` на ревизии `1ad7c507d1ed`. Указанные ниже даты, версии, PASS/FAIL и планы относятся к описанным прогонам, а не к проверке текущего релиза. Изменены навигация и оформление архивного раздела; результаты испытаний не повышались.

[Исходная редакция без документационных изменений](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/thumbnail-provider-plan.md).

# Modern FB2 thumbnail provider: завершено

## Статус

Основная задача modern thumbnail provider для `.fb2` завершена. Provider входит
в штатный shell-контур FBE Next; legacy `IExtractImage`, `IconExtractor`,
`ImageLoader` и bundled `zlib/libpng/libjpeg` не используются и не должны
возвращаться.

## Реализация

- `src/common/fb2/Fb2CoverImage.*` безопасно извлекает `coverpage` и нужный
  `binary` из FB2-потока с существующими лимитами размера.
- `src/common/fb2/Fb2CoverThumbnail.*` использует единый WIC-pipeline:
  `IWICBitmapDecoder -> IWICBitmapFrameDecode -> IWICBitmapScaler ->
  IWICFormatConverter -> 32-bit HBITMAP`.
- Для downscale используется `WICBitmapInterpolationModeFant`; пропорции
  сохраняются, а изображения меньше запрошенного `cx` не увеличиваются.
- PNG с реальной прозрачностью сохраняет alpha и возвращается как
  `WTSAT_ARGB`; JPEG, BMP и непрозрачный PNG возвращаются как `WTSAT_RGB`.
- `src/fbshell/Fb2ThumbnailProvider.*` реализует `IInitializeWithStream` и
  `IThumbnailProvider`; отсутствие или повреждение обложки даёт мягкий отказ
  и обычную fallback-иконку без падения Explorer.

## Подтверждённые проверки

Автоматический контур:

- `tools/tests/test-fb2-thumbnail-provider.ps1` подтверждает PNG/JPEG/BMP,
  alpha PNG, размеры `32/64/128/256/512`, downscale, запрет upscale,
  `WTSAT_RGB`/`WTSAT_ARGB` и реальные пиксели с частичной alpha.
- `tools/tests/test-fb2-shell-thumbnail-matrix.ps1` покрывает прямой COM,
  `CoCreateInstance`, shell API, forced extraction через `IThumbnailCache` и
  мягкие отказы для битой обложки, отсутствующих `coverpage` и `binary`.
- Общий Release regression и Release Win32 build проходят.

Ручной Windows Explorer smoke завершён: проверены крупные и очень крупные
значки, несколько FB2 одновременно, повторное открытие/обновление папки и
стабильность `explorer.exe`.

## Эксплуатационная диагностика

Explorer кэширует успешные и отрицательные результаты. При старом значке после
положительного shell smoke используются `reset-explorer-thumbnail-cache.ps1`
и, только как диагностика stale negative cache,
`prime-fb2-thumbnail-cache.ps1`. Это не часть provider и не release gate.

## Необязательный будущий хвост

Допустима отдельная визуальная постобработка embedded cover image — например,
trim белых полей для preview/details pane. Это не дефект thumbnail provider и
не требует изменения WIC/shell-пути.
