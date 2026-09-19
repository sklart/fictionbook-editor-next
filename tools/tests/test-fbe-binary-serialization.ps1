param(
    [string]$RepoRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
)

$ErrorActionPreference = 'Stop'

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw $Message
    }
}

$docSource = Get-Content -LiteralPath (Join-Path $RepoRoot 'src\fbe\FBDoc.cpp') -Raw
$insertSource = Get-Content -LiteralPath (Join-Path $RepoRoot 'src\fbe\image\ImageDocumentInserter.cpp') -Raw
$mainJsSource = Get-Content -LiteralPath (Join-Path $RepoRoot 'runtime\main.js') -Raw

Assert-True ($docSource -match 'CompactBinaryTextContent') 'Не найдено уплотнение base64 перед сохранением FB2.'
Assert-True ($docSource -match 'if \(compactBinaries\)') 'Уплотнение binary должно выполняться только при сохранении файла.'
Assert-True ($docSource -match 'compact\.GetBuffer\(\)' -and $docSource -match 'compact\.ReleaseBuffer\(output\)') 'Уплотнение Base64 должно выполняться в одном CString in-place.'
Assert-True ($docSource -notmatch 'PutdataType.*bin\.base64') 'Повторный MSXML decode/encode binary при уплотнении не нужен.'
Assert-True ($docSource -notmatch 'const CString source') 'Уплотнение не должно создавать отдельную копию исходной Base64-строки.'
Assert-True ($mainJsSource -match '(?s)newb\.dataType="bin\.base64";.*?newb\.nodeTypedValue=bo\[i\]\.base64data;.*?newb\.dataType=undefined') 'GetBinaries должен сам формировать compact Base64 через MSXML до сохранения.'
$binaryIdHeader = Get-Content -LiteralPath (Join-Path $RepoRoot 'src\fbe\BinaryId.h') -Raw
Assert-True ($docSource -match 'FbeBinary::NormalizeXmlId' -and $insertSource -match 'FbeBinary::NormalizeXmlId') 'Генерация binary ID должна использовать общий helper.'
Assert-True ($binaryIdHeader -match 'IsCharAlphaW' -and $binaryIdHeader -notmatch 'Transliterate') 'Общий helper должен сохранять допустимые Unicode XML ID без транслитерации.'

$document = New-Object -ComObject Msxml2.DOMDocument.6.0
$document.async = $false
$root = $document.createNode(1, 'root', '')
$null = $document.appendChild($root)
$binary = $document.createNode(1, 'binary', '')
$binary.dataType = 'bin.base64'
$original = [byte[]](0..119)
$binary.nodeTypedValue = $original
$null = $root.appendChild($binary)

$compact = $binary.text -replace '\s', ''
$binary.dataType = $null
$child = $binary.firstChild
while ($null -ne $child) {
    $null = $binary.removeChild($child)
    $child = $binary.firstChild
}
$null = $binary.appendChild($document.createTextNode($compact))
$savedText = $binary.text

Assert-True ($savedText -notmatch '\s') 'Компактная base64-строка не должна содержать пробельных символов.'
$serialized = $document.xml
$binaryMatch = [regex]::Match($serialized, '(?s)<binary>(?<data>[^<]*)</binary>')
Assert-True $binaryMatch.Success 'MSXML не сериализовал test-узел binary.'
Assert-True ($binaryMatch.Groups['data'].Value -notmatch '\s') 'После сериализации base64 не должна разбиваться переводами строк.'
Assert-True ($serialized -notmatch 'dt:dt|urn:schemas-microsoft-com:datatypes') 'В сериализованный XML не должны попадать MSXML datatype-метаданные.'
$binary.dataType = 'bin.base64'
$roundTrip = [byte[]]$binary.nodeTypedValue
Assert-True ($roundTrip.Length -eq $original.Length) 'После уплотнения изменилась длина двоичных данных.'
for ($index = 0; $index -lt $original.Length; ++$index) {
    Assert-True ($roundTrip[$index] -eq $original[$index]) "После уплотнения изменился байт $index."
}

Write-Host 'Проверка компактной сериализации binary и общего Unicode-aware генератора ID пройдена.'
