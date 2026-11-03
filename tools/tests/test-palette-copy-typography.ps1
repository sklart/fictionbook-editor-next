[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$resources = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $repoRoot 'src\fbe\FBE.rc')
$dialog = [regex]::Match($resources, '(?ms)^IDD_SETTINGS_PALETTE_COPY DIALOGEX\s+0,\s*0,\s*(\d+),\s*(\d+)\s*\r?\n(.*?)^END\s*$')
if(-not $dialog.Success) { throw 'Palette copy dialog resource not found.' }
$body = $dialog.Groups[3].Value
$font = [regex]::Match($body, '(?m)^FONT\s+(\d+),\s*"([^"]+)"')
$coordinates = '\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)'
$prompt = [regex]::Match($body, '(?m)^\s*LTEXT\s+"[^"]*",IDC_PALETTE_COPY_PROMPT,' + $coordinates)
$edit = [regex]::Match($body, '(?m)^\s*EDITTEXT\s+IDC_PALETTE_COPY_NAME,' + $coordinates)
$create = [regex]::Match($body, '(?m)^\s*DEFPUSHBUTTON\s+"[^"]*",IDOK,' + $coordinates)
$cancel = [regex]::Match($body, '(?m)^\s*PUSHBUTTON\s+"[^"]*",IDCANCEL,' + $coordinates)
if(-not ($font.Success -and $prompt.Success -and $edit.Success -and $create.Success -and $cancel.Success) -or
    $create.Groups[3].Value -ne $cancel.Groups[3].Value -or $create.Groups[4].Value -ne $cancel.Groups[4].Value) {
    throw 'Palette copy dialog typography or button dimensions are unavailable.'
}
if($font.Groups[1].Value -ne '8' -or $font.Groups[2].Value -ne 'MS Shell Dlg 2') { throw 'Palette copy dialog does not use the settings font.' }
$rectangles = @($prompt, $edit, $create, $cancel | ForEach-Object {
    [pscustomobject]@{ X = [int]$_.Groups[1].Value; Y = [int]$_.Groups[2].Value; W = [int]$_.Groups[3].Value; H = [int]$_.Groups[4].Value }
})
if($rectangles.Count -ne 4) { throw 'Expected four palette copy controls.' }
for($i = 0; $i -lt $rectangles.Count; ++$i) {
    $a = $rectangles[$i]
    if($a.W -le 0 -or $a.H -le 0 -or $a.X + $a.W -gt [int]$dialog.Groups[1].Value -or $a.Y + $a.H -gt [int]$dialog.Groups[2].Value) {
        throw "Palette copy control $i is outside the dialog."
    }
    for($j = $i + 1; $j -lt $rectangles.Count; ++$j) {
        $b = $rectangles[$j]
        if($a.X -lt $b.X + $b.W -and $b.X -lt $a.X + $a.W -and $a.Y -lt $b.Y + $b.H -and $b.Y -lt $a.Y + $a.H) {
            throw "Palette copy controls $i and $j overlap."
        }
    }
}
$catalog = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $repoRoot 'localization\app-ui\fbe-small-dialogs.json') | ConvertFrom-Json
$outputDirectory = Join-Path $repoRoot 'build\tests\palette-copy-typography'
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
& (Join-Path $repoRoot 'tools\build\Import-VsDevEnvironment.ps1') -Arch x86 -HostArch x64 -PlatformToolset v143 -VcVarsVersion 14.44
Push-Location $outputDirectory
try {
    & cl.exe /nologo /std:c++17 /EHsc /W4 /WX /utf-8 (Join-Path $PSScriptRoot 'palette-copy-typography-test.cpp') /Fe:palette-copy-typography-test.exe /link gdi32.lib user32.lib
    if($LASTEXITCODE -ne 0) { throw 'Palette copy typography test compilation failed.' }
    foreach($locale in @($catalog.targetLanguages)) {
        $label = $catalog.strings.'fbe.settings.palette.copy_name'.translations.$locale
        $createText = $catalog.strings.'fbe.settings.palette.create'.translations.$locale
        $cancelText = $catalog.strings.'fbe.dialog.idd_tools_settings.cancel'.translations.$locale
        if(-not $label -or -not $createText -or -not $cancelText) { throw "Missing palette copy text for $locale." }
        & .\palette-copy-typography-test.exe $locale $label $createText $cancelText $prompt.Groups[3].Value $prompt.Groups[4].Value $create.Groups[3].Value $create.Groups[4].Value $font.Groups[2].Value $font.Groups[1].Value
        if($LASTEXITCODE -ne 0) { throw "Palette copy typography failed for $locale." }
    }
    Write-Host 'Palette copy typography: PASS (12 locales, simulated 96/120/144/192 DPI).'
} finally {
    Pop-Location
}
