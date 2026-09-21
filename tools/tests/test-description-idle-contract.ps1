$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path $PSScriptRoot -Parent)
$header = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.h')
$frame = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\mainfrm.cpp')
$viewHeader = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.h')
$view = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'src\fbe\FBEview.cpp')

foreach($token in @('WM_DESCRIPTION_FORM_CHANGED', 'OnDescriptionFormChanged')) {
    if($header.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "Description dirty event contract is missing '$token'." }
}
foreach($token in @('DISPID_HTMLDOCUMENTEVENTS2_ONKEYUP', 'OnKeyUp')) {
    if($viewHeader.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "Description input event sink is missing '$token'." }
}
foreach($token in @('WM_DESCRIPTION_FORM_CHANGED', 'm_form_changed = true')) {
    if($view.IndexOf($token, [StringComparison]::Ordinal) -lt 0) { throw "Description input mutation handling is missing '$token'." }
}
$idleStart = $frame.IndexOf('BOOL CMainFrame::OnIdle()')
$idleEnd = $frame.IndexOf('void CMainFrame::AddTbButton', $idleStart)
$idle = $frame.Substring($idleStart, $idleEnd - $idleStart)
if($idle.IndexOf('if(m_need_title_update)', [StringComparison]::Ordinal) -lt 0) { throw 'Title update must be event-driven.' }
if($idle.IndexOf('m_change_state != DocChanged()', [StringComparison]::Ordinal) -ge 0) { throw 'Stable idle must not poll IsFormChanged through DocChanged().' }
if($idle.IndexOf('else if (m_editor_view_state.Current() == BODY)', [StringComparison]::Ordinal) -lt 0) { throw 'DESCRIPTION must not fall through to BODY command-state work.' }
Write-Host 'Description idle contract passed.'
