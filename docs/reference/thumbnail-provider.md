# Современный provider миниатюр FB2

> Тип: Действующая инструкция. Документационная база: FBE Next 3.0.9, `master` / `1ad7c507d1ed`; сверка 19 октября 2026 года.

## Действующий путь

`Fb2CoverImage` извлекает `coverpage` и соответствующий binary с действующими ограничениями. `Fb2CoverThumbnail` строит изображение через WIC: decoder → frame → scaler → format converter → 32-bit HBITMAP. COM-класс реализует `IInitializeWithStream` и `IThumbnailProvider`.

Downscale использует Fant, пропорции сохраняются, изображения меньше требуемого размера не увеличиваются. PNG с реальной прозрачностью даёт `WTSAT_ARGB`; непрозрачные результаты — `WTSAT_RGB`. Нет обложки или данные повреждены — ожидается мягкий отказ и обычная fallback-иконка, а не падение Explorer.

## Что не является текущей реализацией

Legacy `IExtractImage`, IconExtractor, ImageLoader и старые bundled декодеры не следует возвращать вместо WIC-пути. Дополнительная визуальная обработка вроде trim белых полей — отдельное будущее улучшение, а не доказанный дефект существующего provider.

## Проверки

Исходный завершённый этап фиксирует PNG/JPEG/BMP, alpha, размеры, downscale/no-upscale, прямой COM и shell-контуры. Это историческое основание, не новый прогон данного пакета документации.

Текущая [Shell-матрица](../testing/shell-integration.md) отдельно проверяет прямое извлечение и показ в Explorer, где возможен stale negative cache. Очистка/prime кэша применяется только при диагностике и не должна маскировать ошибку самого provider.

## Основания

- [docs/thumbnail-provider-plan.md](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/thumbnail-provider-plan.md)
- [docs/thumbnail-provider-manual-test.md](https://github.com/sklart/fictionbook-editor-next/blob/1ad7c507d1edcd0ca132589fe5a9f16a5574253d/docs/thumbnail-provider-manual-test.md)
