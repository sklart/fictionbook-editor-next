<#
.SYNOPSIS
Checks local and upstream versions of all managed third-party dependencies and dictionaries.
#>

[CmdletBinding()]
param(
    [string[]]$Dependency = @('all'),
    [switch]$WarnOnUpdate,
    [switch]$FailOnUpdate
)

$ErrorActionPreference = 'Stop'

if ($WarnOnUpdate -and $FailOnUpdate) {
    throw 'Use either -WarnOnUpdate or -FailOnUpdate, not both.'
}

. (Join-Path $PSScriptRoot 'ThirdPartySources.ps1')
. (Join-Path $PSScriptRoot 'DictionarySources.ps1')

function Resolve-ItemsToCheck {
    param([string[]]$Selected)

    if ($Selected -contains 'all') {
        return @((Get-DependencyCatalog)) + @((Get-DictionaryCatalog))
    }

    $resolved = foreach ($name in $Selected) {
        if ($name -match '^(dict-)?(en_US|ru_RU|uk_UA|de_DE)$') {
            Get-DictionaryEntry -Name $name
            continue
        }

        try {
            Get-DependencyEntry -Name $name
        }
        catch {
            try { Get-DictionaryEntry -Name $name }
            catch { throw "Unknown dependency or dictionary: $name" }
        }
    }

    return @($resolved)
}

$items = Resolve-ItemsToCheck -Selected $Dependency
$results = foreach ($entry in $items) {
    try {
        if ($entry.Kind -eq 'Dictionary') { Get-DictionaryUpdateInfo -DictionaryEntry $entry }
        else { Get-DependencyUpdateInfo -Dependency $entry }
    }
    catch {
        [pscustomobject]@{
            Name=$entry.Name; DisplayName=$entry.DisplayName; Repository=$entry.Repository; LocalPath=$entry.LocalPath
            LocalVersion=$null; LocalCommit=$null; RemoteVersion=$null; RemoteTag=$null; RemoteCommit=$null
            RemoteZipUrl=$null; RemoteSource=$null; Status='Error'; Kind=$entry.Kind; UpdateMode=$entry.UpdateMode; Error=$_.Exception.Message
        }
    }
}

$results |
    Select-Object `
        @{Name='Dependency';Expression={$_.DisplayName}}, `
        @{Name='Local';Expression={$_.LocalVersion}}, `
        @{Name='Latest';Expression={$_.RemoteVersion}}, `
        @{Name='Status';Expression={Get-DependencyStatusDisplay -Status $_.Status}}, `
        @{Name='Tag';Expression={$_.RemoteTag}}, `
        @{Name='Source';Expression={$_.RemoteSource}} |
    Format-Table -AutoSize

$errors = @($results | Where-Object { $_.Status -eq 'Error' })
if ($errors.Count -gt 0) {
    Write-Host ''
    Write-Host 'Check errors:'
    foreach ($item in $errors) { Write-Warning ("{0}: {1}" -f $item.DisplayName,$item.Error) }
    exit 1
}

$updates = @($results | Where-Object { $_.Status -eq 'UpdateAvailable' })
$reviews = @($results | Where-Object { $_.Status -eq 'NeedsReview' })
$notInstalled = @($results | Where-Object { $_.Status -eq 'NotInstalled' })

Write-Host ''
if ($updates.Count -gt 0) { Write-Host ('Updates available: {0}' -f (($updates.DisplayName) -join ', ')) }
else { Write-Host 'No stable upstream updates found for installed dependencies.' }

if ($reviews.Count -gt 0) {
    Write-Warning ('Manual review required: {0}' -f (($reviews.DisplayName) -join ', '))
}

if ($notInstalled.Count -gt 0) {
    Write-Host ('Optional/not installed: {0}' -f (($notInstalled.DisplayName) -join ', '))
}

if ($WarnOnUpdate) {
    foreach ($item in $updates) {
        $message = '{0}: local {1}, latest {2} ({3})' -f $item.DisplayName,$item.LocalVersion,$item.RemoteVersion,$item.RemoteTag
        Write-Warning $message
        if ($env:GITHUB_ACTIONS -eq 'true') {
            Write-Host ('::warning title=Third-party update available::{0}' -f $message)
        }
    }

    foreach ($item in $reviews) {
        $message = '{0}: manual review required (local {1}, upstream {2}, tag {3})' -f $item.DisplayName,$item.LocalVersion,$item.RemoteVersion,$item.RemoteTag
        Write-Warning $message
        if ($env:GITHUB_ACTIONS -eq 'true') {
            Write-Host ('::warning title=Third-party manual review::{0}' -f $message)
        }
    }
}

if ($FailOnUpdate -and ($updates.Count -gt 0 -or $reviews.Count -gt 0)) {
    exit 2
}

exit 0
