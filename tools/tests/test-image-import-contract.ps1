param(
    [string]$RepoRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
)

$ErrorActionPreference = 'Stop'
function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

$importSource = Get-Content -LiteralPath (Join-Path $RepoRoot 'src\fbe\ImageImport.cpp') -Raw
$docSource = Get-Content -LiteralPath (Join-Path $RepoRoot 'src\fbe\FBDoc.cpp') -Raw
$viewSource = Get-Content -LiteralPath (Join-Path $RepoRoot 'src\fbe\FBEview.cpp') -Raw
$frameSource = Get-Content -LiteralPath (Join-Path $RepoRoot 'src\fbe\mainfrm.cpp') -Raw
$settingsDialogSource = Get-Content -LiteralPath (Join-Path $RepoRoot 'src\fbe\SettingsOtherDlg.cpp') -Raw
$projectSource = Get-Content -LiteralPath (Join-Path $RepoRoot 'src\fbe\FBE.vcxproj') -Raw
$catalogSource = Get-Content -LiteralPath (Join-Path $RepoRoot 'localization\app-ui\catalog.json') -Raw
$catalog = $catalogSource | ConvertFrom-Json
$nativeHarness = Get-Content -LiteralPath (Join-Path $RepoRoot 'tools\tests\image-import-smoke.cpp') -Raw

