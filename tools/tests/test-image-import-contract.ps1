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
Assert-True ($importSource -match 'ComponentByteAt') 'JPEG 2000 sYCC с subsampling должен приводиться к полному RGB-растру.'
Assert-True ($importSource -match 'heif_decode_image') 'AVIF/HEIC/HEIF должны декодироваться через libheif.'
Assert-True ($importSource -match 'get_number_of_top_level_images') 'HEIF sequence не должна молча импортироваться как один кадр.'
Assert-True ($importSource -match 'kMaxImagePixels') 'Импортёр должен ограничивать опасные размеры.'
Assert-True ($importSource -match 'ERROR_FILE_TOO_LARGE') 'Файл выше безопасного лимита должен возвращать контролируемую ошибку размера.'
Assert-True ($importSource -match 'GlobalLock\(h\); if\(!p\) \{ GlobalFree\(h\); return E_OUTOFMEMORY; \}') 'GDI+ decoder должен обрабатывать неудачу GlobalLock без записи через null.'
Assert-True ($importSource -match 'StartupTrace::Event\(L"image-import"') 'Успешный импорт должен оставлять обезличенный диагностический trace.'
Assert-True ($importSource -match 'StartupTrace::HResult\(L"image-import"') 'Ошибка импорта должна оставлять диагностический trace.'
Assert-True ($importSource -notmatch 'CreateFile.*TEMP|GetTempPath|dwebp\.exe|opj_decompress') 'Импорт не должен использовать временные файлы или внешние конвертеры.'
Assert-True ($docSource -match 'AddBinaryData') 'Doc должен принимать готовые байты изображения.'
Assert-True ($viewSource -match 'PrepareDefaultId\(logicalFileName\)') 'ID должен строиться по целевому имени.'
Assert-True ($viewSource -match 'AddImportedBinary') 'Добавление binary должно использовать общий DOM adapter.'
Assert-True ($viewSource -match 'ImportImageForFb2') 'Вставка изображения должна использовать общий импортёр.'
Assert-True ($frameSource -match 'ImportBinary\(fileName') 'Пакетный импорт должен продолжать обработку файлов.'
Assert-True ($frameSource -match 'batch_summary') 'Пакетный импорт должен формировать единый итоговый отчёт.'
Assert-True ($projectSource -match 'libwebp\.lib') 'libwebp должен быть статически подключён.'
Assert-True ($projectSource -match 'openjp2\.lib') 'OpenJPEG должен быть статически подключён.'
foreach ($key in @('fbe.image_import.read_failed', 'fbe.image_import.heif_decode_failed', 'fbe.image_import.filter_supported', 'fbe.image_import.filter_heif')) {
    Assert-True ($catalogSource -match [regex]::Escape($key)) "В runtime-каталоге отсутствует ключ $key."
}
foreach ($key in @('fbe.image_import.output_auto', 'fbe.image_import.output_jpeg', 'fbe.image_import.output_png')) {
    Assert-True ($catalogSource -match [regex]::Escape($key)) "В runtime-каталоге отсутствует ключ настройки $key."
    Assert-True ($settingsDialogSource -match [regex]::Escape($key)) "Диалог настроек не использует локализованный ключ $key."
}
Assert-True ($nativeHarness -match 'TestFb2BinaryRoundTrip') 'Native harness должен проверять FB2 save/reopen round-trip.'
Assert-True ($nativeHarness -match 'E_NOTIMPL') 'Native harness должен закреплять controlled rejection неподдерживаемых последовательностей.'
Assert-True ($nativeHarness -match 'TestCorruptImages') 'Native harness должен проверять controlled failure повреждённых изображений.'
Assert-True ($nativeHarness -match 'TestJpeg2000Fixture\(true\)' -and $nativeHarness -match 'TestJpeg2000Fixture\(false\)' -and $nativeHarness -match 'TestJpeg2000Fixture\(true, true\)' -and $nativeHarness -match 'TestJpeg2000SyccSubsampled') 'Native harness должен проверять JP2, raw J2K, alpha JP2 и sYCC subsampling.'

Write-Host 'Контракт импорта изображений проверен.'
