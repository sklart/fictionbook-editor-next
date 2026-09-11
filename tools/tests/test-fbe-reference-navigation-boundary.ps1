<# Verifies that FB2 note/reference DOM resolution remains outside CFBEView. #>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$reference = Get-Content -Raw (Join-Path $root 'src\fbe\navigation\ReferenceNavigation.cpp')
$linkPure = Get-Content -Raw (Join-Path $root 'src\fbe\LinkNavigation.h')
$linkDom = Get-Content -Raw (Join-Path $root 'src\fbe\navigation\LinkDomNavigation.cpp')
$view = Get-Content -Raw (Join-Path $root 'src\fbe\FBEview.cpp')

foreach($forbidden in @('CFBEView', 'CMainFrame', 'FBEview.h', 'mainfrm.h', 'Settings', 'ShellExecute', 'MessageBox')) {
    if($reference -match [regex]::Escape($forbidden)) { throw "ReferenceNavigation зависит от $forbidden." }
}
foreach($forbidden in @('#include <mshtml', 'IHTML', 'CFBEView', 'CMainFrame', 'mainfrm.h', 'FBEview.h')) {
    if($linkPure -match [regex]::Escape($forbidden)) { throw "Pure LinkNavigation зависит от $forbidden." }
}
foreach($forbidden in @('CFBEView', 'CMainFrame', 'mainfrm.h', 'FBEview.h', 'ShellExecute', 'Settings')) {
    if($linkDom -match [regex]::Escape($forbidden)) { throw "LinkDomNavigation зависит от $forbidden." }
}
foreach($method in @('GoToFootnote', 'GoToReference')) {
    $match = [regex]::Match($view, "bool CFBEView::$method\(bool fCheck\)\s*\{(?<body>.*?)\n\}", [Text.RegularExpressions.RegexOptions]::Singleline)
    if(-not $match.Success) { throw "Не найден wrapper CFBEView::$method." }
    $body = $match.Groups['body'].Value
    foreach($forbidden in @('createRange', 'getElementsByTagName', 'SelectionAnchor', 'GetHP', 'AU::GetAttrCS')) {
        if($body -match [regex]::Escape($forbidden)) { throw "CFBEView::$method retains DOM resolver: $forbidden" }
    }
}
foreach($required in @('FBEReferenceNavigation::FindFootnoteTarget', 'FBEReferenceNavigation::FindReferenceTarget')) {
    if(-not $view.Contains($required)) { throw "CFBEView не координирует $required." }
}
Write-Host 'Reference navigation boundary passed.'
