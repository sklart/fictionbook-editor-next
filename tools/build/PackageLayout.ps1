Set-StrictMode -Version Latest

function Get-FbePackageLayout {
    param([Parameter(Mandatory)][string]$RepositoryRoot)

    $layoutPath = Join-Path $RepositoryRoot 'packaging\layout.json'
    if (-not (Test-Path -LiteralPath $layoutPath -PathType Leaf)) {
        throw "Package layout is missing: $layoutPath"
    }
    $layout = Get-Content -Raw -LiteralPath $layoutPath | ConvertFrom-Json
    if ($layout.schemaVersion -ne 1) { throw "Unsupported package layout schema: $($layout.schemaVersion)" }
    return $layout
}

function Copy-FbePackageLayoutEntries {
    param(
        [Parameter(Mandatory)]$Entries,
        [Parameter(Mandatory)][hashtable]$SourceRoots,
        [Parameter(Mandatory)][string]$StageDirectory
    )

    foreach ($entry in @($Entries)) {
        if (-not $SourceRoots.ContainsKey($entry.sourceRoot)) {
            throw "Package layout refers to unknown source root: $($entry.sourceRoot)"
        }
        $copyContents = $null -ne $entry.PSObject.Properties['contents'] -and [bool]$entry.contents
        $copyRecursively = $null -ne $entry.PSObject.Properties['recursive'] -and [bool]$entry.recursive
        $sourceRoot = $SourceRoots[$entry.sourceRoot]
        $source = if ([string]::IsNullOrWhiteSpace($entry.source)) { $sourceRoot } else { Join-Path $sourceRoot $entry.source }
        $destination = if ([string]::IsNullOrWhiteSpace($entry.destination)) { $StageDirectory } else { Join-Path $StageDirectory $entry.destination }
        if ($entry.required -and -not (Test-Path -LiteralPath $source)) {
            throw "Package layout input is missing: $($entry.sourceRoot)/$($entry.source)"
        }
        $destinationParent = if ($copyContents) { $destination } else { Split-Path -Parent $destination }
        if (-not [string]::IsNullOrWhiteSpace($destinationParent)) { New-Item -ItemType Directory -Force -Path $destinationParent | Out-Null }
        if ($copyContents) {
            Copy-Item -Path (Join-Path $source '*') -Destination $destination -Recurse -Force
        }
        elseif ($copyRecursively) {
            Copy-Item -LiteralPath $source -Destination $destination -Recurse -Force
        }
        else {
            Copy-Item -LiteralPath $source -Destination $destination -Force
        }
    }
}

function Copy-FbePackageLayoutAliases {
    param(
        [Parameter(Mandatory)]$Aliases,
        [Parameter(Mandatory)][string]$StageDirectory
    )

    foreach ($entry in @($Aliases)) {
        $source = Join-Path $StageDirectory $entry.source
        if ($entry.required -and -not (Test-Path -LiteralPath $source -PathType Leaf)) {
            throw "Package layout alias source is missing: $($entry.source)"
        }
        Copy-Item -LiteralPath $source -Destination (Join-Path $StageDirectory $entry.destination) -Force
    }
}
