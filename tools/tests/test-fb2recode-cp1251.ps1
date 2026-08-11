$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$recoder = Join-Path $repoRoot 'runtime\Utilities\fb2recode\fb2recode.js'
$source = Get-Content -Raw -LiteralPath $recoder
$extendedCodePoints = [int[]]@(
    0x0402,0x0403,0x201A,0x0453,0x201E,0x2026,0x2020,0x2021,0x20AC,0x2030,0x0409,0x2039,0x040A,0x040C,0x040B,0x040F,
    0x0452,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,0x2122,0x0459,0x203A,0x045A,0x045C,0x045B,0x045F,
    0x00A0,0x040E,0x045E,0x0408,0x00A4,0x0490,0x00A6,0x00A7,0x0401,0x00A9,0x0404,0x00AB,0x00AC,0x00AD,0x00AE,0x0407,
    0x00B0,0x00B1,0x0406,0x0456,0x0491,0x00B5,0x00B6,0x00B7,0x0451,0x2116,0x0454,0x00BB,0x0458,0x0405,0x0455,0x0457
)
foreach($codePoint in $extendedCodePoints) {
    if($source -notmatch ('0x{0:X4}' -f $codePoint)) { throw "В CP1251 mapping отсутствует U+$('{0:X4}' -f $codePoint)." }
}
if($source -match '0x02DC') { throw 'Undefined CP1251 byte 0x98 не должен отображаться как U+02DC.' }
if($source -notmatch 'isCp1251Character') { throw 'Не найдена точная проверка CP1251 mapping.' }

function Find-ByteSequence([byte[]]$Haystack, [byte[]]$Needle) {
    for($offset = 0; $offset -le $Haystack.Length - $Needle.Length; $offset++) {
        $matched = $true
        for($index = 0; $index -lt $Needle.Length; $index++) {
            if($Haystack[$offset + $index] -ne $Needle[$index]) { $matched = $false; break }
        }
        if($matched) { return $offset }
    }
    return -1
}

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('fbe-fb2recode-cp1251-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tempRoot | Out-Null
try {
    $utf8 = New-Object Text.UTF8Encoding($false)
    $extended = -join ($extendedCodePoints | ForEach-Object { [char]$_ })
    $cyrillic = -join (0x0410..0x044F | ForEach-Object { [char]$_ })
    $xml = "<?xml version=`"1.0`" encoding=`"UTF-8`"?><FictionBook xmlns=`"http://www.gribuser.ru/xml/fictionbook/2.0`"><body><section><p>$extended$cyrillic</p></section></body></FictionBook>"
    $valid = Join-Path $tempRoot 'exact.fb2'
    [IO.File]::WriteAllText($valid, $xml, $utf8)
    & cscript.exe //nologo $recoder '/encoding:windows-1251' '/no-backup' '/quiet' $valid
    if($LASTEXITCODE -ne 0) { throw "FB2Recode не смог перекодировать полный canonical Windows-1251 fixture (exit code $LASTEXITCODE)." }

    $expected = [byte[]]@((0x80..0x97) + (0x99..0xBF) + (0xC0..0xFF))
    $actual = [IO.File]::ReadAllBytes($valid)
    if((Find-ByteSequence $actual $expected) -lt 0) { throw 'Реальная перекодировка не сохранила точное соответствие всех extended Windows-1251 байтов.' }

    $invalid = Join-Path $tempRoot 'unrepresentable.fb2'
    [IO.File]::WriteAllText($invalid, $xml.Replace($extended, '漢'), $utf8)
    $originalHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $invalid).Hash
    & cscript.exe //nologo $recoder '/encoding:windows-1251' '/no-backup' '/quiet' $invalid
    if($LASTEXITCODE -eq 0) { throw 'FB2Recode принял символ, отсутствующий в Windows-1251.' }
    if((Get-FileHash -Algorithm SHA256 -LiteralPath $invalid).Hash -ne $originalHash) { throw 'FB2Recode изменил исходный файл после отказа Windows-1251.' }
}
finally {
    if(Test-Path -LiteralPath $tempRoot) { Remove-Item -LiteralPath $tempRoot -Recurse -Force }
}
$global:LASTEXITCODE = 0
Write-Host 'FB2Recode exact CP1251 behavioural regression passed.'
