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
$projectSource = Get-Content -LiteralPath (Join-Path $RepoRoot 'src\fbe\FBE.vcxproj') -Raw

foreach ($format in @('Jpeg', 'Png', 'Webp', 'Jp2', 'J2k', 'Tiff', 'Bmp', 'Gif')) {
    Assert-True ($importSource -match ('SourceFormat::' + $format)) "Отсутствует поддержка сигнатуры $format."
}
Assert-True ($importSource -match 'keepSupportedImages.*Jpeg.*Png') 'JPEG/PNG должны сохраняться без перекодирования при включённой настройке.'
Assert-True ($importSource -match 'image/jpeg') 'Не задан детерминированный MIME JPEG.'
Assert-True ($importSource -match 'image/png') 'Не задан детерминированный MIME PNG.'
Assert-True ($importSource -match 'WebPGetFeatures') 'WebP должен проверять свойства до декодирования.'
Assert-True ($importSource -match 'has_animation') 'Animated WebP должен контролируемо отклоняться.'
Assert-True ($importSource -match 'flattenTransparentJpeg') 'Принудительный JPEG с alpha должен требовать/выполнять flatten.'
Assert-True ($importSource -match 'Color::White') 'Прозрачность при JPEG flatten должна заменяться белым фоном.'
Assert-True ($importSource -match 'GetFrameCount') 'GIF/TIFF должны проверять число кадров.'
Assert-True ($importSource -match 'opj_create_decompress') 'JPEG 2000 должен декодироваться через OpenJPEG.'
Assert-True ($importSource -match 'kMaxImagePixels') 'Импортёр должен ограничивать опасные размеры.'
Assert-True ($importSource -notmatch 'CreateFile.*TEMP|GetTempPath|dwebp\.exe|opj_decompress') 'Импорт не должен использовать временные файлы или внешние конвертеры.'
Assert-True ($docSource -match 'AddBinaryData') 'Doc должен принимать готовые байты изображения.'
Assert-True ($docSource -match 'PrepareDefaultId\(logicalFileName\)') 'ID должен строиться по целевому имени.'
Assert-True ($viewSource -match 'ImportImageForFb2') 'Вставка изображения должна использовать общий импортёр.'
Assert-True ($frameSource -match 'ImportBinary\(fileName') 'Пакетный импорт должен продолжать обработку файлов.'
Assert-True ($frameSource -match 'batch_summary') 'Пакетный импорт должен формировать единый итоговый отчёт.'
Assert-True ($projectSource -match 'libwebp\.lib') 'libwebp должен быть статически подключён.'
Assert-True ($projectSource -match 'openjp2\.lib') 'OpenJPEG должен быть статически подключён.'

Write-Host 'Контракт импорта изображений проверен.'
