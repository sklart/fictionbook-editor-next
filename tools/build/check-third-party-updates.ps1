<#
.SYNOPSIS
Проверяет локальные и upstream-версии выбранных сторонних зависимостей.
#>

[CmdletBinding()]
param(
    [ValidateSet("all", "scintilla", "lexilla", "pcre2", "hunspell", "wtl")]
    [string[]]$Dependency = @("all")
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "ThirdPartySources.ps1")

function Resolve-DependenciesToCheck {
    param(
        [string[]]$Selected
    )

    if ($Selected -contains "all") {
        return Get-DependencyCatalog
    }

    return $Selected | ForEach-Object { Get-DependencyEntry -Name $_ }
}

$dependencies = Resolve-DependenciesToCheck -Selected $Dependency
$results = foreach ($entry in $dependencies) {
    try {
        Get-DependencyUpdateInfo -Dependency $entry
    }
    catch {
        [pscustomobject]@{
            Name = $entry.Name
            DisplayName = $entry.DisplayName
            Repository = $entry.Repository
            LocalPath = $entry.LocalPath
            LocalVersion = $null
            RemoteVersion = $null
            RemoteTag = $null
            RemoteZipUrl = $null
            RemoteSource = $null
            Status = "Error"
            Error = $_.Exception.Message
        }
    }
}

$results |
    Select-Object `
        @{ Name = (Get-ThirdPartyText -Base64 "0JfQsNCy0LjRgdC40LzQvtGB0YLRjA=="); Expression = { $_.DisplayName } }, `
        @{ Name = (Get-ThirdPartyText -Base64 "0JvQvtC60LDQu9GM0L3QsNGPINCy0LXRgNGB0LjRjw=="); Expression = { $_.LocalVersion } }, `
        @{ Name = (Get-ThirdPartyText -Base64 "0KPQtNCw0LvRkdC90L3QsNGPINCy0LXRgNGB0LjRjw=="); Expression = { $_.RemoteVersion } }, `
        @{ Name = (Get-ThirdPartyText -Base64 "0KHRgtCw0YLRg9GB"); Expression = { Get-DependencyStatusDisplay -Status $_.Status } }, `
        @{ Name = (Get-ThirdPartyText -Base64 "0KPQtNCw0LvRkdC90L3Ri9C5INGC0LXQsw=="); Expression = { $_.RemoteTag } }, `
        @{ Name = (Get-ThirdPartyText -Base64 "0JjRgdGC0L7Rh9C90LjQuiB1cHN0cmVhbS3QstC10YDRgdC40Lg="); Expression = { $_.RemoteSource } } |
    Format-Table -AutoSize

$errors = $results | Where-Object { $_.Status -eq "Error" }
if ($errors) {
    Write-Host ""
    Write-Host (Get-ThirdPartyText -Base64 "0J7RiNC40LHQutC4INC/0YDQvtCy0LXRgNC60Lg6")
    foreach ($item in $errors) {
        Write-Warning ("{0}: {1}" -f $item.DisplayName, $item.Error)
    }

    exit 1
}

$updates = $results | Where-Object { $_.Status -eq "UpdateAvailable" }
Write-Host ""
if ($updates) {
    Write-Host (Format-ThirdPartyText "0JTQvtGB0YLRg9C/0L3RiyDQvtCx0L3QvtCy0LvQtdC90LjRjzogezB9" (($updates.DisplayName) -join ", "))
}
else {
    Write-Host (Get-ThirdPartyText -Base64 "0JLRgdC1INC/0YDQvtCy0LXRgNC10L3QvdGL0LUg0LfQsNCy0LjRgdC40LzQvtGB0YLQuCDRg9C20LUg0LDQutGC0YPQsNC70YzQvdGLLg==")
}