foreach ($format in @('Jpeg', 'Png', 'Webp', 'Jp2', 'J2k', 'Tiff', 'Bmp', 'Gif', 'Avif', 'Heif')) {
    Assert-True ($importSource -match ('SourceFormat::' + $format)) "Отсутствует поддержка сигнатуры $format."
}
Assert-True ($importSource -match 'case SourceFormat::Jpeg[\s\S]*case SourceFormat::Png[\s\S]*keepSupportedImages') 'JPEG/PNG должны сохраняться без перекодирования при включённой настройке.'
Assert-True ($importSource -match 'image/jpeg') 'Не задан детерминированный MIME JPEG.'
Assert-True ($importSource -match 'image/png') 'Не задан детерминированный MIME PNG.'
Assert-True ($importSource -match 'PassThroughName\(result\.logicalFileName, type\)') 'Pass-through должен сохранять корректное исходное имя и исправлять ложное расширение.'
Assert-True ($importSource -match 'WebPGetFeatures') 'WebP должен проверять свойства до декодирования.'
Assert-True ($importSource -match 'has_animation') 'Animated WebP должен контролируемо отклоняться.'
Assert-True ($importSource -match 'flattenTransparentJpeg') 'Принудительный JPEG с alpha должен требовать/выполнять flatten.'
Assert-True ($importSource -match 'Color::White') 'Прозрачность при JPEG flatten должна заменяться белым фоном.'
Assert-True ($importSource -match 'GetFrameCount') 'GIF/TIFF должны проверять число кадров.'
Assert-True ($importSource -match 'opj_create_decompress') 'JPEG 2000 должен декодироваться через OpenJPEG.'
Assert-True ($importSource -match 'ComponentByteAt' -and $importSource -match 'absoluteX' -and $importSource -match 'c\.dx' -and $importSource -match 'c\.x0') 'JPEG 2000 sampling должен учитывать component dx/dy и origin, а не только пропорции raster-size.'
Assert-True ($importSource -match 'heif_decode_image') 'AVIF/HEIC/HEIF должны декодироваться через libheif.'
Assert-True ($importSource -match 'heif_check_filetype' -and $importSource -match 'heif_has_compatible_filetype') 'HEIF должен определяться официальным API libheif, включая compatible brands.'
Assert-True ($importSource -match 'ignore_transformations=0') 'Декодер HEIF должен применять ориентацию контейнера.'
Assert-True ($importSource -match 'get_number_of_top_level_images') 'HEIF sequence не должна молча импортироваться как один кадр.'
Assert-True ($importSource -match 'kMaxImagePixels') 'Импортёр должен ограничивать опасные размеры.'
Assert-True ($importSource -match 'kMaxOutputBytes') 'Импортёр должен ограничивать размер закодированного результата.'
Assert-True ($importSource -match 'ERROR_FILE_TOO_LARGE') 'Файл выше безопасного лимита должен возвращать контролируемую ошибку размера.'
Assert-True ($importSource -match 'catch \(const std::bad_alloc&\)') 'Нехватка памяти должна возвращаться как контролируемая ошибка.'
Assert-True ($importSource -match 'GlobalLock\(h\); if\(!p\) \{ GlobalFree\(h\); return E_OUTOFMEMORY; \}') 'GDI+ decoder должен обрабатывать неудачу GlobalLock без записи через null.'
Assert-True ($importSource -match 'static_cast<ptrdiff_t>\(d\.Stride\)') 'Проверка alpha должна корректно обрабатывать signed GDI+ stride.'
Assert-True ($importSource -match 'StartupTrace::Event\(L"image-import"') 'Успешный импорт должен оставлять обезличенный диагностический trace.'
Assert-True ($importSource -match 'StartupTrace::HResult\(L"image-import"') 'Ошибка импорта должна оставлять диагностический trace.'
Assert-True ($importSource -notmatch 'CreateFile.*TEMP|GetTempPath|dwebp\.exe|opj_decompress') 'Импорт не должен использовать временные файлы или внешние конвертеры.'
Assert-True ($docSource -match 'AddBinaryData') 'Doc должен принимать готовые байты изображения.'
Assert-True ($viewSource -match 'PrepareDefaultId\(logicalFileName\)') 'ID должен строиться по целевому имени.'
Assert-True ($viewSource -match 'AddImportedBinary') 'Добавление binary должно использовать общий DOM adapter.'
Assert-True ($viewSource -match 'body\.Invoke0\(L"FillCoverList"\)') 'Новый binary должен сразу обновлять список изображений и обложек.'
Assert-True ($viewSource -match 'ImportImageForFb2') 'Вставка изображения должна использовать общий импортёр.'
Assert-True ($viewSource -match 'body\.Invoke2\(L"InsImage"' -and $viewSource -match 'body\.Invoke2\(L"InsInlineImage"') 'Новый binary должен вставляться существующими путями обычной и inline-картинки.'
Assert-True ($frameSource -match 'ImportBinary\(fileName') 'Пакетный импорт должен продолжать обработку файлов.'
Assert-True ($frameSource -match 'batch_summary') 'Пакетный импорт должен формировать единый итоговый отчёт.'
Assert-True ($frameSource -match 'else\s+continue;') 'Отказ от JPEG flatten в пакетном импорте не должен считаться ошибкой.'
Assert-True ($viewSource -match '!= IDYES\) return;') 'Отказ от JPEG flatten при вставке не должен показывать ошибку.'
Assert-True ($projectSource -match 'libwebp\.lib') 'libwebp должен быть статически подключён.'
Assert-True ($projectSource -match 'openjp2\.lib') 'OpenJPEG должен быть статически подключён.'
foreach ($key in @('fbe.image_import.read_failed', 'fbe.image_import.heif_decode_failed', 'fbe.image_import.filter_supported', 'fbe.image_import.filter_heif')) {
    Assert-True ($catalogSource -match [regex]::Escape($key)) "В runtime-каталоге отсутствует ключ $key."
}
foreach ($key in @('fbe.image_import.output_auto', 'fbe.image_import.output_jpeg', 'fbe.image_import.output_png')) {
    Assert-True ($catalogSource -match [regex]::Escape($key)) "В runtime-каталоге отсутствует ключ настройки $key."
    Assert-True ($settingsDialogSource -match [regex]::Escape($key)) "Диалог настроек не использует локализованный ключ $key."
}
$formatOnlyKeys = @(
    'fbe.image_import.filter_jpeg', 'fbe.image_import.filter_png', 'fbe.image_import.filter_webp',
    'fbe.image_import.filter_jpeg2000', 'fbe.image_import.filter_bmp', 'fbe.image_import.filter_gif', 'fbe.image_import.filter_tiff',
    'fbe.image_import.filter_avif', 'fbe.image_import.filter_heif', 'fbe.image_import.output_jpeg',
    'fbe.image_import.output_png'
)
foreach ($entry in @($catalog.seedStrings.PSObject.Properties | Where-Object { $_.Name -like 'fbe.image_import.*' })) {
    $english = [string]$entry.Value.translations.'en-US'
    foreach ($language in $catalog.targetLanguages) {
        $translation = [string]$entry.Value.translations.$language
        Assert-True (-not [string]::IsNullOrWhiteSpace($translation)) "В $language отсутствует перевод $($entry.Name)."
        if ($language -ne 'en-US' -and $entry.Name -notin $formatOnlyKeys) {
            Assert-True ($translation -ne $english) "В $language остался английский fallback для $($entry.Name)."
        }
    }
}
Assert-True ($nativeHarness -match 'TestFb2BinaryRoundTrip') 'Native harness должен проверять FB2 save/reopen round-trip.'
Assert-True ($nativeHarness -match 'E_NOTIMPL') 'Native harness должен закреплять controlled rejection неподдерживаемых последовательностей.'
Assert-True ($nativeHarness -match 'TestCorruptImages') 'Native harness должен проверять controlled failure повреждённых изображений.'
Assert-True ($nativeHarness -match 'argv\[19\].*image/jpeg') 'Native harness должен проверять успешную конвертацию одно-страничного TIFF.'
Assert-True ($nativeHarness -match 'TestJpeg2000Fixture\(true\)' -and $nativeHarness -match 'TestJpeg2000Fixture\(false\)' -and $nativeHarness -match 'TestJpeg2000Fixture\(true, true\)' -and $nativeHarness -match 'TestJpeg2000SyccSubsampled') 'Native harness должен проверять JP2, raw J2K, alpha JP2 и sYCC subsampling.'

Write-Host 'Контракт импорта изображений проверен.'
