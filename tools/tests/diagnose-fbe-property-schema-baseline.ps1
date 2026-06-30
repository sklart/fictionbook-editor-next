[CmdletBinding()]
param(
    [switch]$AttemptRepoUnregister,
    [switch]$NoRestartExplorer
)

# .SYNOPSIS
# Диагностирует baseline published-состояния FBE-specific property schema:
# показывает, опубликованы ли FBE-свойства до установки, есть ли schema-файл
# в репозитории и shared shell dir, и при необходимости повторяет замер после
# попытки снять регистрацию repo-схемы.

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

$repoSchemaPath = Join-Path $repoRoot "packaging\property-schema\FBE.Sequence.propdesc"
$sharedShellDir = Join-Path $env:ProgramData "FictionBook Editor\Shell"
$sharedSchemaPath = Join-Path $sharedShellDir "FBE.Sequence.propdesc"
$propertyHandlerKey = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.fb2'
$thumbnailShellExKey = 'HKLM:\SOFTWARE\Classes\.fb2\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}'
$thumbnailClsidKey = 'HKLM:\SOFTWARE\Classes\CLSID\{4F99D1F0-5D76-4B9C-9D3D-9E6B8B4C7E31}\InprocServer32'
$fb2ProgIdKey = 'HKCU:\Software\Classes\FictionBook.2'
$unregisterScript = Join-Path $repoRoot "tools\build\unregister-sequence-property-schema.ps1"

function Ensure-PropSysProbeType {
    if (-not ("FbeSpecificBaselineProbe" -as [type])) {
        Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class FbeSpecificBaselineProbe {
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
    $hr = [FbeSpecificBaselineProbe]::PSGetPropertyDescriptionByName(
        $CanonicalName,
        [ref]$iid,
        [ref]$ptr
    )

    if ($ptr -ne [IntPtr]::Zero) {
        [Runtime.InteropServices.Marshal]::Release($ptr) | Out-Null
    }

    [pscustomobject]@{
        CanonicalName = $CanonicalName
        HResultHex = ('0x{0:X8}' -f ($hr -band 0xffffffff))
        Registered = ($hr -ge 0)
    }
}

function Get-EnvironmentSnapshot {
    param(
        [Parameter(Mandatory)]
        [string]$Label
    )

    [pscustomobject]@{
        Этап = $Label
        RepoSchemaExists = [bool](Test-Path -LiteralPath $repoSchemaPath -PathType Leaf)
        SharedSchemaExists = [bool](Test-Path -LiteralPath $sharedSchemaPath -PathType Leaf)
        PropertyHandlerRegistered = [bool](Test-Path -LiteralPath $propertyHandlerKey)
        ThumbnailShellExRegistered = [bool](Test-Path -LiteralPath $thumbnailShellExKey)
        ThumbnailClsidRegistered = [bool](Test-Path -LiteralPath $thumbnailClsidKey)
        Fb2ProgIdRegistered = [bool](Test-Path -LiteralPath $fb2ProgIdKey)
    }
}

function Get-PropertyProbeSet {
    param(
        [Parameter(Mandatory)]
        [string]$Label
    )

    foreach ($canonicalName in $canonicalNames) {
        $probe = Get-PropertyProbe -CanonicalName $canonicalName
        [pscustomobject]@{
            Этап = $Label
            'Каноническое имя' = $probe.CanonicalName
            HRESULT = $probe.HResultHex
            Зарегистрировано = $probe.Registered
        }
    }
}

Write-Host "Диагностика baseline published-состояния FBE-specific property schema"
Write-Host "  Repo schema: $repoSchemaPath"
Write-Host "  Shared shell dir: $sharedShellDir"
Write-Host ""

$snapshots = @()
$probes = @()

$snapshots += Get-EnvironmentSnapshot -Label "До действий"
$probes += @(Get-PropertyProbeSet -Label "До действий")

if ($AttemptRepoUnregister) {
    Write-Host "Пробую снять регистрацию repo-схемы перед повторным замером..."
    & $unregisterScript -NoRestartExplorer:$NoRestartExplorer
    Write-Host ""

    $snapshots += Get-EnvironmentSnapshot -Label "После unregister repo schema"
    $probes += @(Get-PropertyProbeSet -Label "После unregister repo schema")
}

Write-Host "Снимок окружения"
$snapshots | Format-Table -AutoSize

Write-Host ""
Write-Host "Published FBE-specific свойства"
$probes | Format-Table -AutoSize

