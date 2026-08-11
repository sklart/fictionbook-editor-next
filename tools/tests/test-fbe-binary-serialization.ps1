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
$viewSource = Get-Content -LiteralPath (Join-Path $RepoRoot 'src\fbe\FBEview.cpp') -Raw

Assert-True ($docSource -match 'CompactBinaryTextContent') 'Не найдено уплотнение base64 перед сохранением FB2.'
Assert-True ($docSource -match 'PutdataType.*bin\.base64') 'Перед уплотнением binary должен декодироваться штатным MSXML.'
Assert-True ($docSource.Contains('PutdataType(_bstr_t(L""))')) 'Production-код обязан снять временный MSXML dataType перед сериализацией.'
Assert-True ($docSource -match 'createTextNode') 'Для compact base64 должен создаваться текстовый DOM-узел.'
Assert-True ($docSource -notmatch 'binary->Puttext') 'Для элемента binary нельзя использовать put_text: MSXML6 возвращает E_INVALIDARG.'
Assert-True ($docSource -match 'if \(compactBinaries\)') 'Уплотнение binary должно выполняться только при сохранении файла.'
Assert-True ($docSource -match "c==_T\('-'\)") 'Doc::PrepareDefaultId должен сохранять допустимое тире в ID.'
Assert-True ($viewSource -match "c==_T\('-'\)") 'CFBEView::PrepareDefaultId должен сохранять допустимое тире в ID.'

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

Write-Host 'Проверка компактной сериализации binary и сохранения тире в ID пройдена.'
