function Get-ThirdPartyUpdatesPage {
    param([Parameter(Mandatory)]$Item)

    $repository = [string]$Item.Repository
    if ($Item.Name -eq 'fluent-pin') { return 'https://github.com/microsoft/fluentui-system-icons/tree/main/assets/Pin/SVG' }
    if ($Item.Name -eq 'scintilla') { return 'https://www.scintilla.org/ScintillaDownload.html' }
    if ($Item.Name -eq 'wtl') { return 'https://sourceforge.net/projects/wtl/files/' }
    if ($Item.Name -eq 'dict-de_DE') { return 'https://github.com/LibreOffice/dictionaries/commits/master/de' }
    if ($repository -match '^[^/]+/[^/]+$') { return 'https://github.com/' + $repository + '/releases' }
    if ($repository -match '^https://github\.com/') {
        return ($repository.TrimEnd('/') -replace '\.git$', '') + '/releases'
    }
    if ($repository -match '^https://[^/]+\.googlesource\.com/') {
        return $repository.TrimEnd('/') + '/+refs'
    }
    if ($repository -match '^https://') { return $repository }
    return $null
}

function Write-ThirdPartyUpdateNotice {
    param(
        [Parameter(Mandatory)]$Item,
        [switch]$ManualReview
    )

    if ($Item.Name -eq 'fluent-pin') {
        $message = '{0}: upstream SVGs changed ({1}); pinned {2}' -f $Item.DisplayName,($Item.ChangedFiles -join ', '),$Item.LocalCommit
        $title = 'Third-party update available'
    }
    elseif ($ManualReview) {
        $message = '{0}: manual review required (local {1}, upstream {2}, tag {3})' -f $Item.DisplayName,$Item.LocalVersion,$Item.RemoteVersion,$Item.RemoteTag
        $title = 'Third-party manual review'
    }
    else {
        $message = '{0}: local {1}, latest {2} ({3})' -f $Item.DisplayName,$Item.LocalVersion,$Item.RemoteVersion,$Item.RemoteTag
        $title = 'Third-party update available'
    }

    $updatesPage = Get-ThirdPartyUpdatesPage -Item $Item
    if ($updatesPage) { $message += ' ' + $updatesPage }

    if ($env:GITHUB_ACTIONS -eq 'true') {
        Write-Host ('::warning title={0}::{1}' -f $title,$message)
    }
    else {
        Write-Warning $message
    }
}
