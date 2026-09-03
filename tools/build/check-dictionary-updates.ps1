<#
.SYNOPSIS
Compatibility wrapper: checks dictionary updates using the common third-party update framework.
#>

[CmdletBinding()]
param([switch]$FailOnUpdate)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ThirdPartySources.ps1')
. (Join-Path $PSScriptRoot 'DictionarySources.ps1')

$results = foreach ($entry in (Get-DictionaryCatalog)) {
    try { Get-DictionaryUpdateInfo -DictionaryEntry $entry }
    catch {
        [pscustomobject]@{
            Name=$entry.Name; DisplayName=$entry.DisplayName; LocalVersion=$null; RemoteVersion=$null; RemoteTag=$null
            Repository=$entry.Repository; Status='Error'; Error=$_.Exception.Message
        }
    }
}

$results |
    Select-Object `
        @{Name='Dictionary';Expression={$_.DisplayName}}, `
        @{Name='Installed';Expression={$_.LocalVersion}}, `
        @{Name='Latest';Expression={$_.RemoteVersion}}, `
        @{Name='Status';Expression={Get-DependencyStatusDisplay -Status $_.Status}}, `
        @{Name='Upstream';Expression={$_.Repository}} |
    Format-Table -AutoSize

$errors = @($results | Where-Object { $_.Status -eq 'Error' })
if ($errors.Count -gt 0) {
    foreach ($item in $errors) { Write-Warning ("{0}: {1}" -f $item.DisplayName,$item.Error) }
    exit 1
}

if ($FailOnUpdate -and @($results | Where-Object { $_.Status -eq 'UpdateAvailable' }).Count -gt 0) { exit 2 }
