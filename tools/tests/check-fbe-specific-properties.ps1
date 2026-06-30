[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$canonicalNames = @(
    "FBE.Genre",
    "FBE.Sequence",
    "FBE.DocumentVersion",
    "FBE.DocumentDate",
    "FBE.Keywords",
    "FBE.DocumentId"
)

$expectedFb2ShellStrings = [ordered]@{
    InfoTip = "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;System.Size"
    TileInfo = "prop:System.Author;System.Title"
    Details = "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Genre;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;FBE.Keywords;FBE.DocumentId;System.Size"
    PreviewDetails = "prop:System.ItemTypeText;System.Author;System.Title;System.Language;FBE.Genre;FBE.Sequence;FBE.DocumentVersion;FBE.DocumentDate;FBE.Keywords;FBE.DocumentId;System.Size"
}

function Ensure-PropSysProbeType {
    if (-not ("FbeSpecificPropertyProbe" -as [type])) {
        Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class FbeSpecificPropertyProbe {
    [DllImport("propsys.dll", CharSet = CharSet.Unicode)]
    public static extern int PSGetPropertyDescriptionByName(string canonicalName, ref Guid riid, out IntPtr ppv);
}
"@
    }
}

function Get-PropertyProbe {
    param(
        [Parameter(Mandatory)]
        [string]$CanonicalName
    )

    Ensure-PropSysProbeType

    $iid = [Guid]'6F79D558-3E96-4549-A1D1-7D75D2288814'
    $ptr = [IntPtr]::Zero
    $hr = [FbeSpecificPropertyProbe]::PSGetPropertyDescriptionByName(
        $CanonicalName,
        [ref]$iid,
        [ref]$ptr
    )

    if ($ptr -ne [IntPtr]::Zero) {
        [Runtime.InteropServices.Marshal]::Release($ptr) | Out-Null
    }

    [pscustomobject]@{
        CanonicalName = $CanonicalName
        HResult = ('0x{0:X8}' -f ($hr -band 0xffffffff))
        Registered = ($hr -ge 0)
    }
}

function Get-Fb2ProgIdShellStrings {
    $progIdKey = 'HKCU:\Software\Classes\FictionBook.2'
    $result = [ordered]@{}

    foreach ($valueName in $expectedFb2ShellStrings.Keys) {
        $value = $null
        if (Test-Path $progIdKey) {
            $value = (Get-ItemProperty -Path $progIdKey -Name $valueName -ErrorAction SilentlyContinue).$valueName
        }

        $result[$valueName] = [pscustomobject]@{
            Name = $valueName
            ExpectedValue = $expectedFb2ShellStrings[$valueName]
            ActualValue = $value
            MatchesExpected = ($value -eq $expectedFb2ShellStrings[$valueName])
            Present = -not [string]::IsNullOrWhiteSpace($value)
        }
    }

    return @($result.Values)
}

$schemaPath = Join-Path $repoRoot "packaging\property-schema\FBE.Sequence.propdesc"
$propertyHandlerKey = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.fb2'
$fb2ProgIdKey = 'HKCU:\Software\Classes\FictionBook.2'

Write-Host "Проверка состояния FBE-specific shell-свойств"
Write-Host "  Файл схемы в репозитории: $schemaPath"
Write-Host "  Файл схемы существует: $([bool](Test-Path -LiteralPath $schemaPath -PathType Leaf))"
Write-Host "  PropertyHandlers\\.fb2 зарегистрирован: $([bool](Test-Path $propertyHandlerKey))"
Write-Host "  ProgID FictionBook.2 зарегистрирован: $([bool](Test-Path $fb2ProgIdKey))"
Write-Host ""

$results = foreach ($canonicalName in $canonicalNames) {
    Get-PropertyProbe -CanonicalName $canonicalName
}

$results |
    Select-Object `
        @{ Name = "Каноническое имя"; Expression = { $_.CanonicalName } }, `
        @{ Name = "HRESULT"; Expression = { $_.HResult } }, `
        @{ Name = "Зарегистрировано"; Expression = { $_.Registered } } |
    Format-Table -AutoSize

Write-Host ""
Write-Host "Shell-строки FictionBook.2"
Get-Fb2ProgIdShellStrings |
    Select-Object `
        @{ Name = "Имя"; Expression = { $_.Name } }, `
        @{ Name = "Присутствует"; Expression = { $_.Present } }, `
        @{ Name = "Совпадает с ожидаемым"; Expression = { $_.MatchesExpected } }, `
        @{ Name = "Фактическое значение"; Expression = { $_.ActualValue } } |
    Format-Table -AutoSize
