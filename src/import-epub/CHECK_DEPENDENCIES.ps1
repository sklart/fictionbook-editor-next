$ErrorActionPreference = 'Continue'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$outDir = Join-Path $scriptDir 'out\Release'
if (-not (Test-Path -LiteralPath $outDir)) { $outDir = Join-Path $scriptDir 'out' }
$reportPath = Join-Path $scriptDir 'CHECK_DEPENDENCIES_REPORT.txt'
$lines = New-Object System.Collections.Generic.List[string]
$clsid = '{D4B1B165-4D93-4F2D-8C8A-2D0C649431A1}'

function Say([string]$text, [ConsoleColor]$color = [ConsoleColor]::Gray) {
    $lines.Add($text) | Out-Null
    Write-Host $text -ForegroundColor $color
}

function Get-PeMachine([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return 'missing' }
    $fs = [IO.File]::OpenRead($path)
    try {
        $br = New-Object IO.BinaryReader($fs)
        $fs.Seek(0x3c, [IO.SeekOrigin]::Begin) | Out-Null
        $pe = $br.ReadInt32()
        $fs.Seek($pe + 4, [IO.SeekOrigin]::Begin) | Out-Null
        $m = $br.ReadUInt16()
        switch ($m) { 0x014c { 'Win32/x86' } 0x8664 { 'x64' } default { ('0x{0:X4}' -f $m) } }
    } finally { $fs.Close() }
}

function Show-File([string]$path, [bool]$required) {
    $name = Split-Path -Leaf $path
    if (Test-Path -LiteralPath $path) {
        $item = Get-Item -LiteralPath $path
        $v = $item.VersionInfo
        $machine = Get-PeMachine $path
        Say ('OK:      {0}  {1}  {2} bytes' -f $name, $machine, $item.Length) Green
        Say ('         Path:             ' + $path)
        Say ('         CompanyName:      ' + $v.CompanyName)
        Say ('         FileDescription:  ' + $v.FileDescription)
        Say ('         FileVersion:      ' + $v.FileVersion)
        Say ('         InternalName:     ' + $v.InternalName)
        Say ('         OriginalFilename: ' + $v.OriginalFilename)
        Say ('         ProductName:      ' + $v.ProductName)
        Say ('         ProductVersion:   ' + $v.ProductVersion)
        if ($machine -ne 'Win32/x86') { Say ('WARNING: expected Win32/x86, got ' + $machine) Yellow }
    } elseif ($required) {
        Say ('MISSING: ' + $name) Red
    } else {
        Say ('OPTIONAL MISSING: ' + $name) Yellow
    }
}

function Show-RegistryValue([string]$path, [string]$name) {
    if (Test-Path $path) {
        try {
            $props = Get-ItemProperty -Path $path
            $value = if ($name -eq '(default)') { (Get-Item -Path $path).GetValue('') } else { $props.$name }
            Say ('OK:      ' + $path) Green
            Say ('         ' + $name + ': ' + $value)
            return $value
        } catch {
            Say ('ERROR:   cannot read ' + $path + ' : ' + $_.Exception.Message) Red
        }
    } else {
        Say ('MISSING: ' + $path) Yellow
    }
    return $null
}

Say '=== ImportEPUB dependency report ==='
Say ('Generated: ' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))
Say ('Project:   ' + $scriptDir)
Say ('Output:    ' + $outDir)
Say ''

Say '=== Binaries ==='
Show-File (Join-Path $outDir 'ImportEPUB.dll') $true
Show-File (Join-Path $outDir 'ImportEPUBBatch.exe') $true
Show-File (Join-Path $outDir 'ImportEPUBLunaSVG.dll') $false
Say ''

Say '=== LunaSVG static libraries ==='
foreach ($lib in @('thirdparty\lunasvg\lib\Win32\Release\plutovg.lib','thirdparty\lunasvg\lib\Win32\Release\lunasvg.lib')) {
    $p = Join-Path $scriptDir $lib
    if (Test-Path -LiteralPath $p) { Say ('OK:      ' + $lib) Green } else { Say ('MISSING: ' + $lib) Yellow }
}
Say ''

Say '=== COM registration ==='
$expectedDll = Join-Path $outDir 'ImportEPUB.dll'
$regPaths = @(
    'HKCU:\Software\Classes\CLSID\' + $clsid + '\InprocServer32',
    'HKLM:\SOFTWARE\WOW6432Node\Classes\CLSID\' + $clsid + '\InprocServer32',
    'HKLM:\SOFTWARE\Classes\WOW6432Node\CLSID\' + $clsid + '\InprocServer32'
)
$foundCom = $false
foreach ($rp in $regPaths) {
    if (Test-Path $rp) {
        $foundCom = $true
        $v = Show-RegistryValue $rp '(default)'
        if ($v -and (Test-Path -LiteralPath $expectedDll) -and ([IO.Path]::GetFullPath($v) -ne [IO.Path]::GetFullPath($expectedDll))) {
            Say ('WARNING: registered DLL differs from current output DLL') Yellow
            Say ('         Registered: ' + $v)
            Say ('         Current:    ' + $expectedDll)
        }
    }
}
if (-not $foundCom) { Say 'MISSING: COM InprocServer32 registration was not found in checked locations.' Yellow }
Say ''

Say '=== FBE plugin registration ==='
$pluginPaths = @(
    'HKCU:\Software\FBETeam\FictionBook Editor\Plugins\' + $clsid,
    'HKLM:\SOFTWARE\WOW6432Node\FBETeam\FictionBook Editor\Plugins\' + $clsid,
    'HKLM:\SOFTWARE\FBETeam\FictionBook Editor\Plugins\' + $clsid
)
$foundPlugin = $false
foreach ($pp in $pluginPaths) {
    if (Test-Path $pp) {
        $foundPlugin = $true
        Say ('OK:      ' + $pp) Green
        $props = Get-ItemProperty -Path $pp
        Say ('         Type: ' + $props.Type)
        Say ('         Menu: ' + $props.Menu)
        Say ('         Icon: ' + $props.Icon)
    }
}
if (-not $foundPlugin) { Say 'MISSING: FBE plugin registry entry was not found in checked locations.' Yellow }
Say ''

$batch = Join-Path $outDir 'ImportEPUBBatch.exe'
if (Test-Path -LiteralPath $batch) {
    Say '=== ImportEPUBBatch --version ==='
    try { (& $batch --version) | ForEach-Object { Say $_ } } catch { Say ('ERROR: ' + $_.Exception.Message) Red }
}

$lines | Set-Content -LiteralPath $reportPath -Encoding UTF8
Say ''
Say ('Report saved: ' + $reportPath) Green
