$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path $PSScriptRoot -Parent)
$source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
$idleStart = $source.IndexOf('BOOL CMainFrame::OnIdle()')
$idleEnd = $source.IndexOf('void CMainFrame::AddTbButton', $idleStart)
if($idleStart -lt 0 -or $idleEnd -le $idleStart) { throw 'Unable to locate CMainFrame::OnIdle.' }
$idle = $source.Substring($idleStart, $idleEnd - $idleStart)
foreach($call in @('RefreshLocalizedToolbarButtonTexts(m_CmdToolbar)', 'RefreshLocalizedToolbarButtonTexts(m_ScriptsToolbar)')) {
    if($idle.IndexOf($call, [StringComparison]::Ordinal) -ge 0) {
        throw "SOURCE idle must not relocalize toolbar captions: $call"
    }
    if($source.IndexOf($call, [StringComparison]::Ordinal) -lt 0) {
        throw "Toolbar caption localization must remain available for view/configuration changes: $call"
    }
}
Write-Host 'SOURCE idle toolbar localization contract passed.'
