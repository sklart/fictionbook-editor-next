[CmdletBinding()]
param([string]$ArtifactDirectory = (Join-Path $PSScriptRoot '..\..\out\tests\full-regex-help-html'), [string]$BrowserExe)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ArtifactDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ArtifactDirectory)
New-Item -ItemType Directory -Path $ArtifactDirectory -Force | Out-Null
if(-not $BrowserExe) {
    $candidates = @('C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe','C:\Program Files\Microsoft\Edge\Application\msedge.exe')
    $BrowserExe = @($candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1)[0]
}
if(-not $BrowserExe -or -not (Test-Path -LiteralPath $BrowserExe)) { throw 'Microsoft Edge is required for HTML Help visual smoke.' }
$helpRoot = Join-Path $repoRoot 'runtime\Help\en-US'
$cases = @(
    @{Name='design-top'; Mode='design'; Anchor='top'; Width=1200; Height=800; Dark=$false},
    @{Name='design-table'; Mode='design'; Anchor='table-1'; Width=1200; Height=800; Dark=$false},
    @{Name='design-regex'; Mode='design'; Anchor='regex-1'; Width=1200; Height=800; Dark=$false},
    @{Name='design-example'; Mode='design'; Anchor='example-1'; Width=1200; Height=800; Dark=$false},
    @{Name='design-note'; Mode='design'; Anchor='note-1'; Width=1200; Height=800; Dark=$false},
    @{Name='design-warning'; Mode='design'; Anchor='warning-1'; Width=1200; Height=800; Dark=$false},
    @{Name='source-top'; Mode='source'; Anchor='top'; Width=1200; Height=800; Dark=$false},
    @{Name='source-table'; Mode='source'; Anchor='table-1'; Width=1200; Height=800; Dark=$false},
    @{Name='source-narrow'; Mode='source'; Anchor='regex-1'; Width=640; Height=800; Dark=$false},
    @{Name='source-wide'; Mode='source'; Anchor='table-1'; Width=1600; Height=800; Dark=$false},
    @{Name='design-dark'; Mode='design'; Anchor='example-1'; Width=1200; Height=800; Dark=$true},
    @{Name='source-dark'; Mode='source'; Anchor='table-1'; Width=1200; Height=800; Dark=$true}
)
Add-Type -AssemblyName System.Drawing
$results = @{}
foreach($case in $cases) {
    $page = Join-Path $helpRoot "regex-$($case.Mode).html"
    $source = Get-Content -LiteralPath $page -Raw -Encoding UTF8
    if($source -notmatch ('id="' + [regex]::Escape($case.Anchor) + '"')) { throw "Marker navigation target missing: $($case.Name)" }
    $output = Join-Path $ArtifactDirectory ($case.Name + '.png')
    if(Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output -Force }
    $profile = Join-Path ([IO.Path]::GetTempPath()) ('fbe-regex-help-edge-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $profile | Out-Null
    $view = Join-Path $profile 'view.html'
    $css = ([Uri](Join-Path $repoRoot 'runtime\Help\regex-help.css')).AbsoluteUri
    $captureSource = $source.Replace('../regex-help.css', $css)
    if($case.Anchor -ne 'top') {
        $anchorIndex = $captureSource.IndexOf('id="' + $case.Anchor + '"', [StringComparison]::Ordinal)
        $blockStart = $captureSource.LastIndexOf('<', $anchorIndex)
        $mainStart = $captureSource.IndexOf('<main>', [StringComparison]::Ordinal) + 6
        $mainEnd = $captureSource.IndexOf('</main>', [StringComparison]::Ordinal)
        if($blockStart -lt $mainStart -or $blockStart -ge $mainEnd) { throw "Marker navigation position invalid: $($case.Name)" }
        $captureSource = $captureSource.Substring(0,$mainStart) + $captureSource.Substring($blockStart,$mainEnd-$blockStart) + $captureSource.Substring($mainEnd)
    }
    [IO.File]::WriteAllText($view,$captureSource,[Text.UTF8Encoding]::new($false))
    $url = ([Uri]$view).AbsoluteUri
    $arguments = @('--headless=new','--disable-gpu','--no-first-run','--no-default-browser-check',
        ('--user-data-dir="' + $profile + '"'),('--screenshot="' + $output + '"'),
        ('--window-size=' + $case.Width + ',' + $case.Height),'--virtual-time-budget=1000')
    if($case.Dark) { $arguments += '--force-dark-mode' }
    $arguments += $url
    try {
        $process = Start-Process -FilePath $BrowserExe -ArgumentList $arguments -WindowStyle Hidden -PassThru
        $deadline = [DateTime]::UtcNow.AddSeconds(30)
        while([DateTime]::UtcNow -lt $deadline -and (-not (Test-Path -LiteralPath $output) -or (Get-Item -LiteralPath $output).Length -lt 10000)) { Start-Sleep -Milliseconds 200 }
        if(-not (Test-Path -LiteralPath $output) -or (Get-Item -LiteralPath $output).Length -lt 10000) { throw "HTML Help screenshot missing: $($case.Name)" }
        $bitmap = [Drawing.Image]::FromFile($output)
        try { if($bitmap.Width -lt [Math]::Min(500,$case.Width) -or $bitmap.Height -lt 600) { throw "HTML Help screenshot dimensions are invalid: $($case.Name)" } }
        finally { $bitmap.Dispose() }
        $results[$case.Name] = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash
    } finally {
        if($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue }
        $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
        if([IO.Path]::GetFullPath($profile).StartsWith($tempRoot,[StringComparison]::OrdinalIgnoreCase)) { Remove-Item -LiteralPath $profile -Recurse -Force -ErrorAction SilentlyContinue }
    }
}
foreach($group in @(@('design-top','design-table','design-regex','design-example','design-note','design-warning'),@('source-top','source-table','source-narrow','source-wide'))) {
    if(@($group | ForEach-Object { $results[$_] } | Select-Object -Unique).Count -ne $group.Count) { throw "HTML Help marker screenshots are identical: $($group -join ', ')" }
}
if($results['design-dark'] -eq $results['design-example'] -or $results['source-dark'] -eq $results['source-table']) { throw 'HTML Help dark screenshots are identical to light.' }
@("result=pass","screenshots=$($cases.Count)","browser=$BrowserExe") | Set-Content -LiteralPath (Join-Path $ArtifactDirectory 'report.txt') -Encoding utf8
Write-Host "Regex Help HTML visual smoke passed: $($cases.Count) screenshots."
